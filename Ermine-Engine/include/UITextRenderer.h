/* Start Header ************************************************************************/
/*!
\file       UITextRenderer.h
\author     Edwin Lee Zirui, edwinzirui.lee, 2301299, edwinzirui.lee@digipen.edu
\date       11/07/2025
\brief      Simple text rendering system for UI elements using bitmap font atlas.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"
#include "Shader.h"
#include "MathVector.h"
#include <glad/glad.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace Ermine
{
    struct CharGlyph
    {
        float u0, v0, u1, v1;  // UV coordinates in atlas
        float width, height;    // Size in normalized screen space
        float xOffset, yOffset; // Offset from baseline
        float xAdvance;         // Horizontal advance
    };

    /*!***********************************************************************
    \brief
        Simple text rendering system for UI using a bitmap font atlas.
        Uses stb_truetype to generate font texture at runtime.
    *************************************************************************/
    class UITextRenderer
    {
    public:
        /*!***********************************************************************
        \brief
            Initializes the text renderer with a default font.
        \param[in] fontPath
            Path to TTF font file (optional, uses default if empty).
        \param[in] fontSize
            Font size in pixels (default: 32).
        \return
            True if initialization succeeded, false otherwise.
        *************************************************************************/
        bool Initialize(const std::string& fontPath = "", int fontSize = 32);

        /*!***********************************************************************
        \brief
            Renders text at the specified position.
        \param[in] shader
            UI shader to use for rendering (must be bound before calling).
        \param[in] text
            String to render.
        \param[in] x
            X position in normalized screen coordinates (0-1).
        \param[in] y
            Y position in normalized screen coordinates (0-1).
        \param[in] scale
            Text scale multiplier (default: 1.0).
        \param[in] color
            RGB color values (0-1).
        \param[in] alpha
            Alpha transparency (0-1, default 1.0).
        \param[in] vao
            OpenGL VAO to use for rendering.
        \param[in] vbo
            OpenGL VBO to use for rendering.
        *************************************************************************/
        void RenderText(std::shared_ptr<graphics::Shader> shader,
                       const std::string& text,
                       float x, float y,
                       float scale,
                       const Vec3& color,
                       float alpha,
                       GLuint vao, GLuint vbo);

        /*!***********************************************************************
        \brief
            Calculates the width of a text string.
        \param[in] text
            String to measure.
        \param[in] scale
            Text scale multiplier.
        \return
            Width in normalized screen coordinates.
        *************************************************************************/
        float GetTextWidth(const std::string& text, float scale) const;

        /*!***********************************************************************
        \brief
            Gets the font atlas texture OpenGL ID.
        \return
            OpenGL texture ID for the font atlas.
        *************************************************************************/
        GLuint GetFontTextureID() const { return m_fontTextureID; }

        /*!***********************************************************************
        \brief
            Gets glyph data for a character.
        \param[in] c
            Character to get glyph for.
        \return
            Pointer to glyph data, or nullptr if not found.
        *************************************************************************/
        const CharGlyph* GetGlyph(char c) const;

    private:
        /*!***********************************************************************
        \brief
            Generates font atlas using stb_truetype.
        \param[in] fontPath
            Path to TTF font file.
        \param[in] fontSize
            Font size in pixels.
        \return
            True if generation succeeded.
        *************************************************************************/
        bool GenerateFontAtlas(const std::string& fontPath, int fontSize);

        /*!***********************************************************************
        \brief
            Creates a simple fallback bitmap font (ASCII characters).
        *************************************************************************/
        void CreateFallbackFont();

        GLuint m_fontTextureID = 0;
        std::unordered_map<char, CharGlyph> m_glyphs;
        int m_fontSize = 32;
        float m_lineHeight = 0.0f;
        int m_atlasWidth = 0;
        int m_atlasHeight = 0;
    };

} // namespace Ermine
