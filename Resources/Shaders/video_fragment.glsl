#version 450 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D tex_y;
uniform sampler2D tex_cb;
uniform sampler2D tex_cr;

void main()
{
    float y = texture(tex_y, vTexCoord).r;
    float cb = texture(tex_cb, vTexCoord).r;
    float cr = texture(tex_cr, vTexCoord).r;

    mat4 bt601 = mat4(
        1.16438,  0.00000,  1.59603, -0.87079,
        1.16438, -0.39176, -0.81297,  0.52959,
        1.16438,  2.01723,  0.00000, -1.08139,
        0.0,      0.0,      0.0,      1.0
    );

    vec3 rgb = (vec4(y, cb, cr, 1.0) * bt601).rgb;
    FragColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
