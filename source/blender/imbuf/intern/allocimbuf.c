/*
 * allocimbuf.c
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

/**
 * \file allocimbuf.c
 * \ingroup imbuf
 * \brief Handles de/allocating memory for ImBuf struct variables.
 *
 * This file uses free() and MEM_mallocN() to allocate and deallocate memory
 * to the members of the ImBuf struct.  It also zeroes out pointers and
 * tracks buffer sizes.
 */
/* It's become a bit messy... Basically, only the IMB_ prefixed files
 * should remain. */

#include "IMB_imbuf_types.h"

#include "imbuf.h"
#include "imbuf_patch.h"
#include "IMB_imbuf.h"

#include "IMB_divers.h"
#include "IMB_allocimbuf.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * Default cmap.  Used to initialize the ImBuf->cmap variable.
 */
static unsigned int dfltcmap[16] = {
	0x00000000, 0xffffffff, 0x777777ff, 0xccccccff, 
	0xcc3344ff, 0xdd8844ff, 0xccdd44ff, 0x888833ff, 
	0x338844ff, 0x44dd44ff, 0x44ddccff, 0x3388ccff, 
	0x8888ddff, 0x4433ccff, 0xcc33ccff, 0xcc88ddff
};

/**
 * \brief Clears memory associated to the ImBuf* ibuf->planes
 *
 * This function uses free() to de-allocate the memory associated with
 * the planes member of the ImBuf struct, and sets the pointer to 0.
 * The ibuf->mall also gets the IB_planes bit unset.
 */
void imb_freeplanesImBuf(struct ImBuf * ibuf)
{
	if (ibuf==0) return;
	if (ibuf->planes){
		if (ibuf->mall & IB_planes) free(ibuf->planes);
	}
	ibuf->planes = 0;
	ibuf->mall &= ~IB_planes;
}


/**
 * \brief Clears memory associated to the ImBuf* ibuf->rect
 *
 * This function uses free() to de-allocate the memory associated with
 * the rect member of the ImBuf struct, and sets the pointer to 0.
 * The ibuf->mall also gets the IB_rect bit unset.
 */
void imb_freerectImBuf(struct ImBuf * ibuf)
{
	if (ibuf==0) return;
	if (ibuf->rect){
		if (ibuf->mall & IB_rect) free(ibuf->rect);
	}
	ibuf->rect=0;
	ibuf->mall &= ~IB_rect;
}

/**
 * \brief Clears memory associated to the ImBuf* ibuf->encodedbuffer
 *
 * This function uses free() to de-allocate the memory associated with
 * the encodedbuffer member of the ImBuf struct, and sets the following
 * variables to 0:
 *
 * encodedbuffer
 * encodedbuffersize
 * encodedsize
 *
 * The ibuf->mall also gets the IB_mem bit unset.
 */
static void freeencodedbufferImBuf(struct ImBuf * ibuf)
{
	if (ibuf==0) return;
	if (ibuf->encodedbuffer){
		if (ibuf->mall & IB_mem) free(ibuf->encodedbuffer);
	}
	ibuf->encodedbuffer = 0;
	ibuf->encodedbuffersize = 0;
	ibuf->encodedsize = 0;
	ibuf->mall &= ~IB_mem;
}

/**
 * \brief Clears memory associated to the ImBuf* ibuf->zbuf
 *
 * This function uses free() to de-allocate the memory associated with
 * the zbuf member of the ImBuf struct, and sets the pointer to 0.
 * The ibuf->mall also gets the IB_zbuf bit unset.
 */
void IMB_freezbufImBuf(struct ImBuf * ibuf)
{
	if (ibuf==0) return;
	if (ibuf->zbuf){
		if (ibuf->mall & IB_zbuf) free(ibuf->zbuf);
	}
	ibuf->zbuf=0;
	ibuf->mall &= ~IB_zbuf;
}

/**
 * \brief Clears memory associated to the ImBuf* ibuf->cmap
 *
 * This function uses free() to de-allocate the memory associated with
 * the cmap member of the ImBuf struct, and sets the pointer to 0.
 * The ibuf->mall also gets the IB_cmap bit unset.
 */
void IMB_freecmapImBuf(struct ImBuf * ibuf)
{
	if (ibuf == 0) return;
	if (ibuf->cmap){
		if (ibuf->mall & IB_cmap) free(ibuf->cmap);
	}
	ibuf->cmap = 0;
	ibuf->mall &= ~IB_cmap;
}


/**
 * \brief Clears memory associated to the ImBuf ibuf
 *
 * This function uses the imb_free*ImBuf() functions to free the
 * memory associated to the ibuf argument, and then frees the ibuf
 * pointer using free()
 */
void IMB_freeImBuf(struct ImBuf * ibuf)
{
	if (ibuf){
		imb_freeplanesImBuf(ibuf);
		imb_freerectImBuf(ibuf);
		IMB_freezbufImBuf(ibuf);
		IMB_freecmapImBuf(ibuf);
		freeencodedbufferImBuf(ibuf);
		free(ibuf);
	}
}


/**
 * \brief Allocates memory to the zbuf member of the ImBuf struct.
 *
 * This function uses MEM_mallocN(int,char *) to allocate memory to
 * the zbuf member of the ImBuf struct.  Any memory associated to the
 * zbuf member is overwritten.
 *
 * The IB_zbuf bit of the ibuf->mall variable is set.
 * \return Returns FALSE if ibuf==0 or if MEM_mallocN() fails, otherwise
 * returns TRUE
 */
static short addzbufImBuf(struct ImBuf * ibuf)
{
	int size;

	if (ibuf==0) return(FALSE);
	IMB_freezbufImBuf(ibuf);

	size = ibuf->x * ibuf->y * sizeof(unsigned int);
	if (ibuf->zbuf = MEM_mallocN(size, "addzbufImBuf")){
		ibuf->mall |= IB_zbuf;
		return (TRUE);
	}

	return (FALSE);
}


/**
 * \brief Allocates memory to the encodedbuffer member of the ImBuf struct.
 *
 * This function uses MEM_mallocN(int,char *) to allocate memory to
 * the encodedbuffer member of the ImBuf struct.  Any memory associated to the
 * encodedbuffer member is overwritten.
 * 
 * The ibuf->encodedbuffersize is set to 10000 if it was previously 0.
 *
 * The ibuf->encodedsize is set to 0.
 *
 * The IB_mem bit of the ibuf->mall variable is set.
 * \return Returns FALSE if ibuf==0 or if MEM_mallocN() fails, otherwise
 * returns TRUE
 */
short imb_addencodedbufferImBuf(struct ImBuf * ibuf)
{
	if (ibuf==0) return(FALSE);

	freeencodedbufferImBuf(ibuf);

	if (ibuf->encodedbuffersize == 0) 
		ibuf->encodedbuffersize = 10000;

	ibuf->encodedsize = 0;

	if (ibuf->encodedbuffer = MEM_mallocN(ibuf->encodedbuffersize, "addencodedbufferImBuf")){
		ibuf->mall |= IB_mem;
		return (TRUE);
	}

	return (FALSE);
}


/**
 * \brief Adds memory to ibuf->encodedbuffer
 *
 * This function doubles the size of encodedbuffer if it is less than 10000,
 * otherwise sets encodedbuffersize to 10000.
 *
 * The ibuf members encodedbuffersize and encoded size also get set.
 * \return Returns FALSE if:
 *  - ibuf==0
 *  - ibuf->encodedbuffersize < ibuf->encodedsize
 *  - MEM_mallocN() fails (returns NULL)
 * otherwise, returns TRUE
 */
short imb_enlargeencodedbufferImBuf(struct ImBuf * ibuf)
{
	unsigned int newsize, encodedsize;
	void *newbuffer;

	if (ibuf==0) return(FALSE);

	if (ibuf->encodedbuffersize < ibuf->encodedsize) {
		printf("imb_enlargeencodedbufferImBuf: error in parameters\n");
		return(FALSE);
	}

	newsize = 2 * ibuf->encodedbuffersize;
	if (newsize < 10000) newsize = 10000;

	newbuffer = MEM_mallocN(newsize, "enlargeencodedbufferImBuf");
	if (newbuffer == NULL) return(FALSE);

	if (ibuf->encodedbuffer) {
		memcpy(newbuffer, ibuf->encodedbuffer, ibuf->encodedsize);
	} else {
		ibuf->encodedsize = 0;
	}

	encodedsize = ibuf->encodedsize;

	freeencodedbufferImBuf(ibuf);

	ibuf->encodedbuffersize = newsize;
	ibuf->encodedsize = encodedsize;
	ibuf->encodedbuffer = newbuffer;
	ibuf->mall |= IB_mem;

	return (TRUE);
}

/**
 * \brief Allocates memory to the rect member of the ImBuf struct.
 *
 * This function uses MEM_mallocN(int,char *) to allocate memory to
 * the rect member of the ImBuf struct.  Any memory associated to the
 * rect member is overwritten.
 *
 * The IB_rect bit of the ibuf->mall variable is set.
 * \return Returns FALSE if ibuf==0 or if MEM_mallocN() fails, otherwise
 * returns TRUE
 */

short imb_addrectImBuf(struct ImBuf * ibuf)
{
	int size;

	if (ibuf==0) return(FALSE);
	imb_freerectImBuf(ibuf);

	size = ibuf->x * ibuf->y * sizeof(unsigned int);
	if (ibuf->rect = MEM_mallocN(size, "imb_addrectImBuf")){
		ibuf->mall |= IB_rect;
		if (ibuf->depth > 32) return (addzbufImBuf(ibuf));
		else return (TRUE);
	}

	return (FALSE);
}


/**
 * \brief Allocates memory to the cmap member of the ImBuf struct.
 *
 * This function uses MEM_mallocN(int,char *) to allocate memory to
 * the cmap member of the ImBuf struct.  Any memory associated to the
 * cmap member is overwritten.
 *
 * The memory is initialized to the value of the dfltcmap variable.
 *
 * The IB_cmap bit of the ibuf->mall variable is set.
 * \return Returns FALSE if ibuf==0 or if MEM_mallocN() fails, otherwise
 * returns TRUE
 */
short imb_addcmapImBuf(struct ImBuf *ibuf)
{
	int min;
	
	if (ibuf==0) return(FALSE);
	IMB_freecmapImBuf(ibuf);

	imb_checkncols(ibuf);
	if (ibuf->maxcol == 0) return (TRUE);

	if (ibuf->cmap = MEM_callocN(sizeof(unsigned int) * ibuf->maxcol, "imb_addcmapImBuf")){
		min = ibuf->maxcol * sizeof(unsigned int);
		if (min > sizeof(dfltcmap)) min = sizeof(dfltcmap);
		memcpy(ibuf->cmap, dfltcmap, min);
		ibuf->mall |= IB_cmap;
		return (TRUE);
	}

	return (FALSE);
}


/**
 * \brief Adds memory to the planes member of ibuf.
 *
 * This function adds ibuf->depth number of planes to ibuf.
 *
 * Sets:
 *  - ibuf->skipx
 *  - IB_planes bit of ibuf->mall
 * \return Returns FALSE if:
 *  - ibuf==0
 *  - MEM_mallocN() fails (returns 0)
 * Otherwise, returns TRUE
 */
short imb_addplanesImBuf(struct ImBuf *ibuf)
{
	int size;
	short skipx,d,y;
	unsigned int **planes;
	unsigned int *point2;

	if (ibuf==0) return(FALSE);
	imb_freeplanesImBuf(ibuf);

	skipx = ((ibuf->x+31) >> 5);
	ibuf->skipx=skipx;
	y=ibuf->y;
	d=ibuf->depth;

	planes = MEM_mallocN( (d*skipx*y)*sizeof(int) + d*sizeof(int *), "imb_addplanesImBuf");
	
	ibuf->planes = planes;
	if (planes==0) return (FALSE);

	point2 = (unsigned int *)(planes+d);
	size = skipx*y;

	for (;d>0;d--){
		*(planes++) = point2;
		point2 += size;
	}
	ibuf->mall |= IB_planes;

	return (TRUE);
}

/**
 * \brief This functions allocates memory to an ImBuf struct.
 *
 * This function allocates memory for an ImBuf struct and returns a pointer
 * to the struct.  The MEM_callocN() function us used to allocate the memory.
 *
 * The x and y parameters define the resolution of the image, the d parameter
 * defines the (bit?) depth, the flags parameter defines whether the rect, zbuf or
 * planes members should exist. The bitmap flag will set the IB_planes bit in the
 * flags variable.
 *
 * \return Returns 0 if any of the imb_add*ImBuf() functions return FALSE. If the
 * memory for ibuf is not allocated, the return also should be 0.  Otherwise, returns
 * a pointer to an ImBuf struct.
 */
struct ImBuf *IMB_allocImBuf(short x,short y,uchar d,unsigned int flags,uchar bitmap)
{
	struct ImBuf *ibuf;

	ibuf = MEM_callocN(sizeof(struct ImBuf), "ImBuf_struct");
	if (bitmap) flags |= IB_planes;

	if (ibuf){
		ibuf->x=x;
		ibuf->y=y;
		ibuf->depth=d;
		ibuf->ftype=TGA;

		if (flags & IB_rect){
			if (imb_addrectImBuf(ibuf)==FALSE){
				IMB_freeImBuf(ibuf);
				return (0);
			}
		}
		
		if (flags & IB_zbuf){
			if (addzbufImBuf(ibuf)==FALSE){
				IMB_freeImBuf(ibuf);
				return (0);
			}
		}
		
		if (flags & IB_planes){
			if (imb_addplanesImBuf(ibuf)==FALSE){
				IMB_freeImBuf(ibuf);
				return (0);
			}
		}
	}
	return (ibuf);
}

/**
 * \brief Returns a pointer to a new imbuf which is a duplicate of the ibuf1 parameter.
 *
 * \return Returns 0 if any memory allocation fails (except for the cmap allocation. Is
 * this right?), otherwise returns a copy of ibuf1.
 */
struct ImBuf *IMB_dupImBuf(struct ImBuf *ibuf1)
{
	struct ImBuf *ibuf2, tbuf;
	int flags = 0;
	int x, y;
	
	if (ibuf1 == 0) return (0);

	if (ibuf1->rect) flags |= IB_rect;
	if (ibuf1->planes) flags |= IB_planes;

	x = ibuf1->x;
	y = ibuf1->y;
	if (ibuf1->flags & IB_fields) y *= 2;
	
	ibuf2 = IMB_allocImBuf(x, y, ibuf1->depth, flags, 0);
	if (ibuf2 == 0) return (0);

	if (flags & IB_rect) memcpy(ibuf2->rect,ibuf1->rect,x * y * sizeof(int));
	if (flags & IB_planes) memcpy(*(ibuf2->planes),*(ibuf1->planes),ibuf1->depth * ibuf1->skipx * y * sizeof(int));

	if (ibuf1->encodedbuffer) {
		ibuf2->encodedbuffersize = ibuf1->encodedbuffersize;
		if (imb_addencodedbufferImBuf(ibuf2) == FALSE) {
			IMB_freeImBuf(ibuf2);
			return(0);
		}

		memcpy(ibuf2->encodedbuffer, ibuf1->encodedbuffer, ibuf1->encodedsize);
	}


	tbuf = *ibuf1;
	
	// pointers goedzetten
	tbuf.rect		= ibuf2->rect;
	tbuf.planes		= ibuf2->planes;
	tbuf.cmap		= ibuf2->cmap;
	tbuf.encodedbuffer = ibuf2->encodedbuffer;
	
	// malloc flag goed zetten
	tbuf.mall		= ibuf2->mall;
	
	*ibuf2 = tbuf;
	
	if (ibuf1->cmap){
		imb_addcmapImBuf(ibuf2);
		if (ibuf2->cmap) memcpy(ibuf2->cmap,ibuf1->cmap,ibuf2->maxcol * sizeof(int));
	}

	return(ibuf2);
}
