#version 430

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec3 ViewPos;

out vec4 FragColor;

// Lighting mode toggle
uniform bool isBlinnPhong = true;

// PBR Material properties
struct PBRMaterial {
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
};

// Standard Material properties (for Blinn-Phong)
struct Material {
    vec3 Ka;
    vec3 Kd; 
    vec3 Ks;
    float Shininess;
};

const int NUM_CASCADES = 4;

struct LightData {
    vec4 position_type;     // xyz = position (world space), w = type
    vec4 color_intensity;   // rgb = color, a = intensity
    vec4 direction_range;   // xyz = direction (world space), w = range
    vec4 spot_angles_castshadows_startOffset;
    mat4 lightSpaceMatrix[NUM_CASCADES];
    mat4 pointLightMatrices[6];
    vec4 splitDepths[(NUM_CASCADES + 3) / 4];
};

layout(std430, binding = 4) restrict readonly buffer LightsSSBO { // Matches LIGHT_SSBO_BINDING in SSBO_Bindings.h
    vec4 lightCount;
    LightData lights[];
};

uniform PBRMaterial pbrMaterial = PBRMaterial(vec3(0.5, 0.5, 0.5), 0.0, 0.5, 1.0);
uniform Material material = Material(vec3(0.2), vec3(0.9), vec3(0.8), 100.0);

const float PI = 3.14159265359;

// PBR Functions
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 calculatePBR(vec3 N, vec3 V)
{
    // FIXED: Use pure color from material properties instead of automatic texture sampling
    vec3 albedo = pbrMaterial.albedo;
    float metallic = pbrMaterial.metallic;
    float roughness = pbrMaterial.roughness;
    float ao = pbrMaterial.ao;
    
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    vec3 Lo = vec3(0.0);
    
    // Calculate lighting for each light
    int numLights = int(lightCount.x);
    for(int i = 0; i < numLights; ++i) {
        vec3 lightPos = lights[i].position_type.xyz;
        vec3 lightColor = lights[i].color_intensity.rgb;
        float lightIntensity = lights[i].color_intensity.a;
        
        vec3 L = normalize(lightPos - ViewPos);
        vec3 H = normalize(V + L);
        float distance = length(lightPos - ViewPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = lightColor * lightIntensity * attenuation;
        
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo;
    
    // Gamma correction
    color = pow(color, vec3(1.0/2.2));
    return color;
}

vec3 calculateBlinnPhong(vec3 N, vec3 V)
{
    // FIXED: Use pure color from material properties instead of automatic texture sampling
    vec3 result = vec3(0.0);
    
    // Calculate lighting for each light
    int numLights = int(lightCount.x);
    for(int i = 0; i < numLights; ++i) {
        vec3 lightPos = lights[i].position_type.xyz;
        vec3 lightColor = lights[i].color_intensity.rgb;
        float lightIntensity = lights[i].color_intensity.a;
        
        // Ambient
        vec3 ambient = material.Ka * lightColor * lightIntensity;
        
        // Diffuse
        vec3 lightDir = normalize(lightPos - ViewPos);
        float diff = max(dot(N, lightDir), 0.0);
        vec3 diffuse = material.Kd * diff * lightColor * lightIntensity;
        
        // Specular (Blinn-Phong)
        vec3 halfwayDir = normalize(lightDir + V);
        float spec = pow(max(dot(N, halfwayDir), 0.0), material.Shininess);
        vec3 specular = material.Ks * spec * lightColor * lightIntensity;
        
        result += ambient + diffuse + specular;
    }
    
    // REMOVED: Automatic texture multiplication - now uses pure material colors
    
    // Gamma correction
    result = pow(result, vec3(1.0/2.2));
    return result;
}

void main()
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(-ViewPos); // ViewPos is position in view space
    
    vec3 color;
    if (isBlinnPhong) {
        color = calculateBlinnPhong(N, V);
    } else {
        color = calculatePBR(N, V);
    }
    
    FragColor = vec4(color, 1.0);
}
