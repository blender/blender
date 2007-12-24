/* $Id: objfnt.h 229 2002-12-27 13:11:01Z mein $ 
*/
/*
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
#ifndef OBJFNTDEF
#define OBJFNTDEF

typedef struct chardesc {
    short movex, movey;		/* advance */
    short llx, lly;		/* bounding box */
    short urx, ury;
    short *data;		/* char data */
    long datalen;		
} chardesc;

typedef struct objfnt {
    struct objfnt *freeaddr;	/* if freeaddr != 0, objfnt is one chunck */
    short type;
    short charmin, charmax;
    short my_nchars;
    short scale;
    chardesc *my_chars;
} objfnt;

#define OFMAGIC		0x93339333

#define TM_TYPE		1
#define PO_TYPE		2
#define SP_TYPE		3

/* ops for tmesh characters */

#define	TM_BGNTMESH	(1)
#define	TM_SWAPTMESH	(2)
#define	TM_ENDBGNTMESH	(3)
#define	TM_RETENDTMESH	(4)
#define	TM_RET		(5)

/* ops for poly characters */

#define	PO_BGNLOOP	(1)
#define	PO_ENDBGNLOOP	(2)
#define	PO_RETENDLOOP	(3)
#define	PO_RET		(4)

/* ops for spline  characters */

#define	SP_MOVETO	(1)
#define	SP_LINETO	(2)
#define	SP_CURVETO	(3)
#define	SP_CLOSEPATH	(4)
#define	SP_RETCLOSEPATH	(5)
#define	SP_RET		(6)


#define MIN_ASCII 	' '
#define MAX_ASCII 	'~'
#define NASCII		(256 - 32)

#define NOBBOX		(30000)

typedef struct pschar {
    char *name;
    int code;
    int prog;
} pschar;

extern pschar charlist[NASCII];

/*  objfnt *fontname(void); */
/*  objfnt *readobjfnt(void); */
/*  objfnt *newobjfnt(void); */
/*  float fontstringwidth(void); */
/*  short *getcharprog(void); */
/*  chardesc *BLI_getchardesc(void); */
/*  char *asciiname(void); */

#endif

