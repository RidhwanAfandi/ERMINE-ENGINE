#version 450 core

// Input from vertex shader
in vec4 vColor;
in vec2 vTexCoord;

// Output color
out vec4 FragColor;

// Uniforms
uniform sampler2D uTexture;      // Texture sampler
uniform bool uUseTexture;        // Toggle between textured and color-only rendering
uniform float u_Gamma = 1.0;     // Gamma correction for preview image (1.0 = no effect)

void main()
{
    if (uUseTexture)
    {
        // Sample texture and multiply by color (for tinting)
        vec4 texColor = texture(uTexture, vTexCoord);
        FragColor = texColor * vColor;

        // Apply gamma correction (only active for gamma preview image)
        if (u_Gamma != 1.0)
        {
            FragColor.rgb = pow(FragColor.rgb, vec3(1.0 / u_Gamma));
        }
    }
    else
    {
        // Use solid color only (backward compatibility)
        FragColor = vColor;
    }
}
