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
#ifndef DNA_CUSTOMDATA_TYPES_H
#define DNA_CUSTOMDATA_TYPES_H

/* descriptor and storage for a custom data layer */
typedef struct CustomDataLayer {
	int type;       /* type of data in layer */
	int offset;     /* in editmode, offset of layer in block */
	int flag, pad;  /* general purpose flag */
	void *data;     /* layer data */
} CustomDataLayer;

/* structure which stores custom element data associated with mesh elements
 * (vertices, edges or faces). The custom data is organised into a series of
 * layers, each with a data type (e.g. TFace, MDeformVert, etc.). */
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
#define CD_NUMTYPES		10

/* CustomData.flag */

/* indicates layer should not be copied by CustomData_from_template or
 * CustomData_copy_data */
#define CD_FLAG_NOCOPY    (1<<0)
/* indicates layer should not be freed (for layers backed by external data) */
#define CD_FLAG_NOFREE    (1<<1)
/* indicates the layer is only temporary, also implies no copy */
#define CD_FLAG_TEMPORARY ((1<<2)|CD_FLAG_NOCOPY)

#endif
