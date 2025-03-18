#pragma once

#include <istream>
#include <fstream>
#include <sstream>

#include <memory>
#include <complex>

#include "VulkanComputeHelper.h"
#include "SimulationParameters.h"
#include "Simulation.hpp"

namespace vkch = vkComputeHelper;

struct StepSimulation
{
  bool no_gui;

  std::shared_ptr<vkch::SharedTensor<float> > tensor_tx_locations;

  std::shared_ptr<vkch::SharedTensor<float> > tensor_A;
  std::shared_ptr<vkch::SharedTensor<float> > tensor_B;

  std::shared_ptr<vkch::TensorParameterSet> params_step_AB;
  std::shared_ptr<vkch::TensorParameterSet> params_render_A;

  std::shared_ptr<vkch::Program> program_step;
  std::shared_ptr<vkch::Program> program_render;

  std::shared_ptr<vkch::Schema> schema_upload;
  std::shared_ptr<vkch::Schema> schema_step_00_10;
  std::shared_ptr<vkch::Schema> schema_renders;
  std::shared_ptr<vkch::Schema> schema_download;

  std::shared_ptr<vkch::Schema> last_schema;

  void init_schemas(
    const SimulationParameters &simulation_parameters,
    std::shared_ptr<vkch::Context> &vkch_ctxt)
  {
    const std::tuple<unsigned int, unsigned int, unsigned int> workgroup(
      (static_cast<unsigned int>(simulation_parameters.voxelXCount()) == 0) ?
        0 :
        (((static_cast<unsigned int>(
          simulation_parameters.voxelXCount()) - 1) / 64) + 1),
      static_cast<unsigned int>(simulation_parameters.voxelYCount()),
      static_cast<unsigned int>(simulation_parameters.voxelZCount())
    );

    // TODO: Need to move schema creation back here - the steps do not need
    // recreating now they don't depend on time parameters.
    schema_step_00_10 =
      vkch_ctxt->schema();

    schema_upload =
      vkch_ctxt->schema()
        ->add<vkch::UploadTensors>(
          std::vector<std::shared_ptr<vkch::Tensor> >{
            tensor_A
          }
        )
        ->make();

    schema_download =
      vkch_ctxt->schema()
        ->add<vkch::DownloadTensors>(
          std::vector<std::shared_ptr<vkch::Tensor> >{
            tensor_A
          }
        )
        ->make();

#if !defined(NO_GUI)
    if (!no_gui)
    {
      schema_renders = vkch_ctxt->schema();

    } else
#endif // !defined(NO_GUI)
    {
    }
  }

  void schedule(
    std::shared_ptr<VolumeBuffers> const &volume_buffers,
    const SimulationParameters &simulation_parameters,
    const uintmax_t current_timestep
  )
  {
    const std::tuple<unsigned int, unsigned int, unsigned int> workgroup(
      (static_cast<unsigned int>(simulation_parameters.voxelXCount()) == 0) ?
        0 :
        (((static_cast<unsigned int>(
          simulation_parameters.voxelXCount()) - 1) / 64) + 1),
      static_cast<unsigned int>(simulation_parameters.voxelYCount()),
      static_cast<unsigned int>(simulation_parameters.voxelZCount())
    );

    schema_step_00_10
      ->clear()
      ->add<vkch::Work>(
        workgroup,
        std::vector<vkch::ConstantBase>({}),
        params_step_AB,
        program_step
      )
      ->make();

    if (!no_gui)
    {
      vk::raii::DescriptorSet const *latest_volume_ptr =
        &(volume_buffers->write_descriptor_set());

      schema_renders
        ->clear()
        ->add<vkch::Work>(
          workgroup,
          std::vector<vkch::ConstantBase>({}),
          params_render_A,
          program_render,
          latest_volume_ptr
        )
        ->make();
    } // (!no_gui)
  }

  void submit(
    const bool first_run,
    const bool do_download,
    std::shared_ptr<vkch::Schema> dependency
  )
  {
    std::shared_ptr<vkch::Schema> actual_dependency;
    if (first_run)
    {
      schema_upload->submitForAfter(dependency);
      actual_dependency = schema_upload;
    }
    schema_step_00_10->submitForAfter(
      (first_run) ? actual_dependency : dependency);

    if (!no_gui)
    {
      schema_renders->submitForAfter(schema_step_00_10);

    }

    if (do_download)
    {
      schema_download->submitForAfter(
        (no_gui) ? schema_step_00_10 : schema_renders);
      last_schema = schema_download;
    } else
    {
      last_schema = (no_gui) ? schema_step_00_10 : schema_renders;
    }
  }

  std::shared_ptr<vkch::Schema> getLastSchema()
  {
    return last_schema;
  }
};
