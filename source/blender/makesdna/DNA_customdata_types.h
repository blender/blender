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
	CustomDataLayer *layers;  /* data layer descriptors, ordered by type */
	int numLayers;            /* current number of layers */
	int maxLayers;            /* maximum number of layers */
	int numElems;             /* current number of elements */
	int maxElems;             /* maximum number of elements */
	int subElems;             /* number of sub-elements layers can have */
	int totSize;              /* in editmode, total size of all data layers */
} CustomData;

/* custom data types */
#define LAYERTYPE_MVERT			0
#define LAYERTYPE_MSTICKY		1
#define LAYERTYPE_MDEFORMVERT	2
#define LAYERTYPE_MEDGE			3
#define LAYERTYPE_MFACE			4
#define LAYERTYPE_TFACE			5
#define LAYERTYPE_MCOL			6
#define LAYERTYPE_ORIGINDEX		7
#define LAYERTYPE_NORMAL		8
#define LAYERTYPE_FLAGS			9
#define LAYERTYPE_NUMTYPES		10

#endif
