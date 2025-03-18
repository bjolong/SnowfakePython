#pragma once

#include "gui.h"

#if !defined(NO_GUI)
inline vk::Extent2D getExtentFromWindow(
  SDL_Window *window)
{
  int drawable_width, drawable_height = 0;
  SDL_Vulkan_GetDrawableSize(window,
    &drawable_width, &drawable_height);
  vk::Extent2D swapchain_extent;
  swapchain_extent.width = drawable_width;
  swapchain_extent.height = drawable_height;
  return swapchain_extent;
}
#endif // !defined(NO_GUI)

inline vk::SurfaceFormatKHR pickSurfaceFormat(
  std::vector<vk::SurfaceFormatKHR> const &formats)
{
  vk::SurfaceFormatKHR chosen_format = formats[0];
  if (formats.size() == 1)
  {
    if (formats[0].format == vk::Format::eUndefined)
    {
      chosen_format.format = vk::Format::eB8G8R8A8Unorm;
      chosen_format.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;

    } // (formats[0].format == vk::Format::eUndefined)

  } else // (formats.size() == 1)
  {
    vk::Format suitable_format[] = {
      vk::Format::eB8G8R8A8Unorm,
      vk::Format::eR8G8B8A8Unorm,
      vk::Format::eB8G8R8Unorm,
      vk::Format::eR8G8B8Unorm
    };
    vk::ColorSpaceKHR suitable_colourspace[] = {
      vk::ColorSpaceKHR::eSrgbNonlinear
    };
    bool found_format = false;
    for (std::vector<vk::SurfaceFormatKHR>::const_iterator
      cit = formats.cbegin();
      cit != formats.cend();
      ++cit)
    {
      for (size_t j = 0;
        j < (sizeof(suitable_format)/sizeof(suitable_format[0]));
        j++)
      {
        if (suitable_format[j] != cit->format) continue;

        for (size_t i = 0;
          i < (sizeof(suitable_colourspace)/sizeof(suitable_colourspace[0]));
          i++)
        {
          if (suitable_colourspace[i] != cit->colorSpace) continue;

          chosen_format = *cit;
          found_format = true;

          if (found_format) break;

        } // i

        if (found_format) break;

      } // j

      if (found_format) break;

    } // cit

  } // else (formats.size() == 1)

  return chosen_format;
}

struct SwapchainData
{
  SwapchainData(
    vk::raii::PhysicalDevice const &physical_device,
    vk::raii::Device const &device,
    VkSurfaceKHR surface,
    vk::Extent2D iextent,
    vk::ImageUsageFlags usage,
    vk::raii::SwapchainKHR const *old_swapchain_ptr,
    uint32_t graphics_queue_family_index,
    uint32_t present_queue_family_index)
    : swapchain(nullptr)
  {
    std::vector<vk::SurfaceFormatKHR> formats =
      physical_device.getSurfaceFormatsKHR(surface);

    if (formats.empty())
    {
      throw std::runtime_error(
        "No valid format available for the Vulkan-accessible surface");
    }

    vk::SurfaceFormatKHR chosen_format = pickSurfaceFormat(formats);

    colour_format = chosen_format.format;

    vk::SurfaceCapabilitiesKHR surface_caps =
      physical_device.getSurfaceCapabilitiesKHR(surface);

    if (surface_caps.currentExtent.width == std::numeric_limits<uint32_t>::max())
    {
      extent.width =
        (iextent.width < surface_caps.minImageExtent.width) ?
          surface_caps.minImageExtent.width :
        (iextent.width > surface_caps.maxImageExtent.width) ?
          surface_caps.maxImageExtent.width :
          iextent.width;

      extent.height =
        (iextent.height < surface_caps.minImageExtent.height) ?
          surface_caps.minImageExtent.height :
        (iextent.height > surface_caps.maxImageExtent.height) ?
          surface_caps.maxImageExtent.height :
          iextent.height;
    } else
    {
      extent = surface_caps.currentExtent;
    }

    vk::SurfaceTransformFlagBitsKHR pre_transform =
      (surface_caps.supportedTransforms &
        vk::SurfaceTransformFlagBitsKHR::eIdentity) ?
          vk::SurfaceTransformFlagBitsKHR::eIdentity :
          surface_caps.currentTransform;

    vk::CompositeAlphaFlagBitsKHR composite_alpha =
      (surface_caps.supportedCompositeAlpha &
        vk::CompositeAlphaFlagBitsKHR::ePreMultiplied) ?
          vk::CompositeAlphaFlagBitsKHR::ePreMultiplied :
      (surface_caps.supportedCompositeAlpha &
        vk::CompositeAlphaFlagBitsKHR::ePostMultiplied) ?
          vk::CompositeAlphaFlagBitsKHR::ePostMultiplied :
      (surface_caps.supportedCompositeAlpha &
        vk::CompositeAlphaFlagBitsKHR::eInherit) ?
        vk::CompositeAlphaFlagBitsKHR::eInherit :
          vk::CompositeAlphaFlagBitsKHR::eOpaque;

    vk::PresentModeKHR swapchain_present_mode = vk::PresentModeKHR::eFifo;

    const int surface_count =
      ((surface_caps.minImageCount + 1) < surface_caps.maxImageCount) ?
        (surface_caps.minImageCount + 1) : surface_caps.minImageCount;

    vk::SwapchainCreateInfoKHR swapchain_info(
      vk::SwapchainCreateFlagsKHR(),
      surface,
      surface_count,
      chosen_format.format,
      chosen_format.colorSpace,
      extent,
      1,
      usage,
      vk::SharingMode::eExclusive,
      {},
      pre_transform,
      composite_alpha,
      swapchain_present_mode,
      true,
      old_swapchain_ptr ? (**old_swapchain_ptr) : nullptr);

    std::array<uint32_t, 2> queue_families_involved = {
      graphics_queue_family_index,
      present_queue_family_index
    };

    if (graphics_queue_family_index != present_queue_family_index)
    {
      swapchain_info.imageSharingMode = vk::SharingMode::eConcurrent;
      swapchain_info.queueFamilyIndexCount = 2;
      swapchain_info.pQueueFamilyIndices = queue_families_involved.data();

    } // (graphics_queue_family_index != present_queue_family_index)

    swapchain = vk::raii::SwapchainKHR(device, swapchain_info);

    images = swapchain.getImages();

    image_views.reserve(images.size());
    vk::ImageViewCreateInfo image_views_info(
      {},
      {},
      vk::ImageViewType::e2D,
      chosen_format.format,
      {},
      { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
    );

    for (auto image : images)
    {
      image_views_info.image = image;
      image_views.emplace_back( device, image_views_info);
    }
  }

  SwapchainData(std::nullptr_t)
    : colour_format(vk::Format::eUndefined)
    , swapchain(nullptr)
    , extent(vk::Extent2D(0, 0))
    , images(std::vector<vk::Image>())
    , image_views(std::vector<vk::raii::ImageView>())
  {}

  int size() const
  {
    return images.size();
  }

  vk::Format colour_format;
  vk::raii::SwapchainKHR swapchain;
  vk::Extent2D extent;
  std::vector<vk::Image> images;
  std::vector<vk::raii::ImageView> image_views;
};
