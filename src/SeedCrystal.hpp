#pragma once

#include <cmath>
#include "solver_defaults.h"

struct SeedCrystal
{
public:
  SeedCrystal()
    : _radius(2)
    , _thickness(1)
  {
  }

  int radius() const { return _radius; }
  int thickness() const { return _thickness; }

  void set_radius(int iradius) { _radius = iradius; }
  void set_thickness(int ithickness) { _thickness = ithickness; }

private:
  int _radius;
  int _thickness;
};
