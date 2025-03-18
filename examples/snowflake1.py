
from SnowfakePython import *
from math import *
import sys

# Default medium is 'canonical' air
medium = Medium()
medium.rho = 0.1
medium.phi = 0.0
medium.kappa = 0.1
medium.mu = 0.001
medium.beta_01 = 2.5
medium.beta_10 = 2.0
medium.beta_11 = 2.0
medium.beta_20 = 2.0
medium.beta_21 = 1.0
medium.beta_30 = 1.0
medium.beta_31 = 1.0

seed_crystal = SeedCrystal()
seed_crystal.thickness = 1
seed_crystal.radius = 2

sim_params = SimulationParameters()
sim_params.medium = medium
sim_params.seed = seed_crystal
sim_params.voxel_x_count = 320
sim_params.voxel_y_count = 320
sim_params.voxel_z_count = 256
#sim_params.voxel_x_count = 64
#sim_params.voxel_y_count = 64
#sim_params.voxel_z_count = 64

class MeasurementData:
  def __init__(self, stoptime : int):
    self.stop_time = stoptime
    pass

# Measurement callback
def measure_callback(sim_state: SimulationState,
  time: float,
  data: MeasurementData):
    #for i in range(10):
    #  print("{:d} {:f} {:f}".format(
    #    int(sim_state.occupancy(0, i, 0)),
    #    sim_state.diffusive_mass(0, i, 0),
    #    sim_state.boundary_mass(0, i, 0)))
    print("{:d}... ".format(int(time)), end='', flush=True)
    if (time >= data.stop_time):
      sim_state.exportSTL("snowflake1.stl")
      Simulation.stop()

measurement_data = MeasurementData(45000)

Simulation.measurement(measure_callback, measurement_data)

Simulation.run(sim_params)
