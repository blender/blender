/**
 * $Id: 
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
 * The Original Code is Copyright (C) 2004 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * .blend file reading entry point
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_userdef_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"

#include "BLO_undofile.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"



/* **************** support for memory-write, for undo buffers *************** */

/* not memfile itself */
void BLO_free_memfile(MemFile *memfile)
{
	MemFileChunk *chunk;
	
	while(chunk = (memfile->chunks.first) ) {
		if(chunk->ident==0) MEM_freeN(chunk->buf);
		BLI_remlink(&memfile->chunks, chunk);
		MEM_freeN(chunk);
	}
	memfile->size= 0;
}

/* to keep list of memfiles consistant, 'first' is always first in list */
/* result is that 'first' is being freed */
void BLO_merge_memfile(MemFile *first, MemFile *second)
{
	MemFileChunk *fc, *sc;
	
	fc= first->chunks.first;
	sc= second->chunks.first;
	while (fc || sc) {
		if(fc && sc) {
			if(sc->ident) {
				sc->ident= 0;
				fc->ident= 1;
			}
		}
		if(fc) fc= fc->next;
		if(sc) sc= sc->next;
	}
	
	BLO_free_memfile(first);
}

static int my_memcmp(int *mem1, int *mem2, int len)
{
	register int a= len, *mema= mem1, *memb= mem2;
	
	while(a--) {
		if( *mema != *memb) return 1;
		mema++;
		memb++;
	}
	return 0;
}

void add_memfilechunk(MemFile *compare, MemFile *current, char *buf, unsigned int size)
{
	static MemFileChunk *compchunk=NULL;
	MemFileChunk *curchunk;
	
	/* this function inits when compare != NULL or when current==NULL */
	if(compare) {
		compchunk= compare->chunks.first;
		return;
	}
	if(current==NULL) {
		compchunk= NULL;
		return;
	}
	
	curchunk= MEM_mallocN(sizeof(MemFileChunk), "MemFileChunk");
	curchunk->size= size;
	curchunk->buf= NULL;
	curchunk->ident= 0;
	BLI_addtail(&current->chunks, curchunk);
	
	/* we compare compchunk with buf */
	if(compchunk) {
		if(compchunk->size == curchunk->size) {
			if( my_memcmp((int *)compchunk->buf, (int *)buf, size/4)==0) {
				curchunk->buf= compchunk->buf;
				curchunk->ident= 1;
			}
		}
		compchunk= compchunk->next;
	}
	
	/* not equal... */
	if(curchunk->buf==NULL) {
		curchunk->buf= MEM_mallocN(size, "Chunk buffer");
		memcpy(curchunk->buf, buf, size);
		current->size += size;
	}
}

/* ***************** GLOBAL UNDO *************** */

#define UNDO_DISK	0

#define MAXUNDONAME	64
typedef struct UndoElem {
	struct UndoElem *next, *prev;
	char str[FILE_MAXDIR+FILE_MAXFILE];
	char name[MAXUNDONAME];
	MemFile memfile;
} UndoElem;

#define MAXUNDO	 32
static ListBase undobase={NULL, NULL};
static UndoElem *curundo= NULL;


static int read_undosave(UndoElem *uel)
{
	char scestr[FILE_MAXDIR+FILE_MAXFILE];
	int success=0, fileflags;
	
	strcpy(scestr, G.sce);	/* temporal store */
	fileflags= G.fileflags;
	G.fileflags |= G_FILE_NO_UI;

	if(UNDO_DISK) 
		success= BKE_read_file(uel->str, NULL);
	else
		success= BKE_read_file_from_memfile(&uel->memfile);
	
	/* restore */
	strcpy(G.sce, scestr);
	G.fileflags= fileflags;

	return success;
}

/* name can be a dynamic string */
void BIF_write_undo(char *name)
{
	int nr, success;
	UndoElem *uel;
	
	if( (U.uiflag & USER_GLOBALUNDO)==0) return;

	/* remove all undos after (also when curundo==NULL) */
	while(undobase.last != curundo) {
		uel= undobase.last;
		BLI_remlink(&undobase, uel);
		BLO_free_memfile(&uel->memfile);
		MEM_freeN(uel);
	}
	
	/* make new */
	curundo= uel= MEM_callocN(sizeof(UndoElem), "undo file");
	strncpy(uel->name, name, MAXUNDONAME-1);
	BLI_addtail(&undobase, uel);
	
	/* and limit amount to the maximum */
	nr= 0;
	uel= undobase.last;
	while(uel) {
		nr++;
		if(nr==MAXUNDO) break;
		uel= uel->prev;
	}
	if(uel) {
		while(undobase.first!=uel) {
			UndoElem *first= undobase.first;
			BLI_remlink(&undobase, first);
			/* the merge is because of compression */
			BLO_merge_memfile(&first->memfile, &first->next->memfile);
			MEM_freeN(first);
		}
	}


	/* disk save version */
	if(UNDO_DISK) {
		static int counter= 0;
		char *err, tstr[FILE_MAXDIR+FILE_MAXFILE];
		char numstr[32];
		
		/* calculate current filename */
		counter++;
		counter= counter % MAXUNDO;	
	
		sprintf(numstr, "%d.blend", counter);
		BLI_make_file_string("/", tstr, U.tempdir, numstr);
	
		success= BLO_write_file(tstr, G.fileflags, &err);
		
		strcpy(curundo->str, tstr);
	}
	else {
		MemFile *prevfile=NULL;
		char *err;
		
		if(curundo->prev) prevfile= &(curundo->prev->memfile);
		
		success= BLO_write_file_mem(prevfile, &curundo->memfile, G.fileflags, &err);
		
	}
}

/* 1= an undo, -1 is a redo. we have to make sure 'curundo' remains at current situation */
void BIF_undo_step(int step)
{
	
	if(step==1) {
		/* curundo should never be NULL, after restart or load file it should call undo_save */
		if(curundo==NULL || curundo->prev==NULL) error("No undo available");
		else {
			printf("undo %s\n", curundo->name);
			curundo= curundo->prev;
			read_undosave(curundo);
		}
	}
	else {
		
		/* curundo has to remain current situation! */
		
		if(curundo==NULL || curundo->next==NULL) error("No redo available");
		else {
			read_undosave(curundo->next);
			curundo= curundo->next;
			printf("redo %s\n", curundo->name);
		}
	}
}

void BIF_reset_undo(void)
{
	UndoElem *uel;
	
	uel= undobase.first;
	while(uel) {
		BLO_free_memfile(&uel->memfile);
		uel= uel->next;
	}
	
	BLI_freelistN(&undobase);
	curundo= NULL;
}



