/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */
#include "BLI_compiler_attrs.h"
#include "BLI_enum_flags.hh"
#include "BLI_function_ref.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_span.hh"

#include "BKE_lib_query.hh" /* For LibraryForeachIDCallbackFlag. */

#include "DNA_modifier_types.h" /* Needed for all enum type definitions. */

#include "DNA_customdata_types.h"

namespace blender::bke {
struct GeometrySet;
}
struct ARegionType;
struct bArmature;
struct BMEditMesh;
struct BlendDataReader;
struct BlendWriter;
struct CustomData_MeshMasks;
struct DepsNodeHandle;
struct Depsgraph;
struct ID;
struct IDTypeForeachColorFunctionCallback;
struct ListBase;
struct Main;
struct Mesh;
struct ModifierData;
struct Object;
struct PointerRNA;
struct PropertyRNA;
struct Scene;
struct StructRNA;
struct IDCacheKey;

enum class ModifierTypeType {
  /** Should not be used, only for None modifier type. */
  None,

  /**
   * Modifier only does deformation, implies that modifier
   * type should have a valid deform_verts function. OnlyDeform
   * style modifiers implicitly accept either mesh or CV
   * input but should still declare flags appropriately.
   */
  OnlyDeform,

  /** Modifier adds geometry. */
  Constructive,
  /** Modifier can add and remove geometry. */
  Nonconstructive,

  /**
   * Both deform_verts & applyModifier are valid calls
   * used for particles modifier that doesn't actually modify the object
   * unless it's a mesh and can be exploded -> curve can also emit particles
   */
  DeformOrConstruct,

  /**
   * Like Nonconstructive, but does not affect the geometry
   * of the object, rather some of its CustomData layers.
   * E.g. UVProject and WeightVG modifiers. */
  NonGeometrical,
};

enum ModifierTypeFlag {
  eModifierTypeFlag_AcceptsMesh = (1 << 0),
  eModifierTypeFlag_AcceptsCVs = (1 << 1),
  /**
   * Modifiers that enable this flag can have the modifiers "On Cage" option toggled,
   * see: #eModifierMode_OnCage, where the output of the modifier can be selected directly.
   * In some cases the cage geometry use read to tool code as well (loop-cut & knife are examples).
   *
   * When set, geometry from the resulting mesh can be mapped back to the original indices
   * via #CD_ORIGINDEX.
   *
   * While many modifiers using this flag preserve the order of geometry arrays,
   * this isn't always the case, this flag doesn't imply #ModifierTypeType::OnlyDeform.
   * Geometry from the original mesh may be removed from the resulting mesh or new geometry
   * may be added (where the #CD_ORIGINDEX value will be #ORIGINDEX_NONE).
   *
   * Modifiers that create entirely new geometry from the input should not enable this flag
   * because none of the geometry will be selectable when "On Cage" is enabled.
   */
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
   * be placed after any non-deforming modifier.
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

  eModifierTypeFlag_AcceptsVertexCosOnly = (1 << 10),

  /** Accepts #BMesh input (without conversion). */
  eModifierTypeFlag_AcceptsBMesh = (1 << 11),

  /** Accepts #GreasePencil data input. */
  eModifierTypeFlag_AcceptsGreasePencil = (1 << 12),
};
ENUM_OPERATORS(ModifierTypeFlag)

using IDWalkFunc = void (*)(void *user_data,
                            Object *ob,
                            ID **idpoin,
                            LibraryForeachIDCallbackFlag cb_flag);
using TexWalkFunc = void (*)(void *user_data,
                             Object *ob,
                             ModifierData *md,
                             const PointerRNA *ptr,
                             PropertyRNA *texture_prop);

enum ModifierApplyFlag {
  /** Render time. */
  MOD_APPLY_RENDER = 1 << 0,
  /**
   * Result of evaluation will be cached, so modifier might
   * want to cache data for quick updates (used by subdivision-surface).
   */
  MOD_APPLY_USECACHE = 1 << 1,
  /** Modifier evaluated for undeformed texture coordinates */
  MOD_APPLY_ORCO = 1 << 2,
  /**
   * Ignore scene simplification flag and use subdivisions
   * level set in multires modifier.
   */
  MOD_APPLY_IGNORE_SIMPLIFY = 1 << 3,
  /**
   * The effect of this modifier will be applied to the original geometry
   * The modifier itself will be removed from the modifier stack.
   * This flag can be checked to ignore rendering display data to the mesh.
   * See `OBJECT_OT_modifier_apply` operator.
   */
  MOD_APPLY_TO_ORIGINAL = 1 << 4,
};
ENUM_OPERATORS(ModifierApplyFlag);

struct ModifierUpdateDepsgraphContext {
  Scene *scene;
  Object *object;
  DepsNodeHandle *node;
};

/**
 * Contains the information for deformXXX and applyXXX functions below that
 * doesn't change between consecutive modifiers.
 */
struct ModifierEvalContext {
  Depsgraph *depsgraph;
  Object *object;
  ModifierApplyFlag flag;
};

struct ModifierTypeInfo {
  /**
   * A unique identifier for this modifier. Used to generate the panel id type name.
   * See #BKE_modifier_type_panel_id.
   */
  char idname[64];

  /** The user visible name for this modifier. */
  char name[64];

  /**
   * The DNA struct name for the modifier data type,
   * used to write the DNA data out.
   */
  char struct_name[64];

  /** The size of the modifier data type, used by allocation. */
  int struct_size;

  /** StructRNA of this modifier. This is typically something like `RNA_*Modifier`. */
  StructRNA *srna;

  ModifierTypeType type;
  ModifierTypeFlag flags;

  /** Icon of the modifier. Usually something like ICON_MOD_*. */
  int icon;

  /********************* Non-optional functions *********************/

  /**
   * Copy instance data for this modifier type. Should copy all user
   * level settings to the target modifier.
   *
   * \param flag: Copying options (see BKE_lib_id.hh's LIB_ID_COPY_... flags for more).
   */
  void (*copy_data)(const ModifierData *md, ModifierData *target, int flag);

  /********************* Deform modifier functions *********************/

  /**
   * Apply a deformation to the positions in the \a positions array. If the \a mesh argument is
   * non-null, if will contain proper (not wrapped) mesh data. The \a positions array may or may
   * not be the same as the mesh's position attribute.
   */
  void (*deform_verts)(ModifierData *md,
                       const ModifierEvalContext *ctx,
                       Mesh *mesh,
                       blender::MutableSpan<blender::float3> positions);

  /**
   * Like deform_matrices_EM but called from object mode (for supporting modifiers in sculpt mode).
   */
  void (*deform_matrices)(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          Mesh *mesh,
                          blender::MutableSpan<blender::float3> positions,
                          blender::MutableSpan<blender::float3x3> matrices);
  /**
   * Like deform_verts but called during edit-mode if supported. The \a mesh argument might be a
   * wrapper around edit BMesh data.
   */
  void (*deform_verts_EM)(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          const BMEditMesh *em,
                          Mesh *mesh,
                          blender::MutableSpan<blender::float3> positions);

  /** Set deform matrix per vertex for crazy-space correction. */
  void (*deform_matrices_EM)(ModifierData *md,
                             const ModifierEvalContext *ctx,
                             const BMEditMesh *em,
                             Mesh *mesh,
                             blender::MutableSpan<blender::float3> positions,
                             blender::MutableSpan<blender::float3x3> matrices);

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
  Mesh *(*modify_mesh)(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh);

  /**
   * The modifier has to change the geometry set in-place. The geometry set can contain zero or
   * more geometry components. This callback can be used by modifiers that don't work on any
   * specific type of geometry (e.g. mesh).
   */
  void (*modify_geometry_set)(ModifierData *md,
                              const ModifierEvalContext *ctx,
                              blender::bke::GeometrySet *geometry_set);

  /********************* Optional functions *********************/

  /**
   * Initialize new instance data for this modifier type, this function
   * should set modifier variables to their default values.
   *
   * This function is optional.
   */
  void (*init_data)(ModifierData *md);

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
  void (*required_data_mask)(ModifierData *md, CustomData_MeshMasks *r_cddata_masks);

  /**
   * Free internal modifier data variables, this function should
   * not free the md variable itself.
   *
   * This function is responsible for freeing the runtime data as well.
   *
   * This function is optional.
   */
  void (*free_data)(ModifierData *md);

  /**
   * Return a boolean value indicating if this modifier is able to be
   * calculated based on the modifier data. This is *not* regarding the
   * md->flag, that is tested by the system, this is just if the data
   * validates (for example, a lattice will return false if the lattice
   * object is not defined).
   *
   * This function is optional (assumes never disabled if not present).
   */
  bool (*is_disabled)(const Scene *scene, ModifierData *md, bool use_render_params);

  /**
   * Add the appropriate relations to the dependency graph.
   *
   * This function is optional.
   */
  void (*update_depsgraph)(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx);

  /**
   * Should return true if the modifier needs to be recalculated on time
   * changes.
   *
   * This function is optional (assumes false if not present).
   */
  bool (*depends_on_time)(Scene *scene, ModifierData *md);

  /**
   * Returns true when a deform modifier uses mesh normals as input. This callback is only required
   * for deform modifiers that support deforming positions with an edit mesh (when #deform_verts_EM
   * is implemented).
   */
  bool (*depends_on_normals)(ModifierData *md);

  /**
   * Should call the given walk function with a pointer to each ID
   * pointer (i.e. each data-block pointer) that the modifier data
   * stores. This is used for linking on file load and for
   * unlinking data-blocks or forwarding data-block references.
   *
   * This function is optional.
   */
  void (*foreach_ID_link)(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data);

  /**
   * Should call the given walk function for each texture that the
   * modifier data stores. This is used for finding all textures in
   * the context for the UI.
   *
   * This function is optional. If it is not present, it will be
   * assumed the modifier has no textures.
   */
  void (*foreach_tex_link)(ModifierData *md, Object *ob, TexWalkFunc walk, void *user_data);

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
  void (*free_runtime_data)(void *runtime_data);

  /** Register the panel types for the modifier's UI. */
  void (*panel_register)(ARegionType *region_type);

  /**
   * Is called when the modifier is written to a file. The modifier data struct itself is written
   * already.
   *
   * This method should write any additional arrays and referenced structs that should be
   * stored in the file.
   */
  void (*blend_write)(BlendWriter *writer, const ID *id_owner, const ModifierData *md);

  /**
   * Is called when the modifier is read from a file.
   *
   * It can be used to update pointers to arrays and other structs. Furthermore, fields that have
   * not been written (e.g. runtime data) can be reset.
   */
  void (*blend_read)(BlendDataReader *reader, ModifierData *md);

  /**
   * Iterate over all cache pointers of given modifier. Also see #IDTypeInfo::foreach_cache.
   */
  void (*foreach_cache)(
      Object *object,
      ModifierData *md,
      blender::FunctionRef<void(const IDCacheKey &cache_key, void **cache_p, uint flags)> fn);

  /**
   * Iterate over all working space colors.
   */
  void (*foreach_working_space_color)(ModifierData *md,
                                      const IDTypeForeachColorFunctionCallback &fn);
};

/** Used to set a modifier's panel type. */
#define MODIFIER_TYPE_PANEL_PREFIX "MOD_PT_"

/** Initialize modifier's global data (type info and some common global storage). */
void BKE_modifier_init();

const ModifierTypeInfo *BKE_modifier_get_info(ModifierType type);

/* For modifier UI panels. */

/**
 * Get the idname of the modifier type's panel, which was defined in the #panel_register callback.
 */
void BKE_modifier_type_panel_id(ModifierType type, char *r_idname);
void BKE_modifier_panel_expand(ModifierData *md);

/**
 * Modifier utility calls, do call through type pointer and return
 * default values if pointer is optional.
 */
ModifierData *BKE_modifier_new(int type);

void BKE_modifier_free_ex(ModifierData *md, int flag);
void BKE_modifier_free(ModifierData *md);
/**
 * Use instead of `BLI_remlink` when the object's active modifier should change.
 */
void BKE_modifier_remove_from_list(Object *ob, ModifierData *md);

void BKE_modifier_unique_name(ListBase *modifiers, ModifierData *md);

ModifierData *BKE_modifier_copy_ex(const ModifierData *md, int flag);

/**
 * Callback's can use this to avoid copying every member.
 */
void BKE_modifier_copydata_generic(const ModifierData *md, ModifierData *md_dst, int flag);
void BKE_modifier_copydata(const ModifierData *md, ModifierData *target);
void BKE_modifier_copydata_ex(const ModifierData *md, ModifierData *target, int flag);
bool BKE_modifier_depends_ontime(Scene *scene, ModifierData *md);
bool BKE_modifier_supports_mapping(ModifierData *md);
bool BKE_modifier_supports_cage(Scene *scene, ModifierData *md);
bool BKE_modifier_couldbe_cage(Scene *scene, ModifierData *md);
bool BKE_modifier_is_correctable_deformed(ModifierData *md);
bool BKE_modifier_is_same_topology(ModifierData *md);
bool BKE_modifier_is_non_geometrical(ModifierData *md);
/**
 * Check whether is enabled.
 *
 * \param scene: Current scene, may be NULL,
 * in which case `is_disabled` callback of the modifier is never called.
 */
bool BKE_modifier_is_enabled(const Scene *scene, ModifierData *md, int required_mode);
/**
 * Check whether given modifier is not local (i.e. from linked data) when the object is a library
 * override.
 *
 * \param md: May be NULL, in which case we consider it as a non-local modifier case.
 */
bool BKE_modifier_is_nonlocal_in_liboverride(const Object *ob, const ModifierData *md);

/**
 * Set modifier execution error.
 * The message will be shown in the interface and will be logged as an error to the console.
 */
void BKE_modifier_set_error(const Object *ob, ModifierData *md, const char *format, ...)
    ATTR_PRINTF_FORMAT(3, 4);

/**
 * Set modifier execution warning, which does not prevent the modifier from being applied but which
 * might need an attention. The message will only be shown in the interface,
 * but will not appear in the logs.
 */
void BKE_modifier_set_warning(const Object *ob, ModifierData *md, const char *format, ...)
    ATTR_PRINTF_FORMAT(3, 4);

void BKE_modifiers_foreach_ID_link(Object *ob, IDWalkFunc walk, void *user_data);
void BKE_modifiers_foreach_tex_link(Object *ob, TexWalkFunc walk, void *user_data);

ModifierData *BKE_modifiers_findby_type(const Object *ob, ModifierType type);
ModifierData *BKE_modifiers_findby_name(const Object *ob, const char *name);
ModifierData *BKE_modifiers_findby_persistent_uid(const Object *ob, int persistent_uid);

void BKE_modifiers_clear_errors(Object *ob);

/**
 * Updates `md.persistent_uid` so that it is a valid identifier (>=1) and is unique in the object.
 */
void BKE_modifiers_persistent_uid_init(const Object &object, ModifierData &md);
/**
 * Returns true when all the modifier identifiers are positive and unique. This should generally be
 * true and should only be used by asserts.
 */
bool BKE_modifiers_persistent_uids_are_valid(const Object &object);

/**
 * used for buttons, to find out if the 'draw deformed in edit-mode option is there.
 *
 * Also used in transform_conversion.c, to detect crazy-space (2nd arg then is NULL).
 * Also used for some mesh tools to give warnings.
 */
int BKE_modifiers_get_cage_index(const Scene *scene,
                                 Object *ob,
                                 int *r_lastPossibleCageIndex,
                                 bool is_virtual);

/**
 * Takes an object and returns its first selected armature, else just its armature.
 * This should work for multiple armatures per object.
 */
Object *BKE_modifiers_is_deformed_by_armature(Object *ob);
Object *BKE_modifiers_is_deformed_by_meshdeform(Object *ob);
/**
 * Takes an object and returns its first selected lattice, else just its lattice.
 * This should work for multiple lattices per object.
 */
Object *BKE_modifiers_is_deformed_by_lattice(Object *ob);
/**
 * Takes an object and returns its first selected curve, else just its curve.
 * This should work for multiple curves per object.
 */
Object *BKE_modifiers_is_deformed_by_curve(Object *ob);
bool BKE_modifiers_uses_multires(Object *ob);
bool BKE_modifiers_uses_armature(Object *ob, bArmature *arm);
bool BKE_modifiers_is_correctable_deformed(const Scene *scene, Object *ob);
void BKE_modifier_free_temporary_data(ModifierData *md);

struct CDMaskLink {
  CDMaskLink *next;
  CustomData_MeshMasks mask;
};

/**
 * Calculates and returns a linked list of CustomData_MeshMasks and modified
 * final datamask, indicating the data required by each modifier in the stack
 * pointed to by md for correct evaluation, assuming the data indicated by
 * final_datamask is required at the end of the stack.
 */
CDMaskLink *BKE_modifier_calc_data_masks(const Scene *scene,
                                         ModifierData *md,
                                         CustomData_MeshMasks *final_datamask,
                                         int required_mode);

struct VirtualModifierData {
  ArmatureModifierData amd;
  CurveModifierData cmd;
  LatticeModifierData lmd;
  ShapeKeyModifierData smd;
};

/**
 * This is to include things that are not modifiers in the evaluation of the modifier stack,
 * for example parenting to an armature.
 */
ModifierData *BKE_modifiers_get_virtual_modifierlist(const Object *ob, VirtualModifierData *data);

/**
 * Ensure modifier correctness when changing `ob->data`.
 */
void BKE_modifiers_test_object(Object *ob);

/**
 * Here for #do_versions.
 */
void BKE_modifier_mdef_compact_influences(ModifierData *md);

/**
 * Initializes `path` with either the blend file or temporary directory.
 */
void BKE_modifier_path_init(char *path, int path_maxncpy, const char *name);
const char *BKE_modifier_path_relbase(Main *bmain, Object *ob);
const char *BKE_modifier_path_relbase_from_global(Object *ob);

/* Accessors of original/evaluated modifiers. */

/**
 * For a given modifier data, get corresponding original one.
 * If the modifier data is already original, return it as-is.
 */
ModifierData *BKE_modifier_get_original(const Object *object, ModifierData *md);
ModifierData *BKE_modifier_get_evaluated(Depsgraph *depsgraph, Object *object, ModifierData *md);

/* wrappers for modifier callbacks that ensure valid normals */

Mesh *BKE_modifier_modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh);

/**
 * \return False if the modifier did not support deforming the positions.
 */
bool BKE_modifier_deform_verts(ModifierData *md,
                               const ModifierEvalContext *ctx,
                               Mesh *mesh,
                               blender::MutableSpan<blender::float3> positions);

void BKE_modifier_deform_vertsEM(ModifierData *md,
                                 const ModifierEvalContext *ctx,
                                 const BMEditMesh *em,
                                 Mesh *mesh,
                                 blender::MutableSpan<blender::float3> positions);

/**
 * Get evaluated mesh for other evaluated object, which is used as an operand for the modifier,
 * e.g. second operand for boolean modifier.
 * Note that modifiers in stack always get fully evaluated ID pointers,
 * never original ones. Makes things simpler.
 */
Mesh *BKE_modifier_get_evaluated_mesh_from_evaluated_object(Object *ob_eval);

void BKE_modifier_blend_write(BlendWriter *writer, const ID *id_owner, ListBase *modbase);
void BKE_modifier_blend_read_data(BlendDataReader *reader, ListBase *lb, Object *ob);

namespace blender::bke {

/**
 * A convenience class that can be used to set `ModifierData::execution_time` based on the lifetime
 * of this class.
 */
class ScopedModifierTimer {
 private:
  ModifierData &md_;
  double start_time_;

 public:
  ScopedModifierTimer(ModifierData &md);
  ~ScopedModifierTimer();
};

}  // namespace blender::bke
