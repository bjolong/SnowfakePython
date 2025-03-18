#pragma once

#include "constants.h"
#include "SimulationParameters.h"

class SimulationState
{
public:
  inline SimulationState(float const *iall_simulation_fields,
    SimulationParameters const &isimulation_parameters)
    : _fields_ptr(iall_simulation_fields)
    , _simulation_parameters(isimulation_parameters)
  {}

  inline float occupancy(float x, float y, float z) const
  {
    std::array<float, SOLVER_FIELD_COUNT> q_samples = sample_all(x, y, z);

    return q_samples[FIELD_OCCUPANCY];
  }

  inline float diffusive_mass(float x, float y, float z) const
  {
    std::array<float, SOLVER_FIELD_COUNT> q_samples = sample_all(x, y, z);

    return q_samples[FIELD_DIFFUSIVE_MASS];
  }

  inline float boundary_mass(float x, float y, float z) const
  {
    std::array<float, SOLVER_FIELD_COUNT> q_samples = sample_all(x, y, z);

    return q_samples[FIELD_BOUNDARY_MASS];
  }

private:

  inline std::array<float, SOLVER_FIELD_COUNT> sample_all(
    float x, float y, float z) const
  {
    // Construct triple coordinates.
    const float i = (x / sqrt(3.0)) * 2.0;
    const float j = y - 0.5*i;
    const float h = -(i + j);
    const float k = z;

    // Carry out rounding on triple coordinates
    int new_i = round(i);
    int new_j = round(j);
    int new_h = round(h);

    const float frac_i = abs(new_i - i);
    const float frac_j = abs(new_j - j);
    const float frac_h = abs(new_h - h);

    // Detect axial distances and reproject.
    if ((frac_i > frac_j) && (frac_i > frac_h))
    {
      new_i = -(new_j + new_h);
    } else if (frac_j > frac_h)
    {
      new_j = -(new_i + new_h);
    } else
    {
      new_h = -(new_i + new_j);
    }

    const intmax_t x_size = _simulation_parameters.voxelXCount();
    const intmax_t y_size = _simulation_parameters.voxelYCount();
    const intmax_t z_size = _simulation_parameters.voxelZCount();
    const intmax_t radiusT = _simulation_parameters.radiusT();
    const intmax_t radiusZ = _simulation_parameters.radiusZ();

    const int bi = new_i;
    const int bj = new_j;
    const int bk = int(round(k));
    const bool outside_radius_condition =
      (((-(bi+bj)) > radiusT) || ((-bi) > radiusT) || ((-bj) > radiusT) ||
      (((bi+bj) >= radiusT) || ((bi) >= radiusT) || ((bj) >= radiusT)) ||
      ((-bk) > radiusZ) || (bk >= radiusZ));

    if (outside_radius_condition)
    {
      std::array<float, SOLVER_FIELD_COUNT> samples = {
        0.f,
        float(_simulation_parameters.medium().rho()),
        0.f
      };
      return samples;
    }

    int64_t di = bi + (x_size / 2);
    int64_t dj = bj + (y_size / 2);
    int64_t dk = bk + (z_size / 2);

    int64_t idx =
      dk * ((int64_t)y_size) * ((int64_t)x_size) +
      dj * ((int64_t)x_size) +
      di;

    const intmax_t total_size = x_size*y_size*z_size;

    std::array<float, SOLVER_FIELD_COUNT> retvals;
    for (int m = 0; m < SOLVER_FIELD_COUNT; m++)
    {
      retvals[m] = _fields_ptr[m*total_size + idx];
    } // m

    return retvals;
  }

  inline void writeout_stl_pass(
    FILE *FP, float const *occupancy, int64_t *output_counter)
  {
    const intmax_t x_size = _simulation_parameters.voxelXCount();
    const intmax_t y_size = _simulation_parameters.voxelYCount();
    const intmax_t z_size = _simulation_parameters.voxelZCount();
    const intmax_t radiusT = _simulation_parameters.radiusT();
    const intmax_t radiusZ = _simulation_parameters.radiusZ();

    int64_t counter = 0;
    for (int64_t iz = 1; iz < (z_size - 1); iz++)
      for (int64_t iy = 1; iy < (y_size - 1); iy++)
        for (int64_t ix = 1; ix < (x_size - 1); ix++)
        {
          int64_t bk = iz - (z_size / 2);
          int64_t bj = iy - (y_size / 2);
          int64_t bi = ix - (x_size / 2);
  
          const bool outside_radius_condition =
            (((-(bi+bj)) > radiusT) || ((-bi) > radiusT) || ((-bj) > radiusT) ||
            (((bi+bj) >= radiusT) || ((bi) >= radiusT) || ((bj) >= radiusT)) ||
            ((-bk) > radiusZ) || (bk >= radiusZ));
          const int radiusT_plus_boundary = radiusT + BOUNDARY_THICKNESS;
          const int radiusZ_plus_boundary = radiusZ + BOUNDARY_THICKNESS;
          const bool outside_boundary_condition =
            (((-(bi+bj)) > radiusT_plus_boundary) ||
            ((-bi) > radiusT_plus_boundary) ||
            ((-bj) > radiusT_plus_boundary) ||
            (((bi+bj) >= radiusT_plus_boundary) ||
            ((bi) >= radiusT_plus_boundary) ||
            ((bj) >= radiusT_plus_boundary)) ||
            ((-bk) > radiusZ_plus_boundary) ||
            (bk >= radiusZ_plus_boundary));
  
          if (outside_boundary_condition) continue;
  
          int64_t idx =
            iz * ((int64_t)y_size) * ((int64_t)x_size) +
            iy * ((int64_t)x_size) +
            ix;
  
          if (occupancy[idx])
          {
            for (int a = 0; a < 6; a++)
            {
              bool occ_test = false;
              switch (a)
              {
                case 0: occ_test = (occupancy[idx + 1] == 0); break;
                case 1: occ_test = (occupancy[idx + ((int64_t)x_size)] == 0); break;
                case 2: occ_test = (occupancy[idx + ((int64_t)x_size) - 1] == 0); break;
                case 3: occ_test = (occupancy[idx - 1] == 0); break;
                case 4: occ_test = (occupancy[idx - ((int64_t)x_size)] == 0); break;
                case 5: occ_test = (occupancy[(idx - ((int64_t)x_size)) + 1] == 0); break;
              }
              if (occ_test)
              {
                if (output_counter != nullptr)
                {
                  counter += 2;
                } else // (output_counter != nullptr)
                {
                  const float nrm_x = cos((2.f * M_PI / 6.f) * (a + 0.5f));
                  const float nrm_y = sin((2.f * M_PI / 6.f) * (a + 0.5f));
                  const float nrm_z = 0.f;

                  const float offx0 = (1.f / sqrt(3.f)) * cos((2.f * M_PI / 6.f) * (a + 0.f));
                  const float offy0 = (1.f / sqrt(3.f)) * sin((2.f * M_PI / 6.f) * (a + 0.f));
                  const float offx1 = (1.f / sqrt(3.f)) * cos((2.f * M_PI / 6.f) * (a + 1.f));
                  const float offy1 = (1.f / sqrt(3.f)) * sin((2.f * M_PI / 6.f) * (a + 1.f));

                  const float vx = ix * (sqrt(3.f) / 2.f);
                  const float vy = iy + 0.5 * ix;
                  const float vz = iz;

                  const float vtx_01_x = vx + offx0;
                  const float vtx_01_y = vy + offy0;
                  const float vtx_0_z = vz;
                  const float vtx_1_z = vz + 1.f;
                  const float vtx_23_x = vx + offx1;
                  const float vtx_23_y = vy + offy1;
                  const float vtx_2_z = vz;
                  const float vtx_3_z = vz + 1.f;

                  fwrite(&nrm_x, sizeof(float), 1, FP);
                  fwrite(&nrm_y, sizeof(float), 1, FP);
                  fwrite(&nrm_z, sizeof(float), 1, FP);
                  fwrite(&vtx_01_x, sizeof(float), 1, FP);
                  fwrite(&vtx_01_y, sizeof(float), 1, FP);
                  fwrite(&vtx_0_z, sizeof(float), 1, FP);
                  fwrite(&vtx_23_x, sizeof(float), 1, FP);
                  fwrite(&vtx_23_y, sizeof(float), 1, FP);
                  fwrite(&vtx_3_z, sizeof(float), 1, FP);
                  fwrite(&vtx_01_x, sizeof(float), 1, FP);
                  fwrite(&vtx_01_y, sizeof(float), 1, FP);
                  fwrite(&vtx_1_z, sizeof(float), 1, FP);

                  uint16_t o = 0;
                  fwrite(&o, sizeof(uint16_t), 1, FP);
              
                  fwrite(&nrm_x, sizeof(float), 1, FP);
                  fwrite(&nrm_y, sizeof(float), 1, FP);
                  fwrite(&nrm_z, sizeof(float), 1, FP);
                  fwrite(&vtx_01_x, sizeof(float), 1, FP);
                  fwrite(&vtx_01_y, sizeof(float), 1, FP);
                  fwrite(&vtx_0_z, sizeof(float), 1, FP);
                  fwrite(&vtx_23_x, sizeof(float), 1, FP);
                  fwrite(&vtx_23_y, sizeof(float), 1, FP);
                  fwrite(&vtx_2_z, sizeof(float), 1, FP);
                  fwrite(&vtx_23_x, sizeof(float), 1, FP);
                  fwrite(&vtx_23_y, sizeof(float), 1, FP);
                  fwrite(&vtx_3_z, sizeof(float), 1, FP);

                  fwrite(&o, sizeof(uint16_t), 1, FP);

                } // else (output_counter != nullptr)

              } // (occ_test)
  
            } // a
  
            for (int a = 0; a < 2; a ++)
            {
              bool occ_test = false;
              switch (a)
              {
                case 0: occ_test = (occupancy[idx - ((int64_t)y_size) * ((int64_t)x_size)] == 0); break;
                case 1: occ_test = (occupancy[idx + ((int64_t)y_size) * ((int64_t)x_size)] == 0); break;
              }
              if (occ_test)
              {
                if (output_counter != nullptr)
                {
                  counter += 4;
                } else // (output_counter != nullptr)
                {
                  int vtxs_0[] = {
                    0, 2, 1,
                    0, 3, 2,
                    0, 4, 3,
                    0, 5, 4
                  };
                  int vtxs_1[] = {
                    0, 1, 2,
                    0, 2, 3,
                    0, 3, 4,
                    0, 4, 5
                  };
                  int *vtxs = a ? vtxs_1 : vtxs_0;

                  for (int p = 0; p < 4; p++)
                  {
                    const float nrm_xy = 0.f;
                    const float nrm_z = a ? +1.f : -1.f;

                    const float vx = ix * (sqrt(3.f) / 2.f);
                    const float vy = iy + 0.5 * ix;
                    const float vz = iz;
  
                    fwrite(&nrm_xy, sizeof(float), 1, FP);
                    fwrite(&nrm_xy, sizeof(float), 1, FP);
                    fwrite(&nrm_z, sizeof(float), 1, FP);
  
                    for (int l = 0; l < 3; l++)
                    {
                      const float offx0 =
                        (1.f / sqrt(3.f)) * cos((2.f * M_PI / 6.f) * (vtxs[p*3 + l] + 0.f));
                      const float offy0 =
                        (1.f / sqrt(3.f)) * sin((2.f * M_PI / 6.f) * (vtxs[p*3 + l] + 0.f));
                      const float vtx_x = vx + offx0;
                      const float vtx_y = vy + offy0;
                      const float vtx_z = vz + a;

                      fwrite(&vtx_x, sizeof(float), 1, FP);
                      fwrite(&vtx_y, sizeof(float), 1, FP);
                      fwrite(&vtx_z, sizeof(float), 1, FP);
  
                    } // l

                    uint16_t o = 0;
                    fwrite(&o, sizeof(uint16_t), 1, FP);
  
                  } // p
                
                } // else (output_counter != nullptr
    
              } // (occ_test)
  
            } // a
  
          } // (occupancy[idx])
  
        } // ix
  
    if (output_counter != nullptr)
      (*output_counter) = counter;

  }

public:
  inline void exportSTL(std::string const &filename)
  {
    FILE *FP = fopen(filename.c_str(), "wb");

    if (FP == nullptr)
    {
      fprintf(stderr, "could not open file '%s' for writing\n", filename.c_str());
      return;
    }

    char header[80];
    memset(&(header[0]), 0, sizeof(char) * 80);
    strcpy(&(header[0]), "generated snowfake STL");

    fwrite(&(header[0]), sizeof(char), 80, FP);

    int64_t counter;
    writeout_stl_pass(nullptr, _fields_ptr, &counter);

    uint32_t int_counter = counter;
    fwrite(&int_counter, sizeof(uint32_t), 1, FP);

    writeout_stl_pass(FP, _fields_ptr, nullptr);

    fclose(FP);

  }
  
private:
  float const *_fields_ptr;
  SimulationParameters const &_simulation_parameters;
};
