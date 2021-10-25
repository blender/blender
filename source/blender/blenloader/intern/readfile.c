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
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenloader/intern/readfile.c
 *  \ingroup blenloader
 */


#include "zlib.h"

#include <limits.h>
#include <stdio.h> // for printf fopen fwrite fclose sprintf FILE
#include <stdlib.h> // for getenv atoi
#include <stddef.h> // for offsetof
#include <fcntl.h> // for open
#include <string.h> // for strrchr strncmp strstr
#include <math.h> // for fabs
#include <stdarg.h> /* for va_start/end */
#include <time.h> /* for gmtime */

#include "BLI_utildefines.h"
#ifndef WIN32
#  include <unistd.h> // for read close
#else
#  include <io.h> // for open close read
#  include "winsock2.h"
#  include "BLI_winstuff.h"
#endif

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_actuator_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_cloth_types.h"
#include "DNA_controller_types.h"
#include "DNA_constraint_types.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_effect_types.h"
#include "DNA_fileglobal_types.h"
#include "DNA_genfile.h"
#include "DNA_group_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_lamp_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_meta_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_nla_types.h"
#include "DNA_node_types.h"
#include "DNA_object_fluidsim.h" // NT
#include "DNA_object_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_particle_types.h"
#include "DNA_property_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_text_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_sensor_types.h"
#include "DNA_sdna_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_smoke_types.h"
#include "DNA_speaker_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_vfont_types.h"
#include "DNA_world_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_mask_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_endian_switch.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_mempool.h"

#include "BLT_translation.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_brush.h"
#include "BKE_cachefile.h"
#include "BKE_cloth.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_effect.h"
#include "BKE_fcurve.h"
#include "BKE_global.h" // for G
#include "BKE_group.h"
#include "BKE_library.h" // for which_libbase
#include "BKE_library_idmap.h"
#include "BKE_library_query.h"
#include "BKE_idcode.h"
#include "BKE_idprop.h"
#include "BKE_material.h"
#include "BKE_main.h" // for Main
#include "BKE_mesh.h" // for ME_ defines (patching)
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_node.h" // for tree type defines
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_sca.h" // for init_actuator
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_sequencer.h"
#include "BKE_outliner_treehash.h"
#include "BKE_sound.h"
#include "BKE_colortools.h"

#include "NOD_common.h"
#include "NOD_socket.h"

#include "BLO_readfile.h"
#include "BLO_undofile.h"
#include "BLO_blend_defs.h"

#include "RE_engine.h"

#include "readfile.h"


#include <errno.h>

/**
 * READ
 * ====
 *
 * - Existing Library (#Main) push or free
 * - allocate new #Main
 * - load file
 * - read #SDNA
 * - for each LibBlock
 *   - read LibBlock
 *   - if a Library
 *     - make a new #Main
 *     - attach ID's to it
 *   - else
 *     - read associated 'direct data'
 *     - link direct data (internal and to LibBlock)
 * - read #FileGlobal
 * - read #USER data, only when indicated (file is ``~/X.XX/startup.blend``)
 * - free file
 * - per Library (per #Main)
 *   - read file
 *   - read #SDNA
 *   - find LibBlocks and attach #ID's to #Main
 *     - if external LibBlock
 *       - search all #Main's
 *         - or it's already read,
 *         - or not read yet
 *         - or make new #Main
 *   - per LibBlock
 *     - read recursive
 *     - read associated direct data
 *     - link direct data (internal and to LibBlock)
 *   - free file
 * - per Library with unread LibBlocks
 *   - read file
 *   - read #SDNA
 *   - per LibBlock
 *     - read recursive
 *     - read associated direct data
 *     - link direct data (internal and to LibBlock)
 *   - free file
 * - join all #Main's
 * - link all LibBlocks and indirect pointers to libblocks
 * - initialize #FileGlobal and copy pointers to #Global
 *
 * \note Still a weak point is the new-address function, that doesnt solve reading from
 * multiple files at the same time.
 * (added remark: oh, i thought that was solved? will look at that... (ton).
 */

/* use GHash for BHead name-based lookups (speeds up linking) */
#define USE_GHASH_BHEAD

/* Use GHash for restoring pointers by name */
#define USE_GHASH_RESTORE_POINTER

/***/

typedef struct OldNew {
	const void *old;
	void *newp;
	int nr;
} OldNew;

typedef struct OldNewMap {
	OldNew *entries;
	int nentries, entriessize;
	bool sorted;
	int lasthit;
} OldNewMap;


/* local prototypes */
static void *read_struct(FileData *fd, BHead *bh, const char *blockname);
static void direct_link_modifiers(FileData *fd, ListBase *lb);
static void convert_tface_mt(FileData *fd, Main *main);
static BHead *find_bhead_from_code_name(FileData *fd, const short idcode, const char *name);
static BHead *find_bhead_from_idname(FileData *fd, const char *idname);

/* this function ensures that reports are printed,
 * in the case of libraray linking errors this is important!
 *
 * bit kludge but better then doubling up on prints,
 * we could alternatively have a versions of a report function which forces printing - campbell
 */

void blo_reportf_wrap(ReportList *reports, ReportType type, const char *format, ...)
{
	char fixed_buf[1024]; /* should be long enough */
	
	va_list args;
	
	va_start(args, format);
	vsnprintf(fixed_buf, sizeof(fixed_buf), format, args);
	va_end(args);
	
	fixed_buf[sizeof(fixed_buf) - 1] = '\0';
	
	BKE_report(reports, type, fixed_buf);
	
	if (G.background == 0) {
		printf("%s: %s\n", BKE_report_type_str(type), fixed_buf);
	}
}

/* for reporting linking messages */
static const char *library_parent_filepath(Library *lib)
{
	return lib->parent ? lib->parent->filepath : "<direct>";
}

static OldNewMap *oldnewmap_new(void) 
{
	OldNewMap *onm= MEM_callocN(sizeof(*onm), "OldNewMap");
	
	onm->entriessize = 1024;
	onm->entries = MEM_malloc_arrayN(onm->entriessize, sizeof(*onm->entries), "OldNewMap.entries");
	
	return onm;
}

static int verg_oldnewmap(const void *v1, const void *v2)
{
	const struct OldNew *x1=v1, *x2=v2;
	
	if (x1->old > x2->old) return 1;
	else if (x1->old < x2->old) return -1;
	return 0;
}


static void oldnewmap_sort(FileData *fd) 
{
	BLI_assert(fd->libmap->sorted == false);
	qsort(fd->libmap->entries, fd->libmap->nentries, sizeof(OldNew), verg_oldnewmap);
	fd->libmap->sorted = 1;
}

/* nr is zero for data, and ID code for libdata */
static void oldnewmap_insert(OldNewMap *onm, const void *oldaddr, void *newaddr, int nr)
{
	OldNew *entry;
	
	if (oldaddr==NULL || newaddr==NULL) return;
	
	if (UNLIKELY(onm->nentries == onm->entriessize)) {
		onm->entriessize *= 2;
		onm->entries = MEM_reallocN(onm->entries, sizeof(*onm->entries) * onm->entriessize);
	}

	entry = &onm->entries[onm->nentries++];
	entry->old = oldaddr;
	entry->newp = newaddr;
	entry->nr = nr;
}

void blo_do_versions_oldnewmap_insert(OldNewMap *onm, const void *oldaddr, void *newaddr, int nr)
{
	oldnewmap_insert(onm, oldaddr, newaddr, nr);
}

/**
 * Do a full search (no state).
 *
 * \param lasthit: Use as a reference position to avoid a full search
 * from either end of the array, giving more efficient lookups.
 *
 * \note This would seem an ideal case for hash or btree lookups.
 * However the data is written in-order, using the \a lasthit will normally avoid calling this function.
 * Creating a btree/hash structure adds overhead for the common-case to optimize the corner-case
 * (since most entries will never be retrieved).
 * So just keep full lookups as a fall-back.
 */
static int oldnewmap_lookup_entry_full(const OldNewMap *onm, const void *addr, int lasthit)
{
	const int nentries = onm->nentries;
	const OldNew *entries = onm->entries;
	int i;

	/* search relative to lasthit where possible */
	if (lasthit >= 0 && lasthit < nentries) {

		/* search forwards */
		i = lasthit;
		while (++i != nentries) {
			if (entries[i].old == addr) {
				return i;
			}
		}

		/* search backwards */
		i = lasthit + 1;
		while (i--) {
			if (entries[i].old == addr) {
				return i;
			}
		}
	}
	else {
		/* search backwards (full) */
		i = nentries;
		while (i--) {
			if (entries[i].old == addr) {
				return i;
			}
		}
	}

	return -1;
}

static void *oldnewmap_lookup_and_inc(OldNewMap *onm, const void *addr, bool increase_users)
{
	int i;
	
	if (addr == NULL) return NULL;
	
	if (onm->lasthit < onm->nentries-1) {
		OldNew *entry = &onm->entries[++onm->lasthit];
		
		if (entry->old == addr) {
			if (increase_users)
				entry->nr++;
			return entry->newp;
		}
	}
	
	i = oldnewmap_lookup_entry_full(onm, addr, onm->lasthit);
	if (i != -1) {
		OldNew *entry = &onm->entries[i];
		BLI_assert(entry->old == addr);
		onm->lasthit = i;
		if (increase_users)
			entry->nr++;
		return entry->newp;
	}
	
	return NULL;
}

/* for libdata, nr has ID code, no increment */
static void *oldnewmap_liblookup(OldNewMap *onm, const void *addr, const void *lib)
{
	if (addr == NULL) {
		return NULL;
	}

	/* lasthit works fine for non-libdata, linking there is done in same sequence as writing */
	if (onm->sorted) {
		const OldNew entry_s = {.old = addr};
		OldNew *entry = bsearch(&entry_s, onm->entries, onm->nentries, sizeof(OldNew), verg_oldnewmap);
		if (entry) {
			ID *id = entry->newp;

			if (id && (!lib || id->lib)) {
				return id;
			}
		}
	}
	else {
		/* note, this can be a bottle neck when loading some files */
		const int i = oldnewmap_lookup_entry_full(onm, addr, -1);
		if (i != -1) {
			OldNew *entry = &onm->entries[i];
			ID *id = entry->newp;
			BLI_assert(entry->old == addr);
			if (id && (!lib || id->lib)) {
				return id;
			}
		}
	}

	return NULL;
}

static void oldnewmap_free_unused(OldNewMap *onm) 
{
	int i;

	for (i = 0; i < onm->nentries; i++) {
		OldNew *entry = &onm->entries[i];
		if (entry->nr == 0) {
			MEM_freeN(entry->newp);
			entry->newp = NULL;
		}
	}
}

static void oldnewmap_clear(OldNewMap *onm) 
{
	onm->nentries = 0;
	onm->lasthit = 0;
}

static void oldnewmap_free(OldNewMap *onm) 
{
	MEM_freeN(onm->entries);
	MEM_freeN(onm);
}

/***/

static void read_libraries(FileData *basefd, ListBase *mainlist);

/* ************ help functions ***************** */

static void add_main_to_main(Main *mainvar, Main *from)
{
	ListBase *lbarray[MAX_LIBARRAY], *fromarray[MAX_LIBARRAY];
	int a;
	
	set_listbasepointers(mainvar, lbarray);
	a = set_listbasepointers(from, fromarray);
	while (a--) {
		BLI_movelisttolist(lbarray[a], fromarray[a]);
	}
}

void blo_join_main(ListBase *mainlist)
{
	Main *tojoin, *mainl;
	
	mainl = mainlist->first;
	while ((tojoin = mainl->next)) {
		add_main_to_main(mainl, tojoin);
		BLI_remlink(mainlist, tojoin);
		BKE_main_free(tojoin);
	}
}

static void split_libdata(ListBase *lb_src, Main **lib_main_array, const unsigned int lib_main_array_len)
{
	for (ID *id = lb_src->first, *idnext; id; id = idnext) {
		idnext = id->next;

		if (id->lib) {
			if (((unsigned int)id->lib->temp_index < lib_main_array_len) &&
			    /* this check should never fail, just incase 'id->lib' is a dangling pointer. */
			    (lib_main_array[id->lib->temp_index]->curlib == id->lib))
			{
				Main *mainvar = lib_main_array[id->lib->temp_index];
				ListBase *lb_dst = which_libbase(mainvar, GS(id->name));
				BLI_remlink(lb_src, id);
				BLI_addtail(lb_dst, id);
			}
			else {
				printf("%s: invalid library for '%s'\n", __func__, id->name);
				BLI_assert(0);
			}
		}
	}
}

void blo_split_main(ListBase *mainlist, Main *main)
{
	mainlist->first = mainlist->last = main;
	main->next = NULL;
	
	if (BLI_listbase_is_empty(&main->library))
		return;
	
	/* (Library.temp_index -> Main), lookup table */
	const unsigned int lib_main_array_len = BLI_listbase_count(&main->library);
	Main             **lib_main_array     = MEM_malloc_arrayN(lib_main_array_len, sizeof(*lib_main_array), __func__);

	int i = 0;
	for (Library *lib = main->library.first; lib; lib = lib->id.next, i++) {
		Main *libmain = BKE_main_new();
		libmain->curlib = lib;
		libmain->versionfile = lib->versionfile;
		libmain->subversionfile = lib->subversionfile;
		BLI_addtail(mainlist, libmain);
		lib->temp_index = i;
		lib_main_array[i] = libmain;
	}
	
	ListBase *lbarray[MAX_LIBARRAY];
	i = set_listbasepointers(main, lbarray);
	while (i--) {
		split_libdata(lbarray[i], lib_main_array, lib_main_array_len);
	}

	MEM_freeN(lib_main_array);
}

static void read_file_version(FileData *fd, Main *main)
{
	BHead *bhead;
	
	for (bhead= blo_firstbhead(fd); bhead; bhead= blo_nextbhead(fd, bhead)) {
		if (bhead->code == GLOB) {
			FileGlobal *fg= read_struct(fd, bhead, "Global");
			if (fg) {
				main->subversionfile= fg->subversion;
				main->minversionfile= fg->minversion;
				main->minsubversionfile= fg->minsubversion;
				MEM_freeN(fg);
			}
			else if (bhead->code == ENDB)
				break;
		}
	}
	if (main->curlib) {
		main->curlib->versionfile = main->versionfile;
		main->curlib->subversionfile = main->subversionfile;
	}
}

#ifdef USE_GHASH_BHEAD
static void read_file_bhead_idname_map_create(FileData *fd)
{
	BHead *bhead;

	/* dummy values */
	bool is_link = false;
	int code_prev = ENDB;
	unsigned int reserve = 0;

	for (bhead = blo_firstbhead(fd); bhead; bhead = blo_nextbhead(fd, bhead)) {
		if (code_prev != bhead->code) {
			code_prev = bhead->code;
			is_link = BKE_idcode_is_valid(code_prev) ? BKE_idcode_is_linkable(code_prev) : false;
		}

		if (is_link) {
			reserve += 1;
		}
	}

	BLI_assert(fd->bhead_idname_hash == NULL);

	fd->bhead_idname_hash = BLI_ghash_str_new_ex(__func__, reserve);

	for (bhead = blo_firstbhead(fd); bhead; bhead = blo_nextbhead(fd, bhead)) {
		if (code_prev != bhead->code) {
			code_prev = bhead->code;
			is_link = BKE_idcode_is_valid(code_prev) ? BKE_idcode_is_linkable(code_prev) : false;
		}

		if (is_link) {
			BLI_ghash_insert(fd->bhead_idname_hash, (void *)bhead_id_name(fd, bhead), bhead);
		}
	}
}
#endif


static Main *blo_find_main(FileData *fd, const char *filepath, const char *relabase)
{
	ListBase *mainlist = fd->mainlist;
	Main *m;
	Library *lib;
	char name1[FILE_MAX];
	
	BLI_strncpy(name1, filepath, sizeof(name1));
	BLI_cleanup_path(relabase, name1);
	
//	printf("blo_find_main: relabase  %s\n", relabase);
//	printf("blo_find_main: original in  %s\n", filepath);
//	printf("blo_find_main: converted to %s\n", name1);
	
	for (m = mainlist->first; m; m = m->next) {
		const char *libname = (m->curlib) ? m->curlib->filepath : m->name;
		
		if (BLI_path_cmp(name1, libname) == 0) {
			if (G.debug & G_DEBUG) printf("blo_find_main: found library %s\n", libname);
			return m;
		}
	}
	
	m = BKE_main_new();
	BLI_addtail(mainlist, m);
	
	/* Add library datablock itself to 'main' Main, since libraries are **never** linked data.
	 * Fixes bug where you could end with all ID_LI datablocks having the same name... */
	lib = BKE_libblock_alloc(mainlist->first, ID_LI, "Lib");
	lib->id.us = ID_FAKE_USERS(lib);  /* Important, consistency with main ID reading code from read_libblock(). */
	BLI_strncpy(lib->name, filepath, sizeof(lib->name));
	BLI_strncpy(lib->filepath, name1, sizeof(lib->filepath));
	
	m->curlib = lib;
	
	read_file_version(fd, m);
	
	if (G.debug & G_DEBUG) printf("blo_find_main: added new lib %s\n", filepath);
	return m;
}


/* ************ FILE PARSING ****************** */

static void switch_endian_bh4(BHead4 *bhead)
{
	/* the ID_.. codes */
	if ((bhead->code & 0xFFFF)==0) bhead->code >>= 16;
	
	if (bhead->code != ENDB) {
		BLI_endian_switch_int32(&bhead->len);
		BLI_endian_switch_int32(&bhead->SDNAnr);
		BLI_endian_switch_int32(&bhead->nr);
	}
}

static void switch_endian_bh8(BHead8 *bhead)
{
	/* the ID_.. codes */
	if ((bhead->code & 0xFFFF)==0) bhead->code >>= 16;
	
	if (bhead->code != ENDB) {
		BLI_endian_switch_int32(&bhead->len);
		BLI_endian_switch_int32(&bhead->SDNAnr);
		BLI_endian_switch_int32(&bhead->nr);
	}
}

static void bh4_from_bh8(BHead *bhead, BHead8 *bhead8, int do_endian_swap)
{
	BHead4 *bhead4 = (BHead4 *) bhead;
	int64_t old;

	bhead4->code = bhead8->code;
	bhead4->len = bhead8->len;

	if (bhead4->code != ENDB) {
		/* perform a endian swap on 64bit pointers, otherwise the pointer might map to zero
		 * 0x0000000000000000000012345678 would become 0x12345678000000000000000000000000
		 */
		if (do_endian_swap) {
			BLI_endian_switch_int64(&bhead8->old);
		}
		
		/* this patch is to avoid a long long being read from not-eight aligned positions
		 * is necessary on any modern 64bit architecture) */
		memcpy(&old, &bhead8->old, 8);
		bhead4->old = (int) (old >> 3);
		
		bhead4->SDNAnr = bhead8->SDNAnr;
		bhead4->nr = bhead8->nr;
	}
}

static void bh8_from_bh4(BHead *bhead, BHead4 *bhead4)
{
	BHead8 *bhead8 = (BHead8 *) bhead;
	
	bhead8->code = bhead4->code;
	bhead8->len = bhead4->len;
	
	if (bhead8->code != ENDB) {
		bhead8->old = bhead4->old;
		bhead8->SDNAnr = bhead4->SDNAnr;
		bhead8->nr= bhead4->nr;
	}
}

static BHeadN *get_bhead(FileData *fd)
{
	BHeadN *new_bhead = NULL;
	int readsize;
	
	if (fd) {
		if (!fd->eof) {
			/* initializing to zero isn't strictly needed but shuts valgrind up
			 * since uninitialized memory gets compared */
			BHead8 bhead8 = {0};
			BHead4 bhead4 = {0};
			BHead  bhead = {0};
			
			/* First read the bhead structure.
			 * Depending on the platform the file was written on this can
			 * be a big or little endian BHead4 or BHead8 structure.
			 *
			 * As usual 'ENDB' (the last *partial* bhead of the file)
			 * needs some special handling. We don't want to EOF just yet.
			 */
			if (fd->flags & FD_FLAGS_FILE_POINTSIZE_IS_4) {
				bhead4.code = DATA;
				readsize = fd->read(fd, &bhead4, sizeof(bhead4));
				
				if (readsize == sizeof(bhead4) || bhead4.code == ENDB) {
					if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
						switch_endian_bh4(&bhead4);
					}
					
					if (fd->flags & FD_FLAGS_POINTSIZE_DIFFERS) {
						bh8_from_bh4(&bhead, &bhead4);
					}
					else {
						memcpy(&bhead, &bhead4, sizeof(bhead));
					}
				}
				else {
					fd->eof = 1;
					bhead.len= 0;
				}
			}
			else {
				bhead8.code = DATA;
				readsize = fd->read(fd, &bhead8, sizeof(bhead8));
				
				if (readsize == sizeof(bhead8) || bhead8.code == ENDB) {
					if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
						switch_endian_bh8(&bhead8);
					}
					
					if (fd->flags & FD_FLAGS_POINTSIZE_DIFFERS) {
						bh4_from_bh8(&bhead, &bhead8, (fd->flags & FD_FLAGS_SWITCH_ENDIAN));
					}
					else {
						memcpy(&bhead, &bhead8, sizeof(bhead));
					}
				}
				else {
					fd->eof = 1;
					bhead.len= 0;
				}
			}
			
			/* make sure people are not trying to pass bad blend files */
			if (bhead.len < 0) fd->eof = 1;
			
			/* bhead now contains the (converted) bhead structure. Now read
			 * the associated data and put everything in a BHeadN (creative naming !)
			 */
			if (!fd->eof) {
				new_bhead = MEM_mallocN(sizeof(BHeadN) + bhead.len, "new_bhead");
				if (new_bhead) {
					new_bhead->next = new_bhead->prev = NULL;
					new_bhead->bhead = bhead;
					
					readsize = fd->read(fd, new_bhead + 1, bhead.len);
					
					if (readsize != bhead.len) {
						fd->eof = 1;
						MEM_freeN(new_bhead);
						new_bhead = NULL;
					}
				}
				else {
					fd->eof = 1;
				}
			}
		}
	}

	/* We've read a new block. Now add it to the list
	 * of blocks.
	 */
	if (new_bhead) {
		BLI_addtail(&fd->listbase, new_bhead);
	}
	
	return(new_bhead);
}

BHead *blo_firstbhead(FileData *fd)
{
	BHeadN *new_bhead;
	BHead *bhead = NULL;
	
	/* Rewind the file
	 * Read in a new block if necessary
	 */
	new_bhead = fd->listbase.first;
	if (new_bhead == NULL) {
		new_bhead = get_bhead(fd);
	}
	
	if (new_bhead) {
		bhead = &new_bhead->bhead;
	}
	
	return(bhead);
}

BHead *blo_prevbhead(FileData *UNUSED(fd), BHead *thisblock)
{
	BHeadN *bheadn = (BHeadN *)POINTER_OFFSET(thisblock, -offsetof(BHeadN, bhead));
	BHeadN *prev = bheadn->prev;
	
	return (prev) ? &prev->bhead : NULL;
}

BHead *blo_nextbhead(FileData *fd, BHead *thisblock)
{
	BHeadN *new_bhead = NULL;
	BHead *bhead = NULL;
	
	if (thisblock) {
		/* bhead is actually a sub part of BHeadN
		 * We calculate the BHeadN pointer from the BHead pointer below */
		new_bhead = (BHeadN *)POINTER_OFFSET(thisblock, -offsetof(BHeadN, bhead));
		
		/* get the next BHeadN. If it doesn't exist we read in the next one */
		new_bhead = new_bhead->next;
		if (new_bhead == NULL) {
			new_bhead = get_bhead(fd);
		}
	}
	
	if (new_bhead) {
		/* here we do the reverse:
		 * go from the BHeadN pointer to the BHead pointer */
		bhead = &new_bhead->bhead;
	}
	
	return(bhead);
}

/* Warning! Caller's responsability to ensure given bhead **is** and ID one! */
const char *bhead_id_name(const FileData *fd, const BHead *bhead)
{
	return (const char *)POINTER_OFFSET(bhead, sizeof(*bhead) + fd->id_name_offs);
}

static void decode_blender_header(FileData *fd)
{
	char header[SIZEOFBLENDERHEADER], num[4];
	int readsize;
	
	/* read in the header data */
	readsize = fd->read(fd, header, sizeof(header));
	
	if (readsize == sizeof(header)) {
		if (STREQLEN(header, "BLENDER", 7)) {
			fd->flags |= FD_FLAGS_FILE_OK;
			
			/* what size are pointers in the file ? */
			if (header[7]=='_') {
				fd->flags |= FD_FLAGS_FILE_POINTSIZE_IS_4;
				if (sizeof(void *) != 4) {
					fd->flags |= FD_FLAGS_POINTSIZE_DIFFERS;
				}
			}
			else {
				if (sizeof(void *) != 8) {
					fd->flags |= FD_FLAGS_POINTSIZE_DIFFERS;
				}
			}
			
			/* is the file saved in a different endian
			 * than we need ?
			 */
			if (((header[8] == 'v') ? L_ENDIAN : B_ENDIAN) != ENDIAN_ORDER) {
				fd->flags |= FD_FLAGS_SWITCH_ENDIAN;
			}
			
			/* get the version number */
			memcpy(num, header + 9, 3);
			num[3] = 0;
			fd->fileversion = atoi(num);
		}
	}
}

/**
 * \return Success if the file is read correctly, else set \a r_error_message.
 */
static bool read_file_dna(FileData *fd, const char **r_error_message)
{
	BHead *bhead;
	
	for (bhead = blo_firstbhead(fd); bhead; bhead = blo_nextbhead(fd, bhead)) {
		if (bhead->code == DNA1) {
			const bool do_endian_swap = (fd->flags & FD_FLAGS_SWITCH_ENDIAN) != 0;
			
			fd->filesdna = DNA_sdna_from_data(&bhead[1], bhead->len, do_endian_swap, true, r_error_message);
			if (fd->filesdna) {
				fd->compflags = DNA_struct_get_compareflags(fd->filesdna, fd->memsdna);
				/* used to retrieve ID names from (bhead+1) */
				fd->id_name_offs = DNA_elem_offset(fd->filesdna, "ID", "char", "name[]");

				return true;
			}
			else {
				return false;
			}
			
		}
		else if (bhead->code == ENDB)
			break;
	}
	
	*r_error_message = "Missing DNA block";
	return false;
}

static int *read_file_thumbnail(FileData *fd)
{
	BHead *bhead;
	int *blend_thumb = NULL;

	for (bhead = blo_firstbhead(fd); bhead; bhead = blo_nextbhead(fd, bhead)) {
		if (bhead->code == TEST) {
			const bool do_endian_swap = (fd->flags & FD_FLAGS_SWITCH_ENDIAN) != 0;
			int *data = (int *)(bhead + 1);

			if (bhead->len < (2 * sizeof(int))) {
				break;
			}

			if (do_endian_swap) {
				BLI_endian_switch_int32(&data[0]);
				BLI_endian_switch_int32(&data[1]);
			}

			int width = data[0];
			int height = data[1];

			if (!BLEN_THUMB_SAFE_MEMSIZE(width, height)) {
				break;
			}
			if (bhead->len < BLEN_THUMB_MEMSIZE_FILE(width, height)) {
				break;
			}

			blend_thumb = data;
			break;
		}
		else if (bhead->code != REND) {
			/* Thumbnail is stored in TEST immediately after first REND... */
			break;
		}
	}

	return blend_thumb;
}

static int fd_read_from_file(FileData *filedata, void *buffer, unsigned int size)
{
	int readsize = read(filedata->filedes, buffer, size);
	
	if (readsize < 0) {
		readsize = EOF;
	}
	else {
		filedata->seek += readsize;
	}
	
	return readsize;
}

static int fd_read_gzip_from_file(FileData *filedata, void *buffer, unsigned int size)
{
	int readsize = gzread(filedata->gzfiledes, buffer, size);
	
	if (readsize < 0) {
		readsize = EOF;
	}
	else {
		filedata->seek += readsize;
	}
	
	return (readsize);
}

static int fd_read_from_memory(FileData *filedata, void *buffer, unsigned int size)
{
	/* don't read more bytes then there are available in the buffer */
	int readsize = (int)MIN2(size, (unsigned int)(filedata->buffersize - filedata->seek));
	
	memcpy(buffer, filedata->buffer + filedata->seek, readsize);
	filedata->seek += readsize;
	
	return (readsize);
}

static int fd_read_from_memfile(FileData *filedata, void *buffer, unsigned int size)
{
	static unsigned int seek = (1<<30);	/* the current position */
	static unsigned int offset = 0;		/* size of previous chunks */
	static MemFileChunk *chunk = NULL;
	unsigned int chunkoffset, readsize, totread;
	
	if (size == 0) return 0;
	
	if (seek != (unsigned int)filedata->seek) {
		chunk = filedata->memfile->chunks.first;
		seek = 0;
		
		while (chunk) {
			if (seek + chunk->size > (unsigned) filedata->seek) break;
			seek += chunk->size;
			chunk = chunk->next;
		}
		offset = seek;
		seek = filedata->seek;
	}
	
	if (chunk) {
		totread = 0;
		
		do {
			/* first check if it's on the end if current chunk */
			if (seek-offset == chunk->size) {
				offset += chunk->size;
				chunk = chunk->next;
			}
			
			/* debug, should never happen */
			if (chunk == NULL) {
				printf("illegal read, chunk zero\n");
				return 0;
			}
			
			chunkoffset = seek-offset;
			readsize = size-totread;
			
			/* data can be spread over multiple chunks, so clamp size
			 * to within this chunk, and then it will read further in
			 * the next chunk */
			if (chunkoffset+readsize > chunk->size)
				readsize= chunk->size-chunkoffset;
			
			memcpy(POINTER_OFFSET(buffer, totread), chunk->buf + chunkoffset, readsize);
			totread += readsize;
			filedata->seek += readsize;
			seek += readsize;
		} while (totread < size);
		
		return totread;
	}
	
	return 0;
}

static FileData *filedata_new(void)
{
	FileData *fd = MEM_callocN(sizeof(FileData), "FileData");
	
	fd->filedes = -1;
	fd->gzfiledes = NULL;

	fd->memsdna = DNA_sdna_current_get();

	fd->datamap = oldnewmap_new();
	fd->globmap = oldnewmap_new();
	fd->libmap = oldnewmap_new();
	
	return fd;
}

static FileData *blo_decode_and_check(FileData *fd, ReportList *reports)
{
	decode_blender_header(fd);
	
	if (fd->flags & FD_FLAGS_FILE_OK) {
		const char *error_message = NULL;
		if (read_file_dna(fd, &error_message) == false) {
			BKE_reportf(reports, RPT_ERROR,
			            "Failed to read blend file '%s': %s",
			            fd->relabase, error_message);
			blo_freefiledata(fd);
			fd = NULL;
		}
	}
	else {
		BKE_reportf(reports, RPT_ERROR, "Failed to read blend file '%s', not a blend file", fd->relabase);
		blo_freefiledata(fd);
		fd = NULL;
	}
	
	return fd;
}

/* cannot be called with relative paths anymore! */
/* on each new library added, it now checks for the current FileData and expands relativeness */
FileData *blo_openblenderfile(const char *filepath, ReportList *reports)
{
	gzFile gzfile;
	errno = 0;
	gzfile = BLI_gzopen(filepath, "rb");
	
	if (gzfile == (gzFile)Z_NULL) {
		BKE_reportf(reports, RPT_WARNING, "Unable to open '%s': %s",
		            filepath, errno ? strerror(errno) : TIP_("unknown error reading file"));
		return NULL;
	}
	else {
		FileData *fd = filedata_new();
		fd->gzfiledes = gzfile;
		fd->read = fd_read_gzip_from_file;
		
		/* needed for library_append and read_libraries */
		BLI_strncpy(fd->relabase, filepath, sizeof(fd->relabase));
		
		return blo_decode_and_check(fd, reports);
	}
}

/**
 * Same as blo_openblenderfile(), but does not reads DNA data, only header. Use it for light access
 * (e.g. thumbnail reading).
 */
static FileData *blo_openblenderfile_minimal(const char *filepath)
{
	gzFile gzfile;
	errno = 0;
	gzfile = BLI_gzopen(filepath, "rb");

	if (gzfile != (gzFile)Z_NULL) {
		FileData *fd = filedata_new();
		fd->gzfiledes = gzfile;
		fd->read = fd_read_gzip_from_file;

		decode_blender_header(fd);

		if (fd->flags & FD_FLAGS_FILE_OK) {
			return fd;
		}

		blo_freefiledata(fd);
	}

	return NULL;
}

static int fd_read_gzip_from_memory(FileData *filedata, void *buffer, unsigned int size)
{
	int err;

	filedata->strm.next_out = (Bytef *) buffer;
	filedata->strm.avail_out = size;

	// Inflate another chunk.
	err = inflate (&filedata->strm, Z_SYNC_FLUSH);

	if (err == Z_STREAM_END) {
		return 0;
	}
	else if (err != Z_OK) {
		printf("fd_read_gzip_from_memory: zlib error\n");
		return 0;
	}

	filedata->seek += size;

	return (size);
}

static int fd_read_gzip_from_memory_init(FileData *fd)
{

	fd->strm.next_in = (Bytef *) fd->buffer;
	fd->strm.avail_in = fd->buffersize;
	fd->strm.total_out = 0;
	fd->strm.zalloc = Z_NULL;
	fd->strm.zfree = Z_NULL;
	
	if (inflateInit2(&fd->strm, (16+MAX_WBITS)) != Z_OK)
		return 0;

	fd->read = fd_read_gzip_from_memory;
	
	return 1;
}

FileData *blo_openblendermemory(const void *mem, int memsize, ReportList *reports)
{
	if (!mem || memsize<SIZEOFBLENDERHEADER) {
		BKE_report(reports, RPT_WARNING, (mem) ? TIP_("Unable to read"): TIP_("Unable to open"));
		return NULL;
	}
	else {
		FileData *fd = filedata_new();
		const char *cp = mem;
		
		fd->buffer = mem;
		fd->buffersize = memsize;
		
		/* test if gzip */
		if (cp[0] == 0x1f && cp[1] == 0x8b) {
			if (0 == fd_read_gzip_from_memory_init(fd)) {
				blo_freefiledata(fd);
				return NULL;
			}
		}
		else
			fd->read = fd_read_from_memory;
			
		fd->flags |= FD_FLAGS_NOT_MY_BUFFER;

		return blo_decode_and_check(fd, reports);
	}
}

FileData *blo_openblendermemfile(MemFile *memfile, ReportList *reports)
{
	if (!memfile) {
		BKE_report(reports, RPT_WARNING, "Unable to open blend <memory>");
		return NULL;
	}
	else {
		FileData *fd = filedata_new();
		fd->memfile = memfile;
		
		fd->read = fd_read_from_memfile;
		fd->flags |= FD_FLAGS_NOT_MY_BUFFER;
		
		return blo_decode_and_check(fd, reports);
	}
}


void blo_freefiledata(FileData *fd)
{
	if (fd) {
		if (fd->filedes != -1) {
			close(fd->filedes);
		}
		
		if (fd->gzfiledes != NULL) {
			gzclose(fd->gzfiledes);
		}
		
		if (fd->strm.next_in) {
			if (inflateEnd(&fd->strm) != Z_OK) {
				printf("close gzip stream error\n");
			}
		}
		
		if (fd->buffer && !(fd->flags & FD_FLAGS_NOT_MY_BUFFER)) {
			MEM_freeN((void *)fd->buffer);
			fd->buffer = NULL;
		}
		
		// Free all BHeadN data blocks
		BLI_freelistN(&fd->listbase);

		if (fd->filesdna)
			DNA_sdna_free(fd->filesdna);
		if (fd->compflags)
			MEM_freeN((void *)fd->compflags);
		
		if (fd->datamap)
			oldnewmap_free(fd->datamap);
		if (fd->globmap)
			oldnewmap_free(fd->globmap);
		if (fd->imamap)
			oldnewmap_free(fd->imamap);
		if (fd->movieclipmap)
			oldnewmap_free(fd->movieclipmap);
		if (fd->soundmap)
			oldnewmap_free(fd->soundmap);
		if (fd->packedmap)
			oldnewmap_free(fd->packedmap);
		if (fd->libmap && !(fd->flags & FD_FLAGS_NOT_MY_LIBMAP))
			oldnewmap_free(fd->libmap);
		if (fd->bheadmap)
			MEM_freeN(fd->bheadmap);
		
#ifdef USE_GHASH_BHEAD
		if (fd->bhead_idname_hash) {
			BLI_ghash_free(fd->bhead_idname_hash, NULL, NULL);
		}
#endif

		MEM_freeN(fd);
	}
}

/* ************ DIV ****************** */

/**
 * Check whether given path ends with a blend file compatible extension (.blend, .ble or .blend.gz).
 *
 * \param str The path to check.
 * \return true is this path ends with a blender file extension.
 */
bool BLO_has_bfile_extension(const char *str)
{
	const char *ext_test[4] = {".blend", ".ble", ".blend.gz", NULL};
	return BLI_testextensie_array(str, ext_test);
}

/**
 * Try to explode given path into its 'library components' (i.e. a .blend file, id type/group, and datablock itself).
 *
 * \param path the full path to explode.
 * \param r_dir the string that'll contain path up to blend file itself ('library' path).
 *              WARNING! Must be FILE_MAX_LIBEXTRA long (it also stores group and name strings)!
 * \param r_group the string that'll contain 'group' part of the path, if any. May be NULL.
 * \param r_name the string that'll contain data's name part of the path, if any. May be NULL.
 * \return true if path contains a blend file.
 */
bool BLO_library_path_explode(const char *path, char *r_dir, char **r_group, char **r_name)
{
	/* We might get some data names with slashes, so we have to go up in path until we find blend file itself,
	 * then we now next path item is group, and everything else is data name. */
	char *slash = NULL, *prev_slash = NULL, c = '\0';

	r_dir[0] = '\0';
	if (r_group) {
		*r_group = NULL;
	}
	if (r_name) {
		*r_name = NULL;
	}

	/* if path leads to an existing directory, we can be sure we're not (in) a library */
	if (BLI_is_dir(path)) {
		return false;
	}

	strcpy(r_dir, path);

	while ((slash = (char *)BLI_last_slash(r_dir))) {
		char tc = *slash;
		*slash = '\0';
		if (BLO_has_bfile_extension(r_dir) && BLI_is_file(r_dir)) {
			break;
		}

		if (prev_slash) {
			*prev_slash = c;
		}
		prev_slash = slash;
		c = tc;
	}

	if (!slash) {
		return false;
	}

	if (slash[1] != '\0') {
		BLI_assert(strlen(slash + 1) < BLO_GROUP_MAX);
		if (r_group) {
			*r_group = slash + 1;
		}
	}

	if (prev_slash && (prev_slash[1] != '\0')) {
		BLI_assert(strlen(prev_slash + 1) < MAX_ID_NAME - 2);
		if (r_name) {
			*r_name = prev_slash + 1;
		}
	}

	return true;
}

/**
 * Does a very light reading of given .blend file to extract its stored thumbnail.
 *
 * \param filepath The path of the file to extract thumbnail from.
 * \return The raw thumbnail
 *         (MEM-allocated, as stored in file, use BKE_main_thumbnail_to_imbuf() to convert it to ImBuf image).
 */
BlendThumbnail *BLO_thumbnail_from_file(const char *filepath)
{
	FileData *fd;
	BlendThumbnail *data = NULL;
	int *fd_data;

	fd = blo_openblenderfile_minimal(filepath);
	fd_data = fd ? read_file_thumbnail(fd) : NULL;

	if (fd_data) {
		int width = fd_data[0];
		int height = fd_data[1];

		/* Protect against buffer overflow vulnerability. */
		if (BLEN_THUMB_SAFE_MEMSIZE(width, height)) {
			const size_t sz = BLEN_THUMB_MEMSIZE(width, height);
			data = MEM_mallocN(sz, __func__);

			if (data) {
				BLI_assert((sz - sizeof(*data)) == (BLEN_THUMB_MEMSIZE_FILE(width, height) - (sizeof(*fd_data) * 2)));
				data->width = width;
				data->height = height;
				memcpy(data->rect, &fd_data[2], sz - sizeof(*data));
			}
		}
	}

	blo_freefiledata(fd);

	return data;
}

/* ************** OLD POINTERS ******************* */

static void *newdataadr(FileData *fd, const void *adr)		/* only direct databocks */
{
	return oldnewmap_lookup_and_inc(fd->datamap, adr, true);
}

/* This is a special version of newdataadr() which allows us to keep lasthit of
 * map unchanged. In certain cases this makes file loading time significantly
 * faster.
 *
 * Use this function in cases like restoring pointer from one list element to
 * another list element, but keep lasthit value so we can continue restoring
 * pointers efficiently.
 *
 * Example of this could be found in direct_link_fcurves() which restores the
 * fcurve group pointer and keeps lasthit optimal for linking all further
 * fcurves.
 */
static void *newdataadr_ex(FileData *fd, const void *adr, bool increase_lasthit)		/* only direct databocks */
{
	if (increase_lasthit) {
		return newdataadr(fd, adr);
	}
	else {
		int lasthit = fd->datamap->lasthit;
		void *newadr = newdataadr(fd, adr);
		fd->datamap->lasthit = lasthit;
		return newadr;
	}
}

static void *newdataadr_no_us(FileData *fd, const void *adr)		/* only direct databocks */
{
	return oldnewmap_lookup_and_inc(fd->datamap, adr, false);
}

static void *newglobadr(FileData *fd, const void *adr)	    /* direct datablocks with global linking */
{
	return oldnewmap_lookup_and_inc(fd->globmap, adr, true);
}

static void *newimaadr(FileData *fd, const void *adr)		    /* used to restore image data after undo */
{
	if (fd->imamap && adr)
		return oldnewmap_lookup_and_inc(fd->imamap, adr, true);
	return NULL;
}

static void *newmclipadr(FileData *fd, const void *adr)      /* used to restore movie clip data after undo */
{
	if (fd->movieclipmap && adr)
		return oldnewmap_lookup_and_inc(fd->movieclipmap, adr, true);
	return NULL;
}

static void *newsoundadr(FileData *fd, const void *adr)      /* used to restore sound data after undo */
{
	if (fd->soundmap && adr)
		return oldnewmap_lookup_and_inc(fd->soundmap, adr, true);
	return NULL;
}

static void *newpackedadr(FileData *fd, const void *adr)      /* used to restore packed data after undo */
{
	if (fd->packedmap && adr)
		return oldnewmap_lookup_and_inc(fd->packedmap, adr, true);
	
	return oldnewmap_lookup_and_inc(fd->datamap, adr, true);
}


static void *newlibadr(FileData *fd, const void *lib, const void *adr)		/* only lib data */
{
	return oldnewmap_liblookup(fd->libmap, adr, lib);
}

void *blo_do_versions_newlibadr(FileData *fd, const void *lib, const void *adr)		/* only lib data */
{
	return newlibadr(fd, lib, adr);
}

static void *newlibadr_us(FileData *fd, const void *lib, const void *adr)	/* increases user number */
{
	ID *id = newlibadr(fd, lib, adr);
	
	id_us_plus_no_lib(id);
	
	return id;
}

void *blo_do_versions_newlibadr_us(FileData *fd, const void *lib, const void *adr)	/* increases user number */
{
	return newlibadr_us(fd, lib, adr);
}

static void *newlibadr_real_us(FileData *fd, const void *lib, const void *adr)	/* ensures real user */
{
	ID *id = newlibadr(fd, lib, adr);

	id_us_ensure_real(id);

	return id;
}

static void change_idid_adr_fd(FileData *fd, const void *old, void *new)
{
	int i;
	
	/* use a binary search if we have a sorted libmap, for now it's not needed. */
	BLI_assert(fd->libmap->sorted == false);

	for (i = 0; i < fd->libmap->nentries; i++) {
		OldNew *entry = &fd->libmap->entries[i];
		
		if (old==entry->newp && entry->nr==ID_ID) {
			entry->newp = new;
			if (new) entry->nr = GS( ((ID *)new)->name );
		}
	}
}

static void change_idid_adr(ListBase *mainlist, FileData *basefd, void *old, void *new)
{
	Main *mainptr;
	
	for (mainptr = mainlist->first; mainptr; mainptr = mainptr->next) {
		FileData *fd;
		
		if (mainptr->curlib)
			fd = mainptr->curlib->filedata;
		else
			fd = basefd;
		
		if (fd) {
			change_idid_adr_fd(fd, old, new);
		}
	}
}

/* lib linked proxy objects point to our local data, we need
 * to clear that pointer before reading the undo memfile since
 * the object might be removed, it is set again in reading
 * if the local object still exists */
void blo_clear_proxy_pointers_from_lib(Main *oldmain)
{
	Object *ob = oldmain->object.first;
	
	for (; ob; ob= ob->id.next) {
		if (ob->id.lib)
			ob->proxy_from = NULL;
	}
}

void blo_make_image_pointer_map(FileData *fd, Main *oldmain)
{
	Image *ima = oldmain->image.first;
	Scene *sce = oldmain->scene.first;
	int a;
	
	fd->imamap = oldnewmap_new();
	
	for (; ima; ima = ima->id.next) {
		if (ima->cache)
			oldnewmap_insert(fd->imamap, ima->cache, ima->cache, 0);
		for (a = 0; a < TEXTARGET_COUNT; a++)
			if (ima->gputexture[a])
				oldnewmap_insert(fd->imamap, ima->gputexture[a], ima->gputexture[a], 0);
		if (ima->rr)
			oldnewmap_insert(fd->imamap, ima->rr, ima->rr, 0);
		for (a=0; a < IMA_MAX_RENDER_SLOT; a++)
			if (ima->renders[a])
				oldnewmap_insert(fd->imamap, ima->renders[a], ima->renders[a], 0);
	}
	for (; sce; sce = sce->id.next) {
		if (sce->nodetree && sce->nodetree->previews) {
			bNodeInstanceHashIterator iter;
			NODE_INSTANCE_HASH_ITER(iter, sce->nodetree->previews) {
				bNodePreview *preview = BKE_node_instance_hash_iterator_get_value(&iter);
				oldnewmap_insert(fd->imamap, preview, preview, 0);
			}
		}
	}
}

/* set old main image ibufs to zero if it has been restored */
/* this works because freeing old main only happens after this call */
void blo_end_image_pointer_map(FileData *fd, Main *oldmain)
{
	OldNew *entry = fd->imamap->entries;
	Image *ima = oldmain->image.first;
	Scene *sce = oldmain->scene.first;
	int i;
	
	/* used entries were restored, so we put them to zero */
	for (i = 0; i < fd->imamap->nentries; i++, entry++) {
		if (entry->nr > 0)
			entry->newp = NULL;
	}
	
	for (; ima; ima = ima->id.next) {
		ima->cache = newimaadr(fd, ima->cache);
		if (ima->cache == NULL) {
			ima->tpageflag &= ~IMA_GLBIND_IS_DATA;
			for (i = 0; i < TEXTARGET_COUNT; i++) {
				ima->bindcode[i] = 0;
				ima->gputexture[i] = NULL;
			}
			ima->rr = NULL;
		}
		for (i = 0; i < IMA_MAX_RENDER_SLOT; i++)
			ima->renders[i] = newimaadr(fd, ima->renders[i]);
		
		for (i = 0; i < TEXTARGET_COUNT; i++)
			ima->gputexture[i] = newimaadr(fd, ima->gputexture[i]);
		ima->rr = newimaadr(fd, ima->rr);
	}
	for (; sce; sce = sce->id.next) {
		if (sce->nodetree && sce->nodetree->previews) {
			bNodeInstanceHash *new_previews = BKE_node_instance_hash_new("node previews");
			bNodeInstanceHashIterator iter;
			
			/* reconstruct the preview hash, only using remaining pointers */
			NODE_INSTANCE_HASH_ITER(iter, sce->nodetree->previews) {
				bNodePreview *preview = BKE_node_instance_hash_iterator_get_value(&iter);
				if (preview) {
					bNodePreview *new_preview = newimaadr(fd, preview);
					if (new_preview) {
						bNodeInstanceKey key = BKE_node_instance_hash_iterator_get_key(&iter);
						BKE_node_instance_hash_insert(new_previews, key, new_preview);
					}
				}
			}
			BKE_node_instance_hash_free(sce->nodetree->previews, NULL);
			sce->nodetree->previews = new_previews;
		}
	}
}

void blo_make_movieclip_pointer_map(FileData *fd, Main *oldmain)
{
	MovieClip *clip = oldmain->movieclip.first;
	Scene *sce = oldmain->scene.first;
	
	fd->movieclipmap = oldnewmap_new();
	
	for (; clip; clip = clip->id.next) {
		if (clip->cache)
			oldnewmap_insert(fd->movieclipmap, clip->cache, clip->cache, 0);
		
		if (clip->tracking.camera.intrinsics)
			oldnewmap_insert(fd->movieclipmap, clip->tracking.camera.intrinsics, clip->tracking.camera.intrinsics, 0);
	}
	
	for (; sce; sce = sce->id.next) {
		if (sce->nodetree) {
			bNode *node;
			for (node = sce->nodetree->nodes.first; node; node = node->next)
				if (node->type == CMP_NODE_MOVIEDISTORTION)
					oldnewmap_insert(fd->movieclipmap, node->storage, node->storage, 0);
		}
	}
}

/* set old main movie clips caches to zero if it has been restored */
/* this works because freeing old main only happens after this call */
void blo_end_movieclip_pointer_map(FileData *fd, Main *oldmain)
{
	OldNew *entry = fd->movieclipmap->entries;
	MovieClip *clip = oldmain->movieclip.first;
	Scene *sce = oldmain->scene.first;
	int i;
	
	/* used entries were restored, so we put them to zero */
	for (i=0; i < fd->movieclipmap->nentries; i++, entry++) {
		if (entry->nr > 0)
			entry->newp = NULL;
	}
	
	for (; clip; clip = clip->id.next) {
		clip->cache = newmclipadr(fd, clip->cache);
		clip->tracking.camera.intrinsics = newmclipadr(fd, clip->tracking.camera.intrinsics);
	}
	
	for (; sce; sce = sce->id.next) {
		if (sce->nodetree) {
			bNode *node;
			for (node = sce->nodetree->nodes.first; node; node = node->next)
				if (node->type == CMP_NODE_MOVIEDISTORTION)
					node->storage = newmclipadr(fd, node->storage);
		}
	}
}

void blo_make_sound_pointer_map(FileData *fd, Main *oldmain)
{
	bSound *sound = oldmain->sound.first;
	
	fd->soundmap = oldnewmap_new();
	
	for (; sound; sound = sound->id.next) {
		if (sound->waveform)
			oldnewmap_insert(fd->soundmap, sound->waveform, sound->waveform, 0);			
	}
}

/* set old main sound caches to zero if it has been restored */
/* this works because freeing old main only happens after this call */
void blo_end_sound_pointer_map(FileData *fd, Main *oldmain)
{
	OldNew *entry = fd->soundmap->entries;
	bSound *sound = oldmain->sound.first;
	int i;
	
	/* used entries were restored, so we put them to zero */
	for (i = 0; i < fd->soundmap->nentries; i++, entry++) {
		if (entry->nr > 0)
			entry->newp = NULL;
	}
	
	for (; sound; sound = sound->id.next) {
		sound->waveform = newsoundadr(fd, sound->waveform);
	}
}

/* XXX disabled this feature - packed files also belong in temp saves and quit.blend, to make restore work */

static void insert_packedmap(FileData *fd, PackedFile *pf)
{
	oldnewmap_insert(fd->packedmap, pf, pf, 0);
	oldnewmap_insert(fd->packedmap, pf->data, pf->data, 0);
}

void blo_make_packed_pointer_map(FileData *fd, Main *oldmain)
{
	Image *ima;
	VFont *vfont;
	bSound *sound;
	Library *lib;
	
	fd->packedmap = oldnewmap_new();
	
	for (ima = oldmain->image.first; ima; ima = ima->id.next) {
		ImagePackedFile *imapf;

		if (ima->packedfile)
			insert_packedmap(fd, ima->packedfile);

		for (imapf = ima->packedfiles.first; imapf; imapf = imapf->next)
			if (imapf->packedfile)
				insert_packedmap(fd, imapf->packedfile);
	}
			
	for (vfont = oldmain->vfont.first; vfont; vfont = vfont->id.next)
		if (vfont->packedfile)
			insert_packedmap(fd, vfont->packedfile);
	
	for (sound = oldmain->sound.first; sound; sound = sound->id.next)
		if (sound->packedfile)
			insert_packedmap(fd, sound->packedfile);
	
	for (lib = oldmain->library.first; lib; lib = lib->id.next)
		if (lib->packedfile)
			insert_packedmap(fd, lib->packedfile);

}

/* set old main packed data to zero if it has been restored */
/* this works because freeing old main only happens after this call */
void blo_end_packed_pointer_map(FileData *fd, Main *oldmain)
{
	Image *ima;
	VFont *vfont;
	bSound *sound;
	Library *lib;
	OldNew *entry = fd->packedmap->entries;
	int i;
	
	/* used entries were restored, so we put them to zero */
	for (i=0; i < fd->packedmap->nentries; i++, entry++) {
		if (entry->nr > 0)
			entry->newp = NULL;
	}
	
	for (ima = oldmain->image.first; ima; ima = ima->id.next) {
		ImagePackedFile *imapf;

		ima->packedfile = newpackedadr(fd, ima->packedfile);

		for (imapf = ima->packedfiles.first; imapf; imapf = imapf->next)
			imapf->packedfile = newpackedadr(fd, imapf->packedfile);
	}
	
	for (vfont = oldmain->vfont.first; vfont; vfont = vfont->id.next)
		vfont->packedfile = newpackedadr(fd, vfont->packedfile);

	for (sound = oldmain->sound.first; sound; sound = sound->id.next)
		sound->packedfile = newpackedadr(fd, sound->packedfile);
		
	for (lib = oldmain->library.first; lib; lib = lib->id.next)
		lib->packedfile = newpackedadr(fd, lib->packedfile);
}


/* undo file support: add all library pointers in lookup */
void blo_add_library_pointer_map(ListBase *old_mainlist, FileData *fd)
{
	Main *ptr = old_mainlist->first;
	ListBase *lbarray[MAX_LIBARRAY];
	
	for (ptr = ptr->next; ptr; ptr = ptr->next) {
		int i = set_listbasepointers(ptr, lbarray);
		while (i--) {
			ID *id;
			for (id = lbarray[i]->first; id; id = id->next)
				oldnewmap_insert(fd->libmap, id, id, GS(id->name));
		}
	}

	fd->old_mainlist = old_mainlist;
}


/* ********** END OLD POINTERS ****************** */
/* ********** READ FILE ****************** */

static void switch_endian_structs(const struct SDNA *filesdna, BHead *bhead)
{
	int blocksize, nblocks;
	char *data;
	
	data = (char *)(bhead+1);
	blocksize = filesdna->typelens[ filesdna->structs[bhead->SDNAnr][0] ];
	
	nblocks = bhead->nr;
	while (nblocks--) {
		DNA_struct_switch_endian(filesdna, bhead->SDNAnr, data);
		
		data += blocksize;
	}
}

static void *read_struct(FileData *fd, BHead *bh, const char *blockname)
{
	void *temp = NULL;
	
	if (bh->len) {
		/* switch is based on file dna */
		if (bh->SDNAnr && (fd->flags & FD_FLAGS_SWITCH_ENDIAN))
			switch_endian_structs(fd->filesdna, bh);
		
		if (fd->compflags[bh->SDNAnr] != SDNA_CMP_REMOVED) {
			if (fd->compflags[bh->SDNAnr] == SDNA_CMP_NOT_EQUAL) {
				temp = DNA_struct_reconstruct(fd->memsdna, fd->filesdna, fd->compflags, bh->SDNAnr, bh->nr, (bh+1));
			}
			else {
				/* SDNA_CMP_EQUAL */
				temp = MEM_mallocN(bh->len, blockname);
				memcpy(temp, (bh+1), bh->len);
			}
		}
	}

	return temp;
}

typedef void (*link_list_cb)(FileData *fd, void *data);

static void link_list_ex(FileData *fd, ListBase *lb, link_list_cb callback)		/* only direct data */
{
	Link *ln, *prev;
	
	if (BLI_listbase_is_empty(lb)) return;
	
	lb->first = newdataadr(fd, lb->first);
	if (callback != NULL) {
		callback(fd, lb->first);
	}
	ln = lb->first;
	prev = NULL;
	while (ln) {
		ln->next = newdataadr(fd, ln->next);
		if (ln->next != NULL && callback != NULL) {
			callback(fd, ln->next);
		}
		ln->prev = prev;
		prev = ln;
		ln = ln->next;
	}
	lb->last = prev;
}

static void link_list(FileData *fd, ListBase *lb)		/* only direct data */
{
	link_list_ex(fd, lb, NULL);
}

static void link_glob_list(FileData *fd, ListBase *lb)		/* for glob data */
{
	Link *ln, *prev;
	void *poin;

	if (BLI_listbase_is_empty(lb)) return;
	poin = newdataadr(fd, lb->first);
	if (lb->first) {
		oldnewmap_insert(fd->globmap, lb->first, poin, 0);
	}
	lb->first = poin;
	
	ln = lb->first;
	prev = NULL;
	while (ln) {
		poin = newdataadr(fd, ln->next);
		if (ln->next) {
			oldnewmap_insert(fd->globmap, ln->next, poin, 0);
		}
		ln->next = poin;
		ln->prev = prev;
		prev = ln;
		ln = ln->next;
	}
	lb->last = prev;
}

static void test_pointer_array(FileData *fd, void **mat)
{
	int64_t *lpoin, *lmat;
	int *ipoin, *imat;
	size_t len;

		/* manually convert the pointer array in
		 * the old dna format to a pointer array in
		 * the new dna format.
		 */
	if (*mat) {
		len = MEM_allocN_len(*mat)/fd->filesdna->pointerlen;
			
		if (fd->filesdna->pointerlen==8 && fd->memsdna->pointerlen==4) {
			ipoin=imat= MEM_malloc_arrayN(len, 4, "newmatar");
			lpoin= *mat;
			
			while (len-- > 0) {
				if ((fd->flags & FD_FLAGS_SWITCH_ENDIAN))
					BLI_endian_switch_int64(lpoin);
				*ipoin = (int)((*lpoin) >> 3);
				ipoin++;
				lpoin++;
			}
			MEM_freeN(*mat);
			*mat = imat;
		}
		
		if (fd->filesdna->pointerlen==4 && fd->memsdna->pointerlen==8) {
			lpoin = lmat = MEM_malloc_arrayN(len, 8, "newmatar");
			ipoin = *mat;
			
			while (len-- > 0) {
				*lpoin = *ipoin;
				ipoin++;
				lpoin++;
			}
			MEM_freeN(*mat);
			*mat= lmat;
		}
	}
}

/* ************ READ ID Properties *************** */

static void IDP_DirectLinkProperty(IDProperty *prop, int switch_endian, FileData *fd);
static void IDP_LibLinkProperty(IDProperty *prop, FileData *fd);

static void IDP_DirectLinkIDPArray(IDProperty *prop, int switch_endian, FileData *fd)
{
	IDProperty *array;
	int i;
	
	/* since we didn't save the extra buffer, set totallen to len */
	prop->totallen = prop->len;
	prop->data.pointer = newdataadr(fd, prop->data.pointer);

	array = (IDProperty *)prop->data.pointer;
	
	/* note!, idp-arrays didn't exist in 2.4x, so the pointer will be cleared
	 * theres not really anything we can do to correct this, at least don't crash */
	if (array == NULL) {
		prop->len = 0;
		prop->totallen = 0;
	}
	
	
	for (i = 0; i < prop->len; i++)
		IDP_DirectLinkProperty(&array[i], switch_endian, fd);
}

static void IDP_DirectLinkArray(IDProperty *prop, int switch_endian, FileData *fd)
{
	IDProperty **array;
	int i;
	
	/* since we didn't save the extra buffer, set totallen to len */
	prop->totallen = prop->len;
	prop->data.pointer = newdataadr(fd, prop->data.pointer);
	
	if (prop->subtype == IDP_GROUP) {
		test_pointer_array(fd, prop->data.pointer);
		array = prop->data.pointer;
		
		for (i = 0; i < prop->len; i++)
			IDP_DirectLinkProperty(array[i], switch_endian, fd);
	}
	else if (prop->subtype == IDP_DOUBLE) {
		if (switch_endian) {
			BLI_endian_switch_double_array(prop->data.pointer, prop->len);
		}
	}
	else {
		if (switch_endian) {
			/* also used for floats */
			BLI_endian_switch_int32_array(prop->data.pointer, prop->len);
		}
	}
}

static void IDP_DirectLinkString(IDProperty *prop, FileData *fd)
{
	/*since we didn't save the extra string buffer, set totallen to len.*/
	prop->totallen = prop->len;
	prop->data.pointer = newdataadr(fd, prop->data.pointer);
}

static void IDP_DirectLinkGroup(IDProperty *prop, int switch_endian, FileData *fd)
{
	ListBase *lb = &prop->data.group;
	IDProperty *loop;
	
	link_list(fd, lb);
	
	/*Link child id properties now*/
	for (loop=prop->data.group.first; loop; loop=loop->next) {
		IDP_DirectLinkProperty(loop, switch_endian, fd);
	}
}

static void IDP_DirectLinkProperty(IDProperty *prop, int switch_endian, FileData *fd)
{
	switch (prop->type) {
		case IDP_GROUP:
			IDP_DirectLinkGroup(prop, switch_endian, fd);
			break;
		case IDP_STRING:
			IDP_DirectLinkString(prop, fd);
			break;
		case IDP_ARRAY:
			IDP_DirectLinkArray(prop, switch_endian, fd);
			break;
		case IDP_IDPARRAY:
			IDP_DirectLinkIDPArray(prop, switch_endian, fd);
			break;
		case IDP_DOUBLE:
			/* erg, stupid doubles.  since I'm storing them
			 * in the same field as int val; val2 in the
			 * IDPropertyData struct, they have to deal with
			 * endianness specifically
			 *
			 * in theory, val and val2 would've already been swapped
			 * if switch_endian is true, so we have to first unswap
			 * them then reswap them as a single 64-bit entity.
			 */
			
			if (switch_endian) {
				BLI_endian_switch_int32(&prop->data.val);
				BLI_endian_switch_int32(&prop->data.val2);
				BLI_endian_switch_int64((int64_t *)&prop->data.val);
			}
			break;
		case IDP_INT:
		case IDP_FLOAT:
		case IDP_ID:
			break;  /* Nothing special to do here. */
		default:
			/* Unknown IDP type, nuke it (we cannot handle unknown types everywhere in code,
			 * IDP are way too polymorphic to do it safely. */
			printf("%s: found unknown IDProperty type %d, reset to Integer one !\n", __func__, prop->type);
			/* Note: we do not attempt to free unknown prop, we have no way to know how to do that! */
			prop->type = IDP_INT;
			prop->subtype = 0;
			IDP_Int(prop) = 0;
	}
}

#define IDP_DirectLinkGroup_OrFree(prop, switch_endian, fd) \
       _IDP_DirectLinkGroup_OrFree(prop, switch_endian, fd, __func__)

static void _IDP_DirectLinkGroup_OrFree(IDProperty **prop, int switch_endian, FileData *fd,
                                        const char *caller_func_id)
{
	if (*prop) {
		if ((*prop)->type == IDP_GROUP) {
			IDP_DirectLinkGroup(*prop, switch_endian, fd);
		}
		else {
			/* corrupt file! */
			printf("%s: found non group data, freeing type %d!\n",
			       caller_func_id, (*prop)->type);
			/* don't risk id, data's likely corrupt. */
			// IDP_FreeProperty(*prop);
			*prop = NULL;
		}
	}
}

static void IDP_LibLinkProperty(IDProperty *prop, FileData *fd)
{
	if (!prop)
		return;

	switch (prop->type) {
		case IDP_ID: /* PointerProperty */
		{
			void *newaddr = newlibadr_us(fd, NULL, IDP_Id(prop));
			if (IDP_Id(prop) && !newaddr && G.debug) {
				printf("Error while loading \"%s\". Data not found in file!\n", prop->name);
			}
			prop->data.pointer = newaddr;
			break;
		}
		case IDP_IDPARRAY: /* CollectionProperty */
		{
			IDProperty *idp_array = IDP_IDPArray(prop);
			for (int i = 0; i < prop->len; i++) {
				IDP_LibLinkProperty(&(idp_array[i]), fd);
			}
			break;
		}
		case IDP_GROUP: /* PointerProperty */
		{
			for (IDProperty *loop = prop->data.group.first; loop; loop = loop->next) {
				IDP_LibLinkProperty(loop, fd);
			}
			break;
		}
		default:
			break;  /* Nothing to do for other IDProps. */
	}
}

/* ************ READ IMAGE PREVIEW *************** */

static PreviewImage *direct_link_preview_image(FileData *fd, PreviewImage *old_prv)
{
	PreviewImage *prv = newdataadr(fd, old_prv);
	
	if (prv) {
		int i;
		for (i = 0; i < NUM_ICON_SIZES; ++i) {
			if (prv->rect[i]) {
				prv->rect[i] = newdataadr(fd, prv->rect[i]);
			}
			prv->gputexture[i] = NULL;
		}
		prv->icon_id = 0;
		prv->tag = 0;
	}
	
	return prv;
}

/* ************ READ ID *************** */

static void direct_link_id(FileData *fd, ID *id)
{
	/*link direct data of ID properties*/
	if (id->properties) {
		id->properties = newdataadr(fd, id->properties);
		/* this case means the data was written incorrectly, it should not happen */
		IDP_DirectLinkGroup_OrFree(&id->properties, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
	}
}

/* ************ READ CurveMapping *************** */

/* cuma itself has been read! */
static void direct_link_curvemapping(FileData *fd, CurveMapping *cumap)
{
	int a;
	
	/* flag seems to be able to hang? Maybe old files... not bad to clear anyway */
	cumap->flag &= ~CUMA_PREMULLED;
	
	for (a = 0; a < CM_TOT; a++) {
		cumap->cm[a].curve = newdataadr(fd, cumap->cm[a].curve);
		cumap->cm[a].table = NULL;
		cumap->cm[a].premultable = NULL;
	}
}

/* ************ READ Brush *************** */
/* library brush linking after fileread */
static void lib_link_brush(FileData *fd, Main *main)
{
	/* only link ID pointers */
	for (Brush *brush = main->brush.first; brush; brush = brush->id.next) {
		if (brush->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(brush->id.properties, fd);

			/* brush->(mask_)mtex.obj is ignored on purpose? */
			brush->mtex.tex = newlibadr_us(fd, brush->id.lib, brush->mtex.tex);
			brush->mask_mtex.tex = newlibadr_us(fd, brush->id.lib, brush->mask_mtex.tex);
			brush->clone.image = newlibadr(fd, brush->id.lib, brush->clone.image);
			brush->toggle_brush = newlibadr(fd, brush->id.lib, brush->toggle_brush);
			brush->paint_curve = newlibadr_us(fd, brush->id.lib, brush->paint_curve);

			brush->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_brush(FileData *fd, Brush *brush)
{
	/* brush itself has been read */

	/* fallof curve */
	brush->curve = newdataadr(fd, brush->curve);
	brush->gradient = newdataadr(fd, brush->gradient);

	if (brush->curve)
		direct_link_curvemapping(fd, brush->curve);
	else
		BKE_brush_curve_preset(brush, CURVE_PRESET_SHARP);

	brush->preview = NULL;
	brush->icon_imbuf = NULL;
}

/* ************ READ Palette *************** */
static void lib_link_palette(FileData *fd, Main *main)
{
	/* only link ID pointers */
	for (Palette *palette = main->palettes.first; palette; palette = palette->id.next) {
		if (palette->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(palette->id.properties, fd);

			palette->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_palette(FileData *fd, Palette *palette)
{
	/* palette itself has been read */
	link_list(fd, &palette->colors);
}

static void lib_link_paint_curve(FileData *fd, Main *main)
{
	/* only link ID pointers */
	for (PaintCurve *pc = main->paintcurves.first; pc; pc = pc->id.next) {
		if (pc->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(pc->id.properties, fd);

			pc->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_paint_curve(FileData *fd, PaintCurve *pc)
{
	pc->points = newdataadr(fd, pc->points);
}

/* ************ READ PACKEDFILE *************** */

static PackedFile *direct_link_packedfile(FileData *fd, PackedFile *oldpf)
{
	PackedFile *pf = newpackedadr(fd, oldpf);

	if (pf) {
		pf->data = newpackedadr(fd, pf->data);
	}
	
	return pf;
}

/* ************ READ ANIMATION STUFF ***************** */

/* Legacy Data Support (for Version Patching) ----------------------------- */

// XXX deprecated - old animation system
static void lib_link_ipo(FileData *fd, Main *main)
{
	Ipo *ipo;
	
	for (ipo = main->ipo.first; ipo; ipo = ipo->id.next) {
		if (ipo->id.tag & LIB_TAG_NEED_LINK) {
			IpoCurve *icu;
			for (icu = ipo->curve.first; icu; icu = icu->next) {
				if (icu->driver)
					icu->driver->ob = newlibadr(fd, ipo->id.lib, icu->driver->ob);
			}
			ipo->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

// XXX deprecated - old animation system
static void direct_link_ipo(FileData *fd, Ipo *ipo)
{
	IpoCurve *icu;

	link_list(fd, &(ipo->curve));
	
	for (icu = ipo->curve.first; icu; icu = icu->next) {
		icu->bezt = newdataadr(fd, icu->bezt);
		icu->bp = newdataadr(fd, icu->bp);
		icu->driver = newdataadr(fd, icu->driver);
	}
}

// XXX deprecated - old animation system
static void lib_link_nlastrips(FileData *fd, ID *id, ListBase *striplist)
{
	bActionStrip *strip;
	bActionModifier *amod;
	
	for (strip=striplist->first; strip; strip=strip->next) {
		strip->object = newlibadr(fd, id->lib, strip->object);
		strip->act = newlibadr_us(fd, id->lib, strip->act);
		strip->ipo = newlibadr(fd, id->lib, strip->ipo);
		for (amod = strip->modifiers.first; amod; amod = amod->next)
			amod->ob = newlibadr(fd, id->lib, amod->ob);
	}
}

// XXX deprecated - old animation system
static void direct_link_nlastrips(FileData *fd, ListBase *strips)
{
	bActionStrip *strip;
	
	link_list(fd, strips);
	
	for (strip = strips->first; strip; strip = strip->next)
		link_list(fd, &strip->modifiers);
}

// XXX deprecated - old animation system
static void lib_link_constraint_channels(FileData *fd, ID *id, ListBase *chanbase)
{
	bConstraintChannel *chan;

	for (chan=chanbase->first; chan; chan=chan->next) {
		chan->ipo = newlibadr_us(fd, id->lib, chan->ipo);
	}
}

/* Data Linking ----------------------------- */

static void lib_link_fmodifiers(FileData *fd, ID *id, ListBase *list)
{
	FModifier *fcm;
	
	for (fcm = list->first; fcm; fcm = fcm->next) {
		/* data for specific modifiers */
		switch (fcm->type) {
			case FMODIFIER_TYPE_PYTHON:
			{
				FMod_Python *data = (FMod_Python *)fcm->data;
				data->script = newlibadr(fd, id->lib, data->script);

				break;
			}
		}
	}
}

static void lib_link_fcurves(FileData *fd, ID *id, ListBase *list) 
{
	FCurve *fcu;
	
	if (list == NULL)
		return;
	
	/* relink ID-block references... */
	for (fcu = list->first; fcu; fcu = fcu->next) {
		/* driver data */
		if (fcu->driver) {
			ChannelDriver *driver = fcu->driver;
			DriverVar *dvar;
			
			for (dvar= driver->variables.first; dvar; dvar= dvar->next) {
				DRIVER_TARGETS_LOOPER(dvar)
				{
					/* only relink if still used */
					if (tarIndex < dvar->num_targets)
						dtar->id = newlibadr(fd, id->lib, dtar->id); 
					else
						dtar->id = NULL;
				}
				DRIVER_TARGETS_LOOPER_END
			}
		}
		
		/* modifiers */
		lib_link_fmodifiers(fd, id, &fcu->modifiers);
	}
}


/* NOTE: this assumes that link_list has already been called on the list */
static void direct_link_fmodifiers(FileData *fd, ListBase *list)
{
	FModifier *fcm;
	
	for (fcm = list->first; fcm; fcm = fcm->next) {
		/* relink general data */
		fcm->data  = newdataadr(fd, fcm->data);
		
		/* do relinking of data for specific types */
		switch (fcm->type) {
			case FMODIFIER_TYPE_GENERATOR:
			{
				FMod_Generator *data = (FMod_Generator *)fcm->data;
				
				data->coefficients = newdataadr(fd, data->coefficients);
				
				if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
					BLI_endian_switch_float_array(data->coefficients, data->arraysize);
				}

				break;
			}
			case FMODIFIER_TYPE_ENVELOPE:
			{
				FMod_Envelope *data=  (FMod_Envelope *)fcm->data;
				
				data->data= newdataadr(fd, data->data);

				break;
			}
			case FMODIFIER_TYPE_PYTHON:
			{
				FMod_Python *data = (FMod_Python *)fcm->data;
				
				data->prop = newdataadr(fd, data->prop);
				IDP_DirectLinkGroup_OrFree(&data->prop, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);

				break;
			}
		}
	}
}

/* NOTE: this assumes that link_list has already been called on the list */
static void direct_link_fcurves(FileData *fd, ListBase *list)
{
	FCurve *fcu;
	
	/* link F-Curve data to F-Curve again (non ID-libs) */
	for (fcu = list->first; fcu; fcu = fcu->next) {
		/* curve data */
		fcu->bezt = newdataadr(fd, fcu->bezt);
		fcu->fpt = newdataadr(fd, fcu->fpt);
		
		/* rna path */
		fcu->rna_path = newdataadr(fd, fcu->rna_path);
		
		/* group */
		fcu->grp = newdataadr_ex(fd, fcu->grp, false);
		
		/* clear disabled flag - allows disabled drivers to be tried again ([#32155]),
		 * but also means that another method for "reviving disabled F-Curves" exists
		 */
		fcu->flag &= ~FCURVE_DISABLED;
		
		/* driver */
		fcu->driver= newdataadr(fd, fcu->driver);
		if (fcu->driver) {
			ChannelDriver *driver= fcu->driver;
			DriverVar *dvar;
			
			/* compiled expression data will need to be regenerated (old pointer may still be set here) */
			driver->expr_comp = NULL;
			
			/* give the driver a fresh chance - the operating environment may be different now 
			 * (addons, etc. may be different) so the driver namespace may be sane now [#32155]
			 */
			driver->flag &= ~DRIVER_FLAG_INVALID;
			
			/* relink variables, targets and their paths */
			link_list(fd, &driver->variables);
			for (dvar= driver->variables.first; dvar; dvar= dvar->next) {
				DRIVER_TARGETS_LOOPER(dvar)
				{
					/* only relink the targets being used */
					if (tarIndex < dvar->num_targets)
						dtar->rna_path = newdataadr(fd, dtar->rna_path);
					else
						dtar->rna_path = NULL;
				}
				DRIVER_TARGETS_LOOPER_END
			}
		}
		
		/* modifiers */
		link_list(fd, &fcu->modifiers);
		direct_link_fmodifiers(fd, &fcu->modifiers);
	}
}


static void lib_link_action(FileData *fd, Main *main)
{
	for (bAction *act = main->action.first; act; act = act->id.next) {
		if (act->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(act->id.properties, fd);
			
// XXX deprecated - old animation system <<<
			for (bActionChannel *chan = act->chanbase.first; chan; chan = chan->next) {
				chan->ipo = newlibadr_us(fd, act->id.lib, chan->ipo);
				lib_link_constraint_channels(fd, &act->id, &chan->constraintChannels);
			}
// >>> XXX deprecated - old animation system
			
			lib_link_fcurves(fd, &act->id, &act->curves);

			for (TimeMarker *marker = act->markers.first; marker; marker = marker->next) {
				if (marker->camera) {
					marker->camera = newlibadr(fd, act->id.lib, marker->camera);
				}
			}

			act->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_action(FileData *fd, bAction *act)
{
	bActionChannel *achan; // XXX deprecated - old animation system
	bActionGroup *agrp;

	link_list(fd, &act->curves);
	link_list(fd, &act->chanbase); // XXX deprecated - old animation system
	link_list(fd, &act->groups);
	link_list(fd, &act->markers);

// XXX deprecated - old animation system <<<
	for (achan = act->chanbase.first; achan; achan=achan->next) {
		achan->grp = newdataadr(fd, achan->grp);
		
		link_list(fd, &achan->constraintChannels);
	}
// >>> XXX deprecated - old animation system

	direct_link_fcurves(fd, &act->curves);
	
	for (agrp = act->groups.first; agrp; agrp= agrp->next) {
		agrp->channels.first= newdataadr(fd, agrp->channels.first);
		agrp->channels.last= newdataadr(fd, agrp->channels.last);
	}
}

static void lib_link_nladata_strips(FileData *fd, ID *id, ListBase *list)
{
	NlaStrip *strip;
	
	for (strip = list->first; strip; strip = strip->next) {
		/* check strip's children */
		lib_link_nladata_strips(fd, id, &strip->strips);
		
		/* check strip's F-Curves */
		lib_link_fcurves(fd, id, &strip->fcurves);
		
		/* reassign the counted-reference to action */
		strip->act = newlibadr_us(fd, id->lib, strip->act);
		
		/* fix action id-root (i.e. if it comes from a pre 2.57 .blend file) */
		if ((strip->act) && (strip->act->idroot == 0))
			strip->act->idroot = GS(id->name);
	}
}

static void lib_link_nladata(FileData *fd, ID *id, ListBase *list)
{
	NlaTrack *nlt;
	
	/* we only care about the NLA strips inside the tracks */
	for (nlt = list->first; nlt; nlt = nlt->next) {
		lib_link_nladata_strips(fd, id, &nlt->strips);
	}
}

/* This handles Animato NLA-Strips linking 
 * NOTE: this assumes that link_list has already been called on the list 
 */
static void direct_link_nladata_strips(FileData *fd, ListBase *list)
{
	NlaStrip *strip;
	
	for (strip = list->first; strip; strip = strip->next) {
		/* strip's child strips */
		link_list(fd, &strip->strips);
		direct_link_nladata_strips(fd, &strip->strips);
		
		/* strip's F-Curves */
		link_list(fd, &strip->fcurves);
		direct_link_fcurves(fd, &strip->fcurves);
		
		/* strip's F-Modifiers */
		link_list(fd, &strip->modifiers);
		direct_link_fmodifiers(fd, &strip->modifiers);
	}
}

/* NOTE: this assumes that link_list has already been called on the list */
static void direct_link_nladata(FileData *fd, ListBase *list)
{
	NlaTrack *nlt;
	
	for (nlt = list->first; nlt; nlt = nlt->next) {
		/* relink list of strips */
		link_list(fd, &nlt->strips);
		
		/* relink strip data */
		direct_link_nladata_strips(fd, &nlt->strips);
	}
}

/* ------- */

static void lib_link_keyingsets(FileData *fd, ID *id, ListBase *list)
{
	KeyingSet *ks;
	KS_Path *ksp;
	
	/* here, we're only interested in the ID pointer stored in some of the paths */
	for (ks = list->first; ks; ks = ks->next) {
		for (ksp = ks->paths.first; ksp; ksp = ksp->next) {
			ksp->id= newlibadr(fd, id->lib, ksp->id); 
		}
	}
}

/* NOTE: this assumes that link_list has already been called on the list */
static void direct_link_keyingsets(FileData *fd, ListBase *list)
{
	KeyingSet *ks;
	KS_Path *ksp;
	
	/* link KeyingSet data to KeyingSet again (non ID-libs) */
	for (ks = list->first; ks; ks = ks->next) {
		/* paths */
		link_list(fd, &ks->paths);
		
		for (ksp = ks->paths.first; ksp; ksp = ksp->next) {
			/* rna path */
			ksp->rna_path= newdataadr(fd, ksp->rna_path);
		}
	}
}

/* ------- */

static void lib_link_animdata(FileData *fd, ID *id, AnimData *adt)
{
	if (adt == NULL)
		return;
	
	/* link action data */
	adt->action= newlibadr_us(fd, id->lib, adt->action);
	adt->tmpact= newlibadr_us(fd, id->lib, adt->tmpact);
	
	/* fix action id-roots (i.e. if they come from a pre 2.57 .blend file) */
	if ((adt->action) && (adt->action->idroot == 0))
		adt->action->idroot = GS(id->name);
	if ((adt->tmpact) && (adt->tmpact->idroot == 0))
		adt->tmpact->idroot = GS(id->name);
	
	/* link drivers */
	lib_link_fcurves(fd, id, &adt->drivers);
	
	/* overrides don't have lib-link for now, so no need to do anything */
	
	/* link NLA-data */
	lib_link_nladata(fd, id, &adt->nla_tracks);
}

static void direct_link_animdata(FileData *fd, AnimData *adt)
{
	/* NOTE: must have called newdataadr already before doing this... */
	if (adt == NULL)
		return;
	
	/* link drivers */
	link_list(fd, &adt->drivers);
	direct_link_fcurves(fd, &adt->drivers);
	
	/* link overrides */
	// TODO...
	
	/* link NLA-data */
	link_list(fd, &adt->nla_tracks);
	direct_link_nladata(fd, &adt->nla_tracks);
	
	/* relink active track/strip - even though strictly speaking this should only be used
	 * if we're in 'tweaking mode', we need to be able to have this loaded back for
	 * undo, but also since users may not exit tweakmode before saving (#24535)
	 */
	// TODO: it's not really nice that anyone should be able to save the file in this
	//		state, but it's going to be too hard to enforce this single case...
	adt->act_track = newdataadr(fd, adt->act_track);
	adt->actstrip = newdataadr(fd, adt->actstrip);
}	

/* ************ READ CACHEFILES *************** */

static void lib_link_cachefiles(FileData *fd, Main *bmain)
{
	/* only link ID pointers */
	for (CacheFile *cache_file = bmain->cachefiles.first; cache_file; cache_file = cache_file->id.next) {
		if (cache_file->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(cache_file->id.properties, fd);
			lib_link_animdata(fd, &cache_file->id, cache_file->adt);

			cache_file->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_cachefile(FileData *fd, CacheFile *cache_file)
{
	BLI_listbase_clear(&cache_file->object_paths);
	cache_file->handle = NULL;
	cache_file->handle_mutex = NULL;

	/* relink animdata */
	cache_file->adt = newdataadr(fd, cache_file->adt);
	direct_link_animdata(fd, cache_file->adt);
}

/* ************ READ MOTION PATHS *************** */

/* direct data for cache */
static void direct_link_motionpath(FileData *fd, bMotionPath *mpath)
{
	/* sanity check */
	if (mpath == NULL)
		return;
	
	/* relink points cache */
	mpath->points = newdataadr(fd, mpath->points);
}

/* ************ READ NODE TREE *************** */

/* Single node tree (also used for material/scene trees), ntree is not NULL */
static void lib_link_ntree(FileData *fd, ID *id, bNodeTree *ntree)
{
	bNode *node;
	bNodeSocket *sock;
	
	IDP_LibLinkProperty(ntree->id.properties, fd);
	lib_link_animdata(fd, &ntree->id, ntree->adt);
	
	ntree->gpd = newlibadr_us(fd, id->lib, ntree->gpd);
	
	for (node = ntree->nodes.first; node; node = node->next) {
		/* Link ID Properties -- and copy this comment EXACTLY for easy finding
		 * of library blocks that implement this.*/
		IDP_LibLinkProperty(node->prop, fd);
		
		node->id = newlibadr_us(fd, id->lib, node->id);

		for (sock = node->inputs.first; sock; sock = sock->next) {
			IDP_LibLinkProperty(sock->prop, fd);
		}
		for (sock = node->outputs.first; sock; sock = sock->next) {
			IDP_LibLinkProperty(sock->prop, fd);
		}
	}
	
	for (sock = ntree->inputs.first; sock; sock = sock->next) {
		IDP_LibLinkProperty(sock->prop, fd);
	}
	for (sock = ntree->outputs.first; sock; sock = sock->next) {
		IDP_LibLinkProperty(sock->prop, fd);
	}
}

/* library ntree linking after fileread */
static void lib_link_nodetree(FileData *fd, Main *main)
{
	/* only link ID pointers */
	for (bNodeTree *ntree = main->nodetree.first; ntree; ntree = ntree->id.next) {
		if (ntree->id.tag & LIB_TAG_NEED_LINK) {
			lib_link_ntree(fd, &ntree->id, ntree);

			ntree->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

/* updates group node socket identifier so that
 * external links to/from the group node are preserved.
 */
static void lib_node_do_versions_group_indices(bNode *gnode)
{
	bNodeTree *ngroup = (bNodeTree*)gnode->id;
	bNodeSocket *sock;
	bNodeLink *link;
	
	for (sock=gnode->outputs.first; sock; sock = sock->next) {
		int old_index = sock->to_index;
		
		for (link = ngroup->links.first; link; link = link->next) {
			if (link->tonode == NULL && link->fromsock->own_index == old_index) {
				strcpy(sock->identifier, link->fromsock->identifier);
				/* deprecated */
				sock->own_index = link->fromsock->own_index;
				sock->to_index = 0;
				sock->groupsock = NULL;
			}
		}
	}
	for (sock=gnode->inputs.first; sock; sock = sock->next) {
		int old_index = sock->to_index;
		
		for (link = ngroup->links.first; link; link = link->next) {
			if (link->fromnode == NULL && link->tosock->own_index == old_index) {
				strcpy(sock->identifier, link->tosock->identifier);
				/* deprecated */
				sock->own_index = link->tosock->own_index;
				sock->to_index = 0;
				sock->groupsock = NULL;
			}
		}
	}
}

/* verify types for nodes and groups, all data has to be read */
/* open = 0: appending/linking, open = 1: open new file (need to clean out dynamic
 * typedefs */
static void lib_verify_nodetree(Main *main, int UNUSED(open))
{
	/* this crashes blender on undo/redo */
#if 0
		if (open == 1) {
			reinit_nodesystem();
		}
#endif
	
	/* set node->typeinfo pointers */
	FOREACH_NODETREE(main, ntree, id) {
		ntreeSetTypes(NULL, ntree);
	} FOREACH_NODETREE_END
	
	/* verify static socket templates */
	FOREACH_NODETREE(main, ntree, id) {
		bNode *node;
		for (node=ntree->nodes.first; node; node=node->next)
			node_verify_socket_templates(ntree, node);
	} FOREACH_NODETREE_END
	
	{
		bool has_old_groups = false;
		/* XXX this should actually be part of do_versions, but since we need
		 * finished library linking, it is not possible there. Instead in do_versions
		 * we have set the NTREE_DO_VERSIONS_GROUP_EXPOSE_2_56_2 flag, so at this point we can do the
		 * actual group node updates.
		 */
		for (bNodeTree *ntree = main->nodetree.first; ntree; ntree = ntree->id.next) {
			if (ntree->flag & NTREE_DO_VERSIONS_GROUP_EXPOSE_2_56_2) {
				has_old_groups = 1;
			}
		}
		
		if (has_old_groups) {
			FOREACH_NODETREE(main, ntree, id) {
				/* updates external links for all group nodes in a tree */
				bNode *node;
				for (node = ntree->nodes.first; node; node = node->next) {
					if (node->type == NODE_GROUP) {
						bNodeTree *ngroup = (bNodeTree*)node->id;
						if (ngroup && (ngroup->flag & NTREE_DO_VERSIONS_GROUP_EXPOSE_2_56_2))
							lib_node_do_versions_group_indices(node);
					}
				}
			} FOREACH_NODETREE_END
		}
		
		for (bNodeTree *ntree = main->nodetree.first; ntree; ntree = ntree->id.next) {
			ntree->flag &= ~NTREE_DO_VERSIONS_GROUP_EXPOSE_2_56_2;
		}
	}
	
	{
		/* Convert the previously used ntree->inputs/ntree->outputs lists to interface nodes.
		 * Pre 2.56.2 node trees automatically have all unlinked sockets exposed already
		 * (see NTREE_DO_VERSIONS_GROUP_EXPOSE_2_56_2).
		 *
		 * XXX this should actually be part of do_versions,
		 * but needs valid typeinfo pointers to create interface nodes.
		 *
		 * Note: theoretically only needed in node groups (main->nodetree),
		 * but due to a temporary bug such links could have been added in all trees,
		 * so have to clean up all of them ...
		 */
		
		FOREACH_NODETREE(main, ntree, id) {
			if (ntree->flag & NTREE_DO_VERSIONS_CUSTOMNODES_GROUP) {
				bNode *input_node = NULL, *output_node = NULL;
				int num_inputs = 0, num_outputs = 0;
				bNodeLink *link, *next_link;
				/* Only create new interface nodes for actual older files.
				 * New file versions already have input/output nodes with duplicate links,
				 * in that case just remove the invalid links.
				 */
				const bool create_io_nodes = (ntree->flag & NTREE_DO_VERSIONS_CUSTOMNODES_GROUP_CREATE_INTERFACE) != 0;
				
				float input_locx = 1000000.0f, input_locy = 0.0f;
				float output_locx = -1000000.0f, output_locy = 0.0f;
				/* rough guess, not nice but we don't have access to UI constants here ... */
				static const float offsetx = 42 + 3*20 + 20;
				/*static const float offsety = 0.0f;*/
				
				if (create_io_nodes) {
					if (ntree->inputs.first)
						input_node = nodeAddStaticNode(NULL, ntree, NODE_GROUP_INPUT);
					
					if (ntree->outputs.first)
						output_node = nodeAddStaticNode(NULL, ntree, NODE_GROUP_OUTPUT);
				}
				
				/* Redirect links from/to the node tree interface to input/output node.
				 * If the fromnode/tonode pointers are NULL, this means a link from/to
				 * the ntree interface sockets, which need to be redirected to new interface nodes.
				 */
				for (link = ntree->links.first; link; link = next_link) {
					bool free_link = false;
					next_link = link->next;
					
					if (link->fromnode == NULL) {
						if (input_node) {
							link->fromnode = input_node;
							link->fromsock = node_group_input_find_socket(input_node, link->fromsock->identifier);
							++num_inputs;
							
							if (link->tonode) {
								if (input_locx > link->tonode->locx - offsetx)
									input_locx = link->tonode->locx - offsetx;
								input_locy += link->tonode->locy;
							}
						}
						else {
							free_link = true;
						}
					}
					
					if (link->tonode == NULL) {
						if (output_node) {
							link->tonode = output_node;
							link->tosock = node_group_output_find_socket(output_node, link->tosock->identifier);
							++num_outputs;
							
							if (link->fromnode) {
								if (output_locx < link->fromnode->locx + offsetx)
									output_locx = link->fromnode->locx + offsetx;
								output_locy += link->fromnode->locy;
							}
						}
						else {
							free_link = true;
						}
					}
					
					if (free_link)
						nodeRemLink(ntree, link);
				}
				
				if (num_inputs > 0) {
					input_locy /= num_inputs;
					input_node->locx = input_locx;
					input_node->locy = input_locy;
				}
				if (num_outputs > 0) {
					output_locy /= num_outputs;
					output_node->locx = output_locx;
					output_node->locy = output_locy;
				}
				
				/* clear do_versions flags */
				ntree->flag &= ~(NTREE_DO_VERSIONS_CUSTOMNODES_GROUP | NTREE_DO_VERSIONS_CUSTOMNODES_GROUP_CREATE_INTERFACE);
			}
		}
		FOREACH_NODETREE_END
	}
	
	/* verify all group user nodes */
	for (bNodeTree *ntree = main->nodetree.first; ntree; ntree = ntree->id.next) {
		ntreeVerifyNodes(main, &ntree->id);
	}
	
	/* make update calls where necessary */
	{
		FOREACH_NODETREE(main, ntree, id) {
			/* make an update call for the tree */
			ntreeUpdateTree(main, ntree);
		} FOREACH_NODETREE_END
	}
}

static void direct_link_node_socket(FileData *fd, bNodeSocket *sock)
{
	sock->prop = newdataadr(fd, sock->prop);
	IDP_DirectLinkGroup_OrFree(&sock->prop, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
	
	sock->link = newdataadr(fd, sock->link);
	sock->typeinfo = NULL;
	sock->storage = newdataadr(fd, sock->storage);
	sock->default_value = newdataadr(fd, sock->default_value);
	sock->cache = NULL;
}

/* ntree itself has been read! */
static void direct_link_nodetree(FileData *fd, bNodeTree *ntree)
{
	/* note: writing and reading goes in sync, for speed */
	bNode *node;
	bNodeSocket *sock;
	bNodeLink *link;
	
	ntree->init = 0;		/* to set callbacks and force setting types */
	ntree->is_updating = false;
	ntree->typeinfo= NULL;
	ntree->interface_type = NULL;
	
	ntree->progress = NULL;
	ntree->execdata = NULL;
	ntree->duplilock = NULL;

	ntree->adt = newdataadr(fd, ntree->adt);
	direct_link_animdata(fd, ntree->adt);
	
	ntree->id.tag &= ~(LIB_TAG_ID_RECALC|LIB_TAG_ID_RECALC_DATA);

	link_list(fd, &ntree->nodes);
	for (node = ntree->nodes.first; node; node = node->next) {
		node->typeinfo = NULL;
		
		link_list(fd, &node->inputs);
		link_list(fd, &node->outputs);
		
		node->prop = newdataadr(fd, node->prop);
		IDP_DirectLinkGroup_OrFree(&node->prop, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
		
		link_list(fd, &node->internal_links);
		for (link = node->internal_links.first; link; link = link->next) {
			link->fromnode = newdataadr(fd, link->fromnode);
			link->fromsock = newdataadr(fd, link->fromsock);
			link->tonode = newdataadr(fd, link->tonode);
			link->tosock = newdataadr(fd, link->tosock);
		}
		
		if (node->type == CMP_NODE_MOVIEDISTORTION) {
			node->storage = newmclipadr(fd, node->storage);
		}
		else {
			node->storage = newdataadr(fd, node->storage);
		}
		
		if (node->storage) {
			/* could be handlerized at some point */
			if (ntree->type==NTREE_SHADER) {
				if (node->type==SH_NODE_CURVE_VEC || node->type==SH_NODE_CURVE_RGB) {
					direct_link_curvemapping(fd, node->storage);
				}
				else if (node->type==SH_NODE_SCRIPT) {
					NodeShaderScript *nss = (NodeShaderScript *) node->storage;
					nss->bytecode = newdataadr(fd, nss->bytecode);
				}
				else if (node->type==SH_NODE_TEX_POINTDENSITY) {
					NodeShaderTexPointDensity *npd = (NodeShaderTexPointDensity *) node->storage;
					memset(&npd->pd, 0, sizeof(npd->pd));
				}
			}
			else if (ntree->type==NTREE_COMPOSIT) {
				if (ELEM(node->type, CMP_NODE_TIME, CMP_NODE_CURVE_VEC, CMP_NODE_CURVE_RGB, CMP_NODE_HUECORRECT))
					direct_link_curvemapping(fd, node->storage);
				else if (ELEM(node->type, CMP_NODE_IMAGE, CMP_NODE_R_LAYERS, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER))
					((ImageUser *)node->storage)->ok = 1;
			}
			else if ( ntree->type==NTREE_TEXTURE) {
				if (node->type==TEX_NODE_CURVE_RGB || node->type==TEX_NODE_CURVE_TIME)
					direct_link_curvemapping(fd, node->storage);
				else if (node->type==TEX_NODE_IMAGE)
					((ImageUser *)node->storage)->ok = 1;
			}
		}
	}
	link_list(fd, &ntree->links);
	
	/* and we connect the rest */
	for (node = ntree->nodes.first; node; node = node->next) {
		node->parent = newdataadr(fd, node->parent);
		node->lasty = 0;
		
		for (sock = node->inputs.first; sock; sock = sock->next)
			direct_link_node_socket(fd, sock);
		for (sock = node->outputs.first; sock; sock = sock->next)
			direct_link_node_socket(fd, sock);
	}
	
	/* interface socket lists */
	link_list(fd, &ntree->inputs);
	link_list(fd, &ntree->outputs);
	for (sock = ntree->inputs.first; sock; sock = sock->next)
		direct_link_node_socket(fd, sock);
	for (sock = ntree->outputs.first; sock; sock = sock->next)
		direct_link_node_socket(fd, sock);
	
	for (link = ntree->links.first; link; link= link->next) {
		link->fromnode = newdataadr(fd, link->fromnode);
		link->tonode = newdataadr(fd, link->tonode);
		link->fromsock = newdataadr(fd, link->fromsock);
		link->tosock = newdataadr(fd, link->tosock);
	}
	
#if 0
	if (ntree->previews) {
		bNodeInstanceHash *new_previews = BKE_node_instance_hash_new("node previews");
		bNodeInstanceHashIterator iter;
		
		NODE_INSTANCE_HASH_ITER(iter, ntree->previews) {
			bNodePreview *preview = BKE_node_instance_hash_iterator_get_value(&iter);
			if (preview) {
				bNodePreview *new_preview = newimaadr(fd, preview);
				if (new_preview) {
					bNodeInstanceKey key = BKE_node_instance_hash_iterator_get_key(&iter);
					BKE_node_instance_hash_insert(new_previews, key, new_preview);
				}
			}
		}
		BKE_node_instance_hash_free(ntree->previews, NULL);
		ntree->previews = new_previews;
	}
#else
	/* XXX TODO */
	ntree->previews = NULL;
#endif
	
	/* type verification is in lib-link */
}

/* ************ READ ARMATURE ***************** */

/* temp struct used to transport needed info to lib_link_constraint_cb() */
typedef struct tConstraintLinkData {
	FileData *fd;
	ID *id;
} tConstraintLinkData;
/* callback function used to relink constraint ID-links */
static void lib_link_constraint_cb(bConstraint *UNUSED(con), ID **idpoin, bool is_reference, void *userdata)
{
	tConstraintLinkData *cld= (tConstraintLinkData *)userdata;
	
	/* for reference types, we need to increment the usercounts on load... */
	if (is_reference) {
		/* reference type - with usercount */
		*idpoin = newlibadr_us(cld->fd, cld->id->lib, *idpoin);
	}
	else {
		/* target type - no usercount needed */
		*idpoin = newlibadr(cld->fd, cld->id->lib, *idpoin);
	}
}

static void lib_link_constraints(FileData *fd, ID *id, ListBase *conlist)
{
	tConstraintLinkData cld;
	bConstraint *con;
	
	/* legacy fixes */
	for (con = conlist->first; con; con=con->next) {
		/* patch for error introduced by changing constraints (dunno how) */
		/* if con->data type changes, dna cannot resolve the pointer! (ton) */
		if (con->data == NULL) {
			con->type = CONSTRAINT_TYPE_NULL;
		}
		/* own ipo, all constraints have it */
		con->ipo = newlibadr_us(fd, id->lib, con->ipo); // XXX deprecated - old animation system
	}
	
	/* relink all ID-blocks used by the constraints */
	cld.fd = fd;
	cld.id = id;
	
	BKE_constraints_id_loop(conlist, lib_link_constraint_cb, &cld);
}

static void direct_link_constraints(FileData *fd, ListBase *lb)
{
	bConstraint *con;
	
	link_list(fd, lb);
	for (con=lb->first; con; con=con->next) {
		con->data = newdataadr(fd, con->data);
		
		switch (con->type) {
			case CONSTRAINT_TYPE_PYTHON:
			{
				bPythonConstraint *data= con->data;
				
				link_list(fd, &data->targets);
				
				data->prop = newdataadr(fd, data->prop);
				IDP_DirectLinkGroup_OrFree(&data->prop, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
				break;
			}
			case CONSTRAINT_TYPE_SPLINEIK:
			{
				bSplineIKConstraint *data= con->data;

				data->points= newdataadr(fd, data->points);
				break;
			}
			case CONSTRAINT_TYPE_KINEMATIC:
			{
				bKinematicConstraint *data = con->data;

				con->lin_error = 0.f;
				con->rot_error = 0.f;

				/* version patch for runtime flag, was not cleared in some case */
				data->flag &= ~CONSTRAINT_IK_AUTO;
				break;
			}
			case CONSTRAINT_TYPE_CHILDOF:
			{
				/* XXX version patch, in older code this flag wasn't always set, and is inherent to type */
				if (con->ownspace == CONSTRAINT_SPACE_POSE)
					con->flag |= CONSTRAINT_SPACEONCE;
				break;
			}
			case CONSTRAINT_TYPE_TRANSFORM_CACHE:
			{
				bTransformCacheConstraint *data = con->data;
				data->reader = NULL;
			}
		}
	}
}

static void lib_link_pose(FileData *fd, Main *bmain, Object *ob, bPose *pose)
{
	bArmature *arm = ob->data;
	
	if (!pose || !arm)
		return;
	
	/* always rebuild to match proxy or lib changes, but on Undo */
	bool rebuild = false;

	if (fd->memfile == NULL) {
		if (ob->proxy || (ob->id.lib==NULL && arm->id.lib)) {
			rebuild = true;
		}
	}

	/* avoid string */
	GHash *bone_hash = BKE_armature_bone_from_name_map(arm);

	if (ob->proxy) {
		/* sync proxy layer */
		if (pose->proxy_layer)
			arm->layer = pose->proxy_layer;
		
		/* sync proxy active bone */
		if (pose->proxy_act_bone[0]) {
			Bone *bone = BLI_ghash_lookup(bone_hash, pose->proxy_act_bone);
			if (bone) {
				arm->act_bone = bone;
			}
		}
	}

	for (bPoseChannel *pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
		lib_link_constraints(fd, (ID *)ob, &pchan->constraints);

		pchan->bone = BLI_ghash_lookup(bone_hash, pchan->name);
		
		IDP_LibLinkProperty(pchan->prop, fd);

		pchan->custom = newlibadr_us(fd, arm->id.lib, pchan->custom);
		if (UNLIKELY(pchan->bone == NULL)) {
			rebuild = true;
		}
		else if ((ob->id.lib == NULL) && arm->id.lib) {
			/* local pose selection copied to armature, bit hackish */
			pchan->bone->flag &= ~BONE_SELECTED;
			pchan->bone->flag |= pchan->selectflag;
		}
	}

	BLI_ghash_free(bone_hash, NULL, NULL);
	

	if (rebuild) {
		DAG_id_tag_update_ex(bmain, &ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
		BKE_pose_tag_recalc(bmain, pose);
	}
}

static void lib_link_bones(FileData *fd, Bone *bone)
{
	IDP_LibLinkProperty(bone->prop, fd);

	for (Bone *curbone = bone->childbase.first; curbone; curbone = curbone->next) {
		lib_link_bones(fd, curbone);
	}
}

static void lib_link_armature(FileData *fd, Main *main)
{
	for (bArmature *arm = main->armature.first; arm; arm = arm->id.next) {
		if (arm->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(arm->id.properties, fd);
			lib_link_animdata(fd, &arm->id, arm->adt);

			for (Bone *curbone = arm->bonebase.first; curbone; curbone = curbone->next) {
				lib_link_bones(fd, curbone);
			}

			arm->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_bones(FileData *fd, Bone *bone)
{
	Bone *child;
	
	bone->parent = newdataadr(fd, bone->parent);
	bone->prop = newdataadr(fd, bone->prop);
	IDP_DirectLinkGroup_OrFree(&bone->prop, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
		
	bone->flag &= ~BONE_DRAW_ACTIVE;
	
	link_list(fd, &bone->childbase);
	
	for (child=bone->childbase.first; child; child=child->next)
		direct_link_bones(fd, child);
}

static void direct_link_armature(FileData *fd, bArmature *arm)
{
	Bone *bone;
	
	link_list(fd, &arm->bonebase);
	arm->edbo = NULL;
	arm->sketch = NULL;
	
	arm->adt = newdataadr(fd, arm->adt);
	direct_link_animdata(fd, arm->adt);
	
	for (bone = arm->bonebase.first; bone; bone = bone->next) {
		direct_link_bones(fd, bone);
	}
	
	arm->act_bone = newdataadr(fd, arm->act_bone);
	arm->act_edbone = NULL;
}

/* ************ READ CAMERA ***************** */

static void lib_link_camera(FileData *fd, Main *main)
{
	for (Camera *ca = main->camera.first; ca; ca = ca->id.next) {
		if (ca->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(ca->id.properties, fd);
			lib_link_animdata(fd, &ca->id, ca->adt);
			
			ca->ipo = newlibadr_us(fd, ca->id.lib, ca->ipo); // XXX deprecated - old animation system

			ca->dof_ob = newlibadr(fd, ca->id.lib, ca->dof_ob);
			
			ca->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_camera(FileData *fd, Camera *ca)
{
	ca->adt = newdataadr(fd, ca->adt);
	direct_link_animdata(fd, ca->adt);
}


/* ************ READ LAMP ***************** */

static void lib_link_lamp(FileData *fd, Main *main)
{
	for (Lamp *la = main->lamp.first; la; la = la->id.next) {
		if (la->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(la->id.properties, fd);
			lib_link_animdata(fd, &la->id, la->adt);
			
			for (int a = 0; a < MAX_MTEX; a++) {
				MTex *mtex = la->mtex[a];
				if (mtex) {
					mtex->tex = newlibadr_us(fd, la->id.lib, mtex->tex);
					mtex->object = newlibadr(fd, la->id.lib, mtex->object);
				}
			}
			
			la->ipo = newlibadr_us(fd, la->id.lib, la->ipo); // XXX deprecated - old animation system
			
			if (la->nodetree) {
				lib_link_ntree(fd, &la->id, la->nodetree);
				la->nodetree->id.lib = la->id.lib;
			}
			
			la->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_lamp(FileData *fd, Lamp *la)
{
	int a;
	
	la->adt = newdataadr(fd, la->adt);
	direct_link_animdata(fd, la->adt);
	
	for (a=0; a<MAX_MTEX; a++) {
		la->mtex[a] = newdataadr(fd, la->mtex[a]);
	}
	
	la->curfalloff = newdataadr(fd, la->curfalloff);
	if (la->curfalloff)
		direct_link_curvemapping(fd, la->curfalloff);

	la->nodetree= newdataadr(fd, la->nodetree);
	if (la->nodetree) {
		direct_link_id(fd, &la->nodetree->id);
		direct_link_nodetree(fd, la->nodetree);
	}
	
	la->preview = direct_link_preview_image(fd, la->preview);
}

/* ************ READ keys ***************** */

void blo_do_versions_key_uidgen(Key *key)
{
	KeyBlock *block;

	key->uidgen = 1;
	for (block = key->block.first; block; block = block->next) {
		block->uid = key->uidgen++;
	}
}

static void lib_link_key(FileData *fd, Main *main)
{
	for (Key *key = main->key.first; key; key = key->id.next) {
		BLI_assert((key->id.tag & LIB_TAG_EXTERN) == 0);

		if (key->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(key->id.properties, fd);
			lib_link_animdata(fd, &key->id, key->adt);
			
			key->ipo = newlibadr_us(fd, key->id.lib, key->ipo); // XXX deprecated - old animation system
			key->from = newlibadr(fd, key->id.lib, key->from);
			
			key->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void switch_endian_keyblock(Key *key, KeyBlock *kb)
{
	int elemsize, a, b;
	char *data;
	
	elemsize = key->elemsize;
	data = kb->data;
	
	for (a = 0; a < kb->totelem; a++) {
		const char *cp = key->elemstr;
		char *poin = data;
		
		while (cp[0]) {  /* cp[0] == amount */
			switch (cp[1]) {  /* cp[1] = type */
				case IPO_FLOAT:
				case IPO_BPOINT:
				case IPO_BEZTRIPLE:
					b = cp[0];
					BLI_endian_switch_float_array((float *)poin, b);
					poin += sizeof(float) * b;
					break;
			}
			
			cp += 2;
		}
		data += elemsize;
	}
}

static void direct_link_key(FileData *fd, Key *key)
{
	KeyBlock *kb;
	
	link_list(fd, &(key->block));
	
	key->adt = newdataadr(fd, key->adt);
	direct_link_animdata(fd, key->adt);
		
	key->refkey= newdataadr(fd, key->refkey);
	
	for (kb = key->block.first; kb; kb = kb->next) {
		kb->data = newdataadr(fd, kb->data);
		
		if (fd->flags & FD_FLAGS_SWITCH_ENDIAN)
			switch_endian_keyblock(key, kb);
	}
}

/* ************ READ mball ***************** */

static void lib_link_mball(FileData *fd, Main *main)
{
	for (MetaBall *mb = main->mball.first; mb; mb = mb->id.next) {
		if (mb->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(mb->id.properties, fd);
			lib_link_animdata(fd, &mb->id, mb->adt);
			
			for (int a = 0; a < mb->totcol; a++) {
				mb->mat[a] = newlibadr_us(fd, mb->id.lib, mb->mat[a]);
			}
			
			mb->ipo = newlibadr_us(fd, mb->id.lib, mb->ipo); // XXX deprecated - old animation system
			
			mb->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_mball(FileData *fd, MetaBall *mb)
{
	mb->adt = newdataadr(fd, mb->adt);
	direct_link_animdata(fd, mb->adt);
	
	mb->mat = newdataadr(fd, mb->mat);
	test_pointer_array(fd, (void **)&mb->mat);
	
	link_list(fd, &(mb->elems));
	
	BLI_listbase_clear(&mb->disp);
	mb->editelems = NULL;
/*	mb->edit_elems.first= mb->edit_elems.last= NULL;*/
	mb->lastelem = NULL;
}

/* ************ READ WORLD ***************** */

static void lib_link_world(FileData *fd, Main *main)
{
	for (World *wrld = main->world.first; wrld; wrld = wrld->id.next) {
		if (wrld->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(wrld->id.properties, fd);
			lib_link_animdata(fd, &wrld->id, wrld->adt);
			
			wrld->ipo = newlibadr_us(fd, wrld->id.lib, wrld->ipo); // XXX deprecated - old animation system
			
			for (int a = 0; a < MAX_MTEX; a++) {
				MTex *mtex = wrld->mtex[a];
				if (mtex) {
					mtex->tex = newlibadr_us(fd, wrld->id.lib, mtex->tex);
					mtex->object = newlibadr(fd, wrld->id.lib, mtex->object);
				}
			}
			
			if (wrld->nodetree) {
				lib_link_ntree(fd, &wrld->id, wrld->nodetree);
				wrld->nodetree->id.lib = wrld->id.lib;
			}
			
			wrld->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_world(FileData *fd, World *wrld)
{
	int a;
	
	wrld->adt = newdataadr(fd, wrld->adt);
	direct_link_animdata(fd, wrld->adt);
	
	for (a = 0; a < MAX_MTEX; a++) {
		wrld->mtex[a] = newdataadr(fd, wrld->mtex[a]);
	}
	
	wrld->nodetree = newdataadr(fd, wrld->nodetree);
	if (wrld->nodetree) {
		direct_link_id(fd, &wrld->nodetree->id);
		direct_link_nodetree(fd, wrld->nodetree);
	}
	
	wrld->preview = direct_link_preview_image(fd, wrld->preview);
	BLI_listbase_clear(&wrld->gpumaterial);
}


/* ************ READ VFONT ***************** */

static void lib_link_vfont(FileData *fd, Main *main)
{
	for (VFont *vf = main->vfont.first; vf; vf = vf->id.next) {
		if (vf->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(vf->id.properties, fd);

			vf->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_vfont(FileData *fd, VFont *vf)
{
	vf->data = NULL;
	vf->temp_pf = NULL;
	vf->packedfile = direct_link_packedfile(fd, vf->packedfile);
}

/* ************ READ TEXT ****************** */

static void lib_link_text(FileData *fd, Main *main)
{
	for (Text *text = main->text.first; text; text = text->id.next) {
		if (text->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(text->id.properties, fd);

			text->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_text(FileData *fd, Text *text)
{
	TextLine *ln;
	
	text->name = newdataadr(fd, text->name);
	
	text->undo_pos = -1;
	text->undo_len = TXT_INIT_UNDO;
	text->undo_buf = MEM_mallocN(text->undo_len, "undo buf");
	
	text->compiled = NULL;
	
#if 0
	if (text->flags & TXT_ISEXT) {
		BKE_text_reload(text);
		}
		/* else { */
#endif
	
	link_list(fd, &text->lines);
	
	text->curl = newdataadr(fd, text->curl);
	text->sell = newdataadr(fd, text->sell);
	
	for (ln = text->lines.first; ln; ln = ln->next) {
		ln->line = newdataadr(fd, ln->line);
		ln->format = NULL;
		
		if (ln->len != (int) strlen(ln->line)) {
			printf("Error loading text, line lengths differ\n");
			ln->len = strlen(ln->line);
		}
	}
	
	text->flags = (text->flags) & ~TXT_ISEXT;
	
	id_us_ensure_real(&text->id);
}

/* ************ READ IMAGE ***************** */

static void lib_link_image(FileData *fd, Main *main)
{
	for (Image *ima = main->image.first; ima; ima = ima->id.next) {
		if (ima->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(ima->id.properties, fd);
			
			ima->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_image(FileData *fd, Image *ima)
{
	ImagePackedFile *imapf;

	/* for undo system, pointers could be restored */
	if (fd->imamap)
		ima->cache = newimaadr(fd, ima->cache);
	else
		ima->cache = NULL;

	/* if not restored, we keep the binded opengl index */
	if (!ima->cache) {
		ima->tpageflag &= ~IMA_GLBIND_IS_DATA;
		for (int i = 0; i < TEXTARGET_COUNT; i++) {
			ima->bindcode[i] = 0;
			ima->gputexture[i] = NULL;
		}
		ima->rr = NULL;
	}

	ima->repbind = NULL;
	
	/* undo system, try to restore render buffers */
	if (fd->imamap) {
		int a;
		
		for (a = 0; a < IMA_MAX_RENDER_SLOT; a++)
			ima->renders[a] = newimaadr(fd, ima->renders[a]);
	}
	else {
		memset(ima->renders, 0, sizeof(ima->renders));
		ima->last_render_slot = ima->render_slot;
	}

	link_list(fd, &(ima->views));
	link_list(fd, &(ima->packedfiles));

	if (ima->packedfiles.first) {
		for (imapf = ima->packedfiles.first; imapf; imapf = imapf->next) {
			imapf->packedfile = direct_link_packedfile(fd, imapf->packedfile);
		}
		ima->packedfile = NULL;
	}
	else {
		ima->packedfile = direct_link_packedfile(fd, ima->packedfile);
	}

	BLI_listbase_clear(&ima->anims);
	ima->preview = direct_link_preview_image(fd, ima->preview);
	ima->stereo3d_format = newdataadr(fd, ima->stereo3d_format);
	ima->ok = 1;
}


/* ************ READ CURVE ***************** */

static void lib_link_curve(FileData *fd, Main *main)
{
	for (Curve *cu = main->curve.first; cu; cu = cu->id.next) {
		if (cu->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(cu->id.properties, fd);
			lib_link_animdata(fd, &cu->id, cu->adt);
			
			for (int a = 0; a < cu->totcol; a++) {
				cu->mat[a] = newlibadr_us(fd, cu->id.lib, cu->mat[a]);
			}
			
			cu->bevobj = newlibadr(fd, cu->id.lib, cu->bevobj);
			cu->taperobj = newlibadr(fd, cu->id.lib, cu->taperobj);
			cu->textoncurve = newlibadr(fd, cu->id.lib, cu->textoncurve);
			cu->vfont = newlibadr_us(fd, cu->id.lib, cu->vfont);
			cu->vfontb = newlibadr_us(fd, cu->id.lib, cu->vfontb);
			cu->vfonti = newlibadr_us(fd, cu->id.lib, cu->vfonti);
			cu->vfontbi = newlibadr_us(fd, cu->id.lib, cu->vfontbi);
			
			cu->ipo = newlibadr_us(fd, cu->id.lib, cu->ipo); // XXX deprecated - old animation system
			cu->key = newlibadr_us(fd, cu->id.lib, cu->key);
			
			cu->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}


static void switch_endian_knots(Nurb *nu)
{
	if (nu->knotsu) {
		BLI_endian_switch_float_array(nu->knotsu, KNOTSU(nu));
	}
	if (nu->knotsv) {
		BLI_endian_switch_float_array(nu->knotsv, KNOTSV(nu));
	}
}

static void direct_link_curve(FileData *fd, Curve *cu)
{
	Nurb *nu;
	TextBox *tb;
	
	cu->adt= newdataadr(fd, cu->adt);
	direct_link_animdata(fd, cu->adt);
	
	/* Protect against integer overflow vulnerability. */
	CLAMP(cu->len_wchar, 0, INT_MAX - 4);

	cu->mat = newdataadr(fd, cu->mat);
	test_pointer_array(fd, (void **)&cu->mat);
	cu->str = newdataadr(fd, cu->str);
	cu->strinfo= newdataadr(fd, cu->strinfo);
	cu->tb = newdataadr(fd, cu->tb);

	if (cu->vfont == NULL) {
		link_list(fd, &(cu->nurb));
	}
	else {
		cu->nurb.first=cu->nurb.last= NULL;
		
		tb = MEM_calloc_arrayN(MAXTEXTBOX, sizeof(TextBox), "TextBoxread");
		if (cu->tb) {
			memcpy(tb, cu->tb, cu->totbox*sizeof(TextBox));
			MEM_freeN(cu->tb);
			cu->tb = tb;
		}
		else {
			cu->totbox = 1;
			cu->actbox = 1;
			cu->tb = tb;
			cu->tb[0].w = cu->linewidth;
		}
		if (cu->wordspace == 0.0f) cu->wordspace = 1.0f;
	}

	cu->editnurb = NULL;
	cu->editfont = NULL;
	
	for (nu = cu->nurb.first; nu; nu = nu->next) {
		nu->bezt = newdataadr(fd, nu->bezt);
		nu->bp = newdataadr(fd, nu->bp);
		nu->knotsu = newdataadr(fd, nu->knotsu);
		nu->knotsv = newdataadr(fd, nu->knotsv);
		if (cu->vfont == NULL) nu->charidx = 0;
		
		if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
			switch_endian_knots(nu);
		}
	}
	cu->bb = NULL;
}

/* ************ READ TEX ***************** */

static void lib_link_texture(FileData *fd, Main *main)
{
	for (Tex *tex = main->tex.first; tex; tex = tex->id.next) {
		if (tex->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(tex->id.properties, fd);
			lib_link_animdata(fd, &tex->id, tex->adt);
			
			tex->ima = newlibadr_us(fd, tex->id.lib, tex->ima);
			tex->ipo = newlibadr_us(fd, tex->id.lib, tex->ipo);  // XXX deprecated - old animation system
			if (tex->env)
				tex->env->object = newlibadr(fd, tex->id.lib, tex->env->object);
			if (tex->pd)
				tex->pd->object = newlibadr(fd, tex->id.lib, tex->pd->object);
			if (tex->vd)
				tex->vd->object = newlibadr(fd, tex->id.lib, tex->vd->object);
			if (tex->ot)
				tex->ot->object = newlibadr(fd, tex->id.lib, tex->ot->object);
			
			if (tex->nodetree) {
				lib_link_ntree(fd, &tex->id, tex->nodetree);
				tex->nodetree->id.lib = tex->id.lib;
			}
			
			tex->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_texture(FileData *fd, Tex *tex)
{
	tex->adt = newdataadr(fd, tex->adt);
	direct_link_animdata(fd, tex->adt);

	tex->coba = newdataadr(fd, tex->coba);
	tex->env = newdataadr(fd, tex->env);
	if (tex->env) {
		tex->env->ima = NULL;
		memset(tex->env->cube, 0, 6 * sizeof(void *));
		tex->env->ok= 0;
	}
	tex->pd = newdataadr(fd, tex->pd);
	if (tex->pd) {
		tex->pd->point_tree = NULL;
		tex->pd->coba = newdataadr(fd, tex->pd->coba);
		tex->pd->falloff_curve = newdataadr(fd, tex->pd->falloff_curve);
		if (tex->pd->falloff_curve) {
			direct_link_curvemapping(fd, tex->pd->falloff_curve);
		}
		tex->pd->point_data = NULL; /* runtime data */
	}
	
	tex->vd = newdataadr(fd, tex->vd);
	if (tex->vd) {
		tex->vd->dataset = NULL;
		tex->vd->ok = 0;
	}
	else {
		if (tex->type == TEX_VOXELDATA)
			tex->vd = MEM_callocN(sizeof(VoxelData), "direct_link_texture VoxelData");
	}
	
	tex->ot = newdataadr(fd, tex->ot);
	
	tex->nodetree = newdataadr(fd, tex->nodetree);
	if (tex->nodetree) {
		direct_link_id(fd, &tex->nodetree->id);
		direct_link_nodetree(fd, tex->nodetree);
	}
	
	tex->preview = direct_link_preview_image(fd, tex->preview);
	
	tex->iuser.ok = 1;
}



/* ************ READ MATERIAL ***************** */

static void lib_link_material(FileData *fd, Main *main)
{
	for (Material *ma = main->mat.first; ma; ma = ma->id.next) {
		if (ma->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(ma->id.properties, fd);
			lib_link_animdata(fd, &ma->id, ma->adt);
			
			ma->ipo = newlibadr_us(fd, ma->id.lib, ma->ipo);  // XXX deprecated - old animation system
			ma->group = newlibadr_us(fd, ma->id.lib, ma->group);
			
			for (int a = 0; a < MAX_MTEX; a++) {
				MTex *mtex = ma->mtex[a];
				if (mtex) {
					mtex->tex = newlibadr_us(fd, ma->id.lib, mtex->tex);
					mtex->object = newlibadr(fd, ma->id.lib, mtex->object);
				}
			}
			
			if (ma->nodetree) {
				lib_link_ntree(fd, &ma->id, ma->nodetree);
				ma->nodetree->id.lib = ma->id.lib;
			}
			
			ma->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_material(FileData *fd, Material *ma)
{
	int a;
	
	ma->adt = newdataadr(fd, ma->adt);
	direct_link_animdata(fd, ma->adt);
	
	for (a = 0; a < MAX_MTEX; a++) {
		ma->mtex[a] = newdataadr(fd, ma->mtex[a]);
	}
	ma->texpaintslot = NULL;

	ma->ramp_col = newdataadr(fd, ma->ramp_col);
	ma->ramp_spec = newdataadr(fd, ma->ramp_spec);
	
	ma->nodetree = newdataadr(fd, ma->nodetree);
	if (ma->nodetree) {
		direct_link_id(fd, &ma->nodetree->id);
		direct_link_nodetree(fd, ma->nodetree);
	}
	
	ma->preview = direct_link_preview_image(fd, ma->preview);
	BLI_listbase_clear(&ma->gpumaterial);
}

/* ************ READ PARTICLE SETTINGS ***************** */
/* update this also to writefile.c */
static const char *ptcache_data_struct[] = {
	"", // BPHYS_DATA_INDEX
	"", // BPHYS_DATA_LOCATION
	"", // BPHYS_DATA_VELOCITY
	"", // BPHYS_DATA_ROTATION
	"", // BPHYS_DATA_AVELOCITY / BPHYS_DATA_XCONST */
	"", // BPHYS_DATA_SIZE:
	"", // BPHYS_DATA_TIMES:
	"BoidData" // case BPHYS_DATA_BOIDS:
};

static void direct_link_pointcache_cb(FileData *fd, void *data)
{
	PTCacheMem *pm = data;
	PTCacheExtra *extra;
	int i;
	for (i = 0; i < BPHYS_TOT_DATA; i++) {
		pm->data[i] = newdataadr(fd, pm->data[i]);

		/* the cache saves non-struct data without DNA */
		if (pm->data[i] && ptcache_data_struct[i][0]=='\0' && (fd->flags & FD_FLAGS_SWITCH_ENDIAN)) {
			int tot = (BKE_ptcache_data_size(i) * pm->totpoint) / sizeof(int);  /* data_size returns bytes */
			int *poin = pm->data[i];

			BLI_endian_switch_int32_array(poin, tot);
		}
	}

	link_list(fd, &pm->extradata);

	for (extra=pm->extradata.first; extra; extra=extra->next)
		extra->data = newdataadr(fd, extra->data);
}

static void direct_link_pointcache(FileData *fd, PointCache *cache)
{
	if ((cache->flag & PTCACHE_DISK_CACHE)==0) {
		link_list_ex(fd, &cache->mem_cache, direct_link_pointcache_cb);
	}
	else
		BLI_listbase_clear(&cache->mem_cache);
	
	cache->flag &= ~PTCACHE_SIMULATION_VALID;
	cache->simframe = 0;
	cache->edit = NULL;
	cache->free_edit = NULL;
	cache->cached_frames = NULL;
}

static void direct_link_pointcache_list(FileData *fd, ListBase *ptcaches, PointCache **ocache, int force_disk)
{
	if (ptcaches->first) {
		PointCache *cache= NULL;
		link_list(fd, ptcaches);
		for (cache=ptcaches->first; cache; cache=cache->next) {
			direct_link_pointcache(fd, cache);
			if (force_disk) {
				cache->flag |= PTCACHE_DISK_CACHE;
				cache->step = 1;
			}
		}
		
		*ocache = newdataadr(fd, *ocache);
	}
	else if (*ocache) {
		/* old "single" caches need to be linked too */
		*ocache = newdataadr(fd, *ocache);
		direct_link_pointcache(fd, *ocache);
		if (force_disk) {
			(*ocache)->flag |= PTCACHE_DISK_CACHE;
			(*ocache)->step = 1;
		}
		
		ptcaches->first = ptcaches->last = *ocache;
	}
}

static void lib_link_partdeflect(FileData *fd, ID *id, PartDeflect *pd)
{
	if (pd && pd->tex)
		pd->tex = newlibadr_us(fd, id->lib, pd->tex);
	if (pd && pd->f_source)
		pd->f_source = newlibadr(fd, id->lib, pd->f_source);
}

static void lib_link_particlesettings(FileData *fd, Main *main)
{
	for (ParticleSettings *part = main->particle.first; part; part = part->id.next) {
		if (part->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(part->id.properties, fd);
			lib_link_animdata(fd, &part->id, part->adt);

			part->ipo = newlibadr_us(fd, part->id.lib, part->ipo); // XXX deprecated - old animation system
			
			part->dup_ob = newlibadr(fd, part->id.lib, part->dup_ob);
			part->dup_group = newlibadr(fd, part->id.lib, part->dup_group);
			part->eff_group = newlibadr(fd, part->id.lib, part->eff_group);
			part->bb_ob = newlibadr(fd, part->id.lib, part->bb_ob);
			part->collision_group = newlibadr(fd, part->id.lib, part->collision_group);
			
			lib_link_partdeflect(fd, &part->id, part->pd);
			lib_link_partdeflect(fd, &part->id, part->pd2);
			
			if (part->effector_weights) {
				part->effector_weights->group = newlibadr(fd, part->id.lib, part->effector_weights->group);
			}
			else {
				part->effector_weights = BKE_add_effector_weights(part->eff_group);
			}

			if (part->dupliweights.first && part->dup_group) {
				ParticleDupliWeight *dw;
				int index_ok = 0;
				/* check for old files without indices (all indexes 0) */
				if (BLI_listbase_is_single(&part->dupliweights)) {
					/* special case for only one object in the group */
					index_ok = 1;
				}
				else {
					for (dw = part->dupliweights.first; dw; dw = dw->next) {
						if (dw->index > 0) {
							index_ok = 1;
							break;
						}
					}
				}

				if (index_ok) {
					/* if we have indexes, let's use them */
					for (dw = part->dupliweights.first; dw; dw = dw->next) {
						/* Do not try to restore pointer here, we have to search for group objects in another
						 * separated step.
						 * Reason is, the used group may be linked from another library, which has not yet
						 * been 'lib_linked'.
						 * Since dw->ob is not considered as an object user (it does not make objet directly linked),
						 * we may have no valid way to retrieve it yet.
						 * See T49273. */
						dw->ob = NULL;
					}
				}
				else {
					/* otherwise try to get objects from own library (won't work on library linked groups) */
					for (dw = part->dupliweights.first; dw; dw = dw->next) {
						dw->ob = newlibadr(fd, part->id.lib, dw->ob);
					}
				}
			}
			else {
				BLI_listbase_clear(&part->dupliweights);
			}
			
			if (part->boids) {
				BoidState *state = part->boids->states.first;
				BoidRule *rule;
				for (; state; state=state->next) {
					rule = state->rules.first;
					for (; rule; rule=rule->next) {
						switch (rule->type) {
							case eBoidRuleType_Goal:
							case eBoidRuleType_Avoid:
							{
								BoidRuleGoalAvoid *brga = (BoidRuleGoalAvoid*)rule;
								brga->ob = newlibadr(fd, part->id.lib, brga->ob);
								break;
							}
							case eBoidRuleType_FollowLeader:
							{
								BoidRuleFollowLeader *brfl = (BoidRuleFollowLeader*)rule;
								brfl->ob = newlibadr(fd, part->id.lib, brfl->ob);
								break;
							}
						}
					}
				}
			}

			for (int a = 0; a < MAX_MTEX; a++) {
				MTex *mtex= part->mtex[a];
				if (mtex) {
					mtex->tex = newlibadr_us(fd, part->id.lib, mtex->tex);
					mtex->object = newlibadr(fd, part->id.lib, mtex->object);
				}
			}
			
			part->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_partdeflect(PartDeflect *pd)
{
	if (pd) pd->rng = NULL;
}

static void direct_link_particlesettings(FileData *fd, ParticleSettings *part)
{
	int a;
	
	part->adt = newdataadr(fd, part->adt);
	part->pd = newdataadr(fd, part->pd);
	part->pd2 = newdataadr(fd, part->pd2);

	direct_link_animdata(fd, part->adt);
	direct_link_partdeflect(part->pd);
	direct_link_partdeflect(part->pd2);

	part->clumpcurve = newdataadr(fd, part->clumpcurve);
	if (part->clumpcurve)
		direct_link_curvemapping(fd, part->clumpcurve);
	part->roughcurve = newdataadr(fd, part->roughcurve);
	if (part->roughcurve)
		direct_link_curvemapping(fd, part->roughcurve);

	part->effector_weights = newdataadr(fd, part->effector_weights);
	if (!part->effector_weights)
		part->effector_weights = BKE_add_effector_weights(part->eff_group);

	link_list(fd, &part->dupliweights);

	part->boids = newdataadr(fd, part->boids);
	part->fluid = newdataadr(fd, part->fluid);

	if (part->boids) {
		BoidState *state;
		link_list(fd, &part->boids->states);
		
		for (state=part->boids->states.first; state; state=state->next) {
			link_list(fd, &state->rules);
			link_list(fd, &state->conditions);
			link_list(fd, &state->actions);
		}
	}
	for (a = 0; a < MAX_MTEX; a++) {
		part->mtex[a] = newdataadr(fd, part->mtex[a]);
	}

	/* Protect against integer overflow vulnerability. */
	CLAMP(part->trail_count, 1, 100000);
}

static void lib_link_particlesystems(FileData *fd, Object *ob, ID *id, ListBase *particles)
{
	ParticleSystem *psys, *psysnext;

	for (psys=particles->first; psys; psys=psysnext) {
		psysnext = psys->next;
		
		psys->part = newlibadr_us(fd, id->lib, psys->part);
		if (psys->part) {
			ParticleTarget *pt = psys->targets.first;
			
			for (; pt; pt=pt->next)
				pt->ob=newlibadr(fd, id->lib, pt->ob);
			
			psys->parent = newlibadr(fd, id->lib, psys->parent);
			psys->target_ob = newlibadr(fd, id->lib, psys->target_ob);
			
			if (psys->clmd) {
				/* XXX - from reading existing code this seems correct but intended usage of
				 * pointcache /w cloth should be added in 'ParticleSystem' - campbell */
				psys->clmd->point_cache = psys->pointcache;
				psys->clmd->ptcaches.first = psys->clmd->ptcaches.last= NULL;
				psys->clmd->coll_parms->group = newlibadr(fd, id->lib, psys->clmd->coll_parms->group);
				psys->clmd->modifier.error = NULL;
			}
		}
		else {
			/* particle modifier must be removed before particle system */
			ParticleSystemModifierData *psmd = psys_get_modifier(ob, psys);
			BLI_remlink(&ob->modifiers, psmd);
			modifier_free((ModifierData *)psmd);
			
			BLI_remlink(particles, psys);
			MEM_freeN(psys);
		}
	}
}
static void direct_link_particlesystems(FileData *fd, ListBase *particles)
{
	ParticleSystem *psys;
	ParticleData *pa;
	int a;
	
	for (psys=particles->first; psys; psys=psys->next) {
		psys->particles=newdataadr(fd, psys->particles);
		
		if (psys->particles && psys->particles->hair) {
			for (a=0, pa=psys->particles; a<psys->totpart; a++, pa++)
				pa->hair=newdataadr(fd, pa->hair);
		}
		
		if (psys->particles && psys->particles->keys) {
			for (a=0, pa=psys->particles; a<psys->totpart; a++, pa++) {
				pa->keys= NULL;
				pa->totkey= 0;
			}
			
			psys->flag &= ~PSYS_KEYED;
		}

		if (psys->particles && psys->particles->boid) {
			pa = psys->particles;
			pa->boid = newdataadr(fd, pa->boid);
			pa->boid->ground = NULL;  /* This is purely runtime data, but still can be an issue if left dangling. */
			for (a = 1, pa++; a < psys->totpart; a++, pa++) {
				pa->boid = (pa - 1)->boid + 1;
				pa->boid->ground = NULL;
			}
		}
		else if (psys->particles) {
			for (a=0, pa=psys->particles; a<psys->totpart; a++, pa++)
				pa->boid = NULL;
		}
		
		psys->fluid_springs = newdataadr(fd, psys->fluid_springs);
		
		psys->child = newdataadr(fd, psys->child);
		psys->effectors = NULL;
		
		link_list(fd, &psys->targets);
		
		psys->edit = NULL;
		psys->free_edit = NULL;
		psys->pathcache = NULL;
		psys->childcache = NULL;
		BLI_listbase_clear(&psys->pathcachebufs);
		BLI_listbase_clear(&psys->childcachebufs);
		psys->pdd = NULL;
		psys->renderdata = NULL;
		
		if (psys->clmd) {
			psys->clmd = newdataadr(fd, psys->clmd);
			psys->clmd->clothObject = NULL;
			psys->clmd->hairdata = NULL;
			
			psys->clmd->sim_parms= newdataadr(fd, psys->clmd->sim_parms);
			psys->clmd->coll_parms= newdataadr(fd, psys->clmd->coll_parms);
			
			if (psys->clmd->sim_parms) {
				psys->clmd->sim_parms->effector_weights = NULL;
				if (psys->clmd->sim_parms->presets > 10)
					psys->clmd->sim_parms->presets = 0;
			}
			
			psys->hair_in_dm = psys->hair_out_dm = NULL;
			psys->clmd->solver_result = NULL;
		}

		direct_link_pointcache_list(fd, &psys->ptcaches, &psys->pointcache, 0);
		if (psys->clmd) {
			psys->clmd->point_cache = psys->pointcache;
		}

		psys->tree = NULL;
		psys->bvhtree = NULL;
	}
	return;
}

/* ************ READ MESH ***************** */

static void lib_link_mtface(FileData *fd, Mesh *me, MTFace *mtface, int totface)
{
	MTFace *tf= mtface;
	int i;
	
	/* Add pseudo-references (not fake users!) to images used by texface. A
	 * little bogus; it would be better if each mesh consistently added one ref
	 * to each image it used. - z0r */
	for (i = 0; i < totface; i++, tf++) {
		tf->tpage = newlibadr_real_us(fd, me->id.lib, tf->tpage);
	}
}

static void lib_link_customdata_mtface(FileData *fd, Mesh *me, CustomData *fdata, int totface)
{
	int i;
	for (i = 0; i < fdata->totlayer; i++) {
		CustomDataLayer *layer = &fdata->layers[i];
		
		if (layer->type == CD_MTFACE)
			lib_link_mtface(fd, me, layer->data, totface);
	}

}

static void lib_link_customdata_mtpoly(FileData *fd, Mesh *me, CustomData *pdata, int totface)
{
	int i;

	for (i=0; i < pdata->totlayer; i++) {
		CustomDataLayer *layer = &pdata->layers[i];
		
		if (layer->type == CD_MTEXPOLY) {
			MTexPoly *tf= layer->data;
			int j;
			
			for (j = 0; j < totface; j++, tf++) {
				tf->tpage = newlibadr_real_us(fd, me->id.lib, tf->tpage);
			}
		}
	}
}

static void lib_link_mesh(FileData *fd, Main *main)
{
	Mesh *me;
	
	for (me = main->mesh.first; me; me = me->id.next) {
		if (me->id.tag & LIB_TAG_NEED_LINK) {
			int i;
			
			/* Link ID Properties -- and copy this comment EXACTLY for easy finding
			 * of library blocks that implement this.*/
			IDP_LibLinkProperty(me->id.properties, fd);
			lib_link_animdata(fd, &me->id, me->adt);
			
			/* this check added for python created meshes */
			if (me->mat) {
				for (i = 0; i < me->totcol; i++) {
					me->mat[i] = newlibadr_us(fd, me->id.lib, me->mat[i]);
				}
			}
			else {
				me->totcol = 0;
			}

			me->ipo = newlibadr_us(fd, me->id.lib, me->ipo); // XXX: deprecated: old anim sys
			me->key = newlibadr_us(fd, me->id.lib, me->key);
			me->texcomesh = newlibadr_us(fd, me->id.lib, me->texcomesh);
			
			lib_link_customdata_mtface(fd, me, &me->fdata, me->totface);
			lib_link_customdata_mtpoly(fd, me, &me->pdata, me->totpoly);
			if (me->mr && me->mr->levels.first) {
				lib_link_customdata_mtface(fd, me, &me->mr->fdata,
				                           ((MultiresLevel*)me->mr->levels.first)->totface);
			}
		}
	}

	/* convert texface options to material */
	convert_tface_mt(fd, main);

	for (me = main->mesh.first; me; me = me->id.next) {
		if (me->id.tag & LIB_TAG_NEED_LINK) {
			/*check if we need to convert mfaces to mpolys*/
			if (me->totface && !me->totpoly) {
				/* temporarily switch main so that reading from
				 * external CustomData works */
				Main *gmain = G.main;
				G.main = main;
				
				BKE_mesh_do_versions_convert_mfaces_to_mpolys(me);
				
				G.main = gmain;
			}

			/*
			 * Re-tessellate, even if the polys were just created from tessfaces, this
			 * is important because it:
			 *  - fill the CD_ORIGINDEX layer
			 *  - gives consistency of tessface between loading from a file and
			 *    converting an edited BMesh back into a mesh (i.e. it replaces
			 *    quad tessfaces in a loaded mesh immediately, instead of lazily
			 *    waiting until edit mode has been entered/exited, making it easier
			 *    to recognize problems that would otherwise only show up after edits).
			 */
#ifdef USE_TESSFACE_DEFAULT
			BKE_mesh_tessface_calc(me);
#else
			BKE_mesh_tessface_clear(me);
#endif

			me->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_dverts(FileData *fd, int count, MDeformVert *mdverts)
{
	int i;
	
	if (mdverts == NULL) {
		return;
	}
	
	for (i = count; i > 0; i--, mdverts++) {
		/*convert to vgroup allocation system*/
		MDeformWeight *dw;
		if (mdverts->dw && (dw = newdataadr(fd, mdverts->dw))) {
			const ssize_t dw_len = mdverts->totweight * sizeof(MDeformWeight);
			void *dw_tmp = MEM_mallocN(dw_len, "direct_link_dverts");
			memcpy(dw_tmp, dw, dw_len);
			mdverts->dw = dw_tmp;
			MEM_freeN(dw);
		}
		else {
			mdverts->dw = NULL;
			mdverts->totweight = 0;
		}
	}
}

static void direct_link_mdisps(FileData *fd, int count, MDisps *mdisps, int external)
{
	if (mdisps) {
		int i;
		
		for (i = 0; i < count; ++i) {
			mdisps[i].disps = newdataadr(fd, mdisps[i].disps);
			mdisps[i].hidden = newdataadr(fd, mdisps[i].hidden);
			
			if (mdisps[i].totdisp && !mdisps[i].level) {
				/* this calculation is only correct for loop mdisps;
				 * if loading pre-BMesh face mdisps this will be
				 * overwritten with the correct value in
				 * bm_corners_to_loops() */
				float gridsize = sqrtf(mdisps[i].totdisp);
				mdisps[i].level = (int)(logf(gridsize - 1.0f) / (float)M_LN2) + 1;
			}
			
			if ((fd->flags & FD_FLAGS_SWITCH_ENDIAN) && (mdisps[i].disps)) {
				/* DNA_struct_switch_endian doesn't do endian swap for (*disps)[] */
				/* this does swap for data written at write_mdisps() - readfile.c */
				BLI_endian_switch_float_array(*mdisps[i].disps, mdisps[i].totdisp * 3);
			}
			if (!external && !mdisps[i].disps)
				mdisps[i].totdisp = 0;
		}
	}
}

static void direct_link_grid_paint_mask(FileData *fd, int count, GridPaintMask *grid_paint_mask)
{
	if (grid_paint_mask) {
		int i;
		
		for (i = 0; i < count; ++i) {
			GridPaintMask *gpm = &grid_paint_mask[i];
			if (gpm->data)
				gpm->data = newdataadr(fd, gpm->data);
		}
	}
}

/*this isn't really a public api function, so prototyped here*/
static void direct_link_customdata(FileData *fd, CustomData *data, int count)
{
	int i = 0;
	
	data->layers = newdataadr(fd, data->layers);
	
	/* annoying workaround for bug [#31079] loading legacy files with
	 * no polygons _but_ have stale customdata */
	if (UNLIKELY(count == 0 && data->layers == NULL && data->totlayer != 0)) {
		CustomData_reset(data);
		return;
	}
	
	data->external = newdataadr(fd, data->external);
	
	while (i < data->totlayer) {
		CustomDataLayer *layer = &data->layers[i];
		
		if (layer->flag & CD_FLAG_EXTERNAL)
			layer->flag &= ~CD_FLAG_IN_MEMORY;

		layer->flag &= ~CD_FLAG_NOFREE;
		
		if (CustomData_verify_versions(data, i)) {
			layer->data = newdataadr(fd, layer->data);
			if (layer->type == CD_MDISPS)
				direct_link_mdisps(fd, count, layer->data, layer->flag & CD_FLAG_EXTERNAL);
			else if (layer->type == CD_GRID_PAINT_MASK)
				direct_link_grid_paint_mask(fd, count, layer->data);
			i++;
		}
	}
	
	CustomData_update_typemap(data);
}

static void direct_link_mesh(FileData *fd, Mesh *mesh)
{
	mesh->mat= newdataadr(fd, mesh->mat);
	test_pointer_array(fd, (void **)&mesh->mat);
	
	mesh->mvert = newdataadr(fd, mesh->mvert);
	mesh->medge = newdataadr(fd, mesh->medge);
	mesh->mface = newdataadr(fd, mesh->mface);
	mesh->mloop = newdataadr(fd, mesh->mloop);
	mesh->mpoly = newdataadr(fd, mesh->mpoly);
	mesh->tface = newdataadr(fd, mesh->tface);
	mesh->mtface = newdataadr(fd, mesh->mtface);
	mesh->mcol = newdataadr(fd, mesh->mcol);
	mesh->dvert = newdataadr(fd, mesh->dvert);
	mesh->mloopcol = newdataadr(fd, mesh->mloopcol);
	mesh->mloopuv = newdataadr(fd, mesh->mloopuv);
	mesh->mtpoly = newdataadr(fd, mesh->mtpoly);
	mesh->mselect = newdataadr(fd, mesh->mselect);
	
	/* animdata */
	mesh->adt = newdataadr(fd, mesh->adt);
	direct_link_animdata(fd, mesh->adt);
	
	/* normally direct_link_dverts should be called in direct_link_customdata,
	 * but for backwards compat in do_versions to work we do it here */
	direct_link_dverts(fd, mesh->totvert, mesh->dvert);
	
	direct_link_customdata(fd, &mesh->vdata, mesh->totvert);
	direct_link_customdata(fd, &mesh->edata, mesh->totedge);
	direct_link_customdata(fd, &mesh->fdata, mesh->totface);
	direct_link_customdata(fd, &mesh->ldata, mesh->totloop);
	direct_link_customdata(fd, &mesh->pdata, mesh->totpoly);

	mesh->bb = NULL;
	mesh->edit_btmesh = NULL;
	
	/* happens with old files */
	if (mesh->mselect == NULL) {
		mesh->totselect = 0;
	}

	if (mesh->mloopuv || mesh->mtpoly) {
		/* for now we have to ensure texpoly and mloopuv layers are aligned
		 * in the future we may allow non-aligned layers */
		BKE_mesh_cd_validate(mesh);
	}

	/* Multires data */
	mesh->mr= newdataadr(fd, mesh->mr);
	if (mesh->mr) {
		MultiresLevel *lvl;
		
		link_list(fd, &mesh->mr->levels);
		lvl = mesh->mr->levels.first;
		
		direct_link_customdata(fd, &mesh->mr->vdata, lvl->totvert);
		direct_link_dverts(fd, lvl->totvert, CustomData_get(&mesh->mr->vdata, 0, CD_MDEFORMVERT));
		direct_link_customdata(fd, &mesh->mr->fdata, lvl->totface);
		
		mesh->mr->edge_flags = newdataadr(fd, mesh->mr->edge_flags);
		mesh->mr->edge_creases = newdataadr(fd, mesh->mr->edge_creases);
		
		mesh->mr->verts = newdataadr(fd, mesh->mr->verts);
		
		/* If mesh has the same number of vertices as the
		 * highest multires level, load the current mesh verts
		 * into multires and discard the old data. Needed
		 * because some saved files either do not have a verts
		 * array, or the verts array contains out-of-date
		 * data. */
		if (mesh->totvert == ((MultiresLevel*)mesh->mr->levels.last)->totvert) {
			if (mesh->mr->verts)
				MEM_freeN(mesh->mr->verts);
			mesh->mr->verts = MEM_dupallocN(mesh->mvert);
		}
			
		for (; lvl; lvl = lvl->next) {
			lvl->verts = newdataadr(fd, lvl->verts);
			lvl->faces = newdataadr(fd, lvl->faces);
			lvl->edges = newdataadr(fd, lvl->edges);
			lvl->colfaces = newdataadr(fd, lvl->colfaces);
		}
	}

	/* if multires is present but has no valid vertex data,
	 * there's no way to recover it; silently remove multires */
	if (mesh->mr && !mesh->mr->verts) {
		multires_free(mesh->mr);
		mesh->mr = NULL;
	}
	
	if ((fd->flags & FD_FLAGS_SWITCH_ENDIAN) && mesh->tface) {
		TFace *tf = mesh->tface;
		int i;
		
		for (i = 0; i < mesh->totface; i++, tf++) {
			BLI_endian_switch_uint32_array(tf->col, 4);
		}
	}
}

/* ************ READ LATTICE ***************** */

static void lib_link_latt(FileData *fd, Main *main)
{
	for (Lattice *lt = main->latt.first; lt; lt = lt->id.next) {
		if (lt->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(lt->id.properties, fd);
			lib_link_animdata(fd, &lt->id, lt->adt);
			
			lt->ipo = newlibadr_us(fd, lt->id.lib, lt->ipo); // XXX deprecated - old animation system
			lt->key = newlibadr_us(fd, lt->id.lib, lt->key);
			
			lt->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_latt(FileData *fd, Lattice *lt)
{
	lt->def = newdataadr(fd, lt->def);
	
	lt->dvert = newdataadr(fd, lt->dvert);
	direct_link_dverts(fd, lt->pntsu*lt->pntsv*lt->pntsw, lt->dvert);
	
	lt->editlatt = NULL;
	
	lt->adt = newdataadr(fd, lt->adt);
	direct_link_animdata(fd, lt->adt);
}


/* ************ READ OBJECT ***************** */

static void lib_link_modifiers__linkModifiers(
        void *userData, Object *ob, ID **idpoin, int cb_flag)
{
	FileData *fd = userData;

	*idpoin = newlibadr(fd, ob->id.lib, *idpoin);
	if (*idpoin != NULL && (cb_flag & IDWALK_CB_USER) != 0) {
		id_us_plus_no_lib(*idpoin);
	}
}
static void lib_link_modifiers(FileData *fd, Object *ob)
{
	modifiers_foreachIDLink(ob, lib_link_modifiers__linkModifiers, fd);
}

static void lib_link_object(FileData *fd, Main *main)
{
	bool warn = false;

	for (Object *ob = main->object.first; ob; ob = ob->id.next) {
		if (ob->id.tag & LIB_TAG_NEED_LINK) {
			int a;

			IDP_LibLinkProperty(ob->id.properties, fd);
			lib_link_animdata(fd, &ob->id, ob->adt);
			
// XXX deprecated - old animation system <<<
			ob->ipo = newlibadr_us(fd, ob->id.lib, ob->ipo);
			ob->action = newlibadr_us(fd, ob->id.lib, ob->action);
// >>> XXX deprecated - old animation system

			ob->parent = newlibadr(fd, ob->id.lib, ob->parent);
			ob->track = newlibadr(fd, ob->id.lib, ob->track);
			ob->poselib = newlibadr_us(fd, ob->id.lib, ob->poselib);
			ob->dup_group = newlibadr_us(fd, ob->id.lib, ob->dup_group);
			
			ob->proxy = newlibadr_us(fd, ob->id.lib, ob->proxy);
			if (ob->proxy) {
				/* paranoia check, actually a proxy_from pointer should never be written... */
				if (ob->proxy->id.lib == NULL) {
					ob->proxy->proxy_from = NULL;
					ob->proxy = NULL;
					
					if (ob->id.lib)
						printf("Proxy lost from  object %s lib %s\n", ob->id.name + 2, ob->id.lib->name);
					else
						printf("Proxy lost from  object %s lib <NONE>\n", ob->id.name + 2);
				}
				else {
					/* this triggers object_update to always use a copy */
					ob->proxy->proxy_from = ob;
				}
			}
			ob->proxy_group = newlibadr(fd, ob->id.lib, ob->proxy_group);
			
			void *poin = ob->data;
			ob->data = newlibadr_us(fd, ob->id.lib, ob->data);
			
			if (ob->data == NULL && poin != NULL) {
				if (ob->id.lib)
					printf("Can't find obdata of %s lib %s\n", ob->id.name + 2, ob->id.lib->name);
				else
					printf("Object %s lost data.\n", ob->id.name + 2);
				
				ob->type = OB_EMPTY;
				warn = true;
				
				if (ob->pose) {
					/* we can't call #BKE_pose_free() here because of library linking
					 * freeing will recurse down into every pose constraints ID pointers
					 * which are not always valid, so for now free directly and suffer
					 * some leaked memory rather then crashing immediately
					 * while bad this _is_ an exceptional case - campbell */
#if 0
					BKE_pose_free(ob->pose);
#else
					MEM_freeN(ob->pose);
#endif
					ob->pose= NULL;
					ob->mode &= ~OB_MODE_POSE;
				}
			}
			for (a=0; a < ob->totcol; a++) 
				ob->mat[a] = newlibadr_us(fd, ob->id.lib, ob->mat[a]);
			
			/* When the object is local and the data is library its possible
			 * the material list size gets out of sync. [#22663] */
			if (ob->data && ob->id.lib != ((ID *)ob->data)->lib) {
				const short *totcol_data = give_totcolp(ob);
				/* Only expand so as not to loose any object materials that might be set. */
				if (totcol_data && (*totcol_data > ob->totcol)) {
					/* printf("'%s' %d -> %d\n", ob->id.name, ob->totcol, *totcol_data); */
					BKE_material_resize_object(main, ob, *totcol_data, false);
				}
			}
			
			ob->gpd = newlibadr_us(fd, ob->id.lib, ob->gpd);
			ob->duplilist = NULL;
			
			ob->id.tag &= ~LIB_TAG_NEED_LINK;
			/* if id.us==0 a new base will be created later on */
			
			/* WARNING! Also check expand_object(), should reflect the stuff below. */
			lib_link_pose(fd, main, ob, ob->pose);
			lib_link_constraints(fd, &ob->id, &ob->constraints);
			
// XXX deprecated - old animation system <<<
			lib_link_constraint_channels(fd, &ob->id, &ob->constraintChannels);
			lib_link_nlastrips(fd, &ob->id, &ob->nlastrips);
// >>> XXX deprecated - old animation system
			
			for (PartEff *paf = ob->effect.first; paf; paf = paf->next) {
				if (paf->type == EFF_PARTICLE) {
					paf->group = newlibadr_us(fd, ob->id.lib, paf->group);
				}
			}
			
			for (bSensor *sens = ob->sensors.first; sens; sens = sens->next) {
				for (a = 0; a < sens->totlinks; a++)
					sens->links[a] = newglobadr(fd, sens->links[a]);

				if (sens->type == SENS_MESSAGE) {
					bMessageSensor *ms = sens->data;
					ms->fromObject =
						newlibadr(fd, ob->id.lib, ms->fromObject);
				}
			}
			
			for (bController *cont = ob->controllers.first; cont; cont = cont->next) {
				for (a=0; a < cont->totlinks; a++)
					cont->links[a] = newglobadr(fd, cont->links[a]);
				
				if (cont->type == CONT_PYTHON) {
					bPythonCont *pc = cont->data;
					pc->text = newlibadr(fd, ob->id.lib, pc->text);
				}
				cont->slinks = NULL;
				cont->totslinks = 0;
			}
			
			for (bActuator *act = ob->actuators.first; act; act = act->next) {
				switch (act->type) {
					case ACT_SOUND:
					{
						bSoundActuator *sa = act->data;
						sa->sound = newlibadr_us(fd, ob->id.lib, sa->sound);
						break;
					}
					case ACT_GAME:
						/* bGameActuator *ga= act->data; */
						break;
					case ACT_CAMERA:
					{
						bCameraActuator *ca = act->data;
						ca->ob = newlibadr(fd, ob->id.lib, ca->ob);
						break;
					}
					/* leave this one, it's obsolete but necessary to read for conversion */
					case ACT_ADD_OBJECT:
					{
						bAddObjectActuator *eoa = act->data;
						if (eoa)
							eoa->ob = newlibadr(fd, ob->id.lib, eoa->ob);
						break;
					}
					case ACT_OBJECT:
					{
						bObjectActuator *oa = act->data;
						if (oa == NULL) {
							init_actuator(act);
						}
						else {
							oa->reference = newlibadr(fd, ob->id.lib, oa->reference);
						}
						break;
					}
					case ACT_EDIT_OBJECT:
					{
						bEditObjectActuator *eoa = act->data;
						if (eoa == NULL) {
							init_actuator(act);
						}
						else {
							eoa->ob = newlibadr(fd, ob->id.lib, eoa->ob);
							eoa->me = newlibadr(fd, ob->id.lib, eoa->me);
						}
						break;
					}
					case ACT_SCENE:
					{
						bSceneActuator *sa = act->data;
						sa->camera = newlibadr(fd, ob->id.lib, sa->camera);
						sa->scene = newlibadr(fd, ob->id.lib, sa->scene);
						break;
					}
					case ACT_ACTION:
					{
						bActionActuator *aa = act->data;
						aa->act = newlibadr_us(fd, ob->id.lib, aa->act);
						break;
					}
					case ACT_SHAPEACTION:
					{
						bActionActuator *aa = act->data;
						aa->act = newlibadr_us(fd, ob->id.lib, aa->act);
						break;
					}
					case ACT_PROPERTY:
					{
						bPropertyActuator *pa = act->data;
						pa->ob = newlibadr(fd, ob->id.lib, pa->ob);
						break;
					}
					case ACT_MESSAGE:
					{
						bMessageActuator *ma = act->data;
						ma->toObject = newlibadr(fd, ob->id.lib, ma->toObject);
						break;
					}
					case ACT_2DFILTER:
					{
						bTwoDFilterActuator *_2dfa = act->data;
						_2dfa->text = newlibadr(fd, ob->id.lib, _2dfa->text);
						break;
					}
					case ACT_PARENT:
					{
						bParentActuator *parenta = act->data;
						parenta->ob = newlibadr(fd, ob->id.lib, parenta->ob);
						break;
					}
					case ACT_STATE:
						/* bStateActuator *statea = act->data; */
						break;
					case ACT_ARMATURE:
					{
						bArmatureActuator *arma= act->data;
						arma->target = newlibadr(fd, ob->id.lib, arma->target);
						arma->subtarget = newlibadr(fd, ob->id.lib, arma->subtarget);
						break;
					}
					case ACT_STEERING:
					{
						bSteeringActuator *steeringa = act->data;
						steeringa->target = newlibadr(fd, ob->id.lib, steeringa->target);
						steeringa->navmesh = newlibadr(fd, ob->id.lib, steeringa->navmesh);
						break;
					}
					case ACT_MOUSE:
						/* bMouseActuator *moa = act->data; */
						break;
				}
			}
			
			{
				FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(ob, eModifierType_Fluidsim);
				
				if (fluidmd && fluidmd->fss)
					fluidmd->fss->ipo = newlibadr_us(fd, ob->id.lib, fluidmd->fss->ipo);  // XXX deprecated - old animation system
			}
			
			{
				SmokeModifierData *smd = (SmokeModifierData *)modifiers_findByType(ob, eModifierType_Smoke);
				
				if (smd && (smd->type == MOD_SMOKE_TYPE_DOMAIN) && smd->domain) {
					smd->domain->flags |= MOD_SMOKE_FILE_LOAD; /* flag for refreshing the simulation after loading */
				}
			}
			
			/* texture field */
			if (ob->pd)
				lib_link_partdeflect(fd, &ob->id, ob->pd);
			
			if (ob->soft) {
				ob->soft->collision_group = newlibadr(fd, ob->id.lib, ob->soft->collision_group);

				ob->soft->effector_weights->group = newlibadr(fd, ob->id.lib, ob->soft->effector_weights->group);
			}
			
			lib_link_particlesystems(fd, ob, &ob->id, &ob->particlesystem);
			lib_link_modifiers(fd, ob);

			if (ob->rigidbody_constraint) {
				ob->rigidbody_constraint->ob1 = newlibadr(fd, ob->id.lib, ob->rigidbody_constraint->ob1);
				ob->rigidbody_constraint->ob2 = newlibadr(fd, ob->id.lib, ob->rigidbody_constraint->ob2);
			}

			{
				LodLevel *level;
				for (level = ob->lodlevels.first; level; level = level->next) {
					level->source = newlibadr(fd, ob->id.lib, level->source);

					if (!level->source && level == ob->lodlevels.first)
						level->source = ob;
				}
			}
		}
	}
	
	if (warn) {
		BKE_report(fd->reports, RPT_WARNING, "Warning in console");
	}
}


static void direct_link_pose(FileData *fd, bPose *pose)
{
	bPoseChannel *pchan;

	if (!pose)
		return;

	link_list(fd, &pose->chanbase);
	link_list(fd, &pose->agroups);

	pose->chanhash = NULL;

	for (pchan = pose->chanbase.first; pchan; pchan=pchan->next) {
		pchan->bone = NULL;
		pchan->parent = newdataadr(fd, pchan->parent);
		pchan->child = newdataadr(fd, pchan->child);
		pchan->custom_tx = newdataadr(fd, pchan->custom_tx);
		
		pchan->bbone_prev = newdataadr(fd, pchan->bbone_prev);
		pchan->bbone_next = newdataadr(fd, pchan->bbone_next);
		
		direct_link_constraints(fd, &pchan->constraints);
		
		pchan->prop = newdataadr(fd, pchan->prop);
		IDP_DirectLinkGroup_OrFree(&pchan->prop, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
		
		pchan->mpath = newdataadr(fd, pchan->mpath);
		if (pchan->mpath)
			direct_link_motionpath(fd, pchan->mpath);
		
		BLI_listbase_clear(&pchan->iktree);
		BLI_listbase_clear(&pchan->siktree);
		
		/* in case this value changes in future, clamp else we get undefined behavior */
		CLAMP(pchan->rotmode, ROT_MODE_MIN, ROT_MODE_MAX);
	}
	pose->ikdata = NULL;
	if (pose->ikparam != NULL) {
		pose->ikparam = newdataadr(fd, pose->ikparam);
	}
}

static void direct_link_modifiers(FileData *fd, ListBase *lb)
{
	ModifierData *md;
	
	link_list(fd, lb);
	
	for (md=lb->first; md; md=md->next) {
		md->error = NULL;
		md->scene = NULL;
		
		/* if modifiers disappear, or for upward compatibility */
		if (NULL == modifierType_getInfo(md->type))
			md->type = eModifierType_None;
			
		if (md->type == eModifierType_Subsurf) {
			SubsurfModifierData *smd = (SubsurfModifierData *)md;
			
			smd->emCache = smd->mCache = NULL;
		}
		else if (md->type == eModifierType_Armature) {
			ArmatureModifierData *amd = (ArmatureModifierData *)md;
			
			amd->prevCos = NULL;
		}
		else if (md->type == eModifierType_Cloth) {
			ClothModifierData *clmd = (ClothModifierData *)md;
			
			clmd->clothObject = NULL;
			clmd->hairdata = NULL;
			
			clmd->sim_parms= newdataadr(fd, clmd->sim_parms);
			clmd->coll_parms= newdataadr(fd, clmd->coll_parms);
			
			direct_link_pointcache_list(fd, &clmd->ptcaches, &clmd->point_cache, 0);
			
			if (clmd->sim_parms) {
				if (clmd->sim_parms->presets > 10)
					clmd->sim_parms->presets = 0;
				
				clmd->sim_parms->reset = 0;
				
				clmd->sim_parms->effector_weights = newdataadr(fd, clmd->sim_parms->effector_weights);
				
				if (!clmd->sim_parms->effector_weights) {
					clmd->sim_parms->effector_weights = BKE_add_effector_weights(NULL);
				}
			}
			
			clmd->solver_result = NULL;
		}
		else if (md->type == eModifierType_Fluidsim) {
			FluidsimModifierData *fluidmd = (FluidsimModifierData *)md;
			
			fluidmd->fss = newdataadr(fd, fluidmd->fss);
			if (fluidmd->fss) {
				fluidmd->fss->fmd = fluidmd;
				fluidmd->fss->meshVelocities = NULL;
			}
		}
		else if (md->type == eModifierType_Smoke) {
			SmokeModifierData *smd = (SmokeModifierData *)md;
			
			if (smd->type == MOD_SMOKE_TYPE_DOMAIN) {
				smd->flow = NULL;
				smd->coll = NULL;
				smd->domain = newdataadr(fd, smd->domain);
				smd->domain->smd = smd;
				
				smd->domain->fluid = NULL;
				smd->domain->fluid_mutex = BLI_rw_mutex_alloc();
				smd->domain->wt = NULL;
				smd->domain->shadow = NULL;
				smd->domain->tex = NULL;
				smd->domain->tex_shadow = NULL;
				smd->domain->tex_wt = NULL;
				smd->domain->coba = newdataadr(fd, smd->domain->coba);
				
				smd->domain->effector_weights = newdataadr(fd, smd->domain->effector_weights);
				if (!smd->domain->effector_weights)
					smd->domain->effector_weights = BKE_add_effector_weights(NULL);
				
				direct_link_pointcache_list(fd, &(smd->domain->ptcaches[0]), &(smd->domain->point_cache[0]), 1);
				
				/* Smoke uses only one cache from now on, so store pointer convert */
				if (smd->domain->ptcaches[1].first || smd->domain->point_cache[1]) {
					if (smd->domain->point_cache[1]) {
						PointCache *cache = newdataadr(fd, smd->domain->point_cache[1]);
						if (cache->flag & PTCACHE_FAKE_SMOKE) {
							/* Smoke was already saved in "new format" and this cache is a fake one. */
						}
						else {
							printf("High resolution smoke cache not available due to pointcache update. Please reset the simulation.\n");
						}
						BKE_ptcache_free(cache);
					}
					BLI_listbase_clear(&smd->domain->ptcaches[1]);
					smd->domain->point_cache[1] = NULL;
				}
			}
			else if (smd->type == MOD_SMOKE_TYPE_FLOW) {
				smd->domain = NULL;
				smd->coll = NULL;
				smd->flow = newdataadr(fd, smd->flow);
				smd->flow->smd = smd;
				smd->flow->dm = NULL;
				smd->flow->verts_old = NULL;
				smd->flow->numverts = 0;
				smd->flow->psys = newdataadr(fd, smd->flow->psys);
			}
			else if (smd->type == MOD_SMOKE_TYPE_COLL) {
				smd->flow = NULL;
				smd->domain = NULL;
				smd->coll = newdataadr(fd, smd->coll);
				if (smd->coll) {
					smd->coll->smd = smd;
					smd->coll->verts_old = NULL;
					smd->coll->numverts = 0;
					smd->coll->dm = NULL;
				}
				else {
					smd->type = 0;
					smd->flow = NULL;
					smd->domain = NULL;
					smd->coll = NULL;
				}
			}
		}
		else if (md->type == eModifierType_DynamicPaint) {
			DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
			
			if (pmd->canvas) {
				pmd->canvas = newdataadr(fd, pmd->canvas);
				pmd->canvas->pmd = pmd;
				pmd->canvas->dm = NULL;
				pmd->canvas->flags &= ~MOD_DPAINT_BAKING; /* just in case */
				
				if (pmd->canvas->surfaces.first) {
					DynamicPaintSurface *surface;
					link_list(fd, &pmd->canvas->surfaces);
					
					for (surface=pmd->canvas->surfaces.first; surface; surface=surface->next) {
						surface->canvas = pmd->canvas;
						surface->data = NULL;
						direct_link_pointcache_list(fd, &(surface->ptcaches), &(surface->pointcache), 1);
						
						if (!(surface->effector_weights = newdataadr(fd, surface->effector_weights)))
							surface->effector_weights = BKE_add_effector_weights(NULL);
					}
				}
			}
			if (pmd->brush) {
				pmd->brush = newdataadr(fd, pmd->brush);
				pmd->brush->pmd = pmd;
				pmd->brush->psys = newdataadr(fd, pmd->brush->psys);
				pmd->brush->paint_ramp = newdataadr(fd, pmd->brush->paint_ramp);
				pmd->brush->vel_ramp = newdataadr(fd, pmd->brush->vel_ramp);
				pmd->brush->dm = NULL;
			}
		}
		else if (md->type == eModifierType_Collision) {
			CollisionModifierData *collmd = (CollisionModifierData *)md;
#if 0
			// TODO: CollisionModifier should use pointcache 
			// + have proper reset events before enabling this
			collmd->x = newdataadr(fd, collmd->x);
			collmd->xnew = newdataadr(fd, collmd->xnew);
			collmd->mfaces = newdataadr(fd, collmd->mfaces);
			
			collmd->current_x = MEM_calloc_arrayN(collmd->numverts, sizeof(MVert), "current_x");
			collmd->current_xnew = MEM_calloc_arrayN(collmd->numverts, sizeof(MVert), "current_xnew");
			collmd->current_v = MEM_calloc_arrayN(collmd->numverts, sizeof(MVert), "current_v");
#endif
			
			collmd->x = NULL;
			collmd->xnew = NULL;
			collmd->current_x = NULL;
			collmd->current_xnew = NULL;
			collmd->current_v = NULL;
			collmd->time_x = collmd->time_xnew = -1000;
			collmd->mvert_num = 0;
			collmd->tri_num = 0;
			collmd->is_static = false;
			collmd->bvhtree = NULL;
			collmd->tri = NULL;
			
		}
		else if (md->type == eModifierType_Surface) {
			SurfaceModifierData *surmd = (SurfaceModifierData *)md;
			
			surmd->dm = NULL;
			surmd->bvhtree = NULL;
			surmd->x = NULL;
			surmd->v = NULL;
			surmd->numverts = 0;
		}
		else if (md->type == eModifierType_Hook) {
			HookModifierData *hmd = (HookModifierData *)md;
			
			hmd->indexar = newdataadr(fd, hmd->indexar);
			if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
				BLI_endian_switch_int32_array(hmd->indexar, hmd->totindex);
			}

			hmd->curfalloff = newdataadr(fd, hmd->curfalloff);
			if (hmd->curfalloff) {
				direct_link_curvemapping(fd, hmd->curfalloff);
			}
		}
		else if (md->type == eModifierType_ParticleSystem) {
			ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;
			
			psmd->dm_final = NULL;
			psmd->dm_deformed = NULL;
			psmd->psys= newdataadr(fd, psmd->psys);
			psmd->flag &= ~eParticleSystemFlag_psys_updated;
			psmd->flag |= eParticleSystemFlag_file_loaded;
		}
		else if (md->type == eModifierType_Explode) {
			ExplodeModifierData *psmd = (ExplodeModifierData *)md;
			
			psmd->facepa = NULL;
		}
		else if (md->type == eModifierType_MeshDeform) {
			MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;
			
			mmd->bindinfluences = newdataadr(fd, mmd->bindinfluences);
			mmd->bindoffsets = newdataadr(fd, mmd->bindoffsets);
			mmd->bindcagecos = newdataadr(fd, mmd->bindcagecos);
			mmd->dyngrid = newdataadr(fd, mmd->dyngrid);
			mmd->dyninfluences = newdataadr(fd, mmd->dyninfluences);
			mmd->dynverts = newdataadr(fd, mmd->dynverts);
			
			mmd->bindweights = newdataadr(fd, mmd->bindweights);
			mmd->bindcos = newdataadr(fd, mmd->bindcos);
			
			if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
				if (mmd->bindoffsets)  BLI_endian_switch_int32_array(mmd->bindoffsets, mmd->totvert + 1);
				if (mmd->bindcagecos)  BLI_endian_switch_float_array(mmd->bindcagecos, mmd->totcagevert * 3);
				if (mmd->dynverts)     BLI_endian_switch_int32_array(mmd->dynverts, mmd->totvert);
				if (mmd->bindweights)  BLI_endian_switch_float_array(mmd->bindweights, mmd->totvert);
				if (mmd->bindcos)      BLI_endian_switch_float_array(mmd->bindcos, mmd->totcagevert * 3);
			}
		}
		else if (md->type == eModifierType_Ocean) {
			OceanModifierData *omd = (OceanModifierData *)md;
			omd->oceancache = NULL;
			omd->ocean = NULL;
			omd->refresh = (MOD_OCEAN_REFRESH_ADD|MOD_OCEAN_REFRESH_RESET|MOD_OCEAN_REFRESH_SIM);
		}
		else if (md->type == eModifierType_Warp) {
			WarpModifierData *tmd = (WarpModifierData *)md;
			
			tmd->curfalloff= newdataadr(fd, tmd->curfalloff);
			if (tmd->curfalloff)
				direct_link_curvemapping(fd, tmd->curfalloff);
		}
		else if (md->type == eModifierType_WeightVGEdit) {
			WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;
			
			wmd->cmap_curve = newdataadr(fd, wmd->cmap_curve);
			if (wmd->cmap_curve)
				direct_link_curvemapping(fd, wmd->cmap_curve);
		}
		else if (md->type == eModifierType_LaplacianDeform) {
			LaplacianDeformModifierData *lmd = (LaplacianDeformModifierData *)md;

			lmd->vertexco = newdataadr(fd, lmd->vertexco);
			if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
				BLI_endian_switch_float_array(lmd->vertexco, lmd->total_verts * 3);
			}
			lmd->cache_system = NULL;
		}
		else if (md->type == eModifierType_CorrectiveSmooth) {
			CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData*)md;

			if (csmd->bind_coords) {
				csmd->bind_coords = newdataadr(fd, csmd->bind_coords);
				if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
					BLI_endian_switch_float_array((float *)csmd->bind_coords, csmd->bind_coords_num * 3);
				}
			}

			/* runtime only */
			csmd->delta_cache = NULL;
			csmd->delta_cache_num = 0;
		}
		else if (md->type == eModifierType_MeshSequenceCache) {
			MeshSeqCacheModifierData *msmcd = (MeshSeqCacheModifierData *)md;
			msmcd->reader = NULL;
		}
		else if (md->type == eModifierType_SurfaceDeform) {
			SurfaceDeformModifierData *smd = (SurfaceDeformModifierData *)md;

			smd->verts = newdataadr(fd, smd->verts);

			if (smd->verts) {
				for (int i = 0; i < smd->numverts; i++) {
					smd->verts[i].binds = newdataadr(fd, smd->verts[i].binds);

					if (smd->verts[i].binds) {
						for (int j = 0; j < smd->verts[i].numbinds; j++) {
							smd->verts[i].binds[j].vert_inds = newdataadr(fd, smd->verts[i].binds[j].vert_inds);
							smd->verts[i].binds[j].vert_weights = newdataadr(fd, smd->verts[i].binds[j].vert_weights);

							if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
								if (smd->verts[i].binds[j].vert_inds)
									BLI_endian_switch_uint32_array(smd->verts[i].binds[j].vert_inds, smd->verts[i].binds[j].numverts);

								if (smd->verts[i].binds[j].vert_weights) {
									if (smd->verts[i].binds[j].mode == MOD_SDEF_MODE_CENTROID ||
									    smd->verts[i].binds[j].mode == MOD_SDEF_MODE_LOOPTRI)
										BLI_endian_switch_float_array(smd->verts[i].binds[j].vert_weights, 3);
									else
										BLI_endian_switch_float_array(smd->verts[i].binds[j].vert_weights, smd->verts[i].binds[j].numverts);
								}
							}
						}
					}
				}
			}
		}
	}
}

static void direct_link_object(FileData *fd, Object *ob)
{
	PartEff *paf;
	bProperty *prop;
	bSensor *sens;
	bController *cont;
	bActuator *act;
	
	/* weak weak... this was only meant as draw flag, now is used in give_base_to_objects too */
	ob->flag &= ~OB_FROMGROUP;

	/* This is a transient flag; clear in order to avoid unneeded object update pending from
	 * time when file was saved.
	 */
	ob->recalc = 0;

	/* XXX This should not be needed - but seems like it can happen in some cases, so for now play safe... */
	ob->proxy_from = NULL;

	/* loading saved files with editmode enabled works, but for undo we like
	 * to stay in object mode during undo presses so keep editmode disabled.
	 *
	 * Also when linking in a file don't allow edit and pose modes.
	 * See [#34776, #42780] for more information.
	 */
	if (fd->memfile || (ob->id.tag & (LIB_TAG_EXTERN | LIB_TAG_INDIRECT))) {
		ob->mode &= ~(OB_MODE_EDIT | OB_MODE_PARTICLE_EDIT);
		if (!fd->memfile) {
			ob->mode &= ~OB_MODE_POSE;
		}
	}
	
	ob->adt = newdataadr(fd, ob->adt);
	direct_link_animdata(fd, ob->adt);
	
	ob->pose = newdataadr(fd, ob->pose);
	direct_link_pose(fd, ob->pose);
	
	ob->mpath = newdataadr(fd, ob->mpath);
	if (ob->mpath)
		direct_link_motionpath(fd, ob->mpath);
	
	link_list(fd, &ob->defbase);
// XXX deprecated - old animation system <<<
	direct_link_nlastrips(fd, &ob->nlastrips);
	link_list(fd, &ob->constraintChannels);
// >>> XXX deprecated - old animation system
	
	ob->mat= newdataadr(fd, ob->mat);
	test_pointer_array(fd, (void **)&ob->mat);
	ob->matbits= newdataadr(fd, ob->matbits);
	
	/* do it here, below old data gets converted */
	direct_link_modifiers(fd, &ob->modifiers);
	
	link_list(fd, &ob->effect);
	paf= ob->effect.first;
	while (paf) {
		if (paf->type == EFF_PARTICLE) {
			paf->keys = NULL;
		}
		if (paf->type == EFF_WAVE) {
			WaveEff *wav = (WaveEff*) paf;
			PartEff *next = paf->next;
			WaveModifierData *wmd = (WaveModifierData*) modifier_new(eModifierType_Wave);
			
			wmd->damp = wav->damp;
			wmd->flag = wav->flag;
			wmd->height = wav->height;
			wmd->lifetime = wav->lifetime;
			wmd->narrow = wav->narrow;
			wmd->speed = wav->speed;
			wmd->startx = wav->startx;
			wmd->starty = wav->startx;
			wmd->timeoffs = wav->timeoffs;
			wmd->width = wav->width;
			
			BLI_addtail(&ob->modifiers, wmd);
			
			BLI_remlink(&ob->effect, paf);
			MEM_freeN(paf);
			
			paf = next;
			continue;
		}
		if (paf->type == EFF_BUILD) {
			BuildEff *baf = (BuildEff*) paf;
			PartEff *next = paf->next;
			BuildModifierData *bmd = (BuildModifierData*) modifier_new(eModifierType_Build);
			
			bmd->start = baf->sfra;
			bmd->length = baf->len;
			bmd->randomize = 0;
			bmd->seed = 1;
			
			BLI_addtail(&ob->modifiers, bmd);
			
			BLI_remlink(&ob->effect, paf);
			MEM_freeN(paf);
			
			paf = next;
			continue;
		}
		paf = paf->next;
	}
	
	ob->pd= newdataadr(fd, ob->pd);
	direct_link_partdeflect(ob->pd);
	ob->soft= newdataadr(fd, ob->soft);
	if (ob->soft) {
		SoftBody *sb = ob->soft;
		
		sb->bpoint = NULL;	// init pointers so it gets rebuilt nicely
		sb->bspring = NULL;
		sb->scratch = NULL;
		/* although not used anymore */
		/* still have to be loaded to be compatible with old files */
		sb->keys = newdataadr(fd, sb->keys);
		test_pointer_array(fd, (void **)&sb->keys);
		if (sb->keys) {
			int a;
			for (a = 0; a < sb->totkey; a++) {
				sb->keys[a] = newdataadr(fd, sb->keys[a]);
			}
		}
		
		sb->effector_weights = newdataadr(fd, sb->effector_weights);
		if (!sb->effector_weights)
			sb->effector_weights = BKE_add_effector_weights(NULL);
		
		direct_link_pointcache_list(fd, &sb->ptcaches, &sb->pointcache, 0);
	}
	ob->bsoft = newdataadr(fd, ob->bsoft);
	ob->fluidsimSettings= newdataadr(fd, ob->fluidsimSettings); /* NT */
	
	ob->rigidbody_object = newdataadr(fd, ob->rigidbody_object);
	if (ob->rigidbody_object) {
		RigidBodyOb *rbo = ob->rigidbody_object;
		
		/* must nullify the references to physics sim objects, since they no-longer exist 
		 * (and will need to be recalculated) 
		 */
		rbo->physics_object = NULL;
		rbo->physics_shape = NULL;
	}
	ob->rigidbody_constraint = newdataadr(fd, ob->rigidbody_constraint);
	if (ob->rigidbody_constraint)
		ob->rigidbody_constraint->physics_constraint = NULL;

	link_list(fd, &ob->particlesystem);
	direct_link_particlesystems(fd, &ob->particlesystem);
	
	link_list(fd, &ob->prop);
	for (prop = ob->prop.first; prop; prop = prop->next) {
		prop->poin = newdataadr(fd, prop->poin);
		if (prop->poin == NULL) 
			prop->poin = &prop->data;
	}

	link_list(fd, &ob->sensors);
	for (sens = ob->sensors.first; sens; sens = sens->next) {
		sens->data = newdataadr(fd, sens->data);
		sens->links = newdataadr(fd, sens->links);
		test_pointer_array(fd, (void **)&sens->links);
	}

	direct_link_constraints(fd, &ob->constraints);

	link_glob_list(fd, &ob->controllers);
	if (ob->init_state) {
		/* if a known first state is specified, set it so that the game will start ok */
		ob->state = ob->init_state;
	}
	else if (!ob->state) {
		ob->state = 1;
	}
	for (cont = ob->controllers.first; cont; cont = cont->next) {
		cont->data = newdataadr(fd, cont->data);
		cont->links = newdataadr(fd, cont->links);
		test_pointer_array(fd, (void **)&cont->links);
		if (cont->state_mask == 0)
			cont->state_mask = 1;
	}

	link_glob_list(fd, &ob->actuators);
	for (act = ob->actuators.first; act; act = act->next) {
		act->data = newdataadr(fd, act->data);
	}

	link_list(fd, &ob->hooks);
	while (ob->hooks.first) {
		ObHook *hook = ob->hooks.first;
		HookModifierData *hmd = (HookModifierData *)modifier_new(eModifierType_Hook);
		
		hook->indexar= newdataadr(fd, hook->indexar);
		if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
			BLI_endian_switch_int32_array(hook->indexar, hook->totindex);
		}
		
		/* Do conversion here because if we have loaded
		 * a hook we need to make sure it gets converted
		 * and freed, regardless of version.
		 */
		copy_v3_v3(hmd->cent, hook->cent);
		hmd->falloff = hook->falloff;
		hmd->force = hook->force;
		hmd->indexar = hook->indexar;
		hmd->object = hook->parent;
		memcpy(hmd->parentinv, hook->parentinv, sizeof(hmd->parentinv));
		hmd->totindex = hook->totindex;
		
		BLI_addhead(&ob->modifiers, hmd);
		BLI_remlink(&ob->hooks, hook);
		
		modifier_unique_name(&ob->modifiers, (ModifierData*)hmd);
		
		MEM_freeN(hook);
	}
	
	ob->iuser = newdataadr(fd, ob->iuser);
	if (ob->type == OB_EMPTY && ob->empty_drawtype == OB_EMPTY_IMAGE && !ob->iuser) {
		BKE_object_empty_draw_type_set(ob, ob->empty_drawtype);
	}

	ob->customdata_mask = 0;
	ob->bb = NULL;
	ob->derivedDeform = NULL;
	ob->derivedFinal = NULL;
	BLI_listbase_clear(&ob->gpulamp);
	link_list(fd, &ob->pc_ids);

	/* Runtime curve data  */
	ob->curve_cache = NULL;

	/* in case this value changes in future, clamp else we get undefined behavior */
	CLAMP(ob->rotmode, ROT_MODE_MIN, ROT_MODE_MAX);

	if (ob->sculpt) {
		ob->sculpt = MEM_callocN(sizeof(SculptSession), "reload sculpt session");
	}

	link_list(fd, &ob->lodlevels);
	ob->currentlod = ob->lodlevels.first;

	ob->preview = direct_link_preview_image(fd, ob->preview);
}

/* ************ READ SCENE ***************** */

/* patch for missing scene IDs, can't be in do-versions */
static void composite_patch(bNodeTree *ntree, Scene *scene)
{
	bNode *node;
	
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->id==NULL && node->type == CMP_NODE_R_LAYERS)
			node->id = &scene->id;
	}
}

static void link_paint(FileData *fd, Scene *sce, Paint *p)
{
	if (p) {
		p->brush = newlibadr_us(fd, sce->id.lib, p->brush);
		p->palette = newlibadr_us(fd, sce->id.lib, p->palette);
		p->paint_cursor = NULL;
	}
}

static void lib_link_sequence_modifiers(FileData *fd, Scene *scene, ListBase *lb)
{
	SequenceModifierData *smd;

	for (smd = lb->first; smd; smd = smd->next) {
		if (smd->mask_id)
			smd->mask_id = newlibadr_us(fd, scene->id.lib, smd->mask_id);
	}
}

/* check for cyclic set-scene,
 * libs can cause this case which is normally prevented, see (T#####) */
#define USE_SETSCENE_CHECK

#ifdef USE_SETSCENE_CHECK
/**
 * A version of #BKE_scene_validate_setscene with special checks for linked libs.
 */
static bool scene_validate_setscene__liblink(Scene *sce, const int totscene)
{
	Scene *sce_iter;
	int a;

	if (sce->set == NULL) return 1;

	for (a = 0, sce_iter = sce; sce_iter->set; sce_iter = sce_iter->set, a++) {
		if (sce_iter->id.tag & LIB_TAG_NEED_LINK) {
			return 1;
		}

		if (a > totscene) {
			sce->set = NULL;
			return 0;
		}
	}

	return 1;
}
#endif

static void lib_link_scene(FileData *fd, Main *main)
{
#ifdef USE_SETSCENE_CHECK
	bool need_check_set = false;
	int totscene = 0;
#endif
	
	for (Scene *sce = main->scene.first; sce; sce = sce->id.next) {
		if (sce->id.tag & LIB_TAG_NEED_LINK) {
			/* Link ID Properties -- and copy this comment EXACTLY for easy finding
			 * of library blocks that implement this.*/
			IDP_LibLinkProperty(sce->id.properties, fd);
			lib_link_animdata(fd, &sce->id, sce->adt);
			
			lib_link_keyingsets(fd, &sce->id, &sce->keyingsets);
			
			sce->camera = newlibadr(fd, sce->id.lib, sce->camera);
			sce->world = newlibadr_us(fd, sce->id.lib, sce->world);
			sce->set = newlibadr(fd, sce->id.lib, sce->set);
			sce->gpd = newlibadr_us(fd, sce->id.lib, sce->gpd);
			
			link_paint(fd, sce, &sce->toolsettings->sculpt->paint);
			link_paint(fd, sce, &sce->toolsettings->vpaint->paint);
			link_paint(fd, sce, &sce->toolsettings->wpaint->paint);
			link_paint(fd, sce, &sce->toolsettings->imapaint.paint);
			link_paint(fd, sce, &sce->toolsettings->uvsculpt->paint);

			if (sce->toolsettings->sculpt)
				sce->toolsettings->sculpt->gravity_object =
						newlibadr(fd, sce->id.lib, sce->toolsettings->sculpt->gravity_object);

			if (sce->toolsettings->imapaint.stencil)
				sce->toolsettings->imapaint.stencil =
				        newlibadr_us(fd, sce->id.lib, sce->toolsettings->imapaint.stencil);

			if (sce->toolsettings->imapaint.clone)
				sce->toolsettings->imapaint.clone =
				        newlibadr_us(fd, sce->id.lib, sce->toolsettings->imapaint.clone);

			if (sce->toolsettings->imapaint.canvas)
				sce->toolsettings->imapaint.canvas =
				        newlibadr_us(fd, sce->id.lib, sce->toolsettings->imapaint.canvas);
			
			sce->toolsettings->skgen_template = newlibadr(fd, sce->id.lib, sce->toolsettings->skgen_template);
			
			sce->toolsettings->particle.shape_object = newlibadr(fd, sce->id.lib, sce->toolsettings->particle.shape_object);
			
			for (Base *next, *base = sce->base.first; base; base = next) {
				next = base->next;
				
				base->object = newlibadr_us(fd, sce->id.lib, base->object);
				
				if (base->object == NULL) {
					blo_reportf_wrap(fd->reports, RPT_WARNING, TIP_("LIB: object lost from scene: '%s'"),
					                 sce->id.name + 2);
					BLI_remlink(&sce->base, base);
					if (base == sce->basact) sce->basact = NULL;
					MEM_freeN(base);
				}
			}
			
			Sequence *seq;
			SEQ_BEGIN (sce->ed, seq)
			{
				IDP_LibLinkProperty(seq->prop, fd);

				if (seq->ipo) seq->ipo = newlibadr_us(fd, sce->id.lib, seq->ipo);  // XXX deprecated - old animation system
				seq->scene_sound = NULL;
				if (seq->scene) {
					seq->scene = newlibadr(fd, sce->id.lib, seq->scene);
					if (seq->scene) {
						seq->scene_sound = BKE_sound_scene_add_scene_sound_defaults(sce, seq);
					}
				}
				if (seq->clip) {
					seq->clip = newlibadr_us(fd, sce->id.lib, seq->clip);
				}
				if (seq->mask) {
					seq->mask = newlibadr_us(fd, sce->id.lib, seq->mask);
				}
				if (seq->scene_camera) {
					seq->scene_camera = newlibadr(fd, sce->id.lib, seq->scene_camera);
				}
				if (seq->sound) {
					seq->scene_sound = NULL;
					if (seq->type == SEQ_TYPE_SOUND_HD) {
						seq->type = SEQ_TYPE_SOUND_RAM;
					}
					else {
						seq->sound = newlibadr(fd, sce->id.lib, seq->sound);
					}
					if (seq->sound) {
						id_us_plus_no_lib((ID *)seq->sound);
						seq->scene_sound = BKE_sound_add_scene_sound_defaults(sce, seq);
					}
				}
				BLI_listbase_clear(&seq->anims);

				lib_link_sequence_modifiers(fd, sce, &seq->modifiers);
			}
			SEQ_END

			for (TimeMarker *marker = sce->markers.first; marker; marker = marker->next) {
				if (marker->camera) {
					marker->camera = newlibadr(fd, sce->id.lib, marker->camera);
				}
			}
			
			BKE_sequencer_update_muting(sce->ed);
			BKE_sequencer_update_sound_bounds_all(sce);
			
			
			/* rigidbody world relies on it's linked groups */
			if (sce->rigidbody_world) {
				RigidBodyWorld *rbw = sce->rigidbody_world;
				if (rbw->group)
					rbw->group = newlibadr(fd, sce->id.lib, rbw->group);
				if (rbw->constraints)
					rbw->constraints = newlibadr(fd, sce->id.lib, rbw->constraints);
				if (rbw->effector_weights)
					rbw->effector_weights->group = newlibadr(fd, sce->id.lib, rbw->effector_weights->group);
			}
			
			if (sce->nodetree) {
				lib_link_ntree(fd, &sce->id, sce->nodetree);
				sce->nodetree->id.lib = sce->id.lib;
				composite_patch(sce->nodetree, sce);
			}
			
			for (SceneRenderLayer *srl = sce->r.layers.first; srl; srl = srl->next) {
				srl->mat_override = newlibadr_us(fd, sce->id.lib, srl->mat_override);
				srl->light_override = newlibadr_us(fd, sce->id.lib, srl->light_override);
				for (FreestyleModuleConfig *fmc = srl->freestyleConfig.modules.first; fmc; fmc = fmc->next) {
					fmc->script = newlibadr(fd, sce->id.lib, fmc->script);
				}
				for (FreestyleLineSet *fls = srl->freestyleConfig.linesets.first; fls; fls = fls->next) {
					fls->linestyle = newlibadr_us(fd, sce->id.lib, fls->linestyle);
					fls->group = newlibadr_us(fd, sce->id.lib, fls->group);
				}
			}
			/*Game Settings: Dome Warp Text*/
			sce->gm.dome.warptext = newlibadr(fd, sce->id.lib, sce->gm.dome.warptext);
			
			/* Motion Tracking */
			sce->clip = newlibadr_us(fd, sce->id.lib, sce->clip);

#ifdef USE_SETSCENE_CHECK
			if (sce->set != NULL) {
				/* link flag for scenes with set would be reset later,
				 * so this way we only check cyclic for newly linked scenes.
				 */
				need_check_set = true;
			}
			else {
				/* postpone un-setting the flag until we've checked the set-scene */
				sce->id.tag &= ~LIB_TAG_NEED_LINK;
			}
#else
			sce->id.tag &= ~LIB_TAG_NEED_LINK;
#endif
		}

#ifdef USE_SETSCENE_CHECK
		totscene++;
#endif
	}

#ifdef USE_SETSCENE_CHECK
	if (need_check_set) {
		for (Scene *sce = main->scene.first; sce; sce = sce->id.next) {
			if (sce->id.tag & LIB_TAG_NEED_LINK) {
				sce->id.tag &= ~LIB_TAG_NEED_LINK;
				if (!scene_validate_setscene__liblink(sce, totscene)) {
					printf("Found cyclic background scene when linking %s\n", sce->id.name + 2);
				}
			}
		}
	}
#endif
}

#undef USE_SETSCENE_CHECK


static void link_recurs_seq(FileData *fd, ListBase *lb)
{
	Sequence *seq;
	
	link_list(fd, lb);
	
	for (seq = lb->first; seq; seq = seq->next) {
		if (seq->seqbase.first)
			link_recurs_seq(fd, &seq->seqbase);
	}
}

static void direct_link_paint(FileData *fd, Paint *p)
{
	if (p->num_input_samples < 1)
		p->num_input_samples = 1;

	p->cavity_curve = newdataadr(fd, p->cavity_curve);
	if (p->cavity_curve)
		direct_link_curvemapping(fd, p->cavity_curve);
	else
		BKE_paint_cavity_curve_preset(p, CURVE_PRESET_LINE);
}

static void direct_link_paint_helper(FileData *fd, Paint **paint)
{
	/* TODO. is this needed */
	(*paint) = newdataadr(fd, (*paint));

	if (*paint) {
		direct_link_paint(fd, *paint);
	}
}

static void direct_link_sequence_modifiers(FileData *fd, ListBase *lb)
{
	SequenceModifierData *smd;

	link_list(fd, lb);

	for (smd = lb->first; smd; smd = smd->next) {
		if (smd->mask_sequence)
			smd->mask_sequence = newdataadr(fd, smd->mask_sequence);

		if (smd->type == seqModifierType_Curves) {
			CurvesModifierData *cmd = (CurvesModifierData *) smd;

			direct_link_curvemapping(fd, &cmd->curve_mapping);
		}
		else if (smd->type == seqModifierType_HueCorrect) {
			HueCorrectModifierData *hcmd = (HueCorrectModifierData *) smd;

			direct_link_curvemapping(fd, &hcmd->curve_mapping);
		}
	}
}

static void direct_link_view_settings(FileData *fd, ColorManagedViewSettings *view_settings)
{
	view_settings->curve_mapping = newdataadr(fd, view_settings->curve_mapping);

	if (view_settings->curve_mapping)
		direct_link_curvemapping(fd, view_settings->curve_mapping);
}

static void direct_link_scene(FileData *fd, Scene *sce)
{
	Editing *ed;
	Sequence *seq;
	MetaStack *ms;
	RigidBodyWorld *rbw;
	SceneRenderLayer *srl;
	
	sce->theDag = NULL;
	sce->depsgraph = NULL;
	sce->obedit = NULL;
	sce->stats = NULL;
	sce->fps_info = NULL;
	sce->customdata_mask_modal = 0;
	sce->lay_updated = 0;
	
	BKE_sound_create_scene(sce);
	
	/* set users to one by default, not in lib-link, this will increase it for compo nodes */
	id_us_ensure_real(&sce->id);
	
	link_list(fd, &(sce->base));
	
	sce->adt = newdataadr(fd, sce->adt);
	direct_link_animdata(fd, sce->adt);
	
	link_list(fd, &sce->keyingsets);
	direct_link_keyingsets(fd, &sce->keyingsets);
	
	sce->basact = newdataadr(fd, sce->basact);
	
	sce->toolsettings= newdataadr(fd, sce->toolsettings);
	if (sce->toolsettings) {
		direct_link_paint_helper(fd, (Paint**)&sce->toolsettings->sculpt);
		direct_link_paint_helper(fd, (Paint**)&sce->toolsettings->vpaint);
		direct_link_paint_helper(fd, (Paint**)&sce->toolsettings->wpaint);
		direct_link_paint_helper(fd, (Paint**)&sce->toolsettings->uvsculpt);
		
		direct_link_paint(fd, &sce->toolsettings->imapaint.paint);

		sce->toolsettings->imapaint.paintcursor = NULL;
		sce->toolsettings->particle.paintcursor = NULL;
		sce->toolsettings->particle.scene = NULL;
		sce->toolsettings->particle.object = NULL;
		sce->toolsettings->gp_sculpt.paintcursor = NULL;

		/* in rare cases this is needed, see [#33806] */
		if (sce->toolsettings->vpaint) {
			sce->toolsettings->vpaint->vpaint_prev = NULL;
			sce->toolsettings->vpaint->tot = 0;
		}
		if (sce->toolsettings->wpaint) {
			sce->toolsettings->wpaint->wpaint_prev = NULL;
			sce->toolsettings->wpaint->tot = 0;
		}
		
		/* relink grease pencil drawing brushes */
		link_list(fd, &sce->toolsettings->gp_brushes);
		for (bGPDbrush *brush = sce->toolsettings->gp_brushes.first; brush; brush = brush->next) {
			brush->cur_sensitivity = newdataadr(fd, brush->cur_sensitivity);
			if (brush->cur_sensitivity) {
				direct_link_curvemapping(fd, brush->cur_sensitivity);
			}
			brush->cur_strength = newdataadr(fd, brush->cur_strength);
			if (brush->cur_strength) {
				direct_link_curvemapping(fd, brush->cur_strength);
			}
			brush->cur_jitter = newdataadr(fd, brush->cur_jitter);
			if (brush->cur_jitter) {
				direct_link_curvemapping(fd, brush->cur_jitter);
			}
		}
		
		/* relink grease pencil interpolation curves */
		sce->toolsettings->gp_interpolate.custom_ipo = newdataadr(fd, sce->toolsettings->gp_interpolate.custom_ipo);
		if (sce->toolsettings->gp_interpolate.custom_ipo) {
			direct_link_curvemapping(fd, sce->toolsettings->gp_interpolate.custom_ipo);
		}
	}

	if (sce->ed) {
		ListBase *old_seqbasep = &sce->ed->seqbase;
		
		ed = sce->ed = newdataadr(fd, sce->ed);
		
		ed->act_seq = newdataadr(fd, ed->act_seq);
		
		/* recursive link sequences, lb will be correctly initialized */
		link_recurs_seq(fd, &ed->seqbase);
		
		SEQ_BEGIN (ed, seq)
		{
			seq->seq1= newdataadr(fd, seq->seq1);
			seq->seq2= newdataadr(fd, seq->seq2);
			seq->seq3= newdataadr(fd, seq->seq3);
			
			/* a patch: after introduction of effects with 3 input strips */
			if (seq->seq3 == NULL) seq->seq3 = seq->seq2;
			
			seq->effectdata = newdataadr(fd, seq->effectdata);
			seq->stereo3d_format = newdataadr(fd, seq->stereo3d_format);
			
			if (seq->type & SEQ_TYPE_EFFECT)
				seq->flag |= SEQ_EFFECT_NOT_LOADED;
			
			if (seq->type == SEQ_TYPE_SPEED) {
				SpeedControlVars *s = seq->effectdata;
				s->frameMap = NULL;
			}

			seq->prop = newdataadr(fd, seq->prop);
			IDP_DirectLinkGroup_OrFree(&seq->prop, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);

			seq->strip = newdataadr(fd, seq->strip);
			if (seq->strip && seq->strip->done==0) {
				seq->strip->done = true;
				
				if (ELEM(seq->type, SEQ_TYPE_IMAGE, SEQ_TYPE_MOVIE, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD)) {
					seq->strip->stripdata = newdataadr(fd, seq->strip->stripdata);
				}
				else {
					seq->strip->stripdata = NULL;
				}
				if (seq->flag & SEQ_USE_CROP) {
					seq->strip->crop = newdataadr(
						fd, seq->strip->crop);
				}
				else {
					seq->strip->crop = NULL;
				}
				if (seq->flag & SEQ_USE_TRANSFORM) {
					seq->strip->transform = newdataadr(
						fd, seq->strip->transform);
				}
				else {
					seq->strip->transform = NULL;
				}
				if (seq->flag & SEQ_USE_PROXY) {
					seq->strip->proxy = newdataadr(fd, seq->strip->proxy);
					if (seq->strip->proxy) {
						seq->strip->proxy->anim = NULL;
					}
					else {
						BKE_sequencer_proxy_set(seq, true);
					}
				}
				else {
					seq->strip->proxy = NULL;
				}

				/* need to load color balance to it could be converted to modifier */
				seq->strip->color_balance = newdataadr(fd, seq->strip->color_balance);
			}

			direct_link_sequence_modifiers(fd, &seq->modifiers);
		}
		SEQ_END
		
		/* link metastack, slight abuse of structs here, have to restore pointer to internal part in struct */
		{
			Sequence temp;
			void *poin;
			intptr_t offset;
			
			offset = ((intptr_t)&(temp.seqbase)) - ((intptr_t)&temp);
			
			/* root pointer */
			if (ed->seqbasep == old_seqbasep) {
				ed->seqbasep = &ed->seqbase;
			}
			else {
				poin = POINTER_OFFSET(ed->seqbasep, -offset);
				
				poin = newdataadr(fd, poin);
				if (poin)
					ed->seqbasep = (ListBase *)POINTER_OFFSET(poin, offset);
				else
					ed->seqbasep = &ed->seqbase;
			}
			/* stack */
			link_list(fd, &(ed->metastack));
			
			for (ms = ed->metastack.first; ms; ms= ms->next) {
				ms->parseq = newdataadr(fd, ms->parseq);
				
				if (ms->oldbasep == old_seqbasep)
					ms->oldbasep= &ed->seqbase;
				else {
					poin = POINTER_OFFSET(ms->oldbasep, -offset);
					poin = newdataadr(fd, poin);
					if (poin) 
						ms->oldbasep = (ListBase *)POINTER_OFFSET(poin, offset);
					else 
						ms->oldbasep = &ed->seqbase;
				}
			}
		}
	}
	
	sce->r.avicodecdata = newdataadr(fd, sce->r.avicodecdata);
	if (sce->r.avicodecdata) {
		sce->r.avicodecdata->lpFormat = newdataadr(fd, sce->r.avicodecdata->lpFormat);
		sce->r.avicodecdata->lpParms = newdataadr(fd, sce->r.avicodecdata->lpParms);
	}
	
	sce->r.qtcodecdata = newdataadr(fd, sce->r.qtcodecdata);
	if (sce->r.qtcodecdata) {
		sce->r.qtcodecdata->cdParms = newdataadr(fd, sce->r.qtcodecdata->cdParms);
	}
	if (sce->r.ffcodecdata.properties) {
		sce->r.ffcodecdata.properties = newdataadr(fd, sce->r.ffcodecdata.properties);
		IDP_DirectLinkGroup_OrFree(&sce->r.ffcodecdata.properties, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
	}
	
	link_list(fd, &(sce->markers));
	link_list(fd, &(sce->transform_spaces));
	link_list(fd, &(sce->r.layers));
	link_list(fd, &(sce->r.views));


	for (srl = sce->r.layers.first; srl; srl = srl->next) {
		srl->prop = newdataadr(fd, srl->prop);
		IDP_DirectLinkGroup_OrFree(&srl->prop, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
	}
	for (srl = sce->r.layers.first; srl; srl = srl->next) {
		link_list(fd, &(srl->freestyleConfig.modules));
	}
	for (srl = sce->r.layers.first; srl; srl = srl->next) {
		link_list(fd, &(srl->freestyleConfig.linesets));
	}
	
	sce->nodetree = newdataadr(fd, sce->nodetree);
	if (sce->nodetree) {
		direct_link_id(fd, &sce->nodetree->id);
		direct_link_nodetree(fd, sce->nodetree);
	}

	direct_link_view_settings(fd, &sce->view_settings);
	
	sce->rigidbody_world = newdataadr(fd, sce->rigidbody_world);
	rbw = sce->rigidbody_world;
	if (rbw) {
		/* must nullify the reference to physics sim object, since it no-longer exist 
		 * (and will need to be recalculated) 
		 */
		rbw->physics_world = NULL;
		rbw->objects = NULL;
		rbw->numbodies = 0;

		/* set effector weights */
		rbw->effector_weights = newdataadr(fd, rbw->effector_weights);
		if (!rbw->effector_weights)
			rbw->effector_weights = BKE_add_effector_weights(NULL);

		/* link cache */
		direct_link_pointcache_list(fd, &rbw->ptcaches, &rbw->pointcache, false);
		/* make sure simulation starts from the beginning after loading file */
		if (rbw->pointcache) {
			rbw->ltime = (float)rbw->pointcache->startframe;
		}
	}

	sce->preview = direct_link_preview_image(fd, sce->preview);

	direct_link_curvemapping(fd, &sce->r.mblur_shutter_curve);
}

/* ************ READ WM ***************** */

static void direct_link_windowmanager(FileData *fd, wmWindowManager *wm)
{
	wmWindow *win;
	
	id_us_ensure_real(&wm->id);
	link_list(fd, &wm->windows);
	
	for (win = wm->windows.first; win; win = win->next) {
		win->ghostwin = NULL;
		win->eventstate = NULL;
		win->curswin = NULL;
		win->tweak = NULL;
#ifdef WIN32
		win->ime_data = NULL;
#endif
		
		BLI_listbase_clear(&win->queue);
		BLI_listbase_clear(&win->handlers);
		BLI_listbase_clear(&win->modalhandlers);
		BLI_listbase_clear(&win->subwindows);
		BLI_listbase_clear(&win->gesture);
		BLI_listbase_clear(&win->drawdata);
		
		win->drawmethod = -1;
		win->drawfail = 0;
		win->active = 0;

		win->cursor       = 0;
		win->lastcursor   = 0;
		win->modalcursor  = 0;
		win->grabcursor   = 0;
		win->addmousemove = true;
		win->multisamples = 0;
		win->stereo3d_format = newdataadr(fd, win->stereo3d_format);

		/* multiview always fallback to anaglyph at file opening
		 * otherwise quadbuffer saved files can break Blender */
		if (win->stereo3d_format) {
			win->stereo3d_format->display_mode = S3D_DISPLAY_ANAGLYPH;
		}
	}
	
	BLI_listbase_clear(&wm->timers);
	BLI_listbase_clear(&wm->operators);
	BLI_listbase_clear(&wm->paintcursors);
	BLI_listbase_clear(&wm->queue);
	BKE_reports_init(&wm->reports, RPT_STORE);
	
	BLI_listbase_clear(&wm->keyconfigs);
	wm->defaultconf = NULL;
	wm->addonconf = NULL;
	wm->userconf = NULL;
	
	BLI_listbase_clear(&wm->jobs);
	BLI_listbase_clear(&wm->drags);
	
	wm->windrawable = NULL;
	wm->winactive = NULL;
	wm->initialized = 0;
	wm->op_undo_depth = 0;
	wm->is_interface_locked = 0;
}

static void lib_link_windowmanager(FileData *fd, Main *main)
{
	wmWindowManager *wm;
	wmWindow *win;
	
	for (wm = main->wm.first; wm; wm = wm->id.next) {
		if (wm->id.tag & LIB_TAG_NEED_LINK) {
			/* Note: WM IDProperties are never written to file, hence no need to read/link them here. */
			for (win = wm->windows.first; win; win = win->next) {
				win->screen = newlibadr(fd, NULL, win->screen);
			}
			
			wm->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

/* ****************** READ GREASE PENCIL ***************** */

/* relink's grease pencil data's refs */
static void lib_link_gpencil(FileData *fd, Main *main)
{
	for (bGPdata *gpd = main->gpencil.first; gpd; gpd = gpd->id.next) {
		if (gpd->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(gpd->id.properties, fd);
			lib_link_animdata(fd, &gpd->id, gpd->adt);

			gpd->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

/* relinks grease-pencil data - used for direct_link and old file linkage */
static void direct_link_gpencil(FileData *fd, bGPdata *gpd)
{
	bGPDlayer *gpl;
	bGPDframe *gpf;
	bGPDstroke *gps;
	bGPDpalette *palette;

	/* we must firstly have some grease-pencil data to link! */
	if (gpd == NULL)
		return;
	
	/* relink animdata */
	gpd->adt = newdataadr(fd, gpd->adt);
	direct_link_animdata(fd, gpd->adt);

	/* relink palettes */
	link_list(fd, &gpd->palettes);
	for (palette = gpd->palettes.first; palette; palette = palette->next) {
		link_list(fd, &palette->colors);
	}

	/* relink layers */
	link_list(fd, &gpd->layers);
	
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		/* parent */
		gpl->parent = newlibadr(fd, gpd->id.lib, gpl->parent);
		/* relink frames */
		link_list(fd, &gpl->frames);
		gpl->actframe = newdataadr(fd, gpl->actframe);
		
		for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
			/* relink strokes (and their points) */
			link_list(fd, &gpf->strokes);
			
			for (gps = gpf->strokes.first; gps; gps = gps->next) {
				gps->points = newdataadr(fd, gps->points);
				
				/* the triangulation is not saved, so need to be recalculated */
				gps->triangles = NULL;
				gps->tot_triangles = 0;
				gps->flag |= GP_STROKE_RECALC_CACHES;
				/* the color pointer is not saved, so need to be recalculated using the color name */
				gps->palcolor = NULL;
				gps->flag |= GP_STROKE_RECALC_COLOR;
			}
		}
	}
}

/* ****************** READ SCREEN ***************** */

/* note: file read without screens option G_FILE_NO_UI; 
 * check lib pointers in call below */
static void lib_link_screen(FileData *fd, Main *main)
{
	for (bScreen *sc = main->screen.first; sc; sc = sc->id.next) {
		if (sc->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(sc->id.properties, fd);
			id_us_ensure_real(&sc->id);

			sc->scene = newlibadr(fd, sc->id.lib, sc->scene);

			/* this should not happen, but apparently it does somehow. Until we figure out the cause,
			 * just assign first available scene */
			if (!sc->scene)
				sc->scene = main->scene.first;

			sc->animtimer = NULL; /* saved in rare cases */
			sc->scrubbing = false;
			
			for (ScrArea *sa = sc->areabase.first; sa; sa = sa->next) {
				sa->full = newlibadr(fd, sc->id.lib, sa->full);
				
				for (SpaceLink *sl = sa->spacedata.first; sl; sl= sl->next) {
					switch (sl->spacetype) {
						case SPACE_VIEW3D:
						{
							View3D *v3d = (View3D*) sl;
							BGpic *bgpic = NULL;

							v3d->camera= newlibadr(fd, sc->id.lib, v3d->camera);
							v3d->ob_centre= newlibadr(fd, sc->id.lib, v3d->ob_centre);

							/* should be do_versions but not easy adding into the listbase */
							if (v3d->bgpic) {
								v3d->bgpic = newlibadr(fd, sc->id.lib, v3d->bgpic);
								BLI_addtail(&v3d->bgpicbase, bgpic);
								v3d->bgpic = NULL;
							}

							for (bgpic = v3d->bgpicbase.first; bgpic; bgpic = bgpic->next) {
								bgpic->ima = newlibadr_us(fd, sc->id.lib, bgpic->ima);
								bgpic->clip = newlibadr_us(fd, sc->id.lib, bgpic->clip);
							}
							if (v3d->localvd) {
								v3d->localvd->camera = newlibadr(fd, sc->id.lib, v3d->localvd->camera);
							}
							break;
						}
						case SPACE_IPO:
						{
							SpaceIpo *sipo = (SpaceIpo *)sl;
							bDopeSheet *ads = sipo->ads;

							if (ads) {
								ads->source = newlibadr(fd, sc->id.lib, ads->source);
								ads->filter_grp = newlibadr(fd, sc->id.lib, ads->filter_grp);
							}
							break;
						}
						case SPACE_BUTS:
						{
							SpaceButs *sbuts = (SpaceButs *)sl;
							sbuts->pinid = newlibadr(fd, sc->id.lib, sbuts->pinid);
							if (sbuts->pinid == NULL) {
								sbuts->flag &= ~SB_PIN_CONTEXT;
							}
							break;
						}
						case SPACE_FILE:
							break;
						case SPACE_ACTION:
						{
							SpaceAction *saction = (SpaceAction *)sl;
							bDopeSheet *ads = &saction->ads;

							if (ads) {
								ads->source = newlibadr(fd, sc->id.lib, ads->source);
								ads->filter_grp = newlibadr(fd, sc->id.lib, ads->filter_grp);
							}

							saction->action = newlibadr(fd, sc->id.lib, saction->action);
							break;
						}
						case SPACE_IMAGE:
						{
							SpaceImage *sima = (SpaceImage *)sl;

							sima->image = newlibadr_real_us(fd, sc->id.lib, sima->image);
							sima->mask_info.mask = newlibadr_real_us(fd, sc->id.lib, sima->mask_info.mask);

							/* NOTE: pre-2.5, this was local data not lib data, but now we need this as lib data
							 * so fingers crossed this works fine!
							 */
							sima->gpd = newlibadr_us(fd, sc->id.lib, sima->gpd);
							break;
						}
						case SPACE_SEQ:
						{
							SpaceSeq *sseq = (SpaceSeq *)sl;

							/* NOTE: pre-2.5, this was local data not lib data, but now we need this as lib data
							 * so fingers crossed this works fine!
							 */
							sseq->gpd = newlibadr_us(fd, sc->id.lib, sseq->gpd);
							break;
						}
						case SPACE_NLA:
						{
							SpaceNla *snla= (SpaceNla *)sl;
							bDopeSheet *ads= snla->ads;

							if (ads) {
								ads->source = newlibadr(fd, sc->id.lib, ads->source);
								ads->filter_grp = newlibadr(fd, sc->id.lib, ads->filter_grp);
							}
							break;
						}
						case SPACE_TEXT:
						{
							SpaceText *st= (SpaceText *)sl;

							st->text= newlibadr(fd, sc->id.lib, st->text);
							break;
						}
						case SPACE_SCRIPT:
						{
							SpaceScript *scpt = (SpaceScript *)sl;
							/*scpt->script = NULL; - 2.45 set to null, better re-run the script */
							if (scpt->script) {
								scpt->script = newlibadr(fd, sc->id.lib, scpt->script);
								if (scpt->script) {
									SCRIPT_SET_NULL(scpt->script);
								}
							}
							break;
						}
						case SPACE_OUTLINER:
						{
							SpaceOops *so= (SpaceOops *)sl;
							so->search_tse.id = newlibadr(fd, NULL, so->search_tse.id);

							if (so->treestore) {
								TreeStoreElem *tselem;
								BLI_mempool_iter iter;

								BLI_mempool_iternew(so->treestore, &iter);
								while ((tselem = BLI_mempool_iterstep(&iter))) {
									tselem->id = newlibadr(fd, NULL, tselem->id);
								}
								if (so->treehash) {
									/* rebuild hash table, because it depends on ids too */
									so->storeflag |= SO_TREESTORE_REBUILD;
								}
							}
							break;
						}
						case SPACE_NODE:
						{
							SpaceNode *snode = (SpaceNode *)sl;
							bNodeTreePath *path, *path_next;
							bNodeTree *ntree;

							/* node tree can be stored locally in id too, link this first */
							snode->id = newlibadr(fd, sc->id.lib, snode->id);
							snode->from = newlibadr(fd, sc->id.lib, snode->from);

							ntree = snode->id ? ntreeFromID(snode->id) : NULL;
							snode->nodetree = ntree ? ntree : newlibadr_us(fd, sc->id.lib, snode->nodetree);

							for (path = snode->treepath.first; path; path = path->next) {
								if (path == snode->treepath.first) {
									/* first nodetree in path is same as snode->nodetree */
									path->nodetree = snode->nodetree;
								}
								else
									path->nodetree = newlibadr_us(fd, sc->id.lib, path->nodetree);

								if (!path->nodetree)
									break;
							}

							/* remaining path entries are invalid, remove */
							for (; path; path = path_next) {
								path_next = path->next;

								BLI_remlink(&snode->treepath, path);
								MEM_freeN(path);
							}

							/* edittree is just the last in the path,
							 * set this directly since the path may have been shortened above */
							if (snode->treepath.last) {
								path = snode->treepath.last;
								snode->edittree = path->nodetree;
							}
							else {
								snode->edittree = NULL;
							}
							break;
						}
						case SPACE_CLIP:
						{
							SpaceClip *sclip = (SpaceClip *)sl;

							sclip->clip = newlibadr_real_us(fd, sc->id.lib, sclip->clip);
							sclip->mask_info.mask = newlibadr_real_us(fd, sc->id.lib, sclip->mask_info.mask);
							break;
						}
						case SPACE_LOGIC:
						{
							SpaceLogic *slogic = (SpaceLogic *)sl;

							slogic->gpd = newlibadr_us(fd, sc->id.lib, slogic->gpd);
							break;
						}
						default:
							break;
					}
				}
			}
			sc->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

/* how to handle user count on pointer restore */
typedef enum ePointerUserMode {
	USER_IGNORE = 0,  /* ignore user count */
	USER_REAL   = 1,  /* ensure at least one real user (fake user ignored) */
} ePointerUserMode;

static void restore_pointer_user(ID *id, ID *newid, ePointerUserMode user)
{
	BLI_assert(STREQ(newid->name + 2, id->name + 2));
	BLI_assert(newid->lib == id->lib);
	UNUSED_VARS_NDEBUG(id);

	if (user == USER_REAL) {
		id_us_ensure_real(newid);
	}
}

#ifndef USE_GHASH_RESTORE_POINTER
/**
 * A version of #restore_pointer_by_name that performs a full search (slow!).
 * Use only for limited lookups, when the overhead of
 * creating a #IDNameLib_Map for a single lookup isn't worthwhile.
 */
static void *restore_pointer_by_name_main(Main *mainp, ID *id, ePointerUserMode user)
{
	if (id) {
		ListBase *lb = which_libbase(mainp, GS(id->name));
		if (lb) {  /* there's still risk of checking corrupt mem (freed Ids in oops) */
			ID *idn = lb->first;
			for (; idn; idn = idn->next) {
				if (STREQ(idn->name + 2, id->name + 2)) {
					if (idn->lib == id->lib) {
						restore_pointer_user(id, idn, user);
						break;
					}
				}
			}
			return idn;
		}
	}
	return NULL;
}
#endif

/**
 * Only for undo files, or to restore a screen after reading without UI...
 *
 * \param user:
 * - USER_IGNORE: no usercount change
 * - USER_REAL: ensure a real user (even if a fake one is set)
 * \param id_map: lookup table, use when performing many lookups.
 * this could be made an optional argument (falling back to a full lookup),
 * however at the moment it's always available.
 */
static void *restore_pointer_by_name(struct IDNameLib_Map *id_map, ID *id, ePointerUserMode user)
{
#ifdef USE_GHASH_RESTORE_POINTER
	if (id) {
		/* use fast lookup when available */
		ID *idn = BKE_main_idmap_lookup_id(id_map, id);
		if (idn) {
			restore_pointer_user(id, idn, user);
		}
		return idn;
	}
	return NULL;
#else
	Main *mainp = BKE_main_idmap_main_get(id_map);
	return restore_pointer_by_name_main(mainp, id, user);
#endif
}

static void lib_link_seq_clipboard_pt_restore(ID *id, struct IDNameLib_Map *id_map)
{
	if (id) {
		/* clipboard must ensure this */
		BLI_assert(id->newid != NULL);
		id->newid = restore_pointer_by_name(id_map, id->newid, USER_REAL);
	}
}
static int lib_link_seq_clipboard_cb(Sequence *seq, void *arg_pt)
{
	struct IDNameLib_Map *id_map = arg_pt;

	lib_link_seq_clipboard_pt_restore((ID *)seq->scene, id_map);
	lib_link_seq_clipboard_pt_restore((ID *)seq->scene_camera, id_map);
	lib_link_seq_clipboard_pt_restore((ID *)seq->clip, id_map);
	lib_link_seq_clipboard_pt_restore((ID *)seq->mask, id_map);
	lib_link_seq_clipboard_pt_restore((ID *)seq->sound, id_map);
	return 1;
}

static void lib_link_clipboard_restore(struct IDNameLib_Map *id_map)
{
	/* update IDs stored in sequencer clipboard */
	BKE_sequencer_base_recursive_apply(&seqbase_clipboard, lib_link_seq_clipboard_cb, id_map);
}

/* called from kernel/blender.c */
/* used to link a file (without UI) to the current UI */
/* note that it assumes the old pointers in UI are still valid, so old Main is not freed */
void blo_lib_link_screen_restore(Main *newmain, bScreen *curscreen, Scene *curscene)
{
	wmWindow *win;
	wmWindowManager *wm;
	bScreen *sc;
	ScrArea *sa;

	struct IDNameLib_Map *id_map = BKE_main_idmap_create(newmain);

	/* first windowmanager */
	for (wm = newmain->wm.first; wm; wm = wm->id.next) {
		for (win= wm->windows.first; win; win= win->next) {
			win->screen = restore_pointer_by_name(id_map, (ID *)win->screen, USER_REAL);
			
			if (win->screen == NULL)
				win->screen = curscreen;
			
			win->screen->winid = win->winid;
		}
	}
	
	
	for (sc = newmain->screen.first; sc; sc = sc->id.next) {
		Scene *oldscene = sc->scene;
		
		sc->scene= restore_pointer_by_name(id_map, (ID *)sc->scene, USER_REAL);
		if (sc->scene == NULL)
			sc->scene = curscene;
		
		/* keep cursor location through undo */
		copy_v3_v3(sc->scene->cursor, oldscene->cursor);
		
		for (sa = sc->areabase.first; sa; sa = sa->next) {
			SpaceLink *sl;
			
			for (sl = sa->spacedata.first; sl; sl = sl->next) {
				if (sl->spacetype == SPACE_VIEW3D) {
					View3D *v3d = (View3D *)sl;
					BGpic *bgpic;
					ARegion *ar;
					
					if (v3d->scenelock)
						v3d->camera = NULL; /* always get from scene */
					else
						v3d->camera = restore_pointer_by_name(id_map, (ID *)v3d->camera, USER_REAL);
					if (v3d->camera == NULL)
						v3d->camera = sc->scene->camera;
					v3d->ob_centre = restore_pointer_by_name(id_map, (ID *)v3d->ob_centre, USER_REAL);
					
					for (bgpic= v3d->bgpicbase.first; bgpic; bgpic= bgpic->next) {
						if ((bgpic->ima = restore_pointer_by_name(id_map, (ID *)bgpic->ima, USER_IGNORE))) {
							id_us_plus((ID *)bgpic->ima);
						}
						if ((bgpic->clip = restore_pointer_by_name(id_map, (ID *)bgpic->clip, USER_IGNORE))) {
							id_us_plus((ID *)bgpic->clip);
						}
					}
					if (v3d->localvd) {
						/*Base *base;*/
						
						v3d->localvd->camera = sc->scene->camera;
						
						/* localview can become invalid during undo/redo steps, so we exit it when no could be found */
#if 0					/* XXX  regionlocalview ? */
						for (base= sc->scene->base.first; base; base= base->next) {
							if (base->lay & v3d->lay) break;
						}
						if (base==NULL) {
							v3d->lay= v3d->localvd->lay;
							v3d->layact= v3d->localvd->layact;
							MEM_freeN(v3d->localvd); 
							v3d->localvd= NULL;
						}
#endif
					}
					else if (v3d->scenelock) {
						v3d->lay = sc->scene->lay;
					}
					
					/* not very nice, but could help */
					if ((v3d->layact & v3d->lay) == 0) v3d->layact = v3d->lay;

					/* free render engines for now */
					for (ar = sa->regionbase.first; ar; ar = ar->next) {
						RegionView3D *rv3d= ar->regiondata;
						
						if (rv3d && rv3d->render_engine) {
							RE_engine_free(rv3d->render_engine);
							rv3d->render_engine = NULL;
						}
					}
				}
				else if (sl->spacetype == SPACE_IPO) {
					SpaceIpo *sipo = (SpaceIpo *)sl;
					bDopeSheet *ads = sipo->ads;
					
					if (ads) {
						ads->source = restore_pointer_by_name(id_map, (ID *)ads->source, USER_REAL);
						
						if (ads->filter_grp)
							ads->filter_grp = restore_pointer_by_name(id_map, (ID *)ads->filter_grp, USER_IGNORE);
					}
					
					/* force recalc of list of channels (i.e. includes calculating F-Curve colors)
					 * thus preventing the "black curves" problem post-undo
					 */
					sipo->flag |= SIPO_TEMP_NEEDCHANSYNC;
				}
				else if (sl->spacetype == SPACE_BUTS) {
					SpaceButs *sbuts = (SpaceButs *)sl;
					sbuts->pinid = restore_pointer_by_name(id_map, sbuts->pinid, USER_IGNORE);
					if (sbuts->pinid == NULL) {
						sbuts->flag &= ~SB_PIN_CONTEXT;
					}

					/* TODO: restore path pointers: T40046
					 * (complicated because this contains data pointers too, not just ID)*/
					MEM_SAFE_FREE(sbuts->path);
				}
				else if (sl->spacetype == SPACE_FILE) {
					SpaceFile *sfile = (SpaceFile *)sl;
					sfile->op = NULL;
					sfile->previews_timer = NULL;
				}
				else if (sl->spacetype == SPACE_ACTION) {
					SpaceAction *saction = (SpaceAction *)sl;
					
					saction->action = restore_pointer_by_name(id_map, (ID *)saction->action, USER_REAL);
					saction->ads.source = restore_pointer_by_name(id_map, (ID *)saction->ads.source, USER_REAL);
					
					if (saction->ads.filter_grp)
						saction->ads.filter_grp = restore_pointer_by_name(id_map, (ID *)saction->ads.filter_grp, USER_IGNORE);
						
					
					/* force recalc of list of channels, potentially updating the active action 
					 * while we're at it (as it can only be updated that way) [#28962] 
					 */
					saction->flag |= SACTION_TEMP_NEEDCHANSYNC;
				}
				else if (sl->spacetype == SPACE_IMAGE) {
					SpaceImage *sima = (SpaceImage *)sl;
					
					sima->image = restore_pointer_by_name(id_map, (ID *)sima->image, USER_REAL);
					
					/* this will be freed, not worth attempting to find same scene,
					 * since it gets initialized later */
					sima->iuser.scene = NULL;
					
#if 0
					/* Those are allocated and freed by space code, no need to handle them here. */
					MEM_SAFE_FREE(sima->scopes.waveform_1);
					MEM_SAFE_FREE(sima->scopes.waveform_2);
					MEM_SAFE_FREE(sima->scopes.waveform_3);
					MEM_SAFE_FREE(sima->scopes.vecscope);
#endif
					sima->scopes.ok = 0;
					
					/* NOTE: pre-2.5, this was local data not lib data, but now we need this as lib data
					 * so assume that here we're doing for undo only...
					 */
					sima->gpd = restore_pointer_by_name(id_map, (ID *)sima->gpd, USER_REAL);
					sima->mask_info.mask = restore_pointer_by_name(id_map, (ID *)sima->mask_info.mask, USER_REAL);
				}
				else if (sl->spacetype == SPACE_SEQ) {
					SpaceSeq *sseq = (SpaceSeq *)sl;
					
					/* NOTE: pre-2.5, this was local data not lib data, but now we need this as lib data
					 * so assume that here we're doing for undo only...
					 */
					sseq->gpd = restore_pointer_by_name(id_map, (ID *)sseq->gpd, USER_REAL);
				}
				else if (sl->spacetype == SPACE_NLA) {
					SpaceNla *snla = (SpaceNla *)sl;
					bDopeSheet *ads = snla->ads;
					
					if (ads) {
						ads->source = restore_pointer_by_name(id_map, (ID *)ads->source, USER_REAL);
						
						if (ads->filter_grp)
							ads->filter_grp = restore_pointer_by_name(id_map, (ID *)ads->filter_grp, USER_IGNORE);
					}
				}
				else if (sl->spacetype == SPACE_TEXT) {
					SpaceText *st = (SpaceText *)sl;
					
					st->text = restore_pointer_by_name(id_map, (ID *)st->text, USER_REAL);
					if (st->text == NULL) st->text = newmain->text.first;
				}
				else if (sl->spacetype == SPACE_SCRIPT) {
					SpaceScript *scpt = (SpaceScript *)sl;
					
					scpt->script = restore_pointer_by_name(id_map, (ID *)scpt->script, USER_REAL);
					
					/*sc->script = NULL; - 2.45 set to null, better re-run the script */
					if (scpt->script) {
						SCRIPT_SET_NULL(scpt->script);
					}
				}
				else if (sl->spacetype == SPACE_OUTLINER) {
					SpaceOops *so= (SpaceOops *)sl;
					
					so->search_tse.id = restore_pointer_by_name(id_map, so->search_tse.id, USER_IGNORE);
					
					if (so->treestore) {
						TreeStoreElem *tselem;
						BLI_mempool_iter iter;

						BLI_mempool_iternew(so->treestore, &iter);
						while ((tselem = BLI_mempool_iterstep(&iter))) {
							/* Do not try to restore pointers to drivers/sequence/etc., can crash in undo case! */
							if (TSE_IS_REAL_ID(tselem)) {
								tselem->id = restore_pointer_by_name(id_map, tselem->id, USER_IGNORE);
							}
							else {
								tselem->id = NULL;
							}
						}
						if (so->treehash) {
							/* rebuild hash table, because it depends on ids too */
							so->storeflag |= SO_TREESTORE_REBUILD;
						}
					}
				}
				else if (sl->spacetype == SPACE_NODE) {
					SpaceNode *snode= (SpaceNode *)sl;
					bNodeTreePath *path, *path_next;
					bNodeTree *ntree;
					
					/* node tree can be stored locally in id too, link this first */
					snode->id = restore_pointer_by_name(id_map, snode->id, USER_REAL);
					snode->from = restore_pointer_by_name(id_map, snode->from, USER_IGNORE);
					
					ntree = snode->id ? ntreeFromID(snode->id) : NULL;
					snode->nodetree = ntree ? ntree : restore_pointer_by_name(id_map, (ID *)snode->nodetree, USER_REAL);
					
					for (path = snode->treepath.first; path; path = path->next) {
						if (path == snode->treepath.first) {
							/* first nodetree in path is same as snode->nodetree */
							path->nodetree = snode->nodetree;
						}
						else
							path->nodetree= restore_pointer_by_name(id_map, (ID*)path->nodetree, USER_REAL);
						
						if (!path->nodetree)
							break;
					}
					
					/* remaining path entries are invalid, remove */
					for (; path; path = path_next) {
						path_next = path->next;
						
						BLI_remlink(&snode->treepath, path);
						MEM_freeN(path);
					}
					
					/* edittree is just the last in the path,
					 * set this directly since the path may have been shortened above */
					if (snode->treepath.last) {
						path = snode->treepath.last;
						snode->edittree = path->nodetree;
					}
					else
						snode->edittree = NULL;
				}
				else if (sl->spacetype == SPACE_CLIP) {
					SpaceClip *sclip = (SpaceClip *)sl;
					
					sclip->clip = restore_pointer_by_name(id_map, (ID *)sclip->clip, USER_REAL);
					sclip->mask_info.mask = restore_pointer_by_name(id_map, (ID *)sclip->mask_info.mask, USER_REAL);
					
					sclip->scopes.ok = 0;
				}
				else if (sl->spacetype == SPACE_LOGIC) {
					SpaceLogic *slogic = (SpaceLogic *)sl;
					
					slogic->gpd = restore_pointer_by_name(id_map, (ID *)slogic->gpd, USER_REAL);
				}
			}
		}
	}

	/* update IDs stored in all possible clipboards */
	lib_link_clipboard_restore(id_map);

	BKE_main_idmap_destroy(id_map);
}

static void direct_link_region(FileData *fd, ARegion *ar, int spacetype)
{
	Panel *pa;
	uiList *ui_list;

	link_list(fd, &ar->panels);

	for (pa = ar->panels.first; pa; pa = pa->next) {
		pa->paneltab = newdataadr(fd, pa->paneltab);
		pa->runtime_flag = 0;
		pa->activedata = NULL;
		pa->type = NULL;
	}

	link_list(fd, &ar->panels_category_active);

	link_list(fd, &ar->ui_lists);

	for (ui_list = ar->ui_lists.first; ui_list; ui_list = ui_list->next) {
		ui_list->type = NULL;
		ui_list->dyn_data = NULL;
		ui_list->properties = newdataadr(fd, ui_list->properties);
		IDP_DirectLinkGroup_OrFree(&ui_list->properties, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
	}

	link_list(fd, &ar->ui_previews);

	if (spacetype == SPACE_EMPTY) {
		/* unkown space type, don't leak regiondata */
		ar->regiondata = NULL;
	}
	else {
		ar->regiondata = newdataadr(fd, ar->regiondata);
		if (ar->regiondata) {
			if (spacetype == SPACE_VIEW3D) {
				RegionView3D *rv3d = ar->regiondata;

				rv3d->localvd = newdataadr(fd, rv3d->localvd);
				rv3d->clipbb = newdataadr(fd, rv3d->clipbb);

				rv3d->depths = NULL;
				rv3d->gpuoffscreen = NULL;
				rv3d->render_engine = NULL;
				rv3d->sms = NULL;
				rv3d->smooth_timer = NULL;
				rv3d->compositor = NULL;
			}
		}
	}
	
	ar->v2d.tab_offset = NULL;
	ar->v2d.tab_num = 0;
	ar->v2d.tab_cur = 0;
	ar->v2d.sms = NULL;
	BLI_listbase_clear(&ar->panels_category);
	BLI_listbase_clear(&ar->handlers);
	BLI_listbase_clear(&ar->uiblocks);
	ar->headerstr = NULL;
	ar->swinid = 0;
	ar->type = NULL;
	ar->swap = 0;
	ar->do_draw = 0;
	ar->regiontimer = NULL;
	memset(&ar->drawrct, 0, sizeof(ar->drawrct));
}

/* for the saved 2.50 files without regiondata */
/* and as patch for 2.48 and older */
void blo_do_versions_view3d_split_250(View3D *v3d, ListBase *regions)
{
	ARegion *ar;
	
	for (ar = regions->first; ar; ar = ar->next) {
		if (ar->regiontype==RGN_TYPE_WINDOW && ar->regiondata==NULL) {
			RegionView3D *rv3d;
			
			rv3d = ar->regiondata = MEM_callocN(sizeof(RegionView3D), "region v3d patch");
			rv3d->persp = (char)v3d->persp;
			rv3d->view = (char)v3d->view;
			rv3d->dist = v3d->dist;
			copy_v3_v3(rv3d->ofs, v3d->ofs);
			copy_qt_qt(rv3d->viewquat, v3d->viewquat);
		}
	}
	
	/* this was not initialized correct always */
	if (v3d->twtype == 0)
		v3d->twtype = V3D_MANIP_TRANSLATE;
	if (v3d->gridsubdiv == 0)
		v3d->gridsubdiv = 10;
}

static bool direct_link_screen(FileData *fd, bScreen *sc)
{
	ScrArea *sa;
	ScrVert *sv;
	ScrEdge *se;
	bool wrong_id = false;
	
	link_list(fd, &(sc->vertbase));
	link_list(fd, &(sc->edgebase));
	link_list(fd, &(sc->areabase));
	sc->regionbase.first = sc->regionbase.last= NULL;
	sc->context = NULL;
	
	sc->mainwin = sc->subwinactive= 0;	/* indices */
	sc->swap = 0;

	/* edges */
	for (se = sc->edgebase.first; se; se = se->next) {
		se->v1 = newdataadr(fd, se->v1);
		se->v2 = newdataadr(fd, se->v2);
		if ((intptr_t)se->v1 > (intptr_t)se->v2) {
			sv = se->v1;
			se->v1 = se->v2;
			se->v2 = sv;
		}
		
		if (se->v1 == NULL) {
			printf("Error reading Screen %s... removing it.\n", sc->id.name+2);
			BLI_remlink(&sc->edgebase, se);
			wrong_id = true;
		}
	}
	
	/* areas */
	for (sa = sc->areabase.first; sa; sa = sa->next) {
		SpaceLink *sl;
		ARegion *ar;
		
		link_list(fd, &(sa->spacedata));
		link_list(fd, &(sa->regionbase));
		
		BLI_listbase_clear(&sa->handlers);
		sa->type = NULL;	/* spacetype callbacks */
		sa->region_active_win = -1;

		/* if we do not have the spacetype registered (game player), we cannot
		 * free it, so don't allocate any new memory for such spacetypes. */
		if (!BKE_spacetype_exists(sa->spacetype))
			sa->spacetype = SPACE_EMPTY;
		
		for (ar = sa->regionbase.first; ar; ar = ar->next)
			direct_link_region(fd, ar, sa->spacetype);
		
		/* accident can happen when read/save new file with older version */
		/* 2.50: we now always add spacedata for info */
		if (sa->spacedata.first==NULL) {
			SpaceInfo *sinfo= MEM_callocN(sizeof(SpaceInfo), "spaceinfo");
			sa->spacetype= sinfo->spacetype= SPACE_INFO;
			BLI_addtail(&sa->spacedata, sinfo);
		}
		/* add local view3d too */
		else if (sa->spacetype == SPACE_VIEW3D)
			blo_do_versions_view3d_split_250(sa->spacedata.first, &sa->regionbase);

		/* incase we set above */
		sa->butspacetype = sa->spacetype;

		for (sl = sa->spacedata.first; sl; sl = sl->next) {
			link_list(fd, &(sl->regionbase));

			/* if we do not have the spacetype registered (game player), we cannot
			 * free it, so don't allocate any new memory for such spacetypes. */
			if (!BKE_spacetype_exists(sl->spacetype))
				sl->spacetype = SPACE_EMPTY;

			for (ar = sl->regionbase.first; ar; ar = ar->next)
				direct_link_region(fd, ar, sl->spacetype);
			
			if (sl->spacetype == SPACE_VIEW3D) {
				View3D *v3d= (View3D*) sl;
				BGpic *bgpic;
				
				v3d->flag |= V3D_INVALID_BACKBUF;
				
				link_list(fd, &v3d->bgpicbase);
				
				/* should be do_versions except this doesnt fit well there */
				if (v3d->bgpic) {
					bgpic = newdataadr(fd, v3d->bgpic);
					BLI_addtail(&v3d->bgpicbase, bgpic);
					v3d->bgpic = NULL;
				}
			
				for (bgpic = v3d->bgpicbase.first; bgpic; bgpic = bgpic->next)
					bgpic->iuser.ok = 1;
				
				if (v3d->gpd) {
					v3d->gpd = newdataadr(fd, v3d->gpd);
					direct_link_gpencil(fd, v3d->gpd);
				}
				v3d->localvd = newdataadr(fd, v3d->localvd);
				BLI_listbase_clear(&v3d->afterdraw_transp);
				BLI_listbase_clear(&v3d->afterdraw_xray);
				BLI_listbase_clear(&v3d->afterdraw_xraytransp);
				v3d->properties_storage = NULL;
				v3d->defmaterial = NULL;
				
				/* render can be quite heavy, set to solid on load */
				if (v3d->drawtype == OB_RENDER)
					v3d->drawtype = OB_SOLID;
				v3d->prev_drawtype = OB_SOLID;

				if (v3d->fx_settings.dof)
					v3d->fx_settings.dof = newdataadr(fd, v3d->fx_settings.dof);
				if (v3d->fx_settings.ssao)
					v3d->fx_settings.ssao = newdataadr(fd, v3d->fx_settings.ssao);
				
				blo_do_versions_view3d_split_250(v3d, &sl->regionbase);
			}
			else if (sl->spacetype == SPACE_IPO) {
				SpaceIpo *sipo = (SpaceIpo *)sl;
				
				sipo->ads = newdataadr(fd, sipo->ads);
				BLI_listbase_clear(&sipo->ghostCurves);
			}
			else if (sl->spacetype == SPACE_NLA) {
				SpaceNla *snla = (SpaceNla *)sl;
				
				snla->ads = newdataadr(fd, snla->ads);
			}
			else if (sl->spacetype == SPACE_OUTLINER) {
				SpaceOops *soops = (SpaceOops *) sl;
				
				/* use newdataadr_no_us and do not free old memory avoiding double
				 * frees and use of freed memory. this could happen because of a
				 * bug fixed in revision 58959 where the treestore memory address
				 * was not unique */
				TreeStore *ts = newdataadr_no_us(fd, soops->treestore);
				soops->treestore = NULL;
				if (ts) {
					TreeStoreElem *elems = newdataadr_no_us(fd, ts->data);
					
					soops->treestore = BLI_mempool_create(sizeof(TreeStoreElem), ts->usedelem,
					                                      512, BLI_MEMPOOL_ALLOW_ITER);
					if (ts->usedelem && elems) {
						int i;
						for (i = 0; i < ts->usedelem; i++) {
							TreeStoreElem *new_elem = BLI_mempool_alloc(soops->treestore);
							*new_elem = elems[i];
						}
					}
					/* we only saved what was used */
					soops->storeflag |= SO_TREESTORE_CLEANUP;	// at first draw
				}
				soops->treehash = NULL;
				soops->tree.first = soops->tree.last= NULL;
			}
			else if (sl->spacetype == SPACE_IMAGE) {
				SpaceImage *sima = (SpaceImage *)sl;

				sima->iuser.scene = NULL;
				sima->iuser.ok = 1;
				sima->scopes.waveform_1 = NULL;
				sima->scopes.waveform_2 = NULL;
				sima->scopes.waveform_3 = NULL;
				sima->scopes.vecscope = NULL;
				sima->scopes.ok = 0;
				
				/* WARNING: gpencil data is no longer stored directly in sima after 2.5 
				 * so sacrifice a few old files for now to avoid crashes with new files!
				 * committed: r28002 */
#if 0
				sima->gpd = newdataadr(fd, sima->gpd);
				if (sima->gpd)
					direct_link_gpencil(fd, sima->gpd);
#endif
			}
			else if (sl->spacetype == SPACE_NODE) {
				SpaceNode *snode = (SpaceNode *)sl;
				
				if (snode->gpd) {
					snode->gpd = newdataadr(fd, snode->gpd);
					direct_link_gpencil(fd, snode->gpd);
				}
				
				link_list(fd, &snode->treepath);
				snode->edittree = NULL;
				snode->iofsd = NULL;
				BLI_listbase_clear(&snode->linkdrag);
			}
			else if (sl->spacetype == SPACE_TEXT) {
				SpaceText *st= (SpaceText *)sl;
				
				st->drawcache = NULL;
				st->scroll_accum[0] = 0.0f;
				st->scroll_accum[1] = 0.0f;
			}
			else if (sl->spacetype == SPACE_TIME) {
				SpaceTime *stime = (SpaceTime *)sl;
				BLI_listbase_clear(&stime->caches);
			}
			else if (sl->spacetype == SPACE_LOGIC) {
				SpaceLogic *slogic = (SpaceLogic *)sl;
				
				/* XXX: this is new stuff, which shouldn't be directly linking to gpd... */
				if (slogic->gpd) {
					slogic->gpd = newdataadr(fd, slogic->gpd);
					direct_link_gpencil(fd, slogic->gpd);
				}
			}
			else if (sl->spacetype == SPACE_SEQ) {
				SpaceSeq *sseq = (SpaceSeq *)sl;
				
				/* grease pencil data is not a direct data and can't be linked from direct_link*
				 * functions, it should be linked from lib_link* functions instead
				 *
				 * otherwise it'll lead to lost grease data on open because it'll likely be
				 * read from file after all other users of grease pencil and newdataadr would
				 * simple return NULL here (sergey)
				 */
#if 0
				if (sseq->gpd) {
					sseq->gpd = newdataadr(fd, sseq->gpd);
					direct_link_gpencil(fd, sseq->gpd);
				}
#endif
				sseq->scopes.reference_ibuf = NULL;
				sseq->scopes.zebra_ibuf = NULL;
				sseq->scopes.waveform_ibuf = NULL;
				sseq->scopes.sep_waveform_ibuf = NULL;
				sseq->scopes.vector_ibuf = NULL;
				sseq->scopes.histogram_ibuf = NULL;

			}
			else if (sl->spacetype == SPACE_BUTS) {
				SpaceButs *sbuts = (SpaceButs *)sl;
				
				sbuts->path= NULL;
				sbuts->texuser= NULL;
				sbuts->mainbo = sbuts->mainb;
				sbuts->mainbuser = sbuts->mainb;
			}
			else if (sl->spacetype == SPACE_CONSOLE) {
				SpaceConsole *sconsole = (SpaceConsole *)sl;
				ConsoleLine *cl, *cl_next;
				
				link_list(fd, &sconsole->scrollback);
				link_list(fd, &sconsole->history);
				
				//for (cl= sconsole->scrollback.first; cl; cl= cl->next)
				//	cl->line= newdataadr(fd, cl->line);
				
				/* comma expressions, (e.g. expr1, expr2, expr3) evaluate each expression,
				 * from left to right.  the right-most expression sets the result of the comma
				 * expression as a whole*/
				for (cl = sconsole->history.first; cl; cl = cl_next) {
					cl_next = cl->next;
					cl->line = newdataadr(fd, cl->line);
					if (cl->line) {
						/* the allocted length is not written, so reset here */
						cl->len_alloc = cl->len + 1;
					}
					else {
						BLI_remlink(&sconsole->history, cl);
						MEM_freeN(cl);
					}
				}
			}
			else if (sl->spacetype == SPACE_FILE) {
				SpaceFile *sfile = (SpaceFile *)sl;
				
				/* this sort of info is probably irrelevant for reloading...
				 * plus, it isn't saved to files yet!
				 */
				sfile->folders_prev = sfile->folders_next = NULL;
				sfile->files = NULL;
				sfile->layout = NULL;
				sfile->op = NULL;
				sfile->previews_timer = NULL;
				sfile->params = newdataadr(fd, sfile->params);
			}
			else if (sl->spacetype == SPACE_CLIP) {
				SpaceClip *sclip = (SpaceClip *)sl;
				
				sclip->scopes.track_search = NULL;
				sclip->scopes.track_preview = NULL;
				sclip->scopes.ok = 0;
			}
		}
		
		BLI_listbase_clear(&sa->actionzones);
		
		sa->v1 = newdataadr(fd, sa->v1);
		sa->v2 = newdataadr(fd, sa->v2);
		sa->v3 = newdataadr(fd, sa->v3);
		sa->v4 = newdataadr(fd, sa->v4);
	}
	
	return wrong_id;
}

/* ********** READ LIBRARY *************** */


static void direct_link_library(FileData *fd, Library *lib, Main *main)
{
	Main *newmain;
	
	/* check if the library was already read */
	for (newmain = fd->mainlist->first; newmain; newmain = newmain->next) {
		if (newmain->curlib) {
			if (BLI_path_cmp(newmain->curlib->filepath, lib->filepath) == 0) {
				blo_reportf_wrap(fd->reports, RPT_WARNING,
				                 TIP_("Library '%s', '%s' had multiple instances, save and reload!"),
				                 lib->name, lib->filepath);
				
				change_idid_adr(fd->mainlist, fd, lib, newmain->curlib);
/*				change_idid_adr_fd(fd, lib, newmain->curlib); */
				
				BLI_remlink(&main->library, lib);
				MEM_freeN(lib);
				
				
				return;
			}
		}
	}
	/* make sure we have full path in lib->filepath */
	BLI_strncpy(lib->filepath, lib->name, sizeof(lib->name));
	BLI_cleanup_path(fd->relabase, lib->filepath);
	
//	printf("direct_link_library: name %s\n", lib->name);
//	printf("direct_link_library: filepath %s\n", lib->filepath);
	
	lib->packedfile = direct_link_packedfile(fd, lib->packedfile);
	
	/* new main */
	newmain = BKE_main_new();
	BLI_addtail(fd->mainlist, newmain);
	newmain->curlib = lib;
	
	lib->parent = NULL;
}

static void lib_link_library(FileData *UNUSED(fd), Main *main)
{
	Library *lib;
	for (lib = main->library.first; lib; lib = lib->id.next) {
		id_us_ensure_real(&lib->id);
	}
}

/* Always call this once you have loaded new library data to set the relative paths correctly in relation to the blend file */
static void fix_relpaths_library(const char *basepath, Main *main)
{
	Library *lib;
	/* BLO_read_from_memory uses a blank filename */
	if (basepath == NULL || basepath[0] == '\0') {
		for (lib = main->library.first; lib; lib= lib->id.next) {
			/* when loading a linked lib into a file which has not been saved,
			 * there is nothing we can be relative to, so instead we need to make
			 * it absolute. This can happen when appending an object with a relative
			 * link into an unsaved blend file. See [#27405].
			 * The remap relative option will make it relative again on save - campbell */
			if (BLI_path_is_rel(lib->name)) {
				BLI_strncpy(lib->name, lib->filepath, sizeof(lib->name));
			}
		}
	}
	else {
		for (lib = main->library.first; lib; lib = lib->id.next) {
			/* Libraries store both relative and abs paths, recreate relative paths,
			 * relative to the blend file since indirectly linked libs will be relative to their direct linked library */
			if (BLI_path_is_rel(lib->name)) {  /* if this is relative to begin with? */
				BLI_strncpy(lib->name, lib->filepath, sizeof(lib->name));
				BLI_path_rel(lib->name, basepath);
			}
		}
	}
}

/* ************ READ SPEAKER ***************** */

static void lib_link_speaker(FileData *fd, Main *main)
{
	for (Speaker *spk = main->speaker.first; spk; spk = spk->id.next) {
		if (spk->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(spk->id.properties, fd);
			lib_link_animdata(fd, &spk->id, spk->adt);
			
			spk->sound = newlibadr_us(fd, spk->id.lib, spk->sound);

			spk->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_speaker(FileData *fd, Speaker *spk)
{
	spk->adt = newdataadr(fd, spk->adt);
	direct_link_animdata(fd, spk->adt);

#if 0
	spk->sound = newdataadr(fd, spk->sound);
	direct_link_sound(fd, spk->sound);
#endif
}

/* ************** READ SOUND ******************* */

static void direct_link_sound(FileData *fd, bSound *sound)
{
	sound->handle = NULL;
	sound->playback_handle = NULL;

	/* versioning stuff, if there was a cache, then we enable caching: */
	if (sound->cache) {
		sound->flags |= SOUND_FLAGS_CACHING;
		sound->cache = NULL;
	}

	if (fd->soundmap) {
		sound->waveform = newsoundadr(fd, sound->waveform);
	}	
	else {
		sound->waveform = NULL;
	}
		
	if (sound->spinlock) {
		sound->spinlock = MEM_mallocN(sizeof(SpinLock), "sound_spinlock");
		BLI_spin_init(sound->spinlock);
	}
	/* clear waveform loading flag */
	sound->flags &= ~SOUND_FLAGS_WAVEFORM_LOADING;

	sound->packedfile = direct_link_packedfile(fd, sound->packedfile);
	sound->newpackedfile = direct_link_packedfile(fd, sound->newpackedfile);
}

static void lib_link_sound(FileData *fd, Main *main)
{
	for (bSound *sound = main->sound.first; sound; sound = sound->id.next) {
		if (sound->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(sound->id.properties, fd);

			sound->ipo = newlibadr_us(fd, sound->id.lib, sound->ipo); // XXX deprecated - old animation system
			
			BKE_sound_load(main, sound);

			sound->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}
/* ***************** READ GROUP *************** */

static void direct_link_group(FileData *fd, Group *group)
{
	link_list(fd, &group->gobject);

	group->preview = direct_link_preview_image(fd, group->preview);
}

static void lib_link_group(FileData *fd, Main *main)
{
	for (Group *group = main->group.first; group; group = group->id.next) {
		if (group->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(group->id.properties, fd);
			
			bool add_us = false;
			
			for (GroupObject *go = group->gobject.first; go; go = go->next) {
				go->ob = newlibadr_real_us(fd, group->id.lib, go->ob);
				if (go->ob) {
					go->ob->flag |= OB_FROMGROUP;
					/* if group has an object, it increments user... */
					add_us = true;
				}
			}
			if (add_us) {
				id_us_ensure_real(&group->id);
			}
			BKE_group_object_unlink(group, NULL, NULL, NULL);	/* removes NULL entries */

			group->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

/* ***************** READ MOVIECLIP *************** */

static void direct_link_movieReconstruction(FileData *fd, MovieTrackingReconstruction *reconstruction)
{
	reconstruction->cameras = newdataadr(fd, reconstruction->cameras);
}

static void direct_link_movieTracks(FileData *fd, ListBase *tracksbase)
{
	MovieTrackingTrack *track;
	
	link_list(fd, tracksbase);
	
	for (track = tracksbase->first; track; track = track->next) {
		track->markers = newdataadr(fd, track->markers);
	}
}

static void direct_link_moviePlaneTracks(FileData *fd, ListBase *plane_tracks_base)
{
	MovieTrackingPlaneTrack *plane_track;

	link_list(fd, plane_tracks_base);

	for (plane_track = plane_tracks_base->first;
	     plane_track;
	     plane_track = plane_track->next)
	{
		int i;

		plane_track->point_tracks = newdataadr(fd, plane_track->point_tracks);
		test_pointer_array(fd, (void**)&plane_track->point_tracks);
		for (i = 0; i < plane_track->point_tracksnr; i++) {
			plane_track->point_tracks[i] = newdataadr(fd, plane_track->point_tracks[i]);
		}

		plane_track->markers = newdataadr(fd, plane_track->markers);
	}
}

static void direct_link_movieclip(FileData *fd, MovieClip *clip)
{
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *object;

	clip->adt= newdataadr(fd, clip->adt);

	if (fd->movieclipmap) clip->cache = newmclipadr(fd, clip->cache);
	else clip->cache = NULL;

	if (fd->movieclipmap) clip->tracking.camera.intrinsics = newmclipadr(fd, clip->tracking.camera.intrinsics);
	else clip->tracking.camera.intrinsics = NULL;

	direct_link_movieTracks(fd, &tracking->tracks);
	direct_link_moviePlaneTracks(fd, &tracking->plane_tracks);
	direct_link_movieReconstruction(fd, &tracking->reconstruction);

	clip->tracking.act_track = newdataadr(fd, clip->tracking.act_track);
	clip->tracking.act_plane_track = newdataadr(fd, clip->tracking.act_plane_track);

	clip->anim = NULL;
	clip->tracking_context = NULL;
	clip->tracking.stats = NULL;

	/* Needed for proper versioning, will be NULL for all newer files anyway. */
	clip->tracking.stabilization.rot_track = newdataadr(fd, clip->tracking.stabilization.rot_track);

	clip->tracking.dopesheet.ok = 0;
	BLI_listbase_clear(&clip->tracking.dopesheet.channels);
	BLI_listbase_clear(&clip->tracking.dopesheet.coverage_segments);

	link_list(fd, &tracking->objects);
	
	for (object = tracking->objects.first; object; object = object->next) {
		direct_link_movieTracks(fd, &object->tracks);
		direct_link_moviePlaneTracks(fd, &object->plane_tracks);
		direct_link_movieReconstruction(fd, &object->reconstruction);
	}
}

static void lib_link_movieTracks(FileData *fd, MovieClip *clip, ListBase *tracksbase)
{
	MovieTrackingTrack *track;

	for (track = tracksbase->first; track; track = track->next) {
		track->gpd = newlibadr_us(fd, clip->id.lib, track->gpd);
	}
}

static void lib_link_moviePlaneTracks(FileData *fd, MovieClip *clip, ListBase *tracksbase)
{
	MovieTrackingPlaneTrack *plane_track;

	for (plane_track = tracksbase->first; plane_track; plane_track = plane_track->next) {
		plane_track->image = newlibadr_us(fd, clip->id.lib, plane_track->image);
	}
}

static void lib_link_movieclip(FileData *fd, Main *main)
{
	for (MovieClip *clip = main->movieclip.first; clip; clip = clip->id.next) {
		if (clip->id.tag & LIB_TAG_NEED_LINK) {
			MovieTracking *tracking = &clip->tracking;

			IDP_LibLinkProperty(clip->id.properties, fd);
			lib_link_animdata(fd, &clip->id, clip->adt);
			
			clip->gpd = newlibadr_us(fd, clip->id.lib, clip->gpd);
			
			lib_link_movieTracks(fd, clip, &tracking->tracks);
			lib_link_moviePlaneTracks(fd, clip, &tracking->plane_tracks);

			for (MovieTrackingObject *object = tracking->objects.first; object; object = object->next) {
				lib_link_movieTracks(fd, clip, &object->tracks);
				lib_link_moviePlaneTracks(fd, clip, &object->plane_tracks);
			}

			clip->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

/* ***************** READ MOVIECLIP *************** */

static void direct_link_mask(FileData *fd, Mask *mask)
{
	MaskLayer *masklay;

	mask->adt = newdataadr(fd, mask->adt);

	link_list(fd, &mask->masklayers);

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;
		MaskLayerShape *masklay_shape;

		/* can't use newdataadr since it's a pointer within an array */
		MaskSplinePoint *act_point_search = NULL;

		link_list(fd, &masklay->splines);

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			MaskSplinePoint *points_old = spline->points;
			int i;

			spline->points = newdataadr(fd, spline->points);

			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];

				if (point->tot_uw)
					point->uw = newdataadr(fd, point->uw);
			}

			/* detect active point */
			if ((act_point_search == NULL) &&
			    (masklay->act_point >= points_old) &&
			    (masklay->act_point <  points_old + spline->tot_point))
			{
				act_point_search = &spline->points[masklay->act_point - points_old];
			}
		}

		link_list(fd, &masklay->splines_shapes);

		for (masklay_shape = masklay->splines_shapes.first; masklay_shape; masklay_shape = masklay_shape->next) {
			masklay_shape->data = newdataadr(fd, masklay_shape->data);

			if (masklay_shape->tot_vert) {
				if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
					BLI_endian_switch_float_array(masklay_shape->data,
					                              masklay_shape->tot_vert * sizeof(float) * MASK_OBJECT_SHAPE_ELEM_SIZE);

				}
			}
		}

		masklay->act_spline = newdataadr(fd, masklay->act_spline);
		masklay->act_point = act_point_search;
	}
}

static void lib_link_mask_parent(FileData *fd, Mask *mask, MaskParent *parent)
{
	parent->id = newlibadr_us(fd, mask->id.lib, parent->id);
}

static void lib_link_mask(FileData *fd, Main *main)
{
	for (Mask *mask = main->mask.first; mask; mask = mask->id.next) {
		if (mask->id.tag & LIB_TAG_NEED_LINK) {
			IDP_LibLinkProperty(mask->id.properties, fd);
			lib_link_animdata(fd, &mask->id, mask->adt);

			for (MaskLayer *masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
				MaskSpline *spline;

				spline = masklay->splines.first;
				while (spline) {
					int i;

					for (i = 0; i < spline->tot_point; i++) {
						MaskSplinePoint *point = &spline->points[i];

						lib_link_mask_parent(fd, mask, &point->parent);
					}

					lib_link_mask_parent(fd, mask, &spline->parent);

					spline = spline->next;
				}
			}

			mask->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

/* ************ READ LINE STYLE ***************** */

static void lib_link_linestyle(FileData *fd, Main *main)
{
	for (FreestyleLineStyle *linestyle = main->linestyle.first; linestyle; linestyle = linestyle->id.next) {
		if (linestyle->id.tag & LIB_TAG_NEED_LINK) {
			LineStyleModifier *m;

			IDP_LibLinkProperty(linestyle->id.properties, fd);
			lib_link_animdata(fd, &linestyle->id, linestyle->adt);

			for (m = linestyle->color_modifiers.first; m; m = m->next) {
				switch (m->type) {
				case LS_MODIFIER_DISTANCE_FROM_OBJECT:
					{
						LineStyleColorModifier_DistanceFromObject *cm = (LineStyleColorModifier_DistanceFromObject *)m;
						cm->target = newlibadr(fd, linestyle->id.lib, cm->target);
					}
					break;
				}
			}
			for (m = linestyle->alpha_modifiers.first; m; m = m->next) {
				switch (m->type) {
				case LS_MODIFIER_DISTANCE_FROM_OBJECT:
					{
						LineStyleAlphaModifier_DistanceFromObject *am = (LineStyleAlphaModifier_DistanceFromObject *)m;
						am->target = newlibadr(fd, linestyle->id.lib, am->target);
					}
					break;
				}
			}
			for (m = linestyle->thickness_modifiers.first; m; m = m->next) {
				switch (m->type) {
				case LS_MODIFIER_DISTANCE_FROM_OBJECT:
					{
						LineStyleThicknessModifier_DistanceFromObject *tm = (LineStyleThicknessModifier_DistanceFromObject *)m;
						tm->target = newlibadr(fd, linestyle->id.lib, tm->target);
					}
					break;
				}
			}
			for (int a = 0; a < MAX_MTEX; a++) {
				MTex *mtex = linestyle->mtex[a];
				if (mtex) {
					mtex->tex = newlibadr_us(fd, linestyle->id.lib, mtex->tex);
					mtex->object = newlibadr(fd, linestyle->id.lib, mtex->object);
				}
			}
			if (linestyle->nodetree) {
				lib_link_ntree(fd, &linestyle->id, linestyle->nodetree);
				linestyle->nodetree->id.lib = linestyle->id.lib;
			}

			linestyle->id.tag &= ~LIB_TAG_NEED_LINK;
		}
	}
}

static void direct_link_linestyle_color_modifier(FileData *fd, LineStyleModifier *modifier)
{
	switch (modifier->type) {
	case LS_MODIFIER_ALONG_STROKE:
		{
			LineStyleColorModifier_AlongStroke *m = (LineStyleColorModifier_AlongStroke *)modifier;
			m->color_ramp = newdataadr(fd, m->color_ramp);
		}
		break;
	case LS_MODIFIER_DISTANCE_FROM_CAMERA:
		{
			LineStyleColorModifier_DistanceFromCamera *m = (LineStyleColorModifier_DistanceFromCamera *)modifier;
			m->color_ramp = newdataadr(fd, m->color_ramp);
		}
		break;
	case LS_MODIFIER_DISTANCE_FROM_OBJECT:
		{
			LineStyleColorModifier_DistanceFromObject *m = (LineStyleColorModifier_DistanceFromObject *)modifier;
			m->color_ramp = newdataadr(fd, m->color_ramp);
		}
		break;
	case LS_MODIFIER_MATERIAL:
		{
			LineStyleColorModifier_Material *m = (LineStyleColorModifier_Material *)modifier;
			m->color_ramp = newdataadr(fd, m->color_ramp);
		}
		break;
	case LS_MODIFIER_TANGENT:
		{
			LineStyleColorModifier_Tangent *m = (LineStyleColorModifier_Tangent *)modifier;
			m->color_ramp = newdataadr(fd, m->color_ramp);
		}
		break;
	case LS_MODIFIER_NOISE:
		{
			LineStyleColorModifier_Noise *m = (LineStyleColorModifier_Noise *)modifier;
			m->color_ramp = newdataadr(fd, m->color_ramp);
		}
		break;
	case LS_MODIFIER_CREASE_ANGLE:
		{
			LineStyleColorModifier_CreaseAngle *m = (LineStyleColorModifier_CreaseAngle *)modifier;
			m->color_ramp = newdataadr(fd, m->color_ramp);
		}
		break;
	case LS_MODIFIER_CURVATURE_3D:
		{
			LineStyleColorModifier_Curvature_3D *m = (LineStyleColorModifier_Curvature_3D *)modifier;
			m->color_ramp = newdataadr(fd, m->color_ramp);
		}
		break;
	}
}

static void direct_link_linestyle_alpha_modifier(FileData *fd, LineStyleModifier *modifier)
{
	switch (modifier->type) {
	case LS_MODIFIER_ALONG_STROKE:
		{
			LineStyleAlphaModifier_AlongStroke *m = (LineStyleAlphaModifier_AlongStroke *)modifier;
			m->curve = newdataadr(fd, m->curve);
			direct_link_curvemapping(fd, m->curve);
		}
		break;
	case LS_MODIFIER_DISTANCE_FROM_CAMERA:
		{
			LineStyleAlphaModifier_DistanceFromCamera *m = (LineStyleAlphaModifier_DistanceFromCamera *)modifier;
			m->curve = newdataadr(fd, m->curve);
			direct_link_curvemapping(fd, m->curve);
		}
		break;
	case LS_MODIFIER_DISTANCE_FROM_OBJECT:
		{
			LineStyleAlphaModifier_DistanceFromObject *m = (LineStyleAlphaModifier_DistanceFromObject *)modifier;
			m->curve = newdataadr(fd, m->curve);
			direct_link_curvemapping(fd, m->curve);
		}
		break;
	case LS_MODIFIER_MATERIAL:
		{
			LineStyleAlphaModifier_Material *m = (LineStyleAlphaModifier_Material *)modifier;
			m->curve = newdataadr(fd, m->curve);
			direct_link_curvemapping(fd, m->curve);
		}
		break;
	case LS_MODIFIER_TANGENT:
		{
			LineStyleAlphaModifier_Tangent *m = (LineStyleAlphaModifier_Tangent *)modifier;
			m->curve = newdataadr(fd, m->curve);
			direct_link_curvemapping(fd, m->curve);
		}
		break;
	case LS_MODIFIER_NOISE:
		{
			LineStyleAlphaModifier_Noise *m = (LineStyleAlphaModifier_Noise *)modifier;
			m->curve = newdataadr(fd, m->curve);
			direct_link_curvemapping(fd, m->curve);
		}
		break;
	case LS_MODIFIER_CREASE_ANGLE:
		{
			LineStyleAlphaModifier_CreaseAngle *m = (LineStyleAlphaModifier_CreaseAngle *)modifier;
			m->curve = newdataadr(fd, m->curve);
			direct_link_curvemapping(fd, m->curve);
		}
		break;
	case LS_MODIFIER_CURVATURE_3D:
		{
			LineStyleAlphaModifier_Curvature_3D *m = (LineStyleAlphaModifier_Curvature_3D *)modifier;
			m->curve = newdataadr(fd, m->curve);
			direct_link_curvemapping(fd, m->curve);
		}
		break;
	}
}

static void direct_link_linestyle_thickness_modifier(FileData *fd, LineStyleModifier *modifier)
{
	switch (modifier->type) {
	case LS_MODIFIER_ALONG_STROKE:
		{
			LineStyleThicknessModifier_AlongStroke *m = (LineStyleThicknessModifier_AlongStroke *)modifier;
			m->curve = newdataadr(fd, m->curve);
			direct_link_curvemapping(fd, m->curve);
		}
		break;
	case LS_MODIFIER_DISTANCE_FROM_CAMERA:
		{
			LineStyleThicknessModifier_DistanceFromCamera *m = (LineStyleThicknessModifier_DistanceFromCamera *)modifier;
			m->curve = newdataadr(fd, m->curve);
			direct_link_curvemapping(fd, m->curve);
		}
		break;
	case LS_MODIFIER_DISTANCE_FROM_OBJECT:
		{
			LineStyleThicknessModifier_DistanceFromObject *m = (LineStyleThicknessModifier_DistanceFromObject *)modifier;
			m->curve = newdataadr(fd, m->curve);
			direct_link_curvemapping(fd, m->curve);
		}
		break;
	case LS_MODIFIER_MATERIAL:
		{
			LineStyleThicknessModifier_Material *m = (LineStyleThicknessModifier_Material *)modifier;
			m->curve = newdataadr(fd, m->curve);
			direct_link_curvemapping(fd, m->curve);
		}
		break;
	case LS_MODIFIER_TANGENT:
		{
			LineStyleThicknessModifier_Tangent *m = (LineStyleThicknessModifier_Tangent *)modifier;
			m->curve = newdataadr(fd, m->curve);
			direct_link_curvemapping(fd, m->curve);
		}
		break;
	case LS_MODIFIER_CREASE_ANGLE:
		{
			LineStyleThicknessModifier_CreaseAngle *m = (LineStyleThicknessModifier_CreaseAngle *)modifier;
			m->curve = newdataadr(fd, m->curve);
			direct_link_curvemapping(fd, m->curve);
		}
		break;
	case LS_MODIFIER_CURVATURE_3D:
		{
			LineStyleThicknessModifier_Curvature_3D *m = (LineStyleThicknessModifier_Curvature_3D *)modifier;
			m->curve = newdataadr(fd, m->curve);
			direct_link_curvemapping(fd, m->curve);
		}
		break;
	}
}

static void direct_link_linestyle_geometry_modifier(FileData *UNUSED(fd), LineStyleModifier *UNUSED(modifier))
{
}

static void direct_link_linestyle(FileData *fd, FreestyleLineStyle *linestyle)
{
	int a;
	LineStyleModifier *modifier;

	linestyle->adt= newdataadr(fd, linestyle->adt);
	direct_link_animdata(fd, linestyle->adt);
	link_list(fd, &linestyle->color_modifiers);
	for (modifier = linestyle->color_modifiers.first; modifier; modifier = modifier->next)
		direct_link_linestyle_color_modifier(fd, modifier);
	link_list(fd, &linestyle->alpha_modifiers);
	for (modifier = linestyle->alpha_modifiers.first; modifier; modifier = modifier->next)
		direct_link_linestyle_alpha_modifier(fd, modifier);
	link_list(fd, &linestyle->thickness_modifiers);
	for (modifier = linestyle->thickness_modifiers.first; modifier; modifier = modifier->next)
		direct_link_linestyle_thickness_modifier(fd, modifier);
	link_list(fd, &linestyle->geometry_modifiers);
	for (modifier = linestyle->geometry_modifiers.first; modifier; modifier = modifier->next)
		direct_link_linestyle_geometry_modifier(fd, modifier);
	for (a = 0; a < MAX_MTEX; a++) {
		linestyle->mtex[a] = newdataadr(fd, linestyle->mtex[a]);
	}
	linestyle->nodetree = newdataadr(fd, linestyle->nodetree);
	if (linestyle->nodetree) {
		direct_link_id(fd, &linestyle->nodetree->id);
		direct_link_nodetree(fd, linestyle->nodetree);
	}
}

/* ************** GENERAL & MAIN ******************** */


static const char *dataname(short id_code)
{
	switch (id_code) {
		case ID_OB: return "Data from OB";
		case ID_ME: return "Data from ME";
		case ID_IP: return "Data from IP";
		case ID_SCE: return "Data from SCE";
		case ID_MA: return "Data from MA";
		case ID_TE: return "Data from TE";
		case ID_CU: return "Data from CU";
		case ID_GR: return "Data from GR";
		case ID_AR: return "Data from AR";
		case ID_AC: return "Data from AC";
		case ID_LI: return "Data from LI";
		case ID_MB: return "Data from MB";
		case ID_IM: return "Data from IM";
		case ID_LT: return "Data from LT";
		case ID_LA: return "Data from LA";
		case ID_CA: return "Data from CA";
		case ID_KE: return "Data from KE";
		case ID_WO: return "Data from WO";
		case ID_SCR: return "Data from SCR";
		case ID_VF: return "Data from VF";
		case ID_TXT	: return "Data from TXT";
		case ID_SPK: return "Data from SPK";
		case ID_SO: return "Data from SO";
		case ID_NT: return "Data from NT";
		case ID_BR: return "Data from BR";
		case ID_PA: return "Data from PA";
		case ID_PAL: return "Data from PAL";
		case ID_PC: return "Data from PCRV";
		case ID_GD: return "Data from GD";
		case ID_WM: return "Data from WM";
		case ID_MC: return "Data from MC";
		case ID_MSK: return "Data from MSK";
		case ID_LS: return "Data from LS";
		case ID_CF: return "Data from CF";
	}
	return "Data from Lib Block";
	
}

static BHead *read_data_into_oldnewmap(FileData *fd, BHead *bhead, const char *allocname)
{
	bhead = blo_nextbhead(fd, bhead);
	
	while (bhead && bhead->code==DATA) {
		void *data;
#if 0
		/* XXX DUMB DEBUGGING OPTION TO GIVE NAMES for guarded malloc errors */
		short *sp = fd->filesdna->structs[bhead->SDNAnr];
		char *tmp = malloc(100);
		allocname = fd->filesdna->types[ sp[0] ];
		strcpy(tmp, allocname);
		data = read_struct(fd, bhead, tmp);
#else
		data = read_struct(fd, bhead, allocname);
#endif
		
		if (data) {
			oldnewmap_insert(fd->datamap, bhead->old, data, 0);
		}
		
		bhead = blo_nextbhead(fd, bhead);
	}
	
	return bhead;
}

static BHead *read_libblock(FileData *fd, Main *main, BHead *bhead, const short tag, ID **r_id)
{
	/* this routine reads a libblock and its direct data. Use link functions to connect it all
	 */
	ID *id;
	ListBase *lb;
	const char *allocname;
	bool wrong_id = false;

	/* In undo case, most libs and linked data should be kept as is from previous state (see BLO_read_from_memfile).
	 * However, some needed by the snapshot being read may have been removed in previous one, and would go missing.
	 * This leads e.g. to desappearing objects in some undo/redo case, see T34446.
	 * That means we have to carefully check whether current lib or libdata already exits in old main, if it does
	 * we merely copy it over into new main area, otherwise we have to do a full read of that bhead... */
	if (fd->memfile && ELEM(bhead->code, ID_LI, ID_ID)) {
		const char *idname = bhead_id_name(fd, bhead);

#ifdef PRINT_DEBUG
		printf("Checking %s...\n", idname);
#endif

		if (bhead->code == ID_LI) {
			Main *libmain = fd->old_mainlist->first;
			/* Skip oldmain itself... */
			for (libmain = libmain->next; libmain; libmain = libmain->next) {
#ifdef PRINT_DEBUG
				printf("... against %s: ", libmain->curlib ? libmain->curlib->id.name : "<NULL>");
#endif
				if (libmain->curlib && STREQ(idname, libmain->curlib->id.name)) {
					Main *oldmain = fd->old_mainlist->first;
#ifdef PRINT_DEBUG
					printf("FOUND!\n");
#endif
					/* In case of a library, we need to re-add its main to fd->mainlist, because if we have later
					 * a missing ID_ID, we need to get the correct lib it is linked to!
					 * Order is crucial, we cannot bulk-add it in BLO_read_from_memfile() like it used to be... */
					BLI_remlink(fd->old_mainlist, libmain);
					BLI_remlink_safe(&oldmain->library, libmain->curlib);
					BLI_addtail(fd->mainlist, libmain);
					BLI_addtail(&main->library, libmain->curlib);

					if (r_id) {
						*r_id = NULL;  /* Just in case... */
					}
					return blo_nextbhead(fd, bhead);
				}
#ifdef PRINT_DEBUG
				printf("nothing...\n");
#endif
			}
		}
		else {
#ifdef PRINT_DEBUG
			printf("... in %s (%s): ", main->curlib ? main->curlib->id.name : "<NULL>", main->curlib ? main->curlib->name : "<NULL>");
#endif
			if ((id = BKE_libblock_find_name_ex(main, GS(idname), idname + 2))) {
#ifdef PRINT_DEBUG
				printf("FOUND!\n");
#endif
				/* Even though we found our linked ID, there is no guarantee its address is still the same... */
				if (id != bhead->old) {
					oldnewmap_insert(fd->libmap, bhead->old, id, GS(id->name));
				}

				/* No need to do anything else for ID_ID, it's assumed already present in its lib's main... */
				if (r_id) {
					*r_id = NULL;  /* Just in case... */
				}
				return blo_nextbhead(fd, bhead);
			}
#ifdef PRINT_DEBUG
			printf("nothing...\n");
#endif
		}
	}

	/* read libblock */
	id = read_struct(fd, bhead, "lib block");

	if (id) {
		const short idcode = GS(id->name);
		/* do after read_struct, for dna reconstruct */
		lb = which_libbase(main, idcode);
		if (lb) {
			oldnewmap_insert(fd->libmap, bhead->old, id, bhead->code);	/* for ID_ID check */
			BLI_addtail(lb, id);
		}
		else {
			/* unknown ID type */
			printf("%s: unknown id code '%c%c'\n", __func__, (idcode & 0xff), (idcode >> 8));
			MEM_freeN(id);
			id = NULL;
		}
	}

	if (r_id)
		*r_id = id;
	if (!id)
		return blo_nextbhead(fd, bhead);
	
	id->tag = tag | LIB_TAG_NEED_LINK;
	id->lib = main->curlib;
	id->us = ID_FAKE_USERS(id);
	id->icon_id = 0;
	id->newid = NULL;  /* Needed because .blend may have been saved with crap value here... */
	
	/* this case cannot be direct_linked: it's just the ID part */
	if (bhead->code == ID_ID) {
		return blo_nextbhead(fd, bhead);
	}

	/* That way, we know which datablock needs do_versions (required currently for linking). */
	id->tag |= LIB_TAG_NEW;

	/* need a name for the mallocN, just for debugging and sane prints on leaks */
	allocname = dataname(GS(id->name));
	
	/* read all data into fd->datamap */
	bhead = read_data_into_oldnewmap(fd, bhead, allocname);
	
	/* init pointers direct data */
	direct_link_id(fd, id);
	
	switch (GS(id->name)) {
		case ID_WM:
			direct_link_windowmanager(fd, (wmWindowManager *)id);
			break;
		case ID_SCR:
			wrong_id = direct_link_screen(fd, (bScreen *)id);
			break;
		case ID_SCE:
			direct_link_scene(fd, (Scene *)id);
			break;
		case ID_OB:
			direct_link_object(fd, (Object *)id);
			break;
		case ID_ME:
			direct_link_mesh(fd, (Mesh *)id);
			break;
		case ID_CU:
			direct_link_curve(fd, (Curve *)id);
			break;
		case ID_MB:
			direct_link_mball(fd, (MetaBall *)id);
			break;
		case ID_MA:
			direct_link_material(fd, (Material *)id);
			break;
		case ID_TE:
			direct_link_texture(fd, (Tex *)id);
			break;
		case ID_IM:
			direct_link_image(fd, (Image *)id);
			break;
		case ID_LA:
			direct_link_lamp(fd, (Lamp *)id);
			break;
		case ID_VF:
			direct_link_vfont(fd, (VFont *)id);
			break;
		case ID_TXT:
			direct_link_text(fd, (Text *)id);
			break;
		case ID_IP:
			direct_link_ipo(fd, (Ipo *)id);
			break;
		case ID_KE:
			direct_link_key(fd, (Key *)id);
			break;
		case ID_LT:
			direct_link_latt(fd, (Lattice *)id);
			break;
		case ID_WO:
			direct_link_world(fd, (World *)id);
			break;
		case ID_LI:
			direct_link_library(fd, (Library *)id, main);
			break;
		case ID_CA:
			direct_link_camera(fd, (Camera *)id);
			break;
		case ID_SPK:
			direct_link_speaker(fd, (Speaker *)id);
			break;
		case ID_SO:
			direct_link_sound(fd, (bSound *)id);
			break;
		case ID_GR:
			direct_link_group(fd, (Group *)id);
			break;
		case ID_AR:
			direct_link_armature(fd, (bArmature*)id);
			break;
		case ID_AC:
			direct_link_action(fd, (bAction*)id);
			break;
		case ID_NT:
			direct_link_nodetree(fd, (bNodeTree*)id);
			break;
		case ID_BR:
			direct_link_brush(fd, (Brush*)id);
			break;
		case ID_PA:
			direct_link_particlesettings(fd, (ParticleSettings*)id);
			break;
		case ID_GD:
			direct_link_gpencil(fd, (bGPdata *)id);
			break;
		case ID_MC:
			direct_link_movieclip(fd, (MovieClip *)id);
			break;
		case ID_MSK:
			direct_link_mask(fd, (Mask *)id);
			break;
		case ID_LS:
			direct_link_linestyle(fd, (FreestyleLineStyle *)id);
			break;
		case ID_PAL:
			direct_link_palette(fd, (Palette *)id);
			break;
		case ID_PC:
			direct_link_paint_curve(fd, (PaintCurve *)id);
			break;
		case ID_CF:
			direct_link_cachefile(fd, (CacheFile *)id);
			break;
	}
	
	oldnewmap_free_unused(fd->datamap);
	oldnewmap_clear(fd->datamap);
	
	if (wrong_id) {
		BKE_libblock_free(main, id);
	}
	
	return (bhead);
}

/* note, this has to be kept for reading older files... */
/* also version info is written here */
static BHead *read_global(BlendFileData *bfd, FileData *fd, BHead *bhead)
{
	FileGlobal *fg = read_struct(fd, bhead, "Global");
	
	/* copy to bfd handle */
	bfd->main->subversionfile = fg->subversion;
	bfd->main->minversionfile = fg->minversion;
	bfd->main->minsubversionfile = fg->minsubversion;
	bfd->main->build_commit_timestamp = fg->build_commit_timestamp;
	BLI_strncpy(bfd->main->build_hash, fg->build_hash, sizeof(bfd->main->build_hash));
	
	bfd->fileflags = fg->fileflags;
	bfd->globalf = fg->globalf;
	BLI_strncpy(bfd->filename, fg->filename, sizeof(bfd->filename));
	
	/* error in 2.65 and older: main->name was not set if you save from startup (not after loading file) */
	if (bfd->filename[0] == 0) {
		if (fd->fileversion < 265 || (fd->fileversion == 265 && fg->subversion < 1))
			if ((G.fileflags & G_FILE_RECOVER)==0)
				BLI_strncpy(bfd->filename, bfd->main->name, sizeof(bfd->filename));
		
		/* early 2.50 version patch - filename not in FileGlobal struct at all */
		if (fd->fileversion <= 250)
			BLI_strncpy(bfd->filename, bfd->main->name, sizeof(bfd->filename));
	}
	
	if (G.fileflags & G_FILE_RECOVER)
		BLI_strncpy(fd->relabase, fg->filename, sizeof(fd->relabase));
	
	bfd->curscreen = fg->curscreen;
	bfd->curscene = fg->curscene;
	
	MEM_freeN(fg);
	
	fd->globalf = bfd->globalf;
	fd->fileflags = bfd->fileflags;
	
	return blo_nextbhead(fd, bhead);
}

/* note, this has to be kept for reading older files... */
static void link_global(FileData *fd, BlendFileData *bfd)
{
	bfd->curscreen = newlibadr(fd, NULL, bfd->curscreen);
	bfd->curscene = newlibadr(fd, NULL, bfd->curscene);
	// this happens in files older than 2.35
	if (bfd->curscene == NULL) {
		if (bfd->curscreen) bfd->curscene = bfd->curscreen->scene;
	}
}

static void convert_tface_mt(FileData *fd, Main *main)
{
	Main *gmain;
	
	/* this is a delayed do_version (so it can create new materials) */
	if (main->versionfile < 259 || (main->versionfile == 259 && main->subversionfile < 3)) {
		//XXX hack, material.c uses G.main all over the place, instead of main
		// temporarily set G.main to the current main
		gmain = G.main;
		G.main = main;
		
		if (!(do_version_tface(main))) {
			BKE_report(fd->reports, RPT_WARNING, "Texface conversion problem (see error in console)");
		}
		
		//XXX hack, material.c uses G.main allover the place, instead of main
		G.main = gmain;
	}
}

/* initialize userdef with non-UI dependency stuff */
/* other initializers (such as theme color defaults) go to resources.c */
static void do_versions_userdef(FileData *fd, BlendFileData *bfd)
{
	Main *bmain = bfd->main;
	UserDef *user = bfd->user;
	
	if (user == NULL) return;
	
	if (MAIN_VERSION_OLDER(bmain, 266, 4)) {
		bTheme *btheme;
		
		/* themes for Node and Sequence editor were not using grid color, but back. we copy this over then */
		for (btheme = user->themes.first; btheme; btheme = btheme->next) {
			copy_v4_v4_char(btheme->tnode.grid, btheme->tnode.back);
			copy_v4_v4_char(btheme->tseq.grid, btheme->tseq.back);
		}
	}

	if (!DNA_struct_elem_find(fd->filesdna, "UserDef", "WalkNavigation", "walk_navigation")) {
		user->walk_navigation.mouse_speed = 1.0f;
		user->walk_navigation.walk_speed = 2.5f;       /* m/s */
		user->walk_navigation.walk_speed_factor = 5.0f;
		user->walk_navigation.view_height =  1.6f;   /* m */
		user->walk_navigation.jump_height = 0.4f;      /* m */
		user->walk_navigation.teleport_time = 0.2f; /* s */
	}
}

static void do_versions(FileData *fd, Library *lib, Main *main)
{
	/* WATCH IT!!!: pointers from libdata have not been converted */
	
	if (G.debug & G_DEBUG) {
		char build_commit_datetime[32];
		time_t temp_time = main->build_commit_timestamp;
		struct tm *tm = (temp_time) ? gmtime(&temp_time) : NULL;
		if (LIKELY(tm)) {
			strftime(build_commit_datetime, sizeof(build_commit_datetime), "%Y-%m-%d %H:%M", tm);
		}
		else {
			BLI_strncpy(build_commit_datetime, "unknown", sizeof(build_commit_datetime));
		}

		printf("read file %s\n  Version %d sub %d date %s hash %s\n",
		       fd->relabase, main->versionfile, main->subversionfile,
		       build_commit_datetime, main->build_hash);
	}
	
	blo_do_versions_pre250(fd, lib, main);
	blo_do_versions_250(fd, lib, main);
	blo_do_versions_260(fd, lib, main);
	blo_do_versions_270(fd, lib, main);

	/* WATCH IT!!!: pointers from libdata have not been converted yet here! */
	/* WATCH IT 2!: Userdef struct init see do_versions_userdef() above! */

	/* don't forget to set version number in BKE_blender_version.h! */
}

static void do_versions_after_linking(Main *main)
{
//	printf("%s for %s (%s), %d.%d\n", __func__, main->curlib ? main->curlib->name : main->name,
//	       main->curlib ? "LIB" : "MAIN", main->versionfile, main->subversionfile);

	do_versions_after_linking_270(main);
}

static void lib_link_all(FileData *fd, Main *main)
{
	oldnewmap_sort(fd);
	
	/* No load UI for undo memfiles */
	if (fd->memfile == NULL) {
		lib_link_windowmanager(fd, main);
	}
	/* DO NOT skip screens here, 3Dview may contains pointers to other ID data (like bgpic)! See T41411. */
	lib_link_screen(fd, main);
	lib_link_scene(fd, main);
	lib_link_object(fd, main);
	lib_link_mesh(fd, main);
	lib_link_curve(fd, main);
	lib_link_mball(fd, main);
	lib_link_material(fd, main);
	lib_link_texture(fd, main);
	lib_link_image(fd, main);
	lib_link_ipo(fd, main);        /* XXX deprecated... still needs to be maintained for version patches still */
	lib_link_key(fd, main);
	lib_link_world(fd, main);
	lib_link_lamp(fd, main);
	lib_link_latt(fd, main);
	lib_link_text(fd, main);
	lib_link_camera(fd, main);
	lib_link_speaker(fd, main);
	lib_link_sound(fd, main);
	lib_link_group(fd, main);
	lib_link_armature(fd, main);
	lib_link_action(fd, main);
	lib_link_vfont(fd, main);
	lib_link_nodetree(fd, main);   /* has to be done after scene/materials, this will verify group nodes */
	lib_link_brush(fd, main);
	lib_link_palette(fd, main);
	lib_link_paint_curve(fd, main);
	lib_link_particlesettings(fd, main);
	lib_link_movieclip(fd, main);
	lib_link_mask(fd, main);
	lib_link_linestyle(fd, main);
	lib_link_gpencil(fd, main);
	lib_link_cachefiles(fd, main);

	lib_link_library(fd, main);    /* only init users */
}

static void direct_link_keymapitem(FileData *fd, wmKeyMapItem *kmi)
{
	kmi->properties = newdataadr(fd, kmi->properties);
	IDP_DirectLinkGroup_OrFree(&kmi->properties, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
	kmi->ptr = NULL;
	kmi->flag &= ~KMI_UPDATE;
}

static BHead *read_userdef(BlendFileData *bfd, FileData *fd, BHead *bhead)
{
	UserDef *user;
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	wmKeyMapDiffItem *kmdi;
	bAddon *addon;
	
	bfd->user = user= read_struct(fd, bhead, "user def");
	
	/* User struct has separate do-version handling */
	user->versionfile = bfd->main->versionfile;
	user->subversionfile = bfd->main->subversionfile;
	
	/* read all data into fd->datamap */
	bhead = read_data_into_oldnewmap(fd, bhead, "user def");
	
	if (user->keymaps.first) {
		/* backwards compatibility */
		user->user_keymaps= user->keymaps;
		user->keymaps.first= user->keymaps.last= NULL;
	}
	
	link_list(fd, &user->themes);
	link_list(fd, &user->user_keymaps);
	link_list(fd, &user->addons);
	link_list(fd, &user->autoexec_paths);

	for (keymap=user->user_keymaps.first; keymap; keymap=keymap->next) {
		keymap->modal_items= NULL;
		keymap->poll = NULL;
		keymap->flag &= ~KEYMAP_UPDATE;
		
		link_list(fd, &keymap->diff_items);
		link_list(fd, &keymap->items);
		
		for (kmdi=keymap->diff_items.first; kmdi; kmdi=kmdi->next) {
			kmdi->remove_item= newdataadr(fd, kmdi->remove_item);
			kmdi->add_item= newdataadr(fd, kmdi->add_item);
			
			if (kmdi->remove_item)
				direct_link_keymapitem(fd, kmdi->remove_item);
			if (kmdi->add_item)
				direct_link_keymapitem(fd, kmdi->add_item);
		}
		
		for (kmi=keymap->items.first; kmi; kmi=kmi->next)
			direct_link_keymapitem(fd, kmi);
	}

	for (addon = user->addons.first; addon; addon = addon->next) {
		addon->prop = newdataadr(fd, addon->prop);
		IDP_DirectLinkGroup_OrFree(&addon->prop, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
	}

	// XXX
	user->uifonts.first = user->uifonts.last= NULL;
	
	link_list(fd, &user->uistyles);
	
	/* free fd->datamap again */
	oldnewmap_free_unused(fd->datamap);
	oldnewmap_clear(fd->datamap);
	
	return bhead;
}

BlendFileData *blo_read_file_internal(FileData *fd, const char *filepath)
{
	BHead *bhead = blo_firstbhead(fd);
	BlendFileData *bfd;
	ListBase mainlist = {NULL, NULL};
	
	bfd = MEM_callocN(sizeof(BlendFileData), "blendfiledata");
	bfd->main = BKE_main_new();
	BLI_addtail(&mainlist, bfd->main);
	fd->mainlist = &mainlist;
	
	bfd->main->versionfile = fd->fileversion;
	
	bfd->type = BLENFILETYPE_BLEND;
	BLI_strncpy(bfd->main->name, filepath, sizeof(bfd->main->name));

	if (G.background) {
		/* We only read & store .blend thumbnail in background mode
		 * (because we cannot re-generate it, no OpenGL available).
		 */
		const int *data = read_file_thumbnail(fd);

		if (data) {
			int width = data[0];
			int height = data[1];

			/* Protect against buffer overflow vulnerability. */
			if (BLEN_THUMB_SAFE_MEMSIZE(width, height)) {
				const size_t sz = BLEN_THUMB_MEMSIZE(width, height);
				bfd->main->blen_thumb = MEM_mallocN(sz, __func__);

				BLI_assert((sz - sizeof(*bfd->main->blen_thumb)) ==
				           (BLEN_THUMB_MEMSIZE_FILE(width, height) - (sizeof(*data) * 2)));
				bfd->main->blen_thumb->width = width;
				bfd->main->blen_thumb->height = height;
				memcpy(bfd->main->blen_thumb->rect, &data[2], sz - sizeof(*bfd->main->blen_thumb));
			}
		}
	}

	while (bhead) {
		switch (bhead->code) {
		case DATA:
		case DNA1:
		case TEST: /* used as preview since 2.5x */
		case REND:
			bhead = blo_nextbhead(fd, bhead);
			break;
		case GLOB:
			bhead = read_global(bfd, fd, bhead);
			break;
		case USER:
			if (fd->skip_flags & BLO_READ_SKIP_USERDEF) {
				bhead = blo_nextbhead(fd, bhead);
			}
			else {
				bhead = read_userdef(bfd, fd, bhead);
			}
			break;
		case ENDB:
			bhead = NULL;
			break;
		
		case ID_ID:
			/* Always adds to the most recently loaded ID_LI block, see direct_link_library.
			 * This is part of the file format definition. */
			if (fd->skip_flags & BLO_READ_SKIP_DATA) {
				bhead = blo_nextbhead(fd, bhead);
			}
			else {
				bhead = read_libblock(fd, mainlist.last, bhead, LIB_TAG_READ | LIB_TAG_EXTERN, NULL);
			}
			break;
			/* in 2.50+ files, the file identifier for screens is patched, forward compatibility */
		case ID_SCRN:
			bhead->code = ID_SCR;
			/* pass on to default */
			ATTR_FALLTHROUGH;
		default:
			if (fd->skip_flags & BLO_READ_SKIP_DATA) {
				bhead = blo_nextbhead(fd, bhead);
			}
			else {
				bhead = read_libblock(fd, bfd->main, bhead, LIB_TAG_LOCAL, NULL);
			}
		}
	}
	
	/* do before read_libraries, but skip undo case */
	if (fd->memfile == NULL) {
		do_versions(fd, NULL, bfd->main);
		do_versions_userdef(fd, bfd);
	}
	
	read_libraries(fd, &mainlist);
	
	blo_join_main(&mainlist);
	
	lib_link_all(fd, bfd->main);

	/* Skip in undo case. */
	if (fd->memfile == NULL) {
		/* Yep, second splitting... but this is a very cheap operation, so no big deal. */
		blo_split_main(&mainlist, bfd->main);
		for (Main *mainvar = mainlist.first; mainvar; mainvar = mainvar->next) {
			BLI_assert(mainvar->versionfile != 0);
			do_versions_after_linking(mainvar);
		}
		blo_join_main(&mainlist);
	}

	BKE_main_id_tag_all(bfd->main, LIB_TAG_NEW, false);

	lib_verify_nodetree(bfd->main, true);
	fix_relpaths_library(fd->relabase, bfd->main); /* make all relative paths, relative to the open blend file */
	
	link_global(fd, bfd);	/* as last */
	
	fd->mainlist = NULL;  /* Safety, this is local variable, shall not be used afterward. */

	return bfd;
}

/* ************* APPEND LIBRARY ************** */

struct BHeadSort {
	BHead *bhead;
	const void *old;
};

static int verg_bheadsort(const void *v1, const void *v2)
{
	const struct BHeadSort *x1=v1, *x2=v2;
	
	if (x1->old > x2->old) return 1;
	else if (x1->old < x2->old) return -1;
	return 0;
}

static void sort_bhead_old_map(FileData *fd)
{
	BHead *bhead;
	struct BHeadSort *bhs;
	int tot = 0;
	
	for (bhead = blo_firstbhead(fd); bhead; bhead = blo_nextbhead(fd, bhead))
		tot++;
	
	fd->tot_bheadmap = tot;
	if (tot == 0) return;
	
	bhs = fd->bheadmap = MEM_malloc_arrayN(tot, sizeof(struct BHeadSort), "BHeadSort");
	
	for (bhead = blo_firstbhead(fd); bhead; bhead = blo_nextbhead(fd, bhead), bhs++) {
		bhs->bhead = bhead;
		bhs->old = bhead->old;
	}
	
	qsort(fd->bheadmap, tot, sizeof(struct BHeadSort), verg_bheadsort);
}

static BHead *find_previous_lib(FileData *fd, BHead *bhead)
{
	/* skip library datablocks in undo, see comment in read_libblock */
	if (fd->memfile)
		return NULL;

	for (; bhead; bhead = blo_prevbhead(fd, bhead)) {
		if (bhead->code == ID_LI)
			break;
	}

	return bhead;
}

static BHead *find_bhead(FileData *fd, void *old)
{
#if 0
	BHead *bhead;
#endif
	struct BHeadSort *bhs, bhs_s;
	
	if (!old)
		return NULL;

	if (fd->bheadmap == NULL)
		sort_bhead_old_map(fd);
	
	bhs_s.old = old;
	bhs = bsearch(&bhs_s, fd->bheadmap, fd->tot_bheadmap, sizeof(struct BHeadSort), verg_bheadsort);

	if (bhs)
		return bhs->bhead;
	
#if 0
	for (bhead = blo_firstbhead(fd); bhead; bhead= blo_nextbhead(fd, bhead)) {
		if (bhead->old == old)
			return bhead;
	}
#endif

	return NULL;
}

static BHead *find_bhead_from_code_name(FileData *fd, const short idcode, const char *name)
{
#ifdef USE_GHASH_BHEAD

	char idname_full[MAX_ID_NAME];

	*((short *)idname_full) = idcode;
	BLI_strncpy(idname_full + 2, name, sizeof(idname_full) - 2);

	return BLI_ghash_lookup(fd->bhead_idname_hash, idname_full);

#else
	BHead *bhead;

	for (bhead = blo_firstbhead(fd); bhead; bhead = blo_nextbhead(fd, bhead)) {
		if (bhead->code == idcode) {
			const char *idname_test = bhead_id_name(fd, bhead);
			if (STREQ(idname_test + 2, name)) {
				return bhead;
			}
		}
		else if (bhead->code == ENDB) {
			break;
		}
	}

	return NULL;
#endif
}

static BHead *find_bhead_from_idname(FileData *fd, const char *idname)
{
#ifdef USE_GHASH_BHEAD
	return BLI_ghash_lookup(fd->bhead_idname_hash, idname);
#else
	return find_bhead_from_code_name(fd, GS(idname), idname + 2);
#endif
}

static ID *is_yet_read(FileData *fd, Main *mainvar, BHead *bhead)
{
	const char *idname= bhead_id_name(fd, bhead);
	/* which_libbase can be NULL, intentionally not using idname+2 */
	return BLI_findstring(which_libbase(mainvar, GS(idname)), idname, offsetof(ID, name));
}

static void expand_doit_library(void *fdhandle, Main *mainvar, void *old)
{
	BHead *bhead;
	FileData *fd = fdhandle;
	ID *id;
	
	bhead = find_bhead(fd, old);
	if (bhead) {
		/* from another library? */
		if (bhead->code == ID_ID) {
			BHead *bheadlib= find_previous_lib(fd, bhead);
			
			if (bheadlib) {
				Library *lib = read_struct(fd, bheadlib, "Library");
				Main *ptr = blo_find_main(fd, lib->name, fd->relabase);
				
				if (ptr->curlib == NULL) {
					const char *idname= bhead_id_name(fd, bhead);
					
					blo_reportf_wrap(fd->reports, RPT_WARNING, TIP_("LIB: Data refers to main .blend file: '%s' from %s"),
					                 idname, mainvar->curlib->filepath);
					return;
				}
				else
					id = is_yet_read(fd, ptr, bhead);
				
				if (id == NULL) {
					read_libblock(fd, ptr, bhead, LIB_TAG_READ | LIB_TAG_INDIRECT, NULL);
					// commented because this can print way too much
					// if (G.debug & G_DEBUG) printf("expand_doit: other lib %s\n", lib->name);
					
					/* for outliner dependency only */
					ptr->curlib->parent = mainvar->curlib;
				}
				else {
					/* The line below was commented by Ton (I assume), when Hos did the merge from the orange branch. rev 6568
					 * This line is NEEDED, the case is that you have 3 blend files...
					 * user.blend, lib.blend and lib_indirect.blend - if user.blend already references a "tree" from
					 * lib_indirect.blend but lib.blend does too, linking in a Scene or Group from lib.blend can result in an
					 * empty without the dupli group referenced. Once you save and reload the group would appear. - Campbell */
					/* This crashes files, must look further into it */
					
					/* Update: the issue is that in file reading, the oldnewmap is OK, but for existing data, it has to be
					 * inserted in the map to be found! */
					
					/* Update: previously it was checking for id->tag & LIB_TAG_PRE_EXISTING, however that
					 * does not affect file reading. For file reading we may need to insert it into the libmap as well,
					 * because you might have two files indirectly linking the same datablock, and in that case
					 * we need this in the libmap for the fd of both those files.
					 *
					 * The crash that this check avoided earlier was because bhead->code wasn't properly passed in, making
					 * change_idid_adr not detect the mapping was for an ID_ID datablock. */
					oldnewmap_insert(fd->libmap, bhead->old, id, bhead->code);
					change_idid_adr_fd(fd, bhead->old, id);
					
					// commented because this can print way too much
					// if (G.debug & G_DEBUG) printf("expand_doit: already linked: %s lib: %s\n", id->name, lib->name);
				}
				
				MEM_freeN(lib);
			}
		}
		else {
			id = is_yet_read(fd, mainvar, bhead);
			if (id == NULL) {
				read_libblock(fd, mainvar, bhead, LIB_TAG_TESTIND, NULL);
			}
			else {
				/* this is actually only needed on UI call? when ID was already read before, and another append
				 * happens which invokes same ID... in that case the lookup table needs this entry */
				oldnewmap_insert(fd->libmap, bhead->old, id, bhead->code);
				// commented because this can print way too much
				// if (G.debug & G_DEBUG) printf("expand: already read %s\n", id->name);
			}
		}
	}
}

static BLOExpandDoitCallback expand_doit;

// XXX deprecated - old animation system
static void expand_ipo(FileData *fd, Main *mainvar, Ipo *ipo)
{
	IpoCurve *icu;
	for (icu = ipo->curve.first; icu; icu = icu->next) {
		if (icu->driver)
			expand_doit(fd, mainvar, icu->driver->ob);
	}
}

// XXX deprecated - old animation system
static void expand_constraint_channels(FileData *fd, Main *mainvar, ListBase *chanbase)
{
	bConstraintChannel *chan;
	for (chan = chanbase->first; chan; chan = chan->next) {
		expand_doit(fd, mainvar, chan->ipo);
	}
}

static void expand_idprops(FileData *fd, Main *mainvar, IDProperty *prop)
{
	if (!prop)
		return;

	switch (prop->type) {
		case IDP_ID:
			expand_doit(fd, mainvar, IDP_Id(prop));
			break;
		case IDP_IDPARRAY:
		{
			IDProperty *idp_array = IDP_IDPArray(prop);
			for (int i = 0; i < prop->len; i++) {
				expand_idprops(fd, mainvar, &idp_array[i]);
			}
			break;
		}
		case IDP_GROUP:
			for (IDProperty *loop = prop->data.group.first; loop; loop = loop->next) {
				expand_idprops(fd, mainvar, loop);
			}
			break;
	}
}

static void expand_fmodifiers(FileData *fd, Main *mainvar, ListBase *list)
{
	FModifier *fcm;
	
	for (fcm = list->first; fcm; fcm = fcm->next) {
		/* library data for specific F-Modifier types */
		switch (fcm->type) {
			case FMODIFIER_TYPE_PYTHON:
			{
				FMod_Python *data = (FMod_Python *)fcm->data;
				
				expand_doit(fd, mainvar, data->script);

				break;
			}
		}
	}
}

static void expand_fcurves(FileData *fd, Main *mainvar, ListBase *list)
{
	FCurve *fcu;
	
	for (fcu = list->first; fcu; fcu = fcu->next) {
		/* Driver targets if there is a driver */
		if (fcu->driver) {
			ChannelDriver *driver = fcu->driver;
			DriverVar *dvar;
			
			for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
				DRIVER_TARGETS_LOOPER(dvar) 
				{
					// TODO: only expand those that are going to get used?
					expand_doit(fd, mainvar, dtar->id);
				}
				DRIVER_TARGETS_LOOPER_END
			}
		}
		
		/* F-Curve Modifiers */
		expand_fmodifiers(fd, mainvar, &fcu->modifiers);
	}
}

static void expand_action(FileData *fd, Main *mainvar, bAction *act)
{
	bActionChannel *chan;
	
	// XXX deprecated - old animation system --------------
	for (chan=act->chanbase.first; chan; chan=chan->next) {
		expand_doit(fd, mainvar, chan->ipo);
		expand_constraint_channels(fd, mainvar, &chan->constraintChannels);
	}
	// ---------------------------------------------------
	
	/* F-Curves in Action */
	expand_fcurves(fd, mainvar, &act->curves);

	for (TimeMarker *marker = act->markers.first; marker; marker = marker->next) {
		if (marker->camera) {
			expand_doit(fd, mainvar, marker->camera);
		}
	}
}

static void expand_keyingsets(FileData *fd, Main *mainvar, ListBase *list)
{
	KeyingSet *ks;
	KS_Path *ksp;
	
	/* expand the ID-pointers in KeyingSets's paths */
	for (ks = list->first; ks; ks = ks->next) {
		for (ksp = ks->paths.first; ksp; ksp = ksp->next) {
			expand_doit(fd, mainvar, ksp->id);
		}
	}
}

static void expand_animdata_nlastrips(FileData *fd, Main *mainvar, ListBase *list)
{
	NlaStrip *strip;
	
	for (strip= list->first; strip; strip= strip->next) {
		/* check child strips */
		expand_animdata_nlastrips(fd, mainvar, &strip->strips);
		
		/* check F-Curves */
		expand_fcurves(fd, mainvar, &strip->fcurves);
		
		/* check F-Modifiers */
		expand_fmodifiers(fd, mainvar, &strip->modifiers);
		
		/* relink referenced action */
		expand_doit(fd, mainvar, strip->act);
	}
}

static void expand_animdata(FileData *fd, Main *mainvar, AnimData *adt)
{
	NlaTrack *nlt;
	
	/* own action */
	expand_doit(fd, mainvar, adt->action);
	expand_doit(fd, mainvar, adt->tmpact);
	
	/* drivers - assume that these F-Curves have driver data to be in this list... */
	expand_fcurves(fd, mainvar, &adt->drivers);
	
	/* nla-data - referenced actions */
	for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) 
		expand_animdata_nlastrips(fd, mainvar, &nlt->strips);
}	

static void expand_particlesettings(FileData *fd, Main *mainvar, ParticleSettings *part)
{
	int a;
	
	expand_doit(fd, mainvar, part->dup_ob);
	expand_doit(fd, mainvar, part->dup_group);
	expand_doit(fd, mainvar, part->eff_group);
	expand_doit(fd, mainvar, part->bb_ob);
	expand_doit(fd, mainvar, part->collision_group);
	
	if (part->adt)
		expand_animdata(fd, mainvar, part->adt);
	
	for (a = 0; a < MAX_MTEX; a++) {
		if (part->mtex[a]) {
			expand_doit(fd, mainvar, part->mtex[a]->tex);
			expand_doit(fd, mainvar, part->mtex[a]->object);
		}
	}

	if (part->effector_weights) {
		expand_doit(fd, mainvar, part->effector_weights->group);
	}

	if (part->pd) {
		expand_doit(fd, mainvar, part->pd->tex);
		expand_doit(fd, mainvar, part->pd->f_source);
	}
	if (part->pd2) {
		expand_doit(fd, mainvar, part->pd2->tex);
		expand_doit(fd, mainvar, part->pd2->f_source);
	}

	if (part->boids) {
		BoidState *state;
		BoidRule *rule;

		for (state = part->boids->states.first; state; state = state->next) {
			for (rule = state->rules.first; rule; rule = rule->next) {
				if (rule->type == eBoidRuleType_Avoid) {
					BoidRuleGoalAvoid *gabr = (BoidRuleGoalAvoid *)rule;
					expand_doit(fd, mainvar, gabr->ob);
				}
				else if (rule->type == eBoidRuleType_FollowLeader) {
					BoidRuleFollowLeader *flbr = (BoidRuleFollowLeader *)rule;
					expand_doit(fd, mainvar, flbr->ob);
				}
			}
		}
	}
}

static void expand_group(FileData *fd, Main *mainvar, Group *group)
{
	GroupObject *go;
	
	for (go = group->gobject.first; go; go = go->next) {
		expand_doit(fd, mainvar, go->ob);
	}
}

static void expand_key(FileData *fd, Main *mainvar, Key *key)
{
	expand_doit(fd, mainvar, key->ipo); // XXX deprecated - old animation system
	
	if (key->adt)
		expand_animdata(fd, mainvar, key->adt);
}

static void expand_nodetree(FileData *fd, Main *mainvar, bNodeTree *ntree)
{
	bNode *node;
	bNodeSocket *sock;
	
	if (ntree->adt)
		expand_animdata(fd, mainvar, ntree->adt);
		
	if (ntree->gpd)
		expand_doit(fd, mainvar, ntree->gpd);
	
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->id && node->type != CMP_NODE_R_LAYERS) {
			expand_doit(fd, mainvar, node->id);
		}

		expand_idprops(fd, mainvar, node->prop);

		for (sock = node->inputs.first; sock; sock = sock->next)
			expand_doit(fd, mainvar, sock->prop);
		for (sock = node->outputs.first; sock; sock = sock->next)
			expand_doit(fd, mainvar, sock->prop);
	}

	for (sock = ntree->inputs.first; sock; sock = sock->next)
		expand_doit(fd, mainvar, sock->prop);
	for (sock = ntree->outputs.first; sock; sock = sock->next)
		expand_doit(fd, mainvar, sock->prop);
}

static void expand_texture(FileData *fd, Main *mainvar, Tex *tex)
{
	expand_doit(fd, mainvar, tex->ima);
	expand_doit(fd, mainvar, tex->ipo); // XXX deprecated - old animation system
	
	if (tex->adt)
		expand_animdata(fd, mainvar, tex->adt);
	
	if (tex->nodetree)
		expand_nodetree(fd, mainvar, tex->nodetree);
}

static void expand_brush(FileData *fd, Main *mainvar, Brush *brush)
{
	expand_doit(fd, mainvar, brush->mtex.tex);
	expand_doit(fd, mainvar, brush->mask_mtex.tex);
	expand_doit(fd, mainvar, brush->clone.image);
	expand_doit(fd, mainvar, brush->paint_curve);
}

static void expand_material(FileData *fd, Main *mainvar, Material *ma)
{
	int a;
	
	for (a = 0; a < MAX_MTEX; a++) {
		if (ma->mtex[a]) {
			expand_doit(fd, mainvar, ma->mtex[a]->tex);
			expand_doit(fd, mainvar, ma->mtex[a]->object);
		}
	}
	
	expand_doit(fd, mainvar, ma->ipo); // XXX deprecated - old animation system
	
	if (ma->adt)
		expand_animdata(fd, mainvar, ma->adt);
	
	if (ma->nodetree)
		expand_nodetree(fd, mainvar, ma->nodetree);
	
	if (ma->group)
		expand_doit(fd, mainvar, ma->group);
}

static void expand_lamp(FileData *fd, Main *mainvar, Lamp *la)
{
	int a;
	
	for (a = 0; a < MAX_MTEX; a++) {
		if (la->mtex[a]) {
			expand_doit(fd, mainvar, la->mtex[a]->tex);
			expand_doit(fd, mainvar, la->mtex[a]->object);
		}
	}
	
	expand_doit(fd, mainvar, la->ipo); // XXX deprecated - old animation system
	
	if (la->adt)
		expand_animdata(fd, mainvar, la->adt);
	
	if (la->nodetree)
		expand_nodetree(fd, mainvar, la->nodetree);
}

static void expand_lattice(FileData *fd, Main *mainvar, Lattice *lt)
{
	expand_doit(fd, mainvar, lt->ipo); // XXX deprecated - old animation system
	expand_doit(fd, mainvar, lt->key);
	
	if (lt->adt)
		expand_animdata(fd, mainvar, lt->adt);
}


static void expand_world(FileData *fd, Main *mainvar, World *wrld)
{
	int a;
	
	for (a = 0; a < MAX_MTEX; a++) {
		if (wrld->mtex[a]) {
			expand_doit(fd, mainvar, wrld->mtex[a]->tex);
			expand_doit(fd, mainvar, wrld->mtex[a]->object);
		}
	}
	
	expand_doit(fd, mainvar, wrld->ipo); // XXX deprecated - old animation system
	
	if (wrld->adt)
		expand_animdata(fd, mainvar, wrld->adt);
	
	if (wrld->nodetree)
		expand_nodetree(fd, mainvar, wrld->nodetree);
}


static void expand_mball(FileData *fd, Main *mainvar, MetaBall *mb)
{
	int a;
	
	for (a = 0; a < mb->totcol; a++) {
		expand_doit(fd, mainvar, mb->mat[a]);
	}
	
	if (mb->adt)
		expand_animdata(fd, mainvar, mb->adt);
}

static void expand_curve(FileData *fd, Main *mainvar, Curve *cu)
{
	int a;
	
	for (a = 0; a < cu->totcol; a++) {
		expand_doit(fd, mainvar, cu->mat[a]);
	}
	
	expand_doit(fd, mainvar, cu->vfont);
	expand_doit(fd, mainvar, cu->vfontb);
	expand_doit(fd, mainvar, cu->vfonti);
	expand_doit(fd, mainvar, cu->vfontbi);
	expand_doit(fd, mainvar, cu->key);
	expand_doit(fd, mainvar, cu->ipo); // XXX deprecated - old animation system
	expand_doit(fd, mainvar, cu->bevobj);
	expand_doit(fd, mainvar, cu->taperobj);
	expand_doit(fd, mainvar, cu->textoncurve);
	
	if (cu->adt)
		expand_animdata(fd, mainvar, cu->adt);
}

static void expand_mesh(FileData *fd, Main *mainvar, Mesh *me)
{
	CustomDataLayer *layer;
	TFace *tf;
	int a, i;
	
	if (me->adt)
		expand_animdata(fd, mainvar, me->adt);
		
	for (a = 0; a < me->totcol; a++) {
		expand_doit(fd, mainvar, me->mat[a]);
	}
	
	expand_doit(fd, mainvar, me->key);
	expand_doit(fd, mainvar, me->texcomesh);
	
	if (me->tface) {
		tf = me->tface;
		for (i=0; i<me->totface; i++, tf++) {
			if (tf->tpage)
				expand_doit(fd, mainvar, tf->tpage);
		}
	}

	if (me->mface && !me->mpoly) {
		MTFace *mtf;

		for (a = 0; a < me->fdata.totlayer; a++) {
			layer = &me->fdata.layers[a];

			if (layer->type == CD_MTFACE) {
				mtf = (MTFace *) layer->data;
				for (i = 0; i < me->totface; i++, mtf++) {
					if (mtf->tpage)
						expand_doit(fd, mainvar, mtf->tpage);
				}
			}
		}
	}
	else {
		MTexPoly *mtp;

		for (a = 0; a < me->pdata.totlayer; a++) {
			layer = &me->pdata.layers[a];

			if (layer->type == CD_MTEXPOLY) {
				mtp = (MTexPoly *) layer->data;

				for (i = 0; i < me->totpoly; i++, mtp++) {
					if (mtp->tpage)
						expand_doit(fd, mainvar, mtp->tpage);
				}
			}
		}
	}
}

/* temp struct used to transport needed info to expand_constraint_cb() */
typedef struct tConstraintExpandData {
	FileData *fd;
	Main *mainvar;
} tConstraintExpandData;
/* callback function used to expand constraint ID-links */
static void expand_constraint_cb(bConstraint *UNUSED(con), ID **idpoin, bool UNUSED(is_reference), void *userdata)
{
	tConstraintExpandData *ced = (tConstraintExpandData *)userdata;
	expand_doit(ced->fd, ced->mainvar, *idpoin);
}

static void expand_constraints(FileData *fd, Main *mainvar, ListBase *lb)
{
	tConstraintExpandData ced;
	bConstraint *curcon;
	
	/* relink all ID-blocks used by the constraints */
	ced.fd = fd;
	ced.mainvar = mainvar;
	
	BKE_constraints_id_loop(lb, expand_constraint_cb, &ced);
	
	/* deprecated manual expansion stuff */
	for (curcon = lb->first; curcon; curcon = curcon->next) {
		if (curcon->ipo)
			expand_doit(fd, mainvar, curcon->ipo); // XXX deprecated - old animation system
	}
}

static void expand_pose(FileData *fd, Main *mainvar, bPose *pose)
{
	bPoseChannel *chan;
	
	if (!pose)
		return;
	
	for (chan = pose->chanbase.first; chan; chan = chan->next) {
		expand_constraints(fd, mainvar, &chan->constraints);
		expand_idprops(fd, mainvar, chan->prop);
		expand_doit(fd, mainvar, chan->custom);
	}
}

static void expand_bones(FileData *fd, Main *mainvar, Bone *bone)
{
	expand_idprops(fd, mainvar, bone->prop);

	for (Bone *curBone = bone->childbase.first; curBone; curBone = curBone->next) {
		expand_bones(fd, mainvar, curBone);
	}
}

static void expand_armature(FileData *fd, Main *mainvar, bArmature *arm)
{
	if (arm->adt)
		expand_animdata(fd, mainvar, arm->adt);

	for (Bone *curBone = arm->bonebase.first; curBone; curBone = curBone->next) {
		expand_bones(fd, mainvar, curBone);
	}
}

static void expand_object_expandModifiers(
        void *userData, Object *UNUSED(ob), ID **idpoin, int UNUSED(cb_flag))
{
	struct { FileData *fd; Main *mainvar; } *data= userData;
	
	FileData *fd = data->fd;
	Main *mainvar = data->mainvar;
	
	expand_doit(fd, mainvar, *idpoin);
}

static void expand_object(FileData *fd, Main *mainvar, Object *ob)
{
	ParticleSystem *psys;
	bSensor *sens;
	bController *cont;
	bActuator *act;
	bActionStrip *strip;
	PartEff *paf;
	int a;
	
	expand_doit(fd, mainvar, ob->data);
	
	/* expand_object_expandModifier() */
	if (ob->modifiers.first) {
		struct { FileData *fd; Main *mainvar; } data;
		data.fd = fd;
		data.mainvar = mainvar;
		
		modifiers_foreachIDLink(ob, expand_object_expandModifiers, (void *)&data);
	}
	
	expand_pose(fd, mainvar, ob->pose);
	expand_doit(fd, mainvar, ob->poselib);
	expand_constraints(fd, mainvar, &ob->constraints);
	
	expand_doit(fd, mainvar, ob->gpd);
	
// XXX deprecated - old animation system (for version patching only)
	expand_doit(fd, mainvar, ob->ipo);
	expand_doit(fd, mainvar, ob->action);
	
	expand_constraint_channels(fd, mainvar, &ob->constraintChannels);
	
	for (strip=ob->nlastrips.first; strip; strip=strip->next) {
		expand_doit(fd, mainvar, strip->object);
		expand_doit(fd, mainvar, strip->act);
		expand_doit(fd, mainvar, strip->ipo);
	}
// XXX deprecated - old animation system (for version patching only)
	
	if (ob->adt)
		expand_animdata(fd, mainvar, ob->adt);
	
	for (a = 0; a < ob->totcol; a++) {
		expand_doit(fd, mainvar, ob->mat[a]);
	}
	
	paf = blo_do_version_give_parteff_245(ob);
	if (paf && paf->group) 
		expand_doit(fd, mainvar, paf->group);
	
	if (ob->dup_group)
		expand_doit(fd, mainvar, ob->dup_group);
	
	if (ob->proxy)
		expand_doit(fd, mainvar, ob->proxy);
	if (ob->proxy_group)
		expand_doit(fd, mainvar, ob->proxy_group);
	
	for (psys = ob->particlesystem.first; psys; psys = psys->next)
		expand_doit(fd, mainvar, psys->part);

	for (sens = ob->sensors.first; sens; sens = sens->next) {
		if (sens->type == SENS_MESSAGE) {
			bMessageSensor *ms = sens->data;
			expand_doit(fd, mainvar, ms->fromObject);
		}
	}
	
	for (cont = ob->controllers.first; cont; cont = cont->next) {
		if (cont->type == CONT_PYTHON) {
			bPythonCont *pc = cont->data;
			expand_doit(fd, mainvar, pc->text);
		}
	}
	
	for (act = ob->actuators.first; act; act = act->next) {
		if (act->type == ACT_SOUND) {
			bSoundActuator *sa = act->data;
			expand_doit(fd, mainvar, sa->sound);
		}
		else if (act->type == ACT_CAMERA) {
			bCameraActuator *ca = act->data;
			expand_doit(fd, mainvar, ca->ob);
		}
		else if (act->type == ACT_EDIT_OBJECT) {
			bEditObjectActuator *eoa = act->data;
			if (eoa) {
				expand_doit(fd, mainvar, eoa->ob);
				expand_doit(fd, mainvar, eoa->me);
			}
		}
		else if (act->type == ACT_OBJECT) {
			bObjectActuator *oa = act->data;
			expand_doit(fd, mainvar, oa->reference);
		}
		else if (act->type == ACT_ADD_OBJECT) {
			bAddObjectActuator *aoa = act->data;
			expand_doit(fd, mainvar, aoa->ob);
		}
		else if (act->type == ACT_SCENE) {
			bSceneActuator *sa = act->data;
			expand_doit(fd, mainvar, sa->camera);
			expand_doit(fd, mainvar, sa->scene);
		}
		else if (act->type == ACT_2DFILTER) {
			bTwoDFilterActuator *tdfa = act->data;
			expand_doit(fd, mainvar, tdfa->text);
		}
		else if (act->type == ACT_ACTION) {
			bActionActuator *aa = act->data;
			expand_doit(fd, mainvar, aa->act);
		}
		else if (act->type == ACT_SHAPEACTION) {
			bActionActuator *aa = act->data;
			expand_doit(fd, mainvar, aa->act);
		}
		else if (act->type == ACT_PROPERTY) {
			bPropertyActuator *pa = act->data;
			expand_doit(fd, mainvar, pa->ob);
		}
		else if (act->type == ACT_MESSAGE) {
			bMessageActuator *ma = act->data;
			expand_doit(fd, mainvar, ma->toObject);
		}
		else if (act->type==ACT_PARENT) {
			bParentActuator *pa = act->data;
			expand_doit(fd, mainvar, pa->ob);
		}
		else if (act->type == ACT_ARMATURE) {
			bArmatureActuator *arma = act->data;
			expand_doit(fd, mainvar, arma->target);
		}
		else if (act->type == ACT_STEERING) {
			bSteeringActuator *sta = act->data;
			expand_doit(fd, mainvar, sta->target);
			expand_doit(fd, mainvar, sta->navmesh);
		}
	}
	
	if (ob->pd) {
		expand_doit(fd, mainvar, ob->pd->tex);
		expand_doit(fd, mainvar, ob->pd->f_source);
	}

	if (ob->soft) {
		expand_doit(fd, mainvar, ob->soft->collision_group);

		if (ob->soft->effector_weights) {
			expand_doit(fd, mainvar, ob->soft->effector_weights->group);
		}
	}

	if (ob->rigidbody_constraint) {
		expand_doit(fd, mainvar, ob->rigidbody_constraint->ob1);
		expand_doit(fd, mainvar, ob->rigidbody_constraint->ob2);
	}

	if (ob->currentlod) {
		LodLevel *level;
		for (level = ob->lodlevels.first; level; level = level->next) {
			expand_doit(fd, mainvar, level->source);
		}
	}
}

static void expand_scene(FileData *fd, Main *mainvar, Scene *sce)
{
	Base *base;
	SceneRenderLayer *srl;
	FreestyleModuleConfig *module;
	FreestyleLineSet *lineset;
	
	for (base = sce->base.first; base; base = base->next) {
		expand_doit(fd, mainvar, base->object);
	}
	expand_doit(fd, mainvar, sce->camera);
	expand_doit(fd, mainvar, sce->world);
	
	if (sce->adt)
		expand_animdata(fd, mainvar, sce->adt);
	expand_keyingsets(fd, mainvar, &sce->keyingsets);
	
	if (sce->set)
		expand_doit(fd, mainvar, sce->set);
	
	if (sce->nodetree)
		expand_nodetree(fd, mainvar, sce->nodetree);
	
	for (srl = sce->r.layers.first; srl; srl = srl->next) {
		expand_doit(fd, mainvar, srl->mat_override);
		expand_doit(fd, mainvar, srl->light_override);
		for (module = srl->freestyleConfig.modules.first; module; module = module->next) {
			if (module->script)
				expand_doit(fd, mainvar, module->script);
		}
		for (lineset = srl->freestyleConfig.linesets.first; lineset; lineset = lineset->next) {
			if (lineset->group)
				expand_doit(fd, mainvar, lineset->group);
			expand_doit(fd, mainvar, lineset->linestyle);
		}
	}
	
	if (sce->r.dometext)
		expand_doit(fd, mainvar, sce->gm.dome.warptext);
	
	if (sce->gpd)
		expand_doit(fd, mainvar, sce->gpd);
		
	if (sce->ed) {
		Sequence *seq;
		
		SEQ_BEGIN (sce->ed, seq)
		{
			expand_idprops(fd, mainvar, seq->prop);

			if (seq->scene) expand_doit(fd, mainvar, seq->scene);
			if (seq->scene_camera) expand_doit(fd, mainvar, seq->scene_camera);
			if (seq->clip) expand_doit(fd, mainvar, seq->clip);
			if (seq->mask) expand_doit(fd, mainvar, seq->mask);
			if (seq->sound) expand_doit(fd, mainvar, seq->sound);
		}
		SEQ_END
	}
	
	if (sce->rigidbody_world) {
		expand_doit(fd, mainvar, sce->rigidbody_world->group);
		expand_doit(fd, mainvar, sce->rigidbody_world->constraints);
	}

	for (TimeMarker *marker = sce->markers.first; marker; marker = marker->next) {
		if (marker->camera) {
			expand_doit(fd, mainvar, marker->camera);
		}
	}

	expand_doit(fd, mainvar, sce->clip);
}

static void expand_camera(FileData *fd, Main *mainvar, Camera *ca)
{
	expand_doit(fd, mainvar, ca->ipo); // XXX deprecated - old animation system
	
	if (ca->adt)
		expand_animdata(fd, mainvar, ca->adt);
}

static void expand_cachefile(FileData *fd, Main *mainvar, CacheFile *cache_file)
{
	if (cache_file->adt) {
		expand_animdata(fd, mainvar, cache_file->adt);
	}
}

static void expand_speaker(FileData *fd, Main *mainvar, Speaker *spk)
{
	expand_doit(fd, mainvar, spk->sound);

	if (spk->adt)
		expand_animdata(fd, mainvar, spk->adt);
}

static void expand_sound(FileData *fd, Main *mainvar, bSound *snd)
{
	expand_doit(fd, mainvar, snd->ipo); // XXX deprecated - old animation system
}

static void expand_movieclip(FileData *fd, Main *mainvar, MovieClip *clip)
{
	if (clip->adt)
		expand_animdata(fd, mainvar, clip->adt);
}

static void expand_mask_parent(FileData *fd, Main *mainvar, MaskParent *parent)
{
	if (parent->id) {
		expand_doit(fd, mainvar, parent->id);
	}
}

static void expand_mask(FileData *fd, Main *mainvar, Mask *mask)
{
	MaskLayer *mask_layer;

	if (mask->adt)
		expand_animdata(fd, mainvar, mask->adt);

	for (mask_layer = mask->masklayers.first; mask_layer; mask_layer = mask_layer->next) {
		MaskSpline *spline;

		for (spline = mask_layer->splines.first; spline; spline = spline->next) {
			int i;

			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];

				expand_mask_parent(fd, mainvar, &point->parent);
			}

			expand_mask_parent(fd, mainvar, &spline->parent);
		}
	}
}

static void expand_linestyle(FileData *fd, Main *mainvar, FreestyleLineStyle *linestyle)
{
	int a;
	LineStyleModifier *m;

	for (a = 0; a < MAX_MTEX; a++) {
		if (linestyle->mtex[a]) {
			expand_doit(fd, mainvar, linestyle->mtex[a]->tex);
			expand_doit(fd, mainvar, linestyle->mtex[a]->object);
		}
	}
	if (linestyle->nodetree)
		expand_nodetree(fd, mainvar, linestyle->nodetree);

	if (linestyle->adt)
		expand_animdata(fd, mainvar, linestyle->adt);
	for (m = linestyle->color_modifiers.first; m; m = m->next) {
		if (m->type == LS_MODIFIER_DISTANCE_FROM_OBJECT)
			expand_doit(fd, mainvar, ((LineStyleColorModifier_DistanceFromObject *)m)->target);
	}
	for (m = linestyle->alpha_modifiers.first; m; m = m->next) {
		if (m->type == LS_MODIFIER_DISTANCE_FROM_OBJECT)
			expand_doit(fd, mainvar, ((LineStyleAlphaModifier_DistanceFromObject *)m)->target);
	}
	for (m = linestyle->thickness_modifiers.first; m; m = m->next) {
		if (m->type == LS_MODIFIER_DISTANCE_FROM_OBJECT)
			expand_doit(fd, mainvar, ((LineStyleThicknessModifier_DistanceFromObject *)m)->target);
	}
}

static void expand_gpencil(FileData *fd, Main *mainvar, bGPdata *gpd)
{
	if (gpd->adt)
		expand_animdata(fd, mainvar, gpd->adt);
}

/**
 * Set the callback func used over all ID data found by \a BLO_expand_main func.
 *
 * \param expand_doit_func Called for each ID block it finds.
 */
void BLO_main_expander(BLOExpandDoitCallback expand_doit_func)
{
	expand_doit = expand_doit_func;
}

/**
 * Loop over all ID data in Main to mark relations.
 * Set (id->tag & LIB_TAG_NEED_EXPAND) to mark expanding. Flags get cleared after expanding.
 *
 * \param fdhandle usually filedata, or own handle.
 * \param mainvar the Main database to expand.
 */
void BLO_expand_main(void *fdhandle, Main *mainvar)
{
	ListBase *lbarray[MAX_LIBARRAY];
	FileData *fd = fdhandle;
	ID *id;
	int a;
	bool do_it = true;
	
	while (do_it) {
		do_it = false;
		
		a = set_listbasepointers(mainvar, lbarray);
		while (a--) {
			id = lbarray[a]->first;
			while (id) {
				if (id->tag & LIB_TAG_NEED_EXPAND) {
					expand_idprops(fd, mainvar, id->properties);

					switch (GS(id->name)) {
					case ID_OB:
						expand_object(fd, mainvar, (Object *)id);
						break;
					case ID_ME:
						expand_mesh(fd, mainvar, (Mesh *)id);
						break;
					case ID_CU:
						expand_curve(fd, mainvar, (Curve *)id);
						break;
					case ID_MB:
						expand_mball(fd, mainvar, (MetaBall *)id);
						break;
					case ID_SCE:
						expand_scene(fd, mainvar, (Scene *)id);
						break;
					case ID_MA:
						expand_material(fd, mainvar, (Material *)id);
						break;
					case ID_TE:
						expand_texture(fd, mainvar, (Tex *)id);
						break;
					case ID_WO:
						expand_world(fd, mainvar, (World *)id);
						break;
					case ID_LT:
						expand_lattice(fd, mainvar, (Lattice *)id);
						break;
					case ID_LA:
						expand_lamp(fd, mainvar, (Lamp *)id);
						break;
					case ID_KE:
						expand_key(fd, mainvar, (Key *)id);
						break;
					case ID_CA:
						expand_camera(fd, mainvar, (Camera *)id);
						break;
					case ID_SPK:
						expand_speaker(fd, mainvar, (Speaker *)id);
						break;
					case ID_SO:
						expand_sound(fd, mainvar, (bSound *)id);
						break;
					case ID_AR:
						expand_armature(fd, mainvar, (bArmature *)id);
						break;
					case ID_AC:
						expand_action(fd, mainvar, (bAction *)id); // XXX deprecated - old animation system
						break;
					case ID_GR:
						expand_group(fd, mainvar, (Group *)id);
						break;
					case ID_NT:
						expand_nodetree(fd, mainvar, (bNodeTree *)id);
						break;
					case ID_BR:
						expand_brush(fd, mainvar, (Brush *)id);
						break;
					case ID_IP:
						expand_ipo(fd, mainvar, (Ipo *)id); // XXX deprecated - old animation system
						break;
					case ID_PA:
						expand_particlesettings(fd, mainvar, (ParticleSettings *)id);
						break;
					case ID_MC:
						expand_movieclip(fd, mainvar, (MovieClip *)id);
						break;
					case ID_MSK:
						expand_mask(fd, mainvar, (Mask *)id);
						break;
					case ID_LS:
						expand_linestyle(fd, mainvar, (FreestyleLineStyle *)id);
						break;
					case ID_GD:
						expand_gpencil(fd, mainvar, (bGPdata *)id);
						break;
					case ID_CF:
						expand_cachefile(fd, mainvar, (CacheFile *)id);
						break;
					}
					
					do_it = true;
					id->tag &= ~LIB_TAG_NEED_EXPAND;
					
				}
				id = id->next;
			}
		}
	}
}


/* ***************************** */
	
static bool object_in_any_scene(Main *mainvar, Object *ob)
{
	Scene *sce;
	
	for (sce = mainvar->scene.first; sce; sce = sce->id.next) {
		if (BKE_scene_base_find(sce, ob)) {
			return true;
		}
	}
	
	return false;
}

static void give_base_to_objects(Main *mainvar, Scene *scene, View3D *v3d, Library *lib, const short flag)
{
	Object *ob;
	Base *base;
	const unsigned int active_lay = (flag & FILE_ACTIVELAY) ? BKE_screen_view3d_layer_active(v3d, scene) : 0;
	const bool is_link = (flag & FILE_LINK) != 0;

	BLI_assert(scene);

	/* give all objects which are LIB_TAG_INDIRECT a base, or for a group when *lib has been set */
	for (ob = mainvar->object.first; ob; ob = ob->id.next) {
		if ((ob->id.tag & LIB_TAG_INDIRECT) && (ob->id.tag & LIB_TAG_PRE_EXISTING) == 0) {
			bool do_it = false;

			if (ob->id.us == 0) {
				do_it = true;
			}
			else if (!is_link && (ob->id.lib == lib) && (object_in_any_scene(mainvar, ob) == 0)) {
				/* When appending, make sure any indirectly loaded objects get a base, else they cant be accessed at all
				 * (see T27437). */
				do_it = true;
			}

			if (do_it) {
				base = MEM_callocN(sizeof(Base), __func__);
				BLI_addtail(&scene->base, base);

				if (active_lay) {
					ob->lay = active_lay;
				}
				if (flag & FILE_AUTOSELECT) {
					/* Note that link_object_postprocess() already checks for FILE_AUTOSELECT flag,
					 * but it will miss objects from non-instanciated groups... */
					ob->flag |= SELECT;
					/* do NOT make base active here! screws up GUI stuff, if you want it do it on src/ level */
				}

				base->object = ob;
				base->lay = ob->lay;
				base->flag = ob->flag;

				CLAMP_MIN(ob->id.us, 0);
				id_us_plus_no_lib((ID *)ob);

				ob->id.tag &= ~LIB_TAG_INDIRECT;
				ob->id.tag |= LIB_TAG_EXTERN;
			}
		}
	}
}

static void give_base_to_groups(
        Main *mainvar, Scene *scene, View3D *v3d, Library *UNUSED(lib), const short UNUSED(flag))
{
	Group *group;
	Base *base;
	Object *ob;
	const unsigned int active_lay = BKE_screen_view3d_layer_active(v3d, scene);

	/* give all objects which are tagged a base */
	for (group = mainvar->group.first; group; group = group->id.next) {
		if (group->id.tag & LIB_TAG_DOIT) {
			/* any indirect group should not have been tagged */
			BLI_assert((group->id.tag & LIB_TAG_INDIRECT) == 0);

			/* BKE_object_add(...) messes with the selection */
			ob = BKE_object_add_only_object(mainvar, OB_EMPTY, group->id.name + 2);
			ob->type = OB_EMPTY;
			ob->lay = active_lay;

			/* assign the base */
			base = BKE_scene_base_add(scene, ob);
			base->flag |= SELECT;
			base->object->flag = base->flag;
			DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
			scene->basact = base;

			/* assign the group */
			ob->dup_group = group;
			ob->transflag |= OB_DUPLIGROUP;
			copy_v3_v3(ob->loc, scene->cursor);
		}
	}
}

static ID *create_placeholder(Main *mainvar, const short idcode, const char *idname, const short tag)
{
	ListBase *lb = which_libbase(mainvar, idcode);
	ID *ph_id = BKE_libblock_alloc_notest(idcode);

	*((short *)ph_id->name) = idcode;
	BLI_strncpy(ph_id->name + 2, idname, sizeof(ph_id->name) - 2);
	BKE_libblock_init_empty(ph_id);
	ph_id->lib = mainvar->curlib;
	ph_id->tag = tag | LIB_TAG_MISSING;
	ph_id->us = ID_FAKE_USERS(ph_id);
	ph_id->icon_id = 0;

	BLI_addtail(lb, ph_id);
	id_sort_by_name(lb, ph_id);

	return ph_id;
}

/* returns true if the item was found
 * but it may already have already been appended/linked */
static ID *link_named_part(
        Main *mainl, FileData *fd, const short idcode, const char *name,
        const bool use_placeholders, const bool force_indirect)
{
	BHead *bhead = find_bhead_from_code_name(fd, idcode, name);
	ID *id;

	BLI_assert(BKE_idcode_is_linkable(idcode) && BKE_idcode_is_valid(idcode));

	if (bhead) {
		id = is_yet_read(fd, mainl, bhead);
		if (id == NULL) {
			/* not read yet */
			read_libblock(fd, mainl, bhead, force_indirect ? LIB_TAG_TESTIND : LIB_TAG_TESTEXT, &id);

			if (id) {
				/* sort by name in list */
				ListBase *lb = which_libbase(mainl, idcode);
				id_sort_by_name(lb, id);
			}
		}
		else {
			/* already linked */
			if (G.debug)
				printf("append: already linked\n");
			oldnewmap_insert(fd->libmap, bhead->old, id, bhead->code);
			if (!force_indirect && (id->tag & LIB_TAG_INDIRECT)) {
				id->tag &= ~LIB_TAG_INDIRECT;
				id->tag |= LIB_TAG_EXTERN;
			}
		}
	}
	else if (use_placeholders) {
		/* XXX flag part is weak! */
		id = create_placeholder(mainl, idcode, name, force_indirect ? LIB_TAG_INDIRECT : LIB_TAG_EXTERN);
	}
	else {
		id = NULL;
	}
	
	/* if we found the id but the id is NULL, this is really bad */
	BLI_assert(!((bhead != NULL) && (id == NULL)));
	
	return id;
}

static void link_object_postprocess(ID *id, Scene *scene, View3D *v3d, const short flag)
{
	if (scene) {
		Base *base;
		Object *ob;

		base = MEM_callocN(sizeof(Base), "app_nam_part");
		BLI_addtail(&scene->base, base);

		ob = (Object *)id;

		/* link at active layer (view3d if available in context, else scene one */
		if (flag & FILE_ACTIVELAY) {
			ob->lay = BKE_screen_view3d_layer_active(v3d, scene);
		}

		ob->mode = OB_MODE_OBJECT;
		base->lay = ob->lay;
		base->object = ob;
		base->flag = ob->flag;
		id_us_plus_no_lib((ID *)ob);

		if (flag & FILE_AUTOSELECT) {
			base->flag |= SELECT;
			base->object->flag = base->flag;
			/* do NOT make base active here! screws up GUI stuff, if you want it do it on src/ level */
		}
	}
}

/**
 * Simple reader for copy/paste buffers.
 */
void BLO_library_link_copypaste(Main *mainl, BlendHandle *bh)
{
	FileData *fd = (FileData *)(bh);
	BHead *bhead;
	
	for (bhead = blo_firstbhead(fd); bhead; bhead = blo_nextbhead(fd, bhead)) {
		ID *id = NULL;

		if (bhead->code == ENDB)
			break;
		if (ELEM(bhead->code, ID_OB, ID_GR)) {
			read_libblock(fd, mainl, bhead, LIB_TAG_TESTIND, &id);
		}


		if (id) {
			/* sort by name in list */
			ListBase *lb = which_libbase(mainl, GS(id->name));
			id_sort_by_name(lb, id);

			if (bhead->code == ID_OB) {
				/* Instead of instancing Base's directly, postpone until after groups are loaded
				 * otherwise the base's flag is set incorrectly when groups are used */
				Object *ob = (Object *)id;
				ob->mode = OB_MODE_OBJECT;
				/* ensure give_base_to_objects runs on this object */
				BLI_assert(id->us == 0);
			}
		}
	}
}

static ID *link_named_part_ex(
        Main *mainl, FileData *fd, const short idcode, const char *name, const short flag,
        Scene *scene, View3D *v3d, const bool use_placeholders, const bool force_indirect)
{
	ID *id = link_named_part(mainl, fd, idcode, name, use_placeholders, force_indirect);

	if (id && (GS(id->name) == ID_OB)) {	/* loose object: give a base */
		link_object_postprocess(id, scene, v3d, flag);
	}
	else if (id && (GS(id->name) == ID_GR)) {
		/* tag as needing to be instantiated */
		if (flag & FILE_GROUP_INSTANCE)
			id->tag |= LIB_TAG_DOIT;
	}

	return id;
}

/**
 * Link a named datablock from an external blend file.
 *
 * \param mainl The main database to link from (not the active one).
 * \param bh The blender file handle.
 * \param idcode The kind of datablock to link.
 * \param name The name of the datablock (without the 2 char ID prefix).
 * \return the linked ID when found.
 */
ID *BLO_library_link_named_part(Main *mainl, BlendHandle **bh, const short idcode, const char *name)
{
	FileData *fd = (FileData*)(*bh);
	return link_named_part(mainl, fd, idcode, name, false, false);
}

/**
 * Link a named datablock from an external blend file.
 * Optionally instantiate the object/group in the scene when the flags are set.
 *
 * \param mainl The main database to link from (not the active one).
 * \param bh The blender file handle.
 * \param idcode The kind of datablock to link.
 * \param name The name of the datablock (without the 2 char ID prefix).
 * \param flag Options for linking, used for instantiating.
 * \param scene The scene in which to instantiate objects/groups (if NULL, no instantiation is done).
 * \param v3d The active View3D (only to define active layers for instantiated objects & groups, can be NULL).
 * \param use_placeholders If true, generate a placeholder (empty ID) if not found in current lib file.
 * \param force_indirect If true, force loaded ID to be tagged as LIB_TAG_INDIRECT (used in reload context only).
 * \return the linked ID when found.
 */
ID *BLO_library_link_named_part_ex(
        Main *mainl, BlendHandle **bh,
        const short idcode, const char *name, const short flag,
        Scene *scene, View3D *v3d,
        const bool use_placeholders, const bool force_indirect)
{
	FileData *fd = (FileData*)(*bh);
	return link_named_part_ex(mainl, fd, idcode, name, flag, scene, v3d, use_placeholders, force_indirect);
}

static void link_id_part(ReportList *reports, FileData *fd, Main *mainvar, ID *id, ID **r_id)
{
	BHead *bhead = NULL;
	const bool is_valid = BKE_idcode_is_linkable(GS(id->name)) || ((id->tag & LIB_TAG_EXTERN) == 0);

	if (fd) {
		bhead = find_bhead_from_idname(fd, id->name);
	}

	id->tag &= ~LIB_TAG_READ;

	if (!is_valid) {
		blo_reportf_wrap(
		        reports, RPT_ERROR,
		        TIP_("LIB: %s: '%s' is directly linked from '%s' (parent '%s'), but is a non-linkable data type"),
		        BKE_idcode_to_name(GS(id->name)),
		        id->name + 2,
		        mainvar->curlib->filepath,
		        library_parent_filepath(mainvar->curlib));
	}

	if (bhead) {
		id->tag |= LIB_TAG_NEED_EXPAND;
		// printf("read lib block %s\n", id->name);
		read_libblock(fd, mainvar, bhead, id->tag, r_id);
	}
	else {
		blo_reportf_wrap(
		        reports, RPT_WARNING,
		        TIP_("LIB: %s: '%s' missing from '%s', parent '%s'"),
		        BKE_idcode_to_name(GS(id->name)),
		        id->name + 2,
		        mainvar->curlib->filepath,
		        library_parent_filepath(mainvar->curlib));

		/* Generate a placeholder for this ID (simplified version of read_libblock actually...). */
		if (r_id) {
			*r_id = is_valid ? create_placeholder(mainvar, GS(id->name), id->name + 2, id->tag) : NULL;
		}
	}
}

/* common routine to append/link something from a library */

static Main *library_link_begin(Main *mainvar, FileData **fd, const char *filepath)
{
	Main *mainl;

	(*fd)->mainlist = MEM_callocN(sizeof(ListBase), "FileData.mainlist");
	
	/* clear for group instantiating tag */
	BKE_main_id_tag_listbase(&(mainvar->group), LIB_TAG_DOIT, false);

	/* make mains */
	blo_split_main((*fd)->mainlist, mainvar);
	
	/* which one do we need? */
	mainl = blo_find_main(*fd, filepath, G.main->name);
	
	/* needed for do_version */
	mainl->versionfile = (*fd)->fileversion;
	read_file_version(*fd, mainl);
#ifdef USE_GHASH_BHEAD
	read_file_bhead_idname_map_create(*fd);
#endif
	
	return mainl;
}

/**
 * Initialize the BlendHandle for linking library data.
 *
 * \param mainvar The current main database, e.g. G.main or CTX_data_main(C).
 * \param bh A blender file handle as returned by \a BLO_blendhandle_from_file or \a BLO_blendhandle_from_memory.
 * \param filepath Used for relative linking, copied to the \a lib->name.
 * \return the library Main, to be passed to \a BLO_library_append_named_part as \a mainl.
 */
Main *BLO_library_link_begin(Main *mainvar, BlendHandle **bh, const char *filepath)
{
	FileData *fd = (FileData*)(*bh);
	return library_link_begin(mainvar, &fd, filepath);
}

static void split_main_newid(Main *mainptr, Main *main_newid)
{
	/* We only copy the necessary subset of data in this temp main. */
	main_newid->versionfile = mainptr->versionfile;
	main_newid->subversionfile = mainptr->subversionfile;
	BLI_strncpy(main_newid->name, mainptr->name, sizeof(main_newid->name));
	main_newid->curlib = mainptr->curlib;

	ListBase *lbarray[MAX_LIBARRAY];
	ListBase *lbarray_newid[MAX_LIBARRAY];
	int i = set_listbasepointers(mainptr, lbarray);
	set_listbasepointers(main_newid, lbarray_newid);
	while (i--) {
		BLI_listbase_clear(lbarray_newid[i]);

		for (ID *id = lbarray[i]->first, *idnext; id; id = idnext) {
			idnext = id->next;

			if (id->tag & LIB_TAG_NEW) {
				BLI_remlink(lbarray[i], id);
				BLI_addtail(lbarray_newid[i], id);
			}
		}
	}
}

/* scene and v3d may be NULL. */
static void library_link_end(Main *mainl, FileData **fd, const short flag, Scene *scene, View3D *v3d)
{
	Main *mainvar;
	Library *curlib;

	/* expander now is callback function */
	BLO_main_expander(expand_doit_library);

	/* make main consistent */
	BLO_expand_main(*fd, mainl);

	/* do this when expand found other libs */
	read_libraries(*fd, (*fd)->mainlist);

	curlib = mainl->curlib;

	/* make the lib path relative if required */
	if (flag & FILE_RELPATH) {
		/* use the full path, this could have been read by other library even */
		BLI_strncpy(curlib->name, curlib->filepath, sizeof(curlib->name));

		/* uses current .blend file as reference */
		BLI_path_rel(curlib->name, G.main->name);
	}

	blo_join_main((*fd)->mainlist);
	mainvar = (*fd)->mainlist->first;
	mainl = NULL; /* blo_join_main free's mainl, cant use anymore */

	lib_link_all(*fd, mainvar);

	/* Yep, second splitting... but this is a very cheap operation, so no big deal. */
	blo_split_main((*fd)->mainlist, mainvar);
	Main main_newid = {0};
	for (mainvar = ((Main *)(*fd)->mainlist->first)->next; mainvar; mainvar = mainvar->next) {
		BLI_assert(mainvar->versionfile != 0);
		/* We need to split out IDs already existing, or they will go again through do_versions - bad, very bad! */
		split_main_newid(mainvar, &main_newid);

		do_versions_after_linking(&main_newid);

		add_main_to_main(mainvar, &main_newid);
	}
	blo_join_main((*fd)->mainlist);
	mainvar = (*fd)->mainlist->first;
	MEM_freeN((*fd)->mainlist);

	BKE_main_id_tag_all(mainvar, LIB_TAG_NEW, false);

	lib_verify_nodetree(mainvar, false);
	fix_relpaths_library(G.main->name, mainvar); /* make all relative paths, relative to the open blend file */

	/* Give a base to loose objects. If group append, do it for objects too.
	 * Only directly linked objects & groups are instantiated by `BLO_library_link_named_part_ex()` & co,
	 * here we handle indirect ones and other possible edge-cases. */
	if (scene) {
		give_base_to_objects(mainvar, scene, v3d, curlib, flag);

		if (flag & FILE_GROUP_INSTANCE) {
			give_base_to_groups(mainvar, scene, v3d, curlib, flag);
		}
	}
	else {
		/* printf("library_append_end, scene is NULL (objects wont get bases)\n"); */
	}

	/* clear group instantiating tag */
	BKE_main_id_tag_listbase(&(mainvar->group), LIB_TAG_DOIT, false);

	/* patch to prevent switch_endian happens twice */
	if ((*fd)->flags & FD_FLAGS_SWITCH_ENDIAN) {
		blo_freefiledata(*fd);
		*fd = NULL;
	}
}

/**
 * Finalize linking from a given .blend file (library).
 * Optionally instance the indirect object/group in the scene when the flags are set.
 * \note Do not use \a bh after calling this function, it may frees it.
 *
 * \param mainl The main database to link from (not the active one).
 * \param bh The blender file handle (WARNING! may be freed by this function!).
 * \param flag Options for linking, used for instantiating.
 * \param scene The scene in which to instantiate objects/groups (if NULL, no instantiation is done).
 * \param v3d The active View3D (only to define active layers for instantiated objects & groups, can be NULL).
 */
void BLO_library_link_end(Main *mainl, BlendHandle **bh, short flag, Scene *scene, View3D *v3d)
{
	FileData *fd = (FileData*)(*bh);
	library_link_end(mainl, &fd, flag, scene, v3d);
	*bh = (BlendHandle*)fd;
}

void *BLO_library_read_struct(FileData *fd, BHead *bh, const char *blockname)
{
	return read_struct(fd, bh, blockname);
}

/* ************* READ LIBRARY ************** */

static int mainvar_id_tag_any_check(Main *mainvar, const short tag)
{
	ListBase *lbarray[MAX_LIBARRAY];
	int a;
	
	a = set_listbasepointers(mainvar, lbarray);
	while (a--) {
		ID *id;
		
		for (id = lbarray[a]->first; id; id = id->next) {
			if (id->tag & tag) {
				return true;
			}
		}
	}
	return false;
}

static void read_libraries(FileData *basefd, ListBase *mainlist)
{
	Main *mainl = mainlist->first;
	Main *mainptr;
	ListBase *lbarray[MAX_LIBARRAY];
	int a;
	bool do_it = true;
	
	/* expander now is callback function */
	BLO_main_expander(expand_doit_library);
	
	while (do_it) {
		do_it = false;
		
		/* test 1: read libdata */
		mainptr= mainl->next;
		while (mainptr) {
			if (mainvar_id_tag_any_check(mainptr, LIB_TAG_READ)) {
				// printf("found LIB_TAG_READ %s\n", mainptr->curlib->name);

				FileData *fd = mainptr->curlib->filedata;
				
				if (fd == NULL) {
					
					/* printf and reports for now... its important users know this */
					
					/* if packed file... */
					if (mainptr->curlib->packedfile) {
						PackedFile *pf = mainptr->curlib->packedfile;
						
						blo_reportf_wrap(
						        basefd->reports, RPT_INFO, TIP_("Read packed library:  '%s', parent '%s'"),
						        mainptr->curlib->name,
						        library_parent_filepath(mainptr->curlib));
						fd = blo_openblendermemory(pf->data, pf->size, basefd->reports);
						
						
						/* needed for library_append and read_libraries */
						BLI_strncpy(fd->relabase, mainptr->curlib->filepath, sizeof(fd->relabase));
					}
					else {
						blo_reportf_wrap(
						        basefd->reports, RPT_INFO, TIP_("Read library:  '%s', '%s', parent '%s'"),
						        mainptr->curlib->filepath,
						        mainptr->curlib->name,
						        library_parent_filepath(mainptr->curlib));
						fd = blo_openblenderfile(mainptr->curlib->filepath, basefd->reports);
					}
					/* allow typing in a new lib path */
					if (G.debug_value == -666) {
						while (fd == NULL) {
							char newlib_path[FILE_MAX] = {0};
							printf("Missing library...'\n");
							printf("	current file: %s\n", G.main->name);
							printf("	absolute lib: %s\n", mainptr->curlib->filepath);
							printf("	relative lib: %s\n", mainptr->curlib->name);
							printf("  enter a new path:\n");
							
							if (scanf("%1023s", newlib_path) > 0) {  /* Warning, keep length in sync with FILE_MAX! */
								BLI_strncpy(mainptr->curlib->name, newlib_path, sizeof(mainptr->curlib->name));
								BLI_strncpy(mainptr->curlib->filepath, newlib_path, sizeof(mainptr->curlib->filepath));
								BLI_cleanup_path(G.main->name, mainptr->curlib->filepath);
								
								fd = blo_openblenderfile(mainptr->curlib->filepath, basefd->reports);

								if (fd) {
									fd->mainlist = mainlist;
									printf("found: '%s', party on macuno!\n", mainptr->curlib->filepath);
								}
							}
						}
					}
					
					if (fd) {
						/* share the mainlist, so all libraries are added immediately in a
						 * single list. it used to be that all FileData's had their own list,
						 * but with indirectly linking this meant we didn't catch duplicate
						 * libraries properly */
						fd->mainlist = mainlist;

						fd->reports = basefd->reports;
						
						if (fd->libmap)
							oldnewmap_free(fd->libmap);
						
						fd->libmap = oldnewmap_new();
						
						mainptr->curlib->filedata = fd;
						mainptr->versionfile=  fd->fileversion;
						
						/* subversion */
						read_file_version(fd, mainptr);
#ifdef USE_GHASH_BHEAD
						read_file_bhead_idname_map_create(fd);
#endif

					}
					else {
						mainptr->curlib->filedata = NULL;
						mainptr->curlib->id.tag |= LIB_TAG_MISSING;
						/* Set lib version to current main one... Makes assert later happy. */
						mainptr->versionfile = mainptr->curlib->versionfile = mainl->versionfile;
						mainptr->subversionfile = mainptr->curlib->subversionfile = mainl->subversionfile;
					}
					
					if (fd == NULL) {
						blo_reportf_wrap(basefd->reports, RPT_WARNING, TIP_("Cannot find lib '%s'"),
						                 mainptr->curlib->filepath);
					}
				}
				if (fd) {
					do_it = true;
				}
				a = set_listbasepointers(mainptr, lbarray);
				while (a--) {
					ID *id = lbarray[a]->first;

					while (id) {
						ID *idn = id->next;
						if (id->tag & LIB_TAG_READ) {
							ID *realid = NULL;
							BLI_remlink(lbarray[a], id);

							link_id_part(basefd->reports, fd, mainptr, id, &realid);

							/* realid shall never be NULL - unless some source file/lib is broken
							 * (known case: some directly linked shapekey from a missing lib...). */
							/* BLI_assert(realid != NULL); */

							change_idid_adr(mainlist, basefd, id, realid);

							MEM_freeN(id);
						}
						id = idn;
					}
				}
				BLO_expand_main(fd, mainptr);
			}
			
			mainptr = mainptr->next;
		}
	}
	
	/* test if there are unread libblocks */
	/* XXX This code block is kept for 2.77, until we are sure it never gets reached anymore. Can be removed later. */
	for (mainptr = mainl->next; mainptr; mainptr = mainptr->next) {
		a = set_listbasepointers(mainptr, lbarray);
		while (a--) {
			ID *id, *idn = NULL;
			
			for (id = lbarray[a]->first; id; id = idn) {
				idn = id->next;
				if (id->tag & LIB_TAG_READ) {
					BLI_assert(0);
					BLI_remlink(lbarray[a], id);
					blo_reportf_wrap(
					        basefd->reports, RPT_ERROR,
					        TIP_("LIB: %s: '%s' unread lib block missing from '%s', parent '%s' - "
					             "Please file a bug report if you see this message"),
					        BKE_idcode_to_name(GS(id->name)),
					        id->name + 2,
					        mainptr->curlib->filepath,
					        library_parent_filepath(mainptr->curlib));
					change_idid_adr(mainlist, basefd, id, NULL);
					
					MEM_freeN(id);
				}
			}
		}
	}
	
	/* do versions, link, and free */
	Main main_newid = {0};
	for (mainptr = mainl->next; mainptr; mainptr = mainptr->next) {
		/* some mains still have to be read, then versionfile is still zero! */
		if (mainptr->versionfile) {
			/* We need to split out IDs already existing, or they will go again through do_versions - bad, very bad! */
			split_main_newid(mainptr, &main_newid);

			if (mainptr->curlib->filedata) // can be zero... with shift+f1 append
				do_versions(mainptr->curlib->filedata, mainptr->curlib, &main_newid);
			else
				do_versions(basefd, NULL, &main_newid);

			add_main_to_main(mainptr, &main_newid);
		}
		
		if (mainptr->curlib->filedata)
			lib_link_all(mainptr->curlib->filedata, mainptr);
		
		if (mainptr->curlib->filedata) blo_freefiledata(mainptr->curlib->filedata);
		mainptr->curlib->filedata = NULL;
	}
}


/* reading runtime */

BlendFileData *blo_read_blendafterruntime(int file, const char *name, int actualsize, ReportList *reports)
{
	BlendFileData *bfd = NULL;
	FileData *fd = filedata_new();
	fd->filedes = file;
	fd->buffersize = actualsize;
	fd->read = fd_read_from_file;
	
	/* needed for library_append and read_libraries */
	BLI_strncpy(fd->relabase, name, sizeof(fd->relabase));
	
	fd = blo_decode_and_check(fd, reports);
	if (!fd)
		return NULL;
	
	fd->reports = reports;
	bfd = blo_read_file_internal(fd, "");
	blo_freefiledata(fd);
	
	return bfd;
}
