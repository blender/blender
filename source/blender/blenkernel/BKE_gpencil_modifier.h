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

#include "DNA_gpencil_modifier_types.h" /* needed for all enum typdefs */
#include "BLI_compiler_attrs.h"
#include "BKE_customdata.h"

struct BMEditMesh;
struct DepsNodeHandle;
struct Depsgraph;
struct DerivedMesh;
struct GpencilModifierData;
struct ID;
struct ListBase;
struct Main;
struct Mesh;
struct ModifierUpdateDepsgraphContext;
struct Object;
struct Scene;
struct ViewLayer;
struct bArmature;
/* NOTE: bakeModifier() called from UI:
 * needs to create new databloc-ks, hence the need for this. */
struct bContext;
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
  eGpencilModifierTypeFlag_SupportsMapping = (1 << 0),
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
  eGpencilModifierTypeFlag_RequiresOriginalData = (1 << 3),

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
                       struct bGPDstroke *gps);

  /**
   * Callback for GP "geometry" modifiers that create extra geometry
   * in the frame (e.g. Array)
   *
   * The gpf parameter contains the GP frame/strokes to operate on. This is
   * usually a copy of the original (unmodified and saved to files) stroke data.
   * Modifiers should only add any generated strokes to this frame (and not one accessed
   * via the gpl parameter).
   *
   * The modifier_index parameter indicates where the modifier is
   * in the modifier stack in relation to other modifiers.
   */
  void (*generateStrokes)(struct GpencilModifierData *md,
                          struct Depsgraph *depsgraph,
                          struct Object *ob,
                          struct bGPDlayer *gpl,
                          struct bGPDframe *gpf);

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

  /**
   * Get the number of times the strokes are duplicated in this modifier.
   * This is used to calculate the size of the GPU VBOs
   */
  int (*getDuplicationFactor)(struct GpencilModifierData *md);
} GpencilModifierTypeInfo;

/* Initialize modifier's global data (type info and some common global storages). */
void BKE_gpencil_modifier_init(void);

const GpencilModifierTypeInfo *BKE_gpencil_modifierType_getInfo(GpencilModifierType type);
struct GpencilModifierData *BKE_gpencil_modifier_new(int type);
void BKE_gpencil_modifier_free_ex(struct GpencilModifierData *md, const int flag);
void BKE_gpencil_modifier_free(struct GpencilModifierData *md);
bool BKE_gpencil_modifier_unique_name(struct ListBase *modifiers, struct GpencilModifierData *gmd);
bool BKE_gpencil_modifier_dependsOnTime(struct GpencilModifierData *md);
struct GpencilModifierData *BKE_gpencil_modifiers_findByType(struct Object *ob,
                                                             GpencilModifierType type);
struct GpencilModifierData *BKE_gpencil_modifiers_findByName(struct Object *ob, const char *name);
void BKE_gpencil_modifier_copyData_generic(const struct GpencilModifierData *md_src,
                                           struct GpencilModifierData *md_dst);
void BKE_gpencil_modifier_copyData(struct GpencilModifierData *md,
                                   struct GpencilModifierData *target);
void BKE_gpencil_modifier_copyData_ex(struct GpencilModifierData *md,
                                      struct GpencilModifierData *target,
                                      const int flag);
void BKE_gpencil_modifiers_foreachIDLink(struct Object *ob,
                                         GreasePencilIDWalkFunc walk,
                                         void *userData);
void BKE_gpencil_modifiers_foreachTexLink(struct Object *ob,
                                          GreasePencilTexWalkFunc walk,
                                          void *userData);

bool BKE_gpencil_has_geometry_modifiers(struct Object *ob);
bool BKE_gpencil_has_time_modifiers(struct Object *ob);

void BKE_gpencil_stroke_modifiers(struct Depsgraph *depsgraph,
                                  struct Object *ob,
                                  struct bGPDlayer *gpl,
                                  struct bGPDframe *gpf,
                                  struct bGPDstroke *gps,
                                  bool is_render);
void BKE_gpencil_geometry_modifiers(struct Depsgraph *depsgraph,
                                    struct Object *ob,
                                    struct bGPDlayer *gpl,
                                    struct bGPDframe *gpf,
                                    bool is_render);
int BKE_gpencil_time_modifier(struct Depsgraph *depsgraph,
                              struct Scene *scene,
                              struct Object *ob,
                              struct bGPDlayer *gpl,
                              int cfra,
                              bool is_render);

void BKE_gpencil_lattice_init(struct Object *ob);
void BKE_gpencil_lattice_clear(struct Object *ob);

#endif /* __BKE_GPENCIL_MODIFIER_H__ */
