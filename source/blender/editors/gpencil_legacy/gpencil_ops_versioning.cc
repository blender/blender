/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 * Use deprecated data to convert old 2.7x files.
 */

/* Allow using deprecated functionality. */
#define DNA_DEPRECATED_ALLOW

#include <cstdio>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_main.h"
#include "BKE_object.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_gpencil_legacy.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"

/* Free all of a gp-colors */
static void free_gpencil_colors(bGPDpalette *palette)
{
  /* error checking */
  if (palette == nullptr) {
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
  if (list == nullptr) {
    return;
  }

  /* delete palettes */
  for (bGPDpalette *palette = static_cast<bGPDpalette *>(list->first); palette;
       palette = palette_next)
  {
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

  return int(scene->gpd != nullptr);
}

static int gpencil_convert_old_files_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool is_annotation = RNA_boolean_get(op->ptr, "annotation");
  bGPdata *gpd = scene->gpd;

  /* Convert grease pencil scene datablock to GP object */
  if ((!is_annotation) && (view_layer != nullptr)) {
    Object *ob;
    ob = BKE_object_add_for_data(
        bmain, scene, view_layer, OB_GPENCIL_LEGACY, "GP_Scene", &scene->gpd->id, false);
    zero_v3(ob->loc);
    DEG_relations_tag_update(bmain); /* added object */

    /* convert grease pencil palettes (version >= 2.78)  to materials and weights */
    LISTBASE_FOREACH (const bGPDpalette *, palette, &gpd->palettes) {
      LISTBASE_FOREACH (bGPDpalettecolor *, palcolor, &palette->colors) {

        /* create material slot */
        Material *ma = BKE_gpencil_object_material_new(bmain, ob, palcolor->info, nullptr);

        /* copy color settings */
        MaterialGPencilStyle *gp_style = ma->gp_style;
        copy_v4_v4(gp_style->stroke_rgba, palcolor->color);
        copy_v4_v4(gp_style->fill_rgba, palcolor->fill);

        /* set basic settings */
        gp_style->gradient_radius = 0.5f;
        ARRAY_SET_ITEMS(gp_style->mix_rgba, 1.0f, 1.0f, 1.0f, 0.2f);
        ARRAY_SET_ITEMS(gp_style->gradient_scale, 1.0f, 1.0f);
        ARRAY_SET_ITEMS(gp_style->texture_scale, 1.0f, 1.0f);
        gp_style->texture_pixsize = 100.0f;

        gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
        gp_style->flag |= GP_MATERIAL_FILL_SHOW;

        /* fix strokes */
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
            LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
              if ((gps->colorname[0] != '\0') && STREQ(gps->colorname, palcolor->info)) {
                gps->mat_nr = ob->totcol - 1;
                gps->colorname[0] = '\0';
                /* weights array */
                gps->dvert = nullptr;
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
    BKE_gpencil_batch_cache_dirty_tag(static_cast<bGPdata *>(ob->data));

    scene->gpd = nullptr;
  }

  if (is_annotation) {
    LISTBASE_FOREACH (const bGPDpalette *, palette, &gpd->palettes) {
      LISTBASE_FOREACH (bGPDpalettecolor *, palcolor, &palette->colors) {
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
              if ((gps->colorname[0] != '\0') && STREQ(gps->colorname, palcolor->info)) {
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
  WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

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
  ot->prop = RNA_def_boolean(
      ot->srna, "annotation", false, "Annotation", "Convert to Annotations");
}
