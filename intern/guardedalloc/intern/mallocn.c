/**
 * $Id$
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
 */

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * Guarded memory allocation, and boundary-write detection.
 */

#include <stdlib.h>
#include <string.h>	/* memcpy */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

/* --------------------------------------------------------------------- */
/* Data definition                                                       */
/* --------------------------------------------------------------------- */
/* all memory chunks are put in linked lists */
typedef struct localLink
{
	struct localLink *next,*prev;
} localLink;

typedef struct localListBase 
{
	void *first, *last;
} localListBase;

typedef struct MemHead {
	int tag1;
	int len;
	struct MemHead *next,*prev;
	char * name;
	char * nextname;
	/*  int level; */ /* historical, can be removed, but check alignment issues - zr */
	int tag2;
} MemHead;

typedef struct MemTail {
	int tag3, pad;
} MemTail;

/* --------------------------------------------------------------------- */
/* local functions                                                       */
/* --------------------------------------------------------------------- */

static void addtail(localListBase *listbase, void *vlink);
static void remlink(localListBase *listbase, void *vlink);
static void rem_memblock(MemHead *memh);
static void MemorY_ErroR(char *block, char *error);
static char *check_memlist(MemHead *memh);

/* --------------------------------------------------------------------- */
/* locally used defines                                                  */
/* --------------------------------------------------------------------- */

#if defined( __sgi) || defined (__sun) || defined (__sun__) || defined (__sparc) || defined (__sparc__) || defined (__PPC__) || defined (__APPLE__)
#define MAKE_ID(a,b,c,d) ( (int)(a)<<24 | (int)(b)<<16 | (c)<<8 | (d) )
#else
#define MAKE_ID(a,b,c,d) ( (int)(d)<<24 | (int)(c)<<16 | (b)<<8 | (a) )
#endif

#define MEMTAG1 MAKE_ID('M', 'E', 'M', 'O')
#define MEMTAG2 MAKE_ID('R', 'Y', 'B', 'L')
#define MEMTAG3 MAKE_ID('O', 'C', 'K', '!')
#define MEMFREE MAKE_ID('F', 'R', 'E', 'E')

#define MEMNEXT(x) ((MemHead *)(((char *) x) - ((char *) & (((MemHead *)0)->next))))
	
/* --------------------------------------------------------------------- */
/* vars                                                                  */
/* --------------------------------------------------------------------- */
	
int totblock= 0;
int mem_in_use= 0;

static struct localListBase _membase;
static struct localListBase *membase = &_membase;
static FILE* err_stream = NULL;

#ifdef malloc
#undef malloc
#endif

#ifdef calloc
#undef calloc
#endif

#ifdef free
#undef free
#endif


/* --------------------------------------------------------------------- */
/* implementation                                                        */
/* --------------------------------------------------------------------- */

int MEM_check_memory_integrity()
{
	char* err_val = NULL;
	MemHead* listend;
	/* check_memlist starts from the front, and runs until it finds
	 * the requested chunk. For this test, that's the last one. */
	listend = membase->last;
	
	err_val = check_memlist(listend);

	return (int)err_val;
}


void MEM_set_error_stream(FILE* i)
{
	err_stream = i;
}


int MEM_allocN_len(void *vmemh)
{
	if (vmemh) {
		MemHead *memh= vmemh;
	
		memh--;
		return memh->len;
	} else
		return 0;
}

void *MEM_dupallocN(void *vmemh)
{
	void *newp= NULL;
	
	if (vmemh) {
		MemHead *memh= vmemh;
		memh--;

		if (memh->len) {
			newp= MEM_mallocN(memh->len, "dupli_alloc");
			memcpy(newp, vmemh, memh->len);
		} else
			if (err_stream) fprintf(err_stream, "error: MEM_dupallocN with len==0 %s\n", memh->name);
	}

	return newp;
}

void *MEM_mallocN(unsigned int len, char *str)
{
	MemHead *memh;
	MemTail *memt;

	if(sizeof(long)==8)
		len = (len + 3 ) & ~3; 	/* eenheden van 4 */
	else 
		len = (len + 7 ) & ~7; 	/* eenheden van 8 */
	
	memh= (MemHead *)malloc(len+sizeof(MemHead)+sizeof(MemTail));

	if(memh!=0) {
		memh->tag1 = MEMTAG1;
		memh->name = str;
		memh->nextname = 0;
		memh->len = len;
/*  		memh->level = 0; */
		memh->tag2 = MEMTAG2;

		memt = (MemTail *)(((char *) memh) + sizeof(MemHead) + len);
		memt->tag3 = MEMTAG3;

		addtail(membase,&memh->next);
		if (memh->next) memh->nextname = MEMNEXT(memh->next)->name;

		totblock++;
		mem_in_use += len;
		return (++memh);
	}
	if (err_stream) fprintf(err_stream, "Malloc returns nill: len=%d in %s\n",len,str);
	return 0;
}

void *MEM_callocN(unsigned int len, char *str)
{
	MemHead *memh;
	MemTail *memt;

	if(sizeof(long)==8)
		len = (len + 3 ) & ~3; 	/* eenheden van 4 */
	else 
		len = (len + 7 ) & ~7; 	/* eenheden van 8 */

	memh= (MemHead *)calloc(len+sizeof(MemHead)+sizeof(MemTail),1);

	if(memh!=0) {
		memh->tag1 = MEMTAG1;
		memh->name = str;
		memh->nextname = 0;
		memh->len = len;
/*  		memh->level = 0; */
		memh->tag2 = MEMTAG2;

		memt = (MemTail *)(((char *) memh) + sizeof(MemHead) + len);
		memt->tag3 = MEMTAG3;

		addtail(membase,&memh->next);
		if (memh->next) memh->nextname = MEMNEXT(memh->next)->name;

		totblock++;
		mem_in_use += len;
		return (++memh);
	}
	if (err_stream) fprintf(err_stream, "Calloc returns nill: len=%d in %s\n",len,str);
	return 0;
}


void MEM_printmemlist()
{
	MemHead *membl;

	membl = membase->first;
	if (membl) membl = MEMNEXT(membl);
	while(membl) {
		if (err_stream) fprintf(err_stream, "%s len: %d %p\n",membl->name,membl->len, membl+1);
		if(membl->next)
			membl= MEMNEXT(membl->next);
		else break;
	}
}

short MEM_freeN(void *vmemh)		/* anders compileertie niet meer */
{
	short error = 0;
	MemTail *memt;
	MemHead *memh= vmemh;
	char *name;

	if (memh == 0){
		MemorY_ErroR("free","attempt to free NULL pointer");
		/* if (err_stream) fprintf(err_stream, "%d\n", (memh+4000)->tag1); */
		return(-1);
	}

	if(sizeof(long)==8) {
		if (((long) memh) & 0x7) {
			MemorY_ErroR("free","attempt to free illegal pointer");
			return(-1);
		}
	}
	else {
		if (((long) memh) & 0x3) {
			MemorY_ErroR("free","attempt to free illegal pointer");
			return(-1);
		}
	}
	
	memh--;
	if(memh->tag1 == MEMFREE && memh->tag2 == MEMFREE) {
		MemorY_ErroR(memh->name,"double free");
		return(-1);
	}

	if ((memh->tag1 == MEMTAG1) && (memh->tag2 == MEMTAG2) && ((memh->len & 0x3) == 0)) {
		memt = (MemTail *)(((char *) memh) + sizeof(MemHead) + memh->len);
		if (memt->tag3 == MEMTAG3){
			
			memh->tag1 = MEMFREE;
			memh->tag2 = MEMFREE;
			memt->tag3 = MEMFREE;
			/* na tags !!! */
			rem_memblock(memh);
			
			return(0);
		}
		error = 2;
		MemorY_ErroR(memh->name,"end corrupt");
		name = check_memlist(memh);
		if (name != 0){
			if (name != memh->name) MemorY_ErroR(name,"is also corrupt");
		}
	} else{
		error = -1;
		name = check_memlist(memh);
		if (name == 0) MemorY_ErroR("free","pointer not in memlist");
		else MemorY_ErroR(name,"error in header");
	}

	totblock--;
	/* hier moet een DUMP plaatsvinden */

	return(error);
}

/* --------------------------------------------------------------------- */
/* local functions                                                       */
/* --------------------------------------------------------------------- */

static void addtail(localListBase *listbase, void *vlink)
{
	struct localLink *link= vlink;

	if (link == 0) return;
	if (listbase == 0) return;

	link->next = 0;
	link->prev = listbase->last;

	if (listbase->last) ((struct localLink *)listbase->last)->next = link;
	if (listbase->first == 0) listbase->first = link;
	listbase->last = link;
}

static void remlink(localListBase *listbase, void *vlink)
{
	struct localLink *link= vlink;

	if (link == 0) return;
	if (listbase == 0) return;

	if (link->next) link->next->prev = link->prev;
	if (link->prev) link->prev->next = link->next;

	if (listbase->last == link) listbase->last = link->prev;
	if (listbase->first == link) listbase->first = link->next;
}

static void rem_memblock(MemHead *memh)
{
	remlink(membase,&memh->next);
	if (memh->prev){
		if (memh->next) MEMNEXT(memh->prev)->nextname = MEMNEXT(memh->next)->name;
		else MEMNEXT(memh->prev)->nextname = 0;
	}

	totblock--;
	mem_in_use -= memh->len;
	free(memh);
}

static void MemorY_ErroR(char *block, char *error)
{
	if (err_stream) fprintf(err_stream,"Memoryblock %s: %s\n",block,error);
}

static char *check_memlist(MemHead *memh)
{
	MemHead *forw,*back,*forwok,*backok;
	char *name;

	forw = membase->first;
	if (forw) forw = MEMNEXT(forw);
	forwok = 0;
	while(forw){
		if (forw->tag1 != MEMTAG1 || forw->tag2 != MEMTAG2) break;
		forwok = forw;
		if (forw->next) forw = MEMNEXT(forw->next);
		else forw = 0;
	}

	back = (MemHead *) membase->last;
	if (back) back = MEMNEXT(back);
	backok = 0;
	while(back){
		if (back->tag1 != MEMTAG1 || back->tag2 != MEMTAG2) break;
		backok = back;
		if (back->prev) back = MEMNEXT(back->prev);
		else back = 0;
	}

	if (forw != back) return ("MORE THAN 1 MEMORYBLOCK CORRUPT");

	if (forw == 0 && back == 0){
		/* geen foute headers gevonden dan maar op zoek naar memblock*/

		forw = membase->first;
		if (forw) forw = MEMNEXT(forw);
		forwok = 0;
		while(forw){
			if (forw == memh) break;
			if (forw->tag1 != MEMTAG1 || forw->tag2 != MEMTAG2) break;
			forwok = forw;
			if (forw->next) forw = MEMNEXT(forw->next);
			else forw = 0;
		}
		if (forw == 0) return (0);

		back = (MemHead *) membase->last;
		if (back) back = MEMNEXT(back);
		backok = 0;
		while(back){
			if (back == memh) break;
			if (back->tag1 != MEMTAG1 || back->tag2 != MEMTAG2) break;
			backok = back;
			if (back->prev) back = MEMNEXT(back->prev);
			else back = 0;
		}
	}

	if (forwok) name = forwok->nextname;
	else name = "No name found";

	if (forw == memh){
		/* voor alle zekerheid wordt dit block maar uit de lijst gehaald */
		if (forwok){
			if (backok){
				forwok->next = (MemHead *)&backok->next;
				backok->prev = (MemHead *)&forwok->next;
				forwok->nextname = backok->name;
			} else{
				forwok->next = 0;
  				membase->last = (struct localLink *) &forwok->next; 
/*  				membase->last = (struct Link *) &forwok->next; */
			}
		} else{
			if (backok){
				backok->prev = 0;
				membase->first = &backok->next;
			} else{
				membase->first = membase->last = 0;
			}
		}
	} else{
		MemorY_ErroR(name,"Additional error in header");
		return("Additional error in header");
	}

	return(name);
}

/* eof */
