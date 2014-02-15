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

/** \file blender/blenkernel/intern/library.c
 *  \ingroup bke
 *
 * Contains management of ID's and libraries
 * allocate and free of all library data
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

/* all types are needed here, in order to do memory operations */
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_group_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_mask_types.h"
#include "DNA_nla_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_speaker_types.h"
#include "DNA_sound_types.h"
#include "DNA_text_types.h"
#include "DNA_vfont_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_bpath.h"
#include "BKE_brush.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_gpencil.h"
#include "BKE_idprop.h"
#include "BKE_icons.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_lamp.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_linestyle.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_main.h"
#include "BKE_mball.h"
#include "BKE_movieclip.h"
#include "BKE_mask.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_packedFile.h"
#include "BKE_speaker.h"
#include "BKE_sound.h"
#include "BKE_screen.h"
#include "BKE_scene.h"
#include "BKE_text.h"
#include "BKE_texture.h"
#include "BKE_world.h"

#include "RNA_access.h"

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

/* GS reads the memory pointed at in a specific ordering. 
 * only use this definition, makes little and big endian systems
 * work fine, in conjunction with MAKE_ID */

/* ************* general ************************ */


/* this has to be called from each make_local_* func, we could call
 * from id_make_local() but then the make local functions would not be self
 * contained.
 * also note that the id _must_ have a library - campbell */
void BKE_id_lib_local_paths(Main *bmain, Library *lib, ID *id)
{
	char *bpath_user_data[2] = {bmain->name, lib->filepath};

	BKE_bpath_traverse_id(bmain, id,
	                      BKE_bpath_relocate_visitor,
	                      BKE_BPATH_TRAVERSE_SKIP_MULTIFILE,
	                      bpath_user_data);
}

void id_lib_extern(ID *id)
{
	if (id) {
		if (id->flag & LIB_INDIRECT) {
			id->flag -= LIB_INDIRECT;
			id->flag |= LIB_EXTERN;
		}
	}
}

/* ensure we have a real user */
void id_us_ensure_real(ID *id)
{
	if (id) {
		if (ID_REAL_USERS(id) <= 0) {
			id->us = MAX2(id->us, 0) + 1;
		}
	}
}

void id_us_plus(ID *id)
{
	if (id) {
		id->us++;
		if (id->flag & LIB_INDIRECT) {
			id->flag -= LIB_INDIRECT;
			id->flag |= LIB_EXTERN;
		}
	}
}

/* decrements the user count for *id. */
void id_us_min(ID *id)
{
	if (id) {
		if (id->us < 2 && (id->flag & LIB_FAKEUSER)) {
			id->us = 1;
		}
		else if (id->us <= 0) {
			printf("ID user decrement error: %s\n", id->name);
		}
		else {
			id->us--;
		}
	}
}

/* calls the appropriate make_local method for the block, unless test. Returns true
 * if the block can be made local. */
bool id_make_local(ID *id, bool test)
{
	if (id->flag & LIB_INDIRECT)
		return false;

	switch (GS(id->name)) {
		case ID_SCE:
			return false; /* not implemented */
		case ID_LI:
			return false; /* can't be linked */
		case ID_OB:
			if (!test) BKE_object_make_local((Object *)id);
			return true;
		case ID_ME:
			if (!test) {
				BKE_mesh_make_local((Mesh *)id);
				BKE_key_make_local(((Mesh *)id)->key);
			}
			return true;
		case ID_CU:
			if (!test) {
				BKE_curve_make_local((Curve *)id);
				BKE_key_make_local(((Curve *)id)->key);
			}
			return true;
		case ID_MB:
			if (!test) BKE_mball_make_local((MetaBall *)id);
			return true;
		case ID_MA:
			if (!test) BKE_material_make_local((Material *)id);
			return true;
		case ID_TE:
			if (!test) BKE_texture_make_local((Tex *)id);
			return true;
		case ID_IM:
			if (!test) BKE_image_make_local((Image *)id);
			return true;
		case ID_LT:
			if (!test) {
				BKE_lattice_make_local((Lattice *)id);
				BKE_key_make_local(((Lattice *)id)->key);
			}
			return true;
		case ID_LA:
			if (!test) BKE_lamp_make_local((Lamp *)id);
			return true;
		case ID_CA:
			if (!test) BKE_camera_make_local((Camera *)id);
			return true;
		case ID_SPK:
			if (!test) BKE_speaker_make_local((Speaker *)id);
			return true;
		case ID_IP:
			return false; /* deprecated */
		case ID_KE:
			if (!test) BKE_key_make_local((Key *)id);
			return true;
		case ID_WO:
			if (!test) BKE_world_make_local((World *)id);
			return true;
		case ID_SCR:
			return false; /* can't be linked */
		case ID_VF:
			return false; /* not implemented */
		case ID_TXT:
			return false; /* not implemented */
		case ID_SCRIPT:
			return false; /* deprecated */
		case ID_SO:
			return false; /* not implemented */
		case ID_GR:
			return false; /* not implemented */
		case ID_AR:
			if (!test) BKE_armature_make_local((bArmature *)id);
			return true;
		case ID_AC:
			if (!test) BKE_action_make_local((bAction *)id);
			return true;
		case ID_NT:
			if (!test) ntreeMakeLocal((bNodeTree *)id);
			return true;
		case ID_BR:
			if (!test) BKE_brush_make_local((Brush *)id);
			return true;
		case ID_PA:
			if (!test) BKE_particlesettings_make_local((ParticleSettings *)id);
			return true;
		case ID_WM:
			return false; /* can't be linked */
		case ID_GD:
			return false; /* not implemented */
		case ID_LS:
			return 0; /* not implemented */
	}

	return false;
}

/**
 * Invokes the appropriate copy method for the block and returns the result in
 * newid, unless test. Returns true iff the block can be copied.
 */
bool id_copy(ID *id, ID **newid, bool test)
{
	if (!test) *newid = NULL;

	/* conventions:
	 * - make shallow copy, only this ID block
	 * - id.us of the new ID is set to 1 */
	switch (GS(id->name)) {
		case ID_SCE:
			return false;  /* can't be copied from here */
		case ID_LI:
			return false;  /* can't be copied from here */
		case ID_OB:
			if (!test) *newid = (ID *)BKE_object_copy((Object *)id);
			return true;
		case ID_ME:
			if (!test) *newid = (ID *)BKE_mesh_copy((Mesh *)id);
			return true;
		case ID_CU:
			if (!test) *newid = (ID *)BKE_curve_copy((Curve *)id);
			return true;
		case ID_MB:
			if (!test) *newid = (ID *)BKE_mball_copy((MetaBall *)id);
			return true;
		case ID_MA:
			if (!test) *newid = (ID *)BKE_material_copy((Material *)id);
			return true;
		case ID_TE:
			if (!test) *newid = (ID *)BKE_texture_copy((Tex *)id);
			return true;
		case ID_IM:
			if (!test) *newid = (ID *)BKE_image_copy(G.main, (Image *)id);
			return true;
		case ID_LT:
			if (!test) *newid = (ID *)BKE_lattice_copy((Lattice *)id);
			return true;
		case ID_LA:
			if (!test) *newid = (ID *)BKE_lamp_copy((Lamp *)id);
			return true;
		case ID_SPK:
			if (!test) *newid = (ID *)BKE_speaker_copy((Speaker *)id);
			return true;
		case ID_CA:
			if (!test) *newid = (ID *)BKE_camera_copy((Camera *)id);
			return true;
		case ID_IP:
			return false;  /* deprecated */
		case ID_KE:
			if (!test) *newid = (ID *)BKE_key_copy((Key *)id);
			return true;
		case ID_WO:
			if (!test) *newid = (ID *)BKE_world_copy((World *)id);
			return true;
		case ID_SCR:
			return false;  /* can't be copied from here */
		case ID_VF:
			return false;  /* not implemented */
		case ID_TXT:
			if (!test) *newid = (ID *)BKE_text_copy((Text *)id);
			return true;
		case ID_SCRIPT:
			return false;  /* deprecated */
		case ID_SO:
			return false;  /* not implemented */
		case ID_GR:
			if (!test) *newid = (ID *)BKE_group_copy((Group *)id);
			return true;
		case ID_AR:
			if (!test) *newid = (ID *)BKE_armature_copy((bArmature *)id);
			return true;
		case ID_AC:
			if (!test) *newid = (ID *)BKE_action_copy((bAction *)id);
			return true;
		case ID_NT:
			if (!test) *newid = (ID *)ntreeCopyTree((bNodeTree *)id);
			return true;
		case ID_BR:
			if (!test) *newid = (ID *)BKE_brush_copy((Brush *)id);
			return true;
		case ID_PA:
			if (!test) *newid = (ID *)BKE_particlesettings_copy((ParticleSettings *)id);
			return true;
		case ID_WM:
			return false;  /* can't be copied from here */
		case ID_GD:
			return false;  /* not implemented */
		case ID_MSK:
			if (!test) *newid = (ID *)BKE_mask_copy((Mask *)id);
			return true;
		case ID_LS:
			if (!test) *newid = (ID *)BKE_copy_linestyle((FreestyleLineStyle *)id);
			return 1;
	}
	
	return false;
}

bool id_unlink(ID *id, int test)
{
	Main *mainlib = G.main;
	short type = GS(id->name);

	switch (type) {
		case ID_TXT:
			if (test) return true;
			BKE_text_unlink(mainlib, (Text *)id);
			break;
		case ID_GR:
			if (test) return true;
			BKE_group_unlink((Group *)id);
			break;
		case ID_OB:
			if (test) return true;
			BKE_object_unlink((Object *)id);
			break;
	}

	if (id->us == 0) {
		if (test) return true;

		BKE_libblock_free(mainlib, id);

		return true;
	}

	return false;
}

bool id_single_user(bContext *C, ID *id, PointerRNA *ptr, PropertyRNA *prop)
{
	ID *newid = NULL;
	PointerRNA idptr;
	
	if (id) {
		/* if property isn't editable, we're going to have an extra block hanging around until we save */
		if (RNA_property_editable(ptr, prop)) {
			if (id_copy(id, &newid, false) && newid) {
				/* copy animation actions too */
				BKE_copy_animdata_id_action(id);
				/* us is 1 by convention, but RNA_property_pointer_set
				 * will also increment it, so set it to zero */
				newid->us = 0;
				
				/* assign copy */
				RNA_id_pointer_create(newid, &idptr);
				RNA_property_pointer_set(ptr, prop, idptr);
				RNA_property_update(C, ptr, prop);
				
				return true;
			}
		}
	}
	
	return false;
}

ListBase *which_libbase(Main *mainlib, short type)
{
	switch (type) {
		case ID_SCE:
			return &(mainlib->scene);
		case ID_LI:
			return &(mainlib->library);
		case ID_OB:
			return &(mainlib->object);
		case ID_ME:
			return &(mainlib->mesh);
		case ID_CU:
			return &(mainlib->curve);
		case ID_MB:
			return &(mainlib->mball);
		case ID_MA:
			return &(mainlib->mat);
		case ID_TE:
			return &(mainlib->tex);
		case ID_IM:
			return &(mainlib->image);
		case ID_LT:
			return &(mainlib->latt);
		case ID_LA:
			return &(mainlib->lamp);
		case ID_CA:
			return &(mainlib->camera);
		case ID_IP:
			return &(mainlib->ipo);
		case ID_KE:
			return &(mainlib->key);
		case ID_WO:
			return &(mainlib->world);
		case ID_SCR:
			return &(mainlib->screen);
		case ID_VF:
			return &(mainlib->vfont);
		case ID_TXT:
			return &(mainlib->text);
		case ID_SCRIPT:
			return &(mainlib->script);
		case ID_SPK:
			return &(mainlib->speaker);
		case ID_SO:
			return &(mainlib->sound);
		case ID_GR:
			return &(mainlib->group);
		case ID_AR:
			return &(mainlib->armature);
		case ID_AC:
			return &(mainlib->action);
		case ID_NT:
			return &(mainlib->nodetree);
		case ID_BR:
			return &(mainlib->brush);
		case ID_PA:
			return &(mainlib->particle);
		case ID_WM:
			return &(mainlib->wm);
		case ID_GD:
			return &(mainlib->gpencil);
		case ID_MC:
			return &(mainlib->movieclip);
		case ID_MSK:
			return &(mainlib->mask);
		case ID_LS:
			return &(mainlib->linestyle);
	}
	return NULL;
}

/* Flag all ids in listbase */
void BKE_main_id_flag_listbase(ListBase *lb, const short flag, const bool value)
{
	ID *id;
	if (value) {
		for (id = lb->first; id; id = id->next) id->flag |= flag;
	}
	else {
		const short nflag = ~flag;
		for (id = lb->first; id; id = id->next) id->flag &= nflag;
	}
}

/* Flag all ids in listbase */
void BKE_main_id_flag_all(Main *bmain, const short flag, const bool value)
{
	ListBase *lbarray[MAX_LIBARRAY];
	int a;
	a = set_listbasepointers(bmain, lbarray);
	while (a--) {
		BKE_main_id_flag_listbase(lbarray[a], flag, value);
	}
}

void BKE_main_lib_objects_recalc_all(Main *bmain)
{
	Object *ob;

	/* flag for full recalc */
	for (ob = bmain->object.first; ob; ob = ob->id.next)
		if (ob->id.lib)
			ob->recalc |= OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME;

	DAG_id_type_tag(bmain, ID_OB);
}

/**
 * puts into array *lb pointers to all the ListBase structs in main,
 * and returns the number of them as the function result. This is useful for
 * generic traversal of all the blocks in a Main (by traversing all the
 * lists in turn), without worrying about block types.
 *
 * \note MAX_LIBARRAY define should match this code */
int set_listbasepointers(Main *main, ListBase **lb)
{
	int a = 0;

	/* BACKWARDS! also watch order of free-ing! (mesh<->mat), first items freed last.
	 * This is important because freeing data decreases usercounts of other datablocks,
	 * if this data is its self freed it can crash. */
	lb[a++] = &(main->ipo);
	lb[a++] = &(main->action); // xxx moved here to avoid problems when freeing with animato (aligorith)
	lb[a++] = &(main->key);
	lb[a++] = &(main->gpencil); /* referenced by nodes, objects, view, scene etc, before to free after. */
	lb[a++] = &(main->nodetree);
	lb[a++] = &(main->image);
	lb[a++] = &(main->tex);
	lb[a++] = &(main->mat);
	lb[a++] = &(main->vfont);
	
	/* Important!: When adding a new object type,
	 * the specific data should be inserted here 
	 */

	lb[a++] = &(main->armature);

	lb[a++] = &(main->mesh);
	lb[a++] = &(main->curve);
	lb[a++] = &(main->mball);

	lb[a++] = &(main->latt);
	lb[a++] = &(main->lamp);
	lb[a++] = &(main->camera);

	lb[a++] = &(main->text);
	lb[a++] = &(main->sound);
	lb[a++] = &(main->group);
	lb[a++] = &(main->brush);
	lb[a++] = &(main->script);
	lb[a++] = &(main->particle);
	lb[a++] = &(main->speaker);

	lb[a++] = &(main->world);
	lb[a++] = &(main->screen);
	lb[a++] = &(main->object);
	lb[a++] = &(main->linestyle); /* referenced by scenes */
	lb[a++] = &(main->scene);
	lb[a++] = &(main->library);
	lb[a++] = &(main->wm);
	lb[a++] = &(main->movieclip);
	lb[a++] = &(main->mask);
	
	lb[a] = NULL;

	return a;
}

/* *********** ALLOC AND FREE *****************
 *
 * BKE_libblock_free(ListBase *lb, ID *id )
 * provide a list-basis and datablock, but only ID is read
 *
 * void *BKE_libblock_alloc(ListBase *lb, type, name)
 * inserts in list and returns a new ID
 *
 * **************************** */

/**
 * Allocates and returns memory of the right size for the specified block type,
 * initialized to zero.
 */
static ID *alloc_libblock_notest(short type)
{
	ID *id = NULL;
	
	switch (type) {
		case ID_SCE:
			id = MEM_callocN(sizeof(Scene), "scene");
			break;
		case ID_LI:
			id = MEM_callocN(sizeof(Library), "library");
			break;
		case ID_OB:
			id = MEM_callocN(sizeof(Object), "object");
			break;
		case ID_ME:
			id = MEM_callocN(sizeof(Mesh), "mesh");
			break;
		case ID_CU:
			id = MEM_callocN(sizeof(Curve), "curve");
			break;
		case ID_MB:
			id = MEM_callocN(sizeof(MetaBall), "mball");
			break;
		case ID_MA:
			id = MEM_callocN(sizeof(Material), "mat");
			break;
		case ID_TE:
			id = MEM_callocN(sizeof(Tex), "tex");
			break;
		case ID_IM:
			id = MEM_callocN(sizeof(Image), "image");
			break;
		case ID_LT:
			id = MEM_callocN(sizeof(Lattice), "latt");
			break;
		case ID_LA:
			id = MEM_callocN(sizeof(Lamp), "lamp");
			break;
		case ID_CA:
			id = MEM_callocN(sizeof(Camera), "camera");
			break;
		case ID_IP:
			id = MEM_callocN(sizeof(Ipo), "ipo");
			break;
		case ID_KE:
			id = MEM_callocN(sizeof(Key), "key");
			break;
		case ID_WO:
			id = MEM_callocN(sizeof(World), "world");
			break;
		case ID_SCR:
			id = MEM_callocN(sizeof(bScreen), "screen");
			break;
		case ID_VF:
			id = MEM_callocN(sizeof(VFont), "vfont");
			break;
		case ID_TXT:
			id = MEM_callocN(sizeof(Text), "text");
			break;
		case ID_SCRIPT:
			//XXX id = MEM_callocN(sizeof(Script), "script");
			break;
		case ID_SPK:
			id = MEM_callocN(sizeof(Speaker), "speaker");
			break;
		case ID_SO:
			id = MEM_callocN(sizeof(bSound), "sound");
			break;
		case ID_GR:
			id = MEM_callocN(sizeof(Group), "group");
			break;
		case ID_AR:
			id = MEM_callocN(sizeof(bArmature), "armature");
			break;
		case ID_AC:
			id = MEM_callocN(sizeof(bAction), "action");
			break;
		case ID_NT:
			id = MEM_callocN(sizeof(bNodeTree), "nodetree");
			break;
		case ID_BR:
			id = MEM_callocN(sizeof(Brush), "brush");
			break;
		case ID_PA:
			id = MEM_callocN(sizeof(ParticleSettings), "ParticleSettings");
			break;
		case ID_WM:
			id = MEM_callocN(sizeof(wmWindowManager), "Window manager");
			break;
		case ID_GD:
			id = MEM_callocN(sizeof(bGPdata), "Grease Pencil");
			break;
		case ID_MC:
			id = MEM_callocN(sizeof(MovieClip), "Movie Clip");
			break;
		case ID_MSK:
			id = MEM_callocN(sizeof(Mask), "Mask");
			break;
		case ID_LS:
			id = MEM_callocN(sizeof(FreestyleLineStyle), "Freestyle Line Style");
			break;
	}
	return id;
}

/**
 * Allocates and returns a block of the specified type, with the specified name
 * (adjusted as necessary to ensure uniqueness), and appended to the specified list.
 * The user count is set to 1, all other content (apart from name and links) being
 * initialized to zero.
 */
void *BKE_libblock_alloc(Main *bmain, short type, const char *name)
{
	ID *id = NULL;
	ListBase *lb = which_libbase(bmain, type);
	
	id = alloc_libblock_notest(type);
	if (id) {
		BLI_addtail(lb, id);
		id->us = 1;
		id->icon_id = 0;
		*( (short *)id->name) = type;
		new_id(lb, id, name);
		/* alphabetic insertion: is in new_id */
	}
	DAG_id_type_tag(bmain, type);
	return id;
}

/* by spec, animdata is first item after ID */
/* and, trust that BKE_animdata_from_id() will only find AnimData for valid ID-types */
static void id_copy_animdata(ID *id, const bool do_action)
{
	AnimData *adt = BKE_animdata_from_id(id);
	
	if (adt) {
		IdAdtTemplate *iat = (IdAdtTemplate *)id;
		iat->adt = BKE_copy_animdata(iat->adt, do_action); /* could be set to false, need to investigate */
	}
}

/* material nodes use this since they are not treated as libdata */
void BKE_libblock_copy_data(ID *id, const ID *id_from, const bool do_action)
{
	if (id_from->properties)
		id->properties = IDP_CopyProperty(id_from->properties);

	/* the duplicate should get a copy of the animdata */
	id_copy_animdata(id, do_action);
}

/* used everywhere in blenkernel */
void *BKE_libblock_copy_ex(Main *bmain, ID *id)
{
	ID *idn;
	size_t idn_len;

	idn = BKE_libblock_alloc(bmain, GS(id->name), id->name + 2);

	assert(idn != NULL);

	idn_len = MEM_allocN_len(idn);
	if ((int)idn_len - (int)sizeof(ID) > 0) { /* signed to allow neg result */
		const char *cp = (const char *)id;
		char *cpn = (char *)idn;

		memcpy(cpn + sizeof(ID), cp + sizeof(ID), idn_len - sizeof(ID));
	}
	
	id->newid = idn;
	idn->flag |= LIB_NEW;

	BKE_libblock_copy_data(idn, id, false);
	
	return idn;
}

void *BKE_libblock_copy(ID *id)
{
	return BKE_libblock_copy_ex(G.main, id);
}

static void BKE_library_free(Library *lib)
{
	if (lib->packedfile)
		freePackedFile(lib->packedfile);
}

static void (*free_windowmanager_cb)(bContext *, wmWindowManager *) = NULL;

void set_free_windowmanager_cb(void (*func)(bContext *C, wmWindowManager *) )
{
	free_windowmanager_cb = func;
}

static void (*free_notifier_reference_cb)(const void *) = NULL;

void set_free_notifier_reference_cb(void (*func)(const void *) )
{
	free_notifier_reference_cb = func;
}


static void animdata_dtar_clear_cb(ID *UNUSED(id), AnimData *adt, void *userdata)
{
	ChannelDriver *driver;
	FCurve *fcu;

	/* find the driver this belongs to and update it */
	for (fcu = adt->drivers.first; fcu; fcu = fcu->next) {
		driver = fcu->driver;
		
		if (driver) {
			DriverVar *dvar;
			for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
				DRIVER_TARGETS_USED_LOOPER(dvar) 
				{
					if (dtar->id == userdata)
						dtar->id = NULL;
				}
				DRIVER_TARGETS_LOOPER_END
			}
		}
	}
}

void BKE_libblock_free_data(ID *id)
{
	Main *bmain = G.main;  /* should eventually be an arg */
	
	if (id->properties) {
		IDP_FreeProperty(id->properties);
		MEM_freeN(id->properties);
	}
	
	/* this ID may be a driver target! */
	BKE_animdata_main_cb(bmain, animdata_dtar_clear_cb, (void *)id);
}

/* used in headerbuttons.c image.c mesh.c screen.c sound.c and library.c */
void BKE_libblock_free_ex(Main *bmain, void *idv, bool do_id_user)
{
	ID *id = idv;
	short type = GS(id->name);
	ListBase *lb = which_libbase(bmain, type);

	DAG_id_type_tag(bmain, type);

#ifdef WITH_PYTHON
	BPY_id_release(id);
#endif

	switch (type) {    /* GetShort from util.h */
		case ID_SCE:
			BKE_scene_free((Scene *)id);
			break;
		case ID_LI:
			BKE_library_free((Library *)id);
			break;
		case ID_OB:
			BKE_object_free_ex((Object *)id, do_id_user);
			break;
		case ID_ME:
			BKE_mesh_free((Mesh *)id, 1);
			break;
		case ID_CU:
			BKE_curve_free((Curve *)id);
			break;
		case ID_MB:
			BKE_mball_free((MetaBall *)id);
			break;
		case ID_MA:
			BKE_material_free((Material *)id);
			break;
		case ID_TE:
			BKE_texture_free((Tex *)id);
			break;
		case ID_IM:
			BKE_image_free((Image *)id);
			break;
		case ID_LT:
			BKE_lattice_free((Lattice *)id);
			break;
		case ID_LA:
			BKE_lamp_free((Lamp *)id);
			break;
		case ID_CA:
			BKE_camera_free((Camera *) id);
			break;
		case ID_IP:
			BKE_ipo_free((Ipo *)id);
			break;
		case ID_KE:
			BKE_key_free((Key *)id);
			break;
		case ID_WO:
			BKE_world_free((World *)id);
			break;
		case ID_SCR:
			BKE_screen_free((bScreen *)id);
			break;
		case ID_VF:
			BKE_vfont_free((VFont *)id);
			break;
		case ID_TXT:
			BKE_text_free((Text *)id);
			break;
		case ID_SCRIPT:
			/* deprecated */
			break;
		case ID_SPK:
			BKE_speaker_free((Speaker *)id);
			break;
		case ID_SO:
			BKE_sound_free((bSound *)id);
			break;
		case ID_GR:
			BKE_group_free((Group *)id);
			break;
		case ID_AR:
			BKE_armature_free((bArmature *)id);
			break;
		case ID_AC:
			BKE_action_free((bAction *)id);
			break;
		case ID_NT:
			ntreeFreeTree_ex((bNodeTree *)id, do_id_user);
			break;
		case ID_BR:
			BKE_brush_free((Brush *)id);
			break;
		case ID_PA:
			BKE_particlesettings_free((ParticleSettings *)id);
			break;
		case ID_WM:
			if (free_windowmanager_cb)
				free_windowmanager_cb(NULL, (wmWindowManager *)id);
			break;
		case ID_GD:
			BKE_gpencil_free((bGPdata *)id);
			break;
		case ID_MC:
			BKE_movieclip_free((MovieClip *)id);
			break;
		case ID_MSK:
			BKE_mask_free(bmain, (Mask *)id);
			break;
		case ID_LS:
			BKE_free_linestyle((FreestyleLineStyle *)id);
			break;
	}

	/* avoid notifying on removed data */
	if (free_notifier_reference_cb)
		free_notifier_reference_cb(id);

	BLI_remlink(lb, id);

	BKE_libblock_free_data(id);

	MEM_freeN(id);
}

void BKE_libblock_free(Main *bmain, void *idv)
{
	BKE_libblock_free_ex(bmain, idv, true);
}

void BKE_libblock_free_us(Main *bmain, void *idv)      /* test users */
{
	ID *id = idv;
	
	id->us--;

	if (id->us < 0) {
		if (id->lib) printf("ERROR block %s %s users %d\n", id->lib->name, id->name, id->us);
		else printf("ERROR block %s users %d\n", id->name, id->us);
	}
	if (id->us == 0) {
		if (GS(id->name) == ID_OB) BKE_object_unlink((Object *)id);
		
		BKE_libblock_free(bmain, id);
	}
}

Main *BKE_main_new(void)
{
	Main *bmain = MEM_callocN(sizeof(Main), "new main");
	bmain->eval_ctx = MEM_callocN(sizeof(EvaluationContext), "EvaluationContext");
	return bmain;
}

void BKE_main_free(Main *mainvar)
{
	/* also call when reading a file, erase all, etc */
	ListBase *lbarray[MAX_LIBARRAY];
	int a;

	a = set_listbasepointers(mainvar, lbarray);
	while (a--) {
		ListBase *lb = lbarray[a];
		ID *id;
		
		while ( (id = lb->first) ) {
#if 1
			BKE_libblock_free_ex(mainvar, id, false);
#else
			/* errors freeing ID's can be hard to track down,
			 * enable this so valgrind will give the line number in its error log */
			switch (a) {
				case   0: BKE_libblock_free_ex(mainvar, id, false); break;
				case   1: BKE_libblock_free_ex(mainvar, id, false); break;
				case   2: BKE_libblock_free_ex(mainvar, id, false); break;
				case   3: BKE_libblock_free_ex(mainvar, id, false); break;
				case   4: BKE_libblock_free_ex(mainvar, id, false); break;
				case   5: BKE_libblock_free_ex(mainvar, id, false); break;
				case   6: BKE_libblock_free_ex(mainvar, id, false); break;
				case   7: BKE_libblock_free_ex(mainvar, id, false); break;
				case   8: BKE_libblock_free_ex(mainvar, id, false); break;
				case   9: BKE_libblock_free_ex(mainvar, id, false); break;
				case  10: BKE_libblock_free_ex(mainvar, id, false); break;
				case  11: BKE_libblock_free_ex(mainvar, id, false); break;
				case  12: BKE_libblock_free_ex(mainvar, id, false); break;
				case  13: BKE_libblock_free_ex(mainvar, id, false); break;
				case  14: BKE_libblock_free_ex(mainvar, id, false); break;
				case  15: BKE_libblock_free_ex(mainvar, id, false); break;
				case  16: BKE_libblock_free_ex(mainvar, id, false); break;
				case  17: BKE_libblock_free_ex(mainvar, id, false); break;
				case  18: BKE_libblock_free_ex(mainvar, id, false); break;
				case  19: BKE_libblock_free_ex(mainvar, id, false); break;
				case  20: BKE_libblock_free_ex(mainvar, id, false); break;
				case  21: BKE_libblock_free_ex(mainvar, id, false); break;
				case  22: BKE_libblock_free_ex(mainvar, id, false); break;
				case  23: BKE_libblock_free_ex(mainvar, id, false); break;
				case  24: BKE_libblock_free_ex(mainvar, id, false); break;
				case  25: BKE_libblock_free_ex(mainvar, id, false); break;
				case  26: BKE_libblock_free_ex(mainvar, id, false); break;
				case  27: BKE_libblock_free_ex(mainvar, id, false); break;
				case  28: BKE_libblock_free_ex(mainvar, id, false); break;
				case  29: BKE_libblock_free_ex(mainvar, id, false); break;
				case  30: BKE_libblock_free_ex(mainvar, id, false); break;
				case  31: BKE_libblock_free_ex(mainvar, id, false); break;
				case  32: BKE_libblock_free_ex(mainvar, id, false); break;
				default:
					BLI_assert(0);
					break;
			}
#endif
		}
	}

	MEM_freeN(mainvar->eval_ctx);
	MEM_freeN(mainvar);
}

/* ***************** ID ************************ */


ID *BKE_libblock_find_name(const short type, const char *name)      /* type: "OB" or "MA" etc */
{
	ListBase *lb = which_libbase(G.main, type);
	BLI_assert(lb != NULL);
	return BLI_findstring(lb, name, offsetof(ID, name) + 2);
}

void id_sort_by_name(ListBase *lb, ID *id)
{
	ID *idtest;
	
	/* insert alphabetically */
	if (lb->first != lb->last) {
		BLI_remlink(lb, id);
		
		idtest = lb->first;
		while (idtest) {
			if (BLI_strcasecmp(idtest->name, id->name) > 0 || (idtest->lib && !id->lib)) {
				BLI_insertlinkbefore(lb, idtest, id);
				break;
			}
			idtest = idtest->next;
		}
		/* as last */
		if (idtest == NULL) {
			BLI_addtail(lb, id);
		}
	}
	
}

/**
 * Check to see if there is an ID with the same name as 'name'.
 * Returns the ID if so, if not, returns NULL
 */
static ID *is_dupid(ListBase *lb, ID *id, const char *name)
{
	ID *idtest = NULL;
	
	for (idtest = lb->first; idtest; idtest = idtest->next) {
		/* if idtest is not a lib */ 
		if (id != idtest && idtest->lib == NULL) {
			/* do not test alphabetic! */
			/* optimized */
			if (idtest->name[2] == name[0]) {
				if (strcmp(name, idtest->name + 2) == 0) break;
			}
		}
	}
	
	return idtest;
}

/**
 * Check to see if an ID name is already used, and find a new one if so.
 * Return true if created a new name (returned in name).
 *
 * Normally the ID that's being check is already in the ListBase, so ID *id
 * points at the new entry.  The Python Library module needs to know what
 * the name of a datablock will be before it is appended; in this case ID *id
 * id is NULL
 */

static bool check_for_dupid(ListBase *lb, ID *id, char *name)
{
	ID *idtest;
	int nr = 0, a, left_len;
#define MAX_IN_USE 64
	bool in_use[MAX_IN_USE];
	/* to speed up finding unused numbers within [1 .. MAX_IN_USE - 1] */

	char left[MAX_ID_NAME + 8], leftest[MAX_ID_NAME + 8];

	while (true) {

		/* phase 1: id already exists? */
		idtest = is_dupid(lb, id, name);

		/* if there is no double, done */
		if (idtest == NULL) return false;

		/* we have a dup; need to make a new name */
		/* quick check so we can reuse one of first MAX_IN_USE - 1 ids if vacant */
		memset(in_use, false, sizeof(in_use));

		/* get name portion, number portion ("name.number") */
		left_len = BLI_split_name_num(left, &nr, name, '.');

		/* if new name will be too long, truncate it */
		if (nr > 999 && left_len > (MAX_ID_NAME - 8)) {  /* assumption: won't go beyond 9999 */
			left[MAX_ID_NAME - 8] = 0;
			left_len = MAX_ID_NAME - 8;
		}
		else if (left_len > (MAX_ID_NAME - 7)) {
			left[MAX_ID_NAME - 7] = 0;
			left_len = MAX_ID_NAME - 7;
		}

		for (idtest = lb->first; idtest; idtest = idtest->next) {
			int nrtest;
			if ( (id != idtest) &&
			     (idtest->lib == NULL) &&
			     (*name == *(idtest->name + 2)) &&
			     (strncmp(name, idtest->name + 2, left_len) == 0) &&
			     (BLI_split_name_num(leftest, &nrtest, idtest->name + 2, '.') == left_len)
			     )
			{
				/* will get here at least once, otherwise is_dupid call above would have returned NULL */
				if (nrtest < MAX_IN_USE)
					in_use[nrtest] = true;  /* mark as used */
				if (nr <= nrtest)
					nr = nrtest + 1;    /* track largest unused */
			}
		}
		/* At this point, 'nr' will typically be at least 1. (but not always) */
		// BLI_assert(nr >= 1);

		/* decide which value of nr to use */
		for (a = 0; a < MAX_IN_USE; a++) {
			if (a >= nr) break;  /* stop when we've checked up to biggest */  /* redundant check */
			if (!in_use[a]) { /* found an unused value */
				nr = a;
				/* can only be zero if all potential duplicate names had
				 * nonzero numeric suffixes, which means name itself has
				 * nonzero numeric suffix (else no name conflict and wouldn't
				 * have got here), which means name[left_len] is not a null */
				break;
			}
		}
		/* At this point, nr is either the lowest unused number within [0 .. MAX_IN_USE - 1],
		 * or 1 greater than the largest used number if all those low ones are taken.
		 * We can't be bothered to look for the lowest unused number beyond (MAX_IN_USE - 1). */

		/* If the original name has no numeric suffix, 
		 * rather than just chopping and adding numbers, 
		 * shave off the end chars until we have a unique name.
		 * Check the null terminators match as well so we don't get Cube.000 -> Cube.00 */
		if (nr == 0 && name[left_len] == '\0') {
			int len;
			/* FIXME: this code will never be executed, because either nr will be
			 * at least 1, or name will not end at left_len! */
			BLI_assert(0);

			len = left_len - 1;
			idtest = is_dupid(lb, id, name);
			
			while (idtest && len > 1) {
				name[len--] = '\0';
				idtest = is_dupid(lb, id, name);
			}
			if (idtest == NULL) return true;
			/* otherwise just continue and use a number suffix */
		}
		
		if (nr > 999 && left_len > (MAX_ID_NAME - 8)) {
			/* this would overflow name buffer */
			left[MAX_ID_NAME - 8] = 0;
			/* left_len = MAX_ID_NAME - 8; */ /* for now this isn't used again */
			memcpy(name, left, sizeof(char) * (MAX_ID_NAME - 7));
			continue;
		}
		/* this format specifier is from hell... */
		BLI_snprintf(name, sizeof(id->name) - 2, "%s.%.3d", left, nr);

		return true;
	}

#undef MAX_IN_USE
}

/*
 * Only for local blocks: external en indirect blocks already have a
 * unique ID.
 *
 * return true: created a new name
 */

bool new_id(ListBase *lb, ID *id, const char *tname)
{
	bool result;
	char name[MAX_ID_NAME - 2];

	/* if library, don't rename */
	if (id->lib)
		return false;

	/* if no libdata given, look up based on ID */
	if (lb == NULL)
		lb = which_libbase(G.main, GS(id->name));

	/* if no name given, use name of current ID
	 * else make a copy (tname args can be const) */
	if (tname == NULL)
		tname = id->name + 2;

	BLI_strncpy(name, tname, sizeof(name));

	if (name[0] == '\0') {
		/* disallow empty names */
		BLI_strncpy(name, DATA_(ID_FALLBACK_NAME), sizeof(name));
	}
	else {
		/* disallow non utf8 chars,
		 * the interface checks for this but new ID's based on file names don't */
		BLI_utf8_invalid_strip(name, strlen(name));
	}

	result = check_for_dupid(lb, id, name);
	strcpy(id->name + 2, name);

	/* This was in 2.43 and previous releases
	 * however all data in blender should be sorted, not just duplicate names
	 * sorting should not hurt, but noting just incase it alters the way other
	 * functions work, so sort every time */
#if 0
	if (result)
		id_sort_by_name(lb, id);
#endif

	id_sort_by_name(lb, id);
	
	return result;
}

/**
 * Pull an ID out of a library (make it local). Only call this for IDs that
 * don't have other library users.
 */
void id_clear_lib_data(Main *bmain, ID *id)
{
	bNodeTree *ntree = NULL;

	BKE_id_lib_local_paths(bmain, id->lib, id);

	id->lib = NULL;
	id->flag = LIB_LOCAL;
	new_id(which_libbase(bmain, GS(id->name)), id, NULL);

	/* internal bNodeTree blocks inside ID types below
	 * also stores id->lib, make sure this stays in sync.
	 */
	switch (GS(id->name)) {
		case ID_SCE:	ntree = ((Scene *)id)->nodetree;		break;
		case ID_MA:		ntree = ((Material *)id)->nodetree;		break;
		case ID_LA:		ntree = ((Lamp *)id)->nodetree;			break;
		case ID_WO:		ntree = ((World *)id)->nodetree;		break;
		case ID_TE:		ntree = ((Tex *)id)->nodetree;			break;
	}
	if (ntree)
		ntree->id.lib = NULL;
}

/* next to indirect usage in read/writefile also in editobject.c scene.c */
void BKE_main_id_clear_newpoins(Main *bmain)
{
	ListBase *lbarray[MAX_LIBARRAY];
	ID *id;
	int a;

	a = set_listbasepointers(bmain, lbarray);
	while (a--) {
		id = lbarray[a]->first;
		while (id) {
			id->newid = NULL;
			id->flag &= ~LIB_NEW;
			id = id->next;
		}
	}
}

static void lib_indirect_test_id(ID *id, Library *lib)
{
#define LIBTAG(a)   if (a && a->id.lib) { a->id.flag &= ~LIB_INDIRECT; a->id.flag |= LIB_EXTERN; } (void)0
	
	if (id->lib) {
		/* datablocks that were indirectly related are now direct links
		 * without this, appending data that has a link to other data will fail to write */
		if (lib && id->lib->parent == lib) {
			id_lib_extern(id);
		}
		return;
	}
	
	if (GS(id->name) == ID_OB) {
		Object *ob = (Object *)id;
		Mesh *me;

		int a;

#if 0   /* XXX OLD ANIMSYS, NLASTRIPS ARE NO LONGER USED */
		/* XXX old animation system! -------------------------------------- */
		{
			bActionStrip *strip;
			for (strip = ob->nlastrips.first; strip; strip = strip->next) {
				LIBTAG(strip->object);
				LIBTAG(strip->act);
				LIBTAG(strip->ipo);
			}
		}
		/* XXX: new animation system needs something like this? */
#endif

		for (a = 0; a < ob->totcol; a++) {
			LIBTAG(ob->mat[a]);
		}
	
		LIBTAG(ob->dup_group);
		LIBTAG(ob->proxy);
		
		me = ob->data;
		LIBTAG(me);
	}

#undef LIBTAG
}

void BKE_main_id_tag_listbase(ListBase *lb, const bool tag)
{
	ID *id;
	if (tag) {
		for (id = lb->first; id; id = id->next) {
			id->flag |= LIB_DOIT;
		}
	}
	else {
		for (id = lb->first; id; id = id->next) {
			id->flag &= ~LIB_DOIT;
		}
	}
}

void BKE_main_id_tag_idcode(struct Main *mainvar, const short type, const bool tag)
{
	ListBase *lb = which_libbase(mainvar, type);

	BKE_main_id_tag_listbase(lb, tag);
}

void BKE_main_id_tag_all(struct Main *mainvar, const bool tag)
{
	ListBase *lbarray[MAX_LIBARRAY];
	int a;

	a = set_listbasepointers(mainvar, lbarray);
	while (a--) {
		BKE_main_id_tag_listbase(lbarray[a], tag);
	}
}

/* if lib!=NULL, only all from lib local
 * bmain is almost certainly G.main */
void BKE_library_make_local(Main *bmain, Library *lib, bool untagged_only)
{
	ListBase *lbarray[MAX_LIBARRAY];
	ID *id, *idn;
	int a;

	a = set_listbasepointers(bmain, lbarray);
	while (a--) {
		id = lbarray[a]->first;
		
		while (id) {
			id->newid = NULL;
			idn = id->next;      /* id is possibly being inserted again */
			
			/* The check on the second line (LIB_PRE_EXISTING) is done so its
			 * possible to tag data you don't want to be made local, used for
			 * appending data, so any libdata already linked wont become local
			 * (very nasty to discover all your links are lost after appending)  
			 * */
			if (id->flag & (LIB_EXTERN | LIB_INDIRECT | LIB_NEW) &&
			    ((untagged_only == false) || !(id->flag & LIB_PRE_EXISTING)))
			{
				if (lib == NULL || id->lib == lib) {
					if (id->lib) {
						/* for Make Local > All we should be calling id_make_local,
						 * but doing that breaks append (see #36003 and #36006), we
						 * we should make it work with all datablocks and id.us==0 */
						id_clear_lib_data(bmain, id); /* sets 'id->flag' */

						/* why sort alphabetically here but not in
						 * id_clear_lib_data() ? - campbell */
						id_sort_by_name(lbarray[a], id);
					}
					else {
						id->flag &= ~(LIB_EXTERN | LIB_INDIRECT | LIB_NEW);
					}
				}
			}
			id = idn;
		}
	}

	a = set_listbasepointers(bmain, lbarray);
	while (a--) {
		for (id = lbarray[a]->first; id; id = id->next)
			lib_indirect_test_id(id, lib);
	}
}


void test_idbutton(char *name)
{
	/* called from buttons: when name already exists: call new_id */
	ListBase *lb;
	ID *idtest;
	

	lb = which_libbase(G.main, GS(name) );
	if (lb == NULL) return;
	
	/* search for id */
	idtest = BLI_findstring(lb, name + 2, offsetof(ID, name) + 2);

	if (idtest && !new_id(lb, idtest, name + 2)) {
		id_sort_by_name(lb, idtest);
	}
}

/**
 * Sets the name of a block to name, suitably adjusted for uniqueness.
 */
void rename_id(ID *id, const char *name)
{
	ListBase *lb;

	BLI_strncpy(id->name + 2, name, sizeof(id->name) - 2);
	lb = which_libbase(G.main, GS(id->name) );
	
	new_id(lb, id, name);
}

/**
 * Returns in name the name of the block, with a 3-character prefix prepended
 * indicating whether it comes from a library, has a fake user, or no users.
 */
void name_uiprefix_id(char *name, const ID *id)
{
	name[0] = id->lib ? 'L' : ' ';
	name[1] = id->flag & LIB_FAKEUSER ? 'F' : (id->us == 0) ? '0' : ' ';
	name[2] = ' ';

	strcpy(name + 3, id->name + 2);
}

void BKE_library_filepath_set(Library *lib, const char *filepath)
{
	/* in some cases this is used to update the absolute path from the
	 * relative */
	if (lib->name != filepath) {
		BLI_strncpy(lib->name, filepath, sizeof(lib->name));
	}

	BLI_strncpy(lib->filepath, filepath, sizeof(lib->filepath));

	/* not essential but set filepath is an absolute copy of value which
	 * is more useful if its kept in sync */
	if (BLI_path_is_rel(lib->filepath)) {
		/* note that the file may be unsaved, in this case, setting the
		 * filepath on an indirectly linked path is not allowed from the
		 * outliner, and its not really supported but allow from here for now
		 * since making local could cause this to be directly linked - campbell
		 */
		const char *basepath = lib->parent ? lib->parent->filepath : G.main->name;
		BLI_path_abs(lib->filepath, basepath);
	}
}
