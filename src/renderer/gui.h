#pragma once

#include "../VulkanComputeHelper.h"
#include "VolumeBuffers.h"
#include "SimulationParameters.h"

#if !defined(NO_GUI)
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#endif // !defined(NO_GUI)

namespace vkch = vkComputeHelper;

struct AuxiliaryVulkanContext
{
#if defined(NO_GUI)
  void *window;
#else // defined(NO_GUI)
  SDL_Window *window;
#endif // defined(NO_GUI)
  VkSurfaceKHR surface;

  int width;
  int height;

  std::pair<uint32_t, uint32_t> compute_queue_family_index;
  std::pair<uint32_t, uint32_t> graphics_queue_family_index;
  std::pair<uint32_t, uint32_t> present_queue_family_index;

  bool global_priority_extension_active;
  bool no_gui;
};

void updateOrthoProjection(int width, int height, struct Scene &scene);
void updateModelView(struct Scene &scene);
void updateProjection(int width, int height, struct Scene &scene);

void updateVolumeVertices(struct Scene &scene);
void updateLineVertices(struct Scene &scene);

void waitForIdleGraphicsPipeline(
  std::shared_ptr<vkch::Context> &vkch_ctxt,
  AuxiliaryVulkanContext *aux_ctxt);

int checkEvents(
  volatile int *stop_thread,
  std::shared_ptr<vkch::Context> &vkch_ctxt,
  AuxiliaryVulkanContext *aux_ctxt,
  struct PersistentGUI &persistent_gui);

void gui_process_events(
  volatile int *stop_thread,
  const std::shared_ptr<VolumeBuffers> &volume_buffers,
  std::shared_ptr<vkch::Context> &vkch_ctxt,
  AuxiliaryVulkanContext *aux_ctxt,
  SimulationParameters const &simulation_parameters,
  struct PersistentGUI &persistent_gui);

void gui_process_wait_for_completion(
  std::shared_ptr<vkch::Context> &vkch_ctxt,
  AuxiliaryVulkanContext *aux_ctxt,
  struct PersistentGUI &persistent_gui);
