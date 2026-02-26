#version 460 core

// Depth pre-pass fragment shader
// No outputs needed - we only care about writing depth
// Fragment shader is required by OpenGL spec, but can be minimal

void main()
{
    // Empty - depth is written automatically by the pipeline
    // We could add alpha testing here if needed in the future
}
