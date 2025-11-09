/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"

#include "BLT_translation.hh"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW
#include "DNA_scene_types.h"

#include "DNA_brush_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_userdef_types.h"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_brush.hh"
#include "BKE_collection.hh"
#include "BKE_colortools.hh"
#include "BKE_deform.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_icons.hh"
#include "BKE_idtype.hh"
#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_paint.hh"

#include "DEG_depsgraph.hh"

#include "BLI_math_color.h"
#include "BLI_string_utf8.h"

#include "BLO_read_write.hh"

#include "IMB_colormanagement.hh"

static CLG_LogRef LOG = {"geom.gpencil"};

static void greasepencil_copy_data(Main * /*bmain*/,
                                   std::optional<Library *> /*owner_library*/,
                                   ID *id_dst,
                                   const ID *id_src,
                                   const int /*flag*/)
{
  bGPdata *gpd_dst = (bGPdata *)id_dst;
  const bGPdata *gpd_src = (const bGPdata *)id_src;

  /* duplicate material array */
  if (gpd_src->mat) {
    gpd_dst->mat = static_cast<Material **>(MEM_dupallocN(gpd_src->mat));
  }

  BKE_defgroup_copy_list(&gpd_dst->vertex_group_names, &gpd_src->vertex_group_names);

  /* copy layers */
  BLI_listbase_clear(&gpd_dst->layers);
  LISTBASE_FOREACH (bGPDlayer *, gpl_src, &gpd_src->layers) {
    /* make a copy of source layer and its data */

    /* TODO: here too could add unused flags... */
    bGPDlayer *gpl_dst = BKE_gpencil_layer_duplicate(gpl_src, true, true);

    /* Apply local layer transform to all frames. Calc the active frame is not enough
     * because onion skin can use more frames. This is more slow but required here. */
    if (gpl_dst->actframe != nullptr) {
      bool transformed = (!is_zero_v3(gpl_dst->location) || !is_zero_v3(gpl_dst->rotation) ||
                          !is_one_v3(gpl_dst->scale));
      if (transformed) {
        loc_eul_size_to_mat4(
            gpl_dst->layer_mat, gpl_dst->location, gpl_dst->rotation, gpl_dst->scale);
        bool do_onion = ((gpl_dst->onion_flag & GP_LAYER_ONIONSKIN) != 0);
        bGPDframe *init_gpf = static_cast<bGPDframe *>((do_onion) ? gpl_dst->frames.first :
                                                                    gpl_dst->actframe);
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
  BKE_gpencil_free_data((bGPdata *)id, true);
}

static void greasepencil_foreach_id(ID *id, LibraryForeachIDData *data)
{
  bGPdata *gpencil = (bGPdata *)id;
  /* materials */
  for (int i = 0; i < gpencil->totcol; i++) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, gpencil->mat[i], IDWALK_CB_USER);
  }

  LISTBASE_FOREACH (bGPDlayer *, gplayer, &gpencil->layers) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, gplayer->parent, IDWALK_CB_NOP);
  }
}

static void greasepencil_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  bGPdata *gpd = (bGPdata *)id;

  /* Clean up, important in undo case to reduce false detection of changed data-blocks. */
  /* XXX not sure why the whole run-time data is not cleared in reading code,
   * for now mimicking it here. */
  gpd->runtime.sbuffer = nullptr;
  gpd->runtime.sbuffer_used = 0;
  gpd->runtime.sbuffer_size = 0;

  /* write gpd data block to file */
  BLO_write_id_struct(writer, bGPdata, id_address, &gpd->id);
  BKE_id_blend_write(writer, &gpd->id);

  BKE_defbase_blend_write(writer, &gpd->vertex_group_names);

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
        if (gps->editcurve != nullptr) {
          bGPDcurve *gpc = gps->editcurve;
          BLO_write_struct(writer, bGPDcurve, gpc);
          BLO_write_struct_array(
              writer, bGPDcurve_point, gpc->tot_curve_points, gpc->curve_points);
        }
      }
    }
  }
}

void BKE_gpencil_blend_read_data(BlendDataReader *reader, bGPdata *gpd)
{
  /* We must firstly have some grease-pencil data to link! */
  if (gpd == nullptr) {
    return;
  }

  /* Ensure full object-mode for linked grease pencil. */
  if (ID_IS_LINKED(gpd)) {
    gpd->flag &= ~GP_DATA_STROKE_PAINTMODE;
    gpd->flag &= ~GP_DATA_STROKE_EDITMODE;
    gpd->flag &= ~GP_DATA_STROKE_SCULPTMODE;
    gpd->flag &= ~GP_DATA_STROKE_WEIGHTMODE;
    gpd->flag &= ~GP_DATA_STROKE_VERTEXMODE;
  }

  /* init stroke buffer */
  gpd->runtime.sbuffer = nullptr;
  gpd->runtime.sbuffer_used = 0;
  gpd->runtime.sbuffer_size = 0;

  /* Relink palettes (old palettes deprecated, only to convert old files). */
  BLO_read_struct_list(reader, bGPDpalette, &gpd->palettes);
  if (gpd->palettes.first != nullptr) {
    LISTBASE_FOREACH (bGPDpalette *, palette, &gpd->palettes) {
      BLO_read_struct_list(reader, PaletteColor, &palette->colors);
    }
  }

  BLO_read_struct_list(reader, bDeformGroup, &gpd->vertex_group_names);

  /* Materials. */
  BLO_read_pointer_array(reader, gpd->totcol, (void **)&gpd->mat);

  /* Relink layers. */
  BLO_read_struct_list(reader, bGPDlayer, &gpd->layers);

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* Relink frames. */
    BLO_read_struct_list(reader, bGPDframe, &gpl->frames);

    BLO_read_struct(reader, bGPDframe, &gpl->actframe);

    gpl->runtime.icon_id = 0;

    /* Relink masks. */
    BLO_read_struct_list(reader, bGPDlayer_Mask, &gpl->mask_layers);

    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      /* Relink strokes (and their points). */
      BLO_read_struct_list(reader, bGPDstroke, &gpf->strokes);

      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        /* Relink stroke points array. */
        BLO_read_struct_array(reader, bGPDspoint, gps->totpoints, &gps->points);
        /* Relink geometry. */
        BLO_read_struct_array(reader, bGPDtriangle, gps->tot_triangles, &gps->triangles);

        /* Relink stroke edit curve. */
        BLO_read_struct(reader, bGPDcurve, &gps->editcurve);
        if (gps->editcurve != nullptr) {
          /* Relink curve point array. */
          bGPDcurve *gpc = gps->editcurve;
          BLO_read_struct_array(
              reader, bGPDcurve_point, gpc->tot_curve_points, &gps->editcurve->curve_points);
        }

        /* Relink weight data. */
        if (gps->dvert) {
          BLO_read_struct_array(reader, MDeformVert, gps->totpoints, &gps->dvert);
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

IDTypeInfo IDType_ID_GD_LEGACY = {
    /*id_code*/ bGPdata::id_type,
    /*id_filter*/ FILTER_ID_GD_LEGACY,
    /*dependencies_id_types*/ FILTER_ID_MA,
    /*main_listbase_index*/ INDEX_ID_GD_LEGACY,
    /*struct_size*/ sizeof(bGPdata),
    /*name*/ "Annotation",
    /*name_plural*/ N_("annotations"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_GPENCIL,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ nullptr,
    /*copy_data*/ greasepencil_copy_data,
    /*free_data*/ greasepencil_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ greasepencil_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ greasepencil_blend_write,
    /*blend_read_data*/ greasepencil_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

/* ************************************************** */
/* Memory Management */

void BKE_gpencil_free_point_weights(MDeformVert *dvert)
{
  if (dvert == nullptr) {
    return;
  }
  MEM_SAFE_FREE(dvert->dw);
}

void BKE_gpencil_free_stroke_weights(bGPDstroke *gps)
{
  if (gps == nullptr) {
    return;
  }

  if (gps->dvert == nullptr) {
    return;
  }

  for (int i = 0; i < gps->totpoints; i++) {
    MDeformVert *dvert = &gps->dvert[i];
    BKE_gpencil_free_point_weights(dvert);
  }
}

void BKE_gpencil_free_stroke_editcurve(bGPDstroke *gps)
{
  if (gps == nullptr) {
    return;
  }
  bGPDcurve *editcurve = gps->editcurve;
  if (editcurve == nullptr) {
    return;
  }
  MEM_freeN(editcurve->curve_points);
  MEM_freeN(editcurve);
  gps->editcurve = nullptr;
}

void BKE_gpencil_free_stroke(bGPDstroke *gps)
{
  if (gps == nullptr) {
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
  if (gps->editcurve != nullptr) {
    BKE_gpencil_free_stroke_editcurve(gps);
  }

  MEM_freeN(gps);
}

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

void BKE_gpencil_free_frames(bGPDlayer *gpl)
{
  bGPDframe *gpf_next;

  /* error checking */
  if (gpl == nullptr) {
    return;
  }

  /* free frames */
  for (bGPDframe *gpf = static_cast<bGPDframe *>(gpl->frames.first); gpf; gpf = gpf_next) {
    gpf_next = gpf->next;

    /* free strokes and their associated memory */
    BKE_gpencil_free_strokes(gpf);
    BLI_freelinkN(&gpl->frames, gpf);
  }
  gpl->actframe = nullptr;
}

void BKE_gpencil_free_layer_masks(bGPDlayer *gpl)
{
  /* Free masks. */
  bGPDlayer_Mask *mask_next = nullptr;
  for (bGPDlayer_Mask *mask = static_cast<bGPDlayer_Mask *>(gpl->mask_layers.first); mask;
       mask = mask_next)
  {
    mask_next = mask->next;
    BLI_freelinkN(&gpl->mask_layers, mask);
  }
}
void BKE_gpencil_free_layers(ListBase *list)
{
  bGPDlayer *gpl_next;

  /* error checking */
  if (list == nullptr) {
    return;
  }

  /* delete layers */
  for (bGPDlayer *gpl = static_cast<bGPDlayer *>(list->first); gpl; gpl = gpl_next) {
    gpl_next = gpl->next;

    /* free layers and their data */
    BKE_gpencil_free_frames(gpl);

    /* Free masks. */
    BKE_gpencil_free_layer_masks(gpl);

    BLI_freelinkN(list, gpl);
  }
}

void BKE_gpencil_free_legacy_palette_data(ListBase *list)
{
  LISTBASE_FOREACH_MUTABLE (bGPDpalette *, palette, list) {
    BLI_freelistN(&palette->colors);
    MEM_freeN(palette);
  }
  BLI_listbase_clear(list);
}

void BKE_gpencil_free_data(bGPdata *gpd, bool /*free_all*/)
{
  /* free layers */
  BKE_gpencil_free_layers(&gpd->layers);
  BKE_gpencil_free_legacy_palette_data(&gpd->palettes);

  /* materials */
  MEM_SAFE_FREE(gpd->mat);

  BLI_freelistN(&gpd->vertex_group_names);
}

void BKE_gpencil_tag(bGPdata *gpd)
{
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
}

/* ************************************************** */
/* Container Creation */

bGPDframe *BKE_gpencil_frame_addnew(bGPDlayer *gpl, int cframe)
{
  bGPDframe *gpf = nullptr, *gf = nullptr;
  short state = 0;

  /* error checking */
  if (gpl == nullptr) {
    return nullptr;
  }

  /* allocate memory for this frame */
  gpf = MEM_callocN<bGPDframe>("bGPDframe");
  gpf->framenum = cframe;

  /* find appropriate place to add frame */
  if (gpl->frames.first) {
    for (gf = static_cast<bGPDframe *>(gpl->frames.first); gf; gf = gf->next) {
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
    BLI_assert(gf != nullptr);
    gpf = gf;
  }
  else if (state == 0) {
    /* add to end then! */
    BLI_addtail(&gpl->frames, gpf);
  }

  /* return frame */
  return gpf;
}

bGPDframe *BKE_gpencil_frame_addcopy(bGPDlayer *gpl, int cframe)
{
  bGPDframe *new_frame;
  bool found = false;

  /* Error checking/handling */
  if (gpl == nullptr) {
    /* no layer */
    return nullptr;
  }
  if (gpl->actframe == nullptr) {
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
      /* This only happens when we're editing with frame-lock on.
       * - Delete the new frame and don't do anything else here.
       */
      BKE_gpencil_free_strokes(new_frame);
      MEM_freeN(new_frame);
      new_frame = nullptr;

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

bGPDlayer *BKE_gpencil_layer_addnew(bGPdata *gpd,
                                    const char *name,
                                    const bool setactive,
                                    const bool add_to_header)
{
  bGPDlayer *gpl = nullptr;
  bGPDlayer *gpl_active = nullptr;

  /* check that list is ok */
  if (gpd == nullptr) {
    return nullptr;
  }

  /* allocate memory for frame and add to end of list */
  gpl = MEM_callocN<bGPDlayer>("bGPDlayer");

  gpl_active = BKE_gpencil_layer_active_get(gpd);

  /* Add to data-block. */
  if (add_to_header) {
    BLI_addhead(&gpd->layers, gpl);
  }
  else {
    if (gpl_active == nullptr) {
      BLI_addtail(&gpd->layers, gpl);
    }
    else {
      /* if active layer, add after that layer */
      BLI_insertlinkafter(&gpd->layers, gpl_active, gpl);
    }
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
  STRNCPY_UTF8(gpl->info, DATA_(name));
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

bGPdata *BKE_gpencil_data_addnew(Main *bmain, const char name[])
{
  bGPdata *gpd;

  /* allocate memory for a new block */
  gpd = static_cast<bGPdata *>(BKE_libblock_alloc(bmain, ID_GD_LEGACY, name, 0));

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

/* ************************************************** */
/* Data Duplication */

void BKE_gpencil_stroke_weights_duplicate(bGPDstroke *gps_src, bGPDstroke *gps_dst)
{
  if (gps_src == nullptr) {
    return;
  }
  BLI_assert(gps_src->totpoints == gps_dst->totpoints);

  BKE_defvert_array_copy(gps_dst->dvert, gps_src->dvert, gps_src->totpoints);
}

bGPDstroke *BKE_gpencil_stroke_duplicate(bGPDstroke *gps_src,
                                         const bool dup_points,
                                         const bool /*dup_curve*/)
{
  bGPDstroke *gps_dst = nullptr;

  gps_dst = static_cast<bGPDstroke *>(MEM_dupallocN(gps_src));
  gps_dst->prev = gps_dst->next = nullptr;
  gps_dst->triangles = static_cast<bGPDtriangle *>(MEM_dupallocN(gps_src->triangles));

  if (dup_points) {
    gps_dst->points = static_cast<bGPDspoint *>(MEM_dupallocN(gps_src->points));

    if (gps_src->dvert != nullptr) {
      gps_dst->dvert = static_cast<MDeformVert *>(MEM_dupallocN(gps_src->dvert));
      BKE_gpencil_stroke_weights_duplicate(gps_src, gps_dst);
    }
    else {
      gps_dst->dvert = nullptr;
    }
  }
  else {
    gps_dst->points = nullptr;
    gps_dst->dvert = nullptr;
  }

  gps_dst->editcurve = nullptr;

  /* return new stroke */
  return gps_dst;
}

bGPDframe *BKE_gpencil_frame_duplicate(const bGPDframe *gpf_src, const bool dup_strokes)
{
  bGPDstroke *gps_dst = nullptr;
  bGPDframe *gpf_dst;

  /* error checking */
  if (gpf_src == nullptr) {
    return nullptr;
  }

  /* make a copy of the source frame */
  gpf_dst = static_cast<bGPDframe *>(MEM_dupallocN(gpf_src));
  gpf_dst->prev = gpf_dst->next = nullptr;

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

bGPDlayer *BKE_gpencil_layer_duplicate(const bGPDlayer *gpl_src,
                                       const bool dup_frames,
                                       const bool dup_strokes)
{
  bGPDframe *gpf_dst;
  bGPDlayer *gpl_dst;

  /* error checking */
  if (gpl_src == nullptr) {
    return nullptr;
  }

  /* make a copy of source layer */
  gpl_dst = static_cast<bGPDlayer *>(MEM_dupallocN(gpl_src));
  gpl_dst->prev = gpl_dst->next = nullptr;

  /* copy frames */
  BLI_listbase_clear(&gpl_dst->frames);
  if (dup_frames) {
    LISTBASE_FOREACH (bGPDframe *, gpf_src, &gpl_src->frames) {
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

bGPdata *BKE_gpencil_data_duplicate(Main *bmain, const bGPdata *gpd_src, bool internal_copy)
{
  bGPdata *gpd_dst;

  /* Yuck and super-uber-hyper yuck!!!
   * Should be replaceable with a no-main copy (LIB_ID_COPY_NO_MAIN etc.), but not sure about it,
   * so for now keep old code for that one. */

  /* error checking */
  if (gpd_src == nullptr) {
    return nullptr;
  }

  if (internal_copy) {
    /* make a straight copy for undo buffers used during stroke drawing */
    gpd_dst = static_cast<bGPdata *>(MEM_dupallocN(gpd_src));
  }
  else {
    BLI_assert(bmain != nullptr);
    gpd_dst = (bGPdata *)BKE_id_copy(bmain, &gpd_src->id);
  }

  /* Copy internal data (layers, etc.) */
  greasepencil_copy_data(bmain, std::nullopt, &gpd_dst->id, &gpd_src->id, 0);

  /* return new */
  return gpd_dst;
}

/* ************************************************** */
/* GP Layer API */

bool BKE_gpencil_layer_is_editable(const bGPDlayer *gpl)
{
  /* Sanity check */
  if (gpl == nullptr) {
    return false;
  }

  /* Layer must be: Visible + Editable */
  if ((gpl->flag & (GP_LAYER_HIDE | GP_LAYER_LOCKED)) == 0) {
    return true;
  }

  /* Something failed */
  return false;
}

bGPDframe *BKE_gpencil_layer_frame_find(bGPDlayer *gpl, int cframe)
{
  /* Search in reverse order, since this is often used for playback/adding,
   * where it's less likely that we're interested in the earlier frames
   */
  LISTBASE_FOREACH_BACKWARD (bGPDframe *, gpf, &gpl->frames) {
    if (gpf->framenum == cframe) {
      return gpf;
    }
  }

  return nullptr;
}

bGPDframe *BKE_gpencil_layer_frame_get(bGPDlayer *gpl, int cframe, eGP_GetFrame_Mode addnew)
{
  bGPDframe *gpf = nullptr;
  bool found = false;

  /* error checking */
  if (gpl == nullptr) {
    return nullptr;
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
        /* If this is the last frame or the next frame is at a later time, we found the right
         * frame. */
        if (!(gpf->next) || (gpf->next->framenum > cframe)) {
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
          /* The #BKE_gpencil_frame_addcopy function copies the active frame of gpl,
           * so we need to set the active frame before copying. */
          gpl->actframe = gpf;
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
        gpl->actframe = static_cast<bGPDframe *>(gpl->frames.last);
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
          /* The #BKE_gpencil_frame_addcopy function copies the active frame of gpl;
           * so we need to set the active frame before copying. */
          gpl->actframe = gpf;
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
        gpl->actframe = static_cast<bGPDframe *>(gpl->frames.first);
      }
    }
  }
  else if (gpl->frames.first) {
    /* check which of the ends to start checking from */
    const int first = ((bGPDframe *)(gpl->frames.first))->framenum;
    const int last = ((bGPDframe *)(gpl->frames.last))->framenum;

    if (abs(cframe - first) > abs(cframe - last)) {
      /* find gp-frame which is less than or equal to cframe */
      for (gpf = static_cast<bGPDframe *>(gpl->frames.last); gpf; gpf = gpf->prev) {
        if (gpf->framenum <= cframe) {
          found = true;
          break;
        }
      }
    }
    else {
      /* find gp-frame which is less than or equal to cframe */
      for (gpf = static_cast<bGPDframe *>(gpl->frames.first); gpf; gpf = gpf->next) {
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
      if (gpl->frames.first != nullptr) {
        gpl->actframe = static_cast<bGPDframe *>(gpl->frames.first);
      }
      else {
        /* Unresolved erogenous situation! */
        CLOG_STR_ERROR(&LOG, "cannot find appropriate gp-frame");
        /* `gpl->actframe` should still be nullptr. */
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
      /* gpl->actframe should still be nullptr */
    }
  }

  /* Don't select first frame if greater than current frame. */
  if ((gpl->actframe != nullptr) && (gpl->actframe == gpl->frames.first) &&
      (gpl->actframe->framenum > cframe))
  {
    gpl->actframe = nullptr;
  }

  /* return */
  return gpl->actframe;
}

bool BKE_gpencil_layer_frame_delete(bGPDlayer *gpl, bGPDframe *gpf)
{
  bool changed = false;

  /* error checking */
  if (ELEM(nullptr, gpl, gpf)) {
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

bGPDlayer *BKE_gpencil_layer_named_get(bGPdata *gpd, const char *name)
{
  if (name[0] == '\0') {
    return nullptr;
  }
  return static_cast<bGPDlayer *>(BLI_findstring(&gpd->layers, name, offsetof(bGPDlayer, info)));
}

static int gpencil_cb_cmp_frame(void *thunk, const void *a, const void *b)
{
  const bGPDframe *frame_a = static_cast<const bGPDframe *>(a);
  const bGPDframe *frame_b = static_cast<const bGPDframe *>(b);

  if (frame_a->framenum < frame_b->framenum) {
    return -1;
  }
  if (frame_a->framenum > frame_b->framenum) {
    return 1;
  }
  if (thunk != nullptr) {
    *((bool *)thunk) = true;
  }
  /* Sort selected last. */
  if ((frame_a->flag & GP_FRAME_SELECT) && ((frame_b->flag & GP_FRAME_SELECT) == 0)) {
    return 1;
  }
  return 0;
}

void BKE_gpencil_layer_frames_sort(bGPDlayer *gpl, bool *r_has_duplicate_frames)
{
  BLI_listbase_sort_r(&gpl->frames, gpencil_cb_cmp_frame, r_has_duplicate_frames);
}

bGPDlayer *BKE_gpencil_layer_active_get(bGPdata *gpd)
{
  /* error checking */
  if (ELEM(nullptr, gpd, gpd->layers.first)) {
    return nullptr;
  }

  /* loop over layers until found (assume only one active) */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (gpl->flag & GP_LAYER_ACTIVE) {
      return gpl;
    }
  }

  /* no active layer found */
  return nullptr;
}

void BKE_gpencil_layer_active_set(bGPdata *gpd, bGPDlayer *active)
{
  /* error checking */
  if (ELEM(nullptr, gpd, gpd->layers.first, active)) {
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

void BKE_gpencil_layer_delete(bGPdata *gpd, bGPDlayer *gpl)
{
  /* error checking */
  if (ELEM(nullptr, gpd, gpl)) {
    return;
  }

  /* free layer */
  BKE_gpencil_free_frames(gpl);

  /* Free Masks. */
  BKE_gpencil_free_layer_masks(gpl);

  /* free icon providing preview of icon color */
  BKE_icon_delete(gpl->runtime.icon_id);

  BLI_freelinkN(&gpd->layers, gpl);
}

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
    BKE_brush_tag_unsaved_changes(brush);
  }
}

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
  if (ts->gp_paint->paint.palette != nullptr) {
    return;
  }

  /* Try to find the default palette. */
  const char *palette_id = "Palette";
  Palette *palette = static_cast<Palette *>(
      BLI_findstring(&bmain->palettes, palette_id, offsetof(ID, name) + 2));

  if (palette == nullptr) {
    /* Fall back to the first palette. */
    palette = static_cast<Palette *>(bmain->palettes.first);
  }

  if (palette == nullptr) {
    /* Fall back to creating a palette. */
    palette = BKE_palette_add(bmain, palette_id);
    id_us_min(&palette->id);

    /* Create Colors. */
    for (int i = 0; i < ARRAY_SIZE(hexcol); i++) {
      PaletteColor *palcol = BKE_palette_color_add(palette);
      hex_to_rgb(hexcol[i], palcol->color, palcol->color + 1, palcol->color + 2);
      IMB_colormanagement_srgb_to_scene_linear_v3(palcol->color, palcol->color);
    }
  }

  BLI_assert(palette != nullptr);
  BKE_paint_palette_set(&ts->gp_paint->paint, palette);
  BKE_paint_palette_set(&ts->gp_vertexpaint->paint, palette);
}

/** \} */
