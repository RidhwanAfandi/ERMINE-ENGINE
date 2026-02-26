#version 460

layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    // Flip the Y coordinate to correct upside-down skybox
    TexCoords = vec3(aPos.x, -aPos.y, aPos.z);
    
    // Remove translation from view matrix for skybox
    mat4 skyboxView = mat4(mat3(view));
    vec4 pos = projection * skyboxView * vec4(aPos, 1.0);
    
    // Set z to w so that the skybox is always at the farthest depth
    gl_Position = pos.xyww;
}