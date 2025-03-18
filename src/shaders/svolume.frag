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

layout (location = 0) in vec3 texco3;
layout (location = 1) in vec3 nrmrayvec;

layout (location = 0) out vec4 fragdata;

void main()
{
  uint cut_planes_count = uint(peeling.y);
  float accumulated = 0.f;

  vec3 current = texco3;
  vec3 delta = nrmrayvec * peeling.x;
  vec3 accumulated_colour = vec3(0.f, 0.f, 0.f);

  for (int i = 0; i < cut_planes_count; i++)
  {
    if ((any(lessThan(current.xyz, volboundmin.xyz))) ||
        (any(greaterThan(current.xyz, volboundmax.xyz))))
    {
      fragdata = vec4(accumulated_colour, accumulated);
      return;
    }

  vec3 volcentre = (current - vec3(0.5, 0.5, 0.5)) * volvoxelsize.xyz;
  float hex_coords_x = (volcentre.x / sqrt(3.0))*2.0;
  float hex_coords_y = volcentre.y - 0.5*hex_coords_x;
  float hex_coords_w = -(hex_coords_x + hex_coords_y);
  float hex_coords_z = volcentre.z;

  vec4 hex_coords =
    vec4(hex_coords_x, hex_coords_y, hex_coords_w, hex_coords_z);
  vec4 r_hex_coords = round(hex_coords);
  vec3 frac_hex_coords = abs(r_hex_coords.xyz - hex_coords.xyz);

  if ((frac_hex_coords.x > frac_hex_coords.y) &&
      (frac_hex_coords.x > frac_hex_coords.z))
  {
    r_hex_coords.x = -(r_hex_coords.y + r_hex_coords.z);
  } else if (frac_hex_coords.y > frac_hex_coords.z)
  {
    r_hex_coords.y = -(r_hex_coords.x + r_hex_coords.z);
  } else
  {
    r_hex_coords.z = -(r_hex_coords.x + r_hex_coords.y);
  }

  vec3 int_coords = r_hex_coords.xyw + 0.5*volvoxelsize.xyz;

  vec4 tsample = texture(acoustic_volume, round(int_coords) / volvoxelsize.xyz);

    vec4 hue4 = colouring_matrix * vec4(
      max(0.0, -tsample.r),
      abs(tsample.r),
      max(0.0, tsample.r),
      1.f - abs(tsample.r));

    vec3 hue = hue4.xyz;

    float amplitude = abs(tsample.r);

    float a_value = clamp(
      (amplitude - cubic_components.z) * (cubic_components.x / cubic_components.y)
        + cubic_components.w,
      0.0, 1.0);

    float accum_factor = (1.f - accumulated) * a_value;
    accumulated_colour += (accum_factor * hue);
    accumulated += accum_factor;

    current -= delta;

  } // i

  fragdata = vec4(accumulated_colour, accumulated);
}
