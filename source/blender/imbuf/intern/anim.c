/**
 * anim.c
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
 */

#ifdef _WIN32
#define INC_OLE2
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <memory.h>
#include <commdlg.h>
#include <vfw.h>

#undef AVIIF_KEYFRAME // redefined in AVI_avi.h
#undef AVIIF_LIST // redefined in AVI_avi.h

#define FIXCC(fcc)  if (fcc == 0)       fcc = mmioFOURCC('N', 'o', 'n', 'e'); \
                    if (fcc == BI_RLE8) fcc = mmioFOURCC('R', 'l', 'e', '8');
#endif

#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#ifndef _WIN32
#include <dirent.h>
#else
#include <io.h>
#include "BLI_winstuff.h"
#endif

#include "BLI_blenlib.h" /* BLI_remlink BLI_filesize BLI_addtail
                            BLI_countlist BLI_stringdec */

#include "imbuf.h"
#include "imbuf_patch.h"

#include "AVI_avi.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"
#include "IMB_bitplanes.h"


/* actually hard coded endianness */
#define GET_BIG_LONG(x) (((uchar *) (x))[0] << 24 | ((uchar *) (x))[1] << 16 | ((uchar *) (x))[2] << 8 | ((uchar *) (x))[3])
#define GET_LITTLE_LONG(x) (((uchar *) (x))[3] << 24 | ((uchar *) (x))[2] << 16 | ((uchar *) (x))[1] << 8 | ((uchar *) (x))[0])
#define SWAP_L(x) (((x << 24) & 0xff000000) | ((x << 8) & 0xff0000) | ((x >> 8) & 0xff00) | ((x >> 24) & 0xff))
#define SWAP_S(x) (((x << 8) & 0xff00) | ((x >> 8) & 0xff))

/* more endianness... should move to a separate file... */
#if defined(__sgi) || defined (__sparc) || defined (__PPC__) || defined (__ppc__) || defined (__BIG_ENDIAN__)
#define GET_ID GET_BIG_LONG
#define LITTLE_LONG SWAP_LONG
#else
#define GET_ID GET_LITTLE_LONG
#define LITTLE_LONG ENDIAN_NOP
#endif

/****/

typedef struct Anhd{
	unsigned char type, mask;
	unsigned short w, h;
	unsigned short x, y;
	unsigned short abs16, abs, reala6, real;
	unsigned char interleave, pad0;
	unsigned short bits16, bits;
	unsigned char pad[16];
}Anhd;

typedef struct Anim5Delta {
	struct Anim5Delta * next, * prev;
	void * data;
	int type;
}Anim5Delta;

#define ANIM_NONE		(0)
#define ANIM_SEQUENCE	(1 << 0)
#define ANIM_DIR		(1 << 1)
#define ANIM_ANIM5		(1 << 2)
#define ANIM_TGA		(1 << 3)
#define ANIM_MOVIE		(1 << 4)
#define ANIM_MDEC		(1 << 5)
#define ANIM_AVI		(1 << 6)

#define ANIM5_MMAP		0
#define ANIM5_MALLOC	1
#define ANIM5_SNGBUF	2
#define ANIM5_XOR		4

#define MAXNUMSTREAMS   50

struct anim {
	int ib_flags;
	int curtype;
	int curposition;	/* index  0 = 1e,  1 = 2e, enz. */
	int duration;
	int x, y;
	
		/* voor op nummer */
	char name[256];
		/* voor sequence */
	char first[256];
	
		/* anim5 */
	struct ListBase	anim5base;
	void		* anim5mmap;
	int		anim5len;
	struct Anim5Delta *anim5curdlta;
	void		(*anim5decode)(struct ImBuf *, unsigned char *);
	int		anim5flags;
	
		/* movie */
	void *movie;
	void *track;
	void *params;
	int orientation; 
	size_t framesize;
	int interlacing;
	
		/* data */
	struct ImBuf * ibuf1, * ibuf2;
	
		/* avi */
	struct _AviMovie *avi;

#ifdef _WIN32
		/* windows avi */
	int avistreams;
	int firstvideo;
	int pfileopen;
	PAVIFILE	pfile;
	PAVISTREAM  pavi[MAXNUMSTREAMS];	// the current streams
	PGETFRAME	  pgf;
#endif
};

/****/

#ifdef __sgi

#include <dmedia/moviefile.h>

static void movie_printerror(char * str) {
	const char * errstr = mvGetErrorStr(mvGetErrno());

	if (str) {
		if (errstr) printf("%s: %s\n", str, errstr);
		else printf("%s: returned error\n", str);
	} else printf("%s\n", errstr);
}

static int startmovie(struct anim * anim) {
	if (anim == 0) return(-1);

	if ( mvOpenFile (anim->name, O_BINARY|O_RDONLY, &anim->movie ) != DM_SUCCESS ) {
		printf("Can't open movie: %s\n", anim->name);
		return(-1);
	}
	if ( mvFindTrackByMedium (anim->movie, DM_IMAGE, &anim->track) != DM_SUCCESS ) {
		printf("No image track in movie: %s\n", anim->name);
		mvClose(anim->movie);
		return(-1);
	}

	anim->duration = mvGetTrackLength (anim->track);
	anim->params = mvGetParams( anim->track );

	anim->x = dmParamsGetInt( anim->params, DM_IMAGE_WIDTH);
	anim->y = dmParamsGetInt( anim->params, DM_IMAGE_HEIGHT);
	anim->interlacing = dmParamsGetEnum (anim->params, DM_IMAGE_INTERLACING);
	anim->orientation = dmParamsGetEnum (anim->params, DM_IMAGE_ORIENTATION);
	anim->framesize = dmImageFrameSize(anim->params);

	anim->curposition = 0;

	/*printf("x:%d y:%d size:%d interl:%d dur:%d\n", anim->x, anim->y, anim->framesize, anim->interlacing, anim->duration);*/
	return (0);
}

static ImBuf * movie_fetchibuf(struct anim * anim, int position) {
	ImBuf * ibuf;
/*  	extern rectcpy(); */
	int size;
	unsigned int *rect1, *rect2;

	if (anim == 0) return (0);

	ibuf = IMB_allocImBuf(anim->x, anim->y, 24, IB_rect, 0);

	if ( mvReadFrames(anim->track, position, 1, ibuf->x * ibuf->y * 
	    sizeof(int), ibuf->rect ) != DM_SUCCESS ) {
		movie_printerror("mvReadFrames");
		IMB_freeImBuf(ibuf);
		return(0);
	}

	/*
    if (anim->interlacing == DM_IMAGE_INTERLACED_EVEN)
    {
    rect1 = ibuf->rect + (ibuf->x * ibuf->y) - 1;
    rect2 = rect1 - ibuf->x;
    
    for (size = ibuf->x * (ibuf->y - 1); size > 0; size--){
    *rect1-- = *rect2--;
    }
    }
    */

	if (anim->interlacing == DM_IMAGE_INTERLACED_EVEN)
	{
		rect1 = ibuf->rect;
		rect2 = rect1 + ibuf->x;

		for (size = ibuf->x * (ibuf->y - 1); size > 0; size--){
			*rect1++ = *rect2++;
		}
	}
	/*if (anim->orientation == DM_TOP_TO_BOTTOM) IMB_flipy(ibuf);*/


	return(ibuf);
}

static void free_anim_movie(struct anim * anim) {
	if (anim == NULL) return;

	if (anim->movie) {
		mvClose(anim->movie);
		anim->movie = NULL;
	}
	anim->duration = 0;
}

static int ismovie(char *name) {
	return (mvIsMovieFile(name) == DM_TRUE);
}

#else

static int ismovie(char *name) {
	return 0;
}

	/* never called, just keep the linker happy */
static int startmovie(struct anim * anim) { return 1; }
static ImBuf * movie_fetchibuf(struct anim * anim, int position) { return NULL; }
static void free_anim_movie(struct anim * anim) { ; }

#endif

static int an_stringdec(char *string, char* kop, char *staart,unsigned short *numlen) {
	unsigned short len,nums,nume;
	short i,found=FALSE;

	len=strlen(string);

	for(i=len-1;i>=0;i--){
		if (string[i]=='/') break;
		if (isdigit(string[i])) {
			if (found){
				nums=i;
			} else{
				nume=i;
				nums=i;
				found=TRUE;
			}
		} else{
			if (found) break;
		}
	}
	if (found){
		strcpy(staart,&string[nume+1]);
		strcpy(kop,string);
		kop[nums]=0;
		*numlen=nume-nums+1;
		return ((int)atoi(&(string[nums])));
	}
	staart[0]=0;
	strcpy(kop,string);
	*numlen=0;
	return (1);
}


static void an_stringenc(char *string, char *kop, char *staart, 
unsigned short numlen, int pic) {
	char numstr[10];
	unsigned short len,i;

	len=sprintf(numstr,"%d",pic);

	strcpy(string,kop);
	for(i=len;i<numlen;i++){
		strcat(string,"0");
	}
	strcat(string,numstr);
	strcat(string,staart);
}

/* om anim5's te kunnen lezen, moet een aantal gegevens bijgehouden worden:
 * Een lijst van pointers naar delta's, in geheugen of ge'mmap'ed
 * 
 * Mogelijk kan er ook een 'skiptab' aangelegd worden, om sneller
 * sprongen te kunnen maken.
 * 
 * Er moeten niet direct al plaatjes gegenereed worden, dit maakt de 
 * routines onbruikbaar om snel naar het goede plaatje te springen.
 * Een routine voert dus de delta's uit, een andere routine maakt van
 * voorgrondplaatje een ibuf;
 */


/*
   een aantal functie pointers moet geinporteerd worden, zodat er niet
   nog meer library's / objects meegelinkt hoeven te worden.

   Dezelfde structuur moet ook gebruikt kunnen worden voor het wegschrijven
   van animaties. Hoe geef je dit aan ?
   
   Hoe snel kunnen 10 .dlta's gedecomprimeerd worden
   (zonder omzetten naar rect).
   
   1 - zoek naar 1e plaatje, animatie die aan de eisen voldoet
   2 - probeer volgende plaatje te vinden:
		anim5 - decomprimeer
		sequence - teller ophogen
		directory - volgende entry
   3 - geen succes ? ga naar 1.
   
   
*/

/*
	1. Initialiseer routine met toegestane reeksen, en eerste naam
		- series op naam (.0001)
		- directories
		- anim5 animaties
		- TGA delta's
		- iff 24bits delta's (.delta)
	
	2. haal volgende (vorige ?) plaatje op.
	
	3. vrijgeven
*/

/* selectie volgorde is:
	1 - anim5()
	2 - name
	3 - dir
*/


static void free_anim_anim5(struct anim * anim) {
	ListBase * animbase;
	Anim5Delta * delta, * next;

	if (anim == NULL) return;

	animbase = &anim->anim5base;
	delta = animbase->first;

	while (delta) {
		next = delta->next;

		if (delta->type == ANIM5_MALLOC) free(delta->data);
		BLI_remlink(animbase, delta);
		free(delta);

		delta = next;
	}

	if (anim->anim5mmap && anim->anim5len) {
		MEM_freeN(anim->anim5mmap);
	}

	anim->anim5mmap = NULL;
	anim->anim5len = 0;
	anim->anim5curdlta = 0;
	anim->duration = 0;
}

static void free_anim_avi (struct anim *anim) {
	int i;

	if (anim == NULL) return;
	if (anim->avi == NULL) return;

	AVI_close (anim->avi);
	MEM_freeN (anim->avi);
	anim->avi = NULL;

#ifdef _WIN32

	if (anim->pgf) {
		AVIStreamGetFrameClose(anim->pgf);
		anim->pgf = NULL;
	}

	for (i = 0; i < anim->avistreams; i++){
		AVIStreamRelease(anim->pavi[i]);
	}
	anim->avistreams = 0;

	if (anim->pfileopen) {
		AVIFileRelease(anim->pfile);
		anim->pfileopen = 0;
		AVIFileExit();
	}
#endif

	anim->duration = 0;
}

static void free_anim_ibuf(struct anim * anim) {
	if (anim == NULL) return;

	if (anim->ibuf1) IMB_freeImBuf(anim->ibuf1);
	if (anim->ibuf2) IMB_freeImBuf(anim->ibuf2);

	anim->ibuf1 = anim->ibuf2 = NULL;
}


void IMB_free_anim(struct anim * anim) {
	if (anim == NULL) {
		printf("free anim, anim == NULL\n");
		return;
	}

	free_anim_ibuf(anim);
	free_anim_anim5(anim);
	free_anim_movie(anim);
	free_anim_avi(anim);

	free(anim);
}

void IMB_close_anim(struct anim * anim) {
	if (anim == 0) return;

	IMB_free_anim(anim);
}


struct anim * IMB_open_anim(char * name, int ib_flags) {
	struct anim * anim;

	anim = (struct anim*)MEM_callocN(sizeof(struct anim), "anim struct");
	if (anim != NULL) {
		strcpy(anim->name, name);
		anim->ib_flags = ib_flags;
	}
	return(anim);
}


static int isavi (char *name) {
    return AVI_is_avi (name);
}

static int imb_get_anim_type(char * name) {
	int type;
	struct stat st;

 	if (ib_stat(name,&st) == -1) return(0);
 	if (((st.st_mode) & S_IFMT) != S_IFREG) return(0);

	if (isavi(name)) return (ANIM_AVI);
	if (ismovie(name)) return (ANIM_MOVIE);
 
	type = IMB_ispic(name);
	if (type == ANIM) return (ANIM_ANIM5);
	if (type) return(ANIM_SEQUENCE);
	return(0);
}
 
int IMB_isanim(char * name) {
	int type= imb_get_anim_type(name);
	
	return (type && type!=ANIM_SEQUENCE);
}

static void planes_to_rect(struct ImBuf * ibuf, int flags) {
	if (ibuf == 0) return;

	/* dit komt regelrecht uit de amiga.c */

	if (flags & IB_rect && ibuf->rect == 0) {
		imb_addrectImBuf(ibuf);
		imb_bptolong(ibuf);
		IMB_flipy(ibuf);
		imb_freeplanesImBuf(ibuf);

		if (ibuf->cmap){
			if ((flags & IB_cmap) == 0) {
				IMB_applycmap(ibuf);
				IMB_convert_rgba_to_abgr(ibuf->x*ibuf->y, ibuf->rect);
			}
		} else if (ibuf->depth == 18){
			int i,col;
			unsigned int *rect;

			rect = ibuf->rect;
			for(i=ibuf->x * ibuf->y ; i>0 ; i--){
				col = *rect;
				col = ((col & 0x3f000) << 6) + ((col & 0xfc0) << 4)
				    + ((col & 0x3f) << 2);
				col += (col & 0xc0c0c0) >> 6;
				*rect++ = col;
			}
			ibuf->depth = 24;
		} else if (ibuf->depth <= 8) {
			/* geen colormap en geen 24 bits: zwartwit */
			uchar *rect;
			int size, shift;

			if (ibuf->depth < 8){
				rect = (uchar *) ibuf->rect;
				rect += 3;
				shift = 8 - ibuf->depth;
				for (size = ibuf->x * ibuf->y; size > 0; size --){
					rect[0] <<= shift;
					rect += 4;
				}
			}
			rect = (uchar *) ibuf->rect;
			for (size = ibuf->x * ibuf->y; size > 0; size --){
				rect[1] = rect[2] = rect[3];
				rect += 4;
			}
			ibuf->depth = 8;
		}
	}
}


static void anim5decode(struct ImBuf * ibuf, uchar * dlta) {
	uchar depth;
	int skip;
	int *ofspoint;
	uchar **planes;

	/*	samenstelling delta:
		lijst met ofsets voor delta's per bitplane (ofspoint)
		per kolom in delta (point)
			aantal handelingen (noops)
				code
					bijbehorende data
				...
			...
	*/

	dlta += 8;

	ofspoint = (int *)dlta;
	skip = ibuf->skipx * sizeof(int *);
	planes = (uchar **)ibuf->planes;

	for(depth=ibuf->depth ; depth>0 ; depth--){
		if (GET_BIG_LONG(ofspoint)){
			uchar *planestart;
			uchar *point;
			uchar x;

			point = dlta + GET_BIG_LONG(ofspoint);
			planestart = planes[0];
			x = (ibuf->x + 7) >> 3;

			do{
				uchar noop;

				if (noop = *(point++)){
					uchar *plane;
					uchar code;

					plane = planestart;
					do{
						if ((code = *(point++))==0){
							uchar val;

							code = *(point++);
							val = *(point++);
							do {
								plane[0] = val;
								plane += skip;
							} while(--code);

						} else if (code & 128){

							code &= 0x7f;
							do{
								plane[0] = *(point++);
								plane += skip;
							} while(--code);

						} else plane += code * skip;

					} while(--noop);
				}
				planestart++;
			} while(--x);
		}
		ofspoint++;
		planes++;
	}
}


static void anim5xordecode(struct ImBuf * ibuf, uchar * dlta) {
	uchar depth;
	int skip;
	int *ofspoint;
	uchar **planes;

	/*	samenstelling delta:
		lijst met ofsets voor delta's per bitplane (ofspoint)
		per kolom in delta (point)
			aantal handelingen (noops)
				code
					bijbehorende data
				...
			...
	*/

	dlta += 8;

	ofspoint = (int *)dlta;
	skip = ibuf->skipx * sizeof(int *);
	planes = (uchar **)ibuf->planes;

	for(depth=ibuf->depth ; depth>0 ; depth--){

		if (GET_BIG_LONG(ofspoint)){
			uchar *planestart;
			uchar *point;
			uchar x;

			point = dlta + GET_BIG_LONG(ofspoint);
			planestart = planes[0];
			x = (ibuf->x + 7) >> 3;

			do{
				uchar noop;

				if (noop = *(point++)){
					uchar *plane;
					uchar code;

					plane = planestart;
					do{
						if ((code = *(point++))==0){
							uchar val;

							code = *(point++);
							val = *(point++);
							do{
								plane[0] ^= val;
								plane += skip;
							}while(--code);

						} else if (code & 128){

							code &= 0x7f;
							do{
								plane[0] ^= *(point++);
								plane += skip;
							}while(--code);

						} else plane += code * skip;

					}while(--noop);
				}
				planestart++;
			}while(--x);
		}
		ofspoint++;
		planes++;
	}
}

static int nextanim5(struct anim * anim) {
	Anim5Delta * delta;
	struct ImBuf * ibuf;

	if (anim == 0) return(-1);

	delta = anim->anim5curdlta;

	if (delta == 0) return (-1);

	if (anim->anim5flags & ANIM5_SNGBUF) {
		ibuf = anim->ibuf1;
		if (ibuf == 0) return (0);
		anim->anim5decode(ibuf, delta->data);
	} else {
		ibuf = anim->ibuf2;
		if (ibuf == 0) return (0);
		anim->anim5decode(ibuf, delta->data);
		anim->ibuf2 = anim->ibuf1;
		anim->ibuf1 = ibuf;
	}

	anim->anim5curdlta = anim->anim5curdlta->next;
	anim->curposition++;

	return(0);
}

static int rewindanim5(struct anim * anim) {
	Anim5Delta * delta;
	struct ImBuf * ibuf;

	if (anim == 0) return (-1);

	free_anim_ibuf(anim);

	delta = anim->anim5base.first;
	if (delta == 0) return (-1);

	ibuf = IMB_loadiffmem(delta->data, IB_planes);
	if (ibuf == 0) return(-1);

	anim->ibuf1 = ibuf;
	if ((anim->anim5flags & ANIM5_SNGBUF) == 0) anim->ibuf2 = IMB_dupImBuf(ibuf);

	anim->anim5curdlta = delta->next;
	anim->curposition = 0;

	return(0);
}


static int startanim5(struct anim * anim) {
	int file, buf[20], totlen;
	unsigned int len;
	short * mem;
	ListBase * animbase;
	Anim5Delta * delta;
	Anhd anhd;

	/* Controles */

	if (anim == 0) return(-1);

	file = open(anim->name,O_BINARY|O_RDONLY);
	if (file < 0) return (-1);

	if (read(file, buf, 24) != 24) {
		close(file);
		return(-1);
	}

	if ((GET_ID(buf) != FORM) || (GET_ID(buf + 2) != ANIM)
	    || (GET_ID(buf + 3) != FORM) || (GET_ID(buf + 5) != ILBM)){
		printf("No anim5 file %s\n",anim->name);
		close(file);
		return (-1);
	}

	/* de hele file wordt in het geheugen gemapped */

	totlen = BLI_filesize(file);
	if (totlen && file>=0) {
		lseek(file, 0L, SEEK_SET);
		
		mem= MEM_mallocN(totlen, "mmap");
		if (read(file, mem, totlen) != totlen) {
			MEM_freeN(mem);
			mem = NULL;
		}
	} else {
		mem = NULL;
	}
	close (file);

	if (!mem) return (-1);

	anhd.interleave = 0;
	anhd.bits = 0;
	anhd.type = 5;

	anim->anim5mmap = mem;
	anim->anim5len = totlen;
	anim->anim5flags = 0;
	anim->duration = 0;

	animbase = & anim->anim5base;
	animbase->first = animbase->last = 0;

	/* eerste plaatje inlezen */

	mem = mem + 6;
	totlen -= 12;

	len = GET_BIG_LONG(mem + 2);
	len = (len + 8 + 1) & ~1;
	delta = NEW(Anim5Delta);

	delta->data = mem;
	delta->type = ANIM5_MMAP;

	BLI_addtail(animbase, delta);

	mem += (len >> 1);
	totlen -= len;

	while (totlen > 0) {
		len = GET_BIG_LONG(mem + 2);
		len = (len + 8 + 1) & ~1;

		switch(GET_ID(mem)){
		case FORM:
			len = 12;
			break;
		case ANHD:
			memcpy(&anhd, mem + 4, sizeof(Anhd));
			break;
		case DLTA:
			delta = NEW(Anim5Delta);
			delta->data = mem;
			delta->type = ANIM5_MMAP;
			BLI_addtail(animbase, delta);
			break;
		}

		mem += (len >> 1);
		totlen -= len;
	}

	if (anhd.interleave == 1) anim->anim5flags |= ANIM5_SNGBUF;
	if (BIG_SHORT(anhd.bits) & 2) anim->anim5decode = anim5xordecode;
	else anim->anim5decode = anim5decode;

	/* laatste twee delta's wissen */

	delta = animbase->last;
	if (delta) {
		BLI_remlink(animbase, delta);
		free(delta);
	}

	if ((anim->anim5flags & ANIM5_SNGBUF) == 0) {
		delta = animbase->last;
		if (delta) {
			BLI_remlink(animbase, delta);
			free(delta);
		}
	}

	anim->duration = BLI_countlist(animbase);

	return(rewindanim5(anim));
}


static struct ImBuf * anim5_fetchibuf(struct anim * anim) {
	struct ImBuf * ibuf;

	if (anim == 0) return (0);

	ibuf = IMB_dupImBuf(anim->ibuf1);
	planes_to_rect(ibuf, anim->ib_flags);

	return(ibuf);
}

static int startavi (struct anim *anim) {

	AviError avierror;
#ifdef _WIN32
	HRESULT	hr;
	int i, firstvideo = -1;
	BYTE abFormat[1024];
	LONG l;
	LPBITMAPINFOHEADER lpbi;
	AVISTREAMINFO avis;
#endif

	anim->avi = MEM_callocN (sizeof(AviMovie),"animavi");

	if (anim->avi == NULL) {
		printf("Can't open avi: %s\n", anim->name);
		return -1;
	}

	avierror = AVI_open_movie (anim->name, anim->avi);

#ifdef _WIN32
	if (avierror == AVI_ERROR_COMPRESSION) {
		AVIFileInit();
		hr = AVIFileOpen(&anim->pfile, anim->name, OF_READ, 0L);
		if (hr == 0) {
			anim->pfileopen = 1;
			for (i = 0; i < MAXNUMSTREAMS; i++) {
				if (AVIFileGetStream(anim->pfile, &anim->pavi[i], 0L, i) != AVIERR_OK) {
					break;
				}
				
				AVIStreamInfo(anim->pavi[i], &avis, sizeof(avis));
				if ((avis.fccType == streamtypeVIDEO) && (firstvideo == -1)) {
					anim->pgf = AVIStreamGetFrameOpen(anim->pavi[i], NULL);
					if (anim->pgf) {
						firstvideo = i;

						// get stream length
						anim->avi->header->TotalFrames = AVIStreamLength(anim->pavi[i]);
						
						// get information about images inside the stream
						l = sizeof(abFormat);
						AVIStreamReadFormat(anim->pavi[i], 0, &abFormat, &l);
						lpbi = (LPBITMAPINFOHEADER)abFormat;
						anim->avi->header->Height = lpbi->biHeight;
						anim->avi->header->Width = lpbi->biWidth;
					} else {
						FIXCC(avis.fccHandler);
						FIXCC(avis.fccType);
						printf("Can't find AVI decoder for type : %4.4hs/%4.4hs\n",
							(LPSTR)&avis.fccType,
							(LPSTR)&avis.fccHandler);
					}
				}
			}

			// register number of opened avistreams
			anim->avistreams = i;

			//
			// Couldn't get any video streams out of this file
			//
			if ((anim->avistreams == 0) || (firstvideo == -1)) {
				avierror = AVI_ERROR_FORMAT;
			} else {
				avierror = AVI_ERROR_NONE;
				anim->firstvideo = firstvideo;
			}
		} else {
			AVIFileExit();
		}
	}
#endif

	if (avierror != AVI_ERROR_NONE) {
		AVI_print_error(avierror);
		printf ("Error loading avi: %s\n", anim->name);		
		free_anim_avi(anim);
		return -1;
	}
	
	anim->duration = anim->avi->header->TotalFrames;
	anim->params = 0;

	anim->x = anim->avi->header->Width;
	anim->y = anim->avi->header->Height;
	anim->interlacing = 0;
	anim->orientation = 0;
	anim->framesize = anim->x * anim->y * 4;

	anim->curposition = 0;

	/*  printf("x:%d y:%d size:%d interl:%d dur:%d\n", anim->x, anim->y, anim->framesize, anim->interlacing, anim->duration);*/

	return 0;
}

static ImBuf * avi_fetchibuf (struct anim *anim, int position) {
	ImBuf *ibuf = NULL;
	int *tmp;
	int y;
	
	if (anim == NULL) return (NULL);

#ifdef _WIN32
	if (anim->avistreams) {
		LPBITMAPINFOHEADER lpbi;

		if (anim->pgf) {
		    lpbi = AVIStreamGetFrame(anim->pgf, position + AVIStreamStart(anim->pavi[anim->firstvideo]));
			if (lpbi) {
				ibuf = IMB_ibImageFromMemory((int *) lpbi, 100, IB_rect);
			}
		}
	} else {
#else
	if (1) {
#endif
		ibuf = IMB_allocImBuf (anim->x, anim->y, 24, IB_rect, 0);

		tmp = AVI_read_frame (anim->avi, AVI_FORMAT_RGB32, position,
			AVI_get_stream(anim->avi, AVIST_VIDEO, 0));
		
		if (tmp == NULL) {
			printf ("Error reading frame from AVI");
			IMB_freeImBuf (ibuf);
			return NULL;
		}

		for (y=0; y < anim->y; y++) {
			memcpy (&(ibuf->rect)[((anim->y-y)-1)*anim->x],  &tmp[y*anim->x],  
					anim->x * 4);
		}
		
		MEM_freeN (tmp);
	}

	return ibuf;
}

/* probeer volgende plaatje te lezen */
/* Geen plaatje, probeer dan volgende animatie te openen */
/* gelukt, haal dan eerste plaatje van animatie */

static struct ImBuf * anim_getnew(struct anim * anim) {
	struct ImBuf *ibuf = 0;

	if (anim == NULL) return(0);

	free_anim_anim5(anim);
	free_anim_movie(anim);
	free_anim_avi(anim);

	if (anim->curtype != 0) return (0);
	anim->curtype = imb_get_anim_type(anim->name);	

	switch (anim->curtype) {
	case ANIM_ANIM5:
		if (startanim5(anim)) return (0);
		ibuf = anim5_fetchibuf(anim);
		break;
	case ANIM_SEQUENCE:
		ibuf = IMB_loadiffname(anim->name, anim->ib_flags);
		if (ibuf) {
			strcpy(anim->first, anim->name);
			anim->duration = 1;
		}
		break;
	case ANIM_MOVIE:
		if (startmovie(anim)) return (0);
		ibuf = IMB_allocImBuf (anim->x, anim->y, 24, 0, 0); /* fake */
		break;
	case ANIM_AVI:
		if (startavi(anim)) return (0);
		ibuf = IMB_allocImBuf (anim->x, anim->y, 24, 0, 0);
		break;
	}

	return(ibuf);
}


struct ImBuf * IMB_anim_absolute(struct anim * anim, int position) {
	struct ImBuf * ibuf = 0;
	char head[256], tail[256];
	unsigned short digits;
	int pic;

	if (anim == NULL) return(0);

	if (anim->curtype == 0)	{
		ibuf = anim_getnew(anim);
		if (ibuf == NULL) return (0);
		IMB_freeImBuf(ibuf); /* ???? */
	}

	if (position < 0) return(0);
	if (position >= anim->duration) return(0);

	switch(anim->curtype) {
	case ANIM_ANIM5:
		if (anim->curposition > position) rewindanim5(anim);
		while (anim->curposition < position) {
			if (nextanim5(anim)) return (0);
		}
		ibuf = anim5_fetchibuf(anim);
		break;
	case ANIM_SEQUENCE:
		pic = an_stringdec(anim->first, head, tail, &digits);
		pic += position;
		an_stringenc(anim->name, head, tail, digits, pic);
		ibuf = IMB_loadiffname(anim->name, LI_rect);
		if (ibuf) {
			anim->curposition = position;
			/* patch... by freeing the cmap you prevent a double apply cmap... */
			/* probably the IB_CMAP option isn't working proper
			 * after the abgr->rgba reconstruction
			 */
			IMB_freecmapImBuf(ibuf);
		}
		break;
	case ANIM_MOVIE:
		ibuf = movie_fetchibuf(anim, position);
		if (ibuf) {
			anim->curposition = position;
			IMB_convert_rgba_to_abgr(ibuf->x*ibuf->y, ibuf->rect);
		}
		break;
	case ANIM_AVI:
		ibuf = avi_fetchibuf(anim, position);
		if (ibuf) anim->curposition = position;
		break;
	}

	if (ibuf) {
		if (anim->ib_flags & IB_ttob) IMB_flipy(ibuf);
		sprintf(ibuf->name, "%s.%04d", anim->name, anim->curposition + 1);
		
	}
	return(ibuf);
}

struct ImBuf * IMB_anim_nextpic(struct anim * anim) {
	struct ImBuf * ibuf = 0;

	if (anim == 0) return(0);

	ibuf = IMB_anim_absolute(anim, anim->curposition + 1);

	return(ibuf);
}

/***/

int IMB_anim_get_duration(struct anim *anim) {
	return anim->duration;
}
