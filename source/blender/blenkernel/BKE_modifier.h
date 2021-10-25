/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_MODIFIER_H__
#define __BKE_MODIFIER_H__

/** \file BKE_modifier.h
 *  \ingroup bke
 */

#include "DNA_modifier_types.h"     /* needed for all enum typdefs */
#include "BLI_compiler_attrs.h"
#include "BKE_customdata.h"

struct ID;
struct DerivedMesh;
struct DagForest;
struct DagNode;
struct Object;
struct Scene;
struct ListBase;
struct bArmature;
struct Main;
struct ModifierData;
struct BMEditMesh;
struct DepsNodeHandle;

typedef enum {
	/* Should not be used, only for None modifier type */
	eModifierTypeType_None,

	/* Modifier only does deformation, implies that modifier
	 * type should have a valid deformVerts function. OnlyDeform
	 * style modifiers implicitly accept either mesh or CV
	 * input but should still declare flags appropriately.
	 */
	eModifierTypeType_OnlyDeform,

	eModifierTypeType_Constructive,
	eModifierTypeType_Nonconstructive,

	/* both deformVerts & applyModifier are valid calls
	 * used for particles modifier that doesn't actually modify the object
	 * unless it's a mesh and can be exploded -> curve can also emit particles
	 */
	eModifierTypeType_DeformOrConstruct,

	/* Like eModifierTypeType_Nonconstructive, but does not affect the geometry
	 * of the object, rather some of its CustomData layers.
	 * E.g. UVProject and WeightVG modifiers. */
	eModifierTypeType_NonGeometrical,
} ModifierTypeType;

typedef enum {
	eModifierTypeFlag_AcceptsMesh          = (1 << 0),
	eModifierTypeFlag_AcceptsCVs           = (1 << 1),
	eModifierTypeFlag_SupportsMapping      = (1 << 2),
	eModifierTypeFlag_SupportsEditmode     = (1 << 3),

	/* For modifiers that support editmode this determines if the
	 * modifier should be enabled by default in editmode. This should
	 * only be used by modifiers that are relatively speedy and
	 * also generally used in editmode, otherwise let the user enable
	 * it by hand.
	 */
	eModifierTypeFlag_EnableInEditmode     = (1 << 4),

	/* For modifiers that require original data and so cannot
	 * be placed after any non-deformative modifier.
	 */
	eModifierTypeFlag_RequiresOriginalData = (1 << 5),

	/* For modifiers that support pointcache, so we can check to see if it has files we need to deal with
	 */
	eModifierTypeFlag_UsesPointCache = (1 << 6),

	/* For physics modifiers, max one per type */
	eModifierTypeFlag_Single = (1 << 7),

	/* Some modifier can't be added manually by user */
	eModifierTypeFlag_NoUserAdd = (1 << 8),

	/* For modifiers that use CD_PREVIEW_MCOL for preview. */
	eModifierTypeFlag_UsesPreview = (1 << 9),
	eModifierTypeFlag_AcceptsLattice = (1 << 10),
} ModifierTypeFlag;

/* IMPORTANT! Keep ObjectWalkFunc and IDWalkFunc signatures compatible. */
typedef void (*ObjectWalkFunc)(void *userData, struct Object *ob, struct Object **obpoin, int cb_flag);
typedef void (*IDWalkFunc)(void *userData, struct Object *ob, struct ID **idpoin, int cb_flag);
typedef void (*TexWalkFunc)(void *userData, struct Object *ob, struct ModifierData *md, const char *propname);

typedef enum ModifierApplyFlag {
	MOD_APPLY_RENDER = 1 << 0,       /* Render time. */
	MOD_APPLY_USECACHE = 1 << 1,     /* Result of evaluation will be cached, so modifier might
	                                  * want to cache data for quick updates (used by subsurf) */
	MOD_APPLY_ORCO = 1 << 2,         /* Modifier evaluated for undeformed texture coordinates */
	MOD_APPLY_IGNORE_SIMPLIFY = 1 << 3, /* Ignore scene simplification flag and use subdivisions
	                                     * level set in multires modifier.
	                                     */
	MOD_APPLY_ALLOW_GPU = 1 << 4,  /* Allow modifier to be applied and stored in the GPU.
	                                * Used by the viewport in order to be able to have SS
	                                * happening on GPU.
	                                * Render pipeline (including viewport render) should
	                                * have DM on the CPU.
	                                */
} ModifierApplyFlag;


typedef struct ModifierTypeInfo {
	/* The user visible name for this modifier */
	char name[32];

	/* The DNA struct name for the modifier data type, used to
	 * write the DNA data out.
	 */
	char structName[32];

	/* The size of the modifier data type, used by allocation. */
	int structSize;

	ModifierTypeType type;
	ModifierTypeFlag flags;


	/********************* Non-optional functions *********************/

	/* Copy instance data for this modifier type. Should copy all user
	 * level settings to the target modifier.
	 */
	void (*copyData)(struct ModifierData *md, struct ModifierData *target);

	/********************* Deform modifier functions *********************/

	/* Only for deform types, should apply the deformation
	 * to the given vertex array. If the deformer requires information from
	 * the object it can obtain it from the derivedData argument if non-NULL,
	 * and otherwise the ob argument.
	 */
	void (*deformVerts)(struct ModifierData *md, struct Object *ob,
	                    struct DerivedMesh *derivedData,
	                    float (*vertexCos)[3], int numVerts,
	                    ModifierApplyFlag flag);

	/* Like deformMatricesEM but called from object mode (for supporting modifiers in sculpt mode) */
	void (*deformMatrices)(struct ModifierData *md, struct Object *ob,
	                       struct DerivedMesh *derivedData,
	                       float (*vertexCos)[3], float (*defMats)[3][3], int numVerts);

	/* Like deformVerts but called during editmode (for supporting modifiers)
	 */
	void (*deformVertsEM)(struct ModifierData *md, struct Object *ob,
	                      struct BMEditMesh *editData, struct DerivedMesh *derivedData,
	                      float (*vertexCos)[3], int numVerts);

	/* Set deform matrix per vertex for crazyspace correction */
	void (*deformMatricesEM)(struct ModifierData *md, struct Object *ob,
	                         struct BMEditMesh *editData, struct DerivedMesh *derivedData,
	                         float (*vertexCos)[3], float (*defMats)[3][3], int numVerts);

	/********************* Non-deform modifier functions *********************/

	/* For non-deform types: apply the modifier and return a derived
	 * data object (type is dependent on object type).
	 *
	 * The derivedData argument should always be non-NULL; the modifier
	 * should read the object data from the derived object instead of the
	 * actual object data. 
	 *
	 * The useRenderParams argument indicates if the modifier is being
	 * applied in the service of the renderer which may alter quality
	 * settings.
	 *
	 * The isFinalCalc parameter indicates if the modifier is being
	 * calculated for a final result or for something temporary
	 * (like orcos). This is a hack at the moment, it is meant so subsurf
	 * can know if it is safe to reuse its internal cache.
	 *
	 * The modifier may reuse the derivedData argument (i.e. return it in
	 * modified form), but must not release it.
	 */
	struct DerivedMesh *(*applyModifier)(struct ModifierData *md, struct Object *ob,
	                                     struct DerivedMesh *derivedData,
	                                     ModifierApplyFlag flag);

	/* Like applyModifier but called during editmode (for supporting
	 * modifiers).
	 * 
	 * The derived object that is returned must support the operations that
	 * are expected from editmode objects. The same qualifications regarding
	 * derivedData apply as for applyModifier.
	 */
	struct DerivedMesh *(*applyModifierEM)(struct ModifierData *md, struct Object *ob,
	                                       struct BMEditMesh *editData,
	                                       struct DerivedMesh *derivedData,
	                                       ModifierApplyFlag flag);


	/********************* Optional functions *********************/

	/* Initialize new instance data for this modifier type, this function
	 * should set modifier variables to their default values.
	 * 
	 * This function is optional.
	 */
	void (*initData)(struct ModifierData *md);

	/* Should return a CustomDataMask indicating what data this
	 * modifier needs. If (mask & (1 << (layer type))) != 0, this modifier
	 * needs that custom data layer. This function's return value can change
	 * depending on the modifier's settings.
	 *
	 * Note that this means extra data (e.g. vertex groups) - it is assumed
	 * that all modifiers need mesh data and deform modifiers need vertex
	 * coordinates.
	 *
	 * Note that this limits the number of custom data layer types to 32.
	 *
	 * If this function is not present or it returns 0, it is assumed that
	 * no extra data is needed.
	 *
	 * This function is optional.
	 */
	CustomDataMask (*requiredDataMask)(struct Object *ob, struct ModifierData *md);

	/* Free internal modifier data variables, this function should
	 * not free the md variable itself.
	 *
	 * This function is optional.
	 */
	void (*freeData)(struct ModifierData *md);

	/* Return a boolean value indicating if this modifier is able to be
	 * calculated based on the modifier data. This is *not* regarding the
	 * md->flag, that is tested by the system, this is just if the data
	 * validates (for example, a lattice will return false if the lattice
	 * object is not defined).
	 *
	 * This function is optional (assumes never disabled if not present).
	 */
	bool (*isDisabled)(struct ModifierData *md, int userRenderParams);

	/* Add the appropriate relations to the DEP graph depending on the
	 * modifier data. 
	 *
	 * This function is optional.
	 */
	void (*updateDepgraph)(struct ModifierData *md, struct DagForest *forest,
	                       struct Main *bmain, struct Scene *scene,
	                       struct Object *ob, struct DagNode *obNode);

	/* Add the appropriate relations to the dependency graph.
	 *
	 * This function is optional.
	 */
	/* TODO(sergey): Remove once we finally switched to the new depsgraph. */
	void (*updateDepsgraph)(struct ModifierData *md,
	                        struct Main *bmain,
	                        struct Scene *scene,
	                        struct Object *ob,
	                        struct DepsNodeHandle *node);

	/* Should return true if the modifier needs to be recalculated on time
	 * changes.
	 *
	 * This function is optional (assumes false if not present).
	 */
	bool (*dependsOnTime)(struct ModifierData *md);


	/* True when a deform modifier uses normals, the requiredDataMask
	 * cant be used here because that refers to a normal layer where as
	 * in this case we need to know if the deform modifier uses normals.
	 * 
	 * this is needed because applying 2 deform modifiers will give the
	 * second modifier bogus normals.
	 * */
	bool (*dependsOnNormals)(struct ModifierData *md);


	/* Should call the given walk function on with a pointer to each Object
	 * pointer that the modifier data stores. This is used for linking on file
	 * load and for unlinking objects or forwarding object references.
	 *
	 * This function is optional.
	 */
	void (*foreachObjectLink)(struct ModifierData *md, struct Object *ob,
	                          ObjectWalkFunc walk, void *userData);

	/* Should call the given walk function with a pointer to each ID
	 * pointer (i.e. each datablock pointer) that the modifier data
	 * stores. This is used for linking on file load and for
	 * unlinking datablocks or forwarding datablock references.
	 *
	 * This function is optional. If it is not present, foreachObjectLink
	 * will be used.
	 */
	void (*foreachIDLink)(struct ModifierData *md, struct Object *ob,
	                      IDWalkFunc walk, void *userData);

	/* Should call the given walk function for each texture that the
	 * modifier data stores. This is used for finding all textures in
	 * the context for the UI.
	 *
	 * This function is optional. If it is not present, it will be
	 * assumed the modifier has no textures.
	 */
	void (*foreachTexLink)(struct ModifierData *md, struct Object *ob,
	                       TexWalkFunc walk, void *userData);
} ModifierTypeInfo;

/* Initialize modifier's global data (type info and some common global storages). */
void BKE_modifier_init(void);

const ModifierTypeInfo *modifierType_getInfo(ModifierType type);

/* Modifier utility calls, do call through type pointer and return
 * default values if pointer is optional.
 */
struct ModifierData  *modifier_new(int type);
void          modifier_free(struct ModifierData *md);

bool          modifier_unique_name(struct ListBase *modifiers, struct ModifierData *md);

void          modifier_copyData_generic(const struct ModifierData *md, struct ModifierData *target);
void          modifier_copyData(struct ModifierData *md, struct ModifierData *target);
bool          modifier_dependsOnTime(struct ModifierData *md);
bool          modifier_supportsMapping(struct ModifierData *md);
bool          modifier_supportsCage(struct Scene *scene, struct ModifierData *md);
bool          modifier_couldBeCage(struct Scene *scene, struct ModifierData *md);
bool          modifier_isCorrectableDeformed(struct ModifierData *md);
bool          modifier_isSameTopology(ModifierData *md);
bool          modifier_isNonGeometrical(ModifierData *md);
bool          modifier_isEnabled(struct Scene *scene, struct ModifierData *md, int required_mode);
void          modifier_setError(struct ModifierData *md, const char *format, ...) ATTR_PRINTF_FORMAT(2, 3);
bool          modifier_isPreview(struct ModifierData *md);

void          modifiers_foreachObjectLink(struct Object *ob,
                                          ObjectWalkFunc walk,
                                          void *userData);
void          modifiers_foreachIDLink(struct Object *ob,
                                      IDWalkFunc walk,
                                      void *userData);
void          modifiers_foreachTexLink(struct Object *ob,
                                       TexWalkFunc walk,
                                       void *userData);

struct ModifierData  *modifiers_findByType(struct Object *ob, ModifierType type);
struct ModifierData  *modifiers_findByName(struct Object *ob, const char *name);
void          modifiers_clearErrors(struct Object *ob);
int           modifiers_getCageIndex(struct Scene *scene, struct Object *ob,
                                     int *r_lastPossibleCageIndex, bool is_virtual);

bool          modifiers_isModifierEnabled(struct Object *ob, int modifierType);
bool          modifiers_isSoftbodyEnabled(struct Object *ob);
bool          modifiers_isClothEnabled(struct Object *ob);
bool          modifiers_isParticleEnabled(struct Object *ob);

struct Object *modifiers_isDeformedByArmature(struct Object *ob);
struct Object *modifiers_isDeformedByLattice(struct Object *ob);
struct Object *modifiers_isDeformedByCurve(struct Object *ob);
bool          modifiers_usesArmature(struct Object *ob, struct bArmature *arm);
bool          modifiers_isCorrectableDeformed(struct Scene *scene, struct Object *ob);
void          modifier_freeTemporaryData(struct ModifierData *md);
bool          modifiers_isPreview(struct Object *ob);

typedef struct CDMaskLink {
	struct CDMaskLink *next;
	CustomDataMask mask;
} CDMaskLink;

/* Calculates and returns a linked list of CustomDataMasks indicating the
 * data required by each modifier in the stack pointed to by md for correct
 * evaluation, assuming the data indicated by dataMask is required at the
 * end of the stack.
 */
struct CDMaskLink *modifiers_calcDataMasks(struct Scene *scene,
                                           struct Object *ob,
                                           struct ModifierData *md,
                                           CustomDataMask dataMask,
                                           int required_mode,
                                           ModifierData *previewmd, CustomDataMask previewmask);
struct ModifierData *modifiers_getLastPreview(struct Scene *scene,
                                              struct ModifierData *md,
                                              int required_mode);

typedef struct VirtualModifierData {
	ArmatureModifierData amd;
	CurveModifierData cmd;
	LatticeModifierData lmd;
	ShapeKeyModifierData smd;
} VirtualModifierData;

struct ModifierData  *modifiers_getVirtualModifierList(struct Object *ob, struct VirtualModifierData *data);

/* ensure modifier correctness when changing ob->data */
void test_object_modifiers(struct Object *ob);

/* here for do_versions */
void modifier_mdef_compact_influences(struct ModifierData *md);

void        modifier_path_init(char *path, int path_maxlen, const char *name);
const char *modifier_path_relbase(struct Object *ob);


/* wrappers for modifier callbacks */

struct DerivedMesh *modwrap_applyModifier(
        ModifierData *md, struct Object *ob,
        struct DerivedMesh *dm,
        ModifierApplyFlag flag);

struct DerivedMesh *modwrap_applyModifierEM(
        ModifierData *md, struct Object *ob,
        struct BMEditMesh *em,
        struct DerivedMesh *dm,
        ModifierApplyFlag flag);

void modwrap_deformVerts(
        ModifierData *md, struct Object *ob,
        struct DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts,
        ModifierApplyFlag flag);

void modwrap_deformVertsEM(
        ModifierData *md, struct Object *ob,
        struct BMEditMesh *em, struct DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts);

#endif

