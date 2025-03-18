#version 450
#pragma shader_stage(fragment)

layout(std140, set = 0, binding = 1) uniform volume_state {
  mat4 colouring_matrix;
  vec4 cubic_components;
  vec4 volboundmin;
  vec4 volboundmax;
  vec4 volvoxelsize;
  vec4 peeling;
  vec4 eyepos;
};
layout (set = 1, binding = 0) uniform sampler3D acoustic_volume;

layout (location = 0) in vec3 colvo;

layout (location = 0) out vec4 fragdata;

void main()
{
  fragdata = vec4(colvo, 1.0);
}
