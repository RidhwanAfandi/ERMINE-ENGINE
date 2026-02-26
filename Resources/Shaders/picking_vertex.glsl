#version 460 core
layout(location = 0) in vec3 aPos;

uniform mat4 u_LightViewProj;
uniform mat4 model;

void main()
{
	gl_Position = u_LightViewProj * model * vec4(aPos, 1.0);
}