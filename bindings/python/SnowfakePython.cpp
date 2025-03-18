
#include <cstdlib>
#include <cstring>
#include <functional>

#include <nanobind/nanobind.h>
#include <nanobind/operators.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/complex.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/unique_ptr.h>
#include <nanobind/ndarray.h>

#include "Simulation.hpp"
#include "Medium.hpp"

namespace nb = nanobind;
using namespace nb::literals;

struct Intermediaries_Measure
{
  std::function<void(
      SimulationState const &sim_state,
      double time,
      nb::object &python_object
    )> python_callback;
  nb::object *python_object_ptr;
};

NB_MODULE(SnowfakePython, m)
{
  nb::set_leak_warnings(false);

  m.doc() =
    R"(
    Python API to an implementation of a mesoscale snowflake crystallisation
    simulation on GPU through the Vulkan API.
    )";
    
  nb::class_<Medium>(m, "Medium")
    .def(nb::init<>(),
      R"(
      Initialise a new default snowflake crystallisation medium. This
      necessarily must have `rho` the density of the medium. The globals
      `kappa`, `mu` and `beta` may be set as global or per-neighbour forms.
      These start as sensible defaults.
      )")
    .def_prop_rw("rho",
      &Medium::rho, &Medium::set_rho,
      R"(
      The vapour density of the of the medium, `rho`.
      )")
    .def_prop_rw("phi",
      &Medium::phi, &Medium::set_phi,
      R"(
      The drift of the of the medium, `phi`. This is currently unused.
      )")
    .def_prop_rw("kappa",
      &Medium::kappa, &Medium::set_kappa,
      R"(
      The variable `kappa` is the freezing rate restriction factor, and can vary
      between neighbour configurations. This property gets an average value and
      sets all possible neighbour configurations to the same value.
      )")
    .def_prop_rw("kappa_01",
      &Medium::kappa_01, &Medium::set_kappa_01,
      R"(
      The variable `kappa_01` is the freezing rate restriction factor for the
      zero in-plane neighbour and one out-of-place neighbour case.
      )")
    .def_prop_rw("kappa_10",
      &Medium::kappa_10, &Medium::set_kappa_10,
      R"(
      The variable `kappa_10` is the freezing rate restriction factor for the
      one in-plane neighbour and zero out-of-place neighbours case.
      )")
    .def_prop_rw("kappa_11",
      &Medium::kappa_11, &Medium::set_kappa_11,
      R"(
      The variable `kappa_11` is the freezing rate restriction factor for the
      one in-plane neighbour and one out-of-place neighbour case.
      )")
    .def_prop_rw("kappa_20",
      &Medium::kappa_20, &Medium::set_kappa_20,
      R"(
      The variable `kappa_20` is the freezing rate restriction factor for the
      two in-plane neighbours and zero out-of-place neighbours case.
      )")
    .def_prop_rw("kappa_21",
      &Medium::kappa_21, &Medium::set_kappa_21,
      R"(
      The variable `kappa_21` is the freezing rate restriction factor for the
      two in-plane neighbours and one out-of-place neighbour case.
      )")
    .def_prop_rw("kappa_30",
      &Medium::kappa_30, &Medium::set_kappa_30,
      R"(
      The variable `kappa_30` is the freezing rate restriction factor for the
      three in-plane neighbours and zero out-of-place neighbours case.
      )")
    .def_prop_rw("kappa_31",
      &Medium::kappa_31, &Medium::set_kappa_31,
      R"(
      The variable `kappa_31` is the freezing rate restriction factor for the
      three in-plane neighbours and one out-of-place neighbour case.
      )")
    .def_prop_rw("mu",
      &Medium::mu, &Medium::set_mu,
      R"(
      The variable `mu` is the freezing rate restriction factor, and can vary
      between neighbour configurations. This property gets an average value and
      sets all possible neighbour configurations to the same value.
      )")
    .def_prop_rw("mu_01",
      &Medium::mu_01, &Medium::set_mu_01,
      R"(
      The variable `mu_01` is the melting rate factor for the
      zero in-plane neighbour and one out-of-place neighbour case.
      )")
    .def_prop_rw("mu_10",
      &Medium::mu_10, &Medium::set_mu_10,
      R"(
      The variable `mu_10` is the melting rate factor for the
      one in-plane neighbour and zero out-of-place neighbours case.
      )")
    .def_prop_rw("mu_11",
      &Medium::mu_11, &Medium::set_mu_11,
      R"(
      The variable `mu_11` is the melting rate factor for the
      one in-plane neighbour and one out-of-place neighbour case.
      )")
    .def_prop_rw("mu_20",
      &Medium::mu_20, &Medium::set_mu_20,
      R"(
      The variable `mu_20` is the melting rate factor for the
      two in-plane neighbours and zero out-of-place neighbours case.
      )")
    .def_prop_rw("mu_21",
      &Medium::mu_21, &Medium::set_mu_21,
      R"(
      The variable `mu_21` is the melting rate factor for the
      two in-plane neighbours and one out-of-place neighbour case.
      )")
    .def_prop_rw("mu_30",
      &Medium::mu_30, &Medium::set_mu_30,
      R"(
      The variable `mu_30` is the melting rate factor for the
      three in-plane neighbours and zero out-of-place neighbours case.
      )")
    .def_prop_rw("mu_31",
      &Medium::mu_31, &Medium::set_mu_31,
      R"(
      The variable `mu_31` is the melting rate factor for the
      three in-plane neighbours and one out-of-place neighbour case.
      )")
    .def_prop_rw("beta",
      &Medium::beta, &Medium::set_beta,
      R"(
      The variable `beta` is the attachment threshold, and usually varies
      between neighbour configurations. This property gets an average value and
      sets all possible neighbour configurations to the same value.
      )")
    .def_prop_rw("beta_01",
      &Medium::beta_01, &Medium::set_beta_01,
      R"(
      The variable `beta_01` is the attachment threshold for the zero in-plane
      neighbour and one out-of-place neighbour case.
      )")
    .def_prop_rw("beta_10",
      &Medium::beta_10, &Medium::set_beta_10,
      R"(
      The variable `beta_10` is the attachment threshold for the one in-plane
      neighbour and zero out-of-place neighbours case.
      )")
    .def_prop_rw("beta_11",
      &Medium::beta_11, &Medium::set_beta_11,
      R"(
      The variable `beta_11` is the attachment threshold for the one in-plane
      neighbour and one out-of-place neighbour case.
      )")
    .def_prop_rw("beta_20",
      &Medium::beta_20, &Medium::set_beta_20,
      R"(
      The variable `beta_20` is the attachment threshold for the two in-plane
      neighbours and zero out-of-place neighbours case.
      )")
    .def_prop_rw("beta_21",
      &Medium::beta_21, &Medium::set_beta_21,
      R"(
      The variable `beta_21` is the attachment threshold for the two in-plane
      neighbours and one out-of-place neighbour case.
      )")
    .def_prop_rw("beta_30",
      &Medium::beta_30, &Medium::set_beta_30,
      R"(
      The variable `beta_30` is the attachment threshold for the three in-plane
      neighbours and zero out-of-place neighbours case.
      )")
    .def_prop_rw("beta_31",
      &Medium::beta_31, &Medium::set_beta_31,
      R"(
      The variable `beta_31` is the attachment threshold for the three in-plane
      neighbours and one out-of-place neighbour case.
      )");

  nb::class_<SeedCrystal>(m, "SeedCrystal")
    .def(nb::init<>(),
      R"(
      Initialise a default seed crystal.
      )")
    .def_prop_rw("radius",
      &SeedCrystal::radius,
      &SeedCrystal::set_radius,
      R"(
      Set the radius of the seed crystal to generate in mesoscale voxels.
      )")
    .def_prop_rw("thickness",
      &SeedCrystal::thickness,
      &SeedCrystal::set_thickness,
      R"(
      Set the thickness of the seed crystal to generate in mesoscale voxels.
      )");

  nb::class_<SimulationParameters>(m, "SimulationParameters")
    .def(nb::init<>(),
      R"(
      Initialise a set of simulation parameters.
      )")
    .def_prop_rw("medium",
      &SimulationParameters::medium,
      &SimulationParameters::setMedium,
      R"(
      Set the medium for the simulation to create.
      )")
    .def_prop_rw("seed",
      &SimulationParameters::seed,
      &SimulationParameters::setSeed,
      R"(
      Set the seed crystal to begin the simulation.
      )")
    .def_prop_rw("voxel_x_count",
      &SimulationParameters::voxelXCount,
      &SimulationParameters::setVoxelXCount,
      R"(
      The number of voxels in the simulation extending along the x direction.
      )")
    .def_prop_rw("voxel_y_count",
      &SimulationParameters::voxelYCount,
      &SimulationParameters::setVoxelYCount,
      R"(
      The number of voxels in the simulation extending along the y direction.
      )")
    .def_prop_rw("voxel_z_count",
      &SimulationParameters::voxelZCount,
      &SimulationParameters::setVoxelZCount,
      R"(
      The number of voxels in the simulation extending along the z direction.
      )")
    .def_prop_ro("radiusT",
      &SimulationParameters::radiusT,
      R"(
      The number of hexagonal prism voxels in each direction in the T-plane.
      )")
    .def_prop_ro("radiusZ",
      &SimulationParameters::radiusZ,
      R"(
      The number of hexagonal prism voxels in each Z direction.
      )");
  
  // Maybe add getSTL and getOBJ etc. here.
  nb::class_<SimulationState>(m, "SimulationState")
    .def("occupancy",
      &SimulationState::occupancy,
      "x"_a,
      "y"_a,
      "z"_a,
      R"(
      Obtain the crystal occupancy from the given point in the physical fields
      present in simulation. This is 1.0 if a crystal unit is present, 0.0
      otherwise.
      )")
    .def("diffusive_mass",
      &SimulationState::diffusive_mass,
      "x"_a,
      "y"_a,
      "z"_a,
      R"(
      Obtain the diffusive mass from the given point in the physical fields
      present in simulation.
      )")
    .def("boundary_mass",
      &SimulationState::boundary_mass,
      "x"_a,
      "y"_a,
      "z"_a,
      R"(
      Obtain the boundary mass from the given point in the physical fields
      present in simulation.
      )")
    .def("exportSTL",
      &SimulationState::exportSTL,
      "filename"_a,
      R"(
      Export a binary STL file of the current crystal state in the simulation.
      )");
  
  nb::class_<Simulation>(m, "Simulation")
    .def_prop_ro_static("simulation_parameters", [](nb::handle){
        return Simulation::simulation_parameters();
      },
      R"(
      The simulation parameters of the currently running or last run simulation.
      )")
    .def_static("run",
      &Simulation::run,
      nb::call_guard<nb::gil_scoped_release>(),
      "simulation_parameters"_a,
      R"(
      Start a new simulation with the simulation parameters in the given medium.
      )")
    .def_static("stop",
      &Simulation::stop,
      R"(
      Stop a currently running simulation, usually used from within a callback.
      )")
    .def_static("measurement",
      [](
        std::function<void(
          SimulationState const &sim_state,
          double time,
          nb::object &python_object)> callback,
        nb::object &python_object
      ) -> void
      {
        auto cpp_callback = [](
          SimulationState const &sim_state,
          double time,
          void *user_pointer) -> void
        {
          if (user_pointer == nullptr) return;

          struct Intermediaries_Measure *intermediaries_measure =
            reinterpret_cast<struct Intermediaries_Measure *>(user_pointer);

          std::function<void(
            SimulationState const &sim_state,
            double time,
            nb::object &python_object
          )> outer_callback =
            intermediaries_measure->python_callback;

          if (outer_callback != nullptr)
          {
            outer_callback(
              sim_state,
              time,
              *(intermediaries_measure->python_object_ptr)
            );
          } // (outer_callback != nullptr)
        };

        if (Simulation::get_measurement_user_pointer())
        {
          struct Intermediaries_Measure *intermediaries_measure =
              reinterpret_cast<struct Intermediaries_Measure *>(
                  Simulation::get_measurement_user_pointer());

          delete intermediaries_measure->python_object_ptr;
          delete intermediaries_measure;
        }

        struct Intermediaries_Measure *intermediaries_measure =
          new struct Intermediaries_Measure();
        
        intermediaries_measure->python_callback = callback;
        intermediaries_measure->python_object_ptr = new nb::object(python_object);

        {
          nb::gil_scoped_release release;
          Simulation::measurement(
            cpp_callback, intermediaries_measure);
        }
      });
    
  }
