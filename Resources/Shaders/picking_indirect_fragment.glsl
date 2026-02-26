#version 460 core

layout(location = 0) out uint outId;

// Entity ID from vertex shader (already encoded as +1)
flat in uint v_EntityID;

void main()
{
    outId = v_EntityID;
}
