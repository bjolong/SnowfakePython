#pragma once

#include <string>
#include <vector>

#include "renderer/gui.h"

std::vector<std::string> instance_extensions_selection_callback(
  const std::vector<std::string> &suggested,
  const std::vector<vk::ExtensionProperties> &available,
  void *user_pointer);

uint32_t physical_device_selection_callback(
  vk::raii::Instance const &instance,
  std::vector<vk::raii::PhysicalDevice> &physical_device_options,
  void *user_pointer);

std::vector<std::pair<uint32_t, uint32_t> > queue_family_index_selection_callback(
  vk::raii::PhysicalDevice const &physical_device,
  std::vector<vk::QueueFamilyProperties> &queue_family_properties,
  std::pair<uint32_t, uint32_t> &chosen_compute_index,
  void *user_pointer);

std::vector<std::string> device_extension_selection_callback(
  const std::vector<std::string> &suggested,
  const std::vector<vk::ExtensionProperties> &available,
  void *user_pointer);
