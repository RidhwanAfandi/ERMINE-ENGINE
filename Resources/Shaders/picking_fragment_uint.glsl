#version 460 core
layout(location = 0) out uint outId;

uniform uint u_EntityId;

void main()
{
	outId = u_EntityId;
}