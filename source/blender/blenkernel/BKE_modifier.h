/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BKE_customdata.h"
#include "BLI_compiler_attrs.h"
#include "DNA_modifier_types.h" /* needed for all enum typdefs */

#ifdef __cplusplus
extern "C" {
#endif

struct ARegionType;
struct BMEditMesh;
struct BlendDataReader;
struct BlendLibReader;
struct BlendWriter;
struct CustomData_MeshMasks;
struct DepsNodeHandle;
struct Depsgraph;
struct GeometrySet;
struct ID;
struct ListBase;
struct Main;
struct Mesh;
struct ModifierData;
struct Object;
struct Scene;
struct bArmature;

typedef enum {
  /* Should not be used, only for None modifier type */
  eModifierTypeType_None,

  /**
   * Modifier only does deformation, implies that modifier
   * type should have a valid deformVerts function. OnlyDeform
   * style modifiers implicitly accept either mesh or CV
   * input but should still declare flags appropriately.
   */
  eModifierTypeType_OnlyDeform,

  /** Modifier adds geometry. */
  eModifierTypeType_Constructive,
  /* Modifier can add and remove geometry. */
  eModifierTypeType_Nonconstructive,

  /**
   * Both deformVerts & applyModifier are valid calls
   * used for particles modifier that doesn't actually modify the object
   * unless it's a mesh and can be exploded -> curve can also emit particles
   */
  eModifierTypeType_DeformOrConstruct,

  /**
   * Like eModifierTypeType_Nonconstructive, but does not affect the geometry
   * of the object, rather some of its CustomData layers.
   * E.g. UVProject and WeightVG modifiers. */
  eModifierTypeType_NonGeometrical,
} ModifierTypeType;

typedef enum {
  eModifierTypeFlag_AcceptsMesh = (1 << 0),
  eModifierTypeFlag_AcceptsCVs = (1 << 1),
  eModifierTypeFlag_SupportsMapping = (1 << 2),
  eModifierTypeFlag_SupportsEditmode = (1 << 3),

  /**
   * For modifiers that support editmode this determines if the
   * modifier should be enabled by default in editmode. This should
   * only be used by modifiers that are relatively speedy and
   * also generally used in editmode, otherwise let the user enable
   * it by hand.
   */
  eModifierTypeFlag_EnableInEditmode = (1 << 4),

  /**
   * For modifiers that require original data and so cannot
   * be placed after any non-deformative modifier.
   */
  eModifierTypeFlag_RequiresOriginalData = (1 << 5),

  /**
   * For modifiers that support point-cache,
   * so we can check to see if it has files we need to deal with.
   */
  eModifierTypeFlag_UsesPointCache = (1 << 6),

  /** For physics modifiers, max one per type */
  eModifierTypeFlag_Single = (1 << 7),

  /** Some modifier can't be added manually by user */
  eModifierTypeFlag_NoUserAdd = (1 << 8),

  /** For modifiers that use CD_PREVIEW_MCOL for preview. */
  eModifierTypeFlag_UsesPreview = (1 << 9),
  eModifierTypeFlag_AcceptsVertexCosOnly = (1 << 10),

  /** Accepts #BMesh input (without conversion). */
  eModifierTypeFlag_AcceptsBMesh = (1 << 11),

  /** Modifier output needs to be cached. */
  eModifierTypeFlag_NeedCaching = (1 << 12),
} ModifierTypeFlag;
ENUM_OPERATORS(ModifierTypeFlag, eModifierTypeFlag_AcceptsBMesh)

typedef void (*IDWalkFunc)(void *userData, struct Object *ob, struct ID **idpoin, int cb_flag);
typedef void (*TexWalkFunc)(void *userData,
                            struct Object *ob,
                            struct ModifierData *md,
                            const char *propname);

typedef enum ModifierApplyFlag {
  /** Render time. */
  MOD_APPLY_RENDER = 1 << 0,
  /** Result of evaluation will be cached, so modifier might
   * want to cache data for quick updates (used by subdivision-surface) */
  MOD_APPLY_USECACHE = 1 << 1,
  /** Modifier evaluated for undeformed texture coordinates */
  MOD_APPLY_ORCO = 1 << 2,
  /** Ignore scene simplification flag and use subdivisions
   * level set in multires modifier. */
  MOD_APPLY_IGNORE_SIMPLIFY = 1 << 3,
  /** The effect of this modifier will be applied to the base mesh
   * The modifier itself will be removed from the modifier stack.
   * This flag can be checked to ignore rendering display data to the mesh.
   * See `OBJECT_OT_modifier_apply` operator. */
  MOD_APPLY_TO_BASE_MESH = 1 << 4,
} ModifierApplyFlag;

typedef struct ModifierUpdateDepsgraphContext {
  struct Scene *scene;
  struct Object *object;
  struct DepsNodeHandle *node;
} ModifierUpdateDepsgraphContext;

/* Contains the information for deformXXX and applyXXX functions below that
 * doesn't change between consecutive modifiers. */
typedef struct ModifierEvalContext {
  struct Depsgraph *depsgraph;
  struct Object *object;
  ModifierApplyFlag flag;
} ModifierEvalContext;

typedef struct ModifierTypeInfo {
  /* The user visible name for this modifier */
  char name[32];

  /* The DNA struct name for the modifier data type, used to
   * write the DNA data out.
   */
  char structName[32];

  /* The size of the modifier data type, used by allocation. */
  int structSize;

  /* StructRNA of this modifier. This is typically something like RNA_*Modifier. */
  struct StructRNA *srna;

  ModifierTypeType type;
  ModifierTypeFlag flags;

  /* Icon of the modifier. Usually something like ICON_MOD_*. */
  int icon;

  /********************* Non-optional functions *********************/

  /**
   * Copy instance data for this modifier type. Should copy all user
   * level settings to the target modifier.
   *
   * \param flag: Copying options (see BKE_lib_id.h's LIB_ID_COPY_... flags for more).
   */
  void (*copyData)(const struct ModifierData *md, struct ModifierData *target, int flag);

  /********************* Deform modifier functions *********************/

  /**
   * Only for deform types, should apply the deformation
   * to the given vertex array. If the deformer requires information from
   * the object it can obtain it from the mesh argument if non-NULL,
   * and otherwise the ob argument.
   */
  void (*deformVerts)(struct ModifierData *md,
                      const struct ModifierEvalContext *ctx,
                      struct Mesh *mesh,
                      float (*vertexCos)[3],
                      int numVerts);

  /**
   * Like deformMatricesEM but called from object mode (for supporting modifiers in sculpt mode).
   */
  void (*deformMatrices)(struct ModifierData *md,
                         const struct ModifierEvalContext *ctx,
                         struct Mesh *mesh,
                         float (*vertexCos)[3],
                         float (*defMats)[3][3],
                         int numVerts);
  /**
   * Like deformVerts but called during editmode (for supporting modifiers)
   */
  void (*deformVertsEM)(struct ModifierData *md,
                        const struct ModifierEvalContext *ctx,
                        struct BMEditMesh *editData,
                        struct Mesh *mesh,
                        float (*vertexCos)[3],
                        int numVerts);

  /* Set deform matrix per vertex for crazy-space correction */
  void (*deformMatricesEM)(struct ModifierData *md,
                           const struct ModifierEvalContext *ctx,
                           struct BMEditMesh *editData,
                           struct Mesh *mesh,
                           float (*vertexCos)[3],
                           float (*defMats)[3][3],
                           int numVerts);

  /********************* Non-deform modifier functions *********************/

  /**
   * For non-deform types: apply the modifier and return a mesh data-block.
   *
   * The mesh argument should always be non-NULL; the modifier should use the
   * passed in mesh data-block rather than object->data, as it contains the mesh
   * with modifier applied up to this point.
   *
   * The modifier may modify and return the mesh argument, but must not free it
   * and must ensure any referenced data layers are converted to non-referenced
   * before modification.
   */
  struct Mesh *(*modifyMesh)(struct ModifierData *md,
                             const struct ModifierEvalContext *ctx,
                             struct Mesh *mesh);

  /**
   * The modifier has to change the geometry set in-place. The geometry set can contain zero or
   * more geometry components. This callback can be used by modifiers that don't work on any
   * specific type of geometry (e.g. mesh).
   */
  void (*modifyGeometrySet)(struct ModifierData *md,
                            const struct ModifierEvalContext *ctx,
                            struct GeometrySet *geometry_set);

  /********************* Optional functions *********************/

  /**
   * Initialize new instance data for this modifier type, this function
   * should set modifier variables to their default values.
   *
   * This function is optional.
   */
  void (*initData)(struct ModifierData *md);

  /**
   * Should add to passed \a r_cddata_masks the data types that this
   * modifier needs. If (mask & (1 << (layer type))) != 0, this modifier
   * needs that custom data layer. It can change required layers
   * depending on the modifier's settings.
   *
   * Note that this means extra data (e.g. vertex groups) - it is assumed
   * that all modifiers need mesh data and deform modifiers need vertex
   * coordinates.
   *
   * If this function is not present, it is assumed that no extra data is needed.
   *
   * This function is optional.
   */
  void (*requiredDataMask)(struct Object *ob,
                           struct ModifierData *md,
                           struct CustomData_MeshMasks *r_cddata_masks);

  /**
   * Free internal modifier data variables, this function should
   * not free the md variable itself.
   *
   * This function is responsible for freeing the runtime data as well.
   *
   * This function is optional.
   */
  void (*freeData)(struct ModifierData *md);

  /**
   * Return a boolean value indicating if this modifier is able to be
   * calculated based on the modifier data. This is *not* regarding the
   * md->flag, that is tested by the system, this is just if the data
   * validates (for example, a lattice will return false if the lattice
   * object is not defined).
   *
   * This function is optional (assumes never disabled if not present).
   */
  bool (*isDisabled)(const struct Scene *scene, struct ModifierData *md, bool userRenderParams);

  /**
   * Add the appropriate relations to the dependency graph.
   *
   * This function is optional.
   */
  void (*updateDepsgraph)(struct ModifierData *md, const ModifierUpdateDepsgraphContext *ctx);

  /**
   * Should return true if the modifier needs to be recalculated on time
   * changes.
   *
   * This function is optional (assumes false if not present).
   */
  bool (*dependsOnTime)(struct Scene *scene, struct ModifierData *md);

  /**
   * True when a deform modifier uses normals, the requiredDataMask
   * can't be used here because that refers to a normal layer whereas
   * in this case we need to know if the deform modifier uses normals.
   *
   * this is needed because applying 2 deform modifiers will give the
   * second modifier bogus normals.
   */
  bool (*dependsOnNormals)(struct ModifierData *md);

  /**
   * Should call the given walk function with a pointer to each ID
   * pointer (i.e. each data-block pointer) that the modifier data
   * stores. This is used for linking on file load and for
   * unlinking data-blocks or forwarding data-block references.
   *
   * This function is optional.
   */
  void (*foreachIDLink)(struct ModifierData *md,
                        struct Object *ob,
                        IDWalkFunc walk,
                        void *userData);

  /**
   * Should call the given walk function for each texture that the
   * modifier data stores. This is used for finding all textures in
   * the context for the UI.
   *
   * This function is optional. If it is not present, it will be
   * assumed the modifier has no textures.
   */
  void (*foreachTexLink)(struct ModifierData *md,
                         struct Object *ob,
                         TexWalkFunc walk,
                         void *userData);

  /**
   * Free given run-time data.
   *
   * This data is coming from a modifier of the corresponding type, but actual
   * modifier data is not known here.
   *
   * Notes:
   *  - The data itself is to be de-allocated as well.
   *  - This callback is allowed to receive NULL pointer as a data, so it's
   *    more like "ensure the data is freed".
   */
  void (*freeRuntimeData)(void *runtime_data);

  /** Register the panel types for the modifier's UI. */
  void (*panelRegister)(struct ARegionType *region_type);

  /**
   * Is called when the modifier is written to a file. The modifier data struct itself is written
   * already.
   *
   * This method should write any additional arrays and referenced structs that should be
   * stored in the file.
   */
  void (*blendWrite)(struct BlendWriter *writer,
                     const struct ID *id_owner,
                     const struct ModifierData *md);

  /**
   * Is called when the modifier is read from a file.
   *
   * It can be used to update pointers to arrays and other structs. Furthermore, fields that have
   * not been written (e.g. runtime data) can be reset.
   */
  void (*blendRead)(struct BlendDataReader *reader, struct ModifierData *md);
} ModifierTypeInfo;

/* Used to find a modifier's panel type. */
#define MODIFIER_TYPE_PANEL_PREFIX "MOD_PT_"

/* Initialize modifier's global data (type info and some common global storage). */
void BKE_modifier_init(void);

const ModifierTypeInfo *BKE_modifier_get_info(ModifierType type);

/* For modifier UI panels. */

/**
 * Get the idname of the modifier type's panel, which was defined in the #panelRegister callback.
 */
void BKE_modifier_type_panel_id(ModifierType type, char *r_idname);
void BKE_modifier_panel_expand(struct ModifierData *md);

/* Modifier utility calls, do call through type pointer and return
 * default values if pointer is optional.
 */
struct ModifierData *BKE_modifier_new(int type);

void BKE_modifier_free_ex(struct ModifierData *md, int flag);
void BKE_modifier_free(struct ModifierData *md);
/**
 * Use instead of `BLI_remlink` when the object's active modifier should change.
 */
void BKE_modifier_remove_from_list(struct Object *ob, struct ModifierData *md);

/* Generate new UUID for the given modifier. */
void BKE_modifier_session_uuid_generate(struct ModifierData *md);

bool BKE_modifier_unique_name(struct ListBase *modifiers, struct ModifierData *md);

struct ModifierData *BKE_modifier_copy_ex(const struct ModifierData *md, int flag);

/**
 * Callback's can use this to avoid copying every member.
 */
void BKE_modifier_copydata_generic(const struct ModifierData *md,
                                   struct ModifierData *md_dst,
                                   int flag);
void BKE_modifier_copydata(const struct ModifierData *md, struct ModifierData *target);
void BKE_modifier_copydata_ex(const struct ModifierData *md,
                              struct ModifierData *target,
                              int flag);
bool BKE_modifier_depends_ontime(struct Scene *scene, struct ModifierData *md);
bool BKE_modifier_supports_mapping(struct ModifierData *md);
bool BKE_modifier_supports_cage(struct Scene *scene, struct ModifierData *md);
bool BKE_modifier_couldbe_cage(struct Scene *scene, struct ModifierData *md);
bool BKE_modifier_is_correctable_deformed(struct ModifierData *md);
bool BKE_modifier_is_same_topology(ModifierData *md);
bool BKE_modifier_is_non_geometrical(ModifierData *md);
/**
 * Check whether is enabled.
 *
 * \param scene: Current scene, may be NULL,
 * in which case `isDisabled` callback of the modifier is never called.
 */
bool BKE_modifier_is_enabled(const struct Scene *scene,
                             struct ModifierData *md,
                             int required_mode);
/**
 * Check whether given modifier is not local (i.e. from linked data) when the object is a library
 * override.
 *
 * \param md: May be NULL, in which case we consider it as a non-local modifier case.
 */
bool BKE_modifier_is_nonlocal_in_liboverride(const struct Object *ob,
                                             const struct ModifierData *md);
void BKE_modifier_set_error(const struct Object *ob,
                            struct ModifierData *md,
                            const char *format,
                            ...) ATTR_PRINTF_FORMAT(3, 4);
bool BKE_modifier_is_preview(struct ModifierData *md);

void BKE_modifiers_foreach_ID_link(struct Object *ob, IDWalkFunc walk, void *userData);
void BKE_modifiers_foreach_tex_link(struct Object *ob, TexWalkFunc walk, void *userData);

struct ModifierData *BKE_modifiers_findby_type(const struct Object *ob, ModifierType type);
struct ModifierData *BKE_modifiers_findby_name(const struct Object *ob, const char *name);
struct ModifierData *BKE_modifiers_findby_session_uuid(const struct Object *ob,
                                                       const SessionUUID *session_uuid);
void BKE_modifiers_clear_errors(struct Object *ob);
/**
 * used for buttons, to find out if the 'draw deformed in edit-mode option is there.
 *
 * Also used in transform_conversion.c, to detect crazy-space (2nd arg then is NULL).
 * Also used for some mesh tools to give warnings.
 */
int BKE_modifiers_get_cage_index(const struct Scene *scene,
                                 struct Object *ob,
                                 int *r_lastPossibleCageIndex,
                                 bool is_virtual);

bool BKE_modifiers_is_modifier_enabled(struct Object *ob, int modifierType);
bool BKE_modifiers_is_softbody_enabled(struct Object *ob);
bool BKE_modifiers_is_cloth_enabled(struct Object *ob);
bool BKE_modifiers_is_particle_enabled(struct Object *ob);

/**
 * Takes an object and returns its first selected armature, else just its armature.
 * This should work for multiple armatures per object.
 */
struct Object *BKE_modifiers_is_deformed_by_armature(struct Object *ob);
struct Object *BKE_modifiers_is_deformed_by_meshdeform(struct Object *ob);
/**
 * Takes an object and returns its first selected lattice, else just its lattice.
 * This should work for multiple lattices per object.
 */
struct Object *BKE_modifiers_is_deformed_by_lattice(struct Object *ob);
/**
 * Takes an object and returns its first selected curve, else just its curve.
 * This should work for multiple curves per object.
 */
struct Object *BKE_modifiers_is_deformed_by_curve(struct Object *ob);
bool BKE_modifiers_uses_multires(struct Object *ob);
bool BKE_modifiers_uses_armature(struct Object *ob, struct bArmature *arm);
bool BKE_modifiers_is_correctable_deformed(const struct Scene *scene, struct Object *ob);
void BKE_modifier_free_temporary_data(struct ModifierData *md);

typedef struct CDMaskLink {
  struct CDMaskLink *next;
  struct CustomData_MeshMasks mask;
} CDMaskLink;

/**
 * Calculates and returns a linked list of CustomData_MeshMasks and modified
 * final datamask, indicating the data required by each modifier in the stack
 * pointed to by md for correct evaluation, assuming the data indicated by
 * final_datamask is required at the end of the stack.
 */
struct CDMaskLink *BKE_modifier_calc_data_masks(const struct Scene *scene,
                                                struct Object *ob,
                                                struct ModifierData *md,
                                                struct CustomData_MeshMasks *final_datamask,
                                                int required_mode,
                                                ModifierData *previewmd,
                                                const struct CustomData_MeshMasks *previewmask);
struct ModifierData *BKE_modifier_get_last_preview(const struct Scene *scene,
                                                   struct ModifierData *md,
                                                   int required_mode);

typedef struct VirtualModifierData {
  ArmatureModifierData amd;
  CurveModifierData cmd;
  LatticeModifierData lmd;
  ShapeKeyModifierData smd;
} VirtualModifierData;

/**
 * This is to include things that are not modifiers in the evaluation of the modifier stack,
 * for example parenting to an armature.
 */
struct ModifierData *BKE_modifiers_get_virtual_modifierlist(const struct Object *ob,
                                                            struct VirtualModifierData *data);

/**
 * Ensure modifier correctness when changing `ob->data`.
 */
void BKE_modifiers_test_object(struct Object *ob);

/**
 * Here for #do_versions.
 */
void BKE_modifier_mdef_compact_influences(struct ModifierData *md);

/**
 * Initializes `path` with either the blend file or temporary directory.
 */
void BKE_modifier_path_init(char *path, int path_maxlen, const char *name);
const char *BKE_modifier_path_relbase(struct Main *bmain, struct Object *ob);
const char *BKE_modifier_path_relbase_from_global(struct Object *ob);

/* Accessors of original/evaluated modifiers. */

/**
 * For a given modifier data, get corresponding original one.
 * If the modifier data is already original, return it as-is.
 */
struct ModifierData *BKE_modifier_get_original(const struct Object *object,
                                               struct ModifierData *md);
struct ModifierData *BKE_modifier_get_evaluated(struct Depsgraph *depsgraph,
                                                struct Object *object,
                                                struct ModifierData *md);

/* wrappers for modifier callbacks that ensure valid normals */

struct Mesh *BKE_modifier_modify_mesh(ModifierData *md,
                                      const struct ModifierEvalContext *ctx,
                                      struct Mesh *me);

void BKE_modifier_deform_verts(ModifierData *md,
                               const struct ModifierEvalContext *ctx,
                               struct Mesh *me,
                               float (*vertexCos)[3],
                               int numVerts);

void BKE_modifier_deform_vertsEM(ModifierData *md,
                                 const struct ModifierEvalContext *ctx,
                                 struct BMEditMesh *em,
                                 struct Mesh *me,
                                 float (*vertexCos)[3],
                                 int numVerts);

/**
 * Get evaluated mesh for other evaluated object, which is used as an operand for the modifier,
 * e.g. second operand for boolean modifier.
 * Note that modifiers in stack always get fully evaluated COW ID pointers,
 * never original ones. Makes things simpler.
 *
 * \param get_cage_mesh: Return evaluated mesh with only deforming modifiers applied
 * (i.e. mesh topology remains the same as original one, a.k.a. 'cage' mesh).
 */
struct Mesh *BKE_modifier_get_evaluated_mesh_from_evaluated_object(struct Object *ob_eval,
                                                                   bool get_cage_mesh);

void BKE_modifier_check_uuids_unique_and_report(const struct Object *object);

void BKE_modifier_blend_write(struct BlendWriter *writer,
                              const struct ID *id_owner,
                              struct ListBase *modbase);
void BKE_modifier_blend_read_data(struct BlendDataReader *reader,
                                  struct ListBase *lb,
                                  struct Object *ob);
void BKE_modifier_blend_read_lib(struct BlendLibReader *reader, struct Object *ob);

#ifdef __cplusplus
}
#endif
