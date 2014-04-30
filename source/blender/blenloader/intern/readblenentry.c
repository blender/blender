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
 * .blend file reading entry point
 */

/** \file blender/blenloader/intern/readblenentry.c
 *  \ingroup blenloader
 */


#include <stddef.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_genfile.h"
#include "DNA_sdna_types.h"


#include "BKE_main.h"
#include "BKE_library.h" // for BKE_main_free
#include "BKE_idcode.h"

#include "BLO_readfile.h"
#include "BLO_undofile.h"
#include "BLO_blend_defs.h"

#include "readfile.h"

#include "BLI_sys_types.h" // needed for intptr_t

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

/* local prototypes --------------------- */
void BLO_blendhandle_print_sizes(BlendHandle *, void *); 

/* Access routines used by filesel. */
	 
BlendHandle *BLO_blendhandle_from_file(const char *filepath, ReportList *reports)
{
	BlendHandle *bh;

	bh = (BlendHandle *)blo_openblenderfile(filepath, reports);

	return bh;
}

BlendHandle *BLO_blendhandle_from_memory(const void *mem, int memsize)
{
	BlendHandle *bh;

	bh = (BlendHandle *)blo_openblendermemory(mem, memsize, NULL);

	return bh;
}

void BLO_blendhandle_print_sizes(BlendHandle *bh, void *fp) 
{
	FileData *fd = (FileData *) bh;
	BHead *bhead;

	fprintf(fp, "[\n");
	for (bhead = blo_firstbhead(fd); bhead; bhead = blo_nextbhead(fd, bhead)) {
		if (bhead->code == ENDB)
			break;
		else {
			const short *sp = fd->filesdna->structs[bhead->SDNAnr];
			const char *name = fd->filesdna->types[sp[0]];
			char buf[4];
			
			buf[0] = (bhead->code >> 24) & 0xFF;
			buf[1] = (bhead->code >> 16) & 0xFF;
			buf[2] = (bhead->code >> 8) & 0xFF;
			buf[3] = (bhead->code >> 0) & 0xFF;

			buf[0] = buf[0] ? buf[0] : ' ';
			buf[1] = buf[1] ? buf[1] : ' ';
			buf[2] = buf[2] ? buf[2] : ' ';
			buf[3] = buf[3] ? buf[3] : ' ';
			
			fprintf(fp, "['%.4s', '%s', %d, %ld ],\n", buf, name, bhead->nr, (long int)(bhead->len + sizeof(BHead)));
		}
	}
	fprintf(fp, "]\n");
}

LinkNode *BLO_blendhandle_get_datablock_names(BlendHandle *bh, int ofblocktype, int *tot_names)
{
	FileData *fd = (FileData *) bh;
	LinkNode *names = NULL;
	BHead *bhead;
	int tot = 0;

	for (bhead = blo_firstbhead(fd); bhead; bhead = blo_nextbhead(fd, bhead)) {
		if (bhead->code == ofblocktype) {
			const char *idname = bhead_id_name(fd, bhead);

			BLI_linklist_prepend(&names, strdup(idname + 2));
			tot++;
		}
		else if (bhead->code == ENDB)
			break;
	}

	*tot_names = tot;
	return names;
}

LinkNode *BLO_blendhandle_get_previews(BlendHandle *bh, int ofblocktype, int *tot_prev)
{
	FileData *fd = (FileData *) bh;
	LinkNode *previews = NULL;
	BHead *bhead;
	int looking = 0;
	PreviewImage *prv = NULL;
	PreviewImage *new_prv = NULL;
	int tot = 0;

	for (bhead = blo_firstbhead(fd); bhead; bhead = blo_nextbhead(fd, bhead)) {
		if (bhead->code == ofblocktype) {
			const char *idname = bhead_id_name(fd, bhead);
			switch (GS(idname)) {
				case ID_MA: /* fall through */
				case ID_TE: /* fall through */
				case ID_IM: /* fall through */
				case ID_WO: /* fall through */
				case ID_LA: /* fall through */
					new_prv = MEM_callocN(sizeof(PreviewImage), "newpreview");
					BLI_linklist_prepend(&previews, new_prv);
					tot++;
					looking = 1;
					break;
				default:
					break;
			}
		}
		else if (bhead->code == DATA) {
			if (looking) {
				if (bhead->SDNAnr == DNA_struct_find_nr(fd->filesdna, "PreviewImage") ) {
					prv = BLO_library_read_struct(fd, bhead, "PreviewImage");
					if (prv) {
						memcpy(new_prv, prv, sizeof(PreviewImage));
						if (prv->rect[0]) {
							unsigned int *rect = NULL;
							new_prv->rect[0] = MEM_callocN(new_prv->w[0] * new_prv->h[0] * sizeof(unsigned int), "prvrect");
							bhead = blo_nextbhead(fd, bhead);
							rect = (unsigned int *)(bhead + 1);
							memcpy(new_prv->rect[0], rect, bhead->len);
						}
						else {
							new_prv->rect[0] = NULL;
						}
						
						if (prv->rect[1]) {
							unsigned int *rect = NULL;
							new_prv->rect[1] = MEM_callocN(new_prv->w[1] * new_prv->h[1] * sizeof(unsigned int), "prvrect");
							bhead = blo_nextbhead(fd, bhead);
							rect = (unsigned int *)(bhead + 1);
							memcpy(new_prv->rect[1], rect, bhead->len);
						}
						else {
							new_prv->rect[1] = NULL;
						}
						MEM_freeN(prv);
					}
				}
			}
		}
		else if (bhead->code == ENDB) {
			break;
		}
		else {
			looking = 0;
			new_prv = NULL;
			prv = NULL;
		}
		
	}

	*tot_prev = tot;
	return previews;
}

LinkNode *BLO_blendhandle_get_linkable_groups(BlendHandle *bh) 
{
	FileData *fd = (FileData *) bh;
	GSet *gathered = BLI_gset_ptr_new("linkable_groups gh");
	LinkNode *names = NULL;
	BHead *bhead;
	
	for (bhead = blo_firstbhead(fd); bhead; bhead = blo_nextbhead(fd, bhead)) {
		if (bhead->code == ENDB) {
			break;
		}
		else if (BKE_idcode_is_valid(bhead->code)) {
			if (BKE_idcode_is_linkable(bhead->code)) {
				const char *str = BKE_idcode_to_name(bhead->code);
				
				if (!BLI_gset_haskey(gathered, (void *)str)) {
					BLI_linklist_prepend(&names, strdup(str));
					BLI_gset_insert(gathered, (void *)str);
				}
			}
		}
	}
	
	BLI_gset_free(gathered, NULL);
	
	return names;
}		

void BLO_blendhandle_close(BlendHandle *bh)
{
	FileData *fd = (FileData *) bh;
	
	blo_freefiledata(fd);
}

/**********/

BlendFileData *BLO_read_from_file(const char *filepath, ReportList *reports)
{
	BlendFileData *bfd = NULL;
	FileData *fd;
		
	fd = blo_openblenderfile(filepath, reports);
	if (fd) {
		fd->reports = reports;
		bfd = blo_read_file_internal(fd, filepath);
		blo_freefiledata(fd);
	}

	return bfd;
}

BlendFileData *BLO_read_from_memory(const void *mem, int memsize, ReportList *reports)
{
	BlendFileData *bfd = NULL;
	FileData *fd;
		
	fd = blo_openblendermemory(mem, memsize,  reports);
	if (fd) {
		fd->reports = reports;
		bfd = blo_read_file_internal(fd, "");
		blo_freefiledata(fd);
	}

	return bfd;
}

BlendFileData *BLO_read_from_memfile(Main *oldmain, const char *filename, MemFile *memfile, ReportList *reports)
{
	BlendFileData *bfd = NULL;
	FileData *fd;
	ListBase mainlist;
	
	fd = blo_openblendermemfile(memfile, reports);
	if (fd) {
		fd->reports = reports;
		BLI_strncpy(fd->relabase, filename, sizeof(fd->relabase));
		
		/* clear ob->proxy_from pointers in old main */
		blo_clear_proxy_pointers_from_lib(oldmain);

		/* separate libraries from old main */
		blo_split_main(&mainlist, oldmain);
		/* add the library pointers in oldmap lookup */
		blo_add_library_pointer_map(&mainlist, fd);
		
		/* makes lookup of existing images in old main */
		blo_make_image_pointer_map(fd, oldmain);
		
		/* makes lookup of existing video clips in old main */
		blo_make_movieclip_pointer_map(fd, oldmain);
		
		/* removed packed data from this trick - it's internal data that needs saves */
		
		bfd = blo_read_file_internal(fd, filename);
		
		/* ensures relinked images are not freed */
		blo_end_image_pointer_map(fd, oldmain);
		
		/* ensures relinked movie clips are not freed */
		blo_end_movieclip_pointer_map(fd, oldmain);
				
		/* move libraries from old main to new main */
		if (bfd && mainlist.first != mainlist.last) {
			
			/* Library structs themselves */
			bfd->main->library = oldmain->library;
			BLI_listbase_clear(&oldmain->library);
			
			/* add the Library mainlist to the new main */
			BLI_remlink(&mainlist, oldmain);
			BLI_addhead(&mainlist, bfd->main);
		}
		blo_join_main(&mainlist);
		
		blo_freefiledata(fd);
	}

	return bfd;
}

void BLO_blendfiledata_free(BlendFileData *bfd)
{
	if (bfd->main) {
		BKE_main_free(bfd->main);
	}
	
	if (bfd->user) {
		MEM_freeN(bfd->user);
	}

	MEM_freeN(bfd);
}

