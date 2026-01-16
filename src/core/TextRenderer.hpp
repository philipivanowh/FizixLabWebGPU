#pragma once

#include <iostream>
#include <map>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <webgpu/webgpu.hpp>

#include "math/Vec2i.hpp"

struct Character
{
    WGPUTexture texture;
    WGPUTextureView textureView;
    math::Vec2i size;     // Size of glyph
    math::Vec2i bearing;  // Offset from baseline to left/top of glyph
    unsigned int advance; // Offset to advance to next glyph
};

class TextRenderer
{

public:
    bool Initialize_fonts(WGPUDevice device);

    std::map<char, Character> Characters;

private:
    void Terminate_fonts(FT_Library fontLibrary, FT_Face face);
};