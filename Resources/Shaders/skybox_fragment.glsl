#version 460

in vec3 TexCoords;

out vec4 FragColor;

uniform samplerCube skybox;

void main()
{    
    vec3 envColor = texture(skybox, TexCoords).rgb;

    // Keep skybox output in scene-linear space so the final post-process pass
    // tone-maps the fully composed image once.
    FragColor = vec4(envColor, 1.0);
}
