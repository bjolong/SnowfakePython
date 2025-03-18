#pragma once

#include <cstdio>

#include <memory>
#include <mutex>
#include <thread>

#include "Medium.hpp"
#include "SimulationState.h"

#include "renderer/gui.h"
#include "renderer/PersistentGUI.h"

#include "compute.h"

class Simulation
{
  friend void simulation_thread(
    volatile int *stop_thread,
    bool no_gui,
    const std::shared_ptr<VolumeBuffers> &volume_buffers,
    std::shared_ptr<vkch::Context> &vkch_ctxt,
    SimulationParameters const &simulation_parameters);
public:
  inline static void run(
    SimulationParameters const &isimulation_parameters)
  {
    Simulation &simulation = get();

    {
      std::lock_guard<std::mutex> lock(*(simulation.mtx_ptr.get()));

      // Add simulation parameters first, so they exist.
      // Especially for testing.
      simulation._simulation_parameters =
        std::make_shared<SimulationParameters>(isimulation_parameters);

      if (simulation.running)
      {
        fprintf(stderr, "Simulation already in progress...\n");
        return;
      }

      simulation.running = true;
    }

    if (!simulation.simulation_run())
    {
      fprintf(stderr, "Simulation failed to start.\n");

      std::lock_guard<std::mutex> lock(*(simulation.mtx_ptr.get()));

      simulation.running = false;
    }

#if !defined(SIMULATION_STUBS)
    {
      std::lock_guard<std::mutex> lock(*(simulation.mtx_ptr.get()));

      simulation.running = false;
      //simulation._simulation_parameters = nullptr;
    }

    simulation = std::move(Simulation());
#endif // !defined(SIMULATION_STUBS)
  }

  inline static void stop()
  {
    Simulation &simulation = get();

    std::lock_guard<std::mutex> lock(*(simulation.mtx_ptr.get()));

    if (!(simulation.running))
    {
      fprintf(stderr, "Simulation not running.\n");
      return;
    }

    simulation.finish_threads = 1;
  }

  inline static void measurement(
    void (*callback)(
      SimulationState const &sim_state,
      double time,
      void *user_pointer),
    void *callback__user_pointer
  )
  {
    Simulation &simulation = get();

    std::lock_guard<std::mutex> lock(*(simulation.mtx_ptr.get()));

    if (simulation.running)
    {
      fprintf(stderr, "Cannot change measurement function during "
        "simulation.\n");
      return;
    }

    simulation.data_collection_callback = callback;
    simulation.data_collection_callback__user_pointer = callback__user_pointer;
  }

  inline static void *get_measurement_user_pointer()
  {
    Simulation &simulation = get();

    return simulation.data_collection_callback__user_pointer;
  }

  inline static std::shared_ptr<SimulationParameters const> const &simulation_parameters()
  {
    Simulation &simulation = get();

    return simulation._simulation_parameters;
  }

protected:
  inline static Simulation &get()
  {
    static Simulation simulation;
    return simulation;
  }

  bool simulation_run();

  void perform_measurements(float *all_fields, double time) const;

  Simulation(Simulation &&other) = default;
  Simulation &operator=(Simulation &&other) = default;

  Simulation(Simulation const &other) = delete;
  Simulation &operator=(Simulation const &other) = delete;

  void (*data_collection_callback)(
    SimulationState const &sim_state,
    double time,
    void *user_pointer
  );
  void *data_collection_callback__user_pointer;

private:
  Simulation()
    : data_collection_callback(nullptr)
    , data_collection_callback__user_pointer(nullptr)
    , no_gui(false)
    , running(false)
    , mtx_ptr(std::make_unique<std::mutex>())
    , finish_threads(0)
    , _simulation_parameters(nullptr)
    , persistent_gui(nullptr)
    , vkch_ctxt(nullptr)
    , aux_ctxt(nullptr)
    , volume_buffers(nullptr)
  {}

  bool no_gui;
  bool running;
  std::unique_ptr<std::mutex> mtx_ptr;
  std::thread compute_thread;
  volatile int finish_threads;

  std::shared_ptr<SimulationParameters const> _simulation_parameters;

  std::shared_ptr<PersistentGUI> persistent_gui;

  std::shared_ptr<vkch::Context> vkch_ctxt;
  std::shared_ptr<AuxiliaryVulkanContext> aux_ctxt;
  std::shared_ptr<VolumeBuffers> volume_buffers;
};
