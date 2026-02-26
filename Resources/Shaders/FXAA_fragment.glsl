#version 450 core

in vec2 TexCoord;
out vec4 FragColor;

// Input texture
uniform sampler2D u_LightingTexture;

// FXAA parameters
uniform int u_FXAA = 1;
uniform float u_FXAASpanMax = 8.0;
uniform float u_FXAAReduceMin = 1.0/128.0;
uniform float u_FXAAReduceMul = 1.0/8.0;

vec3 applyFXAA(sampler2D tex, vec2 texCoord)
{
    vec2 inverseVP = 1.0 / textureSize(tex, 0);
    
    // Sample neighboring pixels
    vec3 rgbNW = texture(tex, texCoord + vec2(-1.0, -1.0) * inverseVP).rgb;
    vec3 rgbNE = texture(tex, texCoord + vec2(1.0, -1.0) * inverseVP).rgb;
    vec3 rgbSW = texture(tex, texCoord + vec2(-1.0, 1.0) * inverseVP).rgb;
    vec3 rgbSE = texture(tex, texCoord + vec2(1.0, 1.0) * inverseVP).rgb;
    vec3 rgbM = texture(tex, texCoord).rgb;
    
    // Convert to luma
    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM = dot(rgbM, luma);
    
    // Find luma range
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    
    // Calculate edge direction
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * u_FXAAReduceMul), u_FXAAReduceMin);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    
    dir = min(vec2(u_FXAASpanMax), max(vec2(-u_FXAASpanMax), dir * rcpDirMin)) * inverseVP;
    
    // Sample along the edge
    vec3 rgbA = 0.5 * (
        texture(tex, texCoord + dir * (1.0/3.0 - 0.5)).rgb +
        texture(tex, texCoord + dir * (2.0/3.0 - 0.5)).rgb);
    
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(tex, texCoord + dir * -0.5).rgb +
        texture(tex, texCoord + dir * 0.5).rgb);
    
    float lumaB = dot(rgbB, luma);
    
    // Return appropriate sample based on luma range
    if((lumaB < lumaMin) || (lumaB > lumaMax))
        return rgbA;
    else
        return rgbB;
}

void main()
{
    if (u_FXAA == 0) {
        FragColor = texture(u_LightingTexture, TexCoord);
        return;
    }
    vec3 finalColor = applyFXAA(u_LightingTexture, TexCoord);
    FragColor = vec4(finalColor, 1.0);
}