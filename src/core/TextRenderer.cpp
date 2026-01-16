#include "core/TextRenderer.hpp"
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

bool TextRenderer::Initialize_fonts(WGPUDevice device)
{
    FT_Library fontLibrary;
    if (FT_Init_FreeType(&fontLibrary))
    {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return false;
    }

    FT_Face face;
    if (FT_New_Face(fontLibrary, "fonts/arial.ttf", 0, &face))
    {
        std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, 48);

    for (unsigned char c = 0; c < 128; c++)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
            continue;
        }

        // Create WebGPU texture
        WGPUTextureDescriptor textureDesc = {};
        textureDesc.size = {face->glyph->bitmap.width, face->glyph->bitmap.rows, 1};
        textureDesc.format = WGPUTextureFormat_R8Unorm;
        textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        textureDesc.dimension = WGPUTextureDimension_2D;
        textureDesc.mipLevelCount = 1;
        textureDesc.sampleCount = 1;

        WGPUTexture texture = wgpuDeviceCreateTexture(device, &textureDesc);

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

    return true;
}

void TextRenderer::Terminate_fonts(FT_Library fontLibrary, FT_Face face)
{
    FT_Done_Face(face);
    FT_Done_FreeType(fontLibrary);
}