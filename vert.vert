#version 460 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec4 color;

layout (location = 0) out vec4 out_color;

void main()
{
  gl_Position = vec4(position, 1.0);
  out_color = color;
}