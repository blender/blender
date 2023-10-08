/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BKE_brush.hh"
#include "BKE_context.h"
#include "BKE_grease_pencil.hh"

#include "BLI_math_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

namespace blender::ed::greasepencil {

static float3 drawing_origin(const Scene *scene, const Object *object, char align_flag)
{
  BLI_assert(object != nullptr && object->type == OB_GREASE_PENCIL);
  if (align_flag & GP_PROJECT_VIEWSPACE) {
    if (align_flag & GP_PROJECT_CURSOR) {
      return float3(scene->cursor.location);
    }
    /* Use the object location. */
    return float3(object->object_to_world[3]);
  }
  return float3(scene->cursor.location);
}

static float3 screen_space_to_3d(
    const Scene *scene, const ARegion *region, const View3D *v3d, const Object *object, float2 co)
{
  float3 origin = drawing_origin(scene, object, scene->toolsettings->gpencil_v3d_align);
  float3 r_co;
  ED_view3d_win_to_3d(v3d, region, origin, co, r_co);
  return r_co;
}

float brush_radius_world_space(bContext &C, int x, int y)
{
  ARegion *region = CTX_wm_region(&C);
  View3D *v3d = CTX_wm_view3d(&C);
  Scene *scene = CTX_data_scene(&C);
  Object *object = CTX_data_active_object(&C);
  Brush *brush = scene->toolsettings->gp_paint->paint.brush;

  /* Default radius. */
  float radius = 2.0f;
  if (brush == nullptr || object->type != OB_GREASE_PENCIL) {
    return radius;
  }

  /* Use an (arbitrary) screen space offset in the x direction to measure the size. */
  const int x_offest = 64;
  const float brush_size = float(BKE_brush_size_get(scene, brush));

  /* Get two 3d coordinates to measure the distance from. */
  const float2 screen1(x, y);
  const float2 screen2(x + x_offest, y);
  const float3 pos1 = screen_space_to_3d(scene, region, v3d, object, screen1);
  const float3 pos2 = screen_space_to_3d(scene, region, v3d, object, screen2);

  /* Clip extreme zoom level (and avoid division by zero). */
  const float distance = math::max(math::distance(pos1, pos2), 0.001f);

  /* Calculate the radius of the brush in world space. */
  radius = (1.0f / distance) * (brush_size / 64.0f);

  return radius;
}

}  // namespace blender::ed::greasepencil
