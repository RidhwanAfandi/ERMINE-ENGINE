#version 450 core

// Vertex attributes
layout(location = 0) in vec2 aPosition;  // Normalized screen position (0-1)
layout(location = 1) in vec4 aColor;     // RGBA color
layout(location = 2) in vec2 aTexCoord;  // Texture coordinates (0-1)

// Output to fragment shader
out vec4 vColor;
out vec2 vTexCoord;

// Uniforms
uniform mat4 projection;

void main()
{
    // Transform position using orthographic projection
    gl_Position = projection * vec4(aPosition, 0.0, 1.0);

    // Pass color and texture coordinates to fragment shader
    vColor = aColor;
    vTexCoord = aTexCoord;
}
