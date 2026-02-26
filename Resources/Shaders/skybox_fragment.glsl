#version 460

in vec3 TexCoords;

out vec4 FragColor;

uniform samplerCube skybox;
uniform float exposure = 1.0;
uniform float gamma = 2.2;

void main()
{    
    vec3 envColor = texture(skybox, TexCoords).rgb;
    
    // Tone mapping
    envColor = vec3(1.0) - exp(-envColor * exposure);
    
    // Gamma correction
    envColor = pow(envColor, vec3(1.0/gamma));
    
    FragColor = vec4(envColor, 1.0);
}