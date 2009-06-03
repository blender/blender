/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
#include "DNA_particle_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BKE_context.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"

#include "RNA_access.h"

#include "buttons_intern.h"	// own include

int buttons_context(const bContext *C, const char *member, bContextDataResult *result)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= (scene->basact)? scene->basact->object: NULL;

	if(CTX_data_equals(member, "scene")) {
		CTX_data_pointer_set(result, &scene->id, &RNA_Scene, scene);
		return 1;
	}
	else if(CTX_data_equals(member, "world")) {
		CTX_data_pointer_set(result, &scene->world->id, &RNA_World, scene->world);
		return 1;
	}
	else if(CTX_data_equals(member, "object")) {
		CTX_data_pointer_set(result, &ob->id, &RNA_Object, ob);
		return 1;
	}
	else if(CTX_data_equals(member, "mesh")) {
		if(ob && ob->type == OB_MESH) {
			CTX_data_pointer_set(result, ob->data, &RNA_Mesh, ob->data);
			return 1;
		}
	}
	else if(CTX_data_equals(member, "armature")) {
		if(ob && ob->type == OB_ARMATURE) {
			CTX_data_pointer_set(result, ob->data, &RNA_Armature, ob->data);
			return 1;
		}
	}
	else if(CTX_data_equals(member, "lattice")) {
		if(ob && ob->type == OB_LATTICE) {
			CTX_data_pointer_set(result, ob->data, &RNA_Lattice, ob->data);
			return 1;
		}
	}
	else if(CTX_data_equals(member, "curve")) {
		if(ob && ob->type == OB_CURVE) {
			CTX_data_pointer_set(result, ob->data, &RNA_Curve, ob->data);
			return 1;
		}
	}
	else if(CTX_data_equals(member, "meta_ball")) {
		if(ob && ob->type == OB_MBALL) {
			CTX_data_pointer_set(result, ob->data, &RNA_MetaBall, ob->data);
			return 1;
		}
	}
	else if(CTX_data_equals(member, "lamp")) {
		if(ob && ob->type == OB_LAMP) {
			CTX_data_pointer_set(result, ob->data, &RNA_Lamp, ob->data);
			return 1;
		}
	}
	else if(CTX_data_equals(member, "camera")) {
		if(ob && ob->type == OB_CAMERA) {
			CTX_data_pointer_set(result, ob->data, &RNA_Camera, ob->data);
			return 1;
		}
	}
	else if(CTX_data_equals(member, "material")) {
		if(ob && ob->type && (ob->type<OB_LAMP)) {
			Material *ma= give_current_material(ob, ob->actcol);
			CTX_data_pointer_set(result, &ma->id, &RNA_Material, ma);
			return 1;
		}
	}
	else if(CTX_data_equals(member, "texture")) {
		if(ob && ob->type && (ob->type<OB_LAMP)) {
			Material *ma= give_current_material(ob, ob->actcol);

			if(ma) {
				MTex *mtex= ma->mtex[(int)ma->texact];
				
				if(mtex->tex) {
					CTX_data_pointer_set(result, &mtex->tex->id, &RNA_Texture, mtex->tex);
					return 1;
				}
			}
		}
	}
	else if(CTX_data_equals(member, "material_slot")) {
	}
	else if(CTX_data_equals(member, "texture_slot")) {
		if(ob && ob->type && (ob->type<OB_LAMP)) {
			Material *ma= give_current_material(ob, ob->actcol);

			if(ma) {
				MTex *mtex= ma->mtex[(int)ma->texact];
				
				CTX_data_pointer_set(result, &ma->id, &RNA_TextureSlot, mtex);
				return 1;
			}
		}
	}
	else if(CTX_data_equals(member, "bone")) {
		if(ob && ob->type == OB_ARMATURE) {
			bArmature *arm= ob->data;
			Bone *bone;

			for(bone=arm->bonebase.first; bone; bone=bone->next) {
				if(bone->flag & BONE_ACTIVE) {
					CTX_data_pointer_set(result, &arm->id, &RNA_Bone, bone);
					return 1;
				}
			}
		}
	}
	else if(CTX_data_equals(member, "particle_system")) {
		if(ob) {
			ParticleSystem *psys= psys_get_current(ob);
			CTX_data_pointer_set(result, &ob->id, &RNA_ParticleSystem, psys);
			return 1;
		}
	}
	else if(CTX_data_equals(member, "cloth")) {
		if(ob) {
			ModifierData *md= modifiers_findByType(ob, eModifierType_Cloth);
			CTX_data_pointer_set(result, &ob->id, &RNA_ClothModifier, md);
			return 1;
		}
	}
	else if(CTX_data_equals(member, "soft_body")) {
		if(ob) {
			CTX_data_pointer_set(result, &ob->id, &RNA_SoftBodySettings, ob->soft);
			return 1;
		}
	}
	else if(CTX_data_equals(member, "fluid")) {
		if(ob) {
			ModifierData *md= modifiers_findByType(ob, eModifierType_Fluidsim);
			CTX_data_pointer_set(result, &ob->id, &RNA_FluidSimulationModifier, md);
			return 1;
		}
	}

	return 0;
}

