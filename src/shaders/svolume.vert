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

layout (location = 0) out vec3 texco3;
layout (location = 1) out vec3 nrmrayvec;

void main()
{
  float maxextent = max(max(volextents.x, volextents.y), volextents.z);
  vec3 realposit = posit.xyz * (volextents.xyz / maxextent);
  vec3 fakeeyepos = eyepos.xyz / (volextents.xyz / maxextent);
  texco3 = posit.xyz + vec3(0.5, 0.5, 0.5);
  nrmrayvec = normalize(fakeeyepos.xyz - posit.xyz);
  vec3 vpos = (viewMatrix * vec4(realposit, 1.)).xyz;
  gl_Position = projMatrix * vec4(vpos.xyz, 1.);
}
