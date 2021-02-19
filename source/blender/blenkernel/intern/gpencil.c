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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup bke
 */

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math_vector.h"
#include "BLI_string_utils.h"

#include "BLT_translation.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_gpencil_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_space_types.h"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_collection.h"
#include "BKE_colortools.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_icons.h"
#include "BKE_idtype.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_paint.h"

#include "BLI_math_color.h"

#include "DEG_depsgraph_query.h"

#include "BLO_read_write.h"

#include "BKE_gpencil.h"

static CLG_LogRef LOG = {"bke.gpencil"};

static void greasepencil_copy_data(Main *UNUSED(bmain),
                                   ID *id_dst,
                                   const ID *id_src,
                                   const int UNUSED(flag))
{
  bGPdata *gpd_dst = (bGPdata *)id_dst;
  const bGPdata *gpd_src = (const bGPdata *)id_src;

  /* duplicate material array */
  if (gpd_src->mat) {
    gpd_dst->mat = MEM_dupallocN(gpd_src->mat);
  }

  /* copy layers */
  BLI_listbase_clear(&gpd_dst->layers);
  LISTBASE_FOREACH (bGPDlayer *, gpl_src, &gpd_src->layers) {
    /* make a copy of source layer and its data */

    /* TODO here too could add unused flags... */
    bGPDlayer *gpl_dst = BKE_gpencil_layer_duplicate(gpl_src, true, true);

    /* Apply local layer transform to all frames. Calc the active frame is not enough
     * because onion skin can use more frames. This is more slow but required here. */
    if (gpl_dst->actframe != NULL) {
      bool transformed = ((!is_zero_v3(gpl_dst->location)) || (!is_zero_v3(gpl_dst->rotation)) ||
                          (!is_one_v3(gpl_dst->scale)));
      if (transformed) {
        loc_eul_size_to_mat4(
            gpl_dst->layer_mat, gpl_dst->location, gpl_dst->rotation, gpl_dst->scale);
        bool do_onion = ((gpl_dst->onion_flag & GP_LAYER_ONIONSKIN) != 0);
        bGPDframe *init_gpf = (do_onion) ? gpl_dst->frames.first : gpl_dst->actframe;
        for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
          LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
            bGPDspoint *pt;
            int i;
            for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
              mul_m4_v3(gpl_dst->layer_mat, &pt->x);
            }
          }
          /* if not onion, exit loop. */
          if (!do_onion) {
            break;
          }
        }
      }
    }

    BLI_addtail(&gpd_dst->layers, gpl_dst);
  }
}

static void greasepencil_free_data(ID *id)
{
  /* Really not ideal, but for now will do... In theory custom behaviors like not freeing cache
   * should be handled through specific API, and not be part of the generic one. */
  BKE_gpencil_free((bGPdata *)id, true);
}

static void greasepencil_foreach_id(ID *id, LibraryForeachIDData *data)
{
  bGPdata *gpencil = (bGPdata *)id;
  /* materials */
  for (int i = 0; i < gpencil->totcol; i++) {
    BKE_LIB_FOREACHID_PROCESS(data, gpencil->mat[i], IDWALK_CB_USER);
  }

  LISTBASE_FOREACH (bGPDlayer *, gplayer, &gpencil->layers) {
    BKE_LIB_FOREACHID_PROCESS(data, gplayer->parent, IDWALK_CB_NOP);
  }
}

static void greasepencil_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  bGPdata *gpd = (bGPdata *)id;
  if (gpd->id.us > 0 || BLO_write_is_undo(writer)) {
    /* Clean up, important in undo case to reduce false detection of changed data-blocks. */
    /* XXX not sure why the whole run-time data is not cleared in reading code,
     * for now mimicking it here. */
    gpd->runtime.sbuffer = NULL;
    gpd->runtime.sbuffer_used = 0;
    gpd->runtime.sbuffer_size = 0;
    gpd->runtime.tot_cp_points = 0;

    /* write gpd data block to file */
    BLO_write_id_struct(writer, bGPdata, id_address, &gpd->id);
    BKE_id_blend_write(writer, &gpd->id);

    if (gpd->adt) {
      BKE_animdata_blend_write(writer, gpd->adt);
    }

    BLO_write_pointer_array(writer, gpd->totcol, gpd->mat);

    /* write grease-pencil layers to file */
    BLO_write_struct_list(writer, bGPDlayer, &gpd->layers);
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      /* Write mask list. */
      BLO_write_struct_list(writer, bGPDlayer_Mask, &gpl->mask_layers);
      /* write this layer's frames to file */
      BLO_write_struct_list(writer, bGPDframe, &gpl->frames);
      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        /* write strokes */
        BLO_write_struct_list(writer, bGPDstroke, &gpf->strokes);
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          BLO_write_struct_array(writer, bGPDspoint, gps->totpoints, gps->points);
          BLO_write_struct_array(writer, bGPDtriangle, gps->tot_triangles, gps->triangles);
          BKE_defvert_blend_write(writer, gps->totpoints, gps->dvert);
          if (gps->editcurve != NULL) {
            bGPDcurve *gpc = gps->editcurve;
            BLO_write_struct(writer, bGPDcurve, gpc);
            BLO_write_struct_array(
                writer, bGPDcurve_point, gpc->tot_curve_points, gpc->curve_points);
          }
        }
      }
    }
  }
}

void BKE_gpencil_blend_read_data(BlendDataReader *reader, bGPdata *gpd)
{
  /* we must firstly have some grease-pencil data to link! */
  if (gpd == NULL) {
    return;
  }

  /* relink animdata */
  BLO_read_data_address(reader, &gpd->adt);
  BKE_animdata_blend_read_data(reader, gpd->adt);

  /* Ensure full objectmode for linked grease pencil. */
  if (gpd->id.lib != NULL) {
    gpd->flag &= ~GP_DATA_STROKE_PAINTMODE;
    gpd->flag &= ~GP_DATA_STROKE_EDITMODE;
    gpd->flag &= ~GP_DATA_STROKE_SCULPTMODE;
    gpd->flag &= ~GP_DATA_STROKE_WEIGHTMODE;
    gpd->flag &= ~GP_DATA_STROKE_VERTEXMODE;
  }

  /* init stroke buffer */
  gpd->runtime.sbuffer = NULL;
  gpd->runtime.sbuffer_used = 0;
  gpd->runtime.sbuffer_size = 0;
  gpd->runtime.tot_cp_points = 0;

  /* relink palettes (old palettes deprecated, only to convert old files) */
  BLO_read_list(reader, &gpd->palettes);
  if (gpd->palettes.first != NULL) {
    LISTBASE_FOREACH (Palette *, palette, &gpd->palettes) {
      BLO_read_list(reader, &palette->colors);
    }
  }

  /* materials */
  BLO_read_pointer_array(reader, (void **)&gpd->mat);

  /* relink layers */
  BLO_read_list(reader, &gpd->layers);

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* relink frames */
    BLO_read_list(reader, &gpl->frames);

    BLO_read_data_address(reader, &gpl->actframe);

    gpl->runtime.icon_id = 0;

    /* Relink masks. */
    BLO_read_list(reader, &gpl->mask_layers);

    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      /* relink strokes (and their points) */
      BLO_read_list(reader, &gpf->strokes);

      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        /* relink stroke points array */
        BLO_read_data_address(reader, &gps->points);
        /* Relink geometry*/
        BLO_read_data_address(reader, &gps->triangles);

        /* relink stroke edit curve. */
        BLO_read_data_address(reader, &gps->editcurve);
        if (gps->editcurve != NULL) {
          /* relink curve point array */
          BLO_read_data_address(reader, &gps->editcurve->curve_points);
        }

        /* relink weight data */
        if (gps->dvert) {
          BLO_read_data_address(reader, &gps->dvert);
          BKE_defvert_blend_read(reader, gps->totpoints, gps->dvert);
        }
      }
    }
  }
}

static void greasepencil_blend_read_data(BlendDataReader *reader, ID *id)
{
  bGPdata *gpd = (bGPdata *)id;
  BKE_gpencil_blend_read_data(reader, gpd);
}

static void greasepencil_blend_read_lib(BlendLibReader *reader, ID *id)
{
  bGPdata *gpd = (bGPdata *)id;

  /* Relink all data-lock linked by GP data-lock */
  /* Layers */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* Layer -> Parent References */
    BLO_read_id_address(reader, gpd->id.lib, &gpl->parent);
  }

  /* materials */
  for (int a = 0; a < gpd->totcol; a++) {
    BLO_read_id_address(reader, gpd->id.lib, &gpd->mat[a]);
  }
}

static void greasepencil_blend_read_expand(BlendExpander *expander, ID *id)
{
  bGPdata *gpd = (bGPdata *)id;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    BLO_expand(expander, gpl->parent);
  }

  for (int a = 0; a < gpd->totcol; a++) {
    BLO_expand(expander, gpd->mat[a]);
  }
}

IDTypeInfo IDType_ID_GD = {
    .id_code = ID_GD,
    .id_filter = FILTER_ID_GD,
    .main_listbase_index = INDEX_ID_GD,
    .struct_size = sizeof(bGPdata),
    .name = "GPencil",
    .name_plural = "grease_pencils",
    .translation_context = BLT_I18NCONTEXT_ID_GPENCIL,
    .flags = 0,

    .init_data = NULL,
    .copy_data = greasepencil_copy_data,
    .free_data = greasepencil_free_data,
    .make_local = NULL,
    .foreach_id = greasepencil_foreach_id,
    .foreach_cache = NULL,

    .blend_write = greasepencil_blend_write,
    .blend_read_data = greasepencil_blend_read_data,
    .blend_read_lib = greasepencil_blend_read_lib,
    .blend_read_expand = greasepencil_blend_read_expand,

    .blend_read_undo_preserve = NULL,

    .lib_override_apply_post = NULL,
};

/* ************************************************** */
/* Draw Engine */

void (*BKE_gpencil_batch_cache_dirty_tag_cb)(bGPdata *gpd) = NULL;
void (*BKE_gpencil_batch_cache_free_cb)(bGPdata *gpd) = NULL;

void BKE_gpencil_batch_cache_dirty_tag(bGPdata *gpd)
{
  if (gpd) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);
    BKE_gpencil_batch_cache_dirty_tag_cb(gpd);
  }
}

void BKE_gpencil_batch_cache_free(bGPdata *gpd)
{
  if (gpd) {
    BKE_gpencil_batch_cache_free_cb(gpd);
  }
}

/* ************************************************** */
/* Memory Management */

/* clean vertex groups weights */
void BKE_gpencil_free_point_weights(MDeformVert *dvert)
{
  if (dvert == NULL) {
    return;
  }
  MEM_SAFE_FREE(dvert->dw);
}

void BKE_gpencil_free_stroke_weights(bGPDstroke *gps)
{
  if (gps == NULL) {
    return;
  }

  if (gps->dvert == NULL) {
    return;
  }

  for (int i = 0; i < gps->totpoints; i++) {
    MDeformVert *dvert = &gps->dvert[i];
    BKE_gpencil_free_point_weights(dvert);
  }
}

void BKE_gpencil_free_stroke_editcurve(bGPDstroke *gps)
{
  if (gps == NULL) {
    return;
  }
  bGPDcurve *editcurve = gps->editcurve;
  if (editcurve == NULL) {
    return;
  }
  MEM_freeN(editcurve->curve_points);
  MEM_freeN(editcurve);
  gps->editcurve = NULL;
}

/* free stroke, doesn't unlink from any listbase */
void BKE_gpencil_free_stroke(bGPDstroke *gps)
{
  if (gps == NULL) {
    return;
  }
  /* free stroke memory arrays, then stroke itself */
  if (gps->points) {
    MEM_freeN(gps->points);
  }
  if (gps->dvert) {
    BKE_gpencil_free_stroke_weights(gps);
    MEM_freeN(gps->dvert);
  }
  if (gps->triangles) {
    MEM_freeN(gps->triangles);
  }
  if (gps->editcurve != NULL) {
    BKE_gpencil_free_stroke_editcurve(gps);
  }

  MEM_freeN(gps);
}

/* Free strokes belonging to a gp-frame */
bool BKE_gpencil_free_strokes(bGPDframe *gpf)
{
  bool changed = (BLI_listbase_is_empty(&gpf->strokes) == false);

  /* free strokes */
  LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
    BKE_gpencil_free_stroke(gps);
  }
  BLI_listbase_clear(&gpf->strokes);

  return changed;
}

/* Free all of a gp-layer's frames */
void BKE_gpencil_free_frames(bGPDlayer *gpl)
{
  bGPDframe *gpf_next;

  /* error checking */
  if (gpl == NULL) {
    return;
  }

  /* free frames */
  for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf_next) {
    gpf_next = gpf->next;

    /* free strokes and their associated memory */
    BKE_gpencil_free_strokes(gpf);
    BLI_freelinkN(&gpl->frames, gpf);
  }
  gpl->actframe = NULL;
}

void BKE_gpencil_free_layer_masks(bGPDlayer *gpl)
{
  /* Free masks.*/
  bGPDlayer_Mask *mask_next = NULL;
  for (bGPDlayer_Mask *mask = gpl->mask_layers.first; mask; mask = mask_next) {
    mask_next = mask->next;
    BLI_freelinkN(&gpl->mask_layers, mask);
  }
}
/* Free all of the gp-layers for a viewport (list should be &gpd->layers or so) */
void BKE_gpencil_free_layers(ListBase *list)
{
  bGPDlayer *gpl_next;

  /* error checking */
  if (list == NULL) {
    return;
  }

  /* delete layers */
  for (bGPDlayer *gpl = list->first; gpl; gpl = gpl_next) {
    gpl_next = gpl->next;

    /* free layers and their data */
    BKE_gpencil_free_frames(gpl);

    /* Free masks.*/
    BKE_gpencil_free_layer_masks(gpl);

    BLI_freelinkN(list, gpl);
  }
}

/** Free (or release) any data used by this grease pencil (does not free the gpencil itself). */
void BKE_gpencil_free(bGPdata *gpd, bool free_all)
{
  /* free layers */
  BKE_gpencil_free_layers(&gpd->layers);

  /* materials */
  MEM_SAFE_FREE(gpd->mat);

  /* free all data */
  if (free_all) {
    /* clear cache */
    BKE_gpencil_batch_cache_free(gpd);
  }
}

/**
 * Delete grease pencil evaluated data
 * \param gpd_eval: Grease pencil data-block
 */
void BKE_gpencil_eval_delete(bGPdata *gpd_eval)
{
  BKE_gpencil_free(gpd_eval, true);
  BKE_libblock_free_data(&gpd_eval->id, false);
  MEM_freeN(gpd_eval);
}

/**
 * Tag data-block for depsgraph update.
 * Wrapper to avoid include Depsgraph tag functions in other modules.
 * \param gpd: Grease pencil data-block.
 */
void BKE_gpencil_tag(bGPdata *gpd)
{
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
}

/* ************************************************** */
/* Container Creation */

/**
 * Add a new gp-frame to the given layer.
 * \param gpl: Grease pencil layer
 * \param cframe: Frame number
 * \return Pointer to new frame
 */
bGPDframe *BKE_gpencil_frame_addnew(bGPDlayer *gpl, int cframe)
{
  bGPDframe *gpf = NULL, *gf = NULL;
  short state = 0;

  /* error checking */
  if (gpl == NULL) {
    return NULL;
  }

  /* allocate memory for this frame */
  gpf = MEM_callocN(sizeof(bGPDframe), "bGPDframe");
  gpf->framenum = cframe;

  /* find appropriate place to add frame */
  if (gpl->frames.first) {
    for (gf = gpl->frames.first; gf; gf = gf->next) {
      /* check if frame matches one that is supposed to be added */
      if (gf->framenum == cframe) {
        state = -1;
        break;
      }

      /* if current frame has already exceeded the frame to add, add before */
      if (gf->framenum > cframe) {
        BLI_insertlinkbefore(&gpl->frames, gf, gpf);
        state = 1;
        break;
      }
    }
  }

  /* check whether frame was added successfully */
  if (state == -1) {
    CLOG_ERROR(
        &LOG, "Frame (%d) existed already for this layer_active. Using existing frame", cframe);

    /* free the newly created one, and use the old one instead */
    MEM_freeN(gpf);

    /* return existing frame instead... */
    BLI_assert(gf != NULL);
    gpf = gf;
  }
  else if (state == 0) {
    /* add to end then! */
    BLI_addtail(&gpl->frames, gpf);
  }

  /* return frame */
  return gpf;
}

/**
 * Add a copy of the active gp-frame to the given layer.
 * \param gpl: Grease pencil layer
 * \param cframe: Frame number
 * \return Pointer to new frame
 */
bGPDframe *BKE_gpencil_frame_addcopy(bGPDlayer *gpl, int cframe)
{
  bGPDframe *new_frame;
  bool found = false;

  /* Error checking/handling */
  if (gpl == NULL) {
    /* no layer */
    return NULL;
  }
  if (gpl->actframe == NULL) {
    /* no active frame, so just create a new one from scratch */
    return BKE_gpencil_frame_addnew(gpl, cframe);
  }

  /* Create a copy of the frame */
  new_frame = BKE_gpencil_frame_duplicate(gpl->actframe, true);

  /* Find frame to insert it before */
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    if (gpf->framenum > cframe) {
      /* Add it here */
      BLI_insertlinkbefore(&gpl->frames, gpf, new_frame);

      found = true;
      break;
    }
    if (gpf->framenum == cframe) {
      /* This only happens when we're editing with framelock on...
       * - Delete the new frame and don't do anything else here...
       */
      BKE_gpencil_free_strokes(new_frame);
      MEM_freeN(new_frame);
      new_frame = NULL;

      found = true;
      break;
    }
  }

  if (found == false) {
    /* Add new frame to the end */
    BLI_addtail(&gpl->frames, new_frame);
  }

  /* Ensure that frame is set up correctly, and return it */
  if (new_frame) {
    new_frame->framenum = cframe;
    gpl->actframe = new_frame;
  }

  return new_frame;
}

/**
 * Add a new gp-layer and make it the active layer.
 * \param gpd: Grease pencil data-block
 * \param name: Name of the layer
 * \param setactive: Set as active
 * \return Pointer to new layer
 */
bGPDlayer *BKE_gpencil_layer_addnew(bGPdata *gpd, const char *name, bool setactive)
{
  bGPDlayer *gpl = NULL;
  bGPDlayer *gpl_active = NULL;

  /* check that list is ok */
  if (gpd == NULL) {
    return NULL;
  }

  /* allocate memory for frame and add to end of list */
  gpl = MEM_callocN(sizeof(bGPDlayer), "bGPDlayer");

  gpl_active = BKE_gpencil_layer_active_get(gpd);

  /* Add to data-block. */
  if (gpl_active == NULL) {
    BLI_addtail(&gpd->layers, gpl);
  }
  else {
    /* if active layer, add after that layer */
    BLI_insertlinkafter(&gpd->layers, gpl_active, gpl);
  }

  /* annotation vs GP Object behavior is slightly different */
  if (gpd->flag & GP_DATA_ANNOTATIONS) {
    /* set default color of new strokes for this layer */
    copy_v4_v4(gpl->color, U.gpencil_new_layer_col);
    gpl->opacity = 1.0f;

    /* set default thickness of new strokes for this layer */
    gpl->thickness = 3;

    /* Onion colors */
    ARRAY_SET_ITEMS(gpl->gcolor_prev, 0.302f, 0.851f, 0.302f);
    ARRAY_SET_ITEMS(gpl->gcolor_next, 0.250f, 0.1f, 1.0f);
  }
  else {
    /* thickness parameter represents "thickness change", not absolute thickness */
    gpl->thickness = 0;
    gpl->opacity = 1.0f;
    /* default channel color */
    ARRAY_SET_ITEMS(gpl->color, 0.2f, 0.2f, 0.2f);
    /* Default vertex mix. */
    gpl->vertex_paint_opacity = 1.0f;
    /* Enable onion skin. */
    gpl->onion_flag |= GP_LAYER_ONIONSKIN;
  }

  /* auto-name */
  BLI_strncpy(gpl->info, name, sizeof(gpl->info));
  BLI_uniquename(&gpd->layers,
                 gpl,
                 (gpd->flag & GP_DATA_ANNOTATIONS) ? DATA_("Note") : DATA_("GP_Layer"),
                 '.',
                 offsetof(bGPDlayer, info),
                 sizeof(gpl->info));

  /* Enable always affected by scene lights. */
  gpl->flag |= GP_LAYER_USE_LIGHTS;

  /* Init transform. */
  zero_v3(gpl->location);
  zero_v3(gpl->rotation);
  copy_v3_fl(gpl->scale, 1.0f);
  loc_eul_size_to_mat4(gpl->layer_mat, gpl->location, gpl->rotation, gpl->scale);
  invert_m4_m4(gpl->layer_invmat, gpl->layer_mat);

  /* make this one the active one */
  if (setactive) {
    BKE_gpencil_layer_active_set(gpd, gpl);
  }

  /* return layer */
  return gpl;
}

/**
 * Add a new grease pencil data-block.
 * \param bmain: Main pointer
 * \param name: Name of the datablock
 * \return Pointer to new data-block
 */
bGPdata *BKE_gpencil_data_addnew(Main *bmain, const char name[])
{
  bGPdata *gpd;

  /* allocate memory for a new block */
  gpd = BKE_libblock_alloc(bmain, ID_GD, name, 0);

  /* initial settings */
  gpd->flag = (GP_DATA_DISPINFO | GP_DATA_EXPAND);

  /* general flags */
  gpd->flag |= GP_DATA_VIEWALIGN;
  /* always enable object onion skin switch */
  gpd->flag |= GP_DATA_SHOW_ONIONSKINS;
  /* GP object specific settings */
  ARRAY_SET_ITEMS(gpd->line_color, 0.6f, 0.6f, 0.6f, 0.5f);

  gpd->pixfactor = GP_DEFAULT_PIX_FACTOR;

  gpd->curve_edit_resolution = GP_DEFAULT_CURVE_RESOLUTION;
  gpd->curve_edit_threshold = GP_DEFAULT_CURVE_ERROR;
  gpd->curve_edit_corner_angle = GP_DEFAULT_CURVE_EDIT_CORNER_ANGLE;

  /* use adaptive curve resolution by default */
  gpd->flag |= GP_DATA_CURVE_ADAPTIVE_RESOLUTION;

  gpd->zdepth_offset = 0.150f;

  /* grid settings */
  ARRAY_SET_ITEMS(gpd->grid.color, 0.5f, 0.5f, 0.5f); /* Color */
  ARRAY_SET_ITEMS(gpd->grid.scale, 1.0f, 1.0f);       /* Scale */
  gpd->grid.lines = GP_DEFAULT_GRID_LINES;            /* Number of lines */

  /* Onion-skinning settings (data-block level) */
  gpd->onion_keytype = -1; /* All by default. */
  gpd->onion_flag |= (GP_ONION_GHOST_PREVCOL | GP_ONION_GHOST_NEXTCOL);
  gpd->onion_flag |= GP_ONION_FADE;
  gpd->onion_mode = GP_ONION_MODE_RELATIVE;
  gpd->onion_factor = 0.5f;
  ARRAY_SET_ITEMS(gpd->gcolor_prev, 0.145098f, 0.419608f, 0.137255f); /* green */
  ARRAY_SET_ITEMS(gpd->gcolor_next, 0.125490f, 0.082353f, 0.529412f); /* blue */
  gpd->gstep = 1;
  gpd->gstep_next = 1;

  return gpd;
}

/* ************************************************** */
/* Primitive Creation */
/* Utilities for easier bulk-creation of geometry */

/**
 * Populate stroke with point data from data buffers.
 * \param gps: Grease pencil stroke
 * \param array: Flat array of point data values. Each entry has #GP_PRIM_DATABUF_SIZE values.
 * \param totpoints: Total of points
 * \param mat: 4x4 transform matrix to transform points into the right coordinate space.
 */
void BKE_gpencil_stroke_add_points(bGPDstroke *gps,
                                   const float *array,
                                   const int totpoints,
                                   const float mat[4][4])
{
  for (int i = 0; i < totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    const int x = GP_PRIM_DATABUF_SIZE * i;

    pt->x = array[x];
    pt->y = array[x + 1];
    pt->z = array[x + 2];
    mul_m4_v3(mat, &pt->x);

    pt->pressure = array[x + 3];
    pt->strength = array[x + 4];
  }
}

/**
 * Create a new stroke, with pre-allocated data buffers.
 * \param mat_idx: Index of the material
 * \param totpoints: Total points
 * \param thickness: Stroke thickness
 * \return Pointer to new stroke
 */
bGPDstroke *BKE_gpencil_stroke_new(int mat_idx, int totpoints, short thickness)
{
  /* allocate memory for a new stroke */
  bGPDstroke *gps = MEM_callocN(sizeof(bGPDstroke), "gp_stroke");

  gps->thickness = thickness;
  gps->fill_opacity_fac = 1.0f;
  gps->hardeness = 1.0f;
  copy_v2_fl(gps->aspect_ratio, 1.0f);

  gps->uv_scale = 1.0f;

  gps->inittime = 0;

  gps->flag = GP_STROKE_3DSPACE;

  gps->totpoints = totpoints;
  if (gps->totpoints > 0) {
    gps->points = MEM_callocN(sizeof(bGPDspoint) * gps->totpoints, "gp_stroke_points");
  }
  else {
    gps->points = NULL;
  }

  /* initialize triangle memory to dummy data */
  gps->triangles = NULL;
  gps->tot_triangles = 0;

  gps->mat_nr = mat_idx;

  gps->dvert = NULL;
  gps->editcurve = NULL;

  return gps;
}

/**
 * Create a new stroke and add to frame.
 * \param gpf: Grease pencil frame
 * \param mat_idx: Material index
 * \param totpoints: Total points
 * \param thickness: Stroke thickness
 * \param insert_at_head: Add to the head of the strokes list
 * \return Pointer to new stroke
 */
bGPDstroke *BKE_gpencil_stroke_add(
    bGPDframe *gpf, int mat_idx, int totpoints, short thickness, const bool insert_at_head)
{
  bGPDstroke *gps = BKE_gpencil_stroke_new(mat_idx, totpoints, thickness);

  /* Add to frame. */
  if ((gps != NULL) && (gpf != NULL)) {
    if (!insert_at_head) {
      BLI_addtail(&gpf->strokes, gps);
    }
    else {
      BLI_addhead(&gpf->strokes, gps);
    }
  }

  return gps;
}

/**
 * Add a stroke and copy the temporary drawing color value
 * from one of the existing stroke.
 * \param gpf: Grease pencil frame
 * \param existing: Stroke with the style to copy
 * \param mat_idx: Material index
 * \param totpoints: Total points
 * \param thickness: Stroke thickness
 * \return Pointer to new stroke
 */
bGPDstroke *BKE_gpencil_stroke_add_existing_style(
    bGPDframe *gpf, bGPDstroke *existing, int mat_idx, int totpoints, short thickness)
{
  bGPDstroke *gps = BKE_gpencil_stroke_add(gpf, mat_idx, totpoints, thickness, false);
  /* Copy run-time color data so that strokes added in the modifier has the style.
   * There are depsgraph reference pointers inside,
   * change the copy function if interfere with future drawing implementation. */
  memcpy(&gps->runtime, &existing->runtime, sizeof(bGPDstroke_Runtime));
  return gps;
}

bGPDcurve *BKE_gpencil_stroke_editcurve_new(const int tot_curve_points)
{
  bGPDcurve *new_gp_curve = (bGPDcurve *)MEM_callocN(sizeof(bGPDcurve), __func__);
  new_gp_curve->tot_curve_points = tot_curve_points;
  new_gp_curve->curve_points = (bGPDcurve_point *)MEM_callocN(
      sizeof(bGPDcurve_point) * tot_curve_points, __func__);

  return new_gp_curve;
}

/* ************************************************** */
/* Data Duplication */

/**
 * Make a copy of a given gpencil weights.
 * \param gps_src: Source grease pencil stroke
 * \param gps_dst: Destination grease pencil stroke
 */
void BKE_gpencil_stroke_weights_duplicate(bGPDstroke *gps_src, bGPDstroke *gps_dst)
{
  if (gps_src == NULL) {
    return;
  }
  BLI_assert(gps_src->totpoints == gps_dst->totpoints);

  BKE_defvert_array_copy(gps_dst->dvert, gps_src->dvert, gps_src->totpoints);
}

/* Make a copy of a given gpencil stroke editcurve */
bGPDcurve *BKE_gpencil_stroke_curve_duplicate(bGPDcurve *gpc_src)
{
  bGPDcurve *gpc_dst = MEM_dupallocN(gpc_src);

  if (gpc_src->curve_points != NULL) {
    gpc_dst->curve_points = MEM_dupallocN(gpc_src->curve_points);
  }

  return gpc_dst;
}

/**
 * Make a copy of a given grease-pencil stroke.
 * \param gps_src: Source grease pencil strokes.
 * \param dup_points: Duplicate points data.
 * \param dup_curve: Duplicate curve data.
 * \return Pointer to new stroke.
 */
bGPDstroke *BKE_gpencil_stroke_duplicate(bGPDstroke *gps_src,
                                         const bool dup_points,
                                         const bool dup_curve)
{
  bGPDstroke *gps_dst = NULL;

  gps_dst = MEM_dupallocN(gps_src);
  gps_dst->prev = gps_dst->next = NULL;
  gps_dst->triangles = MEM_dupallocN(gps_src->triangles);

  if (dup_points) {
    gps_dst->points = MEM_dupallocN(gps_src->points);

    if (gps_src->dvert != NULL) {
      gps_dst->dvert = MEM_dupallocN(gps_src->dvert);
      BKE_gpencil_stroke_weights_duplicate(gps_src, gps_dst);
    }
    else {
      gps_dst->dvert = NULL;
    }
  }
  else {
    gps_dst->points = NULL;
    gps_dst->dvert = NULL;
  }

  if (dup_curve && gps_src->editcurve != NULL) {
    gps_dst->editcurve = BKE_gpencil_stroke_curve_duplicate(gps_src->editcurve);
  }
  else {
    gps_dst->editcurve = NULL;
  }

  /* return new stroke */
  return gps_dst;
}

/**
 * Make a copy of a given gpencil frame.
 * \param gpf_src: Source grease pencil frame
 * \return Pointer to new frame
 */
bGPDframe *BKE_gpencil_frame_duplicate(const bGPDframe *gpf_src, const bool dup_strokes)
{
  bGPDstroke *gps_dst = NULL;
  bGPDframe *gpf_dst;

  /* error checking */
  if (gpf_src == NULL) {
    return NULL;
  }

  /* make a copy of the source frame */
  gpf_dst = MEM_dupallocN(gpf_src);
  gpf_dst->prev = gpf_dst->next = NULL;

  /* Copy strokes. */
  BLI_listbase_clear(&gpf_dst->strokes);
  if (dup_strokes) {
    LISTBASE_FOREACH (bGPDstroke *, gps_src, &gpf_src->strokes) {
      /* make copy of source stroke */
      gps_dst = BKE_gpencil_stroke_duplicate(gps_src, true, true);
      BLI_addtail(&gpf_dst->strokes, gps_dst);
    }
  }

  /* return new frame */
  return gpf_dst;
}

/**
 * Make a copy of strokes between gpencil frames.
 * \param gpf_src: Source grease pencil frame
 * \param gpf_dst: Destination grease pencil frame
 */
void BKE_gpencil_frame_copy_strokes(bGPDframe *gpf_src, struct bGPDframe *gpf_dst)
{
  bGPDstroke *gps_dst = NULL;
  /* error checking */
  if ((gpf_src == NULL) || (gpf_dst == NULL)) {
    return;
  }

  /* copy strokes */
  BLI_listbase_clear(&gpf_dst->strokes);
  LISTBASE_FOREACH (bGPDstroke *, gps_src, &gpf_src->strokes) {
    /* make copy of source stroke */
    gps_dst = BKE_gpencil_stroke_duplicate(gps_src, true, true);
    BLI_addtail(&gpf_dst->strokes, gps_dst);
  }
}

/**
 * Make a copy of a given gpencil layer.
 * \param gpl_src: Source grease pencil layer
 * \return Pointer to new layer
 */
bGPDlayer *BKE_gpencil_layer_duplicate(const bGPDlayer *gpl_src,
                                       const bool dup_frames,
                                       const bool dup_strokes)
{
  const bGPDframe *gpf_src;
  bGPDframe *gpf_dst;
  bGPDlayer *gpl_dst;

  /* error checking */
  if (gpl_src == NULL) {
    return NULL;
  }

  /* make a copy of source layer */
  gpl_dst = MEM_dupallocN(gpl_src);
  gpl_dst->prev = gpl_dst->next = NULL;

  /* Copy masks. */
  BLI_listbase_clear(&gpl_dst->mask_layers);
  LISTBASE_FOREACH (bGPDlayer_Mask *, mask_src, &gpl_src->mask_layers) {
    bGPDlayer_Mask *mask_dst = MEM_dupallocN(mask_src);
    mask_dst->prev = mask_dst->next = NULL;
    BLI_addtail(&gpl_dst->mask_layers, mask_dst);
  }

  /* copy frames */
  BLI_listbase_clear(&gpl_dst->frames);
  if (dup_frames) {
    for (gpf_src = gpl_src->frames.first; gpf_src; gpf_src = gpf_src->next) {
      /* make a copy of source frame */
      gpf_dst = BKE_gpencil_frame_duplicate(gpf_src, dup_strokes);
      BLI_addtail(&gpl_dst->frames, gpf_dst);

      /* if source frame was the current layer's 'active' frame, reassign that too */
      if (gpf_src == gpl_dst->actframe) {
        gpl_dst->actframe = gpf_dst;
      }
    }
  }

  /* return new layer */
  return gpl_dst;
}

/**
 * Make a copy of a given gpencil data-block.
 *
 * XXX: Should this be deprecated?
 */
bGPdata *BKE_gpencil_data_duplicate(Main *bmain, const bGPdata *gpd_src, bool internal_copy)
{
  bGPdata *gpd_dst;

  /* Yuck and super-uber-hyper yuck!!!
   * Should be replaceable with a no-main copy (LIB_ID_COPY_NO_MAIN etc.), but not sure about it,
   * so for now keep old code for that one. */

  /* error checking */
  if (gpd_src == NULL) {
    return NULL;
  }

  if (internal_copy) {
    /* make a straight copy for undo buffers used during stroke drawing */
    gpd_dst = MEM_dupallocN(gpd_src);
  }
  else {
    BLI_assert(bmain != NULL);
    gpd_dst = (bGPdata *)BKE_id_copy(bmain, &gpd_src->id);
  }

  /* Copy internal data (layers, etc.) */
  greasepencil_copy_data(bmain, &gpd_dst->id, &gpd_src->id, 0);

  /* return new */
  return gpd_dst;
}

/* ************************************************** */
/* GP Stroke API */

/**
 * Ensure selection status of stroke is in sync with its points.
 * \param gps: Grease pencil stroke
 */
void BKE_gpencil_stroke_sync_selection(bGPdata *gpd, bGPDstroke *gps)
{
  bGPDspoint *pt;
  int i;

  /* error checking */
  if (gps == NULL) {
    return;
  }

  /* we'll stop when we find the first selected point,
   * so initially, we must deselect
   */
  gps->flag &= ~GP_STROKE_SELECT;
  BKE_gpencil_stroke_select_index_set(NULL, gps, true);

  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    if (pt->flag & GP_SPOINT_SELECT) {
      gps->flag |= GP_STROKE_SELECT;
      break;
    }
  }

  if (gps->flag & GP_STROKE_SELECT) {
    BKE_gpencil_stroke_select_index_set(gpd, gps, false);
  }
}

void BKE_gpencil_curve_sync_selection(bGPdata *gpd, bGPDstroke *gps)
{
  bGPDcurve *gpc = gps->editcurve;
  if (gpc == NULL) {
    return;
  }

  gps->flag &= ~GP_STROKE_SELECT;
  BKE_gpencil_stroke_select_index_set(NULL, gps, true);
  gpc->flag &= ~GP_CURVE_SELECT;

  bool is_selected = false;
  for (int i = 0; i < gpc->tot_curve_points; i++) {
    bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
    BezTriple *bezt = &gpc_pt->bezt;

    if (BEZT_ISSEL_ANY(bezt)) {
      gpc_pt->flag |= GP_SPOINT_SELECT;
    }
    else {
      gpc_pt->flag &= ~GP_SPOINT_SELECT;
    }

    if (gpc_pt->flag & GP_SPOINT_SELECT) {
      is_selected = true;
    }
  }

  if (is_selected) {
    gpc->flag |= GP_CURVE_SELECT;
    gps->flag |= GP_STROKE_SELECT;
    BKE_gpencil_stroke_select_index_set(gpd, gps, false);
  }
}

/* Assign unique stroke ID for selection. */
void BKE_gpencil_stroke_select_index_set(bGPdata *gpd, bGPDstroke *gps, const bool reset)
{
  if (!reset) {
    gpd->select_last_index++;
    gps->select_index = gpd->select_last_index;
  }
  else {
    gps->select_index = 0;
  }
}

/* ************************************************** */
/* GP Frame API */

/**
 * Delete the last stroke of the given frame.
 * \param gpl: Grease pencil layer
 * \param gpf: Grease pencil frame
 */
void BKE_gpencil_frame_delete_laststroke(bGPDlayer *gpl, bGPDframe *gpf)
{
  bGPDstroke *gps = (gpf) ? gpf->strokes.last : NULL;
  int cfra = (gpf) ? gpf->framenum : 0; /* assume that the current frame was not locked */

  /* error checking */
  if (ELEM(NULL, gpf, gps)) {
    return;
  }

  /* free the stroke and its data */
  if (gps->points) {
    MEM_freeN(gps->points);
  }
  if (gps->dvert) {
    BKE_gpencil_free_stroke_weights(gps);
    MEM_freeN(gps->dvert);
  }
  MEM_freeN(gps->triangles);
  BLI_freelinkN(&gpf->strokes, gps);

  /* if frame has no strokes after this, delete it */
  if (BLI_listbase_is_empty(&gpf->strokes)) {
    BKE_gpencil_layer_frame_delete(gpl, gpf);
    BKE_gpencil_layer_frame_get(gpl, cfra, GP_GETFRAME_USE_PREV);
  }
}

/* ************************************************** */
/* GP Layer API */

/**
 * Check if the given layer is able to be edited or not.
 * \param gpl: Grease pencil layer
 * \return True if layer is editable
 */
bool BKE_gpencil_layer_is_editable(const bGPDlayer *gpl)
{
  /* Sanity check */
  if (gpl == NULL) {
    return false;
  }

  /* Layer must be: Visible + Editable */
  if ((gpl->flag & (GP_LAYER_HIDE | GP_LAYER_LOCKED)) == 0) {
    return true;
  }

  /* Something failed */
  return false;
}

/**
 * Look up the gp-frame on the requested frame number, but don't add a new one.
 * \param gpl: Grease pencil layer
 * \param cframe: Frame number
 * \return Pointer to frame
 */
bGPDframe *BKE_gpencil_layer_frame_find(bGPDlayer *gpl, int cframe)
{
  bGPDframe *gpf;

  /* Search in reverse order, since this is often used for playback/adding,
   * where it's less likely that we're interested in the earlier frames
   */
  for (gpf = gpl->frames.last; gpf; gpf = gpf->prev) {
    if (gpf->framenum == cframe) {
      return gpf;
    }
  }

  return NULL;
}

/** Get the appropriate gp-frame from a given layer
 * - this sets the layer's actframe var (if allowed to)
 * - extension beyond range (if first gp-frame is after all frame in interest and cannot add)
 *
 * \param gpl: Grease pencil layer
 * \param cframe: Frame number
 * \param addnew: Add option
 * \return Pointer to new frame
 */
bGPDframe *BKE_gpencil_layer_frame_get(bGPDlayer *gpl, int cframe, eGP_GetFrame_Mode addnew)
{
  bGPDframe *gpf = NULL;
  bool found = false;

  /* error checking */
  if (gpl == NULL) {
    return NULL;
  }

  /* check if there is already an active frame */
  if (gpl->actframe) {
    gpf = gpl->actframe;

    /* do not allow any changes to layer's active frame if layer is locked from changes
     * or if the layer has been set to stay on the current frame
     */
    if (gpl->flag & GP_LAYER_FRAMELOCK) {
      return gpf;
    }
    /* do not allow any changes to actframe if frame has painting tag attached to it */
    if (gpf->flag & GP_FRAME_PAINT) {
      return gpf;
    }

    /* try to find matching frame */
    if (gpf->framenum < cframe) {
      for (; gpf; gpf = gpf->next) {
        if (gpf->framenum == cframe) {
          found = true;
          break;
        }
        if ((gpf->next) && (gpf->next->framenum > cframe)) {
          found = true;
          break;
        }
      }

      /* set the appropriate frame */
      if (addnew) {
        if ((found) && (gpf->framenum == cframe)) {
          gpl->actframe = gpf;
        }
        else if (addnew == GP_GETFRAME_ADD_COPY) {
          gpl->actframe = BKE_gpencil_frame_addcopy(gpl, cframe);
        }
        else {
          gpl->actframe = BKE_gpencil_frame_addnew(gpl, cframe);
        }
      }
      else if (found) {
        gpl->actframe = gpf;
      }
      else {
        gpl->actframe = gpl->frames.last;
      }
    }
    else {
      for (; gpf; gpf = gpf->prev) {
        if (gpf->framenum <= cframe) {
          found = true;
          break;
        }
      }

      /* set the appropriate frame */
      if (addnew) {
        if ((found) && (gpf->framenum == cframe)) {
          gpl->actframe = gpf;
        }
        else if (addnew == GP_GETFRAME_ADD_COPY) {
          gpl->actframe = BKE_gpencil_frame_addcopy(gpl, cframe);
        }
        else {
          gpl->actframe = BKE_gpencil_frame_addnew(gpl, cframe);
        }
      }
      else if (found) {
        gpl->actframe = gpf;
      }
      else {
        gpl->actframe = gpl->frames.first;
      }
    }
  }
  else if (gpl->frames.first) {
    /* check which of the ends to start checking from */
    const int first = ((bGPDframe *)(gpl->frames.first))->framenum;
    const int last = ((bGPDframe *)(gpl->frames.last))->framenum;

    if (abs(cframe - first) > abs(cframe - last)) {
      /* find gp-frame which is less than or equal to cframe */
      for (gpf = gpl->frames.last; gpf; gpf = gpf->prev) {
        if (gpf->framenum <= cframe) {
          found = true;
          break;
        }
      }
    }
    else {
      /* find gp-frame which is less than or equal to cframe */
      for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
        if (gpf->framenum <= cframe) {
          found = true;
          break;
        }
      }
    }

    /* set the appropriate frame */
    if (addnew) {
      if ((found) && (gpf->framenum == cframe)) {
        gpl->actframe = gpf;
      }
      else {
        gpl->actframe = BKE_gpencil_frame_addnew(gpl, cframe);
      }
    }
    else if (found) {
      gpl->actframe = gpf;
    }
    else {
      /* If delete first frame, need to find one. */
      if (gpl->frames.first != NULL) {
        gpl->actframe = gpl->frames.first;
      }
      else {
        /* unresolved errogenous situation! */
        CLOG_STR_ERROR(&LOG, "cannot find appropriate gp-frame");
        /* gpl->actframe should still be NULL */
      }
    }
  }
  else {
    /* currently no frames (add if allowed to) */
    if (addnew) {
      gpl->actframe = BKE_gpencil_frame_addnew(gpl, cframe);
    }
    else {
      /* don't do anything... this may be when no frames yet! */
      /* gpl->actframe should still be NULL */
    }
  }

  /* Don't select first frame if greater than current frame. */
  if ((gpl->actframe != NULL) && (gpl->actframe == gpl->frames.first) &&
      (gpl->actframe->framenum > cframe)) {
    gpl->actframe = NULL;
  }

  /* return */
  return gpl->actframe;
}

/**
 * Delete the given frame from a layer.
 * \param gpl: Grease pencil layer
 * \param gpf: Grease pencil frame
 * \return True if delete was done
 */
bool BKE_gpencil_layer_frame_delete(bGPDlayer *gpl, bGPDframe *gpf)
{
  bool changed = false;

  /* error checking */
  if (ELEM(NULL, gpl, gpf)) {
    return false;
  }

  /* if this frame was active, make the previous frame active instead
   * since it's tricky to set active frame otherwise
   */
  if (gpl->actframe == gpf) {
    gpl->actframe = gpf->prev;
  }

  /* free the frame and its data */
  changed = BKE_gpencil_free_strokes(gpf);
  BLI_freelinkN(&gpl->frames, gpf);

  return changed;
}

/**
 * Get layer by name
 * \param gpd: Grease pencil data-block
 * \param name: Layer name
 * \return Pointer to layer
 */
bGPDlayer *BKE_gpencil_layer_named_get(bGPdata *gpd, const char *name)
{
  if (name[0] == '\0') {
    return NULL;
  }
  return BLI_findstring(&gpd->layers, name, offsetof(bGPDlayer, info));
}

/**
 * Get mask layer by name.
 * \param gpl: Grease pencil layer
 * \param name: Mask name
 * \return Pointer to mask layer
 */
bGPDlayer_Mask *BKE_gpencil_layer_mask_named_get(bGPDlayer *gpl, const char *name)
{
  if (name[0] == '\0') {
    return NULL;
  }
  return BLI_findstring(&gpl->mask_layers, name, offsetof(bGPDlayer_Mask, name));
}

/**
 * Add grease pencil mask layer.
 * \param gpl: Grease pencil layer
 * \param name: Name of the mask
 * \return Pointer to new mask layer
 */
bGPDlayer_Mask *BKE_gpencil_layer_mask_add(bGPDlayer *gpl, const char *name)
{

  bGPDlayer_Mask *mask = MEM_callocN(sizeof(bGPDlayer_Mask), "bGPDlayer_Mask");
  BLI_addtail(&gpl->mask_layers, mask);
  BLI_strncpy(mask->name, name, sizeof(mask->name));
  gpl->act_mask++;

  return mask;
}

/**
 * Remove grease pencil mask layer.
 * \param gpl: Grease pencil layer
 * \param mask: Grease pencil mask layer
 */
void BKE_gpencil_layer_mask_remove(bGPDlayer *gpl, bGPDlayer_Mask *mask)
{
  BLI_freelinkN(&gpl->mask_layers, mask);
  gpl->act_mask--;
  CLAMP_MIN(gpl->act_mask, 0);
}

/**
 * Remove any reference to mask layer.
 * \param gpd: Grease pencil data-block
 * \param name: Name of the mask layer
 */
void BKE_gpencil_layer_mask_remove_ref(bGPdata *gpd, const char *name)
{
  bGPDlayer_Mask *mask_next;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    for (bGPDlayer_Mask *mask = gpl->mask_layers.first; mask; mask = mask_next) {
      mask_next = mask->next;
      if (STREQ(mask->name, name)) {
        BKE_gpencil_layer_mask_remove(gpl, mask);
      }
    }
  }
}

static int gpencil_cb_sort_masks(const void *arg1, const void *arg2)
{
  /* sort is inverted as layer list. */
  const struct bGPDlayer_Mask *mask1 = arg1;
  const struct bGPDlayer_Mask *mask2 = arg2;
  int val = 0;

  if (mask1->sort_index < mask2->sort_index) {
    val = 1;
  }
  else if (mask1->sort_index > mask2->sort_index) {
    val = -1;
  }

  return val;
}

/**
 * Sort grease pencil mask layers.
 * \param gpd: Grease pencil data-block
 * \param gpl: Grease pencil layer
 */
void BKE_gpencil_layer_mask_sort(bGPdata *gpd, bGPDlayer *gpl)
{
  /* Update sort index. */
  LISTBASE_FOREACH (bGPDlayer_Mask *, mask, &gpl->mask_layers) {
    bGPDlayer *gpl_mask = BKE_gpencil_layer_named_get(gpd, mask->name);
    if (gpl_mask != NULL) {
      mask->sort_index = BLI_findindex(&gpd->layers, gpl_mask);
    }
    else {
      mask->sort_index = 0;
    }
  }
  BLI_listbase_sort(&gpl->mask_layers, gpencil_cb_sort_masks);
}

/**
 * Sort all grease pencil mask layer.
 * \param gpd: Grease pencil data-block
 */
void BKE_gpencil_layer_mask_sort_all(bGPdata *gpd)
{
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    BKE_gpencil_layer_mask_sort(gpd, gpl);
  }
}

static int gpencil_cb_cmp_frame(void *thunk, const void *a, const void *b)
{
  const bGPDframe *frame_a = a;
  const bGPDframe *frame_b = b;

  if (frame_a->framenum < frame_b->framenum) {
    return -1;
  }
  if (frame_a->framenum > frame_b->framenum) {
    return 1;
  }
  if (thunk != NULL) {
    *((bool *)thunk) = true;
  }
  /* Sort selected last. */
  if ((frame_a->flag & GP_FRAME_SELECT) && ((frame_b->flag & GP_FRAME_SELECT) == 0)) {
    return 1;
  }
  return 0;
}

/**
 * Sort grease pencil frames.
 * \param gpl: Grease pencil layer
 * \param r_has_duplicate_frames: Duplicated frames flag
 */
void BKE_gpencil_layer_frames_sort(struct bGPDlayer *gpl, bool *r_has_duplicate_frames)
{
  BLI_listbase_sort_r(&gpl->frames, gpencil_cb_cmp_frame, r_has_duplicate_frames);
}

/**
 * Get the active grease pencil layer for editing.
 * \param gpd: Grease pencil data-block
 * \return Pointer to layer
 */
bGPDlayer *BKE_gpencil_layer_active_get(bGPdata *gpd)
{
  /* error checking */
  if (ELEM(NULL, gpd, gpd->layers.first)) {
    return NULL;
  }

  /* loop over layers until found (assume only one active) */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (gpl->flag & GP_LAYER_ACTIVE) {
      return gpl;
    }
  }

  /* no active layer found */
  return NULL;
}

/**
 * Set active grease pencil layer.
 * \param gpd: Grease pencil data-block
 * \param active: Grease pencil layer to set as active
 */
void BKE_gpencil_layer_active_set(bGPdata *gpd, bGPDlayer *active)
{
  /* error checking */
  if (ELEM(NULL, gpd, gpd->layers.first, active)) {
    return;
  }

  /* loop over layers deactivating all */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    gpl->flag &= ~GP_LAYER_ACTIVE;
    if (gpd->flag & GP_DATA_AUTOLOCK_LAYERS) {
      gpl->flag |= GP_LAYER_LOCKED;
    }
  }

  /* set as active one */
  active->flag |= GP_LAYER_ACTIVE;
  if (gpd->flag & GP_DATA_AUTOLOCK_LAYERS) {
    active->flag &= ~GP_LAYER_LOCKED;
  }
}

/**
 * Set locked layers for autolock mode.
 * \param gpd: Grease pencil data-block
 * \param unlock: Unlock flag
 */
void BKE_gpencil_layer_autolock_set(bGPdata *gpd, const bool unlock)
{
  BLI_assert(gpd != NULL);

  if (gpd->flag & GP_DATA_AUTOLOCK_LAYERS) {
    bGPDlayer *layer_active = BKE_gpencil_layer_active_get(gpd);

    /* Lock all other layers */
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      /* unlock active layer */
      if (gpl == layer_active) {
        gpl->flag &= ~GP_LAYER_LOCKED;
      }
      else {
        gpl->flag |= GP_LAYER_LOCKED;
      }
    }
  }
  else {
    /* If disable is better unlock all layers by default or it looks there is
     * a problem in the UI because the user expects all layers will be unlocked
     */
    if (unlock) {
      LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
        gpl->flag &= ~GP_LAYER_LOCKED;
      }
    }
  }
}

/**
 * Delete grease pencil layer.
 * \param gpd: Grease pencil data-block
 * \param gpl: Grease pencil layer
 */
void BKE_gpencil_layer_delete(bGPdata *gpd, bGPDlayer *gpl)
{
  /* error checking */
  if (ELEM(NULL, gpd, gpl)) {
    return;
  }

  /* free layer */
  BKE_gpencil_free_frames(gpl);

  /* Free Masks. */
  BKE_gpencil_free_layer_masks(gpl);

  /* Remove any reference to that layer in masking lists. */
  BKE_gpencil_layer_mask_remove_ref(gpd, gpl->info);

  /* free icon providing preview of icon color */
  BKE_icon_delete(gpl->runtime.icon_id);

  BLI_freelinkN(&gpd->layers, gpl);
}

/**
 * Get grease pencil material from brush.
 * \param brush: Brush
 * \return Pointer to material
 */
Material *BKE_gpencil_brush_material_get(Brush *brush)
{
  Material *ma = NULL;

  if ((brush != NULL) && (brush->gpencil_settings != NULL) &&
      (brush->gpencil_settings->material != NULL)) {
    ma = brush->gpencil_settings->material;
  }

  return ma;
}

/**
 * Set grease pencil brush material.
 * \param brush: Brush
 * \param ma: Material
 */
void BKE_gpencil_brush_material_set(Brush *brush, Material *ma)
{
  BLI_assert(brush);
  BLI_assert(brush->gpencil_settings);
  if (brush->gpencil_settings->material != ma) {
    if (brush->gpencil_settings->material) {
      id_us_min(&brush->gpencil_settings->material->id);
    }
    if (ma) {
      id_us_plus(&ma->id);
    }
    brush->gpencil_settings->material = ma;
  }
}

/**
 * Adds the pinned material to the object if necessary.
 * \param bmain: Main pointer
 * \param ob: Grease pencil object
 * \param brush: Brush
 * \return Pointer to material
 */
Material *BKE_gpencil_object_material_ensure_from_brush(Main *bmain, Object *ob, Brush *brush)
{
  if (brush->gpencil_settings->flag & GP_BRUSH_MATERIAL_PINNED) {
    Material *ma = BKE_gpencil_brush_material_get(brush);

    /* check if the material is already on object material slots and add it if missing */
    if (ma && BKE_gpencil_object_material_index_get(ob, ma) < 0) {
      BKE_object_material_slot_add(bmain, ob);
      BKE_object_material_assign(bmain, ob, ma, ob->totcol, BKE_MAT_ASSIGN_USERPREF);
    }

    return ma;
  }

  /* using active material instead */
  return BKE_object_material_get(ob, ob->actcol);
}

/**
 * Assigns the material to object (if not already present) and returns its index (mat_nr).
 * \param bmain: Main pointer
 * \param ob: Grease pencil object
 * \param material: Material
 * \return Index of the material
 */
int BKE_gpencil_object_material_ensure(Main *bmain, Object *ob, Material *material)
{
  if (!material) {
    return -1;
  }
  int index = BKE_gpencil_object_material_index_get(ob, material);
  if (index < 0) {
    BKE_object_material_slot_add(bmain, ob);
    BKE_object_material_assign(bmain, ob, material, ob->totcol, BKE_MAT_ASSIGN_USERPREF);
    return ob->totcol - 1;
  }
  return index;
}

/**
 * Creates a new grease-pencil material and assigns it to object.
 * \param bmain: Main pointer
 * \param ob: Grease pencil object
 * \param name: Material name
 * \param r_index: value is set to zero based index of the new material if \a r_index is not NULL.
 * \return Material pointer.
 */
Material *BKE_gpencil_object_material_new(Main *bmain, Object *ob, const char *name, int *r_index)
{
  Material *ma = BKE_gpencil_material_add(bmain, name);
  id_us_min(&ma->id); /* no users yet */

  BKE_object_material_slot_add(bmain, ob);
  BKE_object_material_assign(bmain, ob, ma, ob->totcol, BKE_MAT_ASSIGN_USERPREF);

  if (r_index) {
    *r_index = ob->actcol - 1;
  }
  return ma;
}

/**
 * Returns the material for a brush with respect to its pinned state.
 * \param ob: Grease pencil object
 * \param brush: Brush
 * \return Material pointer
 */
Material *BKE_gpencil_object_material_from_brush_get(Object *ob, Brush *brush)
{
  if ((brush) && (brush->gpencil_settings) &&
      (brush->gpencil_settings->flag & GP_BRUSH_MATERIAL_PINNED)) {
    Material *ma = BKE_gpencil_brush_material_get(brush);
    return ma;
  }

  return BKE_object_material_get(ob, ob->actcol);
}

/**
 * Returns the material index for a brush with respect to its pinned state.
 * \param ob: Grease pencil object
 * \param brush: Brush
 * \return Material index.
 */
int BKE_gpencil_object_material_get_index_from_brush(Object *ob, Brush *brush)
{
  if ((brush) && (brush->gpencil_settings->flag & GP_BRUSH_MATERIAL_PINNED)) {
    return BKE_gpencil_object_material_index_get(ob, brush->gpencil_settings->material);
  }

  return ob->actcol - 1;
}

/**
 * Guaranteed to return a material assigned to object. Returns never NULL.
 * \param bmain: Main pointer
 * \param ob: Grease pencil object
 * \return Material pointer.
 */
Material *BKE_gpencil_object_material_ensure_from_active_input_toolsettings(Main *bmain,
                                                                            Object *ob,
                                                                            ToolSettings *ts)
{
  if (ts && ts->gp_paint && ts->gp_paint->paint.brush) {
    return BKE_gpencil_object_material_ensure_from_active_input_brush(
        bmain, ob, ts->gp_paint->paint.brush);
  }

  return BKE_gpencil_object_material_ensure_from_active_input_brush(bmain, ob, NULL);
}

/**
 * Guaranteed to return a material assigned to object. Returns never NULL.
 * \param bmain: Main pointer
 * \param ob: Grease pencil object.
 * \param brush: Brush
 * \return Material pointer
 */
Material *BKE_gpencil_object_material_ensure_from_active_input_brush(Main *bmain,
                                                                     Object *ob,
                                                                     Brush *brush)
{
  if (brush) {
    Material *ma = BKE_gpencil_object_material_ensure_from_brush(bmain, ob, brush);
    if (ma) {
      return ma;
    }
    if (brush->gpencil_settings->flag & GP_BRUSH_MATERIAL_PINNED) {
      /* it is easier to just unpin a NULL material, instead of setting a new one */
      brush->gpencil_settings->flag &= ~GP_BRUSH_MATERIAL_PINNED;
    }
  }
  return BKE_gpencil_object_material_ensure_from_active_input_material(ob);
}

/**
 * Guaranteed to return a material assigned to object. Returns never NULL.
 * Only use this for materials unrelated to user input.
 * \param ob: Grease pencil object
 * \return Material pointer
 */
Material *BKE_gpencil_object_material_ensure_from_active_input_material(Object *ob)
{
  Material *ma = BKE_object_material_get(ob, ob->actcol);
  if (ma) {
    return ma;
  }

  return BKE_material_default_gpencil();
}

/**
 * Get active color, and add all default settings if we don't find anything.
 * \param ob: Grease pencil object
 * \return Material pointer
 */
Material *BKE_gpencil_object_material_ensure_active(Object *ob)
{
  Material *ma = NULL;

  /* sanity checks */
  if (ob == NULL) {
    return NULL;
  }

  ma = BKE_gpencil_object_material_ensure_from_active_input_material(ob);
  if (ma->gp_style == NULL) {
    BKE_gpencil_material_attr_init(ma);
  }

  return ma;
}

/* ************************************************** */
/**
 * Check if stroke has any point selected
 * \param gps: Grease pencil stroke
 * \return True if selected
 */
bool BKE_gpencil_stroke_select_check(const bGPDstroke *gps)
{
  const bGPDspoint *pt;
  int i;
  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    if (pt->flag & GP_SPOINT_SELECT) {
      return true;
    }
  }
  return false;
}

/* ************************************************** */
/* GP Object - Vertex Groups */

/**
 * Remove a vertex group.
 * \param ob: Grease pencil object
 * \param defgroup: deform group
 */
void BKE_gpencil_vgroup_remove(Object *ob, bDeformGroup *defgroup)
{
  bGPdata *gpd = ob->data;
  MDeformVert *dvert = NULL;
  const int def_nr = BLI_findindex(&ob->defbase, defgroup);
  const int totgrp = BLI_listbase_count(&ob->defbase);

  /* Remove points data */
  if (gpd) {
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          if (gps->dvert != NULL) {
            for (int i = 0; i < gps->totpoints; i++) {
              dvert = &gps->dvert[i];
              MDeformWeight *dw = BKE_defvert_find_index(dvert, def_nr);
              if (dw != NULL) {
                BKE_defvert_remove_group(dvert, dw);
              }
              /* Reorganize weights for other groups after deleted one. */
              for (int g = 0; g < totgrp; g++) {
                dw = BKE_defvert_find_index(dvert, g);
                if ((dw != NULL) && (dw->def_nr > def_nr)) {
                  dw->def_nr--;
                }
              }
            }
          }
        }
      }
    }
  }

  /* Remove the group */
  BLI_freelinkN(&ob->defbase, defgroup);
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
}

/**
 * Ensure stroke has vertex group.
 * \param gps: Grease pencil stroke
 */
void BKE_gpencil_dvert_ensure(bGPDstroke *gps)
{
  if (gps->dvert == NULL) {
    gps->dvert = MEM_callocN(sizeof(MDeformVert) * gps->totpoints, "gp_stroke_weights");
  }
}

/* ************************************************** */

/**
 * Get range of selected frames in layer.
 * Always the active frame is considered as selected, so if no more selected the range
 * will be equal to the current active frame.
 * \param gpl: Layer.
 * \param r_initframe: Number of first selected frame.
 * \param r_endframe: Number of last selected frame.
 */
void BKE_gpencil_frame_range_selected(bGPDlayer *gpl, int *r_initframe, int *r_endframe)
{
  *r_initframe = gpl->actframe->framenum;
  *r_endframe = gpl->actframe->framenum;

  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    if (gpf->flag & GP_FRAME_SELECT) {
      if (gpf->framenum < *r_initframe) {
        *r_initframe = gpf->framenum;
      }
      if (gpf->framenum > *r_endframe) {
        *r_endframe = gpf->framenum;
      }
    }
  }
}

/**
 * Get Falloff factor base on frame range
 * \param gpf: Frame.
 * \param actnum: Number of active frame in layer.
 * \param f_init: Number of first selected frame.
 * \param f_end: Number of last selected frame.
 * \param cur_falloff: Curve with falloff factors.
 */
float BKE_gpencil_multiframe_falloff_calc(
    bGPDframe *gpf, int actnum, int f_init, int f_end, CurveMapping *cur_falloff)
{
  float fnum = 0.5f; /* default mid curve */
  float value;

  /* check curve is available */
  if (cur_falloff == NULL) {
    return 1.0f;
  }

  /* frames to the right of the active frame */
  if (gpf->framenum < actnum) {
    fnum = (float)(gpf->framenum - f_init) / (actnum - f_init);
    fnum *= 0.5f;
    value = BKE_curvemapping_evaluateF(cur_falloff, 0, fnum);
  }
  /* frames to the left of the active frame */
  else if (gpf->framenum > actnum) {
    fnum = (float)(gpf->framenum - actnum) / (f_end - actnum);
    fnum *= 0.5f;
    value = BKE_curvemapping_evaluateF(cur_falloff, 0, fnum + 0.5f);
  }
  else {
    /* Center of the curve. */
    value = BKE_curvemapping_evaluateF(cur_falloff, 0, 0.5f);
  }

  return value;
}

/**
 * Reassign strokes using a material.
 * \param gpd: Grease pencil data-block
 * \param totcol: Total materials
 * \param index: Index of the material
 */
void BKE_gpencil_material_index_reassign(bGPdata *gpd, int totcol, int index)
{
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        /* reassign strokes */
        if ((gps->mat_nr > index) || (gps->mat_nr > totcol - 1)) {
          gps->mat_nr--;
          CLAMP_MIN(gps->mat_nr, 0);
        }
      }
    }
  }
}

/**
 * Remove strokes using a material.
 * \param gpd: Grease pencil data-block
 * \param index: Index of the material
 * \return True if removed
 */
bool BKE_gpencil_material_index_used(bGPdata *gpd, int index)
{
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (gps->mat_nr == index) {
          return true;
        }
      }
    }
  }

  return false;
}

/**
 * Remap material
 * \param gpd: Grease pencil data-block
 * \param remap: Remap index
 * \param remap_len: Remap length
 */
void BKE_gpencil_material_remap(struct bGPdata *gpd,
                                const unsigned int *remap,
                                unsigned int remap_len)
{
  const short remap_len_short = (short)remap_len;

#define MAT_NR_REMAP(n) \
  if (n < remap_len_short) { \
    BLI_assert(n >= 0 && remap[n] < remap_len_short); \
    n = remap[n]; \
  } \
  ((void)0)

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        /* reassign strokes */
        MAT_NR_REMAP(gps->mat_nr);
      }
    }
  }

#undef MAT_NR_REMAP
}

/**
 * Load a table with material conversion index for merged materials.
 * \param ob: Grease pencil object.
 * \param hue_threshold: Threshold for Hue.
 * \param sat_threshold: Threshold for Saturation.
 * \param val_threshold: Threshold for Value.
 * \param r_mat_table: return material table.
 * \return True if done.
 */
bool BKE_gpencil_merge_materials_table_get(Object *ob,
                                           const float hue_threshold,
                                           const float sat_threshold,
                                           const float val_threshold,
                                           GHash *r_mat_table)
{
  bool changed = false;

  Material *ma_primary = NULL;
  Material *ma_secondary = NULL;
  MaterialGPencilStyle *gp_style_primary = NULL;
  MaterialGPencilStyle *gp_style_secondary = NULL;
  GHash *mat_used = BLI_ghash_int_new(__func__);

  short *totcol = BKE_object_material_len_p(ob);
  if (totcol == 0) {
    return changed;
  }

  for (int idx_primary = 0; idx_primary < *totcol; idx_primary++) {
    /* Read primary material to compare. */
    ma_primary = BKE_gpencil_material(ob, idx_primary + 1);
    if (ma_primary == NULL) {
      continue;
    }
    for (int idx_secondary = 0; idx_secondary < *totcol; idx_secondary++) {
      if ((idx_secondary == idx_primary) ||
          BLI_ghash_haskey(r_mat_table, POINTER_FROM_INT(idx_secondary))) {
        continue;
      }
      if (BLI_ghash_haskey(mat_used, POINTER_FROM_INT(idx_secondary))) {
        continue;
      }

      /* Read secondary material to compare with primary material. */
      ma_secondary = BKE_gpencil_material(ob, idx_secondary + 1);
      if ((ma_secondary == NULL) ||
          (BLI_ghash_haskey(r_mat_table, POINTER_FROM_INT(idx_secondary)))) {
        continue;
      }
      gp_style_primary = ma_primary->gp_style;
      gp_style_secondary = ma_secondary->gp_style;

      if ((gp_style_primary == NULL) || (gp_style_secondary == NULL) ||
          (gp_style_secondary->flag & GP_MATERIAL_LOCKED)) {
        continue;
      }

      /* Check materials have the same mode. */
      if (gp_style_primary->mode != gp_style_secondary->mode) {
        continue;
      }

      /* Check materials have same stroke and fill attributes. */
      if ((gp_style_primary->flag & GP_MATERIAL_STROKE_SHOW) !=
          (gp_style_secondary->flag & GP_MATERIAL_STROKE_SHOW)) {
        continue;
      }

      if ((gp_style_primary->flag & GP_MATERIAL_FILL_SHOW) !=
          (gp_style_secondary->flag & GP_MATERIAL_FILL_SHOW)) {
        continue;
      }

      /* Check materials have the same type. */
      if ((gp_style_primary->stroke_style != gp_style_secondary->stroke_style) ||
          (gp_style_primary->fill_style != gp_style_secondary->fill_style)) {
        continue;
      }

      float s_hsv_a[3], s_hsv_b[3], f_hsv_a[3], f_hsv_b[3], col[3];
      zero_v3(s_hsv_a);
      zero_v3(s_hsv_b);
      zero_v3(f_hsv_a);
      zero_v3(f_hsv_b);

      copy_v3_v3(col, gp_style_primary->stroke_rgba);
      rgb_to_hsv_compat_v(col, s_hsv_a);
      copy_v3_v3(col, gp_style_secondary->stroke_rgba);
      rgb_to_hsv_compat_v(col, s_hsv_b);

      copy_v3_v3(col, gp_style_primary->fill_rgba);
      rgb_to_hsv_compat_v(col, f_hsv_a);
      copy_v3_v3(col, gp_style_secondary->fill_rgba);
      rgb_to_hsv_compat_v(col, f_hsv_b);

      /* Check stroke and fill color. */
      if ((!compare_ff(s_hsv_a[0], s_hsv_b[0], hue_threshold)) ||
          (!compare_ff(s_hsv_a[1], s_hsv_b[1], sat_threshold)) ||
          (!compare_ff(s_hsv_a[2], s_hsv_b[2], val_threshold)) ||
          (!compare_ff(f_hsv_a[0], f_hsv_b[0], hue_threshold)) ||
          (!compare_ff(f_hsv_a[1], f_hsv_b[1], sat_threshold)) ||
          (!compare_ff(f_hsv_a[2], f_hsv_b[2], val_threshold)) ||
          (!compare_ff(gp_style_primary->stroke_rgba[3],
                       gp_style_secondary->stroke_rgba[3],
                       val_threshold)) ||
          (!compare_ff(
              gp_style_primary->fill_rgba[3], gp_style_secondary->fill_rgba[3], val_threshold))) {
        continue;
      }

      /* Save conversion indexes. */
      if (!BLI_ghash_haskey(r_mat_table, POINTER_FROM_INT(idx_secondary))) {
        BLI_ghash_insert(
            r_mat_table, POINTER_FROM_INT(idx_secondary), POINTER_FROM_INT(idx_primary));
        changed = true;

        if (!BLI_ghash_haskey(mat_used, POINTER_FROM_INT(idx_primary))) {
          BLI_ghash_insert(mat_used, POINTER_FROM_INT(idx_primary), POINTER_FROM_INT(idx_primary));
        }
      }
    }
  }
  /* Free hash memory. */
  BLI_ghash_free(mat_used, NULL, NULL);

  return changed;
}

/**
 * Merge similar materials
 * \param ob: Grease pencil object
 * \param hue_threshold: Threshold for Hue
 * \param sat_threshold: Threshold for Saturation
 * \param val_threshold: Threshold for Value
 * \param r_removed: Number of materials removed
 * \return True if done
 */
bool BKE_gpencil_merge_materials(Object *ob,
                                 const float hue_threshold,
                                 const float sat_threshold,
                                 const float val_threshold,
                                 int *r_removed)
{
  bGPdata *gpd = ob->data;

  short *totcol = BKE_object_material_len_p(ob);
  if (totcol == 0) {
    *r_removed = 0;
    return 0;
  }

  /* Review materials. */
  GHash *mat_table = BLI_ghash_int_new(__func__);

  bool changed = BKE_gpencil_merge_materials_table_get(
      ob, hue_threshold, sat_threshold, val_threshold, mat_table);

  *r_removed = BLI_ghash_len(mat_table);

  /* Update stroke material index. */
  if (changed) {
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      if (gpl->flag & GP_LAYER_HIDE) {
        continue;
      }

      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* Check if the color is editable. */
          MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
          if (gp_style != NULL) {
            if (gp_style->flag & GP_MATERIAL_HIDE) {
              continue;
            }
            if (((gpl->flag & GP_LAYER_UNLOCK_COLOR) == 0) &&
                (gp_style->flag & GP_MATERIAL_LOCKED)) {
              continue;
            }
          }

          if (BLI_ghash_haskey(mat_table, POINTER_FROM_INT(gps->mat_nr))) {
            int *idx = BLI_ghash_lookup(mat_table, POINTER_FROM_INT(gps->mat_nr));
            gps->mat_nr = POINTER_AS_INT(idx);
          }
        }
      }
    }
  }

  /* Free hash memory. */
  BLI_ghash_free(mat_table, NULL, NULL);

  return changed;
}

/**
 * Calc grease pencil statistics functions.
 * \param gpd: Grease pencil data-block
 */
void BKE_gpencil_stats_update(bGPdata *gpd)
{
  gpd->totlayer = 0;
  gpd->totframe = 0;
  gpd->totstroke = 0;
  gpd->totpoint = 0;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    gpd->totlayer++;
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      gpd->totframe++;
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        gpd->totstroke++;
        gpd->totpoint += gps->totpoints;
      }
    }
  }
}

/**
 * Get material index (0-based like mat_nr not actcol).
 * \param ob: Grease pencil object
 * \param ma: Material
 * \return Index of the material
 */
int BKE_gpencil_object_material_index_get(Object *ob, Material *ma)
{
  short *totcol = BKE_object_material_len_p(ob);
  Material *read_ma = NULL;
  for (short i = 0; i < *totcol; i++) {
    read_ma = BKE_object_material_get(ob, i + 1);
    if (ma == read_ma) {
      return i;
    }
  }

  return -1;
}

/**
 * Create a default palette.
 * \param bmain: Main pointer
 * \param scene: Scene
 */
void BKE_gpencil_palette_ensure(Main *bmain, Scene *scene)
{
  const char *hexcol[] = {
      "FFFFFF", "F2F2F2", "E6E6E6", "D9D9D9", "CCCCCC", "BFBFBF", "B2B2B2", "A6A6A6", "999999",
      "8C8C8C", "808080", "737373", "666666", "595959", "4C4C4C", "404040", "333333", "262626",
      "1A1A1A", "000000", "F2FC24", "FFEA00", "FEA711", "FE8B68", "FB3B02", "FE3521", "D00000",
      "A81F3D", "780422", "2B0000", "F1E2C5", "FEE4B3", "FEDABB", "FEC28E", "D88F57", "BD6340",
      "A2402B", "63352D", "6B2833", "34120C", "E7CB8F", "D1B38B", "C1B17F", "D7980B", "FFB100",
      "FE8B00", "FF6A00", "B74100", "5F3E1D", "3B2300", "FECADA", "FE65CB", "FE1392", "DD3062",
      "C04A6D", "891688", "4D2689", "441521", "2C1139", "241422", "FFFF7D", "FFFF00", "FF7F00",
      "FF7D7D", "FF7DFF", "FF00FE", "FF007F", "FF0000", "7F0000", "0A0A00", "F6FDFF", "E9F7FF",
      "CFE6FE", "AAC7FE", "77B3FE", "1E74FD", "0046AA", "2F4476", "003052", "0E0E25", "EEF5F0",
      "D6E5DE", "ACD8B9", "6CADC6", "42A9AF", "007F7F", "49675C", "2E4E4E", "1D3239", "0F1C21",
      "D8FFF4", "B8F4F5", "AECCB5", "76C578", "358757", "409B68", "468768", "1F512B", "2A3C37",
      "122E1D", "EFFFC9", "E6F385", "BCF51C", "D4DC18", "82D322", "5C7F00", "59932B", "297F00",
      "004320", "1C3322", "00FF7F", "00FF00", "7DFF7D", "7DFFFF", "00FFFF", "7D7DFF", "7F00FF",
      "0000FF", "3F007F", "00007F"};

  ToolSettings *ts = scene->toolsettings;
  if (ts->gp_paint->paint.palette != NULL) {
    return;
  }

  /* Try to find the default palette. */
  const char *palette_id = "Palette";
  struct Palette *palette = BLI_findstring(&bmain->palettes, palette_id, offsetof(ID, name) + 2);

  if (palette == NULL) {
    /* Fall back to the first palette. */
    palette = bmain->palettes.first;
  }

  if (palette == NULL) {
    /* Fall back to creating a palette. */
    palette = BKE_palette_add(bmain, palette_id);
    id_us_min(&palette->id);

    /* Create Colors. */
    for (int i = 0; i < ARRAY_SIZE(hexcol); i++) {
      PaletteColor *palcol = BKE_palette_color_add(palette);
      hex_to_rgb(hexcol[i], palcol->rgb, palcol->rgb + 1, palcol->rgb + 2);
    }
  }

  BLI_assert(palette != NULL);
  BKE_paint_palette_set(&ts->gp_paint->paint, palette);
  BKE_paint_palette_set(&ts->gp_vertexpaint->paint, palette);
}

/**
 * Create grease pencil strokes from image
 * \param sima: Image
 * \param gpd: Grease pencil data-block
 * \param gpf: Grease pencil frame
 * \param size: Size
 * \param mask: Mask
 * \return  True if done
 */
bool BKE_gpencil_from_image(
    SpaceImage *sima, bGPdata *gpd, bGPDframe *gpf, const float size, const bool mask)
{
  Image *image = sima->image;
  bool done = false;

  if (image == NULL) {
    return false;
  }

  ImageUser iuser = sima->iuser;
  void *lock;
  ImBuf *ibuf;

  ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

  if (ibuf && ibuf->rect) {
    int img_x = ibuf->x;
    int img_y = ibuf->y;

    float color[4];
    bGPDspoint *pt;
    for (int row = 0; row < img_y; row++) {
      /* Create new stroke */
      bGPDstroke *gps = BKE_gpencil_stroke_add(gpf, 0, img_x, size * 1000, false);
      done = true;
      for (int col = 0; col < img_x; col++) {
        IMB_sampleImageAtLocation(ibuf, col, row, true, color);
        pt = &gps->points[col];
        pt->pressure = 1.0f;
        pt->x = col * size;
        pt->z = row * size;
        if (!mask) {
          copy_v3_v3(pt->vert_color, color);
          pt->vert_color[3] = 1.0f;
          pt->strength = color[3];
        }
        else {
          zero_v3(pt->vert_color);
          pt->vert_color[3] = 1.0f;
          pt->strength = 1.0f - color[3];
        }

        /* Selet Alpha points. */
        if (pt->strength < 0.03f) {
          gps->flag |= GP_STROKE_SELECT;
          pt->flag |= GP_SPOINT_SELECT;
        }
      }

      if (gps->flag & GP_STROKE_SELECT) {
        BKE_gpencil_stroke_select_index_set(gpd, gps, false);
      }

      BKE_gpencil_stroke_geometry_update(gpd, gps);
    }
  }

  /* Free memory. */
  BKE_image_release_ibuf(image, ibuf, lock);

  return done;
}

/**
 * Helper to check if a layers is used as mask
 * \param view_layer: Actual view layer.
 * \param gpd: Grease pencil data-block.
 * \param gpl_mask: Actual Layer.
 * \return True if the layer is used as mask.
 */
static bool gpencil_is_layer_mask(ViewLayer *view_layer, bGPdata *gpd, bGPDlayer *gpl_mask)
{
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if ((gpl->viewlayername[0] != '\0') && (!STREQ(view_layer->name, gpl->viewlayername))) {
      continue;
    }

    LISTBASE_FOREACH (bGPDlayer_Mask *, mask, &gpl->mask_layers) {
      if (STREQ(gpl_mask->info, mask->name)) {
        return true;
      }
    }
  }

  return false;
}

/* -------------------------------------------------------------------- */
/** \name Iterators
 *
 * Iterate over all visible stroke of all visible layers inside a gpObject.
 * Also take into account onion-skinning.
 * \{ */

void BKE_gpencil_visible_stroke_iter(ViewLayer *view_layer,
                                     Object *ob,
                                     gpIterCb layer_cb,
                                     gpIterCb stroke_cb,
                                     void *thunk,
                                     bool do_onion,
                                     int cfra)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_multiedit = ((GPENCIL_MULTIEDIT_SESSIONS_ON(gpd)) && (!GPENCIL_PLAY_ON(gpd)));
  const bool is_onion = do_onion && ((gpd->flag & GP_DATA_STROKE_WEIGHTMODE) == 0);
  const bool is_drawing = (gpd->runtime.sbuffer_used > 0);

  /* Onion skinning. */
  const bool onion_mode_abs = (gpd->onion_mode == GP_ONION_MODE_ABSOLUTE);
  const bool onion_mode_sel = (gpd->onion_mode == GP_ONION_MODE_SELECTED);
  const bool onion_loop = (gpd->onion_flag & GP_ONION_LOOP) != 0;
  const short onion_keytype = gpd->onion_keytype;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* Reset by layer. */
    bool is_before_first = false;

    bGPDframe *act_gpf = gpl->actframe;
    bGPDframe *sta_gpf = act_gpf;
    bGPDframe *end_gpf = act_gpf ? act_gpf->next : NULL;

    if (gpl->flag & GP_LAYER_HIDE) {
      continue;
    }

    /* If scale to 0 the layer must be invisible. */
    if (is_zero_v3(gpl->scale)) {
      continue;
    }

    /* Hide the layer if it's defined a view layer filter. This is used to
     * generate renders, putting only selected GP layers for each View Layer.
     * This is used only in final render and never in Viewport. */
    if ((view_layer != NULL) && (gpl->viewlayername[0] != '\0') &&
        (!STREQ(view_layer->name, gpl->viewlayername))) {
      /* If the layer is used as mask, cannot be filtered or the masking system
       * will crash because needs the mask layer in the draw pipeline. */
      if (!gpencil_is_layer_mask(view_layer, gpd, gpl)) {
        continue;
      }
    }

    if (is_multiedit) {
      sta_gpf = end_gpf = NULL;
      /* Check the whole range and tag the editable frames. */
      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        if (act_gpf != NULL && (gpf == act_gpf || (gpf->flag & GP_FRAME_SELECT))) {
          gpf->runtime.onion_id = 0;
          if (do_onion) {
            if (gpf->framenum < act_gpf->framenum) {
              gpf->runtime.onion_id = -1;
            }
            else {
              gpf->runtime.onion_id = 1;
            }
          }

          if (sta_gpf == NULL) {
            sta_gpf = gpf;
          }
          end_gpf = gpf->next;
        }
        else {
          gpf->runtime.onion_id = INT_MAX;
        }
      }
    }
    else if (is_onion && (gpl->onion_flag & GP_LAYER_ONIONSKIN)) {
      /* Special cases when cframe is before first frame. */
      bGPDframe *gpf_first = gpl->frames.first;
      if ((gpf_first != NULL) && (act_gpf != NULL) && (gpf_first->framenum > act_gpf->framenum)) {
        is_before_first = true;
      }
      if ((gpf_first != NULL) && (act_gpf == NULL)) {
        act_gpf = gpf_first;
        is_before_first = true;
      }

      if (act_gpf) {
        bGPDframe *last_gpf = gpl->frames.last;

        int frame_len = 0;
        LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
          gpf->runtime.frameid = frame_len++;
        }

        LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
          bool is_wrong_keytype = (onion_keytype > -1) && (gpf->key_type != onion_keytype);
          bool is_in_range;
          int delta = (onion_mode_abs) ? (gpf->framenum - cfra) :
                                         (gpf->runtime.frameid - act_gpf->runtime.frameid);

          if (is_before_first) {
            delta++;
          }

          if (onion_mode_sel) {
            is_in_range = (gpf->flag & GP_FRAME_SELECT) != 0;
          }
          else {
            is_in_range = (-delta <= gpd->gstep) && (delta <= gpd->gstep_next);

            if (onion_loop && !is_in_range) {
              /* We wrap the value using the last frame and 0 as reference. */
              /* FIXME: This might not be good for animations not starting at 0. */
              int shift = (onion_mode_abs) ? last_gpf->framenum : last_gpf->runtime.frameid;
              delta += (delta < 0) ? (shift + 1) : -(shift + 1);
              /* Test again with wrapped value. */
              is_in_range = (-delta <= gpd->gstep) && (delta <= gpd->gstep_next);
            }
          }
          /* Mask frames that have wrong keytype of are not in range. */
          gpf->runtime.onion_id = (is_wrong_keytype || !is_in_range) ? INT_MAX : delta;
        }
        /* Active frame is always shown. */
        if (!is_before_first || is_drawing) {
          act_gpf->runtime.onion_id = 0;
        }
      }

      sta_gpf = gpl->frames.first;
      end_gpf = NULL;
    }
    else {
      /* Bypass multiedit/onion skinning. */
      end_gpf = sta_gpf = NULL;
    }

    if (sta_gpf == NULL && act_gpf == NULL) {
      if (layer_cb) {
        layer_cb(gpl, act_gpf, NULL, thunk);
      }
      continue;
    }

    /* Draw multiedit/onion skinning first */
    for (bGPDframe *gpf = sta_gpf; gpf && gpf != end_gpf; gpf = gpf->next) {
      if ((gpf->runtime.onion_id == INT_MAX || gpf == act_gpf) && (!is_before_first)) {
        continue;
      }

      /* Only do once for frame before first. */
      if (is_before_first && gpf == act_gpf) {
        is_before_first = false;
      }

      if (layer_cb) {
        layer_cb(gpl, gpf, NULL, thunk);
      }

      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (gps->totpoints == 0) {
          continue;
        }
        stroke_cb(gpl, gpf, gps, thunk);
      }
    }
    /* Draw Active frame on top. */
    /* Use evaluated frame (with modifiers for active stroke)/ */
    act_gpf = gpl->actframe;
    if (act_gpf) {
      act_gpf->runtime.onion_id = 0;
      if (layer_cb) {
        layer_cb(gpl, act_gpf, NULL, thunk);
      }

      /* If layer solo mode and Paint mode, only keyframes with data are displayed. */
      if (GPENCIL_PAINT_MODE(gpd) && (gpl->flag & GP_LAYER_SOLO_MODE) &&
          (act_gpf->framenum != cfra)) {
        continue;
      }

      LISTBASE_FOREACH (bGPDstroke *, gps, &act_gpf->strokes) {
        if (gps->totpoints == 0) {
          continue;
        }
        stroke_cb(gpl, act_gpf, gps, thunk);
      }
    }
  }
}

/**
 * Update original pointers in evaluated frame.
 * \param gpf_orig: Original grease-pencil frame.
 * \param gpf_eval: Evaluated grease pencil frame.
 */
void BKE_gpencil_frame_original_pointers_update(const struct bGPDframe *gpf_orig,
                                                const struct bGPDframe *gpf_eval)
{
  bGPDstroke *gps_eval = gpf_eval->strokes.first;
  LISTBASE_FOREACH (bGPDstroke *, gps_orig, &gpf_orig->strokes) {

    /* Assign original stroke pointer. */
    if (gps_eval != NULL) {
      gps_eval->runtime.gps_orig = gps_orig;

      /* Assign original point pointer. */
      for (int i = 0; i < gps_orig->totpoints; i++) {
        if (i > gps_eval->totpoints - 1) {
          break;
        }
        bGPDspoint *pt_orig = &gps_orig->points[i];
        bGPDspoint *pt_eval = &gps_eval->points[i];
        pt_orig->runtime.pt_orig = NULL;
        pt_orig->runtime.idx_orig = i;
        pt_eval->runtime.pt_orig = pt_orig;
        pt_eval->runtime.idx_orig = i;
      }
      /* Increase pointer. */
      gps_eval = gps_eval->next;
    }
  }
}

/**
 * Update pointers of eval data to original data to keep references.
 * \param ob_orig: Original grease pencil object
 * \param ob_eval: Evaluated grease pencil object
 */
void BKE_gpencil_update_orig_pointers(const Object *ob_orig, const Object *ob_eval)
{
  bGPdata *gpd_eval = (bGPdata *)ob_eval->data;
  bGPdata *gpd_orig = (bGPdata *)ob_orig->data;

  /* Assign pointers to the original stroke and points to the evaluated data. This must
   * be done before applying any modifier because at this moment the structure is equals,
   * so we can assume the layer index is the same in both data-blocks.
   * This data will be used by operators. */

  bGPDlayer *gpl_eval = gpd_eval->layers.first;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd_orig->layers) {
    if (gpl_eval != NULL) {
      /* Update layer reference pointers. */
      gpl_eval->runtime.gpl_orig = (bGPDlayer *)gpl;

      bGPDframe *gpf_eval = gpl_eval->frames.first;
      LISTBASE_FOREACH (bGPDframe *, gpf_orig, &gpl->frames) {
        if (gpf_eval != NULL) {
          /* Update frame reference pointers. */
          gpf_eval->runtime.gpf_orig = (bGPDframe *)gpf_orig;
          BKE_gpencil_frame_original_pointers_update(gpf_orig, gpf_eval);
          gpf_eval = gpf_eval->next;
        }
      }
      gpl_eval = gpl_eval->next;
    }
  }
}

/**
 * Get parent matrix, including layer parenting.
 * \param depsgraph: Depsgraph
 * \param obact: Grease pencil object
 * \param gpl: Grease pencil layer
 * \param diff_mat: Result parent matrix
 */
void BKE_gpencil_layer_transform_matrix_get(const Depsgraph *depsgraph,
                                            Object *obact,
                                            bGPDlayer *gpl,
                                            float diff_mat[4][4])
{
  Object *ob_eval = depsgraph != NULL ? DEG_get_evaluated_object(depsgraph, obact) : obact;
  Object *obparent = gpl->parent;
  Object *obparent_eval = depsgraph != NULL ? DEG_get_evaluated_object(depsgraph, obparent) :
                                              obparent;

  /* if not layer parented, try with object parented */
  if (obparent_eval == NULL) {
    if ((ob_eval != NULL) && (ob_eval->type == OB_GPENCIL)) {
      copy_m4_m4(diff_mat, ob_eval->obmat);
      mul_m4_m4m4(diff_mat, diff_mat, gpl->layer_mat);
      return;
    }
    /* not gpencil object */
    unit_m4(diff_mat);
    return;
  }

  if (ELEM(gpl->partype, PAROBJECT, PARSKEL)) {
    mul_m4_m4m4(diff_mat, obparent_eval->obmat, gpl->inverse);
    add_v3_v3(diff_mat[3], ob_eval->obmat[3]);
    mul_m4_m4m4(diff_mat, diff_mat, gpl->layer_mat);
    return;
  }
  if (gpl->partype == PARBONE) {
    bPoseChannel *pchan = BKE_pose_channel_find_name(obparent_eval->pose, gpl->parsubstr);
    if (pchan) {
      float tmp_mat[4][4];
      mul_m4_m4m4(tmp_mat, obparent_eval->obmat, pchan->pose_mat);
      mul_m4_m4m4(diff_mat, tmp_mat, gpl->inverse);
      add_v3_v3(diff_mat[3], ob_eval->obmat[3]);
    }
    else {
      /* if bone not found use object (armature) */
      mul_m4_m4m4(diff_mat, obparent_eval->obmat, gpl->inverse);
      add_v3_v3(diff_mat[3], ob_eval->obmat[3]);
    }
    mul_m4_m4m4(diff_mat, diff_mat, gpl->layer_mat);
    return;
  }

  unit_m4(diff_mat); /* not defined type */
}

/**
 * Update parent matrix and local transforms.
 * \param depsgraph: Depsgraph
 * \param ob: Grease pencil object
 */
void BKE_gpencil_update_layer_transforms(const Depsgraph *depsgraph, Object *ob)
{
  if (ob->type != OB_GPENCIL) {
    return;
  }

  bGPdata *gpd = (bGPdata *)ob->data;
  float cur_mat[4][4];

  bool changed = false;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    unit_m4(cur_mat);
    if (gpl->actframe != NULL) {
      if (gpl->parent != NULL) {
        Object *ob_parent = DEG_get_evaluated_object(depsgraph, gpl->parent);
        /* calculate new matrix */
        if (ELEM(gpl->partype, PAROBJECT, PARSKEL)) {
          copy_m4_m4(cur_mat, ob_parent->obmat);
        }
        else if (gpl->partype == PARBONE) {
          bPoseChannel *pchan = BKE_pose_channel_find_name(ob_parent->pose, gpl->parsubstr);
          if (pchan != NULL) {
            copy_m4_m4(cur_mat, ob->imat);
            mul_m4_m4m4(cur_mat, ob_parent->obmat, pchan->pose_mat);
          }
          else {
            unit_m4(cur_mat);
          }
        }
        changed = !equals_m4m4(gpl->inverse, cur_mat);
      }

      /* Calc local layer transform. */
      bool transformed = ((!is_zero_v3(gpl->location)) || (!is_zero_v3(gpl->rotation)) ||
                          (!is_one_v3(gpl->scale)));
      if (transformed) {
        loc_eul_size_to_mat4(gpl->layer_mat, gpl->location, gpl->rotation, gpl->scale);
      }

      /* only redo if any change */
      if (changed || transformed) {
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpl->actframe->strokes) {
          bGPDspoint *pt;
          int i;
          for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
            if (changed) {
              mul_m4_v3(gpl->inverse, &pt->x);
              mul_m4_v3(cur_mat, &pt->x);
            }

            if (transformed) {
              mul_m4_v3(gpl->layer_mat, &pt->x);
            }
          }
        }
      }
    }
  }
}

/**
 * Find material by name prefix.
 * \param ob: Object pointer
 * \param name_prefix: Prefix name of the material
 * \return  Index
 */
int BKE_gpencil_material_find_index_by_name_prefix(Object *ob, const char *name_prefix)
{
  const int name_prefix_len = strlen(name_prefix);
  for (int i = 0; i < ob->totcol; i++) {
    Material *ma = BKE_object_material_get(ob, i + 1);
    if ((ma != NULL) && (ma->gp_style != NULL) &&
        (STREQLEN(ma->id.name + 2, name_prefix, name_prefix_len))) {
      return i;
    }
  }

  return -1;
}

/* Create a hash with the list of selected frame number. */
void BKE_gpencil_frame_selected_hash(bGPdata *gpd, struct GHash *r_list)
{
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  LISTBASE_FOREACH (bGPDlayer *, gpl_iter, &gpd->layers) {
    if ((gpl != NULL) && (!is_multiedit) && (gpl != gpl_iter)) {
      continue;
    }

    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl_iter->frames) {
      if (((gpf == gpl->actframe) && (!is_multiedit)) ||
          ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (!BLI_ghash_lookup(r_list, POINTER_FROM_INT(gpf->framenum))) {
          BLI_ghash_insert(r_list, POINTER_FROM_INT(gpf->framenum), gpf);
        }
      }
    }
  }
}

/** \} */
