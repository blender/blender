/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_colortools.hh"
#include "BKE_deform.hh"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lattice.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_material.h"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "BLO_read_write.hh"

typedef enum {
  /** Should not be used, only for None modifier type. */
  eGpencilModifierTypeType_None,

  /** Grease pencil modifiers. */
  eGpencilModifierTypeType_Gpencil,
} GpencilModifierTypeType;

typedef enum {
  /* eGpencilModifierTypeFlag_SupportsMapping = (1 << 0), */ /* UNUSED */
  eGpencilModifierTypeFlag_SupportsEditmode = (1 << 1),

  /**
   * For modifiers that support edit-mode this determines if the
   * modifier should be enabled by default in edit-mode. This should
   * only be used by modifiers that are relatively speedy and
   * also generally used in edit-mode, otherwise let the user enable it by hand.
   */
  eGpencilModifierTypeFlag_EnableInEditmode = (1 << 2),

  /**
   * For modifiers that require original data and so cannot
   * be placed after any non-deform modifier.
   */
  /* eGpencilModifierTypeFlag_RequiresOriginalData = (1 << 3), */ /* UNUSED */

  /** Max one per type. */
  eGpencilModifierTypeFlag_Single = (1 << 4),

  /** Can't be added manually by user. */
  eGpencilModifierTypeFlag_NoUserAdd = (1 << 5),
  /** Can't be applied. */
  eGpencilModifierTypeFlag_NoApply = (1 << 6),
} GpencilModifierTypeFlag;

typedef struct GpencilModifierTypeInfo {
  /** The user visible name for this modifier */
  char name[32];

  /**
   * The DNA struct name for the modifier data type, used to
   * write the DNA data out.
   */
  char struct_name[32];

  /** The size of the modifier data type, used by allocation. */
  int struct_size;

  GpencilModifierTypeType type;
  GpencilModifierTypeFlag flags;

  /********************* Non-optional functions *********************/

  /**
   * Copy instance data for this modifier type. Should copy all user
   * level settings to the target modifier.
   */
  void (*copy_data)(const struct GpencilModifierData *md, struct GpencilModifierData *target);

  /**
   * Callback for GP "stroke" modifiers that operate on the
   * shape and parameters of the provided strokes (e.g. Thickness, Noise, etc.)
   *
   * The gpl parameter contains the GP layer that the strokes come from.
   * While access is provided to this data, you should not directly access
   * the gpl->frames data from the modifier. Instead, use the gpf parameter
   * instead.
   *
   * The gps parameter contains the GP stroke to operate on. This is usually a copy
   * of the original (unmodified and saved to files) stroke data.
   */
  void (*deform_stroke)(struct GpencilModifierData *md,
                        struct Depsgraph *depsgraph,
                        struct Object *ob,
                        struct bGPDlayer *gpl,
                        struct bGPDframe *gpf,
                        struct bGPDstroke *gps);

  /**
   * Callback for GP "geometry" modifiers that create extra geometry
   * in the frame (e.g. Array)
   */
  void (*generate_strokes)(struct GpencilModifierData *md,
                           struct Depsgraph *depsgraph,
                           struct Object *ob);

  /**
   * Bake-down GP modifier's effects into the GP data-block.
   *
   * This gets called when the user clicks the "Apply" button in the UI.
   * As such, this callback needs to go through all layers/frames in the
   * data-block, mutating the geometry and/or creating new data-blocks/objects
   */
  void (*bake_modifier)(struct Main *bmain,
                        struct Depsgraph *depsgraph,
                        struct GpencilModifierData *md,
                        struct Object *ob);

  /********************* Optional functions *********************/

  /**
   * Callback for GP "time" modifiers that offset keyframe time
   * Returns the frame number to be used after apply the modifier. This is
   * usually an offset of the animation for duplicated data-blocks.
   *
   * This function is optional.
   */
  int (*remap_time)(struct GpencilModifierData *md,
                    struct Depsgraph *depsgraph,
                    struct Scene *scene,
                    struct Object *ob,
                    struct bGPDlayer *gpl,
                    int cfra);

  /**
   * Initialize new instance data for this modifier type, this function
   * should set modifier variables to their default values.
   *
   * This function is optional.
   */
  void (*init_data)(struct GpencilModifierData *md);

  /**
   * Free internal modifier data variables, this function should
   * not free the md variable itself.
   *
   * This function is optional.
   */
  void (*free_data)(struct GpencilModifierData *md);

  /**
   * Return a boolean value indicating if this modifier is able to be
   * calculated based on the modifier data. This is *not* regarding the
   * md->flag, that is tested by the system, this is just if the data
   * validates (for example, a lattice will return false if the lattice
   * object is not defined).
   *
   * This function is optional (assumes never disabled if not present).
   */
  bool (*is_disabled)(struct GpencilModifierData *md, bool use_render_params);

  /**
   * Add the appropriate relations to the dependency graph.
   *
   * This function is optional.
   */
  void (*update_depsgraph)(struct GpencilModifierData *md,
                           const struct ModifierUpdateDepsgraphContext *ctx,
                           int mode);

  /**
   * Should return true if the modifier needs to be recalculated on time
   * changes.
   *
   * This function is optional (assumes false if not present).
   */
  bool (*depends_on_time)(struct GpencilModifierData *md);

  /**
   * Should call the given walk function with a pointer to each ID
   * pointer (i.e. each data-block pointer) that the modifier data
   * stores. This is used for linking on file load and for
   * unlinking data-blocks or forwarding data-block references.
   *
   * This function is optional.
   */
  void (*foreach_ID_link)(struct GpencilModifierData *md,
                          struct Object *ob,
                          GreasePencilIDWalkFunc walk,
                          void *user_data);

  /**
   * Should call the given walk function for each texture that the
   * modifier data stores. This is used for finding all textures in
   * the context for the UI.
   *
   * This function is optional. If it is not present, it will be
   * assumed the modifier has no textures.
   */
  void (*foreach_tex_link)(struct GpencilModifierData *md,
                           struct Object *ob,
                           GreasePencilTexWalkFunc walk,
                           void *user_data);

  /* Register the panel types for the modifier's UI. */
  void (*panel_register)(struct ARegionType *region_type);
} GpencilModifierTypeInfo;

static GpencilModifierTypeInfo *modifier_gpencil_types[NUM_GREASEPENCIL_MODIFIER_TYPES] = {
    nullptr};

/* *************************************************** */
/* Modifier Methods - Evaluation Loops, etc. */

static const GpencilModifierTypeInfo *BKE_gpencil_modifier_get_info(GpencilModifierType type)
{
  /* type unsigned, no need to check < 0 */
  if (type < NUM_GREASEPENCIL_MODIFIER_TYPES && type > 0 &&
      modifier_gpencil_types[type]->name[0] != '\0')
  {
    return modifier_gpencil_types[type];
  }

  return nullptr;
}

void BKE_gpencil_frame_active_set(Depsgraph *depsgraph, bGPdata *gpd)
{
  DEG_debug_print_eval(depsgraph, __func__, gpd->id.name, gpd);
  int ctime = int(DEG_get_ctime(depsgraph));

  /* update active frame */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    gpl->actframe = BKE_gpencil_layer_frame_get(gpl, ctime, GP_GETFRAME_USE_PREV);
  }

  if (DEG_is_active(depsgraph)) {
    bGPdata *gpd_orig = (bGPdata *)DEG_get_original_id(&gpd->id);

    /* sync "actframe" changes back to main-db too,
     * so that editing tools work with copy-on-evaluation
     * when the current frame changes
     */
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd_orig->layers) {
      gpl->actframe = BKE_gpencil_layer_frame_get(gpl, ctime, GP_GETFRAME_USE_PREV);
    }
  }
}

static void modifier_free_data_id_us_cb(void * /*user_data*/,
                                        Object * /*ob*/,
                                        ID **idpoin,
                                        int cb_flag)
{
  ID *id = *idpoin;
  if (id != nullptr && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_min(id);
  }
}

void BKE_gpencil_modifier_free_ex(GpencilModifierData *md, const int flag)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(
      GpencilModifierType(md->type));

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    if (mti->foreach_ID_link) {
      mti->foreach_ID_link(md, nullptr, modifier_free_data_id_us_cb, nullptr);
    }
  }

  if (mti->free_data) {
    mti->free_data(md);
  }
  if (md->error) {
    MEM_freeN(md->error);
  }

  MEM_freeN(md);
}

void BKE_gpencil_modifier_free(GpencilModifierData *md)
{
  BKE_gpencil_modifier_free_ex(md, 0);
}

void BKE_gpencil_modifiers_foreach_ID_link(Object *ob,
                                           GreasePencilIDWalkFunc walk,
                                           void *user_data)
{
  GpencilModifierData *md = static_cast<GpencilModifierData *>(ob->greasepencil_modifiers.first);

  for (; md; md = md->next) {
    const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(
        GpencilModifierType(md->type));

    if (mti->foreach_ID_link) {
      mti->foreach_ID_link(md, ob, walk, user_data);
    }
  }
}

void BKE_gpencil_modifier_blend_write(BlendWriter *writer, ListBase *modbase)
{
  if (modbase == nullptr) {
    return;
  }

  LISTBASE_FOREACH (GpencilModifierData *, md, modbase) {
    const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(
        GpencilModifierType(md->type));
    if (mti == nullptr) {
      return;
    }

    BLO_write_struct_by_name(writer, mti->struct_name, md);

    if (md->type == eGpencilModifierType_Thick) {
      ThickGpencilModifierData *gpmd = (ThickGpencilModifierData *)md;

      if (gpmd->curve_thickness) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_thickness);
      }
    }
    else if (md->type == eGpencilModifierType_Noise) {
      NoiseGpencilModifierData *gpmd = (NoiseGpencilModifierData *)md;

      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Hook) {
      HookGpencilModifierData *gpmd = (HookGpencilModifierData *)md;

      if (gpmd->curfalloff) {
        BKE_curvemapping_blend_write(writer, gpmd->curfalloff);
      }
    }
    else if (md->type == eGpencilModifierType_Tint) {
      TintGpencilModifierData *gpmd = (TintGpencilModifierData *)md;
      if (gpmd->colorband) {
        BLO_write_struct(writer, ColorBand, gpmd->colorband);
      }
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Smooth) {
      SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Color) {
      ColorGpencilModifierData *gpmd = (ColorGpencilModifierData *)md;
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Opacity) {
      OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_write(writer, gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Dash) {
      DashGpencilModifierData *gpmd = (DashGpencilModifierData *)md;
      BLO_write_struct_array(
          writer, DashGpencilModifierSegment, gpmd->segments_len, gpmd->segments);
    }
    else if (md->type == eGpencilModifierType_Time) {
      TimeGpencilModifierData *gpmd = (TimeGpencilModifierData *)md;
      BLO_write_struct_array(
          writer, TimeGpencilModifierSegment, gpmd->segments_len, gpmd->segments);
    }
  }
}

void BKE_gpencil_modifier_blend_read_data(BlendDataReader *reader, ListBase *lb, Object *ob)
{
  BLO_read_struct_list(reader, GpencilModifierData, lb);

  LISTBASE_FOREACH (GpencilModifierData *, md, lb) {
    md->error = nullptr;

    /* if modifiers disappear, or for upward compatibility */
    if (nullptr == BKE_gpencil_modifier_get_info(GpencilModifierType(md->type))) {
      md->type = eModifierType_None;
    }

    /* If linking from a library, clear 'local' library override flag. */
    if (ID_IS_LINKED(ob)) {
      md->flag &= ~eGpencilModifierFlag_OverrideLibrary_Local;
    }

    if (md->type == eGpencilModifierType_Lattice) {
      LatticeGpencilModifierData *gpmd = (LatticeGpencilModifierData *)md;
      gpmd->cache_data = nullptr;
    }
    else if (md->type == eGpencilModifierType_Hook) {
      HookGpencilModifierData *hmd = (HookGpencilModifierData *)md;

      BLO_read_struct(reader, CurveMapping, &hmd->curfalloff);
      if (hmd->curfalloff) {
        BKE_curvemapping_blend_read(reader, hmd->curfalloff);
        BKE_curvemapping_init(hmd->curfalloff);
      }
    }
    else if (md->type == eGpencilModifierType_Noise) {
      NoiseGpencilModifierData *gpmd = (NoiseGpencilModifierData *)md;

      BLO_read_struct(reader, CurveMapping, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        /* Initialize the curve. Maybe this could be moved to modifier logic. */
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Thick) {
      ThickGpencilModifierData *gpmd = (ThickGpencilModifierData *)md;

      BLO_read_struct(reader, CurveMapping, &gpmd->curve_thickness);
      if (gpmd->curve_thickness) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_thickness);
        BKE_curvemapping_init(gpmd->curve_thickness);
      }
    }
    else if (md->type == eGpencilModifierType_Tint) {
      TintGpencilModifierData *gpmd = (TintGpencilModifierData *)md;
      BLO_read_struct(reader, ColorBand, &gpmd->colorband);
      BLO_read_struct(reader, CurveMapping, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Smooth) {
      SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;
      BLO_read_struct(reader, CurveMapping, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Color) {
      ColorGpencilModifierData *gpmd = (ColorGpencilModifierData *)md;
      BLO_read_struct(reader, CurveMapping, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Opacity) {
      OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;
      BLO_read_struct(reader, CurveMapping, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md->type == eGpencilModifierType_Dash) {
      DashGpencilModifierData *gpmd = (DashGpencilModifierData *)md;
      BLO_read_struct_array(
          reader, DashGpencilModifierSegment, gpmd->segments_len, &gpmd->segments);
      for (int i = 0; i < gpmd->segments_len; i++) {
        gpmd->segments[i].dmd = gpmd;
      }
    }
    else if (md->type == eGpencilModifierType_Time) {
      TimeGpencilModifierData *gpmd = (TimeGpencilModifierData *)md;
      BLO_read_struct_array(
          reader, TimeGpencilModifierSegment, gpmd->segments_len, &gpmd->segments);
      for (int i = 0; i < gpmd->segments_len; i++) {
        gpmd->segments[i].gpmd = gpmd;
      }
    }
    if (md->type == eGpencilModifierType_Shrinkwrap) {
      ShrinkwrapGpencilModifierData *gpmd = (ShrinkwrapGpencilModifierData *)md;
      gpmd->cache_data = nullptr;
    }
  }
}
