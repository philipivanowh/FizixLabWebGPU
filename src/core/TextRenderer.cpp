
#include "core/TextRenderer.hpp"
#include <iostream>
#include <map>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <webgpu/webgpu.hpp>

/*
bool TextRenderer::Initialize_fonts(WGPUDevice device)
{
    FT_Library fontLibrary;
    if (FT_Init_FreeType(&fontLibrary))
    {
        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return false;
    }

    FT_Face face;
    if (FT_New_Face(fontLibrary, "fonts/NeoEuler-VGO00.otf", 0, &face))
    {
        if (FT_New_Face(fontLibrary, path, 0, &face) == 0)
        {
            std::cout << "Successfully loaded font from: " << path << std::endl;
            fontLoaded = true;
            break;
        }
    }
    
if (!fontLoaded)
{
    std::cerr << "ERROR::FREETYPE: Failed to load font from any path" << std::endl;
    std::cerr << "Tried the following paths:" << std::endl;
    for (const char* path : fontPaths)
    {
        std::cerr << "  - " << path << std::endl;
    }
    FT_Done_FreeType(fontLibrary);
    return false;
}

    FT_Set_Pixel_Sizes(face, 0, 48);

    for (unsigned char c = 0; c < 128; c++)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            std::cerr << "ERROR::FREETYPE: Failed to load Glyph '" << c << "'" << std::endl;
            continue;
        }

        // Skip characters with no bitmap (like space)
        if (face->glyph->bitmap.width == 0 || face->glyph->bitmap.rows == 0)
        {
            Character character = {
                nullptr,
                nullptr,
                math::Vec2i(0, 0),
                math::Vec2i(face->glyph->bitmap_left, face->glyph->bitmap_top),
                static_cast<unsigned int>(face->glyph->advance.x)
            };
            Characters.insert(std::pair<char, Character>(c, character));
            continue;
        }

        // Create WebGPU texture
        WGPUTextureDescriptor textureDesc = {};
        textureDesc.label = nullptr;
        textureDesc.size = {face->glyph->bitmap.width, face->glyph->bitmap.rows, 1};
        textureDesc.format = WGPUTextureFormat_R8Unorm;
        textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        textureDesc.dimension = WGPUTextureDimension_2D;
        textureDesc.mipLevelCount = 1;
        textureDesc.sampleCount = 1;

        WGPUTexture texture = wgpuDeviceCreateTexture(device, &textureDesc);
        
        if (!texture)
        {
            std::cerr << "ERROR: Failed to create texture for character '" << c << "'" << std::endl;
            continue;
        }

        // Upload glyph bitmap data to texture
        WGPUImageCopyTexture destination = {};
        destination.texture = texture;
        destination.mipLevel = 0;
        destination.origin = {0, 0, 0};
        destination.aspect = WGPUTextureAspect_All;

        WGPUTextureDataLayout dataLayout = {};
        dataLayout.offset = 0;
        dataLayout.bytesPerRow = face->glyph->bitmap.width;
        dataLayout.rowsPerImage = face->glyph->bitmap.rows;

        WGPUExtent3D writeSize = {face->glyph->bitmap.width, face->glyph->bitmap.rows, 1};

        wgpuQueueWriteTexture(
            wgpuDeviceGetQueue(device),
            &destination,
            face->glyph->bitmap.buffer,
            face->glyph->bitmap.width * face->glyph->bitmap.rows,
            &dataLayout,
            &writeSize
        );

        // Create texture view
        WGPUTextureViewDescriptor viewDesc = {};
        viewDesc.label = nullptr;
        viewDesc.format = WGPUTextureFormat_R8Unorm;
        viewDesc.dimension = WGPUTextureViewDimension_2D;
        viewDesc.baseMipLevel = 0;
        viewDesc.mipLevelCount = 1;
        viewDesc.baseArrayLayer = 0;
        viewDesc.arrayLayerCount = 1;
        viewDesc.aspect = WGPUTextureAspect_All;

        WGPUTextureView textureView = wgpuTextureCreateView(texture, &viewDesc);

        Character character = {
            texture,
            textureView,
            math::Vec2i(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            math::Vec2i(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        
        Characters.insert(std::pair<char, Character>(c, character));
    }

    Terminate_fonts(fontLibrary, face);
    
    std::cout << "Successfully initialized " << Characters.size() << " characters" << std::endl;
    return true;
}

    // Rest of your initialization code...
void TextRenderer::Terminate_fonts(FT_Library fontLibrary, FT_Face face)
{
    FT_Done_Face(face);
    FT_Done_FreeType(fontLibrary);
}
    */