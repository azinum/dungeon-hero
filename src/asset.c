// asset.c

void AssetsInit(assets* Assets) {
  memset(Assets, 0, sizeof(assets));
  Assets->MeshCount = 0;
  Assets->TextureCount = 0;
  Assets->SkyboxCount = 0;
}

void AssetsLoadAll(assets* Assets) {
  char Path[MAX_PATH_SIZE] = {0};
  AssetsInit(Assets);

  for (u32 Index = 0; Index < MAX_MESH; ++Index) {
    snprintf(Path, MAX_PATH_SIZE, "%s/%s.obj", MESH_PATH, MeshFileNames[Index]);
    mesh Mesh;
    MeshInit(&Mesh);
    if (MeshLoadOBJ(Path, &Mesh) != 0) {
      continue;
    }
    Assets->Meshes[Index] = Mesh;
    ++Assets->MeshCount;
  }

  for (u32 Index = 0; Index < MAX_TEXTURE; ++Index) {
    snprintf(Path, MAX_PATH_SIZE, "%s/%s.png", TEXTURE_PATH, TextureFileNames[Index]);
    image Texture;
    if (LoadImage(Path, &Texture) != 0) {
      continue;
    }
    Assets->Textures[Index] = Texture;
    ++Assets->TextureCount;
  }

  for (u32 Index = 0; Index < MAX_SKYBOX_TEXTURE; ++Index) {
    snprintf(Path, MAX_PATH_SIZE, "%s/%s.png", TEXTURE_PATH, SkyboxFileNames[Index]);
    image Texture;
    if (LoadImage(Path, &Texture) != 0) {
      continue;
    }
    Assets->Skyboxes[Index] = Texture;
    ++Assets->SkyboxCount;
  }

  for (u32 Index = 0; Index < MAX_AUDIO; ++Index) {
    // TODO(lucas): Load audio files
  }
}

void AssetsUnloadAll(assets* Assets) {
  for (u32 Index = 0; Index < Assets->MeshCount; ++Index) {
    mesh* Mesh = &Assets->Meshes[Index];
    MeshUnload(Mesh);
  }
  for (u32 Index = 0; Index < Assets->TextureCount; ++Index) {
    image* Texture = &Assets->Textures[Index];
    UnloadImage(Texture);
  }
  for (u32 Index = 0; Index < Assets->SkyboxCount; ++Index) {
    image* Texture = &Assets->Skyboxes[Index];
    UnloadImage(Texture);
  }
  for (u32 Index = 0; Index < MAX_AUDIO; ++Index) {
    // TODO(lucas): Unload audio files
  }
}
