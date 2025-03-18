#pragma once

#include <cstdint>
#include <cinttypes>
#include <cstring>

#include <memory>

#include "constants.h"
#include "Medium.hpp"
#include "SeedCrystal.hpp"

#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884197169399375
#endif // M_PI

struct SimulationParameters
{
public:
  inline SimulationParameters()
    : _x_size(DEFAULT_XSIZE)
    , _y_size(DEFAULT_YSIZE)
    , _z_size(DEFAULT_ZSIZE)
  {
    recalculate_radii();
  }

  inline SimulationParameters(
    const Medium &imedia)
    : _medium(imedia)
    , _x_size(DEFAULT_XSIZE)
    , _y_size(DEFAULT_YSIZE)
    , _z_size(DEFAULT_ZSIZE)
  {
    recalculate_radii();
  }
  
  inline void setMedium(Medium const &imedia)
  {
    _medium = imedia;
  }

  inline Medium const &medium() const
  {
    return _medium;
  }

  inline void setSeed(SeedCrystal const &iseed)
  {
    _seed = iseed;
  }

  inline SeedCrystal const &seed() const
  {
    return _seed;
  }

  inline void setVoxelXCount(int ix_size)
  {
    _x_size = ix_size;
    recalculate_radii();
  }

  inline int voxelXCount() const
  {
    return _x_size;
  }

  inline void setVoxelYCount(int iy_size)
  {
    _y_size = iy_size;
    recalculate_radii();
  }

  inline int voxelYCount() const
  {
    return _y_size;
  }

  inline void setVoxelZCount(int iz_size)
  {
    _z_size = iz_size;
    recalculate_radii();
  }

  inline int voxelZCount() const
  {
    return _z_size;
  }

  inline int radiusT() const
  {
    return _radiusT;
  }

  inline int radiusZ() const
  {
    return _radiusZ;
  }

private:

  inline void recalculate_radii()
  {
    // Determine minimum voxel count in dimension
    int mimimum_T = _x_size;
    mimimum_T = (mimimum_T < _y_size) ? mimimum_T : _y_size;
    _radiusT = (mimimum_T / 2) - (1 + BOUNDARY_THICKNESS);

    int minimum_Z = _z_size;
    _radiusZ = (minimum_Z / 2) - (1 + BOUNDARY_THICKNESS);
  }

  Medium _medium;
  SeedCrystal _seed;

  // Voxel sizes
  int _x_size, _y_size, _z_size;
  int _radiusT, _radiusZ;
};
