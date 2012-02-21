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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
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

#include "DNA_modifier_types.h"		/* needed for all enum typdefs */
#include "BKE_customdata.h"

struct ID;
struct EditMesh;
struct DerivedMesh;
struct DagForest;
struct DagNode;
struct Object;
struct Scene;
struct ListBase;
struct LinkNode;
struct bArmature;
struct ModifierData;
struct BMEditMesh;

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
	eModifierTypeFlag_AcceptsMesh          = (1<<0),
	eModifierTypeFlag_AcceptsCVs           = (1<<1),
	eModifierTypeFlag_SupportsMapping      = (1<<2),
	eModifierTypeFlag_SupportsEditmode     = (1<<3),

	/* For modifiers that support editmode this determines if the
	 * modifier should be enabled by default in editmode. This should
	 * only be used by modifiers that are relatively speedy and
	 * also generally used in editmode, otherwise let the user enable
	 * it by hand.
	 */
	eModifierTypeFlag_EnableInEditmode     = (1<<4),

	/* For modifiers that require original data and so cannot
	 * be placed after any non-deformative modifier.
	 */
	eModifierTypeFlag_RequiresOriginalData = (1<<5),

	/* For modifiers that support pointcache, so we can check to see if it has files we need to deal with
	*/
	eModifierTypeFlag_UsesPointCache = (1<<6),

	/* For physics modifiers, max one per type */
	eModifierTypeFlag_Single = (1<<7),

	/* Some modifier can't be added manually by user */
	eModifierTypeFlag_NoUserAdd = (1<<8),

	/* For modifiers that use CD_WEIGHT_MCOL for preview. */
	eModifierTypeFlag_UsesPreview = (1<<9)
} ModifierTypeFlag;

typedef void (*ObjectWalkFunc)(void *userData, struct Object *ob, struct Object **obpoin);
typedef void (*IDWalkFunc)(void *userData, struct Object *ob, struct ID **idpoin);
typedef void (*TexWalkFunc)(void *userData, struct Object *ob, struct ModifierData *md, const char *propname);

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
						int useRenderParams, int isFinalCalc);

	/* Like deformMatricesEM but called from object mode (for supporting modifiers in sculpt mode) */
	void (*deformMatrices)(
				struct ModifierData *md, struct Object *ob,
				struct DerivedMesh *derivedData,
				float (*vertexCos)[3], float (*defMats)[3][3], int numVerts);

	/* Like deformVerts but called during editmode (for supporting modifiers)
	 */
	void (*deformVertsEM)(
				struct ModifierData *md, struct Object *ob,
				struct BMEditMesh *editData, struct DerivedMesh *derivedData,
				float (*vertexCos)[3], int numVerts);

	/* Set deform matrix per vertex for crazyspace correction */
	void (*deformMatricesEM)(
				struct ModifierData *md, struct Object *ob,
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
	struct DerivedMesh *(*applyModifier)(
								struct ModifierData *md, struct Object *ob,
								struct DerivedMesh *derivedData,
								int useRenderParams, int isFinalCalc);

	/* Like applyModifier but called during editmode (for supporting
	 * modifiers).
	 * 
	 * The derived object that is returned must support the operations that
	 * are expected from editmode objects. The same qualifications regarding
	 * derivedData apply as for applyModifier.
	 */
	struct DerivedMesh *(*applyModifierEM)(
								struct ModifierData *md, struct Object *ob,
								struct BMEditMesh *editData,
								struct DerivedMesh *derivedData);


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
	int (*isDisabled)(struct ModifierData *md, int userRenderParams);

	/* Add the appropriate relations to the DEP graph depending on the
	 * modifier data. 
	 *
	 * This function is optional.
	 */
	void (*updateDepgraph)(struct ModifierData *md, struct DagForest *forest, struct Scene *scene,
						   struct Object *ob, struct DagNode *obNode);

	/* Should return true if the modifier needs to be recalculated on time
	 * changes.
	 *
	 * This function is optional (assumes false if not present).
	 */
	int (*dependsOnTime)(struct ModifierData *md);


	/* True when a deform modifier uses normals, the requiredDataMask
	 * cant be used here because that refers to a normal layer where as
	 * in this case we need to know if the deform modifier uses normals.
	 * 
	 * this is needed because applying 2 deform modifiers will give the
	 * second modifier bogus normals.
	 * */
	int (*dependsOnNormals)(struct ModifierData *md);


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

ModifierTypeInfo *modifierType_getInfo (ModifierType type);

/* Modifier utility calls, do call through type pointer and return
 * default values if pointer is optional.
 */
struct ModifierData  *modifier_new(int type);
void          modifier_free(struct ModifierData *md);

void 		  modifier_unique_name(struct ListBase *modifiers, struct ModifierData *md);

void          modifier_copyData(struct ModifierData *md, struct ModifierData *target);
int           modifier_dependsOnTime(struct ModifierData *md);
int           modifier_supportsMapping(struct ModifierData *md);
int           modifier_couldBeCage(struct Scene *scene, struct ModifierData *md);
int           modifier_isCorrectableDeformed(struct ModifierData *md);
int			  modifier_sameTopology(ModifierData *md);
int           modifier_nonGeometrical(ModifierData *md);
int           modifier_isEnabled(struct Scene *scene, struct ModifierData *md, int required_mode);
void          modifier_setError(struct ModifierData *md, const char *format, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 2, 3)))
#endif
;
int           modifier_isPreview(struct ModifierData *md);

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
									 int *lastPossibleCageIndex_r, int virtual_);

int           modifiers_isSoftbodyEnabled(struct Object *ob);
int           modifiers_isClothEnabled(struct Object *ob);
int           modifiers_isParticleEnabled(struct Object *ob);

struct Object *modifiers_isDeformedByArmature(struct Object *ob);
struct Object *modifiers_isDeformedByLattice(struct Object *ob);
int           modifiers_usesArmature(struct Object *ob, struct bArmature *arm);
int           modifiers_isCorrectableDeformed(struct Object *ob);
void          modifier_freeTemporaryData(struct ModifierData *md);
int           modifiers_isPreview(struct Object *ob);

int           modifiers_indexInObject(struct Object *ob, struct ModifierData *md);

/* Calculates and returns a linked list of CustomDataMasks indicating the
 * data required by each modifier in the stack pointed to by md for correct
 * evaluation, assuming the data indicated by dataMask is required at the
 * end of the stack.
 */
struct LinkNode *modifiers_calcDataMasks(struct Scene *scene, 
										 struct Object *ob,
										 struct ModifierData *md,
										 CustomDataMask dataMask,
										 int required_mode);
struct ModifierData *modifiers_getLastPreview(struct Scene *scene,
                                              struct ModifierData *md,
                                              int required_mode);
struct ModifierData  *modifiers_getVirtualModifierList(struct Object *ob);

/* ensure modifier correctness when changing ob->data */
void test_object_modifiers(struct Object *ob);

/* here for do_versions */
void modifier_mdef_compact_influences(struct ModifierData *md);

void        modifier_path_init(char *path, int path_maxlen, const char *name);
const char *modifier_path_relbase(struct Object *ob);

#endif

