/*
 * readfile.c
 *
 * .blend file reading
 *
 * $Id$
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
 * 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "winsock2.h"
#include "BLI_winstuff.h"
#endif

#include <stdio.h> // for printf fopen fwrite fclose sprintf FILE
#include <stdlib.h> // for getenv atoi
#include <fcntl.h> // for open

#ifndef WIN32 
    #include <unistd.h> // for read close
    #include <sys/param.h> // for MAXPATHLEN
#else
    #include <io.h> // for open close read
#endif

#include "nla.h"

#include "DNA_ID.h"
#include "DNA_packedFile_types.h"
#include "DNA_property_types.h"
#include "DNA_actuator_types.h"
#include "DNA_controller_types.h"
#include "DNA_sensor_types.h"
#include "DNA_sdna_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_ika_types.h"
#include "DNA_camera_types.h"
#include "DNA_lattice_types.h"
#include "DNA_texture_types.h"
#include "DNA_key_types.h"
#include "DNA_meta_types.h"
#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_world_types.h"
#include "DNA_ipo_types.h"
#include "DNA_mesh_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_curve_types.h"
#include "DNA_vfont_types.h"
#include "DNA_effect_types.h"
#include "DNA_text_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_oops_types.h"
#include "DNA_group_types.h"
#include "DNA_userdef_types.h"
#include "DNA_fileglobal_types.h"
#include "DNA_constraint_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_nla_types.h"

#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
#include "BLI_storage_types.h" // for relname flags

#include "BKE_bad_level_calls.h" // for reopen_text build_seqar (from WHILE_SEQ) open_plugin_seq set_rects_butspace check_imasel_copy

#include "BKE_constraint.h"
#include "BKE_utildefines.h" // SWITCH_INT WHILE_SEQ END_SEQ DATA ENDB DNA1 O_BINARY GLOB USER TEST REND
#include "BKE_main.h" // for Main
#include "BKE_global.h" // for G
#include "BKE_property.h" // for get_property
#include "BKE_library.h" // for wich_libbase 
#include "BKE_texture.h" // for open_plugin_tex
#include "BKE_effect.h" // for give_parteff 
#include "BKE_sca.h" // for init_actuator
#include "BKE_mesh.h" // for ME_ defines (patching)
#include "BKE_armature.h"	//	for precalc_bonelist_irestmats
#include "BKE_action.h"

#include "BPY_extern.h" // for BPY_do_pyscript

#include "BLO_readfile.h"
#include "readfile.h"

#include "genfile.h"

#include "BLO_readblenfile.h" // streaming read pipe, for BLO_readblenfile BLO_readblenfilememory

#include "mydevice.h"

#include <string.h> // for strcasecmp strrchr strncmp strstr



/*
 Remark: still a weak point is the newadress() function, that doesnt solve reading from
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
- read USER data, only when indicated (file is ~/.B.blend)
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
// only used here in readfile.c
#define SWITCH_SHORT(a)	{ \
    char s_i, *p_i; \
    p_i= (char *)&(a); \
    s_i=p_i[0]; p_i[0]=p_i[1]; p_i[1]=s_i; }

/***/

static char *functionality_check= "\0FUNCTIONALITY_CHECK += blo_readfile\n";

/***/

typedef struct OldNew {
	void *old, *newp;
	int nr;
} OldNew;

typedef struct OldNewMap {
	OldNew *entries;
	int nentries, entriessize;
	
	int lasthit;
} OldNewMap;

static OldNewMap *oldnewmap_new(void) {
	OldNewMap *onm= MEM_mallocN(sizeof(*onm), "OldNewMap");
	onm->lasthit= 0;
	onm->nentries= 0;
	onm->entriessize= 1024;
	onm->entries= MEM_mallocN(sizeof(*onm->entries)*onm->entriessize, "OldNewMap.entries");
	
	return onm;
}

static void oldnewmap_insert(OldNewMap *onm, void *oldaddr, void *newaddr, int nr) {
	OldNew *entry;
	
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

static void *oldnewmap_lookup_and_inc(OldNewMap *onm, void *addr) {
	int i;
	
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

static void *oldnewmap_liblookup_and_inc(OldNewMap *onm, void *addr, void *lib) {
	int i;
	
	if (onm->lasthit<onm->nentries-1) {
		OldNew *entry= &onm->entries[++onm->lasthit];
		
		if (entry->old==addr) {
			ID *id= entry->newp;
			
			if (id && (!lib || id->lib)) {
				entry->nr++;
			
				return entry->newp;
			}
		}
	}
	
	for (i=0; i<onm->nentries; i++) {
		OldNew *entry= &onm->entries[i];
		
		if (entry->old==addr) {
			ID *id= entry->newp;
			
			if (id && (!lib || id->lib)) {
				entry->nr++;
			
				return entry->newp;
			}
		}
	}

	return NULL;
}

static void *oldnewmap_typelookup_and_inc(OldNewMap *onm, void *addr, short type) {
	int i;
	
	if (onm->lasthit<onm->nentries-1) {
		OldNew *entry= &onm->entries[++onm->lasthit];
		
		if (entry->old==addr) {
			ID *id= entry->newp;
			
			if (id && (GS(id->name) == type)) {
				entry->nr++;
			
				return entry->newp;
			}
		}
	}
	
	for (i=0; i<onm->nentries; i++) {
		OldNew *entry= &onm->entries[i];
		
		if (entry->old==addr) {
			ID *id= entry->newp;
			
			if (id && (GS(id->name) == type)) {
				entry->nr++;
			
				return entry->newp;
			}
		}
	}

	return NULL;
}

static void oldnewmap_free_unused(OldNewMap *onm) {
	int i;
	
	for (i=0; i<onm->nentries; i++) {
		OldNew *entry= &onm->entries[i];
		if (entry->nr==0) {
			MEM_freeN(entry->newp);
			entry->newp= NULL;
		}
	}
}

static void oldnewmap_clear(OldNewMap *onm) {
	onm->nentries= 0;
	onm->lasthit= 0;
}

static void oldnewmap_free(OldNewMap *onm) {
	MEM_freeN(onm->entries);
	MEM_freeN(onm);
}

/***/

static void read_libraries(FileData *basefd, ListBase *mainlist);

/* ************ help functions ***************** */

static void add_main_to_main(Main *mainvar, Main *from)
{
	ListBase *lbarray[30], *fromarray[30];
	int a;
	
	a= set_listbasepointers(mainvar, lbarray);
	a= set_listbasepointers(from, fromarray);
	while(a--) {
		addlisttolist(lbarray[a], fromarray[a]);
	}
}

void blo_join_main(ListBase *mainlist)
{
	Main *tojoin, *mainl= mainlist->first;

	while (tojoin= mainl->next) {
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
	while(id) {
		idnext= id->next;
		if(id->lib) {
			mainvar= first;
			while(mainvar) {
				if(mainvar->curlib==id->lib) {
					lbn= wich_libbase(mainvar, GS(id->name));
					BLI_remlink(lb, id);
					BLI_addtail(lbn, id);
					break;
				}
				mainvar= mainvar->next;
			}
			if(mainvar==0) printf("error split_libdata\n");
		}
		id= idnext;
	}
}

void blo_split_main(ListBase *mainlist)
{
	Main *mainl= mainlist->first;
	ListBase *lbarray[30];
	Library *lib;
	int i;
	
	for (lib= mainl->library.first; lib; lib= lib->id.next) {
		Main *libmain= MEM_callocN(sizeof(*libmain), "libmain");
		libmain->curlib= lib;

		BLI_addtail(mainlist, libmain);
	}
	
	i= set_listbasepointers(mainl, lbarray);
	while(i--)
		split_libdata(lbarray[i], mainl->next);
}

static Main *blo_find_main(ListBase *mainlist, char *name)
{
	Main *m;
	Library *lib;
	
	for (m= mainlist->first; m; m= m->next) {
		char *libname= (m->curlib)?m->curlib->name:m->name;

		if (BLI_streq(name, libname))
			return m;
	}

	m= MEM_callocN(sizeof(*m), "find_main");
	BLI_addtail(mainlist, m);
	
	lib= alloc_libblock(&m->library, ID_LI, "lib");
	strcpy(lib->name, name);
	m->curlib= lib;
	
	return m;
}


/* ************ FILE PARSING ****************** */

static void switch_endian_bh4(BHead4 *bhead)
{
	/* the ID_.. codes */
	if((bhead->code & 0xFFFF)==0) bhead->code >>=16;

	if (bhead->code != ENDB) {
		SWITCH_INT(bhead->len);
		SWITCH_INT(bhead->SDNAnr);
		SWITCH_INT(bhead->nr);
	}
}

static void switch_endian_bh8(BHead8 *bhead)
{
	/* the ID_.. codes */
	if((bhead->code & 0xFFFF)==0) bhead->code >>=16;

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
	
		// why is this here ??
		if (do_endian_swap) {
			SWITCH_LONGINT(bhead8->old);
		}
		
		/* this patch is to avoid a long long being read from not-eight aligned positions
		   is necessary on SGI with -n32 compiling (no, is necessary on
		   any modern 64bit architecture) */
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
	BHead8 bhead8;
	BHead4 bhead4;
	BHead  bhead;
	BHeadN *new_bhead = 0;
	int readsize;

	if (fd) {
		if ( ! fd->eof) {

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
					} else {
						memcpy(&bhead, &bhead4, sizeof(bhead));
					}
				} else {
					fd->eof = 1;
				}
			} else {
				bhead8.code = DATA;
				readsize = fd->read(fd, &bhead8, sizeof(bhead8));
				
				if (readsize == sizeof(bhead8) || bhead8.code == ENDB) {
					if (fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
						switch_endian_bh8(&bhead8);
					}
					
					if (fd->flags & FD_FLAGS_POINTSIZE_DIFFERS) {
						bh4_from_bh8(&bhead, &bhead8, (fd->flags & FD_FLAGS_SWITCH_ENDIAN));
					} else {
						memcpy(&bhead, &bhead8, sizeof(bhead));
					}
				} else {
					fd->eof = 1;
				}
			}
			
			// bhead now contains the (converted) bhead structure. Now read
			// the associated data and put everything in a BHeadN (creative naming !)

			if ( ! fd->eof) {
				new_bhead = MEM_mallocN(sizeof(BHeadN) + bhead.len, "new_bhead");
				if (new_bhead) {
					new_bhead->next = new_bhead->prev = 0;
					new_bhead->bhead = bhead;
					
					readsize = fd->read(fd, new_bhead + 1, bhead.len);
					
					if (readsize != bhead.len) {
						fd->eof = 1;
						MEM_freeN(new_bhead);
					}
				} else {
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
	BHead *bhead = 0;
	
	// Rewind the file
	// Read in a new block if necessary
	
	new_bhead = fd->listbase.first;
	if (new_bhead == 0) {
		new_bhead = get_bhead(fd);
	}
	
	if (new_bhead) {
		bhead = &new_bhead->bhead;
	}
	
	return(bhead);
}

BHead *blo_prevbhead(FileData *fd, BHead *thisblock)
{
	BHeadN *bheadn= (BHeadN *) (((char *) thisblock) - (int) (&((BHeadN*)0)->bhead));
	BHeadN *prev= bheadn->prev;
	
	return prev?&prev->bhead:NULL;
}

BHead *blo_nextbhead(FileData *fd, BHead *thisblock)
{
	BHeadN *new_bhead = 0;
	BHead *bhead = 0;

	if (thisblock) {
		// bhead is actually a sub part of BHeadN
		// We calculate the BHeadN pointer from the BHead pointer below
		new_bhead = (BHeadN *) (((char *) thisblock) - (int) (&((BHeadN*)0)->bhead));

		// get the next BHeadN. If it doesn't exist we read in the next one
		new_bhead = new_bhead->next;
		if (new_bhead == 0) {
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
		if(strncmp(header, "BLENDER", 7) == 0) {
			int remove_this_endian_test= 1;
			
			fd->flags |= FD_FLAGS_FILE_OK;
		
			// what size are pointers in the file ?
			if(header[7]=='_') {
				fd->flags |= FD_FLAGS_FILE_POINTSIZE_IS_4;
				if (sizeof(void *) != 4) {
					fd->flags |= FD_FLAGS_POINTSIZE_DIFFERS;
				}
			} else {
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
	
			fd->filesdna= dna_sdna_from_data(&bhead[1], bhead->len, do_endian_swap);
			if (fd->filesdna)
				fd->compflags= dna_get_structDNA_compareflags(fd->filesdna, fd->memsdna);
				
			return 1;
		} else if (bhead->code==ENDB)
			break;
	}
	
	return 0;
}
static int fd_read_from_file(FileData *filedata, void *buffer, int size)
{
	int readsize = read(filedata->filedes, buffer, size);
	
	if (readsize < 0) {
		readsize = EOF;
	} else {
		filedata->seek += readsize;
	}

	return (readsize);	
}

static int fd_read_from_memory(FileData *filedata, void *buffer, int size)
{
		// don't read more bytes then there are available in the buffer
	int readsize = MIN2(size, filedata->buffersize - filedata->seek);

	memcpy(buffer, filedata->buffer + filedata->seek, readsize);
	filedata->seek += readsize;

	return (readsize);
}

static FileData *filedata_new(void)
{
	extern char DNAstr[];	/* DNA.c */
	extern int DNAlen;
	FileData *fd = MEM_callocN(sizeof(*fd), "FileData");
	
	fd->filedes = -1;	
	
		/* XXX, this doesn't need to be done all the time,
		 * but it keeps us reentrant,  remove once we have
		 * a lib that provides a nice lock. - zr
		 */
	fd->memsdna = dna_sdna_from_data(DNAstr,  DNAlen,  0);
	
	fd->datamap = oldnewmap_new();
	fd->globmap = oldnewmap_new();
	fd->libmap = oldnewmap_new();

	return fd;
}

FileData *blo_openblenderfile(char *name)
{
	int file= open(name, O_BINARY|O_RDONLY);
	
	if (file == -1) {
		return NULL;
	} else {
		FileData *fd = filedata_new();
		fd->filedes = file;
		fd->buffersize = BLI_filesize(file);
		fd->read = fd_read_from_file;
	
		decode_blender_header(fd);
		
		if (fd->flags & FD_FLAGS_FILE_OK) {
			if (!read_file_dna(fd)) {
				blo_freefiledata(fd);
				fd= NULL;
			}
		} else {
			blo_freefiledata(fd);
			fd= NULL;
		}

		return fd;
	}
}

FileData *blo_openblendermemory(void *mem, int memsize)
{
	if (!mem || memsize<SIZEOFBLENDERHEADER) {
		return NULL;
	} else {
		FileData *fd= filedata_new();
		fd->buffer= mem;
		fd->buffersize= memsize;
		fd->read= fd_read_from_memory;
		fd->flags|= FD_FLAGS_NOT_MY_BUFFER;
		
		decode_blender_header(fd);
		
		if (fd->flags & FD_FLAGS_FILE_OK) {
			if (!read_file_dna(fd)) {
				blo_freefiledata(fd);
				fd= NULL;
			}
		} else {
			blo_freefiledata(fd);
			fd= NULL;
		}
		
		return fd;
	}
}

void blo_freefiledata(FileData *fd)
{
	if (fd) {
		if (fd->filedes != -1) {
			close(fd->filedes);
		}

		if (fd->buffer && !(fd->flags & FD_FLAGS_NOT_MY_BUFFER)) {
			MEM_freeN(fd->buffer);
			fd->buffer = 0;
		}
		
		// Free all BHeadN data blocks
		BLI_freelistN(&fd->listbase);

		if (fd->memsdna)
			dna_freestructDNA(fd->memsdna);
		if (fd->filesdna)
			dna_freestructDNA(fd->filesdna);
		if (fd->compflags)
			MEM_freeN(fd->compflags);

		if (fd->datamap)
			oldnewmap_free(fd->datamap);
		if (fd->globmap)
			oldnewmap_free(fd->globmap);
		if (fd->libmap && !(fd->flags & FD_FLAGS_NOT_MY_LIBMAP))
			oldnewmap_free(fd->libmap);
		
		MEM_freeN(fd);
	}
}

/* ************ DIV ****************** */

int BLO_has_bfile_extension(char *str)
{
	return (BLI_testextensie(str, ".ble") || BLI_testextensie(str, ".blend"));
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

static void *newlibadr(FileData *fd, void *lib, void *adr)		/* only lib data */
{
	return oldnewmap_liblookup_and_inc(fd->libmap, adr, lib);
}

static void *newlibadr_us_type(FileData *fd, short type, void *adr)	/* only Lib data */
{
	ID *id= oldnewmap_typelookup_and_inc(fd->libmap, adr, type);
	
	if (id) {
		id->us++;
	}
	
	return id;
}

static void *newlibadr_us(FileData *fd, void *lib, void *adr)	/* increases user number */
{
	ID *id= newlibadr(fd, lib, adr);

	if(id) {
		id->us++;
	}
	
	return id;
}

static void change_libadr(FileData *fd, void *old, void *new)
{
	int i;
	
		/* changed one thing here, the old change_libadr
		 * only remapped addresses that had an id->lib, 
		 * but that doesn't make sense to me... its an
		 * old pointer, period, it needs to be remapped. - zr
		 */

		/*
		 * Ton seemed to think it was necessary to look
		 * through all entries, and not return after finding
		 * a match, leaving this cryptic comment, 
		 * // no return, maybe there can be more?
		 * 
		 * That doesn't make sense to me either but I am
		 * too scared to remove it... it only would make
		 * sense if two distinct old address map to the
		 * same new address - obviously that shouldn't happen
		 * because memory addresses are unique.
		 * 
		 * The only case it might happen is when two distinct
		 * libraries are mapped using the same table... this
		 * won't work to start with... At some point this 
		 * all needs to be made sense of and made understandable, 
		 * but I'm afraid I don't have time now. -zr
		 * 
		 */
    /* the code is nasty, and needs a lot of energy to get into full understanding
       again... i now translate dutch comments, maybe that gives me more insight!
       But i guess it has to do with the assumption that 2 addresses can be allocated
       in different sessions, and therefore be the same... like the remark in the top
       of this c file (ton) */
     
	for (i=0; i<fd->libmap->nentries; i++) {
		OldNew *entry= &fd->libmap->entries[i];
		
		if (old==entry->newp) {
			entry->newp= new;
			break;
		}
	}
}


/* ********** END OLD POINTERS ****************** */
/* ********** READ FILE ****************** */

static void switch_endian_structs(struct SDNA *filesdna, BHead *bhead)
{
	int blocksize, nblocks;
	char *data;
	
	data= (char *)(bhead+1); /*  BHEAD+DATA dependancy */
	blocksize= filesdna->typelens[ filesdna->structs[bhead->SDNAnr][0] ];

	nblocks= bhead->nr;
	while(nblocks--) {
		dna_switch_endian_struct(filesdna, bhead->SDNAnr, data);
		
		data+= blocksize;
	}
}

static void *read_struct(FileData *fd, BHead *bh)
{
	void *temp= NULL;
	
	if (bh->len) {
		if (bh->SDNAnr && (fd->flags & FD_FLAGS_SWITCH_ENDIAN))
			switch_endian_structs(fd->filesdna, bh);
		
		if (fd->compflags[bh->SDNAnr]) {	/* flag==0: doesn't exist anymore */
			if(fd->compflags[bh->SDNAnr]==2) {
				temp= dna_reconstruct(fd->memsdna, fd->filesdna, fd->compflags, bh->SDNAnr, bh->nr, (bh+1));
			} else {
				temp= MEM_mallocN(bh->len, "read_struct");
				memcpy(temp, (bh+1), bh->len); /*  BHEAD+DATA dependancy */
			}
		}
	}
	
	return temp;	
}

static void link_list(FileData *fd, ListBase *lb)		/* only direct data */
{
	Link *ln, *prev;
	
	if(lb->first==0) return;

	lb->first= newdataadr(fd, lb->first);
	ln= lb->first;
	prev= 0;
	while(ln) {
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
    
	if(lb->first==0) return;
	
	poin= newdataadr(fd, lb->first);
	if(lb->first) {
		oldnewmap_insert(fd->globmap, lb->first, poin, 0);
	}
	lb->first= poin;
	
	ln= lb->first;
	prev= 0;
	while(ln) {
		poin= newdataadr(fd, ln->next);
		if(ln->next) {
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
	int len, *ipoin, *imat;

		/* manually convert the pointer array in
		 * the old dna format to a pointer array in
		 * the new dna format.
		 */
	if(*mat) {
		len= MEM_allocN_len(*mat)/fd->filesdna->pointerlen;
		
		if(fd->filesdna->pointerlen==8 && fd->memsdna->pointerlen==4) {
			ipoin=imat= MEM_mallocN( len*4, "newmatar");
			lpoin= *mat;

			while(len-- > 0) {
				if((fd->flags & FD_FLAGS_SWITCH_ENDIAN))	
					SWITCH_LONGINT(*lpoin);
				*ipoin= (int) ((*lpoin) >> 3);
				ipoin++;
				lpoin++;
			}
			MEM_freeN(*mat);
			*mat= imat;
		}
		
		if(fd->filesdna->pointerlen==4 && fd->memsdna->pointerlen==8) {
			lpoin=lmat= MEM_mallocN( len*8, "newmatar");
			ipoin= *mat;

			while(len-- > 0) {
				*lpoin= *ipoin;
				ipoin++;
				lpoin++;
			}
			MEM_freeN(*mat);
			*mat= lmat;
		}
	}
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

/* ************ READ SCRIPTLINK *************** */

static void lib_link_scriptlink(FileData *fd, ID *id, ScriptLink *slink)
{
	int i;

	for(i=0; i<slink->totscript; i++) {
		slink->scripts[i]= newlibadr(fd, id->lib, slink->scripts[i]);
	}
}		

static void direct_link_scriptlink(FileData *fd, ScriptLink *slink)
{
	slink->scripts= newdataadr(fd, slink->scripts);	
	slink->flag= newdataadr(fd, slink->flag);	

	if(fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
		int a;
		
		for(a=0; a<slink->totscript; a++) {
			SWITCH_SHORT(slink->flag[a]);
		}
	}
}

/* ************ READ IKA ***************** */

static void lib_link_ika(FileData *fd, Main *main)
{
	Ika *ika;
	int a;
	Deform *def;
	
	ika= main->ika.first;
	while(ika) {
		if(ika->id.flag & LIB_NEEDLINK) {
			
			ika->parent= newlibadr(fd, ika->id.lib, ika->parent);
			
			a= ika->totdef;
			def= ika->def;
			while(a--) {
				def->ob=  newlibadr(fd, ika->id.lib, def->ob);
				def++;
			}
			ika->id.flag -= LIB_NEEDLINK;
		}
		ika= ika->id.next;
	}
}

static void direct_link_ika(FileData *fd, Ika *ika)
{
	link_list(fd, &ika->limbbase);

	ika->def= newdataadr(fd, ika->def);

	/* error from V.138 and older */
	if(ika->def==0) ika->totdef= 0;
}

/* ************ READ ARMATURE ***************** */

static void lib_link_nlastrips(FileData *fd, ID *id, ListBase *striplist)
{
	bActionStrip *strip;

	for (strip=striplist->first; strip; strip=strip->next){
		strip->act = newlibadr(fd, id->lib, strip->act);
		strip->ipo = newlibadr(fd, id->lib, strip->ipo);
	};
}

static void lib_link_constraint_channels(FileData *fd, ID *id, ListBase *chanbase)
{
	bConstraintChannel *chan;

	for (chan=chanbase->first; chan; chan=chan->next){
		chan->ipo = newlibadr_us(fd, id->lib, chan->ipo);
	}
}

static void lib_link_constraints(FileData *fd, ID *id, ListBase *conlist)
{
	bConstraint *con;
	
	for (con = conlist->first; con; con=con->next) {
		switch (con->type) {
		case CONSTRAINT_TYPE_ACTION:
			{
				bActionConstraint *data;
				data= ((bActionConstraint*)con->data);
				data->tar = newlibadr(fd, id->lib, data->tar);
				data->act = newlibadr(fd, id->lib, data->act);
			}
			break;
		case CONSTRAINT_TYPE_LOCLIKE:
			{
				bLocateLikeConstraint *data;
				data= ((bLocateLikeConstraint*)con->data);
				data->tar = newlibadr(fd, id->lib, data->tar);
			};
			break;
		case CONSTRAINT_TYPE_ROTLIKE:
			{
				bRotateLikeConstraint *data;
				data= ((bRotateLikeConstraint*)con->data);
				data->tar = newlibadr(fd, id->lib, data->tar);
			};
			break;
		case CONSTRAINT_TYPE_KINEMATIC:
			{
				bKinematicConstraint *data;
				data = ((bKinematicConstraint*)con->data);
				data->tar = newlibadr(fd, id->lib, data->tar);
			}
			break;
		case CONSTRAINT_TYPE_NULL:
			break;
		case CONSTRAINT_TYPE_TRACKTO:
			{
				bTrackToConstraint *data;
				data = ((bTrackToConstraint*)con->data);
				data->tar = newlibadr(fd, id->lib, data->tar);
			}
			break;

		}
	}
}

static void direct_link_constraints(FileData *fd, ListBase *lb)
{
	bConstraint *cons;

	link_list(fd, lb);
	for (cons=lb->first; cons; cons=cons->next) {
		cons->data = newdataadr(fd, cons->data);
		switch (cons->type) {
		default:
			break;
		}
		// Link data
	}
}

static void lib_link_bone(FileData *fd, ID *id, Bone *bone)
{
	Bone *curBone;

//	lib_link_constraints(fd, id, &bone->constraints);

	for (curBone=bone->childbase.first; curBone; curBone=curBone->next) {
		lib_link_bone(fd, id, curBone);
	}
}


static void lib_link_pose(FileData *fd, ID *id, bPose *pose)
{
	bPoseChannel *chan;

	if (!pose)
		return;

	for (chan = pose->chanbase.first; chan; chan=chan->next) {
		lib_link_constraints(fd, id, &chan->constraints);
	}
}

static void lib_link_armature(FileData *fd, Main *main)
{
	bArmature *arm;
	Bone *bone;

	arm= main->armature.first;

	while(arm) {
		if(arm->id.flag & LIB_NEEDLINK) {
			arm->id.flag -= LIB_NEEDLINK;
		}

		for (bone=arm->bonebase.first; bone; bone=bone->next) {
			lib_link_bone(fd, &arm->id, bone);
		}
		
		arm= arm->id.next;
	}
}

static void lib_link_action(FileData *fd, Main *main)
{
	bAction *act;
	bActionChannel *chan;
	
	act= main->action.first;
	while(act) {
		if(act->id.flag & LIB_NEEDLINK) {
			act->id.flag -= LIB_NEEDLINK;
			
			for (chan=act->chanbase.first; chan; chan=chan->next) {
				chan->ipo= newlibadr_us(fd, act->id.lib, chan->ipo);
				lib_link_constraint_channels(fd, &act->id, &chan->constraintChannels);
			}
			
		}
		act= act->id.next;
	}
}

static void direct_link_bones(FileData *fd, Bone* bone)
{
	Bone	*child;

	bone->parent= newdataadr(fd, bone->parent);

	link_list(fd, &bone->childbase);

	for (child=bone->childbase.first; child; child=child->next) {
		direct_link_bones(fd, child);
	}
}


static void direct_link_action(FileData *fd, bAction *act)
{
	bActionChannel *achan;

	link_list(fd, &act->chanbase);
	
	for (achan = act->chanbase.first; achan; achan=achan->next)
		link_list(fd, &achan->constraintChannels);

}

static void direct_link_armature(FileData *fd, bArmature *arm)
{
	Bone	*bone; 

	link_list(fd, &arm->bonebase);

	bone=arm->bonebase.first;
	while (bone) {
		direct_link_bones(fd, bone);
		bone=bone->next;
	}
}

/* ************ READ CAMERA ***************** */

static void lib_link_camera(FileData *fd, Main *main)
{
	Camera *ca;
	
	ca= main->camera.first;
	while(ca) {
		if(ca->id.flag & LIB_NEEDLINK) {
			
			ca->ipo= newlibadr_us(fd, ca->id.lib, ca->ipo);

			lib_link_scriptlink(fd, &ca->id, &ca->scriptlink);
			
			ca->id.flag -= LIB_NEEDLINK;
		}
		ca= ca->id.next;
	}
}

static void direct_link_camera(FileData *fd, Camera *ca)
{
	direct_link_scriptlink(fd, &ca->scriptlink);
}

	
/* ************ READ LATTICE ***************** */

static void lib_link_latt(FileData *fd, Main *main)
{
	Lattice *lt;
	
	lt= main->latt.first;
	while(lt) {
		if(lt->id.flag & LIB_NEEDLINK) {
			
			lt->ipo= newlibadr_us(fd, lt->id.lib, lt->ipo);
			lt->key= newlibadr_us(fd, lt->id.lib, lt->key);
			
			lt->id.flag -= LIB_NEEDLINK;
		}
		lt= lt->id.next;
	}
}

static void direct_link_latt(FileData *fd, Lattice *lt)
{
	lt->def= newdataadr(fd, lt->def);
}

/* ************ READ LAMP ***************** */

static void lib_link_lamp(FileData *fd, Main *main)
{
	Lamp *la;
	MTex *mtex;
	int a;
	
	la= main->lamp.first;
	while(la) {
		if(la->id.flag & LIB_NEEDLINK) {

			for(a=0; a<8; a++) {
				mtex= la->mtex[a];
				if(mtex) {
					mtex->tex= newlibadr_us(fd, la->id.lib, mtex->tex);
					mtex->object= newlibadr(fd, la->id.lib, mtex->object);
				}
			}
		
			la->ipo= newlibadr_us(fd, la->id.lib, la->ipo);
			
			lib_link_scriptlink(fd, &la->id, &la->scriptlink);
			
			la->id.flag -= LIB_NEEDLINK;
		}
		la= la->id.next;
	}
}

static void direct_link_lamp(FileData *fd, Lamp *la)
{
	int a;

	direct_link_scriptlink(fd, &la->scriptlink);
		
	for(a=0; a<8; a++) {
		la->mtex[a]= newdataadr(fd, la->mtex[a]);
	}
}

/* ************ READ keys ***************** */

static void lib_link_key(FileData *fd, Main *main)
{
	Key *key;
	
	key= main->key.first;
	while(key) {
		if(key->id.flag & LIB_NEEDLINK) {
			
			key->ipo= newlibadr_us(fd, key->id.lib, key->ipo);
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

	for(a=0; a<kb->totelem; a++) {
	
		cp= key->elemstr;	
		poin= data;
		
		while( cp[0] ) {	/* cp[0]==amount */
		
			switch(cp[1]) {		/* cp[1]= type */
			case IPO_FLOAT:
			case IPO_BPOINT:
			case IPO_BEZTRIPLE:
				b= cp[0];
				while(b--) {
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
	
	key->refkey= newdataadr(fd, key->refkey);
	
	kb= key->block.first;
	while(kb) {
		
		kb->data= newdataadr(fd, kb->data);
		
		if(fd->flags & FD_FLAGS_SWITCH_ENDIAN)
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
	while(mb) {
		if(mb->id.flag & LIB_NEEDLINK) {
			
			for(a=0; a<mb->totcol; a++) mb->mat[a]= newlibadr_us(fd, mb->id.lib, mb->mat[a]);

			mb->ipo= newlibadr_us(fd, mb->id.lib, mb->ipo);

			mb->id.flag -= LIB_NEEDLINK;
		}
		mb= mb->id.next;
	}
}

static void direct_link_mball(FileData *fd, MetaBall *mb)
{
	mb->mat= newdataadr(fd, mb->mat);
	test_pointer_array(fd, (void **)&mb->mat);

	link_list(fd, &(mb->elems));
	
	mb->disp.first= mb->disp.last= 0;
	
	mb->bb= 0;
}

/* ************ READ WORLD ***************** */

static void lib_link_world(FileData *fd, Main *main)
{
	World *wrld;
	MTex *mtex;
	int a;
	
	wrld= main->world.first;
	while(wrld) {
		if(wrld->id.flag & LIB_NEEDLINK) {
		
			wrld->ipo= newlibadr_us(fd, wrld->id.lib, wrld->ipo);
			
			for(a=0; a<8; a++) {
				mtex= wrld->mtex[a];
				if(mtex) {
					mtex->tex= newlibadr_us(fd, wrld->id.lib, mtex->tex);
					mtex->object= newlibadr(fd, wrld->id.lib, mtex->object);
				}
			}

			lib_link_scriptlink(fd, &wrld->id, &wrld->scriptlink);
			
			wrld->id.flag -= LIB_NEEDLINK;
		}
		wrld= wrld->id.next;
	}
}

static void direct_link_world(FileData *fd, World *wrld)
{
	int a;

	direct_link_scriptlink(fd, &wrld->scriptlink);
	
	for(a=0; a<8; a++) {
		wrld->mtex[a]= newdataadr(fd, wrld->mtex[a]);
	}
}


/* ************ READ IPO ***************** */

static void lib_link_ipo(FileData *fd, Main *main)
{
	Ipo *ipo;
	
	ipo= main->ipo.first;
	while(ipo) {
		if(ipo->id.flag & LIB_NEEDLINK) {
			
			ipo->id.flag -= LIB_NEEDLINK;
		}
		ipo= ipo->id.next;
	}
}

static void direct_link_ipo(FileData *fd, Ipo *ipo)
{
	IpoCurve *icu;
	
	link_list(fd, &(ipo->curve));
	icu= ipo->curve.first;
	while(icu) {
		icu->bezt= newdataadr(fd, icu->bezt);
		icu->bp= newdataadr(fd, icu->bp);
		icu= icu->next;
	}
}

/* ************ READ VFONT ***************** */

static void lib_link_vfont(FileData *fd, Main *main)
{
	VFont *vf;
	
	vf= main->vfont.first;
	while(vf) {
		if(vf->id.flag & LIB_NEEDLINK) {
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

static void lib_link_text(FileData *fd, Main *main)
{
	Text *text;
	
	text= main->text.first;
	while(text) {
		if(text->id.flag & LIB_NEEDLINK) {
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
	if(text->flags & TXT_ISEXT) {
		reopen_text(text);
	} else {
*/

	link_list(fd, &text->lines);

	text->curl= newdataadr(fd, text->curl);
	text->sell= newdataadr(fd, text->sell);

	ln= text->lines.first;
	while(ln) {
		ln->line= newdataadr(fd, ln->line);

		if (ln->len != (int) strlen(ln->line)) {
			printf("Error loading text, line lengths differ\n");
			ln->len = strlen(ln->line);
		}

		ln= ln->next;
	}

	text->flags = (text->flags|TXT_ISTMP) & ~TXT_ISEXT;

	text->id.us= 1;
}

/* ************ READ IMAGE ***************** */

static void lib_link_image(FileData *fd, Main *main)
{
	Image *ima;
	
	ima= main->image.first;
	while (ima) {
		if(ima->id.flag & LIB_NEEDLINK) {
			
			ima->id.flag -= LIB_NEEDLINK;
		}
		ima= ima->id.next;
	}
}

static void direct_link_image(FileData *fd, Image *ima)
{
	ima->ibuf= 0;
	ima->anim= 0;
	memset(ima->mipmap, 0, sizeof(ima->mipmap));
	ima->repbind= 0;
	ima->bindcode= 0;
		
	ima->packedfile = direct_link_packedfile(fd, ima->packedfile);

	ima->ok= 1;
}


/* ************ READ CURVE ***************** */

static void lib_link_curve(FileData *fd, Main *main)
{
	Curve *cu;
	int a;
	
	cu= main->curve.first;
	while(cu) {
		if(cu->id.flag & LIB_NEEDLINK) {
		
			for(a=0; a<cu->totcol; a++) cu->mat[a]= newlibadr_us(fd, cu->id.lib, cu->mat[a]);

			cu->bevobj= newlibadr(fd, cu->id.lib, cu->bevobj);
			cu->textoncurve= newlibadr(fd, cu->id.lib, cu->textoncurve);
			cu->vfont= newlibadr_us(fd, cu->id.lib, cu->vfont);

			cu->ipo= newlibadr_us(fd, cu->id.lib, cu->ipo);
			cu->key= newlibadr_us(fd, cu->id.lib, cu->key);
			
			cu->id.flag -= LIB_NEEDLINK;
		}
		cu= cu->id.next;
	}
}


static void switch_endian_knots(Nurb *nu)
{
	int len;
	
	if(nu->knotsu) {
		len= KNOTSU(nu);
		while(len--) {
			SWITCH_INT(nu->knotsu[len]);
		}
	}
	if(nu->knotsv) {
		len= KNOTSV(nu);
		while(len--) {
			SWITCH_INT(nu->knotsv[len]);
		}
	}
}

static void direct_link_curve(FileData *fd, Curve *cu)
{
	Nurb *nu;
	
	cu->mat= newdataadr(fd, cu->mat);
	test_pointer_array(fd, (void **)&cu->mat);
	cu->str= newdataadr(fd, cu->str);

	if(cu->vfont==0) link_list(fd, &(cu->nurb));
	else {
		cu->nurb.first=cu->nurb.last= 0;
	}
	
	cu->bev.first=cu->bev.last= 0;
	cu->disp.first=cu->disp.last= 0;
	cu->path= 0;
	
	nu= cu->nurb.first;
	while(nu) {
		nu->bezt= newdataadr(fd, nu->bezt);
		nu->bp= newdataadr(fd, nu->bp);
		nu->knotsu= newdataadr(fd, nu->knotsu);
		nu->knotsv= newdataadr(fd, nu->knotsv);

		if(fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
			switch_endian_knots(nu);
		}
		
		nu= nu->next;
	}
	cu->bb= 0;
}

/* ************ READ TEX ***************** */

static void lib_link_texture(FileData *fd, Main *main)
{
	Tex *tex;
	
	tex= main->tex.first;
	while(tex) {
		if(tex->id.flag & LIB_NEEDLINK) {
		
			tex->ima= newlibadr_us(fd, tex->id.lib, tex->ima);
			tex->ipo= newlibadr_us(fd, tex->id.lib, tex->ipo);
			if(tex->env) tex->env->object= newlibadr(fd, tex->id.lib, tex->env->object);
			
			tex->id.flag -= LIB_NEEDLINK;
		}
		tex= tex->id.next;
	}
}

static void direct_link_texture(FileData *fd, Tex *tex)
{
	tex->plugin= newdataadr(fd, tex->plugin);
	if(tex->plugin) {
		tex->plugin->handle= 0;
		open_plugin_tex(tex->plugin);
	}
	tex->coba= newdataadr(fd, tex->coba);
	tex->env= newdataadr(fd, tex->env);
	if(tex->env) {
		tex->env->ima= 0;
		memset(tex->env->cube, 0, 6*sizeof(void *));
		tex->env->ok= 0;
	}
}



/* ************ READ MATERIAL ***************** */

static void lib_link_material(FileData *fd, Main *main)
{
	Material *ma;
	MTex *mtex;
	int a;
	
	ma= main->mat.first;
	while(ma) {
		if(ma->id.flag & LIB_NEEDLINK) {
		
			ma->ipo= newlibadr_us(fd, ma->id.lib, ma->ipo);
			
			for(a=0; a<8; a++) {
				mtex= ma->mtex[a];
				if(mtex) {
					mtex->tex= newlibadr_us(fd, ma->id.lib, mtex->tex);
					mtex->object= newlibadr(fd, ma->id.lib, mtex->object);
				}
			}
			lib_link_scriptlink(fd, &ma->id, &ma->scriptlink);
			ma->id.flag -= LIB_NEEDLINK;
		}
		ma= ma->id.next;
	}
}

static void direct_link_material(FileData *fd, Material *ma)
{
	int a;
	
	direct_link_scriptlink(fd, &ma->scriptlink);
	
	for(a=0; a<8; a++) {
		ma->mtex[a]= newdataadr(fd, ma->mtex[a]);
	}
	ma->ren= 0;	/* should not be needed, nevertheless... */
}

/* ************ READ MESH ***************** */

static void lib_link_mesh(FileData *fd, Main *main)
{
	Mesh *me;
		
	me= main->mesh.first;
	while(me) {
		if(me->id.flag & LIB_NEEDLINK) {
			int i;

			for(i=0; i<me->totcol; i++)
				me->mat[i]= newlibadr_us(fd, me->id.lib, me->mat[i]);
				
			me->ipo= newlibadr_us(fd, me->id.lib, me->ipo);
			me->key= newlibadr_us(fd, me->id.lib, me->key);
			me->texcomesh= newlibadr_us(fd, me->id.lib, me->texcomesh);
			
			if(me->tface) {
				TFace *tfaces= me->tface;
				
				for (i=0; i<me->totface; i++) {
					TFace *tf= &tfaces[i];

					tf->tpage= newlibadr(fd, me->id.lib, tf->tpage);
					if(tf->tpage) {
						Image *ima= tf->tpage;
						if(ima->id.us==0)
							ima->id.us= 1;
					}
				}
			}
			me->id.flag -= LIB_NEEDLINK;
		}
		me= me->id.next;
	}
}

static void direct_link_dverts(FileData *fd, int count, MDeformVert *mdverts)
{
	int	i, j;

	if (!mdverts)
		return;

	for (i=0; i<count; i++) {
		mdverts[i].dw=newdataadr(fd, mdverts[i].dw);
		if (!mdverts[i].dw)
			mdverts[i].totweight=0;

		for (j=0; j< mdverts[i].totweight; j++) {
			mdverts[i].dw[j].data = NULL;	// not saved in file, clear pointer
		}
	}
}

static void direct_link_mesh(FileData *fd, Mesh *mesh)
{
	mesh->mat= newdataadr(fd, mesh->mat);
	test_pointer_array(fd, (void **)&mesh->mat);
	mesh->mvert= newdataadr(fd, mesh->mvert);

	mesh->dvert= newdataadr(fd, mesh->dvert);
	direct_link_dverts(fd, mesh->totvert, mesh->dvert);

	mesh->mface= newdataadr(fd, mesh->mface);
	mesh->tface= newdataadr(fd, mesh->tface);
	mesh->mcol= newdataadr(fd, mesh->mcol);
	mesh->msticky= newdataadr(fd, mesh->msticky);
	
	mesh->disp.first= mesh->disp.last= 0;
	mesh->bb= 0;
	mesh->oc= 0;
	mesh->dface= 0;
	mesh->orco= 0;
	
	if (mesh->tface) {
		TFace *tfaces= mesh->tface;
		int i;
		
		for (i=0; i<mesh->totface; i++) {
			TFace *tf= &tfaces[i];

			if(fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
				SWITCH_INT(tf->col[0]);
				SWITCH_INT(tf->col[1]);
				SWITCH_INT(tf->col[2]);
				SWITCH_INT(tf->col[3]);
			}
		}
	}
}

/* ************ READ OBJECT ***************** */

static void lib_link_object(FileData *fd, Main *main)
{
	Object *ob;
	bSensor *sens;
	bController *cont;
	bActuator *act;

	void *poin;
	int warn=0, a;
	
	ob= main->object.first;
	while(ob) {
		if(ob->id.flag & LIB_NEEDLINK) {
	
			ob->parent= newlibadr(fd, ob->id.lib, ob->parent);
			ob->track= newlibadr(fd, ob->id.lib, ob->track);
			ob->ipo= newlibadr_us(fd, ob->id.lib, ob->ipo);
			ob->action = newlibadr_us(fd, ob->id.lib, ob->action);

//			ob->activecon = newglobadr(fd, ob->activecon);

			poin= ob->data;		
			ob->data= newlibadr_us(fd, ob->id.lib, ob->data);
			
			if(ob->data==NULL && poin!=NULL) {
				ob->type= OB_EMPTY;
				warn= 1;
				if(ob->id.lib) printf("Can't find obdata of %s lib %s\n", ob->id.name+2, ob->id.lib->name);
				else printf("Object %s lost data. Lib:%x\n", ob->id.name+2, (unsigned int) ob->id.lib);
			}
			for(a=0; a<ob->totcol; a++) ob->mat[a]= newlibadr_us(fd, ob->id.lib, ob->mat[a]);
			
			ob->id.flag -= LIB_NEEDLINK;
			/* if id.us==0 a new base will be created later on */
			
			/* WARNING! Also check expand_object(), should reflect the stuff below. */
			lib_link_pose(fd, &ob->id, ob->pose);
			lib_link_constraints(fd, &ob->id, &ob->constraints);
			lib_link_nlastrips(fd, &ob->id, &ob->nlastrips);
			lib_link_constraint_channels(fd, &ob->id, &ob->constraintChannels);
			

			sens= ob->sensors.first;
			while(sens) {
				for(a=0; a<sens->totlinks; a++) {
					sens->links[a]= newglobadr(fd, sens->links[a]);
				}
				if(sens->type==SENS_TOUCH) {
					bTouchSensor *ts= sens->data;
					ts->ma= newlibadr(fd, ob->id.lib, ts->ma);
				}
				else if(sens->type==SENS_MESSAGE) {
					bMessageSensor *ms= sens->data;
					ms->fromObject=
					    newlibadr(fd, ob->id.lib, ms->fromObject);
				}
				sens= sens->next;
			}
			
			cont= ob->controllers.first;
			while(cont) {
				for(a=0; a<cont->totlinks; a++) {
					cont->links[a]= newglobadr(fd, cont->links[a]);
				}
				if(cont->type==CONT_PYTHON) {
					bPythonCont *pc= cont->data;
					pc->text= newlibadr(fd, ob->id.lib, pc->text);
				}
				cont->slinks= NULL;
				cont->totslinks= 0;
				
				cont= cont->next;
			}
			
			act= ob->actuators.first;
			while(act) {
				if(act->type==ACT_SOUND) {
					bSoundActuator *sa= act->data;
					sa->sound= newlibadr_us(fd, ob->id.lib, sa->sound);
				}
				else if(act->type==ACT_CD) {
					/* bCDActuator *cda= act->data; */
				}
				else if(act->type==ACT_GAME) {
					/* bGameActuator *ga= act->data; */
				}
				else if(act->type==ACT_CAMERA) {
					bCameraActuator *ca= act->data;
					ca->ob= newlibadr(fd, ob->id.lib, ca->ob);
				}
					/* leave this one, it's obsolete but necessary to read for conversion */
				else if(act->type==ACT_ADD_OBJECT) {
					bAddObjectActuator *eoa= act->data;
					if(eoa) eoa->ob= newlibadr(fd, ob->id.lib, eoa->ob);
				}
				else if(act->type==ACT_EDIT_OBJECT) {
					bEditObjectActuator *eoa= act->data;
					if(eoa==NULL) {
						init_actuator(act);
					}
					eoa->ob= newlibadr(fd, ob->id.lib, eoa->ob);
					eoa->me= newlibadr(fd, ob->id.lib, eoa->me);
				}
				else if(act->type==ACT_SCENE) {
					bSceneActuator *sa= act->data;
					sa->camera= newlibadr(fd, ob->id.lib, sa->camera);
					sa->scene= newlibadr(fd, ob->id.lib, sa->scene);
				}
				else if(act->type==ACT_ACTION) {
					bActionActuator *aa= act->data;
					aa->act= newlibadr(fd, ob->id.lib, aa->act);
				}
				else if(act->type==ACT_PROPERTY) {
					bPropertyActuator *pa= act->data;
					pa->ob= newlibadr(fd, ob->id.lib, pa->ob);
				}
				else if(act->type==ACT_MESSAGE) {
					bMessageActuator *ma= act->data;
					ma->toObject= newlibadr(fd, ob->id.lib, ma->toObject);
				}
				act= act->next;
			}
			
			lib_link_scriptlink(fd, &ob->id, &ob->scriptlink);
		}
		ob= ob->id.next;
	}
	
	if(warn) error("WARNING IN CONSOLE");
}


static void direct_link_pose(FileData *fd, bPose *pose) {

	bPoseChannel *chan;

	if (!pose)
		return;

	link_list(fd, &pose->chanbase);

	for (chan = pose->chanbase.first; chan; chan=chan->next) {
		direct_link_constraints(fd, &chan->constraints);
	}

}

static void direct_link_object(FileData *fd, Object *ob)
{
	PartEff *paf;
	bProperty *prop;
	bSensor *sens;
	bController *cont;
	bActuator *act;

	ob->disp.first=ob->disp.last= 0;

	ob->pose= newdataadr(fd, ob->pose);
	direct_link_pose(fd, ob->pose);
	
	link_list(fd, &ob->defbase);
	link_list(fd, &ob->nlastrips);
	link_list(fd, &ob->constraintChannels);

	ob->activecon = newdataadr(fd, ob->activecon);

	direct_link_scriptlink(fd, &ob->scriptlink);

	ob->mat= newdataadr(fd, ob->mat);
	test_pointer_array(fd, (void **)&ob->mat);
	link_list(fd, &ob->effect);
	paf= ob->effect.first;
	while(paf) {
		if(paf->type==EFF_PARTICLE) {
			paf->keys= 0;
		}
		if(paf->type==EFF_WAVE) {
			
		}
		paf= paf->next;
	}

	link_list(fd, &ob->network);
	
	link_list(fd, &ob->prop);
	prop= ob->prop.first;
	while(prop) {
		prop->poin= newdataadr(fd, prop->poin);
		if(prop->poin==0) prop->poin= &prop->data;
		prop= prop->next;
	}

	link_list(fd, &ob->sensors);
	sens= ob->sensors.first;
	while(sens) {
		sens->data= newdataadr(fd, sens->data);	
		sens->links= newdataadr(fd, sens->links);
		test_pointer_array(fd, (void **)&sens->links);
		sens= sens->next;
	}

	direct_link_constraints(fd, &ob->constraints);

	link_glob_list(fd, &ob->controllers);
	cont= ob->controllers.first;
	while(cont) {
		cont->data= newdataadr(fd, cont->data);
		cont->links= newdataadr(fd, cont->links);
		test_pointer_array(fd, (void **)&cont->links);
		cont= cont->next;
	}

	link_glob_list(fd, &ob->actuators);
	act= ob->actuators.first;
	while(act) {
		act->data= newdataadr(fd, act->data);
		act= act->next;
	}

	ob->bb= 0;
}

/* ************ READ SCENE ***************** */

static void lib_link_scene(FileData *fd, Main *main)
{
	Scene *sce;
	Base *base, *next;
	Editing *ed;
	Sequence *seq;
	
	sce= main->scene.first;
	while(sce) {
		if(sce->id.flag & LIB_NEEDLINK) {
			sce->id.us= 1;
			sce->camera= newlibadr(fd, sce->id.lib, sce->camera);
			sce->world= newlibadr_us(fd, sce->id.lib, sce->world);
			sce->set= newlibadr(fd, sce->id.lib, sce->set);
			sce->ima= newlibadr_us(fd, sce->id.lib, sce->ima);
			sce->group= newlibadr_us(fd, sce->id.lib, sce->group);

			base= sce->base.first;
			while(base) {
				next= base->next;
				
				/* base->object= newlibadr_us(fd, sce->id.lib, base->object); */

				base->object= newlibadr_us_type(fd, ID_OB, base->object);
				
				if(base->object==0) {
					printf("LIB ERROR: base removed\n");
					BLI_remlink(&sce->base, base);
					if(base==sce->basact) sce->basact= 0;
					MEM_freeN(base);
				}
				base= next;
			}
			
			ed= sce->ed;
			if(ed) {
				WHILE_SEQ(ed->seqbasep) {
					if(seq->ipo) seq->ipo= newlibadr_us(fd, sce->id.lib, seq->ipo);
					if(seq->scene) seq->scene= newlibadr(fd, sce->id.lib, seq->scene);
					if(seq->sound) {
						seq->sound= newlibadr(fd, sce->id.lib, seq->sound);
						if (seq->sound) {
							seq->sound->id.us++;
							seq->sound->flags |= SOUND_FLAGS_SEQUENCE;
						}
					}
					seq->anim= 0;
				}
				END_SEQ
			}
			sce->id.flag -= LIB_NEEDLINK;
		}
		
		lib_link_scriptlink(fd, &sce->id, &sce->scriptlink);
		
		sce= sce->id.next;
	}
}

static void link_recurs_seq(FileData *fd, ListBase *lb)
{
	Sequence *seq;

	link_list(fd, lb);
	seq= lb->first;
	while(seq) {
		if(seq->seqbase.first) link_recurs_seq(fd, &seq->seqbase);
		seq= seq->next;
	}
}

static void direct_link_scene(FileData *fd, Scene *sce)
{
	Editing *ed;
	Sequence *seq;
	StripElem *se;
	int a;
	
	link_list(fd, &(sce->base));

	sce->basact= newdataadr(fd, sce->basact);

	sce->radio= newdataadr(fd, sce->radio);
	sce->fcam= newdataadr(fd, sce->fcam);

	sce->r.avicodecdata = newdataadr(fd, sce->r.avicodecdata);
	if (sce->r.avicodecdata) {
		sce->r.avicodecdata->lpFormat = newdataadr(fd, sce->r.avicodecdata->lpFormat);
		sce->r.avicodecdata->lpParms = newdataadr(fd, sce->r.avicodecdata->lpParms);
	}

	sce->r.qtcodecdata = newdataadr(fd, sce->r.qtcodecdata);
	if (sce->r.qtcodecdata) {
		sce->r.qtcodecdata->cdParms = newdataadr(fd, sce->r.qtcodecdata->cdParms);
	}

	if(sce->ed) {
		ed= sce->ed= newdataadr(fd, sce->ed);
		
		ed->metastack.first= ed->metastack.last= 0;
		
		/* recursive link sequences, lb will be correctly initialized */
		link_recurs_seq(fd, &ed->seqbase);
		
		ed->seqbasep= &ed->seqbase;
		
		WHILE_SEQ(ed->seqbasep) {
			seq->seq1= newdataadr(fd, seq->seq1);
			seq->seq2= newdataadr(fd, seq->seq2);
			seq->seq3= newdataadr(fd, seq->seq3);
			/* a patch: after introduction of effects with 3 input strips */
			if(seq->seq3==0) seq->seq3= seq->seq2;
			
			seq->curelem= 0;
			
			seq->plugin= newdataadr(fd, seq->plugin);
			if(seq->plugin) open_plugin_seq(seq->plugin, seq->name+2);
	
			seq->strip= newdataadr(fd, seq->strip);
			if(seq->strip && seq->strip->done==0) {
				seq->strip->done= 1;
				
				/* standard: strips from effects/metas are not written, but are mallocced */
				
				if(seq->type==SEQ_IMAGE) {
					seq->strip->stripdata= newdataadr(fd, seq->strip->stripdata);
					se= seq->strip->stripdata;
					if(se) {
						for(a=0; a<seq->strip->len; a++, se++) {
							se->ok= 1;
							se->ibuf= 0;
						}
					}
				}
				else if(seq->type==SEQ_MOVIE) {
					/* only first stripelem is in file */
					se= newdataadr(fd, seq->strip->stripdata);
					
					if(se) {
						seq->strip->stripdata= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
						*seq->strip->stripdata= *se;
						MEM_freeN(se);
						
						se= seq->strip->stripdata;
					
						for(a=0; a<seq->strip->len; a++, se++) {
							se->ok= 1;
							se->ibuf= 0;
							se->nr= a + 1;
						}
					}
				}
				else if(seq->type==SEQ_SOUND) {
					/* only first stripelem is in file */
					se= newdataadr(fd, seq->strip->stripdata);
					
					if(se) {
						seq->strip->stripdata= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");
						*seq->strip->stripdata= *se;
						MEM_freeN(se);
						
						se= seq->strip->stripdata;
											
						for(a=0; a<seq->strip->len; a++, se++) {
							se->ok= 2; /* why? */
							se->ibuf= 0;
							se->nr= a + 1;
						}
					}
				}				
				else if(seq->len>0) 
					seq->strip->stripdata= MEM_callocN(seq->len*sizeof(StripElem), "stripelem");

			}
		}
		END_SEQ
	}

	direct_link_scriptlink(fd, &sce->scriptlink);
}

/* ************ READ SCREEN ***************** */

static void lib_link_screen(FileData *fd, Main *main)
{
	bScreen *sc;
	ScrArea *sa;
	
	sc= main->screen.first;
	while(sc) {
		if(sc->id.flag & LIB_NEEDLINK) {
			sc->id.us= 1;
			sc->scene= newlibadr(fd, sc->id.lib, sc->scene);
			
			sa= sc->areabase.first;
			while(sa) {
				SpaceLink *sl;
				
				sa->full= newlibadr(fd, sc->id.lib, sa->full);
	
				for (sl= sa->spacedata.first; sl; sl= sl->next) {
					if(sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D*) sl;
						
						v3d->camera= newlibadr(fd, sc->id.lib, v3d->camera);
						
						if(v3d->bgpic) {
							v3d->bgpic->ima= newlibadr_us(fd, sc->id.lib, v3d->bgpic->ima);
							v3d->bgpic->tex= newlibadr_us(fd, sc->id.lib, v3d->bgpic->tex);
							v3d->bgpic->rect= 0;
						}
						if(v3d->localvd) {
							v3d->localvd->camera= newlibadr(fd, sc->id.lib, v3d->localvd->camera);
						}
					}
					else if(sl->spacetype==SPACE_IPO) {
						SpaceIpo *sipo= (SpaceIpo *)sl;
						sipo->editipo= 0;
						sipo->from= newlibadr(fd, sc->id.lib, sipo->from);
						sipo->ipokey.first= sipo->ipokey.last= 0;
						sipo->ipo= newlibadr(fd, sc->id.lib, sipo->ipo);
					}
					else if(sl->spacetype==SPACE_BUTS) {
						SpaceButs *sbuts= (SpaceButs *)sl;
						sbuts->rect= 0;
						sbuts->lockpoin= 0;
						if(main->versionfile<132) set_rects_butspace(sbuts);
					}
					else if(sl->spacetype==SPACE_FILE) {
						SpaceFile *sfile= (SpaceFile *)sl;
						
						sfile->filelist= 0;
						sfile->libfiledata= 0;
						sfile->returnfunc= 0;
					}
					else if(sl->spacetype==SPACE_IMASEL) {
						check_imasel_copy((SpaceImaSel *)sl);
					}
					else if(sl->spacetype==SPACE_ACTION) {
						SpaceAction *saction= (SpaceAction *)sl;
						saction->action = newlibadr(fd, sc->id.lib, saction->action);
					}
					else if(sl->spacetype==SPACE_IMAGE) {
						SpaceImage *sima= (SpaceImage *)sl;
						
						sima->image= newlibadr_us(fd, sc->id.lib, sima->image);
					}
					else if(sl->spacetype==SPACE_NLA){
						SpaceNla *snla= (SpaceNla *)sl;	
					}
					else if(sl->spacetype==SPACE_TEXT) {
						SpaceText *st= (SpaceText *)sl;
				
						st->text= newlibadr(fd, sc->id.lib, st->text);
						
						st->py_draw= NULL;
						st->py_event= NULL;
						st->py_button= NULL;
						st->py_globaldict= NULL;
					}
					else if(sl->spacetype==SPACE_OOPS) {
						SpaceOops *so= (SpaceOops *)sl;
						Oops *oops;
						
						oops= so->oops.first;
						while(oops) {
							oops->id= newlibadr(fd, 0, oops->id);
							oops= oops->next;
						}
						so->lockpoin= 0;
					}
					else if(sl->spacetype==SPACE_SOUND) {
						SpaceSound *ssound= (SpaceSound *)sl;
						
						ssound->sound= newlibadr_us(fd, sc->id.lib, ssound->sound);
					}
				}
				sa= sa->next;
			}
			sc->id.flag -= LIB_NEEDLINK;
		}
		sc= sc->id.next;
	}
}

static void direct_link_screen(FileData *fd, bScreen *sc)
{
	ScrArea *sa;
	ScrVert *sv;
	ScrEdge *se;
	Oops *oops;

	link_list(fd, &(sc->vertbase));
	link_list(fd, &(sc->edgebase));
	link_list(fd, &(sc->areabase));
	sc->winakt= 0;

	/* edges */
	se= sc->edgebase.first;
	while(se) {
		se->v1= newdataadr(fd, se->v1);
		se->v2= newdataadr(fd, se->v2);
		if( (long)se->v1 > (long)se->v2) {
			sv= se->v1;
			se->v1= se->v2;
			se->v2= sv;
		}
		
		if(se->v1==NULL) {
			printf("error reading screen... file corrupt\n");
			se->v1= se->v2;
		}
		se= se->next;
	}

	/* areas */
	sa= sc->areabase.first;
	while(sa) {
		SpaceLink *sl;
		
		link_list(fd, &(sa->spacedata));

		for (sl= sa->spacedata.first; sl; sl= sl->next) {
			if (sl->spacetype==SPACE_VIEW3D) {
				View3D *v3d= (View3D*) sl;
				v3d->bgpic= newdataadr(fd, v3d->bgpic);
				v3d->localvd= newdataadr(fd, v3d->localvd);
			}
			else if (sl->spacetype==SPACE_OOPS) {
				SpaceOops *soops= (SpaceOops*) sl;
				link_list(fd, &(soops->oops));
				oops= soops->oops.first;
				while(oops) {
					oops->link.first= oops->link.last= 0;
					oops= oops->next;
				}
			}
		}
		
		sa->v1= newdataadr(fd, sa->v1);
		sa->v2= newdataadr(fd, sa->v2);
		sa->v3= newdataadr(fd, sa->v3);
		sa->v4= newdataadr(fd, sa->v4);
		
		sa->win= sa->headwin= 0;

		sa->uiblocks.first= sa->uiblocks.last= NULL;
		
		sa= sa->next;
	}
} 

/* ********** READ LIBRARY *************** */


static void direct_link_library(FileData *fd, Library *lib)
{
	Main *newmain;
	
	/* new main */
	newmain= MEM_callocN(sizeof(Main), "directlink");
	BLI_addtail(&fd->mainlist, newmain);
	newmain->curlib= lib;
}

static void lib_link_library(FileData *fd, Main *main)
{
	Library *lib;
	
	lib= main->library.first;
	while(lib) {
		lib->id.us= 1;
		lib= lib->id.next;
	}
}

/* ************** READ SOUND ******************* */

static void direct_link_sound(FileData *fd, bSound *sound)
{
	sound->sample = NULL;
	sound->snd_sound = NULL;

	sound->packedfile = direct_link_packedfile(fd, sound->packedfile);
	sound->newpackedfile = direct_link_packedfile(fd, sound->newpackedfile);
}

static void lib_link_sound(FileData *fd, Main *main)
{
	bSound *sound;
	
	sound= main->sound.first;
	while(sound) {
		if(sound->id.flag & LIB_NEEDLINK) {
			sound->id.flag -= LIB_NEEDLINK;
			sound->ipo= newlibadr_us(fd, sound->id.lib, sound->ipo);
			sound->stream = 0;
		}
		sound= sound->id.next;
	}
}
/* ***************** READ GROUP *************** */

static void direct_link_group(FileData *fd, Group *group)
{
	GroupObject *go;
	ObjectKey *ok;
	
	link_list(fd, &group->gobject);
	link_list(fd, &group->gkey);
	group->active= newdataadr(fd, group->active);
	
	go= group->gobject.first;
	while(go) {
		link_list(fd, &go->okey);
		ok= go->okey.first;
		while(ok) {
			ok->gkey= newdataadr(fd, ok->gkey);
			ok= ok->next;
		}
		go= go->next;
	}
}

static void lib_link_group(FileData *fd, Main *main)
{
	Group *group= main->group.first;
	GroupObject *go;
	ObjectKey *ok;
	
	while(group) {
		if(group->id.flag & LIB_NEEDLINK) {
			group->id.flag -= LIB_NEEDLINK;
	
			go= group->gobject.first;
			while(go) {
				go->ob= newlibadr(fd, group->id.lib, go->ob);
				ok= go->okey.first;
				while(ok) {
					ok->parent= newlibadr(fd, group->id.lib, ok->parent);
					ok->track= newlibadr(fd, group->id.lib, ok->track);	
					ok->ipo= newlibadr_us(fd, group->id.lib, ok->ipo);	
					ok= ok->next;
				}
				go= go->next;
			}
		}
		group= group->id.next;
	}
}

/* ************** GENERAL & MAIN ******************** */

static BHead *read_libblock(FileData *fd, Main *main, BHead *bhead, int flag, ID **id_r)
{
	/* this routine reads a libblock and its direct data. Use link functions
	 * to connect it all
	 */
	
	ID *id;
	ListBase *lb;
	
	if(bhead->code==ID_ID) {
		ID *linkedid= (ID *)(bhead + 1); /*  BHEAD+DATA dependancy */
		
		lb= wich_libbase(main, GS(linkedid->name));
	}
	else {
		lb= wich_libbase(main, bhead->code);
	}
	
	/* read libblock */
	id = read_struct(fd, bhead);
	if (id_r)
		*id_r= id;
	if (!id)
		return blo_nextbhead(fd, bhead);
		
	oldnewmap_insert(fd->libmap, bhead->old, id, 1);
	BLI_addtail(lb, id);
	
	/* clear first 8 bits */
	id->flag= (id->flag & 0xFF00) | flag | LIB_NEEDLINK;
	id->lib= main->curlib;
	if(id->flag & LIB_FAKEUSER) id->us= 1;
	else id->us= 0;
	
	/* this case cannot be direct_linked: it's just the ID part */
	if(bhead->code==ID_ID) {
		return blo_nextbhead(fd, bhead);
	}
	
	bhead = blo_nextbhead(fd, bhead);
	
		/* read all data */
	while(bhead && bhead->code==DATA) {
		void *data= read_struct(fd, bhead);
	
		if (data) {
			oldnewmap_insert(fd->datamap, bhead->old, data, 0);
		}
	
		bhead = blo_nextbhead(fd, bhead);
	}
	
	/* init pointers direct data */
	switch( GS(id->name) ) {
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
		case ID_IK:
			direct_link_ika(fd, (Ika *)id);
			break;
		case ID_WO:
			direct_link_world(fd, (World *)id);
			break;
		case ID_LI:
			direct_link_library(fd, (Library *)id);
			break;
		case ID_CA:
			direct_link_camera(fd, (Camera *)id);
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
	}
	
	oldnewmap_free_unused(fd->datamap);
	oldnewmap_clear(fd->datamap);
		
	return (bhead);
}

static void link_global(FileData *fd, BlendFileData *bfd, FileGlobal *fg)
{
	// this is nonsense... will get rid of it once (ton) 
	bfd->winpos= fg->winpos;
	bfd->fileflags= fg->fileflags;
	bfd->displaymode= fg->displaymode;
	bfd->globalf= fg->globalf;
	bfd->curscreen= newlibadr(fd, 0, fg->curscreen);
}

static void vcol_to_fcol(Mesh *me)
{
	MFace *mface;
	unsigned int *mcol, *mcoln, *mcolmain;
	int a;
	
	if(me->totface==0 || me->mcol==0) return;
	
	mcoln= mcolmain= MEM_mallocN(4*sizeof(int)*me->totface, "mcoln");
	mcol = (unsigned int *)me->mcol;
	mface= me->mface;
	for(a=me->totface; a>0; a--, mface++) {
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
	case 312:	return F12KEY;
	case 159:	return PADSLASHKEY;
	case 161:	return PAD0;
	case 154:	return PAD1;
	case 150:	return PAD2;
	case 155:	return PAD3;
	case 151:	return PAD4;
	case 156:	return PAD5;
	case 152:	return PAD6;
	case 157:	return PAD7;
	case 153:	return PAD8;
	case 158:	return PAD9;
	default: return code;
	}
}

static void do_versions(Main *main)
{
	/* watch it: pointers from libdata have not been converted */
	
	if(main->versionfile == 100) {
		/* tex->extend and tex->imageflag have changed: */
		Tex *tex = main->tex.first;
		while(tex) {
			if(tex->id.flag & LIB_NEEDLINK) {
				
				if(tex->extend==0) {
					if(tex->xrepeat || tex->yrepeat) tex->extend= TEX_REPEAT;
					else {
						tex->extend= TEX_EXTEND;
						tex->xrepeat= tex->yrepeat= 1;
					}
				}
	
				if(tex->imaflag & TEX_ANIM5) {
					tex->imaflag |= TEX_MORKPATCH;
					tex->imaflag |= TEX_ANTIALI;
				}
			}
			tex= tex->id.next;
		}
	}
	if(main->versionfile <= 101) {
		/* frame mapping */
		Scene *sce = main->scene.first;
		while(sce) {
			sce->r.framapto= 100;
			sce->r.images= 100;
			sce->r.framelen= 1.0;
			sce= sce->id.next;
		}
	}
	if(main->versionfile <= 102) {
		/* init halo's at 1.0 */
		Material *ma = main->mat.first;
		while(ma) {
			ma->add= 1.0;
			ma= ma->id.next;
		}
	}
	if(main->versionfile <= 103) {
		/* new variable in object: colbits */
		Object *ob = main->object.first;
		int a;
		while(ob) {
			ob->colbits= 0;
			if(ob->totcol) {
				for(a=0; a<ob->totcol; a++) {
					if(ob->mat[a]) ob->colbits |= (1<<a);
				}
			}
			ob= ob->id.next;
		}
	}
	if(main->versionfile <= 104) {
		/* timeoffs moved */
		Object *ob = main->object.first;
		while(ob) {
			if(ob->transflag & 1) {
				ob->transflag -= 1;
				ob->ipoflag |= OB_OFFS_OB;
			}
			ob= ob->id.next;
		}
	}
	if(main->versionfile <= 105) {
		Object *ob = main->object.first;
		while(ob) {
			ob->dupon= 1; ob->dupoff= 0;
			ob->dupsta= 1; ob->dupend= 100;
			ob= ob->id.next;
		}
	}
	if(main->versionfile <= 106) {
		/* mcol changed */
		Mesh *me = main->mesh.first;
		while(me) {
			if(me->mcol) vcol_to_fcol(me);
			me= me->id.next;
		}
		
	}
	if(main->versionfile <= 107) {
		Object *ob;
		Scene *sce = main->scene.first;
		while(sce) {
			sce->r.mode |= R_GAMMA;
			sce= sce->id.next;
		}		
		ob= main->object.first;
		while(ob) {
			ob->ipoflag |= OB_OFFS_PARENT;
			if(ob->dt==0) ob->dt= 3;
			ob= ob->id.next;
		}
		
	}
	if(main->versionfile <= 109) {
		/* new variable: gridlines */
		bScreen *sc = main->screen.first;
		while(sc) {
			ScrArea *sa= sc->areabase.first;
			while(sa) {
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
	if(main->versionfile <= 112) {
		Mesh *me = main->mesh.first;
		while(me) {
			me->cubemapsize= 1.0;
			me= me->id.next;
		}
	}
	if(main->versionfile <= 113) {
		Material *ma = main->mat.first;
		while(ma) {
			if(ma->flaresize==0.0) ma->flaresize= 1.0;
			ma->subsize= 1.0;
			ma->flareboost= 1.0;
			ma= ma->id.next;
		}
	}
	if(main->versionfile <= 114) {
		Mesh *me= main->mesh.first;
		MFace *mface;
		int a_int;
	
		/* edge drawflags changed */
		while(me) {
			a_int= me->totface;
			mface= me->mface;
			while(a_int--) {
				if(mface->edcode & 16) {
					mface->edcode -= 16;
					mface->edcode |= ME_V3V1;
				}
				mface++;
			}
			me= me->id.next;
		}
	}
	
	
	if(main->versionfile <= 134) {
		Tex *tex = main->tex.first;
		while (tex) {
			if ((tex->rfac == 0.0) &&
			    (tex->gfac == 0.0) &&
			    (tex->bfac == 0.0)) {
				tex->rfac = 1.0;
				tex->gfac = 1.0;
				tex->bfac = 1.0;
				tex->filtersize = 1.0;
			}
			tex = tex->id.next;
		}
	}
	if(main->versionfile <= 140) {
		/* r-g-b-fac in texure */
		Tex *tex = main->tex.first;
		while (tex) {
			if ((tex->rfac == 0.0) &&
			    (tex->gfac == 0.0) &&
			    (tex->bfac == 0.0)) {
				tex->rfac = 1.0;
				tex->gfac = 1.0;
				tex->bfac = 1.0;
				tex->filtersize = 1.0;
			}
			tex = tex->id.next;
		}
	}
	if(main->versionfile <= 153) {
		Scene *sce = main->scene.first;
		while(sce) {
			if(sce->r.blurfac==0.0) sce->r.blurfac= 1.0;
			sce= sce->id.next;
		}
	}
	if(main->versionfile <= 163) {
		Scene *sce = main->scene.first;
		while(sce) {
			if(sce->r.frs_sec==0) sce->r.frs_sec= 25;
			sce= sce->id.next;
		}
	}
	if(main->versionfile <= 164) {
		Mesh *me= main->mesh.first;
		while(me) {
			me->smoothresh= 30;
			me= me->id.next;
		}
	}
	if(main->versionfile <= 165) {
		Mesh *me= main->mesh.first;
		TFace *tface;
		Ika *ika= main->ika.first;
		Deform *def;
		int nr;
		char *cp;
		
		while(ika) {
			ika->xyconstraint= .5;
			
			def= ika->def;
			nr= ika->totdef;
			while(nr--) {
				if(def->fac==0.0) def->fac= 1.0;
				def++;
			}
			ika= ika->id.next;
		}
		
		while(me) {
			if(me->tface) {
				nr= me->totface;
				tface= me->tface;
				while(nr--) {
					cp= (char *)&tface->col[0];
					if(cp[1]>126) cp[1]= 255; else cp[1]*=2;
					if(cp[2]>126) cp[2]= 255; else cp[2]*=2;
					if(cp[3]>126) cp[3]= 255; else cp[3]*=2;
					cp= (char *)&tface->col[1];
					if(cp[1]>126) cp[1]= 255; else cp[1]*=2;
					if(cp[2]>126) cp[2]= 255; else cp[2]*=2;
					if(cp[3]>126) cp[3]= 255; else cp[3]*=2;
					cp= (char *)&tface->col[2];
					if(cp[1]>126) cp[1]= 255; else cp[1]*=2;
					if(cp[2]>126) cp[2]= 255; else cp[2]*=2;
					if(cp[3]>126) cp[3]= 255; else cp[3]*=2;
					cp= (char *)&tface->col[3];
					if(cp[1]>126) cp[1]= 255; else cp[1]*=2;
					if(cp[2]>126) cp[2]= 255; else cp[2]*=2;
					if(cp[3]>126) cp[3]= 255; else cp[3]*=2;
					
					tface++;
				}
			}
			me= me->id.next;
		}
	}
	
	if(main->versionfile <= 169) {
		Mesh *me= main->mesh.first;
		while(me) {
			if(me->subdiv==0) me->subdiv= 3;
			me= me->id.next;
		}
	}
	
	if(main->versionfile <= 169) {
		bScreen *sc= main->screen.first;
		while(sc) {
			ScrArea *sa= sc->areabase.first;
			while(sa) {
				SpaceLink *sl= sa->spacedata.first;
				while(sl) {
					if(sl->spacetype==SPACE_IPO) {
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
	
	if(main->versionfile <= 170) {
		Object *ob = main->object.first;
		PartEff *paf;
		while (ob) {
			paf = give_parteff(ob);
			if (paf) {
				if (paf->staticstep == 0) {
					paf->staticstep= 5;
				}
			}
			ob = ob->id.next;
		}
	}
	
	if(main->versionfile <= 171) {
		bScreen *sc= main->screen.first;
		while(sc) {
			ScrArea *sa= sc->areabase.first;
			while(sa) {
				SpaceLink *sl= sa->spacedata.first;
				while(sl) {
					if(sl->spacetype==SPACE_TEXT) {
						SpaceText *st= (SpaceText*) sl;
						if(st->font_id>1) {
							st->font_id= 0;
							st->lheight= 13;
						}
					}
					sl= sl->next;
				}
				sa= sa->next;
			}
			sc= sc->id.next;
		}
	}
	
	if(main->versionfile <= 173) {
		int a, b;
		Mesh *me= main->mesh.first;
		while(me) {
			if(me->tface) {
				TFace *tface= me->tface;
				for(a=0; a<me->totface; a++, tface++) {
					for(b=0; b<4; b++) {
						tface->uv[b][0]/= 32767.0;
						tface->uv[b][1]/= 32767.0;
					}
				}
			}
			me= me->id.next;
		}
	}
	
	if(main->versionfile <= 191) {
		bScreen *sc= main->screen.first;
		Object *ob= main->object.first;
		Material *ma = main->mat.first;
		
		/* let faces have default add factor of 0.0 */
		while(ma) {
		  if (!(ma->mode & MA_HALO)) ma->add = 0.0;
		  ma = ma->id.next;
		}
	
		while(ob) {
			ob->mass= 1.0f;
			ob->damping= 0.1f;
			ob->quat[1]= 1.0f;		
			ob= ob->id.next;
		}
	
		while(sc) {
			ScrArea *sa= sc->areabase.first;
			while(sa) {
				SpaceLink *sl= sa->spacedata.first;
				while(sl) {
					if(sl->spacetype==SPACE_BUTS) {
						SpaceButs *sbuts= (SpaceButs*) sl;
						sbuts->scaflag= BUTS_SENS_LINK|BUTS_SENS_ACT|BUTS_CONT_ACT|BUTS_ACT_ACT|BUTS_ACT_LINK;
					}
					sl= sl->next;
				}
				sa= sa->next;
			}
			sc= sc->id.next;
		}
	}
	
	if(main->versionfile <= 193) {
		Object *ob= main->object.first;
		while(ob) {
			ob->inertia= 1.0f;
			ob->rdamping= 0.1f;
			ob= ob->id.next;
		}
	}
	
	if(main->versionfile <= 196) {
		Mesh *me= main->mesh.first;
		int a, b;
		while(me) {
			if(me->tface) {
				TFace *tface= me->tface;
				for(a=0; a<me->totface; a++, tface++) {
					for(b=0; b<4; b++) {
						tface->mode |= TF_DYNAMIC;
						tface->mode &= ~TF_INVISIBLE;
					}
				}
			}
			me= me->id.next;
		}
	}
		
	if(main->versionfile <= 200) {
		Object *ob= main->object.first;
		while(ob) {
			ob->scaflag = ob->gameflag & (64+128+256+512+1024+2048);
			    /* 64 is do_fh */
			ob->gameflag &= ~(128+256+512+1024+2048);
			ob = ob->id.next;
		}
	}
	
	if(main->versionfile <= 201) {
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
				if(act->type==ACT_IPO) {
					ia= act->data;
					prop= get_property(ob, ia->name);
					if(prop) {
						ia->type= ACT_IPO_FROM_PROP;
					}
				}
				else if(act->type==ACT_ADD_OBJECT) {
					aoa= act->data;
					eoa= MEM_callocN(sizeof(bEditObjectActuator), "edit ob act");
					eoa->type= ACT_EDOB_ADD_OBJECT;
					eoa->ob= aoa->ob;
					eoa->time= aoa->time;
					MEM_freeN(aoa);
					act->data= eoa;
					act->type= act->otype= ACT_EDIT_OBJECT;
				}
				else if(act->type==ACT_END_OBJECT) {
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
	
	if(main->versionfile <= 202) {
		/* add-object and end-object are joined to edit-object
		 * actuator */
		Object *ob= main->object.first;
		bActuator *act;
		bObjectActuator *oa;
		while(ob) {
			act= ob->actuators.first;
			while(act) {
				if(act->type==ACT_OBJECT) {
					oa= act->data;
					oa->flag &= ~(ACT_TORQUE_LOCAL|ACT_DROT_LOCAL);		/* this actuator didn't do local/glob rot before */
				}
				act= act->next;
			}
			ob= ob->id.next;
		}
	}
	
	if(main->versionfile <= 204) {
		/* patches for new physics */
		Object *ob= main->object.first;
		Material *ma= main->mat.first;
		bActuator *act;
		bObjectActuator *oa;
		bSound *sound;
		while(ob) {
			
			/* please check this for demo20 files like
			 * original Egypt levels etc.  converted
			 * rotation factor of 50 is not workable */
			act= ob->actuators.first;
			while(act) {
				if(act->type==ACT_OBJECT) {
					oa= act->data;
	
					oa->forceloc[0]*= 25.0;
					oa->forceloc[1]*= 25.0;
					oa->forceloc[2]*= 25.0;
					
					oa->forcerot[0]*= 10.0;
					oa->forcerot[1]*= 10.0;
					oa->forcerot[2]*= 10.0;
				}
				act= act->next;
			}
			ob= ob->id.next;
		}

		sound = main->sound.first;
		while (sound) {
			if (sound->volume < 0.01) {
				sound->volume = 1.0;
			}
			sound = sound->id.next;
		}
	}
	
	if(main->versionfile <= 205) {
		/* patches for new physics */
		Object *ob= main->object.first;
		bActuator *act;
		bSensor *sens;
		bEditObjectActuator *oa;
		bRaySensor *rs;
		bCollisionSensor *cs;
		while(ob) {
		    /* Set anisotropic friction off for old objects,
		     * values to 1.0.  */
			ob->gameflag &= ~OB_ANISOTROPIC_FRICTION;
			ob->anisotropicFriction[0] = 1.0;
			ob->anisotropicFriction[1] = 1.0;
			ob->anisotropicFriction[2] = 1.0;
			
			act= ob->actuators.first;
			while(act) {
				if(act->type==ACT_EDIT_OBJECT) {
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
				if(sens->type == SENS_RADAR) {
					bRadarSensor *s = sens->data;
					s->range = 10000.0;
				}
	
				/* Pulsing: defaults for new sensors. */
				if(sens->type != SENS_ALWAYS) {
					sens->pulse = 0;
					sens->freq = 0;
				} else {
					sens->pulse = 1;
				}
				
				/* Invert: off. */
				sens->invert = 0;
	
				/* Collision and ray: default = trigger
				 * on property. The material field can
				 * remain empty. */
				if(sens->type == SENS_COLLISION) {
					cs = (bCollisionSensor*) sens->data;
					cs->mode = 0;
				}
				if(sens->type == SENS_RAY) {
					rs = (bRaySensor*) sens->data;
					rs->mode = 0;
				}
				sens = sens->next;
			}
			ob= ob->id.next;
		}
		/* have to check the exact multiplier */
	}
	
	if(main->versionfile <= 210) {
		Scene *sce= main->scene.first;
		
		while(sce) {
			if(sce->r.postmul== 0.0) sce->r.postmul= 1.0;
			if(sce->r.postgamma== 0.0) sce->r.postgamma= 1.0;
			sce= sce->id.next;
		}
	}
	
	if(main->versionfile <= 211) {
		/* Render setting: per scene, the applicable gamma value
		 * can be set. Default is 1.0, which means no
		 * correction.  */
		bActuator *act;
		bObjectActuator *oa;
		Object *ob;
		Scene *sce= main->scene.first;
		while(sce) {
			sce->r.gamma = 2.0;
			sce= sce->id.next;
		}
		
		/* added alpha in obcolor */
		ob= main->object.first;
		while(ob) {
			ob->col[3]= 1.0;
			ob= ob->id.next;
		}		
	
		/* added alpha in obcolor */
		ob= main->object.first;
		while(ob) {
			act= ob->actuators.first;
			while(act) {
				if (act->type==ACT_OBJECT) {
					/* multiply velocity with 50 in old files */
					oa= act->data;
					if (fabs(oa->linearvelocity[0]) >= 0.01f)
						oa->linearvelocity[0] *= 50.0;
					if (fabs(oa->linearvelocity[1]) >= 0.01f)
						oa->linearvelocity[1] *= 50.0;
					if (fabs(oa->linearvelocity[2]) >= 0.01f)
						oa->linearvelocity[2] *= 50.0;
					if (fabs(oa->angularvelocity[0])>=0.01f)
						oa->angularvelocity[0] *= 50.0;
					if (fabs(oa->angularvelocity[1])>=0.01f)
						oa->angularvelocity[1] *= 50.0;
					if (fabs(oa->angularvelocity[2])>=0.01f)
						oa->angularvelocity[2] *= 50.0;
				}
				act= act->next;
			}
			ob= ob->id.next;
		}		
	}
	
	if(main->versionfile <= 212) {
	
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
	
			if (sound->attenuation > 0.0)
				sound->flags |= SOUND_FLAGS_3D;
			else
				sound->flags &= ~SOUND_FLAGS_3D;
	
			sound = sound->id.next;
		}
	
		ob = main->object.first;
	
		while (ob) {
			prop= ob->prop.first;
			while(prop) {
				if (prop->type == PROP_TIME) {
					// convert old PROP_TIME values from int to float
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
			} else {
				if (me->subdiv<2)
					me->subdiv= 1;
				else
					me->subdiv--;
			}
		}
	}
	
	if(main->versionfile <= 220) {
		Object *ob;
		Mesh *me;
		bArmature *arm;
	
		ob = main->object.first;
	
		/* adapt form factor in order to get the 'old' physics
		 * behaviour back...*/
	
		while (ob) {
			/* in future, distinguish between different
			 * object bounding shapes */
			ob->formfactor = 0.4f;
			/* patch form factor , note that inertia equiv radius
			 * of a rotation symmetrical obj */
			if (ob->inertia != 1.0) {
				ob->formfactor /= ob->inertia * ob->inertia;
			}	
			ob = ob->id.next;
		}
	
			/* Precalculate rest position matrices for old armatures. -rvo
			 */
		for (arm= main->armature.first; arm; arm= arm->id.next) {
			precalc_bonelist_irestmats (&arm->bonebase);
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
	if(main->versionfile <= 221) {
		Scene *sce= main->scene.first;
	
		// new variables for std-alone player and runtime
		while(sce) {
	
			sce->r.xplay= 640;
			sce->r.yplay= 480;
			sce->r.freqplay= 60;
		
			sce= sce->id.next;
		}
		
	}
	if(main->versionfile <= 222) {
		Scene *sce= main->scene.first;
	
		// new variables for std-alone player and runtime
		while(sce) {
	
			sce->r.depth= 32;

			sce= sce->id.next;
		}
	}


	if(main->versionfile <= 223) {
		VFont *vf;
		Image *ima;
		Object *ob;

		for (vf= main->vfont.first; vf; vf= vf->id.next) {
			if (BLI_streq(vf->name+strlen(vf->name)-6, ".Bfont")) {
				strcpy(vf->name, "<builtin>");
			}
		}

		/* Old textures animate at 25 FPS */
		for (ima = main->image.first; ima; ima=ima->id.next){
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
	if(main->versionfile <= 224) {
		bSound* sound;
		Scene *sce;
		Mesh *me;
		bScreen *sc;
		Object *ob;

		for (sound=main->sound.first; sound; sound=sound->id.next)
		{
			if (sound->packedfile) {
				if (sound->newpackedfile == NULL) {
					sound->newpackedfile = sound->packedfile;
				}
				sound->packedfile = NULL;
			}
		}

		/* Clear some (now) unused pose flags */
		for (ob=main->object.first; ob; ob=ob->id.next){
			if (ob->pose){
				bPoseChannel *pchan;
				for (pchan=ob->pose->chanbase.first; pchan; pchan=pchan->next){
					pchan->flag &= ~(POSE_UNUSED1|POSE_UNUSED2|POSE_UNUSED3|POSE_UNUSED4|POSE_UNUSED5);
				}
			}
		}

		/* Make sure that old subsurf meshes don't have zero subdivision level for rendering */
		for (me=main->mesh.first; me; me=me->id.next){
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
	if(main->versionfile <= 227) {
		Scene *sce;
		Material *ma;
		bScreen *sc;

		for (sce= main->scene.first; sce; sce= sce->id.next) {
			sce->audio.mixrate = 44100;
			sce->audio.flag |= AUDIO_SCRUB;
		}
		// init new shader vars
		for (ma= main->mat.first; ma; ma= ma->id.next) {
			ma->refrac= 4.0;
			ma->roughness= 0.5;
			ma->param[0]= 0.5;
			ma->param[1]= 0.1;
			ma->param[2]= 0.1;
			ma->param[3]= 0.05;
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

	/* don't forget to set version number in blender.c! */
}

static void lib_link_all(FileData *fd, Main *main)
{
	lib_link_screen(fd, main);
	lib_link_scene(fd, main);
	lib_link_object(fd, main);
	lib_link_curve(fd, main);
	lib_link_mball(fd, main);
	lib_link_material(fd, main);
	lib_link_texture(fd, main);
	lib_link_image(fd, main);
	lib_link_ipo(fd, main);
	lib_link_key(fd, main);
	lib_link_world(fd, main);
	lib_link_lamp(fd, main);
	lib_link_latt(fd, main);
	lib_link_ika(fd, main);
	lib_link_text(fd, main);
	lib_link_camera(fd, main);
	lib_link_sound(fd, main);
	lib_link_group(fd, main);
	lib_link_armature(fd, main);
	lib_link_action(fd, main);
	lib_link_vfont(fd, main);
	
	lib_link_mesh(fd, main);	/* as last: tpage images with users at zero */
	
	lib_link_library(fd, main);	/* only init users */
}

BlendFileData *blo_read_file_internal(FileData *fd, BlendReadError *error_r)
{
	BHead *bhead= blo_firstbhead(fd);
	BlendFileData *bfd;
	FileGlobal *fg = (FileGlobal *)NULL;

	bfd= MEM_callocN(sizeof(*bfd), "blendfiledata");
	bfd->main= MEM_callocN(sizeof(*bfd->main), "main");
	BLI_addtail(&fd->mainlist, bfd->main);
	
	bfd->main->versionfile= fd->fileversion;
		
	while(bhead) {
		switch(bhead->code) {
		case GLOB:
		case DATA:
		case DNA1:
		case TEST:
		case REND:
		case USER:
			if (bhead->code==USER) {
				bfd->user= read_struct(fd, bhead);
			} else if (bhead->code==GLOB) {
				fg= read_struct(fd, bhead);
			}
			bhead = blo_nextbhead(fd, bhead);
			break;
		case ENDB:
			bhead = NULL;
			break;
			
		case ID_LI:
			bhead = read_libblock(fd, bfd->main, bhead, LIB_LOCAL, NULL);
			break;
		case ID_ID:
				/* always adds to the most recently loaded
				 * ID_LI block, see direct_link_library.
				 * this is part of the file format definition.
				 */
			bhead = read_libblock(fd, fd->mainlist.last, bhead, LIB_READ+LIB_EXTERN, NULL);
			break;
			
		default:
			bhead = read_libblock(fd, bfd->main, bhead, LIB_LOCAL, NULL);
		}
	}

	/* before read_libraries */
	do_versions(bfd->main);
	read_libraries(fd, &fd->mainlist);
	blo_join_main(&fd->mainlist);
	
	lib_link_all(fd, bfd->main);
	link_global(fd, bfd, fg);	/* as last */

	if (!bfd->curscreen)
		bfd->curscreen= bfd->main->screen.first;

	if (bfd->curscreen) {
		bfd->curscene= bfd->curscreen->scene;
		if (!bfd->curscene) {
			bfd->curscene= bfd->main->scene.first;
			bfd->curscreen->scene= bfd->curscene;
		}
	}
		
	MEM_freeN(fg);

		/* require all files to have an active scene
		 * and screen. (implicitly: require all files
		 * to have at least one scene and one screen).
		 */
	if (!bfd->curscreen || !bfd->curscene) {
		*error_r= (!bfd->curscreen)?BRE_NO_SCREEN:BRE_NO_SCENE;
		
		BLO_blendfiledata_free(bfd);
		return NULL;
	}
	
	return bfd;
}

/* ************* APPEND LIBRARY ************** */

static BHead *find_previous_lib(FileData *fd, BHead *bhead)
{
	for (; bhead; bhead= blo_prevbhead(fd, bhead))
		if (bhead->code==ID_LI)
			break;
	
	return bhead;
}

static BHead *find_bhead(FileData *fd, void *old)
{
	BHead *bhead;
	
	if (!old)
		return NULL;
			
	for (bhead= blo_firstbhead(fd); bhead; bhead= blo_nextbhead(fd, bhead))
		if (bhead->old==old)
			return bhead;
	
	return NULL;
}

static ID *is_yet_read(Main *mainvar, BHead *bhead)
{
	ListBase *lb;
	ID *idtest, *id;
	
	// BHEAD+DATA dependancy
	idtest= (ID *)(bhead +1);
	lb= wich_libbase(mainvar, GS(idtest->name));
	if(lb) {
		id= lb->first;
		while(id) {
			if( strcmp(id->name, idtest->name)==0 ) return id;
			id= id->next;
		}
	}
	return 0;
}

static void expand_doit(FileData *fd, Main *mainvar, void *old)
{
	BHead *bhead;
	ID *id;
	
	bhead= find_bhead(fd, old);
	if(bhead) {
			/* from another library? */
		if(bhead->code==ID_ID) {
			BHead *bheadlib= find_previous_lib(fd, bhead);
			
			if(bheadlib) {
				// BHEAD+DATA dependancy
				Library *lib= (Library *)(bheadlib+1);
				mainvar= blo_find_main(&fd->mainlist, lib->name);
	
				id= is_yet_read(mainvar, bhead);
		
				if(id==0) {
					read_libblock(fd, mainvar, bhead, LIB_READ+LIB_INDIRECT, NULL);
					printf("expand: other lib %s\n", lib->name);
				}
				else {
					oldnewmap_insert(fd->libmap, bhead->old, id, 1);		
					printf("expand: already linked: %s lib: %s\n", id->name, lib->name);
				}
			}
		}
		else {
			id= is_yet_read(mainvar, bhead);
			if(id==0) {
				// BHEAD+DATA dependancy
				id= (ID *)(bhead+1);
				read_libblock(fd, mainvar, bhead, LIB_TESTIND, NULL);
			}
			else {
				oldnewmap_insert(fd->libmap, bhead->old, id, 1);		
				/* printf("expand: already read %s\n", id->name); */
			}
		}
	}
}

static void expand_key(FileData *fd, Main *mainvar, Key *key)
{
	expand_doit(fd, mainvar, key->ipo);
}


static void expand_texture(FileData *fd, Main *mainvar, Tex *tex)
{
	expand_doit(fd, mainvar, tex->ima);
}

static void expand_material(FileData *fd, Main *mainvar, Material *ma)
{
	int a;
	
	for(a=0; a<8; a++) {
		if(ma->mtex[a]) {
			expand_doit(fd, mainvar, ma->mtex[a]->tex);
			expand_doit(fd, mainvar, ma->mtex[a]->object);
		}
	}
	expand_doit(fd, mainvar, ma->ipo);
}

static void expand_lamp(FileData *fd, Main *mainvar, Lamp *la)
{
	int a;
	
	for(a=0; a<8; a++) {
		if(la->mtex[a]) {
			expand_doit(fd, mainvar, la->mtex[a]->tex);
			expand_doit(fd, mainvar, la->mtex[a]->object);
		}
	}
	expand_doit(fd, mainvar, la->ipo);
}

static void expand_lattice(FileData *fd, Main *mainvar, Lattice *lt)
{
	expand_doit(fd, mainvar, lt->ipo);
	expand_doit(fd, mainvar, lt->key);
}


static void expand_world(FileData *fd, Main *mainvar, World *wrld)
{
	int a;
	
	for(a=0; a<8; a++) {
		if(wrld->mtex[a]) {
			expand_doit(fd, mainvar, wrld->mtex[a]->tex);
			expand_doit(fd, mainvar, wrld->mtex[a]->object);
		}
	}
	expand_doit(fd, mainvar, wrld->ipo);
}


static void expand_mball(FileData *fd, Main *mainvar, MetaBall *mb)
{
	int a;
	
	for(a=0; a<mb->totcol; a++) {
		expand_doit(fd, mainvar, mb->mat[a]);
	}
}

static void expand_curve(FileData *fd, Main *mainvar, Curve *cu)
{
	int a;
	
	for(a=0; a<cu->totcol; a++) {
		expand_doit(fd, mainvar, cu->mat[a]);
	}
	expand_doit(fd, mainvar, cu->vfont);
	expand_doit(fd, mainvar, cu->key);
	expand_doit(fd, mainvar, cu->ipo);
	expand_doit(fd, mainvar, cu->bevobj);
	expand_doit(fd, mainvar, cu->textoncurve);
}

static void expand_mesh(FileData *fd, Main *mainvar, Mesh *me)
{
	int a;
	TFace *tface;
	
	for(a=0; a<me->totcol; a++) {
		expand_doit(fd, mainvar, me->mat[a]);
	}
	
	expand_doit(fd, mainvar, me->key);
	expand_doit(fd, mainvar, me->texcomesh);
	
	if(me->tface) {
		tface= me->tface;
		a= me->totface;
		while(a--) {
			if(tface->tpage) expand_doit(fd, mainvar, tface->tpage);
			tface++;
		}
	}
}

static void expand_constraints(FileData *fd, Main *mainvar, ListBase *lb)
{
	bConstraint *curcon;
	
	for (curcon=lb->first; curcon; curcon=curcon->next) {
		switch (curcon->type) {
		case CONSTRAINT_TYPE_ACTION:
			{
				bActionConstraint *data = (bActionConstraint*)curcon->data;
				expand_doit(fd, mainvar, data->tar);
				expand_doit(fd, mainvar, data->act);
			}
			break;
		case CONSTRAINT_TYPE_LOCLIKE:
			{
				bLocateLikeConstraint *data = (bLocateLikeConstraint*)curcon->data;
				expand_doit(fd, mainvar, data->tar);
				break;
			}
		case CONSTRAINT_TYPE_ROTLIKE:
			{
				bRotateLikeConstraint *data = (bRotateLikeConstraint*)curcon->data;
				expand_doit(fd, mainvar, data->tar);
				break;
			}
		case CONSTRAINT_TYPE_KINEMATIC:
			{
				bKinematicConstraint *data = (bKinematicConstraint*)curcon->data;
				expand_doit(fd, mainvar, data->tar);
				break;
			}
		case CONSTRAINT_TYPE_TRACKTO:
			{
				bTrackToConstraint *data = (bTrackToConstraint*)curcon->data;
				expand_doit(fd, mainvar, data->tar);
				break;
			}
		case CONSTRAINT_TYPE_NULL:
			break;
		default:
			break;
		}
	}
}

static void expand_bones(FileData *fd, Main *mainvar, Bone *bone)
{
	Bone *curBone;
	
//	expand_constraints(fd, main, &bone->constraints);
	
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
	}
}

static void expand_armature(FileData *fd, Main *mainvar, bArmature *arm)
{
	Bone *curBone;
	
	for (curBone = arm->bonebase.first; curBone; curBone=curBone->next) {
		expand_bones(fd, mainvar, curBone);
	}
}

static void expand_constraint_channels(FileData *fd, Main *mainvar, ListBase *chanbase)
{
	bConstraintChannel *chan;
	for (chan=chanbase->first; chan; chan=chan->next){
		expand_doit(fd, mainvar, chan->ipo);
	}
}

static void expand_action(FileData *fd, Main *mainvar, bAction *act)
{
	bActionChannel *chan;
	for (chan=act->chanbase.first; chan; chan=chan->next) {
		expand_doit(fd, mainvar, chan->ipo);
		expand_constraint_channels(fd, mainvar, &chan->constraintChannels);
	}
}

static void expand_object(FileData *fd, Main *mainvar, Object *ob)
{
	bSensor *sens;
	bController *cont;
	bActuator *act;
	bActionStrip *strip;
	int a;
	
		
	expand_doit(fd, mainvar, ob->data);
	expand_doit(fd, mainvar, ob->ipo);
	expand_doit(fd, mainvar, ob->action);
	
	expand_pose(fd, mainvar, ob->pose);	
	expand_constraints(fd, mainvar, &ob->constraints);
	expand_constraint_channels(fd, mainvar, &ob->constraintChannels);
	
	for (strip=ob->nlastrips.first; strip; strip=strip->next){
		expand_doit(fd, mainvar, strip->act);
		expand_doit(fd, mainvar, strip->ipo);
	}

	for(a=0; a<ob->totcol; a++) {
		expand_doit(fd, mainvar, ob->mat[a]);
	}
	sens= ob->sensors.first;
	while(sens) {
		if(sens->type==SENS_TOUCH) {
			bTouchSensor *ts= sens->data;
			expand_doit(fd, mainvar, ts->ma);
		}
		else if(sens->type==SENS_MESSAGE) {
			bMessageSensor *ms= sens->data;
			expand_doit(fd, mainvar, ms->fromObject);
		}
		sens= sens->next;
	}
	
	cont= ob->controllers.first;
	while(cont) {
		if(cont->type==CONT_PYTHON) {
			bPythonCont *pc= cont->data;
			expand_doit(fd, mainvar, pc->text);
		}
		cont= cont->next;
	}
	
	act= ob->actuators.first;
	while(act) {
		if(act->type==ACT_SOUND) {
			bSoundActuator *sa= act->data;
			expand_doit(fd, mainvar, sa->sound);
		}
		else if(act->type==ACT_CAMERA) {
			bCameraActuator *ca= act->data;
			expand_doit(fd, mainvar, ca->ob);
		}
		else if(act->type==ACT_EDIT_OBJECT) {
			bEditObjectActuator *eoa= act->data;
			if(eoa) {
				expand_doit(fd, mainvar, eoa->ob);
				expand_doit(fd, mainvar, eoa->me);
			}
		}
		else if(act->type==ACT_SCENE) {
			bSceneActuator *sa= act->data;
			expand_doit(fd, mainvar, sa->camera);
			expand_doit(fd, mainvar, sa->scene);
		}
		else if(act->type==ACT_ACTION) {
			bActionActuator *aa= act->data;
			expand_doit(fd, mainvar, aa->act);
		}
		else if(act->type==ACT_PROPERTY) {
			bPropertyActuator *pa= act->data;
			expand_doit(fd, mainvar, pa->ob);
		}
		else if(act->type==ACT_MESSAGE) {
			bMessageActuator *ma= act->data;
			expand_doit(fd, mainvar, ma->toObject);
		}
		act= act->next;
	}
}

static void expand_scene(FileData *fd, Main *mainvar, Scene *sce)
{
	Base *base;
	
	base= sce->base.first;
	while(base) {
		expand_doit(fd, mainvar, base->object);
		base= base->next;
	}
	expand_doit(fd, mainvar, sce->camera);
	expand_doit(fd, mainvar, sce->world);
}

static void expand_camera(FileData *fd, Main *mainvar, Camera *ca)
{
	expand_doit(fd, mainvar, ca->ipo);
}

static void expand_sound(FileData *fd, Main *mainvar, bSound *snd)
{
	expand_doit(fd, mainvar, snd->ipo);
}


static void expand_main(FileData *fd, Main *mainvar)
{
	ListBase *lbarray[30];
	ID *id;
	int a, doit= 1;
	
	if(fd==0) return;
	
	while(doit) {
		doit= 0;
		
		a= set_listbasepointers(mainvar, lbarray);
		while(a--) {
			id= lbarray[a]->first;

			while(id) {
				if(id->flag & LIB_TEST) {
					
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
					case ID_SO:
						expand_sound(fd, mainvar, (bSound *)id);
						break;
					case ID_AR:
						expand_armature(fd, mainvar, (bArmature *)id);
						break;
					case ID_AC:
						expand_action(fd, mainvar, (bAction *)id);
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

#if 0
static void give_base_to_objects(Scene *sce, ListBase *lb)
{
	Object *ob;
	Base *base;
	
	/* give all objects which are LIB_EXTERN and LIB_NEEDLINK a base */
	ob= lb->first;
	while(ob) {

		if(ob->id.us==0) {

			if(ob->id.flag & LIB_NEEDLINK) {
			
				ob->id.flag -= LIB_NEEDLINK;
				
				if( ob->id.flag & LIB_INDIRECT ) {
					
					base= MEM_callocN( sizeof(Base), "add_ext_base");
					BLI_addtail(&(sce->base), base);
					base->lay= ob->lay;
					base->object= ob;
					ob->id.us= 1;
					
					ob->id.flag -= LIB_INDIRECT;
					ob->id.flag |= LIB_EXTERN;

				}
			}
		}
		ob= ob->id.next;
	}
}
#endif

static void append_named_part(SpaceFile *sfile, Main *mainvar, Scene *scene, char *name, int idcode)
{
	Object *ob;
	Base *base;
	BHead *bhead;
	ID *id;
	FileData *fd= (FileData*) sfile->libfiledata;
	int afbreek=0;

	bhead = blo_firstbhead(fd);
	while(bhead && afbreek==0) {
		
		if(bhead->code==ENDB) afbreek= 1;
		else if(bhead->code==idcode) {
			// BHEAD+DATA dependancy
			id= (ID *)(bhead+1);
			if(strcmp(id->name+2, name)==0) {
				
				id= is_yet_read(mainvar, bhead);
				if(id==0) {
					read_libblock(fd, mainvar, bhead, LIB_TESTEXT, NULL);
				}
				else {
					printf("append: already linked\n");
					oldnewmap_insert(fd->libmap, bhead->old, id, 1);		
					if(id->flag & LIB_INDIRECT) {
						id->flag -= LIB_INDIRECT;
						id->flag |= LIB_EXTERN;
					}
				}
				
				if(idcode==ID_OB) {	/* loose object: give a base */
					base= MEM_callocN( sizeof(Base), "app_nam_part");
					BLI_addtail(&scene->base, base);
					
					if(id==0) ob= mainvar->object.last;
					else ob= (Object *)id;
					
					base->lay= ob->lay;
					base->object= ob;
					ob->id.us++;
				}
				afbreek= 1;
			}
		}

		bhead = blo_nextbhead(fd, bhead);
	}
}

static void append_id_part(FileData *fd, Main *mainvar, ID *id, ID **id_r)
{
	BHead *bhead;
	
	for (bhead= blo_firstbhead(fd); bhead; bhead= blo_nextbhead(fd, bhead)) {
		if (bhead->code == GS(id->name)) {
			ID *idread= (ID *)(bhead+1); /*  BHEAD+DATA dependancy */

			if (BLI_streq(id->name, idread->name)) {
				id->flag -= LIB_READ;
				id->flag |= LIB_TEST;
				
				read_libblock(fd, mainvar, bhead, id->flag, id_r);
				
				break;
			}
		} else if (bhead->code==ENDB)
			break;
	}
}


	/* append to G.scene */
void BLO_library_append(SpaceFile *sfile, char *dir, int idcode)
{
	FileData *fd= (FileData*) sfile->libfiledata;
	ListBase mainlist;
	Main *mainl;
	int a, totsel=0;
	
	/* are there files selected? */
	for(a=0; a<sfile->totfile; a++) {
		if(sfile->filelist[a].flags & ACTIVE) {
			totsel++;
		}
	}
	
	if(totsel==0) {
		/* is the indicated file in the filelist? */
		if(sfile->file[0]) {
			for(a=0; a<sfile->totfile; a++) {
				if( strcmp(sfile->filelist[a].relname, sfile->file)==0) break;
			}
			if(a==sfile->totfile) {
				error("Wrong indicated name");
				return;
			}
		}
		else {
			error("Nothing indicated");
			return;
		}
	}
	/* now we have or selected, or an indicated file */
	
	mainlist.first= mainlist.last= G.main;
	G.main->next= NULL;
	
	/* make mains */
	blo_split_main(&mainlist);
	
	/* which one do we need? */
	mainl = blo_find_main(&mainlist, dir);

	if(totsel==0) {
		append_named_part(sfile, mainl, G.scene, sfile->file, idcode);
	}
	else {
		for(a=0; a<sfile->totfile; a++) {
			if(sfile->filelist[a].flags & ACTIVE) {
				append_named_part(sfile, mainl, G.scene, sfile->filelist[a].relname, idcode);
			}
		}
	}
	
	/* make main consistant */
	expand_main(fd, mainl);
	
	/* do this when expand found other libs */
	read_libraries(fd, &mainlist);

	blo_join_main(&mainlist);
	G.main= mainlist.first;

	lib_link_all(fd, G.main);

	/* give a base to loose objects */
	/* give_base_to_objects(G.scene, &(G.main->object)); */
	/* has been removed... erm, why? (ton) */

	/* patch to prevent switch_endian happens twice */
	if(fd->flags & FD_FLAGS_SWITCH_ENDIAN) {
		blo_freefiledata((FileData*) sfile->libfiledata);
		sfile->libfiledata= 0;
	}
}

/* ************* READ LIBRARY ************** */

static int mainvar_count_libread_blocks(Main *mainvar)
{
	ListBase *lbarray[30];
	int a, tot= 0;

	a= set_listbasepointers(mainvar, lbarray);
	while(a--) {
		ID *id= lbarray[a]->first;
		
		for (id= lbarray[a]->first; id; id= id->next)
			if (id->flag & LIB_READ)
				tot++;
	}
	return tot;
}

static void read_libraries(FileData *basefd, ListBase *mainlist)
{
	Main *main= mainlist->first;
	Main *mainptr;
	ListBase *lbarray[30];
	int a, doit= 1;
	
	while(doit) {
		doit= 0;
		
		/* test 1: read libdata */
		mainptr= main->next;
		
		while(mainptr) {
			int tot= mainvar_count_libread_blocks(mainptr);
			
			if(tot) {
				FileData *fd= mainptr->curlib->filedata;
				
				if(fd==0) {
					printf("read lib %s\n", mainptr->curlib->name);
					fd= blo_openblenderfile(mainptr->curlib->name);
					if (fd) {
						if (fd->libmap)
							oldnewmap_free(fd->libmap);
						
						fd->libmap= basefd->libmap;
						fd->flags|= FD_FLAGS_NOT_MY_LIBMAP;
					}
						
					mainptr->curlib->filedata= fd;
					
					if (!fd)
						printf("ERROR: can't find lib %s \n", mainptr->curlib->name);
				}
				if(fd) {
					doit= 1;
					a= set_listbasepointers(mainptr, lbarray);
					while(a--) {
						ID *id= lbarray[a]->first;
						
						while(id) {
							ID *idn= id->next;
							if(id->flag & LIB_READ) {
								ID *realid= NULL;
								BLI_remlink(lbarray[a], id);
								
								append_id_part(fd, mainptr, id, &realid);
								if (!realid)
									printf("LIB ERROR: can't find %s\n", id->name);
								change_libadr(fd, id, realid);
								
								MEM_freeN(id);
							}
							id= idn;
						}
					}

					expand_main(fd, mainptr);
				}
			}
			
			mainptr= mainptr->next;
		}
	}
	mainptr= main->next;
	while(mainptr) {
		/* test if there are unread libblocks */
		a= set_listbasepointers(mainptr, lbarray);
		while(a--) {
			ID *id= lbarray[a]->first;
			while(id) {
				ID *idn= id->next;
				if(id->flag & LIB_READ) {
					BLI_remlink(lbarray[a], id);
										
					printf("LIB ERROR: can't find %s\n", id->name);
					change_libadr(basefd, id, 0);
					
					MEM_freeN(id);
				}
				id= idn;
			}
		}
		
		/* some mains still have to be read, then
		 * versionfile is still zero! */
		if(mainptr->versionfile) do_versions(mainptr);

		if(mainptr->curlib->filedata) blo_freefiledata(mainptr->curlib->filedata);
		mainptr->curlib->filedata= 0;

		mainptr= mainptr->next;
	}
}

// ****************** STREAM GLUE READER **********************

static int fd_read_from_streambuffer(FileData *filedata, void *buffer, int size)
{
	int readsize = EOF;
	int type;
	
	if (size <= (filedata->inbuffer - filedata->seek)) {
		memmove(buffer, filedata->buffer + filedata->seek, size);
		filedata->seek += size;
		readsize = size;
	} else {
		// special ENDB handling
		if (((filedata->inbuffer - filedata->seek) == 8) && (size > 8)) {
			memmove(&type, filedata->buffer + filedata->seek, sizeof(type));

			if (type == ENDB) {
				memmove(buffer, filedata->buffer + filedata->seek, 8);
				readsize = 8;
			}
		}
	}
	
	return (readsize);
}

void *blo_readstreamfile_begin(void *endControl)
{
	void **params = endControl;
	
	FileData *fd = filedata_new();
	fd->read = fd_read_from_streambuffer;
	fd->buffersize = 100000;
	fd->buffer = MEM_mallocN(fd->buffersize, "Buffer readstreamfile");
	fd->bfd_r = params[0];
	fd->error_r = params[1];
	
	return fd;
}

int blo_readstreamfile_process(void *filedataVoidPtr, unsigned char *data, unsigned int dataIn)
{
	struct FileData *filedata = filedataVoidPtr;
	int err = 0;
	int size, datasize;
	char *newbuffer;
	BHead8 bhead8;
	BHead4 bhead4;
	
	// copy everything in the buffer
	
	if (((int) dataIn + filedata->inbuffer) > filedata->buffersize) {
		// do we need a bigger buffer ?
		if (((int) dataIn + filedata->inbuffer - filedata->seek) > filedata->buffersize) {
			// copy data and ajust settings
			filedata->buffersize = dataIn + filedata->inbuffer - filedata->seek;
			newbuffer = MEM_mallocN(filedata->buffersize, "readstreamfile newbuffer");
			memmove(newbuffer, filedata->buffer + filedata->seek, filedata->inbuffer - filedata->seek);
			MEM_freeN(filedata->buffer);
			filedata->buffer = newbuffer;
		} else {
			// we just move the existing data to the start
			// of the block
			memmove(filedata->buffer, filedata->buffer + filedata->seek, filedata->inbuffer - filedata->seek);
		}
		// adjust seek and inbuffer accordingly
		filedata->inbuffer -= filedata->seek;
		filedata->seek = 0;
	}
	
	memmove(filedata->buffer + filedata->inbuffer, data, dataIn);
	filedata->inbuffer += dataIn;
	
	// OK, so now we have everything in one buffer. What are we
	// going to do with it...
	
	while (1) {
		datasize = filedata->inbuffer - filedata->seek;
	
		if (filedata->headerdone) {
			if (filedata->flags & FD_FLAGS_FILE_POINTSIZE_IS_4) {
				if (datasize > sizeof(bhead4)) {
					datasize -= sizeof(bhead4);
					memmove(&bhead4, filedata->buffer + filedata->seek, sizeof(bhead4));
					size = bhead4.len;
				} else {
					break;
				}
			} else {
				if (datasize > sizeof(bhead8)) {
					datasize -= sizeof(bhead8);
					memmove(&bhead8, filedata->buffer + filedata->seek, sizeof(bhead8));
					size = bhead8.len;
				} else {
					break;
				}
			}
	
			if (filedata->flags & FD_FLAGS_SWITCH_ENDIAN) {
				SWITCH_INT(size);
			}
	
			// do we have enough left in the buffer to read
			// in a full bhead + data ?
			if (size <= datasize) {
				get_bhead(filedata);
			} else {
				break;
			}
	
		} else {
			if (datasize < SIZEOFBLENDERHEADER) {
				// still need more data to continue..
				break;
			} else {
				decode_blender_header(filedata);
				filedata->headerdone = 1;
				if (! (filedata->flags & FD_FLAGS_FILE_OK)) {
					// not a blender file ... ?
					err = 1;
					break;
				}
			}
		}
	}
	
	return err;
}

int blo_readstreamfile_end(void *filedataVoidPtr)
{
	struct FileData *fd = filedataVoidPtr;
	int err = 1;

	*fd->bfd_r= NULL;
	if (!(fd->flags & FD_FLAGS_FILE_OK)) {
		*fd->error_r= BRE_NOT_A_BLEND;
	} else if ((fd->inbuffer - fd->seek) != 8) {
		*fd->error_r= BRE_INCOMPLETE;
	} else if (!get_bhead(fd) || !read_file_dna(fd)) {
			// ENDB block !
		*fd->error_r= BRE_INCOMPLETE;
	} else {
		*fd->bfd_r= blo_read_file_internal(fd, fd->error_r);
		err = 0;
	}
	
	blo_freefiledata(fd);
	
	return err;
}
