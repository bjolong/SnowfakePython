
#include "constants.h"
#include "compute.h"
#include "StepSimulation.h"

#include "Simulation.hpp"

#include "shader_headers/solver_substep.comp.spv.h"
#include "shader_headers/sample_occupancy.comp.spv.h"

void simulation_thread(
  volatile int *stop_thread,
  bool no_gui,
  const std::shared_ptr<VolumeBuffers> &volume_buffers,
  std::shared_ptr<vkch::Context> &vkch_ctxt,
  SimulationParameters const &simulation_parameters)
{
  // quiescent field values for initialisation / boundaries
  float initial_dirichlet_params[SOLVER_FIELD_COUNT] = {
    0.f,
    float(simulation_parameters.medium().rho()),
    0.f
  };

  std::vector<uint32_t> spirv_solver_substep(
    &(shader__solver_substep_comp[0]),
    &(shader__solver_substep_comp[0]) + (
      sizeof(shader__solver_substep_comp) /
        sizeof(shader__solver_substep_comp[0])
    )
  );
  std::vector<uint32_t> spirv_render(
    &(shader__sample_occupancy_comp[0]),
    &(shader__sample_occupancy_comp[0]) + (
      sizeof(shader__sample_occupancy_comp) /
        sizeof(shader__sample_occupancy_comp[0])
    )
  );

  StepSimulation Step_A;
  StepSimulation Step_B;
  Step_A.no_gui = Step_B.no_gui = no_gui;

  const uintmax_t per_field_size =
    uintmax_t(simulation_parameters.voxelXCount()) *
    uintmax_t(simulation_parameters.voxelYCount()) *
    uintmax_t(simulation_parameters.voxelZCount());

  // Dry run initended allocations on the memory pool first.
  vkch_ctxt->dryrunSharedTensorAllocate(
    per_field_size * SOLVER_FIELD_COUNT * sizeof(float));
  vkch_ctxt->dryrunSharedTensorAllocate(
    per_field_size * SOLVER_FIELD_COUNT * sizeof(float));

  std::shared_ptr<vkch::SharedTensor<float> > tensor_0 =
    vkch_ctxt->sharedTensor<float>(per_field_size * SOLVER_FIELD_COUNT);
  Step_A.tensor_A = tensor_0;
  Step_B.tensor_B = tensor_0;

  std::shared_ptr<vkch::SharedTensor<float> > tensor_1 =
    vkch_ctxt->sharedTensor<float>(per_field_size * SOLVER_FIELD_COUNT);
  Step_A.tensor_B = tensor_1;
  Step_B.tensor_A = tensor_1;

  for (int m = 0; m < SOLVER_FIELD_COUNT; m++)
  {
    for (int p = 0; p < per_field_size; p++)
    {
      Step_A.tensor_A->data()[m*per_field_size + p] =
        initial_dirichlet_params[m];
    }
  }

  const int64_t ctre_idx =
    (uintmax_t(simulation_parameters.voxelZCount()) / 2)*
    uintmax_t(simulation_parameters.voxelYCount())*
    uintmax_t(simulation_parameters.voxelXCount()) +
    (uintmax_t(simulation_parameters.voxelYCount()) / 2)*
    uintmax_t(simulation_parameters.voxelXCount()) +
    (uintmax_t(simulation_parameters.voxelXCount()) / 2);

  for (int k = -(simulation_parameters.seed().thickness() / 2);
    k < (simulation_parameters.seed().thickness() -
      (simulation_parameters.seed().thickness() / 2)); k++)
  {
    for (int j = -simulation_parameters.seed().radius();
      j < (simulation_parameters.seed().radius() + 1); j++)
    {
      for (int i = -simulation_parameters.seed().radius();
        i < (simulation_parameters.seed().radius() + 1); i++)
      {
        if ((abs(i+j) < (simulation_parameters.seed().radius() + 1)) &&
            (abs(i) < (simulation_parameters.seed().radius() + 1)) &&
            (abs(j) < (simulation_parameters.seed().radius() + 1)))
        {
          Step_A.tensor_A->data()[ctre_idx +
            k*
            uintmax_t(simulation_parameters.voxelYCount())*
            uintmax_t(simulation_parameters.voxelXCount()) +
            j*
            uintmax_t(simulation_parameters.voxelXCount()) +
            i] = 1.f;
        }

      } // i

    } // j

  } // k

  std::vector<vkch::ConstantBase> spec_constants_step = {
    // Voxel sizes
    vkch::Constant<float>(simulation_parameters.voxelXCount()), // 0
    vkch::Constant<float>(simulation_parameters.voxelYCount()), // 1
    vkch::Constant<float>(simulation_parameters.voxelZCount()), // 2
    vkch::Constant<float>(simulation_parameters.radiusT()), // 3
    vkch::Constant<float>(simulation_parameters.radiusZ()), // 4

    // Simulation parameters
    vkch::Constant<float>(simulation_parameters.medium().rho()), // 5
    vkch::Constant<float>(simulation_parameters.medium().phi()), // 6

    vkch::Constant<float>(simulation_parameters.medium().kappa_01()), // 7
    vkch::Constant<float>(simulation_parameters.medium().kappa_10()), // 8
    vkch::Constant<float>(simulation_parameters.medium().kappa_11()), // 9
    vkch::Constant<float>(simulation_parameters.medium().kappa_20()), // 10
    vkch::Constant<float>(simulation_parameters.medium().kappa_21()), // 11
    vkch::Constant<float>(simulation_parameters.medium().kappa_30()), // 12
    vkch::Constant<float>(simulation_parameters.medium().kappa_31()), // 13
    vkch::Constant<float>(simulation_parameters.medium().mu_01()), // 14
    vkch::Constant<float>(simulation_parameters.medium().mu_10()), // 15
    vkch::Constant<float>(simulation_parameters.medium().mu_11()), // 16
    vkch::Constant<float>(simulation_parameters.medium().mu_20()), // 17
    vkch::Constant<float>(simulation_parameters.medium().mu_21()), // 18
    vkch::Constant<float>(simulation_parameters.medium().mu_30()), // 19
    vkch::Constant<float>(simulation_parameters.medium().mu_31()), // 20

    vkch::Constant<float>(simulation_parameters.medium().beta_01()), // 21
    vkch::Constant<float>(simulation_parameters.medium().beta_10()), // 22
    vkch::Constant<float>(simulation_parameters.medium().beta_11()), // 23
    vkch::Constant<float>(simulation_parameters.medium().beta_20()), // 24
    vkch::Constant<float>(simulation_parameters.medium().beta_21()), // 25
    vkch::Constant<float>(simulation_parameters.medium().beta_30()), // 26
    vkch::Constant<float>(simulation_parameters.medium().beta_31()), // 27
  };

  uintmax_t current_timestep = 0;

  std::shared_ptr<vkch::TensorParameterSet> params_step_01 =
    vkch_ctxt->tensorParameterSet({
      tensor_0,
      tensor_1
    });
  Step_A.params_step_AB = params_step_01;

  std::shared_ptr<vkch::TensorParameterSet> params_step_10 =
    vkch_ctxt->tensorParameterSet({
      tensor_1,
      tensor_0
    });
  Step_B.params_step_AB = params_step_10;

  std::shared_ptr<vkch::TensorParameterSet> params_render_0 =
    vkch_ctxt->tensorParameterSet({
      tensor_0
    });
  Step_A.params_render_A = params_render_0;
  std::shared_ptr<vkch::TensorParameterSet> params_render_1 =
    vkch_ctxt->tensorParameterSet({
      tensor_1
    });
  Step_B.params_render_A = params_render_1;

  std::shared_ptr<vkch::Program> program_step =
    Step_A.program_step = Step_B.program_step =
      vkch_ctxt->program(
        spec_constants_step,
        std::vector<vkch::ConstantBase>({}), // example
        params_step_01, // example
        spirv_solver_substep
      );

#if !defined(NO_GUI)
  if (!no_gui)
  {
    std::vector<vkch::ConstantBase> push_constants_sample_render_example = {
      vkch::Constant<uint32_t>(0.f),
      vkch::Constant<float>(0.f),
      vkch::Constant<float>(0.f),
      vkch::Constant<float>(0.f)
    };

    std::shared_ptr<vkch::Program> program_render =
      Step_A.program_render = Step_B.program_render =
        vkch_ctxt->program(
          spec_constants_step,
          push_constants_sample_render_example, // example
          params_render_0, // example
          spirv_render,
          &(volume_buffers->write_descriptor_set_layout())
        );

  } // (!no_gui)
#endif // !defined(NO_GUI)

/*
  TimeSeriesMeasurementData<float> time_series_measurements;
  time_series_measurements.squared_pressure_timeseries = nullptr;
  time_series_measurements.squared_velocity_timeseries = nullptr;
*/

  Step_A.init_schemas(simulation_parameters, vkch_ctxt);
  Step_B.init_schemas(simulation_parameters, vkch_ctxt);

  Step_A.schedule(
    volume_buffers,
    simulation_parameters, current_timestep++);
  Step_A.submit(true, true, nullptr);
  // Must wait for this to complete to prevent overwriting of the tx_data
  // tensors later?
  Step_A.getLastSchema()->waitForCompletion();
  Step_B.schedule(
    volume_buffers,
    simulation_parameters, current_timestep++);
  Step_B.submit(false, true, Step_A.getLastSchema());

  int steps_until_end_of_transition = 0;
  while (!(*stop_thread))
  {
    Step_A.getLastSchema()->waitForCompletion();
    Simulation::get().perform_measurements(
      tensor_1->data(),
      (current_timestep - 1));

    if (!no_gui)
    {
      if (steps_until_end_of_transition == 0)
      {
        if (volume_buffers->tryTransitionToWriteNew())
        {
          steps_until_end_of_transition = 2;
        }
      } else
      {
        if (!(--steps_until_end_of_transition))
        {
          volume_buffers->completeTransitionToWriteNew();
        }
      }

    } // (!no_gui)

    Step_A.schedule(
      volume_buffers,
      simulation_parameters, current_timestep++);
    Step_A.submit(false, true, Step_B.getLastSchema());

    Step_B.getLastSchema()->waitForCompletion();
    Simulation::get().perform_measurements(
      tensor_0->data(),
      (current_timestep - 1));

#if !defined(NO_GUI)
    if (!no_gui)
    {
      if (steps_until_end_of_transition == 0)
      {
        if (volume_buffers->tryTransitionToWriteNew())
        {
          steps_until_end_of_transition = 2;
        }
      } else
      {
        if (!(--steps_until_end_of_transition))
        {
          volume_buffers->completeTransitionToWriteNew();
        }
      }

    } // (!no_gui)
#endif // !defined(NO_GUI)

    Step_B.schedule(
      volume_buffers,
      simulation_parameters, current_timestep++);
    Step_B.submit(false, true, Step_A.getLastSchema());

  }

  Step_A.getLastSchema()->waitForCompletion();
  Step_B.getLastSchema()->waitForCompletion();

#if !defined(BUILD_PYTHON_BINDINGS)
  fprintf(stderr, "completed shutdown (compute)...\n");
#endif // !defined(BUILD_PYTHON_BINDINGS)

}
