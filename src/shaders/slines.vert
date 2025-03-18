#version 450
#pragma shader_stage(vertex)

layout(std140, set = 0, binding = 0) uniform matrix_state {
  mat4 projMatrix;
  mat4 viewMatrix;
  vec4 volextents;
  vec4 eyepos;
};

layout (location = 0) in vec3 posit;
layout (location = 1) in vec3 colvi;

layout (location = 0) out vec3 colvo;

void main()
{
  colvo = colvi;
  vec3 vpos = (viewMatrix * vec4(posit.xyz, 1.)).xyz;
  gl_Position = projMatrix * vec4(vpos.xyz, 1.);
}
