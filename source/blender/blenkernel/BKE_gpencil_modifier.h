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
 */
#ifndef __BKE_GPENCIL_MODIFIER_H__
#define __BKE_GPENCIL_MODIFIER_H__

/** \file
 * \ingroup bke
 */

#include "BLI_compiler_attrs.h"
#include "DNA_gpencil_modifier_types.h" /* needed for all enum typdefs */

#ifdef __cplusplus
extern "C" {
#endif

struct ARegionType;
struct Depsgraph;
struct GpencilModifierData;
struct ID;
struct ListBase;
struct Main;
struct ModifierUpdateDepsgraphContext;
struct Object;
struct Scene;
/* NOTE: bakeModifier() called from UI:
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

/* IMPORTANT! Keep ObjectWalkFunc and IDWalkFunc signatures compatible. */
typedef void (*GreasePencilObjectWalkFunc)(void *userData,
                                           struct Object *ob,
                                           struct Object **obpoin,
                                           int cb_flag);
typedef void (*GreasePencilIDWalkFunc)(void *userData,
                                       struct Object *ob,
                                       struct ID **idpoin,
                                       int cb_flag);
typedef void (*GreasePencilTexWalkFunc)(void *userData,
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
  void (*copyData)(const struct GpencilModifierData *md, struct GpencilModifierData *target);

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
  void (*deformStroke)(struct GpencilModifierData *md,
                       struct Depsgraph *depsgraph,
                       struct Object *ob,
                       struct bGPDlayer *gpl,
                       struct bGPDframe *gpf,
                       struct bGPDstroke *gps);

  /**
   * Callback for GP "geometry" modifiers that create extra geometry
   * in the frame (e.g. Array)
   */
  void (*generateStrokes)(struct GpencilModifierData *md,
                          struct Depsgraph *depsgraph,
                          struct Object *ob);

  /**
   * Bake-down GP modifier's effects into the GP data-block.
   *
   * This gets called when the user clicks the "Apply" button in the UI.
   * As such, this callback needs to go through all layers/frames in the
   * data-block, mutating the geometry and/or creating new data-blocks/objects
   */
  void (*bakeModifier)(struct Main *bmain,
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
  int (*remapTime)(struct GpencilModifierData *md,
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
  void (*initData)(struct GpencilModifierData *md);

  /**
   * Free internal modifier data variables, this function should
   * not free the md variable itself.
   *
   * This function is optional.
   */
  void (*freeData)(struct GpencilModifierData *md);

  /**
   * Return a boolean value indicating if this modifier is able to be
   * calculated based on the modifier data. This is *not* regarding the
   * md->flag, that is tested by the system, this is just if the data
   * validates (for example, a lattice will return false if the lattice
   * object is not defined).
   *
   * This function is optional (assumes never disabled if not present).
   */
  bool (*isDisabled)(struct GpencilModifierData *md, int userRenderParams);

  /**
   * Add the appropriate relations to the dependency graph.
   *
   * This function is optional.
   */
  void (*updateDepsgraph)(struct GpencilModifierData *md,
                          const struct ModifierUpdateDepsgraphContext *ctx);

  /**
   * Should return true if the modifier needs to be recalculated on time
   * changes.
   *
   * This function is optional (assumes false if not present).
   */
  bool (*dependsOnTime)(struct GpencilModifierData *md);

  /**
   * Should call the given walk function on with a pointer to each Object
   * pointer that the modifier data stores. This is used for linking on file
   * load and for unlinking objects or forwarding object references.
   *
   * This function is optional.
   */
  void (*foreachObjectLink)(struct GpencilModifierData *md,
                            struct Object *ob,
                            GreasePencilObjectWalkFunc walk,
                            void *userData);

  /**
   * Should call the given walk function with a pointer to each ID
   * pointer (i.e. each data-block pointer) that the modifier data
   * stores. This is used for linking on file load and for
   * unlinking data-blocks or forwarding data-block references.
   *
   * This function is optional. If it is not present, foreachObjectLink
   * will be used.
   */
  void (*foreachIDLink)(struct GpencilModifierData *md,
                        struct Object *ob,
                        GreasePencilIDWalkFunc walk,
                        void *userData);

  /**
   * Should call the given walk function for each texture that the
   * modifier data stores. This is used for finding all textures in
   * the context for the UI.
   *
   * This function is optional. If it is not present, it will be
   * assumed the modifier has no textures.
   */
  void (*foreachTexLink)(struct GpencilModifierData *md,
                         struct Object *ob,
                         GreasePencilTexWalkFunc walk,
                         void *userData);

  /* Register the panel types for the modifier's UI. */
  void (*panelRegister)(struct ARegionType *region_type);
} GpencilModifierTypeInfo;

#define GPENCIL_MODIFIER_TYPE_PANEL_PREFIX "MOD_PT_gpencil_"

/* Initialize modifier's global data (type info and some common global storages). */
void BKE_gpencil_modifier_init(void);

void BKE_gpencil_modifierType_panel_id(GpencilModifierType type, char *r_idname);
const GpencilModifierTypeInfo *BKE_gpencil_modifier_get_info(GpencilModifierType type);
struct GpencilModifierData *BKE_gpencil_modifier_new(int type);
void BKE_gpencil_modifier_free_ex(struct GpencilModifierData *md, const int flag);
void BKE_gpencil_modifier_free(struct GpencilModifierData *md);
bool BKE_gpencil_modifier_unique_name(struct ListBase *modifiers, struct GpencilModifierData *gmd);
bool BKE_gpencil_modifier_depends_ontime(struct GpencilModifierData *md);
struct GpencilModifierData *BKE_gpencil_modifiers_findby_type(struct Object *ob,
                                                              GpencilModifierType type);
struct GpencilModifierData *BKE_gpencil_modifiers_findby_name(struct Object *ob, const char *name);
void BKE_gpencil_modifier_copydata_generic(const struct GpencilModifierData *md_src,
                                           struct GpencilModifierData *md_dst);
void BKE_gpencil_modifier_copydata(struct GpencilModifierData *md,
                                   struct GpencilModifierData *target);
void BKE_gpencil_modifier_copydata_ex(struct GpencilModifierData *md,
                                      struct GpencilModifierData *target,
                                      const int flag);
void BKE_gpencil_modifier_set_error(struct GpencilModifierData *md, const char *format, ...)
    ATTR_PRINTF_FORMAT(2, 3);
void BKE_gpencil_modifiers_foreach_ID_link(struct Object *ob,
                                           GreasePencilIDWalkFunc walk,
                                           void *userData);
void BKE_gpencil_modifiers_foreach_tex_link(struct Object *ob,
                                            GreasePencilTexWalkFunc walk,
                                            void *userData);

bool BKE_gpencil_has_geometry_modifiers(struct Object *ob);
bool BKE_gpencil_has_time_modifiers(struct Object *ob);
bool BKE_gpencil_has_transform_modifiers(struct Object *ob);

void BKE_gpencil_lattice_init(struct Object *ob);
void BKE_gpencil_lattice_clear(struct Object *ob);

void BKE_gpencil_modifiers_calc(struct Depsgraph *depsgraph,
                                struct Scene *scene,
                                struct Object *ob);

void BKE_gpencil_prepare_eval_data(struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *ob);

struct bGPDframe *BKE_gpencil_frame_retime_get(struct Depsgraph *depsgraph,
                                               struct Scene *scene,
                                               struct Object *ob,
                                               struct bGPDlayer *gpl);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_GPENCIL_MODIFIER_H__ */
