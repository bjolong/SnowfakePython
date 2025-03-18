#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include "BufferData.h"

inline void setImageLayout(
  vk::raii::CommandBuffer const &command_buffer,
  vk::Image image,
  vk::Format format,
  vk::ImageLayout old_image_layout,
  vk::ImageLayout new_image_layout)
{
  vk::AccessFlags source_access_mask;
  switch (old_image_layout)
  {
    case vk::ImageLayout::eTransferDstOptimal:
    {
      source_access_mask = vk::AccessFlagBits::eTransferWrite;
    } break;
    case vk::ImageLayout::ePreinitialized:
    {
      source_access_mask = vk::AccessFlagBits::eHostWrite;
    } break;
    case vk::ImageLayout::eGeneral:
    case vk::ImageLayout::eUndefined:
      break;
    default:
    {
      throw std::runtime_error("image data transition from"
        " unimplemented layout");
    } break;
  }

  vk::PipelineStageFlags source_stage;
  switch (old_image_layout)
  {
    case vk::ImageLayout::eGeneral:
    case vk::ImageLayout::ePreinitialized:
    {
      source_stage = vk::PipelineStageFlagBits::eHost;
    } break;
    case vk::ImageLayout::eTransferDstOptimal:
    {
      source_stage = vk::PipelineStageFlagBits::eTransfer;
    } break;
    case vk::ImageLayout::eUndefined:
    {
      source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
    } break;
    default:
    {
      throw std::runtime_error("image data transition from"
        " unimplemented layout");
    } break;
  }

  vk::AccessFlags destination_access_mask;
  switch (new_image_layout)
  {
    case vk::ImageLayout::eColorAttachmentOptimal:
    {
      destination_access_mask =
        vk::AccessFlagBits::eColorAttachmentWrite;
    } break;
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
    {
      destination_access_mask =
        vk::AccessFlagBits::eDepthStencilAttachmentRead |
        vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    } break;
    case vk::ImageLayout::eGeneral:
    case vk::ImageLayout::ePresentSrcKHR:
      break; // empty destinationAccessMask
    case vk::ImageLayout::eShaderReadOnlyOptimal:
    {
      destination_access_mask = vk::AccessFlagBits::eShaderRead;
    } break;
    case vk::ImageLayout::eTransferSrcOptimal:
    {
      destination_access_mask = vk::AccessFlagBits::eTransferRead;
    } break;
    case vk::ImageLayout::eTransferDstOptimal:
    {
      destination_access_mask = vk::AccessFlagBits::eTransferWrite;
    } break;
    default:
    {
      throw std::runtime_error("image data transition to"
        " unimplemented layout");
    } break;
  }

  vk::PipelineStageFlags destination_stage;
  switch (new_image_layout)
  {
    case vk::ImageLayout::eColorAttachmentOptimal:
    {
      destination_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    } break;
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
    {
      destination_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    } break;
    case vk::ImageLayout::eGeneral:
    {
      destination_stage = vk::PipelineStageFlagBits::eHost;
    } break;
    case vk::ImageLayout::ePresentSrcKHR:
    {
      destination_stage = vk::PipelineStageFlagBits::eBottomOfPipe;
    } break;
    case vk::ImageLayout::eShaderReadOnlyOptimal:
    {
      destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
    } break;
    case vk::ImageLayout::eTransferDstOptimal:
    case vk::ImageLayout::eTransferSrcOptimal:
    {
      destination_stage = vk::PipelineStageFlagBits::eTransfer;
    } break;
    default:
    {
      throw std::runtime_error("image data transition to"
        " unimplemented layout");
    } break;
  }

  vk::ImageAspectFlags aspect_mask;
  if (new_image_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
  {
    aspect_mask = vk::ImageAspectFlagBits::eDepth;
    if ((format == vk::Format::eD32SfloatS8Uint) ||
      (format == vk::Format::eD24UnormS8Uint))
    {
      aspect_mask |= vk::ImageAspectFlagBits::eStencil;
    }
  }
  else
  {
    aspect_mask = vk::ImageAspectFlagBits::eColor;
  }

  vk::ImageSubresourceRange image_subresource_range(aspect_mask, 0, 1, 0, 1 );

  vk::ImageMemoryBarrier image_memory_barrier(
    source_access_mask,
    destination_access_mask,
    old_image_layout,
    new_image_layout,
    VK_QUEUE_FAMILY_IGNORED,
    VK_QUEUE_FAMILY_IGNORED,
    image,
    vk::ImageSubresourceRange(
      {aspect_mask, 0, 1, 0, 1}
    )
  );

  return command_buffer.pipelineBarrier(
    source_stage,
    destination_stage,
    {},
    nullptr,
    nullptr,
    image_memory_barrier
  );
}

struct ImageData
{
  ImageData(
    vk::raii::PhysicalDevice const &physical_device,
    vk::raii::Device const &device,
    vk::Format iformat,
    vk::Extent2D const &extent,
    vk::ImageTiling tiling,
    vk::ImageUsageFlags usage,
    vk::ImageLayout initial_layout,
    vk::MemoryPropertyFlags memory_properties,
    vk::ImageAspectFlags aspect_mask)
    : format(iformat)
    , device_memory(nullptr)
    , image(
      device, {
        vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        format,
        vk::Extent3D(extent, 1),
        1, 1,
        vk::SampleCountFlagBits::e1,
        tiling,
        usage | vk::ImageUsageFlagBits::eSampled,
        vk::SharingMode::eExclusive,
        {},
        initial_layout
      })
    , image_view(nullptr)
  {
    device_memory = allocateDeviceMemory(
      device, physical_device.getMemoryProperties(),
      image.getMemoryRequirements(), memory_properties);
    image.bindMemory(device_memory, 0);
    image_view = vk::raii::ImageView(
      device,
      vk::ImageViewCreateInfo(
        {},
        image,
        vk::ImageViewType::e2D,
        format,
        {},
        { aspect_mask, 0, 1, 0, 1 }
      )
    );
  }

  ImageData(std::nullptr_t)
    : format()
    , device_memory(nullptr)
    , image(nullptr)
    , image_view(nullptr)
  {}

  // the DeviceMemory should be destroyed before the Image it is bound to; to get that order with the standard destructor
  // of the ImageData, the order of DeviceMemory and Image here matters
  vk::Format format;
  vk::raii::DeviceMemory device_memory;
  vk::raii::Image image;
  vk::raii::ImageView image_view;
};

struct DepthBufferData : public ImageData
{
  DepthBufferData(nullptr_t)
    : ImageData(nullptr)
  {}

  DepthBufferData(vk::raii::PhysicalDevice const &physical_device,
    vk::raii::Device const &device, vk::Format format,
    vk::Extent2D const &extent)
    : ImageData(physical_device, device, format, extent,
      vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eDepthStencilAttachment,
      vk::ImageLayout::eUndefined, vk::MemoryPropertyFlagBits::eDeviceLocal,
      vk::ImageAspectFlagBits::eDepth)
  {}
};
