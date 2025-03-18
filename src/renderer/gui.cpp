
#include <iostream>
#include <sstream>
#include <fstream>
#include <istream>
#include <mutex>
#include <limits>

#include <chrono>
#include <thread>

#include <cmath>

#include "SimulationParameters.h"
#include "Simulation.hpp"

#include "GraphicsPipeline.h"
#include "ImageData.h"
#include "SwapchainData.h"
#include "VolumeBuffers.h"
#include "FrameData.h"

#include "PersistentGUI.h"
#include "gui.h"

void updateOrthoProjection(int width, int height, struct Scene &scene)
{
  const float left = -width / 2.;
  const float right = +width / 2.;
  const float top = -height / 2.;
  const float bottom = +height / 2.;
  const float farVal = 1.f;
  const float nearVal = -1.f;

  scene.ortho_matrix[0] = 2.f / (right - left);
  scene.ortho_matrix[1] = 0.f;
  scene.ortho_matrix[2] = 0.f;
  scene.ortho_matrix[3] = 0.f;

  scene.ortho_matrix[4] = 0.f;
  scene.ortho_matrix[5] = 2.f / (top - bottom);
  scene.ortho_matrix[6] = 0.f;
  scene.ortho_matrix[7] = 0.f;

  scene.ortho_matrix[8] = 0.f;
  scene.ortho_matrix[9] = -2.f / (farVal - nearVal);
  scene.ortho_matrix[10] = 0.f;
  scene.ortho_matrix[11] = 0.f;

  scene.ortho_matrix[12] = -(right + left) / (right - left);
  scene.ortho_matrix[13] = -(top + bottom) / (top - bottom);
  scene.ortho_matrix[14] = -(farVal + nearVal) / (farVal - nearVal);
  scene.ortho_matrix[15] = 1.f;
}

void updateModelView(struct Scene &scene)
{
  const float F_un_x = scene.centrepos[0] - scene.eyepos[0];
  const float F_un_y = scene.centrepos[1] - scene.eyepos[1];
  const float F_un_z = scene.centrepos[2] - scene.eyepos[2];

  const float F_sqrt = sqrt(F_un_x*F_un_x + F_un_y*F_un_y + F_un_z*F_un_z);
  const float F_rcp_sqrt = (F_sqrt > 0.f) ? (1.f / F_sqrt) : 0.f;

  const float F_x = F_un_x * F_rcp_sqrt;
  const float F_y = F_un_y * F_rcp_sqrt;
  const float F_z = F_un_z * F_rcp_sqrt;

  const float UP_un_x = scene.upvec[0];
  const float UP_un_y = scene.upvec[1];
  const float UP_un_z = scene.upvec[2];

  const float UP_sqrt = sqrt(UP_un_x*UP_un_x + UP_un_y*UP_un_y + UP_un_z*UP_un_z);
  const float UP_rcp_sqrt = (UP_sqrt > 0.f) ? (1.f / UP_sqrt) : 0.f;

  const float UP_x = UP_un_x * UP_rcp_sqrt;
  const float UP_y = UP_un_y * UP_rcp_sqrt;
  const float UP_z = UP_un_z * UP_rcp_sqrt;

  const float s_x = F_y * UP_z - F_z * UP_y;
  const float s_y = F_z * UP_x - F_x * UP_z;
  const float s_z = F_x * UP_y - F_y * UP_x;

  const float u_x = s_y * F_z - s_z * F_y;
  const float u_y = s_z * F_x - s_x * F_z;
  const float u_z = s_x * F_y - s_y * F_x;

  scene.lookat_matrix[0] = s_x;
  scene.lookat_matrix[1] = u_x;
  scene.lookat_matrix[2] = -F_x;
  scene.lookat_matrix[3] = 0.f;

  scene.lookat_matrix[4] = s_y;
  scene.lookat_matrix[5] = u_y;
  scene.lookat_matrix[6] = -F_y;
  scene.lookat_matrix[7] = 0.f;

  scene.lookat_matrix[8] = s_z;
  scene.lookat_matrix[9] = u_z;
  scene.lookat_matrix[10] = -F_z;
  scene.lookat_matrix[11] = 0.f;

  scene.lookat_matrix[12] = -(
    scene.eyepos[0] * s_x +
    scene.eyepos[1] * s_y +
    scene.eyepos[2] * s_z);
  scene.lookat_matrix[13] = -(
    scene.eyepos[0] * u_x +
    scene.eyepos[1] * u_y +
    scene.eyepos[2] * u_z);
  scene.lookat_matrix[14] =
    scene.eyepos[0] * F_x +
    scene.eyepos[1] * F_y +
    scene.eyepos[2] * F_z;
  scene.lookat_matrix[15] = 1.f;
}

void updateProjection(int width, int height, struct Scene &scene)
{
  const float fovy = 45.0f;
  const float aspect = ((float)width) / ((float)height);
  const float zNear = 0.1f;
  const float zFar = 10.0f;
  // Build perspective projection matrix
  const float f = 1. / std::tan(M_PI * 0.5f * fovy / 180.0f);
  const float rcp_near_m_far = 1. / (zNear - zFar);

  scene.persp_matrix[0] = f / aspect;
  scene.persp_matrix[1] = 0.f;
  scene.persp_matrix[2] = 0.f;
  scene.persp_matrix[3] = 0.f;

  scene.persp_matrix[4] = 0.f;
  scene.persp_matrix[5] = f;
  scene.persp_matrix[6] = 0.f;
  scene.persp_matrix[7] = 0.f;

  scene.persp_matrix[8] = 0.f;
  scene.persp_matrix[9] = 0.f;
  scene.persp_matrix[10] = (zFar + zNear) * rcp_near_m_far;
  scene.persp_matrix[11] = -1.f;

  scene.persp_matrix[12] = 0.f;
  scene.persp_matrix[13] = 0.f;
  scene.persp_matrix[14] = 2.f * zFar * zNear * rcp_near_m_far;
  scene.persp_matrix[15] = 0.f;
}

void updateVolumeVertices(struct Scene &scene)
{
  float avtx_x[8], avtx_y[8], avtx_z[8];
  float atxc_x[8], atxc_y[8], atxc_z[8];
  int idx = 0;
  for (size_t ak = 0; ak < 2; ak++)
  {
    const float z_extent = scene.volume_bound_z_max -
      scene.volume_bound_z_min;
    const float txc_z = ((ak * z_extent + scene.volume_bound_z_min) /
      (float(scene.volume_zsze)));
    const float vtx_z = txc_z - 0.5f;
    for (size_t aj = 0; aj < 2; aj++)
    {
      const float y_extent = scene.volume_bound_y_max -
        scene.volume_bound_y_min;
      const float txc_y = ((aj * y_extent + scene.volume_bound_y_min) /
        (float(scene.volume_ysze)));
      const float vtx_y = txc_y - 0.5f;
      for (size_t ai = 0; ai < 2; ai++)
      {
        const float x_extent = scene.volume_bound_x_max -
          scene.volume_bound_x_min;
        const float txc_x = ((ai * x_extent + scene.volume_bound_x_min) /
          (float(scene.volume_xsze)));
        const float vtx_x = txc_x - 0.5f;

        avtx_x[idx] = vtx_x;
        avtx_y[idx] = vtx_y;
        avtx_z[idx] = vtx_z;
        atxc_x[idx] = txc_x;
        atxc_y[idx] = txc_y;
        atxc_z[idx] = txc_z;

        idx++;

      } // ai
    } // aj
  } // ak

  int indices[] = {
    1, 3, 5, 5, 3, 7,
    0, 4, 2, 2, 4, 6,
    3, 1, 2, 2, 1, 0,
    7, 6, 5, 5, 6, 4,
    7, 3, 6, 6, 3, 2,
    5, 4, 1, 1, 4, 0
  };

  idx = 0;
  for (int i = 0; i < (sizeof(indices)/sizeof(indices[0])); i++)
  {
    scene.volm_vertices[idx++] = avtx_x[indices[i]];
    scene.volm_vertices[idx++] = avtx_y[indices[i]];
    scene.volm_vertices[idx++] = avtx_z[indices[i]];
    scene.volm_vertices[idx++] = 0.f;
    scene.volm_vertices[idx++] = atxc_x[indices[i]];
    scene.volm_vertices[idx++] = atxc_y[indices[i]];
    scene.volm_vertices[idx++] = atxc_z[indices[i]];
    scene.volm_vertices[idx++] = 1.f;
  }
}

void updateLineVertices(struct Scene &scene)
{
  const float v = 0.1;
  int idx = 0;
  for (size_t ak = 0; ak < 2; ak++)
  {
    const float z_extent = scene.volume_bound_z_max -
      scene.volume_bound_z_min;
    const float txc_z = ((ak * z_extent + scene.volume_bound_z_min) /
      (float(scene.volume_zsze)));
    const float vtx_z = txc_z - 0.5f;
    for (size_t aj = 0; aj < 2; aj++)
    {
      const float y_extent = scene.volume_bound_y_max -
        scene.volume_bound_y_min;
      const float txc_y = ((aj * y_extent + scene.volume_bound_y_min) /
        (float(scene.volume_ysze)));
      const float vtx_y = txc_y - 0.5f;
      for (size_t ai = 0; ai < 2; ai++)
      {
        const float x_extent = scene.volume_bound_x_max -
          scene.volume_bound_x_min;
        const float txc_x = ((ai * x_extent + scene.volume_bound_x_min) /
          (float(scene.volume_xsze)));
        const float vtx_x = txc_x - 0.5f;
        
        scene.line_vertices[idx++] = vtx_x;
        scene.line_vertices[idx++] = vtx_y;
        scene.line_vertices[idx++] = vtx_z;
        scene.line_vertices[idx++] = 0.f;
        scene.line_vertices[idx++] = (float)ai;
        scene.line_vertices[idx++] = (float)aj;
        scene.line_vertices[idx++] = (float)ak;
        scene.line_vertices[idx++] = 1.f;

        scene.line_vertices[idx++] = vtx_x + ((ai > 0) ? -1.f : +1.f) * v;
        scene.line_vertices[idx++] = vtx_y;
        scene.line_vertices[idx++] = vtx_z;
        scene.line_vertices[idx++] = 0.f;
        scene.line_vertices[idx++] = 1.f;
        scene.line_vertices[idx++] = 0.f;
        scene.line_vertices[idx++] = 0.f;
        scene.line_vertices[idx++] = 1.f;

        scene.line_vertices[idx++] = vtx_x;
        scene.line_vertices[idx++] = vtx_y;
        scene.line_vertices[idx++] = vtx_z;
        scene.line_vertices[idx++] = 0.f;
        scene.line_vertices[idx++] = (float)ai;
        scene.line_vertices[idx++] = (float)aj;
        scene.line_vertices[idx++] = (float)ak;
        scene.line_vertices[idx++] = 1.f;

        scene.line_vertices[idx++] = vtx_x;
        scene.line_vertices[idx++] = vtx_y + ((aj > 0) ? -1.f : +1.f) * v;
        scene.line_vertices[idx++] = vtx_z;
        scene.line_vertices[idx++] = 0.f;
        scene.line_vertices[idx++] = 0.f;
        scene.line_vertices[idx++] = 1.f;
        scene.line_vertices[idx++] = 0.f;
        scene.line_vertices[idx++] = 1.f;

        scene.line_vertices[idx++] = vtx_x;
        scene.line_vertices[idx++] = vtx_y;
        scene.line_vertices[idx++] = vtx_z;
        scene.line_vertices[idx++] = 0.f;
        scene.line_vertices[idx++] = (float)ai;
        scene.line_vertices[idx++] = (float)aj;
        scene.line_vertices[idx++] = (float)ak;
        scene.line_vertices[idx++] = 1.f;

        scene.line_vertices[idx++] = vtx_x;
        scene.line_vertices[idx++] = vtx_y;
        scene.line_vertices[idx++] = vtx_z + ((ak > 0) ? -1.f : +1.f) * v;
        scene.line_vertices[idx++] = 0.f;
        scene.line_vertices[idx++] = 0.f;
        scene.line_vertices[idx++] = 0.f;
        scene.line_vertices[idx++] = 1.f;
        scene.line_vertices[idx++] = 1.f;

      } // ai
    } // aj
  } // ak
}

void waitForIdleGraphicsPipeline(
  std::shared_ptr<vkch::Context> &vkch_ctxt,
  AuxiliaryVulkanContext *aux_ctxt)
{
  // Wait for idle graphics and present queues
  for (int i = 0; i < vkch_ctxt->auxiliaryQueueCount(); i++)
  {
    vkch_ctxt->auxiliaryQueueFamilyQueue(i)->waitIdle();

  } // i
}

int checkEvents(
  volatile int *stop_thread,
  std::shared_ptr<vkch::Context> &vkch_ctxt,
  AuxiliaryVulkanContext *aux_ctxt,
  std::shared_ptr<VolumeBuffers> const &volume_buffers,
  struct PersistentGUI &persistent_gui)
{
  Scene &scene(persistent_gui.scene);
  // This should in no way be used, only subclassed.
  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    switch (event.type)
    {
      case SDL_KEYUP:
      {
        switch (event.key.keysym.sym)
        {
          case SDLK_LSHIFT:
          case SDLK_RSHIFT:
          {
            scene.global_incr -= 4;
          } break;
          default: break;
        }
      } break;
      case SDL_KEYDOWN:
      {
        bool resize_volume = false;
        float resize_x = 0.f;
        float resize_y = 0.f;
        float resize_z = 0.f;

        switch (event.key.keysym.sym)
        {
          case SDLK_LSHIFT:
          case SDLK_RSHIFT:
          {
            scene.global_incr += 4;
          } break;
          case SDLK_r:
          {
            scene.eyepos[0] = 0.f;
            scene.eyepos[1] = 3.f;
            scene.eyepos[2] = 0.f;
            scene.centrepos[0] = 0.f;
            scene.centrepos[1] = 0.f;
            scene.centrepos[2] = 0.f;
            scene.upvec[0] = 0.f;
            scene.upvec[1] = 0.f;
            scene.upvec[2] = 1.f;

            updateLineVertices(scene);
            updateVolumeVertices(scene);
            updateModelView(scene);

          } break;
          case SDLK_SPACE:
          {
            scene.location_show = (scene.location_show + 1) % 9;

            updateLineVertices(scene);
            updateVolumeVertices(scene);
          } break;
          case SDLK_UP:
          {
            resize_x = 0.f; resize_y = -1.f; resize_z = 0.f;
            resize_volume = true;
          } break;
          case SDLK_DOWN:
          {
            resize_x = 0.f; resize_y = +1.f; resize_z = 0.f;
            resize_volume = true;
          } break;
          case SDLK_LEFT:
          {
            resize_x = -1.f; resize_y = 0.f; resize_z = 0.f;
            resize_volume = true;
          } break;
          case SDLK_RIGHT:
          {
            resize_x = +1.f; resize_y = 0.f; resize_z = 0.f;
            resize_volume = true;
          } break;
        }

        if (resize_volume)
        {
          int incr = scene.global_incr;

          const size_t base_idx = (scene.location_show - 1) * 6;
          float tmp[12];
          for (size_t k = 0; k < 4; k++)
          {
            int k_offset = (k == 0) ? 0 : (k*2 - 1);
            // Multiply through by lookat matrix
            float t1x =
              scene.lookat_matrix[0] *
                scene.line_vertices[(base_idx + k)*8 + 0] +
              scene.lookat_matrix[4] *
                scene.line_vertices[(base_idx + k)*8 + 1] +
              scene.lookat_matrix[8] *
                scene.line_vertices[(base_idx + k)*8 + 2] +
              scene.lookat_matrix[12];
            float t1y =
              scene.lookat_matrix[1] *
                scene.line_vertices[(base_idx + k)*8 + 0] +
              scene.lookat_matrix[5] *
                scene.line_vertices[(base_idx + k)*8 + 1] +
              scene.lookat_matrix[9] *
                scene.line_vertices[(base_idx + k)*8 + 2] +
              scene.lookat_matrix[13];
            float t1z =
              scene.lookat_matrix[2] *
                scene.line_vertices[(base_idx + k)*8 + 0] +
              scene.lookat_matrix[6] *
                scene.line_vertices[(base_idx + k)*8 + 1] +
              scene.lookat_matrix[10] *
                scene.line_vertices[(base_idx + k)*8 + 2] +
              scene.lookat_matrix[14];

            // Multiply through by perspective matrix
            tmp[k*3+0] =
              scene.persp_matrix[0] * t1x +
              scene.persp_matrix[4] * t1y +
              scene.persp_matrix[8] * t1z +
              scene.persp_matrix[12];
            tmp[k*3+1] =
              scene.persp_matrix[1] * t1x +
              scene.persp_matrix[5] * t1y +
              scene.persp_matrix[9] * t1z +
              scene.persp_matrix[13];
            tmp[k*3+2] =
              scene.persp_matrix[2] * t1x +
              scene.persp_matrix[6] * t1y +
              scene.persp_matrix[10] * t1z +
              scene.persp_matrix[14];
          }

          // Look at dot product between indicating arrow and key press
          float dp_x =
            (tmp[3] - tmp[0]) * resize_x +
            (tmp[4] - tmp[1]) * resize_y +
            (tmp[5] - tmp[2]) * resize_z;
          float dp_y =
            (tmp[6] - tmp[0]) * resize_x +
            (tmp[7] - tmp[1]) * resize_y +
            (tmp[8] - tmp[2]) * resize_z;
          float dp_z =
            (tmp[9] - tmp[0]) * resize_x +
            (tmp[10] - tmp[1]) * resize_y +
            (tmp[11] - tmp[2]) * resize_z;

          float max_dp = -std::numeric_limits<float>::max();
          int to_do = 0;
          if ((max_dp < dp_x) && (dp_x > 0.))
          {
            to_do = 1;
            max_dp = dp_x;
          }
          if ((max_dp < dp_y) && (dp_y > 0.))
          {
            to_do = 2;
            max_dp = dp_y;
          }
          if ((max_dp < dp_z) && (dp_z > 0.))
          {
            to_do = 3;
            max_dp = dp_z;
          }
          if ((max_dp < (-dp_x)) && ((-dp_x) > 0.))
          {
            to_do = 4;
            max_dp = (-dp_x);
          }
          if ((max_dp < (-dp_y)) && ((-dp_y) > 0.))
          {
            to_do = 5;
            max_dp = (-dp_y);
          }
          if ((max_dp < (-dp_z)) && ((-dp_z) > 0.))
          {
            to_do = 6;
            max_dp = (-dp_z);
          }
          // 1 - in same direction as x axis line
          // 2 - in same direction as y axis line
          // 3 - in same direction as z axis line
          // 4 - in opposite direction as x axis line
          // 5 - in opposite direction as y axis line
          // 6 - in opposite direction as z axis line

          switch (to_do)
          {
            case 1:
            {
              if (scene.line_vertices[(base_idx + 1)*8 + 0] >
                scene.line_vertices[(base_idx + 0)*8 + 0])
              {
                if (((scene.volume_bound_x_min + incr) >= 0) &&
                  ((scene.volume_bound_x_min + incr) <
                    scene.volume_xsze) &&
                  ((scene.volume_bound_x_min + incr) <
                    scene.volume_bound_x_max))
                  scene.volume_bound_x_min += incr;
              } else
              {
                if (((scene.volume_bound_x_max - incr) >= 0) &&
                  ((scene.volume_bound_x_max - incr) <
                    scene.volume_xsze) &&
                  ((scene.volume_bound_x_max - incr) >
                    scene.volume_bound_x_min))
                  scene.volume_bound_x_max -= incr;
              }
            } break;
            case 2:
            {
              if (scene.line_vertices[(base_idx + 3)*8 + 1] >
                scene.line_vertices[(base_idx + 0)*8 + 1])
              {
                if (((scene.volume_bound_y_min + incr) >= 0) &&
                  ((scene.volume_bound_y_min + incr) <
                    scene.volume_ysze) &&
                  ((scene.volume_bound_y_min + incr) <
                    scene.volume_bound_y_max))
                  scene.volume_bound_y_min += incr;
              } else
              {
                if (((scene.volume_bound_y_max - incr) >= 0) &&
                  ((scene.volume_bound_y_max - incr) <
                    scene.volume_ysze) &&
                  ((scene.volume_bound_y_max - incr) >
                    scene.volume_bound_y_min))
                  scene.volume_bound_y_max -= incr;
              }
            } break;
            case 3:
            {
              if (scene.line_vertices[(base_idx + 5)*8 + 2] >
                scene.line_vertices[(base_idx + 0)*8 + 2])
              {
                if (((scene.volume_bound_z_min + incr) >= 0) &&
                  ((scene.volume_bound_z_min + incr) <
                    scene.volume_zsze) &&
                  ((scene.volume_bound_z_min + incr) <
                    scene.volume_bound_z_max))
                  scene.volume_bound_z_min += incr;
              } else
              {
                if (((scene.volume_bound_z_max - incr) >= 0) &&
                  ((scene.volume_bound_z_max - incr) <
                    scene.volume_zsze) &&
                  ((scene.volume_bound_z_max - incr) >
                    scene.volume_bound_z_min))
                  scene.volume_bound_z_max -= incr;
              }
            } break;
            case 4:
            {
              if (scene.line_vertices[(base_idx + 1)*8 + 0] >
                scene.line_vertices[(base_idx + 0)*8 + 0])
              {
                if (((scene.volume_bound_x_min - incr) >= 0) &&
                  ((scene.volume_bound_x_min - incr) <
                    scene.volume_xsze) &&
                  ((scene.volume_bound_x_min - incr) <
                    scene.volume_bound_x_max))
                  scene.volume_bound_x_min -= incr;
              } else
              {
                if (((scene.volume_bound_x_max + incr) >= 0) &&
                  ((scene.volume_bound_x_max + incr) <
                    scene.volume_xsze) &&
                  ((scene.volume_bound_x_max + incr) >
                    scene.volume_bound_x_min))
                  scene.volume_bound_x_max += incr;
              }
            } break;
            case 5:
            {
              if (scene.line_vertices[(base_idx + 3)*8 + 1] >
                scene.line_vertices[(base_idx + 0)*8 + 1])
              {
                if (((scene.volume_bound_y_min - incr) >= 0) &&
                  ((scene.volume_bound_y_min - incr) <
                    scene.volume_ysze) &&
                  ((scene.volume_bound_y_min - incr) <
                    scene.volume_bound_y_max))
                  scene.volume_bound_y_min -= incr;
              } else
              {
                if (((scene.volume_bound_y_max + incr) >= 0) &&
                  ((scene.volume_bound_y_max + incr) <
                    scene.volume_ysze) &&
                  ((scene.volume_bound_y_max + incr) >
                    scene.volume_bound_y_min))
                  scene.volume_bound_y_max += incr;
              }
            } break;
            case 6:
            {
              if (scene.line_vertices[(base_idx + 5)*8 + 2] >
                scene.line_vertices[(base_idx + 0)*8 + 2])
              {
                if (((scene.volume_bound_z_min - incr) >= 0) &&
                  ((scene.volume_bound_z_min - incr) <
                    scene.volume_zsze) &&
                  ((scene.volume_bound_z_min - incr) <
                    scene.volume_bound_z_max))
                  scene.volume_bound_z_min -= incr;
              } else
              {
                if (((scene.volume_bound_z_max + incr) >= 0) &&
                  ((scene.volume_bound_z_max + incr) <
                    scene.volume_zsze) &&
                  ((scene.volume_bound_z_max + incr) >
                    scene.volume_bound_z_min))
                  scene.volume_bound_z_max += incr;
              }
            } break;
          }

          updateLineVertices(scene);
          updateVolumeVertices(scene);
        }
        break;
      }
      case SDL_WINDOWEVENT:
      {
        switch (event.window.event)
        {
          case SDL_WINDOWEVENT_ENTER: break;
          case SDL_WINDOWEVENT_LEAVE: break;
          case SDL_WINDOWEVENT_SIZE_CHANGED:
          {
            fprintf(stderr, "SDL Resize!\n");
            resizeWindow(
              vkch_ctxt,
              aux_ctxt,
              persistent_gui);

            break;
          }
        }
        break;
      }
      case SDL_MULTIGESTURE:
      {
        scene.eyepos[0] *= (1.f - event.mgesture.dDist);
        scene.eyepos[1] *= (1.f - event.mgesture.dDist);
        scene.eyepos[2] *= (1.f - event.mgesture.dDist);

        const float x_rotate = scene.eyepos[0] - scene.centrepos[0];
        const float y_rotate = scene.eyepos[1] - scene.centrepos[1];
        const float z_rotate = scene.eyepos[2] - scene.centrepos[2];
        const float rotate_length =
          sqrt(x_rotate*x_rotate + y_rotate*y_rotate + z_rotate*z_rotate);
        const float rcp_rotate_length =
          (rotate_length > 0.f) ? (1. / rotate_length) : 0.f;

        const float x_rotate_axis = x_rotate * rcp_rotate_length;
        const float y_rotate_axis = y_rotate * rcp_rotate_length;
        const float z_rotate_axis = z_rotate * rcp_rotate_length;

        const float sin_base = sin(-event.mgesture.dTheta);
        const float cos_base = cos(-event.mgesture.dTheta);
        const float one_minus_cos = 1. - cos_base;

        const float txx =
          cos_base +
          x_rotate_axis * x_rotate_axis * one_minus_cos;
        const float tyx =
          x_rotate_axis * y_rotate_axis * one_minus_cos -
          z_rotate_axis * sin_base;
        const float tzx =
          x_rotate_axis * z_rotate_axis * one_minus_cos +
          y_rotate_axis * sin_base;

        const float txy =
          y_rotate_axis * x_rotate_axis * one_minus_cos +
          z_rotate_axis * sin_base;
        const float tyy =
          cos_base +
          y_rotate_axis * y_rotate_axis * one_minus_cos;
        const float tzy =
          y_rotate_axis * z_rotate_axis * one_minus_cos -
          x_rotate_axis * sin_base;

        const float txz =
          z_rotate_axis * x_rotate_axis * one_minus_cos -
          y_rotate_axis * sin_base;
        const float tyz =
          z_rotate_axis * y_rotate_axis * one_minus_cos +
          x_rotate_axis * sin_base;
        const float tzz = 
          cos_base +
          z_rotate_axis * z_rotate_axis * one_minus_cos;

        const float modif_upvec_x =
          txx * scene.upvec[0] + tyx * scene.upvec[1] + tzx * scene.upvec[2];
        const float modif_upvec_y =
          txy * scene.upvec[0] + tyy * scene.upvec[1] + tzy * scene.upvec[2];
        const float modif_upvec_z =
          txz * scene.upvec[0] + tyz * scene.upvec[1] + tzz * scene.upvec[2];

        scene.upvec[0] = modif_upvec_x;
        scene.upvec[1] = modif_upvec_y;
        scene.upvec[2] = modif_upvec_z;

        updateModelView(scene);
        updateLineVertices(scene);
        updateVolumeVertices(scene);

        break;
      }
      case SDL_MOUSEMOTION:
      {
        if (event.motion.state & SDL_BUTTON_LMASK)
        {
          // Positive direction is negative upvec
          // Right direction is cross product between upvec and centre eyevec
          //
          const float F_un_x = scene.centrepos[0] - scene.eyepos[0];
          const float F_un_y = scene.centrepos[1] - scene.eyepos[1];
          const float F_un_z = scene.centrepos[2] - scene.eyepos[2];

          const float UP_un_x = scene.upvec[0];
          const float UP_un_y = scene.upvec[1];
          const float UP_un_z = scene.upvec[2];

          const float s_un_x = F_un_y * UP_un_z - F_un_z * UP_un_y;
          const float s_un_y = F_un_z * UP_un_x - F_un_x * UP_un_z;
          const float s_un_z = F_un_x * UP_un_y - F_un_y * UP_un_x;

          const float s_sqrt = sqrt(s_un_x*s_un_x + s_un_y*s_un_y + s_un_z*s_un_z);
          const float s_rcp_sqrt = (s_sqrt > 0.f) ? (1.f / s_sqrt) : 0.f;

          // Rotation axis is then this

          const float rotation_speed = 0.005;

          const float axis_vec_un_x =
            s_un_x * event.motion.yrel * s_rcp_sqrt -
              scene.upvec[0] * event.motion.xrel;
          const float axis_vec_un_y =
            s_un_y * event.motion.yrel * s_rcp_sqrt -
              scene.upvec[1] * event.motion.xrel;
          const float axis_vec_un_z =
            s_un_z * event.motion.yrel * s_rcp_sqrt -
              scene.upvec[2] * event.motion.xrel;

          const float angle = sqrt(
            event.motion.xrel*event.motion.xrel +
            event.motion.yrel*event.motion.yrel) * rotation_speed;

          const float axis_sqrt = sqrt(
            axis_vec_un_x*axis_vec_un_x +
            axis_vec_un_y*axis_vec_un_y +
            axis_vec_un_z*axis_vec_un_z);
          const float rcp_axis_sqrt =
            (axis_sqrt > 0.f) ? (1.f / axis_sqrt) : 0.f;

          const float x_rotate_axis = axis_vec_un_x * rcp_axis_sqrt;
          const float y_rotate_axis = axis_vec_un_y * rcp_axis_sqrt;
          const float z_rotate_axis = axis_vec_un_z * rcp_axis_sqrt;

          const float sin_base = sin(angle);
          const float cos_base = cos(angle);
          const float one_minus_cos = 1. - cos_base;

          const float txx =
            cos_base +
            x_rotate_axis * x_rotate_axis * one_minus_cos;
          const float tyx =
            x_rotate_axis * y_rotate_axis * one_minus_cos -
            z_rotate_axis * sin_base;
          const float tzx =
            x_rotate_axis * z_rotate_axis * one_minus_cos +
            y_rotate_axis * sin_base;

          const float txy =
            y_rotate_axis * x_rotate_axis * one_minus_cos +
            z_rotate_axis * sin_base;
          const float tyy =
            cos_base +
            y_rotate_axis * y_rotate_axis * one_minus_cos;
          const float tzy =
            y_rotate_axis * z_rotate_axis * one_minus_cos -
            x_rotate_axis * sin_base;

          const float txz =
            z_rotate_axis * x_rotate_axis * one_minus_cos - y_rotate_axis * sin_base;
          const float tyz =
            z_rotate_axis * y_rotate_axis * one_minus_cos + x_rotate_axis * sin_base;
          const float tzz =
            cos_base +
            z_rotate_axis * z_rotate_axis * one_minus_cos;

          const float modif_eyed_x =
            -(txx * F_un_x + tyx * F_un_y + tzx * F_un_z);
          const float modif_eyed_y =
            -(txy * F_un_x + tyy * F_un_y + tzy * F_un_z);
          const float modif_eyed_z =
            -(txz * F_un_x + tyz * F_un_y + tzz * F_un_z);

          const float modif_upvec_x =
            txx * scene.upvec[0] +
            tyx * scene.upvec[1] +
            tzx * scene.upvec[2];
          const float modif_upvec_y =
            txy * scene.upvec[0] +
            tyy * scene.upvec[1] +
            tzy * scene.upvec[2];
          const float modif_upvec_z =
            txz * scene.upvec[0] +
            tyz * scene.upvec[1] +
            tzz * scene.upvec[2];

          scene.eyepos[0] = modif_eyed_x + scene.centrepos[0];
          scene.eyepos[1] = modif_eyed_y + scene.centrepos[1];
          scene.eyepos[2] = modif_eyed_z + scene.centrepos[2];

          scene.upvec[0] = modif_upvec_x;
          scene.upvec[1] = modif_upvec_y;
          scene.upvec[2] = modif_upvec_z;

          updateModelView(scene);
          updateLineVertices(scene);
          updateVolumeVertices(scene);

        }
        if (event.motion.state & SDL_BUTTON_RMASK)
        {
          scene.setting_value = scene.setting_value +
            (event.motion.yrel / 10.);
          if (scene.setting_value < -2.) scene.setting_value = -2.;
          if (scene.setting_value > +2.) scene.setting_value = +2.;
          if (scene.setting_value < 0)
          {
            scene.cubic_components[0] = exp(scene.setting_value);
            scene.cubic_components[1] = 1.f;
          } else
          {
            scene.cubic_components[0] = 1.f;
            scene.cubic_components[1] = exp(-scene.setting_value);
          }
          scene.cubic_components[2] += (event.motion.xrel / 100.);
          if (scene.cubic_components[2] < 0.f) scene.cubic_components[2] = 0.f;
          if (scene.cubic_components[2] > 1.f) scene.cubic_components[2] = 1.f;
        }
        break;
      }
      case SDL_QUIT:
      {
        (*stop_thread) = 1;
        return 1;
        break;
      }
    }
  }
  return 0;
}

void gui_process_events(
  volatile int *stop_thread,
  const std::shared_ptr<VolumeBuffers> &volume_buffers,
  std::shared_ptr<vkch::Context> &vkch_ctxt,
  AuxiliaryVulkanContext *aux_ctxt,
  SimulationParameters const &simulation_parameters,
  struct PersistentGUI &persistent_gui)
{
  // Wait for fence on processing... just in case.
  if (persistent_gui.frame_pool.current_frame().in_use())
  {
    while (vk::Result::eTimeout ==
      vkch_ctxt->device().waitForFences(
        *(persistent_gui.frame_pool.current_frame().fence()),
        VK_TRUE, 5000000));
    persistent_gui.frame_pool.current_frame().set_not_in_use();
  }

  // No wait now, sleep until needed.
  std::this_thread::sleep_for(
    std::chrono::microseconds(
      static_cast<unsigned int>(1e6f / DESIRED_FRAME_RATE_PER_SECOND)) -
    (std::chrono::steady_clock::now() - persistent_gui.last_frame_rendered)
  );

  if (*stop_thread) return;

  checkEvents(stop_thread, vkch_ctxt, aux_ctxt, volume_buffers,
    persistent_gui);

  vk::Result result;
  uint32_t backbuffer_index;
  try
  {
    std::tie(result, backbuffer_index) =
      persistent_gui.swapchain_data.swapchain.acquireNextImage(
        1000000000,
        persistent_gui.frame_pool.current_frame().present_semaphore()
      );
  }
  catch (vk::OutOfDateKHRError outOfDateError)
  {
    resizeWindow(
      vkch_ctxt,
      aux_ctxt,
      persistent_gui);
  }

  if (result != vk::Result::eSuccess)
  {
    fprintf(stderr, "result not success on acquireNextImage... (%d)\n",
      static_cast<int>(result));
    // Try the frame render again...
    return;
  }

  // Update buffers
  persistent_gui.frame_pool.current_frame(
    ).scene_buffers->syncBufferWithScene(
      vkch_ctxt->device(), persistent_gui.scene);

  persistent_gui.frame_pool.current_frame().clearAndBegin();

  vk::raii::CommandBuffer const &command_buffer =
    persistent_gui.frame_pool.current_frame().command_buffer();

  vk::Extent2D window_extent(aux_ctxt->width, aux_ctxt->height);
  std::array<vk::ClearValue, 2> clearValues;
  clearValues[0].color = vk::ClearColorValue(0.f, 0.f, 0.f, 0.f);
  clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);
  command_buffer.beginRenderPass(
    vk::RenderPassBeginInfo(
      *(persistent_gui.render_pass),
      *(persistent_gui.framebuffers[backbuffer_index]),
      vk::Rect2D(
        vk::Offset2D(0, 0),
        window_extent),
      clearValues
    ),
    vk::SubpassContents::eInline
  );
  command_buffer.setViewport(
    0, vk::Viewport(0.0f, 0.0f,
    static_cast<float>(aux_ctxt->width),
    static_cast<float>(aux_ctxt->height), 0.0f, 1.0f));
  command_buffer.setScissor(0,
    vk::Rect2D(
        vk::Offset2D(0, 0),
        window_extent)
  );
  std::vector<vk::DescriptorSet> descriptor_sets = {
    *(persistent_gui.frame_pool.current_frame(
      ).scene_buffers->_descriptor_set),
    *(volume_buffers->read_descriptor_set())
  };
  command_buffer.bindDescriptorSets(
    vk::PipelineBindPoint::eGraphics, persistent_gui.pipeline_layout,
    0, descriptor_sets, nullptr);
  
  command_buffer.bindPipeline(
    vk::PipelineBindPoint::eGraphics, *(persistent_gui.line_pipeline));
  command_buffer.bindVertexBuffers(0, { 
    persistent_gui.frame_pool.current_frame(
      ).scene_buffers->lines_vertex_buffer._buffer
  }, { 0 } );
  command_buffer.draw(
    sizeof(persistent_gui.scene.line_vertices)/
      (sizeof(persistent_gui.scene.line_vertices[0]) * 8),
    1, 0, 0);

  command_buffer.bindPipeline(
    vk::PipelineBindPoint::eGraphics, *(persistent_gui.volume_pipeline));
  command_buffer.bindVertexBuffers(0, { 
    persistent_gui.frame_pool.current_frame(
      ).scene_buffers->volume_vertex_buffer._buffer
  }, { 0 } );
  command_buffer.draw(
    sizeof(persistent_gui.scene.volm_vertices)/
      (sizeof(persistent_gui.scene.volm_vertices[0]) * 8),
    1, 0, 0);

  command_buffer.endRenderPass();

  vkch_ctxt->device().resetFences(
    *(persistent_gui.frame_pool.current_frame().fence()));

  persistent_gui.frame_pool.current_frame().endAndSubmit();

  vk::PresentInfoKHR present_info_KHR(
    *(persistent_gui.frame_pool.current_frame().render_semaphore()),
    *(persistent_gui.swapchain_data.swapchain),
    backbuffer_index
  );
  {
    vk::Result result;
    std::lock_guard<std::mutex> lock(
      *(persistent_gui.present_queue_mutex_ptr));
    try
    {
      result =
        persistent_gui.present_queue->presentKHR(present_info_KHR);
    }
    catch (vk::OutOfDateKHRError outOfDateError)
    {
      resizeWindow(
        vkch_ctxt,
        aux_ctxt,
        persistent_gui);
    }
    if (result != vk::Result::eSuccess)
    {
      fprintf(stderr, "result not success on presentKHR... (%d)\n",
        static_cast<int>(result));
    }
  }

  if (persistent_gui.frames_until_end_of_transition == 0)
  {
    if (volume_buffers->tryTransitionToReadNew())
    {
      persistent_gui.frames_until_end_of_transition =
        persistent_gui.swapchain_data.size();
    }
  } else
  {
    if (!(--(persistent_gui.frames_until_end_of_transition)))
    {
      volume_buffers->completeTransitionToReadNew();
      // Update title bar
      char buffer_titlebar[256];
//      sprintf(&(buffer_titlebar[0]), "snowflake crystal"); 
//      SDL_SetWindowTitle(aux_ctxt->window, &(buffer_titlebar[0]));
      SDL_SetWindowTitle(aux_ctxt->window, "snowflake crystal");
    }
  }

  persistent_gui.frame_pool.incrementFrameCount();
}

void gui_process_wait_for_completion(
  std::shared_ptr<vkch::Context> &vkch_ctxt,
  AuxiliaryVulkanContext *aux_ctxt,
  struct PersistentGUI &persistent_gui)
{
  // Wait for in-flight frames.
  for (int i = 0; i < persistent_gui.frame_pool.count(); i++)
  {
    if (persistent_gui.frame_pool._frames[i]->in_use())
    {
      while (vk::Result::eTimeout ==
        vkch_ctxt->device().waitForFences(
          *(persistent_gui.frame_pool._frames[i]->fence()),
          VK_TRUE, 5000000));
      persistent_gui.frame_pool._frames[i]->set_not_in_use();
    }

  } // i

  waitForIdleGraphicsPipeline(vkch_ctxt, aux_ctxt);

#if !defined(BUILD_PYTHON_BINDINGS)
  fprintf(stderr, "completed shutdown (GUI)...\n");
#endif // !defined(BUILD_PYTHON_BINDINGS)

}
