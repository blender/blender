/**
 * anim5.c
 *
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): phase, code torn apart from anim.c
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "BLI_blenlib.h" /* BLI_remlink BLI_filesize BLI_addtail
                            BLI_countlist BLI_stringdec */

#include "imbuf.h"
#include "imbuf_patch.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_cmap.h"
#include "IMB_allocimbuf.h"
#include "IMB_bitplanes.h"
#include "IMB_amiga.h"

#include "IMB_anim.h"

#include "IMB_anim5.h"

#ifdef _WIN32
#include <io.h>
#include "BLI_winstuff.h"
#endif

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

void free_anim_anim5(struct anim * anim) {
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
				IMB_convert_rgba_to_abgr(ibuf);
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

	/*	composition delta:
		list with ofsets for delta' s by bitplane (ofspoint)
		by column in delta (point)
			number of operations (noops)
				code
					associated data
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

				if ( (noop = *(point++)) ){
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

				if ( (noop = *(point++)) ){
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


int nextanim5(struct anim * anim) {
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

int rewindanim5(struct anim * anim) {
	Anim5Delta * delta;
	struct ImBuf * ibuf;

	if (anim == 0) return (-1);

	IMB_free_anim_ibuf(anim);

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


int startanim5(struct anim * anim) {
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
	if (totlen>0 && file>=0) {
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


struct ImBuf * anim5_fetchibuf(struct anim * anim) {
	struct ImBuf * ibuf;

	if (anim == 0) return (0);

	ibuf = IMB_dupImBuf(anim->ibuf1);
	planes_to_rect(ibuf, anim->ib_flags);

	ibuf->profile = IB_PROFILE_SRGB;
	
	return(ibuf);
}

