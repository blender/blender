/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#ifndef BKE_BMESHCUSTOMDATA_H
#define BKE_BMESHCUSTOMDATA_H

/** \file BKE_bmeshCustomData.h
 *  \ingroup bke
 *  \since January 2007
 *  \brief BMesh modeler structure and functions - custom data.
 */

struct BLI_mempool;

/*Custom Data Types and defines
	Eventual plan is to move almost everything to custom data and let caller
	decide when making the mesh what layers they want to store in the mesh

	This stuff should probably go in a seperate file....
*/

#define BME_CD_FACETEX		0		/*Image texture/texface*/
#define BME_CD_LOOPTEX		1		/*UV coordinates*/
#define BME_CD_LOOPCOL		2		/*Vcolors*/
#define BME_CD_DEFORMVERT	3		/*Vertex Group/Weights*/
#define BME_CD_NUMTYPES		4

typedef struct BME_CustomDataLayer {
	int type;       				/* type of data in layer */
	int offset;     				/* offset of layer in block */
	int active;     				/* offset of active layer*/
	char name[32];  				/* layer name */
} BME_CustomDataLayer;

typedef struct BME_CustomData {
	struct BME_CustomDataLayer *layers;	/*Custom Data Layers*/
	struct BLI_mempool *pool;				/*pool for alloc of blocks*/
	int totlayer, totsize;         	/*total layers and total size in bytes of each block*/
} BME_CustomData;

typedef struct BME_CustomDataInit{
	int layout[BME_CD_NUMTYPES];
	int active[BME_CD_NUMTYPES];
	int totlayers;
	char *nametemplate;
} BME_CustomDataInit;

/*Custom data types*/
typedef struct BME_DeformWeight {
	int				def_nr;
	float			weight;
} BME_DeformWeight;

typedef struct BME_DeformVert {
	struct BME_DeformWeight *dw;
	int totweight;
} BME_DeformVert;

typedef struct BME_facetex{
	struct Image *tpage;
	char flag, transp;
	short mode, tile, unwrap;
}BME_facetex;

typedef struct BME_looptex{
	float u, v;
}BME_looptex;

typedef struct BME_loopcol{
	char r, g, b, a;
}BME_loopcol;

/*CUSTOM DATA API*/
void BME_CD_Create(struct BME_CustomData *data, struct BME_CustomDataInit *init, int initalloc);
void BME_CD_Free(struct BME_CustomData *data);
void BME_CD_free_block(struct BME_CustomData *data, void **block);
void BME_CD_copy_data(const struct BME_CustomData *source, struct BME_CustomData *dest, void *src_block, void **dest_block);
void BME_CD_set_default(struct BME_CustomData *data, void **block);

#endif
