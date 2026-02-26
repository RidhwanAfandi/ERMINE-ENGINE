#version 410

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out vec3 ViewPos; // Position in view space for lighting calculations

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 NormalMatrix; //  already calculate this in Renderer.cpp

void main()
{
    // Calculate positions
    vec4 viewPos = view * model * vec4(aPos, 1.0);
    ViewPos = viewPos.xyz;
    
    gl_Position = projection * viewPos;
    TexCoord = aTexCoord;

    // Transform normal to view space ( already calculate NormalMatrix)
    Normal = normalize(NormalMatrix * aNormal);

    // Calculate fragment position in world space for potential future use
    FragPos = vec3(model * vec4(aPos, 1.0));
}