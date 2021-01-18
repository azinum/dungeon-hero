// renderer_opengl.c

static render_state RenderState;
static i32 DefaultShader;
static i32 SkyboxShader;
static model CubeModel;

#define SKYBOX_SIZE 1.0f

float SkyboxVertices[] = {
	-SKYBOX_SIZE,  SKYBOX_SIZE, -SKYBOX_SIZE,
	-SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
	 SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
	 SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
	 SKYBOX_SIZE,  SKYBOX_SIZE, -SKYBOX_SIZE,
	-SKYBOX_SIZE,  SKYBOX_SIZE, -SKYBOX_SIZE,

	-SKYBOX_SIZE, -SKYBOX_SIZE,  SKYBOX_SIZE,
	-SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
	-SKYBOX_SIZE,  SKYBOX_SIZE, -SKYBOX_SIZE,
	-SKYBOX_SIZE,  SKYBOX_SIZE, -SKYBOX_SIZE,
	-SKYBOX_SIZE,  SKYBOX_SIZE,  SKYBOX_SIZE,
	-SKYBOX_SIZE, -SKYBOX_SIZE,  SKYBOX_SIZE,

	 SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
	 SKYBOX_SIZE, -SKYBOX_SIZE,  SKYBOX_SIZE,
	 SKYBOX_SIZE,  SKYBOX_SIZE,  SKYBOX_SIZE,
	 SKYBOX_SIZE,  SKYBOX_SIZE,  SKYBOX_SIZE,
	 SKYBOX_SIZE,  SKYBOX_SIZE, -SKYBOX_SIZE,
	 SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,

	-SKYBOX_SIZE, -SKYBOX_SIZE,  SKYBOX_SIZE,
	-SKYBOX_SIZE,  SKYBOX_SIZE,  SKYBOX_SIZE,
	 SKYBOX_SIZE,  SKYBOX_SIZE,  SKYBOX_SIZE,
	 SKYBOX_SIZE,  SKYBOX_SIZE,  SKYBOX_SIZE,
	 SKYBOX_SIZE, -SKYBOX_SIZE,  SKYBOX_SIZE,
	-SKYBOX_SIZE, -SKYBOX_SIZE,  SKYBOX_SIZE,

	-SKYBOX_SIZE,  SKYBOX_SIZE, -SKYBOX_SIZE,
	 SKYBOX_SIZE,  SKYBOX_SIZE, -SKYBOX_SIZE,
	 SKYBOX_SIZE,  SKYBOX_SIZE,  SKYBOX_SIZE,
	 SKYBOX_SIZE,  SKYBOX_SIZE,  SKYBOX_SIZE,
	-SKYBOX_SIZE,  SKYBOX_SIZE,  SKYBOX_SIZE,
	-SKYBOX_SIZE,  SKYBOX_SIZE, -SKYBOX_SIZE,

	-SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
	-SKYBOX_SIZE, -SKYBOX_SIZE,  SKYBOX_SIZE,
	 SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
	 SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
	-SKYBOX_SIZE, -SKYBOX_SIZE,  SKYBOX_SIZE,
	 SKYBOX_SIZE, -SKYBOX_SIZE,  SKYBOX_SIZE
};

#define ERR_BUFFER_SIZE 512

static void OutputZBufferToFile(render_state* RenderState, const char* Path) {
  (void)RenderState;
  (void)Path;
}

static void OutputFrameBufferToFile(render_state* RenderState, const char* Path) {
  (void)RenderState;
  (void)Path;
}

static void StoreAttrib(model* Model, i32 AttribNum, u32 Count, u32 Size, void* Data) {
  glEnableVertexAttribArray(AttribNum);

  glGenBuffers(1, &Model->VBO);
  glBindBuffer(GL_ARRAY_BUFFER, Model->VBO);
  glBufferData(GL_ARRAY_BUFFER, Size, Data, GL_STATIC_DRAW);
  glVertexAttribPointer(AttribNum, Count, GL_FLOAT, GL_FALSE, 0, (void*)0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static i32 UploadModel(model* Model, mesh* Mesh) {
  Model->DrawCount = Mesh->IndexCount;

  glGenVertexArrays(1, &Model->VAO);
  glBindVertexArray(Model->VAO);

  glGenBuffers(1, &Model->EBO);

  StoreAttrib(Model, 0, 3, Mesh->VertexCount * sizeof(float) * 3, &Mesh->Vertices[0]);
  StoreAttrib(Model, 1, 2, Mesh->UVCount * sizeof(float) * 2, &Mesh->UV[0]);
  StoreAttrib(Model, 2, 3, Mesh->NormalCount * sizeof(float) * 3, &Mesh->Normals[0]);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Model->EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, Mesh->IndexCount * sizeof(u32), &Mesh->Indices[0], GL_STATIC_DRAW);

  glBindVertexArray(0);
  return 0;
}

static i32 UploadModel2(model* Model, float* Vertices, u32 VertexCount) {
  Model->DrawCount = VertexCount / 3;

  glGenVertexArrays(1, &Model->VAO);
  glBindVertexArray(Model->VAO);

  glGenBuffers(1, &Model->VBO);
  glBindBuffer(GL_ARRAY_BUFFER, Model->VBO);
  glBufferData(GL_ARRAY_BUFFER, VertexCount * sizeof(*Vertices), Vertices, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);

  glVertexAttribPointer(0, 3, GL_FLOAT, 3 * sizeof(float), GL_FALSE, (void*)0);

  glBindVertexArray(0);
  return 0;
}

static i32 UploadAndIndexModel(model* Model, mesh* Mesh) {
  i32 IndexResult = MeshSortIndexedData(Mesh);
  if (IndexResult != 0) {
    return IndexResult;
  }
  return UploadModel(Model, Mesh);
}

static i32 UploadTexture(u32* TextureId, image* Texture) {
  glGenTextures(1, TextureId);
  glBindTexture(GL_TEXTURE_2D, *TextureId);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glGenerateMipmap(GL_TEXTURE_2D);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Texture->Width, Texture->Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, Texture->PixelBuffer);
  glBindTexture(GL_TEXTURE_2D, 0);
  return 0;
}

static i32 UploadCubemapTexture(u32* TextureId, image* Texture) {
  glGenTextures(1, TextureId);
  glBindTexture(GL_TEXTURE_CUBE_MAP, *TextureId);

  for (i32 Index = 0; Index < 6; ++Index) {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + Index, 0, GL_RGBA, Texture->Width, Texture->Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, Texture->PixelBuffer);
  }

  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  return 0;
}

// TODO(lucas): The top texture seems to be rotated wrong, is this an issue with the cubemapping or is it the skyboxes (textures) themselves? Fix!
static i32 UploadSkyboxTexture(u32* TextureId, u32 SkyboxId, assets* Assets) {
  glGenTextures(1, TextureId);
  glBindTexture(GL_TEXTURE_CUBE_MAP, *TextureId);

  for (i32 Index = 0; Index < 6; ++Index) {
    image* Texture = &Assets->Skyboxes[SkyboxId + Index];
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + Index, 0, GL_RGBA, Texture->Width, Texture->Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, Texture->PixelBuffer);
  }

  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  return 0;
}

static i32 ShaderCompile(const char* ShaderPath) {
  i32 Program = -1;
  u32 VertShader = 0;
  u32 FragShader = 0;

  char VertPath[MAX_PATH_SIZE] = {0};
  snprintf(VertPath, MAX_PATH_SIZE, "%s.vert", ShaderPath);
  const char* VertSource = ReadFileAndNullTerminate(VertPath);
  char FragPath[MAX_PATH_SIZE] = {0};
  snprintf(FragPath, MAX_PATH_SIZE, "%s.frag", ShaderPath);
  const char* FragSource = ReadFileAndNullTerminate(FragPath);
  if (!VertSource || !FragSource) {
    fprintf(stderr, "Failed to open shader file(s)\n");
    goto done;
  }

  i32 Report = -1;
  char ErrorLog[ERR_BUFFER_SIZE] = {0};

  VertShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(VertShader, 1, &VertSource, NULL);
  glCompileShader(VertShader);
{
  glGetShaderiv(VertShader, GL_COMPILE_STATUS, &Report);
  if (!Report) {
    glGetShaderInfoLog(VertShader, ERR_BUFFER_SIZE, NULL, ErrorLog);
    fprintf(stderr, "%s:%s\n", VertPath, ErrorLog);
    goto done;
  }
}

  FragShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(FragShader, 1, &FragSource, NULL);
  glCompileShader(FragShader);
{
  glGetShaderiv(FragShader, GL_COMPILE_STATUS, &Report);
  if (!Report) {
    glGetShaderInfoLog(FragShader, ERR_BUFFER_SIZE, NULL, ErrorLog);
    fprintf(stderr, "%s:%s\n", FragPath, ErrorLog);
    goto done;
  }
}

  Program = glCreateProgram();
  glAttachShader(Program, VertShader);
  glAttachShader(Program, FragShader);
  glLinkProgram(Program);

{
  glGetProgramiv(Program, GL_VALIDATE_STATUS, &Report);
  if (Report != GL_NO_ERROR) {
    glGetProgramInfoLog(Program, ERR_BUFFER_SIZE, NULL, ErrorLog);
    fprintf(stderr, "%s:%s\n", ShaderPath, ErrorLog);
    goto done;
  }
}

done:
  if (VertShader > 0) glDeleteShader(VertShader);
  if (FragShader > 0) glDeleteShader(FragShader);
  if (VertSource)     free((void*)VertSource);
  if (FragSource)     free((void*)FragSource);
  return Program;
}

#define DrawSimpleTexture2D(RenderState, X, Y, W, H, TEXTURE, TINT) \
  DrawTexture2D(RenderState, X, Y, W, H, 0, 0, 1, 1, TEXTURE, TINT)

inline void DrawTexture2D(render_state* RenderState, i32 X, i32 Y, i32 W, i32 H, float XOffset, float YOffset, float XRange, float YRange, image* Texture, color Tint) {
  // TODO(lucas): Do implement this thiiiinng!
  (void)RenderState; (void)X; (void)Y; (void)W; (void)H; (void)XOffset; (void)YOffset; (void)XRange; (void)YRange; (void)Texture; (void)Tint;
}

static void DrawMesh(render_state* RenderState, assets* Assets, u32 MeshId, u32 TextureId, v3 P, v3 Light, float Rotation, v3 Scaling, camera* Camera) {
  (void)Assets;
  u32 Handle = DefaultShader;
  glUseProgram(Handle);
  u32 Texture = RenderState->Textures[TextureId];

  Model = Translate(P);
  Model = MultiplyMat4(Model, Rotate(Rotation, V3(0, 1, 0)));
  Model = MultiplyMat4(Model, Scale(Scaling));

  // TODO(lucas): Do this once elsewhere!
  View = LookAt(Camera->P, AddToV3(Camera->P, Camera->Forward), Camera->Up);
  // View = InverseMat4(View);

  glUniformMatrix4fv(glGetUniformLocation(Handle, "Projection"), 1, GL_FALSE, (float*)&Projection);
  glUniformMatrix4fv(glGetUniformLocation(Handle, "View"), 1, GL_FALSE, (float*)&View);
  glUniformMatrix4fv(glGetUniformLocation(Handle, "Model"), 1, GL_FALSE, (float*)&Model);
  glUniform3f(glGetUniformLocation(Handle, "Light"), Light.X, Light.Y, Light.Z);

  model* Model = &RenderState->Models[MeshId];

  glBindVertexArray(Model->VAO);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, Texture);

  glDrawElements(GL_TRIANGLES, Model->DrawCount, GL_UNSIGNED_INT, 0);

  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);

  glBindVertexArray(0);
  glUseProgram(0);
}

static void OpenGLInit() {
  PlatformOpenGLInit();

  i32 GlewError = glewInit();
  if (GlewError != GLEW_OK) {
    fprintf(stderr, "Failed to initialize GLEW: %s\n", glewGetErrorString(GlewError));
    return;
  }

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glShadeModel(GL_FLAT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  glEnable(GL_TEXTURE_CUBE_MAP_EXT);
  glEnable(GL_TEXTURE_GEN_S);
  glEnable(GL_TEXTURE_GEN_R);
  glEnable(GL_TEXTURE_GEN_T);
  glEnable(GL_NORMALIZE);

  fprintf(stdout, "GL VENDOR:    %s\n", glGetString(GL_VENDOR));
  fprintf(stdout, "GL RENDERER:  %s\n", glGetString(GL_RENDERER));
  fprintf(stdout, "GL VERSION:   %s\n", glGetString(GL_VERSION));
  fprintf(stdout, "GLSL VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
}

i32 RendererInit(render_state* RenderState, assets* Assets) {
  OpenGLInit();
  RenderState->ModelCount = 0;
  RenderState->TextureCount = 0;

  if (G_CompatibleOpenGL) {
    DefaultShader = ShaderCompile("resource/shader/compat_default");
  }
  else {
    DefaultShader = ShaderCompile("resource/shader/default");
  }

  SkyboxShader = ShaderCompile("resource/shader/skybox");

  if (DefaultShader < 0 || SkyboxShader < 0) {
    fprintf(stderr, "Failed to load shader(s)\n");
    return -1;
  }

  // TODO(lucas): Figure out and explore how asset/resource loading and unloading should work
  // when it comes to using different renderers. Should we be able to switch rendering context, i.e. switch from software to hardware rendering, in run-time?
  // How will we handle resources in that case?
  for (u32 Index = 0; Index < Assets->MeshCount; ++Index) {
    mesh* Mesh = &Assets->Meshes[Index];
    model Model;
    UploadAndIndexModel(&Model, Mesh);
    RenderState->Models[Index] = Model;
    RenderState->ModelCount++;
  }
  for (u32 Index = 0; Index < Assets->TextureCount; ++Index) {
    image* Texture = &Assets->Textures[Index];
    u32 TextureId = 0;
    UploadTexture(&TextureId, Texture);
    RenderState->Textures[Index] = TextureId;
    RenderState->TextureCount++;
  }
  for (u32 Index = 0; Index < Assets->SkyboxCount / 6; ++Index) {
    u32 TextureId = 0;
    UploadSkyboxTexture(&TextureId, Index * 6, Assets);
    RenderState->Cubemaps[Index] = TextureId;
    RenderState->CubemapCount++;
  }
	UploadModel2(&CubeModel, SkyboxVertices, ARR_SIZE(SkyboxVertices));
  return 0;
}

static void RendererSwapBuffers() {
  WindowSwapBuffers(&RenderState);
}

static void RendererClear(u8 ColorR, u8 ColorG, u8 ColorB) {
  glClearColor(ColorR / 255.0f, ColorG / 255.0f, ColorB / 255.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void RendererUpdateBuffers(render_state* RenderState) {
  (void)RenderState;
  // TODO(lucas): Implement
}

static void DrawSkybox(render_state* RenderState, assets* Assets, camera* Camera, u32 TextureId) {
  (void)Assets;
  u32 Texture = RenderState->Cubemaps[TextureId];
  model* Model = &CubeModel;
  u32 Handle = SkyboxShader;
  glUseProgram(Handle);
  glDepthFunc(GL_LEQUAL);

  // TODO(lucas): Do this once elsewhere
  View = LookAt(Camera->P, AddToV3(Camera->P, Camera->Forward), Camera->Up);
  // NOTE(lucas): We translate the view to the origin here, this is so that we don't move outside the skybox
  View.Elements[3][0] = 0;
  View.Elements[3][1] = 0;
  View.Elements[3][2] = 0;

  glUniformMatrix4fv(glGetUniformLocation(Handle, "Projection"), 1, GL_FALSE, (float*)&Projection);
  glUniformMatrix4fv(glGetUniformLocation(Handle, "View"), 1, GL_FALSE, (float*)&View);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, Texture);

  glBindVertexArray(Model->VAO);
  glDrawArrays(GL_TRIANGLES, 0, Model->DrawCount);
  glBindVertexArray(0);

  glDepthFunc(GL_LESS);
  glUseProgram(0);
}

void RendererDestroy(render_state* RenderState) {
  glDeleteVertexArrays(1, &CubeModel.VAO);
  glDeleteVertexArrays(1, &CubeModel.VBO);
  CubeModel.DrawCount = 0;

  for (u32 Index = 0; Index < RenderState->ModelCount; ++Index) {
    model* Model = &RenderState->Models[Index];
    glDeleteVertexArrays(1, &Model->VAO);
    glDeleteVertexArrays(1, &Model->VBO);
    glDeleteBuffers(1, &Model->EBO);
  }
  RenderState->ModelCount = 0;

  for (u32 Index = 0; Index < RenderState->TextureCount; ++Index) {
    u32 TextureId = RenderState->Textures[Index];
    glDeleteTextures(1, &TextureId);
  }
  RenderState->TextureCount = 0;

  for (u32 Index = 0; Index < RenderState->CubemapCount; ++Index) {
    u32 TextureId = RenderState->Cubemaps[Index];
    glDeleteTextures(1, &TextureId);
  }
  RenderState->CubemapCount = 0;

  glDeleteShader(DefaultShader);
  glDeleteShader(SkyboxShader);
}
