/**
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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef DNA_CUSTOMDATA_TYPES_H
#define DNA_CUSTOMDATA_TYPES_H

/* descriptor and storage for a custom data layer */
typedef struct CustomDataLayer {
	int type;       /* type of data in layer */
	int offset;     /* in editmode, offset of layer in block */
	int flag;       /* general purpose flag */
	int active;     /* number of the active layer of this type */
	int active_rnd; /* number of the layer to render*/
	char pad[4];
	char name[32];  /* layer name */
	void *data;     /* layer data */
} CustomDataLayer;

/* structure which stores custom element data associated with mesh elements
 * (vertices, edges or faces). The custom data is organised into a series of
 * layers, each with a data type (e.g. MTFace, MDeformVert, etc.). */
typedef struct CustomData {
	CustomDataLayer *layers;  /* CustomDataLayers, ordered by type */
	int totlayer, maxlayer;   /* number of layers, size of layers array */
	int totsize, pad;         /* in editmode, total size of all data layers */
} CustomData;

/* CustomData.type */
#define CD_MVERT		0
#define CD_MSTICKY		1
#define CD_MDEFORMVERT	2
#define CD_MEDGE		3
#define CD_MFACE		4
#define CD_MTFACE		5
#define CD_MCOL			6
#define CD_ORIGINDEX	7
#define CD_NORMAL		8
#define CD_FLAGS		9
#define CD_PROP_FLT		10
#define CD_PROP_INT		11
#define CD_PROP_STR		12
#define CD_ORIGSPACE	13 /* for modifier stack face location mapping */
#define CD_ORCO			14
#define CD_TANGENT		15
#define CD_NUMTYPES		16

/* Bits for CustomDataMask */
#define CD_MASK_MVERT		(1 << CD_MVERT)
#define CD_MASK_MSTICKY		(1 << CD_MSTICKY)
#define CD_MASK_MDEFORMVERT	(1 << CD_MDEFORMVERT)
#define CD_MASK_MEDGE		(1 << CD_MEDGE)
#define CD_MASK_MFACE		(1 << CD_MFACE)
#define CD_MASK_MTFACE		(1 << CD_MTFACE)
#define CD_MASK_MCOL		(1 << CD_MCOL)
#define CD_MASK_ORIGINDEX	(1 << CD_ORIGINDEX)
#define CD_MASK_NORMAL		(1 << CD_NORMAL)
#define CD_MASK_FLAGS		(1 << CD_FLAGS)
#define CD_MASK_PROP_FLT	(1 << CD_PROP_FLT)
#define CD_MASK_PROP_INT	(1 << CD_PROP_INT)
#define CD_MASK_PROP_STR	(1 << CD_PROP_STR)
#define CD_MASK_ORIGSPACE	(1 << CD_ORIGSPACE)
#define CD_MASK_ORCO		(1 << CD_ORCO)
#define CD_MASK_TANGENT		(1 << CD_TANGENT)


/* CustomData.flag */

/* indicates layer should not be copied by CustomData_from_template or
 * CustomData_copy_data */
#define CD_FLAG_NOCOPY    (1<<0)
/* indicates layer should not be freed (for layers backed by external data) */
#define CD_FLAG_NOFREE    (1<<1)
/* indicates the layer is only temporary, also implies no copy */
#define CD_FLAG_TEMPORARY ((1<<2)|CD_FLAG_NOCOPY)

/* Limits */
#define MAX_MTFACE 8
#define MAX_MCOL   8

#endif
