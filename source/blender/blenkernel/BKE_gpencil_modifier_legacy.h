/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_compiler_attrs.h"
#include "DNA_gpencil_modifier_types.h" /* needed for all enum typdefs */

#ifdef __cplusplus
extern "C" {
#endif

struct ARegionType;
struct BlendDataReader;
struct BlendLibReader;
struct BlendWriter;
struct Depsgraph;
struct GpencilModifierData;
struct ID;
struct ListBase;
struct Main;
struct ModifierUpdateDepsgraphContext;
struct Object;
struct Scene;
/* NOTE: bake_modifier() called from UI:
 * needs to create new data-blocks, hence the need for this. */
struct bGPDframe;
struct bGPDlayer;
struct bGPDstroke;

#define GPENCIL_MODIFIER_ACTIVE(_md, _is_render) \
  ((((_md)->mode & eGpencilModifierMode_Realtime) && (_is_render == false)) || \
   (((_md)->mode & eGpencilModifierMode_Render) && (_is_render == true)))
#define GPENCIL_MODIFIER_EDIT(_md, _is_edit) \
  ((((_md)->mode & eGpencilModifierMode_Editmode) == 0) && (_is_edit))

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

typedef void (*GreasePencilIDWalkFunc)(void *user_data,
                                       struct Object *ob,
                                       struct ID **idpoin,
                                       int cb_flag);
typedef void (*GreasePencilTexWalkFunc)(void *user_data,
                                        struct Object *ob,
                                        struct GpencilModifierData *md,
                                        const char *propname);

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

#define GPENCIL_MODIFIER_TYPE_PANEL_PREFIX "MOD_PT_gpencil_"

/**
 * Initialize modifier's global data (type info and some common global storage).
 */
void BKE_gpencil_modifier_init(void);

/**
 * Get the idname of the modifier type's panel, which was defined in the #panel_register callback.
 *
 * \param type: Type of modifier.
 * \param r_idname: ID name.
 */
void BKE_gpencil_modifierType_panel_id(GpencilModifierType type, char *r_idname);
void BKE_gpencil_modifier_panel_expand(struct GpencilModifierData *md);
/**
 * Get grease pencil modifier information.
 * \param type: Type of modifier.
 * \return Pointer to type
 */
const GpencilModifierTypeInfo *BKE_gpencil_modifier_get_info(GpencilModifierType type);
/**
 * Create new grease pencil modifier.
 * \param type: Type of modifier.
 * \return New modifier pointer.
 */
struct GpencilModifierData *BKE_gpencil_modifier_new(int type);
/**
 * Free grease pencil modifier data
 * \param md: Modifier data.
 * \param flag: Flags.
 */
void BKE_gpencil_modifier_free_ex(struct GpencilModifierData *md, int flag);
/**
 * Free grease pencil modifier data
 * \param md: Modifier data.
 */
void BKE_gpencil_modifier_free(struct GpencilModifierData *md);
/* check unique name */
bool BKE_gpencil_modifier_unique_name(struct ListBase *modifiers, struct GpencilModifierData *gmd);
/**
 * Check if grease pencil modifier depends on time.
 * \param md: Modifier data.
 * \return True if depends on time.
 */
bool BKE_gpencil_modifier_depends_ontime(struct GpencilModifierData *md);
struct GpencilModifierData *BKE_gpencil_modifiers_findby_type(struct Object *ob,
                                                              GpencilModifierType type);
/**
 * Find grease pencil modifier by name.
 * \param ob: Grease pencil object.
 * \param name: Name to find.
 * \return Pointer to modifier.
 */
struct GpencilModifierData *BKE_gpencil_modifiers_findby_name(struct Object *ob, const char *name);
/**
 * Generic grease pencil modifier copy data.
 * \param md_src: Source modifier data.
 * \param md_dst: Target modifier data.
 */
void BKE_gpencil_modifier_copydata_generic(const struct GpencilModifierData *md_src,
                                           struct GpencilModifierData *md_dst);
/**
 * Copy grease pencil modifier data.
 * \param md: Source modifier data.
 * \param target: Target modifier data.
 */
void BKE_gpencil_modifier_copydata(struct GpencilModifierData *md,
                                   struct GpencilModifierData *target);
/**
 * Copy grease pencil modifier data.
 * \param md: Source modifier data.
 * \param target: Target modifier data.
 * \param flag: Flags.
 */
void BKE_gpencil_modifier_copydata_ex(struct GpencilModifierData *md,
                                      struct GpencilModifierData *target,
                                      int flag);
/**
 * Set grease pencil modifier error.
 * \param md: Modifier data.
 * \param format: Format.
 */
void BKE_gpencil_modifier_set_error(struct GpencilModifierData *md, const char *format, ...)
    ATTR_PRINTF_FORMAT(2, 3);
/**
 * Link grease pencil modifier related IDs.
 * \param ob: Grease pencil object.
 * \param walk: Walk option.
 * \param user_data: User data.
 */
void BKE_gpencil_modifiers_foreach_ID_link(struct Object *ob,
                                           GreasePencilIDWalkFunc walk,
                                           void *user_data);
/**
 * Link grease pencil modifier related Texts.
 * \param ob: Grease pencil object.
 * \param walk: Walk option.
 * \param user_data: User data.
 */
void BKE_gpencil_modifiers_foreach_tex_link(struct Object *ob,
                                            GreasePencilTexWalkFunc walk,
                                            void *user_data);

/**
 * Check whether given modifier is not local (i.e. from linked data) when the object is a library
 * override.
 *
 * \param gmd: May be NULL, in which case we consider it as a non-local modifier case.
 */
bool BKE_gpencil_modifier_is_nonlocal_in_liboverride(const struct Object *ob,
                                                     const struct GpencilModifierData *gmd);

typedef struct GpencilVirtualModifierData {
  ArmatureGpencilModifierData amd;
  LatticeGpencilModifierData lmd;
} GpencilVirtualModifierData;

/**
 * This is to include things that are not modifiers in the evaluation of the modifier stack,
 * for example parenting to an armature or lattice without having a real modifier.
 */
struct GpencilModifierData *BKE_gpencil_modifiers_get_virtual_modifierlist(
    const struct Object *ob, struct GpencilVirtualModifierData *data);

/**
 * Check if object has grease pencil Geometry modifiers.
 * \param ob: Grease pencil object.
 * \return True if exist.
 */
bool BKE_gpencil_has_geometry_modifiers(struct Object *ob);
/**
 * Check if object has grease pencil Time modifiers.
 * \param ob: Grease pencil object.
 * \return True if exist.
 */
bool BKE_gpencil_has_time_modifiers(struct Object *ob);
/**
 * Check if object has grease pencil transform stroke modifiers.
 * \param ob: Grease pencil object.
 * \return True if exist.
 */
bool BKE_gpencil_has_transform_modifiers(struct Object *ob);

/* Stores the maximum calculation range in the whole modifier stack for line art so the cache can
 * cover everything that will be visible. */
typedef struct GpencilLineartLimitInfo {
  char min_level;
  char max_level;
  short edge_types;
  char shadow_selection;
  char silhouette_selection;
} GpencilLineartLimitInfo;

GpencilLineartLimitInfo BKE_gpencil_get_lineart_modifier_limits(const struct Object *ob);

void BKE_gpencil_set_lineart_modifier_limits(struct GpencilModifierData *md,
                                             const struct GpencilLineartLimitInfo *info,
                                             bool is_first_lineart);
bool BKE_gpencil_is_first_lineart_in_stack(const struct Object *ob,
                                           const struct GpencilModifierData *md);

/**
 * Init grease pencil cache deform data.
 * \param ob: Grease pencil object
 */
void BKE_gpencil_cache_data_init(struct Depsgraph *depsgraph, struct Object *ob);
/**
 * Clear grease pencil cache deform data.
 * \param ob: Grease pencil object
 */
void BKE_gpencil_cache_data_clear(struct Object *ob);

/**
 * Calculate grease-pencil modifiers.
 * \param depsgraph: Current depsgraph.
 * \param scene: Current scene.
 * \param ob: Grease pencil object.
 */
void BKE_gpencil_modifiers_calc(struct Depsgraph *depsgraph,
                                struct Scene *scene,
                                struct Object *ob);

/**
 * Prepare grease pencil eval data for modifiers
 * \param depsgraph: Current depsgraph.
 * \param scene: Current scene.
 * \param ob: Grease pencil object.
 */
void BKE_gpencil_prepare_eval_data(struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *ob);

/**
 * Get the current frame re-timed with time modifiers.
 * \param depsgraph: Current depsgraph.
 * \param scene: Current scene.
 * \param ob: Grease pencil object.
 * \param gpl: Grease pencil layer.
 * \return New frame number.
 */
struct bGPDframe *BKE_gpencil_frame_retime_get(struct Depsgraph *depsgraph,
                                               struct Scene *scene,
                                               struct Object *ob,
                                               struct bGPDlayer *gpl);
/**
 * Get Time modifier frame number.
 */
int BKE_gpencil_time_modifier_cfra(struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *ob,
                                   struct bGPDlayer *gpl,
                                   int cfra,
                                   bool is_render);

void BKE_gpencil_modifier_blend_write(struct BlendWriter *writer, struct ListBase *modbase);
void BKE_gpencil_modifier_blend_read_data(struct BlendDataReader *reader,
                                          struct ListBase *lb,
                                          struct Object *ob);
void BKE_gpencil_modifier_blend_read_lib(struct BlendLibReader *reader, struct Object *ob);

#ifdef __cplusplus
}
#endif
