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
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_mask_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_speaker_types.h"
#include "DNA_sound_types.h"
#include "DNA_text_types.h"
#include "DNA_vfont_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLI_threads.h"
#include "BLT_translation.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_bpath.h"
#include "BKE_brush.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_gpencil.h"
#include "BKE_idcode.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_lamp.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_linestyle.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_main.h"
#include "BKE_mball.h"
#include "BKE_mask.h"
#include "BKE_movieclip.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_packedFile.h"
#include "BKE_sound.h"
#include "BKE_speaker.h"
#include "BKE_scene.h"
#include "BKE_text.h"
#include "BKE_texture.h"
#include "BKE_world.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

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
	const char *bpath_user_data[2] = {bmain->name, lib->filepath};

	BKE_bpath_traverse_id(bmain, id,
	                      BKE_bpath_relocate_visitor,
	                      BKE_BPATH_TRAVERSE_SKIP_MULTIFILE,
	                      (void *)bpath_user_data);
}

void id_lib_extern(ID *id)
{
	if (id && ID_IS_LINKED_DATABLOCK(id)) {
		BLI_assert(BKE_idcode_is_linkable(GS(id->name)));
		if (id->tag & LIB_TAG_INDIRECT) {
			id->tag -= LIB_TAG_INDIRECT;
			id->tag |= LIB_TAG_EXTERN;
		}
	}
}

/* ensure we have a real user */
/* Note: Now that we have flags, we could get rid of the 'fake_user' special case, flags are enough to ensure
 *       we always have a real user.
 *       However, ID_REAL_USERS is used in several places outside of core library.c, so think we can wait later
 *       to make this change... */
void id_us_ensure_real(ID *id)
{
	if (id) {
		const int limit = ID_FAKE_USERS(id);
		id->tag |= LIB_TAG_EXTRAUSER;
		if (id->us <= limit) {
			if (id->us < limit || ((id->us == limit) && (id->tag & LIB_TAG_EXTRAUSER_SET))) {
				printf("ID user count error: %s (from '%s')\n", id->name, id->lib ? id->lib->filepath : "[Main]");
				BLI_assert(0);
			}
			id->us = limit + 1;
			id->tag |= LIB_TAG_EXTRAUSER_SET;
		}
	}
}

void id_us_clear_real(ID *id)
{
	if (id && (id->tag & LIB_TAG_EXTRAUSER)) {
		if (id->tag & LIB_TAG_EXTRAUSER_SET) {
			id->us--;
			BLI_assert(id->us >= ID_FAKE_USERS(id));
		}
		id->tag &= ~(LIB_TAG_EXTRAUSER | LIB_TAG_EXTRAUSER_SET);
	}
}

/**
 * Same as \a id_us_plus, but does not handle lib indirect -> extern.
 * Only used by readfile.c so far, but simpler/safer to keep it here nonetheless.
 */
void id_us_plus_no_lib(ID *id)
{
	if (id) {
		if ((id->tag & LIB_TAG_EXTRAUSER) && (id->tag & LIB_TAG_EXTRAUSER_SET)) {
			BLI_assert(id->us >= 1);
			/* No need to increase count, just tag extra user as no more set.
			 * Avoids annoying & inconsistent +1 in user count. */
			id->tag &= ~LIB_TAG_EXTRAUSER_SET;
		}
		else {
			BLI_assert(id->us >= 0);
			id->us++;
		}
	}
}


void id_us_plus(ID *id)
{
	if (id) {
		id_us_plus_no_lib(id);
		id_lib_extern(id);
	}
}

/* decrements the user count for *id. */
void id_us_min(ID *id)
{
	if (id) {
		const int limit = ID_FAKE_USERS(id);

		if (id->us <= limit) {
			printf("ID user decrement error: %s (from '%s'): %d <= %d\n",
			       id->name, id->lib ? id->lib->filepath : "[Main]", id->us, limit);
			BLI_assert(0);
			id->us = limit;
		}
		else {
			id->us--;
		}

		if ((id->us == limit) && (id->tag & LIB_TAG_EXTRAUSER)) {
			/* We need an extra user here, but never actually incremented user count for it so far, do it now. */
			id_us_ensure_real(id);
		}
	}
}

void id_fake_user_set(ID *id)
{
	if (id && !(id->flag & LIB_FAKEUSER)) {
		id->flag |= LIB_FAKEUSER;
		id_us_plus(id);
	}
}

void id_fake_user_clear(ID *id)
{
	if (id && (id->flag & LIB_FAKEUSER)) {
		id->flag &= ~LIB_FAKEUSER;
		id_us_min(id);
	}
}

static int id_expand_local_callback(
        void *UNUSED(user_data), struct ID *UNUSED(id_self), struct ID **id_pointer, int UNUSED(cd_flag))
{
	if (*id_pointer) {
		id_lib_extern(*id_pointer);
	}

	return IDWALK_RET_NOP;
}

/**
 * Expand ID usages of given id as 'extern' (and no more indirect) linked data. Used by ID copy/make_local functions.
 */
void BKE_id_expand_local(ID *id)
{
	BKE_library_foreach_ID_link(id, id_expand_local_callback, NULL, 0);
}

/**
 * Ensure new (copied) ID is fully made local.
 */
void BKE_id_copy_ensure_local(Main *bmain, ID *old_id, ID *new_id)
{
	if (ID_IS_LINKED_DATABLOCK(old_id)) {
		BKE_id_expand_local(new_id);
		BKE_id_lib_local_paths(bmain, old_id->lib, new_id);
	}
}

/**
 * Generic 'make local' function, works for most of datablock types...
 */
void BKE_id_make_local_generic(Main *bmain, ID *id, const bool id_in_mainlist, const bool lib_local)
{
	bool is_local = false, is_lib = false;

	/* - only lib users: do nothing (unless force_local is set)
	 * - only local users: set flag
	 * - mixed: make copy
	 * In case we make a whole lib's content local, we always want to localize, and we skip remapping (done later).
	 */

	if (!ID_IS_LINKED_DATABLOCK(id)) {
		return;
	}

	BKE_library_ID_test_usages(bmain, id, &is_local, &is_lib);

	if (lib_local || is_local) {
		if (!is_lib) {
			id_clear_lib_data_ex(bmain, id, id_in_mainlist);
			BKE_id_expand_local(id);
		}
		else {
			ID *id_new;

			/* Should not fail in expected usecases, but id_copy does not copy Scene e.g. */
			if (id_copy(bmain, id, &id_new, false)) {
				id_new->us = 0;

				if (!lib_local) {
					BKE_libblock_remap(bmain, id, id_new, ID_REMAP_SKIP_INDIRECT_USAGE);
				}
			}
		}
	}
}

/**
 * Calls the appropriate make_local method for the block, unless test is set.
 *
 * \param lib_local Special flag used when making a whole library's content local, it needs specific handling.
 *
 * \return true if the block can be made local.
 */
bool id_make_local(Main *bmain, ID *id, const bool test, const bool lib_local)
{
	/* We don't care whether ID is directly or indirectly linked in case we are making a whole lib local... */
	if (!lib_local && (id->tag & LIB_TAG_INDIRECT)) {
		return false;
	}

	switch ((ID_Type)GS(id->name)) {
		case ID_SCE:
			/* Partially implemented (has no copy...). */
			if (!test) BKE_scene_make_local(bmain, (Scene *)id, lib_local);
			return true;
		case ID_OB:
			if (!test) BKE_object_make_local(bmain, (Object *)id, lib_local);
			return true;
		case ID_ME:
			if (!test) BKE_mesh_make_local(bmain, (Mesh *)id, lib_local);
			return true;
		case ID_CU:
			if (!test) BKE_curve_make_local(bmain, (Curve *)id, lib_local);
			return true;
		case ID_MB:
			if (!test) BKE_mball_make_local(bmain, (MetaBall *)id, lib_local);
			return true;
		case ID_MA:
			if (!test) BKE_material_make_local(bmain, (Material *)id, lib_local);
			return true;
		case ID_TE:
			if (!test) BKE_texture_make_local(bmain, (Tex *)id, lib_local);
			return true;
		case ID_IM:
			if (!test) BKE_image_make_local(bmain, (Image *)id, lib_local);
			return true;
		case ID_LT:
			if (!test) BKE_lattice_make_local(bmain, (Lattice *)id, lib_local);
			return true;
		case ID_LA:
			if (!test) BKE_lamp_make_local(bmain, (Lamp *)id, lib_local);
			return true;
		case ID_CA:
			if (!test) BKE_camera_make_local(bmain, (Camera *)id, lib_local);
			return true;
		case ID_SPK:
			if (!test) BKE_speaker_make_local(bmain, (Speaker *)id, lib_local);
			return true;
		case ID_WO:
			if (!test) BKE_world_make_local(bmain, (World *)id, lib_local);
			return true;
		case ID_VF:
			/* Partially implemented (has no copy...). */
			if (!test) BKE_vfont_make_local(bmain, (VFont *)id, lib_local);
			return true;
		case ID_TXT:
			if (!test) BKE_text_make_local(bmain, (Text *)id, lib_local);
			return true;
		case ID_SO:
			/* Partially implemented (has no copy...). */
			if (!test) BKE_sound_make_local(bmain, (bSound *)id, lib_local);
			return true;
		case ID_GR:
			if (!test) BKE_group_make_local(bmain, (Group *)id, lib_local);
			return true;
		case ID_AR:
			if (!test) BKE_armature_make_local(bmain, (bArmature *)id, lib_local);
			return true;
		case ID_AC:
			if (!test) BKE_action_make_local(bmain, (bAction *)id, lib_local);
			return true;
		case ID_NT:
			if (!test) ntreeMakeLocal(bmain, (bNodeTree *)id, true, lib_local);
			return true;
		case ID_BR:
			if (!test) BKE_brush_make_local(bmain, (Brush *)id, lib_local);
			return true;
		case ID_PA:
			if (!test) BKE_particlesettings_make_local(bmain, (ParticleSettings *)id, lib_local);
			return true;
		case ID_GD:
			if (!test) BKE_gpencil_make_local(bmain, (bGPdata *)id, lib_local);
			return true;
		case ID_MC:
			if (!test) BKE_movieclip_make_local(bmain, (MovieClip *)id, lib_local);
			return true;
		case ID_MSK:
			if (!test) BKE_mask_make_local(bmain, (Mask *)id, lib_local);
			return true;
		case ID_LS:
			if (!test) BKE_linestyle_make_local(bmain, (FreestyleLineStyle *)id, lib_local);
			return true;
		case ID_PAL:
			if (!test) BKE_palette_make_local(bmain, (Palette *)id, lib_local);
			return true;
		case ID_PC:
			if (!test) BKE_paint_curve_make_local(bmain, (PaintCurve *)id, lib_local);
			return true;
		case ID_SCR:
		case ID_LI:
		case ID_KE:
		case ID_WM:
			return false; /* can't be linked */
		case ID_IP:
			return false; /* deprecated */
	}

	return false;
}

/**
 * Invokes the appropriate copy method for the block and returns the result in
 * newid, unless test. Returns true if the block can be copied.
 */
bool id_copy(Main *bmain, ID *id, ID **newid, bool test)
{
	if (!test) {
		*newid = NULL;
	}

	/* conventions:
	 * - make shallow copy, only this ID block
	 * - id.us of the new ID is set to 1 */
	switch ((ID_Type)GS(id->name)) {
		case ID_OB:
			if (!test) *newid = (ID *)BKE_object_copy(bmain, (Object *)id);
			return true;
		case ID_ME:
			if (!test) *newid = (ID *)BKE_mesh_copy(bmain, (Mesh *)id);
			return true;
		case ID_CU:
			if (!test) *newid = (ID *)BKE_curve_copy(bmain, (Curve *)id);
			return true;
		case ID_MB:
			if (!test) *newid = (ID *)BKE_mball_copy(bmain, (MetaBall *)id);
			return true;
		case ID_MA:
			if (!test) *newid = (ID *)BKE_material_copy(bmain, (Material *)id);
			return true;
		case ID_TE:
			if (!test) *newid = (ID *)BKE_texture_copy(bmain, (Tex *)id);
			return true;
		case ID_IM:
			if (!test) *newid = (ID *)BKE_image_copy(bmain, (Image *)id);
			return true;
		case ID_LT:
			if (!test) *newid = (ID *)BKE_lattice_copy(bmain, (Lattice *)id);
			return true;
		case ID_LA:
			if (!test) *newid = (ID *)BKE_lamp_copy(bmain, (Lamp *)id);
			return true;
		case ID_SPK:
			if (!test) *newid = (ID *)BKE_speaker_copy(bmain, (Speaker *)id);
			return true;
		case ID_CA:
			if (!test) *newid = (ID *)BKE_camera_copy(bmain, (Camera *)id);
			return true;
		case ID_KE:
			if (!test) *newid = (ID *)BKE_key_copy(bmain, (Key *)id);
			return true;
		case ID_WO:
			if (!test) *newid = (ID *)BKE_world_copy(bmain, (World *)id);
			return true;
		case ID_TXT:
			if (!test) *newid = (ID *)BKE_text_copy(bmain, (Text *)id);
			return true;
		case ID_GR:
			if (!test) *newid = (ID *)BKE_group_copy(bmain, (Group *)id);
			return true;
		case ID_AR:
			if (!test) *newid = (ID *)BKE_armature_copy(bmain, (bArmature *)id);
			return true;
		case ID_AC:
			if (!test) *newid = (ID *)BKE_action_copy(bmain, (bAction *)id);
			return true;
		case ID_NT:
			if (!test) *newid = (ID *)ntreeCopyTree(bmain, (bNodeTree *)id);
			return true;
		case ID_BR:
			if (!test) *newid = (ID *)BKE_brush_copy(bmain, (Brush *)id);
			return true;
		case ID_PA:
			if (!test) *newid = (ID *)BKE_particlesettings_copy(bmain, (ParticleSettings *)id);
			return true;
		case ID_GD:
			if (!test) *newid = (ID *)BKE_gpencil_data_duplicate(bmain, (bGPdata *)id, false);
			return true;
		case ID_MC:
			if (!test) *newid = (ID *)BKE_movieclip_copy(bmain, (MovieClip *)id);
			return true;
		case ID_MSK:
			if (!test) *newid = (ID *)BKE_mask_copy(bmain, (Mask *)id);
			return true;
		case ID_LS:
			if (!test) *newid = (ID *)BKE_linestyle_copy(bmain, (FreestyleLineStyle *)id);
			return true;
		case ID_PAL:
			if (!test) *newid = (ID *)BKE_palette_copy(bmain, (Palette *)id);
			return true;
		case ID_PC:
			if (!test) *newid = (ID *)BKE_paint_curve_copy(bmain, (PaintCurve *)id);
			return true;
		case ID_SCE:
		case ID_LI:
		case ID_SCR:
		case ID_WM:
			return false;  /* can't be copied from here */
		case ID_VF:
		case ID_SO:
			return false;  /* not implemented */
		case ID_IP:
			return false;  /* deprecated */
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
			if (id_copy(CTX_data_main(C), id, &newid, false) && newid) {
				/* copy animation actions too */
				BKE_animdata_copy_id_action(id);
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
	switch ((ID_Type)type) {
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
		case ID_PAL:
			return &(mainlib->palettes);
		case ID_PC:
			return &(mainlib->paintcurves);
	}
	return NULL;
}

/**
 * Clear or set given tags for all ids in listbase (runtime tags).
 */
void BKE_main_id_tag_listbase(ListBase *lb, const int tag, const bool value)
{
	ID *id;
	if (value) {
		for (id = lb->first; id; id = id->next) {
			id->tag |= tag;
		}
	}
	else {
		const int ntag = ~tag;
		for (id = lb->first; id; id = id->next) {
			id->tag &= ntag;
		}
	}
}

/**
 * Clear or set given tags for all ids of given type in bmain (runtime tags).
 */
void BKE_main_id_tag_idcode(struct Main *mainvar, const short type, const int tag, const bool value)
{
	ListBase *lb = which_libbase(mainvar, type);

	BKE_main_id_tag_listbase(lb, tag, value);
}

/**
 * Clear or set given tags for all ids in bmain (runtime tags).
 */
void BKE_main_id_tag_all(struct Main *mainvar, const int tag, const bool value)
{
	ListBase *lbarray[MAX_LIBARRAY];
	int a;

	a = set_listbasepointers(mainvar, lbarray);
	while (a--) {
		BKE_main_id_tag_listbase(lbarray[a], tag, value);
	}
}


/**
 * Clear or set given flags for all ids in listbase (persistent flags).
 */
void BKE_main_id_flag_listbase(ListBase *lb, const int flag, const bool value)
{
	ID *id;
	if (value) {
		for (id = lb->first; id; id = id->next)
			id->tag |= flag;
	}
	else {
		const int nflag = ~flag;
		for (id = lb->first; id; id = id->next)
			id->tag &= nflag;
	}
}

/**
 * Clear or set given flags for all ids in bmain (persistent flags).
 */
void BKE_main_id_flag_all(Main *bmain, const int flag, const bool value)
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
	for (ob = bmain->object.first; ob; ob = ob->id.next) {
		if (ID_IS_LINKED_DATABLOCK(ob)) {
			DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
		}
	}

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
	lb[a++] = &(main->library);  /* Libraries may be accessed from pretty much any other ID... */
	lb[a++] = &(main->ipo);
	lb[a++] = &(main->action); /* moved here to avoid problems when freeing with animato (aligorith) */
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
	lb[a++] = &(main->palettes);
	lb[a++] = &(main->paintcurves);
	lb[a++] = &(main->brush);
	lb[a++] = &(main->particle);
	lb[a++] = &(main->speaker);

	lb[a++] = &(main->world);
	lb[a++] = &(main->movieclip);
	lb[a++] = &(main->screen);
	lb[a++] = &(main->object);
	lb[a++] = &(main->linestyle); /* referenced by scenes */
	lb[a++] = &(main->scene);
	lb[a++] = &(main->wm);
	lb[a++] = &(main->mask);
	
	lb[a] = NULL;

	BLI_assert(a + 1 == MAX_LIBARRAY);

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
void *BKE_libblock_alloc_notest(short type)
{
	ID *id = NULL;
	
	switch ((ID_Type)type) {
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
		case ID_PAL:
			id = MEM_callocN(sizeof(Palette), "Palette");
			break;
		case ID_PC:
			id = MEM_callocN(sizeof(PaintCurve), "Paint Curve");
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
	
	id = BKE_libblock_alloc_notest(type);
	if (id) {
		BKE_main_lock(bmain);
		BLI_addtail(lb, id);
		id->us = 1;
		id->icon_id = 0;
		*( (short *)id->name) = type;
		new_id(lb, id, name);
		/* alphabetic insertion: is in new_id */
		BKE_main_unlock(bmain);
	}
	DAG_id_type_tag(bmain, type);
	return id;
}

/**
 * Initialize an ID of given type, such that it has valid 'empty' data.
 * ID is assumed to be just calloc'ed.
 */
void BKE_libblock_init_empty(ID *id)
{
	/* Note that only ID types that are not valid when filled of zero should have a callback here. */
	switch ((ID_Type)GS(id->name)) {
		case ID_SCE:
			BKE_scene_init((Scene *)id);
			break;
		case ID_LI:
			/* Nothing to do. */
			break;
		case ID_OB:
		{
			Object *ob = (Object *)id;
			ob->type = OB_EMPTY;
			BKE_object_init(ob);
			break;
		}
		case ID_ME:
			BKE_mesh_init((Mesh *)id);
			break;
		case ID_CU:
			BKE_curve_init((Curve *)id);
			break;
		case ID_MB:
			BKE_mball_init((MetaBall *)id);
			break;
		case ID_MA:
			BKE_material_init((Material *)id);
			break;
		case ID_TE:
			BKE_texture_default((Tex *)id);
			break;
		case ID_IM:
			BKE_image_init((Image *)id);
			break;
		case ID_LT:
			BKE_lattice_init((Lattice *)id);
			break;
		case ID_LA:
			BKE_lamp_init((Lamp *)id);
			break;
		case ID_SPK:
			BKE_speaker_init((Speaker *)id);
			break;
		case ID_CA:
			BKE_camera_init((Camera *)id);
			break;
		case ID_WO:
			BKE_world_init((World *)id);
			break;
		case ID_SCR:
			/* Nothing to do. */
			break;
		case ID_VF:
			BKE_vfont_init((VFont *)id);
			break;
		case ID_TXT:
			BKE_text_init((Text *)id);
			break;
		case ID_SO:
			/* Another fuzzy case, think NULLified content is OK here... */
			break;
		case ID_GR:
			/* Nothing to do. */
			break;
		case ID_AR:
			/* Nothing to do. */
			break;
		case ID_AC:
			/* Nothing to do. */
			break;
		case ID_NT:
			ntreeInitDefault((bNodeTree *)id);
			break;
		case ID_BR:
			BKE_brush_init((Brush *)id);
			break;
		case ID_PA:
			/* Nothing to do. */
			break;
		case ID_PC:
			/* Nothing to do. */
			break;
		case ID_GD:
			/* Nothing to do. */
			break;
		case ID_MSK:
			/* Nothing to do. */
			break;
		case ID_LS:
			BKE_linestyle_init((FreestyleLineStyle *)id);
			break;
		case ID_KE:
			/* Shapekeys are a complex topic too - they depend on their 'user' data type...
			 * They are not linkable, though, so it should never reach here anyway. */
			BLI_assert(0);
			break;
		case ID_WM:
			/* We should never reach this. */
			BLI_assert(0);
			break;
		case ID_IP:
			/* Should not be needed - animation from lib pre-2.5 is broken anyway. */
			BLI_assert(0);
			break;
		default:
			BLI_assert(0);  /* Should never reach this point... */
	}
}

/* by spec, animdata is first item after ID */
/* and, trust that BKE_animdata_from_id() will only find AnimData for valid ID-types */
static void id_copy_animdata(ID *id, const bool do_action)
{
	AnimData *adt = BKE_animdata_from_id(id);
	
	if (adt) {
		IdAdtTemplate *iat = (IdAdtTemplate *)id;
		iat->adt = BKE_animdata_copy(iat->adt, do_action); /* could be set to false, need to investigate */
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
void *BKE_libblock_copy(Main *bmain, ID *id)
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
	idn->tag |= LIB_TAG_NEW;

	BKE_libblock_copy_data(idn, id, false);
	
	return idn;
}

void *BKE_libblock_copy_nolib(ID *id, const bool do_action)
{
	ID *idn;
	size_t idn_len;

	idn = BKE_libblock_alloc_notest(GS(id->name));
	assert(idn != NULL);

	BLI_strncpy(idn->name, id->name, sizeof(idn->name));

	idn_len = MEM_allocN_len(idn);
	if ((int)idn_len - (int)sizeof(ID) > 0) { /* signed to allow neg result */
		const char *cp = (const char *)id;
		char *cpn = (char *)idn;

		memcpy(cpn + sizeof(ID), cp + sizeof(ID), idn_len - sizeof(ID));
	}

	id->newid = idn;
	idn->tag |= LIB_TAG_NEW;
	idn->us = 1;

	BKE_libblock_copy_data(idn, id, do_action);

	return idn;
}

static int id_relink_looper(void *UNUSED(user_data), ID *UNUSED(self_id), ID **id_pointer, const int cd_flag)
{
	ID *id = *id_pointer;
	if (id) {
		/* See: NEW_ID macro */
		if (id->newid) {
			BKE_library_update_ID_link_user(id->newid, id, cd_flag);
			*id_pointer = id->newid;
		}
		else if (id->tag & LIB_TAG_NEW) {
			id->tag &= ~LIB_TAG_NEW;
			BKE_libblock_relink(id);
		}
	}
	return IDWALK_RET_NOP;
}

void BKE_libblock_relink(ID *id)
{
	if (ID_IS_LINKED_DATABLOCK(id))
		return;

	BKE_library_foreach_ID_link(id, id_relink_looper, NULL, 0);
}

void BKE_library_free(Library *lib)
{
	if (lib->packedfile)
		freePackedFile(lib->packedfile);
}

Main *BKE_main_new(void)
{
	Main *bmain = MEM_callocN(sizeof(Main), "new main");
	bmain->eval_ctx = DEG_evaluation_context_new(DAG_EVAL_VIEWPORT);
	bmain->lock = MEM_mallocN(sizeof(SpinLock), "main lock");
	BLI_spin_init((SpinLock *)bmain->lock);
	return bmain;
}

void BKE_main_free(Main *mainvar)
{
	/* also call when reading a file, erase all, etc */
	ListBase *lbarray[MAX_LIBARRAY];
	int a;

	MEM_SAFE_FREE(mainvar->blen_thumb);

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
				case  33: BKE_libblock_free_ex(mainvar, id, false); break;
				default:
					BLI_assert(0);
					break;
			}
#endif
		}
	}

	BLI_spin_end((SpinLock *)mainvar->lock);
	MEM_freeN(mainvar->lock);
	DEG_evaluation_context_free(mainvar->eval_ctx);
	MEM_freeN(mainvar);
}

void BKE_main_lock(struct Main *bmain)
{
	BLI_spin_lock((SpinLock *) bmain->lock);
}

void BKE_main_unlock(struct Main *bmain)
{
	BLI_spin_unlock((SpinLock *) bmain->lock);
}

/**
 * Generates a raw .blend file thumbnail data from given image.
 *
 * \param bmain If not NULL, also store generated data in this Main.
 * \param img ImBuf image to generate thumbnail data from.
 * \return The generated .blend file raw thumbnail data.
 */
BlendThumbnail *BKE_main_thumbnail_from_imbuf(Main *bmain, ImBuf *img)
{
	BlendThumbnail *data = NULL;

	if (bmain) {
		MEM_SAFE_FREE(bmain->blen_thumb);
	}

	if (img) {
		const size_t sz = BLEN_THUMB_MEMSIZE(img->x, img->y);
		data = MEM_mallocN(sz, __func__);

		IMB_rect_from_float(img);  /* Just in case... */
		data->width = img->x;
		data->height = img->y;
		memcpy(data->rect, img->rect, sz - sizeof(*data));
	}

	if (bmain) {
		bmain->blen_thumb = data;
	}
	return data;
}

/**
 * Generates an image from raw .blend file thumbnail \a data.
 *
 * \param bmain Use this bmain->blen_thumb data if given \a data is NULL.
 * \param data Raw .blend file thumbnail data.
 * \return An ImBuf from given data, or NULL if invalid.
 */
ImBuf *BKE_main_thumbnail_to_imbuf(Main *bmain, BlendThumbnail *data)
{
	ImBuf *img = NULL;

	if (!data && bmain) {
		data = bmain->blen_thumb;
	}

	if (data) {
		/* Note: we cannot use IMB_allocFromBuffer(), since it tries to dupalloc passed buffer, which will fail
		 *       here (we do not want to pass the first two ints!). */
		img = IMB_allocImBuf((unsigned int)data->width, (unsigned int)data->height, 32, IB_rect | IB_metadata);
		memcpy(img->rect, data->rect, BLEN_THUMB_MEMSIZE(data->width, data->height) - sizeof(*data));
	}

	return img;
}

/**
 * Generates an empty (black) thumbnail for given Main.
 */
void BKE_main_thumbnail_create(struct Main *bmain)
{
	MEM_SAFE_FREE(bmain->blen_thumb);

	bmain->blen_thumb = MEM_callocN(BLEN_THUMB_MEMSIZE(BLEN_THUMB_SIZE, BLEN_THUMB_SIZE), __func__);
	bmain->blen_thumb->width = BLEN_THUMB_SIZE;
	bmain->blen_thumb->height = BLEN_THUMB_SIZE;
}

/* ***************** ID ************************ */
ID *BKE_libblock_find_name_ex(struct Main *bmain, const short type, const char *name)
{
	ListBase *lb = which_libbase(bmain, type);
	BLI_assert(lb != NULL);
	return BLI_findstring(lb, name, offsetof(ID, name) + 2);
}
ID *BKE_libblock_find_name(const short type, const char *name)
{
	return BKE_libblock_find_name_ex(G.main, type, name);
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
		if (id != idtest && !ID_IS_LINKED_DATABLOCK(idtest)) {
			/* do not test alphabetic! */
			/* optimized */
			if (idtest->name[2] == name[0]) {
				if (STREQ(name, idtest->name + 2)) break;
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
			     !ID_IS_LINKED_DATABLOCK(idtest) &&
			     (*name == *(idtest->name + 2)) &&
			     STREQLEN(name, idtest->name + 2, left_len) &&
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
	if (ID_IS_LINKED_DATABLOCK(id))
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
void id_clear_lib_data_ex(Main *bmain, ID *id, const bool id_in_mainlist)
{
	bNodeTree *ntree = NULL;
	Key *key = NULL;

	BKE_id_lib_local_paths(bmain, id->lib, id);

	id_fake_user_clear(id);

	id->lib = NULL;
	id->tag &= ~(LIB_TAG_INDIRECT | LIB_TAG_EXTERN);
	if (id_in_mainlist)
		new_id(which_libbase(bmain, GS(id->name)), id, NULL);

	/* Internal bNodeTree blocks inside datablocks also stores id->lib, make sure this stays in sync. */
	if ((ntree = ntreeFromID(id))) {
		id_clear_lib_data_ex(bmain, &ntree->id, false);  /* Datablocks' nodetree is never in Main. */
	}

	/* Same goes for shapekeys. */
	if ((key = BKE_key_from_id(id))) {
		id_clear_lib_data_ex(bmain, &key->id, id_in_mainlist);  /* sigh, why are keys in Main? */
	}

	if (GS(id->name) == ID_OB) {
		Object *object = (Object *)id;
		if (object->proxy_from != NULL) {
			object->proxy_from->proxy = NULL;
			object->proxy_from->proxy_group = NULL;
		}
		object->proxy = object->proxy_from = object->proxy_group = NULL;
	}
}

void id_clear_lib_data(Main *bmain, ID *id)
{
	id_clear_lib_data_ex(bmain, id, true);
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
			id->tag &= ~LIB_TAG_NEW;
			id = id->next;
		}
	}
}

/** Make linked datablocks local.
 *
 * \param bmain Almost certainly G.main.
 * \param lib If not NULL, only make local datablocks from this library.
 * \param untagged_only If true, only make local datablocks not tagged with LIB_TAG_PRE_EXISTING.
 * \param set_fake If true, set fake user on all localized datablocks (except group and objects ones).
 */
/* XXX TODO This function should probably be reworked.
 *
 * Old (2.77) version was simply making (tagging) datablocks as local, without actually making any check whether
 * they were also indirectly used or not...
 *
 * Current version uses regular id_make_local callback, but this is not super-efficient since this ends up
 * duplicating some IDs and then removing original ones (due to missing knowledge of which ID uses some other ID).
 *
 * We could first check all IDs and detect those to be made local that are only used by other local or future-local
 * datablocks, and directly tag those as local (instead of going through id_make_local) maybe...
 *
 * We'll probably need at some point a true dependency graph between datablocks, but for now this should work
 * good enough (performances is not a critical point here anyway).
 */
void BKE_library_make_local(Main *bmain, const Library *lib, const bool untagged_only, const bool set_fake)
{
	ListBase *lbarray[MAX_LIBARRAY];
	ID *id, *id_next;
	int a;

	for (a = set_listbasepointers(bmain, lbarray); a--; ) {
		id = lbarray[a]->first;

		/* Do not explicitly make local non-linkable IDs (shapekeys, in fact), they are assumed to be handled
		 * by real datablocks responsible of them. */
		const bool do_skip = (id && BKE_idcode_is_linkable(GS(id->name)));

		for (; id; id = id_next) {
			id->newid = NULL;
			id_next = id->next;  /* id is possibly being inserted again */
			
			/* The check on the second line (LIB_TAG_PRE_EXISTING) is done so its
			 * possible to tag data you don't want to be made local, used for
			 * appending data, so any libdata already linked wont become local
			 * (very nasty to discover all your links are lost after appending)  
			 * */
			if (!do_skip && id->tag & (LIB_TAG_EXTERN | LIB_TAG_INDIRECT | LIB_TAG_NEW) &&
			    ((untagged_only == false) || !(id->tag & LIB_TAG_PRE_EXISTING)))
			{
				if (lib == NULL || id->lib == lib) {
					if (id->lib) {
						/* In this specific case, we do want to make ID local even if it has no local usage yet... */
						id_make_local(bmain, id, false, true);
					}
					else {
						id->tag &= ~(LIB_TAG_EXTERN | LIB_TAG_INDIRECT | LIB_TAG_NEW);
					}
				}

				if (set_fake) {
					if (!ELEM(GS(id->name), ID_OB, ID_GR)) {
						/* do not set fake user on objects, groups (instancing) */
						id_fake_user_set(id);
					}
				}
			}
		}
	}

	/* We have to remap local usages of old (linked) ID to new (local) id in a second loop, as lbarray ordering is not
	 * enough to ensure us we did catch all dependencies (e.g. if making local a parent object before its child...).
	 * See T48907. */
	for (a = set_listbasepointers(bmain, lbarray); a--; ) {
		for (id = lbarray[a]->first; id; id = id->next) {
			if (id->newid) {
				BKE_libblock_remap(bmain, id, id->newid, ID_REMAP_SKIP_INDIRECT_USAGE);
			}
		}
	}

	/* Third step: remove datablocks that have been copied to be localized and are no more used in the end...
	 * Note that we may have to loop more than once here, to tackle dependencies between linked objects... */
	bool do_loop = true;
	while (do_loop) {
		do_loop = false;
		for (a = set_listbasepointers(bmain, lbarray); a--; ) {
			for (id = lbarray[a]->first; id; id = id_next) {
				id_next = id->next;
				if (id->newid) {
					bool is_local = false, is_lib = false;

					BKE_library_ID_test_usages(bmain, id, &is_local, &is_lib);
					if (!is_local && !is_lib) {
						BKE_libblock_free(bmain, id);
						do_loop = true;
					}
				}
			}
		}
	}
}

/**
 * Use after setting the ID's name
 * When name exists: call 'new_id'
 */
void BLI_libblock_ensure_unique_name(Main *bmain, const char *name)
{
	ListBase *lb;
	ID *idtest;


	lb = which_libbase(bmain, GS(name));
	if (lb == NULL) return;
	
	/* search for id */
	idtest = BLI_findstring(lb, name + 2, offsetof(ID, name) + 2);

	if (idtest && !new_id(lb, idtest, idtest->name + 2)) {
		id_sort_by_name(lb, idtest);
	}
}

/**
 * Sets the name of a block to name, suitably adjusted for uniqueness.
 */
void BKE_libblock_rename(Main *bmain, ID *id, const char *name)
{
	ListBase *lb = which_libbase(bmain, GS(id->name));
	new_id(lb, id, name);
}

/**
 * Returns in name the name of the block, with a 3-character prefix prepended
 * indicating whether it comes from a library, has a fake user, or no users.
 */
void BKE_id_ui_prefix(char name[MAX_ID_NAME + 1], const ID *id)
{
	name[0] = id->lib ? (ID_MISSING(id) ? 'M' : 'L') : ' ';
	name[1] = (id->flag & LIB_FAKEUSER) ? 'F' : ((id->us == 0) ? '0' : ' ');
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
