#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>

inline uint32_t findMemoryType(
  vk::PhysicalDeviceMemoryProperties const &memory_properties,
  uint32_t type_bits,
  vk::MemoryPropertyFlags requirements_mask)
{
  uint32_t type_index = uint32_t(~0);
  for (uint32_t
    i = 0;
    i < memory_properties.memoryTypeCount;
    i++)
  {
    if ((type_bits & 1) &&
      ((memory_properties.memoryTypes[i].propertyFlags &
        requirements_mask) == requirements_mask))
    {
      return i;
    }
    type_bits >>= 1;
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

inline vk::raii::DeviceMemory allocateDeviceMemory(
  vk::raii::Device const &device,
  vk::PhysicalDeviceMemoryProperties const &memory_properties,
  vk::MemoryRequirements const &memory_requirements,
  vk::MemoryPropertyFlags memory_property_flags)
{
  uint32_t memoryTypeIndex = findMemoryType(
    memory_properties, memory_requirements.memoryTypeBits,
    memory_property_flags);
  vk::MemoryAllocateInfo memory_allocate_info(
    memory_requirements.size, memoryTypeIndex);
  return std::move(vk::raii::DeviceMemory(device, memory_allocate_info));
}
