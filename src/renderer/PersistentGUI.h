#pragma once

#include <memory>
#include <mutex>
#include <chrono>

#include "SwapchainData.h"
#include "BufferData.h"
#include "FrameData.h"

#define DESIRED_FRAME_RATE_PER_SECOND 20

struct Scene
{
  // Camera location and direction
  float eyepos[3];
  float centrepos[3];
  float upvec[3];

  // Matrices
  float persp_matrix[16];
  float lookat_matrix[16];
  float ortho_matrix[16];
  float identity_matrix[16];

  // Shader details and controls
  float cubic_components[4];
  // Set point for shader transparency?
  float setting_value;
  // Increment for corner movement
  float global_incr;
  // Volume extents
  float volume_bound_x_min;
  float volume_bound_y_min;
  float volume_bound_z_min;
  float volume_bound_x_max;
  float volume_bound_y_max;
  float volume_bound_z_max;
  // Highlighted corner controlled
  int location_show;

  // Copy of volume dimensions in voxels.
  float *volextents;
  int volume_xsze;
  int volume_ysze;
  int volume_zsze;

  // Next volume texture number
  int next_volume_texture__descriptor_set;
  // Next frame uniform number
  int next_uniforms__descriptor_set;

  // Volume vertex buffer
  float volm_vertices[6 * 6 * 8];
  // Line vertex buffer
  float line_vertices[8 * 3 * 2 * 8];

  // Current sampler type
  int sampler_current;
};

struct SceneBuffers
{
  inline SceneBuffers(
    vk::raii::PhysicalDevice const & physical_device,
    vk::raii::Device const &device,
    vk::raii::DescriptorSetLayout const &descriptor_set_layout_uniform)
    : vertex_uniforms(nullptr)
    , fragment_uniforms(nullptr)
    , lines_vertex_buffer(nullptr)
    , volume_vertex_buffer(nullptr)
  {
    vertex_uniforms = std::move(
      BufferData(
        physical_device, device,
        sizeof(float) * (16*2 + 4*2),
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent
      )
    );
    fragment_uniforms = std::move(
      BufferData(
        physical_device, device,
        sizeof(float) * (16*1 + 4*5),
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent
      )
    );
    lines_vertex_buffer = std::move(
      BufferData(
        physical_device, device,
        // 8 corners each 3 lines each 2 points each 8 floats
        sizeof(float) * (8*3*2*8),
        vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent
      )
    );
    volume_vertex_buffer = std::move(
      BufferData(
        physical_device, device,
        // 6 faces each 2 triangles each 3 points each 8 floats
        sizeof(float) * (6*2*3*8),
        vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent
      )
    );

    vk::DescriptorPoolSize descriptor_pool_size(
      vk::DescriptorType::eUniformBuffer, 2);

    vk::DescriptorPoolCreateInfo descriptor_pool_info(
      vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      1, 1,
      &descriptor_pool_size
    );

    _descriptor_pool = std::make_unique<vk::raii::DescriptorPool>(
      device, descriptor_pool_info
    );

    vk::DescriptorSetAllocateInfo descriptor_set_allocate_info(
      **_descriptor_pool, *descriptor_set_layout_uniform
    );

    _descriptor_set = std::make_unique<vk::raii::DescriptorSet>(
      std::move(
        vk::raii::DescriptorSets(
          device,
          descriptor_set_allocate_info).front()
      )
    );

    vk::DescriptorBufferInfo descriptor_buffer_info__vertex(
      vertex_uniforms._buffer,
      0,
      VK_WHOLE_SIZE
    );
    vk::DescriptorBufferInfo descriptor_buffer_info__fragment(
      fragment_uniforms._buffer,
      0,
      VK_WHOLE_SIZE
    );

    vk::WriteDescriptorSet write_descriptor_set__vertex(
      *_descriptor_set,
      0, // shader bind location
      0, 1, vk::DescriptorType::eUniformBuffer,
      nullptr,
      &descriptor_buffer_info__vertex);
    vk::WriteDescriptorSet write_descriptor_set__fragment(
      *_descriptor_set,
      1, // shader bind location
      0, 1, vk::DescriptorType::eUniformBuffer,
      nullptr,
      &descriptor_buffer_info__fragment);

    std::vector<vk::WriteDescriptorSet> descriptor_set_writes =
      { write_descriptor_set__vertex, write_descriptor_set__fragment };

    device.updateDescriptorSets(descriptor_set_writes, nullptr);
  }

  inline SceneBuffers(std::nullptr_t)
    : vertex_uniforms(nullptr)
    , fragment_uniforms(nullptr)
    , lines_vertex_buffer(nullptr)
    , volume_vertex_buffer(nullptr)
  {}

  inline void syncBufferWithScene(
    vk::raii::Device const &device,
    Scene &scene)
  {
    // Vertex uniforms
    float vertex_uniforms_array[16*2 + 4*2];

    // projMatrix
    std::memcpy(
      &(vertex_uniforms_array[0*16]),
      &(scene.persp_matrix[0]),
      sizeof(float) * 16);
    // viewMatrix
    std::memcpy(
      &(vertex_uniforms_array[1*16]),
      &(scene.lookat_matrix[0]),
      sizeof(float) * 16);
    vertex_uniforms_array[2*16 + 0] = float(scene.volume_xsze);
    vertex_uniforms_array[2*16 + 1] = float(scene.volume_ysze);
    vertex_uniforms_array[2*16 + 2] = float(scene.volume_zsze);
    vertex_uniforms_array[2*16 + 3] = 0.f; // padding
    std::memcpy(
      &(vertex_uniforms_array[2*16 + 1*4]),
      &(scene.eyepos[0]),
      sizeof(float) * 3);
    vertex_uniforms_array[2*16 + 1*4 + 3] = 0.f; // padding

    vertex_uniforms.upload(
      device,
      &(vertex_uniforms_array[0]),
      sizeof(vertex_uniforms_array));

    // Fragment uniforms
    float fragment_uniforms_array[16*1 + 4*6];

    // Normalised camera to eye vector
    const float F_un_x = scene.centrepos[0] - scene.eyepos[0];
    const float F_un_y = scene.centrepos[1] - scene.eyepos[1];
    const float F_un_z = scene.centrepos[2] - scene.eyepos[2];

    const float F_sqrt = sqrt(F_un_x*F_un_x + F_un_y*F_un_y + F_un_z*F_un_z);
    const float F_rcp_sqrt = (F_sqrt > 0.f) ? (1.f / F_sqrt) : 0.f;

    const float F_x = F_un_x * F_rcp_sqrt;
    const float F_y = F_un_y * F_rcp_sqrt;
    const float F_z = F_un_z * F_rcp_sqrt;

    float min_dp = +std::numeric_limits<float>::max();
    float max_dp = -std::numeric_limits<float>::max();

    // Find 8 extremal points
    // Determine minimal and maximal cutting planes
    for (size_t k = 0; k < 2; k++)
    {
      float ak = k - 0.5;

      float z_diff = ak - scene.eyepos[2];
      for (size_t j = 0; j < 2; j++)
      {
        float aj = j - 0.5;

        float y_diff = aj - scene.eyepos[1];
        for (size_t i = 0; i < 2; i++)
        {
          float ai = i - 0.5;

          float x_diff = ai - scene.eyepos[0];

          // Dot product this vector

          float this_dp = x_diff*F_x + y_diff*F_y + z_diff*F_z;

          min_dp = (min_dp > this_dp) ? this_dp : min_dp;
          max_dp = (max_dp < this_dp) ? this_dp : max_dp;
        }
      }
    }

    // Uses the first 16 elements.
    float *colouring_matrix = &(fragment_uniforms_array[0]);
    colouring_matrix[0] = 1.f;
    colouring_matrix[1] = 0.f;
    colouring_matrix[2] = 0.f;
    colouring_matrix[3] = 0.f;

    colouring_matrix[4] = 0.f;
    colouring_matrix[5] = 1.f;
    colouring_matrix[6] = 0.f;
    colouring_matrix[7] = 0.f;

    colouring_matrix[8] = 0.f;
    colouring_matrix[9] = 0.f;
    colouring_matrix[10] = 1.f;
    colouring_matrix[11] = 0.f;

    colouring_matrix[12] = 0.f;
    colouring_matrix[13] = 0.f;
    colouring_matrix[14] = 0.f;
    colouring_matrix[15] = 0.f;

    // Want to decide how many cut planes to have in the middle.
    int cut_planes =
      (scene.volume_xsze > scene.volume_ysze) ?
        scene.volume_xsze : scene.volume_ysze;
    cut_planes =
      (scene.volume_zsze > cut_planes) ?
        scene.volume_zsze : cut_planes;

    // cubic_components
    std::memcpy(
      &(fragment_uniforms_array[16*1 + 0*4]),
      &(scene.cubic_components[0]),
      sizeof(float) * 4);
    fragment_uniforms_array[20 + 0*4 + 0] =
      scene.volume_bound_x_min / float(scene.volume_xsze);
    fragment_uniforms_array[20 + 0*4 + 1] =
      scene.volume_bound_y_min / float(scene.volume_ysze);
    fragment_uniforms_array[20 + 0*4 + 2] =
      scene.volume_bound_z_min / float(scene.volume_zsze);
    fragment_uniforms_array[20 + 0*4 + 3] = 0.f; // padding

    fragment_uniforms_array[20 + 1*4 + 0] =
      scene.volume_bound_x_max / float(scene.volume_xsze);
    fragment_uniforms_array[20 + 1*4 + 1] =
      scene.volume_bound_y_max / float(scene.volume_ysze);
    fragment_uniforms_array[20 + 1*4 + 2] =
      scene.volume_bound_z_max / float(scene.volume_zsze);
    fragment_uniforms_array[20 + 1*4 + 3] = 0.f; // padding

    fragment_uniforms_array[20 + 2*4 + 0] =
      float(scene.volume_xsze);
    fragment_uniforms_array[20 + 2*4 + 1] =
      float(scene.volume_ysze);
    fragment_uniforms_array[20 + 2*4 + 2] =
      float(scene.volume_zsze);
    fragment_uniforms_array[20 + 2*4 + 3] = 0.f; // padding

    fragment_uniforms_array[20 + 3*4 + 0] =
      (max_dp - min_dp) / float(cut_planes);
    fragment_uniforms_array[20 + 3*4 + 1] = float(cut_planes);
    fragment_uniforms_array[20 + 3*4 + 2] = 0.f; // padding
    fragment_uniforms_array[20 + 3*4 + 3] = 0.f; // padding

    std::memcpy(
      &(fragment_uniforms_array[20 + 4*4 + 0]),
      &(scene.eyepos[0]),
      sizeof(float) * 3);

    fragment_uniforms_array[20 + 4*4 + 3] = 0.f; // padding

    fragment_uniforms.upload(
      device,
      &(fragment_uniforms_array[0]),
      sizeof(fragment_uniforms_array));

    // lines_vertex_buffer
    lines_vertex_buffer.upload(
      device,
      &(scene.line_vertices[0]),
      sizeof(float) * 8*3*2*8
    );

    // lines_vertex_buffer
    volume_vertex_buffer.upload(
      device,
      &(scene.volm_vertices[0]),
      sizeof(float) * 6*3*2*8
    );
  }

  std::unique_ptr<vk::raii::DescriptorPool> _descriptor_pool;
  std::unique_ptr<vk::raii::DescriptorSet> _descriptor_set;

  BufferData vertex_uniforms;
  BufferData fragment_uniforms;
  BufferData lines_vertex_buffer;
  BufferData volume_vertex_buffer;
};

struct PersistentGUI
{
  PersistentGUI(nullptr_t)
    : graphics_queue(nullptr)
    , graphics_queue_mutex_ptr(nullptr)
    , present_queue(nullptr)
    , present_queue_mutex_ptr(nullptr)
    , swapchain_data(nullptr)
    , depth_buffer_data(nullptr)
    , descriptor_set_layout_base(nullptr)
    , pipeline_layout(nullptr)
    , slines_vertex(nullptr)
    , slines_fragment(nullptr)
    , svolume_vertex(nullptr)
    , svolume_fragment(nullptr)
    , render_pass(nullptr)
    , line_pipeline(nullptr)
    , volume_pipeline(nullptr)
    , frame_pool(nullptr)
    , frames_until_end_of_transition(0)
    , last_frame_rendered()
  {
    std::memset(&scene, 0, sizeof(scene));
  }

  std::shared_ptr<vk::raii::Queue> graphics_queue;
  std::mutex *graphics_queue_mutex_ptr;

  std::shared_ptr<vk::raii::Queue> present_queue;
  std::mutex *present_queue_mutex_ptr;

  SwapchainData swapchain_data;
  DepthBufferData depth_buffer_data;

  vk::raii::DescriptorSetLayout descriptor_set_layout_base;
  vk::raii::PipelineLayout pipeline_layout;

  std::unique_ptr<vk::raii::ShaderModule> slines_vertex;
  std::unique_ptr<vk::raii::ShaderModule> slines_fragment;
  std::unique_ptr<vk::raii::ShaderModule> svolume_vertex;
  std::unique_ptr<vk::raii::ShaderModule> svolume_fragment;

  vk::raii::RenderPass render_pass;
  std::vector<vk::raii::Framebuffer> framebuffers;

  vk::raii::Pipeline line_pipeline;
  vk::raii::Pipeline volume_pipeline;

  Scene scene;
  FramePool frame_pool;

  int frames_until_end_of_transition;
  std::chrono::steady_clock::time_point last_frame_rendered;
};

void initialise_gui(
  const std::shared_ptr<VolumeBuffers> &volume_buffers,
  std::shared_ptr<vkch::Context> &vkch_ctxt,
  AuxiliaryVulkanContext *aux_ctxt,
  SimulationParameters const &simulation_parameters,
  struct PersistentGUI &persistent_gui);

void resizeWindow(
  std::shared_ptr<vkch::Context> &vkch_ctxt,
  AuxiliaryVulkanContext *aux_ctxt,
  struct PersistentGUI &persistent_gui);
