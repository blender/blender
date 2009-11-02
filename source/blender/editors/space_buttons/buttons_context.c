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
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_particle_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_screen.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_world.h"

#include "RNA_access.h"

#include "ED_armature.h"
#include "ED_screen.h"
#include "ED_physics.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "buttons_intern.h"	// own include

typedef struct ButsContextPath {
	PointerRNA ptr[8];
	int len;
	int flag;
} ButsContextPath;

static int set_pointer_type(ButsContextPath *path, bContextDataResult *result, StructRNA *type)
{
	PointerRNA *ptr;
	int a;

	for(a=0; a<path->len; a++) {
		ptr= &path->ptr[a];

		if(RNA_struct_is_a(ptr->type, type)) {
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

	for(a=0; a<path->len; a++) {
		ptr= &path->ptr[a];

		if(RNA_struct_is_a(ptr->type, type))
			return ptr;
	}

	return NULL;
}

/************************* Creating the Path ************************/

static int buttons_context_path_scene(ButsContextPath *path)
{
	PointerRNA *ptr= &path->ptr[path->len-1];

	/* this one just verifies */
	return RNA_struct_is_a(ptr->type, &RNA_Scene);
}

/* note: this function can return 1 without adding a world to the path
 * so the buttons stay visible, but be sure to check the ID type if a ID_WO */
static int buttons_context_path_world(ButsContextPath *path)
{
	Scene *scene;
	World *world;
	PointerRNA *ptr= &path->ptr[path->len-1];

	/* if we already have a (pinned) world, we're done */
	if(RNA_struct_is_a(ptr->type, &RNA_World)) {
		return 1;
	}
	/* if we have a scene, use the scene's world */
	else if(buttons_context_path_scene(path)) {
		scene= path->ptr[path->len-1].data;
		world= scene->world;
		
		if(world) {
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


static int buttons_context_path_object(ButsContextPath *path)
{
	Scene *scene;
	Object *ob;
	PointerRNA *ptr= &path->ptr[path->len-1];

	/* if we already have a (pinned) object, we're done */
	if(RNA_struct_is_a(ptr->type, &RNA_Object)) {
		return 1;
	}
	/* if we have a scene, use the scene's active object */
	else if(buttons_context_path_scene(path)) {
		scene= path->ptr[path->len-1].data;
		ob= (scene->basact)? scene->basact->object: NULL;

		if(ob) {
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
	PointerRNA *ptr= &path->ptr[path->len-1];

	/* if we already have a data, we're done */
	if(RNA_struct_is_a(ptr->type, &RNA_Mesh) && (type == -1 || type == OB_MESH)) return 1;
	else if(RNA_struct_is_a(ptr->type, &RNA_Curve) && (type == -1 || ELEM3(type, OB_CURVE, OB_SURF, OB_FONT))) return 1;
	else if(RNA_struct_is_a(ptr->type, &RNA_Armature) && (type == -1 || type == OB_ARMATURE)) return 1;
	else if(RNA_struct_is_a(ptr->type, &RNA_MetaBall) && (type == -1 || type == OB_MBALL)) return 1;
	else if(RNA_struct_is_a(ptr->type, &RNA_Lattice) && (type == -1 || type == OB_LATTICE)) return 1;
	else if(RNA_struct_is_a(ptr->type, &RNA_Camera) && (type == -1 || type == OB_CAMERA)) return 1;
	else if(RNA_struct_is_a(ptr->type, &RNA_Lamp) && (type == -1 || type == OB_LAMP)) return 1;
	/* try to get an object in the path, no pinning supported here */
	else if(buttons_context_path_object(path)) {
		ob= path->ptr[path->len-1].data;

		if(ob && (type == -1 || type == ob->type)) {
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

	if(buttons_context_path_object(path)) {
		ob= path->ptr[path->len-1].data;

		if(ob && ELEM5(ob->type, OB_MESH, OB_CURVE, OB_FONT, OB_SURF, OB_LATTICE))
			return 1;
	}

	return 0;
}

static int buttons_context_path_material(ButsContextPath *path)
{
	Object *ob;
	PointerRNA *ptr= &path->ptr[path->len-1];
	Material *ma;

	/* if we already have a (pinned) material, we're done */
	if(RNA_struct_is_a(ptr->type, &RNA_Material)) {
		return 1;
	}
	/* if we have an object, use the object material slot */
	else if(buttons_context_path_object(path)) {
		ob= path->ptr[path->len-1].data;

		if(ob && ob->type && (ob->type<OB_LAMP)) {
			ma= give_current_material(ob, ob->actcol);
			RNA_id_pointer_create(&ma->id, &path->ptr[path->len]);
			path->len++;
			return 1;
		}
	}

	/* no path to a material possible */
	return 0;
}

static Bone *find_active_bone(Bone *bone)
{
	Bone *active;

	for(; bone; bone=bone->next) {
		if(bone->flag & BONE_ACTIVE)
			return bone;

		active= find_active_bone(bone->childbase.first);
		if(active)
			return active;
	}

	return NULL;
}

static int buttons_context_path_bone(ButsContextPath *path)
{
	bArmature *arm;
	Bone *bone;
	EditBone *edbo;

	/* if we have an armature, get the active bone */
	if(buttons_context_path_data(path, OB_ARMATURE)) {
		arm= path->ptr[path->len-1].data;

		if(arm->edbo) {
			for(edbo=arm->edbo->first; edbo; edbo=edbo->next) {
				if(edbo->flag & BONE_ACTIVE) {
					RNA_pointer_create(&arm->id, &RNA_EditBone, edbo, &path->ptr[path->len]);
					path->len++;
					return 1;
				}
			}
		}
		else {
			bone= find_active_bone(arm->bonebase.first);

			if(bone) {
				RNA_pointer_create(&arm->id, &RNA_Bone, bone, &path->ptr[path->len]);
				path->len++;
				return 1;
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

	/* if we have an object, get the active particle system */
	if(buttons_context_path_object(path)) {
		ob= path->ptr[path->len-1].data;

		if(ob && ob->type == OB_MESH) {
			psys= psys_get_current(ob);

			RNA_pointer_create(&ob->id, &RNA_ParticleSystem, psys, &path->ptr[path->len]);
			path->len++;
			return 1;
		}
	}

	/* no path to a particle system possible */
	return 0;
}

static int buttons_context_path_brush(const bContext *C, ButsContextPath *path)
{
	Scene *scene;
	ToolSettings *ts;
	Brush *br= NULL;
	PointerRNA *ptr= &path->ptr[path->len-1];

	/* if we already have a (pinned) brush, we're done */
	if(RNA_struct_is_a(ptr->type, &RNA_Brush)) {
		return 1;
	}
	/* if we have a scene, use the toolsettings brushes */
	else if(buttons_context_path_scene(path)) {
		scene= path->ptr[path->len-1].data;
		ts= scene->toolsettings;

		if(scene)
			br= paint_brush(paint_get_active(scene));

		if(br) {
			RNA_id_pointer_create(&br->id, &path->ptr[path->len]);
			path->len++;

			return 1;
		}
	}

	/* no path to a brush possible */
	return 0;
}

static int buttons_context_path_texture(const bContext *C, ButsContextPath *path)
{
	Material *ma;
	Lamp *la;
	Brush *br;
	World *wo;
	Tex *tex;
	PointerRNA *ptr= &path->ptr[path->len-1];

	/* if we already have a (pinned) texture, we're done */
	if(RNA_struct_is_a(ptr->type, &RNA_Texture)) {
		return 1;
	}
	/* try brush */
	else if((path->flag & SB_BRUSH_TEX) && buttons_context_path_brush(C, path)) {
		br= path->ptr[path->len-1].data;

		if(br) {
			tex= give_current_brush_texture(br);

			RNA_id_pointer_create(&tex->id, &path->ptr[path->len]);
			path->len++;
			return 1;
		}
	}
	/* try world */
	else if((path->flag & SB_WORLD_TEX) && buttons_context_path_world(path)) {
		wo= path->ptr[path->len-1].data;

		if(wo && GS(wo->id.name)==ID_WO) {
			tex= give_current_world_texture(wo);

			RNA_id_pointer_create(&tex->id, &path->ptr[path->len]);
			path->len++;
			return 1;
		}
	}
	/* try material */
	else if(buttons_context_path_material(path)) {
		ma= path->ptr[path->len-1].data;

		if(ma) {
			tex= give_current_material_texture(ma);

			RNA_id_pointer_create(&tex->id, &path->ptr[path->len]);
			path->len++;
			return 1;
		}
	}
	/* try lamp */
	else if(buttons_context_path_data(path, OB_LAMP)) {
		la= path->ptr[path->len-1].data;

		if(la) {
			tex= give_current_lamp_texture(la);

			RNA_id_pointer_create(&tex->id, &path->ptr[path->len]);
			path->len++;
			return 1;
		}
	}
	/* TODO: material nodes */

	/* no path to a texture possible */
	return 0;
}


static int buttons_context_path(const bContext *C, ButsContextPath *path, int mainb, int flag)
{
	SpaceButs *sbuts= CTX_wm_space_buts(C);
	ID *id;
	int found;

	memset(path, 0, sizeof(*path));
	path->flag= flag;

	/* if some ID datablock is pinned, set the root pointer */
	if(sbuts->pinid) {
		id= sbuts->pinid;

		RNA_id_pointer_create(id, &path->ptr[0]);
		path->len++;
	}

	/* no pinned root, use scene as root */
	if(path->len == 0) {
		id= (ID*)CTX_data_scene(C);
		RNA_id_pointer_create(id, &path->ptr[0]);
		path->len++;
	}

	/* now for each buttons context type, we try to construct a path,
	 * tracing back recursively */
	switch(mainb) {
		case BCONTEXT_SCENE:
		case BCONTEXT_RENDER:
			found= buttons_context_path_scene(path);
			break;
		case BCONTEXT_WORLD:
			found= buttons_context_path_world(path);
			break;
		case BCONTEXT_OBJECT:
		case BCONTEXT_PHYSICS:
		case BCONTEXT_CONSTRAINT:
			found= buttons_context_path_object(path);
			break;
		case BCONTEXT_MODIFIER:
			found= buttons_context_path_modifier(path);
			break;
		case BCONTEXT_DATA:
			found= buttons_context_path_data(path, -1);
			break;
		case BCONTEXT_PARTICLE:
			found= buttons_context_path_particle(path);
			break;
		case BCONTEXT_MATERIAL:
			found= buttons_context_path_material(path);
			break;
		case BCONTEXT_TEXTURE:
			found= buttons_context_path_texture(C, path);
			break;
		case BCONTEXT_BONE:
		case BCONTEXT_BONE_CONSTRAINT:
			found= buttons_context_path_bone(path);
			if(!found)
				found= buttons_context_path_data(path, OB_ARMATURE);
			break;
		default:
			found= 0;
			break;
	}

	return found;
}

void buttons_context_compute(const bContext *C, SpaceButs *sbuts)
{
	ButsContextPath *path;
	PointerRNA *ptr;
	int a, pflag, flag= 0;

	if(!sbuts->path)
		sbuts->path= MEM_callocN(sizeof(ButsContextPath), "ButsContextPath");
	
	path= sbuts->path;
	pflag= (sbuts->flag & (SB_WORLD_TEX|SB_BRUSH_TEX));
	
	/* for each context, see if we can compute a valid path to it, if
	 * this is the case, we know we have to display the button */
	for(a=0; a<BCONTEXT_TOT; a++) {
		if(buttons_context_path(C, path, a, pflag)) {
			flag |= (1<<a);

			/* setting icon for data context */
			if(a == BCONTEXT_DATA) {
				ptr= &path->ptr[path->len-1];

				if(ptr->type)
					sbuts->dataicon= RNA_struct_ui_icon(ptr->type);
				else
					sbuts->dataicon= ICON_EMPTY_DATA;
			}
		}
	}

	/* always try to use the tab that was explicitly
	 * set to the user, so that once that context comes
	 * back, the tab is activated again */
	sbuts->mainb= sbuts->mainbuser;

	/* in case something becomes invalid, change */
	if((flag & (1 << sbuts->mainb)) == 0) {
		if(flag & BCONTEXT_OBJECT) {
			sbuts->mainb= BCONTEXT_OBJECT;
		}
		else {
			for(a=0; a<BCONTEXT_TOT; a++) {
				if(flag & (1 << a)) {
					sbuts->mainb= a;
					break;
				}
			}
		}
	}

	buttons_context_path(C, path, sbuts->mainb, pflag);

	if(!(flag & (1 << sbuts->mainb))) {
		if(flag & (1 << BCONTEXT_OBJECT))
			sbuts->mainb= BCONTEXT_OBJECT;
		else
			sbuts->mainb= BCONTEXT_SCENE;
	}

	sbuts->pathflag= flag;
}

/************************* Context Callback ************************/

int buttons_context(const bContext *C, const char *member, bContextDataResult *result)
{
	SpaceButs *sbuts= CTX_wm_space_buts(C);
	ButsContextPath *path= sbuts?sbuts->path:NULL;

	if(!path)
		return 0;

	/* here we handle context, getting data from precomputed path */
	if(CTX_data_dir(member)) {
		static const char *dir[] = {
			"world", "object", "mesh", "armature", "lattice", "curve",
			"meta_ball", "lamp", "camera", "material", "material_slot",
			"texture", "texture_slot", "bone", "edit_bone", "particle_system", "particle_system_editable",
			"cloth", "soft_body", "fluid", "smoke", "collision", "brush", NULL};

		CTX_data_dir_set(result, dir);
		return 1;
	}
	else if(CTX_data_equals(member, "world")) {
		set_pointer_type(path, result, &RNA_World);
		return 1;
	}
	else if(CTX_data_equals(member, "object")) {
		set_pointer_type(path, result, &RNA_Object);
		return 1;
	}
	else if(CTX_data_equals(member, "mesh")) {
		set_pointer_type(path, result, &RNA_Mesh);
		return 1;
	}
	else if(CTX_data_equals(member, "armature")) {
		set_pointer_type(path, result, &RNA_Armature);
		return 1;
	}
	else if(CTX_data_equals(member, "lattice")) {
		set_pointer_type(path, result, &RNA_Lattice);
		return 1;
	}
	else if(CTX_data_equals(member, "curve")) {
		set_pointer_type(path, result, &RNA_Curve);
		return 1;
	}
	else if(CTX_data_equals(member, "meta_ball")) {
		set_pointer_type(path, result, &RNA_MetaBall);
		return 1;
	}
	else if(CTX_data_equals(member, "lamp")) {
		set_pointer_type(path, result, &RNA_Lamp);
		return 1;
	}
	else if(CTX_data_equals(member, "camera")) {
		set_pointer_type(path, result, &RNA_Camera);
		return 1;
	}
	else if(CTX_data_equals(member, "material")) {
		set_pointer_type(path, result, &RNA_Material);
		return 1;
	}
	else if(CTX_data_equals(member, "texture")) {
		set_pointer_type(path, result, &RNA_Texture);
		return 1;
	}
	else if(CTX_data_equals(member, "material_slot")) {
		PointerRNA *ptr= get_pointer_type(path, &RNA_Object);

		if(ptr) {
			Object *ob= ptr->data;

			if(ob && ob->type && (ob->type<OB_LAMP) && ob->totcol)
				CTX_data_pointer_set(result, &ob->id, &RNA_MaterialSlot, ob->mat+ob->actcol-1);
		}

		return 1;
	}
	else if(CTX_data_equals(member, "texture_slot")) {
		PointerRNA *ptr;

		if((ptr=get_pointer_type(path, &RNA_Material))) {
			Material *ma= ptr->data; /* should this be made a different option? */
			Material *ma_node= give_node_material(ma);
			ma= ma_node?ma_node:ma;

			if(ma)
				CTX_data_pointer_set(result, &ma->id, &RNA_MaterialTextureSlot, ma->mtex[(int)ma->texact]);
		}
		else if((ptr=get_pointer_type(path, &RNA_Lamp))) {
			Lamp *la= ptr->data;

			if(la)
				CTX_data_pointer_set(result, &la->id, &RNA_LampTextureSlot, la->mtex[(int)la->texact]);
		}
		else if((ptr=get_pointer_type(path, &RNA_World))) {
			World *wo= ptr->data;

			if(wo)
				CTX_data_pointer_set(result, &wo->id, &RNA_WorldTextureSlot, wo->mtex[(int)wo->texact]);
		}
		else if((ptr=get_pointer_type(path, &RNA_Brush))) { /* how to get this into context? */
			Brush *br= ptr->data;

			if(br)
				CTX_data_pointer_set(result, &br->id, &RNA_BrushTextureSlot, br->mtex[(int)br->texact]);
		}

		return 1;
	}
	else if(CTX_data_equals(member, "bone")) {
		set_pointer_type(path, result, &RNA_Bone);
		return 1;
	}
	else if(CTX_data_equals(member, "edit_bone")) {
		set_pointer_type(path, result, &RNA_EditBone);
		return 1;
	}
	else if(CTX_data_equals(member, "particle_system")) {
		set_pointer_type(path, result, &RNA_ParticleSystem);
		return 1;
	}
	else if(CTX_data_equals(member, "particle_system_editable")) {
		if(PE_poll((bContext*)C))
			set_pointer_type(path, result, &RNA_ParticleSystem);
		else
			CTX_data_pointer_set(result, NULL, &RNA_ParticleSystem, NULL);
		return 1;
	}	
	else if(CTX_data_equals(member, "cloth")) {
		PointerRNA *ptr= get_pointer_type(path, &RNA_Object);

		if(ptr && ptr->data) {
			Object *ob= ptr->data;
			ModifierData *md= modifiers_findByType(ob, eModifierType_Cloth);
			CTX_data_pointer_set(result, &ob->id, &RNA_ClothModifier, md);
			return 1;
		}
	}
	else if(CTX_data_equals(member, "soft_body")) {
		PointerRNA *ptr= get_pointer_type(path, &RNA_Object);

		if(ptr && ptr->data) {
			Object *ob= ptr->data;
			ModifierData *md= modifiers_findByType(ob, eModifierType_Softbody);
			CTX_data_pointer_set(result, &ob->id, &RNA_SoftBodyModifier, md);
			return 1;
		}
	}
	else if(CTX_data_equals(member, "fluid")) {
		PointerRNA *ptr= get_pointer_type(path, &RNA_Object);

		if(ptr && ptr->data) {
			Object *ob= ptr->data;
			ModifierData *md= modifiers_findByType(ob, eModifierType_Fluidsim);
			CTX_data_pointer_set(result, &ob->id, &RNA_FluidSimulationModifier, md);
			return 1;
		}
	}
	
	else if(CTX_data_equals(member, "smoke")) {
		PointerRNA *ptr= get_pointer_type(path, &RNA_Object);

		if(ptr && ptr->data) {
			Object *ob= ptr->data;
			ModifierData *md= modifiers_findByType(ob, eModifierType_Smoke);
			CTX_data_pointer_set(result, &ob->id, &RNA_SmokeModifier, md);
			return 1;
		}
	}
	else if(CTX_data_equals(member, "collision")) {
		PointerRNA *ptr= get_pointer_type(path, &RNA_Object);

		if(ptr && ptr->data) {
			Object *ob= ptr->data;
			ModifierData *md= modifiers_findByType(ob, eModifierType_Collision);
			CTX_data_pointer_set(result, &ob->id, &RNA_CollisionModifier, md);
			return 1;
		}
	}
	else if(CTX_data_equals(member, "brush")) {
		set_pointer_type(path, result, &RNA_Brush);
		return 1;
	}

	return 0;
}

/************************* Drawing the Path ************************/

static void pin_cb(bContext *C, void *arg1, void *arg2)
{
	SpaceButs *sbuts= CTX_wm_space_buts(C);
	ButsContextPath *path= sbuts->path;
	PointerRNA *ptr;
	int a;

	if(sbuts->flag & SB_PIN_CONTEXT) {
		if(path->len) {
			for(a=path->len-1; a>=0; a--) {
				ptr= &path->ptr[a];

				if(ptr->id.data) {
					sbuts->pinid= ptr->id.data;
					break;
				}
			}
		}
	}
	else
		sbuts->pinid= NULL;
	
	ED_area_tag_redraw(CTX_wm_area(C));
}

void buttons_context_draw(const bContext *C, uiLayout *layout)
{
	SpaceButs *sbuts= CTX_wm_space_buts(C);
	ButsContextPath *path= sbuts->path;
	uiLayout *row;
	uiBlock *block;
	uiBut *but;
	PointerRNA *ptr;
	char namebuf[128], *name;
	int a, icon;

	if(!path)
		return;

	row= uiLayoutRow(layout, 1);
	uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_LEFT);

	block= uiLayoutGetBlock(row);
	uiBlockSetEmboss(block, UI_EMBOSSN);
	but= uiDefIconButBitC(block, ICONTOG, SB_PIN_CONTEXT, 0, ICON_UNPINNED, 0, 0, UI_UNIT_X, UI_UNIT_Y, &sbuts->flag, 0, 0, 0, 0, "Follow context or keep fixed datablock displayed.");
	uiButSetFunc(but, pin_cb, NULL, NULL);

	for(a=0; a<path->len; a++) {
		ptr= &path->ptr[a];

		if(a != 0)
			uiDefIconBut(block, LABEL, 0, VICON_SMALL_TRI_RIGHT, 0, 0, 10, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");

		if(ptr->data) {
			icon= RNA_struct_ui_icon(ptr->type);
			name= RNA_struct_name_get_alloc(ptr, namebuf, sizeof(namebuf));

			if(name) {
				if(!ELEM(sbuts->mainb, BCONTEXT_RENDER, BCONTEXT_SCENE) && ptr->type == &RNA_Scene)
					uiItemL(row, "", icon); /* save some space */
				else
					uiItemL(row, name, icon);

				if(name != namebuf)
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

	pt= MEM_callocN(sizeof(PanelType), "spacetype buttons panel context");
	strcpy(pt->idname, "BUTTONS_PT_context");
	strcpy(pt->label, "Context");
	pt->draw= buttons_panel_context;
	pt->flag= PNL_NO_HEADER;
	BLI_addtail(&art->paneltypes, pt);
}
