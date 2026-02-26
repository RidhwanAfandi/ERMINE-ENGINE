#version 410
layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec3 vertex_colour;

uniform mat4 MVP;

out vec3 color;
// out vec2 tex_coord;

void main() {
  gl_Position = MVP * vec4(vertex_position, 1.0);
  color = vertex_colour;

  // tex_coord = vertex_tex;  
}