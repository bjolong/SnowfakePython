
#include "aux_vulkan.h"
#include "Simulation.hpp"

void Simulation::perform_measurements(float *all_fields, double time) const
{
  SimulationState sim_state(all_fields, *_simulation_parameters);

  if (data_collection_callback != nullptr)
  {
    (*data_collection_callback)(
      sim_state,
      time,
      data_collection_callback__user_pointer
    );
  } // (data_collection_callback != nullptr)
}

#if defined(SIMULATION_STUBS)
// Cannot run simulation, functionality stubbed out.
bool Simulation::simulation_run() { return false; }
#else // defined(SIMULATION_STUBS)

bool Simulation::simulation_run()
{
  aux_ctxt = std::make_shared<AuxiliaryVulkanContext>();
  aux_ctxt->width = 0;
  aux_ctxt->height = 0;
  aux_ctxt->window = nullptr;
  aux_ctxt->no_gui = no_gui;

#if !defined(NO_GUI)

  if (!no_gui)
  {
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    const char *window_name = "Acoustic volume simulator";

    // Target the correct monitor.
    const int displayIndex = 0;
    // Get maximum resolution mode
    SDL_DisplayMode displayMode;
    if (SDL_GetDisplayMode(displayIndex, 0, &displayMode) != 0)
    {
      fprintf(stderr, "SDL_GetDisplayMode failed: %s\n", SDL_GetError());
      displayMode.w = 800;
      displayMode.h = 800;
    }

    aux_ctxt->width = (displayMode.w > 800) ? 800 : displayMode.w;
    aux_ctxt->height = (displayMode.h > 800) ? 800 : displayMode.h;

    aux_ctxt->window = SDL_CreateWindow(
      window_name,
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      aux_ctxt->width,
      aux_ctxt->height,
      SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    if (aux_ctxt->window == NULL)
    {
      fprintf(stderr, "SDL window failed to initialise: %s\n", SDL_GetError());
    }

    SDL_ShowWindow(aux_ctxt->window);

  } // (!no_gui)

#endif // else defined(NO_GUI)

  if (vkch_ctxt == nullptr)
  {
    vkch_ctxt =
      vkch::Context::create(
        (aux_ctxt->no_gui) ? nullptr : &instance_extensions_selection_callback,
        reinterpret_cast<void *>(aux_ctxt.get()),
        nullptr, nullptr,
        (aux_ctxt->no_gui) ? nullptr : &physical_device_selection_callback,
        reinterpret_cast<void *>(aux_ctxt.get()),
        (aux_ctxt->no_gui) ? nullptr : &queue_family_index_selection_callback,
        reinterpret_cast<void *>(aux_ctxt.get()),
        (aux_ctxt->no_gui) ? nullptr : &device_extension_selection_callback,
        reinterpret_cast<void *>(aux_ctxt.get())
      );
  } else
  {
#if !defined(NO_GUI)
    if (!no_gui)
    {
      // Create surface for rendering
      SDL_Vulkan_CreateSurface(aux_ctxt->window, vkch_ctxt->instance(),
        &(aux_ctxt->surface));
    } // (!no_gui)
#endif // !defined(NO_GUI)

  }
  vkch_ctxt->clear();

  finish_threads = 0;

#if !defined(NO_GUI)

  persistent_gui = std::make_shared<PersistentGUI>(nullptr);

  if (!no_gui)
  {
    volume_buffers =
      std::make_shared<VolumeBuffers>(
        *(vkch_ctxt),
        static_cast<unsigned int>(
          _simulation_parameters->voxelXCount()),
        static_cast<unsigned int>(
          _simulation_parameters->voxelYCount()),
        static_cast<unsigned int>(
          _simulation_parameters->voxelZCount())
      );

    initialise_gui(
      volume_buffers,
      vkch_ctxt,
      aux_ctxt.get(),
      *_simulation_parameters,
      *(persistent_gui.get())
    );
  }
#endif // !defined(NO_GUI)

  compute_thread = std::thread(
    [&]()
    {
      simulation_thread(&(finish_threads),
        no_gui,
        volume_buffers,
        vkch_ctxt,
        *(_simulation_parameters.get()));
    }
  );

#if !defined(NO_GUI)
  if (!no_gui)
  {

    while (!(finish_threads))
    {

      gui_process_events(
        &finish_threads,
        volume_buffers,
        vkch_ctxt,
        aux_ctxt.get(),
        *_simulation_parameters,
        *(persistent_gui.get()));

    } // (!(finish_threads))

    gui_process_wait_for_completion(
      vkch_ctxt,
      aux_ctxt.get(),
      *(persistent_gui.get()));

    finish_threads = 1;
  
  } else
#endif // !defined(NO_GUI)
  {
    while (!(finish_threads));
  }

  if (compute_thread.joinable())
    compute_thread.join();

  vkch_ctxt->device().waitIdle();

  persistent_gui = nullptr;

#if !defined(NO_GUI)
  if (!no_gui)
  {
    vkDestroySurfaceKHR(vkch_ctxt->instance(), aux_ctxt->surface, nullptr);

    SDL_DestroyWindow(aux_ctxt->window);

    SDL_Quit();
  }
#endif // !defined(NO_GUI)

  aux_ctxt = nullptr;
  volume_buffers = nullptr;
  vkch_ctxt = nullptr;

  return true;
}

#endif // else defined(SIMULATION_STUBS)
