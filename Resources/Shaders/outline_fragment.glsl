#version 460 core
#extension GL_ARB_bindless_texture : require

in vec2 vUV;
out vec4 FragColor;

// Picking buffer handle (uint ID per pixel)
uniform uvec2 u_PickingHandle; // bindless
// Optional depth for thickness falloff (if desired)
uniform uvec2 u_DepthHandle;

// Selected id list (small count; fallback to SSBO if you expect large numbers)
uniform int  u_SelectedCount;
uniform uint u_SelectedIds[128]; // adjust max

uniform vec4 u_OutlineColor      = vec4(0.25, 0.6, 1.0, 1.0);
uniform float u_Thickness         = 1.5;  // in pixels
uniform int   u_Steps             = 2;    // radial steps for thicker outline
uniform float u_DepthFadeStart    = 200.0;
uniform float u_DepthFadeEnd      = 400.0;

bool IsSelected(uint id)
{
    if (id == 0) return false;
    for (int i = 0; i < u_SelectedCount; ++i)
        if (u_SelectedIds[i] == id) return true;
    return false;
}

void main()
{
    usampler2D picking = usampler2D(u_PickingHandle);
    ivec2 texSize = textureSize(picking, 0);
    ivec2 coord   = ivec2(vUV * texSize);

    uint centerID = texelFetch(picking, coord, 0).r;
    bool centerSelected = IsSelected(centerID);

    if (!centerSelected)
    {
        // Check neighbors to draw outline just outside selected area
        bool nearSelected = false;
        for (int dy = -1; dy <= 1 && !nearSelected; ++dy)
            for (int dx = -1; dx <= 1 && !nearSelected; ++dx)
            {
                ivec2 nc = coord + ivec2(dx, dy);
                if (nc.x < 0 || nc.y < 0 || nc.x >= texSize.x || nc.y >= texSize.y) continue;
                uint nid = texelFetch(picking, nc, 0).r;
                if (IsSelected(nid)) nearSelected = true;
            }
        if (!nearSelected) discard;
        centerSelected = true;
    }

    // Edge detection
    bool isEdge = false;
    int radiusMax = int(ceil(u_Thickness));
    for (int r = 1; r <= radiusMax && !isEdge; ++r)
    {
        for (int s = -r; s <= r && !isEdge; ++s)
        {
            ivec2 offsets[4] = { ivec2(s, r), ivec2(s, -r), ivec2(r, s), ivec2(-r, s) };
            for (int k = 0; k < 4 && !isEdge; ++k)
            {
                ivec2 nc = coord + offsets[k];
                if (nc.x < 0 || nc.y < 0 || nc.x >= texSize.x || nc.y >= texSize.y) { isEdge = true; break; }
                uint nid = texelFetch(picking, nc, 0).r;
                bool sel = IsSelected(nid);
                if (sel && nid == centerID) continue;
                isEdge = true;
            }
        }
    }
    if (!isEdge) discard;

    // Optional depth fade (only if a valid depth handle was provided)
    float alpha = u_OutlineColor.a;
    float fade  = 1.0;
    sampler2D depthTex = sampler2D(u_DepthHandle);
    float depth = texelFetch(depthTex, coord, 0).r;
    if (depth > u_DepthFadeStart)
        fade = 1.0 - clamp((depth - u_DepthFadeStart) / (u_DepthFadeEnd - u_DepthFadeStart), 0.0, 1.0);

    FragColor = vec4(u_OutlineColor.rgb, alpha * fade);
}