#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

inline vk::raii::DescriptorPool makeDescriptorPool(
  vk::raii::Device const &device,
  std::vector<vk::DescriptorPoolSize> const &pool_sizes)
{
  assert(!(pool_sizes.empty()));
  uint32_t descriptor_count = 0;
  for (size_t i = 0; i < pool_sizes.size(); i++)
    descriptor_count += pool_sizes[i].descriptorCount;

  vk::DescriptorPoolCreateInfo descriptor_pool_info(
    vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
    descriptor_count,
    pool_sizes
  );
  return vk::raii::DescriptorPool(device, descriptor_pool_info);
}

inline vk::raii::DescriptorSetLayout makeDescriptorSetLayout(
  vk::raii::Device const &device,
  std::vector<std::tuple<vk::DescriptorType, uint32_t, vk::ShaderStageFlags>>
    const &binding_data,
  vk::DescriptorSetLayoutCreateFlags flags)
{
  std::vector<vk::DescriptorSetLayoutBinding> bindings(binding_data.size());
  for (size_t i = 0; i < binding_data.size(); i++)
  {
    bindings[i] = vk::DescriptorSetLayoutBinding(
      static_cast<uint32_t>(i),
      std::get<0>(binding_data[i]),
      std::get<1>(binding_data[i]),
      std::get<2>(binding_data[i])
    );

  } // i

  vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info(flags, bindings);

  return vk::raii::DescriptorSetLayout(device, descriptor_set_layout_info);
}

inline std::vector<vk::raii::Framebuffer> makeFramebuffers(
  vk::raii::Device const &device,
  vk::raii::RenderPass const &render_pass,
  std::vector<vk::raii::ImageView> const &image_views,
  vk::raii::ImageView const *depth_image_view_ptr,
  vk::Extent2D const &extent)
{
  vk::ImageView attachments[2];
  attachments[1] = depth_image_view_ptr ? *depth_image_view_ptr : vk::ImageView();

  vk::FramebufferCreateInfo framebuffer_info(
    vk::FramebufferCreateFlags(),
    render_pass,
    depth_image_view_ptr ? 2 : 1,
    attachments,
    extent.width, extent.height, 1
  );

  std::vector<vk::raii::Framebuffer> framebuffers;
  framebuffers.reserve(image_views.size());
  for (vk::raii::ImageView const &image_view : image_views)
  {
    attachments[0] = image_view;
    framebuffers.push_back(
      vk::raii::Framebuffer(device, framebuffer_info)
    );
  }

  return framebuffers;
}

inline vk::raii::RenderPass makeRenderPass(
  vk::raii::Device const &device,
  vk::Format colour_format,
  vk::Format depth_format,
  vk::AttachmentLoadOp load_op,
  vk::ImageLayout colour_final_layout)
{
  std::vector<vk::AttachmentDescription> attachment_descriptions;

  assert(colour_format != vk::Format::eUndefined);

  attachment_descriptions.emplace_back(
    vk::AttachmentDescriptionFlags(),
    colour_format,
    vk::SampleCountFlagBits::e1,
    load_op,
    vk::AttachmentStoreOp::eStore,
    vk::AttachmentLoadOp::eDontCare,
    vk::AttachmentStoreOp::eDontCare,
    vk::ImageLayout::eUndefined,
    colour_final_layout);

  if (depth_format != vk::Format::eUndefined)
  {
    attachment_descriptions.emplace_back(
      vk::AttachmentDescriptionFlags(),
      depth_format,
      vk::SampleCountFlagBits::e1,
      load_op,
      vk::AttachmentStoreOp::eDontCare,
      vk::AttachmentLoadOp::eDontCare,
      vk::AttachmentStoreOp::eDontCare,
      vk::ImageLayout::eUndefined,
      vk::ImageLayout::eDepthStencilAttachmentOptimal);
  }

  vk::AttachmentReference colour_attachment(
    0, vk::ImageLayout::eColorAttachmentOptimal);
  vk::AttachmentReference depth_attachment(
    1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
  vk::SubpassDescription subpass_description(
    vk::SubpassDescriptionFlags(),
    vk::PipelineBindPoint::eGraphics,
      {},
    colour_attachment,
      {},
    (depth_format != vk::Format::eUndefined) ?
      &depth_attachment : nullptr
    );
  vk::RenderPassCreateInfo render_pass_info(
    vk::RenderPassCreateFlags(),
    attachment_descriptions,
    subpass_description
  );
  return vk::raii::RenderPass(device, render_pass_info);
}

inline vk::raii::Pipeline makeGraphicsPipeline(
  vk::raii::Device const &device,
  vk::raii::ShaderModule const &vertex_shader_module,
  vk::SpecializationInfo const *vertex_shader_specialization_info,
  vk::raii::ShaderModule const &fragment_shader_module,
  vk::SpecializationInfo const *fragment_shader_specialization_info,
  uint32_t vertex_stride,
  vk::PrimitiveTopology primitive_topology,
  std::vector<std::pair<vk::Format, uint32_t> > const &
    vertex_input_attribute_format_offset,
  vk::FrontFace front_face,
  bool depth_buffered,
  vk::raii::PipelineLayout const &pipeline_layout,
  vk::raii::RenderPass const &render_pass)
{
  std::array<vk::PipelineShaderStageCreateInfo, 2> pipeline_shader_stage_infos =
  {
    vk::PipelineShaderStageCreateInfo(
        {}, vk::ShaderStageFlagBits::eVertex, vertex_shader_module, "main",
        vertex_shader_specialization_info ),
    vk::PipelineShaderStageCreateInfo(
        {}, vk::ShaderStageFlagBits::eFragment, fragment_shader_module, "main",
        fragment_shader_specialization_info )
  };

  std::vector<vk::VertexInputAttributeDescription>
    vertex_input_attribute_descriptions;
  vk::PipelineVertexInputStateCreateInfo pipeline_vertex_input_state_info;
  vk::VertexInputBindingDescription vertex_input_binding_description(
    0, vertex_stride);

  if (vertex_stride > 0)
  {
    vertex_input_attribute_descriptions.reserve(
        vertex_input_attribute_format_offset.size() );
    for (uint32_t i = 0; i < vertex_input_attribute_format_offset.size(); i++)
    {
      vertex_input_attribute_descriptions.emplace_back(
        i, 0, vertex_input_attribute_format_offset[i].first,
        vertex_input_attribute_format_offset[i].second);
    }
    pipeline_vertex_input_state_info.setVertexBindingDescriptions(
      vertex_input_binding_description);
    pipeline_vertex_input_state_info.setVertexAttributeDescriptions(
      vertex_input_attribute_descriptions);
  }

  vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_info(
    vk::PipelineInputAssemblyStateCreateFlags(),
    primitive_topology
  );

  vk::PipelineViewportStateCreateInfo pipeline_viewport_state_info(
    vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

  vk::PipelineRasterizationStateCreateInfo pipeline_rasterization_state_info(
    vk::PipelineRasterizationStateCreateFlags(),
    false,
    false,
    vk::PolygonMode::eFill,
    (primitive_topology == vk::PrimitiveTopology::eLineList) ?
      vk::CullModeFlagBits::eNone :
      vk::CullModeFlagBits::eBack,
    front_face,
    false,
    0.0f,
    0.0f,
    0.0f,
    1.0f
  );

  vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_info(
    {}, vk::SampleCountFlagBits::e1
  );

  vk::StencilOpState stencil_op_state(
    vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, 
    vk::CompareOp::eAlways);

  vk::PipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_info(
    vk::PipelineDepthStencilStateCreateFlags(),
    depth_buffered, depth_buffered, vk::CompareOp::eLessOrEqual,
    false, false, stencil_op_state, stencil_op_state);

  vk::ColorComponentFlags color_component_flags(
    vk::ColorComponentFlagBits::eR |
    vk::ColorComponentFlagBits::eG |
    vk::ColorComponentFlagBits::eB |
    vk::ColorComponentFlagBits::eA );

  vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state(
    false,
    vk::BlendFactor::eZero,
    vk::BlendFactor::eZero,
    vk::BlendOp::eAdd,
    vk::BlendFactor::eZero,
    vk::BlendFactor::eZero,
    vk::BlendOp::eAdd,
    color_component_flags
  );

  vk::PipelineColorBlendStateCreateInfo pipeline_color_blend_state_info(
    vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eNoOp,
    pipeline_color_blend_attachment_state, {{1.0f, 1.0f, 1.0f, 1.0f}}
  );

  std::array<vk::DynamicState, 2> dynamic_states = {
    vk::DynamicState::eViewport,
    vk::DynamicState::eScissor
  };

  vk::PipelineDynamicStateCreateInfo pipeline_dynamic_state_info(
    vk::PipelineDynamicStateCreateFlags(), dynamic_states
  );

  vk::GraphicsPipelineCreateInfo graphics_pipeline_info(
    vk::PipelineCreateFlags(),
    pipeline_shader_stage_infos,
    &pipeline_vertex_input_state_info,
    &pipeline_input_assembly_state_info,
    nullptr,
    &pipeline_viewport_state_info,
    &pipeline_rasterization_state_info,
    &pipeline_multisample_state_info,
    &pipeline_depth_stencil_state_info,
    &pipeline_color_blend_state_info,
    &pipeline_dynamic_state_info,
    pipeline_layout,
    render_pass
  );

  return device.createGraphicsPipeline(nullptr, graphics_pipeline_info);

//  return vk::raii::Pipeline(device, nullptr, graphics_pipeline_info);
}
