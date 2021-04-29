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

#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_main.h"
#include "BKE_material.h"

#include "DEG_depsgraph.h"

#include "ED_gpencil.h"

/* Definition of the most important info from a color */
typedef struct ColorTemplate {
  const char *name;
  float line[4];
  float fill[4];
} ColorTemplate;

/* Add color an ensure duplications (matched by name) */
static int gpencil_stroke_material(Main *bmain, Object *ob, const ColorTemplate *pct)
{
  int index;
  Material *ma = BKE_gpencil_object_material_ensure_by_name(bmain, ob, pct->name, &index);

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
    "Black",
    {0.0f, 0.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
};

/* ***************************************************************** */
/* Blank API */

/* Add a Simple empty object with one layer and one color. */
void ED_gpencil_create_blank(bContext *C, Object *ob, float UNUSED(mat[4][4]))
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  bGPdata *gpd = (bGPdata *)ob->data;

  /* create colors */
  int color_black = gpencil_stroke_material(bmain, ob, &gp_stroke_material_black);

  /* set first color as active and in brushes */
  ob->actcol = color_black + 1;

  /* layers */
  bGPDlayer *layer = BKE_gpencil_layer_addnew(gpd, "GP_Layer", true);

  /* frames */
  BKE_gpencil_frame_addnew(layer, CFRA);

  /* update depsgraph */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
}
