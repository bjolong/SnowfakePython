#pragma once

#include "SimulationParameters.h"

#include "VulkanComputeHelper.h"
#include "renderer/VolumeBuffers.h"

namespace vkch = vkComputeHelper;

void simulation_thread(
  volatile int *stop_thread,
  bool no_gui,
  const std::shared_ptr<VolumeBuffers> &volume_buffers,
  std::shared_ptr<vkch::Context> &vkch_ctxt,
  SimulationParameters const &simulation_parameters);
