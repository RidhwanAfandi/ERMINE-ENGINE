#pragma once

#define NUM_CASCADES 4u
#define SHADOW_MAX_LAYERS_DESIRED 16u // 2MB per layer for 1024x1024 (16 layers = 32MB) (about 4 lights with shadows, each with 4 cascades)
extern unsigned int SHADOW_MAX_LAYERS; // defined in Renderer.cpp
#define SHADOW_MAP_RESOLUTION 1024u 
#define SHADOW_MAP_ARRAY_LAMBDA 0.7f
#define SHADOW_MAP_REFRESH_INTERVAL_IN_FRAMES 1 // 1 in x frames