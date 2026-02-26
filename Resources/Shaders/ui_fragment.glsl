#version 450 core

// Input from vertex shader
in vec4 vColor;
in vec2 vTexCoord;

// Output color
out vec4 FragColor;

// Uniforms
uniform sampler2D uTexture;      // Texture sampler
uniform bool uUseTexture;        // Toggle between textured and color-only rendering

void main()
{
    if (uUseTexture)
    {
        // Sample texture and multiply by color (for tinting)
        vec4 texColor = texture(uTexture, vTexCoord);
        FragColor = texColor * vColor;
    }
    else
    {
        // Use solid color only (backward compatibility)
        FragColor = vColor;
    }
}
