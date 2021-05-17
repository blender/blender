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
 * The Original Code is Copyright (C) 2017 Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup edgpencil
 */

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_material.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "ED_gpencil.h"

/* Definition of the most important info from a color */
typedef struct ColorTemplate {
  const char *name;
  float line[4];
  float fill[4];
} ColorTemplate;

/* Add color an ensure duplications (matched by name) */
static int gpencil_lineart_material(Main *bmain,
                                    Object *ob,
                                    const ColorTemplate *pct,
                                    const bool fill)
{
  int index;
  Material *ma = BKE_gpencil_object_material_ensure_by_name(bmain, ob, pct->name, &index);

  copy_v4_v4(ma->gp_style->stroke_rgba, pct->line);
  srgb_to_linearrgb_v4(ma->gp_style->stroke_rgba, ma->gp_style->stroke_rgba);

  copy_v4_v4(ma->gp_style->fill_rgba, pct->fill);
  srgb_to_linearrgb_v4(ma->gp_style->fill_rgba, ma->gp_style->fill_rgba);

  if (fill) {
    ma->gp_style->flag |= GP_MATERIAL_FILL_SHOW;
  }

  return index;
}

/* ***************************************************************** */
/* Color Data */

static const ColorTemplate gp_stroke_material_black = {
    "Black",
    {0.0f, 0.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
};

/* ***************************************************************** */
/* LineArt API */

/* Add a Simple LineArt setup. */
void ED_gpencil_create_lineart(bContext *C, Object *ob)
{
  Main *bmain = CTX_data_main(C);
  bGPdata *gpd = (bGPdata *)ob->data;

  /* create colors */
  int color_black = gpencil_lineart_material(bmain, ob, &gp_stroke_material_black, false);

  /* set first color as active and in brushes */
  ob->actcol = color_black + 1;

  /* layers */
  bGPDlayer *lines = BKE_gpencil_layer_addnew(gpd, "Lines", true, false);

  /* frames */
  BKE_gpencil_frame_addnew(lines, 0);

  /* update depsgraph */
  /* To trigger modifier update, this is still needed although we don't have any strokes. */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
}
