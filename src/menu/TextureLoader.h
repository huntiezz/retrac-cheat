#pragma once
#include <d3d11.h>
#include "../menu/imgui/stb_image.h"

bool LoadTextureFromMemory(ID3D11Device* device, const unsigned char* data, size_t size, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);
