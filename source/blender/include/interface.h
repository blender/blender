/**
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

#ifndef INTERFACE_H
#define INTERFACE_H

/* general defines */

#define UI_MAX_DRAW_STR	400
#define UI_MAX_NAME_STR	64
#define UI_ARRAY	29

/* block->font, for now: bold = medium+1 */
#define UI_HELV			0
#define UI_HELVB		1

/* Button types */
#define CHA	32
#define SHO	64
#define INT	96
#define FLO	128
#define FUN	192
#define BIT	256

#define BUTPOIN	(128+64+32)

#define BUT	(1<<9)
#define ROW	(2<<9)
#define TOG	(3<<9)
#define SLI	(4<<9)
#define	NUM	(5<<9)
#define TEX	(6<<9)
#define TOG3	(7<<9)
#define TOGR	(8<<9)
#define TOGN	(9<<9)
#define LABEL	(10<<9)
#define MENU	(11<<9)
#define ICONROW	(12<<9)
#define ICONTOG	(13<<9)
#define NUMSLI	(14<<9)
#define COL		(15<<9)
#define IDPOIN	(16<<9)
#define HSVSLI 	(17<<9)
#define SCROLL	(18<<9)
#define BLOCK	(19<<9)
#define BUTM	(20<<9)
#define SEPR	(21<<9)
#define LINK	(22<<9)
#define INLINK	(23<<9)
#define KEYEVT	(24<<9)
#define ICONTEXTROW (25<<9)

#define BUTTYPE	(31<<9)

#define MAXBUTSTR	20


/* return from uiDoBlock */
#define UI_CONT				0
#define UI_NOTHING			1
#define UI_RETURN_CANCEL	2
#define UI_RETURN_OK		4
#define UI_RETURN_OUT		8
#define UI_RETURN			14
#define UI_EXIT_LOOP		16

/* uiBut->flag */
#define UI_SELECT		1
#define UI_MOUSE_OVER	2
#define UI_ACTIVE		4
#define UI_HAS_ICON		8
#define UI_TEXT_LEFT	16
/* definitions for icons (and their alignment) in buttons */
#define UI_ICON_LEFT		32
#define UI_ICON_RIGHT		64
/* definitions for icons (and their alignment) in buttons */

/* uiBlock->flag */
#define UI_BLOCK_LOOP		1
#define UI_BLOCK_REDRAW		2
#define UI_BLOCK_RET_1		4
#define UI_BLOCK_BUSY		8
#define UI_BLOCK_NUMSELECT	16
#define UI_BLOCK_ENTER_OK	32


/* uiBlock->dt */
#define UI_EMBOSSX		0	/* Rounded embossed button */
#define UI_EMBOSSW		1	/* Flat bordered button */
#define UI_EMBOSSN		2	/* No border */
#define UI_EMBOSSF		3	/* Square embossed button */
#define UI_EMBOSSM		4	/* Colored Border */
#define UI_EMBOSSP		5	/* Borderless coloured button */
#define UI_EMBOSSA		6	/* same as EMBOSSX but with arrows to simulate */
#define UI_EMBOSSTABL	7
#define UI_EMBOSSTABM	8
#define UI_EMBOSSTABR	9
#define UI_EMBOSST		10
#define UI_EMBOSSMB		11	/* emboss menu button */

/* uiBlock->direction */
#define UI_TOP		0
#define UI_DOWN		1
#define UI_LEFT		2
#define UI_RIGHT	3

/* uiBlock->autofill */
#define UI_BLOCK_COLLUMNS	1
#define UI_BLOCK_ROWS		2

#define UI_PNL_TRANSP	0
#define UI_PNL_SOLID	1

#endif

