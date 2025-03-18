
#include "PersistentGUI.h"
#include "gui.h"

#include "shader_headers/slines.vert.spv.h"
#include "shader_headers/slines.frag.spv.h"
#include "shader_headers/svolume.vert.spv.h"
#include "shader_headers/svolume.frag.spv.h"

void initialise_scene(
  struct Scene &scene,
  size_t t_xsze,
  size_t t_ysze,
  size_t t_zsze)
{
  scene.volume_xsze = 0;
  scene.volume_ysze = 0;
  scene.volume_zsze = 0;
  scene.cubic_components[0] = 1.f;
  scene.cubic_components[1] = 1.f;
  scene.cubic_components[2] = 1.f;
  scene.cubic_components[3] = 0.1f;
  scene.setting_value = 0;
  scene.location_show = true;

  scene.eyepos[0] = 0.f;
  scene.eyepos[1] = 3.f;
  scene.eyepos[2] = 0.f;
  scene.centrepos[0] = 0.f;
  scene.centrepos[1] = 0.f;
  scene.centrepos[2] = 0.f;
  scene.upvec[0] = 0.f;
  scene.upvec[1] = 0.f;
  scene.upvec[2] = 1.f;

  memset(&(scene.identity_matrix[0]), 0, sizeof(float) * 16);
  scene.identity_matrix[0] = 1.f;
  scene.identity_matrix[5] = 1.f;
  scene.identity_matrix[10] = 1.f;
  scene.identity_matrix[15] = 1.f;

  scene.volume_xsze = t_xsze;
  scene.volume_ysze = t_ysze;
  scene.volume_zsze = t_zsze;

  scene.volume_bound_x_min = 0;
  scene.volume_bound_y_min = 0;
  scene.volume_bound_z_min = 0;
  scene.volume_bound_x_max = scene.volume_xsze;
  scene.volume_bound_y_max = scene.volume_ysze;
  scene.volume_bound_z_max = scene.volume_zsze;

  scene.global_incr = 1;
}

void initialise_gui(
  const std::shared_ptr<VolumeBuffers> &volume_buffers,
  std::shared_ptr<vkch::Context> &vkch_ctxt,
  AuxiliaryVulkanContext *aux_ctxt,
  SimulationParameters const &simulation_parameters,
  struct PersistentGUI &persistent_gui)
{
  persistent_gui.graphics_queue = nullptr;
  persistent_gui.graphics_queue_mutex_ptr = nullptr;

  if (aux_ctxt->graphics_queue_family_index ==
    vkch_ctxt->computeQueueFamilyIndex())
  {
    persistent_gui.graphics_queue = vkch_ctxt->computeQueueFamilyQueue();
    persistent_gui.graphics_queue_mutex_ptr = vkch_ctxt->computeQueueMutex();
  } else
  {
    for (int k = 0; k < vkch_ctxt->auxiliaryQueueCount(); k++)
    {
      if (aux_ctxt->graphics_queue_family_index ==
        vkch_ctxt->auxiliaryQueueFamilyIndex(k))
      {
        persistent_gui.graphics_queue =
          vkch_ctxt->auxiliaryQueueFamilyQueue(k);
        persistent_gui.graphics_queue_mutex_ptr =
          vkch_ctxt->auxiliaryQueueMutex(k);
      }

    } // k
  }

  persistent_gui.present_queue = nullptr;
  persistent_gui.present_queue_mutex_ptr = nullptr;

  if (aux_ctxt->present_queue_family_index ==
    vkch_ctxt->computeQueueFamilyIndex())
  {
    persistent_gui.present_queue = vkch_ctxt->computeQueueFamilyQueue();
    persistent_gui.present_queue_mutex_ptr = vkch_ctxt->computeQueueMutex();
  } else
  {
    for (int k = 0; k < vkch_ctxt->auxiliaryQueueCount(); k++)
    {
      if (aux_ctxt->present_queue_family_index ==
        vkch_ctxt->auxiliaryQueueFamilyIndex(k))
      {
        persistent_gui.present_queue =
          vkch_ctxt->auxiliaryQueueFamilyQueue(k);
        persistent_gui.present_queue_mutex_ptr =
          vkch_ctxt->auxiliaryQueueMutex(k);
      }

    } // k
  }

  SDL_ShowWindow(aux_ctxt->window);

  vk::Extent2D extent = getExtentFromWindow(aux_ctxt->window);
  persistent_gui.swapchain_data = std::move(
    SwapchainData(
      vkch_ctxt->physical_device(),
      vkch_ctxt->device(),
      aux_ctxt->surface,
      extent,
      vk::ImageUsageFlagBits::eColorAttachment,
      nullptr,
      aux_ctxt->graphics_queue_family_index.first,
      aux_ctxt->present_queue_family_index.first
    )
  );

  persistent_gui.depth_buffer_data = std::move(
    DepthBufferData(
      vkch_ctxt->physical_device(),
      vkch_ctxt->device(),
      vk::Format::eD16Unorm,
      persistent_gui.swapchain_data.extent
    )
  );

  persistent_gui.descriptor_set_layout_base = std::move(
    vk::raii::DescriptorSetLayout(
      makeDescriptorSetLayout(
        vkch_ctxt->device(), {
          { vk::DescriptorType::eUniformBuffer, 1,
            vk::ShaderStageFlagBits::eVertex },
          { vk::DescriptorType::eUniformBuffer, 1,
            vk::ShaderStageFlagBits::eFragment }
        }, vk::DescriptorSetLayoutCreateFlags()
      )
    )
  );

  std::vector<vk::DescriptorSetLayout> descriptor_set_layouts =
    { *(persistent_gui.descriptor_set_layout_base),
      *(volume_buffers->read_descriptor_set_layout()) };
  persistent_gui.pipeline_layout = std::move(
    vk::raii::PipelineLayout(
      vkch_ctxt->device(),
      vk::PipelineLayoutCreateInfo(
        vk::PipelineLayoutCreateFlags(),
        descriptor_set_layouts
      )
    )
  );

  std::vector<uint32_t> spirv_slines_vertex(
    &(shader__slines_vert[0]),
    &(shader__slines_vert[0]) + (
      sizeof(shader__slines_vert) /
        sizeof(shader__slines_vert[0])
    )
  );
  std::vector<uint32_t> spirv_slines_fragment(
    &(shader__slines_frag[0]),
    &(shader__slines_frag[0]) + (
      sizeof(shader__slines_frag) /
        sizeof(shader__slines_frag[0])
    )
  );
  std::vector<uint32_t> spirv_svolume_vertex(
    &(shader__svolume_vert[0]),
    &(shader__svolume_vert[0]) + (
      sizeof(shader__svolume_vert) /
        sizeof(shader__svolume_vert[0])
    )
  );
  std::vector<uint32_t> spirv_svolume_fragment(
    &(shader__svolume_frag[0]),
    &(shader__svolume_frag[0]) + (
      sizeof(shader__svolume_frag) /
        sizeof(shader__svolume_frag[0])
    )
  );

  persistent_gui.slines_vertex =
    std::make_unique<vk::raii::ShaderModule>(
      vkch_ctxt->device(), vk::ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(), spirv_slines_vertex)
    );
  persistent_gui.slines_fragment =
    std::make_unique<vk::raii::ShaderModule>(
      vkch_ctxt->device(), vk::ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(), spirv_slines_fragment)
    );
  persistent_gui.svolume_vertex =
    std::make_unique<vk::raii::ShaderModule>(
      vkch_ctxt->device(), vk::ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(), spirv_svolume_vertex)
    );
  persistent_gui.svolume_fragment =
    std::make_unique<vk::raii::ShaderModule>(
      vkch_ctxt->device(), vk::ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(), spirv_svolume_fragment)
    );

  std::vector<vk::SurfaceFormatKHR> formats =
    vkch_ctxt->physical_device().getSurfaceFormatsKHR(
      aux_ctxt->surface);

  if (formats.empty())
  {
    throw std::runtime_error(
      "No valid format available for the Vulkan-accessible surface");
  }

  vk::Format colour_format = pickSurfaceFormat(formats).format;

  persistent_gui.render_pass = makeRenderPass(
    vkch_ctxt->device(), colour_format,
    persistent_gui.depth_buffer_data.format, vk::AttachmentLoadOp::eClear,
    vk::ImageLayout::ePresentSrcKHR);

  persistent_gui.framebuffers = std::move(
    std::vector<vk::raii::Framebuffer>(
      makeFramebuffers(vkch_ctxt->device(), persistent_gui.render_pass,
      persistent_gui.swapchain_data.image_views,
      &(persistent_gui.depth_buffer_data.image_view),
      persistent_gui.swapchain_data.extent)
    )
  );

  persistent_gui.line_pipeline = std::move(
    vk::raii::Pipeline(
      makeGraphicsPipeline(
        vkch_ctxt->device(),
        *(persistent_gui.slines_vertex), nullptr,
        *(persistent_gui.slines_fragment), nullptr,
        sizeof(float) * 8,
        vk::PrimitiveTopology::eLineList,
        {
          { vk::Format::eR32G32B32Sfloat, 0 },
          { vk::Format::eR32G32B32A32Sfloat, 16 }
        },
        vk::FrontFace::eClockwise,
        true,
        persistent_gui.pipeline_layout,
        persistent_gui.render_pass
      )
    )
  );

  persistent_gui.volume_pipeline = std::move(
    vk::raii::Pipeline(
      makeGraphicsPipeline(
        vkch_ctxt->device(),
        *(persistent_gui.svolume_vertex), nullptr,
        *(persistent_gui.svolume_fragment), nullptr,
        sizeof(float) * 8,
        vk::PrimitiveTopology::eTriangleList,
        {
          { vk::Format::eR32G32B32Sfloat, 0 },
          { vk::Format::eR32G32B32A32Sfloat, 16 }
        },
        vk::FrontFace::eClockwise,
        true,
        persistent_gui.pipeline_layout,
        persistent_gui.render_pass
      )
    )
  );

  persistent_gui.frame_pool = std::move(
    FramePool(
      vkch_ctxt->physical_device(),
      vkch_ctxt->device(),
      *(persistent_gui.graphics_queue),
      persistent_gui.graphics_queue_mutex_ptr,
      aux_ctxt->graphics_queue_family_index.first
    )
  );

  initialise_scene(
    persistent_gui.scene,
    simulation_parameters.voxelXCount(),
    simulation_parameters.voxelYCount(),
    simulation_parameters.voxelZCount()
  );

  // Borrow the initial command pool
  volume_buffers->initialise(
    vkch_ctxt->device(),
    persistent_gui.frame_pool.current_frame().command_buffer(),
    *(persistent_gui.graphics_queue),
    persistent_gui.graphics_queue_mutex_ptr);

  // Make scene buffers, vertex etc.
  for (int i = 0; i < persistent_gui.frame_pool.count(); i++)
  {
    persistent_gui.frame_pool._frames[i]->scene_buffers = std::move(
      std::make_unique<struct SceneBuffers>(
        vkch_ctxt->physical_device(), vkch_ctxt->device(),
        persistent_gui.descriptor_set_layout_base
      )
    );

  } // i

  updateProjection(aux_ctxt->width, aux_ctxt->height, persistent_gui.scene);
  updateOrthoProjection(aux_ctxt->width, aux_ctxt->height,
    persistent_gui.scene);
  updateModelView(persistent_gui.scene);

  updateLineVertices(persistent_gui.scene);
  updateVolumeVertices(persistent_gui.scene);

  persistent_gui.frames_until_end_of_transition = 0;

  persistent_gui.last_frame_rendered =
    std::chrono::steady_clock::now() -
    std::chrono::microseconds(static_cast<unsigned int>(
      1e6f / DESIRED_FRAME_RATE_PER_SECOND));
}

void resizeWindow(
  std::shared_ptr<vkch::Context> &vkch_ctxt,
  AuxiliaryVulkanContext *aux_ctxt,
  struct PersistentGUI &persistent_gui)
{
  vk::Extent2D iextent;
  int w, h;
	SDL_GetWindowSize(aux_ctxt->window, &w, &h);

  if ((aux_ctxt->width == w) && (aux_ctxt->height == h)) return;

	iextent.width = w;
	iextent.height = h;

  // Wait for in-flight frames.
  for (int i = 0; i < persistent_gui.frame_pool.count(); i++)
  {
    if (persistent_gui.frame_pool._frames[i]->in_use())
    {
      while (vk::Result::eTimeout ==
        vkch_ctxt->device().waitForFences(
          *(persistent_gui.frame_pool._frames[i]->fence()),
          VK_TRUE, 5000000));
      persistent_gui.frame_pool._frames[i]->set_not_in_use();

    } // (persistent_gui.frame_pool._frames[i]->in_use())

  } // i

  waitForIdleGraphicsPipeline(vkch_ctxt, aux_ctxt);
  
  fprintf(stderr, "window resize to %d x %d\n", w, h);

  persistent_gui.swapchain_data = std::move(
    SwapchainData(
      vkch_ctxt->physical_device(),
      vkch_ctxt->device(),
      aux_ctxt->surface,
      iextent,
      vk::ImageUsageFlagBits::eColorAttachment,
      &(persistent_gui.swapchain_data.swapchain),
      aux_ctxt->graphics_queue_family_index.first,
      aux_ctxt->present_queue_family_index.first
    )
  );

  persistent_gui.depth_buffer_data = std::move(
    DepthBufferData(
      vkch_ctxt->physical_device(),
      vkch_ctxt->device(),
      vk::Format::eD16Unorm,
      persistent_gui.swapchain_data.extent
    )
  );

  persistent_gui.framebuffers.clear();
  persistent_gui.framebuffers = makeFramebuffers(
    vkch_ctxt->device(), persistent_gui.render_pass,
    persistent_gui.swapchain_data.image_views,
    &(persistent_gui.depth_buffer_data.image_view),
    persistent_gui.swapchain_data.extent
  );

  aux_ctxt->width = iextent.width;
  aux_ctxt->height = iextent.height;

  updateProjection(aux_ctxt->width, aux_ctxt->height, persistent_gui.scene);
  updateOrthoProjection(aux_ctxt->width, aux_ctxt->height,
    persistent_gui.scene);
  updateModelView(persistent_gui.scene);

  updateLineVertices(persistent_gui.scene);
  updateVolumeVertices(persistent_gui.scene);
}
