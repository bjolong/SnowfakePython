
#include "aux_vulkan.h"

std::vector<std::string> instance_extensions_selection_callback(
  const std::vector<std::string> &suggested,
  const std::vector<vk::ExtensionProperties> &available,
  void *user_pointer)
{
  AuxiliaryVulkanContext *aux_ctxt_ptr =
    static_cast<AuxiliaryVulkanContext *>(user_pointer);

  // Find extensions required to render to the window from Vulkan.
  unsigned int extension_count = 0;
#if !defined(NO_GUI)
  if (!(aux_ctxt_ptr->no_gui))
  {
    SDL_Vulkan_GetInstanceExtensions(
      aux_ctxt_ptr->window, &extension_count, nullptr);
  }
#endif // !defined(NO_GUI)

  std::vector<const char *> extension_names(extension_count);
#if !defined(NO_GUI)
  if (!(aux_ctxt_ptr->no_gui))
  {
    SDL_Vulkan_GetInstanceExtensions(
      aux_ctxt_ptr->window, &extension_count, extension_names.data());
  }
#endif // !defined(NO_GUI)

  std::vector<std::string> extensions_to_use = suggested;
  for (int k = 0; k < extension_count; k++)
    extensions_to_use.push_back(extension_names[k]);
  std::sort(extensions_to_use.begin(), extensions_to_use.end());
  std::vector<std::string>::iterator end_unique =
    std::unique(extensions_to_use.begin(), extensions_to_use.end());
  extensions_to_use.erase(end_unique, extensions_to_use.end());

  return extensions_to_use;
}

uint32_t physical_device_selection_callback(
  vk::raii::Instance const &instance,
  std::vector<vk::raii::PhysicalDevice> &physical_device_options,
  void *user_pointer)
{
  AuxiliaryVulkanContext *aux_ctxt_ptr =
    static_cast<AuxiliaryVulkanContext *>(user_pointer);

#if !defined(NO_GUI)
  if (!(aux_ctxt_ptr->no_gui))
  {
    // Create surface for rendering
    SDL_Vulkan_CreateSurface(aux_ctxt_ptr->window, *instance,
      &(aux_ctxt_ptr->surface));
  }
#endif // !defined(NO_GUI)

  return 0;
}

std::vector<std::pair<uint32_t, uint32_t> > queue_family_index_selection_callback(
  vk::raii::PhysicalDevice const &physical_device,
  std::vector<vk::QueueFamilyProperties> &queue_family_properties,
  std::pair<uint32_t, uint32_t> &chosen_compute_index,
  void *user_pointer)
{
  AuxiliaryVulkanContext *aux_ctxt_ptr =
    static_cast<AuxiliaryVulkanContext *>(user_pointer);

  enum
  {
    COMPUTE = 0,
    GRAPHICS = 1,
    PRESENT = 2,
    TYPE_COUNT = 3
  };

  std::pair<uint32_t, uint32_t> chosen_index[TYPE_COUNT];
  std::pair<uint32_t, uint32_t> possible_choices[TYPE_COUNT];
  for (int i = 0; i < TYPE_COUNT; i++)
  {
    chosen_index[i] = std::pair<uint32_t, uint32_t>(-1, -1);
    possible_choices[i] = 
      std::pair<uint32_t, uint32_t>(0, i);
  } // i

  for (std::vector<vk::QueueFamilyProperties>::size_type
    i = 0;
    i < queue_family_properties.size();
    i++)
  {
    if (queue_family_properties.at(i).queueFlags &
      vk::QueueFlagBits::eCompute)
    {
      possible_choices[COMPUTE].first +=
        queue_family_properties.at(i).queueCount;
    }
    if (queue_family_properties.at(i).queueFlags &
      vk::QueueFlagBits::eGraphics)
    {
      possible_choices[GRAPHICS].first +=
        queue_family_properties.at(i).queueCount;
    }
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(
      *physical_device, i, aux_ctxt_ptr->surface,
        &presentSupport);
    if (presentSupport)
    {
      possible_choices[PRESENT].first +=
        queue_family_properties.at(i).queueCount;
    }

  } // i

  std::sort(
    &(possible_choices[0]),
    &(possible_choices[TYPE_COUNT])
  );

  std::unique_ptr<int[]> queue_counter =
    std::make_unique<int[]>(queue_family_properties.size());
//  int *queue_counter = new int[queue_family_properties.size()];
  for (int l = 0; l < queue_family_properties.size(); l++)
    queue_counter[l] = 0;

  chosen_compute_index =
    std::pair<uint32_t, uint32_t>(-1, -1);
  std::pair<uint32_t, uint32_t> chosen_graphic_index =
    std::pair<uint32_t, uint32_t>(-1, -1);
  std::pair<uint32_t, uint32_t> present_index =
    std::pair<uint32_t, uint32_t>(-1, -1);

  for (int l = 0; l < TYPE_COUNT; l++)
  {
    for (std::vector<vk::QueueFamilyProperties>::size_type
      i = 0;
      i < queue_family_properties.size();
      i++)
    {
      if ((possible_choices[l].second == COMPUTE) &&
        (queue_family_properties.at(i).queueCount > queue_counter[i]) &&
        (queue_family_properties.at(i).queueFlags &
          vk::QueueFlagBits::eCompute))
      {
        chosen_compute_index =
          std::pair<uint32_t, uint32_t>(
            i, (queue_counter[i])++);
      }

      if ((possible_choices[l].second == GRAPHICS) &&
        (queue_family_properties.at(i).queueCount > queue_counter[i]) &&
        (queue_family_properties.at(i).queueFlags &
          vk::QueueFlagBits::eGraphics))
      {
        chosen_graphic_index =
          std::pair<uint32_t, uint32_t>(
            i, (queue_counter[i])++);
      }

      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(
        *physical_device, i, aux_ctxt_ptr->surface,
          &presentSupport);
      if ((possible_choices[l].second == PRESENT) &&
        (queue_family_properties.at(i).queueCount > queue_counter[i]) &&
        presentSupport)
      {
        present_index =
          std::pair<uint32_t, uint32_t>(
            i, (queue_counter[i])++);
      }

    }
  }

  // Allow sharing queues as a last resort
  for (int l = 0; l < TYPE_COUNT; l++)
  {
    for (std::vector<vk::QueueFamilyProperties>::size_type
      i = 0;
      i < queue_family_properties.size();
      i++)
    {
      if ((possible_choices[l].second == COMPUTE) &&
        (queue_family_properties.at(i).queueCount > 0) &&
        (queue_family_properties.at(i).queueFlags &
          vk::QueueFlagBits::eCompute) &&
        (chosen_compute_index.first == uint32_t(-1)))
      {
        chosen_compute_index =
          std::pair<uint32_t, uint32_t>(
            i, 0);
      }

      if ((possible_choices[l].second == GRAPHICS) &&
        (queue_family_properties.at(i).queueCount > 0) &&
        (queue_family_properties.at(i).queueFlags &
          vk::QueueFlagBits::eGraphics) &&
        (chosen_graphic_index.first == uint32_t(-1)))
      {
        chosen_graphic_index =
          std::pair<uint32_t, uint32_t>(
            i, 0);
      }

      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(
        *physical_device, i, aux_ctxt_ptr->surface,
          &presentSupport);
      if ((possible_choices[l].second == PRESENT) &&
        (queue_family_properties.at(i).queueCount > 0) &&
        presentSupport &&
        (present_index.first == uint32_t(-1)))
      {
        present_index =
          std::pair<uint32_t, uint32_t>(
            i, 0);
      }

    } // i

  } // l

  if (chosen_graphic_index.first == -1)
  {
    throw std::runtime_error("No graphics-capable queue found.");
  } else
  {
#if !defined(BUILD_PYTHON_BINDINGS)
    fprintf(stderr, "select GRAPHICS queue - family %d, index %d\n",
      chosen_graphic_index.first,
      chosen_graphic_index.second);
#endif // !defined(BUILD_PYTHON_BINDINGS)
  }

  if (chosen_compute_index.first == -1)
  {
    throw std::runtime_error("No compute-capable queue found.");
  } else
  {
#if !defined(BUILD_PYTHON_BINDINGS)
    fprintf(stderr, "select COMPUTE queue - family %d, index %d\n",
      chosen_compute_index.first,
      chosen_compute_index.second);
#endif // !defined(BUILD_PYTHON_BINDINGS)
  }

  if (present_index.first == -1)
  {
    throw std::runtime_error("No present-capable queue found.");
  } else
  {
#if !defined(BUILD_PYTHON_BINDINGS)
    fprintf(stderr, "select PRESENT queue - family %d, index %d\n",
      present_index.first,
      present_index.second);
#endif // !defined(BUILD_PYTHON_BINDINGS)
  }

  aux_ctxt_ptr->graphics_queue_family_index = chosen_graphic_index;
  aux_ctxt_ptr->compute_queue_family_index = chosen_compute_index;
  aux_ctxt_ptr->present_queue_family_index = present_index;

  std::vector<std::pair<uint32_t, uint32_t> > unique_queue_family_index = {
    chosen_graphic_index,
    chosen_compute_index,
    present_index,
  };
  std::sort(unique_queue_family_index.begin(), unique_queue_family_index.end());
  std::vector<std::pair<uint32_t, uint32_t> >::iterator end_unique =
    std::unique(unique_queue_family_index.begin(),
      unique_queue_family_index.end());
  unique_queue_family_index.erase(end_unique, unique_queue_family_index.end());

  std::vector<std::pair<uint32_t, uint32_t> > other_auxiliary_families;
  for (int i = 0; i < unique_queue_family_index.size(); i++)
  {
    if (unique_queue_family_index[i] != chosen_compute_index)
      other_auxiliary_families.push_back(unique_queue_family_index[i]);
  }

  return other_auxiliary_families;
}

std::vector<std::string> device_extension_selection_callback(
  const std::vector<std::string> &suggested,
  const std::vector<vk::ExtensionProperties> &available,
  void *user_pointer)
{
  AuxiliaryVulkanContext *aux_ctxt_ptr =
    static_cast<AuxiliaryVulkanContext *>(user_pointer);

/*
  fprintf(stderr, "Available extensions: ");
  for (int p = 0; p < available.size(); p++)
  {
    fprintf(stderr, "%s, ", &(available[p].extensionName[0]));
  }
  fprintf(stderr, "\n");
*/

  aux_ctxt_ptr->global_priority_extension_active = 0;
  std::vector<std::string> device_extensions_to_use = suggested;
  device_extensions_to_use.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  for (int i = 0; i < available.size(); i++)
  {
    if (std::string(available[i].extensionName) ==
      std::string("VK_KHR_global_priority"))
    {
      device_extensions_to_use.push_back("VK_KHR_global_priority");
      aux_ctxt_ptr->global_priority_extension_active = 1;
    }
    if (std::string(available[i].extensionName) ==
      std::string("VK_EXT_global_priority"))
    {
      device_extensions_to_use.push_back("VK_EXT_global_priority");
      aux_ctxt_ptr->global_priority_extension_active = 1;
    }
  }
  return device_extensions_to_use;
}
