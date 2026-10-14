#pragma once

#include <vector>
#include <cstdint>
#include <cstdio>

#include <Windows.h>

#include "../thirdparty/stb_image.h"

// CPU side image, RGBA8 with the top row first. Same order D3D expects, so no vertical flip is needed
struct Image {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> pixels;
};

inline Image LoadImageFile(const char* path) {
    Image image;

    int width, height, channels;
    stbi_uc* data = stbi_load(path, &width, &height, &channels, 4);

    if (!data) {
        char buffer[512];
        sprintf_s(buffer, "Couldn't load image %s: %s\n", path, stbi_failure_reason());
        OutputDebugStringA(buffer);
        return image;
    }

    image.width = width;
    image.height = height;
    image.pixels.assign(data, data + size_t(width) * height * 4);

    stbi_image_free(data);
    return image;
}
