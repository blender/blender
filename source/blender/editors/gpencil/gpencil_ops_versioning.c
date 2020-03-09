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
 * The Original Code is Copyright (C) 2018, Blender Foundation,
 * This is a new part of Blender
 * Use deprecated data to convert old 2.7x files
 */

/** \file
 * \ingroup edgpencil
 */

/* allow to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_main.h"
#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_object.h"
#include "BKE_material.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_object.h"
#include "ED_gpencil.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"

/* Free all of a gp-colors */
static void free_gpencil_colors(bGPDpalette *palette)
{
  /* error checking */
  if (palette == NULL) {
    return;
  }

  /* free colors */
  BLI_freelistN(&palette->colors);
}

/* Free all of the gp-palettes and colors */
static void free_palettes(ListBase *list)
{
  bGPDpalette *palette_next;

  /* error checking */
  if (list == NULL) {
    return;
  }

  /* delete palettes */
  for (bGPDpalette *palette = list->first; palette; palette = palette_next) {
    palette_next = palette->next;
    /* free palette colors */
    free_gpencil_colors(palette);

    MEM_freeN(palette);
  }
  BLI_listbase_clear(list);
}

/* ***************** Convert old 2.7 files to 2.8 ************************ */
static bool gpencil_convert_old_files_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);

  return (int)(scene->gpd != NULL);
}

static int gpencil_convert_old_files_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool is_annotation = RNA_boolean_get(op->ptr, "annotation");
  bGPdata *gpd = scene->gpd;

  /* Convert grease pencil scene datablock to GP object */
  if ((!is_annotation) && (view_layer != NULL)) {
    Object *ob;
    ob = BKE_object_add_for_data(
        bmain, view_layer, OB_GPENCIL, "GP_Scene", &scene->gpd->id, false);
    zero_v3(ob->loc);
    DEG_relations_tag_update(bmain); /* added object */

    /* convert grease pencil palettes (version >= 2.78)  to materials and weights */
    for (const bGPDpalette *palette = gpd->palettes.first; palette; palette = palette->next) {
      for (bGPDpalettecolor *palcolor = palette->colors.first; palcolor;
           palcolor = palcolor->next) {

        /* create material slot */
        Material *ma = BKE_gpencil_object_material_new(bmain, ob, palcolor->info, NULL);

        /* copy color settings */
        MaterialGPencilStyle *gp_style = ma->gp_style;
        copy_v4_v4(gp_style->stroke_rgba, palcolor->color);
        copy_v4_v4(gp_style->fill_rgba, palcolor->fill);

        /* set basic settings */
        gp_style->gradient_radius = 0.5f;
        ARRAY_SET_ITEMS(gp_style->mix_rgba, 1.0f, 1.0f, 1.0f, 0.2f);
        ARRAY_SET_ITEMS(gp_style->gradient_scale, 1.0f, 1.0f);
        ARRAY_SET_ITEMS(gp_style->texture_scale, 1.0f, 1.0f);
        gp_style->texture_opacity = 1.0f;
        gp_style->texture_pixsize = 100.0f;

        gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
        gp_style->flag |= GP_MATERIAL_FILL_SHOW;

        /* fix strokes */
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
            LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
              if ((gps->colorname[0] != '\0') && (STREQ(gps->colorname, palcolor->info))) {
                gps->mat_nr = ob->totcol - 1;
                gps->colorname[0] = '\0';
                /* weights array */
                gps->dvert = NULL;
              }
            }
          }
        }
      }
    }

    /* free palettes */
    free_palettes(&gpd->palettes);

    /* disable all GP modes */
    ED_gpencil_setup_modes(C, gpd, 0);

    /* set cache as dirty */
    BKE_gpencil_batch_cache_dirty_tag(ob->data);

    scene->gpd = NULL;
  }

  if (is_annotation) {
    for (const bGPDpalette *palette = gpd->palettes.first; palette; palette = palette->next) {
      for (bGPDpalettecolor *palcolor = palette->colors.first; palcolor;
           palcolor = palcolor->next) {
        /* fix layers */
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          /* unlock/unhide layer */
          gpl->flag &= ~GP_LAYER_LOCKED;
          gpl->flag &= ~GP_LAYER_HIDE;
          /* set opacity to 1 */
          gpl->opacity = 1.0f;
          /* disable tint */
          gpl->tintcolor[3] = 0.0f;
          LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
            LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
              if ((gps->colorname[0] != '\0') && (STREQ(gps->colorname, palcolor->info))) {
                /* copy color settings */
                copy_v4_v4(gpl->color, palcolor->color);
              }
            }
          }
        }
      }
    }
  }

  /* notifiers */
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_convert_old_files(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Convert Grease Pencil";
  ot->idname = "GPENCIL_OT_convert_old_files";
  ot->description = "Convert 2.7x grease pencil files to 2.80";

  /* callbacks */
  ot->exec = gpencil_convert_old_files_exec;
  ot->poll = gpencil_convert_old_files_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_boolean(ot->srna, "annotation", 0, "Annotation", "Convert to Annotations");
}
