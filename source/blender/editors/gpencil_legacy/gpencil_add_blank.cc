/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 */

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_main.h"
#include "BKE_material.h"

#include "BLT_translation.h"

#include "DEG_depsgraph.h"

#include "ED_gpencil_legacy.hh"

/* Definition of the most important info from a color */
struct ColorTemplate {
  const char *name;
  float line[4];
  float fill[4];
};

/* Add color an ensure duplications (matched by name) */
static int gpencil_stroke_material(Main *bmain, Object *ob, const ColorTemplate *pct)
{
  int index;
  Material *ma = BKE_gpencil_object_material_ensure_by_name(bmain, ob, DATA_(pct->name), &index);

  copy_v4_v4(ma->gp_style->stroke_rgba, pct->line);
  srgb_to_linearrgb_v4(ma->gp_style->stroke_rgba, ma->gp_style->stroke_rgba);

  copy_v4_v4(ma->gp_style->fill_rgba, pct->fill);
  srgb_to_linearrgb_v4(ma->gp_style->fill_rgba, ma->gp_style->fill_rgba);

  return index;
}

/* ***************************************************************** */
/* Stroke Geometry */

/* ***************************************************************** */
/* Color Data */

static const ColorTemplate gp_stroke_material_black = {
    N_("Black"),
    {0.0f, 0.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
};

/* ***************************************************************** */
/* Blank API */

void ED_gpencil_create_blank(bContext *C, Object *ob, float[4][4] /*mat*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  bGPdata *gpd = (bGPdata *)ob->data;

  /* create colors */
  int color_black = gpencil_stroke_material(bmain, ob, &gp_stroke_material_black);

  /* set first color as active and in brushes */
  ob->actcol = color_black + 1;

  /* layers */
  bGPDlayer *layer = BKE_gpencil_layer_addnew(gpd, "GP_Layer", true, false);

  /* frames */
  BKE_gpencil_frame_addnew(layer, scene->r.cfra);

  /* update depsgraph */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
}
