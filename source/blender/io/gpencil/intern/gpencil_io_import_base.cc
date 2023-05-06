/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup bgpencil
 */
#include "BLI_math_vector.h"

#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_gpencil_legacy.h"
#include "BKE_material.h"

#include "ED_gpencil_legacy.h"
#include "ED_object.h"

#include "gpencil_io_import_base.hh"

namespace blender::io::gpencil {

/* Constructor. */
GpencilImporter::GpencilImporter(const GpencilIOParams *iparams) : GpencilIO(iparams)
{
  /* Nothing to do yet */
}

Object *GpencilImporter::create_object()
{
  const float *cur_loc = scene_->cursor.location;
  const float rot[3] = {0.0f};
  ushort local_view_bits = (params_.v3d && params_.v3d->localvd) ? params_.v3d->local_view_uuid :
                                                                   ushort(0);

  Object *ob_gpencil = ED_object_add_type(params_.C,
                                          OB_GPENCIL_LEGACY,
                                          (params_.filename[0] != '\0') ? params_.filename :
                                                                          nullptr,
                                          cur_loc,
                                          rot,
                                          false,
                                          local_view_bits);

  /* Set object defaults. */
  ED_gpencil_add_defaults(params_.C, ob_gpencil);

  return ob_gpencil;
}

int32_t GpencilImporter::create_material(const char *name, const bool stroke, const bool fill)
{
  const float default_stroke_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  const float default_fill_color[4] = {0.5f, 0.5f, 0.5f, 1.0f};
  int32_t mat_index = BKE_gpencil_material_find_index_by_name_prefix(params_.ob, name);
  /* Stroke and Fill material. */
  if (mat_index == -1) {
    int32_t new_idx;
    Material *mat_gp = BKE_gpencil_object_material_new(bmain_, params_.ob, name, &new_idx);
    MaterialGPencilStyle *gp_style = mat_gp->gp_style;
    gp_style->flag &= ~GP_MATERIAL_STROKE_SHOW;
    gp_style->flag &= ~GP_MATERIAL_FILL_SHOW;

    copy_v4_v4(gp_style->stroke_rgba, default_stroke_color);
    copy_v4_v4(gp_style->fill_rgba, default_fill_color);
    if (stroke) {
      gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
    }
    if (fill) {
      gp_style->flag |= GP_MATERIAL_FILL_SHOW;
    }
    mat_index = params_.ob->totcol - 1;
  }

  return mat_index;
}

}  // namespace blender::io::gpencil
