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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_buttons/buttons_context.c
 *  \ingroup spbuttons
 */


#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "DNA_armature_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "DNA_brush_types.h"
#include "DNA_linestyle_types.h"

#include "BKE_context.h"
#include "BKE_action.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_screen.h"
#include "BKE_texture.h"
#include "BKE_linestyle.h"

#include "RNA_access.h"

#include "ED_armature.h"
#include "ED_screen.h"
#include "ED_physics.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "buttons_intern.h" // own include

static int set_pointer_type(ButsContextPath *path, bContextDataResult *result, StructRNA *type)
{
	PointerRNA *ptr;
	int a;

	for (a = 0; a < path->len; a++) {
		ptr = &path->ptr[a];

		if (RNA_struct_is_a(ptr->type, type)) {
			CTX_data_pointer_set(result, ptr->id.data, ptr->type, ptr->data);
			return 1;
		}
	}

	return 0;
}

static PointerRNA *get_pointer_type(ButsContextPath *path, StructRNA *type)
{
	PointerRNA *ptr;
	int a;

	for (a = 0; a < path->len; a++) {
		ptr = &path->ptr[a];

		if (RNA_struct_is_a(ptr->type, type))
			return ptr;
	}

	return NULL;
}

/************************* Creating the Path ************************/

static int buttons_context_path_scene(ButsContextPath *path)
{
	PointerRNA *ptr = &path->ptr[path->len - 1];

	/* this one just verifies */
	return RNA_struct_is_a(ptr->type, &RNA_Scene);
}

/* note: this function can return 1 without adding a world to the path
 * so the buttons stay visible, but be sure to check the ID type if a ID_WO */
static int buttons_context_path_world(ButsContextPath *path)
{
	Scene *scene;
	World *world;
	PointerRNA *ptr = &path->ptr[path->len - 1];

	/* if we already have a (pinned) world, we're done */
	if (RNA_struct_is_a(ptr->type, &RNA_World)) {
		return 1;
	}
	/* if we have a scene, use the scene's world */
	else if (buttons_context_path_scene(path)) {
		scene = path->ptr[path->len - 1].data;
		world = scene->world;
		
		if (world) {
			RNA_id_pointer_create(&scene->world->id, &path->ptr[path->len]);
			path->len++;
			return 1;
		}
		else {
			return 1;
		}
	}

	/* no path to a world possible */
	return 0;
}

static int buttons_context_path_linestyle(ButsContextPath *path)
{
	Scene *scene;
	FreestyleLineStyle *linestyle;
	PointerRNA *ptr = &path->ptr[path->len - 1];

	/* if we already have a (pinned) linestyle, we're done */
	if (RNA_struct_is_a(ptr->type, &RNA_FreestyleLineStyle)) {
		return 1;
	}
	/* if we have a scene, use the lineset's linestyle */
	else if (buttons_context_path_scene(path)) {
		scene = path->ptr[path->len - 1].data;
		linestyle = BKE_linestyle_active_from_scene(scene);
		if (linestyle) {
			RNA_id_pointer_create(&linestyle->id, &path->ptr[path->len]);
			path->len++;
			return 1;
		}
	}

	/* no path to a linestyle possible */
	return 0;
}

static int buttons_context_path_object(ButsContextPath *path)
{
	Scene *scene;
	Object *ob;
	PointerRNA *ptr = &path->ptr[path->len - 1];

	/* if we already have a (pinned) object, we're done */
	if (RNA_struct_is_a(ptr->type, &RNA_Object)) {
		return 1;
	}
	/* if we have a scene, use the scene's active object */
	else if (buttons_context_path_scene(path)) {
		scene = path->ptr[path->len - 1].data;
		ob = (scene->basact) ? scene->basact->object : NULL;

		if (ob) {
			RNA_id_pointer_create(&ob->id, &path->ptr[path->len]);
			path->len++;

			return 1;
		}
	}

	/* no path to a object possible */
	return 0;
}

static int buttons_context_path_data(ButsContextPath *path, int type)
{
	Object *ob;
	PointerRNA *ptr = &path->ptr[path->len - 1];

	/* if we already have a data, we're done */
	if (RNA_struct_is_a(ptr->type, &RNA_Mesh) && (type == -1 || type == OB_MESH)) return 1;
	else if (RNA_struct_is_a(ptr->type, &RNA_Curve) && (type == -1 || ELEM3(type, OB_CURVE, OB_SURF, OB_FONT))) return 1;
	else if (RNA_struct_is_a(ptr->type, &RNA_Armature) && (type == -1 || type == OB_ARMATURE)) return 1;
	else if (RNA_struct_is_a(ptr->type, &RNA_MetaBall) && (type == -1 || type == OB_MBALL)) return 1;
	else if (RNA_struct_is_a(ptr->type, &RNA_Lattice) && (type == -1 || type == OB_LATTICE)) return 1;
	else if (RNA_struct_is_a(ptr->type, &RNA_Camera) && (type == -1 || type == OB_CAMERA)) return 1;
	else if (RNA_struct_is_a(ptr->type, &RNA_Lamp) && (type == -1 || type == OB_LAMP)) return 1;
	else if (RNA_struct_is_a(ptr->type, &RNA_Speaker) && (type == -1 || type == OB_SPEAKER)) return 1;
	/* try to get an object in the path, no pinning supported here */
	else if (buttons_context_path_object(path)) {
		ob = path->ptr[path->len - 1].data;

		if (ob && (type == -1 || type == ob->type)) {
			RNA_id_pointer_create(ob->data, &path->ptr[path->len]);
			path->len++;

			return 1;
		}
	}

	/* no path to data possible */
	return 0;
}

static int buttons_context_path_modifier(ButsContextPath *path)
{
	Object *ob;

	if (buttons_context_path_object(path)) {
		ob = path->ptr[path->len - 1].data;

		if (ob && ELEM5(ob->type, OB_MESH, OB_CURVE, OB_FONT, OB_SURF, OB_LATTICE))
			return 1;
	}

	return 0;
}

static int buttons_context_path_material(ButsContextPath *path, bool for_texture, bool new_shading)
{
	Object *ob;
	PointerRNA *ptr = &path->ptr[path->len - 1];
	Material *ma;

	/* if we already have a (pinned) material, we're done */
	if (RNA_struct_is_a(ptr->type, &RNA_Material)) {
		return 1;
	}
	/* if we have an object, use the object material slot */
	else if (buttons_context_path_object(path)) {
		ob = path->ptr[path->len - 1].data;

		if (ob && OB_TYPE_SUPPORT_MATERIAL(ob->type)) {
			ma = give_current_material(ob, ob->actcol);
			RNA_id_pointer_create(&ma->id, &path->ptr[path->len]);
			path->len++;

			if (for_texture && give_current_material_texture_node(ma))
				return 1;

			if (!new_shading) {
				/* Only try to get mat from node in case of old shading system (see T40331). */
				ma = give_node_material(ma);
				if (ma) {
					RNA_id_pointer_create(&ma->id, &path->ptr[path->len]);
					path->len++;
				}
			}
			return 1;
		}
	}

	/* no path to a material possible */
	return 0;
}

static int buttons_context_path_bone(ButsContextPath *path)
{
	bArmature *arm;
	EditBone *edbo;

	/* if we have an armature, get the active bone */
	if (buttons_context_path_data(path, OB_ARMATURE)) {
		arm = path->ptr[path->len - 1].data;

		if (arm->edbo) {
			if (arm->act_edbone) {
				edbo = arm->act_edbone;
				RNA_pointer_create(&arm->id, &RNA_EditBone, edbo, &path->ptr[path->len]);
				path->len++;
				return 1;
			}
		}
		else {
			if (arm->act_bone) {
				RNA_pointer_create(&arm->id, &RNA_Bone, arm->act_bone, &path->ptr[path->len]);
				path->len++;
				return 1;
			}
		}
	}

	/* no path to a bone possible */
	return 0;
}

static int buttons_context_path_pose_bone(ButsContextPath *path)
{
	PointerRNA *ptr = &path->ptr[path->len - 1];

	/* if we already have a (pinned) PoseBone, we're done */
	if (RNA_struct_is_a(ptr->type, &RNA_PoseBone)) {
		return 1;
	}

	/* if we have an armature, get the active bone */
	if (buttons_context_path_object(path)) {
		Object *ob = path->ptr[path->len - 1].data;
		bArmature *arm = ob->data; /* path->ptr[path->len-1].data - works too */

		if (ob->type != OB_ARMATURE || arm->edbo) {
			return 0;
		}
		else {
			if (arm->act_bone) {
				bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, arm->act_bone->name);
				if (pchan) {
					RNA_pointer_create(&ob->id, &RNA_PoseBone, pchan, &path->ptr[path->len]);
					path->len++;
					return 1;
				}
			}
		}
	}

	/* no path to a bone possible */
	return 0;
}


static int buttons_context_path_particle(ButsContextPath *path)
{
	Object *ob;
	ParticleSystem *psys;
	PointerRNA *ptr = &path->ptr[path->len - 1];

	/* if we already have (pinned) particle settings, we're done */
	if (RNA_struct_is_a(ptr->type, &RNA_ParticleSettings)) {
		return 1;
	}
	/* if we have an object, get the active particle system */
	if (buttons_context_path_object(path)) {
		ob = path->ptr[path->len - 1].data;

		if (ob && ob->type == OB_MESH) {
			psys = psys_get_current(ob);

			RNA_pointer_create(&ob->id, &RNA_ParticleSystem, psys, &path->ptr[path->len]);
			path->len++;
			return 1;
		}
	}

	/* no path to a particle system possible */
	return 0;
}

static int buttons_context_path_brush(ButsContextPath *path)
{
	Scene *scene;
	Brush *br = NULL;
	PointerRNA *ptr = &path->ptr[path->len - 1];

	/* if we already have a (pinned) brush, we're done */
	if (RNA_struct_is_a(ptr->type, &RNA_Brush)) {
		return 1;
	}
	/* if we have a scene, use the toolsettings brushes */
	else if (buttons_context_path_scene(path)) {
		scene = path->ptr[path->len - 1].data;

		if (scene)
			br = BKE_paint_brush(BKE_paint_get_active(scene));

		if (br) {
			RNA_id_pointer_create((ID *)br, &path->ptr[path->len]);
			path->len++;

			return 1;
		}
	}

	/* no path to a brush possible */
	return 0;
}

static int buttons_context_path_texture(ButsContextPath *path, ButsContextTexture *ct)
{
	if (ct) {
		/* new shading system */
		PointerRNA *ptr = &path->ptr[path->len - 1];
		ID *id;

		/* if we already have a (pinned) texture, we're done */
		if (RNA_struct_is_a(ptr->type, &RNA_Texture))
			return 1;

		if (!ct->user)
			return 0;
		
		id = ct->user->id;

		if (id) {
			if (GS(id->name) == ID_BR)
				buttons_context_path_brush(path);
			else if (GS(id->name) == ID_MA)
				buttons_context_path_material(path, false, true);
			else if (GS(id->name) == ID_WO)
				buttons_context_path_world(path);
			else if (GS(id->name) == ID_LA)
				buttons_context_path_data(path, OB_LAMP);
			else if (GS(id->name) == ID_PA)
				buttons_context_path_particle(path);
			else if (GS(id->name) == ID_OB)
				buttons_context_path_object(path);
			else if (GS(id->name) == ID_LS)
				buttons_context_path_linestyle(path);
		}

		if (ct->texture) {
			RNA_id_pointer_create(&ct->texture->id, &path->ptr[path->len]);
			path->len++;
		}

		return 1;
	}
	else {
		/* old shading system */
		Material *ma;
		Lamp *la;
		World *wo;
		ParticleSystem *psys;
		FreestyleLineStyle *ls;
		Tex *tex;
		PointerRNA *ptr = &path->ptr[path->len - 1];

		/* if we already have a (pinned) texture, we're done */
		if (RNA_struct_is_a(ptr->type, &RNA_Texture)) {
			return 1;
		}
		/* try world */
		else if ((path->tex_ctx == SB_TEXC_WORLD) && buttons_context_path_world(path)) {
			wo = path->ptr[path->len - 1].data;

			if (wo && GS(wo->id.name) == ID_WO) {
				tex = give_current_world_texture(wo);

				RNA_id_pointer_create(&tex->id, &path->ptr[path->len]);
				path->len++;
				return 1;
			}
		}
		/* try particles */
		else if ((path->tex_ctx == SB_TEXC_PARTICLES) && buttons_context_path_particle(path)) {
			if (path->ptr[path->len - 1].type == &RNA_ParticleSettings) {
				ParticleSettings *part = path->ptr[path->len - 1].data;

				tex = give_current_particle_texture(part);
				RNA_id_pointer_create(&tex->id, &path->ptr[path->len]);
				path->len++;
				return 1;
			}
			else {
				psys = path->ptr[path->len - 1].data;

				if (psys && psys->part && GS(psys->part->id.name) == ID_PA) {
					tex = give_current_particle_texture(psys->part);

					RNA_id_pointer_create(&tex->id, &path->ptr[path->len]);
					path->len++;
					return 1;
				}
			}
		}
		/* try material */
		else if ((path->tex_ctx == SB_TEXC_MATERIAL) && buttons_context_path_material(path, true, false)) {
			ma = path->ptr[path->len - 1].data;

			if (ma) {
				tex = give_current_material_texture(ma);

				RNA_id_pointer_create(&tex->id, &path->ptr[path->len]);
				path->len++;
				return 1;
			}
		}
		/* try lamp */
		else if ((path->tex_ctx == SB_TEXC_LAMP) && buttons_context_path_data(path, OB_LAMP)) {
			la = path->ptr[path->len - 1].data;

			if (la) {
				tex = give_current_lamp_texture(la);

				RNA_id_pointer_create(&tex->id, &path->ptr[path->len]);
				path->len++;
				return 1;
			}
		}
		/* try linestyle */
		else if ((path->tex_ctx == SB_TEXC_LINESTYLE) && buttons_context_path_linestyle(path)) {
			ls = path->ptr[path->len - 1].data;

			if (ls) {
				tex = give_current_linestyle_texture(ls);

				RNA_id_pointer_create(&tex->id, &path->ptr[path->len]);
				path->len++;
				return 1;
			}
		}
	}

	/* no path to a texture possible */
	return 0;
}

#ifdef WITH_FREESTYLE
static bool buttons_context_linestyle_pinnable(const bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *actsrl;
	FreestyleConfig *config;
	SpaceButs *sbuts;

	/* if Freestyle is disabled in the scene */
	if ((scene->r.mode & R_EDGE_FRS) == 0) {
		return false;
	}
	/* if Freestyle is not in the Parameter Editor mode */
	actsrl = BLI_findlink(&scene->r.layers, scene->r.actlay);
	config = &actsrl->freestyleConfig;
	if (config->mode != FREESTYLE_CONTROL_EDITOR_MODE) {
		return false;
	}
	/* if the scene has already been pinned */
	sbuts = CTX_wm_space_buts(C);
	if (sbuts->pinid && sbuts->pinid == &scene->id) {
		return false;
	}
	return true;
}
#endif

static int buttons_context_path(const bContext *C, ButsContextPath *path, int mainb, int flag)
{
	SpaceButs *sbuts = CTX_wm_space_buts(C);
	ID *id;
	int found;

	memset(path, 0, sizeof(*path));
	path->flag = flag;
	path->tex_ctx = sbuts->texture_context;

	/* if some ID datablock is pinned, set the root pointer */
	if (sbuts->pinid) {
		id = sbuts->pinid;

		RNA_id_pointer_create(id, &path->ptr[0]);
		path->len++;
	}

	/* no pinned root, use scene as root */
	if (path->len == 0) {
		id = (ID *)CTX_data_scene(C);
		RNA_id_pointer_create(id, &path->ptr[0]);
		path->len++;
	}

	/* now for each buttons context type, we try to construct a path,
	 * tracing back recursively */
	switch (mainb) {
		case BCONTEXT_SCENE:
		case BCONTEXT_RENDER:
			found = buttons_context_path_scene(path);
			break;
		case BCONTEXT_RENDER_LAYER:
#ifdef WITH_FREESTYLE
			if (buttons_context_linestyle_pinnable(C)) {
				found = buttons_context_path_linestyle(path);
				if (found) {
					break;
				}
			}
#endif
			found = buttons_context_path_scene(path);
			break;
		case BCONTEXT_WORLD:
			found = buttons_context_path_world(path);
			break;
		case BCONTEXT_OBJECT:
		case BCONTEXT_PHYSICS:
		case BCONTEXT_CONSTRAINT:
			found = buttons_context_path_object(path);
			break;
		case BCONTEXT_MODIFIER:
			found = buttons_context_path_modifier(path);
			break;
		case BCONTEXT_DATA:
			found = buttons_context_path_data(path, -1);
			break;
		case BCONTEXT_PARTICLE:
			found = buttons_context_path_particle(path);
			break;
		case BCONTEXT_MATERIAL:
			found = buttons_context_path_material(path, false, (sbuts->texuser != NULL));
			break;
		case BCONTEXT_TEXTURE:
			found = buttons_context_path_texture(path, sbuts->texuser);
			break;
		case BCONTEXT_BONE:
			found = buttons_context_path_bone(path);
			if (!found)
				found = buttons_context_path_data(path, OB_ARMATURE);
			break;
		case BCONTEXT_BONE_CONSTRAINT:
			found = buttons_context_path_pose_bone(path);
			break;
		default:
			found = 0;
			break;
	}

	return found;
}

static int buttons_shading_context(const bContext *C, int mainb)
{
	Object *ob = CTX_data_active_object(C);

	if (ELEM3(mainb, BCONTEXT_MATERIAL, BCONTEXT_WORLD, BCONTEXT_TEXTURE))
		return 1;
	if (mainb == BCONTEXT_DATA && ob && ELEM(ob->type, OB_LAMP, OB_CAMERA))
		return 1;
	
	return 0;
}

static int buttons_shading_new_context(const bContext *C, int flag)
{
	Object *ob = CTX_data_active_object(C);

	if (flag & (1 << BCONTEXT_MATERIAL))
		return BCONTEXT_MATERIAL;
	else if (ob && ELEM(ob->type, OB_LAMP, OB_CAMERA) && (flag & (1 << BCONTEXT_DATA)))
		return BCONTEXT_DATA;
	else if (flag & (1 << BCONTEXT_WORLD))
		return BCONTEXT_WORLD;
	
	return BCONTEXT_RENDER;
}

void buttons_context_compute(const bContext *C, SpaceButs *sbuts)
{
	ButsContextPath *path;
	PointerRNA *ptr;
	int a, pflag = 0, flag = 0;

	if (!sbuts->path)
		sbuts->path = MEM_callocN(sizeof(ButsContextPath), "ButsContextPath");

	path = sbuts->path;

	/* We need to set Scene path now! Else, buttons_texture_context_compute() might not get a valid scene. */
	buttons_context_path(C, path, BCONTEXT_SCENE, pflag);

	buttons_texture_context_compute(C, sbuts);

	/* for each context, see if we can compute a valid path to it, if
	 * this is the case, we know we have to display the button */
	for (a = 0; a < BCONTEXT_TOT; a++) {
		if (buttons_context_path(C, path, a, pflag)) {
			flag |= (1 << a);

			/* setting icon for data context */
			if (a == BCONTEXT_DATA) {
				ptr = &path->ptr[path->len - 1];

				if (ptr->type)
					sbuts->dataicon = RNA_struct_ui_icon(ptr->type);
				else
					sbuts->dataicon = ICON_EMPTY_DATA;
			}
		}
	}

	/* always try to use the tab that was explicitly
	 * set to the user, so that once that context comes
	 * back, the tab is activated again */
	sbuts->mainb = sbuts->mainbuser;

	/* in case something becomes invalid, change */
	if ((flag & (1 << sbuts->mainb)) == 0) {
		if (sbuts->flag & SB_SHADING_CONTEXT) {
			/* try to keep showing shading related buttons */
			sbuts->mainb = buttons_shading_new_context(C, flag);
		}
		else if (flag & BCONTEXT_OBJECT) {
			sbuts->mainb = BCONTEXT_OBJECT;
		}
		else {
			for (a = 0; a < BCONTEXT_TOT; a++) {
				if (flag & (1 << a)) {
					sbuts->mainb = a;
					break;
				}
			}
		}
	}

	buttons_context_path(C, path, sbuts->mainb, pflag);

	if (!(flag & (1 << sbuts->mainb))) {
		if (flag & (1 << BCONTEXT_OBJECT))
			sbuts->mainb = BCONTEXT_OBJECT;
		else
			sbuts->mainb = BCONTEXT_SCENE;
	}

	if (buttons_shading_context(C, sbuts->mainb))
		sbuts->flag |= SB_SHADING_CONTEXT;
	else
		sbuts->flag &= ~SB_SHADING_CONTEXT;

	sbuts->pathflag = flag;
}

/************************* Context Callback ************************/

const char *buttons_context_dir[] = {
	"texture_slot", "scene", "world", "object", "mesh", "armature", "lattice", "curve",
	"meta_ball", "lamp", "speaker", "camera", "material", "material_slot",
	"texture", "texture_user", "texture_user_property", "bone", "edit_bone",
	"pose_bone", "particle_system", "particle_system_editable", "particle_settings",
	"cloth", "soft_body", "fluid", "smoke", "collision", "brush", "dynamic_paint",
	"line_style", NULL
};

int buttons_context(const bContext *C, const char *member, bContextDataResult *result)
{
	SpaceButs *sbuts = CTX_wm_space_buts(C);
	ButsContextPath *path = sbuts ? sbuts->path : NULL;

	if (!path)
		return 0;

	/* here we handle context, getting data from precomputed path */
	if (CTX_data_dir(member)) {
		/* in case of new shading system we skip texture_slot, complex python
		 * UI script logic depends on checking if this is available */
		if (sbuts->texuser)
			CTX_data_dir_set(result, buttons_context_dir + 1);
		else
			CTX_data_dir_set(result, buttons_context_dir);
		return 1;
	}
	else if (CTX_data_equals(member, "scene")) {
		/* Do not return one here if scene not found in path, in this case we want to get default context scene! */
		return set_pointer_type(path, result, &RNA_Scene);
	}
	else if (CTX_data_equals(member, "world")) {
		set_pointer_type(path, result, &RNA_World);
		return 1;
	}
	else if (CTX_data_equals(member, "object")) {
		set_pointer_type(path, result, &RNA_Object);
		return 1;
	}
	else if (CTX_data_equals(member, "mesh")) {
		set_pointer_type(path, result, &RNA_Mesh);
		return 1;
	}
	else if (CTX_data_equals(member, "armature")) {
		set_pointer_type(path, result, &RNA_Armature);
		return 1;
	}
	else if (CTX_data_equals(member, "lattice")) {
		set_pointer_type(path, result, &RNA_Lattice);
		return 1;
	}
	else if (CTX_data_equals(member, "curve")) {
		set_pointer_type(path, result, &RNA_Curve);
		return 1;
	}
	else if (CTX_data_equals(member, "meta_ball")) {
		set_pointer_type(path, result, &RNA_MetaBall);
		return 1;
	}
	else if (CTX_data_equals(member, "lamp")) {
		set_pointer_type(path, result, &RNA_Lamp);
		return 1;
	}
	else if (CTX_data_equals(member, "camera")) {
		set_pointer_type(path, result, &RNA_Camera);
		return 1;
	}
	else if (CTX_data_equals(member, "speaker")) {
		set_pointer_type(path, result, &RNA_Speaker);
		return 1;
	}
	else if (CTX_data_equals(member, "material")) {
		set_pointer_type(path, result, &RNA_Material);
		return 1;
	}
	else if (CTX_data_equals(member, "texture")) {
		ButsContextTexture *ct = sbuts->texuser;

		if (ct) {
			/* new shading system */
			CTX_data_pointer_set(result, &ct->texture->id, &RNA_Texture, ct->texture);
		}
		else {
			/* old shading system */
			set_pointer_type(path, result, &RNA_Texture);
		}

		return 1;
	}
	else if (CTX_data_equals(member, "material_slot")) {
		PointerRNA *ptr = get_pointer_type(path, &RNA_Object);

		if (ptr) {
			Object *ob = ptr->data;

			if (ob && OB_TYPE_SUPPORT_MATERIAL(ob->type) && ob->totcol) {
				/* a valid actcol isn't ensured [#27526] */
				int matnr = ob->actcol - 1;
				if (matnr < 0) matnr = 0;
				CTX_data_pointer_set(result, &ob->id, &RNA_MaterialSlot, &ob->mat[matnr]);
			}
		}

		return 1;
	}
	else if (CTX_data_equals(member, "texture_user")) {
		ButsContextTexture *ct = sbuts->texuser;

		if (!ct)
			return -1;  /* old shading system (found but not available) */

		if (ct->user && ct->user->ptr.data) {
			ButsTextureUser *user = ct->user;
			CTX_data_pointer_set(result, user->ptr.id.data, user->ptr.type, user->ptr.data);
		}

		return 1;
	}
	else if (CTX_data_equals(member, "texture_user_property")) {
		ButsContextTexture *ct = sbuts->texuser;

		if (!ct)
			return -1;  /* old shading system (found but not available) */

		if (ct->user && ct->user->ptr.data) {
			ButsTextureUser *user = ct->user;
			CTX_data_pointer_set(result, NULL, &RNA_Property, user->prop);
		}

		return 1;
	}
	else if (CTX_data_equals(member, "texture_node")) {
		ButsContextTexture *ct = sbuts->texuser;

		if (ct) {
			/* new shading system */
			if (ct->user && ct->user->node) {
				CTX_data_pointer_set(result, &ct->user->ntree->id, &RNA_Node, ct->user->node);
			}

			return 1;
		}
		else {
			/* old shading system */
			PointerRNA *ptr;

			if ((ptr = get_pointer_type(path, &RNA_Material))) {
				Material *ma = ptr->data;

				if (ma) {
					bNode *node = give_current_material_texture_node(ma);
					CTX_data_pointer_set(result, &ma->nodetree->id, &RNA_Node, node);
				}
			}

			return 1;
		}
	}
	else if (CTX_data_equals(member, "texture_slot")) {
		ButsContextTexture *ct = sbuts->texuser;
		PointerRNA *ptr;

		/* Particles slots are used in both old and new textures handling. */
		if ((ptr = get_pointer_type(path, &RNA_ParticleSystem))) {
			ParticleSettings *part = ((ParticleSystem *)ptr->data)->part;

			if (part)
				CTX_data_pointer_set(result, &part->id, &RNA_ParticleSettingsTextureSlot, part->mtex[(int)part->texact]);
		}
		else if (ct) {
			return 0;  /* new shading system */
		}
		else if ((ptr = get_pointer_type(path, &RNA_Material))) {
			Material *ma = ptr->data;

			/* if we have a node material, get slot from material in material node */
			if (ma && ma->use_nodes && ma->nodetree) {
				/* if there's an active texture node in the node tree,
				 * then that texture is in context directly, without a texture slot */
				if (give_current_material_texture_node(ma))
					return 0;

				ma = give_node_material(ma);
				if (ma)
					CTX_data_pointer_set(result, &ma->id, &RNA_MaterialTextureSlot, ma->mtex[(int)ma->texact]);
				else
					return 0;
			}
			else if (ma) {
				CTX_data_pointer_set(result, &ma->id, &RNA_MaterialTextureSlot, ma->mtex[(int)ma->texact]);
			}
		}
		else if ((ptr = get_pointer_type(path, &RNA_Lamp))) {
			Lamp *la = ptr->data;

			if (la)
				CTX_data_pointer_set(result, &la->id, &RNA_LampTextureSlot, la->mtex[(int)la->texact]);
		}
		else if ((ptr = get_pointer_type(path, &RNA_World))) {
			World *wo = ptr->data;

			if (wo)
				CTX_data_pointer_set(result, &wo->id, &RNA_WorldTextureSlot, wo->mtex[(int)wo->texact]);
		}
		else if ((ptr = get_pointer_type(path, &RNA_FreestyleLineStyle))) {
			FreestyleLineStyle *ls = ptr->data;

			if (ls)
				CTX_data_pointer_set(result, &ls->id, &RNA_LineStyleTextureSlot, ls->mtex[(int)ls->texact]);
		}

		return 1;
	}
	else if (CTX_data_equals(member, "bone")) {
		set_pointer_type(path, result, &RNA_Bone);
		return 1;
	}
	else if (CTX_data_equals(member, "edit_bone")) {
		set_pointer_type(path, result, &RNA_EditBone);
		return 1;
	}
	else if (CTX_data_equals(member, "pose_bone")) {
		set_pointer_type(path, result, &RNA_PoseBone);
		return 1;
	}
	else if (CTX_data_equals(member, "particle_system")) {
		set_pointer_type(path, result, &RNA_ParticleSystem);
		return 1;
	}
	else if (CTX_data_equals(member, "particle_system_editable")) {
		if (PE_poll((bContext *)C))
			set_pointer_type(path, result, &RNA_ParticleSystem);
		else
			CTX_data_pointer_set(result, NULL, &RNA_ParticleSystem, NULL);
		return 1;
	}
	else if (CTX_data_equals(member, "particle_settings")) {
		/* only available when pinned */
		PointerRNA *ptr = get_pointer_type(path, &RNA_ParticleSettings);
		
		if (ptr && ptr->data) {
			CTX_data_pointer_set(result, ptr->id.data, &RNA_ParticleSettings, ptr->data);
			return 1;
		}
		else {
			/* get settings from active particle system instead */
			ptr = get_pointer_type(path, &RNA_ParticleSystem);
			
			if (ptr && ptr->data) {
				ParticleSettings *part = ((ParticleSystem *)ptr->data)->part;
				CTX_data_pointer_set(result, ptr->id.data, &RNA_ParticleSettings, part);
				return 1;
			}
		}
		set_pointer_type(path, result, &RNA_ParticleSettings);
		return 1;
	}
	else if (CTX_data_equals(member, "cloth")) {
		PointerRNA *ptr = get_pointer_type(path, &RNA_Object);

		if (ptr && ptr->data) {
			Object *ob = ptr->data;
			ModifierData *md = modifiers_findByType(ob, eModifierType_Cloth);
			CTX_data_pointer_set(result, &ob->id, &RNA_ClothModifier, md);
			return 1;
		}
	}
	else if (CTX_data_equals(member, "soft_body")) {
		PointerRNA *ptr = get_pointer_type(path, &RNA_Object);

		if (ptr && ptr->data) {
			Object *ob = ptr->data;
			ModifierData *md = modifiers_findByType(ob, eModifierType_Softbody);
			CTX_data_pointer_set(result, &ob->id, &RNA_SoftBodyModifier, md);
			return 1;
		}
	}
	else if (CTX_data_equals(member, "fluid")) {
		PointerRNA *ptr = get_pointer_type(path, &RNA_Object);

		if (ptr && ptr->data) {
			Object *ob = ptr->data;
			ModifierData *md = modifiers_findByType(ob, eModifierType_Fluidsim);
			CTX_data_pointer_set(result, &ob->id, &RNA_FluidSimulationModifier, md);
			return 1;
		}
	}
	
	else if (CTX_data_equals(member, "smoke")) {
		PointerRNA *ptr = get_pointer_type(path, &RNA_Object);

		if (ptr && ptr->data) {
			Object *ob = ptr->data;
			ModifierData *md = modifiers_findByType(ob, eModifierType_Smoke);
			CTX_data_pointer_set(result, &ob->id, &RNA_SmokeModifier, md);
			return 1;
		}
	}
	else if (CTX_data_equals(member, "collision")) {
		PointerRNA *ptr = get_pointer_type(path, &RNA_Object);

		if (ptr && ptr->data) {
			Object *ob = ptr->data;
			ModifierData *md = modifiers_findByType(ob, eModifierType_Collision);
			CTX_data_pointer_set(result, &ob->id, &RNA_CollisionModifier, md);
			return 1;
		}
	}
	else if (CTX_data_equals(member, "brush")) {
		set_pointer_type(path, result, &RNA_Brush);
		return 1;
	}
	else if (CTX_data_equals(member, "dynamic_paint")) {
		PointerRNA *ptr = get_pointer_type(path, &RNA_Object);

		if (ptr && ptr->data) {
			Object *ob = ptr->data;
			ModifierData *md = modifiers_findByType(ob, eModifierType_DynamicPaint);
			CTX_data_pointer_set(result, &ob->id, &RNA_DynamicPaintModifier, md);
			return 1;
		}
	}
	else if (CTX_data_equals(member, "line_style")) {
		set_pointer_type(path, result, &RNA_FreestyleLineStyle);
		return 1;
	}
	else {
		return 0; /* not found */
	}

	return -1; /* found but not available */
}

/************************* Drawing the Path ************************/

static void pin_cb(bContext *C, void *UNUSED(arg1), void *UNUSED(arg2))
{
	SpaceButs *sbuts = CTX_wm_space_buts(C);

	if (sbuts->flag & SB_PIN_CONTEXT) {
		sbuts->pinid = buttons_context_id_path(C);
	}
	else
		sbuts->pinid = NULL;
	
	ED_area_tag_redraw(CTX_wm_area(C));
}

void buttons_context_draw(const bContext *C, uiLayout *layout)
{
	SpaceButs *sbuts = CTX_wm_space_buts(C);
	ButsContextPath *path = sbuts->path;
	uiLayout *row;
	uiBlock *block;
	uiBut *but;
	PointerRNA *ptr;
	char namebuf[128], *name;
	int a, icon;

	if (!path)
		return;

	row = uiLayoutRow(layout, true);
	uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_LEFT);

	block = uiLayoutGetBlock(row);
	uiBlockSetEmboss(block, UI_EMBOSSN);
	but = uiDefIconButBitC(block, ICONTOG, SB_PIN_CONTEXT, 0, ICON_UNPINNED, 0, 0, UI_UNIT_X, UI_UNIT_Y, &sbuts->flag,
	                       0, 0, 0, 0, TIP_("Follow context or keep fixed datablock displayed"));
	uiButClearFlag(but, UI_BUT_UNDO); /* skip undo on screen buttons */
	uiButSetFunc(but, pin_cb, NULL, NULL);

	for (a = 0; a < path->len; a++) {
		ptr = &path->ptr[a];

		if (a != 0)
			uiItemL(row, "", VICO_SMALL_TRI_RIGHT_VEC);

		if (ptr->data) {
			icon = RNA_struct_ui_icon(ptr->type);
			name = RNA_struct_name_get_alloc(ptr, namebuf, sizeof(namebuf), NULL);

			if (name) {
				if (!ELEM3(sbuts->mainb, BCONTEXT_RENDER, BCONTEXT_SCENE, BCONTEXT_RENDER_LAYER) && ptr->type == &RNA_Scene)
					uiItemLDrag(row, ptr, "", icon);  /* save some space */
				else
					uiItemLDrag(row, ptr, name, icon);

				if (name != namebuf)
					MEM_freeN(name);
			}
			else
				uiItemL(row, "", icon);
		}
	}
}

static void buttons_panel_context(const bContext *C, Panel *pa)
{
	buttons_context_draw(C, pa->layout);
}

void buttons_context_register(ARegionType *art)
{
	PanelType *pt;

	pt = MEM_callocN(sizeof(PanelType), "spacetype buttons panel context");
	strcpy(pt->idname, "BUTTONS_PT_context");
	strcpy(pt->label, N_("Context"));  /* XXX C panels are not available through RNA (bpy.types)! */
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = buttons_panel_context;
	pt->flag = PNL_NO_HEADER;
	BLI_addtail(&art->paneltypes, pt);
}

ID *buttons_context_id_path(const bContext *C)
{
	SpaceButs *sbuts = CTX_wm_space_buts(C);
	ButsContextPath *path = sbuts->path;
	PointerRNA *ptr;
	int a;

	if (path->len) {
		for (a = path->len - 1; a >= 0; a--) {
			ptr = &path->ptr[a];

			/* pin particle settings instead of system, since only settings are an idblock*/
			if (sbuts->mainb == BCONTEXT_PARTICLE && sbuts->flag & SB_PIN_CONTEXT) {
				if (ptr->type == &RNA_ParticleSystem && ptr->data) {
					ParticleSystem *psys = (ParticleSystem *)ptr->data;
					return &psys->part->id;
				}
			}

			if (ptr->id.data) {
				return ptr->id.data;
			}
		}
	}

	return NULL;
}
