/* Start Header ************************************************************************/
/*!
\file       UITextRenderer.cpp
\author     Edwin Lee Zirui, edwinzirui.lee, 2301299, edwinzirui.lee@digipen.edu
\date       11/07/2025
\brief      Implementation of simple text rendering system for UI using stb_truetype.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "UITextRenderer.h"
#include "AssetManager.h"
#include "Logger.h"
#include <fstream>

// stb_truetype implementation
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../../ThirdParty/recastnavigation/RecastDemo/Contrib/stb_truetype.h"

namespace Ermine
{
    bool UITextRenderer::Initialize(const std::string& fontPath, int fontSize)
    {
        EE_CORE_INFO("UITextRenderer::Initialize - Starting initialization with font: '{}', size: {}", 
                     fontPath.empty() ? "NONE" : fontPath, fontSize);
        
        m_fontSize = fontSize;
        m_lineHeight = static_cast<float>(fontSize) / 720.0f; // Normalize to screen space

        if (!fontPath.empty())
        {
            // Check if font file exists first
            std::ifstream testFile(fontPath);
            if (!testFile.good())
            {
                EE_CORE_ERROR("Font file not found: {}", fontPath);
                EE_CORE_WARN("Creating fallback font for text rendering");
                CreateFallbackFont();
                return true; // Return true so text rendering continues with fallback
            }
            testFile.close();

            EE_CORE_INFO("Font file exists, attempting to generate font atlas...");
            if (GenerateFontAtlas(fontPath, fontSize))
            {
                EE_CORE_INFO("UITextRenderer initialized successfully with font: {}", fontPath);
                return true;
            }
            EE_CORE_WARN("Failed to generate font atlas from '{}', using fallback", fontPath);
        }
        else
        {
            EE_CORE_WARN("No font path provided, using fallback font");
        }

        // Use fallback font if no path provided or loading failed
        CreateFallbackFont();
        return true;
    }

    bool UITextRenderer::GenerateFontAtlas(const std::string& fontPath, int fontSize)
    {
        EE_CORE_INFO("GenerateFontAtlas - Reading font file: {}", fontPath);
        
        // Read font file
        std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            EE_CORE_ERROR("Failed to open font file: {}", fontPath);
            return false;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<unsigned char> fontBuffer(size);
        if (!file.read(reinterpret_cast<char*>(fontBuffer.data()), size))
        {
            EE_CORE_ERROR("Failed to read font file: {}", fontPath);
            return false;
        }

        EE_CORE_INFO("Font file read successfully ({} bytes), initializing stb_truetype...", size);

        // Initialize stb_truetype
        stbtt_fontinfo fontInfo;
        if (!stbtt_InitFont(&fontInfo, fontBuffer.data(), 0))
        {
            EE_CORE_ERROR("Failed to initialize font: {}", fontPath);
            return false;
        }

        EE_CORE_INFO("stb_truetype initialized, baking font to atlas...");

        // Atlas size (power of 2 for GPU)
        m_atlasWidth = 512;
        m_atlasHeight = 512;

        // Allocate bitmap for atlas
        std::vector<unsigned char> atlasData(m_atlasWidth * m_atlasHeight);

        // Bake font to atlas (ASCII printable characters: 32-126)
        const int firstChar = 32;
        const int numChars = 95; // 32 to 126 inclusive
        std::vector<stbtt_bakedchar> charData(numChars);

        int result = stbtt_BakeFontBitmap(
            fontBuffer.data(), 0,
            static_cast<float>(fontSize),
            atlasData.data(), m_atlasWidth, m_atlasHeight,
            firstChar, numChars,
            charData.data()
        );

        if (result <= 0)
        {
            EE_CORE_ERROR("stbtt_BakeFontBitmap failed with result: {}", result);
            return false;
        }

        EE_CORE_INFO("Font baked successfully, result: {} (negative value indicates largest character height)", result);

        // Convert grayscale to RGBA for OpenGL
        std::vector<unsigned char> rgbaData(m_atlasWidth * m_atlasHeight * 4);
        for (int i = 0; i < m_atlasWidth * m_atlasHeight; ++i)
        {
            rgbaData[i * 4 + 0] = 255;              // R
            rgbaData[i * 4 + 1] = 255;              // G
            rgbaData[i * 4 + 2] = 255;              // B
            rgbaData[i * 4 + 3] = atlasData[i];     // A (font alpha)
        }

        EE_CORE_INFO("Creating OpenGL texture for font atlas...");

        // Create OpenGL texture
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_atlasWidth, m_atlasHeight, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, rgbaData.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, 0);

        // Store texture ID for later use
        m_fontTextureID = textureID;

        EE_CORE_INFO("OpenGL texture created (ID: {}), storing glyph data...", textureID);

        // Store glyph data
        m_glyphs.clear();
        float invWidth = 1.0f / m_atlasWidth;
        float invHeight = 1.0f / m_atlasHeight;

        for (int i = 0; i < numChars; ++i)
        {
            char c = static_cast<char>(firstChar + i);
            const auto& baked = charData[i];

            CharGlyph glyph;
            glyph.u0 = baked.x0 * invWidth;
            glyph.v0 = baked.y0 * invHeight; 
            glyph.u1 = baked.x1 * invWidth;
            glyph.v1 = baked.y1 * invHeight;  

            // Normalize to screen space (assuming 1280x720 reference)
            glyph.width = (baked.x1 - baked.x0) / 1280.0f;
            glyph.height = (baked.y1 - baked.y0) / 720.0f;
            glyph.xOffset = baked.xoff / 1280.0f;
            glyph.yOffset = -baked.yoff / 720.0f;  // Negate because stb uses Y-down, OpenGL uses Y-up
            glyph.xAdvance = baked.xadvance / 1280.0f;

            m_glyphs[c] = glyph;
        }

        EE_CORE_INFO("Font atlas generated successfully: {}x{}, {} characters stored", 
                     m_atlasWidth, m_atlasHeight, numChars);
        return true;
    }

    void UITextRenderer::CreateFallbackFont()
    {
        EE_CORE_INFO("CreateFallbackFont - Creating simple 5x7 bitmap fallback font...");
        
        // Simple 5x7 bitmap font (8x8 cell size) for 95 printable ASCII characters (32-126)
        // We'll arrange them in a 16x6 grid (96 characters total, leaving last slot empty)
        const int cellWidth = 8;
        const int cellHeight = 8;
        const int charsPerRow = 16;
        const int charRows = 6;

        m_atlasWidth = cellWidth * charsPerRow;   // 128 pixels
        m_atlasHeight = cellHeight * charRows;     // 48 pixels

        std::vector<unsigned char> atlasData(m_atlasWidth * m_atlasHeight * 4, 0);

        // Simple 5x7 font data for each ASCII character (32-126)
        // 1 = white pixel, 0 = transparent pixel
        // This is a very basic monospace font
        auto drawChar = [&](int charIndex, const std::vector<std::string>& pattern)
        {
            int col = charIndex % charsPerRow;
            int row = charIndex / charsPerRow;
            int startX = col * cellWidth + 1;  // +1 for spacing
            int startY = row * cellHeight + 1;  // +1 for spacing

            for (size_t py = 0; py < pattern.size() && py < 7; ++py)
            {
                for (size_t px = 0; px < pattern[py].length() && px < 5; ++px)
                {
                    if (pattern[py][px] == '1')
                    {
                        int idx = ((startY + py) * m_atlasWidth + (startX + px)) * 4;
                        atlasData[idx + 0] = 255;   // R
                        atlasData[idx + 1] = 255;   // G
                        atlasData[idx + 2] = 255;   // B
                        atlasData[idx + 3] = 255;   // A
                    }
                }
            }
        };

        EE_CORE_INFO("Drawing fallback font characters...");

        // Draw all printable ASCII characters (simple 5x7 patterns)
        // Space (32)
        drawChar(0, {"00000", "00000", "00000", "00000", "00000", "00000", "00000"});
        // ! (33)
        drawChar(1, {"00100", "00100", "00100", "00100", "00000", "00100", "00000"});
        // Letters A-Z (uppercase) - simplified patterns
        // A (65) - index 33
        drawChar(33, {"01110", "10001", "10001", "11111", "10001", "10001", "00000"});
        // B (66)
        drawChar(34, {"11110", "10001", "11110", "10001", "10001", "11110", "00000"});
        // C (67)
        drawChar(35, {"01110", "10001", "10000", "10000", "10001", "01110", "00000"});
        // For remaining characters, use simple filled rectangles as placeholders
        for (int i = 2; i < 95; ++i)
        {
            if (i == 33 || i == 34 || i == 35) continue; // Skip A, B, C (already drawn)
            // Draw a simple 3x5 rectangle for each character
            drawChar(i, {"11100", "10100", "10100", "10100", "11100", "00000", "00000"});
        }

        EE_CORE_INFO("Creating OpenGL texture for fallback font...");

        // Create texture from atlas
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_atlasWidth, m_atlasHeight, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, atlasData.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_fontTextureID = textureID;

        EE_CORE_INFO("Fallback font texture created (ID: {}), storing glyph mappings...", textureID);

        // Create glyph mapping (5x7 characters in 8x8 cells)
        for (char c = 32; c < 127; ++c)
        {
            int idx = c - 32;
            int col = idx % charsPerRow;
            int row = idx / charsPerRow;

            CharGlyph glyph;
            glyph.u0 = (col * cellWidth) / static_cast<float>(m_atlasWidth);
            glyph.v0 = (row * cellHeight) / static_cast<float>(m_atlasHeight); 
            glyph.u1 = ((col + 1) * cellWidth) / static_cast<float>(m_atlasWidth);
            glyph.v1 = ((row + 1) * cellHeight) / static_cast<float>(m_atlasHeight); 

            // Normalize to screen space (assuming 1920x1080 reference for better readability)
            glyph.width = 8.0f / 1920.0f;
            glyph.height = 8.0f / 1080.0f;
            glyph.xOffset = 0.0f;
            glyph.yOffset = 0.0f;
            glyph.xAdvance = 9.0f / 1920.0f;  // Slightly more than width for spacing

            m_glyphs[c] = glyph;
        }

        EE_CORE_INFO("Fallback font created successfully: {}x{}, 95 characters", 
                     m_atlasWidth, m_atlasHeight);
        EE_CORE_WARN("Using fallback font (simple 5x7 bitmap font) - consider providing a valid TTF font for better text quality");
    }

    void UITextRenderer::RenderText(std::shared_ptr<graphics::Shader> shader,
        const std::string& text,
        float x, float y,
        float scale,
        const Vec3& color,
        float alpha,
        GLuint vao, GLuint vbo)
    {
        if (m_fontTextureID == 0 || m_glyphs.empty())
        {
            EE_CORE_ERROR("UITextRenderer::RenderText - Font not initialized! (textureID: {}, glyphs: {})",
                         m_fontTextureID, m_glyphs.size());
            return;
        }

        if (!shader || !shader->IsValid())
        {
            EE_CORE_ERROR("UITextRenderer::RenderText - Invalid shader!");
            return;
        }

        // Log first render call (only once)
        static bool firstRender = true;
        if (firstRender)
        {
            EE_CORE_INFO("UITextRenderer::RenderText - First render call");
            EE_CORE_INFO("  Text: '{}', Position: ({}, {}), Scale: {}", text, x, y, scale);
            EE_CORE_INFO("  Color: ({}, {}, {}), Alpha: {}", color.x, color.y, color.z, alpha);
            EE_CORE_INFO("  Font texture ID: {}, Glyph count: {}", m_fontTextureID, m_glyphs.size());
            EE_CORE_INFO("  VAO: {}, VBO: {}", vao, vbo);
            firstRender = false;
        }

        // Bind VAO and VBO for text rendering
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        // Enable texture mode in shader
        shader->SetUniform1i("uUseTexture", 1);

        // Bind font texture to texture unit 0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_fontTextureID);
        shader->SetUniform1i("uTexture", 0);

        float cursorX = x;
        float cursorY = y;

        int renderedGlyphs = 0;
        int missingGlyphs = 0;

        for (char c : text)
        {
            if (c == '\n')
            {
                cursorX = x;
                cursorY -= m_lineHeight * scale;
                continue;
            }

            auto it = m_glyphs.find(c);
            if (it == m_glyphs.end())
            {
                missingGlyphs++;
                continue; // Skip unknown characters
            }

            const CharGlyph& glyph = it->second;

            // Calculate quad position
            // yOffset points to TOP of glyph, so we calculate top first, then subtract height for bottom
            float x0 = cursorX + glyph.xOffset * scale;
            float x1 = x0 + glyph.width * scale;
            float y1 = cursorY + glyph.yOffset * scale;           // Top = baseline + offset to top
            float y0 = y1 - glyph.height * scale;                 // Bottom = top - height

            // Build vertex data (2D position, RGBA color, UV)
            // Note: V coordinates are swapped because stb_truetype has v0 at top, OpenGL has v=0 at bottom
            float vertices[] = {
                // Position       // Color                        // UV (V flipped)
                x0, y0,          color.x, color.y, color.z, alpha,  glyph.u0, glyph.v1,  // bottom-left uses v1
                x1, y0,          color.x, color.y, color.z, alpha,  glyph.u1, glyph.v1,  // bottom-right uses v1
                x1, y1,          color.x, color.y, color.z, alpha,  glyph.u1, glyph.v0,  // top-right uses v0

                x0, y0,          color.x, color.y, color.z, alpha,  glyph.u0, glyph.v1,  // bottom-left uses v1
                x1, y1,          color.x, color.y, color.z, alpha,  glyph.u1, glyph.v0,  // top-right uses v0
                x0, y1,          color.x, color.y, color.z, alpha,  glyph.u0, glyph.v0   // top-left uses v0
            };

            // Submit to GPU
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            // Advance cursor
            cursorX += glyph.xAdvance * scale;
            renderedGlyphs++;
        }

        // Log summary after first render
        if (renderedGlyphs > 0 && missingGlyphs > 0)
        {
            EE_CORE_WARN("UITextRenderer::RenderText - Rendered {} glyphs, {} missing glyphs",
                        renderedGlyphs, missingGlyphs);
        }

        // Cleanup: unbind texture and disable texture mode
        glBindTexture(GL_TEXTURE_2D, 0);
        shader->SetUniform1i("uUseTexture", 0);
        glBindVertexArray(0);
    }

    float UITextRenderer::GetTextWidth(const std::string& text, float scale) const
    {
        float width = 0.0f;
        for (char c : text)
        {
            auto it = m_glyphs.find(c);
            if (it != m_glyphs.end())
            {
                width += it->second.xAdvance * scale;
            }
        }
        return width;
    }

    const CharGlyph* UITextRenderer::GetGlyph(char c) const
    {
        auto it = m_glyphs.find(c);
        if (it != m_glyphs.end())
            return &it->second;
        return nullptr;
    }

} // namespace Ermine
