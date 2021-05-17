/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation
 * All rights reserved.
 */
#pragma once

/** \file
 * \ingroup bgpencil
 */

#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "BLI_float4x4.hh"
#include "BLI_vector.hh"

#include "DNA_space_types.h" /* for FILE_MAX */

#include "gpencil_io.h"

struct Depsgraph;
struct Main;
struct Object;
struct RegionView3D;
struct Scene;

struct bGPdata;
struct bGPDlayer;
struct bGPDstroke;

using blender::Vector;

namespace blender::io::gpencil {

class GpencilIO {
 public:
  GpencilIO(const GpencilIOParams *iparams);

  void frame_number_set(const int value);
  void prepare_camera_params(const GpencilIOParams *iparams);

 protected:
  GpencilIOParams params_;

  bool invert_axis_[2];
  float4x4 diff_mat_;
  char filename_[FILE_MAX];

  /* Used for sorting objects. */
  struct ObjectZ {
    float zdepth;
    struct Object *ob;
  };

  /** List of included objects. */
  blender::Vector<ObjectZ> ob_list_;

  /* Data for easy access. */
  struct Depsgraph *depsgraph_;
  struct bGPdata *gpd_;
  struct Main *bmain_;
  struct Scene *scene_;
  struct RegionView3D *rv3d_;

  int16_t winx_, winy_;
  int16_t render_x_, render_y_;
  float camera_ratio_;
  rctf camera_rect_;

  float2 offset_;

  int cfra_;

  float stroke_color_[4], fill_color_[4];

  /* Geometry functions. */
  bool gpencil_3D_point_to_screen_space(const float3 co, float2 &r_co);
  float2 gpencil_3D_point_to_render_space(const float3 co, const bool is_ortho);
  float2 gpencil_3D_point_to_2D(const float3 co);

  float stroke_point_radius_get(struct bGPDlayer *gpl, struct bGPDstroke *gps);
  void create_object_list();

  bool is_camera_mode();
  bool is_orthographic();

  float stroke_average_opacity_get();

  void prepare_layer_export_matrix(struct Object *ob, struct bGPDlayer *gpl);
  void prepare_stroke_export_colors(struct Object *ob, struct bGPDstroke *gps);

  void selected_objects_boundbox_calc();
  void selected_objects_boundbox_get(rctf *boundbox);
  void filename_set(const char *filename);

 private:
  float avg_opacity_;
  bool is_camera_;
  bool is_ortho_;
  rctf select_boundbox_;

  /* Camera matrix. */
  float persmat_[4][4];
};

}  // namespace blender::io::gpencil
