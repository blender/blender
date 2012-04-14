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

#ifndef WIN32
	#include <unistd.h> // for read close
#else
	#include <io.h> // for open close read
#include "winsock2.h"
#include "BLI_winstuff.h"
#endif

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_actuator_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
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
#include "DNA_meta_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_nla_types.h"
#include "DNA_node_types.h"
#include "DNA_object_fluidsim.h" // NT
#include "DNA_packedFile_types.h"
#include "DNA_particle_types.h"
#include "DNA_property_types.h"
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

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"

#include "BKE_anim.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_deform.h"
#include "BKE_effect.h"
#include "BKE_fcurve.h"
#include "BKE_global.h" // for G
#include "BKE_group.h"
#include "BKE_image.h"
#include "BKE_lattice.h"
#include "BKE_library.h" // for which_libbase
#include "BKE_idcode.h"
#include "BKE_material.h"
#include "BKE_main.h" // for Main
#include "BKE_mesh.h" // for ME_ defines (patching)
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_node.h" // for tree type defines
#include "BKE_ocean.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_property.h" // for get_ob_property
#include "BKE_report.h"
#include "BKE_sca.h" // for init_actuator
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_sequencer.h"
#include "BKE_text.h" // for txt_extended_ascii_as_utf8
#include "BKE_texture.h" // for open_plugin_tex
#include "BKE_tracking.h"
#include "BKE_utildefines.h" // SWITCH_INT DATA ENDB DNA1 O_BINARY GLOB USER TEST REND
#include "BKE_sound.h"

#include "IMB_imbuf.h"  // for proxy / timecode versioning stuff

#include "NOD_socket.h"

//XXX #include "BIF_butspace.h" // badlevel, for do_versions, patching event codes
//XXX #include "BIF_filelist.h" // badlevel too, where to move this? - elubie
//XXX #include "BIF_previewrender.h" // bedlelvel, for struct RenderInfo
#include "BLO_readfile.h"
#include "BLO_undofile.h"

#include "RE_engine.h"

#include "readfile.h"

#include "PIL_time.h"

#include <errno.h>

/*
 Remark: still a weak point is the newaddress() function, that doesnt solve reading from
 multiple files at the same time

 (added remark: oh, i thought that was solved? will look at that... (ton)

READ
- Existing Library (Main) push or free
- allocate new Main
- load file
- read SDNA
- for each LibBlock
	- read LibBlock
	- if a Library
		- make a new Main
		- attach ID's to it
	- else
		- read associated 'direct data'
		- link direct data (internal and to LibBlock)
- read FileGlobal
- read USER data, only when indicated (file is ~/X.XX/startup.blend)
- free file
- per Library (per Main)
	- read file
	- read SDNA
	- find LibBlocks and attach IDs to Main
		- if external LibBlock
			- search all Main's
				- or it's already read,
				- or not read yet
				- or make new Main
	- per LibBlock
		- read recursive
		- read associated direct data
		- link direct data (internal and to LibBlock)
	- free file
- per Library with unread LibBlocks
	- read file
	- read SDNA
	- per LibBlock
			   - read recursive
			   - read associated direct data
			   - link direct data (internal and to LibBlock)
		- free file
- join all Mains
- link all LibBlocks and indirect pointers to libblocks
- initialize FileGlobal and copy pointers to Global
*/

/* also occurs in library.c */
/* GS reads the memory pointed at in a specific ordering. There are,
 * however two definitions for it. I have jotted them down here, both,
 * but I think the first one is actually used. The thing is that
 * big-endian systems might read this the wrong way round. OTOH, we
 * constructed the IDs that are read out with this macro explicitly as
 * well. I expect we'll sort it out soon... */

/* from blendef: */
#define GS(a)	(*((short *)(a)))

/* from misc_util: flip the bytes from x  */
/*  #define GS(x) (((unsigned char *)(x))[0] << 8 | ((unsigned char *)(x))[1]) */

// only used here in readfile.c
#define SWITCH_LONGINT(a) { \
	char s_i, *p_i; \
	p_i= (char *)&(a);  \
	s_i=p_i[0]; p_i[0]=p_i[7]; p_i[7]=s_i; \
	s_i=p_i[1]; p_i[1]=p_i[6]; p_i[6]=s_i; \
	s_i=p_i[2]; p_i[2]=p_i[5]; p_i[5]=s_i; \
	s_i=p_i[3]; p_i[3]=p_i[4]; p_i[4]=s_i; }

/***/

typedef struct OldNew {
	void *old, *newp;
	int nr;
} OldNew;

typedef struct OldNewMap {
	OldNew *entries;
	int nentries, entriessize;
	int sorted;
	int lasthit;
} OldNewMap;


/* local prototypes */
static void *read_struct(FileData *fd, BHead *bh, const char *blockname);
static void direct_link_modifiers(FileData *fd, ListBase *lb);
static void convert_tface_mt(FileData *fd, Main *main);

/* this function ensures that reports are printed,
 * in the case of libraray linking errors this is important!
 *
 * bit kludge but better then doubling up on prints,
 * we could alternatively have a versions of a report function which foces printing - campbell
 */
static void BKE_reportf_wrap(ReportList *reports, ReportType type, const char *format, ...)
{
	char fixed_buf[1024]; /* should be long enough */

	va_list args;

	va_start(args, format);
	vsnprintf(fixed_buf, sizeof(fixed_buf), format, args);
	va_end(args);

	fixed_buf[sizeof(fixed_buf) - 1] = '\0';

	BKE_report(reports, type, fixed_buf);

	if (G.background==0) {
		printf("%s\n", fixed_buf);
	}
}

static OldNewMap *oldnewmap_new(void) 
{
	OldNewMap *onm= MEM_callocN(sizeof(*onm), "OldNewMap");
	
	onm->entriessize= 1024;
	onm->entries= MEM_mallocN(sizeof(*onm->entries)*onm->entriessize, "OldNewMap.entries");
	
	return onm;
}

static int verg_oldnewmap(const void *v1, const void *v2)
{
	const struct OldNew *x1=v1, *x2=v2;
	
	if ( x1->old > x2->old) return 1;
	else if ( x1->old < x2->old) return -1;
	return 0;
}


static void oldnewmap_sort(FileData *fd) 
{
	qsort(fd->libmap->entries, fd->libmap->nentries, sizeof(OldNew), verg_oldnewmap);
	fd->libmap->sorted= 1;
}

/* nr is zero for data, and ID code for libdata */
static void oldnewmap_insert(OldNewMap *onm, void *oldaddr, void *newaddr, int nr) 
{
	OldNew *entry;

	if (oldaddr==NULL || newaddr==NULL) return;
	
	if (onm->nentries==onm->entriessize) {
		int osize= onm->entriessize;
		OldNew *oentries= onm->entries;

		onm->entriessize*= 2;
		onm->entries= MEM_mallocN(sizeof(*onm->entries)*onm->entriessize, "OldNewMap.entries");

		memcpy(onm->entries, oentries, sizeof(*oentries)*osize);
		MEM_freeN(oentries);
	}

	entry= &onm->entries[onm->nentries++];
	entry->old= oldaddr;
	entry->newp= newaddr;
	entry->nr= nr;
}

static void *oldnewmap_lookup_and_inc(OldNewMap *onm, void *addr) 
{
	int i;

	if (addr==NULL) return NULL;

	if (onm->lasthit<onm->nentries-1) {
		OldNew *entry= &onm->entries[++onm->lasthit];

		if (entry->old==addr) {
			entry->nr++;
			return entry->newp;
		}
	}

	for (i=0; i<onm->nentries; i++) {
		OldNew *entry= &onm->entries[i];

		if (entry->old==addr) {
			onm->lasthit= i;

			entry->nr++;
			return entry->newp;
		}
	}

	return NULL;
}

/* for libdata, nr has ID code, no increment */
static void *oldnewmap_liblookup(OldNewMap *onm, void *addr, void *lib) 
{
	int i;
	
	if (addr==NULL) return NULL;
	
	/* lasthit works fine for non-libdata, linking there is done in same sequence as writing */
	if (onm->sorted) {
		OldNew entry_s, *entry;
		
		entry_s.old= addr;
		
		entry= bsearch(&entry_s, onm->entries, onm->nentries, sizeof(OldNew), verg_oldnewmap);
		if (entry) {
			ID *id= entry->newp;
			
			if (id && (!lib || id->lib)) {
				return entry->newp;
			}
		}
	}
	
	for (i=0; i<onm->nentries; i++) {
		OldNew *entry= &onm->entries[i];

		if (entry->old==addr) {
			ID *id= entry->newp;

			if (id && (!lib || id->lib)) {
				return entry->newp;
			}
		}
	}

	return NULL;
}

static void oldnewmap_free_unused(OldNewMap *onm) 
{
	int i;

	for (i=0; i<onm->nentries; i++) {
		OldNew *entry= &onm->entries[i];
		if (entry->nr==0) {
			MEM_freeN(entry->newp);
			entry->newp= NULL;
		}
	}
}

static void oldnewmap_clear(OldNewMap *onm) 
{
	onm->nentries= 0;
	onm->lasthit= 0;
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
	a= set_listbasepointers(from, fromarray);
	while (a--) {
		BLI_movelisttolist(lbarray[a], fromarray[a]);
	}
}

void blo_join_main(ListBase *mainlist)
{
	Main *tojoin, *mainl;
	
	
	mainl= mainlist->first;
	while ((tojoin= mainl->next)) {
		add_main_to_main(mainl, tojoin);
		BLI_remlink(mainlist, tojoin);
		MEM_freeN(tojoin);
	}
}

static void split_libdata(ListBase *lb, Main *first)
{
	ListBase *lbn;
	ID *id, *idnext;
	Main *mainvar;

	id= lb->first;
	while (id) {
		idnext= id->next;
		if (id->lib) {
			mainvar= first;
			while (mainvar) {
				if (mainvar->curlib==id->lib) {
					lbn= which_libbase(mainvar, GS(id->name));
					BLI_remlink(lb, id);
					BLI_addtail(lbn, id);
					break;
				}
				mainvar= mainvar->next;
			}
			if (mainvar==NULL) printf("error split_libdata\n");
		}
		id= idnext;
	}
}

void blo_split_main(ListBase *mainlist, Main *main)
{
	ListBase *lbarray[MAX_LIBARRAY];
	Library *lib;
	int i;

	mainlist->first= mainlist->last= main;
	main->next= NULL;

	if (main->library.first==NULL)
		return;
	
	for (lib= main->library.first; lib; lib= lib->id.next) {
		Main *libmain= MEM_callocN(sizeof(Main), "libmain");
		libmain->curlib= lib;
		BLI_addtail(mainlist, libmain);
	}

	i= set_listbasepointers(main, lbarray);
	while (i--)
		split_libdata(lbarray[i], main->next);
}

/* removes things like /blah/blah/../../blah/ etc, then writes in *name the full path */
static void cleanup_path(const char *relabase, char *name)
{
	char filename[FILE_MAXFILE];
	
	BLI_splitdirstring(name, filename);
	BLI_cleanup_dir(relabase, name);
	strcat(name, filename);
}

static void read_file_version(FileData *fd, Main *main)
{
	BHead *bhead;
	
	for (bhead= blo_firstbhead(fd); bhead; bhead= blo_nextbhead(fd, bhead)) {
		if (bhead->code==GLOB) {
			FileGlobal *fg= read_struct(fd, bhead, "Global");
			if (fg) {
				main->subversionfile= fg->subversion;
				main->minversionfile= fg->minversion;
				main->minsubversionfile= fg->minsubversion;
				MEM_freeN(fg);
			}
			else if (bhead->code==ENDB)
				break;
		}
	}
}


static Main *blo_find_main(FileData *fd, ListBase *mainlist, const char *filepath, const char *relabase)
{
	Main *m;
	Library *lib;
	char name1[FILE_MAX];
	
	BLI_strncpy(name1, filepath, sizeof(name1));
	cleanup_path(relabase, name1);
//	printf("blo_find_main: original in  %s\n", name);
//	printf("blo_find_main: converted to %s\n", name1);

	for (m= mainlist->first; m; m= m->next) {
		char *libname= (m->curlib)?m->curlib->filepath:m->name;
		
		if (BLI_path_cmp(name1, libname) == 0) {
			if (G.debug & G_DEBUG) printf("blo_find_main: found library %s\n", libname);
			return m;
		}
	}

	m= MEM_callocN(sizeof(Main), "find_main");
	BLI_addtail(mainlist, m);

	lib= alloc_libblock(&m->library, ID_LI, "lib");
	BLI_strncpy(lib->name, filepath, sizeof(lib->name));
	BLI_strncpy(lib->filepath, name1, sizeof(lib->filepath));
	
	m->curlib= lib;
	
	read_file_version(fd, m);
	
	if (G.debug & G_DEBUG) printf("blo_find_main: added new lib %s\n", filepath);
	return m;
}


/* ************ FILE PARSING ****************** */

static void switch_endian_bh4(BHead4 *bhead)
{
	/* the ID_.. codes */
	if ((bhead->code & 0xFFFF)==0) bhead->code >>=16;

	if (bhead->code != ENDB) {
		SWITCH_INT(bhead->len);
		SWITCH_INT(bhead->SDNAnr);
		SWITCH_INT(bhead->nr);
	}
}

static void switch_endian_bh8(BHead8 *bhead)
{
	/* the ID_.. codes */
	if ((bhead->code & 0xFFFF)==0) bhead->code >>=16;

	if (bhead->code != ENDB) {
		SWITCH_INT(bhead->len);
		SWITCH_INT(bhead->SDNAnr);
		SWITCH_INT(bhead->nr);
	}
}

static void bh4_from_bh8(BHead *bhead, BHead8 *bhead8, int do_endian_swap)
{
	BHead4 *bhead4 = (BHead4 *) bhead;
#if defined(WIN32) && !defined(FREE_WINDOWS)
	__int64 old;
#else
	long long old;
#endif

	bhead4->code= bhead8->code;
	bhead4->len= bhead8->len;

	if (bhead4->code != ENDB) {

		//perform a endian swap on 64bit pointers, otherwise the pointer might map to zero
		//0x0000000000000000000012345678 would become 0x12345678000000000000000000000000
		if (do_endian_swap) {
			SWITCH_LONGINT(bhead8->old);
		}

		/* this patch is to avoid a long long being read from not-eight aligned positions
		   is necessary on any modern 64bit architecture) */
		memcpy(&old, &bhead8->old, 8);
		bhead4->old = (int) (old >> 3);

		bhead4->SDNAnr= bhead8->SDNAnr;
		bhead4->nr= bhead8->nr;
	}
}

static void bh8_from_bh4(BHead *bhead, BHead4 *bhead4)
{
	BHead8 *bhead8 = (BHead8 *) bhead;

	bhead8->code= bhead4->code;
	bhead8->len= bhead4->len;

	if (bhead8->code != ENDB) {
		bhead8->old= bhead4->old;
		bhead8->SDNAnr= bhead4->SDNAnr;
		bhead8->nr= bhead4->nr;
	}
}

static BHeadN *get_bhead(FileData *fd)
{
	BHeadN *new_bhead = NULL;
	int readsize;
	
	if (fd) {
		if ( ! fd->eof) {
			/* initializing to zero isn't strictly needed but shuts valgrind up
			 * since uninitialized memory gets compared */
			BHead8 bhead8= {0};
			BHead4 bhead4= {0};
			BHead  bhead= {0};
			
			// First read the bhead structure.
			// Depending on the platform the file was written on this can
			// be a big or little endian BHead4 or BHead8 structure.

			// As usual 'ENDB' (the last *partial* bhead of the file)
			// needs some special handling. We don't want to EOF just yet.

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

			// bhead now contains the (converted) bhead structure. Now read
			// the associated data and put everything in a BHeadN (creative naming !)

			if ( ! fd->eof) {
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

	// We've read a new block. Now add it to the list
	// of blocks.

	if (new_bhead) {
		BLI_addtail(&fd->listbase, new_bhead);
	}

	return(new_bhead);
}

BHead *blo_firstbhead(FileData *fd)
{
	BHeadN *new_bhead;
	BHead *bhead = NULL;

	// Rewind the file
	// Read in a new block if necessary

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
	BHeadN *bheadn= (BHeadN *) (((char *) thisblock) - GET_INT_FROM_POINTER( &((BHeadN*)0)->bhead) );
	BHeadN *prev= bheadn->prev;

	return prev?&prev->bhead:NULL;
}

BHead *blo_nextbhead(FileData *fd, BHead *thisblock)
{
	BHeadN *new_bhead = NULL;
	BHead *bhead = NULL;

	if (thisblock) {
		// bhead is actually a sub part of BHeadN
		// We calculate the BHeadN pointer from the BHead pointer below
		new_bhead = (BHeadN *) (((char *) thisblock) - GET_INT_FROM_POINTER( &((BHeadN*)0)->bhead) );

		// get the next BHeadN. If it doesn't exist we read in the next one
		new_bhead = new_bhead->next;
		if (new_bhead == NULL) {
			new_bhead = get_bhead(fd);
		}
	}

	if (new_bhead) {
		// here we do the reverse:
		// go from the BHeadN pointer to the BHead pointer
		bhead = &new_bhead->bhead;
	}

	return(bhead);
}

static void decode_blender_header(FileData *fd)
{
	char header[SIZEOFBLENDERHEADER], num[4];
	int readsize;

	// read in the header data
	readsize = fd->read(fd, header, sizeof(header));

	if (readsize == sizeof(header)) {
		if (strncmp(header, "BLENDER", 7) == 0) {
			int remove_this_endian_test= 1;

			fd->flags |= FD_FLAGS_FILE_OK;

			// what size are pointers in the file ?
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

			// is the file saved in a different endian
			// than we need ?
			if (((((char*)&remove_this_endian_test)[0]==1)?L_ENDIAN:B_ENDIAN) != ((header[8]=='v')?L_ENDIAN:B_ENDIAN)) {
				fd->flags |= FD_FLAGS_SWITCH_ENDIAN;
			}

			// get the version number

			memcpy(num, header+9, 3);
			num[3] = 0;
			fd->fileversion = atoi(num);
		}
	}
}

static int read_file_dna(FileData *fd)
{
	BHead *bhead;

	for (bhead= blo_firstbhead(fd); bhead; bhead= blo_nextbhead(fd, bhead)) {
		if (bhead->code==DNA1) {
			int do_endian_swap= (fd->flags&FD_FLAGS_SWITCH_ENDIAN)?1:0;

			fd->filesdna= DNA_sdna_from_data(&bhead[1], bhead->len, do_endian_swap);
			if (fd->filesdna) {
				
				fd->compflags= DNA_struct_get_compareflags(fd->filesdna, fd->memsdna);
				/* used to retrieve ID names from (bhead+1) */
				fd->id_name_offs= DNA_elem_offset(fd->filesdna, "ID", "char", "name[]");
			}

			return 1;
		}
		else if (bhead->code==ENDB)
			break;
	}

	return 0;
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

	return (readsize);
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
	// don't read more bytes then there are available in the buffer
	int readsize = (int)MIN2(size, (unsigned int)(filedata->buffersize - filedata->seek));

	memcpy(buffer, filedata->buffer + filedata->seek, readsize);
	filedata->seek += readsize;

	return (readsize);
}

static int fd_read_from_memfile(FileData *filedata, void *buffer, unsigned int size)
{
	static unsigned int seek= 1<<30;	/* the current position */
	static unsigned int offset= 0;		/* size of previous chunks */
	static MemFileChunk *chunk=NULL;
	unsigned int chunkoffset, readsize, totread;
	
	if (size==0) return 0;
	
	if (seek != (unsigned int)filedata->seek) {
		chunk= filedata->memfile->chunks.first;
		seek= 0;
		
		while (chunk) {
			if (seek + chunk->size > (unsigned) filedata->seek) break;
			seek+= chunk->size;
			chunk= chunk->next;
		}
		offset= seek;
		seek= filedata->seek;
	}
	
	if (chunk) {
		totread= 0;

		do {
			/* first check if it's on the end if current chunk */
			if (seek-offset == chunk->size) {
				offset+= chunk->size;
				chunk= chunk->next;
			}

			/* debug, should never happen */
			if (chunk==NULL) {
				printf("illegal read, chunk zero\n");
				return 0;
			}

			chunkoffset= seek-offset;
			readsize= size-totread;

			/* data can be spread over multiple chunks, so clamp size
			 * to within this chunk, and then it will read further in
			 * the next chunk */
			if (chunkoffset+readsize > chunk->size)
				readsize= chunk->size-chunkoffset;

			memcpy((char*)buffer+totread, chunk->buf+chunkoffset, readsize);
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

		/* XXX, this doesn't need to be done all the time,
		 * but it keeps us re-entrant,  remove once we have
		 * a lib that provides a nice lock. - zr
		 */
	fd->memsdna = DNA_sdna_from_data(DNAstr,  DNAlen,  0);

	fd->datamap = oldnewmap_new();
	fd->globmap = oldnewmap_new();
	fd->libmap = oldnewmap_new();

	return fd;
}

static FileData *blo_decode_and_check(FileData *fd, ReportList *reports)
{
	decode_blender_header(fd);

	if (fd->flags & FD_FLAGS_FILE_OK) {
		if (!read_file_dna(fd)) {
			BKE_reportf(reports, RPT_ERROR, "Failed to read blend file: \"%s\", incomplete", fd->relabase);
			blo_freefiledata(fd);
			fd= NULL;
		}
	} 
	else {
		BKE_reportf(reports, RPT_ERROR, "Failed to read blend file: \"%s\", not a blend file", fd->relabase);
		blo_freefiledata(fd);
		fd= NULL;
	}

	return fd;
}

/* cannot be called with relative paths anymore! */
/* on each new library added, it now checks for the current FileData and expands relativeness */
FileData *blo_openblenderfile(const char *filepath, ReportList *reports)
{
	gzFile gzfile;
	errno= 0;
	gzfile= BLI_gzopen(filepath, "rb");

	if (gzfile == (gzFile)Z_NULL) {
		BKE_reportf(reports, RPT_ERROR, "Unable to open \"%s\": %s.", filepath, errno ? strerror(errno) : "Unknown error reading file");
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

FileData *blo_openblendermemory(void *mem, int memsize, ReportList *reports)
{
	if (!mem || memsize<SIZEOFBLENDERHEADER) {
		BKE_report(reports, RPT_ERROR, (mem)? "Unable to read": "Unable to open");
		return NULL;
	}
	else {
		FileData *fd= filedata_new();
		fd->buffer= mem;
		fd->buffersize= memsize;
		fd->read= fd_read_from_memory;
		fd->flags|= FD_FLAGS_NOT_MY_BUFFER;

		return blo_decode_and_check(fd, reports);
	}
}

FileData *blo_openblendermemfile(MemFile *memfile, ReportList *reports)
{
	if (!memfile) {
		BKE_report(reports, RPT_ERROR, "Unable to open blend <memory>");
		return NULL;
	}
	else {
		FileData *fd= filedata_new();
		fd->memfile= memfile;

		fd->read= fd_read_from_memfile;
		fd->flags|= FD_FLAGS_NOT_MY_BUFFER;

		return blo_decode_and_check(fd, reports);
	}
}


void blo_freefiledata(FileData *fd)
{
	if (fd) {
		
		if (fd->filedes != -1) {
			close(fd->filedes);
		}

		if (fd->gzfiledes != NULL)
		{
			gzclose(fd->gzfiledes);
		}

		if (fd->buffer && !(fd->flags & FD_FLAGS_NOT_MY_BUFFER)) {
			MEM_freeN(fd->buffer);
			fd->buffer = NULL;
		}

		// Free all BHeadN data blocks
		BLI_freelistN(&fd->listbase);

		if (fd->memsdna)
			DNA_sdna_free(fd->memsdna);
		if (fd->filesdna)
			DNA_sdna_free(fd->filesdna);
		if (fd->compflags)
			MEM_freeN(fd->compflags);

		if (fd->datamap)
			oldnewmap_free(fd->datamap);
		if (fd->globmap)
			oldnewmap_free(fd->globmap);
		if (fd->imamap)
			oldnewmap_free(fd->imamap);
		if (fd->movieclipmap)
			oldnewmap_free(fd->movieclipmap);
		if (fd->libmap && !(fd->flags & FD_FLAGS_NOT_MY_LIBMAP))
			oldnewmap_free(fd->libmap);
		if (fd->bheadmap)
			MEM_freeN(fd->bheadmap);
		
		MEM_freeN(fd);
	}
}

/* ************ DIV ****************** */

int BLO_has_bfile_extension(const char *str)
{
	return (BLI_testextensie(str, ".ble") || BLI_testextensie(str, ".blend") || BLI_testextensie(str, ".blend.gz"));
}

int BLO_is_a_library(const char *path, char *dir, char *group)
{
	/* return ok when a blenderfile, in dir is the filename,
	 * in group the type of libdata
	 */
	int len;
	char *fd;
	
	strcpy(dir, path);
	len= strlen(dir);
	if (len<7) return 0;
	if ( dir[len-1] != '/' && dir[len-1] != '\\') return 0;
	
	group[0]= 0;
	dir[len-1]= 0;

	/* Find the last slash */
	fd= BLI_last_slash(dir);

	if (fd==NULL) return 0;
	*fd= 0;
	if (BLO_has_bfile_extension(fd+1)) {
		/* the last part of the dir is a .blend file, no group follows */
		*fd= '/'; /* put back the removed slash separating the dir and the .blend file name */
	}
	else {		
		char *gp = fd+1; // in case we have a .blend file, gp points to the group

		/* Find the last slash */
		fd= BLI_last_slash(dir);
		if (!fd || !BLO_has_bfile_extension(fd+1)) return 0;

		/* now we know that we are in a blend file and it is safe to 
		   assume that gp actually points to a group */
		if (strcmp("Screen", gp)!=0)
			BLI_strncpy(group, gp, GROUP_MAX);
	}
	return 1;
}

/* ************** OLD POINTERS ******************* */

static void *newdataadr(FileData *fd, void *adr)		/* only direct databocks */
{
	return oldnewmap_lookup_and_inc(fd->datamap, adr);
}

static void *newglobadr(FileData *fd, void *adr)		/* direct datablocks with global linking */
{
	return oldnewmap_lookup_and_inc(fd->globmap, adr);
}

static void *newimaadr(FileData *fd, void *adr)		/* used to restore image data after undo */
{
	if (fd->imamap && adr)
		return oldnewmap_lookup_and_inc(fd->imamap, adr);
	return NULL;
}

static void *newmclipadr(FileData *fd, void *adr)              /* used to restore movie clip data after undo */
{
	if (fd->movieclipmap && adr)
		return oldnewmap_lookup_and_inc(fd->movieclipmap, adr);
	return NULL;
}


static void *newlibadr(FileData *fd, void *lib, void *adr)		/* only lib data */
{
	return oldnewmap_liblookup(fd->libmap, adr, lib);
}

static void *newlibadr_us(FileData *fd, void *lib, void *adr)	/* increases user number */
{
	ID *id= newlibadr(fd, lib, adr);

	if (id)
		id->us++;

	return id;
}

static void change_idid_adr_fd(FileData *fd, void *old, void *new)
{
	int i;
	
	for (i=0; i<fd->libmap->nentries; i++) {
		OldNew *entry= &fd->libmap->entries[i];
		
		if (old==entry->newp && entry->nr==ID_ID) {
			entry->newp= new;
			if (new) entry->nr= GS( ((ID *)new)->name );
			break;
		}
	}
}

static void change_idid_adr(ListBase *mainlist, FileData *basefd, void *old, void *new)
{
	Main *mainptr;
	
	for (mainptr= mainlist->first; mainptr; mainptr= mainptr->next) {
		FileData *fd;
		
		if (mainptr->curlib) fd= mainptr->curlib->filedata;
		else fd= basefd;
		
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
	Object *ob= oldmain->object.first;
	
	for (;ob; ob= ob->id.next)
		if (ob->id.lib)
			ob->proxy_from= NULL;
}

void blo_make_image_pointer_map(FileData *fd, Main *oldmain)
{
	Image *ima= oldmain->image.first;
	Scene *sce= oldmain->scene.first;
	int a;
	
	fd->imamap= oldnewmap_new();
	
	for (;ima; ima= ima->id.next) {
		Link *ibuf= ima->ibufs.first;
		for (; ibuf; ibuf= ibuf->next)
			oldnewmap_insert(fd->imamap, ibuf, ibuf, 0);
		if (ima->gputexture)
			oldnewmap_insert(fd->imamap, ima->gputexture, ima->gputexture, 0);
		for (a=0; a<IMA_MAX_RENDER_SLOT; a++)
			if (ima->renders[a])
				oldnewmap_insert(fd->imamap, ima->renders[a], ima->renders[a], 0);
	}
	for (; sce; sce= sce->id.next) {
		if (sce->nodetree) {
			bNode *node;
			for (node= sce->nodetree->nodes.first; node; node= node->next)
				oldnewmap_insert(fd->imamap, node->preview, node->preview, 0);
		}
	}
}

/* set old main image ibufs to zero if it has been restored */
/* this works because freeing old main only happens after this call */
void blo_end_image_pointer_map(FileData *fd, Main *oldmain)
{
	OldNew *entry= fd->imamap->entries;
	Image *ima= oldmain->image.first;
	Scene *sce= oldmain->scene.first;
	int i;
	
	/* used entries were restored, so we put them to zero */
	for (i=0; i<fd->imamap->nentries; i++, entry++) {
		if (entry->nr>0)
			entry->newp= NULL;
	}
	
	for (;ima; ima= ima->id.next) {
		Link *ibuf, *next;
		
		/* this mirrors direct_link_image */
		for (ibuf= ima->ibufs.first; ibuf; ibuf= next) {
			next= ibuf->next;
			if (NULL==newimaadr(fd, ibuf)) {	/* so was restored */
				BLI_remlink(&ima->ibufs, ibuf);
				ima->bindcode= 0;
				ima->gputexture= NULL;
			}
		}
		for (i=0; i<IMA_MAX_RENDER_SLOT; i++)
			ima->renders[i]= newimaadr(fd, ima->renders[i]);

		ima->gputexture= newimaadr(fd, ima->gputexture);
	}
	for (; sce; sce= sce->id.next) {
		if (sce->nodetree) {
			bNode *node;
			for (node= sce->nodetree->nodes.first; node; node= node->next)
				node->preview= newimaadr(fd, node->preview);
		}
	}
}

void blo_make_movieclip_pointer_map(FileData *fd, Main *oldmain)
{
	MovieClip *clip= oldmain->movieclip.first;
	Scene *sce= oldmain->scene.first;

	fd->movieclipmap= oldnewmap_new();

	for (;clip; clip= clip->id.next) {
		if (clip->cache)
			oldnewmap_insert(fd->movieclipmap, clip->cache, clip->cache, 0);

		if (clip->tracking.camera.intrinsics)
			oldnewmap_insert(fd->movieclipmap, clip->tracking.camera.intrinsics, clip->tracking.camera.intrinsics, 0);
	}

	for (; sce; sce= sce->id.next) {
		if (sce->nodetree) {
			bNode *node;
			for (node= sce->nodetree->nodes.first; node; node= node->next)
				if (node->type==CMP_NODE_MOVIEDISTORTION)
					oldnewmap_insert(fd->movieclipmap, node->storage, node->storage, 0);
		}
	}
}

/* set old main movie clips caches to zero if it has been restored */
/* this works because freeing old main only happens after this call */
void blo_end_movieclip_pointer_map(FileData *fd, Main *oldmain)
{
	OldNew *entry= fd->movieclipmap->entries;
	MovieClip *clip= oldmain->movieclip.first;
	Scene *sce= oldmain->scene.first;
	int i;

	/* used entries were restored, so we put them to zero */
	for (i=0; i<fd->movieclipmap->nentries; i++, entry++) {
		if (entry->nr>0)
				entry->newp= NULL;
	}

	for (;clip; clip= clip->id.next) {
		clip->cache= newmclipadr(fd, clip->cache);
		clip->tracking.camera.intrinsics= newmclipadr(fd, clip->tracking.camera.intrinsics);
	}

	for (; sce; sce= sce->id.next) {
		if (sce->nodetree) {
			bNode *node;
			for (node= sce->nodetree->nodes.first; node; node= node->next)
				if (node->type==CMP_NODE_MOVIEDISTORTION)
					node->storage= newmclipadr(fd, node->storage);
		}
	}
}


/* undo file support: add all library pointers in lookup */
void blo_add_library_pointer_map(ListBase *mainlist, FileData *fd)
{
	Main *ptr= mainlist->first;
	ListBase *lbarray[MAX_LIBARRAY];
	
	for (ptr= ptr->next; ptr; ptr= ptr->next) {
		int i= set_listbasepointers(ptr, lbarray);
		while (i--) {
			ID *id;
			for (id= lbarray[i]->first; id; id= id->next)
				oldnewmap_insert(fd->libmap, id, id, GS(id->name));
		}
	}
}
		

/* ********** END OLD POINTERS ****************** */
/* ********** READ FILE ****************** */

static void switch_endian_structs(struct SDNA *filesdna, BHead *bhead)
{
	int blocksize, nblocks;
	char *data;

	data= (char *)(bhead+1);
	blocksize= filesdna->typelens[ filesdna->structs[bhead->SDNAnr][0] ];

	nblocks= bhead->nr;
	while (nblocks--) {
		DNA_struct_switch_endian(filesdna, bhead->SDNAnr, data);

		data+= blocksize;
	}
}

static void *read_struct(FileData *fd, BHead *bh, const char *blockname)
{
	void *temp= NULL;

	if (bh->len) {
		/* switch is based on file dna */
		if (bh->SDNAnr && (fd->flags & FD_FLAGS_SWITCH_ENDIAN))
			switch_endian_structs(fd->filesdna, bh);

		if (fd->compflags[bh->SDNAnr]) {	/* flag==0: doesn't exist anymore */
			if (fd->compflags[bh->SDNAnr]==2) {
				temp= DNA_struct_reconstruct(fd->memsdna, fd->filesdna, fd->compflags, bh->SDNAnr, bh->nr, (bh+1));
			}
			else {
				temp= MEM_mallocN(bh->len, blockname);
				memcpy(temp, (bh+1), bh->len);
			}
		}
	}

	return temp;
}

static void link_list(FileData *fd, ListBase *lb)		/* only direct data */
{
	Link *ln, *prev;

	if (lb->first==NULL) return;

	lb->first= newdataadr(fd, lb->first);
	ln= lb->first;
	prev= NULL;
	while (ln) {
		ln->next= newdataadr(fd, ln->next);
		ln->prev= prev;
		prev= ln;
		ln= ln->next;
	}
	lb->last= prev;
}

static void link_glob_list(FileData *fd, ListBase *lb)		/* for glob data */
{
	Link *ln, *prev;
	void *poin;

	if (lb->first==NULL) return;
	poin= newdataadr(fd, lb->first);
	if (lb->first) {
		oldnewmap_insert(fd->globmap, lb->first, poin, 0);
	}
	lb->first= poin;

	ln= lb->first;
	prev= NULL;
	while (ln) {
		poin= newdataadr(fd, ln->next);
		if (ln->next) {
			oldnewmap_insert(fd->globmap, ln->next, poin, 0);
		}
		ln->next= poin;
		ln->prev= prev;
		prev= ln;
		ln= ln->next;
	}
	lb->last= prev;
}

static void test_pointer_array(FileData *fd, void **mat)
{
#if defined(WIN32) && !defined(FREE_WINDOWS)
	__int64 *lpoin, *lmat;
#else
	long long *lpoin, *lmat;
#endif
	int *ipoin, *imat;
	size_t len;

		/* manually convert the pointer array in
		 * the old dna format to a pointer array in
		 * the new dna format.
		 */
	if (*mat) {
		len= MEM_allocN_len(*mat)/fd->filesdna->pointerlen;

		if (fd->filesdna->pointerlen==8 && fd->memsdna->pointerlen==4) {
			ipoin=imat= MEM_mallocN( len*4, "newmatar");
			lpoin= *mat;

			while (len-- > 0) {
				if ((fd->flags & FD_FLAGS_SWITCH_ENDIAN))
					SWITCH_LONGINT(*lpoin);
				*ipoin= (int) ((*lpoin) >> 3);
				ipoin++;
				lpoin++;
			}
			MEM_freeN(*mat);
			*mat= imat;
		}

		if (fd->filesdna->pointerlen==4 && fd->memsdna->pointerlen==8) {
			lpoin=lmat= MEM_mallocN( len*8, "newmatar");
			ipoin= *mat;

			while (len-- > 0) {
				*lpoin= *ipoin;
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
static void IDP_LibLinkProperty(IDProperty *prop, int switch_endian, FileData *fd);

static void IDP_DirectLinkIDPArray(IDProperty *prop, int switch_endian, FileData *fd)
{
	IDProperty *array;
	int i;

	/*since we didn't save the extra buffer, set totallen to len.*/
	prop->totallen = prop->len;
	prop->data.pointer = newdataadr(fd, prop->data.pointer);

	array= (IDProperty*) prop->data.pointer;
	
	/* note!, idp-arrays didn't exist in 2.4x, so the pointer will be cleared
	 * theres not really anything we can do to correct this, at least don't crash */
	if (array==NULL) {
		prop->len= 0;
		prop->totallen= 0;
	}
	

	for (i=0; i<prop->len; i++)
		IDP_DirectLinkProperty(&array[i], switch_endian, fd);
}

static void IDP_DirectLinkArray(IDProperty *prop, int switch_endian, FileData *fd)
{
	IDProperty **array;
	int i;

	/*since we didn't save the extra buffer, set totallen to len.*/
	prop->totallen = prop->len;
	prop->data.pointer = newdataadr(fd, prop->data.pointer);

	if (prop->subtype == IDP_GROUP) {
		test_pointer_array(fd, prop->data.pointer);
		array= prop->data.pointer;

		for (i=0; i<prop->len; i++)
			IDP_DirectLinkProperty(array[i], switch_endian, fd);
	}
	else if (prop->subtype == IDP_DOUBLE) {
		if (switch_endian) {
			for (i=0; i<prop->len; i++) {
				SWITCH_LONGINT(((double*)prop->data.pointer)[i]);
			}
		}
	}
	else {
		if (switch_endian) {
			for (i=0; i<prop->len; i++) {
				SWITCH_INT(((int*)prop->data.pointer)[i]);
			}
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

			 * in theory, val and val2 would've already been swapped
			 * if switch_endian is true, so we have to first unswap
			 * them then reswap them as a single 64-bit entity.
			 */
			
			if (switch_endian) {
				SWITCH_INT(prop->data.val);
				SWITCH_INT(prop->data.val2);
				SWITCH_LONGINT(prop->data.val);
			}
			
			break;
	}
}

/*stub function*/
static void IDP_LibLinkProperty(IDProperty *UNUSED(prop), int UNUSED(switch_endian), FileData *UNUSED(fd))
{
}

/* ************ READ CurveMapping *************** */

/* cuma itself has been read! */
static void direct_link_curvemapping(FileData *fd, CurveMapping *cumap)
{
	int a;
	
	/* flag seems to be able to hang? Maybe old files... not bad to clear anyway */
	cumap->flag &= ~CUMA_PREMULLED;
	
	for (a=0; a<CM_TOT; a++) {
		cumap->cm[a].curve= newdataadr(fd, cumap->cm[a].curve);
		cumap->cm[a].table= NULL;
		cumap->cm[a].premultable= NULL;
	}
}

/* ************ READ Brush *************** */
/* library brush linking after fileread */
static void lib_link_brush(FileData *fd, Main *main)
{
	Brush *brush;
	
	/* only link ID pointers */
	for (brush= main->brush.first; brush; brush= brush->id.next) {
		if (brush->id.flag & LIB_NEEDLINK) {
			brush->id.flag -= LIB_NEEDLINK;

			brush->mtex.tex= newlibadr_us(fd, brush->id.lib, brush->mtex.tex);
			brush->clone.image= newlibadr_us(fd, brush->id.lib, brush->clone.image);
		}
	}
}

static void direct_link_brush(FileData *fd, Brush *brush)
{
	/* brush itself has been read */

	/* fallof curve */
	brush->curve= newdataadr(fd, brush->curve);
	if (brush->curve)
		direct_link_curvemapping(fd, brush->curve);
	else
		brush_curve_preset(brush, CURVE_PRESET_SHARP);

	brush->preview= NULL;
	brush->icon_imbuf= NULL;
}

static void direct_link_script(FileData *UNUSED(fd), Script *script)
{
	script->id.us = 1;
	SCRIPT_SET_NULL(script);
}


/* ************ READ PACKEDFILE *************** */

static PackedFile *direct_link_packedfile(FileData *fd, PackedFile *oldpf)
{
	PackedFile *pf= newdataadr(fd, oldpf);

	if (pf) {
		pf->data= newdataadr(fd, pf->data);
	}

	return pf;
}

/* ************ READ IMAGE PREVIEW *************** */

static PreviewImage *direct_link_preview_image(FileData *fd, PreviewImage *old_prv)
{
	PreviewImage *prv= newdataadr(fd, old_prv);

	if (prv) {
		int i;
		for (i=0; i < NUM_ICON_SIZES; ++i) {
			if (prv->rect[i]) {
				prv->rect[i] = newdataadr(fd, prv->rect[i]);
			}
		}
	}

	return prv;
}

/* ************ READ ANIMATION STUFF ***************** */

/* Legacy Data Support (for Version Patching) ----------------------------- */

// XXX depreceated - old animation system
static void lib_link_ipo(FileData *fd, Main *main)
{
	Ipo *ipo;

	ipo= main->ipo.first;
	while (ipo) {
		if (ipo->id.flag & LIB_NEEDLINK) {
			IpoCurve *icu;
			for (icu= ipo->curve.first; icu; icu= icu->next) {
				if (icu->driver)
					icu->driver->ob= newlibadr(fd, ipo->id.lib, icu->driver->ob);
			}
			ipo->id.flag -= LIB_NEEDLINK;
		}
		ipo= ipo->id.next;
	}
}

// XXX depreceated - old animation system
static void direct_link_ipo(FileData *fd, Ipo *ipo)
{
	IpoCurve *icu;

	link_list(fd, &(ipo->curve));
	icu= ipo->curve.first;
	while (icu) {
		icu->bezt= newdataadr(fd, icu->bezt);
		icu->bp= newdataadr(fd, icu->bp);
		icu->driver= newdataadr(fd, icu->driver);
		icu= icu->next;
	}
}

// XXX depreceated - old animation system
static void lib_link_nlastrips(FileData *fd, ID *id, ListBase *striplist)
{
	bActionStrip *strip;
	bActionModifier *amod;
	
	for (strip=striplist->first; strip; strip=strip->next) {
		strip->object = newlibadr(fd, id->lib, strip->object);
		strip->act = newlibadr_us(fd, id->lib, strip->act);
		strip->ipo = newlibadr(fd, id->lib, strip->ipo);
		for (amod= strip->modifiers.first; amod; amod= amod->next)
			amod->ob= newlibadr(fd, id->lib, amod->ob);
	}
}

// XXX depreceated - old animation system
static void direct_link_nlastrips(FileData *fd, ListBase *strips)
{
	bActionStrip *strip;
	
	link_list(fd, strips);
	
	for (strip= strips->first; strip; strip= strip->next)
		link_list(fd, &strip->modifiers);
}

// XXX depreceated - old animation system
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
	
	for (fcm= list->first; fcm; fcm= fcm->next) {
		/* data for specific modifiers */
		switch (fcm->type) {
			case FMODIFIER_TYPE_PYTHON:
			{
				FMod_Python *data= (FMod_Python *)fcm->data;
				data->script = newlibadr(fd, id->lib, data->script);
			}
				break;
		}
	}
}

static void lib_link_fcurves(FileData *fd, ID *id, ListBase *list) 
{
	FCurve *fcu;
	
	if (list == NULL)
		return;
	
	/* relink ID-block references... */
	for (fcu= list->first; fcu; fcu= fcu->next) {
		/* driver data */
		if (fcu->driver) {
			ChannelDriver *driver= fcu->driver;
			DriverVar *dvar;
			
			for (dvar= driver->variables.first; dvar; dvar= dvar->next) {
				DRIVER_TARGETS_LOOPER(dvar)
				{	
					/* only relink if still used */
					if (tarIndex < dvar->num_targets)
						dtar->id= newlibadr(fd, id->lib, dtar->id); 
					else
						dtar->id= NULL;
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
	
	for (fcm= list->first; fcm; fcm= fcm->next) {
		/* relink general data */
		fcm->data = newdataadr(fd, fcm->data);
		fcm->edata= NULL;
		
		/* do relinking of data for specific types */
		switch (fcm->type) {
			case FMODIFIER_TYPE_GENERATOR:
			{
				FMod_Generator *data= (FMod_Generator *)fcm->data;
				
				data->coefficients= newdataadr(fd, data->coefficients);

				if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
					unsigned int a;
					for (a = 0; a < data->arraysize; a++)
						SWITCH_INT(data->coefficients[a]);
				}
			}
				break;
			case FMODIFIER_TYPE_ENVELOPE:
			{
				FMod_Envelope *data= (FMod_Envelope *)fcm->data;
				
				data->data= newdataadr(fd, data->data);
			}
				break;
			case FMODIFIER_TYPE_PYTHON:
			{
				FMod_Python *data= (FMod_Python *)fcm->data;
				
				data->prop = newdataadr(fd, data->prop);
				IDP_DirectLinkProperty(data->prop, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
			}
				break;
		}
	}
}

/* NOTE: this assumes that link_list has already been called on the list */
static void direct_link_fcurves(FileData *fd, ListBase *list)
{
	FCurve *fcu;
	
	/* link F-Curve data to F-Curve again (non ID-libs) */
	for (fcu= list->first; fcu; fcu= fcu->next) {
		/* curve data */
		fcu->bezt= newdataadr(fd, fcu->bezt);
		fcu->fpt= newdataadr(fd, fcu->fpt);
		
		/* rna path */
		fcu->rna_path= newdataadr(fd, fcu->rna_path);
		
		/* group */
		fcu->grp= newdataadr(fd, fcu->grp);
		
		/* driver */
		fcu->driver= newdataadr(fd, fcu->driver);
		if (fcu->driver) {
			ChannelDriver *driver= fcu->driver;
			DriverVar *dvar;

			driver->expr_comp= NULL;
			
			/* relink variables, targets and their paths */
			link_list(fd, &driver->variables);
			for (dvar= driver->variables.first; dvar; dvar= dvar->next) {
				DRIVER_TARGETS_LOOPER(dvar)
				{
					/* only relink the targets being used */
					if (tarIndex < dvar->num_targets)
						dtar->rna_path= newdataadr(fd, dtar->rna_path);
					else
						dtar->rna_path= NULL;
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
	bAction *act;
	bActionChannel *chan;

	for (act= main->action.first; act; act= act->id.next) {
		if (act->id.flag & LIB_NEEDLINK) {
			act->id.flag -= LIB_NEEDLINK;
			
// XXX depreceated - old animation system <<<
			for (chan=act->chanbase.first; chan; chan=chan->next) {
				chan->ipo= newlibadr_us(fd, act->id.lib, chan->ipo);
				lib_link_constraint_channels(fd, &act->id, &chan->constraintChannels);
			}
// >>> XXX depreceated - old animation system
			
			lib_link_fcurves(fd, &act->id, &act->curves);
		}
	}
}

static void direct_link_action(FileData *fd, bAction *act)
{
	bActionChannel *achan; // XXX depreceated - old animation system
	bActionGroup *agrp;

	link_list(fd, &act->curves);
	link_list(fd, &act->chanbase); // XXX depreceated - old animation system
	link_list(fd, &act->groups);
	link_list(fd, &act->markers);

// XXX depreceated - old animation system <<<
	for (achan = act->chanbase.first; achan; achan=achan->next) {
		achan->grp= newdataadr(fd, achan->grp);
		
		link_list(fd, &achan->constraintChannels);
	}
// >>> XXX depreceated - old animation system

	direct_link_fcurves(fd, &act->curves);
	
	for (agrp = act->groups.first; agrp; agrp= agrp->next) {
		agrp->channels.first= newdataadr(fd, agrp->channels.first);
		agrp->channels.last= newdataadr(fd, agrp->channels.last);
	}
}

static void lib_link_nladata_strips(FileData *fd, ID *id, ListBase *list)
{
	NlaStrip *strip;
	
	for (strip= list->first; strip; strip= strip->next) {
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
	for (nlt= list->first; nlt; nlt= nlt->next) {
		lib_link_nladata_strips(fd, id, &nlt->strips);
	}
}

/* This handles Animato NLA-Strips linking 
 * NOTE: this assumes that link_list has already been called on the list 
 */
static void direct_link_nladata_strips(FileData *fd, ListBase *list)
{
	NlaStrip *strip;
	
	for (strip= list->first; strip; strip= strip->next) {
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
	
	for (nlt= list->first; nlt; nlt= nlt->next) {
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
	for (ks= list->first; ks; ks= ks->next) {
		for (ksp= ks->paths.first; ksp; ksp= ksp->next) {
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
	for (ks= list->first; ks; ks= ks->next) {
		/* paths */
		link_list(fd, &ks->paths);
		
		for (ksp= ks->paths.first; ksp; ksp= ksp->next) {
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
	
	/* relink active strip - even though strictly speaking this should only be used
	 * if we're in 'tweaking mode', we need to be able to have this loaded back for
	 * undo, but also since users may not exit tweakmode before saving (#24535)
	 */
	// TODO: it's not really nice that anyone should be able to save the file in this
	//		state, but it's going to be too hard to enforce this single case...
	adt->actstrip= newdataadr(fd, adt->actstrip);
}	

/* ************ READ MOTION PATHS *************** */

/* direct data for cache */
static void direct_link_motionpath(FileData *fd, bMotionPath *mpath)
{
	/* sanity check */
	if (mpath == NULL)
		return;
	
	/* relink points cache */
	mpath->points= newdataadr(fd, mpath->points);
}

/* ************ READ NODE TREE *************** */

/* singe node tree (also used for material/scene trees), ntree is not NULL */
static void lib_link_ntree(FileData *fd, ID *id, bNodeTree *ntree)
{
	bNode *node;
	
	if (ntree->adt) lib_link_animdata(fd, &ntree->id, ntree->adt);
	
	ntree->gpd= newlibadr_us(fd, id->lib, ntree->gpd);
	
	for (node= ntree->nodes.first; node; node= node->next)
		node->id= newlibadr_us(fd, id->lib, node->id);
}

/* library ntree linking after fileread */
static void lib_link_nodetree(FileData *fd, Main *main)
{
	bNodeTree *ntree;
	
	/* only link ID pointers */
	for (ntree= main->nodetree.first; ntree; ntree= ntree->id.next) {
		if (ntree->id.flag & LIB_NEEDLINK) {
			ntree->id.flag -= LIB_NEEDLINK;
			lib_link_ntree(fd, &ntree->id, ntree);
		}
	}
}

static void do_versions_socket_default_value(bNodeSocket *sock)
{
	bNodeSocketValueFloat *valfloat;
	bNodeSocketValueVector *valvector;
	bNodeSocketValueRGBA *valrgba;
	
	if (sock->default_value)
		return;
	
	switch (sock->type) {
	case SOCK_FLOAT:
		valfloat = sock->default_value = MEM_callocN(sizeof(bNodeSocketValueFloat), "default socket value");
		valfloat->value = sock->ns.vec[0];
		valfloat->min = sock->ns.min;
		valfloat->max = sock->ns.max;
		valfloat->subtype = PROP_NONE;
		break;
	case SOCK_VECTOR:
		valvector = sock->default_value = MEM_callocN(sizeof(bNodeSocketValueVector), "default socket value");
		copy_v3_v3(valvector->value, sock->ns.vec);
		valvector->min = sock->ns.min;
		valvector->max = sock->ns.max;
		valvector->subtype = PROP_NONE;
		break;
	case SOCK_RGBA:
		valrgba = sock->default_value = MEM_callocN(sizeof(bNodeSocketValueRGBA), "default socket value");
		copy_v4_v4(valrgba->value, sock->ns.vec);
		break;
	}
}

static void do_versions_nodetree_default_value(bNodeTree *ntree)
{
	bNode *node;
	bNodeSocket *sock;
	for (node=ntree->nodes.first; node; node=node->next) {
		for (sock=node->inputs.first; sock; sock=sock->next)
			do_versions_socket_default_value(sock);
		for (sock=node->outputs.first; sock; sock=sock->next)
			do_versions_socket_default_value(sock);
	}
	for (sock=ntree->inputs.first; sock; sock=sock->next)
		do_versions_socket_default_value(sock);
	for (sock=ntree->outputs.first; sock; sock=sock->next)
		do_versions_socket_default_value(sock);
}

static void lib_nodetree_init_types_cb(void *UNUSED(data), ID *UNUSED(id), bNodeTree *ntree)
{
	bNode *node;
	
	ntreeInitTypes(ntree);

	/* need to do this here instead of in do_versions, otherwise next function can crash */
	do_versions_nodetree_default_value(ntree);
	
	/* XXX could be replaced by do_versions for new nodes */
	for (node=ntree->nodes.first; node; node=node->next)
		node_verify_socket_templates(ntree, node);
}

/* updates group node socket own_index so that
 * external links to/from the group node are preserved.
 */
static void lib_node_do_versions_group_indices(bNode *gnode)
{
	bNodeTree *ngroup= (bNodeTree*)gnode->id;
	bNode *intnode;
	bNodeSocket *sock, *gsock, *intsock;
	int found;
	
	for (sock=gnode->outputs.first; sock; sock=sock->next) {
		int old_index = sock->to_index;
		for (gsock=ngroup->outputs.first; gsock; gsock=gsock->next) {
			if (gsock->link && gsock->link->fromsock->own_index == old_index) {
				sock->own_index = gsock->own_index;
				break;
			}
		}
	}
	for (sock=gnode->inputs.first; sock; sock=sock->next) {
		int old_index = sock->to_index;
		/* can't use break in double loop */
		found = 0;
		for (intnode=ngroup->nodes.first; intnode && !found; intnode=intnode->next) {
			for (intsock=intnode->inputs.first; intsock; intsock=intsock->next) {
				if (intsock->own_index == old_index && intsock->link) {
					sock->own_index = intsock->link->fromsock->own_index;
					found = 1;
					break;
				}
			}
		}
	}
}

/* updates external links for all group nodes in a tree */
static void lib_nodetree_do_versions_group_indices_cb(void *UNUSED(data), ID *UNUSED(id), bNodeTree *ntree)
{
	bNode *node;
	
	for (node=ntree->nodes.first; node; node=node->next) {
		if (node->type==NODE_GROUP) {
			bNodeTree *ngroup= (bNodeTree*)node->id;
			if (ngroup && (ngroup->flag & NTREE_DO_VERSIONS_GROUP_EXPOSE))
				lib_node_do_versions_group_indices(node);
		}
	}
}

/* make an update call for the tree */
static void lib_nodetree_do_versions_update_cb(void *UNUSED(data), ID *UNUSED(id), bNodeTree *ntree)
{
	if (ntree->update)
		ntreeUpdateTree(ntree);
}

/* verify types for nodes and groups, all data has to be read */
/* open = 0: appending/linking, open = 1: open new file (need to clean out dynamic
* typedefs*/
static void lib_verify_nodetree(Main *main, int UNUSED(open))
{
	bNodeTree *ntree;
	int i;
	bNodeTreeType *ntreetype;

	/* this crashes blender on undo/redo
		if (open==1) {
			reinit_nodesystem();
		}*/
	
	/* set node->typeinfo pointers */
	for (i=0; i < NUM_NTREE_TYPES; ++i) {
		ntreetype= ntreeGetType(i);
		if (ntreetype && ntreetype->foreach_nodetree)
			ntreetype->foreach_nodetree(main, NULL, lib_nodetree_init_types_cb);
	}
	for (ntree= main->nodetree.first; ntree; ntree= ntree->id.next)
		lib_nodetree_init_types_cb(NULL, NULL, ntree);
	
	{
		int has_old_groups=0;
		/* XXX this should actually be part of do_versions, but since we need
		 * finished library linking, it is not possible there. Instead in do_versions
		 * we have set the NTREE_DO_VERSIONS flag, so at this point we can do the
		 * actual group node updates.
		 */
		for (ntree= main->nodetree.first; ntree; ntree= ntree->id.next) {
			if (ntree->flag & NTREE_DO_VERSIONS_GROUP_EXPOSE) {
				/* this adds copies and links from all unlinked internal sockets to group inputs/outputs. */
				node_group_expose_all_sockets(ntree);
				has_old_groups = 1;
			}
		}
		
		if (has_old_groups) {
			for (i=0; i < NUM_NTREE_TYPES; ++i) {
				ntreetype= ntreeGetType(i);
				if (ntreetype && ntreetype->foreach_nodetree)
					ntreetype->foreach_nodetree(main, NULL, lib_nodetree_do_versions_group_indices_cb);
			}
		}
		
		for (ntree= main->nodetree.first; ntree; ntree= ntree->id.next)
			ntree->flag &= ~NTREE_DO_VERSIONS_GROUP_EXPOSE;
	}
	
	/* verify all group user nodes */
	for (ntree= main->nodetree.first; ntree; ntree= ntree->id.next) {
		ntreeVerifyNodes(main, &ntree->id);
	}
	
	/* make update calls where necessary */
	{
		for (ntree= main->nodetree.first; ntree; ntree= ntree->id.next)
			if (ntree->update)
				ntreeUpdateTree(ntree);

		for (i=0; i < NUM_NTREE_TYPES; ++i) {
			ntreetype= ntreeGetType(i);
			if (ntreetype && ntreetype->foreach_nodetree)
				ntreetype->foreach_nodetree(main, NULL, lib_nodetree_do_versions_update_cb);
		}
	}
}

static void direct_link_node_socket(FileData *fd, bNodeSocket *sock)
{
	sock->link= newdataadr(fd, sock->link);
	sock->storage= newdataadr(fd, sock->storage);
	sock->default_value= newdataadr(fd, sock->default_value);
	sock->cache= NULL;
}

/* ntree itself has been read! */
static void direct_link_nodetree(FileData *fd, bNodeTree *ntree)
{
	/* note: writing and reading goes in sync, for speed */
	bNode *node;
	bNodeSocket *sock;
	bNodeLink *link;
	
	ntree->init= 0;		/* to set callbacks and force setting types */
	ntree->progress= NULL;
	ntree->execdata= NULL;
	
	ntree->adt= newdataadr(fd, ntree->adt);
	direct_link_animdata(fd, ntree->adt);
	
	link_list(fd, &ntree->nodes);
	for (node= ntree->nodes.first; node; node= node->next) {
		if (node->type == NODE_DYNAMIC) {
			node->custom1= 0;
			node->custom1= BSET(node->custom1, NODE_DYNAMIC_LOADED);
		}

		node->typeinfo= NULL;
		
		link_list(fd, &node->inputs);
		link_list(fd, &node->outputs);
		
		if (node->type == CMP_NODE_MOVIEDISTORTION) {
			node->storage= newmclipadr(fd, node->storage);
		}
		else
			node->storage= newdataadr(fd, node->storage);

		if (node->storage) {
			/* could be handlerized at some point */
			if (ntree->type==NTREE_SHADER && (node->type==SH_NODE_CURVE_VEC || node->type==SH_NODE_CURVE_RGB))
				direct_link_curvemapping(fd, node->storage);
			else if (ntree->type==NTREE_COMPOSIT) {
				if ( ELEM4(node->type, CMP_NODE_TIME, CMP_NODE_CURVE_VEC, CMP_NODE_CURVE_RGB, CMP_NODE_HUECORRECT))
					direct_link_curvemapping(fd, node->storage);
				else if (ELEM3(node->type, CMP_NODE_IMAGE, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER))
					((ImageUser *)node->storage)->ok= 1;
			}
			else if ( ntree->type==NTREE_TEXTURE) {
				if (node->type==TEX_NODE_CURVE_RGB || node->type==TEX_NODE_CURVE_TIME)
					direct_link_curvemapping(fd, node->storage);
				else if (node->type==TEX_NODE_IMAGE)
					((ImageUser *)node->storage)->ok= 1;
			}
		}
	}
	link_list(fd, &ntree->links);
	
	/* external sockets */
	link_list(fd, &ntree->inputs);
	link_list(fd, &ntree->outputs);
	
	/* and we connect the rest */
	for (node= ntree->nodes.first; node; node= node->next) {
		node->parent = newdataadr(fd, node->parent);
		node->preview= newimaadr(fd, node->preview);
		node->lasty= 0;
		
		for (sock= node->inputs.first; sock; sock= sock->next)
			direct_link_node_socket(fd, sock);
		for (sock= node->outputs.first; sock; sock= sock->next)
			direct_link_node_socket(fd, sock);
	}
	for (sock= ntree->inputs.first; sock; sock= sock->next)
		direct_link_node_socket(fd, sock);
	for (sock= ntree->outputs.first; sock; sock= sock->next)
		direct_link_node_socket(fd, sock);
	
	for (link= ntree->links.first; link; link= link->next) {
		link->fromnode= newdataadr(fd, link->fromnode);
		link->tonode= newdataadr(fd, link->tonode);
		link->fromsock= newdataadr(fd, link->fromsock);
		link->tosock= newdataadr(fd, link->tosock);
	}
	
	/* type verification is in lib-link */
}

/* ************ READ ARMATURE ***************** */

/* temp struct used to transport needed info to lib_link_constraint_cb() */
typedef struct tConstraintLinkData {
	FileData *fd;
	ID *id;
} tConstraintLinkData;
/* callback function used to relink constraint ID-links */
static void lib_link_constraint_cb(bConstraint *UNUSED(con), ID **idpoin, void *userdata)
{
	tConstraintLinkData *cld= (tConstraintLinkData *)userdata;
	*idpoin = newlibadr(cld->fd, cld->id->lib, *idpoin);
}

static void lib_link_constraints(FileData *fd, ID *id, ListBase *conlist)
{
	tConstraintLinkData cld;
	bConstraint *con;
	
	/* legacy fixes */
	for (con = conlist->first; con; con=con->next) {
		/* patch for error introduced by changing constraints (dunno how) */
		/* if con->data type changes, dna cannot resolve the pointer! (ton) */
		if (con->data==NULL) {
			con->type= CONSTRAINT_TYPE_NULL;
		}
		/* own ipo, all constraints have it */
		con->ipo= newlibadr_us(fd, id->lib, con->ipo); // XXX depreceated - old animation system
	}
	
	/* relink all ID-blocks used by the constraints */
	cld.fd= fd;
	cld.id= id;
	
	id_loop_constraints(conlist, lib_link_constraint_cb, &cld);
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
				if (data->prop)
					IDP_DirectLinkProperty(data->prop, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
			}
				break;
			case CONSTRAINT_TYPE_SPLINEIK:
			{
				bSplineIKConstraint *data= con->data;
				
				data->points= newdataadr(fd, data->points);
			}
				break;
			case CONSTRAINT_TYPE_KINEMATIC:
			{
				con->lin_error = 0.f;
				con->rot_error = 0.f;
			}
			case CONSTRAINT_TYPE_CHILDOF:
			{
				/* XXX version patch, in older code this flag wasn't always set, and is inherent to type */
				if (con->ownspace == CONSTRAINT_SPACE_POSE)
					con->flag |= CONSTRAINT_SPACEONCE;
			}
				break;
		}
	}
}

static void lib_link_pose(FileData *fd, Object *ob, bPose *pose)
{
	bPoseChannel *pchan;
	bArmature *arm= ob->data;
	int rebuild;
	
	if (!pose || !arm)
		return;
	

	/* always rebuild to match proxy or lib changes */
	rebuild= ob->proxy || (ob->id.lib==NULL && arm->id.lib);

	if (ob->proxy) {
		/* sync proxy layer */
		if (pose->proxy_layer)
			arm->layer = pose->proxy_layer;
		
		/* sync proxy active bone */
		if (pose->proxy_act_bone[0]) {
			Bone *bone = get_named_bone(arm, pose->proxy_act_bone);
			if (bone)
				arm->act_bone = bone;
		}
	}

	for (pchan = pose->chanbase.first; pchan; pchan=pchan->next) {
		lib_link_constraints(fd, (ID *)ob, &pchan->constraints);
		
		/* hurms... loop in a loop, but yah... later... (ton) */
		pchan->bone= get_named_bone(arm, pchan->name);
		
		pchan->custom= newlibadr_us(fd, arm->id.lib, pchan->custom);
		if (pchan->bone==NULL)
			rebuild= 1;
		else if (ob->id.lib==NULL && arm->id.lib) {
			/* local pose selection copied to armature, bit hackish */
			pchan->bone->flag &= ~BONE_SELECTED;
			pchan->bone->flag |= pchan->selectflag;
		}
	}
	
	if (rebuild) {
		ob->recalc= OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME;
		pose->flag |= POSE_RECALC;
	}
}

static void lib_link_armature(FileData *fd, Main *main)
{
	bArmature *arm;

	arm= main->armature.first;

	while (arm) {
		if (arm->id.flag & LIB_NEEDLINK) {
			if (arm->adt) lib_link_animdata(fd, &arm->id, arm->adt);
			arm->id.flag -= LIB_NEEDLINK;
		}
		arm= arm->id.next;
	}
}

static void direct_link_bones(FileData *fd, Bone* bone)
{
	Bone	*child;

	bone->parent= newdataadr(fd, bone->parent);
	bone->prop= newdataadr(fd, bone->prop);
	if (bone->prop)
		IDP_DirectLinkProperty(bone->prop, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
		
	bone->flag &= ~BONE_DRAW_ACTIVE;

	link_list(fd, &bone->childbase);

	for (child=bone->childbase.first; child; child=child->next)
		direct_link_bones(fd, child);
}

static void direct_link_armature(FileData *fd, bArmature *arm)
{
	Bone	*bone;

	link_list(fd, &arm->bonebase);
	arm->edbo= NULL;
	arm->sketch = NULL;
	
	arm->adt= newdataadr(fd, arm->adt);
	direct_link_animdata(fd, arm->adt);
	
	bone=arm->bonebase.first;
	while (bone) {
		direct_link_bones(fd, bone);
		bone=bone->next;
	}

	arm->act_bone= newdataadr(fd, arm->act_bone);
	arm->act_edbone= NULL;
}

/* ************ READ CAMERA ***************** */

static void lib_link_camera(FileData *fd, Main *main)
{
	Camera *ca;

	ca= main->camera.first;
	while (ca) {
		if (ca->id.flag & LIB_NEEDLINK) {
			if (ca->adt) lib_link_animdata(fd, &ca->id, ca->adt);
			
			ca->ipo= newlibadr_us(fd, ca->id.lib, ca->ipo); // XXX depreceated - old animation system
			
			ca->dof_ob= newlibadr_us(fd, ca->id.lib, ca->dof_ob);
			
			ca->id.flag -= LIB_NEEDLINK;
		}
		ca= ca->id.next;
	}
}

static void direct_link_camera(FileData *fd, Camera *ca)
{
	ca->adt= newdataadr(fd, ca->adt);
	direct_link_animdata(fd, ca->adt);
}


/* ************ READ LAMP ***************** */

static void lib_link_lamp(FileData *fd, Main *main)
{
	Lamp *la;
	MTex *mtex;
	int a;

	la= main->lamp.first;
	while (la) {
		if (la->id.flag & LIB_NEEDLINK) {
			if (la->adt) lib_link_animdata(fd, &la->id, la->adt);
			
			for (a=0; a<MAX_MTEX; a++) {
				mtex= la->mtex[a];
				if (mtex) {
					mtex->tex= newlibadr_us(fd, la->id.lib, mtex->tex);
					mtex->object= newlibadr(fd, la->id.lib, mtex->object);
				}
			}
			
			la->ipo= newlibadr_us(fd, la->id.lib, la->ipo); // XXX depreceated - old animation system

			if (la->nodetree)
				lib_link_ntree(fd, &la->id, la->nodetree);
			
			la->id.flag -= LIB_NEEDLINK;
		}
		la= la->id.next;
	}
}

static void direct_link_lamp(FileData *fd, Lamp *la)
{
	int a;
	
	la->adt= newdataadr(fd, la->adt);
	direct_link_animdata(fd, la->adt);

	for (a=0; a<MAX_MTEX; a++) {
		la->mtex[a]= newdataadr(fd, la->mtex[a]);
	}
	
	la->curfalloff= newdataadr(fd, la->curfalloff);
	if (la->curfalloff)
		direct_link_curvemapping(fd, la->curfalloff);

	la->nodetree= newdataadr(fd, la->nodetree);
	if (la->nodetree)
		direct_link_nodetree(fd, la->nodetree);
	
	la->preview = direct_link_preview_image(fd, la->preview);
}

/* ************ READ keys ***************** */

static void lib_link_key(FileData *fd, Main *main)
{
	Key *key;

	key= main->key.first;
	while (key) {
		/*check if we need to generate unique ids for the shapekeys*/
		if (!key->uidgen) {
			KeyBlock *block;

			key->uidgen = 1;
			for (block=key->block.first; block; block=block->next) {
				block->uid = key->uidgen++;
			}
		}

		if (key->id.flag & LIB_NEEDLINK) {
			if (key->adt) lib_link_animdata(fd, &key->id, key->adt);
			
			key->ipo= newlibadr_us(fd, key->id.lib, key->ipo); // XXX depreceated - old animation system
			key->from= newlibadr(fd, key->id.lib, key->from);

			key->id.flag -= LIB_NEEDLINK;
		}
		key= key->id.next;
	}
}

static void switch_endian_keyblock(Key *key, KeyBlock *kb)
{
	int elemsize, a, b;
	char *data, *poin, *cp;

	elemsize= key->elemsize;
	data= kb->data;

	for (a=0; a<kb->totelem; a++) {

		cp= key->elemstr;
		poin= data;

		while ( cp[0] ) {	/* cp[0]==amount */

			switch(cp[1]) {		/* cp[1]= type */
			case IPO_FLOAT:
			case IPO_BPOINT:
			case IPO_BEZTRIPLE:
				b= cp[0];
				while (b--) {
					SWITCH_INT((*poin));
					poin+= 4;
				}
				break;
			}

			cp+= 2;

		}
		data+= elemsize;
	}
}

static void direct_link_key(FileData *fd, Key *key)
{
	KeyBlock *kb;

	link_list(fd, &(key->block));

	key->adt= newdataadr(fd, key->adt);
	direct_link_animdata(fd, key->adt);
	
	key->refkey= newdataadr(fd, key->refkey);

	kb= key->block.first;
	while (kb) {

		kb->data= newdataadr(fd, kb->data);
		
		if (fd->flags & FD_FLAGS_SWITCH_ENDIAN)
			switch_endian_keyblock(key, kb);

		kb= kb->next;
	}
}

/* ************ READ mball ***************** */

static void lib_link_mball(FileData *fd, Main *main)
{
	MetaBall *mb;
	int a;

	mb= main->mball.first;
	while (mb) {
		if (mb->id.flag & LIB_NEEDLINK) {
			if (mb->adt) lib_link_animdata(fd, &mb->id, mb->adt);
			
			for (a=0; a<mb->totcol; a++) mb->mat[a]= newlibadr_us(fd, mb->id.lib, mb->mat[a]);

			mb->ipo= newlibadr_us(fd, mb->id.lib, mb->ipo); // XXX depreceated - old animation system

			mb->id.flag -= LIB_NEEDLINK;
		}
		mb= mb->id.next;
	}
}

static void direct_link_mball(FileData *fd, MetaBall *mb)
{
	mb->adt= newdataadr(fd, mb->adt);
	direct_link_animdata(fd, mb->adt);
	
	mb->mat= newdataadr(fd, mb->mat);
	test_pointer_array(fd, (void **)&mb->mat);

	link_list(fd, &(mb->elems));

	mb->disp.first= mb->disp.last= NULL;
	mb->editelems= NULL;
	mb->bb= NULL;
/*	mb->edit_elems.first= mb->edit_elems.last= NULL;*/
	mb->lastelem= NULL;
}

/* ************ READ WORLD ***************** */

static void lib_link_world(FileData *fd, Main *main)
{
	World *wrld;
	MTex *mtex;
	int a;

	wrld= main->world.first;
	while (wrld) {
		if (wrld->id.flag & LIB_NEEDLINK) {
			if (wrld->adt) lib_link_animdata(fd, &wrld->id, wrld->adt);
			
			wrld->ipo= newlibadr_us(fd, wrld->id.lib, wrld->ipo); // XXX depreceated - old animation system
			
			for (a=0; a<MAX_MTEX; a++) {
				mtex= wrld->mtex[a];
				if (mtex) {
					mtex->tex= newlibadr_us(fd, wrld->id.lib, mtex->tex);
					mtex->object= newlibadr(fd, wrld->id.lib, mtex->object);
				}
			}

			if (wrld->nodetree)
				lib_link_ntree(fd, &wrld->id, wrld->nodetree);
			
			wrld->id.flag -= LIB_NEEDLINK;
		}
		wrld= wrld->id.next;
	}
}

static void direct_link_world(FileData *fd, World *wrld)
{
	int a;

	wrld->adt= newdataadr(fd, wrld->adt);
	direct_link_animdata(fd, wrld->adt);

	for (a=0; a<MAX_MTEX; a++) {
		wrld->mtex[a]= newdataadr(fd, wrld->mtex[a]);
	}

	wrld->nodetree= newdataadr(fd, wrld->nodetree);
	if (wrld->nodetree)
		direct_link_nodetree(fd, wrld->nodetree);

	wrld->preview = direct_link_preview_image(fd, wrld->preview);
}


/* ************ READ VFONT ***************** */

static void lib_link_vfont(FileData *UNUSED(fd), Main *main)
{
	VFont *vf;

	vf= main->vfont.first;
	while (vf) {
		if (vf->id.flag & LIB_NEEDLINK) {
			vf->id.flag -= LIB_NEEDLINK;
		}
		vf= vf->id.next;
	}
}

static void direct_link_vfont(FileData *fd, VFont *vf)
{
	vf->data= NULL;
	vf->packedfile= direct_link_packedfile(fd, vf->packedfile);
}

/* ************ READ TEXT ****************** */

static void lib_link_text(FileData *UNUSED(fd), Main *main)
{
	Text *text;

	text= main->text.first;
	while (text) {
		if (text->id.flag & LIB_NEEDLINK) {
			text->id.flag -= LIB_NEEDLINK;
		}
		text= text->id.next;
	}
}

static void direct_link_text(FileData *fd, Text *text)
{
	TextLine *ln;

	text->name= newdataadr(fd, text->name);

	text->undo_pos= -1;
	text->undo_len= TXT_INIT_UNDO;
	text->undo_buf= MEM_mallocN(text->undo_len, "undo buf");

	text->compiled= NULL;

/*
	if (text->flags & TXT_ISEXT) {
		reopen_text(text);
		}
		else {
*/

	link_list(fd, &text->lines);
	link_list(fd, &text->markers);

	text->curl= newdataadr(fd, text->curl);
	text->sell= newdataadr(fd, text->sell);

	ln= text->lines.first;
	while (ln) {
		ln->line= newdataadr(fd, ln->line);
		ln->format= NULL;
		
		if (ln->len != (int) strlen(ln->line)) {
			printf("Error loading text, line lengths differ\n");
			ln->len = strlen(ln->line);
		}

		ln= ln->next;
	}

	text->flags = (text->flags) & ~TXT_ISEXT;

	text->id.us= 1;
}

/* ************ READ IMAGE ***************** */

static void lib_link_image(FileData *fd, Main *main)
{
	Image *ima;

	ima= main->image.first;
	while (ima) {
		if (ima->id.flag & LIB_NEEDLINK) {
			if (ima->id.properties) IDP_LibLinkProperty(ima->id.properties, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);

			ima->id.flag -= LIB_NEEDLINK;
		}
		ima= ima->id.next;
	}
}

static void link_ibuf_list(FileData *fd, ListBase *lb)
{
	Link *ln, *prev;
	
	if (lb->first==NULL) return;
	
	lb->first= newimaadr(fd, lb->first);
	ln= lb->first;
	prev= NULL;
	while (ln) {
		ln->next= newimaadr(fd, ln->next);
		ln->prev= prev;
		prev= ln;
		ln= ln->next;
	}
	lb->last= prev;
}

static void direct_link_image(FileData *fd, Image *ima)
{
	/* for undo system, pointers could be restored */
	if (fd->imamap)
		link_ibuf_list(fd, &ima->ibufs);
	else
		ima->ibufs.first= ima->ibufs.last= NULL;
	
	/* if not restored, we keep the binded opengl index */
	if (ima->ibufs.first==NULL) {
		ima->bindcode= 0;
		ima->gputexture= NULL;
	}
	
	ima->anim= NULL;
	ima->rr= NULL;
	ima->repbind= NULL;
	
	/* undo system, try to restore render buffers */
	if (fd->imamap) {
		int a;
		
		for (a=0; a<IMA_MAX_RENDER_SLOT; a++)
			ima->renders[a]= newimaadr(fd, ima->renders[a]);
	}
	else {
		memset(ima->renders, 0, sizeof(ima->renders));
		ima->last_render_slot= ima->render_slot;
	}
	
	ima->packedfile = direct_link_packedfile(fd, ima->packedfile);
	ima->preview = direct_link_preview_image(fd, ima->preview);
	ima->ok= 1;
}


/* ************ READ CURVE ***************** */

static void lib_link_curve(FileData *fd, Main *main)
{
	Curve *cu;
	int a;

	cu= main->curve.first;
	while (cu) {
		if (cu->id.flag & LIB_NEEDLINK) {
			if (cu->adt) lib_link_animdata(fd, &cu->id, cu->adt);

			for (a=0; a<cu->totcol; a++) cu->mat[a]= newlibadr_us(fd, cu->id.lib, cu->mat[a]);

			cu->bevobj= newlibadr(fd, cu->id.lib, cu->bevobj);
			cu->taperobj= newlibadr(fd, cu->id.lib, cu->taperobj);
			cu->textoncurve= newlibadr(fd, cu->id.lib, cu->textoncurve);
			cu->vfont= newlibadr_us(fd, cu->id.lib, cu->vfont);
			cu->vfontb= newlibadr_us(fd, cu->id.lib, cu->vfontb);			
			cu->vfonti= newlibadr_us(fd, cu->id.lib, cu->vfonti);
			cu->vfontbi= newlibadr_us(fd, cu->id.lib, cu->vfontbi);

			cu->ipo= newlibadr_us(fd, cu->id.lib, cu->ipo); // XXX depreceated - old animation system
			cu->key= newlibadr_us(fd, cu->id.lib, cu->key);

			cu->id.flag -= LIB_NEEDLINK;
		}
		cu= cu->id.next;
	}
}


static void switch_endian_knots(Nurb *nu)
{
	int len;

	if (nu->knotsu) {
		len= KNOTSU(nu);
		while (len--) {
			SWITCH_INT(nu->knotsu[len]);
		}
	}
	if (nu->knotsv) {
		len= KNOTSV(nu);
		while (len--) {
			SWITCH_INT(nu->knotsv[len]);
		}
	}
}

static void direct_link_curve(FileData *fd, Curve *cu)
{
	Nurb *nu;
	TextBox *tb;
	
	cu->adt= newdataadr(fd, cu->adt);
	direct_link_animdata(fd, cu->adt);
	
	cu->mat= newdataadr(fd, cu->mat);
	test_pointer_array(fd, (void **)&cu->mat);
	cu->str= newdataadr(fd, cu->str);
	cu->strinfo= newdataadr(fd, cu->strinfo);	
	cu->tb= newdataadr(fd, cu->tb);

	if (cu->vfont == NULL) link_list(fd, &(cu->nurb));
	else {
		cu->nurb.first=cu->nurb.last= NULL;

		tb= MEM_callocN(MAXTEXTBOX*sizeof(TextBox), "TextBoxread");
		if (cu->tb) {
			memcpy(tb, cu->tb, cu->totbox*sizeof(TextBox));
			MEM_freeN(cu->tb);
			cu->tb= tb;			
		}
		else {
			cu->totbox = 1;
			cu->actbox = 1;
			cu->tb = tb;
			cu->tb[0].w = cu->linewidth;
		}		
		if (cu->wordspace == 0.0f) cu->wordspace = 1.0f;
	}

	cu->bev.first=cu->bev.last= NULL;
	cu->disp.first=cu->disp.last= NULL;
	cu->editnurb= NULL;
	cu->lastsel= NULL;
	cu->path= NULL;
	cu->editfont= NULL;
	
	nu= cu->nurb.first;
	while (nu) {
		nu->bezt= newdataadr(fd, nu->bezt);
		nu->bp= newdataadr(fd, nu->bp);
		nu->knotsu= newdataadr(fd, nu->knotsu);
		nu->knotsv= newdataadr(fd, nu->knotsv);
		if (cu->vfont == NULL) nu->charidx= nu->mat_nr;

		if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
			switch_endian_knots(nu);
		}

		nu= nu->next;
	}
	cu->bb= NULL;
}

/* ************ READ TEX ***************** */

static void lib_link_texture(FileData *fd, Main *main)
{
	Tex *tex;

	tex= main->tex.first;
	while (tex) {
		if (tex->id.flag & LIB_NEEDLINK) {
			if (tex->adt) lib_link_animdata(fd, &tex->id, tex->adt);

			tex->ima= newlibadr_us(fd, tex->id.lib, tex->ima);
			tex->ipo= newlibadr_us(fd, tex->id.lib, tex->ipo);
			if (tex->env) tex->env->object= newlibadr(fd, tex->id.lib, tex->env->object);
			if (tex->pd)
				tex->pd->object= newlibadr(fd, tex->id.lib, tex->pd->object);
			if (tex->vd) tex->vd->object= newlibadr(fd, tex->id.lib, tex->vd->object);
			if (tex->ot) tex->ot->object= newlibadr(fd, tex->id.lib, tex->ot->object);
				

			if (tex->nodetree)
				lib_link_ntree(fd, &tex->id, tex->nodetree);
			
			tex->id.flag -= LIB_NEEDLINK;
		}
		tex= tex->id.next;
	}
}

static void direct_link_texture(FileData *fd, Tex *tex)
{
	tex->adt= newdataadr(fd, tex->adt);
	direct_link_animdata(fd, tex->adt);
	
	tex->plugin= newdataadr(fd, tex->plugin);
	if (tex->plugin) {
		tex->plugin->handle= NULL;
		open_plugin_tex(tex->plugin);
		/* initialize data for this instance, if an initialization
		 * function exists.
		 */
		if (tex->plugin->instance_init)
			tex->plugin->instance_init((void *) tex->plugin->data);
	}
	tex->coba= newdataadr(fd, tex->coba);
	tex->env= newdataadr(fd, tex->env);
	if (tex->env) {
		tex->env->ima= NULL;
		memset(tex->env->cube, 0, 6*sizeof(void *));
		tex->env->ok= 0;
	}
	tex->pd= newdataadr(fd, tex->pd);
	if (tex->pd) {
		tex->pd->point_tree = NULL;
		tex->pd->coba= newdataadr(fd, tex->pd->coba);
		tex->pd->falloff_curve= newdataadr(fd, tex->pd->falloff_curve);
		if (tex->pd->falloff_curve) {
			direct_link_curvemapping(fd, tex->pd->falloff_curve);
		}
	}
	
	tex->vd= newdataadr(fd, tex->vd);
	if (tex->vd) {
		tex->vd->dataset = NULL;
		tex->vd->ok = 0;
	}
	else {
		if (tex->type == TEX_VOXELDATA)
			tex->vd= MEM_callocN(sizeof(VoxelData), "direct_link_texture VoxelData");
	}
	
	tex->ot= newdataadr(fd, tex->ot);
	
	tex->nodetree= newdataadr(fd, tex->nodetree);
	if (tex->nodetree)
		direct_link_nodetree(fd, tex->nodetree);
	
	tex->preview = direct_link_preview_image(fd, tex->preview);

	tex->iuser.ok= 1;
}



/* ************ READ MATERIAL ***************** */

static void lib_link_material(FileData *fd, Main *main)
{
	Material *ma;
	MTex *mtex;
	int a;

	ma= main->mat.first;
	while (ma) {
		if (ma->id.flag & LIB_NEEDLINK) {
			if (ma->adt) lib_link_animdata(fd, &ma->id, ma->adt);

			/*Link ID Properties -- and copy this comment EXACTLY for easy finding
			of library blocks that implement this.*/
			if (ma->id.properties) IDP_LibLinkProperty(ma->id.properties, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);

			ma->ipo= newlibadr_us(fd, ma->id.lib, ma->ipo);
			ma->group= newlibadr_us(fd, ma->id.lib, ma->group);
			
			for (a=0; a<MAX_MTEX; a++) {
				mtex= ma->mtex[a];
				if (mtex) {
					mtex->tex= newlibadr_us(fd, ma->id.lib, mtex->tex);
					mtex->object= newlibadr(fd, ma->id.lib, mtex->object);
				}
			}
			
			if (ma->nodetree)
				lib_link_ntree(fd, &ma->id, ma->nodetree);
			
			ma->id.flag -= LIB_NEEDLINK;
		}
		ma= ma->id.next;
	}
}

static void direct_link_material(FileData *fd, Material *ma)
{
	int a;

	ma->adt= newdataadr(fd, ma->adt);
	direct_link_animdata(fd, ma->adt);
	
	for (a=0; a<MAX_MTEX; a++) {
		ma->mtex[a]= newdataadr(fd, ma->mtex[a]);
	}

	ma->ramp_col= newdataadr(fd, ma->ramp_col);
	ma->ramp_spec= newdataadr(fd, ma->ramp_spec);
	
	ma->nodetree= newdataadr(fd, ma->nodetree);
	if (ma->nodetree)
		direct_link_nodetree(fd, ma->nodetree);

	ma->preview = direct_link_preview_image(fd, ma->preview);
	ma->gpumaterial.first = ma->gpumaterial.last = NULL;
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
static void direct_link_pointcache(FileData *fd, PointCache *cache)
{
	if ((cache->flag & PTCACHE_DISK_CACHE)==0) {
		PTCacheMem *pm;
		PTCacheExtra *extra;
		int i;

		link_list(fd, &cache->mem_cache);

		pm = cache->mem_cache.first;

		for (; pm; pm=pm->next) {
			for (i=0; i<BPHYS_TOT_DATA; i++) {
				pm->data[i] = newdataadr(fd, pm->data[i]);
				
				/* the cache saves non-struct data without DNA */
				if (pm->data[i] && ptcache_data_struct[i][0]=='\0' && (fd->flags & FD_FLAGS_SWITCH_ENDIAN)) {
					int j, tot= (BKE_ptcache_data_size (i) * pm->totpoint)/4; /* data_size returns bytes */
					int *poin= pm->data[i];
					
					for (j= 0; j<tot; j++)
						SWITCH_INT(poin[j]);
				}
			}
			
			link_list(fd, &pm->extradata);

			for (extra=pm->extradata.first; extra; extra=extra->next)
				extra->data = newdataadr(fd, extra->data);
		}
	}
	else
		cache->mem_cache.first = cache->mem_cache.last = NULL;

	cache->flag &= ~PTCACHE_SIMULATION_VALID;
	cache->simframe= 0;
	cache->edit= NULL;
	cache->free_edit= NULL;
	cache->cached_frames= NULL;
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
		pd->tex=newlibadr_us(fd, id->lib, pd->tex);
}

static void lib_link_particlesettings(FileData *fd, Main *main)
{
	ParticleSettings *part;
	ParticleDupliWeight *dw;
	MTex *mtex;
	int a;

	part= main->particle.first;
	while (part) {
		if (part->id.flag & LIB_NEEDLINK) {
			if (part->adt) lib_link_animdata(fd, &part->id, part->adt);
			part->ipo= newlibadr_us(fd, part->id.lib, part->ipo); // XXX depreceated - old animation system
			
			part->dup_ob = newlibadr(fd, part->id.lib, part->dup_ob);
			part->dup_group = newlibadr(fd, part->id.lib, part->dup_group);
			part->eff_group = newlibadr(fd, part->id.lib, part->eff_group);
			part->bb_ob = newlibadr(fd, part->id.lib, part->bb_ob);

			lib_link_partdeflect(fd, &part->id, part->pd);
			lib_link_partdeflect(fd, &part->id, part->pd2);

			if (part->effector_weights)
				part->effector_weights->group = newlibadr(fd, part->id.lib, part->effector_weights->group);

			if (part->dupliweights.first && part->dup_group) {
				int index_ok = 0;
				/* check for old files without indices (all indexes 0) */
				dw = part->dupliweights.first;
				if (part->dupliweights.first == part->dupliweights.last) {
					/* special case for only one object in the group */
					index_ok = 1;
				}
				else { 
					for (; dw; dw=dw->next) {
						if (dw->index > 0) {
							index_ok = 1;
							break;
						}
					}
				}

				if (index_ok) {
					/* if we have indexes, let's use them */
					dw = part->dupliweights.first;
					for (; dw; dw=dw->next) {
						GroupObject *go = (GroupObject *)BLI_findlink(&part->dup_group->gobject, dw->index);
						dw->ob = go ? go->ob : NULL;
					}
				}
				else {
					/* otherwise try to get objects from own library (won't work on library linked groups) */
					for (; dw; dw=dw->next)
						dw->ob = newlibadr(fd, part->id.lib, dw->ob);
				}
			}
			else {
				part->dupliweights.first = part->dupliweights.last = NULL;
			}

			if (part->boids) {
				BoidState *state = part->boids->states.first;
				BoidRule *rule;
				for (; state; state=state->next) {
					rule = state->rules.first;
				for (; rule; rule=rule->next)
					switch(rule->type) {
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
			
			for (a=0; a<MAX_MTEX; a++) {
				mtex= part->mtex[a];
				if (mtex) {
					mtex->tex = newlibadr_us(fd, part->id.lib, mtex->tex);
					mtex->object = newlibadr(fd, part->id.lib, mtex->object);
				}
			}

			part->id.flag -= LIB_NEEDLINK;
		}
		part= part->id.next;
	}
}

static void direct_link_partdeflect(PartDeflect *pd)
{
	if (pd) pd->rng=NULL;
}

static void direct_link_particlesettings(FileData *fd, ParticleSettings *part)
{
	int a;
	part->adt= newdataadr(fd, part->adt);
	part->pd= newdataadr(fd, part->pd);
	part->pd2= newdataadr(fd, part->pd2);

	direct_link_animdata(fd, part->adt);
	direct_link_partdeflect(part->pd);
	direct_link_partdeflect(part->pd2);

	part->effector_weights = newdataadr(fd, part->effector_weights);
	if (!part->effector_weights)
		part->effector_weights = BKE_add_effector_weights(part->eff_group);

	link_list(fd, &part->dupliweights);

	part->boids= newdataadr(fd, part->boids);
	part->fluid= newdataadr(fd, part->fluid);

	if (part->boids) {
		BoidState *state;
		link_list(fd, &part->boids->states);
		
		for (state=part->boids->states.first; state; state=state->next) {
			link_list(fd, &state->rules);
			link_list(fd, &state->conditions);
			link_list(fd, &state->actions);
		}
	}
	for (a=0; a<MAX_MTEX; a++) {
		part->mtex[a]= newdataadr(fd, part->mtex[a]);
	}
}

static void lib_link_particlesystems(FileData *fd, Object *ob, ID *id, ListBase *particles)
{
	ParticleSystem *psys, *psysnext;

	for (psys=particles->first; psys; psys=psysnext) {
		psysnext= psys->next;
		
		psys->part = newlibadr_us(fd, id->lib, psys->part);
		if (psys->part) {
			ParticleTarget *pt = psys->targets.first;

			for (; pt; pt=pt->next)
				pt->ob=newlibadr(fd, id->lib, pt->ob);

			psys->parent= newlibadr_us(fd, id->lib, psys->parent);
			psys->target_ob = newlibadr(fd, id->lib, psys->target_ob);

			if (psys->clmd) {
				/* XXX - from reading existing code this seems correct but intended usage of
				 * pointcache should /w cloth should be added in 'ParticleSystem' - campbell */
				psys->clmd->point_cache= psys->pointcache;
				psys->clmd->ptcaches.first= psys->clmd->ptcaches.last= NULL;
				psys->clmd->coll_parms->group= newlibadr(fd, id->lib, psys->clmd->coll_parms->group);
			}
		}
		else {
			/* particle modifier must be removed before particle system */
			ParticleSystemModifierData *psmd= psys_get_modifier(ob,psys);
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
		psys->particles=newdataadr(fd,psys->particles);
		
		if (psys->particles && psys->particles->hair) {
			for (a=0,pa=psys->particles; a<psys->totpart; a++, pa++)
				pa->hair=newdataadr(fd,pa->hair);
		}
		
		if (psys->particles && psys->particles->keys) {
			for (a=0,pa=psys->particles; a<psys->totpart; a++, pa++) {
				pa->keys= NULL;
				pa->totkey= 0;
			}

			psys->flag &= ~PSYS_KEYED;
		}

		if (psys->particles && psys->particles->boid) {
			pa = psys->particles;
			pa->boid = newdataadr(fd, pa->boid);
			for (a=1,pa++; a<psys->totpart; a++, pa++)
				pa->boid = (pa-1)->boid + 1;
		}
		else if (psys->particles) {
			for (a=0,pa=psys->particles; a<psys->totpart; a++, pa++)
				pa->boid = NULL;
		}

		psys->fluid_springs = newdataadr(fd, psys->fluid_springs);

		psys->child = newdataadr(fd,psys->child);
		psys->effectors = NULL;

		link_list(fd, &psys->targets);

		psys->edit = NULL;
		psys->free_edit = NULL;
		psys->pathcache = NULL;
		psys->childcache = NULL;
		psys->pathcachebufs.first = psys->pathcachebufs.last = NULL;
		psys->childcachebufs.first = psys->childcachebufs.last = NULL;
		psys->frand = NULL;
		psys->pdd = NULL;
		psys->renderdata = NULL;
		
		direct_link_pointcache_list(fd, &psys->ptcaches, &psys->pointcache, 0);

		if (psys->clmd) {
			psys->clmd = newdataadr(fd, psys->clmd);
			psys->clmd->clothObject = NULL;
			
			psys->clmd->sim_parms= newdataadr(fd, psys->clmd->sim_parms);
			psys->clmd->sim_parms->effector_weights = NULL;
			psys->clmd->coll_parms= newdataadr(fd, psys->clmd->coll_parms);
			
			if (psys->clmd->sim_parms) {
				if (psys->clmd->sim_parms->presets > 10)
					psys->clmd->sim_parms->presets = 0;
			}

			psys->hair_in_dm = psys->hair_out_dm = NULL;

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
	for (i=0; i<totface; i++, tf++) {
		tf->tpage= newlibadr(fd, me->id.lib, tf->tpage);
		if (tf->tpage && tf->tpage->id.us==0)
			tf->tpage->id.us= 1;
	}
}

static void lib_link_customdata_mtface(FileData *fd, Mesh *me, CustomData *fdata, int totface)
{
	int i;	
	for (i=0; i<fdata->totlayer; i++) {
		CustomDataLayer *layer = &fdata->layers[i];
		
		if (layer->type == CD_MTFACE)
			lib_link_mtface(fd, me, layer->data, totface);
	}

}

static void lib_link_customdata_mtpoly(FileData *fd, Mesh *me, CustomData *pdata, int totface)
{
	int i;

	for (i=0; i<pdata->totlayer; i++) {
		CustomDataLayer *layer = &pdata->layers[i];
		
		if (layer->type == CD_MTEXPOLY) {
			MTexPoly *tf= layer->data;
			int i;

			for (i=0; i<totface; i++, tf++) {
				tf->tpage= newlibadr(fd, me->id.lib, tf->tpage);
				if (tf->tpage && tf->tpage->id.us==0)
					tf->tpage->id.us= 1;
			}
		}
	}
}

static void lib_link_mesh(FileData *fd, Main *main)
{
	Mesh *me;

	me= main->mesh.first;
	while (me) {
		if (me->id.flag & LIB_NEEDLINK) {
			int i;

			/*Link ID Properties -- and copy this comment EXACTLY for easy finding
			of library blocks that implement this.*/
			if (me->id.properties) IDP_LibLinkProperty(me->id.properties, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
			if (me->adt) lib_link_animdata(fd, &me->id, me->adt);

			/* this check added for python created meshes */
			if (me->mat) {
				for (i=0; i<me->totcol; i++) {
					me->mat[i]= newlibadr_us(fd, me->id.lib, me->mat[i]);
				}
			}
			else me->totcol= 0;

			me->ipo= newlibadr_us(fd, me->id.lib, me->ipo);
			me->key= newlibadr_us(fd, me->id.lib, me->key);
			me->texcomesh= newlibadr_us(fd, me->id.lib, me->texcomesh);

			lib_link_customdata_mtface(fd, me, &me->fdata, me->totface);
			lib_link_customdata_mtpoly(fd, me, &me->pdata, me->totpoly);
			if (me->mr && me->mr->levels.first)
				lib_link_customdata_mtface(fd, me, &me->mr->fdata,
							   ((MultiresLevel*)me->mr->levels.first)->totface);

			/*check if we need to convert mfaces to mpolys*/
			if (me->totface && !me->totpoly) {
				/* temporarily switch main so that reading from
				   external CustomData works */
				Main *gmain = G.main;
				G.main = main;
				
				BKE_mesh_convert_mfaces_to_mpolys(me);

				G.main = gmain;
			}
			
			/*
			 * Re-tessellate, even if the polys were just created from tessfaces, this
			 * is important because it:
			 *  - fill the CD_POLYINDEX layer
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

			me->id.flag -= LIB_NEEDLINK;
		}
		me= me->id.next;
	}

	/* convert texface options to material */
	convert_tface_mt(fd, main);
}

static void direct_link_dverts(FileData *fd, int count, MDeformVert *mdverts)
{
	int i;

	if (mdverts == NULL) {
		return;
	}

	for (i= count; i > 0; i--, mdverts++) {
		/*convert to vgroup allocation system*/
		MDeformWeight *dw;
		if (mdverts->dw && (dw= newdataadr(fd, mdverts->dw))) {
			const ssize_t dw_len= mdverts->totweight * sizeof(MDeformWeight);
			void *dw_tmp= MEM_mallocN(dw_len, "direct_link_dverts");
			memcpy(dw_tmp, dw, dw_len);
			mdverts->dw= dw_tmp;
			MEM_freeN(dw);
		}
		else {
			mdverts->dw= NULL;
			mdverts->totweight= 0;
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
				   if loading pre-BMesh face mdisps this will be
				   overwritten with the correct value in
				   bm_corners_to_loops() */
				float gridsize = sqrtf(mdisps[i].totdisp);
				mdisps[i].level = (int)(logf(gridsize - 1.0f) / M_LN2) + 1;
			}

			if ( (fd->flags & FD_FLAGS_SWITCH_ENDIAN) && (mdisps[i].disps) ) {
				/* DNA_struct_switch_endian doesn't do endian swap for (*disps)[] */
				/* this does swap for data written at write_mdisps() - readfile.c */
				int x;
				float *tmpdisps= *mdisps[i].disps;
				for (x=0;x<mdisps[i].totdisp*3;x++) {
					SWITCH_INT(*tmpdisps);
					tmpdisps++;
				}
			}
			if (!external && !mdisps[i].disps)
				mdisps[i].totdisp = 0;
		}
	}
}

/*this isn't really a public api function, so prototyped here*/
static void direct_link_customdata(FileData *fd, CustomData *data, int count)
{
	int i = 0;

	data->layers= newdataadr(fd, data->layers);
	data->external= newdataadr(fd, data->external);

	while (i < data->totlayer) {
		CustomDataLayer *layer = &data->layers[i];

		if (layer->flag & CD_FLAG_EXTERNAL)
			layer->flag &= ~CD_FLAG_IN_MEMORY;

		if (CustomData_verify_versions(data, i)) {
			layer->data = newdataadr(fd, layer->data);
			if (layer->type == CD_MDISPS)
				direct_link_mdisps(fd, count, layer->data, layer->flag & CD_FLAG_EXTERNAL);
			i++;
		}
	}

	CustomData_update_typemap(data);
}

static void direct_link_mesh(FileData *fd, Mesh *mesh)
{
	mesh->mat= newdataadr(fd, mesh->mat);
	test_pointer_array(fd, (void **)&mesh->mat);

	mesh->mvert= newdataadr(fd, mesh->mvert);
	mesh->medge= newdataadr(fd, mesh->medge);
	mesh->mface= newdataadr(fd, mesh->mface);
	mesh->mloop= newdataadr(fd, mesh->mloop);
	mesh->mpoly= newdataadr(fd, mesh->mpoly);
	mesh->tface= newdataadr(fd, mesh->tface);
	mesh->mtface= newdataadr(fd, mesh->mtface);
	mesh->mcol= newdataadr(fd, mesh->mcol);
	mesh->msticky= newdataadr(fd, mesh->msticky);
	mesh->dvert= newdataadr(fd, mesh->dvert);
	mesh->mloopcol= newdataadr(fd, mesh->mloopcol);
	mesh->mloopuv= newdataadr(fd, mesh->mloopuv);
	mesh->mtpoly= newdataadr(fd, mesh->mtpoly);
	mesh->mselect = newdataadr(fd, mesh->mselect);

	/* animdata */
	mesh->adt= newdataadr(fd, mesh->adt);
	direct_link_animdata(fd, mesh->adt);

	/* normally direct_link_dverts should be called in direct_link_customdata,
	   but for backwards compat in do_versions to work we do it here */
	direct_link_dverts(fd, mesh->totvert, mesh->dvert);

	direct_link_customdata(fd, &mesh->vdata, mesh->totvert);
	direct_link_customdata(fd, &mesh->edata, mesh->totedge);
	direct_link_customdata(fd, &mesh->fdata, mesh->totface);
	direct_link_customdata(fd, &mesh->ldata, mesh->totloop);
	direct_link_customdata(fd, &mesh->pdata, mesh->totpoly);
	

#ifdef USE_BMESH_FORWARD_COMPAT
	/* NEVER ENABLE THIS CODE INTO BMESH!
	 * THIS IS FOR LOADING BMESH INTO OLDER FILES ONLY */
	mesh->mpoly= newdataadr(fd, mesh->mpoly);
	mesh->mloop= newdataadr(fd, mesh->mloop);

	direct_link_customdata(fd, &mesh->pdata, mesh->totpoly);
	direct_link_customdata(fd, &mesh->ldata, mesh->totloop);

	if (mesh->mpoly) {
		/* be clever and load polygons as mfaces */

		mesh->totface= mesh_mpoly_to_mface(&mesh->fdata, &mesh->ldata, &mesh->pdata,
		                                   mesh->totface, mesh->totloop, mesh->totpoly);

		CustomData_free(&mesh->pdata, mesh->totpoly);
		memset(&mesh->pdata, 0, sizeof(CustomData));
		mesh->totpoly = 0;

		CustomData_free(&mesh->ldata, mesh->totloop);
		memset(&mesh->ldata, 0, sizeof(CustomData));
		mesh->totloop = 0;

		mesh_update_customdata_pointers(mesh);
	}

#endif


	mesh->bb= NULL;
	mesh->edit_btmesh= NULL;
	
	/* Multires data */
	mesh->mr= newdataadr(fd, mesh->mr);
	if (mesh->mr) {
		MultiresLevel *lvl;
		
		link_list(fd, &mesh->mr->levels);
		lvl= mesh->mr->levels.first;
		
		direct_link_customdata(fd, &mesh->mr->vdata, lvl->totvert);
		direct_link_dverts(fd, lvl->totvert, CustomData_get(&mesh->mr->vdata, 0, CD_MDEFORMVERT));
		direct_link_customdata(fd, &mesh->mr->fdata, lvl->totface);
		
		mesh->mr->edge_flags= newdataadr(fd, mesh->mr->edge_flags);
		mesh->mr->edge_creases= newdataadr(fd, mesh->mr->edge_creases);

		mesh->mr->verts = newdataadr(fd, mesh->mr->verts);
		
		/* If mesh has the same number of vertices as the
		   highest multires level, load the current mesh verts
		   into multires and discard the old data. Needed
		   because some saved files either do not have a verts
		   array, or the verts array contains out-of-date
		   data. */
		if (mesh->totvert == ((MultiresLevel*)mesh->mr->levels.last)->totvert) {
			if (mesh->mr->verts)
				MEM_freeN(mesh->mr->verts);
			mesh->mr->verts = MEM_dupallocN(mesh->mvert);
		}
			
		for (; lvl; lvl= lvl->next) {
			lvl->verts= newdataadr(fd, lvl->verts);
			lvl->faces= newdataadr(fd, lvl->faces);
			lvl->edges= newdataadr(fd, lvl->edges);
			lvl->colfaces= newdataadr(fd, lvl->colfaces);
		}
	}

	/* if multires is present but has no valid vertex data,
	 * there's no way to recover it; silently remove multires */
	if (mesh->mr && !mesh->mr->verts) {
		multires_free(mesh->mr);
		mesh->mr = NULL;
	}
	
	if ((fd->flags & FD_FLAGS_SWITCH_ENDIAN) && mesh->tface) {
		TFace *tf= mesh->tface;
		int i;

		for (i=0; i< (mesh->totface); i++, tf++) {
			SWITCH_INT(tf->col[0]);
			SWITCH_INT(tf->col[1]);
			SWITCH_INT(tf->col[2]);
			SWITCH_INT(tf->col[3]);
		}
	}
}

/* ************ READ LATTICE ***************** */

static void lib_link_latt(FileData *fd, Main *main)
{
	Lattice *lt;
	
	lt= main->latt.first;
	while (lt) {
		if (lt->id.flag & LIB_NEEDLINK) {
			if (lt->adt) lib_link_animdata(fd, &lt->id, lt->adt);
			
			lt->ipo= newlibadr_us(fd, lt->id.lib, lt->ipo); // XXX depreceated - old animation system
			lt->key= newlibadr_us(fd, lt->id.lib, lt->key);
			
			lt->id.flag -= LIB_NEEDLINK;
		}
		lt= lt->id.next;
	}
}

static void direct_link_latt(FileData *fd, Lattice *lt)
{
	lt->def= newdataadr(fd, lt->def);
	
	lt->dvert= newdataadr(fd, lt->dvert);
	direct_link_dverts(fd, lt->pntsu*lt->pntsv*lt->pntsw, lt->dvert);
	
	lt->editlatt= NULL;
	
	lt->adt = newdataadr(fd, lt->adt);
	direct_link_animdata(fd, lt->adt);
}


/* ************ READ OBJECT ***************** */

static void lib_link_modifiers__linkModifiers(void *userData, Object *ob,
											  ID **idpoin)
{
	FileData *fd = userData;

	*idpoin = newlibadr(fd, ob->id.lib, *idpoin);
	/* hardcoded bad exception; non-object modifier data gets user count (texture, displace) */
	if (*idpoin && GS((*idpoin)->name)!=ID_OB)
		(*idpoin)->us++;
}
static void lib_link_modifiers(FileData *fd, Object *ob)
{
	modifiers_foreachIDLink(ob, lib_link_modifiers__linkModifiers, fd);
}

static void lib_link_object(FileData *fd, Main *main)
{
	Object *ob;
	PartEff *paf;
	bSensor *sens;
	bController *cont;
	bActuator *act;
	void *poin;
	int warn=0, a;

	ob= main->object.first;
	while (ob) {
		if (ob->id.flag & LIB_NEEDLINK) {
			if (ob->id.properties) IDP_LibLinkProperty(ob->id.properties, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
			if (ob->adt) lib_link_animdata(fd, &ob->id, ob->adt);
			
// XXX depreceated - old animation system <<<			
			ob->ipo= newlibadr_us(fd, ob->id.lib, ob->ipo);
			ob->action = newlibadr_us(fd, ob->id.lib, ob->action);
// >>> XXX depreceated - old animation system

			ob->parent= newlibadr(fd, ob->id.lib, ob->parent);
			ob->track= newlibadr(fd, ob->id.lib, ob->track);
			ob->poselib= newlibadr_us(fd, ob->id.lib, ob->poselib);
			ob->dup_group= newlibadr_us(fd, ob->id.lib, ob->dup_group);
			
			ob->proxy= newlibadr_us(fd, ob->id.lib, ob->proxy);
			if (ob->proxy) {
				/* paranoia check, actually a proxy_from pointer should never be written... */
				if (ob->proxy->id.lib==NULL) {
					ob->proxy->proxy_from= NULL;
					ob->proxy= NULL;
					
					if (ob->id.lib)
						printf("Proxy lost from  object %s lib %s\n", ob->id.name+2, ob->id.lib->name);
					else
						printf("Proxy lost from  object %s lib <NONE>\n", ob->id.name+2);
				}
				else {
					/* this triggers object_update to always use a copy */
					ob->proxy->proxy_from= ob;
					/* force proxy updates after load/undo, a bit weak */
					ob->recalc= ob->proxy->recalc= OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME;
				}
			}
			ob->proxy_group= newlibadr(fd, ob->id.lib, ob->proxy_group);
			
			poin= ob->data;
			ob->data= newlibadr_us(fd, ob->id.lib, ob->data);

			if (ob->data==NULL && poin!=NULL) {
				if (ob->id.lib)
					printf("Can't find obdata of %s lib %s\n", ob->id.name+2, ob->id.lib->name);
				else
					printf("Object %s lost data.\n", ob->id.name+2);

				ob->type= OB_EMPTY;
				warn= 1;

				if (ob->pose) {
					free_pose(ob->pose);
					ob->pose= NULL;
					ob->mode &= ~OB_MODE_POSE;
				}
			}
			for (a=0; a<ob->totcol; a++) ob->mat[a]= newlibadr_us(fd, ob->id.lib, ob->mat[a]);
			
			/* When the object is local and the data is library its possible
			 * the material list size gets out of sync. [#22663] */
			if (ob->data && ob->id.lib != ((ID *)ob->data)->lib) {
				short *totcol_data= give_totcolp(ob);
				/* Only expand so as not to loose any object materials that might be set. */
				if (totcol_data && *totcol_data > ob->totcol) {
					/* printf("'%s' %d -> %d\n", ob->id.name, ob->totcol, *totcol_data); */
					resize_object_material(ob, *totcol_data);
				}
			}

			ob->gpd= newlibadr_us(fd, ob->id.lib, ob->gpd);
			ob->duplilist= NULL;

			ob->id.flag -= LIB_NEEDLINK;
			/* if id.us==0 a new base will be created later on */
			
			/* WARNING! Also check expand_object(), should reflect the stuff below. */
			lib_link_pose(fd, ob, ob->pose);
			lib_link_constraints(fd, &ob->id, &ob->constraints);
			
// XXX depreceated - old animation system <<<	
			lib_link_constraint_channels(fd, &ob->id, &ob->constraintChannels);
			lib_link_nlastrips(fd, &ob->id, &ob->nlastrips);
// >>> XXX depreceated - old animation system

			for (paf= ob->effect.first; paf; paf= paf->next) {
				if (paf->type==EFF_PARTICLE) {
					paf->group= newlibadr_us(fd, ob->id.lib, paf->group);
				}
			}				

			sens= ob->sensors.first;
			while (sens) {
				for (a=0; a<sens->totlinks; a++)
					sens->links[a]= newglobadr(fd, sens->links[a]);

				if (sens->type==SENS_TOUCH) {
					bTouchSensor *ts= sens->data;
					ts->ma= newlibadr(fd, ob->id.lib, ts->ma);
				}
				else if (sens->type==SENS_MESSAGE) {
					bMessageSensor *ms= sens->data;
					ms->fromObject=
						newlibadr(fd, ob->id.lib, ms->fromObject);
				}
				sens= sens->next;
			}

			cont= ob->controllers.first;
			while (cont) {
				for (a=0; a<cont->totlinks; a++)
					cont->links[a]= newglobadr(fd, cont->links[a]);

				if (cont->type==CONT_PYTHON) {
					bPythonCont *pc= cont->data;
					pc->text= newlibadr(fd, ob->id.lib, pc->text);
				}
				cont->slinks= NULL;
				cont->totslinks= 0;

				cont= cont->next;
			}

			act= ob->actuators.first;
			while (act) {
				if (act->type==ACT_SOUND) {
					bSoundActuator *sa= act->data;
					sa->sound= newlibadr_us(fd, ob->id.lib, sa->sound);
				}
				else if (act->type==ACT_GAME) {
					/* bGameActuator *ga= act->data; */
				}
				else if (act->type==ACT_CAMERA) {
					bCameraActuator *ca= act->data;
					ca->ob= newlibadr(fd, ob->id.lib, ca->ob);
				}
					/* leave this one, it's obsolete but necessary to read for conversion */
				else if (act->type==ACT_ADD_OBJECT) {
					bAddObjectActuator *eoa= act->data;
					if (eoa) eoa->ob= newlibadr(fd, ob->id.lib, eoa->ob);
				}
				else if (act->type==ACT_OBJECT) {
					bObjectActuator *oa= act->data;
					if (oa==NULL) {
						init_actuator(act);
					}
					else {
						oa->reference= newlibadr(fd, ob->id.lib, oa->reference);
					}
				}
				else if (act->type==ACT_EDIT_OBJECT) {
					bEditObjectActuator *eoa= act->data;
					if (eoa==NULL) {
						init_actuator(act);
					}
					else {
						eoa->ob= newlibadr(fd, ob->id.lib, eoa->ob);
						eoa->me= newlibadr(fd, ob->id.lib, eoa->me);
					}
				}
				else if (act->type==ACT_SCENE) {
					bSceneActuator *sa= act->data;
					sa->camera= newlibadr(fd, ob->id.lib, sa->camera);
					sa->scene= newlibadr(fd, ob->id.lib, sa->scene);
				}
				else if (act->type==ACT_ACTION) {
					bActionActuator *aa= act->data;
					aa->act= newlibadr(fd, ob->id.lib, aa->act);
				}
				else if (act->type==ACT_SHAPEACTION) {
					bActionActuator *aa= act->data;
					aa->act= newlibadr(fd, ob->id.lib, aa->act);
				}
				else if (act->type==ACT_PROPERTY) {
					bPropertyActuator *pa= act->data;
					pa->ob= newlibadr(fd, ob->id.lib, pa->ob);
				}
				else if (act->type==ACT_MESSAGE) {
					bMessageActuator *ma= act->data;
					ma->toObject= newlibadr(fd, ob->id.lib, ma->toObject);
				}
				else if (act->type==ACT_2DFILTER) {
					bTwoDFilterActuator *_2dfa = act->data; 
					_2dfa->text= newlibadr(fd, ob->id.lib, _2dfa->text);
				}
				else if (act->type==ACT_PARENT) {
					bParentActuator *parenta = act->data; 
					parenta->ob = newlibadr(fd, ob->id.lib, parenta->ob);
				}
				else if (act->type==ACT_STATE) {
					/* bStateActuator *statea = act->data; */
				}
				else if (act->type==ACT_ARMATURE) {
					bArmatureActuator *arma= act->data;
					arma->target= newlibadr(fd, ob->id.lib, arma->target);
					arma->subtarget= newlibadr(fd, ob->id.lib, arma->subtarget);
				}
				else if (act->type==ACT_STEERING) {
					bSteeringActuator *steeringa = act->data; 
					steeringa->target = newlibadr(fd, ob->id.lib, steeringa->target);
					steeringa->navmesh = newlibadr(fd, ob->id.lib, steeringa->navmesh);
				}
				act= act->next;
			}
			
			{
				FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(ob, eModifierType_Fluidsim);
				
				if (fluidmd && fluidmd->fss)
					fluidmd->fss->ipo = newlibadr_us(fd, ob->id.lib, fluidmd->fss->ipo);
			}

			{
				SmokeModifierData *smd = (SmokeModifierData *)modifiers_findByType(ob, eModifierType_Smoke);
				
				if (smd && smd->type == MOD_SMOKE_TYPE_DOMAIN && smd->domain)
				{
					smd->domain->flags |= MOD_SMOKE_FILE_LOAD; /* flag for refreshing the simulation after loading */
				}
			}

			/* texture field */
			if (ob->pd)
				lib_link_partdeflect(fd, &ob->id, ob->pd);

			if (ob->soft)
				ob->soft->effector_weights->group = newlibadr(fd, ob->id.lib, ob->soft->effector_weights->group);

			lib_link_particlesystems(fd, ob, &ob->id, &ob->particlesystem);
			lib_link_modifiers(fd, ob);
		}
		ob= ob->id.next;
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

	pose->chanhash= NULL;

	for (pchan = pose->chanbase.first; pchan; pchan=pchan->next) {
		pchan->bone= NULL;
		pchan->parent= newdataadr(fd, pchan->parent);
		pchan->child= newdataadr(fd, pchan->child);
		pchan->custom_tx= newdataadr(fd, pchan->custom_tx);
		
		direct_link_constraints(fd, &pchan->constraints);
		
		pchan->prop = newdataadr(fd, pchan->prop);
		if (pchan->prop)
			IDP_DirectLinkProperty(pchan->prop, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
		
		pchan->mpath= newdataadr(fd, pchan->mpath);
		if (pchan->mpath)
			direct_link_motionpath(fd, pchan->mpath);
		
		pchan->iktree.first= pchan->iktree.last= NULL;
		pchan->siktree.first= pchan->siktree.last= NULL;
		
		/* in case this value changes in future, clamp else we get undefined behavior */
		CLAMP(pchan->rotmode, ROT_MODE_MIN, ROT_MODE_MAX);
	}
	pose->ikdata = NULL;
	if (pose->ikparam != NULL) {
		pose->ikparam= newdataadr(fd, pose->ikparam);
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
		if (NULL==modifierType_getInfo(md->type))
			md->type= eModifierType_None;
			
		if (md->type==eModifierType_Subsurf) {
			SubsurfModifierData *smd = (SubsurfModifierData*) md;

			smd->emCache = smd->mCache = NULL;
		}
		else if (md->type==eModifierType_Armature) {
			ArmatureModifierData *amd = (ArmatureModifierData*) md;
			
			amd->prevCos= NULL;
		}
		else if (md->type==eModifierType_Cloth) {
			ClothModifierData *clmd = (ClothModifierData*) md;
			
			clmd->clothObject = NULL;
			
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
		}
		else if (md->type==eModifierType_Fluidsim) {
			FluidsimModifierData *fluidmd = (FluidsimModifierData*) md;
			
			fluidmd->fss= newdataadr(fd, fluidmd->fss);
			if (fluidmd->fss) {
				fluidmd->fss->fmd= fluidmd;
				fluidmd->fss->meshVelocities = NULL;
			}
		}
		else if (md->type==eModifierType_Smoke) {
			SmokeModifierData *smd = (SmokeModifierData*) md;

			if (smd->type==MOD_SMOKE_TYPE_DOMAIN)
			{
				smd->flow = NULL;
				smd->coll = NULL;
				smd->domain = newdataadr(fd, smd->domain);
				smd->domain->smd = smd;

				smd->domain->fluid = NULL;
				smd->domain->wt = NULL;
				smd->domain->shadow = NULL;
				smd->domain->tex = NULL;
				smd->domain->tex_shadow = NULL;
				smd->domain->tex_wt = NULL;

				smd->domain->effector_weights = newdataadr(fd, smd->domain->effector_weights);
				if (!smd->domain->effector_weights)
					smd->domain->effector_weights = BKE_add_effector_weights(NULL);

				direct_link_pointcache_list(fd, &(smd->domain->ptcaches[0]), &(smd->domain->point_cache[0]), 1);

				/* Smoke uses only one cache from now on, so store pointer convert */
				if (smd->domain->ptcaches[1].first || smd->domain->point_cache[1]) {
					if (smd->domain->point_cache[1]) {
						PointCache *cache = newdataadr(fd, smd->domain->point_cache[1]);
						if (cache->flag & PTCACHE_FAKE_SMOKE)
							; /* Smoke was already saved in "new format" and this cache is a fake one. */
						else
							printf("High resolution smoke cache not available due to pointcache update. Please reset the simulation.\n");
						BKE_ptcache_free(cache);
					}
					smd->domain->ptcaches[1].first = NULL;
					smd->domain->ptcaches[1].last = NULL;
					smd->domain->point_cache[1] = NULL;
				}
			}
			else if (smd->type==MOD_SMOKE_TYPE_FLOW) {
				smd->domain = NULL;
				smd->coll = NULL;
				smd->flow = newdataadr(fd, smd->flow);
				smd->flow->smd = smd;
				smd->flow->psys = newdataadr(fd, smd->flow->psys);
			}
			else if (smd->type==MOD_SMOKE_TYPE_COLL) {
				smd->flow = NULL;
				smd->domain = NULL;
				smd->coll = newdataadr(fd, smd->coll);
				if (smd->coll)
				{
					smd->coll->points = NULL;
					smd->coll->numpoints = 0;
				}
				else
					smd->type = 0;

			}
		}
		else if (md->type==eModifierType_DynamicPaint) {
			DynamicPaintModifierData *pmd = (DynamicPaintModifierData*) md;

			if (pmd->canvas)
			{
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
			if (pmd->brush)
			{
				pmd->brush = newdataadr(fd, pmd->brush);
				pmd->brush->pmd = pmd;
				pmd->brush->psys = newdataadr(fd, pmd->brush->psys);
				pmd->brush->paint_ramp = newdataadr(fd, pmd->brush->paint_ramp);
				pmd->brush->vel_ramp = newdataadr(fd, pmd->brush->vel_ramp);
				pmd->brush->dm = NULL;
			}
		}
		else if (md->type==eModifierType_Collision) {
			
			CollisionModifierData *collmd = (CollisionModifierData*) md;
			/*
			// TODO: CollisionModifier should use pointcache 
			// + have proper reset events before enabling this
			collmd->x = newdataadr(fd, collmd->x);
			collmd->xnew = newdataadr(fd, collmd->xnew);
			collmd->mfaces = newdataadr(fd, collmd->mfaces);
			
			collmd->current_x = MEM_callocN(sizeof(MVert)*collmd->numverts,"current_x");
			collmd->current_xnew = MEM_callocN(sizeof(MVert)*collmd->numverts,"current_xnew");
			collmd->current_v = MEM_callocN(sizeof(MVert)*collmd->numverts,"current_v");
			*/
			
			collmd->x = NULL;
			collmd->xnew = NULL;
			collmd->current_x = NULL;
			collmd->current_xnew = NULL;
			collmd->current_v = NULL;
			collmd->time_x = collmd->time_xnew = -1000;
			collmd->numverts = 0;
			collmd->bvhtree = NULL;
			collmd->mfaces = NULL;
			
		}
		else if (md->type==eModifierType_Surface) {
			SurfaceModifierData *surmd = (SurfaceModifierData*) md;

			surmd->dm = NULL;
			surmd->bvhtree = NULL;
			surmd->x = NULL;
			surmd->v = NULL;
			surmd->numverts = 0;
		}
		else if (md->type==eModifierType_Hook) {
			HookModifierData *hmd = (HookModifierData*) md;

			hmd->indexar= newdataadr(fd, hmd->indexar);
			if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
				int a;
				for (a=0; a<hmd->totindex; a++) {
					SWITCH_INT(hmd->indexar[a]);
				}
			}
		}
		else if (md->type==eModifierType_ParticleSystem) {
			ParticleSystemModifierData *psmd = (ParticleSystemModifierData*) md;

			psmd->dm= NULL;
			psmd->psys= newdataadr(fd, psmd->psys);
			psmd->flag &= ~eParticleSystemFlag_psys_updated;
			psmd->flag |= eParticleSystemFlag_file_loaded;
		}
		else if (md->type==eModifierType_Explode) {
			ExplodeModifierData *psmd = (ExplodeModifierData*) md;

			psmd->facepa=NULL;
		}
		else if (md->type==eModifierType_MeshDeform) {
			MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;

			mmd->bindinfluences= newdataadr(fd, mmd->bindinfluences);
			mmd->bindoffsets= newdataadr(fd, mmd->bindoffsets);
			mmd->bindcagecos= newdataadr(fd, mmd->bindcagecos);
			mmd->dyngrid= newdataadr(fd, mmd->dyngrid);
			mmd->dyninfluences= newdataadr(fd, mmd->dyninfluences);
			mmd->dynverts= newdataadr(fd, mmd->dynverts);

			mmd->bindweights= newdataadr(fd, mmd->bindweights);
			mmd->bindcos= newdataadr(fd, mmd->bindcos);

			if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
				int a;

				if (mmd->bindoffsets)
					for (a=0; a<mmd->totvert+1; a++)
						SWITCH_INT(mmd->bindoffsets[a]);
				if (mmd->bindcagecos)
					for (a=0; a<mmd->totcagevert*3; a++)
						SWITCH_INT(mmd->bindcagecos[a]);
				if (mmd->dynverts)
					for (a=0; a<mmd->totvert; a++)
						SWITCH_INT(mmd->dynverts[a]);

				if (mmd->bindweights)
					for (a=0; a<mmd->totcagevert*mmd->totvert; a++)
						SWITCH_INT(mmd->bindweights[a]);
				if (mmd->bindcos)
					for (a=0; a<mmd->totcagevert*3; a++)
						SWITCH_INT(mmd->bindcos[a]);
			}
		}
		else if (md->type==eModifierType_Ocean) {
			OceanModifierData *omd = (OceanModifierData*) md;
			omd->oceancache = NULL;
			omd->ocean = NULL;
			omd->refresh = (MOD_OCEAN_REFRESH_ADD|MOD_OCEAN_REFRESH_RESET|MOD_OCEAN_REFRESH_SIM);
		}
		else if (md->type==eModifierType_Warp) {
			WarpModifierData *tmd = (WarpModifierData *) md;

			tmd->curfalloff= newdataadr(fd, tmd->curfalloff);
			if (tmd->curfalloff)
				direct_link_curvemapping(fd, tmd->curfalloff);
		}
		else if (md->type==eModifierType_WeightVGEdit) {
			WeightVGEditModifierData *wmd = (WeightVGEditModifierData*) md;

			wmd->cmap_curve = newdataadr(fd, wmd->cmap_curve);
			if (wmd->cmap_curve)
				direct_link_curvemapping(fd, wmd->cmap_curve);
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

	/* loading saved files with editmode enabled works, but for undo we like
	   to stay in object mode during undo presses so keep editmode disabled */
	if (fd->memfile)
		ob->mode &= ~(OB_MODE_EDIT|OB_MODE_PARTICLE_EDIT);
	
	ob->disp.first=ob->disp.last= NULL;
	
	ob->adt= newdataadr(fd, ob->adt);
	direct_link_animdata(fd, ob->adt);
	
	ob->pose= newdataadr(fd, ob->pose);
	direct_link_pose(fd, ob->pose);
	
	ob->mpath= newdataadr(fd, ob->mpath);
	if (ob->mpath)
		direct_link_motionpath(fd, ob->mpath);

	link_list(fd, &ob->defbase);
// XXX depreceated - old animation system <<<
	direct_link_nlastrips(fd, &ob->nlastrips);
	link_list(fd, &ob->constraintChannels);
// >>> XXX depreceated - old animation system 

	ob->mat= newdataadr(fd, ob->mat);
	test_pointer_array(fd, (void **)&ob->mat);
	ob->matbits= newdataadr(fd, ob->matbits);
	
	/* do it here, below old data gets converted */
	direct_link_modifiers(fd, &ob->modifiers);
	
	link_list(fd, &ob->effect);
	paf= ob->effect.first;
	while (paf) {
		if (paf->type==EFF_PARTICLE) {
			paf->keys= NULL;
		}
		if (paf->type==EFF_WAVE) {
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
		if (paf->type==EFF_BUILD) {
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
		paf= paf->next;
	}

	ob->pd= newdataadr(fd, ob->pd);
	direct_link_partdeflect(ob->pd);
	ob->soft= newdataadr(fd, ob->soft);
	if (ob->soft) {
		SoftBody *sb= ob->soft;		
		
		sb->bpoint= NULL;	// init pointers so it gets rebuilt nicely
		sb->bspring= NULL;
		sb->scratch= NULL;
		/* although not used anymore */
		/* still have to be loaded to be compatible with old files */
		sb->keys= newdataadr(fd, sb->keys);
		test_pointer_array(fd, (void **)&sb->keys);
		if (sb->keys) {
			int a;
			for (a=0; a<sb->totkey; a++) {
				sb->keys[a]= newdataadr(fd, sb->keys[a]);
			}
		}

		sb->effector_weights = newdataadr(fd, sb->effector_weights);
		if (!sb->effector_weights)
			sb->effector_weights = BKE_add_effector_weights(NULL);

		direct_link_pointcache_list(fd, &sb->ptcaches, &sb->pointcache, 0);
	}
	ob->bsoft= newdataadr(fd, ob->bsoft);
	ob->fluidsimSettings= newdataadr(fd, ob->fluidsimSettings); /* NT */

	link_list(fd, &ob->particlesystem);
	direct_link_particlesystems(fd,&ob->particlesystem);
	
	link_list(fd, &ob->prop);
	prop= ob->prop.first;
	while (prop) {
		prop->poin= newdataadr(fd, prop->poin);
		if (prop->poin==NULL) prop->poin= &prop->data;
		prop= prop->next;
	}

	link_list(fd, &ob->sensors);
	sens= ob->sensors.first;
	while (sens) {
		sens->data= newdataadr(fd, sens->data);
		sens->links= newdataadr(fd, sens->links);
		test_pointer_array(fd, (void **)&sens->links);
		sens= sens->next;
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
	cont= ob->controllers.first;
	while (cont) {
		cont->data= newdataadr(fd, cont->data);
		cont->links= newdataadr(fd, cont->links);
		test_pointer_array(fd, (void **)&cont->links);
		if (cont->state_mask == 0)
			cont->state_mask = 1;
		cont= cont->next;
	}

	link_glob_list(fd, &ob->actuators);
	act= ob->actuators.first;
	while (act) {
		act->data= newdataadr(fd, act->data);
		act= act->next;
	}

	link_list(fd, &ob->hooks);
	while (ob->hooks.first) {
		ObHook *hook = ob->hooks.first;
		HookModifierData *hmd = (HookModifierData*) modifier_new(eModifierType_Hook);

		hook->indexar= newdataadr(fd, hook->indexar);
		if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
			int a;
			for (a=0; a<hook->totindex; a++) {
				SWITCH_INT(hook->indexar[a]);
			}
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
	
	ob->customdata_mask= 0;
	ob->bb= NULL;
	ob->derivedDeform= NULL;
	ob->derivedFinal= NULL;
	ob->gpulamp.first= ob->gpulamp.last= NULL;
	link_list(fd, &ob->pc_ids);

	/* in case this value changes in future, clamp else we get undefined behavior */
	CLAMP(ob->rotmode, ROT_MODE_MIN, ROT_MODE_MAX);

	if (ob->sculpt) {
		ob->sculpt= MEM_callocN(sizeof(SculptSession), "reload sculpt session");
	}
}

/* ************ READ SCENE ***************** */

/* patch for missing scene IDs, can't be in do-versions */
static void composite_patch(bNodeTree *ntree, Scene *scene)
{
	bNode *node;
	
	for (node= ntree->nodes.first; node; node= node->next)
		if (node->id==NULL && ELEM4(node->type, CMP_NODE_R_LAYERS, CMP_NODE_COMPOSITE, CMP_NODE_DEFOCUS, CMP_NODE_OUTPUT_FILE))
			node->id= &scene->id;
}

static void link_paint(FileData *fd, Scene *sce, Paint *p)
{
	if (p) {
		p->brush= newlibadr_us(fd, sce->id.lib, p->brush);
		p->paint_cursor= NULL;
	}
}

static void lib_link_scene(FileData *fd, Main *main)
{
	Scene *sce;
	Base *base, *next;
	Sequence *seq;
	SceneRenderLayer *srl;
	TimeMarker *marker;
	
	sce= main->scene.first;
	while (sce) {
		if (sce->id.flag & LIB_NEEDLINK) {
			/*Link ID Properties -- and copy this comment EXACTLY for easy finding
			of library blocks that implement this.*/
			if (sce->id.properties) IDP_LibLinkProperty(sce->id.properties, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
			if (sce->adt) lib_link_animdata(fd, &sce->id, sce->adt);
			
			lib_link_keyingsets(fd, &sce->id, &sce->keyingsets);
			
			sce->camera= newlibadr(fd, sce->id.lib, sce->camera);
			sce->world= newlibadr_us(fd, sce->id.lib, sce->world);
			sce->set= newlibadr(fd, sce->id.lib, sce->set);
			sce->gpd= newlibadr_us(fd, sce->id.lib, sce->gpd);
			
			link_paint(fd, sce, &sce->toolsettings->sculpt->paint);
			link_paint(fd, sce, &sce->toolsettings->vpaint->paint);
			link_paint(fd, sce, &sce->toolsettings->wpaint->paint);
			link_paint(fd, sce, &sce->toolsettings->imapaint.paint);
			link_paint(fd, sce, &sce->toolsettings->uvsculpt->paint);
			sce->toolsettings->skgen_template = newlibadr(fd, sce->id.lib, sce->toolsettings->skgen_template);

			for (base= sce->base.first; base; base= next) {
				next= base->next;

				/* base->object= newlibadr_us(fd, sce->id.lib, base->object); */
				base->object= newlibadr_us(fd, sce->id.lib, base->object);
				
				if (base->object==NULL) {
					BKE_reportf_wrap(fd->reports, RPT_ERROR,
					                 "LIB ERROR: Object lost from scene:'%s\'\n",
					                 sce->id.name+2);
					BLI_remlink(&sce->base, base);
					if (base==sce->basact) sce->basact= NULL;
					MEM_freeN(base);
				}
			}

			SEQ_BEGIN(sce->ed, seq) {
				if (seq->ipo) seq->ipo= newlibadr_us(fd, sce->id.lib, seq->ipo);
				seq->scene_sound = NULL;
				if (seq->scene) {
					seq->scene= newlibadr(fd, sce->id.lib, seq->scene);
					if (seq->scene) {
						seq->scene_sound = sound_scene_add_scene_sound_defaults(sce, seq);
					}
				}
				if (seq->clip) {
					seq->clip = newlibadr(fd, sce->id.lib, seq->clip);
					seq->clip->id.us++;
				}
				if (seq->scene_camera) seq->scene_camera= newlibadr(fd, sce->id.lib, seq->scene_camera);
				if (seq->sound) {
					seq->scene_sound = NULL;
					if (seq->type == SEQ_HD_SOUND)
						seq->type = SEQ_SOUND;
					else
						seq->sound= newlibadr(fd, sce->id.lib, seq->sound);
					if (seq->sound) {
						seq->sound->id.us++;
						seq->scene_sound = sound_add_scene_sound_defaults(sce, seq);
					}
				}
				seq->anim= NULL;
			}
			SEQ_END

#ifdef DURIAN_CAMERA_SWITCH
			for (marker= sce->markers.first; marker; marker= marker->next) {
				if (marker->camera) {
					marker->camera= newlibadr(fd, sce->id.lib, marker->camera);
				}
			}
#else
			(void)marker;
#endif

			seq_update_muting(sce->ed);
			seq_update_sound_bounds_all(sce);
			
			if (sce->nodetree) {
				lib_link_ntree(fd, &sce->id, sce->nodetree);
				composite_patch(sce->nodetree, sce);
			}
			
			for (srl= sce->r.layers.first; srl; srl= srl->next) {
				srl->mat_override= newlibadr_us(fd, sce->id.lib, srl->mat_override);
				srl->light_override= newlibadr_us(fd, sce->id.lib, srl->light_override);
			}
			/*Game Settings: Dome Warp Text*/
			sce->gm.dome.warptext= newlibadr(fd, sce->id.lib, sce->gm.dome.warptext);

			/* Motion Tracking */
			sce->clip= newlibadr_us(fd, sce->id.lib, sce->clip);

			sce->id.flag -= LIB_NEEDLINK;
		}

		sce= sce->id.next;
	}
}

static void link_recurs_seq(FileData *fd, ListBase *lb)
{
	Sequence *seq;

	link_list(fd, lb);

	for (seq=lb->first; seq; seq=seq->next)
		if (seq->seqbase.first)
			link_recurs_seq(fd, &seq->seqbase);
}

static void direct_link_paint(FileData *fd, Paint **paint)
{
/* TODO. is this needed */
	(*paint)= newdataadr(fd, (*paint));
}

static void direct_link_scene(FileData *fd, Scene *sce)
{
	Editing *ed;
	Sequence *seq;
	MetaStack *ms;

	sce->theDag = NULL;
	sce->dagisvalid = 0;
	sce->obedit= NULL;
	sce->stats= NULL;
	sce->fps_info= NULL;
	sce->customdata_mask_modal= 0;
	sce->lay_updated = 0;

	sound_create_scene(sce);

	/* set users to one by default, not in lib-link, this will increase it for compo nodes */
	sce->id.us= 1;

	link_list(fd, &(sce->base));
	
	sce->adt= newdataadr(fd, sce->adt);
	direct_link_animdata(fd, sce->adt);
	
	link_list(fd, &sce->keyingsets);
	direct_link_keyingsets(fd, &sce->keyingsets);
	
	sce->basact= newdataadr(fd, sce->basact);
	
	sce->toolsettings= newdataadr(fd, sce->toolsettings);
	if (sce->toolsettings) {
		direct_link_paint(fd, (Paint**)&sce->toolsettings->sculpt);
		direct_link_paint(fd, (Paint**)&sce->toolsettings->vpaint);
		direct_link_paint(fd, (Paint**)&sce->toolsettings->wpaint);
		direct_link_paint(fd, (Paint**)&sce->toolsettings->uvsculpt);

		sce->toolsettings->imapaint.paintcursor= NULL;
		sce->toolsettings->particle.paintcursor= NULL;
	}

	if (sce->ed) {
		ListBase *old_seqbasep= &((Editing *)sce->ed)->seqbase;
		
		ed= sce->ed= newdataadr(fd, sce->ed);

		ed->act_seq= newdataadr(fd, ed->act_seq);

		/* recursive link sequences, lb will be correctly initialized */
		link_recurs_seq(fd, &ed->seqbase);

		SEQ_BEGIN(ed, seq) {
			seq->seq1= newdataadr(fd, seq->seq1);
			seq->seq2= newdataadr(fd, seq->seq2);
			seq->seq3= newdataadr(fd, seq->seq3);
			/* a patch: after introduction of effects with 3 input strips */
			if (seq->seq3==NULL) seq->seq3= seq->seq2;

			seq->plugin= newdataadr(fd, seq->plugin);
			seq->effectdata= newdataadr(fd, seq->effectdata);
			
			if (seq->type & SEQ_EFFECT)
				seq->flag |= SEQ_EFFECT_NOT_LOADED;

			if (seq->type == SEQ_SPEED) {
				SpeedControlVars *s= seq->effectdata;
				s->frameMap= NULL;
			}

			seq->strip= newdataadr(fd, seq->strip);
			if (seq->strip && seq->strip->done==0) {
				seq->strip->done= 1;

				if (seq->type == SEQ_IMAGE ||
				   seq->type == SEQ_MOVIE ||
				   seq->type == SEQ_RAM_SOUND ||
				   seq->type == SEQ_HD_SOUND) {
					seq->strip->stripdata = newdataadr(
						fd, seq->strip->stripdata);
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
					seq->strip->proxy = newdataadr(
						fd, seq->strip->proxy);
					seq->strip->proxy->anim = NULL;
				}
				else {
					seq->strip->proxy = NULL;
				}
				if (seq->flag & SEQ_USE_COLOR_BALANCE) {
					seq->strip->color_balance = newdataadr(
						fd, seq->strip->color_balance);
				}
				else {
					seq->strip->color_balance = NULL;
				}
				if (seq->strip->color_balance) {
					// seq->strip->color_balance->gui = 0; // XXX - peter, is this relevant in 2.5?
				}
			}
		}
		SEQ_END
		
		/* link metastack, slight abuse of structs here, have to restore pointer to internal part in struct */
		{
			Sequence temp;
			char *poin;
			intptr_t offset;
			
			offset= ((intptr_t)&(temp.seqbase)) - ((intptr_t)&temp);
			
			/* root pointer */
			if (ed->seqbasep == old_seqbasep) {
				ed->seqbasep= &ed->seqbase;
			}
			else {
				
				poin= (char *)ed->seqbasep;
				poin -= offset;
				
				poin= newdataadr(fd, poin);
				if (poin) ed->seqbasep= (ListBase *)(poin+offset);
				else ed->seqbasep= &ed->seqbase;
			}			
			/* stack */
			link_list(fd, &(ed->metastack));
			
			for (ms= ed->metastack.first; ms; ms= ms->next) {
				ms->parseq= newdataadr(fd, ms->parseq);
				
				if (ms->oldbasep == old_seqbasep)
					ms->oldbasep= &ed->seqbase;
				else {
					poin= (char *)ms->oldbasep;
					poin -= offset;
					poin= newdataadr(fd, poin);
					if (poin) ms->oldbasep= (ListBase *)(poin+offset);
					else ms->oldbasep= &ed->seqbase;
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
		sce->r.ffcodecdata.properties = newdataadr(
			fd, sce->r.ffcodecdata.properties);
		if (sce->r.ffcodecdata.properties) { 
			IDP_DirectLinkProperty(
				sce->r.ffcodecdata.properties, 
				(fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
		}
	}

	link_list(fd, &(sce->markers));
	link_list(fd, &(sce->transform_spaces));
	link_list(fd, &(sce->r.layers));

	sce->nodetree= newdataadr(fd, sce->nodetree);
	if (sce->nodetree)
		direct_link_nodetree(fd, sce->nodetree);
}

/* ************ READ WM ***************** */

static void direct_link_windowmanager(FileData *fd, wmWindowManager *wm)
{
	wmWindow *win;
	
	wm->id.us= 1;
	link_list(fd, &(wm->windows));
	
	for (win= wm->windows.first; win; win= win->next) {
		win->ghostwin= NULL;
		win->eventstate= NULL;
		win->curswin= NULL;
		win->tweak= NULL;

		win->queue.first= win->queue.last= NULL;
		win->handlers.first= win->handlers.last= NULL;
		win->modalhandlers.first= win->modalhandlers.last= NULL;
		win->subwindows.first= win->subwindows.last= NULL;
		win->gesture.first= win->gesture.last= NULL;

		win->drawdata= NULL;
		win->drawmethod= -1;
		win->drawfail= 0;
	}
	
	wm->timers.first= wm->timers.last= NULL;
	wm->operators.first= wm->operators.last= NULL;
	wm->paintcursors.first= wm->paintcursors.last= NULL;
	wm->queue.first= wm->queue.last= NULL;
	BKE_reports_init(&wm->reports, RPT_STORE);

	wm->keyconfigs.first= wm->keyconfigs.last= NULL;
	wm->defaultconf= NULL;
	wm->addonconf= NULL;
	wm->userconf= NULL;

	wm->jobs.first= wm->jobs.last= NULL;
	wm->drags.first= wm->drags.last= NULL;
	
	wm->windrawable= NULL;
	wm->winactive= NULL;
	wm->initialized= 0;
	wm->op_undo_depth= 0;
}

static void lib_link_windowmanager(FileData *fd, Main *main)
{
	wmWindowManager *wm;
	wmWindow *win;
	
	for (wm= main->wm.first; wm; wm= wm->id.next) {
		if (wm->id.flag & LIB_NEEDLINK) {
			for (win= wm->windows.first; win; win= win->next)
				win->screen= newlibadr(fd, NULL, win->screen);

			wm->id.flag -= LIB_NEEDLINK;
		}
	}
}

/* ****************** READ GREASE PENCIL ***************** */

/* relinks grease-pencil data - used for direct_link and old file linkage */
static void direct_link_gpencil(FileData *fd, bGPdata *gpd)
{
	bGPDlayer *gpl;
	bGPDframe *gpf;
	bGPDstroke *gps;
	
	/* we must firstly have some grease-pencil data to link! */
	if (gpd == NULL)
		return;
	
	/* relink layers */
	link_list(fd, &gpd->layers);
	
	for (gpl= gpd->layers.first; gpl; gpl= gpl->next) {
		/* relink frames */
		link_list(fd, &gpl->frames);
		gpl->actframe= newdataadr(fd, gpl->actframe);
		
		for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
			/* relink strokes (and their points) */
			link_list(fd, &gpf->strokes);
			
			for (gps= gpf->strokes.first; gps; gps= gps->next) {
				gps->points= newdataadr(fd, gps->points);
			}
		}
	}
}

/* ****************** READ SCREEN ***************** */

static void butspace_version_132(SpaceButs *buts)
{
	buts->v2d.tot.xmin = 0.0f;
	buts->v2d.tot.ymin = 0.0f;
	buts->v2d.tot.xmax = 1279.0f;
	buts->v2d.tot.ymax = 228.0f;

	buts->v2d.min[0]= 256.0f;
	buts->v2d.min[1]= 42.0f;

	buts->v2d.max[0]= 2048.0f;
	buts->v2d.max[1]= 450.0f;

	buts->v2d.minzoom= 0.5f;
	buts->v2d.maxzoom= 1.21f;

	buts->v2d.scroll= 0;
	buts->v2d.keepzoom= 1;
	buts->v2d.keeptot= 1;
}

/* note: file read without screens option G_FILE_NO_UI; 
   check lib pointers in call below */
static void lib_link_screen(FileData *fd, Main *main)
{
	bScreen *sc;
	ScrArea *sa;

	for (sc= main->screen.first; sc; sc= sc->id.next) {
		if (sc->id.flag & LIB_NEEDLINK) {
			sc->id.us= 1;
			sc->scene= newlibadr(fd, sc->id.lib, sc->scene);
			sc->animtimer= NULL; /* saved in rare cases */
			
			sa= sc->areabase.first;
			while (sa) {
				SpaceLink *sl;
				
				sa->full= newlibadr(fd, sc->id.lib, sa->full);
				
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D*) sl;
						BGpic *bgpic = NULL;
						
						v3d->camera= newlibadr(fd, sc->id.lib, v3d->camera);
						v3d->ob_centre= newlibadr(fd, sc->id.lib, v3d->ob_centre);
						
						/* should be do_versions but not easy adding into the listbase */
						if (v3d->bgpic) {
							v3d->bgpic= newlibadr(fd, sc->id.lib, v3d->bgpic);
							BLI_addtail(&v3d->bgpicbase, bgpic);
							v3d->bgpic= NULL;
						}

						for (bgpic= v3d->bgpicbase.first; bgpic; bgpic= bgpic->next) {
							bgpic->ima= newlibadr_us(fd, sc->id.lib, bgpic->ima);
							bgpic->clip= newlibadr_us(fd, sc->id.lib, bgpic->clip);
						}
						if (v3d->localvd) {
							v3d->localvd->camera= newlibadr(fd, sc->id.lib, v3d->localvd->camera);
						}
					}
					else if (sl->spacetype==SPACE_IPO) {
						SpaceIpo *sipo= (SpaceIpo *)sl;
						bDopeSheet *ads= sipo->ads;
						
						if (ads) {
							ads->source= newlibadr(fd, sc->id.lib, ads->source);
							ads->filter_grp= newlibadr(fd, sc->id.lib, ads->filter_grp);
						}
					}
					else if (sl->spacetype==SPACE_BUTS) {
						SpaceButs *sbuts= (SpaceButs *)sl;
						sbuts->pinid= newlibadr(fd, sc->id.lib, sbuts->pinid);
						sbuts->mainbo= sbuts->mainb;
						sbuts->mainbuser= sbuts->mainb;
						if (main->versionfile<132)
							butspace_version_132(sbuts);
					}
					else if (sl->spacetype==SPACE_FILE) {
						SpaceFile *sfile= (SpaceFile *)sl;
						sfile->files= NULL;
						sfile->op= NULL;
						sfile->layout= NULL;
						sfile->folders_prev= NULL;
						sfile->folders_next= NULL;
					}
					else if (sl->spacetype==SPACE_ACTION) {
						SpaceAction *saction= (SpaceAction *)sl;
						bDopeSheet *ads= &saction->ads;
						
						if (ads) {
							ads->source= newlibadr(fd, sc->id.lib, ads->source);
							ads->filter_grp= newlibadr(fd, sc->id.lib, ads->filter_grp);
						}
						
						saction->action = newlibadr(fd, sc->id.lib, saction->action);
					}
					else if (sl->spacetype==SPACE_IMAGE) {
						SpaceImage *sima= (SpaceImage *)sl;

						sima->image= newlibadr_us(fd, sc->id.lib, sima->image);
						
						/* NOTE: pre-2.5, this was local data not lib data, but now we need this as lib data
						 * so fingers crossed this works fine!
						 */
						sima->gpd= newlibadr_us(fd, sc->id.lib, sima->gpd);
					}
					else if (sl->spacetype==SPACE_NLA) {
						SpaceNla *snla= (SpaceNla *)sl;
						bDopeSheet *ads= snla->ads;
						
						if (ads) {
							ads->source= newlibadr(fd, sc->id.lib, ads->source);
							ads->filter_grp= newlibadr(fd, sc->id.lib, ads->filter_grp);
						}
					}
					else if (sl->spacetype==SPACE_TEXT) {
						SpaceText *st= (SpaceText *)sl;

						st->text= newlibadr(fd, sc->id.lib, st->text);
						st->drawcache= NULL;

					}
					else if (sl->spacetype==SPACE_SCRIPT) {

						SpaceScript *scpt= (SpaceScript *)sl;
						/*scpt->script = NULL; - 2.45 set to null, better re-run the script */
						if (scpt->script) {
							scpt->script= newlibadr(fd, sc->id.lib, scpt->script);
							if (scpt->script) {
								SCRIPT_SET_NULL(scpt->script);
							}
						}
					}
					else if (sl->spacetype==SPACE_OUTLINER) {
						SpaceOops *so= (SpaceOops *)sl;
						TreeStoreElem *tselem;
						int a;

						so->tree.first= so->tree.last= NULL;
						so->search_tse.id= newlibadr(fd, NULL, so->search_tse.id);
						
						if (so->treestore) {
							tselem= so->treestore->data;
							for (a=0; a<so->treestore->usedelem; a++, tselem++) {
								tselem->id= newlibadr(fd, NULL, tselem->id);
							}
						}
					}
					else if (sl->spacetype==SPACE_NODE) {
						SpaceNode *snode= (SpaceNode *)sl;
						
						snode->id= newlibadr(fd, sc->id.lib, snode->id);
						snode->edittree= NULL;
						
						if (ELEM3(snode->treetype, NTREE_COMPOSIT, NTREE_SHADER, NTREE_TEXTURE)) {
							/* internal data, a bit patchy */
							snode->nodetree= NULL;
							if (snode->id) {
								if (GS(snode->id->name)==ID_MA)
									snode->nodetree= ((Material *)snode->id)->nodetree;
								else if (GS(snode->id->name)==ID_WO)
									snode->nodetree= ((World *)snode->id)->nodetree;
								else if (GS(snode->id->name)==ID_LA)
									snode->nodetree= ((Lamp *)snode->id)->nodetree;
								else if (GS(snode->id->name)==ID_SCE)
									snode->nodetree= ((Scene *)snode->id)->nodetree;
								else if (GS(snode->id->name)==ID_TE)
									snode->nodetree= ((Tex *)snode->id)->nodetree;
							}
						}
						else {
							snode->nodetree= newlibadr_us(fd, sc->id.lib, snode->nodetree);
						}
						
						snode->linkdrag.first = snode->linkdrag.last = NULL;
					}
					else if (sl->spacetype==SPACE_CLIP) {
						SpaceClip *sclip= (SpaceClip *)sl;

						sclip->clip= newlibadr_us(fd, sc->id.lib, sclip->clip);

						sclip->scopes.track_preview = NULL;
						sclip->scopes.ok = 0;
					}
				}
				sa= sa->next;
			}
			sc->id.flag -= LIB_NEEDLINK;
		}
	}
}

/* Only for undo files, or to restore a screen after reading without UI... */
static void *restore_pointer_by_name(Main *mainp, ID *id, int user)
{
		
	if (id) {
		ListBase *lb= which_libbase(mainp, GS(id->name));
		
		if (lb) {	// there's still risk of checking corrupt mem (freed Ids in oops)
			ID *idn= lb->first;
			char *name= id->name+2;
			
			while (idn) {
				if (idn->name[2]==name[0] && strcmp(idn->name+2, name)==0) {
					if (idn->lib==id->lib) {
						if (user && idn->us==0) idn->us++;
						break;
					}
				}
				idn= idn->next;
			}
			return idn;
		}
	}
	return NULL;
}

static int lib_link_seq_clipboard_cb(Sequence *seq, void *arg_pt)
{
	Main *newmain = (Main *)arg_pt;

	if (seq->sound) {
		seq->sound = restore_pointer_by_name(newmain, (ID *)seq->sound, 0);
		seq->sound->id.us++;
	}

	if (seq->scene)
		seq->scene = restore_pointer_by_name(newmain, (ID *)seq->scene, 1);

	if (seq->scene_camera)
		seq->scene_camera = restore_pointer_by_name(newmain, (ID *)seq->scene_camera, 1);

	return 1;
}

static void lib_link_clipboard_restore(Main *newmain)
{
	/* update IDs stored in sequencer clipboard */
	seqbase_recursive_apply(&seqbase_clipboard, lib_link_seq_clipboard_cb, newmain);
}

/* called from kernel/blender.c */
/* used to link a file (without UI) to the current UI */
/* note that it assumes the old pointers in UI are still valid, so old Main is not freed */
void lib_link_screen_restore(Main *newmain, bScreen *curscreen, Scene *curscene)
{
	wmWindow *win;
	wmWindowManager *wm;
	bScreen *sc;
	ScrArea *sa;

	/* first windowmanager */
	for (wm= newmain->wm.first; wm; wm= wm->id.next) {
		for (win= wm->windows.first; win; win= win->next) {
			win->screen= restore_pointer_by_name(newmain, (ID *)win->screen, 1);
			
			if (win->screen==NULL)
				win->screen= curscreen;

			win->screen->winid= win->winid;
		}
	}
	
	
	for (sc= newmain->screen.first; sc; sc= sc->id.next) {
		Scene *oldscene= sc->scene;

		sc->scene= restore_pointer_by_name(newmain, (ID *)sc->scene, 1);
		if (sc->scene==NULL)
			sc->scene= curscene;

		/* keep cursor location through undo */
		copy_v3_v3(sc->scene->cursor, oldscene->cursor);

		sa= sc->areabase.first;
		while (sa) {
			SpaceLink *sl;

			for (sl= sa->spacedata.first; sl; sl= sl->next) {
				if (sl->spacetype==SPACE_VIEW3D) {
					View3D *v3d= (View3D*) sl;
					BGpic *bgpic;
					ARegion *ar;
					
					if (v3d->scenelock)
						v3d->camera= NULL; /* always get from scene */
					else
						v3d->camera= restore_pointer_by_name(newmain, (ID *)v3d->camera, 1);
					if (v3d->camera==NULL)
						v3d->camera= sc->scene->camera;
					v3d->ob_centre= restore_pointer_by_name(newmain, (ID *)v3d->ob_centre, 1);
					
					for (bgpic= v3d->bgpicbase.first; bgpic; bgpic= bgpic->next) {
						bgpic->ima= restore_pointer_by_name(newmain, (ID *)bgpic->ima, 1);
						bgpic->clip= restore_pointer_by_name(newmain, (ID *)bgpic->clip, 1);
					}
					if (v3d->localvd) {
						/*Base *base;*/

						v3d->localvd->camera= sc->scene->camera;
						
						/* localview can become invalid during undo/redo steps, so we exit it when no could be found */
						/* XXX  regionlocalview ?
						for (base= sc->scene->base.first; base; base= base->next) {
							if (base->lay & v3d->lay) break;
						}
						if (base==NULL) {
							v3d->lay= v3d->localvd->lay;
							v3d->layact= v3d->localvd->layact;
							MEM_freeN(v3d->localvd); 
							v3d->localvd= NULL;
						}
						*/
					}
					else if (v3d->scenelock) v3d->lay= sc->scene->lay;

					/* not very nice, but could help */
					if ((v3d->layact & v3d->lay)==0) v3d->layact= v3d->lay;
					
					/* free render engines for now */
					for (ar= sa->regionbase.first; ar; ar= ar->next) {
						RegionView3D *rv3d= ar->regiondata;

						if (rv3d && rv3d->render_engine) {
							RE_engine_free(rv3d->render_engine);
							rv3d->render_engine= NULL;
						}
					}
				}
				else if (sl->spacetype==SPACE_IPO) {
					SpaceIpo *sipo= (SpaceIpo *)sl;
					bDopeSheet *ads= sipo->ads;
					
					if (ads) {
						ads->source= restore_pointer_by_name(newmain, (ID *)ads->source, 1);
						
						if (ads->filter_grp)
							ads->filter_grp= restore_pointer_by_name(newmain, (ID *)ads->filter_grp, 0);
					}
				}
				else if (sl->spacetype==SPACE_BUTS) {
					SpaceButs *sbuts= (SpaceButs *)sl;
					sbuts->pinid = restore_pointer_by_name(newmain, sbuts->pinid, 0);
					//XXX if (sbuts->ri) sbuts->ri->curtile = 0;
				}
				else if (sl->spacetype==SPACE_FILE) {
					
					SpaceFile *sfile= (SpaceFile *)sl;
					sfile->op= NULL;
				}
				else if (sl->spacetype==SPACE_ACTION) {
					SpaceAction *saction= (SpaceAction *)sl;
					
					saction->action = restore_pointer_by_name(newmain, (ID *)saction->action, 1);
					saction->ads.source= restore_pointer_by_name(newmain, (ID *)saction->ads.source, 1);

					if (saction->ads.filter_grp)
						saction->ads.filter_grp= restore_pointer_by_name(newmain, (ID *)saction->ads.filter_grp, 0);
				}
				else if (sl->spacetype==SPACE_IMAGE) {
					SpaceImage *sima= (SpaceImage *)sl;

					sima->image= restore_pointer_by_name(newmain, (ID *)sima->image, 1);

					/* this will be freed, not worth attempting to find same scene,
					 * since it gets initialized later */
					sima->iuser.scene = NULL;

					sima->scopes.waveform_1 = NULL;
					sima->scopes.waveform_2 = NULL;
					sima->scopes.waveform_3 = NULL;
					sima->scopes.vecscope = NULL;
					sima->scopes.ok = 0;
					
					/* NOTE: pre-2.5, this was local data not lib data, but now we need this as lib data
					 * so assume that here we're doing for undo only...
					 */
					sima->gpd= restore_pointer_by_name(newmain, (ID *)sima->gpd, 1);
				}
				else if (sl->spacetype==SPACE_NLA) {
					SpaceNla *snla= (SpaceNla *)sl;
					bDopeSheet *ads= snla->ads;
					
					if (ads) {
						ads->source= restore_pointer_by_name(newmain, (ID *)ads->source, 1);
						
						if (ads->filter_grp)
							ads->filter_grp= restore_pointer_by_name(newmain, (ID *)ads->filter_grp, 0);
					}
				}
				else if (sl->spacetype==SPACE_TEXT) {
					SpaceText *st= (SpaceText *)sl;

					st->text= restore_pointer_by_name(newmain, (ID *)st->text, 1);
					if (st->text==NULL) st->text= newmain->text.first;
				}
				else if (sl->spacetype==SPACE_SCRIPT) {
					SpaceScript *scpt= (SpaceScript *)sl;
					
					scpt->script= restore_pointer_by_name(newmain, (ID *)scpt->script, 1);
					
					/*sc->script = NULL; - 2.45 set to null, better re-run the script */
					if (scpt->script) {
						SCRIPT_SET_NULL(scpt->script);
					}
				}
				else if (sl->spacetype==SPACE_OUTLINER) {
					SpaceOops *so= (SpaceOops *)sl;
					int a;
					
					so->search_tse.id= restore_pointer_by_name(newmain, so->search_tse.id, 0);
					
					if (so->treestore) {
						TreeStore *ts= so->treestore;
						TreeStoreElem *tselem=ts->data;
						for (a=0; a<ts->usedelem; a++, tselem++) {
							tselem->id= restore_pointer_by_name(newmain, tselem->id, 0);
						}
					}
				}
				else if (sl->spacetype==SPACE_NODE) {
					SpaceNode *snode= (SpaceNode *)sl;
					
					snode->id= restore_pointer_by_name(newmain, snode->id, 1);
					snode->edittree= NULL;
					
					if (ELEM3(snode->treetype, NTREE_COMPOSIT, NTREE_SHADER, NTREE_TEXTURE)) {
						snode->nodetree= NULL;
						if (snode->id) {
							if (GS(snode->id->name)==ID_MA)
								snode->nodetree= ((Material *)snode->id)->nodetree;
							else if (GS(snode->id->name)==ID_SCE)
								snode->nodetree= ((Scene *)snode->id)->nodetree;
							else if (GS(snode->id->name)==ID_TE)
								snode->nodetree= ((Tex *)snode->id)->nodetree;
						}
					}
					else {
						snode->nodetree= restore_pointer_by_name(newmain, &snode->nodetree->id, 1);
					}
				}
				else if (sl->spacetype==SPACE_CLIP) {
					SpaceClip *sclip= (SpaceClip *)sl;

					sclip->clip= restore_pointer_by_name(newmain, (ID *)sclip->clip, 1);

					sclip->scopes.ok = 0;
				}
			}
			sa= sa->next;
		}
	}

	/* update IDs stored in all possible clipboards */
	lib_link_clipboard_restore(newmain);
}

static void direct_link_region(FileData *fd, ARegion *ar, int spacetype)
{
	Panel *pa;

	link_list(fd, &(ar->panels));

	for (pa= ar->panels.first; pa; pa=pa->next) {
		pa->paneltab= newdataadr(fd, pa->paneltab);
		pa->runtime_flag= 0;
		pa->activedata= NULL;
		pa->type= NULL;
	}
	
	ar->regiondata= newdataadr(fd, ar->regiondata);
	if (ar->regiondata) {
		if (spacetype==SPACE_VIEW3D) {
			RegionView3D *rv3d= ar->regiondata;
			
			rv3d->localvd= newdataadr(fd, rv3d->localvd);
			rv3d->clipbb= newdataadr(fd, rv3d->clipbb);
			
			rv3d->depths= NULL;
			rv3d->ri= NULL;
			rv3d->render_engine= NULL;
			rv3d->sms= NULL;
			rv3d->smooth_timer= NULL;
		}
	}
	
	ar->v2d.tab_offset= NULL;
	ar->v2d.tab_num= 0;
	ar->v2d.tab_cur= 0;
	ar->handlers.first= ar->handlers.last= NULL;
	ar->uiblocks.first= ar->uiblocks.last= NULL;
	ar->headerstr= NULL;
	ar->swinid= 0;
	ar->type= NULL;
	ar->swap= 0;
	ar->do_draw= 0;
	memset(&ar->drawrct, 0, sizeof(ar->drawrct));
}

/* for the saved 2.50 files without regiondata */
/* and as patch for 2.48 and older */
static void view3d_split_250(View3D *v3d, ListBase *regions)
{
	ARegion *ar;
	
	for (ar= regions->first; ar; ar= ar->next) {
		if (ar->regiontype==RGN_TYPE_WINDOW && ar->regiondata==NULL) {
			RegionView3D *rv3d;
			
			rv3d= ar->regiondata= MEM_callocN(sizeof(RegionView3D), "region v3d patch");
			rv3d->persp= (char)v3d->persp;
			rv3d->view= (char)v3d->view;
			rv3d->dist= v3d->dist;
			copy_v3_v3(rv3d->ofs, v3d->ofs);
			copy_qt_qt(rv3d->viewquat, v3d->viewquat);
		}
	}

	/* this was not initialized correct always */
	if (v3d->twtype == 0)
		v3d->twtype= V3D_MANIP_TRANSLATE;
}

static void direct_link_screen(FileData *fd, bScreen *sc)
{
	ScrArea *sa;
	ScrVert *sv;
	ScrEdge *se;
	int a;
	
	link_list(fd, &(sc->vertbase));
	link_list(fd, &(sc->edgebase));
	link_list(fd, &(sc->areabase));
	sc->regionbase.first= sc->regionbase.last= NULL;
	sc->context= NULL;

	sc->mainwin= sc->subwinactive= 0;	/* indices */
	sc->swap= 0;
	
	/* hacky patch... but people have been saving files with the verse-blender,
	   causing the handler to keep running for ever, with no means to disable it */
	for (a=0; a<SCREEN_MAXHANDLER; a+=2) {
		if ( sc->handler[a]==SCREEN_HANDLER_VERSE) {
			sc->handler[a]= 0;
			break;
		}
	}
	
	/* edges */
	for (se= sc->edgebase.first; se; se= se->next) {
		se->v1= newdataadr(fd, se->v1);
		se->v2= newdataadr(fd, se->v2);
		if ( (intptr_t)se->v1 > (intptr_t)se->v2) {
			sv= se->v1;
			se->v1= se->v2;
			se->v2= sv;
		}

		if (se->v1==NULL) {
			printf("error reading screen... file corrupt\n");
			se->v1= se->v2;
		}
	}

	/* areas */
	for (sa= sc->areabase.first; sa; sa= sa->next) {
		SpaceLink *sl;
		ARegion *ar;

		link_list(fd, &(sa->spacedata));
		link_list(fd, &(sa->regionbase));

		sa->handlers.first= sa->handlers.last= NULL;
		sa->type= NULL;	/* spacetype callbacks */
		
		for (ar= sa->regionbase.first; ar; ar= ar->next)
			direct_link_region(fd, ar, sa->spacetype);
		
		/* accident can happen when read/save new file with older version */
		/* 2.50: we now always add spacedata for info */
		if (sa->spacedata.first==NULL) {
			SpaceInfo *sinfo= MEM_callocN(sizeof(SpaceInfo), "spaceinfo");
			sa->spacetype= sinfo->spacetype= SPACE_INFO;
			BLI_addtail(&sa->spacedata, sinfo);
		}
		/* add local view3d too */
		else if (sa->spacetype==SPACE_VIEW3D)
			view3d_split_250(sa->spacedata.first, &sa->regionbase);
		
		for (sl= sa->spacedata.first; sl; sl= sl->next) {
			link_list(fd, &(sl->regionbase));

			for (ar= sl->regionbase.first; ar; ar= ar->next)
				direct_link_region(fd, ar, sl->spacetype);

			if (sl->spacetype==SPACE_VIEW3D) {
				View3D *v3d= (View3D*) sl;
				BGpic *bgpic;

				v3d->flag |= V3D_INVALID_BACKBUF;

				link_list(fd, &(v3d->bgpicbase));

				/* should be do_versions except this doesnt fit well there */
				if (v3d->bgpic) {
					bgpic= newdataadr(fd, v3d->bgpic);
					BLI_addtail(&v3d->bgpicbase, bgpic);
					v3d->bgpic= NULL;
				}

				for (bgpic= v3d->bgpicbase.first; bgpic; bgpic= bgpic->next)
					bgpic->iuser.ok= 1;

				if (v3d->gpd) {
					v3d->gpd= newdataadr(fd, v3d->gpd);
					direct_link_gpencil(fd, v3d->gpd);
				}
				v3d->localvd= newdataadr(fd, v3d->localvd);
				v3d->afterdraw_transp.first= v3d->afterdraw_transp.last= NULL;
				v3d->afterdraw_xray.first= v3d->afterdraw_xray.last= NULL;
				v3d->afterdraw_xraytransp.first= v3d->afterdraw_xraytransp.last= NULL;
				v3d->properties_storage= NULL;

				/* render can be quite heavy, set to wire on load */
				if (v3d->drawtype == OB_RENDER)
					v3d->drawtype = OB_WIRE;
				
				view3d_split_250(v3d, &sl->regionbase);
			}
			else if (sl->spacetype==SPACE_IPO) {
				SpaceIpo *sipo= (SpaceIpo*)sl;
				
				sipo->ads= newdataadr(fd, sipo->ads);
				sipo->ghostCurves.first= sipo->ghostCurves.last= NULL;
			}
			else if (sl->spacetype==SPACE_NLA) {
				SpaceNla *snla= (SpaceNla*)sl;
				
				snla->ads= newdataadr(fd, snla->ads);
			}
			else if (sl->spacetype==SPACE_OUTLINER) {
				SpaceOops *soops= (SpaceOops*) sl;
				
				soops->treestore= newdataadr(fd, soops->treestore);
				if (soops->treestore) {
					soops->treestore->data= newdataadr(fd, soops->treestore->data);
					/* we only saved what was used */
					soops->treestore->totelem= soops->treestore->usedelem;
					soops->storeflag |= SO_TREESTORE_CLEANUP;	// at first draw
				}
			}
			else if (sl->spacetype==SPACE_IMAGE) {
				SpaceImage *sima= (SpaceImage *)sl;
				
				sima->cumap= newdataadr(fd, sima->cumap);
				if (sima->cumap)
					direct_link_curvemapping(fd, sima->cumap);
				
				sima->iuser.scene= NULL;
				sima->iuser.ok= 1;
				sima->scopes.waveform_1 = NULL;
				sima->scopes.waveform_2 = NULL;
				sima->scopes.waveform_3 = NULL;
				sima->scopes.vecscope = NULL;
				sima->scopes.ok = 0;
				
				/* WARNING: gpencil data is no longer stored directly in sima after 2.5 
				 * so sacrifice a few old files for now to avoid crashes with new files!
				 * committed: r28002 */
#if 0
				sima->gpd= newdataadr(fd, sima->gpd);
				if (sima->gpd)
					direct_link_gpencil(fd, sima->gpd);
#endif
			}
			else if (sl->spacetype==SPACE_NODE) {
				SpaceNode *snode= (SpaceNode *)sl;
				
				if (snode->gpd) {
					snode->gpd= newdataadr(fd, snode->gpd);
					direct_link_gpencil(fd, snode->gpd);
				}
			}
			else if (sl->spacetype==SPACE_TIME) {
				SpaceTime *stime= (SpaceTime *)sl;
				stime->caches.first= stime->caches.last= NULL;
			}
			else if (sl->spacetype==SPACE_LOGIC) {
				SpaceLogic *slogic= (SpaceLogic *)sl;
					
				if (slogic->gpd) {
					slogic->gpd= newdataadr(fd, slogic->gpd);
					direct_link_gpencil(fd, slogic->gpd);
				}
			}
			else if (sl->spacetype==SPACE_SEQ) {
				SpaceSeq *sseq= (SpaceSeq *)sl;
				if (sseq->gpd) {
					sseq->gpd= newdataadr(fd, sseq->gpd);
					direct_link_gpencil(fd, sseq->gpd);
				}
			}
			else if (sl->spacetype==SPACE_BUTS) {
				SpaceButs *sbuts= (SpaceButs *)sl;
				sbuts->path= NULL;
				sbuts->texuser= NULL;
			}
			else if (sl->spacetype==SPACE_CONSOLE) {
				SpaceConsole *sconsole= (SpaceConsole *)sl;
				ConsoleLine *cl, *cl_next;
				
				link_list(fd, &sconsole->scrollback);
				link_list(fd, &sconsole->history);
				
				//for (cl= sconsole->scrollback.first; cl; cl= cl->next)
				//	cl->line= newdataadr(fd, cl->line);
				
				/*comma expressions, (e.g. expr1, expr2, expr3) evalutate each expression,
				  from left to right.  the right-most expression sets the result of the comma
				  expression as a whole*/
				for (cl= sconsole->history.first; cl; cl= cl_next) {
					cl_next= cl->next;
					cl->line= newdataadr(fd, cl->line);
					if (cl->line) {
						/* the allocted length is not written, so reset here */
						cl->len_alloc= cl->len + 1;
					}
					else {
						BLI_remlink(&sconsole->history, cl);
						MEM_freeN(cl);
					}
				}
			}
			else if (sl->spacetype==SPACE_FILE) {
				SpaceFile *sfile= (SpaceFile *)sl;
				
				/* this sort of info is probably irrelevant for reloading...
				 * plus, it isn't saved to files yet!
				 */
				sfile->folders_prev= sfile->folders_next= NULL;
				sfile->files= NULL;
				sfile->layout= NULL;
				sfile->op= NULL;
				sfile->params= newdataadr(fd, sfile->params);
			}
		}
		
		sa->actionzones.first= sa->actionzones.last= NULL;

		sa->v1= newdataadr(fd, sa->v1);
		sa->v2= newdataadr(fd, sa->v2);
		sa->v3= newdataadr(fd, sa->v3);
		sa->v4= newdataadr(fd, sa->v4);
	}
}

/* ********** READ LIBRARY *************** */


static void direct_link_library(FileData *fd, Library *lib, Main *main)
{
	Main *newmain;
	
	for (newmain= fd->mainlist.first; newmain; newmain= newmain->next) {
		if (newmain->curlib) {
			if (BLI_path_cmp(newmain->curlib->filepath, lib->filepath) == 0) {
				BKE_reportf_wrap(fd->reports, RPT_WARNING,
				                 "Library '%s', '%s' had multiple instances, save and reload!",
				                 lib->name, lib->filepath);

				change_idid_adr(&fd->mainlist, fd, lib, newmain->curlib);
//				change_idid_adr_fd(fd, lib, newmain->curlib);
				
				BLI_remlink(&main->library, lib);
				MEM_freeN(lib);


				return;
			}
		}
	}
	/* make sure we have full path in lib->filename */
	BLI_strncpy(lib->filepath, lib->name, sizeof(lib->name));
	cleanup_path(fd->relabase, lib->filepath);
	
//	printf("direct_link_library: name %s\n", lib->name);
//	printf("direct_link_library: filename %s\n", lib->filename);
	
	/* new main */
	newmain= MEM_callocN(sizeof(Main), "directlink");
	BLI_addtail(&fd->mainlist, newmain);
	newmain->curlib= lib;

	lib->parent= NULL;
}

static void lib_link_library(FileData *UNUSED(fd), Main *main)
{
	Library *lib;
	for (lib= main->library.first; lib; lib= lib->id.next) {
		lib->id.us= 1;
	}
}

/* Always call this once you have loaded new library data to set the relative paths correctly in relation to the blend file */
static void fix_relpaths_library(const char *basepath, Main *main)
{
	Library *lib;
	/* BLO_read_from_memory uses a blank filename */
	if (basepath==NULL || basepath[0] == '\0') {
		for (lib= main->library.first; lib; lib= lib->id.next) {
			/* when loading a linked lib into a file which has not been saved,
			 * there is nothing we can be relative to, so instead we need to make
			 * it absolute. This can happen when appending an object with a relative
			 * link into an unsaved blend file. See [#27405].
			 * The remap relative option will make it relative again on save - campbell */
			if (strncmp(lib->name, "//", 2)==0) {
				BLI_strncpy(lib->name, lib->filepath, sizeof(lib->name));
			}
		}
	}
	else {
		for (lib= main->library.first; lib; lib= lib->id.next) {
			/* Libraries store both relative and abs paths, recreate relative paths,
			 * relative to the blend file since indirectly linked libs will be relative to their direct linked library */
			if (strncmp(lib->name, "//", 2)==0) { /* if this is relative to begin with? */
				BLI_strncpy(lib->name, lib->filepath, sizeof(lib->name));
				BLI_path_rel(lib->name, basepath);
			}
		}
	}
}

/* ************ READ SPEAKER ***************** */

static void lib_link_speaker(FileData *fd, Main *main)
{
	Speaker *spk;

	spk= main->speaker.first;
	while (spk) {
		if (spk->id.flag & LIB_NEEDLINK) {
			if (spk->adt) lib_link_animdata(fd, &spk->id, spk->adt);

			spk->sound= newlibadr(fd, spk->id.lib, spk->sound);
			if (spk->sound) {
				spk->sound->id.us++;
			}

			spk->id.flag -= LIB_NEEDLINK;
		}
		spk= spk->id.next;
	}
}

static void direct_link_speaker(FileData *fd, Speaker *spk)
{
	spk->adt= newdataadr(fd, spk->adt);
	direct_link_animdata(fd, spk->adt);

	/*spk->sound= newdataadr(fd, spk->sound);
	direct_link_sound(fd, spk->sound);*/
}

/* ************** READ SOUND ******************* */

static void direct_link_sound(FileData *fd, bSound *sound)
{
	sound->handle = NULL;
	sound->playback_handle = NULL;
	sound->waveform = NULL;

	// versioning stuff, if there was a cache, then we enable caching:
	if (sound->cache)
	{
		sound->flags |= SOUND_FLAGS_CACHING;
		sound->cache = NULL;
	}

	sound->packedfile = direct_link_packedfile(fd, sound->packedfile);
	sound->newpackedfile = direct_link_packedfile(fd, sound->newpackedfile);
}

static void lib_link_sound(FileData *fd, Main *main)
{
	bSound *sound;

	sound= main->sound.first;
	while (sound) {
		if (sound->id.flag & LIB_NEEDLINK) {
			sound->id.flag -= LIB_NEEDLINK;
			sound->ipo= newlibadr_us(fd, sound->id.lib, sound->ipo); // XXX depreceated - old animation system
			
			sound_load(main, sound);
		}
		sound= sound->id.next;
	}
}
/* ***************** READ GROUP *************** */

static void direct_link_group(FileData *fd, Group *group)
{
	link_list(fd, &group->gobject);
}

static void lib_link_group(FileData *fd, Main *main)
{
	Group *group= main->group.first;
	GroupObject *go;
	int add_us;
	
	while (group) {
		if (group->id.flag & LIB_NEEDLINK) {
			group->id.flag -= LIB_NEEDLINK;
			
			add_us= 0;
			
			go= group->gobject.first;
			while (go) {
				go->ob= newlibadr(fd, group->id.lib, go->ob);
				if (go->ob) {
					go->ob->flag |= OB_FROMGROUP;
					/* if group has an object, it increments user... */
					add_us= 1;
					if (go->ob->id.us==0)
						go->ob->id.us= 1;
				}
				go= go->next;
			}
			if (add_us) group->id.us++;
			rem_from_group(group, NULL, NULL, NULL);	/* removes NULL entries */
		}
		group= group->id.next;
	}
}

/* ***************** READ MOVIECLIP *************** */

static void direct_link_movieReconstruction(FileData *fd, MovieTrackingReconstruction *reconstruction)
{
	reconstruction->cameras= newdataadr(fd, reconstruction->cameras);
}

static void direct_link_movieTracks(FileData *fd, ListBase *tracksbase)
{
	MovieTrackingTrack *track;

	link_list(fd, tracksbase);

	track= tracksbase->first;
	while (track) {
		track->markers= newdataadr(fd, track->markers);

		track= track->next;
	}
}

static void direct_link_movieclip(FileData *fd, MovieClip *clip)
{
	MovieTracking *tracking= &clip->tracking;
	MovieTrackingObject *object;

	clip->adt= newdataadr(fd, clip->adt);

	if (fd->movieclipmap) clip->cache= newmclipadr(fd, clip->cache);
	else clip->cache= NULL;

	if (fd->movieclipmap) clip->tracking.camera.intrinsics= newmclipadr(fd, clip->tracking.camera.intrinsics);
	else clip->tracking.camera.intrinsics= NULL;

	direct_link_movieTracks(fd, &tracking->tracks);
	direct_link_movieReconstruction(fd, &tracking->reconstruction);

	clip->tracking.act_track= newdataadr(fd, clip->tracking.act_track);

	clip->anim= NULL;
	clip->tracking_context= NULL;
	clip->tracking.stats= NULL;

	clip->tracking.stabilization.ok= 0;
	clip->tracking.stabilization.scaleibuf= NULL;
	clip->tracking.stabilization.rot_track= newdataadr(fd, clip->tracking.stabilization.rot_track);

	link_list(fd, &tracking->objects);

	object= tracking->objects.first;
	while (object) {
		direct_link_movieTracks(fd, &object->tracks);
		direct_link_movieReconstruction(fd, &object->reconstruction);

		object= object->next;
	}
}

static void lib_link_movieclip(FileData *fd, Main *main)
{
	MovieClip *clip;

	clip= main->movieclip.first;
	while (clip) {
		if (clip->id.flag & LIB_NEEDLINK) {
			if (clip->adt)
				lib_link_animdata(fd, &clip->id, clip->adt);

			clip->gpd= newlibadr_us(fd, clip->id.lib, clip->gpd);

			clip->id.flag -= LIB_NEEDLINK;
		}
		clip= clip->id.next;
	}
}

/* ************** GENERAL & MAIN ******************** */


static const char *dataname(short id_code)
{
	
	switch( id_code ) {
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
		case ID_GD: return "Data from GD";
		case ID_MC: return "Data from MC";
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
		short *sp= fd->filesdna->structs[bhead->SDNAnr];
		char *tmp= malloc(100);
		allocname = fd->filesdna->types[ sp[0] ];
		strcpy(tmp, allocname);
		data= read_struct(fd, bhead, tmp);
#else
		data= read_struct(fd, bhead, allocname);
#endif
		
		if (data) {
			oldnewmap_insert(fd->datamap, bhead->old, data, 0);
		}

		bhead = blo_nextbhead(fd, bhead);
	}

	return bhead;
}

static BHead *read_libblock(FileData *fd, Main *main, BHead *bhead, int flag, ID **id_r)
{
	/* this routine reads a libblock and its direct data. Use link functions
	 * to connect it all
	 */

	ID *id;
	ListBase *lb;
	const char *allocname;
	
	/* read libblock */
	id = read_struct(fd, bhead, "lib block");
	if (id_r)
		*id_r= id;
	if (!id)
		return blo_nextbhead(fd, bhead);
	
	oldnewmap_insert(fd->libmap, bhead->old, id, bhead->code);	/* for ID_ID check */
	
	/* do after read_struct, for dna reconstruct */
	if (bhead->code==ID_ID) {
		lb= which_libbase(main, GS(id->name));
	}
	else {
		lb= which_libbase(main, bhead->code);
	}
	
	BLI_addtail(lb, id);

	/* clear first 8 bits */
	id->flag= (id->flag & 0xFF00) | flag | LIB_NEEDLINK;
	id->lib= main->curlib;
	if (id->flag & LIB_FAKEUSER) id->us= 1;
	else id->us= 0;
	id->icon_id = 0;
	id->flag &= ~(LIB_ID_RECALC|LIB_ID_RECALC_DATA);

	/* this case cannot be direct_linked: it's just the ID part */
	if (bhead->code==ID_ID) {
		return blo_nextbhead(fd, bhead);
	}

	/* need a name for the mallocN, just for debugging and sane prints on leaks */
	allocname= dataname(GS(id->name));
	
	/* read all data into fd->datamap */
	bhead= read_data_into_oldnewmap(fd, bhead, allocname);

	/* init pointers direct data */
	switch( GS(id->name) ) {
		case ID_WM:
			direct_link_windowmanager(fd, (wmWindowManager *)id);
			break;
		case ID_SCR:
			direct_link_screen(fd, (bScreen *)id);
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
		case ID_SCRIPT:
			direct_link_script(fd, (Script*)id);
			break;
		case ID_GD:
			direct_link_gpencil(fd, (bGPdata *)id);
			break;
		case ID_MC:
			direct_link_movieclip(fd, (MovieClip *)id);
			break;
	}
	
	/*link direct data of ID properties*/
	if (id->properties) {
		id->properties = newdataadr(fd, id->properties);
		if (id->properties) { /* this case means the data was written incorrectly, it should not happen */
			IDP_DirectLinkProperty(id->properties, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
		}
	}

	oldnewmap_free_unused(fd->datamap);
	oldnewmap_clear(fd->datamap);

	return (bhead);
}

/* note, this has to be kept for reading older files... */
/* also version info is written here */
static BHead *read_global(BlendFileData *bfd, FileData *fd, BHead *bhead)
{
	FileGlobal *fg= read_struct(fd, bhead, "Global");
	
	/* copy to bfd handle */
	bfd->main->subversionfile= fg->subversion;
	bfd->main->minversionfile= fg->minversion;
	bfd->main->minsubversionfile= fg->minsubversion;
	bfd->main->revision= fg->revision;
	
	bfd->winpos= fg->winpos;
	bfd->fileflags= fg->fileflags;
	bfd->displaymode= fg->displaymode;
	bfd->globalf= fg->globalf;
	BLI_strncpy(bfd->filename, fg->filename, sizeof(bfd->filename));
	
	if (G.fileflags & G_FILE_RECOVER)
		BLI_strncpy(fd->relabase, fg->filename, sizeof(fd->relabase));
	
	bfd->curscreen= fg->curscreen;
	bfd->curscene= fg->curscene;
	
	MEM_freeN(fg);

	fd->globalf= bfd->globalf;
	fd->fileflags= bfd->fileflags;
	
	return blo_nextbhead(fd, bhead);
}

/* note, this has to be kept for reading older files... */
static void link_global(FileData *fd, BlendFileData *bfd)
{
	
	bfd->curscreen= newlibadr(fd, NULL, bfd->curscreen);
	bfd->curscene= newlibadr(fd, NULL, bfd->curscene);
	// this happens in files older than 2.35
	if (bfd->curscene==NULL) {
		if (bfd->curscreen) bfd->curscene= bfd->curscreen->scene;
	}
}

static void vcol_to_fcol(Mesh *me)
{
	MFace *mface;
	unsigned int *mcol, *mcoln, *mcolmain;
	int a;

	if (me->totface==0 || me->mcol==NULL) return;

	mcoln= mcolmain= MEM_mallocN(4*sizeof(int)*me->totface, "mcoln");
	mcol = (unsigned int *)me->mcol;
	mface= me->mface;
	for (a=me->totface; a>0; a--, mface++) {
		mcoln[0]= mcol[mface->v1];
		mcoln[1]= mcol[mface->v2];
		mcoln[2]= mcol[mface->v3];
		mcoln[3]= mcol[mface->v4];
		mcoln+= 4;
	}

	MEM_freeN(me->mcol);
	me->mcol= (MCol *)mcolmain;
}

static int map_223_keybd_code_to_224_keybd_code(int code)
{
	switch (code) {
		case 312:	return 311; /* F12KEY */
		case 159:	return 161; /* PADSLASHKEY */
		case 161:	return 150; /* PAD0 */
		case 154:	return 151; /* PAD1 */
		case 150:	return 152; /* PAD2 */
		case 155:	return 153; /* PAD3 */
		case 151:	return 154; /* PAD4 */
		case 156:	return 155; /* PAD5 */
		case 152:	return 156; /* PAD6 */
		case 157:	return 157; /* PAD7 */
		case 153:	return 158; /* PAD8 */
		case 158:	return 159; /* PAD9 */
		default: return code;
	}
}

static void do_version_bone_head_tail_237(Bone *bone)
{
	Bone *child;
	float vec[3];

	/* head */
	copy_v3_v3(bone->arm_head, bone->arm_mat[3]);

	/* tail is in current local coord system */
	copy_v3_v3(vec, bone->arm_mat[1]);
	mul_v3_fl(vec, bone->length);
	add_v3_v3v3(bone->arm_tail, bone->arm_head, vec);

	for (child= bone->childbase.first; child; child= child->next)
		do_version_bone_head_tail_237(child);
}

static void bone_version_238(ListBase *lb)
{
	Bone *bone;
	
	for (bone= lb->first; bone; bone= bone->next) {
		if (bone->rad_tail==0.0f && bone->rad_head==0.0f) {
			bone->rad_head= 0.25f*bone->length;
			bone->rad_tail= 0.1f*bone->length;
			
			bone->dist-= bone->rad_head;
			if (bone->dist<=0.0f) bone->dist= 0.0f;
		}
		bone_version_238(&bone->childbase);
	}
}

static void bone_version_239(ListBase *lb)
{
	Bone *bone;
	
	for (bone= lb->first; bone; bone= bone->next) {
		if (bone->layer==0)
			bone->layer= 1;
		bone_version_239(&bone->childbase);
	}
}

static void ntree_version_241(bNodeTree *ntree)
{
	bNode *node;
	
	if (ntree->type==NTREE_COMPOSIT) {
		for (node= ntree->nodes.first; node; node= node->next) {
			if (node->type==CMP_NODE_BLUR) {
				if (node->storage==NULL) {
					NodeBlurData *nbd= MEM_callocN(sizeof(NodeBlurData), "node blur patch");
					nbd->sizex= node->custom1;
					nbd->sizey= node->custom2;
					nbd->filtertype= R_FILTER_QUAD;
					node->storage= nbd;
				}
			}
			else if (node->type==CMP_NODE_VECBLUR) {
				if (node->storage==NULL) {
					NodeBlurData *nbd= MEM_callocN(sizeof(NodeBlurData), "node blur patch");
					nbd->samples= node->custom1;
					nbd->maxspeed= node->custom2;
					nbd->fac= 1.0f;
					node->storage= nbd;
				}
			}
		}
	}
}

static void ntree_version_242(bNodeTree *ntree)
{
	bNode *node;
	
	if (ntree->type==NTREE_COMPOSIT) {
		for (node= ntree->nodes.first; node; node= node->next) {
			if (node->type==CMP_NODE_HUE_SAT) {
				if (node->storage) {
					NodeHueSat *nhs= node->storage;
					if (nhs->val==0.0f) nhs->val= 1.0f;
				}
			}
		}
	}
	else if (ntree->type==NTREE_SHADER) {
		for (node= ntree->nodes.first; node; node= node->next)
			if (node->type == SH_NODE_GEOMETRY && node->storage == NULL)
				node->storage= MEM_callocN(sizeof(NodeGeometry), "NodeGeometry");
	}
	
}

static void customdata_version_242(Mesh *me)
{
	CustomDataLayer *layer;
	MTFace *mtf;
	MCol *mcol;
	TFace *tf;
	int a, mtfacen, mcoln;

	if (!me->vdata.totlayer) {
		CustomData_add_layer(&me->vdata, CD_MVERT, CD_ASSIGN, me->mvert, me->totvert);

		if (me->msticky)
			CustomData_add_layer(&me->vdata, CD_MSTICKY, CD_ASSIGN, me->msticky, me->totvert);
		if (me->dvert)
			CustomData_add_layer(&me->vdata, CD_MDEFORMVERT, CD_ASSIGN, me->dvert, me->totvert);
	}

	if (!me->edata.totlayer)
		CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, me->medge, me->totedge);
	
	if (!me->fdata.totlayer) {
		CustomData_add_layer(&me->fdata, CD_MFACE, CD_ASSIGN, me->mface, me->totface);

		if (me->tface) {
			if (me->mcol)
				MEM_freeN(me->mcol);

			me->mcol= CustomData_add_layer(&me->fdata, CD_MCOL, CD_CALLOC, NULL, me->totface);
			me->mtface= CustomData_add_layer(&me->fdata, CD_MTFACE, CD_CALLOC, NULL, me->totface);

			mtf= me->mtface;
			mcol= me->mcol;
			tf= me->tface;

			for (a=0; a < me->totface; a++, mtf++, tf++, mcol+=4) {
				memcpy(mcol, tf->col, sizeof(tf->col));
				memcpy(mtf->uv, tf->uv, sizeof(tf->uv));

				mtf->flag= tf->flag;
				mtf->unwrap= tf->unwrap;
				mtf->mode= tf->mode;
				mtf->tile= tf->tile;
				mtf->tpage= tf->tpage;
				mtf->transp= tf->transp;
			}

			MEM_freeN(me->tface);
			me->tface= NULL;
		}
		else if (me->mcol) {
			me->mcol= CustomData_add_layer(&me->fdata, CD_MCOL, CD_ASSIGN, me->mcol, me->totface);
		}
	}

	if (me->tface) {
		MEM_freeN(me->tface);
		me->tface= NULL;
	}

	for (a=0, mtfacen=0, mcoln=0; a < me->fdata.totlayer; a++) {
		layer= &me->fdata.layers[a];

		if (layer->type == CD_MTFACE) {
			if (layer->name[0] == 0) {
				if (mtfacen == 0) strcpy(layer->name, "UVMap");
				else BLI_snprintf(layer->name, sizeof(layer->name), "UVMap.%.3d", mtfacen);
			}
			mtfacen++;
		}
		else if (layer->type == CD_MCOL) {
			if (layer->name[0] == 0) {
				if (mcoln == 0) strcpy(layer->name, "Col");
				else BLI_snprintf(layer->name, sizeof(layer->name), "Col.%.3d", mcoln);
			}
			mcoln++;
		}
	}

	mesh_update_customdata_pointers(me, TRUE);
}

/*only copy render texface layer from active*/
static void customdata_version_243(Mesh *me)
{
	CustomDataLayer *layer;
	int a;

	for (a=0; a < me->fdata.totlayer; a++) {
		layer= &me->fdata.layers[a];
		layer->active_rnd = layer->active;
	}
}

/* struct NodeImageAnim moved to ImageUser, and we make it default available */
static void do_version_ntree_242_2(bNodeTree *ntree)
{
	bNode *node;
	
	if (ntree->type==NTREE_COMPOSIT) {
		for (node= ntree->nodes.first; node; node= node->next) {
			if (ELEM3(node->type, CMP_NODE_IMAGE, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
				/* only image had storage */
				if (node->storage) {
					NodeImageAnim *nia= node->storage;
					ImageUser *iuser= MEM_callocN(sizeof(ImageUser), "ima user node");

					iuser->frames= nia->frames;
					iuser->sfra= nia->sfra;
					iuser->offset= nia->nr-1;
					iuser->cycl= nia->cyclic;
					iuser->fie_ima= 2;
					iuser->ok= 1;
					
					node->storage= iuser;
					MEM_freeN(nia);
				}
				else {
					ImageUser *iuser= node->storage= MEM_callocN(sizeof(ImageUser), "node image user");
					iuser->sfra= 1;
					iuser->fie_ima= 2;
					iuser->ok= 1;
				}
			}
		}
	}
}

static void ntree_version_245(FileData *fd, Library *lib, bNodeTree *ntree)
{
	bNode *node;
	NodeTwoFloats *ntf;
	ID *nodeid;
	Image *image;
	ImageUser *iuser;

	if (ntree->type==NTREE_COMPOSIT) {
		for (node= ntree->nodes.first; node; node= node->next) {
			if (node->type == CMP_NODE_ALPHAOVER) {
				if (!node->storage) {
					ntf= MEM_callocN(sizeof(NodeTwoFloats), "NodeTwoFloats");
					node->storage= ntf;
					if (node->custom1)
						ntf->x= 1.0f;
				}
			}
			
			/* fix for temporary flag changes during 245 cycle */
			nodeid= newlibadr(fd, lib, node->id);
			if (node->storage && nodeid && GS(nodeid->name) == ID_IM) {
				image= (Image*)nodeid;
				iuser= node->storage;
				if (iuser->flag & IMA_OLD_PREMUL) {
					iuser->flag &= ~IMA_OLD_PREMUL;
					iuser->flag |= IMA_DO_PREMUL;
				}
				if (iuser->flag & IMA_DO_PREMUL) {
					image->flag &= ~IMA_OLD_PREMUL;
					image->flag |= IMA_DO_PREMUL;
				}
			}
		}
	}
}

static void idproperties_fix_groups_lengths_recurse(IDProperty *prop)
{
	IDProperty *loop;
	int i;
	
	for (loop=prop->data.group.first, i=0; loop; loop=loop->next, i++) {
		if (loop->type == IDP_GROUP) idproperties_fix_groups_lengths_recurse(loop);
	}
	
	if (prop->len != i) {
		printf("Found and fixed bad id property group length.\n");
		prop->len = i;
	}
}

static void idproperties_fix_group_lengths(ListBase idlist)
{
	ID *id;
	
	for (id=idlist.first; id; id=id->next) {
		if (id->properties) {
			idproperties_fix_groups_lengths_recurse(id->properties);
		}
	}
}

static void alphasort_version_246(FileData *fd, Library *lib, Mesh *me)
{
	Material *ma;
	MFace *mf;
	MTFace *tf;
	int a, b, texalpha;

	/* verify we have a tface layer */
	for (b=0; b<me->fdata.totlayer; b++)
		if (me->fdata.layers[b].type == CD_MTFACE)
			break;
	
	if (b == me->fdata.totlayer)
		return;

	/* if we do, set alpha sort if the game engine did it before */
	for (a=0, mf=me->mface; a<me->totface; a++, mf++) {
		if (mf->mat_nr < me->totcol) {
			ma= newlibadr(fd, lib, me->mat[mf->mat_nr]);
			texalpha = 0;

			/* we can't read from this if it comes from a library,
			 * because direct_link might not have happened on it,
			 * so ma->mtex is not pointing to valid memory yet */
			if (ma && ma->id.lib)
				ma= NULL;

			for (b=0; ma && b<MAX_MTEX; b++)
				if (ma->mtex && ma->mtex[b] && ma->mtex[b]->mapto & MAP_ALPHA)
					texalpha = 1;
		}
		else {
			ma= NULL;
			texalpha = 0;
		}

		for (b=0; b<me->fdata.totlayer; b++) {
			if (me->fdata.layers[b].type == CD_MTFACE) {
				tf = ((MTFace*)me->fdata.layers[b].data) + a;

				tf->mode &= ~TF_ALPHASORT;
				if (ma && (ma->mode & MA_ZTRANSP))
					if (ELEM(tf->transp, TF_ALPHA, TF_ADD) || (texalpha && (tf->transp != TF_CLIP)))
						tf->mode |= TF_ALPHASORT;
			}
		}
	}
}

/* 2.50 patch */
static void area_add_header_region(ScrArea *sa, ListBase *lb)
{
	ARegion *ar= MEM_callocN(sizeof(ARegion), "area region from do_versions");
	
	BLI_addtail(lb, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	if (sa->headertype==HEADERDOWN)
		ar->alignment= RGN_ALIGN_BOTTOM;
	else
		ar->alignment= RGN_ALIGN_TOP;
	
	/* initialize view2d data for header region, to allow panning */
	/* is copy from ui_view2d.c */
	ar->v2d.keepzoom = (V2D_LOCKZOOM_X|V2D_LOCKZOOM_Y|V2D_LIMITZOOM|V2D_KEEPASPECT);
	ar->v2d.keepofs = V2D_LOCKOFS_Y;
	ar->v2d.keeptot = V2D_KEEPTOT_STRICT; 
	ar->v2d.align = V2D_ALIGN_NO_NEG_X|V2D_ALIGN_NO_NEG_Y;
	ar->v2d.flag = (V2D_PIXELOFS_X|V2D_PIXELOFS_Y);
}

static void sequencer_init_preview_region(ARegion* ar)
{
	// XXX a bit ugly still, copied from space_sequencer
	/* NOTE: if you change values here, also change them in space_sequencer.c, sequencer_new */
	ar->regiontype= RGN_TYPE_PREVIEW;
	ar->alignment= RGN_ALIGN_TOP;
	ar->flag |= RGN_FLAG_HIDDEN;
	ar->v2d.keepzoom= V2D_KEEPASPECT | V2D_KEEPZOOM;
	ar->v2d.minzoom= 0.00001f;
	ar->v2d.maxzoom= 100000.0f;
	ar->v2d.tot.xmin = -960.0f; /* 1920 width centered */
	ar->v2d.tot.ymin = -540.0f; /* 1080 height centered */
	ar->v2d.tot.xmax = 960.0f;
	ar->v2d.tot.ymax = 540.0f;
	ar->v2d.min[0]= 0.0f;
	ar->v2d.min[1]= 0.0f;
	ar->v2d.max[0]= 12000.0f;
	ar->v2d.max[1]= 12000.0f;
	ar->v2d.cur= ar->v2d.tot;
	ar->v2d.align= V2D_ALIGN_FREE; // (V2D_ALIGN_NO_NEG_X|V2D_ALIGN_NO_NEG_Y);
	ar->v2d.keeptot= V2D_KEEPTOT_FREE;
}

/* 2.50 patch */
static void area_add_window_regions(ScrArea *sa, SpaceLink *sl, ListBase *lb)
{
	ARegion *ar;
	ARegion *ar_main;

	if (sl) {
		/* first channels for ipo action nla... */
		switch(sl->spacetype) {
			case SPACE_IPO:
				ar= MEM_callocN(sizeof(ARegion), "area region from do_versions");
				BLI_addtail(lb, ar);
				ar->regiontype= RGN_TYPE_CHANNELS;
				ar->alignment= RGN_ALIGN_LEFT; 
				ar->v2d.scroll= (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM);
				
					// for some reason, this doesn't seem to go auto like for NLA...
				ar= MEM_callocN(sizeof(ARegion), "area region from do_versions");
				BLI_addtail(lb, ar);
				ar->regiontype= RGN_TYPE_UI;
				ar->alignment= RGN_ALIGN_RIGHT;
				ar->v2d.scroll= V2D_SCROLL_RIGHT;
				ar->v2d.flag = RGN_FLAG_HIDDEN;
				break;
				
			case SPACE_ACTION:
				ar= MEM_callocN(sizeof(ARegion), "area region from do_versions");
				BLI_addtail(lb, ar);
				ar->regiontype= RGN_TYPE_CHANNELS;
				ar->alignment= RGN_ALIGN_LEFT;
				ar->v2d.scroll= V2D_SCROLL_BOTTOM;
				ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;
				break;
				
			case SPACE_NLA:
				ar= MEM_callocN(sizeof(ARegion), "area region from do_versions");
				BLI_addtail(lb, ar);
				ar->regiontype= RGN_TYPE_CHANNELS;
				ar->alignment= RGN_ALIGN_LEFT;
				ar->v2d.scroll= V2D_SCROLL_BOTTOM;
				ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;
				
					// for some reason, some files still don't get this auto
				ar= MEM_callocN(sizeof(ARegion), "area region from do_versions");
				BLI_addtail(lb, ar);
				ar->regiontype= RGN_TYPE_UI;
				ar->alignment= RGN_ALIGN_RIGHT;
				ar->v2d.scroll= V2D_SCROLL_RIGHT;
				ar->v2d.flag = RGN_FLAG_HIDDEN;
				break;
				
			case SPACE_NODE:
				ar= MEM_callocN(sizeof(ARegion), "nodetree area for node");
				BLI_addtail(lb, ar);
				ar->regiontype= RGN_TYPE_UI;
				ar->alignment= RGN_ALIGN_LEFT;
				ar->v2d.scroll = (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM);
				ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;
				/* temporarily hide it */
				ar->flag = RGN_FLAG_HIDDEN;
				break;
			case SPACE_FILE:
				ar= MEM_callocN(sizeof(ARegion), "nodetree area for node");
				BLI_addtail(lb, ar);
				ar->regiontype= RGN_TYPE_CHANNELS;
				ar->alignment= RGN_ALIGN_LEFT;

				ar= MEM_callocN(sizeof(ARegion), "ui area for file");
				BLI_addtail(lb, ar);
				ar->regiontype= RGN_TYPE_UI;
				ar->alignment= RGN_ALIGN_TOP;
				break;
			case SPACE_SEQ:
				ar_main = (ARegion*)lb->first;
				for (; ar_main; ar_main = ar_main->next) {
					if (ar_main->regiontype == RGN_TYPE_WINDOW)
						break;
				}
				ar= MEM_callocN(sizeof(ARegion), "preview area for sequencer");
				BLI_insertlinkbefore(lb, ar_main, ar);
				sequencer_init_preview_region(ar);
				break;
			case SPACE_VIEW3D:
				/* toolbar */
				ar= MEM_callocN(sizeof(ARegion), "toolbar for view3d");
				
				BLI_addtail(lb, ar);
				ar->regiontype= RGN_TYPE_TOOLS;
				ar->alignment= RGN_ALIGN_LEFT;
				ar->flag = RGN_FLAG_HIDDEN;
				
				/* tool properties */
				ar= MEM_callocN(sizeof(ARegion), "tool properties for view3d");
				
				BLI_addtail(lb, ar);
				ar->regiontype= RGN_TYPE_TOOL_PROPS;
				ar->alignment= RGN_ALIGN_BOTTOM|RGN_SPLIT_PREV;
				ar->flag = RGN_FLAG_HIDDEN;
				
				/* buttons/list view */
				ar= MEM_callocN(sizeof(ARegion), "buttons for view3d");
				
				BLI_addtail(lb, ar);
				ar->regiontype= RGN_TYPE_UI;
				ar->alignment= RGN_ALIGN_RIGHT;
				ar->flag = RGN_FLAG_HIDDEN;
#if 0
			case SPACE_BUTS:
				/* context UI region */
				ar= MEM_callocN(sizeof(ARegion), "area region from do_versions");
				BLI_addtail(lb, ar);
				ar->regiontype= RGN_TYPE_UI;
				ar->alignment= RGN_ALIGN_RIGHT;
				
				break;
#endif
		}
	}

	/* main region */
	ar= MEM_callocN(sizeof(ARegion), "area region from do_versions");
	
	BLI_addtail(lb, ar);
	ar->winrct= sa->totrct;
	
	ar->regiontype= RGN_TYPE_WINDOW;
	
	if (sl) {
		/* if active spacetype has view2d data, copy that over to main region */
		/* and we split view3d */
		switch(sl->spacetype) {
			case SPACE_VIEW3D:
				view3d_split_250((View3D *)sl, lb);
				break;		
						
			case SPACE_OUTLINER:
			{
				SpaceOops *soops= (SpaceOops *)sl;
				
				memcpy(&ar->v2d, &soops->v2d, sizeof(View2D));
				
				ar->v2d.scroll &= ~V2D_SCROLL_LEFT;
				ar->v2d.scroll |= (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM_O);
				ar->v2d.align = (V2D_ALIGN_NO_NEG_X|V2D_ALIGN_NO_POS_Y);
				ar->v2d.keepzoom |= (V2D_LOCKZOOM_X|V2D_LOCKZOOM_Y|V2D_KEEPASPECT);
				ar->v2d.keeptot = V2D_KEEPTOT_STRICT;
				ar->v2d.minzoom= ar->v2d.maxzoom= 1.0f;
				//ar->v2d.flag |= V2D_IS_INITIALISED;
			}
				break;
			case SPACE_TIME:
			{
				SpaceTime *stime= (SpaceTime *)sl;
				memcpy(&ar->v2d, &stime->v2d, sizeof(View2D));
				
				ar->v2d.scroll |= (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
				ar->v2d.align |= V2D_ALIGN_NO_NEG_Y;
				ar->v2d.keepofs |= V2D_LOCKOFS_Y;
				ar->v2d.keepzoom |= V2D_LOCKZOOM_Y;
				ar->v2d.tot.ymin = ar->v2d.cur.ymin = -10.0;
				ar->v2d.min[1]= ar->v2d.max[1]= 20.0;
			}
				break;
			case SPACE_IPO:
			{
				SpaceIpo *sipo= (SpaceIpo *)sl;
				memcpy(&ar->v2d, &sipo->v2d, sizeof(View2D));
				
				/* init mainarea view2d */
				ar->v2d.scroll |= (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
				ar->v2d.scroll |= (V2D_SCROLL_LEFT|V2D_SCROLL_SCALE_VERTICAL);
				
				ar->v2d.min[0]= FLT_MIN;
				ar->v2d.min[1]= FLT_MIN;
				
				ar->v2d.max[0]= MAXFRAMEF;
				ar->v2d.max[1]= FLT_MAX;
				
				//ar->v2d.flag |= V2D_IS_INITIALISED;
				break;
			}
			case SPACE_NLA:
			{
				SpaceNla *snla= (SpaceNla *)sl;
				memcpy(&ar->v2d, &snla->v2d, sizeof(View2D));
				
				ar->v2d.tot.ymin = (float)(-sa->winy)/3.0f;
				ar->v2d.tot.ymax = 0.0f;
				
				ar->v2d.scroll |= (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
				ar->v2d.scroll |= (V2D_SCROLL_RIGHT);
				ar->v2d.align = V2D_ALIGN_NO_POS_Y;
				ar->v2d.flag |= V2D_VIEWSYNC_AREA_VERTICAL;
				break;
			}
			case SPACE_ACTION:
			{
				SpaceAction *saction= (SpaceAction *)sl;
				
				/* we totally reinit the view for the Action Editor, as some old instances had some weird cruft set */
				ar->v2d.tot.xmin = -20.0f;
				ar->v2d.tot.ymin = (float)(-sa->winy)/3.0f;
				ar->v2d.tot.xmax = (float)((sa->winx > 120)? (sa->winx) : 120);
				ar->v2d.tot.ymax = 0.0f;
				
				ar->v2d.cur= ar->v2d.tot;
				
				ar->v2d.min[0]= 0.0f;
				ar->v2d.min[1]= 0.0f;
				
				ar->v2d.max[0]= MAXFRAMEF;
				ar->v2d.max[1]= FLT_MAX;

				ar->v2d.minzoom= 0.01f;
				ar->v2d.maxzoom= 50;
				ar->v2d.scroll = (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
				ar->v2d.scroll |= (V2D_SCROLL_RIGHT);
				ar->v2d.keepzoom= V2D_LOCKZOOM_Y;
				ar->v2d.align= V2D_ALIGN_NO_POS_Y;
				ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;
				
				/* for old files with ShapeKey editors open + an action set, clear the action as 
				 * it doesn't make sense in the new system (i.e. violates concept that ShapeKey edit
				 * only shows ShapeKey-rooted actions only)
				 */
				if (saction->mode == SACTCONT_SHAPEKEY)
					saction->action = NULL;
				break;
			}
			case SPACE_SEQ:
			{
				SpaceSeq *sseq= (SpaceSeq *)sl;
				memcpy(&ar->v2d, &sseq->v2d, sizeof(View2D));
				
				ar->v2d.scroll |= (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
				ar->v2d.scroll |= (V2D_SCROLL_LEFT|V2D_SCROLL_SCALE_VERTICAL);
				ar->v2d.align= V2D_ALIGN_NO_NEG_Y;
				ar->v2d.flag |= V2D_IS_INITIALISED;
				break;
			}
			case SPACE_NODE:
			{
				SpaceNode *snode= (SpaceNode *)sl;
				memcpy(&ar->v2d, &snode->v2d, sizeof(View2D));
				
				ar->v2d.scroll= (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM);
				ar->v2d.keepzoom= V2D_LIMITZOOM|V2D_KEEPASPECT;
				break;
			}
			case SPACE_BUTS:
			{
				SpaceButs *sbuts= (SpaceButs *)sl;
				memcpy(&ar->v2d, &sbuts->v2d, sizeof(View2D));
				
				ar->v2d.scroll |= (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM); 
				break;
			}
			case SPACE_FILE:
			 {
				// SpaceFile *sfile= (SpaceFile *)sl;
				ar->v2d.tot.xmin = ar->v2d.tot.ymin = 0;
				ar->v2d.tot.xmax = ar->winx;
				ar->v2d.tot.ymax = ar->winy;
				ar->v2d.cur = ar->v2d.tot;
				ar->regiontype= RGN_TYPE_WINDOW;
				ar->v2d.scroll = (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM_O);
				ar->v2d.align = (V2D_ALIGN_NO_NEG_X|V2D_ALIGN_NO_POS_Y);
				ar->v2d.keepzoom = (V2D_LOCKZOOM_X|V2D_LOCKZOOM_Y|V2D_LIMITZOOM|V2D_KEEPASPECT);
				break;
			}
			case SPACE_TEXT:
			{
				SpaceText *st= (SpaceText *)sl;
				st->flags |= ST_FIND_WRAP;
			}
				//case SPACE_XXX: // FIXME... add other ones
				//	memcpy(&ar->v2d, &((SpaceXxx *)sl)->v2d, sizeof(View2D));
				//	break;
		}
	}
}

static void do_versions_windowmanager_2_50(bScreen *screen)
{
	ScrArea *sa;
	SpaceLink *sl;
	
	/* add regions */
	for (sa= screen->areabase.first; sa; sa= sa->next) {
		
		/* we keep headertype variable to convert old files only */
		if (sa->headertype)
			area_add_header_region(sa, &sa->regionbase);
		
		area_add_window_regions(sa, sa->spacedata.first, &sa->regionbase);
		
		/* space imageselect is deprecated */
		for (sl= sa->spacedata.first; sl; sl= sl->next) {
			if (sl->spacetype==SPACE_IMASEL)
				sl->spacetype= SPACE_EMPTY;	/* spacedata then matches */
		}
		
		/* space sound is deprecated */
		for (sl= sa->spacedata.first; sl; sl= sl->next) {
			if (sl->spacetype==SPACE_SOUND)
				sl->spacetype= SPACE_EMPTY;	/* spacedata then matches */
		}
		
		/* it seems to be possible in 2.5 to have this saved, filewindow probably */
		sa->butspacetype= sa->spacetype;
		
		/* pushed back spaces also need regions! */
		if (sa->spacedata.first) {
			sl= sa->spacedata.first;
			for (sl= sl->next; sl; sl= sl->next) {
				if (sa->headertype)
					area_add_header_region(sa, &sl->regionbase);
				area_add_window_regions(sa, sl, &sl->regionbase);
			}
		}
	}
}

static void versions_gpencil_add_main(ListBase *lb, ID *id, const char *name)
{
	
	BLI_addtail(lb, id);
	id->us= 1;
	id->flag= LIB_FAKEUSER;
	*( (short *)id->name )= ID_GD;
	
	new_id(lb, id, name);
	/* alphabetic insterion: is in new_id */
	
	if (G.debug & G_DEBUG)
		printf("Converted GPencil to ID: %s\n", id->name+2);
}

static void do_versions_gpencil_2_50(Main *main, bScreen *screen)
{
	ScrArea *sa;
	SpaceLink *sl;
	
	/* add regions */
	for (sa= screen->areabase.first; sa; sa= sa->next) {
		for (sl= sa->spacedata.first; sl; sl= sl->next) {
			if (sl->spacetype==SPACE_VIEW3D) {
				View3D *v3d= (View3D*) sl;
				if (v3d->gpd) {
					versions_gpencil_add_main(&main->gpencil, (ID *)v3d->gpd, "GPencil View3D");
					v3d->gpd= NULL;
				}
			}
			else if (sl->spacetype==SPACE_NODE) {
				SpaceNode *snode= (SpaceNode *)sl;
				if (snode->gpd) {
					versions_gpencil_add_main(&main->gpencil, (ID *)snode->gpd, "GPencil Node");
					snode->gpd= NULL;
				}
			}
			else if (sl->spacetype==SPACE_SEQ) {
				SpaceSeq *sseq= (SpaceSeq *)sl;
				if (sseq->gpd) {
					versions_gpencil_add_main(&main->gpencil, (ID *)sseq->gpd, "GPencil Node");
					sseq->gpd= NULL;
				}
			}
			else if (sl->spacetype==SPACE_IMAGE) {
				SpaceImage *sima= (SpaceImage *)sl;
#if 0			/* see comment on r28002 */
				if (sima->gpd) {
					versions_gpencil_add_main(&main->gpencil, (ID *)sima->gpd, "GPencil Image");
					sima->gpd= NULL;
				}
#else
				sima->gpd= NULL;
#endif
			}
		}
	}		
}

/* deprecated, only keep this for readfile.c */
static PartEff *do_version_give_parteff_245(Object *ob)
{
	PartEff *paf;

	paf= ob->effect.first;
	while (paf) {
		if (paf->type==EFF_PARTICLE) return paf;
		paf= paf->next;
	}
	return NULL;
}
static void do_version_free_effect_245(Effect *eff)
{
	PartEff *paf;

	if (eff->type==EFF_PARTICLE) {
		paf= (PartEff *)eff;
		if (paf->keys) MEM_freeN(paf->keys);
	}
	MEM_freeN(eff);
}
static void do_version_free_effects_245(ListBase *lb)
{
	Effect *eff;

	eff= lb->first;
	while (eff) {
		BLI_remlink(lb, eff);
		do_version_free_effect_245(eff);
		eff= lb->first;
	}
}

static void do_version_mtex_factor_2_50(MTex **mtex_array, short idtype)
{
	MTex *mtex;
	float varfac, colfac;
	int a, neg;

	if (!mtex_array)
		return;

	for (a=0; a<MAX_MTEX; a++) {
		if (mtex_array[a]) {
			mtex= mtex_array[a];

			neg= mtex->maptoneg;
			varfac= mtex->varfac;
			colfac= mtex->colfac;

			if (neg & MAP_DISP) mtex->dispfac= -mtex->dispfac;
			if (neg & MAP_NORM) mtex->norfac= -mtex->norfac;
			if (neg & MAP_WARP) mtex->warpfac= -mtex->warpfac;

			mtex->colspecfac= (neg & MAP_COLSPEC)? -colfac: colfac;
			mtex->mirrfac= (neg & MAP_COLMIR)? -colfac: colfac;
			mtex->alphafac= (neg & MAP_ALPHA)? -varfac: varfac;
			mtex->difffac= (neg & MAP_REF)? -varfac: varfac;
			mtex->specfac= (neg & MAP_SPEC)? -varfac: varfac;
			mtex->emitfac= (neg & MAP_EMIT)? -varfac: varfac;
			mtex->hardfac= (neg & MAP_HAR)? -varfac: varfac;
			mtex->raymirrfac= (neg & MAP_RAYMIRR)? -varfac: varfac;
			mtex->translfac= (neg & MAP_TRANSLU)? -varfac: varfac;
			mtex->ambfac= (neg & MAP_AMB)? -varfac: varfac;
			mtex->colemitfac= (neg & MAP_EMISSION_COL)? -colfac: colfac;
			mtex->colreflfac= (neg & MAP_REFLECTION_COL)? -colfac: colfac;
			mtex->coltransfac= (neg & MAP_TRANSMISSION_COL)? -colfac: colfac;
			mtex->densfac= (neg & MAP_DENSITY)? -varfac: varfac;
			mtex->scatterfac= (neg & MAP_SCATTERING)? -varfac: varfac;
			mtex->reflfac= (neg & MAP_REFLECTION)? -varfac: varfac;

			mtex->timefac= (neg & MAP_PA_TIME)? -varfac: varfac;
			mtex->lengthfac= (neg & MAP_PA_LENGTH)? -varfac: varfac;
			mtex->clumpfac= (neg & MAP_PA_CLUMP)? -varfac: varfac;
			mtex->kinkfac= (neg & MAP_PA_KINK)? -varfac: varfac;
			mtex->roughfac= (neg & MAP_PA_ROUGH)? -varfac: varfac;
			mtex->padensfac= (neg & MAP_PA_DENS)? -varfac: varfac;
			mtex->lifefac= (neg & MAP_PA_LIFE)? -varfac: varfac;
			mtex->sizefac= (neg & MAP_PA_SIZE)? -varfac: varfac;
			mtex->ivelfac= (neg & MAP_PA_IVEL)? -varfac: varfac;

			mtex->shadowfac= (neg & LAMAP_SHAD)? -colfac: colfac;

			mtex->zenupfac= (neg & WOMAP_ZENUP)? -colfac: colfac;
			mtex->zendownfac= (neg & WOMAP_ZENDOWN)? -colfac: colfac;
			mtex->blendfac= (neg & WOMAP_BLEND)? -varfac: varfac;

			if (idtype == ID_MA)
				mtex->colfac= (neg & MAP_COL)? -colfac: colfac;
			else if (idtype == ID_LA)
				mtex->colfac= (neg & LAMAP_COL)? -colfac: colfac;
			else if (idtype == ID_WO)
				mtex->colfac= (neg & WOMAP_HORIZ)? -colfac: colfac;
		}
	}
}

static void do_version_mdef_250(Main *main)
{
	Object *ob;
	ModifierData *md;
	MeshDeformModifierData *mmd;

	for (ob= main->object.first; ob; ob=ob->id.next) {
		for (md=ob->modifiers.first; md; md=md->next) {
			if (md->type == eModifierType_MeshDeform) {
				mmd= (MeshDeformModifierData*)md;

				if (mmd->bindcos) {
					/* make bindcos NULL in order to trick older versions
					   into thinking that the mesh was not bound yet */
					mmd->bindcagecos= mmd->bindcos;
					mmd->bindcos= NULL;

					modifier_mdef_compact_influences(md);
				}
			}
		}
	}
}

static void do_version_constraints_radians_degrees_250(ListBase *lb)
{
	bConstraint *con;

	for	(con=lb->first; con; con=con->next) {
		if (con->type==CONSTRAINT_TYPE_RIGIDBODYJOINT) {
			bRigidBodyJointConstraint *data = con->data;
			data->axX *= (float)(M_PI/180.0);
			data->axY *= (float)(M_PI/180.0);
			data->axZ *= (float)(M_PI/180.0);
		}
		else if (con->type==CONSTRAINT_TYPE_KINEMATIC) {
			bKinematicConstraint *data = con->data;
			data->poleangle *= (float)(M_PI/180.0);
		}
		else if (con->type==CONSTRAINT_TYPE_ROTLIMIT) {
			bRotLimitConstraint *data = con->data;

			data->xmin *= (float)(M_PI/180.0);
			data->xmax *= (float)(M_PI/180.0);
			data->ymin *= (float)(M_PI/180.0);
			data->ymax *= (float)(M_PI/180.0);
			data->zmin *= (float)(M_PI/180.0);
			data->zmax *= (float)(M_PI/180.0);
		}
	}
}

/* NOTE: this version patch is intended for versions < 2.52.2, but was initially introduced in 2.27 already */
static void do_version_old_trackto_to_constraints(Object *ob)
{
	/* create new trackto constraint from the relationship */
	if (ob->track)
	{
		bConstraint *con= add_ob_constraint(ob, "AutoTrack", CONSTRAINT_TYPE_TRACKTO);
		bTrackToConstraint *data = con->data;
		
		/* copy tracking settings from the object */
		data->tar = ob->track;
		data->reserved1 = ob->trackflag;
		data->reserved2 = ob->upflag;
	}
	
	/* clear old track setting */
	ob->track = NULL;
}

static void do_versions_seq_unique_name_all_strips(
	Scene * sce, ListBase *seqbasep)
{
	Sequence * seq = seqbasep->first;

	while (seq) {
		seqbase_unique_name_recursive(&sce->ed->seqbase, seq);
		if (seq->seqbase.first) {
			do_versions_seq_unique_name_all_strips(
				sce, &seq->seqbase);
		}
		seq=seq->next;
	}
}


static void do_version_bone_roll_256(Bone *bone)
{
	Bone *child;
	float submat[3][3];
	
	copy_m3_m4(submat, bone->arm_mat);
	mat3_to_vec_roll(submat, NULL, &bone->arm_roll);
	
	for (child = bone->childbase.first; child; child = child->next)
		do_version_bone_roll_256(child);
}

static void do_versions_nodetree_dynamic_sockets(bNodeTree *ntree)
{
	bNodeSocket *sock;
	for (sock=ntree->inputs.first; sock; sock=sock->next)
		sock->flag |= SOCK_DYNAMIC;
	for (sock=ntree->outputs.first; sock; sock=sock->next)
		sock->flag |= SOCK_DYNAMIC;
}

void convert_tface_mt(FileData *fd, Main *main)
{
	Main *gmain;

	/* this is a delayed do_version (so it can create new materials) */
	if (main->versionfile < 259 || (main->versionfile == 259 && main->subversionfile < 3)) {

		//XXX hack, material.c uses G.main all over the place, instead of main
		// temporarily set G.main to the current main
		gmain = G.main;
		G.main = main;

		if (!(do_version_tface(main, 1))) {
			BKE_report(fd->reports, RPT_WARNING, "Texface conversion problem. Error in console");
		}

		//XXX hack, material.c uses G.main allover the place, instead of main
		G.main = gmain;
	}
}

static void do_versions_nodetree_image_default_alpha_output(bNodeTree *ntree)
{
	bNode *node;
	bNodeSocket *sock;
	for (node=ntree->nodes.first; node; node=node->next) {
		if (ELEM(node->type, CMP_NODE_IMAGE, CMP_NODE_R_LAYERS)) {
			/* default Image output value should have 0 alpha */
			sock = node->outputs.first;
			((bNodeSocketValueRGBA*)sock->default_value)->value[3] = 0.0f;
		}
	}
}

static void do_version_ntree_tex_mapping_260(void *UNUSED(data), ID *UNUSED(id), bNodeTree *ntree)
{
	bNode *node;

	for (node=ntree->nodes.first; node; node=node->next) {
		if (node->type == SH_NODE_MAPPING) {
			TexMapping *tex_mapping;

			tex_mapping= node->storage;
			tex_mapping->projx= PROJ_X;
			tex_mapping->projy= PROJ_Y;
			tex_mapping->projz= PROJ_Z;
		}
	}
}

static void do_versions_nodetree_convert_angle(bNodeTree *ntree)
{
	bNode *node;
	for (node=ntree->nodes.first; node; node=node->next) {
		if (node->type == CMP_NODE_ROTATE) {
			/* Convert degrees to radians. */
			bNodeSocket *sock = ((bNodeSocket*)node->inputs.first)->next;
			((bNodeSocketValueFloat*)sock->default_value)->value = DEG2RADF(((bNodeSocketValueFloat*)sock->default_value)->value);
		}
		else if (node->type == CMP_NODE_DBLUR) {
			/* Convert degrees to radians. */
			NodeDBlurData *ndbd= node->storage;
			ndbd->angle = DEG2RADF(ndbd->angle);
			ndbd->spin = DEG2RADF(ndbd->spin);
		}
		else if (node->type == CMP_NODE_DEFOCUS) {
			/* Convert degrees to radians. */
			NodeDefocus *nqd = node->storage;
			/* XXX DNA char to float conversion seems to map the char value into the [0.0f, 1.0f] range... */
			nqd->rotation = DEG2RADF(nqd->rotation*255.0f);
		}
		else if (node->type == CMP_NODE_CHROMA_MATTE) {
			/* Convert degrees to radians. */
			NodeChroma *ndc = node->storage;
			ndc->t1 = DEG2RADF(ndc->t1);
			ndc->t2 = DEG2RADF(ndc->t2);
		}
		else if (node->type == CMP_NODE_GLARE) {
			/* Convert degrees to radians. */
			NodeGlare* ndg = node->storage;
			/* XXX DNA char to float conversion seems to map the char value into the [0.0f, 1.0f] range... */
			ndg->angle_ofs = DEG2RADF(ndg->angle_ofs*255.0f);
		}
		/* XXX TexMapping struct is used by other nodes too (at least node_composite_mapValue),
		 *     but not the rot part...
		 */
		else if (node->type == SH_NODE_MAPPING) {
			/* Convert degrees to radians. */
			TexMapping* tmap = node->storage;
			tmap->rot[0] = DEG2RADF(tmap->rot[0]);
			tmap->rot[1] = DEG2RADF(tmap->rot[1]);
			tmap->rot[2] = DEG2RADF(tmap->rot[2]);
		}
	}
}

void do_versions_image_settings_2_60(Scene *sce)
{
	/* note: rd->subimtype is moved into individual settings now and no longer
	 * exists */
	RenderData *rd= &sce->r;
	ImageFormatData *imf= &sce->r.im_format;

	/* we know no data loss happens here, the old values were in char range */
	imf->imtype=   (char)rd->imtype;
	imf->planes=   (char)rd->planes;
	imf->compress= (char)rd->quality;
	imf->quality=  (char)rd->quality;

	/* default, was stored in multiple places, may override later */
	imf->depth= R_IMF_CHAN_DEPTH_8;

	/* openexr */
	imf->exr_codec = rd->quality & 7; /* strange but true! 0-4 are valid values, OPENEXR_COMPRESS */

	switch (imf->imtype) {
	case R_IMF_IMTYPE_OPENEXR:
		imf->depth=  (rd->subimtype & R_OPENEXR_HALF) ? R_IMF_CHAN_DEPTH_16 : R_IMF_CHAN_DEPTH_32;
		if (rd->subimtype & R_PREVIEW_JPG) {
			imf->flag |= R_IMF_FLAG_PREVIEW_JPG;
		}
		if (rd->subimtype & R_OPENEXR_ZBUF) {
			imf->flag |= R_IMF_FLAG_ZBUF;
		}
		break;
	case R_IMF_IMTYPE_TIFF:
		if (rd->subimtype & R_TIFF_16BIT) {
			imf->depth= R_IMF_CHAN_DEPTH_16;
		}
		break;
	case R_IMF_IMTYPE_JP2:
		if (rd->subimtype & R_JPEG2K_16BIT) {
			imf->depth= R_IMF_CHAN_DEPTH_16;
		}
		else if (rd->subimtype & R_JPEG2K_12BIT) {
			imf->depth= R_IMF_CHAN_DEPTH_12;
		}

		if (rd->subimtype & R_JPEG2K_YCC) {
			imf->jp2_flag |= R_IMF_JP2_FLAG_YCC;
		}
		if (rd->subimtype & R_JPEG2K_CINE_PRESET) {
			imf->jp2_flag |= R_IMF_JP2_FLAG_CINE_PRESET;
		}
		if (rd->subimtype & R_JPEG2K_CINE_48FPS) {
			imf->jp2_flag |= R_IMF_JP2_FLAG_CINE_48;
		}
		break;
	case R_IMF_IMTYPE_CINEON:
	case R_IMF_IMTYPE_DPX:
		if (rd->subimtype & R_CINEON_LOG) {
			imf->cineon_flag |= R_IMF_CINEON_FLAG_LOG;
		}
		break;
	}

}

/* socket use flags were only temporary before */
static void do_versions_nodetree_socket_use_flags_2_62(bNodeTree *ntree)
{
	bNode *node;
	bNodeSocket *sock;
	bNodeLink *link;
	
	for (node=ntree->nodes.first; node; node=node->next) {
		for (sock=node->inputs.first; sock; sock=sock->next)
			sock->flag &= ~SOCK_IN_USE;
		for (sock=node->outputs.first; sock; sock=sock->next)
			sock->flag &= ~SOCK_IN_USE;
	}
	for (sock=ntree->inputs.first; sock; sock=sock->next)
		sock->flag &= ~SOCK_IN_USE;
	for (sock=ntree->outputs.first; sock; sock=sock->next)
		sock->flag &= ~SOCK_IN_USE;
	
	for (link=ntree->links.first; link; link=link->next) {
		link->fromsock->flag |= SOCK_IN_USE;
		link->tosock->flag |= SOCK_IN_USE;
	}
}

/* set the SOCK_AUTO_HIDDEN flag on collapsed nodes */
static void do_versions_nodetree_socket_auto_hidden_flags_2_62(bNodeTree *ntree)
{
	bNode *node;
	bNodeSocket *sock;
	
	for (node=ntree->nodes.first; node; node=node->next) {
		if (node->flag & NODE_HIDDEN) {
			for (sock=node->inputs.first; sock; sock=sock->next) {
				if (sock->link==NULL)
					sock->flag |= SOCK_AUTO_HIDDEN;
			}
			for (sock=node->outputs.first; sock; sock= sock->next) {
				if (nodeCountSocketLinks(ntree, sock)==0)
					sock->flag |= SOCK_AUTO_HIDDEN;
			}
		}
		else {
			for (sock=node->inputs.first; sock; sock= sock->next)
				sock->flag &= ~SOCK_AUTO_HIDDEN;
			for (sock=node->outputs.first; sock; sock= sock->next)
				sock->flag &= ~SOCK_AUTO_HIDDEN;
		}
	}
}

static void do_versions_nodetree_multi_file_output_format_2_62_1(Scene *sce, bNodeTree *ntree)
{
	bNode *node;
	bNodeSocket *sock;
	
	for (node=ntree->nodes.first; node; node=node->next) {
		if (node->type==CMP_NODE_OUTPUT_FILE) {
			/* previous CMP_NODE_OUTPUT_FILE nodes get converted to multi-file outputs */
			NodeImageFile *old_data = node->storage;
			NodeImageMultiFile *nimf= MEM_callocN(sizeof(NodeImageMultiFile), "node image multi file");
			bNodeSocket *old_image = BLI_findlink(&node->inputs, 0);
			bNodeSocket *old_z = BLI_findlink(&node->inputs, 1);
			bNodeSocket *sock;
			
			node->storage= nimf;
			
			BLI_strncpy(nimf->base_path, old_data->name, sizeof(nimf->base_path));
			nimf->format = old_data->im_format;
			
			/* if z buffer is saved, change the image type to multilayer exr.
			 * XXX this is slightly messy, Z buffer was ignored before for anything but EXR and IRIS ...
			 * i'm just assuming here that IRIZ means IRIS with z buffer ...
			 */
			if (ELEM(old_data->im_format.imtype, R_IMF_IMTYPE_IRIZ, R_IMF_IMTYPE_OPENEXR)) {
				nimf->format.imtype = R_IMF_IMTYPE_MULTILAYER;
				sock = ntreeCompositOutputFileAddSocket(ntree, node, old_image->name, &nimf->format);
				if (old_image->link) {
					old_image->link->tosock = sock;
					sock->link = old_image->link;
				}
				sock = ntreeCompositOutputFileAddSocket(ntree, node, old_z->name, &nimf->format);
				if (old_z->link) {
					old_z->link->tosock = sock;
					sock->link = old_z->link;
				}
			}
			else {
				/* saves directly to base path, which is the old image output path */
				sock = ntreeCompositOutputFileAddSocket(ntree, node, "", &nimf->format);
				if (old_image->link) {
					old_image->link->tosock = sock;
					sock->link = old_image->link;
				}
			}
			
			nodeRemoveSocket(ntree, node, old_image);
			nodeRemoveSocket(ntree, node, old_z);
			MEM_freeN(old_data);
		}
		else if (node->type==CMP_NODE_OUTPUT_MULTI_FILE__DEPRECATED) {
			NodeImageMultiFile *nimf = node->storage;
			
			/* CMP_NODE_OUTPUT_MULTI_FILE has been redeclared as CMP_NODE_OUTPUT_FILE */
			node->type = CMP_NODE_OUTPUT_FILE;
			
			/* initialize the node-wide image format from render data, if available */
			if (sce)
				nimf->format = sce->r.im_format;
			
			/* transfer render format toggle to node format toggle */
			for (sock=node->inputs.first; sock; sock=sock->next) {
				NodeImageMultiFileSocket *simf = sock->storage;
				simf->use_node_format = simf->use_render_format;
			}
			
			/* we do have preview now */
			node->flag |= NODE_PREVIEW;
		}
	}
}

/* blue and red are swapped pre 2.62.1, be sane (red == red) now! */
static void do_versions_mesh_mloopcol_swap_2_62_1(Mesh *me)
{
	CustomDataLayer *layer;
	MLoopCol *mloopcol;
	int a;
	int i;

	for (a = 0; a < me->ldata.totlayer; a++) {
		layer = &me->ldata.layers[a];

		if (layer->type == CD_MLOOPCOL) {
			mloopcol = (MLoopCol *)layer->data;
			for (i = 0; i < me->totloop; i++, mloopcol++) {
				SWAP(char, mloopcol->r, mloopcol->b);
			}
		}
	}
}

static void do_versions(FileData *fd, Library *lib, Main *main)
{
	/* WATCH IT!!!: pointers from libdata have not been converted */

	if (G.debug & G_DEBUG)
		printf("read file %s\n  Version %d sub %d svn r%d\n", fd->relabase, main->versionfile, main->subversionfile, main->revision);
	
	if (main->versionfile == 100) {
		/* tex->extend and tex->imageflag have changed: */
		Tex *tex = main->tex.first;
		while (tex) {
			if (tex->id.flag & LIB_NEEDLINK) {

				if (tex->extend==0) {
					if (tex->xrepeat || tex->yrepeat) tex->extend= TEX_REPEAT;
					else {
						tex->extend= TEX_EXTEND;
						tex->xrepeat= tex->yrepeat= 1;
					}
				}

			}
			tex= tex->id.next;
		}
	}
	if (main->versionfile <= 101) {
		/* frame mapping */
		Scene *sce = main->scene.first;
		while (sce) {
			sce->r.framapto= 100;
			sce->r.images= 100;
			sce->r.framelen= 1.0;
			sce= sce->id.next;
		}
	}
	if (main->versionfile <= 102) {
		/* init halo's at 1.0 */
		Material *ma = main->mat.first;
		while (ma) {
			ma->add= 1.0;
			ma= ma->id.next;
		}
	}
	if (main->versionfile <= 103) {
		/* new variable in object: colbits */
		Object *ob = main->object.first;
		int a;
		while (ob) {
			ob->colbits= 0;
			if (ob->totcol) {
				for (a=0; a<ob->totcol; a++) {
					if (ob->mat[a]) ob->colbits |= (1<<a);
				}
			}
			ob= ob->id.next;
		}
	}
	if (main->versionfile <= 104) {
		/* timeoffs moved */
		Object *ob = main->object.first;
		while (ob) {
			if (ob->transflag & 1) {
				ob->transflag -= 1;
				//ob->ipoflag |= OB_OFFS_OB;
			}
			ob= ob->id.next;
		}
	}
	if (main->versionfile <= 105) {
		Object *ob = main->object.first;
		while (ob) {
			ob->dupon= 1; ob->dupoff= 0;
			ob->dupsta= 1; ob->dupend= 100;
			ob= ob->id.next;
		}
	}
	if (main->versionfile <= 106) {
		/* mcol changed */
		Mesh *me = main->mesh.first;
		while (me) {
			if (me->mcol) vcol_to_fcol(me);
			me= me->id.next;
		}

	}
	if (main->versionfile <= 107) {
		Object *ob;
		Scene *sce = main->scene.first;
		while (sce) {
			sce->r.mode |= R_GAMMA;
			sce= sce->id.next;
		}
		ob= main->object.first;
		while (ob) {
			//ob->ipoflag |= OB_OFFS_PARENT;
			if (ob->dt==0) ob->dt= OB_SOLID;
			ob= ob->id.next;
		}

	}
	if (main->versionfile <= 109) {
		/* new variable: gridlines */
		bScreen *sc = main->screen.first;
		while (sc) {
			ScrArea *sa= sc->areabase.first;
			while (sa) {
				SpaceLink *sl= sa->spacedata.first;
				while (sl) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D*) sl;

						if (v3d->gridlines==0) v3d->gridlines= 20;
					}
					sl= sl->next;
				}
				sa= sa->next;
			}
			sc= sc->id.next;
		}
	}
	if (main->versionfile <= 113) {
		Material *ma = main->mat.first;
		while (ma) {
			if (ma->flaresize==0.0f) ma->flaresize= 1.0f;
			ma->subsize= 1.0f;
			ma->flareboost= 1.0f;
			ma= ma->id.next;
		}
	}

	if (main->versionfile <= 134) {
		Tex *tex = main->tex.first;
		while (tex) {
			if ((tex->rfac == 0.0f) &&
				(tex->gfac == 0.0f) &&
				(tex->bfac == 0.0f)) {
				tex->rfac = 1.0f;
				tex->gfac = 1.0f;
				tex->bfac = 1.0f;
				tex->filtersize = 1.0f;
			}
			tex = tex->id.next;
		}
	}
	if (main->versionfile <= 140) {
		/* r-g-b-fac in texture */
		Tex *tex = main->tex.first;
		while (tex) {
			if ((tex->rfac == 0.0f) &&
				(tex->gfac == 0.0f) &&
				(tex->bfac == 0.0f)) {
				tex->rfac = 1.0f;
				tex->gfac = 1.0f;
				tex->bfac = 1.0f;
				tex->filtersize = 1.0f;
			}
			tex = tex->id.next;
		}
	}
	if (main->versionfile <= 153) {
		Scene *sce = main->scene.first;
		while (sce) {
			if (sce->r.blurfac==0.0f) sce->r.blurfac= 1.0f;
			sce= sce->id.next;
		}
	}
	if (main->versionfile <= 163) {
		Scene *sce = main->scene.first;
		while (sce) {
			if (sce->r.frs_sec==0) sce->r.frs_sec= 25;
			sce= sce->id.next;
		}
	}
	if (main->versionfile <= 164) {
		Mesh *me= main->mesh.first;
		while (me) {
			me->smoothresh= 30;
			me= me->id.next;
		}
	}
	if (main->versionfile <= 165) {
		Mesh *me= main->mesh.first;
		TFace *tface;
		int nr;
		char *cp;

		while (me) {
			if (me->tface) {
				nr= me->totface;
				tface= me->tface;
				while (nr--) {
					cp= (char *)&tface->col[0];
					if (cp[1]>126) cp[1]= 255; else cp[1]*=2;
					if (cp[2]>126) cp[2]= 255; else cp[2]*=2;
					if (cp[3]>126) cp[3]= 255; else cp[3]*=2;
					cp= (char *)&tface->col[1];
					if (cp[1]>126) cp[1]= 255; else cp[1]*=2;
					if (cp[2]>126) cp[2]= 255; else cp[2]*=2;
					if (cp[3]>126) cp[3]= 255; else cp[3]*=2;
					cp= (char *)&tface->col[2];
					if (cp[1]>126) cp[1]= 255; else cp[1]*=2;
					if (cp[2]>126) cp[2]= 255; else cp[2]*=2;
					if (cp[3]>126) cp[3]= 255; else cp[3]*=2;
					cp= (char *)&tface->col[3];
					if (cp[1]>126) cp[1]= 255; else cp[1]*=2;
					if (cp[2]>126) cp[2]= 255; else cp[2]*=2;
					if (cp[3]>126) cp[3]= 255; else cp[3]*=2;

					tface++;
				}
			}
			me= me->id.next;
		}
	}

	if (main->versionfile <= 169) {
		Mesh *me= main->mesh.first;
		while (me) {
			if (me->subdiv==0) me->subdiv= 1;
			me= me->id.next;
		}
	}

	if (main->versionfile <= 169) {
		bScreen *sc= main->screen.first;
		while (sc) {
			ScrArea *sa= sc->areabase.first;
			while (sa) {
				SpaceLink *sl= sa->spacedata.first;
				while (sl) {
					if (sl->spacetype==SPACE_IPO) {
						SpaceIpo *sipo= (SpaceIpo*) sl;
						sipo->v2d.max[0]= 15000.0;
					}
					sl= sl->next;
				}
				sa= sa->next;
			}
			sc= sc->id.next;
		}
	}

	if (main->versionfile <= 170) {
		Object *ob = main->object.first;
		PartEff *paf;
		while (ob) {
			paf = do_version_give_parteff_245(ob);
			if (paf) {
				if (paf->staticstep == 0) {
					paf->staticstep= 5;
				}
			}
			ob = ob->id.next;
		}
	}

	if (main->versionfile <= 171) {
		bScreen *sc= main->screen.first;
		while (sc) {
			ScrArea *sa= sc->areabase.first;
			while (sa) {
				SpaceLink *sl= sa->spacedata.first;
				while (sl) {
					if (sl->spacetype==SPACE_TEXT) {
						SpaceText *st= (SpaceText*) sl;
						st->lheight= 12;
					}
					sl= sl->next;
				}
				sa= sa->next;
			}
			sc= sc->id.next;
		}
	}

	if (main->versionfile <= 173) {
		int a, b;
		Mesh *me= main->mesh.first;
		while (me) {
			if (me->tface) {
				TFace *tface= me->tface;
				for (a=0; a<me->totface; a++, tface++) {
					for (b=0; b<4; b++) {
						tface->uv[b][0]/= 32767.0f;
						tface->uv[b][1]/= 32767.0f;
					}
				}
			}
			me= me->id.next;
		}
	}

	if (main->versionfile <= 191) {
		Object *ob= main->object.first;
		Material *ma = main->mat.first;

		/* let faces have default add factor of 0.0 */
		while (ma) {
		  if (!(ma->mode & MA_HALO)) ma->add = 0.0;
		  ma = ma->id.next;
		}

		while (ob) {
			ob->mass= 1.0f;
			ob->damping= 0.1f;
			/*ob->quat[1]= 1.0f;*/ /* quats arnt used yet */
			ob= ob->id.next;
		}
	}

	if (main->versionfile <= 193) {
		Object *ob= main->object.first;
		while (ob) {
			ob->inertia= 1.0f;
			ob->rdamping= 0.1f;
			ob= ob->id.next;
		}
	}

	if (main->versionfile <= 196) {
		Mesh *me= main->mesh.first;
		int a, b;
		while (me) {
			if (me->tface) {
				TFace *tface= me->tface;
				for (a=0; a<me->totface; a++, tface++) {
					for (b=0; b<4; b++) {
						tface->mode |= TF_DYNAMIC;
						tface->mode &= ~TF_INVISIBLE;
					}
				}
			}
			me= me->id.next;
		}
	}

	if (main->versionfile <= 200) {
		Object *ob= main->object.first;
		while (ob) {
			ob->scaflag = ob->gameflag & (OB_DO_FH|OB_ROT_FH|OB_ANISOTROPIC_FRICTION|OB_GHOST|OB_RIGID_BODY|OB_BOUNDS);
				/* 64 is do_fh */
			ob->gameflag &= ~(OB_ROT_FH|OB_ANISOTROPIC_FRICTION|OB_GHOST|OB_RIGID_BODY|OB_BOUNDS);
			ob = ob->id.next;
		}
	}

	if (main->versionfile <= 201) {
		/* add-object + end-object are joined to edit-object actuator */
		Object *ob = main->object.first;
		bProperty *prop;
		bActuator *act;
		bIpoActuator *ia;
		bEditObjectActuator *eoa;
		bAddObjectActuator *aoa;
		while (ob) {
			act = ob->actuators.first;
			while (act) {
				if (act->type==ACT_IPO) {
					ia= act->data;
					prop= get_ob_property(ob, ia->name);
					if (prop) {
						ia->type= ACT_IPO_FROM_PROP;
					}
				}
				else if (act->type==ACT_ADD_OBJECT) {
					aoa= act->data;
					eoa= MEM_callocN(sizeof(bEditObjectActuator), "edit ob act");
					eoa->type= ACT_EDOB_ADD_OBJECT;
					eoa->ob= aoa->ob;
					eoa->time= aoa->time;
					MEM_freeN(aoa);
					act->data= eoa;
					act->type= act->otype= ACT_EDIT_OBJECT;
				}
				else if (act->type==ACT_END_OBJECT) {
					eoa= MEM_callocN(sizeof(bEditObjectActuator), "edit ob act");
					eoa->type= ACT_EDOB_END_OBJECT;
					act->data= eoa;
					act->type= act->otype= ACT_EDIT_OBJECT;
				}
				act= act->next;
			}
			ob = ob->id.next;
		}
	}

	if (main->versionfile <= 202) {
		/* add-object and end-object are joined to edit-object
		 * actuator */
		Object *ob= main->object.first;
		bActuator *act;
		bObjectActuator *oa;
		while (ob) {
			act= ob->actuators.first;
			while (act) {
				if (act->type==ACT_OBJECT) {
					oa= act->data;
					oa->flag &= ~(ACT_TORQUE_LOCAL|ACT_DROT_LOCAL);		/* this actuator didn't do local/glob rot before */
				}
				act= act->next;
			}
			ob= ob->id.next;
		}
	}

	if (main->versionfile <= 204) {
		/* patches for new physics */
		Object *ob= main->object.first;
		bActuator *act;
		bObjectActuator *oa;
		bSound *sound;
		while (ob) {

			/* please check this for demo20 files like
			 * original Egypt levels etc.  converted
			 * rotation factor of 50 is not workable */
			act= ob->actuators.first;
			while (act) {
				if (act->type==ACT_OBJECT) {
					oa= act->data;

					oa->forceloc[0]*= 25.0f;
					oa->forceloc[1]*= 25.0f;
					oa->forceloc[2]*= 25.0f;

					oa->forcerot[0]*= 10.0f;
					oa->forcerot[1]*= 10.0f;
					oa->forcerot[2]*= 10.0f;
				}
				act= act->next;
			}
			ob= ob->id.next;
		}

		sound = main->sound.first;
		while (sound) {
			if (sound->volume < 0.01f) {
				sound->volume = 1.0f;
			}
			sound = sound->id.next;
		}
	}

	if (main->versionfile <= 205) {
		/* patches for new physics */
		Object *ob= main->object.first;
		bActuator *act;
		bSensor *sens;
		bEditObjectActuator *oa;
		bRaySensor *rs;
		bCollisionSensor *cs;
		while (ob) {
			/* Set anisotropic friction off for old objects,
			 * values to 1.0.  */
			ob->gameflag &= ~OB_ANISOTROPIC_FRICTION;
			ob->anisotropicFriction[0] = 1.0;
			ob->anisotropicFriction[1] = 1.0;
			ob->anisotropicFriction[2] = 1.0;

			act= ob->actuators.first;
			while (act) {
				if (act->type==ACT_EDIT_OBJECT) {
					/* Zero initial velocity for newly
					 * added objects */
					oa= act->data;
					oa->linVelocity[0] = 0.0;
					oa->linVelocity[1] = 0.0;
					oa->linVelocity[2] = 0.0;
					oa->localflag = 0;
				}
				act= act->next;
			}

			sens= ob->sensors.first;
			while (sens) {
				/* Extra fields for radar sensors. */
				if (sens->type == SENS_RADAR) {
					bRadarSensor *s = sens->data;
					s->range = 10000.0;
				}

				/* Pulsing: defaults for new sensors. */
				if (sens->type != SENS_ALWAYS) {
					sens->pulse = 0;
					sens->freq = 0;
				}
				else {
					sens->pulse = 1;
				}

				/* Invert: off. */
				sens->invert = 0;

				/* Collision and ray: default = trigger
				 * on property. The material field can
				 * remain empty. */
				if (sens->type == SENS_COLLISION) {
					cs = (bCollisionSensor*) sens->data;
					cs->mode = 0;
				}
				if (sens->type == SENS_RAY) {
					rs = (bRaySensor*) sens->data;
					rs->mode = 0;
				}
				sens = sens->next;
			}
			ob= ob->id.next;
		}
		/* have to check the exact multiplier */
	}

	if (main->versionfile <= 211) {
		/* Render setting: per scene, the applicable gamma value
		 * can be set. Default is 1.0, which means no
		 * correction.  */
		bActuator *act;
		bObjectActuator *oa;
		Object *ob;

		/* added alpha in obcolor */
		ob= main->object.first;
		while (ob) {
			ob->col[3]= 1.0;
			ob= ob->id.next;
		}

		/* added alpha in obcolor */
		ob= main->object.first;
		while (ob) {
			act= ob->actuators.first;
			while (act) {
				if (act->type==ACT_OBJECT) {
					/* multiply velocity with 50 in old files */
					oa= act->data;
					if (fabsf(oa->linearvelocity[0]) >= 0.01f)
						oa->linearvelocity[0] *= 50.0f;
					if (fabsf(oa->linearvelocity[1]) >= 0.01f)
						oa->linearvelocity[1] *= 50.0f;
					if (fabsf(oa->linearvelocity[2]) >= 0.01f)
						oa->linearvelocity[2] *= 50.0f;
					if (fabsf(oa->angularvelocity[0])>=0.01f)
						oa->angularvelocity[0] *= 50.0f;
					if (fabsf(oa->angularvelocity[1])>=0.01f)
						oa->angularvelocity[1] *= 50.0f;
					if (fabsf(oa->angularvelocity[2])>=0.01f)
						oa->angularvelocity[2] *= 50.0f;
				}
				act= act->next;
			}
			ob= ob->id.next;
		}
	}

	if (main->versionfile <= 212) {

		bSound* sound;
		bProperty *prop;
		Object *ob;
		Mesh *me;

		sound = main->sound.first;
		while (sound)
		{
			sound->max_gain = 1.0;
			sound->min_gain = 0.0;
			sound->distance = 1.0;

			if (sound->attenuation > 0.0f)
				sound->flags |= SOUND_FLAGS_3D;
			else
				sound->flags &= ~SOUND_FLAGS_3D;

			sound = sound->id.next;
		}

		ob = main->object.first;

		while (ob) {
			prop= ob->prop.first;
			while (prop) {
				if (prop->type == GPROP_TIME) {
					// convert old GPROP_TIME values from int to float
					*((float *)&prop->data) = (float) prop->data;
				}

				prop= prop->next;
			}
			ob = ob->id.next;
		}

			/* me->subdiv changed to reflect the actual reparametization
			 * better, and smeshes were removed - if it was a smesh make
			 * it a subsurf, and reset the subdiv level because subsurf
			 * takes a lot more work to calculate.
			 */
		for (me= main->mesh.first; me; me= me->id.next) {
			if (me->flag&ME_SMESH) {
				me->flag&= ~ME_SMESH;
				me->flag|= ME_SUBSURF;

				me->subdiv= 1;
			}
			else {
				if (me->subdiv<2)
					me->subdiv= 1;
				else
					me->subdiv--;
			}
		}
	}

	if (main->versionfile <= 220) {
		Object *ob;
		Mesh *me;

		ob = main->object.first;

		/* adapt form factor in order to get the 'old' physics
		 * behavior back...*/

		while (ob) {
			/* in future, distinguish between different
			 * object bounding shapes */
			ob->formfactor = 0.4f;
			/* patch form factor , note that inertia equiv radius
			 * of a rotation symmetrical obj */
			if (ob->inertia != 1.0f) {
				ob->formfactor /= ob->inertia * ob->inertia;
			}
			ob = ob->id.next;
		}

			/* Began using alpha component of vertex colors, but
			 * old file vertex colors are undefined, reset them
			 * to be fully opaque. -zr
			 */
		for (me= main->mesh.first; me; me= me->id.next) {
			if (me->mcol) {
				int i;

				for (i=0; i<me->totface*4; i++) {
					MCol *mcol= &me->mcol[i];
					mcol->a= 255;
				}
			}
			if (me->tface) {
				int i, j;

				for (i=0; i<me->totface; i++) {
					TFace *tf= &((TFace*) me->tface)[i];

					for (j=0; j<4; j++) {
						char *col= (char*) &tf->col[j];

						col[0]= 255;
					}
				}
			}
		}
	}
	if (main->versionfile <= 221) {
		Scene *sce= main->scene.first;

		// new variables for std-alone player and runtime
		while (sce) {

			sce->r.xplay= 640;
			sce->r.yplay= 480;
			sce->r.freqplay= 60;

			sce= sce->id.next;
		}

	}
	if (main->versionfile <= 222) {
		Scene *sce= main->scene.first;

		// new variables for std-alone player and runtime
		while (sce) {

			sce->r.depth= 32;

			sce= sce->id.next;
		}
	}


	if (main->versionfile <= 223) {
		VFont *vf;
		Image *ima;
		Object *ob;

		for (vf= main->vfont.first; vf; vf= vf->id.next) {
			if (strcmp(vf->name+strlen(vf->name)-6, ".Bfont")==0) {
				strcpy(vf->name, FO_BUILTIN_NAME);
			}
		}

		/* Old textures animate at 25 FPS */
		for (ima = main->image.first; ima; ima=ima->id.next) {
			ima->animspeed = 25;
		}

			/* Zr remapped some keyboard codes to be linear (stupid zr) */
		for (ob= main->object.first; ob; ob= ob->id.next) {
			bSensor *sens;

			for (sens= ob->sensors.first; sens; sens= sens->next) {
				if (sens->type==SENS_KEYBOARD) {
					bKeyboardSensor *ks= sens->data;

					ks->key= map_223_keybd_code_to_224_keybd_code(ks->key);
					ks->qual= map_223_keybd_code_to_224_keybd_code(ks->qual);
					ks->qual2= map_223_keybd_code_to_224_keybd_code(ks->qual2);
				}
			}
		}
	}
	if (main->versionfile <= 224) {
		bSound* sound;
		Scene *sce;
		Mesh *me;
		bScreen *sc;

		for (sound=main->sound.first; sound; sound=sound->id.next) {
			if (sound->packedfile) {
				if (sound->newpackedfile == NULL) {
					sound->newpackedfile = sound->packedfile;
				}
				sound->packedfile = NULL;
			}
		}
		/* Make sure that old subsurf meshes don't have zero subdivision level for rendering */
		for (me=main->mesh.first; me; me=me->id.next) {
			if ((me->flag & ME_SUBSURF) && (me->subdivr==0))
				me->subdivr=me->subdiv;
		}

		for (sce= main->scene.first; sce; sce= sce->id.next) {
			sce->r.stereomode = 1;  // no stereo
		}

			/* some oldfile patch, moved from set_func_space */
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;

			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;

				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_IPO) {
						SpaceSeq *sseq= (SpaceSeq*) sl;
						sseq->v2d.keeptot= 0;
					}
				}
			}
		}

	}


	if (main->versionfile <= 225) {
		World *wo;
		/* Use Sumo for old games */
		for (wo = main->world.first; wo; wo= wo->id.next) {
			wo->physicsEngine = 2;
		}
	}

	if (main->versionfile <= 227) {
		Scene *sce;
		Material *ma;
		bScreen *sc;
		Object *ob;

		/*  As of now, this insures that the transition from the old Track system
			to the new full constraint Track is painless for everyone. - theeth
		*/
		ob = main->object.first;

		while (ob) {
			ListBase *list;
			list = &ob->constraints;

			/* check for already existing TrackTo constraint
			   set their track and up flag correctly */

			if (list) {
				bConstraint *curcon;
				for (curcon = list->first; curcon; curcon=curcon->next) {
					if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
						bTrackToConstraint *data = curcon->data;
						data->reserved1 = ob->trackflag;
						data->reserved2 = ob->upflag;
					}
				}
			}

			if (ob->type == OB_ARMATURE) {
				if (ob->pose) {
					bConstraint *curcon;
					bPoseChannel *pchan;
					for (pchan = ob->pose->chanbase.first;
						 pchan; pchan=pchan->next) {
						for (curcon = pchan->constraints.first;
							 curcon; curcon=curcon->next) {
							if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
								bTrackToConstraint *data = curcon->data;
								data->reserved1 = ob->trackflag;
								data->reserved2 = ob->upflag;
							}
						}
					}
				}
			}

			/* Change Ob->Track in real TrackTo constraint */
			do_version_old_trackto_to_constraints(ob);
			
			ob = ob->id.next;
		}


		for (sce= main->scene.first; sce; sce= sce->id.next) {
			sce->audio.mixrate = 44100;
			sce->audio.flag |= AUDIO_SCRUB;
			sce->r.mode |= R_ENVMAP;
		}
		// init new shader vars
		for (ma= main->mat.first; ma; ma= ma->id.next) {
			ma->refrac= 4.0f;
			ma->roughness= 0.5f;
			ma->param[0]= 0.5f;
			ma->param[1]= 0.1f;
			ma->param[2]= 0.1f;
			ma->param[3]= 0.05f;
		}
		// patch for old wrong max view2d settings, allows zooming out more
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;

			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;

				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_ACTION) {
						SpaceAction *sac= (SpaceAction *) sl;
						sac->v2d.max[0]= 32000;
					}
					else if (sl->spacetype==SPACE_NLA) {
						SpaceNla *sla= (SpaceNla *) sl;
						sla->v2d.max[0]= 32000;
					}
				}
			}
		}
	}
	if (main->versionfile <= 228) {
		Scene *sce;
		bScreen *sc;
		Object *ob;


		/*  As of now, this insures that the transition from the old Track system
			to the new full constraint Track is painless for everyone.*/
		ob = main->object.first;

		while (ob) {
			ListBase *list;
			list = &ob->constraints;

			/* check for already existing TrackTo constraint
			   set their track and up flag correctly */

			if (list) {
				bConstraint *curcon;
				for (curcon = list->first; curcon; curcon=curcon->next) {
					if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
						bTrackToConstraint *data = curcon->data;
						data->reserved1 = ob->trackflag;
						data->reserved2 = ob->upflag;
					}
				}
			}

			if (ob->type == OB_ARMATURE) {
				if (ob->pose) {
					bConstraint *curcon;
					bPoseChannel *pchan;
					for (pchan = ob->pose->chanbase.first;
						 pchan; pchan=pchan->next) {
						for (curcon = pchan->constraints.first;
							 curcon; curcon=curcon->next) {
							if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
								bTrackToConstraint *data = curcon->data;
								data->reserved1 = ob->trackflag;
								data->reserved2 = ob->upflag;
							}
						}
					}
				}
			}

			ob = ob->id.next;
		}

		for (sce= main->scene.first; sce; sce= sce->id.next) {
			sce->r.mode |= R_ENVMAP;
		}

		// convert old mainb values for new button panels
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;

			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;

				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_BUTS) {
						SpaceButs *sbuts= (SpaceButs *) sl;

						sbuts->v2d.maxzoom= 1.2f;
						sbuts->align= 1;	/* horizontal default */
					
						if (sbuts->mainb==BUTS_LAMP) {
							sbuts->mainb= CONTEXT_SHADING;
							//sbuts->tab[CONTEXT_SHADING]= TAB_SHADING_LAMP;
						}
						else if (sbuts->mainb==BUTS_MAT) {
							sbuts->mainb= CONTEXT_SHADING;
							//sbuts->tab[CONTEXT_SHADING]= TAB_SHADING_MAT;
						}
						else if (sbuts->mainb==BUTS_TEX) {
							sbuts->mainb= CONTEXT_SHADING;
							//sbuts->tab[CONTEXT_SHADING]= TAB_SHADING_TEX;
						}
						else if (sbuts->mainb==BUTS_ANIM) {
							sbuts->mainb= CONTEXT_OBJECT;
						}
						else if (sbuts->mainb==BUTS_WORLD) {
							sbuts->mainb= CONTEXT_SCENE;
							//sbuts->tab[CONTEXT_SCENE]= TAB_SCENE_WORLD;
						}
						else if (sbuts->mainb==BUTS_RENDER) {
							sbuts->mainb= CONTEXT_SCENE;
							//sbuts->tab[CONTEXT_SCENE]= TAB_SCENE_RENDER;
						}
						else if (sbuts->mainb==BUTS_GAME) {
							sbuts->mainb= CONTEXT_LOGIC;
						}
						else if (sbuts->mainb==BUTS_FPAINT) {
							sbuts->mainb= CONTEXT_EDITING;
						}
						else if (sbuts->mainb==BUTS_RADIO) {
							sbuts->mainb= CONTEXT_SHADING;
							//sbuts->tab[CONTEXT_SHADING]= TAB_SHADING_RAD;
						}
						else if (sbuts->mainb==BUTS_CONSTRAINT) {
							sbuts->mainb= CONTEXT_OBJECT;
						}
						else if (sbuts->mainb==BUTS_SCRIPT) {
							sbuts->mainb= CONTEXT_OBJECT;
						}
						else if (sbuts->mainb==BUTS_EDIT) {
							sbuts->mainb= CONTEXT_EDITING;
						}
						else sbuts->mainb= CONTEXT_SCENE;
					}
				}
			}
		}
	}
	/* ton: made this 230 instead of 229,
	   to be sure (tuho files) and this is a reliable check anyway
	   nevertheless, we might need to think over a fitness (initialize)
	   check apart from the do_versions() */

	if (main->versionfile <= 230) {
		bScreen *sc;

		// new variable blockscale, for panels in any area
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;

			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;

				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->blockscale==0.0f) sl->blockscale= 0.7f;
					/* added: 5x better zoom in for action */
					if (sl->spacetype==SPACE_ACTION) {
						SpaceAction *sac= (SpaceAction *)sl;
						sac->v2d.maxzoom= 50;
					}
				}
			}
		}
	}
	if (main->versionfile <= 231) {
		/* new bit flags for showing/hiding grid floor and axes */
		bScreen *sc = main->screen.first;
		while (sc) {
			ScrArea *sa= sc->areabase.first;
			while (sa) {
				SpaceLink *sl= sa->spacedata.first;
				while (sl) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D*) sl;

						if (v3d->gridflag==0) {
							v3d->gridflag |= V3D_SHOW_X;
							v3d->gridflag |= V3D_SHOW_Y;
							v3d->gridflag |= V3D_SHOW_FLOOR;
							v3d->gridflag &= ~V3D_SHOW_Z;
						}
					}
					sl= sl->next;
				}
				sa= sa->next;
			}
			sc= sc->id.next;
		}
	}
	if (main->versionfile <= 231) {
		Material *ma= main->mat.first;
		bScreen *sc = main->screen.first;
		Scene *sce;
		Lamp *la;
		World *wrld;

		/* introduction of raytrace */
		while (ma) {
			if (ma->fresnel_tra_i==0.0f) ma->fresnel_tra_i= 1.25f;
			if (ma->fresnel_mir_i==0.0f) ma->fresnel_mir_i= 1.25f;

			ma->ang= 1.0;
			ma->ray_depth= 2;
			ma->ray_depth_tra= 2;
			ma->fresnel_tra= 0.0;
			ma->fresnel_mir= 0.0;

			ma= ma->id.next;
		}
		sce= main->scene.first;
		while (sce) {
			if (sce->r.gauss==0.0f) sce->r.gauss= 1.0f;
			sce= sce->id.next;
		}
		la= main->lamp.first;
		while (la) {
			if (la->k==0.0f) la->k= 1.0;
			if (la->ray_samp==0) la->ray_samp= 1;
			if (la->ray_sampy==0) la->ray_sampy= 1;
			if (la->ray_sampz==0) la->ray_sampz= 1;
			if (la->area_size==0.0f) la->area_size= 1.0f;
			if (la->area_sizey==0.0f) la->area_sizey= 1.0f;
			if (la->area_sizez==0.0f) la->area_sizez= 1.0f;
			la= la->id.next;
		}
		wrld= main->world.first;
		while (wrld) {
			if (wrld->range==0.0f) {
				wrld->range= 1.0f/wrld->exposure;
			}
			wrld= wrld->id.next;
		}

		/* new bit flags for showing/hiding grid floor and axes */

		while (sc) {
			ScrArea *sa= sc->areabase.first;
			while (sa) {
				SpaceLink *sl= sa->spacedata.first;
				while (sl) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D*) sl;

						if (v3d->gridflag==0) {
							v3d->gridflag |= V3D_SHOW_X;
							v3d->gridflag |= V3D_SHOW_Y;
							v3d->gridflag |= V3D_SHOW_FLOOR;
							v3d->gridflag &= ~V3D_SHOW_Z;
						}
					}
					sl= sl->next;
				}
				sa= sa->next;
			}
			sc= sc->id.next;
		}
	}
	if (main->versionfile <= 232) {
		Tex *tex= main->tex.first;
		World *wrld= main->world.first;
		bScreen *sc;
		Scene *sce;

		while (tex) {
			if ((tex->flag & (TEX_CHECKER_ODD+TEX_CHECKER_EVEN))==0) {
				tex->flag |= TEX_CHECKER_ODD;
			}
			/* copied from kernel texture.c */
			if (tex->ns_outscale==0.0f) {
				/* musgrave */
				tex->mg_H = 1.0f;
				tex->mg_lacunarity = 2.0f;
				tex->mg_octaves = 2.0f;
				tex->mg_offset = 1.0f;
				tex->mg_gain = 1.0f;
				tex->ns_outscale = 1.0f;
				/* distnoise */
				tex->dist_amount = 1.0f;
				/* voronoi */
				tex->vn_w1 = 1.0f;
				tex->vn_mexp = 2.5f;
			}
			tex= tex->id.next;
		}

		while (wrld) {
			if (wrld->aodist==0.0f) {
				wrld->aodist= 10.0f;
				wrld->aobias= 0.05f;
			}
			if (wrld->aosamp==0) wrld->aosamp= 5;
			if (wrld->aoenergy==0.0f) wrld->aoenergy= 1.0f;
			wrld= wrld->id.next;
		}


		// new variable blockscale, for panels in any area, do again because new
		// areas didnt initialize it to 0.7 yet
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->blockscale==0.0f) sl->blockscale= 0.7f;

					/* added: 5x better zoom in for nla */
					if (sl->spacetype==SPACE_NLA) {
						SpaceNla *snla= (SpaceNla *)sl;
						snla->v2d.maxzoom= 50;
					}
				}
			}
		}
		sce= main->scene.first;
		while (sce) {
			if (sce->r.ocres==0) sce->r.ocres= 64;
			sce= sce->id.next;
		}

	}
	if (main->versionfile <= 233) {
		bScreen *sc;
		Material *ma= main->mat.first;
		/* Object *ob= main->object.first; */
		
		while (ma) {
			if (ma->rampfac_col==0.0f) ma->rampfac_col= 1.0;
			if (ma->rampfac_spec==0.0f) ma->rampfac_spec= 1.0;
			if (ma->pr_lamp==0) ma->pr_lamp= 3;
			ma= ma->id.next;
		}
		
		/* this should have been done loooong before! */
#if 0   /* deprecated in 2.5+ */
		while (ob) {
			if (ob->ipowin==0) ob->ipowin= ID_OB;
			ob= ob->id.next;
		}
#endif
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D *)sl;
						v3d->flag |= V3D_SELECT_OUTLINE;
					}
				}
			}
		}
	}


	

	if (main->versionfile <= 234) {
		World *wo;
		bScreen *sc;
		
		// force sumo engine to be active
		for (wo = main->world.first; wo; wo= wo->id.next) {
			if (wo->physicsEngine==0) wo->physicsEngine = 2;
		}
		
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D *)sl;
						v3d->flag |= V3D_ZBUF_SELECT;
					}
					else if (sl->spacetype==SPACE_TEXT) {
						SpaceText *st= (SpaceText *)sl;
						if (st->tabnumber==0) st->tabnumber= 2;
					}
				}
			}
		}
	}
	if (main->versionfile <= 235) {
		Tex *tex= main->tex.first;
		Scene *sce= main->scene.first;
		Sequence *seq;
		Editing *ed;
		
		while (tex) {
			if (tex->nabla==0.0f) tex->nabla= 0.025f;
			tex= tex->id.next;
		}
		while (sce) {
			ed= sce->ed;
			if (ed) {
				SEQ_BEGIN(sce->ed, seq) {
					if (seq->type==SEQ_IMAGE || seq->type==SEQ_MOVIE)
						seq->flag |= SEQ_MAKE_PREMUL;
				}
				SEQ_END
			}
			
			sce= sce->id.next;
		}
	}
	if (main->versionfile <= 236) {
		Object *ob;
		Camera *cam= main->camera.first;
		Material *ma;
		bScreen *sc;

		while (cam) {
			if (cam->ortho_scale==0.0f) {
				cam->ortho_scale= 256.0f/cam->lens;
				if (cam->type==CAM_ORTHO) printf("NOTE: ortho render has changed, tweak new Camera 'scale' value.\n");
			}
			cam= cam->id.next;
		}
		/* set manipulator type */
		/* force oops draw if depgraph was set*/
		/* set time line var */
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D *)sl;
						if (v3d->twtype==0) v3d->twtype= V3D_MANIP_TRANSLATE;
					}
				}
			}
		}
		// init new shader vars
		for (ma= main->mat.first; ma; ma= ma->id.next) {
			if (ma->darkness==0.0f) {
				ma->rms=0.1f;
				ma->darkness=1.0f;
			}
		}
		
		/* softbody init new vars */
		for (ob= main->object.first; ob; ob= ob->id.next) {
			if (ob->soft) {
				if (ob->soft->defgoal==0.0f) ob->soft->defgoal= 0.7f;
				if (ob->soft->physics_speed==0.0f) ob->soft->physics_speed= 1.0f;
				
				if (ob->soft->interval==0) {
					ob->soft->interval= 2;
					ob->soft->sfra= 1;
					ob->soft->efra= 100;
				}
			}
			if (ob->soft && ob->soft->vertgroup==0) {
				bDeformGroup *locGroup = defgroup_find_name(ob, "SOFTGOAL");
				if (locGroup) {
					/* retrieve index for that group */
					ob->soft->vertgroup =  1 + BLI_findindex(&ob->defbase, locGroup);
				}
			}
		}
	}
	if (main->versionfile <= 237) {
		bArmature *arm;
		bConstraint *con;
		Object *ob;
		Bone *bone;
		
		// armature recode checks 
		for (arm= main->armature.first; arm; arm= arm->id.next) {
			where_is_armature(arm);

			for (bone= arm->bonebase.first; bone; bone= bone->next)
				do_version_bone_head_tail_237(bone);
		}
		for (ob= main->object.first; ob; ob= ob->id.next) {
			if (ob->parent) {
				Object *parent= newlibadr(fd, lib, ob->parent);
				if (parent && parent->type==OB_LATTICE)
					ob->partype = PARSKEL;
			}

			// btw. armature_rebuild_pose is further only called on leave editmode
			if (ob->type==OB_ARMATURE) {
				if (ob->pose)
					ob->pose->flag |= POSE_RECALC;
				ob->recalc |= OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME;	// cannot call stuff now (pointers!), done in setup_app_data

				/* new generic xray option */
				arm= newlibadr(fd, lib, ob->data);
				if (arm->flag & ARM_DRAWXRAY) {
					ob->dtx |= OB_DRAWXRAY;
				}
			}
			else if (ob->type==OB_MESH) {
				Mesh *me = newlibadr(fd, lib, ob->data);
				
				if ((me->flag&ME_SUBSURF)) {
					SubsurfModifierData *smd = (SubsurfModifierData*) modifier_new(eModifierType_Subsurf);
					
					smd->levels = MAX2(1, me->subdiv);
					smd->renderLevels = MAX2(1, me->subdivr);
					smd->subdivType = me->subsurftype;
					
					smd->modifier.mode = 0;
					if (me->subdiv!=0)
						smd->modifier.mode |= 1;
					if (me->subdivr!=0)
						smd->modifier.mode |= 2;
					if (me->flag&ME_OPT_EDGES)
						smd->flags |= eSubsurfModifierFlag_ControlEdges;
					
					BLI_addtail(&ob->modifiers, smd);
					
					modifier_unique_name(&ob->modifiers, (ModifierData*)smd);
				}
			}
			
			// follow path constraint needs to set the 'path' option in curves...
			for (con=ob->constraints.first; con; con= con->next) {
				if (con->type==CONSTRAINT_TYPE_FOLLOWPATH) {
					bFollowPathConstraint *data = con->data;
					Object *obc= newlibadr(fd, lib, data->tar);
					
					if (obc && obc->type==OB_CURVE) {
						Curve *cu= newlibadr(fd, lib, obc->data);
						if (cu) cu->flag |= CU_PATH;
					}
				}
			}
		}
	}
	if (main->versionfile <= 238) {
		Lattice *lt;
		Object *ob;
		bArmature *arm;
		Mesh *me;
		Key *key;
		Scene *sce= main->scene.first;

		while (sce) {
			if (sce->toolsettings == NULL) {
				sce->toolsettings = MEM_callocN(sizeof(struct ToolSettings),"Tool Settings Struct");	
				sce->toolsettings->cornertype=0;
				sce->toolsettings->degr = 90; 
				sce->toolsettings->step = 9;
				sce->toolsettings->turn = 1; 				
				sce->toolsettings->extr_offs = 1; 
				sce->toolsettings->doublimit = 0.001f;
				sce->toolsettings->segments = 32;
				sce->toolsettings->rings = 32;
				sce->toolsettings->vertices = 32;
				sce->toolsettings->editbutflag =1;
			}
			sce= sce->id.next;	
		}

		for (lt=main->latt.first; lt; lt=lt->id.next) {
			if (lt->fu==0.0f && lt->fv==0.0f && lt->fw==0.0f) {
				calc_lat_fudu(lt->flag, lt->pntsu, &lt->fu, &lt->du);
				calc_lat_fudu(lt->flag, lt->pntsv, &lt->fv, &lt->dv);
				calc_lat_fudu(lt->flag, lt->pntsw, &lt->fw, &lt->dw);
			}
		}

		for (ob=main->object.first; ob; ob= ob->id.next) {
			ModifierData *md;
			PartEff *paf;

			for (md=ob->modifiers.first; md; md=md->next) {
				if (md->type==eModifierType_Subsurf) {
					SubsurfModifierData *smd = (SubsurfModifierData*) md;

					smd->flags &= ~(eSubsurfModifierFlag_Incremental|eSubsurfModifierFlag_DebugIncr);
				}
			}

			if ((ob->softflag&OB_SB_ENABLE) && !modifiers_findByType(ob, eModifierType_Softbody)) {
				if (ob->softflag&OB_SB_POSTDEF) {
					md = ob->modifiers.first;

					while (md && modifierType_getInfo(md->type)->type==eModifierTypeType_OnlyDeform) {
						md = md->next;
					}

					BLI_insertlinkbefore(&ob->modifiers, md, modifier_new(eModifierType_Softbody));
				}
				else {
					BLI_addhead(&ob->modifiers, modifier_new(eModifierType_Softbody));
				}

				ob->softflag &= ~OB_SB_ENABLE;
			}
			if (ob->pose) {
				bPoseChannel *pchan;
				bConstraint *con;
				for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
					// note, pchan->bone is also lib-link stuff
					if (pchan->limitmin[0] == 0.0f && pchan->limitmax[0] == 0.0f) {
						pchan->limitmin[0]= pchan->limitmin[1]= pchan->limitmin[2]= -180.0f;
						pchan->limitmax[0]= pchan->limitmax[1]= pchan->limitmax[2]= 180.0f;
						
						for (con= pchan->constraints.first; con; con= con->next) {
							if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
								bKinematicConstraint *data = (bKinematicConstraint*)con->data;
								data->weight = 1.0f;
								data->orientweight = 1.0f;
								data->flag &= ~CONSTRAINT_IK_ROT;
								
								/* enforce conversion from old IK_TOPARENT to rootbone index */
								data->rootbone= -1;
								
								/* update_pose_etc handles rootbone==-1 */
								ob->pose->flag |= POSE_RECALC;
							}	
						}
					}
				}
			}

			paf = do_version_give_parteff_245(ob);
			if (paf) {
				if (paf->disp == 0)
					paf->disp = 100;
				if (paf->speedtex == 0)
					paf->speedtex = 8;
				if (paf->omat == 0)
					paf->omat = 1;
			}
		}
		
		for (arm=main->armature.first; arm; arm= arm->id.next) {
			bone_version_238(&arm->bonebase);
			arm->deformflag |= ARM_DEF_VGROUP;
		}

		for (me=main->mesh.first; me; me= me->id.next) {
			if (!me->medge) {
				make_edges(me, 1);	/* 1 = use mface->edcode */
			}
			else {
				mesh_strip_loose_faces(me);
			}
		}
		
		for (key= main->key.first; key; key= key->id.next) {
			KeyBlock *kb;
			int index = 1;

			for (kb= key->block.first; kb; kb= kb->next) {
				if (kb==key->refkey) {
					if (kb->name[0]==0)
						strcpy(kb->name, "Basis");
				}
				else {
					if (kb->name[0]==0) {
						BLI_snprintf(kb->name, sizeof(kb->name), "Key %d", index);
					}
					index++;
				}
			}
		}
	}
	if (main->versionfile <= 239) {
		bArmature *arm;
		Object *ob;
		Scene *sce= main->scene.first;
		Camera *cam= main->camera.first;
		Material *ma= main->mat.first;
		int set_passepartout= 0;
		
		/* deformflag is local in modifier now */
		for (ob=main->object.first; ob; ob= ob->id.next) {
			ModifierData *md;
			
			for (md=ob->modifiers.first; md; md=md->next) {
				if (md->type==eModifierType_Armature) {
					ArmatureModifierData *amd = (ArmatureModifierData*) md;
					if (amd->object && amd->deformflag==0) {
						Object *oba= newlibadr(fd, lib, amd->object);
						arm= newlibadr(fd, lib, oba->data);
						amd->deformflag= arm->deformflag;
					}
				}
			}
		}
		
		/* updating stepsize for ghost drawing */
		for (arm= main->armature.first; arm; arm= arm->id.next) {
			if (arm->ghostsize==0) arm->ghostsize=1;
			bone_version_239(&arm->bonebase);
			if (arm->layer==0) arm->layer= 1;
		}
		
		for (;sce;sce= sce->id.next) {
			/* make 'innervert' the default subdivide type, for backwards compat */
			sce->toolsettings->cornertype=1;
		
			if (sce->r.scemode & R_PASSEPARTOUT) {
				set_passepartout= 1;
				sce->r.scemode &= ~R_PASSEPARTOUT;
			}
			/* gauss is filter variable now */
			if (sce->r.mode & R_GAUSS) {
				sce->r.filtertype= R_FILTER_GAUSS;
				sce->r.mode &= ~R_GAUSS;
			}
		}
		
		for (;cam; cam= cam->id.next) {
			if (set_passepartout)
				cam->flag |= CAM_SHOWPASSEPARTOUT;
			
			/* make sure old cameras have title safe on */
			if (!(cam->flag & CAM_SHOWTITLESAFE))
				cam->flag |= CAM_SHOWTITLESAFE;
			
			/* set an appropriate camera passepartout alpha */
			if (!(cam->passepartalpha)) cam->passepartalpha = 0.2f;
		}
		
		for (; ma; ma= ma->id.next) {
			if (ma->strand_sta==0.0f) {
				ma->strand_sta= ma->strand_end= 1.0f;
				ma->mode |= MA_TANGENT_STR;
			}
			if (ma->mode & MA_TRACEBLE) ma->mode |= MA_SHADBUF;
		}
	}
	
	if (main->versionfile <= 241) {
		Object *ob;
		Tex *tex;
		Scene *sce;
		World *wo;
		Lamp *la;
		Material *ma;
		bArmature *arm;
		bNodeTree *ntree;
		
		for (wo = main->world.first; wo; wo= wo->id.next) {
			/* Migrate to Bullet for games, except for the NaN versions */
			/* People can still explicitly choose for Sumo (after 2.42 is out) */
			if (main->versionfile > 225)
				wo->physicsEngine = WOPHY_BULLET;
			if (WO_AODIST == wo->aomode)
				wo->aocolor= WO_AOPLAIN;
		}
		
		/* updating layers still */
		for (arm= main->armature.first; arm; arm= arm->id.next) {
			bone_version_239(&arm->bonebase);
			if (arm->layer==0) arm->layer= 1;
		}
		for (sce= main->scene.first; sce; sce= sce->id.next) {
			if (sce->audio.mixrate==0) sce->audio.mixrate= 44100;

			if (sce->r.xparts<2) sce->r.xparts= 4;
			if (sce->r.yparts<2) sce->r.yparts= 4;
			/* adds default layer */
			if (sce->r.layers.first==NULL)
				scene_add_render_layer(sce, NULL);
			else {
				SceneRenderLayer *srl;
				/* new layer flag for sky, was default for solid */
				for (srl= sce->r.layers.first; srl; srl= srl->next) {
					if (srl->layflag & SCE_LAY_SOLID)
						srl->layflag |= SCE_LAY_SKY;
					srl->passflag &= (SCE_PASS_COMBINED|SCE_PASS_Z|SCE_PASS_NORMAL|SCE_PASS_VECTOR);
				}
			}
			
			/* node version changes */
			if (sce->nodetree)
				ntree_version_241(sce->nodetree);

			/* uv calculation options moved to toolsettings */
			if (sce->toolsettings->uvcalc_radius == 0.0f) {
				sce->toolsettings->uvcalc_radius = 1.0f;
				sce->toolsettings->uvcalc_cubesize = 1.0f;
				sce->toolsettings->uvcalc_mapdir = 1;
				sce->toolsettings->uvcalc_mapalign = 1;
				sce->toolsettings->uvcalc_flag = UVCALC_FILLHOLES;
				sce->toolsettings->unwrapper = 1;
			}

			if (sce->r.mode & R_PANORAMA) {
				/* all these checks to ensure saved files with svn version keep working... */
				if (sce->r.xsch < sce->r.ysch) {
					Object *obc= newlibadr(fd, lib, sce->camera);
					if (obc && obc->type==OB_CAMERA) {
						Camera *cam= newlibadr(fd, lib, obc->data);
						if (cam->lens>=10.0f) {
							sce->r.xsch*= sce->r.xparts;
							cam->lens*= (float)sce->r.ysch/(float)sce->r.xsch;
						}
					}
				}
			}
		}
		
		for (ntree= main->nodetree.first; ntree; ntree= ntree->id.next)
			ntree_version_241(ntree);
		
		for (la= main->lamp.first; la; la= la->id.next)
			if (la->buffers==0)
				la->buffers= 1;
		
		for (tex= main->tex.first; tex; tex= tex->id.next) {
			if (tex->env && tex->env->viewscale==0.0f)
				tex->env->viewscale= 1.0f;
//			tex->imaflag |= TEX_GAUSS_MIP;
		}
		
		/* for empty drawsize and drawtype */
		for (ob=main->object.first; ob; ob= ob->id.next) {
			if (ob->empty_drawsize==0.0f) {
				ob->empty_drawtype = OB_ARROWS;
				ob->empty_drawsize = 1.0;
			}
		}
		
		for (ma= main->mat.first; ma; ma= ma->id.next) {
			/* stucci returns intensity from now on */
			int a;
			for (a=0; a<MAX_MTEX; a++) {
				if (ma->mtex[a] && ma->mtex[a]->tex) {
					tex= newlibadr(fd, lib, ma->mtex[a]->tex);
					if (tex && tex->type==TEX_STUCCI)
						ma->mtex[a]->mapto &= ~(MAP_COL|MAP_SPEC|MAP_REF);
				}
			}
			/* transmissivity defaults */
			if (ma->tx_falloff==0.0f) ma->tx_falloff= 1.0f;
		}
		
		/* during 2.41 images with this name were used for viewer node output, lets fix that */
		if (main->versionfile == 241) {
			Image *ima;
			for (ima= main->image.first; ima; ima= ima->id.next)
				if (strcmp(ima->name, "Compositor")==0) {
					strcpy(ima->id.name+2, "Viewer Node");
					strcpy(ima->name, "Viewer Node");
				}
		}
	}
		
	if (main->versionfile <= 242) {
		Scene *sce;
		bScreen *sc;
		Object *ob;
		Curve *cu;
		Material *ma;
		Mesh *me;
		Group *group;
		Nurb *nu;
		BezTriple *bezt;
		BPoint *bp;
		bNodeTree *ntree;
		int a;
		
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			sa= sc->areabase.first;
			while (sa) {
				SpaceLink *sl;

				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D*) sl;
						if (v3d->gridsubdiv == 0)
							v3d->gridsubdiv = 10;
					}
				}
				sa = sa->next;
			}
		}
		
		for (sce= main->scene.first; sce; sce= sce->id.next) {
			if (sce->toolsettings->select_thresh == 0.0f)
				sce->toolsettings->select_thresh= 0.01f;
			if (sce->toolsettings->clean_thresh == 0.0f) 
				sce->toolsettings->clean_thresh = 0.1f;
				
			if (sce->r.threads==0) {
				if (sce->r.mode & R_THREADS)
					sce->r.threads= 2;
				else
					sce->r.threads= 1;
			}
			if (sce->nodetree)
				ntree_version_242(sce->nodetree);
		}
		
		for (ntree= main->nodetree.first; ntree; ntree= ntree->id.next)
			ntree_version_242(ntree);
		
		/* add default radius values to old curve points */
		for (cu= main->curve.first; cu; cu= cu->id.next) {
			for (nu= cu->nurb.first; nu; nu= nu->next) {
				if (nu) {
					if (nu->bezt) {
						for (bezt=nu->bezt, a=0; a<nu->pntsu; a++, bezt++) {
							if (!bezt->radius) bezt->radius= 1.0;
						}
					}
					else if (nu->bp) {
						for (bp=nu->bp, a=0; a<nu->pntsu*nu->pntsv; a++, bp++) {
							if (!bp->radius) bp->radius= 1.0;
						}
					}
				}
			}
		}
		
		for (ob = main->object.first; ob; ob= ob->id.next) {
			ModifierData *md;
			ListBase *list;
			list = &ob->constraints;

			/* check for already existing MinMax (floor) constraint
			   and update the sticky flagging */

			if (list) {
				bConstraint *curcon;
				for (curcon = list->first; curcon; curcon=curcon->next) {
					switch (curcon->type) {
						case CONSTRAINT_TYPE_MINMAX:
						{
							bMinMaxConstraint *data = curcon->data;
							if (data->sticky==1) 
								data->flag |= MINMAX_STICKY;
							else 
								data->flag &= ~MINMAX_STICKY;
						}
							break;
						case CONSTRAINT_TYPE_ROTLIKE:
						{
							bRotateLikeConstraint *data = curcon->data;
							
							/* version patch from buttons_object.c */
							if (data->flag==0)
								data->flag = ROTLIKE_X|ROTLIKE_Y|ROTLIKE_Z;
						}
							break;
					}
				}
			}

			if (ob->type == OB_ARMATURE) {
				if (ob->pose) {
					bConstraint *curcon;
					bPoseChannel *pchan;
					for (pchan = ob->pose->chanbase.first; pchan; pchan=pchan->next) {
						for (curcon = pchan->constraints.first; curcon; curcon=curcon->next) {
							switch (curcon->type) {
								case CONSTRAINT_TYPE_MINMAX:
								{
									bMinMaxConstraint *data = curcon->data;
									if (data->sticky==1) 
										data->flag |= MINMAX_STICKY;
									else 
										data->flag &= ~MINMAX_STICKY;
								}
									break;
								case CONSTRAINT_TYPE_KINEMATIC:
								{
									bKinematicConstraint *data = curcon->data;
									if (!(data->flag & CONSTRAINT_IK_POS)) {
										data->flag |= CONSTRAINT_IK_POS;
										data->flag |= CONSTRAINT_IK_STRETCH;
									}
								}
									break;
								case CONSTRAINT_TYPE_ROTLIKE:
								{
									bRotateLikeConstraint *data = curcon->data;
									
									/* version patch from buttons_object.c */
									if (data->flag==0)
										data->flag = ROTLIKE_X|ROTLIKE_Y|ROTLIKE_Z;
								}
									break;
							}
						}
					}
				}
			}
			
			/* copy old object level track settings to curve modifers */
			for (md=ob->modifiers.first; md; md=md->next) {
				if (md->type==eModifierType_Curve) {
					CurveModifierData *cmd = (CurveModifierData*) md;

					if (cmd->defaxis == 0) cmd->defaxis = ob->trackflag+1;
				}
			}
			
		}
		
		for (ma = main->mat.first; ma; ma= ma->id.next) {
			if (ma->shad_alpha==0.0f)
				ma->shad_alpha= 1.0f;
			if (ma->nodetree)
				ntree_version_242(ma->nodetree);
		}

		for (me=main->mesh.first; me; me=me->id.next)
			customdata_version_242(me);
		
		for (group= main->group.first; group; group= group->id.next)
			if (group->layer==0)
				group->layer= (1<<20)-1;

		/* now, subversion control! */
		if (main->subversionfile < 3) {
			Image *ima;
			Tex *tex;
			
			/* Image refactor initialize */
			for (ima= main->image.first; ima; ima= ima->id.next) {
				ima->source= IMA_SRC_FILE;
				ima->type= IMA_TYPE_IMAGE;
				
				ima->gen_x= 256; ima->gen_y= 256;
				ima->gen_type= 1;
				
				if (0==strncmp(ima->id.name+2, "Viewer Node", sizeof(ima->id.name)-2)) {
					ima->source= IMA_SRC_VIEWER;
					ima->type= IMA_TYPE_COMPOSITE;
				}
				if (0==strncmp(ima->id.name+2, "Render Result", sizeof(ima->id.name)-2)) {
					ima->source= IMA_SRC_VIEWER;
					ima->type= IMA_TYPE_R_RESULT;
				}
				
			}
			for (tex= main->tex.first; tex; tex= tex->id.next) {
				if (tex->type==TEX_IMAGE && tex->ima) {
					ima= newlibadr(fd, lib, tex->ima);
					if (tex->imaflag & TEX_ANIM5_)
						ima->source= IMA_SRC_MOVIE;
					if (tex->imaflag & TEX_FIELDS_)
						ima->flag |= IMA_FIELDS;
					if (tex->imaflag & TEX_STD_FIELD_)
						ima->flag |= IMA_STD_FIELD;
				}
				tex->iuser.frames= tex->frames;
				tex->iuser.fie_ima= (char)tex->fie_ima;
				tex->iuser.offset= tex->offset;
				tex->iuser.sfra= tex->sfra;
				tex->iuser.cycl= (tex->imaflag & TEX_ANIMCYCLIC_)!=0;
			}
			for (sce= main->scene.first; sce; sce= sce->id.next) {
				if (sce->nodetree)
					do_version_ntree_242_2(sce->nodetree);
			}
			for (ntree= main->nodetree.first; ntree; ntree= ntree->id.next)
				do_version_ntree_242_2(ntree);
			for (ma = main->mat.first; ma; ma= ma->id.next)
				if (ma->nodetree)
					do_version_ntree_242_2(ma->nodetree);
			
			for (sc= main->screen.first; sc; sc= sc->id.next) {
				ScrArea *sa;
				for (sa= sc->areabase.first; sa; sa= sa->next) {
					SpaceLink *sl;
					for (sl= sa->spacedata.first; sl; sl= sl->next) {
						if (sl->spacetype==SPACE_IMAGE)
							((SpaceImage *)sl)->iuser.fie_ima= 2;
						else if (sl->spacetype==SPACE_VIEW3D) {
							View3D *v3d= (View3D *)sl;
							BGpic *bgpic;
							for (bgpic= v3d->bgpicbase.first; bgpic; bgpic= bgpic->next)
								bgpic->iuser.fie_ima= 2;
						}
					}
				}
			}
		}
		
		if (main->subversionfile < 4) {
			for (sce= main->scene.first; sce; sce= sce->id.next) {
				sce->r.bake_mode= 1;	/* prevent to include render stuff here */
				sce->r.bake_filter= 2;
				sce->r.bake_osa= 5;
				sce->r.bake_flag= R_BAKE_CLEAR;
			}
		}

		if (main->subversionfile < 5) {
			for (sce= main->scene.first; sce; sce= sce->id.next) {
				/* improved triangle to quad conversion settings */
				if (sce->toolsettings->jointrilimit==0.0f)
					sce->toolsettings->jointrilimit= 0.8f;
			}
		}
	}
	if (main->versionfile <= 243) {
		Object *ob= main->object.first;
		Material *ma;

		for (ma=main->mat.first; ma; ma= ma->id.next) {
			if (ma->sss_scale==0.0f) {
				ma->sss_radius[0]= 1.0f;
				ma->sss_radius[1]= 1.0f;
				ma->sss_radius[2]= 1.0f;
				ma->sss_col[0]= 0.8f;
				ma->sss_col[1]= 0.8f;
				ma->sss_col[2]= 0.8f;
				ma->sss_error= 0.05f;
				ma->sss_scale= 0.1f;
				ma->sss_ior= 1.3f;
				ma->sss_colfac= 1.0f;
				ma->sss_texfac= 0.0f;
			}
			if (ma->sss_front==0 && ma->sss_back==0) {
				ma->sss_front= 1.0f;
				ma->sss_back= 1.0f;
			}
			if (ma->sss_col[0]==0 && ma->sss_col[1]==0 && ma->sss_col[2]==0) {
				ma->sss_col[0]= ma->r;
				ma->sss_col[1]= ma->g;
				ma->sss_col[2]= ma->b;
			}
		}
		
		for (; ob; ob= ob->id.next) {
			bDeformGroup *curdef;
			
			for (curdef= ob->defbase.first; curdef; curdef=curdef->next) {
				/* replace an empty-string name with unique name */
				if (curdef->name[0] == '\0') {
					defgroup_unique_name(curdef, ob);
				}
			}

			if (main->versionfile < 243 || main->subversionfile < 1) {
				ModifierData *md;

				/* translate old mirror modifier axis values to new flags */
				for (md=ob->modifiers.first; md; md=md->next) {
					if (md->type==eModifierType_Mirror) {
						MirrorModifierData *mmd = (MirrorModifierData*) md;

						switch(mmd->axis)
						{
						case 0:
							mmd->flag |= MOD_MIR_AXIS_X;
							break;
						case 1:
							mmd->flag |= MOD_MIR_AXIS_Y;
							break;
						case 2:
							mmd->flag |= MOD_MIR_AXIS_Z;
							break;
						}

						mmd->axis = 0;
					}
				}
			}
		}
		
		/* render layer added, this is not the active layer */
		if (main->versionfile <= 243 || main->subversionfile < 2) {
			Mesh *me;
			for (me=main->mesh.first; me; me=me->id.next)
				customdata_version_243(me);
		}		

	}
	
	if (main->versionfile <= 244) {
		Scene *sce;
		bScreen *sc;
		Lamp *la;
		World *wrld;
		
		if (main->versionfile != 244 || main->subversionfile < 2) {
			for (sce= main->scene.first; sce; sce= sce->id.next)
				sce->r.mode |= R_SSS;

			/* correct older action editors - incorrect scrolling */
			for (sc= main->screen.first; sc; sc= sc->id.next) {
				ScrArea *sa;
				sa= sc->areabase.first;
				while (sa) {
					SpaceLink *sl;

					for (sl= sa->spacedata.first; sl; sl= sl->next) {
						if (sl->spacetype==SPACE_ACTION) {
							SpaceAction *saction= (SpaceAction*) sl;
							
							saction->v2d.tot.ymin = -1000.0;
							saction->v2d.tot.ymax = 0.0;
							
							saction->v2d.cur.ymin = -75.0;
							saction->v2d.cur.ymax = 5.0;
						}
					}
					sa = sa->next;
				}
			}
		}
		if (main->versionfile != 244 || main->subversionfile < 3) {	
			/* constraints recode version patch used to be here. Moved to 245 now... */
			
			
			for (wrld=main->world.first; wrld; wrld= wrld->id.next) {
				if (wrld->mode & WO_AMB_OCC)
					wrld->ao_samp_method = WO_AOSAMP_CONSTANT;
				else
					wrld->ao_samp_method = WO_AOSAMP_HAMMERSLEY;
				
				wrld->ao_adapt_thresh = 0.005f;
			}
			
			for (la=main->lamp.first; la; la= la->id.next) {
				if (la->type == LA_AREA)
					la->ray_samp_method = LA_SAMP_CONSTANT;
				else
					la->ray_samp_method = LA_SAMP_HALTON;
				
				la->adapt_thresh = 0.001f;
			}
		}
	}
	if (main->versionfile <= 245) {
		Scene *sce;
		Object *ob;
		Image *ima;
		Lamp *la;
		Material *ma;
		ParticleSettings *part;
		World *wrld;
		Mesh *me;
		bNodeTree *ntree;
		Tex *tex;
		ModifierData *md;
		ParticleSystem *psys;
		
		/* unless the file was created 2.44.3 but not 2.45, update the constraints */
		if ( !(main->versionfile==244 && main->subversionfile==3) &&
			 ((main->versionfile<245) || (main->versionfile==245 && main->subversionfile==0)) ) 
		{
			for (ob = main->object.first; ob; ob= ob->id.next) {
				ListBase *list;
				list = &ob->constraints;
				
				/* fix up constraints due to constraint recode changes (originally at 2.44.3) */
				if (list) {
					bConstraint *curcon;
					for (curcon = list->first; curcon; curcon=curcon->next) {
						/* old CONSTRAINT_LOCAL check -> convert to CONSTRAINT_SPACE_LOCAL */
						if (curcon->flag & 0x20) {
							curcon->ownspace = CONSTRAINT_SPACE_LOCAL;
							curcon->tarspace = CONSTRAINT_SPACE_LOCAL;
						}
						
						switch (curcon->type) {
							case CONSTRAINT_TYPE_LOCLIMIT:
							{
								bLocLimitConstraint *data= (bLocLimitConstraint *)curcon->data;
								
								/* old limit without parent option for objects */
								if (data->flag2)
									curcon->ownspace = CONSTRAINT_SPACE_LOCAL;
							}
								break;
						}	
					}
				}
				
				/* correctly initialize constinv matrix */
				unit_m4(ob->constinv);
				
				if (ob->type == OB_ARMATURE) {
					if (ob->pose) {
						bConstraint *curcon;
						bPoseChannel *pchan;
						
						for (pchan = ob->pose->chanbase.first; pchan; pchan=pchan->next) {
							/* make sure constraints are all up to date */
							for (curcon = pchan->constraints.first; curcon; curcon=curcon->next) {
								/* old CONSTRAINT_LOCAL check -> convert to CONSTRAINT_SPACE_LOCAL */
								if (curcon->flag & 0x20) {
									curcon->ownspace = CONSTRAINT_SPACE_LOCAL;
									curcon->tarspace = CONSTRAINT_SPACE_LOCAL;
								}
								
								switch (curcon->type) {
									case CONSTRAINT_TYPE_ACTION:
									{
										bActionConstraint *data= (bActionConstraint *)curcon->data;
										
										/* 'data->local' used to mean that target was in local-space */
										if (data->local)
											curcon->tarspace = CONSTRAINT_SPACE_LOCAL;
									}							
										break;
								}
							}
							
							/* correctly initialize constinv matrix */
							unit_m4(pchan->constinv);
						}
					}
				}
			}
		}
		
		/* fix all versions before 2.45 */
		if (main->versionfile != 245) {

			/* repair preview from 242 - 244*/
			for (ima= main->image.first; ima; ima= ima->id.next) {
				ima->preview = NULL;
			}
		}

		/* add point caches */
		for (ob=main->object.first; ob; ob=ob->id.next) {
			if (ob->soft && !ob->soft->pointcache)
				ob->soft->pointcache= BKE_ptcache_add(&ob->soft->ptcaches);

			for (psys=ob->particlesystem.first; psys; psys=psys->next) {
				if (psys->pointcache) {
					if (psys->pointcache->flag & PTCACHE_BAKED && (psys->pointcache->flag & PTCACHE_DISK_CACHE)==0) {
						printf("Old memory cache isn't supported for particles, so re-bake the simulation!\n");
						psys->pointcache->flag &= ~PTCACHE_BAKED;
					}
				}
				else
					psys->pointcache= BKE_ptcache_add(&psys->ptcaches);
			}

			for (md=ob->modifiers.first; md; md=md->next) {
				if (md->type==eModifierType_Cloth) {
					ClothModifierData *clmd = (ClothModifierData*) md;
					if (!clmd->point_cache)
						clmd->point_cache= BKE_ptcache_add(&clmd->ptcaches);
				}
			}
		}

		/* Copy over old per-level multires vertex data
		   into a single vertex array in struct Multires */
		for (me = main->mesh.first; me; me=me->id.next) {
			if (me->mr && !me->mr->verts) {
				MultiresLevel *lvl = me->mr->levels.last;
				if (lvl) {
					me->mr->verts = lvl->verts;
					lvl->verts = NULL;
					/* Don't need the other vert arrays */
					for (lvl = lvl->prev; lvl; lvl = lvl->prev) {
						MEM_freeN(lvl->verts);
						lvl->verts = NULL;
					}
				}
			}
		}
		
		if (main->versionfile != 245 || main->subversionfile < 1) {
			for (la=main->lamp.first; la; la= la->id.next) {
				if (la->mode & LA_QUAD) la->falloff_type = LA_FALLOFF_SLIDERS;
				else la->falloff_type = LA_FALLOFF_INVLINEAR;
				
				if (la->curfalloff == NULL) {
					la->curfalloff = curvemapping_add(1, 0.0f, 1.0f, 1.0f, 0.0f);
					curvemapping_initialize(la->curfalloff);
				}
			}
		}		
		
		for (ma=main->mat.first; ma; ma= ma->id.next) {
			if (ma->samp_gloss_mir == 0) {
				ma->gloss_mir = ma->gloss_tra= 1.0f;
				ma->aniso_gloss_mir = 1.0f;
				ma->samp_gloss_mir = ma->samp_gloss_tra= 18;
				ma->adapt_thresh_mir = ma->adapt_thresh_tra = 0.005f;
				ma->dist_mir = 0.0f;
				ma->fadeto_mir = MA_RAYMIR_FADETOSKY;
			}

			if (ma->strand_min == 0.0f)
				ma->strand_min= 1.0f;
		}

		for (part=main->particle.first; part; part=part->id.next) {
			if (part->ren_child_nbr==0)
				part->ren_child_nbr= part->child_nbr;

			if (part->simplify_refsize==0) {
				part->simplify_refsize= 1920;
				part->simplify_rate= 1.0f;
				part->simplify_transition= 0.1f;
				part->simplify_viewport= 0.8f;
			}
		}

		for (wrld=main->world.first; wrld; wrld= wrld->id.next) {
			if (wrld->ao_approx_error == 0.0f)
				wrld->ao_approx_error= 0.25f;
		}

		for (sce= main->scene.first; sce; sce= sce->id.next) {
			if (sce->nodetree)
				ntree_version_245(fd, lib, sce->nodetree);

			if (sce->r.simplify_shadowsamples == 0) {
				sce->r.simplify_subsurf= 6;
				sce->r.simplify_particles= 1.0f;
				sce->r.simplify_shadowsamples= 16;
				sce->r.simplify_aosss= 1.0f;
			}

			if (sce->r.cineongamma == 0) {
				sce->r.cineonblack= 95;
				sce->r.cineonwhite= 685;
				sce->r.cineongamma= 1.7f;
			}
		}

		for (ntree=main->nodetree.first; ntree; ntree= ntree->id.next)
			ntree_version_245(fd, lib, ntree);

		/* fix for temporary flag changes during 245 cycle */
		for (ima= main->image.first; ima; ima= ima->id.next) {
			if (ima->flag & IMA_OLD_PREMUL) {
				ima->flag &= ~IMA_OLD_PREMUL;
				ima->flag |= IMA_DO_PREMUL;
			}
		}

		for (tex=main->tex.first; tex; tex=tex->id.next) {
			if (tex->iuser.flag & IMA_OLD_PREMUL) {
				tex->iuser.flag &= ~IMA_OLD_PREMUL;
				tex->iuser.flag |= IMA_DO_PREMUL;

			}

			ima= newlibadr(fd, lib, tex->ima);
			if (ima && (tex->iuser.flag & IMA_DO_PREMUL)) {
				ima->flag &= ~IMA_OLD_PREMUL;
				ima->flag |= IMA_DO_PREMUL;
			}
		}
	}
	
	/* sanity check for skgen
	 * */
	{
		Scene *sce;
		for (sce=main->scene.first; sce; sce = sce->id.next)
		{
			if (sce->toolsettings->skgen_subdivisions[0] == sce->toolsettings->skgen_subdivisions[1] ||
				sce->toolsettings->skgen_subdivisions[0] == sce->toolsettings->skgen_subdivisions[2] ||
				sce->toolsettings->skgen_subdivisions[1] == sce->toolsettings->skgen_subdivisions[2])
			{
					sce->toolsettings->skgen_subdivisions[0] = SKGEN_SUB_CORRELATION;
					sce->toolsettings->skgen_subdivisions[1] = SKGEN_SUB_LENGTH;
					sce->toolsettings->skgen_subdivisions[2] = SKGEN_SUB_ANGLE;
			}
		}
	}
	

	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 2)) {
		Image *ima;

		/* initialize 1:1 Aspect */
		for (ima= main->image.first; ima; ima= ima->id.next) {
			ima->aspx = ima->aspy = 1.0f;				
		}

	}

	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 4)) {
		bArmature *arm;
		ModifierData *md;
		Object *ob;
		
		for (arm= main->armature.first; arm; arm= arm->id.next)
			arm->deformflag |= ARM_DEF_B_BONE_REST;
		
		for (ob = main->object.first; ob; ob= ob->id.next) {
			for (md=ob->modifiers.first; md; md=md->next) {
				if (md->type==eModifierType_Armature)
					((ArmatureModifierData*)md)->deformflag |= ARM_DEF_B_BONE_REST;
			}
		}
	}

	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 5)) {
		/* foreground color needs to be something other then black */
		Scene *sce;
		for (sce= main->scene.first; sce; sce=sce->id.next) {
			sce->r.fg_stamp[0] = sce->r.fg_stamp[1] = sce->r.fg_stamp[2] = 0.8f;
			sce->r.fg_stamp[3] = 1.0f; /* don't use text alpha yet */
			sce->r.bg_stamp[3] = 0.25f; /* make sure the background has full alpha */
		}
	}

	
	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 6)) {
		Scene *sce;
		/* fix frs_sec_base */
		for (sce= main->scene.first; sce; sce= sce->id.next) {
			if (sce->r.frs_sec_base == 0) {
				sce->r.frs_sec_base = 1;
			}
		}
	}
	
	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 7)) {
		Object *ob;
		bPoseChannel *pchan;
		bConstraint *con;
		bConstraintTarget *ct;
		
		for (ob = main->object.first; ob; ob= ob->id.next) {
			if (ob->pose) {
				for (pchan=ob->pose->chanbase.first; pchan; pchan=pchan->next) {
					for (con=pchan->constraints.first; con; con=con->next) {
						if (con->type == CONSTRAINT_TYPE_PYTHON) {
							bPythonConstraint *data= (bPythonConstraint *)con->data;
							if (data->tar) {
								/* version patching needs to be done */
								ct= MEM_callocN(sizeof(bConstraintTarget), "PyConTarget");
								
								ct->tar = data->tar;
								BLI_strncpy(ct->subtarget, data->subtarget, sizeof(ct->subtarget));
								ct->space = con->tarspace;
								
								BLI_addtail(&data->targets, ct);
								data->tarnum++;
								
								/* clear old targets to avoid problems */
								data->tar = NULL;
								data->subtarget[0]= '\0';
							}
						}
						else if (con->type == CONSTRAINT_TYPE_LOCLIKE) {
							bLocateLikeConstraint *data= (bLocateLikeConstraint *)con->data;
							
							/* new headtail functionality makes Bone-Tip function obsolete */
							if (data->flag & LOCLIKE_TIP)
								con->headtail = 1.0f;
						}
					}
				}
			}
			
			for (con=ob->constraints.first; con; con=con->next) {
				if (con->type==CONSTRAINT_TYPE_PYTHON) {
					bPythonConstraint *data= (bPythonConstraint *)con->data;
					if (data->tar) {
						/* version patching needs to be done */
						ct= MEM_callocN(sizeof(bConstraintTarget), "PyConTarget");
						
						ct->tar = data->tar;
						BLI_strncpy(ct->subtarget, data->subtarget, sizeof(ct->subtarget));
						ct->space = con->tarspace;
						
						BLI_addtail(&data->targets, ct);
						data->tarnum++;
						
						/* clear old targets to avoid problems */
						data->tar = NULL;
						data->subtarget[0]= '\0';
					}
				}
				else if (con->type == CONSTRAINT_TYPE_LOCLIKE) {
					bLocateLikeConstraint *data= (bLocateLikeConstraint *)con->data;
					
					/* new headtail functionality makes Bone-Tip function obsolete */
					if (data->flag & LOCLIKE_TIP)
						con->headtail = 1.0f;
				}
			}

			if (ob->soft && ob->soft->keys) {
				SoftBody *sb = ob->soft;
				int k;

				for (k=0; k<sb->totkey; k++) {
					if (sb->keys[k])
						MEM_freeN(sb->keys[k]);
				}

				MEM_freeN(sb->keys);

				sb->keys = NULL;
				sb->totkey = 0;
			}
		}
	}

	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 8)) {
		Scene *sce;
		Object *ob;
		PartEff *paf=NULL;

		for (ob = main->object.first; ob; ob= ob->id.next) {
			if (ob->soft && ob->soft->keys) {
				SoftBody *sb = ob->soft;
				int k;

				for (k=0; k<sb->totkey; k++) {
					if (sb->keys[k])
						MEM_freeN(sb->keys[k]);
				}

				MEM_freeN(sb->keys);

				sb->keys = NULL;
				sb->totkey = 0;
			}

			/* convert old particles to new system */
			if ((paf = do_version_give_parteff_245(ob))) {
				ParticleSystem *psys;
				ModifierData *md;
				ParticleSystemModifierData *psmd;
				ParticleSettings *part;

				/* create new particle system */
				psys = MEM_callocN(sizeof(ParticleSystem), "particle_system");
				psys->pointcache = BKE_ptcache_add(&psys->ptcaches);

				part = psys->part = psys_new_settings("ParticleSettings", main);
				
				/* needed for proper libdata lookup */
				oldnewmap_insert(fd->libmap, psys->part, psys->part, 0);
				part->id.lib= ob->id.lib;

				part->id.us--;
				part->id.flag |= (ob->id.flag & LIB_NEEDLINK);
				
				psys->totpart=0;
				psys->flag= PSYS_ENABLED|PSYS_CURRENT;

				BLI_addtail(&ob->particlesystem, psys);

				md= modifier_new(eModifierType_ParticleSystem);
				BLI_snprintf(md->name, sizeof(md->name), "ParticleSystem %i", BLI_countlist(&ob->particlesystem));
				psmd= (ParticleSystemModifierData*) md;
				psmd->psys=psys;
				BLI_addtail(&ob->modifiers, md);

				/* convert settings from old particle system */
				/* general settings */
				part->totpart = MIN2(paf->totpart, 100000);
				part->sta = paf->sta;
				part->end = paf->end;
				part->lifetime = paf->lifetime;
				part->randlife = paf->randlife;
				psys->seed = paf->seed;
				part->disp = paf->disp;
				part->omat = paf->mat[0];
				part->hair_step = paf->totkey;

				part->eff_group = paf->group;

				/* old system didn't interpolate between keypoints at render time */
				part->draw_step = part->ren_step = 0;

				/* physics */
				part->normfac = paf->normfac * 25.0f;
				part->obfac = paf->obfac;
				part->randfac = paf->randfac * 25.0f;
				part->dampfac = paf->damp;
				copy_v3_v3(part->acc, paf->force);

				/* flags */
				if (paf->stype & PAF_VECT) {
					if (paf->flag & PAF_STATIC) {
						/* new hair lifetime is always 100.0f */
						float fac = paf->lifetime / 100.0f;

						part->draw_as = PART_DRAW_PATH;
						part->type = PART_HAIR;
						psys->recalc |= PSYS_RECALC_REDO;

						part->normfac *= fac;
						part->randfac *= fac;
					}
					else {
						part->draw_as = PART_DRAW_LINE;
						part->draw |= PART_DRAW_VEL_LENGTH;
						part->draw_line[1] = 0.04f;
					}
				}

				part->rotmode = PART_ROT_VEL;
				
				part->flag |= (paf->flag & PAF_BSPLINE) ? PART_HAIR_BSPLINE : 0;
				part->flag |= (paf->flag & PAF_TRAND) ? PART_TRAND : 0;
				part->flag |= (paf->flag & PAF_EDISTR) ? PART_EDISTR : 0;
				part->flag |= (paf->flag & PAF_UNBORN) ? PART_UNBORN : 0;
				part->flag |= (paf->flag & PAF_DIED) ? PART_DIED : 0;
				part->from |= (paf->flag & PAF_FACE) ? PART_FROM_FACE : 0;
				part->draw |= (paf->flag & PAF_SHOWE) ? PART_DRAW_EMITTER : 0;

				psys->vgroup[PSYS_VG_DENSITY] = paf->vertgroup;
				psys->vgroup[PSYS_VG_VEL] = paf->vertgroup_v;
				psys->vgroup[PSYS_VG_LENGTH] = paf->vertgroup_v;

				/* dupliobjects */
				if (ob->transflag & OB_DUPLIVERTS) {
					Object *dup = main->object.first;

					for (; dup; dup= dup->id.next) {
						if (ob == newlibadr(fd, lib, dup->parent)) {
							part->dup_ob = dup;
							ob->transflag |= OB_DUPLIPARTS;
							ob->transflag &= ~OB_DUPLIVERTS;

							part->draw_as = PART_DRAW_OB;

							/* needed for proper libdata lookup */
							oldnewmap_insert(fd->libmap, dup, dup, 0);
						}
					}
				}

				
				{
					FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(ob, eModifierType_Fluidsim);
					if (fluidmd && fluidmd->fss && fluidmd->fss->type == OB_FLUIDSIM_PARTICLE)
						part->type = PART_FLUID;
				}

				do_version_free_effects_245(&ob->effect);

				printf("Old particle system converted to new system.\n");
			}
		}

		for (sce= main->scene.first; sce; sce=sce->id.next) {
			ParticleEditSettings *pset= &sce->toolsettings->particle;
			int a;

			if (pset->brush[0].size == 0) {
				pset->flag= PE_KEEP_LENGTHS|PE_LOCK_FIRST|PE_DEFLECT_EMITTER;
				pset->emitterdist= 0.25f;
				pset->totrekey= 5;
				pset->totaddkey= 5;
				pset->brushtype= PE_BRUSH_NONE;

				for (a=0; a<PE_TOT_BRUSH; a++) {
					pset->brush[a].strength= 50;
					pset->brush[a].size= 50;
					pset->brush[a].step= 10;
				}

				pset->brush[PE_BRUSH_CUT].strength= 100;
			}
		}
	}
	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 9)) {
		Material *ma;
		int a;

		for (ma=main->mat.first; ma; ma= ma->id.next)
			if (ma->mode & MA_NORMAP_TANG)
				for (a=0; a<MAX_MTEX; a++)
					if (ma->mtex[a] && ma->mtex[a]->tex)
						ma->mtex[a]->normapspace = MTEX_NSPACE_TANGENT;
	}
	
	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 10)) {
		Object *ob;
		
		/* dupliface scale */
		for (ob= main->object.first; ob; ob= ob->id.next)
			ob->dupfacesca = 1.0f;
	}
	
	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 11)) {
		Object *ob;
		bActionStrip *strip;
		
		/* nla-strips - scale */		
		for (ob= main->object.first; ob; ob= ob->id.next) {
			for (strip= ob->nlastrips.first; strip; strip= strip->next) {
				float length, actlength, repeat;
				
				if (strip->flag & ACTSTRIP_USESTRIDE)
					repeat= 1.0f;
				else
					repeat= strip->repeat;
				
				length = strip->end-strip->start;
				if (length == 0.0f) length= 1.0f;
				actlength = strip->actend-strip->actstart;
				
				strip->scale = length / (repeat * actlength);
				if (strip->scale == 0.0f) strip->scale= 1.0f;
			}	
			if (ob->soft) {
				ob->soft->inpush =  ob->soft->inspring;
				ob->soft->shearstiff = 1.0f; 
			}
		}
	}

	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 14)) {
		Scene *sce;
		Sequence *seq;
		
		for (sce=main->scene.first; sce; sce=sce->id.next) {
			SEQ_BEGIN(sce->ed, seq) {
				if (seq->blend_mode == 0)
					seq->blend_opacity = 100.0f;
			}
			SEQ_END
		}
	}
	
	/*fix broken group lengths in id properties*/
	if ((main->versionfile < 245) || (main->versionfile == 245 && main->subversionfile < 15)) {
		idproperties_fix_group_lengths(main->scene);
		idproperties_fix_group_lengths(main->library);
		idproperties_fix_group_lengths(main->object);
		idproperties_fix_group_lengths(main->mesh);
		idproperties_fix_group_lengths(main->curve);
		idproperties_fix_group_lengths(main->mball);
		idproperties_fix_group_lengths(main->mat);
		idproperties_fix_group_lengths(main->tex);
		idproperties_fix_group_lengths(main->image);
		idproperties_fix_group_lengths(main->latt);
		idproperties_fix_group_lengths(main->lamp);
		idproperties_fix_group_lengths(main->camera);
		idproperties_fix_group_lengths(main->ipo);
		idproperties_fix_group_lengths(main->key);
		idproperties_fix_group_lengths(main->world);
		idproperties_fix_group_lengths(main->screen);
		idproperties_fix_group_lengths(main->script);
		idproperties_fix_group_lengths(main->vfont);
		idproperties_fix_group_lengths(main->text);
		idproperties_fix_group_lengths(main->sound);
		idproperties_fix_group_lengths(main->group);
		idproperties_fix_group_lengths(main->armature);
		idproperties_fix_group_lengths(main->action);
		idproperties_fix_group_lengths(main->nodetree);
		idproperties_fix_group_lengths(main->brush);
		idproperties_fix_group_lengths(main->particle);		
	}

	/* sun/sky */
	if (main->versionfile < 246) {
		Object *ob;
		bActuator *act;

		/* dRot actuator change direction in 2.46 */
		for (ob = main->object.first; ob; ob= ob->id.next) {
			for (act= ob->actuators.first; act; act= act->next) {
				if (act->type == ACT_OBJECT) {
					bObjectActuator *ba= act->data;

					ba->drot[0] = -ba->drot[0];
					ba->drot[1] = -ba->drot[1];
					ba->drot[2] = -ba->drot[2];
				}
			}
		}
	}
	
	// convert fluids to modifier
	if (main->versionfile < 246 || (main->versionfile == 246 && main->subversionfile < 1))
	{
		Object *ob;
		
		for (ob = main->object.first; ob; ob= ob->id.next) {
			if (ob->fluidsimSettings)
			{
				FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifier_new(eModifierType_Fluidsim);
				BLI_addhead(&ob->modifiers, (ModifierData *)fluidmd);
				
				MEM_freeN(fluidmd->fss);
				fluidmd->fss = MEM_dupallocN(ob->fluidsimSettings);
				fluidmd->fss->ipo = newlibadr_us(fd, ob->id.lib, ob->fluidsimSettings->ipo);
				MEM_freeN(ob->fluidsimSettings);
				
				fluidmd->fss->lastgoodframe = INT_MAX;
				fluidmd->fss->flag = 0;
				fluidmd->fss->meshVelocities = NULL;
			}
		}
	}
	

	if (main->versionfile < 246 || (main->versionfile == 246 && main->subversionfile < 1)) {
		Mesh *me;

		for (me=main->mesh.first; me; me= me->id.next)
			alphasort_version_246(fd, lib, me);
	}
	
	if (main->versionfile < 246 || (main->versionfile == 246 && main->subversionfile < 1)) {
		Object *ob;
		for (ob = main->object.first; ob; ob= ob->id.next) {
			if (ob->pd && (ob->pd->forcefield == PFIELD_WIND))
				ob->pd->f_noise = 0.0f;
		}
	}

	if (main->versionfile < 247 || (main->versionfile == 247 && main->subversionfile < 2)) {
		Object *ob;
		for (ob = main->object.first; ob; ob= ob->id.next) {
			ob->gameflag |= OB_COLLISION;
			ob->margin = 0.06f;
		}
	}

	if (main->versionfile < 247 || (main->versionfile == 247 && main->subversionfile < 3)) {
		Object *ob;
		for (ob = main->object.first; ob; ob= ob->id.next) {
			// Starting from subversion 3, ACTOR is a separate feature.
			// Before it was conditioning all the other dynamic flags
			if (!(ob->gameflag & OB_ACTOR))
				ob->gameflag &= ~(OB_GHOST|OB_DYNAMIC|OB_RIGID_BODY|OB_SOFT_BODY|OB_COLLISION_RESPONSE);
			/* suitable default for older files */
		}
	}

	if (main->versionfile < 247 || (main->versionfile == 247 && main->subversionfile < 5)) {
		Lamp *la= main->lamp.first;
		for (; la; la= la->id.next) {
			la->skyblendtype= MA_RAMP_ADD;
			la->skyblendfac= 1.0f;
		}
	}
	
	/* set the curve radius interpolation to 2.47 default - easy */
	if (main->versionfile < 247 || (main->versionfile == 247 && main->subversionfile < 6)) {
		Curve *cu;
		Nurb *nu;
		
		for (cu= main->curve.first; cu; cu= cu->id.next) {
			for (nu= cu->nurb.first; nu; nu= nu->next) {
				if (nu) {
					nu->radius_interp = 3;
					
					/* resolu and resolv are now used differently for surfaces
					 * rather than using the resolution to define the entire number of divisions,
					 * use it for the number of divisions per segment
					 */
					if (nu->pntsv > 1) {
						nu->resolu = MAX2( 1, (int)(((float)nu->resolu / (float)nu->pntsu)+0.5f) );
						nu->resolv = MAX2( 1, (int)(((float)nu->resolv / (float)nu->pntsv)+0.5f) );
					}
				}
			}
		}
	}
	/* direction constraint actuators were always local in previous version */
	if (main->versionfile < 247 || (main->versionfile == 247 && main->subversionfile < 7)) {
		bActuator *act;
		Object *ob;
		
		for (ob = main->object.first; ob; ob= ob->id.next) {
			for (act= ob->actuators.first; act; act= act->next) {
				if (act->type == ACT_CONSTRAINT) {
					bConstraintActuator *coa = act->data;
					if (coa->type == ACT_CONST_TYPE_DIST) {
						coa->flag |= ACT_CONST_LOCAL;
					}
				}
			}
		}
	}

	if (main->versionfile < 247 || (main->versionfile == 247 && main->subversionfile < 9)) {
		Lamp *la= main->lamp.first;
		for (; la; la= la->id.next) {
			la->sky_exposure= 1.0f;
		}
	}
	
	/* BGE message actuators needed OB prefix, very confusing */
	if (main->versionfile < 247 || (main->versionfile == 247 && main->subversionfile < 10)) {
		bActuator *act;
		Object *ob;
		
		for (ob = main->object.first; ob; ob= ob->id.next) {
			for (act= ob->actuators.first; act; act= act->next) {
				if (act->type == ACT_MESSAGE) {
					bMessageActuator *msgAct = (bMessageActuator *) act->data;
					if (BLI_strnlen(msgAct->toPropName, 3) > 2) {
						/* strip first 2 chars, would have only worked if these were OB anyway */
						memmove(msgAct->toPropName, msgAct->toPropName + 2, sizeof(msgAct->toPropName) - 2);
					}
					else {
						msgAct->toPropName[0] = '\0';
					}
				}
			}
		}
	}

	if (main->versionfile < 248) {
		Lamp *la;

		for (la=main->lamp.first; la; la= la->id.next) {
			if (la->atm_turbidity == 0.0f) {
				la->sun_effect_type = 0;
				la->horizon_brightness = 1.0f;
				la->spread = 1.0f;
				la->sun_brightness = 1.0f;
				la->sun_size = 1.0f;
				la->backscattered_light = 1.0f;
				la->atm_turbidity = 2.0f;
				la->atm_inscattering_factor = 1.0f;
				la->atm_extinction_factor = 1.0f;
				la->atm_distance_factor = 1.0f;
				la->sun_intensity = 1.0f;
			}
		}
	}

	if (main->versionfile < 248 || (main->versionfile == 248 && main->subversionfile < 2)) {
		Scene *sce;
		
		/* Note, these will need to be added for painting */
		for (sce= main->scene.first; sce; sce= sce->id.next) {
			sce->toolsettings->imapaint.seam_bleed = 2;
			sce->toolsettings->imapaint.normal_angle = 80;

			/* initialize skeleton generation toolsettings */
			sce->toolsettings->skgen_resolution = 250;
			sce->toolsettings->skgen_threshold_internal 	= 0.1f;
			sce->toolsettings->skgen_threshold_external 	= 0.1f;
			sce->toolsettings->skgen_angle_limit			= 30.0f;
			sce->toolsettings->skgen_length_ratio			= 1.3f;
			sce->toolsettings->skgen_length_limit			= 1.5f;
			sce->toolsettings->skgen_correlation_limit		= 0.98f;
			sce->toolsettings->skgen_symmetry_limit			= 0.1f;
			sce->toolsettings->skgen_postpro = SKGEN_SMOOTH;
			sce->toolsettings->skgen_postpro_passes = 3;
			sce->toolsettings->skgen_options = SKGEN_FILTER_INTERNAL|SKGEN_FILTER_EXTERNAL|SKGEN_FILTER_SMART|SKGEN_SUB_CORRELATION|SKGEN_HARMONIC;
			sce->toolsettings->skgen_subdivisions[0] = SKGEN_SUB_CORRELATION;
			sce->toolsettings->skgen_subdivisions[1] = SKGEN_SUB_LENGTH;
			sce->toolsettings->skgen_subdivisions[2] = SKGEN_SUB_ANGLE;

			
			sce->toolsettings->skgen_retarget_angle_weight = 1.0f;
			sce->toolsettings->skgen_retarget_length_weight = 1.0f;
			sce->toolsettings->skgen_retarget_distance_weight = 1.0f;
	
			/* Skeleton Sketching */
			sce->toolsettings->bone_sketching = 0;
			sce->toolsettings->skgen_retarget_roll = SK_RETARGET_ROLL_VIEW;
		}
	}
	if (main->versionfile < 248 || (main->versionfile == 248 && main->subversionfile < 3)) {
		bScreen *sc;
		
		/* adjust default settings for Animation Editors */
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			
			for (sa= sc->areabase.first; sa; sa= sa->next) { 
				SpaceLink *sl;
				
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					switch (sl->spacetype) {
						case SPACE_ACTION:
						{
							SpaceAction *sact= (SpaceAction *)sl;
							
							sact->mode= SACTCONT_DOPESHEET;
							sact->autosnap= SACTSNAP_FRAME;
						}
							break;
						case SPACE_IPO:
						{
							SpaceIpo *sipo= (SpaceIpo *)sl;
							sipo->autosnap= SACTSNAP_FRAME;
						}
							break;
						case SPACE_NLA:
						{
							SpaceNla *snla= (SpaceNla *)sl;
							snla->autosnap= SACTSNAP_FRAME;
						}
							break;
					}
				}
			}
		}
	}

	if (main->versionfile < 248 || (main->versionfile == 248 && main->subversionfile < 3)) {
		Object *ob;

		/* Adjustments needed after Bullets update */
		for (ob = main->object.first; ob; ob= ob->id.next) {
			ob->damping *= 0.635f;
			ob->rdamping = 0.1f + (0.8f * ob->rdamping);
		}
	}
	
	if (main->versionfile < 248 || (main->versionfile == 248 && main->subversionfile < 4)) {
		Scene *sce;
		World *wrld;

		/*  Dome (Fisheye) default parameters  */
		for (sce= main->scene.first; sce; sce= sce->id.next) {
			sce->r.domeangle = 180;
			sce->r.domemode = 1;
			sce->r.domeres = 4;
			sce->r.domeresbuf = 1.0f;
			sce->r.dometilt = 0;
		}
		/* DBVT culling by default */
		for (wrld=main->world.first; wrld; wrld= wrld->id.next) {
			wrld->mode |= WO_DBVT_CULLING;
			wrld->occlusionRes = 128;
		}
	}

	if (main->versionfile < 248 || (main->versionfile == 248 && main->subversionfile < 5)) {
		Object *ob;
		World *wrld;
		for (ob = main->object.first; ob; ob= ob->id.next) {
			ob->m_contactProcessingThreshold = 1.0f; //pad3 is used for m_contactProcessingThreshold
			if (ob->parent) {
				/* check if top parent has compound shape set and if yes, set this object
				   to compound shaper as well (was the behavior before, now it's optional) */
				Object *parent= newlibadr(fd, lib, ob->parent);
				while (parent && parent != ob &&  parent->parent != NULL) {
					parent = newlibadr(fd, lib, parent->parent);
				}
				if (parent) {
					if (parent->gameflag & OB_CHILD)
						ob->gameflag |= OB_CHILD;
				}
			}
		}
		for (wrld=main->world.first; wrld; wrld= wrld->id.next) {
			wrld->ticrate = 60;
			wrld->maxlogicstep = 5;
			wrld->physubstep = 1;
			wrld->maxphystep = 5;
		}
	}
	
	// correct introduce of seed for wind force
	if (main->versionfile < 249 && main->subversionfile < 1) {
		Object *ob;
		for (ob = main->object.first; ob; ob= ob->id.next) {
			if (ob->pd)
				ob->pd->seed = ((unsigned int)(ceil(PIL_check_seconds_timer()))+1) % 128;
		}
	
	}

	if (main->versionfile < 249 && main->subversionfile < 2) {
		Scene *sce= main->scene.first;
		Sequence *seq;
		Editing *ed;
		
		while (sce) {
			ed= sce->ed;
			if (ed) {
				SEQP_BEGIN(ed, seq) {
					if (seq->strip && seq->strip->proxy) {
						seq->strip->proxy->quality =90;
					}
				}
				SEQ_END
			}
			
			sce= sce->id.next;
		}

	}

	if (main->versionfile < 250) {
		bScreen *screen;
		Scene *scene;
		Base *base;
		Material *ma;
		Camera *cam;
		Mesh *me;
		Curve *cu;
		Scene *sce;
		Tex *tx;
		ParticleSettings *part;
		Object *ob;
		//PTCacheID *pid;
		//ListBase pidlist;

		bSound *sound;
		Sequence *seq;
		bActuator *act;
		int a;

		for (sound = main->sound.first; sound; sound = sound->id.next)
		{
			if (sound->newpackedfile)
			{
				sound->packedfile = sound->newpackedfile;
				sound->newpackedfile = NULL;
			}
		}

		for (ob = main->object.first; ob; ob= ob->id.next) {
			for (act= ob->actuators.first; act; act= act->next) {
				if (act->type == ACT_SOUND) {
					bSoundActuator *sAct = (bSoundActuator*) act->data;
					if (sAct->sound)
					{
						sound = newlibadr(fd, lib, sAct->sound);
						sAct->flag = sound->flags & SOUND_FLAGS_3D ? ACT_SND_3D_SOUND : 0;
						sAct->pitch = sound->pitch;
						sAct->volume = sound->volume;
						sAct->sound3D.reference_distance = sound->distance;
						sAct->sound3D.max_gain = sound->max_gain;
						sAct->sound3D.min_gain = sound->min_gain;
						sAct->sound3D.rolloff_factor = sound->attenuation;
					}
					else {
						sAct->sound3D.reference_distance = 1.0f;
						sAct->volume = 1.0f;
						sAct->sound3D.max_gain = 1.0f;
						sAct->sound3D.rolloff_factor = 1.0f;
					}
					sAct->sound3D.cone_inner_angle = 360.0f;
					sAct->sound3D.cone_outer_angle = 360.0f;
					sAct->sound3D.max_distance = FLT_MAX;
				}
			}
		}

		for (scene = main->scene.first; scene; scene = scene->id.next)
		{
			if (scene->ed && scene->ed->seqbasep)
			{
				SEQ_BEGIN(scene->ed, seq) {
					if (seq->type == SEQ_HD_SOUND)
					{
						char str[FILE_MAX];
						BLI_join_dirfile(str, sizeof(str), seq->strip->dir, seq->strip->stripdata->name);
						BLI_path_abs(str, main->name);
						seq->sound = sound_new_file(main, str);
					}
					/* don't know, if anybody used that
					   this way, but just in case, upgrade
					   to new way... */
					if ((seq->flag & SEQ_USE_PROXY_CUSTOM_FILE) &&
					   !(seq->flag & SEQ_USE_PROXY_CUSTOM_DIR))
					{
						
						BLI_snprintf(seq->strip->proxy->dir, 
							 FILE_MAXDIR, "%s/BL_proxy", 
							 seq->strip->dir);
					}
				}
				SEQ_END
			}
		}

		for (screen= main->screen.first; screen; screen= screen->id.next) {
			do_versions_windowmanager_2_50(screen);
			do_versions_gpencil_2_50(main, screen);
		}
		
		/* shader, composite and texture node trees have id.name empty, put something in
		 * to have them show in RNA viewer and accessible otherwise.
		 */
		for (ma= main->mat.first; ma; ma= ma->id.next) {
			if (ma->nodetree && ma->nodetree->id.name[0] == '\0')
				strcpy(ma->nodetree->id.name, "NTShader Nodetree");
			
			/* which_output 0 is now "not specified" */
			for (a=0; a<MAX_MTEX; a++) {
				if (ma->mtex[a]) {
					tx= newlibadr(fd, lib, ma->mtex[a]->tex);
					if (tx && tx->use_nodes)
						ma->mtex[a]->which_output++;
				}
			}
		}
		/* and composite trees */
		for (sce= main->scene.first; sce; sce= sce->id.next) {
			if (sce->nodetree && sce->nodetree->id.name[0] == '\0')
				strcpy(sce->nodetree->id.name, "NTCompositing Nodetree");

			/* move to cameras */
			if (sce->r.mode & R_PANORAMA) {
				for (base=sce->base.first; base; base=base->next) {
					ob= newlibadr(fd, lib, base->object);

					if (ob->type == OB_CAMERA && !ob->id.lib) {
						cam= newlibadr(fd, lib, ob->data);
						cam->flag |= CAM_PANORAMA;
					}
				}

				sce->r.mode &= ~R_PANORAMA;
			}
		}
		/* and texture trees */
		for (tx= main->tex.first; tx; tx= tx->id.next) {
			bNode *node;

			if (tx->nodetree) {
				if (tx->nodetree->id.name[0] == '\0')
					strcpy(tx->nodetree->id.name, "NTTexture Nodetree");

				/* which_output 0 is now "not specified" */
				for (node=tx->nodetree->nodes.first; node; node=node->next)
					if (node->type == TEX_NODE_OUTPUT)
						node->custom1++;
			}
		}
		
		/* copy standard draw flag to meshes(used to be global, is not available here) */
		for (me= main->mesh.first; me; me= me->id.next) {
			me->drawflag= ME_DRAWEDGES|ME_DRAWFACES|ME_DRAWCREASES;
		}

		/* particle draw and render types */
		for (part= main->particle.first; part; part= part->id.next) {
			if (part->draw_as) {
				if (part->draw_as == PART_DRAW_DOT) {
					part->ren_as = PART_DRAW_HALO;
					part->draw_as = PART_DRAW_REND;
				}
				else if (part->draw_as <= PART_DRAW_AXIS) {
					part->ren_as = PART_DRAW_HALO;
				}
				else {
					part->ren_as = part->draw_as;
					part->draw_as = PART_DRAW_REND;
				}
			}
			part->path_end = 1.0f;
			part->clength = 1.0f;
		}
		/* set old pointcaches to have disk cache flag */
		for (ob = main->object.first; ob; ob= ob->id.next) {

			//BKE_ptcache_ids_from_object(&pidlist, ob);

			//for (pid=pidlist.first; pid; pid=pid->next)
			//	pid->cache->flag |= PTCACHE_DISK_CACHE;

			//BLI_freelistN(&pidlist);
		}

		/* type was a mixed flag & enum. move the 2d flag elsewhere */
		for (cu = main->curve.first; cu; cu= cu->id.next) {
			Nurb *nu;

			for (nu= cu->nurb.first; nu; nu= nu->next) {
				nu->flag |= (nu->type & CU_2D);
				nu->type &= CU_TYPE;
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 1)) {
		Object *ob;
		Material *ma;
		Tex *tex;
		Scene *sce;
		ToolSettings *ts;
		//PTCacheID *pid;
		//ListBase pidlist;

		for (ob = main->object.first; ob; ob = ob->id.next) {
			//BKE_ptcache_ids_from_object(&pidlist, ob);

			//for (pid=pidlist.first; pid; pid=pid->next) {
			//	if (pid->ptcaches->first == NULL)
			//		pid->ptcaches->first = pid->ptcaches->last = pid->cache;
			//}

			//BLI_freelistN(&pidlist);

			if (ob->type == OB_MESH) {
				Mesh *me = newlibadr(fd, lib, ob->data);
				void *olddata = ob->data;
				ob->data = me;

				/* XXX - library meshes crash on loading most yoFrankie levels,
				 * the multires pointer gets invalid -  Campbell */
				if (me && me->id.lib==NULL && me->mr && me->mr->level_count > 1) {
					multires_load_old(ob, me);
				}

				ob->data = olddata;
			}

			if (ob->totcol && ob->matbits == NULL) {
				int a;

				ob->matbits= MEM_callocN(sizeof(char)*ob->totcol, "ob->matbits");
				for (a=0; a<ob->totcol; a++)
					ob->matbits[a]= ob->colbits & (1<<a);
			}
		}

		/* texture filter */
		for (tex = main->tex.first; tex; tex = tex->id.next) {
			if (tex->afmax == 0)
				tex->afmax= 8;
		}

		for (ma = main->mat.first; ma; ma = ma->id.next) {
			int a;
			if (ma->mode & MA_WIRE) {
				ma->material_type= MA_TYPE_WIRE;
				ma->mode &= ~MA_WIRE;
			}
			if (ma->mode & MA_HALO) {
				ma->material_type= MA_TYPE_HALO;
				ma->mode &= ~MA_HALO;
			}

			if (ma->mode & (MA_ZTRANSP|MA_RAYTRANSP)) {
				ma->mode |= MA_TRANSP;
			}
			else {
				/* ma->mode |= MA_ZTRANSP; */ /* leave ztransp as is even if its not used [#28113] */
				ma->mode &= ~MA_TRANSP;
			}

			/* set new bump for unused slots */
			for (a=0; a<MAX_MTEX; a++) {
				if (ma->mtex[a]) {
					tex= ma->mtex[a]->tex;
					if (!tex) {
						ma->mtex[a]->texflag |= MTEX_3TAP_BUMP;
						ma->mtex[a]->texflag |= MTEX_BUMP_OBJECTSPACE;
					}
					else {
						tex= (Tex*)newlibadr(fd, ma->id.lib, tex);
						if (tex && tex->type == 0) { /* invalid type */
							ma->mtex[a]->texflag |= MTEX_3TAP_BUMP;
							ma->mtex[a]->texflag |= MTEX_BUMP_OBJECTSPACE;
						}
					}
				}
			}
			
			/* volume rendering settings */
			if (ma->vol.stepsize < 0.0001f) {
				ma->vol.density = 1.0f;
				ma->vol.emission = 0.0f;
				ma->vol.scattering = 1.0f;
				ma->vol.emission_col[0] = ma->vol.emission_col[1] = ma->vol.emission_col[2] = 1.0f;
				ma->vol.density_scale = 1.0f;
				ma->vol.depth_cutoff = 0.01f;
				ma->vol.stepsize_type = MA_VOL_STEP_RANDOMIZED;
				ma->vol.stepsize = 0.2f;
				ma->vol.shade_type = MA_VOL_SHADE_SHADED;
				ma->vol.shadeflag |= MA_VOL_PRECACHESHADING;
				ma->vol.precache_resolution = 50;
			}
		}

		for (sce = main->scene.first; sce; sce = sce->id.next) {
			ts= sce->toolsettings;
			if (ts->normalsize == 0.0f || !ts->uv_selectmode || ts->vgroup_weight == 0.0f) {
				ts->normalsize= 0.1f;
				ts->selectmode= SCE_SELECT_VERTEX;
				
				/* autokeying - setting should be taken from the user-prefs
				 * but the userprefs version may not have correct flags set 
				 * (i.e. will result in blank box when enabled)
				 */
				ts->autokey_mode= U.autokey_mode;
				if (ts->autokey_mode == 0) 
					ts->autokey_mode= 2; /* 'add/replace' but not on */
				ts->uv_selectmode= UV_SELECT_VERTEX;
				ts->vgroup_weight= 1.0f;
			}

			/* Game Settings */
			//Dome
			sce->gm.dome.angle = sce->r.domeangle;
			sce->gm.dome.mode = sce->r.domemode;
			sce->gm.dome.res = sce->r.domeres;
			sce->gm.dome.resbuf = sce->r.domeresbuf;
			sce->gm.dome.tilt = sce->r.dometilt;
			sce->gm.dome.warptext = sce->r.dometext;

			//Stand Alone
			sce->gm.playerflag |= (sce->r.fullscreen?GAME_PLAYER_FULLSCREEN:0);
			sce->gm.xplay = sce->r.xplay;
			sce->gm.yplay = sce->r.yplay;
			sce->gm.freqplay = sce->r.freqplay;
			sce->gm.depth = sce->r.depth;
			sce->gm.attrib = sce->r.attrib;

			//Stereo
			sce->gm.stereomode = sce->r.stereomode;
			/* reassigning stereomode NO_STEREO and DOME to a separeted flag*/
			if (sce->gm.stereomode == 1) { //1 = STEREO_NOSTEREO
				sce->gm.stereoflag = STEREO_NOSTEREO;
				sce->gm.stereomode = STEREO_ANAGLYPH;
			}
			else if (sce->gm.stereomode == 8) { //8 = STEREO_DOME
				sce->gm.stereoflag = STEREO_DOME;
				sce->gm.stereomode = STEREO_ANAGLYPH;
			}
			else
				sce->gm.stereoflag = STEREO_ENABLED;

			//Framing
			sce->gm.framing = sce->framing;
			sce->gm.xplay = sce->r.xplay;
			sce->gm.yplay = sce->r.yplay;
			sce->gm.freqplay= sce->r.freqplay;
			sce->gm.depth= sce->r.depth;

			//Physic (previously stored in world)
			sce->gm.gravity =9.8f;
			sce->gm.physicsEngine= WOPHY_BULLET;// Bullet by default
			sce->gm.mode = WO_DBVT_CULLING;	// DBVT culling by default
			sce->gm.occlusionRes = 128;
			sce->gm.ticrate = 60;
			sce->gm.maxlogicstep = 5;
			sce->gm.physubstep = 1;
			sce->gm.maxphystep = 5;
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 2)) {
		Scene *sce;
		Object *ob;

		for (sce = main->scene.first; sce; sce = sce->id.next) {
			if (fd->fileflags & G_FILE_ENABLE_ALL_FRAMES)
				sce->gm.flag |= GAME_ENABLE_ALL_FRAMES;
			if (fd->fileflags & G_FILE_SHOW_DEBUG_PROPS)
				sce->gm.flag |= GAME_SHOW_DEBUG_PROPS;
			if (fd->fileflags & G_FILE_SHOW_FRAMERATE)
				sce->gm.flag |= GAME_SHOW_FRAMERATE;
			if (fd->fileflags & G_FILE_SHOW_PHYSICS)
				sce->gm.flag |= GAME_SHOW_PHYSICS;
			if (fd->fileflags & G_FILE_GLSL_NO_SHADOWS)
				sce->gm.flag |= GAME_GLSL_NO_SHADOWS;
			if (fd->fileflags & G_FILE_GLSL_NO_SHADERS)
				sce->gm.flag |= GAME_GLSL_NO_SHADERS;
			if (fd->fileflags & G_FILE_GLSL_NO_RAMPS)
				sce->gm.flag |= GAME_GLSL_NO_RAMPS;
			if (fd->fileflags & G_FILE_GLSL_NO_NODES)
				sce->gm.flag |= GAME_GLSL_NO_NODES;
			if (fd->fileflags & G_FILE_GLSL_NO_EXTRA_TEX)
				sce->gm.flag |= GAME_GLSL_NO_EXTRA_TEX;
			if (fd->fileflags & G_FILE_IGNORE_DEPRECATION_WARNINGS)
				sce->gm.flag |= GAME_IGNORE_DEPRECATION_WARNINGS;

			if (fd->fileflags & G_FILE_GAME_MAT_GLSL)
				sce->gm.matmode= GAME_MAT_GLSL;
			else if (fd->fileflags & G_FILE_GAME_MAT)
				sce->gm.matmode= GAME_MAT_MULTITEX;
			else
				sce->gm.matmode= GAME_MAT_TEXFACE;

			sce->gm.flag |= GAME_DISPLAY_LISTS;
		}
		
		for (ob = main->object.first; ob; ob = ob->id.next) {
			if (ob->flag & 8192) // OB_POSEMODE = 8192
				ob->mode |= OB_MODE_POSE;
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 4)) {
		Scene *sce;
		Object *ob;
		Material *ma;
		Lamp *la;
		World *wo;
		Tex *tex;
		ParticleSettings *part;
		int do_gravity = 0;

		for (sce = main->scene.first; sce; sce = sce->id.next)
			if (sce->unit.scale_length == 0.0f)
				sce->unit.scale_length= 1.0f;
		
		for (ob = main->object.first; ob; ob = ob->id.next) {
			/* fluid-sim stuff */
			FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(ob, eModifierType_Fluidsim);
			if (fluidmd) fluidmd->fss->fmd = fluidmd;
			
			/* rotation modes were added, but old objects would now default to being 'quaternion based' */
			ob->rotmode= ROT_MODE_EUL;
		}
		
		for (ma = main->mat.first; ma; ma=ma->id.next) {
			if (ma->vol.reflection == 0.f) {
				ma->vol.reflection = 1.f;
				ma->vol.transmission_col[0] = ma->vol.transmission_col[1] = ma->vol.transmission_col[2] = 1.0f;
				ma->vol.reflection_col[0] = ma->vol.reflection_col[1] = ma->vol.reflection_col[2] = 1.0f;
			}

			do_version_mtex_factor_2_50(ma->mtex, ID_MA);
		}

		for (la = main->lamp.first; la; la=la->id.next)
			do_version_mtex_factor_2_50(la->mtex, ID_LA);

		for (wo = main->world.first; wo; wo=wo->id.next)
			do_version_mtex_factor_2_50(wo->mtex, ID_WO);

		for (tex = main->tex.first; tex; tex=tex->id.next)
			if (tex->vd)
				if (tex->vd->extend == 0)
					tex->vd->extend = TEX_CLIP;
		
		for (sce= main->scene.first; sce; sce= sce->id.next)
		{
			if (sce->audio.main == 0.0f)
				sce->audio.main = 1.0f;

			sce->r.ffcodecdata.audio_mixrate = sce->audio.mixrate;
			sce->r.ffcodecdata.audio_volume = sce->audio.main;
			sce->audio.distance_model = 2;
			sce->audio.doppler_factor = 1.0f;
			sce->audio.speed_of_sound = 343.3f;
		}

		/* Add default gravity to scenes */
		for (sce= main->scene.first; sce; sce= sce->id.next) {
			if ((sce->physics_settings.flag & PHYS_GLOBAL_GRAVITY) == 0
				&& len_v3(sce->physics_settings.gravity) == 0.0f) {

				sce->physics_settings.gravity[0] = sce->physics_settings.gravity[1] = 0.0f;
				sce->physics_settings.gravity[2] = -9.81f;
				sce->physics_settings.flag = PHYS_GLOBAL_GRAVITY;
				do_gravity = 1;
			}
		}

		/* Assign proper global gravity weights for dynamics (only z-coordinate is taken into account) */
		if (do_gravity) for (part= main->particle.first; part; part= part->id.next)
			part->effector_weights->global_gravity = part->acc[2]/-9.81f;

		for (ob = main->object.first; ob; ob = ob->id.next) {
			ModifierData *md;

			if (do_gravity) {
				for (md= ob->modifiers.first; md; md= md->next) {
					ClothModifierData *clmd = (ClothModifierData *)modifiers_findByType(ob, eModifierType_Cloth);
					if (clmd)
						clmd->sim_parms->effector_weights->global_gravity = clmd->sim_parms->gravity[2]/-9.81f;
				}

				if (ob->soft)
					ob->soft->effector_weights->global_gravity = ob->soft->grav/9.81f;
			}

			/* Normal wind shape is plane */
			if (ob->pd) {
				if (ob->pd->forcefield == PFIELD_WIND)
					ob->pd->shape = PFIELD_SHAPE_PLANE;
				
				if (ob->pd->flag & PFIELD_PLANAR)
					ob->pd->shape = PFIELD_SHAPE_PLANE;
				else if (ob->pd->flag & PFIELD_SURFACE)
					ob->pd->shape = PFIELD_SHAPE_SURFACE;

				ob->pd->flag |= PFIELD_DO_LOCATION;
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 6)) {
		Object *ob;
		Lamp *la;
		
		/* New variables for axis-angle rotations and/or quaternion rotations were added, and need proper initialization */
		for (ob= main->object.first; ob; ob= ob->id.next) {
			/* new variables for all objects */
			ob->quat[0]= 1.0f;
			ob->rotAxis[1]= 1.0f;
			
			/* bones */
			if (ob->pose) {
				bPoseChannel *pchan;
				
				for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
					/* just need to initalise rotation axis properly... */
					pchan->rotAxis[1]= 1.0f;
				}
			}
		}

		for (la = main->lamp.first; la; la=la->id.next)
			la->compressthresh= 0.05f;
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 7)) {
		Mesh *me;
		Nurb *nu;
		Lattice *lt;
		Curve *cu;
		Key *key;
		float *data;
		int a, tot;

		/* shape keys are no longer applied to the mesh itself, but rather
		   to the derivedmesh/displist, so here we ensure that the basis
		   shape key is always set in the mesh coordinates. */

		for (me= main->mesh.first; me; me= me->id.next) {
			if ((key = newlibadr(fd, lib, me->key)) && key->refkey) {
				data= key->refkey->data;
				tot= MIN2(me->totvert, key->refkey->totelem);

				for (a=0; a<tot; a++, data+=3)
					copy_v3_v3(me->mvert[a].co, data);
			}
		}

		for (lt= main->latt.first; lt; lt= lt->id.next) {
			if ((key = newlibadr(fd, lib, lt->key)) && key->refkey) {
				data= key->refkey->data;
				tot= MIN2(lt->pntsu*lt->pntsv*lt->pntsw, key->refkey->totelem);

				for (a=0; a<tot; a++, data+=3)
					copy_v3_v3(lt->def[a].vec, data);
			}
		}

		for (cu= main->curve.first; cu; cu= cu->id.next) {
			if ((key = newlibadr(fd, lib, cu->key)) && key->refkey) {
				data= key->refkey->data;

				for (nu=cu->nurb.first; nu; nu=nu->next) {
					if (nu->bezt) {
						BezTriple *bezt = nu->bezt;

						for (a=0; a<nu->pntsu; a++, bezt++) {
							copy_v3_v3(bezt->vec[0], data); data+=3;
							copy_v3_v3(bezt->vec[1], data); data+=3;
							copy_v3_v3(bezt->vec[2], data); data+=3;
							bezt->alfa= *data; data++;
						}
					}
					else if (nu->bp) {
						BPoint *bp = nu->bp;

						for (a=0; a<nu->pntsu*nu->pntsv; a++, bp++) {
							copy_v3_v3(bp->vec, data); data+=3;
							bp->alfa= *data; data++;
						}
					}
				}
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 8))
	{
		{
			Scene *sce= main->scene.first;
			while (sce) {
				if (sce->r.frame_step==0)
					sce->r.frame_step= 1;
				if (sce->r.mblur_samples==0)
					sce->r.mblur_samples = sce->r.osa;
				
				if (sce->ed && sce->ed->seqbase.first) {
					do_versions_seq_unique_name_all_strips(
						sce, &sce->ed->seqbase);
				}
			
				sce= sce->id.next;
			}
		}
		{
			/* ensure all nodes have unique names */
			bNodeTree *ntree= main->nodetree.first;
			while (ntree) {
				bNode *node=ntree->nodes.first;
				
				while (node) {
					nodeUniqueName(ntree, node);
					node= node->next;
				}
				
				ntree= ntree->id.next;
			}
		}
		{
			Object *ob=main->object.first;
			while (ob) {
				/* shaded mode disabled for now */
				if (ob->dt == OB_MATERIAL) ob->dt = OB_TEXTURE;
				ob=ob->id.next;
			}
		}
		
		{
			bScreen *screen;
			ScrArea *sa;
			SpaceLink *sl;
			
			for (screen= main->screen.first; screen; screen= screen->id.next) {
				for (sa= screen->areabase.first; sa; sa= sa->next) {
					for (sl= sa->spacedata.first; sl; sl= sl->next) {
						if (sl->spacetype==SPACE_VIEW3D) {
							View3D *v3d = (View3D *)sl;
							if (v3d->drawtype == OB_MATERIAL) v3d->drawtype = OB_SOLID;
						}
					}
				}
			}
		}
		
		/* only convert old 2.50 files with color management */
		if (main->versionfile == 250) {
			Scene *sce=main->scene.first;
			Material *ma=main->mat.first;
			World *wo=main->world.first;
			Tex *tex=main->tex.first;
			int i, convert=0;
			
			/* convert to new color management system:
			 while previously colors were stored as srgb, 
			 now they are stored as linear internally, 
			 with screen gamma correction in certain places in the UI. */

			/* don't know what scene is active, so we'll convert if any scene has it enabled... */
			while (sce) {
				if (sce->r.color_mgt_flag & R_COLOR_MANAGEMENT)
					convert=1;
				sce=sce->id.next;
			}
			
			if (convert) {
				while (ma) {
					if (ma->ramp_col) {
						ColorBand *band = (ColorBand *)ma->ramp_col;
						for (i=0; i<band->tot; i++) {
							CBData *data = band->data + i;
							srgb_to_linearrgb_v3_v3(&data->r, &data->r);
						}
					}
					if (ma->ramp_spec) {
						ColorBand *band = (ColorBand *)ma->ramp_spec;
						for (i=0; i<band->tot; i++) {
							CBData *data = band->data + i;
							srgb_to_linearrgb_v3_v3(&data->r, &data->r);
						}
					}
					
					srgb_to_linearrgb_v3_v3(&ma->r, &ma->r);
					srgb_to_linearrgb_v3_v3(&ma->specr, &ma->specr);
					srgb_to_linearrgb_v3_v3(&ma->mirr, &ma->mirr);
					srgb_to_linearrgb_v3_v3(ma->sss_col, ma->sss_col);
					ma=ma->id.next;
				}
				
				while (tex) {
					if (tex->coba) {
						ColorBand *band = (ColorBand *)tex->coba;
						for (i=0; i<band->tot; i++) {
							CBData *data = band->data + i;
							srgb_to_linearrgb_v3_v3(&data->r, &data->r);
						}
					}
					tex=tex->id.next;
				}
				
				while (wo) {
					srgb_to_linearrgb_v3_v3(&wo->ambr, &wo->ambr);
					srgb_to_linearrgb_v3_v3(&wo->horr, &wo->horr);
					srgb_to_linearrgb_v3_v3(&wo->zenr, &wo->zenr);
					wo=wo->id.next;
				}
			}
		}
	}
	
	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 9))
	{
		Scene *sce;
		Mesh *me;
		Object *ob;

		for (sce=main->scene.first; sce; sce=sce->id.next)
			if (!sce->toolsettings->particle.selectmode)
				sce->toolsettings->particle.selectmode= SCE_SELECT_PATH;

		if (main->versionfile == 250 && main->subversionfile > 1) {
			for (me=main->mesh.first; me; me=me->id.next)
				multires_load_old_250(me);

			for (ob=main->object.first; ob; ob=ob->id.next) {
				MultiresModifierData *mmd = (MultiresModifierData *)modifiers_findByType(ob, eModifierType_Multires);

				if (mmd) {
					mmd->totlvl--;
					mmd->lvl--;
					mmd->sculptlvl= mmd->lvl;
					mmd->renderlvl= mmd->lvl;
				}
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 10))
	{
		Object *ob;

		/* properly initialize hair clothsim data on old files */
		for (ob = main->object.first; ob; ob = ob->id.next) {
			ModifierData *md;
			for (md= ob->modifiers.first; md; md= md->next) {
				if (md->type == eModifierType_Cloth) {
					ClothModifierData *clmd = (ClothModifierData *)md;
					if (clmd->sim_parms->velocity_smooth < 0.01f)
						clmd->sim_parms->velocity_smooth = 0.f;
				}
			}
		}
	}

	/* fix bad area setup in subversion 10 */
	if (main->versionfile == 250 && main->subversionfile == 10)
	{
		/* fix for new view type in sequencer */
		bScreen *screen;
		ScrArea *sa;
		SpaceLink *sl;


		/* remove all preview window in wrong spaces */
		for (screen= main->screen.first; screen; screen= screen->id.next) {
			for (sa= screen->areabase.first; sa; sa= sa->next) {
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype!=SPACE_SEQ) {
						ARegion *ar;
						ListBase *regionbase;

						if (sl == sa->spacedata.first) {
							regionbase = &sa->regionbase;
						}
						else {
							regionbase = &sl->regionbase;
						}


						for ( ar = regionbase->first; ar; ar = ar->next) {
							if (ar->regiontype == RGN_TYPE_PREVIEW)
								break;
						}

						if (ar && (ar->regiontype == RGN_TYPE_PREVIEW)) {
							SpaceType *st= BKE_spacetype_from_id(SPACE_SEQ);
							BKE_area_region_free(st, ar);
							BLI_freelinkN(regionbase, ar);
						}
					}
				}
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 11))
	{
		{
			/* fix for new view type in sequencer */
			bScreen *screen;
			ScrArea *sa;
			SpaceLink *sl;


			for (screen= main->screen.first; screen; screen= screen->id.next) {
				for (sa= screen->areabase.first; sa; sa= sa->next) {
					for (sl= sa->spacedata.first; sl; sl= sl->next) {
						if (sl->spacetype==SPACE_SEQ) {
							ARegion *ar;
							ARegion *ar_main;
							ListBase *regionbase;
							SpaceSeq *sseq = (SpaceSeq *)sl;

							if (sl == sa->spacedata.first) {
								regionbase = &sa->regionbase;
							}
							else {
								regionbase = &sl->regionbase;
							}

							if (sseq->view == 0) sseq->view = SEQ_VIEW_SEQUENCE;
							if (sseq->mainb == 0) sseq->mainb = SEQ_DRAW_IMG_IMBUF;

							ar_main = (ARegion*)regionbase->first;
							for (; ar_main; ar_main = ar_main->next) {
								if (ar_main->regiontype == RGN_TYPE_WINDOW)
									break;
							}
							ar= MEM_callocN(sizeof(ARegion), "preview area for sequencer");
							BLI_insertlinkbefore(regionbase, ar_main, ar);
							sequencer_init_preview_region(ar);
						}
					}
				}
			}
		}
	}

	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 12))
	{
		Scene *sce;
		Object *ob;
		Brush *brush;
		Material *ma;
		
		/* game engine changes */
		for (sce = main->scene.first; sce; sce = sce->id.next) {
			sce->gm.eyeseparation = 0.10f;
		}
		
		/* anim viz changes */
		for (ob= main->object.first; ob; ob= ob->id.next) {
			/* initialize object defaults */
			animviz_settings_init(&ob->avs);
			
			/* if armature, copy settings for pose from armature data 
			 * performing initialization where appropriate
			 */
			if (ob->pose && ob->data) {
				bArmature *arm= newlibadr(fd, lib, ob->data);
				if (arm) { /* XXX - why does this fail in some cases? */
					bAnimVizSettings *avs= &ob->pose->avs;
					
					/* ghosting settings ---------------- */
						/* ranges */
					avs->ghost_bc= avs->ghost_ac= arm->ghostep;
					
					avs->ghost_sf= arm->ghostsf;
					avs->ghost_ef= arm->ghostef;
					if ((avs->ghost_sf == avs->ghost_ef) && (avs->ghost_sf == 0)) {
						avs->ghost_sf= 1;
						avs->ghost_ef= 100;
					}
					
						/* type */
					if (arm->ghostep == 0)
						avs->ghost_type= GHOST_TYPE_NONE;
					else
						avs->ghost_type= arm->ghosttype + 1;
					
						/* stepsize */
					avs->ghost_step= arm->ghostsize;
					if (avs->ghost_step == 0)
						avs->ghost_step= 1;
					
					/* path settings --------------------- */
						/* ranges */
					avs->path_bc= arm->pathbc;
					avs->path_ac= arm->pathac;
					if ((avs->path_bc == avs->path_ac) && (avs->path_bc == 0))
						avs->path_bc= avs->path_ac= 10;
					
					avs->path_sf= arm->pathsf;
					avs->path_ef= arm->pathef;
					if ((avs->path_sf == avs->path_ef) && (avs->path_sf == 0)) {
						avs->path_sf= 1;
						avs->path_ef= 250;
					}
					
						/* flags */
					if (arm->pathflag & ARM_PATH_FNUMS)
						avs->path_viewflag |= MOTIONPATH_VIEW_FNUMS;
					if (arm->pathflag & ARM_PATH_KFRAS)
						avs->path_viewflag |= MOTIONPATH_VIEW_KFRAS;
					if (arm->pathflag & ARM_PATH_KFNOS)
						avs->path_viewflag |= MOTIONPATH_VIEW_KFNOS;
					
						/* bake flags */
					if (arm->pathflag & ARM_PATH_HEADS)
						avs->path_bakeflag |= MOTIONPATH_BAKE_HEADS;
					
						/* type */
					if (arm->pathflag & ARM_PATH_ACFRA)
						avs->path_type = MOTIONPATH_TYPE_ACFRA;
					
						/* stepsize */
					avs->path_step= arm->pathsize;
					if (avs->path_step == 0)
						avs->path_step= 1;
				}
				else
					animviz_settings_init(&ob->pose->avs);
			}
		}
		
		/* brush texture changes */
		for (brush= main->brush.first; brush; brush= brush->id.next) {
			default_mtex(&brush->mtex);
		}

		for (ma= main->mat.first; ma; ma= ma->id.next) {
			if (ma->vol.ms_spread < 0.0001f) {
				ma->vol.ms_spread = 0.2f;
				ma->vol.ms_diff = 1.f;
				ma->vol.ms_intensity = 1.f;	
			}
		}
	}
	
	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 13)) {
		/* NOTE: if you do more conversion, be sure to do it outside of this and
		   increase subversion again, otherwise it will not be correct */
		Object *ob;
		
		/* convert degrees to radians for internal use */
		for (ob=main->object.first; ob; ob=ob->id.next) {
			bPoseChannel *pchan;

			do_version_constraints_radians_degrees_250(&ob->constraints);

			if (ob->pose) {
				for (pchan=ob->pose->chanbase.first; pchan; pchan=pchan->next) {
					pchan->limitmin[0] *= (float)(M_PI/180.0);
					pchan->limitmin[1] *= (float)(M_PI/180.0);
					pchan->limitmin[2] *= (float)(M_PI/180.0);
					pchan->limitmax[0] *= (float)(M_PI/180.0);
					pchan->limitmax[1] *= (float)(M_PI/180.0);
					pchan->limitmax[2] *= (float)(M_PI/180.0);

					do_version_constraints_radians_degrees_250(&pchan->constraints);
				}
			}
		}
	}
	
	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 14)) {
		/* fix for bad View2D extents for Animation Editors */
		bScreen *screen;
		ScrArea *sa;
		SpaceLink *sl;
		
		for (screen= main->screen.first; screen; screen= screen->id.next) {
			for (sa= screen->areabase.first; sa; sa= sa->next) {
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					ListBase *regionbase;
					ARegion *ar;
					
					if (sl == sa->spacedata.first)
						regionbase = &sa->regionbase;
					else
						regionbase = &sl->regionbase;
						
					if (ELEM(sl->spacetype, SPACE_ACTION, SPACE_NLA)) {
						for (ar = (ARegion*)regionbase->first; ar; ar = ar->next) {
							if (ar->regiontype == RGN_TYPE_WINDOW) {
								ar->v2d.cur.ymax = ar->v2d.tot.ymax = 0.0f;
								ar->v2d.cur.ymin = ar->v2d.tot.ymin = (float)(-sa->winy) / 3.0f;
							}
						}
					}
				}
			}
		}
	}
	
	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 15)) {
		World *wo;
		Material *ma;

		/* ambient default from 0.5f to 1.0f */
		for (ma= main->mat.first; ma; ma=ma->id.next)
			ma->amb *= 2.0f;

		for (wo= main->world.first; wo; wo=wo->id.next) {
			/* ao splitting into ao/env/indirect */
			wo->ao_env_energy= wo->aoenergy;
			wo->aoenergy= 1.0f;

			if (wo->ao_indirect_bounces == 0)
				wo->ao_indirect_bounces= 1;
			else
				wo->mode |= WO_INDIRECT_LIGHT;

			if (wo->aomix == WO_AOSUB)
				wo->ao_env_energy= -wo->ao_env_energy;
			else if (wo->aomix == WO_AOADDSUB)
				wo->mode |= WO_AMB_OCC;

			wo->aomix= WO_AOMUL;

			/* ambient default from 0.5f to 1.0f */
			mul_v3_fl(&wo->ambr, 0.5f);
			wo->ao_env_energy *= 0.5f;
		}
	}
	
	if (main->versionfile < 250 || (main->versionfile == 250 && main->subversionfile < 17)) {
		Scene *sce;
		Sequence *seq;
		Material *ma;

		/* initialize to sane default so toggling on border shows something */
		for (sce = main->scene.first; sce; sce = sce->id.next) {
			if (sce->r.border.xmin == 0.0f && sce->r.border.ymin == 0.0f &&
			   sce->r.border.xmax == 0.0f && sce->r.border.ymax == 0.0f) {
				sce->r.border.xmin = 0.0f;
				sce->r.border.ymin = 0.0f;
				sce->r.border.xmax = 1.0f;
				sce->r.border.ymax = 1.0f;
			}

			if ((sce->r.ffcodecdata.flags & FFMPEG_MULTIPLEX_AUDIO) == 0)
				sce->r.ffcodecdata.audio_codec = 0x0; // CODEC_ID_NONE

			SEQ_BEGIN(sce->ed, seq) {
				seq->volume = 1.0f;
			}
			SEQ_END
		}

		/* particle brush strength factor was changed from int to float */
		for (sce= main->scene.first; sce; sce=sce->id.next) {
			ParticleEditSettings *pset= &sce->toolsettings->particle;
			int a;

			for (a=0; a<PE_TOT_BRUSH; a++)
				pset->brush[a].strength /= 100.0f;
		}

		for (ma = main->mat.first; ma; ma=ma->id.next)
			if (ma->mode & MA_TRACEBLE)
				ma->shade_flag |= MA_APPROX_OCCLUSION;

		/* sequencer changes */
		{
			bScreen *screen;
			ScrArea *sa;
			SpaceLink *sl;

			for (screen= main->screen.first; screen; screen= screen->id.next) {
				for (sa= screen->areabase.first; sa; sa= sa->next) {
					for (sl= sa->spacedata.first; sl; sl= sl->next) {
						if (sl->spacetype==SPACE_SEQ) {
							ARegion *ar_preview;
							ListBase *regionbase;

							if (sl == sa->spacedata.first) {
								regionbase = &sa->regionbase;
							}
							else {
								regionbase = &sl->regionbase;
							}

							ar_preview = (ARegion*)regionbase->first;
							for (; ar_preview; ar_preview = ar_preview->next) {
								if (ar_preview->regiontype == RGN_TYPE_PREVIEW)
									break;
							}
							if (ar_preview && (ar_preview->regiontype == RGN_TYPE_PREVIEW)) {
								sequencer_init_preview_region(ar_preview);
							}
						}
					}
				}
			}
		} /* sequencer changes */
	}
	
	if (main->versionfile <= 251) {	/* 2.5.1 had no subversions */
		bScreen *sc;
		
		/* Blender 2.5.2 - subversion 0 introduced a new setting: V3D_RENDER_OVERRIDE.
		 * This bit was used in the past for V3D_TRANSFORM_SNAP, which is now deprecated. 
		 * Here we clear it for old files so they don't come in with V3D_RENDER_OVERRIDE set,
		 * which would cause cameras, lamps, etc to become invisible */
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D* v3d = (View3D *)sl;
						v3d->flag2 &= ~V3D_RENDER_OVERRIDE;
					}
				}
			}
		}
	}

	if (main->versionfile < 252 || (main->versionfile == 252 && main->subversionfile < 1)) {
		Brush *brush;
		Object *ob;
		Scene *scene;
		bNodeTree *ntree;
		
		for (brush= main->brush.first; brush; brush= brush->id.next) {
			if (brush->curve) brush->curve->preset = CURVE_PRESET_SMOOTH;
		}
		
		/* properly initialize active flag for fluidsim modifiers */
		for (ob = main->object.first; ob; ob = ob->id.next) {
			ModifierData *md;
			for (md= ob->modifiers.first; md; md= md->next) {
				if (md->type == eModifierType_Fluidsim) {
					FluidsimModifierData *fmd = (FluidsimModifierData *)md;
					fmd->fss->flag |= OB_FLUIDSIM_ACTIVE; 
					fmd->fss->flag |= OB_FLUIDSIM_OVERRIDE_TIME;
				}
			}
		}
		
		/* adjustment to color balance node values */
		for (scene= main->scene.first; scene; scene= scene->id.next) {
			if (scene->nodetree) {
				bNode *node=scene->nodetree->nodes.first;
				
				while (node) {
					if (node->type == CMP_NODE_COLORBALANCE) {
						NodeColorBalance *n= (NodeColorBalance *)node->storage;
						n->lift[0] += 1.f;
						n->lift[1] += 1.f;
						n->lift[2] += 1.f;
					}
					node= node->next;
				}
			}
		}
		/* check inside node groups too */
		for (ntree= main->nodetree.first; ntree; ntree=ntree->id.next) {
			bNode *node=ntree->nodes.first;
			
			while (node) {
				if (node->type == CMP_NODE_COLORBALANCE) {
					NodeColorBalance *n= (NodeColorBalance *)node->storage;
					n->lift[0] += 1.f;
					n->lift[1] += 1.f;
					n->lift[2] += 1.f;
				}
				node= node->next;
			}
		}
	}
	
	/* old-track -> constraints (this time we're really doing it!) */
	if (main->versionfile < 252 || (main->versionfile == 252 && main->subversionfile < 2)) {
		Object *ob;
		
		for (ob = main->object.first; ob; ob = ob->id.next)
			do_version_old_trackto_to_constraints(ob);
	}
	
	if (main->versionfile < 252 || (main->versionfile == 252 && main->subversionfile < 5)) {
		bScreen *sc;
		
		/* Image editor scopes */
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_IMAGE) {
						SpaceImage *sima = (SpaceImage *)sl;
						scopes_new(&sima->scopes);
					}
				}
			}
		}
	}
	

	if (main->versionfile < 253)
	{
		Object *ob;
		Scene *scene;
		bScreen *sc;
		Tex *tex;
		Brush *brush;

		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype == SPACE_NODE) {
						SpaceNode *snode= (SpaceNode *)sl;
						ListBase *regionbase;
						ARegion *ar;

						if (sl == sa->spacedata.first)
							regionbase = &sa->regionbase;
						else
							regionbase = &sl->regionbase;

						if (snode->v2d.minzoom > 0.09f)
							snode->v2d.minzoom= 0.09f;
						if (snode->v2d.maxzoom < 2.31f)
							snode->v2d.maxzoom= 2.31f;

						for (ar= regionbase->first; ar; ar= ar->next) {
							if (ar->regiontype == RGN_TYPE_WINDOW) {
								if (ar->v2d.minzoom > 0.09f)
									ar->v2d.minzoom= 0.09f;
								if (ar->v2d.maxzoom < 2.31f)
									ar->v2d.maxzoom= 2.31f;
							}
						}
					}
					else if (sl->spacetype == SPACE_TIME) {
						SpaceTime *stime= (SpaceTime *)sl;
						
						/* enable all cache display */
						stime->cache_display |= TIME_CACHE_DISPLAY;
						stime->cache_display |= (TIME_CACHE_SOFTBODY|TIME_CACHE_PARTICLES);
						stime->cache_display |= (TIME_CACHE_CLOTH|TIME_CACHE_SMOKE|TIME_CACHE_DYNAMICPAINT);
					}
				}
			}
		}

		do_version_mdef_250(main);

		/* parent type to modifier */
		for (ob = main->object.first; ob; ob = ob->id.next) {
			if (ob->parent) {
				Object *parent= (Object *)newlibadr(fd, lib, ob->parent);
				if (parent) { /* parent may not be in group */
					if (parent->type==OB_ARMATURE && ob->partype==PARSKEL) {
						ArmatureModifierData *amd;
						bArmature *arm= (bArmature *)newlibadr(fd, lib, parent->data);

						amd = (ArmatureModifierData*) modifier_new(eModifierType_Armature);
						amd->object = ob->parent;
						BLI_addtail((ListBase*)&ob->modifiers, amd);
						amd->deformflag= arm->deformflag;
						ob->partype = PAROBJECT;
					}
					else if (parent->type==OB_LATTICE && ob->partype==PARSKEL) {
						LatticeModifierData *lmd;

						lmd = (LatticeModifierData*) modifier_new(eModifierType_Lattice);
						lmd->object = ob->parent;
						BLI_addtail((ListBase*)&ob->modifiers, lmd);
						ob->partype = PAROBJECT;
					}
					else if (parent->type==OB_CURVE && ob->partype==PARCURVE) {
						CurveModifierData *cmd;

						cmd = (CurveModifierData*) modifier_new(eModifierType_Curve);
						cmd->object = ob->parent;
						BLI_addtail((ListBase*)&ob->modifiers, cmd);
						ob->partype = PAROBJECT;
					}
				}
			}
		}
		
		/* initialize scene active layer */
		for (scene= main->scene.first; scene; scene=scene->id.next) {
			int i;
			for (i=0; i<20; i++) {
				if (scene->lay & (1<<i)) {
					scene->layact= 1<<i;
					break;
				}
			}
		}

		for (tex= main->tex.first; tex; tex= tex->id.next) {
			/* if youre picky, this isn't correct until we do a version bump
			 * since you could set saturation to be 0.0*/
			if (tex->saturation==0.0f)
				tex->saturation= 1.0f;
		}

		{
			Curve *cu;
			for (cu= main->curve.first; cu; cu= cu->id.next) {
				cu->smallcaps_scale= 0.75f;
			}
		}

		for (scene= main->scene.first; scene; scene=scene->id.next) {
			if (scene) {
				Sequence *seq;
				SEQ_BEGIN(scene->ed, seq) {
					if (seq->sat==0.0f) {
						seq->sat= 1.0f;
					}
				}
				SEQ_END
			}
		}

		/* GSOC 2010 Sculpt - New settings for Brush */

		for (brush= main->brush.first; brush; brush= brush->id.next) {
			/* Sanity Check */

			// infinite number of dabs
			if (brush->spacing == 0)
				brush->spacing = 10;

			// will have no effect
			if (brush->alpha == 0)
				brush->alpha = 0.5f;

			// bad radius
			if (brush->unprojected_radius == 0)
				brush->unprojected_radius = 0.125f;

			// unusable size
			if (brush->size == 0)
				brush->size = 35;

			// can't see overlay
			if (brush->texture_overlay_alpha == 0)
				brush->texture_overlay_alpha = 33;

			// same as draw brush
			if (brush->crease_pinch_factor == 0)
				brush->crease_pinch_factor = 0.5f;

			// will sculpt no vertexes
			if (brush->plane_trim == 0)
				brush->plane_trim = 0.5f;

			// same as smooth stroke off
			if (brush->smooth_stroke_radius == 0)
				brush->smooth_stroke_radius= 75;

			// will keep cursor in one spot
			if (brush->smooth_stroke_radius == 1)
				brush->smooth_stroke_factor= 0.9f;

			// same as dots
			if (brush->rate == 0)
				brush->rate = 0.1f;

			/* New Settings */
			if (main->versionfile < 252 || (main->versionfile == 252 && main->subversionfile < 5)) {
				brush->flag |= BRUSH_SPACE_ATTEN; // explicitly enable adaptive space

				// spacing was originally in pixels, convert it to percentage for new version
				// size should not be zero due to sanity check above
				brush->spacing = (int)(100*((float)brush->spacing) / ((float)brush->size));

				if (brush->add_col[0] == 0 &&
					brush->add_col[1] == 0 &&
					brush->add_col[2] == 0)
				{
					brush->add_col[0] = 1.00f;
					brush->add_col[1] = 0.39f;
					brush->add_col[2] = 0.39f;
				}

				if (brush->sub_col[0] == 0 &&
					brush->sub_col[1] == 0 &&
					brush->sub_col[2] == 0)
				{
					brush->sub_col[0] = 0.39f;
					brush->sub_col[1] = 0.39f;
					brush->sub_col[2] = 1.00f;
				}
			}
		}
	}

	/* GSOC Sculpt 2010 - Sanity check on Sculpt/Paint settings */
	if (main->versionfile < 253) {
		Scene *sce;
		for (sce= main->scene.first; sce; sce= sce->id.next) {
			if (sce->toolsettings->sculpt_paint_unified_alpha == 0)
				sce->toolsettings->sculpt_paint_unified_alpha = 0.5f;

			if (sce->toolsettings->sculpt_paint_unified_unprojected_radius == 0) 
				sce->toolsettings->sculpt_paint_unified_unprojected_radius = 0.125f;

			if (sce->toolsettings->sculpt_paint_unified_size == 0)
				sce->toolsettings->sculpt_paint_unified_size = 35;
		}
	}

	if (main->versionfile < 253 || (main->versionfile == 253 && main->subversionfile < 1))
		{
			Object *ob;

			for (ob = main->object.first; ob; ob = ob->id.next) {
				ModifierData *md;
				for (md= ob->modifiers.first; md; md= md->next) {
					if (md->type == eModifierType_Smoke) {
						SmokeModifierData *smd = (SmokeModifierData *)md;

						if ((smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain)
						{
							smd->domain->vorticity = 2.0f;
							smd->domain->time_scale = 1.0f;

							if (!(smd->domain->flags & (1<<4)))
								continue;

							/* delete old MOD_SMOKE_INITVELOCITY flag */
							smd->domain->flags &= ~(1<<4);

							/* for now just add it to all flow objects in the scene */
							{
								Object *ob2;
								for (ob2 = main->object.first; ob2; ob2 = ob2->id.next) {
									ModifierData *md2;
									for (md2= ob2->modifiers.first; md2; md2= md2->next) {
										if (md2->type == eModifierType_Smoke) {
											SmokeModifierData *smd2 = (SmokeModifierData *)md2;

											if ((smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow)
											{
												smd2->flow->flags |= MOD_SMOKE_FLOW_INITVELOCITY;
											}
										}
									}
								}
							}

						}
						else if ((smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow) {
							smd->flow->vel_multi = 1.0f;
						}

					}
				}
			}
		}

	if (main->versionfile < 255 || (main->versionfile == 255 && main->subversionfile < 1)) {
		Brush *br;
		ParticleSettings *part;
		bScreen *sc;
		Object *ob;

		for (br= main->brush.first; br; br= br->id.next) {
			if (br->ob_mode==0)
				br->ob_mode= OB_MODE_ALL_PAINT;
		}

		for (part = main->particle.first; part; part = part->id.next) {
			if (part->boids)
				part->boids->pitch = 1.0f;

			part->flag &= ~PART_HAIR_REGROW; /* this was a deprecated flag before */
			part->kink_amp_clump = 1.f; /* keep old files looking similar */
		}

		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype == SPACE_INFO) {
						SpaceInfo *sinfo= (SpaceInfo *)sl;
						ARegion *ar;

						sinfo->rpt_mask= INFO_RPT_OP;

						for (ar= sa->regionbase.first; ar; ar= ar->next) {
							if (ar->regiontype == RGN_TYPE_WINDOW) {
								ar->v2d.scroll = (V2D_SCROLL_RIGHT);
								ar->v2d.align = V2D_ALIGN_NO_NEG_X|V2D_ALIGN_NO_NEG_Y; /* align bottom left */
								ar->v2d.keepofs = V2D_LOCKOFS_X;
								ar->v2d.keepzoom = (V2D_LOCKZOOM_X|V2D_LOCKZOOM_Y|V2D_LIMITZOOM|V2D_KEEPASPECT);
								ar->v2d.keeptot= V2D_KEEPTOT_BOUNDS;
								ar->v2d.minzoom= ar->v2d.maxzoom= 1.0f;
							}
						}
					}
				}
			}
		}

		/* fix rotation actuators for objects so they use real angles (radians)
		 * since before blender went opensource this strange scalar was used: (1 / 0.02) * 2 * math.pi/360 */
		for (ob= main->object.first; ob; ob= ob->id.next) {
			bActuator *act= ob->actuators.first;
			while (act) {
				if (act->type==ACT_OBJECT) {
					/* multiply velocity with 50 in old files */
					bObjectActuator *oa= act->data;
					mul_v3_fl(oa->drot, 0.8726646259971648f);
				}
				act= act->next;
			}
		}
	}
	
	// init facing axis property of steering actuators
	{					
		Object *ob;
		for (ob = main->object.first; ob; ob = ob->id.next) {
			bActuator *act;
			for (act= ob->actuators.first; act; act= act->next) {
				if (act->type==ACT_STEERING) {
					bSteeringActuator* stact = act->data;
					if (stact->facingaxis==0)
					{
						stact->facingaxis=1;
					}						
				}
			}
		}
	}

	if (main->versionfile < 255 || (main->versionfile == 255 && main->subversionfile < 3)) {
		Object *ob;

		/* ocean res is now squared, reset old ones - will be massive */
		for (ob = main->object.first; ob; ob = ob->id.next) {
			ModifierData *md;
			for (md= ob->modifiers.first; md; md= md->next) {
				if (md->type == eModifierType_Ocean) {
					OceanModifierData *omd = (OceanModifierData *)md;
					omd->resolution = 7;
					omd->oceancache = NULL;
				}
			}
		}		
	}

	if (main->versionfile < 256) {
		bScreen *sc;
		ScrArea *sa;
		Key *key;
		
		/* Fix for sample line scope initializing with no height */
		for (sc= main->screen.first; sc; sc= sc->id.next) {
			sa= sc->areabase.first;
			while (sa) {
				SpaceLink *sl;
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_IMAGE) {
						SpaceImage *sima= (SpaceImage *)sl;
						if (sima->sample_line_hist.height == 0 )
							sima->sample_line_hist.height = 100;
					}
				}
				sa= sa->next;
			}
		}
		
		/* old files could have been saved with slidermin = slidermax = 0.0, but the UI in
		 * 2.4x would never reveal this to users as a dummy value always ended up getting used
		 * instead
		 */
		for (key = main->key.first; key; key = key->id.next) {
			KeyBlock *kb;
			
			for (kb = key->block.first; kb; kb = kb->next) {
				if (IS_EQF(kb->slidermin, kb->slidermax) && IS_EQ(kb->slidermax, 0))
					kb->slidermax = kb->slidermin + 1.0f;
			}
		}
	}
	
	if (main->versionfile < 256 || (main->versionfile == 256 && main->subversionfile < 1)) {
		/* fix for bones that didn't have arm_roll before */
		bArmature* arm;
		Bone* bone;
		Object *ob;

		for (arm = main->armature.first; arm; arm = arm->id.next)
			for (bone = arm->bonebase.first; bone; bone = bone->next)
				do_version_bone_roll_256(bone);

		/* fix for objects which have zero dquat's
		 * since this is multiplied with the quat rather than added */
		for (ob= main->object.first; ob; ob= ob->id.next) {
			if (is_zero_v4(ob->dquat)) {
				unit_qt(ob->dquat);
			}
			if (is_zero_v3(ob->drotAxis) && ob->drotAngle == 0.0f) {
				unit_axis_angle(ob->drotAxis, &ob->drotAngle);
			}
		}
	}

	if (main->versionfile < 256 || (main->versionfile == 256 && main->subversionfile < 2)) {
		bNodeTree *ntree;
		
		/* node sockets are not exposed automatically any more,
		 * this mimics the old behavior by adding all unlinked sockets to groups.
		 */
		for (ntree=main->nodetree.first; ntree; ntree=ntree->id.next) {
			/* XXX Only setting a flag here. Actual adding of group sockets
			 * is done in lib_verify_nodetree, because at this point the internal
			 * nodes may not be up-to-date! (missing lib-link)
			 */
			ntree->flag |= NTREE_DO_VERSIONS_GROUP_EXPOSE;
		}
	}

	if (main->versionfile < 256 || (main->versionfile == 256 && main->subversionfile <3)) {
		bScreen *sc;
		Brush *brush;
		Object *ob;
		ParticleSettings *part;
		Material *mat;
		int tex_nr, transp_tex;
		
		for (mat = main->mat.first; mat; mat = mat->id.next) {
			if (!(mat->mode & MA_TRANSP) && !(mat->material_type & MA_TYPE_VOLUME)) {
				
				transp_tex= 0;
				
				for (tex_nr=0; tex_nr<MAX_MTEX; tex_nr++) {
					if (!mat->mtex[tex_nr]) continue;
					if (mat->mtex[tex_nr]->mapto & MAP_ALPHA) transp_tex= 1;
				}

				/* weak! material alpha could be animated */
				if (mat->alpha < 1.0f || mat->fresnel_tra > 0.0f || transp_tex) {
					mat->mode |= MA_TRANSP;
					mat->mode &= ~(MA_ZTRANSP|MA_RAYTRANSP);
				}
			}
		}

		/* redraws flag in SpaceTime has been moved to Screen level */
		for (sc = main->screen.first; sc; sc= sc->id.next) {
			if (sc->redraws_flag == 0) {
				/* just initialize to default? */
				// XXX: we could also have iterated through areas, and taken them from the first timeline available...
				sc->redraws_flag = TIME_ALL_3D_WIN|TIME_ALL_ANIM_WIN;
			}
		}

		for (brush= main->brush.first; brush; brush= brush->id.next) {
			if (brush->height == 0)
				brush->height= 0.4f;
		}

		/* replace 'rim material' option for in offset*/
		for (ob = main->object.first; ob; ob = ob->id.next) {
			ModifierData *md;
			for (md= ob->modifiers.first; md; md= md->next) {
				if (md->type == eModifierType_Solidify) {
					SolidifyModifierData *smd = (SolidifyModifierData *)md;
					if (smd->flag & MOD_SOLIDIFY_RIM_MATERIAL) {
						smd->mat_ofs_rim= 1;
						smd->flag &= ~MOD_SOLIDIFY_RIM_MATERIAL;
					}
				}
			}
		}

		/* particle draw color from material */
		for (part = main->particle.first; part; part = part->id.next) {
			if (part->draw & PART_DRAW_MAT_COL)
				part->draw_col = PART_DRAW_COL_MAT;
		}
	}

	if (main->versionfile < 256 || (main->versionfile == 256 && main->subversionfile < 6)) {
		Mesh *me;

		for (me= main->mesh.first; me; me= me->id.next)
			mesh_calc_normals_tessface(me->mvert, me->totvert, me->mface, me->totface, NULL);
	}

	if (main->versionfile < 256 || (main->versionfile == 256 && main->subversionfile < 2)) {
		/* update blur area sizes from 0..1 range to 0..100 percentage */
		Scene *scene;
		bNode *node;
		for (scene=main->scene.first; scene; scene=scene->id.next)
			if (scene->nodetree)
				for (node=scene->nodetree->nodes.first; node; node=node->next)
					if (node->type==CMP_NODE_BLUR) {
						NodeBlurData *nbd= node->storage;
						nbd->percentx *= 100.0f;
						nbd->percenty *= 100.0f;
					}
	}

	if (main->versionfile < 258 || (main->versionfile == 258 && main->subversionfile < 1)) {
		/* screen view2d settings were not properly initialized [#27164]
		 * v2d->scroll caused the bug but best reset other values too which are in old blend files only.
		 * need to make less ugly - possibly an iterator? */
		bScreen *screen;
		for (screen= main->screen.first; screen; screen= screen->id.next) {
			ScrArea *sa;
			/* add regions */
			for (sa= screen->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl= sa->spacedata.first;
				if (sl->spacetype==SPACE_IMAGE) {
					ARegion *ar;
					for (ar=sa->regionbase.first; ar; ar= ar->next) {
						if (ar->regiontype == RGN_TYPE_WINDOW) {
							View2D *v2d= &ar->v2d;
							v2d->minzoom= v2d->maxzoom= v2d->scroll= v2d->keeptot= v2d->keepzoom= v2d->keepofs= v2d->align= 0;
						}
					}
				}
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_IMAGE) {
						ARegion *ar;
						for (ar=sl->regionbase.first; ar; ar= ar->next) {
							if (ar->regiontype == RGN_TYPE_WINDOW) {
								View2D *v2d= &ar->v2d;
								v2d->minzoom= v2d->maxzoom= v2d->scroll= v2d->keeptot= v2d->keepzoom= v2d->keepofs= v2d->align= 0;
							}
						}
					}
				}
			}
		}

		{
			/* Initialize texture point density curve falloff */
			Tex *tex;
			for (tex= main->tex.first; tex; tex= tex->id.next) {
				if (tex->pd) {
					if (tex->pd->falloff_speed_scale == 0.0f)
						tex->pd->falloff_speed_scale = 100.0f;
					
					if (!tex->pd->falloff_curve) {
						tex->pd->falloff_curve = curvemapping_add(1, 0, 0, 1, 1);
						
						tex->pd->falloff_curve->preset = CURVE_PRESET_LINE;
						tex->pd->falloff_curve->cm->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
						curvemap_reset(tex->pd->falloff_curve->cm, &tex->pd->falloff_curve->clipr, tex->pd->falloff_curve->preset, CURVEMAP_SLOPE_POSITIVE);
						curvemapping_changed(tex->pd->falloff_curve, 0);
					}
				}
			}
		}

		{
			/* add default value for behind strength of camera actuator */
			Object *ob;
			bActuator *act;
			for (ob = main->object.first; ob; ob= ob->id.next) {
				for (act= ob->actuators.first; act; act= act->next) {
					if (act->type == ACT_CAMERA) {
						bCameraActuator *ba= act->data;

						ba->damping = 1.0/32.0;
					}
				}
			}
		}

		{
			ParticleSettings *part;
			for (part = main->particle.first; part; part = part->id.next) {
				/* Initialize particle billboard scale */
				part->bb_size[0] = part->bb_size[1] = 1.0f;
			}
		}
	}

	if (main->versionfile < 259 || (main->versionfile == 259 && main->subversionfile < 1)) {
		{
			Scene *scene;
			Sequence *seq;

			for (scene=main->scene.first; scene; scene=scene->id.next)
			{
				scene->r.ffcodecdata.audio_channels = 2;
				scene->audio.volume = 1.0f;
				SEQ_BEGIN(scene->ed, seq) {
					seq->pitch = 1.0f;
				}
				SEQ_END
			}
		}
		{
			bScreen *screen;
			for (screen= main->screen.first; screen; screen= screen->id.next) {
				ScrArea *sa;
				/* add regions */
				for (sa= screen->areabase.first; sa; sa= sa->next) {
					SpaceLink *sl= sa->spacedata.first;
					if (sl->spacetype==SPACE_SEQ) {
						ARegion *ar;
						for (ar=sa->regionbase.first; ar; ar= ar->next) {
							if (ar->regiontype == RGN_TYPE_WINDOW) {
								if (ar->v2d.min[1] == 4.0f)
									ar->v2d.min[1]= 0.5f;
							}
						}
					}
					for (sl= sa->spacedata.first; sl; sl= sl->next) {
						if (sl->spacetype==SPACE_SEQ) {
							ARegion *ar;
							for (ar=sl->regionbase.first; ar; ar= ar->next) {
								if (ar->regiontype == RGN_TYPE_WINDOW) {
									if (ar->v2d.min[1] == 4.0f)
										ar->v2d.min[1]= 0.5f;
								}
							}
						}
					}
				}
			}
		}
		{
			/* Make "auto-clamped" handles a per-keyframe setting instead of per-FCurve 
			 *
			 * We're only patching F-Curves in Actions here, since it is assumed that most
			 * drivers out there won't be using this (and if they are, they're in the minority).
			 * While we should aim to fix everything ideally, in practice it's far too hard
			 * to get to every animdata block, not to mention the performance hit that'd have
			 */
			bAction *act;
			FCurve *fcu;
			
			for (act = main->action.first; act; act = act->id.next) {
				for (fcu = act->curves.first; fcu; fcu = fcu->next) {
					BezTriple *bezt;
					unsigned int i = 0;
					
					/* only need to touch curves that had this flag set */
					if ((fcu->flag & FCURVE_AUTO_HANDLES) == 0)
						continue;
					if ((fcu->totvert == 0) || (fcu->bezt == NULL))
						continue;
						
					/* only change auto-handles to auto-clamped */
					for (bezt=fcu->bezt; i < fcu->totvert; i++, bezt++) {
						if (bezt->h1 == HD_AUTO) bezt->h1 = HD_AUTO_ANIM;
						if (bezt->h2 == HD_AUTO) bezt->h2 = HD_AUTO_ANIM;
					}
					
					fcu->flag &= ~FCURVE_AUTO_HANDLES;
				}
			}
		}
		{
			/* convert fcurve and shape action actuators to action actuators */
			Object *ob;
			bActuator *act;
			bIpoActuator *ia;
			bActionActuator *aa;

			for (ob= main->object.first; ob; ob= ob->id.next) {
				for (act= ob->actuators.first; act; act= act->next) {
					if (act->type == ACT_IPO) {
						// Create the new actuator
						ia= act->data;
						aa= MEM_callocN(sizeof(bActionActuator), "fcurve -> action actuator do_version");

						// Copy values
						aa->type = ia->type;
						aa->flag = ia->flag;
						aa->sta = ia->sta;
						aa->end = ia->end;
						BLI_strncpy(aa->name, ia->name, sizeof(aa->name));
						BLI_strncpy(aa->frameProp, ia->frameProp, sizeof(aa->frameProp));
						if (ob->adt)
							aa->act = ob->adt->action;

						// Get rid of the old actuator
						MEM_freeN(ia);

						// Assign the new actuator
						act->data = aa;
						act->type= act->otype= ACT_ACTION;
						
					}
					else if (act->type == ACT_SHAPEACTION) {
						act->type = act->otype = ACT_ACTION;
					}
				}
			}
		}
	}

	if (main->versionfile < 259 || (main->versionfile == 259 && main->subversionfile < 2)) {
		{
			/* Convert default socket values from bNodeStack */
			Scene *sce;
			Material *mat;
			Tex *tex;
			bNodeTree *ntree;
			for (ntree=main->nodetree.first; ntree; ntree=ntree->id.next) {
				do_versions_nodetree_default_value(ntree);
				ntree->update |= NTREE_UPDATE;
			}
			for (sce=main->scene.first; sce; sce=sce->id.next)
				if (sce->nodetree) {
				do_versions_nodetree_default_value(sce->nodetree);
				sce->nodetree->update |= NTREE_UPDATE;
			}
			for (mat=main->mat.first; mat; mat=mat->id.next)
				if (mat->nodetree) {
				do_versions_nodetree_default_value(mat->nodetree);
				mat->nodetree->update |= NTREE_UPDATE;
			}
			for (tex=main->tex.first; tex; tex=tex->id.next)
				if (tex->nodetree) {
				do_versions_nodetree_default_value(tex->nodetree);
				tex->nodetree->update |= NTREE_UPDATE;
			}
		}

		/* add SOCK_DYNAMIC flag to existing group sockets */
		{
			bNodeTree *ntree;
			/* only need to do this for trees in main, local trees are not used as groups */
			for (ntree=main->nodetree.first; ntree; ntree=ntree->id.next) {
				do_versions_nodetree_dynamic_sockets(ntree);
				ntree->update |= NTREE_UPDATE;
			}
		}

		{
			/* Initialize group tree nodetypes.
			 * These are used to distinguish tree types and
			 * associate them with specific node types for polling.
			 */
			bNodeTree *ntree;
			/* all node trees in main->nodetree are considered groups */
			for (ntree=main->nodetree.first; ntree; ntree=ntree->id.next)
				ntree->nodetype = NODE_GROUP;
		}
	}

	if (main->versionfile < 259 || (main->versionfile == 259 && main->subversionfile < 4)) {
		{
			/* Adaptive time step for particle systems */
			ParticleSettings *part;
			for (part = main->particle.first; part; part = part->id.next) {
				part->courant_target = 0.2f;
				part->time_flag &= ~PART_TIME_AUTOSF;
			}
		}

		{
			/* set defaults for obstacle avoidance, recast data */
			Scene *sce;
			for (sce = main->scene.first; sce; sce = sce->id.next)
			{
				if (sce->gm.levelHeight == 0.f)
					sce->gm.levelHeight = 2.f;

				if (sce->gm.recastData.cellsize == 0.0f)
					sce->gm.recastData.cellsize = 0.3f;
				if (sce->gm.recastData.cellheight == 0.0f)
					sce->gm.recastData.cellheight = 0.2f;
				if (sce->gm.recastData.agentmaxslope == 0.0f)
					sce->gm.recastData.agentmaxslope = (float)M_PI/4;
				if (sce->gm.recastData.agentmaxclimb == 0.0f)
					sce->gm.recastData.agentmaxclimb = 0.9f;
				if (sce->gm.recastData.agentheight == 0.0f)
					sce->gm.recastData.agentheight = 2.0f;
				if (sce->gm.recastData.agentradius == 0.0f)
					sce->gm.recastData.agentradius = 0.6f;
				if (sce->gm.recastData.edgemaxlen == 0.0f)
					sce->gm.recastData.edgemaxlen = 12.0f;
				if (sce->gm.recastData.edgemaxerror == 0.0f)
					sce->gm.recastData.edgemaxerror = 1.3f;
				if (sce->gm.recastData.regionminsize == 0.0f)
					sce->gm.recastData.regionminsize = 8.f;
				if (sce->gm.recastData.regionmergesize == 0.0f)
					sce->gm.recastData.regionmergesize = 20.f;
				if (sce->gm.recastData.vertsperpoly<3)
					sce->gm.recastData.vertsperpoly = 6;
				if (sce->gm.recastData.detailsampledist == 0.0f)
					sce->gm.recastData.detailsampledist = 6.0f;
				if (sce->gm.recastData.detailsamplemaxerror == 0.0f)
					sce->gm.recastData.detailsamplemaxerror = 1.0f;
			}
		}
	}

	if (main->versionfile < 260) {
		{
			/* set default alpha value of Image outputs in image and render layer nodes to 0 */
			Scene *sce;
			bNodeTree *ntree;
			
			for (sce=main->scene.first; sce; sce=sce->id.next) {
				/* there are files with invalid audio_channels value, the real cause
				   is unknown, but we fix it here anyway to avoid crashes */
				if (sce->r.ffcodecdata.audio_channels == 0)
					sce->r.ffcodecdata.audio_channels = 2;

				if (sce->nodetree)
					do_versions_nodetree_image_default_alpha_output(sce->nodetree);
			}

			for (ntree=main->nodetree.first; ntree; ntree=ntree->id.next)
				do_versions_nodetree_image_default_alpha_output(ntree);
		}

		{
			/* support old particle dupliobject rotation settings */
			ParticleSettings *part;

			for (part=main->particle.first; part; part=part->id.next) {
				if (ELEM(part->ren_as, PART_DRAW_OB, PART_DRAW_GR)) {
					part->draw |= PART_DRAW_ROTATE_OB;

					if (part->rotmode == 0)
						part->rotmode = PART_ROT_VEL;
				}
			}
		}
	}

	if (main->versionfile < 260 || (main->versionfile == 260 && main->subversionfile < 1)) {
		Object *ob;

		for (ob= main->object.first; ob; ob= ob->id.next) {
			ob->collision_boundtype= ob->boundtype;
		}

		{
			Camera *cam;
			for (cam= main->camera.first; cam; cam= cam->id.next) {
				if (cam->sensor_x < 0.01f)
					cam->sensor_x = DEFAULT_SENSOR_WIDTH;

				if (cam->sensor_y < 0.01f)
					cam->sensor_y = DEFAULT_SENSOR_HEIGHT;
			}
		}
	}

	if (main->versionfile < 260 || (main->versionfile == 260 && main->subversionfile < 2)) {
		bNodeTreeType *ntreetype= ntreeGetType(NTREE_SHADER);

		if (ntreetype && ntreetype->foreach_nodetree)
			ntreetype->foreach_nodetree(main, NULL, do_version_ntree_tex_mapping_260);
	}

	if (main->versionfile < 260 || (main->versionfile == 260 && main->subversionfile < 4)) {
		{
			/* Convert node angles to radians! */
			Scene *sce;
			Material *mat;
			bNodeTree *ntree;

			for (sce=main->scene.first; sce; sce=sce->id.next) {
				if (sce->nodetree)
					do_versions_nodetree_convert_angle(sce->nodetree);
			}

			for (mat=main->mat.first; mat; mat=mat->id.next) {
				if (mat->nodetree)
					do_versions_nodetree_convert_angle(mat->nodetree);
			}

			for (ntree=main->nodetree.first; ntree; ntree=ntree->id.next)
				do_versions_nodetree_convert_angle(ntree);
		}

		{
			/* Tomato compatibility code. */
			bScreen *sc;
			MovieClip *clip;

			for (sc= main->screen.first; sc; sc= sc->id.next) {
				ScrArea *sa;
				for (sa= sc->areabase.first; sa; sa= sa->next) {
					SpaceLink *sl;
					for (sl= sa->spacedata.first; sl; sl= sl->next) {
						if (sl->spacetype==SPACE_VIEW3D) {
							View3D *v3d= (View3D *)sl;
							if (v3d->bundle_size==0.0f) {
								v3d->bundle_size= 0.2f;
								v3d->flag2 |= V3D_SHOW_RECONSTRUCTION;
							}
							else if (sl->spacetype==SPACE_CLIP) {
								SpaceClip *sc= (SpaceClip *)sl;
								if (sc->scopes.track_preview_height==0)
									sc->scopes.track_preview_height= 120;
							}

							if (v3d->bundle_drawtype==0)
								v3d->bundle_drawtype= OB_PLAINAXES;
						}
					}
				}
			}

			for (clip= main->movieclip.first; clip; clip= clip->id.next) {
				MovieTrackingTrack *track;

				if (clip->aspx<1.0f) {
					clip->aspx= 1.0f;
					clip->aspy= 1.0f;
				}

				clip->proxy.build_tc_flag= IMB_TC_RECORD_RUN |
				                           IMB_TC_FREE_RUN |
				                           IMB_TC_INTERPOLATED_REC_DATE_FREE_RUN;

				if (clip->proxy.build_size_flag==0)
					clip->proxy.build_size_flag= IMB_PROXY_25;

				if (clip->proxy.quality==0)
					clip->proxy.quality= 90;

				if (clip->tracking.camera.pixel_aspect<0.01f)
					clip->tracking.camera.pixel_aspect= 1.f;

				track= clip->tracking.tracks.first;
				while (track) {
					if (track->pyramid_levels==0)
						track->pyramid_levels= 2;

					if (track->minimum_correlation==0.0f)
						track->minimum_correlation= 0.75f;

					track= track->next;
				}
			}
		}
	}

	if (main->versionfile < 260 || (main->versionfile == 260 && main->subversionfile < 6))
	{
		Scene *sce;
		MovieClip *clip;
		bScreen *sc;

		for (sce = main->scene.first; sce; sce = sce->id.next) {
			do_versions_image_settings_2_60(sce);
		}

		for (clip= main->movieclip.first; clip; clip= clip->id.next) {
			MovieTrackingSettings *settings= &clip->tracking.settings;

			if (settings->default_pyramid_levels==0) {
				settings->default_tracker= TRACKER_KLT;
				settings->default_pyramid_levels= 2;
				settings->default_minimum_correlation= 0.75;
				settings->default_pattern_size= 11;
				settings->default_search_size= 51;
			}
		}

		for (sc= main->screen.first; sc; sc= sc->id.next) {
			ScrArea *sa;
			for (sa= sc->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if (sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D *)sl;
						v3d->flag2&= ~V3D_RENDER_SHADOW;
					}
				}
			}
		}

		{
			Object *ob;
			for (ob= main->object.first; ob; ob= ob->id.next) {
				/* convert delta addition into delta scale */
				int i;
				for (i= 0; i < 3; i++) {
					if ( (ob->dsize[i] == 0.0f) || /* simple case, user never touched dsize */
					     (ob->size[i]  == 0.0f))   /* cant scale the dsize to give a non zero result, so fallback to 1.0f */
					{
						ob->dscale[i]= 1.0f;
					}
					else {
						ob->dscale[i]= (ob->size[i] + ob->dsize[i]) / ob->size[i];
					}
				}
			}
		}
	}
	/* sigh, this dscale vs dsize version patching was not done right, fix for fix,
	 * this intentionally checks an exact subversion, also note this was never in a release,
	 * at some point this could be removed. */
	else if (main->versionfile == 260 && main->subversionfile == 6) {
		Object *ob;
		for (ob = main->object.first; ob; ob= ob->id.next) {
			if (is_zero_v3(ob->dscale)) {
				fill_vn_fl(ob->dscale, 3, 1.0f);
			}
		}
	}

	if (main->versionfile < 260 || (main->versionfile == 260 && main->subversionfile < 8))
	{
		Brush *brush;

		for (brush= main->brush.first; brush; brush= brush->id.next) {
			if (brush->sculpt_tool == SCULPT_TOOL_ROTATE)
				brush->alpha= 1.0f;
		}
	}

	if (main->versionfile < 261 || (main->versionfile == 261 && main->subversionfile < 1))
	{
		{
			/* update use flags for node sockets (was only temporary before) */
			Scene *sce;
			Material *mat;
			Tex *tex;
			Lamp *lamp;
			World *world;
			bNodeTree *ntree;

			for (sce=main->scene.first; sce; sce=sce->id.next)
				if (sce->nodetree)
					do_versions_nodetree_socket_use_flags_2_62(sce->nodetree);

			for (mat=main->mat.first; mat; mat=mat->id.next)
				if (mat->nodetree)
					do_versions_nodetree_socket_use_flags_2_62(mat->nodetree);

			for (tex=main->tex.first; tex; tex=tex->id.next)
				if (tex->nodetree)
					do_versions_nodetree_socket_use_flags_2_62(tex->nodetree);

			for (lamp=main->lamp.first; lamp; lamp=lamp->id.next)
				if (lamp->nodetree)
					do_versions_nodetree_socket_use_flags_2_62(lamp->nodetree);

			for (world=main->world.first; world; world=world->id.next)
				if (world->nodetree)
					do_versions_nodetree_socket_use_flags_2_62(world->nodetree);

			for (ntree=main->nodetree.first; ntree; ntree=ntree->id.next)
				do_versions_nodetree_socket_use_flags_2_62(ntree);
		}
		{
			/* Initialize BGE exit key to esc key */
			Scene *scene;
			for (scene= main->scene.first; scene; scene= scene->id.next) {
				if (!scene->gm.exitkey)
					scene->gm.exitkey = 218; // Blender key code for ESC
			}
		}
		{
			MovieClip *clip;
			Object *ob;

			for (clip= main->movieclip.first; clip; clip= clip->id.next) {
				MovieTracking *tracking= &clip->tracking;
				MovieTrackingObject *tracking_object= tracking->objects.first;

				clip->proxy.build_tc_flag|= IMB_TC_RECORD_RUN_NO_GAPS;

				if (!tracking->settings.object_distance)
					tracking->settings.object_distance= 1.0f;

				if (tracking->objects.first == NULL)
					BKE_tracking_new_object(tracking, "Camera");

				while (tracking_object) {
					if (!tracking_object->scale)
						tracking_object->scale= 1.0f;

					tracking_object= tracking_object->next;
				}
			}

			for (ob= main->object.first; ob; ob= ob->id.next) {
				bConstraint *con;
				for (con= ob->constraints.first; con; con=con->next) {
					bConstraintTypeInfo *cti= constraint_get_typeinfo(con);

					if (!cti)
						continue;

					if (cti->type==CONSTRAINT_TYPE_OBJECTSOLVER) {
						bObjectSolverConstraint *data= (bObjectSolverConstraint *)con->data;

						if (data->invmat[3][3]==0.0f)
							unit_m4(data->invmat);
					}
				}
			}
		}
		{
		/* Warn the user if he is using ["Text"] properties for Font objects */
			Object *ob;
			bProperty *prop;

			for (ob= main->object.first; ob; ob= ob->id.next) {
				if (ob->type == OB_FONT) {
					prop = get_ob_property(ob, "Text");
					if (prop) {
						BKE_reportf_wrap(fd->reports, RPT_WARNING,
						                 "Game property name conflict in object: \"%s\".\nText objects reserve the "
						                 "[\"Text\"] game property to change their content through Logic Bricks.\n",
						                 ob->id.name+2);
					}
				}
			}
		}
		{
			/* set the SOCK_AUTO_HIDDEN flag on collapsed nodes */
			Scene *sce;
			Material *mat;
			Tex *tex;
			Lamp *lamp;
			World *world;
			bNodeTree *ntree;

			for (sce=main->scene.first; sce; sce=sce->id.next)
				if (sce->nodetree)
					do_versions_nodetree_socket_auto_hidden_flags_2_62(sce->nodetree);

			for (mat=main->mat.first; mat; mat=mat->id.next)
				if (mat->nodetree)
					do_versions_nodetree_socket_auto_hidden_flags_2_62(mat->nodetree);

			for (tex=main->tex.first; tex; tex=tex->id.next)
				if (tex->nodetree)
					do_versions_nodetree_socket_auto_hidden_flags_2_62(tex->nodetree);

			for (lamp=main->lamp.first; lamp; lamp=lamp->id.next)
				if (lamp->nodetree)
					do_versions_nodetree_socket_auto_hidden_flags_2_62(lamp->nodetree);

			for (world=main->world.first; world; world=world->id.next)
				if (world->nodetree)
					do_versions_nodetree_socket_auto_hidden_flags_2_62(world->nodetree);

			for (ntree=main->nodetree.first; ntree; ntree=ntree->id.next)
				do_versions_nodetree_socket_auto_hidden_flags_2_62(ntree);
		}
	}

	if (main->versionfile < 261 || (main->versionfile == 261 && main->subversionfile < 2))
	{
		{
			/* convert Camera Actuator values to defines */
			Object *ob;
			bActuator *act;
			for (ob = main->object.first; ob; ob= ob->id.next) {
				for (act= ob->actuators.first; act; act= act->next) {
					if (act->type == ACT_CAMERA) {
						bCameraActuator *ba= act->data;

						if (ba->axis==(float) 'x') ba->axis=OB_POSX;
						else if (ba->axis==(float)'y') ba->axis=OB_POSY;
						/* don't do an if/else to avoid imediate subversion bump*/
//					ba->axis=((ba->axis == (float) 'x')?OB_POSX_X:OB_POSY);
					}
				}
			}
		}

		{
			/* convert deprecated sculpt_paint_unified_* fields to
			   UnifiedPaintSettings */
			Scene *scene;
			for (scene= main->scene.first; scene; scene= scene->id.next) {
				ToolSettings *ts= scene->toolsettings;
				UnifiedPaintSettings *ups= &ts->unified_paint_settings;
				ups->size= ts->sculpt_paint_unified_size;
				ups->unprojected_radius= ts->sculpt_paint_unified_unprojected_radius;
				ups->alpha= ts->sculpt_paint_unified_alpha;
				ups->flag= ts->sculpt_paint_settings;
			}
		}
	}

	if (main->versionfile < 261 || (main->versionfile == 261 && main->subversionfile < 3))
	{
		{
			/* convert extended ascii to utf-8 for text editor */
			Text *text;
			for (text= main->text.first; text; text= text->id.next)
				if (!(text->flags & TXT_ISEXT)) {
					TextLine *tl;
					
					for (tl= text->lines.first; tl; tl= tl->next) {
						int added= txt_extended_ascii_as_utf8(&tl->line);
						tl->len+= added;
						
						/* reset cursor position if line was changed */
						if (added && tl == text->curl)
							text->curc = 0;
					}
				}
		}
		{
			/* set new dynamic paint values */
			Object *ob;
			for (ob = main->object.first; ob; ob = ob->id.next) {
				ModifierData *md;
				for (md= ob->modifiers.first; md; md= md->next) {
					if (md->type == eModifierType_DynamicPaint) {
						DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
						if (pmd->canvas)
						{
							DynamicPaintSurface *surface = pmd->canvas->surfaces.first;
							for (; surface; surface=surface->next) {
								surface->color_dry_threshold = 1.0f;
								surface->influence_scale = 1.0f;
								surface->radius_scale = 1.0f;
								surface->flags |= MOD_DPAINT_USE_DRYING;
							}
						}
					}
				}
			}
		}
	}
	
	if (main->versionfile < 262)
	{
		Object *ob;
		for (ob=main->object.first; ob; ob= ob->id.next) {
			ModifierData *md;

			for (md=ob->modifiers.first; md; md=md->next) {
				if (md->type==eModifierType_Cloth) {
					ClothModifierData *clmd = (ClothModifierData*) md;
					if (clmd->sim_parms)
						clmd->sim_parms->vel_damping = 1.0f;
				}
			}
		}
	}

	if (main->versionfile < 263)
	{
		/* set fluidsim rate. the version patch for this in 2.62 was wrong, so
		   try to correct it, if rate is 0.0 that's likely not intentional */
		Object *ob;

		for (ob = main->object.first; ob; ob = ob->id.next) {
			ModifierData *md;
			for (md = ob->modifiers.first; md; md = md->next) {
				if (md->type == eModifierType_Fluidsim) {
					FluidsimModifierData *fmd = (FluidsimModifierData *)md;
					if (fmd->fss->animRate == 0.0f)
						fmd->fss->animRate = 1.0f;
				}
			}
		}
	}

	if (main->versionfile < 262 || (main->versionfile == 262 && main->subversionfile < 1))
	{
		/* update use flags for node sockets (was only temporary before) */
		Scene *sce;
		bNodeTree *ntree;
		
		for (sce=main->scene.first; sce; sce=sce->id.next)
			if (sce->nodetree)
				do_versions_nodetree_multi_file_output_format_2_62_1(sce, sce->nodetree);
		
		/* XXX can't associate with scene for group nodes, image format will stay uninitialized */
		for (ntree=main->nodetree.first; ntree; ntree=ntree->id.next)
			do_versions_nodetree_multi_file_output_format_2_62_1(NULL, ntree);
	}

	/* only swap for pre-release bmesh merge which had MLoopCol red/blue swap */
	if (main->versionfile == 262 && main->subversionfile == 1)
	{
		{
			Mesh *me;
			for (me = main->mesh.first; me; me = me->id.next) {
				do_versions_mesh_mloopcol_swap_2_62_1(me);
			}
		}

	}

	if (main->versionfile < 262 || (main->versionfile == 262 && main->subversionfile < 2))
	{
		{
			/* Set new idname of keyingsets from their now "label-only" name. */
			Scene *scene;
			for (scene = main->scene.first; scene; scene = scene->id.next) {
				KeyingSet *ks;
				for (ks = scene->keyingsets.first; ks; ks = ks->next) {
					if (!ks->idname[0])
						BLI_strncpy(ks->idname, ks->name, sizeof(ks->idname));
				}
			}
		}
	}
	
	if (main->versionfile < 262 || (main->versionfile == 262 && main->subversionfile < 3))
	{
		Object *ob;
		ModifierData *md;
	
		for(ob = main->object.first; ob; ob = ob->id.next) {
			for(md=ob->modifiers.first; md; md=md->next) {
				if(md->type == eModifierType_Lattice) {
					LatticeModifierData *lmd = (LatticeModifierData *)md;
					lmd->strength = 1.0f;
				}
			}
		}
	}


	{
		/* Default for old files is to save particle rotations to pointcache */
		ParticleSettings *part;
		for (part = main->particle.first; part; part = part->id.next)
			part->flag |= PART_ROTATIONS;
	}

	/* put compatibility code here until next subversion bump */
	{

	}

	/* WATCH IT!!!: pointers from libdata have not been converted yet here! */
	/* WATCH IT 2!: Userdef struct init has to be in editors/interface/resources.c! */

	/* don't forget to set version number in blender.c! */
}

#if 0 // XXX: disabled for now... we still don't have this in the right place in the loading code for it to work
static void do_versions_after_linking(FileData *fd, Library *lib, Main *main)
{
	/* old Animation System (using IPO's) needs to be converted to the new Animato system */
	if (main->versionfile < 250)
		do_versions_ipos_to_animato(main);
}
#endif

static void lib_link_all(FileData *fd, Main *main)
{
	oldnewmap_sort(fd);
	
	lib_link_windowmanager(fd, main);
	lib_link_screen(fd, main);
	lib_link_scene(fd, main);
	lib_link_object(fd, main);
	lib_link_curve(fd, main);
	lib_link_mball(fd, main);
	lib_link_material(fd, main);
	lib_link_texture(fd, main);
	lib_link_image(fd, main);
	lib_link_ipo(fd, main);		// XXX depreceated... still needs to be maintained for version patches still
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
	lib_link_nodetree(fd, main);	/* has to be done after scene/materials, this will verify group nodes */
	lib_link_brush(fd, main);
	lib_link_particlesettings(fd, main);
	lib_link_movieclip(fd, main);

	lib_link_mesh(fd, main);		/* as last: tpage images with users at zero */

	lib_link_library(fd, main);		/* only init users */
}

static void direct_link_keymapitem(FileData *fd, wmKeyMapItem *kmi)
{
	kmi->properties= newdataadr(fd, kmi->properties);
	if (kmi->properties)
		IDP_DirectLinkProperty(kmi->properties, (fd->flags & FD_FLAGS_SWITCH_ENDIAN), fd);
	kmi->ptr= NULL;
	kmi->flag &= ~KMI_UPDATE;
}

static BHead *read_userdef(BlendFileData *bfd, FileData *fd, BHead *bhead)
{
	UserDef *user;
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	wmKeyMapDiffItem *kmdi;

	bfd->user= user= read_struct(fd, bhead, "user def");

	/* read all data into fd->datamap */
	bhead= read_data_into_oldnewmap(fd, bhead, "user def");

	if (user->keymaps.first) {
		/* backwards compatibility */
		user->user_keymaps= user->keymaps;
		user->keymaps.first= user->keymaps.last= NULL;
	}

	link_list(fd, &user->themes);
	link_list(fd, &user->user_keymaps);
	link_list(fd, &user->addons);

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

	// XXX
	user->uifonts.first= user->uifonts.last= NULL;
	
	link_list(fd, &user->uistyles);

	/* free fd->datamap again */
	oldnewmap_free_unused(fd->datamap);
	oldnewmap_clear(fd->datamap);

	return bhead;
}

BlendFileData *blo_read_file_internal(FileData *fd, const char *filepath)
{
	BHead *bhead= blo_firstbhead(fd);
	BlendFileData *bfd;

	bfd= MEM_callocN(sizeof(BlendFileData), "blendfiledata");
	bfd->main= MEM_callocN(sizeof(Main), "readfile_Main");
	BLI_addtail(&fd->mainlist, bfd->main);

	bfd->main->versionfile= fd->fileversion;
	
	bfd->type= BLENFILETYPE_BLEND;
	BLI_strncpy(bfd->main->name, filepath, sizeof(bfd->main->name));

	while (bhead) {
		switch(bhead->code) {
		case DATA:
		case DNA1:
		case TEST: /* used as preview since 2.5x */
		case REND:
			bhead = blo_nextbhead(fd, bhead);
			break;
		case GLOB:
			bhead= read_global(bfd, fd, bhead);
			break;
		case USER:
			bhead= read_userdef(bfd, fd, bhead);
			break;
		case ENDB:
			bhead = NULL;
			break;

		case ID_LI:
			/* skip library datablocks in undo, this works together with
			   BLO_read_from_memfile, where the old main->library is restored
			   overwriting  the libraries from the memory file. previously
			   it did not save ID_LI/ID_ID blocks in this case, but they are
			   needed to make quit.blend recover them correctly. */
			if (fd->memfile)
				bhead= blo_nextbhead(fd, bhead);
			else
				bhead= read_libblock(fd, bfd->main, bhead, LIB_LOCAL, NULL);
			break;
		case ID_ID:
			/* same as above */
			if (fd->memfile)
				bhead= blo_nextbhead(fd, bhead);
			else
				/* always adds to the most recently loaded
				 * ID_LI block, see direct_link_library.
				 * this is part of the file format definition. */
				bhead = read_libblock(fd, fd->mainlist.last, bhead, LIB_READ+LIB_EXTERN, NULL);
			break;
			
			/* in 2.50+ files, the file identifier for screens is patched, forward compatibility */
		case ID_SCRN:
			bhead->code= ID_SCR;
			/* deliberate pass on to default */
		default:
			bhead = read_libblock(fd, bfd->main, bhead, LIB_LOCAL, NULL);
		}
	}

	/* do before read_libraries, but skip undo case */
//	if (fd->memfile==NULL) (the mesh shuffle hacks don't work yet? ton)
		do_versions(fd, NULL, bfd->main);

	read_libraries(fd, &fd->mainlist);
	
	blo_join_main(&fd->mainlist);

	lib_link_all(fd, bfd->main);
	//do_versions_after_linking(fd, NULL, bfd->main); // XXX: not here (or even in this function at all)! this causes crashes on many files - Aligorith (July 04, 2010)
	lib_verify_nodetree(bfd->main, TRUE);
	fix_relpaths_library(fd->relabase, bfd->main); /* make all relative paths, relative to the open blend file */
	
	link_global(fd, bfd);	/* as last */
	
	return bfd;
}

/* ************* APPEND LIBRARY ************** */

struct bheadsort {
	BHead *bhead;
	void *old;
};

static int verg_bheadsort(const void *v1, const void *v2)
{
	const struct bheadsort *x1=v1, *x2=v2;
	
	if ( x1->old > x2->old) return 1;
	else if ( x1->old < x2->old) return -1;
	return 0;
}

static void sort_bhead_old_map(FileData *fd)
{
	BHead *bhead;
	struct bheadsort *bhs;
	int tot= 0;
	
	for (bhead= blo_firstbhead(fd); bhead; bhead= blo_nextbhead(fd, bhead))
		tot++;
	
	fd->tot_bheadmap= tot;
	if (tot==0) return;
	
	bhs= fd->bheadmap= MEM_mallocN(tot*sizeof(struct bheadsort), "bheadsort");
	
	for (bhead= blo_firstbhead(fd); bhead; bhead= blo_nextbhead(fd, bhead), bhs++) {
		bhs->bhead= bhead;
		bhs->old= bhead->old;
	}
	
	qsort(fd->bheadmap, tot, sizeof(struct bheadsort), verg_bheadsort);
		
}

static BHead *find_previous_lib(FileData *fd, BHead *bhead)
{
	/* skip library datablocks in undo, see comment in read_libblock */
	if (fd->memfile)
		return NULL;

	for (; bhead; bhead= blo_prevbhead(fd, bhead))
		if (bhead->code==ID_LI)
			break;

	return bhead;
}

static BHead *find_bhead(FileData *fd, void *old)
{
#if 0
	BHead *bhead;
#endif
	struct bheadsort *bhs, bhs_s;
	
	if (!old)
		return NULL;

	if (fd->bheadmap==NULL)
		sort_bhead_old_map(fd);
	
	bhs_s.old= old;
	bhs= bsearch(&bhs_s, fd->bheadmap, fd->tot_bheadmap, sizeof(struct bheadsort), verg_bheadsort);

	if (bhs)
		return bhs->bhead;
	
#if 0
	for (bhead= blo_firstbhead(fd); bhead; bhead= blo_nextbhead(fd, bhead))
		if (bhead->old==old)
			return bhead;
#endif

	return NULL;
}

char *bhead_id_name(FileData *fd, BHead *bhead)
{
	return ((char *)(bhead+1)) + fd->id_name_offs;
}

static ID *is_yet_read(FileData *fd, Main *mainvar, BHead *bhead)
{
	const char *idname= bhead_id_name(fd, bhead);
	/* which_libbase can be NULL, intentionally not using idname+2 */
	return BLI_findstring(which_libbase(mainvar, GS(idname)), idname, offsetof(ID, name));
}

static void expand_doit(FileData *fd, Main *mainvar, void *old)
{
	BHead *bhead;
	ID *id;

	bhead= find_bhead(fd, old);
	if (bhead) {
			/* from another library? */
		if (bhead->code==ID_ID) {
			BHead *bheadlib= find_previous_lib(fd, bhead);

			if (bheadlib) {
				Library *lib= read_struct(fd, bheadlib, "Library");
				Main *ptr= blo_find_main(fd, &fd->mainlist, lib->name, fd->relabase);

				id= is_yet_read(fd, ptr, bhead);

				if (id==NULL) {
					read_libblock(fd, ptr, bhead, LIB_READ+LIB_INDIRECT, NULL);
					// commented because this can print way too much
					// if (G.debug & G_DEBUG) printf("expand_doit: other lib %s\n", lib->name);
					
					/* for outliner dependency only */
					ptr->curlib->parent= mainvar->curlib;
				}
				else {
					/* The line below was commented by Ton (I assume), when Hos did the merge from the orange branch. rev 6568
					 * This line is NEEDED, the case is that you have 3 blend files...
					 * user.blend, lib.blend and lib_indirect.blend - if user.blend already references a "tree" from
					 * lib_indirect.blend but lib.blend does too, linking in a Scene or Group from lib.blend can result in an
					 * empty without the dupli group referenced. Once you save and reload the group would appier. - Campbell */
					/* This crashes files, must look further into it */
					
					/* Update: the issue is that in file reading, the oldnewmap is OK, but for existing data, it has to be
					   inserted in the map to be found! */
					if (id->flag & LIB_PRE_EXISTING)
						oldnewmap_insert(fd->libmap, bhead->old, id, 1);
					
					change_idid_adr_fd(fd, bhead->old, id);
					// commented because this can print way too much
					// if (G.debug & G_DEBUG) printf("expand_doit: already linked: %s lib: %s\n", id->name, lib->name);
				}
				
				MEM_freeN(lib);
			}
		}
		else {
			id= is_yet_read(fd, mainvar, bhead);
			if (id==NULL) {
				read_libblock(fd, mainvar, bhead, LIB_TESTIND, NULL);
			}
			else {
				/* this is actually only needed on UI call? when ID was already read before, and another append
				   happens which invokes same ID... in that case the lookup table needs this entry */
				oldnewmap_insert(fd->libmap, bhead->old, id, 1);
				// commented because this can print way too much
				// if (G.debug & G_DEBUG) printf("expand: already read %s\n", id->name);
			}
		}
	}
}



// XXX depreceated - old animation system
static void expand_ipo(FileData *fd, Main *mainvar, Ipo *ipo)
{
	IpoCurve *icu;
	for (icu= ipo->curve.first; icu; icu= icu->next) {
		if (icu->driver)
			expand_doit(fd, mainvar, icu->driver->ob);
	}
}

// XXX depreceated - old animation system
static void expand_constraint_channels(FileData *fd, Main *mainvar, ListBase *chanbase)
{
	bConstraintChannel *chan;
	for (chan=chanbase->first; chan; chan=chan->next) {
		expand_doit(fd, mainvar, chan->ipo);
	}
}

static void expand_fmodifiers(FileData *fd, Main *mainvar, ListBase *list)
{
	FModifier *fcm;
	
	for (fcm= list->first; fcm; fcm= fcm->next) {
		/* library data for specific F-Modifier types */
		switch (fcm->type) {
			case FMODIFIER_TYPE_PYTHON:
			{
				FMod_Python *data= (FMod_Python *)fcm->data;
				
				expand_doit(fd, mainvar, data->script);
			}
				break;
		}
	}
}

static void expand_fcurves(FileData *fd, Main *mainvar, ListBase *list)
{
	FCurve *fcu;
	
	for (fcu= list->first; fcu; fcu= fcu->next) {
		/* Driver targets if there is a driver */
		if (fcu->driver) {
			ChannelDriver *driver= fcu->driver;
			DriverVar *dvar;
			
			for (dvar= driver->variables.first; dvar; dvar= dvar->next) {
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
	
	// XXX depreceated - old animation system --------------
	for (chan=act->chanbase.first; chan; chan=chan->next) {
		expand_doit(fd, mainvar, chan->ipo);
		expand_constraint_channels(fd, mainvar, &chan->constraintChannels);
	}
	// ---------------------------------------------------
	
	/* F-Curves in Action */
	expand_fcurves(fd, mainvar, &act->curves);
}

static void expand_keyingsets(FileData *fd, Main *mainvar, ListBase *list)
{
	KeyingSet *ks;
	KS_Path *ksp;
	
	/* expand the ID-pointers in KeyingSets's paths */
	for (ks= list->first; ks; ks= ks->next) {
		for (ksp= ks->paths.first; ksp; ksp= ksp->next) {
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
	for (nlt= adt->nla_tracks.first; nlt; nlt= nlt->next) 
		expand_animdata_nlastrips(fd, mainvar, &nlt->strips);
}	

static void expand_particlesettings(FileData *fd, Main *mainvar, ParticleSettings *part)
{
	int a;

	expand_doit(fd, mainvar, part->dup_ob);
	expand_doit(fd, mainvar, part->dup_group);
	expand_doit(fd, mainvar, part->eff_group);
	expand_doit(fd, mainvar, part->bb_ob);
	
	if (part->adt)
		expand_animdata(fd, mainvar, part->adt);

	for (a=0; a<MAX_MTEX; a++) {
		if (part->mtex[a]) {
			expand_doit(fd, mainvar, part->mtex[a]->tex);
			expand_doit(fd, mainvar, part->mtex[a]->object);
		}
	}
}

static void expand_group(FileData *fd, Main *mainvar, Group *group)
{
	GroupObject *go;
	
	for (go= group->gobject.first; go; go= go->next) {
		expand_doit(fd, mainvar, go->ob);
	}
}

static void expand_key(FileData *fd, Main *mainvar, Key *key)
{
	expand_doit(fd, mainvar, key->ipo); // XXX depreceated - old animation system
	
	if (key->adt)
		expand_animdata(fd, mainvar, key->adt);
}

static void expand_nodetree(FileData *fd, Main *mainvar, bNodeTree *ntree)
{
	bNode *node;
	
	if (ntree->adt)
		expand_animdata(fd, mainvar, ntree->adt);
		
	if (ntree->gpd)
		expand_doit(fd, mainvar, ntree->gpd);
	
	for (node= ntree->nodes.first; node; node= node->next)
		if (node->id && node->type!=CMP_NODE_R_LAYERS)
			expand_doit(fd, mainvar, node->id);

}

static void expand_texture(FileData *fd, Main *mainvar, Tex *tex)
{
	expand_doit(fd, mainvar, tex->ima);
	expand_doit(fd, mainvar, tex->ipo); // XXX depreceated - old animation system
	
	if (tex->adt)
		expand_animdata(fd, mainvar, tex->adt);
	
	if (tex->nodetree)
		expand_nodetree(fd, mainvar, tex->nodetree);
}

static void expand_brush(FileData *fd, Main *mainvar, Brush *brush)
{
	expand_doit(fd, mainvar, brush->mtex.tex);
	expand_doit(fd, mainvar, brush->clone.image);
}

static void expand_material(FileData *fd, Main *mainvar, Material *ma)
{
	int a;

	for (a=0; a<MAX_MTEX; a++) {
		if (ma->mtex[a]) {
			expand_doit(fd, mainvar, ma->mtex[a]->tex);
			expand_doit(fd, mainvar, ma->mtex[a]->object);
		}
	}
	
	expand_doit(fd, mainvar, ma->ipo); // XXX depreceated - old animation system
	
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

	for (a=0; a<MAX_MTEX; a++) {
		if (la->mtex[a]) {
			expand_doit(fd, mainvar, la->mtex[a]->tex);
			expand_doit(fd, mainvar, la->mtex[a]->object);
		}
	}
	
	expand_doit(fd, mainvar, la->ipo); // XXX depreceated - old animation system
	
	if (la->adt)
		expand_animdata(fd, mainvar, la->adt);

	if (la->nodetree)
		expand_nodetree(fd, mainvar, la->nodetree);
}

static void expand_lattice(FileData *fd, Main *mainvar, Lattice *lt)
{
	expand_doit(fd, mainvar, lt->ipo); // XXX depreceated - old animation system
	expand_doit(fd, mainvar, lt->key);
	
	if (lt->adt)
		expand_animdata(fd, mainvar, lt->adt);
}


static void expand_world(FileData *fd, Main *mainvar, World *wrld)
{
	int a;

	for (a=0; a<MAX_MTEX; a++) {
		if (wrld->mtex[a]) {
			expand_doit(fd, mainvar, wrld->mtex[a]->tex);
			expand_doit(fd, mainvar, wrld->mtex[a]->object);
		}
	}
	
	expand_doit(fd, mainvar, wrld->ipo); // XXX depreceated - old animation system
	
	if (wrld->adt)
		expand_animdata(fd, mainvar, wrld->adt);

	if (wrld->nodetree)
		expand_nodetree(fd, mainvar, wrld->nodetree);
}


static void expand_mball(FileData *fd, Main *mainvar, MetaBall *mb)
{
	int a;

	for (a=0; a<mb->totcol; a++) {
		expand_doit(fd, mainvar, mb->mat[a]);
	}
	
	if (mb->adt)
		expand_animdata(fd, mainvar, mb->adt);
}

static void expand_curve(FileData *fd, Main *mainvar, Curve *cu)
{
	int a;

	for (a=0; a<cu->totcol; a++) {
		expand_doit(fd, mainvar, cu->mat[a]);
	}
	
	expand_doit(fd, mainvar, cu->vfont);
	expand_doit(fd, mainvar, cu->vfontb);	
	expand_doit(fd, mainvar, cu->vfonti);
	expand_doit(fd, mainvar, cu->vfontbi);
	expand_doit(fd, mainvar, cu->key);
	expand_doit(fd, mainvar, cu->ipo); // XXX depreceated - old animation system
	expand_doit(fd, mainvar, cu->bevobj);
	expand_doit(fd, mainvar, cu->taperobj);
	expand_doit(fd, mainvar, cu->textoncurve);
	
	if (cu->adt)
		expand_animdata(fd, mainvar, cu->adt);
}

static void expand_mesh(FileData *fd, Main *mainvar, Mesh *me)
{
	CustomDataLayer *layer;
	MTFace *mtf;
	TFace *tf;
	int a, i;
	
	if (me->adt)
		expand_animdata(fd, mainvar, me->adt);
		
	for (a=0; a<me->totcol; a++) {
		expand_doit(fd, mainvar, me->mat[a]);
	}

	expand_doit(fd, mainvar, me->key);
	expand_doit(fd, mainvar, me->texcomesh);

	if (me->tface) {
		tf= me->tface;
		for (i=0; i<me->totface; i++, tf++)
			if (tf->tpage)
				expand_doit(fd, mainvar, tf->tpage);
	}

	for (a=0; a<me->fdata.totlayer; a++) {
		layer= &me->fdata.layers[a];

		if (layer->type == CD_MTFACE) {
			mtf= (MTFace*)layer->data;
			for (i=0; i<me->totface; i++, mtf++)
				if (mtf->tpage)
					expand_doit(fd, mainvar, mtf->tpage);
		}
	}
}

/* temp struct used to transport needed info to expand_constraint_cb() */
typedef struct tConstraintExpandData {
	FileData *fd;
	Main *mainvar;
} tConstraintExpandData;
/* callback function used to expand constraint ID-links */
static void expand_constraint_cb(bConstraint *UNUSED(con), ID **idpoin, void *userdata)
{
	tConstraintExpandData *ced= (tConstraintExpandData *)userdata;
	expand_doit(ced->fd, ced->mainvar, *idpoin);
}

static void expand_constraints(FileData *fd, Main *mainvar, ListBase *lb)
{
	tConstraintExpandData ced;
	bConstraint *curcon;
	
	/* relink all ID-blocks used by the constraints */
	ced.fd= fd;
	ced.mainvar= mainvar;
	
	id_loop_constraints(lb, expand_constraint_cb, &ced);
	
	/* depreceated manual expansion stuff */
	for (curcon=lb->first; curcon; curcon=curcon->next) {
		if (curcon->ipo)
			expand_doit(fd, mainvar, curcon->ipo); // XXX depreceated - old animation system
	}
}

static void expand_bones(FileData *fd, Main *mainvar, Bone *bone)
{
	Bone *curBone;

	for (curBone = bone->childbase.first; curBone; curBone=curBone->next) {
		expand_bones(fd, mainvar, curBone);
	}

}

static void expand_pose(FileData *fd, Main *mainvar, bPose *pose)
{
	bPoseChannel *chan;

	if (!pose)
		return;

	for (chan = pose->chanbase.first; chan; chan=chan->next) {
		expand_constraints(fd, mainvar, &chan->constraints);
		expand_doit(fd, mainvar, chan->custom);
	}
}

static void expand_armature(FileData *fd, Main *mainvar, bArmature *arm)
{
	Bone *curBone;

	if (arm->adt)
		expand_animdata(fd, mainvar, arm->adt);

	for (curBone = arm->bonebase.first; curBone; curBone=curBone->next) {
		expand_bones(fd, mainvar, curBone);
	}
}

static void expand_object_expandModifiers(void *userData, Object *UNUSED(ob),
											  ID **idpoin)
{
	struct { FileData *fd; Main *mainvar; } *data= userData;

	FileData *fd= data->fd;
	Main *mainvar= data->mainvar;

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
		data.fd= fd;
		data.mainvar= mainvar;

		modifiers_foreachIDLink(ob, expand_object_expandModifiers, (void *)&data);
	}

	expand_pose(fd, mainvar, ob->pose);
	expand_doit(fd, mainvar, ob->poselib);
	expand_constraints(fd, mainvar, &ob->constraints);
	
	expand_doit(fd, mainvar, ob->gpd);
	
// XXX depreceated - old animation system (for version patching only) 
	expand_doit(fd, mainvar, ob->ipo);
	expand_doit(fd, mainvar, ob->action);
	
	expand_constraint_channels(fd, mainvar, &ob->constraintChannels);

	for (strip=ob->nlastrips.first; strip; strip=strip->next) {
		expand_doit(fd, mainvar, strip->object);
		expand_doit(fd, mainvar, strip->act);
		expand_doit(fd, mainvar, strip->ipo);
	}
// XXX depreceated - old animation system (for version patching only)
	
	if (ob->adt)
		expand_animdata(fd, mainvar, ob->adt);
	
	for (a=0; a<ob->totcol; a++) {
		expand_doit(fd, mainvar, ob->mat[a]);
	}
	
	paf = do_version_give_parteff_245(ob);
	if (paf && paf->group) 
		expand_doit(fd, mainvar, paf->group);

	if (ob->dup_group)
		expand_doit(fd, mainvar, ob->dup_group);
	
	if (ob->proxy)
		expand_doit(fd, mainvar, ob->proxy);
	if (ob->proxy_group)
		expand_doit(fd, mainvar, ob->proxy_group);

	for (psys=ob->particlesystem.first; psys; psys=psys->next)
		expand_doit(fd, mainvar, psys->part);

	sens= ob->sensors.first;
	while (sens) {
		if (sens->type==SENS_TOUCH) {
			bTouchSensor *ts= sens->data;
			expand_doit(fd, mainvar, ts->ma);
		}
		else if (sens->type==SENS_MESSAGE) {
			bMessageSensor *ms= sens->data;
			expand_doit(fd, mainvar, ms->fromObject);
		}
		sens= sens->next;
	}

	cont= ob->controllers.first;
	while (cont) {
		if (cont->type==CONT_PYTHON) {
			bPythonCont *pc= cont->data;
			expand_doit(fd, mainvar, pc->text);
		}
		cont= cont->next;
	}

	act= ob->actuators.first;
	while (act) {
		if (act->type==ACT_SOUND) {
			bSoundActuator *sa= act->data;
			expand_doit(fd, mainvar, sa->sound);
		}
		else if (act->type==ACT_CAMERA) {
			bCameraActuator *ca= act->data;
			expand_doit(fd, mainvar, ca->ob);
		}
		else if (act->type==ACT_EDIT_OBJECT) {
			bEditObjectActuator *eoa= act->data;
			if (eoa) {
				expand_doit(fd, mainvar, eoa->ob);
				expand_doit(fd, mainvar, eoa->me);
			}
		}
		else if (act->type==ACT_OBJECT) {
			bObjectActuator *oa= act->data;
			expand_doit(fd, mainvar, oa->reference);
		}
		else if (act->type==ACT_ADD_OBJECT) {
			bAddObjectActuator *aoa= act->data;
			expand_doit(fd, mainvar, aoa->ob);
		}
		else if (act->type==ACT_SCENE) {
			bSceneActuator *sa= act->data;
			expand_doit(fd, mainvar, sa->camera);
			expand_doit(fd, mainvar, sa->scene);
		}
		else if (act->type==ACT_2DFILTER) {
			bTwoDFilterActuator *tdfa= act->data;
			expand_doit(fd, mainvar, tdfa->text);
		}
		else if (act->type==ACT_ACTION) {
			bActionActuator *aa= act->data;
			expand_doit(fd, mainvar, aa->act);
		}
		else if (act->type==ACT_SHAPEACTION) {
			bActionActuator *aa= act->data;
			expand_doit(fd, mainvar, aa->act);
		}
		else if (act->type==ACT_PROPERTY) {
			bPropertyActuator *pa= act->data;
			expand_doit(fd, mainvar, pa->ob);
		}
		else if (act->type==ACT_MESSAGE) {
			bMessageActuator *ma= act->data;
			expand_doit(fd, mainvar, ma->toObject);
		}
		else if (act->type==ACT_PARENT) {
			bParentActuator *pa= act->data;
			expand_doit(fd, mainvar, pa->ob);
		}
		else if (act->type==ACT_ARMATURE) {
			bArmatureActuator *arma= act->data;
			expand_doit(fd, mainvar, arma->target);
		}
		else if (act->type==ACT_STEERING) {
			bSteeringActuator *sta= act->data;
			expand_doit(fd, mainvar, sta->target);
			expand_doit(fd, mainvar, sta->navmesh);
		}
		act= act->next;
	}

	if (ob->pd && ob->pd->tex)
		expand_doit(fd, mainvar, ob->pd->tex);
	
}

static void expand_scene(FileData *fd, Main *mainvar, Scene *sce)
{
	Base *base;
	SceneRenderLayer *srl;

	for (base= sce->base.first; base; base= base->next) {
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
	
	for (srl= sce->r.layers.first; srl; srl= srl->next) {
		expand_doit(fd, mainvar, srl->mat_override);
		expand_doit(fd, mainvar, srl->light_override);
	}

	if (sce->r.dometext)
		expand_doit(fd, mainvar, sce->gm.dome.warptext);
		
	if (sce->gpd)
		expand_doit(fd, mainvar, sce->gpd);

	if (sce->ed) {
		Sequence *seq;

		SEQ_BEGIN(sce->ed, seq) {
			if (seq->scene) expand_doit(fd, mainvar, seq->scene);
			if (seq->scene_camera) expand_doit(fd, mainvar, seq->scene_camera);
			if (seq->sound) expand_doit(fd, mainvar, seq->sound);
		}
		SEQ_END
	}

#ifdef DURIAN_CAMERA_SWITCH
	{
		TimeMarker *marker;

		for (marker= sce->markers.first; marker; marker= marker->next) {
			if (marker->camera) {
				expand_doit(fd, mainvar, marker->camera);
			}
		}
	}
#endif

	expand_doit(fd, mainvar, sce->clip);
}

static void expand_camera(FileData *fd, Main *mainvar, Camera *ca)
{
	expand_doit(fd, mainvar, ca->ipo); // XXX depreceated - old animation system
	
	if (ca->adt)
		expand_animdata(fd, mainvar, ca->adt);
}

static void expand_speaker(FileData *fd, Main *mainvar, Speaker *spk)
{
	expand_doit(fd, mainvar, spk->sound);

	if (spk->adt)
		expand_animdata(fd, mainvar, spk->adt);
}

static void expand_sound(FileData *fd, Main *mainvar, bSound *snd)
{
	expand_doit(fd, mainvar, snd->ipo); // XXX depreceated - old animation system
}

static void expand_movieclip(FileData *fd, Main *mainvar, MovieClip *clip)
{
	if (clip->adt)
		expand_animdata(fd, mainvar, clip->adt);
}

static void expand_main(FileData *fd, Main *mainvar)
{
	ListBase *lbarray[MAX_LIBARRAY];
	ID *id;
	int a, doit= 1;

	if (fd==NULL) return;

	while (doit) {
		doit= 0;

		a= set_listbasepointers(mainvar, lbarray);
		while (a--) {
			id= lbarray[a]->first;

			while (id) {
				if (id->flag & LIB_TEST) {

					switch(GS(id->name)) {

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
						expand_lamp(fd, mainvar,(Lamp *)id);
						break;
					case ID_KE:
						expand_key(fd, mainvar, (Key *)id);
						break;
					case ID_CA:
						expand_camera(fd, mainvar, (Camera *)id);
						break;
					case ID_SPK:
						expand_speaker(fd, mainvar,(Speaker *)id);
						break;
					case ID_SO:
						expand_sound(fd, mainvar, (bSound *)id);
						break;
					case ID_AR:
						expand_armature(fd, mainvar, (bArmature *)id);
						break;
					case ID_AC:
						expand_action(fd, mainvar, (bAction *)id); // XXX depreceated - old animation system
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
						expand_ipo(fd, mainvar, (Ipo *)id); // XXX depreceated - old animation system
						break;
					case ID_PA:
						expand_particlesettings(fd, mainvar, (ParticleSettings *)id);
						break;
					case ID_MC:
						expand_movieclip(fd, mainvar, (MovieClip *)id);
						break;
					}

					doit= 1;
					id->flag -= LIB_TEST;

				}
				id= id->next;
			}
		}
	}
}

static int object_in_any_scene(Main *mainvar, Object *ob)
{
	Scene *sce;
	
	for (sce= mainvar->scene.first; sce; sce= sce->id.next)
		if (object_in_scene(ob, sce))
			return 1;
	return 0;
}

static void give_base_to_objects(Main *mainvar, Scene *sce, Library *lib, const short idcode, const short is_link)
{
	Object *ob;
	Base *base;
	const short is_group_append= (is_link==FALSE && idcode==ID_GR);

	/* give all objects which are LIB_INDIRECT a base, or for a group when *lib has been set */
	for (ob= mainvar->object.first; ob; ob= ob->id.next) {
		
		if ( ob->id.flag & LIB_INDIRECT ) {
			
				/* IF below is quite confusing!
				if we are appending, but this object wasnt just added along with a group,
				then this is already used indirectly in the scene somewhere else and we didnt just append it.
				
				(ob->id.flag & LIB_PRE_EXISTING)==0 means that this is a newly appended object - Campbell */
			if (is_group_append==0 || (ob->id.flag & LIB_PRE_EXISTING)==0) {
				
				int do_it= 0;
				
				if (ob->id.us==0) {
					do_it= 1;
				}
				else if (idcode==ID_GR) {
					if (ob->id.us==1 && is_link==FALSE && ob->id.lib==lib) {
						if ((ob->flag & OB_FROMGROUP) && object_in_any_scene(mainvar, ob)==0) {
							do_it= 1;
						}
					}
				}
				else {
					/* when appending, make sure any indirectly loaded objects
					 * get a base else they cant be accessed at all [#27437] */
					if (ob->id.us==1 && is_link==FALSE && ob->id.lib==lib) {

						/* we may be appending from a scene where we already
						 *  have a linked object which is not in any scene [#27616] */
						if ((ob->id.flag & LIB_PRE_EXISTING)==0) {

							if (object_in_any_scene(mainvar, ob)==0) {
								do_it= 1;
							}
						}
					}
				}

				if (do_it) {
					base= MEM_callocN( sizeof(Base), "add_ext_base");
					BLI_addtail(&(sce->base), base);
					base->lay= ob->lay;
					base->object= ob;
					base->flag= ob->flag;
					ob->id.us= 1;
					
					ob->id.flag -= LIB_INDIRECT;
					ob->id.flag |= LIB_EXTERN;
				}
			}
		}
	}
}

static void give_base_to_groups(Main *mainvar, Scene *scene)
{
	Group *group;

	/* give all objects which are LIB_INDIRECT a base, or for a group when *lib has been set */
	for (group= mainvar->group.first; group; group= group->id.next) {
		if (((group->id.flag & LIB_INDIRECT)==0 && (group->id.flag & LIB_PRE_EXISTING)==0)) {
			Base *base;

			/* add_object(...) messes with the selection */
			Object *ob= add_only_object(OB_EMPTY, group->id.name+2);
			ob->type= OB_EMPTY;
			ob->lay= scene->lay;

			/* assign the base */
			base= scene_add_base(scene, ob);
			base->flag |= SELECT;
			base->object->flag= base->flag;
			ob->recalc |= OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME;
			scene->basact= base;

			/* assign the group */
			ob->dup_group= group;
			ob->transflag |= OB_DUPLIGROUP;
			rename_id(&ob->id, group->id.name+2);
			copy_v3_v3(ob->loc, scene->cursor);
		}
	}
}

/* returns true if the item was found
* but it may already have already been appended/linked */
static ID *append_named_part(Main *mainl, FileData *fd, const char *idname, const short idcode)
{
	BHead *bhead;
	ID *id= NULL;
	int found=0;

	for (bhead= blo_firstbhead(fd); bhead; bhead= blo_nextbhead(fd, bhead)) {
		if (bhead->code==idcode) {
			const char *idname_test= bhead_id_name(fd, bhead);

			if (strcmp(idname_test + 2, idname)==0) {
				found= 1;
				id= is_yet_read(fd, mainl, bhead);
				if (id==NULL) {
					/* not read yet */
					read_libblock(fd, mainl, bhead, LIB_TESTEXT, &id);

					if (id) {
						/* sort by name in list */
						ListBase *lb= which_libbase(mainl, idcode);
						id_sort_by_name(lb, id);
					}
				}
				else {
					/* already linked */
					printf("append: already linked\n");
					oldnewmap_insert(fd->libmap, bhead->old, id, 1);
					if (id->flag & LIB_INDIRECT) {
						id->flag -= LIB_INDIRECT;
						id->flag |= LIB_EXTERN;
					}
				}

				break;
			}
		}
		else if (bhead->code==ENDB) {
			break;
		}
	}

	/* if we found the id but the id is NULL, this is really bad */
	BLI_assert((found != 0) == (id != NULL));

	return found ? id : NULL;
}

static ID *append_named_part_ex(const bContext *C, Main *mainl, FileData *fd, const char *idname, const int idcode, const int flag)
{
	ID *id= append_named_part(mainl, fd, idname, idcode);

	if (id && (GS(id->name) == ID_OB)) {	/* loose object: give a base */
		Scene *scene= CTX_data_scene(C); /* can be NULL */
		if (scene) {
			Base *base;
			Object *ob;

			base= MEM_callocN( sizeof(Base), "app_nam_part");
			BLI_addtail(&scene->base, base);

			ob= (Object *)id;

			/* link at active layer (view3d->lay if in context, else scene->lay */
			if ((flag & FILE_ACTIVELAY)) {
				View3D *v3d = CTX_wm_view3d(C);
				ob->lay = v3d ? v3d->layact : scene->lay;
			}

			ob->mode= 0;
			base->lay= ob->lay;
			base->object= ob;
			ob->id.us++;

			if (flag & FILE_AUTOSELECT) {
				base->flag |= SELECT;
				base->object->flag = base->flag;
				/* do NOT make base active here! screws up GUI stuff, if you want it do it on src/ level */
			}
		}
	}

	return id;
}

ID *BLO_library_append_named_part(Main *mainl, BlendHandle** bh, const char *idname, const int idcode)
{
	FileData *fd= (FileData*)(*bh);
	return append_named_part(mainl, fd, idname, idcode);
}

ID *BLO_library_append_named_part_ex(const bContext *C, Main *mainl, BlendHandle** bh, const char *idname, const int idcode, const short flag)
{
	FileData *fd= (FileData*)(*bh);
	return append_named_part_ex(C, mainl, fd, idname, idcode, flag);
}

static void append_id_part(FileData *fd, Main *mainvar, ID *id, ID **id_r)
{
	BHead *bhead;

	for (bhead= blo_firstbhead(fd); bhead; bhead= blo_nextbhead(fd, bhead)) {
		if (bhead->code == GS(id->name)) {

			if (strcmp(id->name, bhead_id_name(fd, bhead))==0) {
				id->flag &= ~LIB_READ;
				id->flag |= LIB_TEST;
//				printf("read lib block %s\n", id->name);
				read_libblock(fd, mainvar, bhead, id->flag, id_r);

				break;
			}
		}
		else if (bhead->code==ENDB)
			break;
	}
}

/* common routine to append/link something from a library */

static Main* library_append_begin(Main *mainvar, FileData **fd, const char *filepath)
{
	Main *mainl;

	/* make mains */
	blo_split_main(&(*fd)->mainlist, mainvar);

	/* which one do we need? */
	mainl = blo_find_main(*fd, &(*fd)->mainlist, filepath, G.main->name);
	
	/* needed for do_version */
	mainl->versionfile= (*fd)->fileversion;
	read_file_version(*fd, mainl);
	
	return mainl;
}

Main* BLO_library_append_begin(Main *mainvar, BlendHandle** bh, const char *filepath)
{
	FileData *fd= (FileData*)(*bh);
	return library_append_begin(mainvar, &fd, filepath);
}


/* Context == NULL signifies not to do any scene manipulation */
static void library_append_end(const bContext *C, Main *mainl, FileData **fd, int idcode, short flag)
{
	Main *mainvar;
	Library *curlib;

	/* make main consistent */
	expand_main(*fd, mainl);

	/* do this when expand found other libs */
	read_libraries(*fd, &(*fd)->mainlist);
	
	curlib= mainl->curlib;

	/* make the lib path relative if required */
	if (flag & FILE_RELPATH) {

		/* use the full path, this could have been read by other library even */
		BLI_strncpy(curlib->name, curlib->filepath, sizeof(curlib->name));
		
		/* uses current .blend file as reference */
		BLI_path_rel(curlib->name, G.main->name);
	}

	blo_join_main(&(*fd)->mainlist);
	mainvar= (*fd)->mainlist.first;
	mainl= NULL; /* blo_join_main free's mainl, cant use anymore */

	lib_link_all(*fd, mainvar);
	lib_verify_nodetree(mainvar, FALSE);
	fix_relpaths_library(G.main->name, mainvar); /* make all relative paths, relative to the open blend file */

	if (C) {
		Scene *scene= CTX_data_scene(C);

		/* give a base to loose objects. If group append, do it for objects too */
		if (scene) {
			const short is_link= (flag & FILE_LINK) != 0;
			if (idcode==ID_SCE) {
				/* don't instance anything when linking in scenes, assume the scene its self instances the data */
			}
			else {
				give_base_to_objects(mainvar, scene, curlib, idcode, is_link);

				if (flag & FILE_GROUP_INSTANCE) {
					give_base_to_groups(mainvar, scene);
				}
			}
		}
		else {
			printf("library_append_end, scene is NULL (objects wont get bases)\n");
		}
	}
	/* has been removed... erm, why? s..ton) */
	/* 20040907: looks like they are give base already in append_named_part(); -Nathan L */
	/* 20041208: put back. It only linked direct, not indirect objects (ton) */
	
	/* patch to prevent switch_endian happens twice */
	if ((*fd)->flags & FD_FLAGS_SWITCH_ENDIAN) {
		blo_freefiledata( *fd );
		*fd = NULL;
	}	
}

void BLO_library_append_end(const bContext *C, struct Main *mainl, BlendHandle** bh, int idcode, short flag)
{
	FileData *fd= (FileData*)(*bh);
	library_append_end(C, mainl, &fd, idcode, flag);
	*bh= (BlendHandle*)fd;
}

void *BLO_library_read_struct(FileData *fd, BHead *bh, const char *blockname)
{
	return read_struct(fd, bh, blockname);
}

/* ************* READ LIBRARY ************** */

static int mainvar_count_libread_blocks(Main *mainvar)
{
	ListBase *lbarray[MAX_LIBARRAY];
	int a, tot= 0;

	a= set_listbasepointers(mainvar, lbarray);
	while (a--) {
		ID *id;

		for (id= lbarray[a]->first; id; id= id->next)
			if (id->flag & LIB_READ)
				tot++;
	}
	return tot;
}

static void read_libraries(FileData *basefd, ListBase *mainlist)
{
	Main *mainl= mainlist->first;
	Main *mainptr;
	ListBase *lbarray[MAX_LIBARRAY];
	int a, doit= 1;

	while (doit) {
		doit= 0;

		/* test 1: read libdata */
		mainptr= mainl->next;
		while (mainptr) {
			int tot= mainvar_count_libread_blocks(mainptr);
			
			// printf("found LIB_READ %s\n", mainptr->curlib->name);
			if (tot) {
				FileData *fd= mainptr->curlib->filedata;

				if (fd==NULL) {

					/* printf and reports for now... its important users know this */
					BKE_reportf_wrap(basefd->reports, RPT_INFO,
					                 "read library:  '%s', '%s'\n",
					                 mainptr->curlib->filepath, mainptr->curlib->name);

					fd= blo_openblenderfile(mainptr->curlib->filepath, basefd->reports);
					
					/* allow typing in a new lib path */
					if (G.rt==-666) {
						while (fd==NULL) {
							char newlib_path[FILE_MAX] = { 0 };
							printf("Missing library...'\n");
							printf("	current file: %s\n", G.main->name);
							printf("	absolute lib: %s\n", mainptr->curlib->filepath);
							printf("	relative lib: %s\n", mainptr->curlib->name);
							printf("  enter a new path:\n");

							if (scanf("%s", newlib_path) > 0) {
								BLI_strncpy(mainptr->curlib->name, newlib_path, sizeof(mainptr->curlib->name));
								BLI_strncpy(mainptr->curlib->filepath, newlib_path, sizeof(mainptr->curlib->filepath));
								cleanup_path(G.main->name, mainptr->curlib->filepath);
								
								fd= blo_openblenderfile(mainptr->curlib->filepath, basefd->reports);

								if (fd) {
									printf("found: '%s', party on macuno!\n", mainptr->curlib->filepath);
								}
							}
						}
					}

					if (fd) {
						fd->reports= basefd->reports;
						
						if (fd->libmap)
							oldnewmap_free(fd->libmap);

						fd->libmap = oldnewmap_new();
						
						mainptr->curlib->filedata= fd;
						mainptr->versionfile= fd->fileversion;
						
						/* subversion */
						read_file_version(fd, mainptr);
					}
					else mainptr->curlib->filedata= NULL;

					if (fd==NULL) {
						BKE_reportf_wrap(basefd->reports, RPT_ERROR,
						                 "Can't find lib '%s'\n",
						                 mainptr->curlib->filepath);
					}
				}
				if (fd) {
					doit= 1;
					a= set_listbasepointers(mainptr, lbarray);
					while (a--) {
						ID *id= lbarray[a]->first;

						while (id) {
							ID *idn= id->next;
							if (id->flag & LIB_READ) {
								ID *realid= NULL;
								BLI_remlink(lbarray[a], id);

								append_id_part(fd, mainptr, id, &realid);
								if (!realid) {
									BKE_reportf_wrap(fd->reports, RPT_ERROR,
									                 "LIB ERROR: %s:'%s' missing from '%s'\n",
									                 BKE_idcode_to_name(GS(id->name)),
									                 id->name+2, mainptr->curlib->filepath);
								}
								
								change_idid_adr(mainlist, basefd, id, realid);

								MEM_freeN(id);
							}
							id= idn;
						}
					}

					expand_main(fd, mainptr);
					
					/* dang FileData... now new libraries need to be appended to original filedata,
					 * it is not a good replacement for the old global (ton) */
					while ( fd->mainlist.first ) {
						Main *mp= fd->mainlist.first;
						BLI_remlink(&fd->mainlist, mp);
						BLI_addtail(&basefd->mainlist, mp);
					}
				}
			}

			mainptr= mainptr->next;
		}
	}
	
	/* test if there are unread libblocks */
	for (mainptr= mainl->next; mainptr; mainptr= mainptr->next) {
		a= set_listbasepointers(mainptr, lbarray);
		while (a--) {
			ID *id= lbarray[a]->first;
			while (id) {
				ID *idn= id->next;
				if (id->flag & LIB_READ) {
					BLI_remlink(lbarray[a], id);
					BKE_reportf_wrap(basefd->reports, RPT_ERROR,
					                 "LIB ERROR: %s:'%s' unread libblock missing from '%s'\n",
					                 BKE_idcode_to_name(GS(id->name)), id->name+2, mainptr->curlib->filepath);
					change_idid_adr(mainlist, basefd, id, NULL);

					MEM_freeN(id);
				}
				id= idn;
			}
		}
	}
	
	/* do versions, link, and free */
	for (mainptr= mainl->next; mainptr; mainptr= mainptr->next) {
		/* some mains still have to be read, then
		 * versionfile is still zero! */
		if (mainptr->versionfile) {
			if (mainptr->curlib->filedata) // can be zero... with shift+f1 append
				do_versions(mainptr->curlib->filedata, mainptr->curlib, mainptr);
			else
				do_versions(basefd, NULL, mainptr);
		}
		
		if (mainptr->curlib->filedata)
			lib_link_all(mainptr->curlib->filedata, mainptr);
		
		if (mainptr->curlib->filedata) blo_freefiledata(mainptr->curlib->filedata);
		mainptr->curlib->filedata= NULL;
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

	fd->reports= reports;
	bfd= blo_read_file_internal(fd, "");
	blo_freefiledata(fd);

	return bfd;
}
