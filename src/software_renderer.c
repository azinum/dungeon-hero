// software_renderer.c

static render_state RenderState;

static mat4 Proj;

typedef enum blend_mode {
  BLEND_MODE_NORMAL = 0,
  BLEND_MODE_ADD,
} blend_mode;

#define LightStrength 200.0f
#define AMBIENT_LIGHT 3
#define DRAW_SOLID 1
#define DRAW_BOUNDING_BOX 0
#define DRAW_BOUNDING_BOX_POINTS 0
#define DRAW_VERTICES 1
#define DITHERING 1

#define Degenerate(V0, V1, V2) \
  ((V0.X == V1.X && V0.Y == V1.Y) || \
  ((V1.X == V2.X && V1.Y == V2.Y)))

static void FrameBufferCreate(framebuffer* FrameBuffer, u32 Width, u32 Height) {
  FrameBuffer->Data = malloc(Width * Height * 4);
  FrameBuffer->Width = Width;
  FrameBuffer->Height = Height;
}

static void FrameBufferClear(framebuffer* FrameBuffer, color Color) {
  u32 Count = FrameBuffer->Width * FrameBuffer->Height;
#if USE_SSE
  __m128i* Dest = (__m128i*)FrameBuffer->Color;

#if DITHERING
  __m128i Value = _mm_setr_epi8(
    Color.B, Color.G, Color.R, Color.A,
    Color.B >> 2, Color.G >> 2, Color.R >> 2, Color.A,
    Color.B, Color.G, Color.R, Color.A,
    Color.B >> 2, Color.G >> 2, Color.R >> 2, Color.A
  );
#else
  __m128i Value = _mm_setr_epi8(
    Color.B, Color.G, Color.R, Color.A,
    Color.B, Color.G, Color.R, Color.A,
    Color.B, Color.G, Color.R, Color.A,
    Color.B, Color.G, Color.R, Color.A
  );
#endif
  for (u32 Index = 0; Index < (Count / 4); ++Index) {
    *(Dest)++ = Value;
  }
#else
  color* Dest = FrameBuffer->Color;

  for (u32 Index = 0; Index < Count; ++Index) {
    *(Dest)++ = Color;
  }
#endif
}

static void ZBufferClear(float* ZBuffer, u32 Width, u32 Height) {
  u32 Count = Width * Height;
  float FillValue = -1.0f;
#if USE_SSE
  __m128* Dest = (__m128*)ZBuffer;
  __m128 Value = _mm_set_ps1(FillValue);

  for (u32 Index = 0; Index < (Count / 4); ++Index) {
    *(Dest++) = Value;
  }
#else
  for (u32 Index = 0; Index < Count; ++Index) {
    *(ZBuffer++) = FillValue;
  }
#endif
}

static void FrameBufferDestroy(framebuffer* FrameBuffer) {
  Assert(FrameBuffer);
  if (FrameBuffer->Data) {
    free(FrameBuffer->Data);
    FrameBuffer->Data = NULL;
    FrameBuffer->Width = 0;
    FrameBuffer->Height = 0;
  }
}

inline void Barycentric(v3 P, v3 A, v3 B, v3 C, float* W0, float* W1, float* W2) {
  v3 V0 = DifferenceV3(B, A);
  v3 V1 = DifferenceV3(C, A);
  v3 V2 = DifferenceV3(P, A);

  float D00 = DotVec3(V0, V0);
  float D01 = DotVec3(V0, V1);
  float D11 = DotVec3(V1, V1);
  float D20 = DotVec3(V2, V0);
  float D21 = DotVec3(V2, V1);

  float Denominator = D00 * D11 - D01 * D01;
  if (Denominator != 0.0f) {
    *W1 = (D11 * D20 - D01 * D21) / Denominator;
    *W2 = (D00 * D21 - D01 * D20) / Denominator;
  }
  *W0 = 1.0f - *W1 - *W2;
}

inline v3 CartesianV3(v3 A, v3 B, v3 C, float W0, float W1, float W2) {
  v3 Result;

  Result.X = (A.X * W0) + (B.X * W0) + (C.X * W0);
  Result.Y = (A.Y * W1) + (B.Y * W1) + (C.Y * W1);
  Result.Z = (A.Z * W2) + (B.Z * W2) + (C.Z * W2);

  return Result;
}

inline v2 Cartesian(v2 V0, v2 V1, v2 V2, float W0, float W1, float W2) {
  v2 Result;

  Result.X = (V0.X * W0) + (V1.X * W1) + (V2.X * W2);
  Result.Y = (V0.Y * W0) + (V1.Y * W1) + (V2.Y * W2);

  return Result;
}

inline color RGBToBGR(u8* A) {
  color Result;

  Result.R = *(A + 0);
  Result.G = *(A + 1);
  Result.B = *(A + 2);
  Result.A = 255;

  return Result;
}

inline void DrawPixel(framebuffer* FrameBuffer, i32 X, i32 Y, color Color) {
  if (X < 0 || Y < 0 || X >= (i32)FrameBuffer->Width || Y >= (i32)FrameBuffer->Height) {
    return;
  }

#if DITHERING
  u8 Dither = (X % 2) * (Y % 2);
  Color.R >>= 2 * Dither;
  Color.G >>= 2 * Dither;
  Color.B >>= 2 * Dither;
#endif

  // NOTE(lucas): Origin is at the bottom left corner!
  color* Pixel = (color*)&FrameBuffer->Color[(FrameBuffer->Height - Y - 1) * FrameBuffer->Width + X];
  *Pixel = Color;
}

inline void DrawPixelAdd(framebuffer* FrameBuffer, i32 X, i32 Y, color Color) {
  if (X < 0 || Y < 0 || X >= (i32)FrameBuffer->Width || Y >= (i32)FrameBuffer->Height) {
    return;
  }

  float Opacity = 0.0f;
  if (Color.A != 0.0f) {
    Opacity = Color.A / 255.0f;
  }
#if DITHERING
  if (!(X % 2) && !(Y % 2)) {
    Color.R >>= 2;
    Color.G >>= 2;
    Color.B >>= 2;
  }
#endif
  color* Pixel = (color*)&FrameBuffer->Color[(FrameBuffer->Height - Y - 1) * FrameBuffer->Width + X];
  Pixel->R = Clamp(Pixel->R + (Opacity * Color.R), 0, 255);
  Pixel->G = Clamp(Pixel->G + (Opacity * Color.G), 0, 255);
  Pixel->B = Clamp(Pixel->B + (Opacity * Color.B), 0, 255);
}

// NOTE(lucas): Bresenham!
inline void DrawLine(framebuffer* FrameBuffer, v2 A, v2 B, color Color) {
  u8 Steep = 0;
  if (Abs(A.X - B.X) < Abs(A.Y - B.Y)) {
    Steep = 1;
    Swap(A.X, A.Y);
    Swap(B.X, B.Y);
  }
  if (A.X > B.X) {
    Swap(A.X, B.X);
    Swap(A.Y, B.Y);
  }

  for (i32 X = A.X; X < B.X; X++) {
    i32 T = (X - A.X) / (float)(B.X - A.X);
    i32 Y = A.Y * (1.0f - T) + (B.Y * T);
    if (Steep) {
      DrawPixel(FrameBuffer, Y, X, Color);
    }
    else {
      DrawPixel(FrameBuffer, X, Y, Color);
    }
  }
}

inline void DrawTexture2D(framebuffer* FrameBuffer, i32 X, i32 Y, i32 W, i32 H, float XOffset, float YOffset, float XRange, float YRange, image* Texture, color Tint) {
  i32 MinX = X;
  i32 MinY = Y;
  i32 MaxX = X + W;
  i32 MaxY = Y + H;
  i32 XCoord = 0;
  i32 YCoord = 0;
  i32 XDiff = 0;
  i32 YDiff = 0;

  // TODO(lucas): Cleanup sampling!
  for (Y = MinY; Y < MaxY - 1; ++Y) {
    YDiff = MaxY - Y;
    YCoord = Texture->Height * ((float)YDiff / H) * YRange + (YOffset * Texture->Height);

    for (X = MinX; X < MaxX - 1; ++X) {
      XDiff = X - MaxX;
      XCoord = Texture->Width * ((float)(XDiff) / W) * XRange + (XOffset * Texture->Width);
      color Texel = RGBToBGR(&Texture->PixelBuffer[(i32)(4 * ((YCoord * Texture->Width) + XCoord))]);

      if (Texel.R == 255 && Texel.G == 0 && Texel.B == 255) {
        continue;
      }

      // TODO(lucas): Add full color tint
      Texel.R *= Tint.R;
      Texel.G *= Tint.G;
      Texel.B *= Tint.B;

      DrawPixel(FrameBuffer, X, Y, Texel);
    }
  }
}

#define DrawSimpleTexture2D(FrameBuffer, X, Y, W, H, TEXTURE, TINT) \
  DrawTexture2D(FrameBuffer, X, Y, W, H, 0, 0, 1, 1, TEXTURE, TINT)

static void DrawRect(framebuffer* FrameBuffer, i32 X, i32 Y, i32 W, i32 H, color Color, blend_mode BlendMode) {
  i32 MinX = X;
  i32 MinY = Y;
  i32 MaxX = X + W;
  i32 MaxY = Y + H;
  for (Y = MinY; Y < MaxY; ++Y) {
    for (X = MinX; X < MaxX; ++X) {
      switch (BlendMode) {
        case BLEND_MODE_NORMAL:
          DrawPixel(FrameBuffer, X, Y, Color);
          break;
        case BLEND_MODE_ADD:
          DrawPixelAdd(FrameBuffer, X, Y, Color);
          break;
      }
    }
  }
}

// NOTE(lucas): Triangles are drawn in counterclockwise order
static void DrawFilledTriangle(framebuffer* FrameBuffer, float* ZBuffer, v3 A, v3 B, v3 C, v2 T0, v2 T1, v2 T2, image* Texture, float LightFactor) {
  if (A.X < 0 || A.Y < 0 || B.X < 0 || B.Y < 0 || C.X < 0 || C.Y < 0) {
    return;
  }
  if (A.X >= FrameBuffer->Width || A.Y >= FrameBuffer->Height || B.X >= FrameBuffer->Width || B.Y >= FrameBuffer->Height || C.X >= FrameBuffer->Width || C.Y >= FrameBuffer->Height) {
    return;
  }

  i32 MinX = Min3(A.X, B.X, C.X);
  i32 MinY = Min3(A.Y, B.Y, C.Y);
  i32 MaxX = Max3(A.X, B.X, C.X);
  i32 MaxY = Max3(A.Y, B.Y, C.Y);

  MinX = Max(MinX, 0);
  MinY = Max(MinY, 0);
  MaxX = Min(MaxX, FrameBuffer->Width - 1);
  MaxY = Min(MaxY, FrameBuffer->Height - 1);

  v3 P = {0};
  for (P.Y = MinY; P.Y <= MaxY; P.Y++) {
    for (P.X = MinX; P.X <= MaxX; P.X++) {
      float W0 = 0.0f;
      float W1 = 0.0f;
      float W2 = 0.0f;
      Barycentric(P, A, B, C, &W0, &W1, &W2);
      float WSum = W0 + W1;

      // NOTE(lucas): Are we inside the triangle?
      if (W0 >= 0.0f && W1 >= 0.0f && W2 >= 0.0f && WSum <= 1.0f) {
        float Z = 0;
        Z += A.Z * W0;
        Z += B.Z * W1;
        Z += C.Z * W2;

        i32 Index = ((FrameBuffer->Height - P.Y) * FrameBuffer->Width) - P.X;
        if (ZBuffer[Index] < Z) {
          ZBuffer[Index] = Z;
          color Texel;
#if DRAW_SOLID
          Texel = COLOR(255, 255, 255);
#else
          v2 UV = Cartesian(
            T0, T1, T2,
            W0, W1, W2
          );
          i32 XCoord = (i32)Abs(Texture->Width * UV.U) % Texture->Width;
          i32 YCoord = (i32)Abs(Texture->Height * UV.V) % Texture->Height;
          Texel = RGBToBGR(&Texture->PixelBuffer[4 * ((YCoord * Texture->Width) + XCoord)]);
#endif
          Texel.R = Clamp(Texel.R * LightFactor, AMBIENT_LIGHT, 0xff);
          Texel.G = Clamp(Texel.G * LightFactor, AMBIENT_LIGHT, 0xff);
          Texel.B = Clamp(Texel.B * LightFactor, AMBIENT_LIGHT, 0xff);
          DrawPixel(FrameBuffer, P.X, P.Y, Texel);
        }
      }
    }
  }

#if DRAW_BOUNDING_BOX
  color LineColor = COLOR(50, 50, 255);

  DrawLine(FrameBuffer, V2(MinX, MinY), V2(MaxX, MinY), LineColor);
  DrawLine(FrameBuffer, V2(MinX, MaxY), V2(MaxX, MaxY), LineColor);
  DrawLine(FrameBuffer, V2(MinX, MinY), V2(MinX, MaxY), LineColor);
  DrawLine(FrameBuffer, V2(MaxX, MinY), V2(MaxX, MaxY), LineColor);
#endif

#if DRAW_BOUNDING_BOX_POINTS
  DrawRect(FrameBuffer, MinX, MinY, 4, 4, COLOR(50, 255, 50), BLEND_MODE_NORMAL);
  DrawRect(FrameBuffer, MaxX, MaxY, 4, 4, COLOR(255, 50, 50), BLEND_MODE_NORMAL);
#endif

#if DRAW_VERTICES
  DrawRect(FrameBuffer, A.X, A.Y, 2, 2, COLORA(255, 255, 255, 100), BLEND_MODE_ADD);
  DrawRect(FrameBuffer, B.X, B.Y, 2, 2, COLORA(255, 255, 255, 100), BLEND_MODE_ADD);
  DrawRect(FrameBuffer, C.X, C.Y, 2, 2, COLORA(255, 255, 255, 100), BLEND_MODE_ADD);
#endif
}

static void DrawMesh(framebuffer* FrameBuffer, float* ZBuffer, mesh* Mesh, image* Texture, v3 P, v3 Light) {
  mat4 Model = Translate(P);
  mat4 Mat = MultiplyMat4(Proj, Model);

  for (u32 Index = 0; Index < Mesh->IndexCount; Index += 3) {
    v3 V[3];  // Vertices
    v2 T[3];  // UV coords
    v3 R[3];  // Resulting transformed vertices
    v3 Normal;

    V[0] = Mesh->Vertices[Mesh->Indices[Index + 0]];
    V[1] = Mesh->Vertices[Mesh->Indices[Index + 1]];
    V[2] = Mesh->Vertices[Mesh->Indices[Index + 2]];

    T[0] = Mesh->UV[Mesh->UVIndices[Index + 0]];
    T[1] = Mesh->UV[Mesh->UVIndices[Index + 1]];
    T[2] = Mesh->UV[Mesh->UVIndices[Index + 2]];

    Normal = Mesh->Normals[Mesh->NormalIndices[Index + 0]];

    R[0] = MultiplyMatrixVector(Mat, V[0]);
    R[1] = MultiplyMatrixVector(Mat, V[1]);
    R[2] = MultiplyMatrixVector(Mat, V[2]);

    R[0].X += 1.0f; R[0].Y += 1.0f;
    R[1].X += 1.0f; R[1].Y += 1.0f;
    R[2].X += 1.0f; R[2].Y += 1.0f;

    R[0].X *= 0.5f * Win.Width; R[0].Y *= 0.5f * Win.Height;
    R[1].X *= 0.5f * Win.Width; R[1].Y *= 0.5f * Win.Height;
    R[2].X *= 0.5f * Win.Width; R[2].Y *= 0.5f * Win.Height;

    // TODO(lucas): Is this the correct way to calculate point light normals?
    v3 LightDelta = DifferenceV3(Light, R[0]);
    LightDelta.X = -LightDelta.X;
    float LightDistance = DistanceV3(Light, R[0]);
    v3 LightNormal = NormalizeVec3(LightDelta);
    float LightFactor = (1.0f / (1.0f + LightDistance)) * DotVec3(Normal, LightNormal) * LightStrength;

    v3 CameraNormal = V3(0, 0, -1.0f);
    float DotValue = DotVec3(CameraNormal, Normal);
    if (DotValue < 0.0f) {
      continue;
    }
    if (Degenerate(R[0], R[1], R[2])) {
      continue;
    }
    DrawFilledTriangle(FrameBuffer, ZBuffer, R[0], R[1], R[2], T[0], T[1], T[2], Texture, LightFactor);
  }
}

i32 RendererInit(u32 Width, u32 Height) {
  render_state* State = &RenderState;
  FrameBufferCreate(&State->FrameBuffer, Width, Height);
  State->ZBuffer = calloc(Width * Height, sizeof(float));

  WindowOpen(Width, Height, WINDOW_TITLE);
 
  Proj = Perspective(60, (float)Win.Width / Win.Height, 0.1f, 500);
  return 0;
}


static void RendererSwapBuffers() {
  WindowSwapBuffers(&RenderState.FrameBuffer);
}

static void RendererClear(u8 ColorR, u8 ColorG, u8 ColorB) {
  FrameBufferClear(&RenderState.FrameBuffer, COLOR(ColorR, ColorG, ColorB));
  ZBufferClear(RenderState.ZBuffer, RenderState.FrameBuffer.Width, RenderState.FrameBuffer.Height);
}

void RendererDestroy() {
  FrameBufferDestroy(&RenderState.FrameBuffer);
  free(RenderState.ZBuffer);
  WindowClose();
}
