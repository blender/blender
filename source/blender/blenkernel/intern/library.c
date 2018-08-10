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
#include "DNA_cachefile_types.h"
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
#include "DNA_modifier_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_mask_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_speaker_types.h"
#include "DNA_sound_types.h"
#include "DNA_text_types.h"
#include "DNA_vfont_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"
#include "DNA_workspace_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"
#include "BLI_mempool.h"
#include "BLI_string_utils.h"
#include "BLI_threads.h"

#include "BLT_translation.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_bpath.h"
#include "BKE_brush.h"
#include "BKE_camera.h"
#include "BKE_cachefile.h"
#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_idcode.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_lamp.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_library_override.h"
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
#include "BKE_lightprobe.h"
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

#include "atomic_ops.h"

//#define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "PIL_time_utildefines.h"
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
	const char *bpath_user_data[2] = {BKE_main_blendfile_path(bmain), lib->filepath};

	BKE_bpath_traverse_id(bmain, id,
	                      BKE_bpath_relocate_visitor,
	                      BKE_BPATH_TRAVERSE_SKIP_MULTIFILE,
	                      (void *)bpath_user_data);
}

void id_lib_extern(ID *id)
{
	if (id && ID_IS_LINKED(id)) {
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

void BKE_id_clear_newpoin(ID *id)
{
	if (id->newid) {
		id->newid->tag &= ~LIB_TAG_NEW;
	}
	id->newid = NULL;
}

static int id_expand_local_callback(
        void *UNUSED(user_data), struct ID *id_self, struct ID **id_pointer, int cb_flag)
{
	if (cb_flag & IDWALK_CB_PRIVATE) {
		return IDWALK_RET_NOP;
	}

	/* Can hapen that we get unlinkable ID here, e.g. with shapekey referring to itself (through drivers)...
	 * Just skip it, shape key can only be either indirectly linked, or fully local, period.
	 * And let's curse one more time that stupid useless shapekey ID type! */
	if (*id_pointer && *id_pointer != id_self && BKE_idcode_is_linkable(GS((*id_pointer)->name))) {
		id_lib_extern(*id_pointer);
	}

	return IDWALK_RET_NOP;
}

/**
 * Expand ID usages of given id as 'extern' (and no more indirect) linked data. Used by ID copy/make_local functions.
 */
void BKE_id_expand_local(Main *bmain, ID *id)
{
	BKE_library_foreach_ID_link(bmain, id, id_expand_local_callback, NULL, IDWALK_READONLY);
}

/**
 * Ensure new (copied) ID is fully made local.
 */
void BKE_id_copy_ensure_local(Main *bmain, const ID *old_id, ID *new_id)
{
	if (ID_IS_LINKED(old_id)) {
		BKE_id_expand_local(bmain, new_id);
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

	if (!ID_IS_LINKED(id)) {
		return;
	}

	BKE_library_ID_test_usages(bmain, id, &is_local, &is_lib);

	if (lib_local || is_local) {
		if (!is_lib) {
			id_clear_lib_data_ex(bmain, id, id_in_mainlist);
			BKE_id_expand_local(bmain, id);
		}
		else {
			ID *id_new;

			/* Should not fail in expected usecases, but id_copy does not copy Scene e.g. */
			if (id_copy(bmain, id, &id_new, false)) {
				id_new->us = 0;

				/* setting newid is mandatory for complex make_lib_local logic... */
				ID_NEW_SET(id, id_new);
				Key *key = BKE_key_from_id(id), *key_new = BKE_key_from_id(id);
				if (key && key_new) {
					ID_NEW_SET(key, key_new);
				}
				bNodeTree *ntree = ntreeFromID(id), *ntree_new = ntreeFromID(id_new);
				if (ntree && ntree_new) {
					ID_NEW_SET(ntree, ntree_new);
				}

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
 * \note Always set ID->newid pointer in case it gets duplicated...
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
		case ID_LP:
			if (!test) BKE_lightprobe_make_local(bmain, (LightProbe *)id, lib_local);
			return true;
		case ID_WO:
			if (!test) BKE_world_make_local(bmain, (World *)id, lib_local);
			return true;
		case ID_VF:
			if (!test) BKE_vfont_make_local(bmain, (VFont *)id, lib_local);
			return true;
		case ID_TXT:
			if (!test) BKE_text_make_local(bmain, (Text *)id, lib_local);
			return true;
		case ID_SO:
			if (!test) BKE_sound_make_local(bmain, (bSound *)id, lib_local);
			return true;
		case ID_GR:
			if (!test) BKE_collection_make_local(bmain, (Collection *)id, lib_local);
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
		case ID_CF:
			if (!test) BKE_cachefile_make_local(bmain, (CacheFile *)id, lib_local);
			return true;
		case ID_WS:
		case ID_SCR:
			/* A bit special: can be appended but not linked. Return false
			 * since supporting make-local doesn't make much sense. */
			return false;
		case ID_LI:
		case ID_KE:
		case ID_WM:
			return false; /* can't be linked */
		case ID_IP:
			return false; /* deprecated */
	}

	return false;
}

struct IDCopyLibManagementData {
	const ID *id_src;
	ID *id_dst;
	int flag;
};

/* Increases usercount as required, and remap self ID pointers. */
static int id_copy_libmanagement_cb(void *user_data, ID *UNUSED(id_self), ID **id_pointer, int cb_flag)
{
	struct IDCopyLibManagementData *data = user_data;
	ID *id = *id_pointer;

	/* Remap self-references to new copied ID. */
	if (id == data->id_src) {
		/* We cannot use id_self here, it is not *always* id_dst (thanks to $£!+@#&/? nodetrees). */
		id = *id_pointer = data->id_dst;
	}

	/* Increase used IDs refcount if needed and required. */
	if ((data->flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0 && (cb_flag & IDWALK_CB_USER)) {
		id_us_plus(id);
	}

	return IDWALK_RET_NOP;
}

/**
 * Generic entry point for copying a datablock (new API).
 *
 * \note Copy is only affecting given data-block (no ID used by copied one will be affected, besides usercount).
 *       There is only one exception, if LIB_ID_COPY_ACTIONS is defined, actions used by animdata will be duplicated.
 *
 * \note Usercount of new copy is always set to 1.
 *
 * \param bmain Main database, may be NULL only if LIB_ID_CREATE_NO_MAIN is specified.
 * \param id Source datablock.
 * \param r_newid Pointer to new (copied) ID pointer.
 * \param flag Set of copy options, see DNA_ID.h enum for details (leave to zero for default, full copy).
 * \param test If set, do not do any copy, just test whether copy is supported.
 * \return False when copying that ID type is not supported, true otherwise.
 */
/* XXX TODO remove test thing, *all* IDs should be copyable that way! */
bool BKE_id_copy_ex(Main *bmain, const ID *id, ID **r_newid, const int flag, const bool test)
{
#define LIB_ID_TYPES_NOCOPY ID_LI, ID_SCR, ID_WM,  /* Not supported */ \
                            ID_IP  /* Deprecated */

	BLI_assert(test || (r_newid != NULL));
	/* Early output is source is NULL. */
	if (id == NULL) {
		return false;
	}
	/* Make sure destination pointer is all good. */
	if ((flag & LIB_ID_CREATE_NO_ALLOCATE) == 0) {
		if (r_newid != NULL) {
			*r_newid = NULL;
		}
	}
	else {
		if (r_newid != NULL && *r_newid != NULL) {
			/* Allow some garbage non-initialized memory to go in. */
			const size_t size = BKE_libblock_get_alloc_info(GS(id->name), NULL);
			memset(*r_newid, 0, size);
		}
	}
	if (ELEM(GS(id->name), LIB_ID_TYPES_NOCOPY)) {
		return false;
	}
	else if (test) {
		return true;
	}

	BKE_libblock_copy_ex(bmain, id, r_newid, flag);

	switch ((ID_Type)GS(id->name)) {
		case ID_SCE:
			BKE_scene_copy_data(bmain, (Scene *)*r_newid, (Scene *)id, flag);
			break;
		case ID_OB:
			BKE_object_copy_data(bmain, (Object *)*r_newid, (Object *)id, flag);
			break;
		case ID_ME:
			BKE_mesh_copy_data(bmain, (Mesh *)*r_newid, (Mesh *)id, flag);
			break;
		case ID_CU:
			BKE_curve_copy_data(bmain, (Curve *)*r_newid, (Curve *)id, flag);
			break;
		case ID_MB:
			BKE_mball_copy_data(bmain, (MetaBall *)*r_newid, (MetaBall *)id, flag);
			break;
		case ID_MA:
			BKE_material_copy_data(bmain, (Material *)*r_newid, (Material *)id, flag);
			break;
		case ID_TE:
			BKE_texture_copy_data(bmain, (Tex *)*r_newid, (Tex *)id, flag);
			break;
		case ID_IM:
			BKE_image_copy_data(bmain, (Image *)*r_newid, (Image *)id, flag);
			break;
		case ID_LT:
			BKE_lattice_copy_data(bmain, (Lattice *)*r_newid, (Lattice *)id, flag);
			break;
		case ID_LA:
			BKE_lamp_copy_data(bmain, (Lamp *)*r_newid, (Lamp *)id, flag);
			break;
		case ID_SPK:
			BKE_speaker_copy_data(bmain, (Speaker *)*r_newid, (Speaker *)id, flag);
			break;
		case ID_LP:
			BKE_lightprobe_copy_data(bmain, (LightProbe *)*r_newid, (LightProbe *)id, flag);
			break;
		case ID_CA:
			BKE_camera_copy_data(bmain, (Camera *)*r_newid, (Camera *)id, flag);
			break;
		case ID_KE:
			BKE_key_copy_data(bmain, (Key *)*r_newid, (Key *)id, flag);
			break;
		case ID_WO:
			BKE_world_copy_data(bmain, (World *)*r_newid, (World *)id, flag);
			break;
		case ID_TXT:
			BKE_text_copy_data(bmain, (Text *)*r_newid, (Text *)id, flag);
			break;
		case ID_GR:
			BKE_collection_copy_data(bmain, (Collection *)*r_newid, (Collection *)id, flag);
			break;
		case ID_AR:
			BKE_armature_copy_data(bmain, (bArmature *)*r_newid, (bArmature *)id, flag);
			break;
		case ID_AC:
			BKE_action_copy_data(bmain, (bAction *)*r_newid, (bAction *)id, flag);
			break;
		case ID_NT:
			BKE_node_tree_copy_data(bmain, (bNodeTree *)*r_newid, (bNodeTree *)id, flag);
			break;
		case ID_BR:
			BKE_brush_copy_data(bmain, (Brush *)*r_newid, (Brush *)id, flag);
			break;
		case ID_PA:
			BKE_particlesettings_copy_data(bmain, (ParticleSettings *)*r_newid, (ParticleSettings *)id, flag);
			break;
		case ID_GD:
			BKE_gpencil_copy_data((bGPdata *)*r_newid, (bGPdata *)id, flag);
			break;
		case ID_MC:
			BKE_movieclip_copy_data(bmain, (MovieClip *)*r_newid, (MovieClip *)id, flag);
			break;
		case ID_MSK:
			BKE_mask_copy_data(bmain, (Mask *)*r_newid, (Mask *)id, flag);
			break;
		case ID_LS:
			BKE_linestyle_copy_data(bmain, (FreestyleLineStyle *)*r_newid, (FreestyleLineStyle *)id, flag);
			break;
		case ID_PAL:
			BKE_palette_copy_data(bmain, (Palette *)*r_newid, (Palette *)id, flag);
			break;
		case ID_PC:
			BKE_paint_curve_copy_data(bmain, (PaintCurve *)*r_newid, (PaintCurve *)id, flag);
			break;
		case ID_CF:
			BKE_cachefile_copy_data(bmain, (CacheFile *)*r_newid, (CacheFile *)id, flag);
			break;
		case ID_SO:
			BKE_sound_copy_data(bmain, (bSound *)*r_newid, (bSound *)id, flag);
			break;
		case ID_VF:
			BKE_vfont_copy_data(bmain, (VFont *)*r_newid, (VFont *)id, flag);
			break;
		case ID_LI:
		case ID_SCR:
		case ID_WM:
		case ID_WS:
		case ID_IP:
			BLI_assert(0);  /* Should have been rejected at start of function! */
			break;
	}

	/* Update ID refcount, remap pointers to self in new ID. */
	struct IDCopyLibManagementData data = {.id_src = id, .id_dst = *r_newid, .flag = flag};
	BKE_library_foreach_ID_link(bmain, *r_newid, id_copy_libmanagement_cb, &data, IDWALK_NOP);

	/* Do not make new copy local in case we are copying outside of main...
	 * XXX TODO: is this behavior OK, or should we need own flag to control that? */
	if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
		BLI_assert((flag & LIB_ID_COPY_KEEP_LIB) == 0);
		BKE_id_copy_ensure_local(bmain, id, *r_newid);
	}
	else {
		(*r_newid)->lib = id->lib;
	}

	return true;
}

/**
 * Invokes the appropriate copy method for the block and returns the result in
 * newid, unless test. Returns true if the block can be copied.
 */
bool id_copy(Main *bmain, const ID *id, ID **newid, bool test)
{
	return BKE_id_copy_ex(bmain, id, newid, LIB_ID_COPY_SHAPEKEY, test);
}

/** Does a mere memory swap over the whole IDs data (including type-specific memory).
 *  \note Most internal ID data itself is not swapped (only IDProperties are). */
void BKE_id_swap(Main *bmain, ID *id_a, ID *id_b)
{
	BLI_assert(GS(id_a->name) == GS(id_b->name));

	const ID id_a_back = *id_a;
	const ID id_b_back = *id_b;

#define CASE_SWAP(_gs, _type) \
	case _gs: \
		SWAP(_type, *(_type *)id_a, *(_type *)id_b); \
		break

	switch ((ID_Type)GS(id_a->name)) {
		CASE_SWAP(ID_SCE, Scene);
		CASE_SWAP(ID_LI, Library);
		CASE_SWAP(ID_OB, Object);
		CASE_SWAP(ID_ME, Mesh);
		CASE_SWAP(ID_CU, Curve);
		CASE_SWAP(ID_MB, MetaBall);
		CASE_SWAP(ID_MA, Material);
		CASE_SWAP(ID_TE, Tex);
		CASE_SWAP(ID_IM, Image);
		CASE_SWAP(ID_LT, Lattice);
		CASE_SWAP(ID_LA, Lamp);
		CASE_SWAP(ID_LP, LightProbe);
		CASE_SWAP(ID_CA, Camera);
		CASE_SWAP(ID_KE, Key);
		CASE_SWAP(ID_WO, World);
		CASE_SWAP(ID_SCR, bScreen);
		CASE_SWAP(ID_VF, VFont);
		CASE_SWAP(ID_TXT, Text);
		CASE_SWAP(ID_SPK, Speaker);
		CASE_SWAP(ID_SO, bSound);
		CASE_SWAP(ID_GR, Collection);
		CASE_SWAP(ID_AR, bArmature);
		CASE_SWAP(ID_AC, bAction);
		CASE_SWAP(ID_NT, bNodeTree);
		CASE_SWAP(ID_BR, Brush);
		CASE_SWAP(ID_PA, ParticleSettings);
		CASE_SWAP(ID_WM, wmWindowManager);
		CASE_SWAP(ID_WS, WorkSpace);
		CASE_SWAP(ID_GD, bGPdata);
		CASE_SWAP(ID_MC, MovieClip);
		CASE_SWAP(ID_MSK, Mask);
		CASE_SWAP(ID_LS, FreestyleLineStyle);
		CASE_SWAP(ID_PAL, Palette);
		CASE_SWAP(ID_PC, PaintCurve);
		CASE_SWAP(ID_CF, CacheFile);
		case ID_IP:
			break;  /* Deprecated. */
	}

#undef CASE_SWAP

	/* Restore original ID's internal data. */
	*id_a = id_a_back;
	*id_b = id_b_back;

	/* Exception: IDProperties. */
	id_a->properties = id_b_back.properties;
	id_b->properties = id_a_back.properties;

	/* Swap will have broken internal references to itself, restore them. */
	BKE_libblock_relink_ex(bmain, id_a, id_b, id_a, false);
	BKE_libblock_relink_ex(bmain, id_b, id_a, id_b, false);
}

/** Does *not* set ID->newid pointer. */
bool id_single_user(bContext *C, ID *id, PointerRNA *ptr, PropertyRNA *prop)
{
	ID *newid = NULL;
	PointerRNA idptr;

	if (id) {
		/* if property isn't editable, we're going to have an extra block hanging around until we save */
		if (RNA_property_editable(ptr, prop)) {
			Main *bmain = CTX_data_main(C);
			if (id_copy(bmain, id, &newid, false) && newid) {
				/* copy animation actions too */
				BKE_animdata_copy_id_action(bmain, id, false);
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

static int libblock_management_us_plus(void *UNUSED(user_data), ID *UNUSED(id_self), ID **id_pointer, int cb_flag)
{
	if (cb_flag & IDWALK_CB_USER) {
		id_us_plus(*id_pointer);
	}
	if (cb_flag & IDWALK_CB_USER_ONE) {
		id_us_ensure_real(*id_pointer);
	}

	return IDWALK_RET_NOP;
}

static int libblock_management_us_min(void *UNUSED(user_data), ID *UNUSED(id_self), ID **id_pointer, int cb_flag)
{
	if (cb_flag & IDWALK_CB_USER) {
		id_us_min(*id_pointer);
	}
	/* We can do nothing in IDWALK_CB_USER_ONE case! */

	return IDWALK_RET_NOP;
}

/** Add a 'NO_MAIN' datablock to given main (also sets usercounts of its IDs if needed). */
void BKE_libblock_management_main_add(Main *bmain, void *idv)
{
	ID *id = idv;

	BLI_assert(bmain != NULL);
	if ((id->tag & LIB_TAG_NO_MAIN) == 0) {
		return;
	}

	if ((id->tag & LIB_TAG_NOT_ALLOCATED) != 0) {
		/* We cannot add non-allocated ID to Main! */
		return;
	}

	/* We cannot allow non-userrefcounting IDs in Main database! */
	if ((id->tag & LIB_TAG_NO_USER_REFCOUNT) != 0) {
		BKE_library_foreach_ID_link(bmain, id, libblock_management_us_plus, NULL, IDWALK_NOP);
	}

	ListBase *lb = which_libbase(bmain, GS(id->name));
	BKE_main_lock(bmain);
	BLI_addtail(lb, id);
	new_id(lb, id, NULL);
	/* alphabetic insertion: is in new_id */
	id->tag &= ~(LIB_TAG_NO_MAIN | LIB_TAG_NO_USER_REFCOUNT);
	bmain->is_memfile_undo_written = false;
	BKE_main_unlock(bmain);
}

/** Remove a datablock from given main (set it to 'NO_MAIN' status). */
void BKE_libblock_management_main_remove(Main *bmain, void *idv)
{
	ID *id = idv;

	BLI_assert(bmain != NULL);
	if ((id->tag & LIB_TAG_NO_MAIN) != 0) {
		return;
	}

	/* For now, allow userrefcounting IDs to get out of Main - can be handy in some cases... */

	ListBase *lb = which_libbase(bmain, GS(id->name));
	BKE_main_lock(bmain);
	BLI_remlink(lb, id);
	id->tag |= LIB_TAG_NO_MAIN;
	bmain->is_memfile_undo_written = false;
	BKE_main_unlock(bmain);
}

void BKE_libblock_management_usercounts_set(Main *bmain, void *idv)
{
	ID *id = idv;

	if ((id->tag & LIB_TAG_NO_USER_REFCOUNT) == 0) {
		return;
	}

	BKE_library_foreach_ID_link(bmain, id, libblock_management_us_plus, NULL, IDWALK_NOP);
	id->tag &= ~LIB_TAG_NO_USER_REFCOUNT;
}

void BKE_libblock_management_usercounts_clear(Main *bmain, void *idv)
{
	ID *id = idv;

	/* We do not allow IDs in Main database to not be userrefcounting. */
	if ((id->tag & LIB_TAG_NO_USER_REFCOUNT) != 0 || (id->tag & LIB_TAG_NO_MAIN) != 0) {
		return;
	}

	BKE_library_foreach_ID_link(bmain, id, libblock_management_us_min, NULL, IDWALK_NOP);
	id->tag |= LIB_TAG_NO_USER_REFCOUNT;
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
		case ID_LP:
			return &(mainlib->lightprobe);
		case ID_SO:
			return &(mainlib->sound);
		case ID_GR:
			return &(mainlib->collection);
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
		case ID_CF:
			return &(mainlib->cachefiles);
		case ID_WS:
			return &(mainlib->workspaces);
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
		if (ID_IS_LINKED(ob)) {
			DEG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
		}
	}

	DEG_id_type_tag(bmain, ID_OB);
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
	/* BACKWARDS! also watch order of free-ing! (mesh<->mat), first items freed last.
	 * This is important because freeing data decreases usercounts of other datablocks,
	 * if this data is its self freed it can crash. */
	lb[INDEX_ID_LI] = &(main->library);  /* Libraries may be accessed from pretty much any other ID... */
	lb[INDEX_ID_IP] = &(main->ipo);
	lb[INDEX_ID_AC] = &(main->action); /* moved here to avoid problems when freeing with animato (aligorith) */
	lb[INDEX_ID_KE] = &(main->key);
	lb[INDEX_ID_PAL] = &(main->palettes); /* referenced by gpencil, so needs to be before that to avoid crashes */
	lb[INDEX_ID_GD] = &(main->gpencil); /* referenced by nodes, objects, view, scene etc, before to free after. */
	lb[INDEX_ID_NT] = &(main->nodetree);
	lb[INDEX_ID_IM] = &(main->image);
	lb[INDEX_ID_TE] = &(main->tex);
	lb[INDEX_ID_MA] = &(main->mat);
	lb[INDEX_ID_VF] = &(main->vfont);

	/* Important!: When adding a new object type,
	 * the specific data should be inserted here
	 */

	lb[INDEX_ID_AR] = &(main->armature);

	lb[INDEX_ID_CF] = &(main->cachefiles);
	lb[INDEX_ID_ME] = &(main->mesh);
	lb[INDEX_ID_CU] = &(main->curve);
	lb[INDEX_ID_MB] = &(main->mball);

	lb[INDEX_ID_LT] = &(main->latt);
	lb[INDEX_ID_LA] = &(main->lamp);
	lb[INDEX_ID_CA] = &(main->camera);

	lb[INDEX_ID_TXT] = &(main->text);
	lb[INDEX_ID_SO]  = &(main->sound);
	lb[INDEX_ID_GR]  = &(main->collection);
	lb[INDEX_ID_PAL] = &(main->palettes);
	lb[INDEX_ID_PC]  = &(main->paintcurves);
	lb[INDEX_ID_BR]  = &(main->brush);
	lb[INDEX_ID_PA]  = &(main->particle);
	lb[INDEX_ID_SPK] = &(main->speaker);
	lb[INDEX_ID_LP]  = &(main->lightprobe);

	lb[INDEX_ID_WO]  = &(main->world);
	lb[INDEX_ID_MC]  = &(main->movieclip);
	lb[INDEX_ID_SCR] = &(main->screen);
	lb[INDEX_ID_OB]  = &(main->object);
	lb[INDEX_ID_LS]  = &(main->linestyle); /* referenced by scenes */
	lb[INDEX_ID_SCE] = &(main->scene);
	lb[INDEX_ID_WS]  = &(main->workspaces); /* before wm, so it's freed after it! */
	lb[INDEX_ID_WM]  = &(main->wm);
	lb[INDEX_ID_MSK] = &(main->mask);

	lb[INDEX_ID_NULL] = NULL;

	return (MAX_LIBARRAY - 1);
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
 * Get allocation size fo a given datablock type and optionally allocation name.
 */
size_t BKE_libblock_get_alloc_info(short type, const char **name)
{
#define CASE_RETURN(id_code, type)  \
	case id_code:                   \
		do {                        \
			if (name != NULL) {     \
				*name = #type;      \
			}                       \
			return sizeof(type);    \
		} while(0)

	switch ((ID_Type)type) {
		CASE_RETURN(ID_SCE, Scene);
		CASE_RETURN(ID_LI,  Library);
		CASE_RETURN(ID_OB,  Object);
		CASE_RETURN(ID_ME,  Mesh);
		CASE_RETURN(ID_CU,  Curve);
		CASE_RETURN(ID_MB,  MetaBall);
		CASE_RETURN(ID_MA,  Material);
		CASE_RETURN(ID_TE,  Tex);
		CASE_RETURN(ID_IM,  Image);
		CASE_RETURN(ID_LT,  Lattice);
		CASE_RETURN(ID_LA,  Lamp);
		CASE_RETURN(ID_CA,  Camera);
		CASE_RETURN(ID_IP,  Ipo);
		CASE_RETURN(ID_KE,  Key);
		CASE_RETURN(ID_WO,  World);
		CASE_RETURN(ID_SCR, bScreen);
		CASE_RETURN(ID_VF,  VFont);
		CASE_RETURN(ID_TXT, Text);
		CASE_RETURN(ID_SPK, Speaker);
		CASE_RETURN(ID_LP,  LightProbe);
		CASE_RETURN(ID_SO,  bSound);
		CASE_RETURN(ID_GR,  Collection);
		CASE_RETURN(ID_AR,  bArmature);
		CASE_RETURN(ID_AC,  bAction);
		CASE_RETURN(ID_NT,  bNodeTree);
		CASE_RETURN(ID_BR,  Brush);
		CASE_RETURN(ID_PA,  ParticleSettings);
		CASE_RETURN(ID_WM,  wmWindowManager);
		CASE_RETURN(ID_GD,  bGPdata);
		CASE_RETURN(ID_MC,  MovieClip);
		CASE_RETURN(ID_MSK, Mask);
		CASE_RETURN(ID_LS,  FreestyleLineStyle);
		CASE_RETURN(ID_PAL, Palette);
		CASE_RETURN(ID_PC,  PaintCurve);
		CASE_RETURN(ID_CF,  CacheFile);
		CASE_RETURN(ID_WS,  WorkSpace);
	}
	return 0;
#undef CASE_RETURN
}

/**
 * Allocates and returns memory of the right size for the specified block type,
 * initialized to zero.
 */
void *BKE_libblock_alloc_notest(short type)
{
	const char *name;
	size_t size = BKE_libblock_get_alloc_info(type, &name);
	if (size != 0) {
		return MEM_callocN(size, name);
	}
	BLI_assert(!"Request to allocate unknown data type");
	return NULL;
}

/**
 * Allocates and returns a block of the specified type, with the specified name
 * (adjusted as necessary to ensure uniqueness), and appended to the specified list.
 * The user count is set to 1, all other content (apart from name and links) being
 * initialized to zero.
 */
void *BKE_libblock_alloc(Main *bmain, short type, const char *name, const int flag)
{
	BLI_assert((flag & LIB_ID_CREATE_NO_ALLOCATE) == 0);

	ID *id = BKE_libblock_alloc_notest(type);

	if (id) {
		if ((flag & LIB_ID_CREATE_NO_MAIN) != 0) {
			id->tag |= LIB_TAG_NO_MAIN;
		}
		if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) != 0) {
			id->tag |= LIB_TAG_NO_USER_REFCOUNT;
		}

		id->icon_id = 0;
		*((short *)id->name) = type;
		if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
			id->us = 1;
		}
		if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
			ListBase *lb = which_libbase(bmain, type);

			BKE_main_lock(bmain);
			BLI_addtail(lb, id);
			new_id(lb, id, name);
			bmain->is_memfile_undo_written = false;
			/* alphabetic insertion: is in new_id */
			BKE_main_unlock(bmain);

			/* TODO to be removed from here! */
			if ((flag & LIB_ID_CREATE_NO_DEG_TAG) == 0) {
				DEG_id_type_tag(bmain, type);
			}
		}
		else {
			BLI_strncpy(id->name + 2, name, sizeof(id->name) - 2);
		}
	}

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
		case ID_LP:
			BKE_lightprobe_init((LightProbe *)id);
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
		case ID_CF:
			BKE_cachefile_init((CacheFile *)id);
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

/** Generic helper to create a new empty datablock of given type in given \a bmain database.
 *
 * \param name can be NULL, in which case we get default name for this ID type. */
void *BKE_id_new(Main *bmain, const short type, const char *name)
{
	BLI_assert(bmain != NULL);

	if (name == NULL) {
		name = DATA_(BKE_idcode_to_name(type));
	}

	ID *id = BKE_libblock_alloc(bmain, type, name, 0);
	BKE_libblock_init_empty(id);

	return id;
}

/** Generic helper to create a new temporary empty datablock of given type, *outside* of any Main database.
 *
 * \param name can be NULL, in which case we get default name for this ID type. */
void *BKE_id_new_nomain(const short type, const char *name)
{
	if (name == NULL) {
		name = DATA_(BKE_idcode_to_name(type));
	}

	ID *id = BKE_libblock_alloc(NULL, type, name,
	                            LIB_ID_CREATE_NO_MAIN |
	                            LIB_ID_CREATE_NO_USER_REFCOUNT |
	                            LIB_ID_CREATE_NO_DEG_TAG);
	BKE_libblock_init_empty(id);

	return id;
}

/* by spec, animdata is first item after ID */
/* and, trust that BKE_animdata_from_id() will only find AnimData for valid ID-types */
static void id_copy_animdata(Main *bmain, ID *id, const bool do_action, const bool do_id_user)
{
	AnimData *adt = BKE_animdata_from_id(id);

	if (adt) {
		IdAdtTemplate *iat = (IdAdtTemplate *)id;
		iat->adt = BKE_animdata_copy(bmain, iat->adt, do_action, do_id_user);
	}
}

void BKE_libblock_copy_ex(Main *bmain, const ID *id, ID **r_newid, const int flag)
{
	ID *new_id = *r_newid;

	/* Grrrrrrrrr... Not adding 'root' nodetrees to bmain.... grrrrrrrrrrrrrrrrrrrr! */
	/* This is taken from original ntree copy code, might be weak actually? */
	const bool use_nodetree_alloc_exception = ((GS(id->name) == ID_NT) && (bmain != NULL) &&
	                                           (BLI_findindex(&bmain->nodetree, id) < 0));

	BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) != 0 || bmain != NULL);
	BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) != 0 || (flag & LIB_ID_CREATE_NO_ALLOCATE) == 0);
	BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) == 0 || (flag & LIB_ID_CREATE_NO_USER_REFCOUNT) != 0);
	/* Never implicitely copy shapekeys when generating temp data outside of Main database. */
	BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) == 0 || (flag & LIB_ID_COPY_SHAPEKEY) == 0);

	if ((flag & LIB_ID_CREATE_NO_ALLOCATE) != 0) {
		/* r_newid already contains pointer to allocated memory. */
		/* TODO do we want to memset(0) whole mem before filling it? */
		BLI_strncpy(new_id->name, id->name, sizeof(new_id->name));
		new_id->us = 0;
		new_id->tag |= LIB_TAG_NOT_ALLOCATED | LIB_TAG_NO_MAIN | LIB_TAG_NO_USER_REFCOUNT;
		/* TODO Do we want/need to copy more from ID struct itself? */
	}
	else {
		new_id = BKE_libblock_alloc(bmain, GS(id->name), id->name + 2, flag | (use_nodetree_alloc_exception ? LIB_ID_CREATE_NO_MAIN : 0));
	}
	BLI_assert(new_id != NULL);

	const size_t id_len = BKE_libblock_get_alloc_info(GS(new_id->name), NULL);
	const size_t id_offset = sizeof(ID);
	if ((int)id_len - (int)id_offset > 0) { /* signed to allow neg result */ /* XXX ????? */
		const char *cp = (const char *)id;
		char *cpn = (char *)new_id;

		memcpy(cpn + id_offset, cp + id_offset, id_len - id_offset);
	}

	if (id->properties) {
		new_id->properties = IDP_CopyProperty_ex(id->properties, flag);
	}

	/* XXX Again... We need a way to control what we copy in a much more refined way.
	 * We cannot always copy this, some internal copying will die on it! */
	/* For now, upper level code will have to do that itself when required. */
#if 0
	if (id->override != NULL) {
		BKE_override_copy(new_id, id);
	}
#endif

	/* the duplicate should get a copy of the animdata */
	if ((flag & LIB_ID_COPY_NO_ANIMDATA) == 0) {
		BLI_assert((flag & LIB_ID_COPY_ACTIONS) == 0 || (flag & LIB_ID_CREATE_NO_MAIN) == 0);
		id_copy_animdata(bmain, new_id,
		                 (flag & LIB_ID_COPY_ACTIONS) != 0 && (flag & LIB_ID_CREATE_NO_MAIN) == 0,
		                 (flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0);
	}
	else if (id_can_have_animdata(new_id)) {
		IdAdtTemplate *iat = (IdAdtTemplate *)new_id;
		iat->adt = NULL;
	}

	if ((flag & LIB_ID_CREATE_NO_DEG_TAG) == 0 && (flag & LIB_ID_CREATE_NO_MAIN) == 0) {
		DEG_id_type_tag(bmain, GS(new_id->name));
	}

	*r_newid = new_id;
}

/* used everywhere in blenkernel */
void *BKE_libblock_copy(Main *bmain, const ID *id)
{
	ID *idn;

	BKE_libblock_copy_ex(bmain, id, &idn, 0);

	return idn;
}

void *BKE_libblock_copy_nolib(const ID *id, const bool do_action)
{
	ID *idn;

	BKE_libblock_copy_ex(NULL, id, &idn, LIB_ID_CREATE_NO_MAIN | LIB_ID_CREATE_NO_USER_REFCOUNT | (do_action ? LIB_ID_COPY_ACTIONS : 0));

	return idn;
}

void BKE_library_free(Library *lib)
{
	if (lib->packedfile)
		freePackedFile(lib->packedfile);
}

Main *BKE_main_new(void)
{
	Main *bmain = MEM_callocN(sizeof(Main), "new main");
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
			BKE_libblock_free_ex(mainvar, id, false, false);
#else
			/* errors freeing ID's can be hard to track down,
			 * enable this so valgrind will give the line number in its error log */
			switch (a) {
				case   0: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case   1: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case   2: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case   3: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case   4: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case   5: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case   6: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case   7: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case   8: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case   9: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  10: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  11: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  12: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  13: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  14: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  15: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  16: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  17: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  18: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  19: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  20: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  21: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  22: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  23: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  24: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  25: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  26: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  27: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  28: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  29: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  30: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  31: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  32: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  33: BKE_libblock_free_ex(mainvar, id, false, false); break;
				case  34: BKE_libblock_free_ex(mainvar, id, false, false); break;
				default:
					BLI_assert(0);
					break;
			}
#endif
		}
	}

	if (mainvar->relations) {
		BKE_main_relations_free(mainvar);
	}

	BLI_spin_end((SpinLock *)mainvar->lock);
	MEM_freeN(mainvar->lock);
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


static int main_relations_create_cb(void *user_data, ID *id_self, ID **id_pointer, int cb_flag)
{
	MainIDRelations *rel = user_data;

	if (*id_pointer) {
		MainIDRelationsEntry *entry, **entry_p;

		entry = BLI_mempool_alloc(rel->entry_pool);
		if (BLI_ghash_ensure_p(rel->id_user_to_used, id_self, (void ***)&entry_p)) {
			entry->next = *entry_p;
		}
		else {
			entry->next = NULL;
		}
		entry->id_pointer = id_pointer;
		entry->usage_flag = cb_flag;
		*entry_p = entry;

		entry = BLI_mempool_alloc(rel->entry_pool);
		if (BLI_ghash_ensure_p(rel->id_used_to_user, *id_pointer, (void ***)&entry_p)) {
			entry->next = *entry_p;
		}
		else {
			entry->next = NULL;
		}
		entry->id_pointer = (ID **)id_self;
		entry->usage_flag = cb_flag;
		*entry_p = entry;
	}

	return IDWALK_RET_NOP;
}

/** Generate the mappings between used IDs and their users, and vice-versa. */
void BKE_main_relations_create(Main *bmain)
{
	ListBase *lbarray[MAX_LIBARRAY];
	ID *id;
	int a;

	if (bmain->relations != NULL) {
		BKE_main_relations_free(bmain);
	}

	bmain->relations = MEM_mallocN(sizeof(*bmain->relations), __func__);
	bmain->relations->id_used_to_user = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
	bmain->relations->id_user_to_used = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
	bmain->relations->entry_pool = BLI_mempool_create(sizeof(MainIDRelationsEntry), 128, 128, BLI_MEMPOOL_NOP);

	for (a = set_listbasepointers(bmain, lbarray); a--; ) {
		for (id = lbarray[a]->first; id; id = id->next) {
			BKE_library_foreach_ID_link(NULL, id, main_relations_create_cb, bmain->relations, IDWALK_READONLY);
		}
	}
}

void BKE_main_relations_free(Main *bmain)
{
	if (bmain->relations) {
		if (bmain->relations->id_used_to_user) {
			BLI_ghash_free(bmain->relations->id_used_to_user, NULL, NULL);
		}
		if (bmain->relations->id_user_to_used) {
			BLI_ghash_free(bmain->relations->id_user_to_used, NULL, NULL);
		}
		BLI_mempool_destroy(bmain->relations->entry_pool);
		MEM_freeN(bmain->relations);
		bmain->relations = NULL;
	}
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

/**
 * Return filepath of given \a main.
 */
const char *BKE_main_blendfile_path(const Main *bmain)
{
	return bmain->name;
}

/**
 * Return filepath of global main (G_MAIN).
 *
 * \warning Usage is not recommended, you should always try to get a valid Main pointer from context...
 */
const char *BKE_main_blendfile_path_from_global(void)
{
	return BKE_main_blendfile_path(G_MAIN);
}

/* ***************** ID ************************ */
ID *BKE_libblock_find_name(struct Main *bmain, const short type, const char *name)
{
	ListBase *lb = which_libbase(bmain, type);
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
		if (id != idtest && !ID_IS_LINKED(idtest)) {
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
	int nr = 0, a;
	size_t left_len;
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
			left[MAX_ID_NAME - 8] = '\0';
			left_len = MAX_ID_NAME - 8;
		}
		else if (left_len > (MAX_ID_NAME - 7)) {
			left[MAX_ID_NAME - 7] = '\0';
			left_len = MAX_ID_NAME - 7;
		}

		/* Code above may have generated invalid utf-8 string, due to raw truncation.
		 * Ensure we get a valid one now! */
		left_len -= (size_t)BLI_utf8_invalid_strip(left, left_len);

		for (idtest = lb->first; idtest; idtest = idtest->next) {
			int nrtest;
			if ( (id != idtest) &&
			     !ID_IS_LINKED(idtest) &&
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
			size_t len;
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
	if (ID_IS_LINKED(id))
		return false;

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


static void library_make_local_copying_check(ID *id, GSet *loop_tags, MainIDRelations *id_relations, GSet *done_ids)
{
	if (BLI_gset_haskey(done_ids, id)) {
		return;  /* Already checked, nothing else to do. */
	}

	MainIDRelationsEntry *entry = BLI_ghash_lookup(id_relations->id_used_to_user, id);
	BLI_gset_insert(loop_tags, id);
	for (; entry != NULL; entry = entry->next) {
		ID *par_id = (ID *)entry->id_pointer;  /* used_to_user stores ID pointer, not pointer to ID pointer... */

		/* Our oh-so-beloved 'from' pointers... */
		if (entry->usage_flag & IDWALK_CB_LOOPBACK) {
			/* We totally disregard Object->proxy_from 'usage' here, this one would only generate fake positives. */
			if (GS(par_id->name) == ID_OB) {
				BLI_assert(((Object *)par_id)->proxy_from == (Object *)id);
				continue;
			}

			/* Shapekeys are considered 'private' to their owner ID here, and never tagged (since they cannot be linked),
			 * so we have to switch effective parent to their owner. */
			if (GS(par_id->name) == ID_KE) {
				par_id = ((Key *)par_id)->from;
			}
		}

		if (par_id->lib == NULL) {
			/* Local user, early out to avoid some gset querying... */
			continue;
		}
		if (!BLI_gset_haskey(done_ids, par_id)) {
			if (BLI_gset_haskey(loop_tags, par_id)) {
				/* We are in a 'dependency loop' of IDs, this does not say us anything, skip it.
				 * Note that this is the situation that can lead to archipelagoes of linked data-blocks
				 * (since all of them have non-local users, they would all be duplicated, leading to a loop of unused
				 * linked data-blocks that cannot be freed since they all use each other...). */
				continue;
			}
			/* Else, recursively check that user ID. */
			library_make_local_copying_check(par_id, loop_tags, id_relations, done_ids);
		}

		if (par_id->tag & LIB_TAG_DOIT) {
			/* This user will be fully local in future, so far so good, nothing to do here but check next user. */
		}
		else {
			/* This user won't be fully local in future, so current ID won't be either. And we are done checking it. */
			id->tag &= ~LIB_TAG_DOIT;
			break;
		}
	}
	BLI_gset_add(done_ids, id);
	BLI_gset_remove(loop_tags, id, NULL);
}

/** Make linked datablocks local.
 *
 * \param bmain Almost certainly global main.
 * \param lib If not NULL, only make local datablocks from this library.
 * \param untagged_only If true, only make local datablocks not tagged with LIB_TAG_PRE_EXISTING.
 * \param set_fake If true, set fake user on all localized datablocks (except group and objects ones).
 */
/* Note: Old (2.77) version was simply making (tagging) datablocks as local, without actually making any check whether
 * they were also indirectly used or not...
 *
 * Current version uses regular id_make_local callback, with advanced pre-processing step to detect all cases of
 * IDs currently indirectly used, but which will be used by local data only once this function is finished.
 * This allows to avoid any unneeded duplication of IDs, and hence all time lost afterwards to remove
 * orphaned linked data-blocks...
 */
void BKE_library_make_local(
        Main *bmain, const Library *lib, GHash *old_to_new_ids, const bool untagged_only, const bool set_fake)
{
	ListBase *lbarray[MAX_LIBARRAY];
	ID *id;
	int a;

	LinkNode *todo_ids = NULL;
	LinkNode *copied_ids = NULL;
	MemArena *linklist_mem = BLI_memarena_new(512 * sizeof(*todo_ids), __func__);

	GSet *done_ids = BLI_gset_ptr_new(__func__);

#ifdef DEBUG_TIME
	TIMEIT_START(make_local);
#endif

	BKE_main_relations_create(bmain);

#ifdef DEBUG_TIME
	printf("Pre-compute current ID relations: Done.\n");
	TIMEIT_VALUE_PRINT(make_local);
#endif

	/* Step 1: Detect datablocks to make local. */
	for (a = set_listbasepointers(bmain, lbarray); a--; ) {
		id = lbarray[a]->first;

		/* Do not explicitly make local non-linkable IDs (shapekeys, in fact), they are assumed to be handled
		 * by real datablocks responsible of them. */
		const bool do_skip = (id && !BKE_idcode_is_linkable(GS(id->name)));

		for (; id; id = id->next) {
			ID *ntree = (ID *)ntreeFromID(id);

			id->tag &= ~LIB_TAG_DOIT;
			if (ntree != NULL) {
				ntree->tag &= ~LIB_TAG_DOIT;
			}

			if (id->lib == NULL) {
				id->tag &= ~(LIB_TAG_EXTERN | LIB_TAG_INDIRECT | LIB_TAG_NEW);
			}
			/* The check on the fourth line (LIB_TAG_PRE_EXISTING) is done so its possible to tag data you don't want to
			 * be made local, used for appending data, so any libdata already linked wont become local (very nasty
			 * to discover all your links are lost after appending).
			 * Also, never ever make proxified objects local, would not make any sense. */
			/* Some more notes:
			 *   - Shapekeys are never tagged here (since they are not linkable).
			 *   - Nodetrees used in materials etc. have to be tagged manually, since they do not exist in Main (!).
			 * This is ok-ish on 'make local' side of things (since those are handled by their 'owner' IDs),
			 * but complicates slightly the pre-processing of relations between IDs at step 2... */
			else if (!do_skip && id->tag & (LIB_TAG_EXTERN | LIB_TAG_INDIRECT | LIB_TAG_NEW) &&
			         ELEM(lib, NULL, id->lib) &&
			         !(GS(id->name) == ID_OB && ((Object *)id)->proxy_from != NULL) &&
			         ((untagged_only == false) || !(id->tag & LIB_TAG_PRE_EXISTING)))
			{
				BLI_linklist_prepend_arena(&todo_ids, id, linklist_mem);
				id->tag |= LIB_TAG_DOIT;

				/* Tag those nasty non-ID nodetrees, but do not add them to todo list, making them local is handled
				 * by 'owner' ID. This is needed for library_make_local_copying_check() to work OK at step 2. */
				if (ntree != NULL) {
					ntree->tag |= LIB_TAG_DOIT;
				}
			}
			else {
				/* Linked ID that we won't be making local (needed info for step 2, see below). */
				BLI_gset_add(done_ids, id);
			}
		}
	}

#ifdef DEBUG_TIME
	printf("Step 1: Detect datablocks to make local: Done.\n");
	TIMEIT_VALUE_PRINT(make_local);
#endif

	/* Step 2: Check which datablocks we can directly make local (because they are only used by already, or future,
	 * local data), others will need to be duplicated. */
	GSet *loop_tags = BLI_gset_ptr_new(__func__);
	for (LinkNode *it = todo_ids; it; it = it->next) {
		library_make_local_copying_check(it->link, loop_tags, bmain->relations, done_ids);
		BLI_assert(BLI_gset_len(loop_tags) == 0);
	}
	BLI_gset_free(loop_tags, NULL);
	BLI_gset_free(done_ids, NULL);

	/* Next step will most likely add new IDs, better to get rid of this mapping now. */
	BKE_main_relations_free(bmain);

#ifdef DEBUG_TIME
	printf("Step 2: Check which datablocks we can directly make local: Done.\n");
	TIMEIT_VALUE_PRINT(make_local);
#endif

	/* Step 3: Make IDs local, either directly (quick and simple), or using generic process,
	 * which involves more complex checks and might instead create a local copy of original linked ID. */
	for (LinkNode *it = todo_ids, *it_next; it; it = it_next) {
		it_next = it->next;
		id = it->link;

		if (id->tag & LIB_TAG_DOIT) {
			/* We know all users of this object are local or will be made fully local, even if currently there are
			 * some indirect usages. So instead of making a copy that we'll likely get rid of later, directly make
			 * that data block local. Saves a tremendous amount of time with complex scenes... */
			id_clear_lib_data_ex(bmain, id, true);
			BKE_id_expand_local(bmain, id);
			id->tag &= ~LIB_TAG_DOIT;
		}
		else {
			/* In this specific case, we do want to make ID local even if it has no local usage yet... */
			if (GS(id->name) == ID_OB) {
				/* Special case for objects because we don't want proxy pointers to be
				 * cleared yet. This will happen down the road in this function.
				 */
				BKE_object_make_local_ex(bmain, (Object *)id, true, false);
			}
			else {
				id_make_local(bmain, id, false, true);
			}

			if (id->newid) {
				/* Reuse already allocated LinkNode (transferring it from todo_ids to copied_ids). */
				BLI_linklist_prepend_nlink(&copied_ids, id, it);
			}
		}

		if (set_fake) {
			if (!ELEM(GS(id->name), ID_OB, ID_GR)) {
				/* do not set fake user on objects, groups (instancing) */
				id_fake_user_set(id);
			}
		}
	}

#ifdef DEBUG_TIME
	printf("Step 3: Make IDs local: Done.\n");
	TIMEIT_VALUE_PRINT(make_local);
#endif

	/* At this point, we are done with directly made local IDs. Now we have to handle duplicated ones, since their
	 * remaining linked original counterpart may not be needed anymore... */
	todo_ids = NULL;

	/* Step 4: We have to remap local usages of old (linked) ID to new (local) ID in a separated loop,
	 * as lbarray ordering is not enough to ensure us we did catch all dependencies
	 * (e.g. if making local a parent object before its child...). See T48907. */
	/* TODO This is now the biggest step by far (in term of processing time). We may be able to gain here by
	 * using again main->relations mapping, but... this implies BKE_libblock_remap & co to be able to update
	 * main->relations on the fly. Have to think about it a bit more, and see whether new code is OK first, anyway. */
	for (LinkNode *it = copied_ids; it; it = it->next) {
		id = it->link;

		BLI_assert(id->newid != NULL);
		BLI_assert(id->lib != NULL);

		BKE_libblock_remap(bmain, id, id->newid, ID_REMAP_SKIP_INDIRECT_USAGE);
		if (old_to_new_ids) {
			BLI_ghash_insert(old_to_new_ids, id, id->newid);
		}

		/* Special hack for groups... Thing is, since we can't instantiate them here, we need to ensure
		 * they remain 'alive' (only instantiation is a real group 'user'... *sigh* See T49722. */
		if (GS(id->name) == ID_GR && (id->tag & LIB_TAG_INDIRECT) != 0) {
			id_us_ensure_real(id->newid);
		}
	}

#ifdef DEBUG_TIME
	printf("Step 4: Remap local usages of old (linked) ID to new (local) ID: Done.\n");
	TIMEIT_VALUE_PRINT(make_local);
#endif

	/* Note: Keeping both version of the code (old one being safer, since it still has checks against unused IDs)
	 * for now, we can remove old one once it has been tested for some time in master... */
#if 1
	/* Step 5: proxy 'remapping' hack. */
	for (LinkNode *it = copied_ids; it; it = it->next) {
		/* Attempt to re-link copied proxy objects. This allows appending of an entire scene
		 * from another blend file into this one, even when that blend file contains proxified
		 * armatures that have local references. Since the proxified object needs to be linked
		 * (not local), this will only work when the "Localize all" checkbox is disabled.
		 * TL;DR: this is a dirty hack on top of an already weak feature (proxies). */
		if (GS(id->name) == ID_OB && ((Object *)id)->proxy != NULL) {
			Object *ob = (Object *)id;
			Object *ob_new = (Object *)id->newid;
			bool is_local = false, is_lib = false;

			/* Proxies only work when the proxified object is linked-in from a library. */
			if (ob->proxy->id.lib == NULL) {
				printf("Warning, proxy object %s will loose its link to %s, because the "
				       "proxified object is local.\n", id->newid->name, ob->proxy->id.name);
				continue;
			}

			BKE_library_ID_test_usages(bmain, id, &is_local, &is_lib);

			/* We can only switch the proxy'ing to a made-local proxy if it is no longer
			 * referred to from a library. Not checking for local use; if new local proxy
			 * was not used locally would be a nasty bug! */
			if (is_local || is_lib) {
				printf("Warning, made-local proxy object %s will loose its link to %s, "
				       "because the linked-in proxy is referenced (is_local=%i, is_lib=%i).\n",
				       id->newid->name, ob->proxy->id.name, is_local, is_lib);
			}
			else {
				/* we can switch the proxy'ing from the linked-in to the made-local proxy.
				 * BKE_object_make_proxy() shouldn't be used here, as it allocates memory that
				 * was already allocated by BKE_object_make_local_ex() (which called BKE_object_copy_ex). */
				ob_new->proxy = ob->proxy;
				ob_new->proxy_group = ob->proxy_group;
				ob_new->proxy_from = ob->proxy_from;
				ob_new->proxy->proxy_from = ob_new;
				ob->proxy = ob->proxy_from = ob->proxy_group = NULL;
			}
		}
	}

#ifdef DEBUG_TIME
	printf("Step 5: Proxy 'remapping' hack: Done.\n");
	TIMEIT_VALUE_PRINT(make_local);
#endif

#else
	LinkNode *linked_loop_candidates = NULL;

	/* Step 5: remove datablocks that have been copied to be localized and are no more used in the end...
	 * Note that we may have to loop more than once here, to tackle dependencies between linked objects... */
	bool do_loop = true;
	while (do_loop) {
		do_loop = false;
		for (LinkNode *it = copied_ids; it; it = it->next) {
			if ((id = it->link) == NULL) {
				continue;
			}

			bool is_local = false, is_lib = false;

			BKE_library_ID_test_usages(bmain, id, &is_local, &is_lib);

			/* Attempt to re-link copied proxy objects. This allows appending of an entire scene
			 * from another blend file into this one, even when that blend file contains proxified
			 * armatures that have local references. Since the proxified object needs to be linked
			 * (not local), this will only work when the "Localize all" checkbox is disabled.
			 * TL;DR: this is a dirty hack on top of an already weak feature (proxies). */
			if (GS(id->name) == ID_OB && ((Object *)id)->proxy != NULL) {
				Object *ob = (Object *)id;
				Object *ob_new = (Object *)id->newid;

				/* Proxies only work when the proxified object is linked-in from a library. */
				if (ob->proxy->id.lib == NULL) {
					printf("Warning, proxy object %s will loose its link to %s, because the "
						   "proxified object is local.\n", id->newid->name, ob->proxy->id.name);
				}
				/* We can only switch the proxy'ing to a made-local proxy if it is no longer
				 * referred to from a library. Not checking for local use; if new local proxy
				 * was not used locally would be a nasty bug! */
				else if (is_local || is_lib) {
					printf("Warning, made-local proxy object %s will loose its link to %s, "
					       "because the linked-in proxy is referenced (is_local=%i, is_lib=%i).\n",
					       id->newid->name, ob->proxy->id.name, is_local, is_lib);
				}
				else {
					/* we can switch the proxy'ing from the linked-in to the made-local proxy.
					 * BKE_object_make_proxy() shouldn't be used here, as it allocates memory that
					 * was already allocated by BKE_object_make_local_ex() (which called BKE_object_copy_ex). */
					ob_new->proxy = ob->proxy;
					ob_new->proxy_group = ob->proxy_group;
					ob_new->proxy_from = ob->proxy_from;
					ob_new->proxy->proxy_from = ob_new;
					ob->proxy = ob->proxy_from = ob->proxy_group = NULL;
				}
			}

			if (!is_local) {
				if (!is_lib) {  /* Not used at all, we can free it! */
					BLI_assert(!"Unused linked data copy remaining from MakeLibLocal process, should not happen anymore");
					printf("\t%s (from %s)\n", id->name, id->lib->id.name);
					BKE_libblock_free(bmain, id);
					it->link = NULL;
					do_loop = true;
				}
				/* Only used by linked data, potential candidate to ugly lib-only dependency cycles... */
				else if ((id->tag & LIB_TAG_DOIT) == 0) {  /* Check TAG_DOIT to avoid adding same ID several times... */
					/* Note that we store the node, not directly ID pointer, that way if it->link is set to NULL
					 * later we can skip it in lib-dependency cycles search later. */
					BLI_linklist_prepend_arena(&linked_loop_candidates, it, linklist_mem);
					id->tag |= LIB_TAG_DOIT;

					/* Grrrrrrr... those half-datablocks-stuff... grrrrrrrrrrr...
					 * Here we have to also tag them as potential candidates, otherwise they would falsy report
					 * ID they used as 'directly used' in sixth step. */
					ID *ntree = (ID *)ntreeFromID(id);
					if (ntree != NULL) {
						ntree->tag |= LIB_TAG_DOIT;
					}
				}
			}
		}
	}

#ifdef DEBUG_TIME
	printf("Step 5: Remove linked datablocks that have been copied and ended fully localized: Done.\n");
	TIMEIT_VALUE_PRINT(make_local);
#endif

	/* Step 6: Try to find circle dependencies between indirectly-linked-only datablocks.
	 * Those are fake 'usages' that prevent their deletion. See T49775 for nice ugly case. */
	BKE_library_unused_linked_data_set_tag(bmain, false);
	for (LinkNode *it = linked_loop_candidates; it; it = it->next) {
		if (it->link == NULL) {
			continue;
		}
		if ((id = ((LinkNode *)it->link)->link) == NULL) {
			it->link = NULL;
			continue;
		}

		/* Note: in theory here we are only handling datablocks forming exclusive linked dependency-cycles-based
		 * archipelagos, so no need to check again after we have deleted one, as done in previous step. */
		if (id->tag & LIB_TAG_DOIT) {
			BLI_assert(!"Unused linked data copy remaining from MakeLibLocal process (archipelago case), should not happen anymore");
			/* Object's deletion rely on valid ob->data, but ob->data may have already been freed here...
			 * Setting it to NULL may not be 100% correct, but should be safe and do the work. */
			if (GS(id->name) == ID_OB) {
				((Object *)id)->data = NULL;
			}

			/* Note: *in theory* IDs tagged here are fully *outside* of file scope, totally unused, so we can
			 *       directly wipe them out without caring about clearing their usages.
			 *       However, this is a highly-risky presumption, and nice crasher in case something goes wrong here.
			 *       So for 2.78a will keep the safe option, and switch to more efficient one in master later. */
#if 1
			BKE_libblock_free_ex(bmain, id, false, true);
#else
			BKE_libblock_unlink(bmain, id, false, false);
			BKE_libblock_free(bmain, id);
#endif
			((LinkNode *)it->link)->link = NULL;  /* Not strictly necessary, but safer (see T49903)... */
			it->link = NULL;
		}
	}

#ifdef DEBUG_TIME
	printf("Step 6: Try to find circle dependencies between indirectly-linked-only datablocks: Done.\n");
	TIMEIT_VALUE_PRINT(make_local);
#endif

#endif

	/* This is probably more of a hack than something we should do here, but...
	 * Issue is, the whole copying + remapping done in complex cases above may leave pose channels of armatures
	 * in complete invalid state (more precisely, the bone pointers of the pchans - very crappy cross-datablocks
	 * relationship), se we tag it to be fully recomputed, but this does not seems to be enough in some cases,
	 * and evaluation code ends up trying to evaluate a not-yet-updated armature object's deformations.
	 * Try "make all local" in 04_01_H.lighting.blend from Agent327 without this, e.g. */
	for (Object *ob = bmain->object.first; ob; ob = ob->id.next) {
		if (ob->data != NULL && ob->type == OB_ARMATURE && ob->pose != NULL && ob->pose->flag & POSE_RECALC) {
			BKE_pose_rebuild(bmain, ob, ob->data, true);
		}
	}

#ifdef DEBUG_TIME
	printf("Hack: Forcefully rebuild armature object poses: Done.\n");
	TIMEIT_VALUE_PRINT(make_local);
#endif

	BKE_main_id_clear_newpoins(bmain);
	BLI_memarena_free(linklist_mem);

#ifdef DEBUG_TIME
	printf("Cleanup and finish: Done.\n");
	TIMEIT_END(make_local);
#endif
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

void BKE_library_filepath_set(Main *bmain, Library *lib, const char *filepath)
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
		/* Never make paths relative to parent lib - reading code (blenloader) always set *all* lib->name relative to
		 * current main, not to their parent for indirectly linked ones. */
		const char *basepath = BKE_main_blendfile_path(bmain);
		BLI_path_abs(lib->filepath, basepath);
	}
}

void BKE_id_tag_set_atomic(ID *id, int tag)
{
	atomic_fetch_and_or_int32(&id->tag, tag);
}

void BKE_id_tag_clear_atomic(ID *id, int tag)
{
	atomic_fetch_and_and_int32(&id->tag, ~tag);
}

/** Check that given ID pointer actually is in G_MAIN.
 * Main intended use is for debug asserts in places we cannot easily get rid of G_Main... */
bool BKE_id_is_in_gobal_main(ID *id)
{
	/* We do not want to fail when id is NULL here, even though this is a bit strange behavior... */
	return (id == NULL || BLI_findindex(which_libbase(G_MAIN, GS(id->name)), id) != -1);
}
