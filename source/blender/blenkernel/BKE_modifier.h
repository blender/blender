/**
 *	
 * $$ 
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef BKE_MODIFIER_H
#define BKE_MODIFIER_H

struct DerivedMesh;
struct ModifierData;
struct Object;

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
} ModifierTypeType;

typedef enum {
	eModifierTypeFlag_AcceptsMesh = (1<<0),
	eModifierTypeFlag_AcceptsCVs = (1<<1),
	eModifierTypeFlag_SupportsMapping = (1<<2),
	eModifierTypeFlag_RequiresObject = (1<<3),
} ModifierTypeFlag;

typedef struct ModifierTypeInfo {
	char name[32], structName[32];
	ModifierTypeType type;
	ModifierTypeFlag flags;

		/* Create new instance data for this modifier type.
		 * 
		 * This function must be present.
		 */
	struct ModifierData *(*allocData)(void);

		/* Return a boolean value indicating if this modifier is able to be calculated
		 * based on the modifier data. This is *not* regarding the md->flag, that is
		 * tested by the system, this is just if the data validates (for example, a
		 * lattice will return false if the lattice object is not defined).
		 *
		 * This function must be present.
		 */
	int (*isDisabled)(struct ModifierData *md);

		/* Only for deform types, should apply the deformation
		 * to the given vertex array. Object is guaranteed to be
		 * non-NULL.
		 */
	void (*deformVerts)(struct ModifierData *md, struct Object *ob, float (*vertexCos)[3], int numVerts);

		/* For non-deform types: apply the modifier and return a new derived
		 * data object (type is dependent on object type). If the _derivedData_
		 * argument is non-NULL then the modifier should read the object data 
		 * from the derived object instead of the _data_ object. 
		 *
		 * If the _vertexCos_ argument is non-NULL then the modifier should read 
		 * the vertex coordinates from that (even if _derivedData_ is non-NULL).
		 * The length of the _vertexCos_ array is either the number of verts in
		 * the derived object (if non-NULL) or otherwise the number of verts in
		 * the original object.
		 *
		 * The _useRenderParams_ indicates if the modifier is being applied in
		 * the service of the renderer which may alter quality settings.
		 *
		 * The modifier is expected to release (or reuse) the _derivedData_ argument
		 * if non-NULL. The modifier *MAY NOT* share the _vertexCos_ argument.
		 *
		 * It is possible for _ob_ to be NULL if the modifier type is not flagged
		 * to require an object. A NULL _ob_ occurs when original coordinate data
		 * is requested for an object.
		 */
	void *(*applyModifier)(struct ModifierData *md, void *data, struct Object *ob, 
		 								void *derivedData, float (*vertexCos)[3], int useRenderParams);
} ModifierTypeInfo;

ModifierTypeInfo *modifierType_get_info(ModifierType type);

#endif

