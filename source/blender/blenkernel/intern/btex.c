/*
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
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_string.h"

#include "BKE_btex.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

/************************* File Format Definitions ***************************/

#define BTEX_ENDIAN_LITTLE	0
#define BTEX_ENDIAN_BIG		1

#define BTEX_DATA_FLOAT	0

typedef struct BTexHeader {
	char ID[4];					/* "BTEX" */
	char endian;				/* little, big */
	char version;				/* non-compatible versions */
	char subversion;			/* compatible sub versions */
	char pad;					/* padding */

	int structbytes;			/* size of this struct in bytes */
	int type;					/* image, mesh */
	int totlayer;				/* number of layers in the file */
} BTexHeader;

typedef struct BTexImageHeader {
	int structbytes;			/* size of this struct in bytes */
	int width;					/* image width */
	int height;					/* image height */
	int tile_size;				/* tile size (required power of 2) */
} BTexImageHeader;

typedef struct BTexMeshHeader {
	int structbytes;			/* size of this struct in bytes */
	int totgrid;				/* number of grids */
	int gridsize;				/* width of grids */
} BTexMeshHeader;

struct BTexLayer {
	int structbytes;				/* size of this struct in bytes */
	int datatype;					/* only float for now */
	int datasize;					/* size of data in layer */
	int type;						/* layer type */
	char name[BTEX_LAYER_NAME_MAX];	/* layer name */
};

/**************************** Other Definitions ******************************/

#define BTEX_VERSION		0
#define BTEX_SUBVERSION		0
#define BTEX_TILE_SIZE		64

struct BTex {
	int type;

	BTexHeader header;
	union {
		BTexImageHeader image;
		BTexMeshHeader mesh;
	} btype;

	BTexLayer *layer;
	int totlayer;

	FILE *readf;
	FILE *writef;
	int switchendian;
	size_t dataoffset;
};

/********************************* Create/Free *******************************/

static int btex_endian(void)
{
	if(ENDIAN_ORDER == L_ENDIAN)
		return BTEX_ENDIAN_LITTLE;
	else
		return BTEX_ENDIAN_BIG;
}

/*static int btex_data_type_size(int datatype)
{
	if(datatype == BTEX_DATA_FLOAT)
		return sizeof(float);
	
	return 0;
}*/

BTex *btex_create(int type)
{
	BTex *btex= MEM_callocN(sizeof(BTex), "BTex");

	btex->type= type;

	return btex;
}

void btex_free(BTex *btex)
{
	btex_read_close(btex);
	btex_write_close(btex);

	if(btex->layer)
		MEM_freeN(btex->layer);

	MEM_freeN(btex);
}

/********************************* Read/Write ********************************/

static int btex_read_header(BTex *btex)
{
	BTexHeader *header;
	BTexImageHeader *image;
	BTexMeshHeader *mesh;
	BTexLayer *layer;
	FILE *f= btex->readf;
	size_t offset = 0;
	int a;

	header= &btex->header;

	if(!fread(header, sizeof(BTexHeader), 1, btex->readf))
		return 0;
	
	if(memcmp(header->ID, "BTEX", sizeof(header->ID)) != 0)
		return 0;
	if(header->version > BTEX_VERSION)
		return 0;

	btex->switchendian= header->endian != btex_endian();
	header->endian= btex_endian();

	if(btex->switchendian) {
		SWITCH_INT(header->type);
		SWITCH_INT(header->totlayer);
		SWITCH_INT(header->structbytes);
	}

	if(!ELEM(header->type, BTEX_TYPE_IMAGE, BTEX_TYPE_MESH))
		return 0;

	offset += header->structbytes;
	header->structbytes= sizeof(BTexHeader);

	if(fseek(f, offset, SEEK_SET) != 0)
		return 0;
	
	if(header->type == BTEX_TYPE_IMAGE) {
		image= &btex->btype.image;
		if(!fread(image, sizeof(BTexImageHeader), 1, f))
			return 0;

		if(btex->switchendian) {
			SWITCH_INT(image->width);
			SWITCH_INT(image->height);
			SWITCH_INT(image->tile_size);
			SWITCH_INT(image->structbytes);
		}

		offset += image->structbytes;
		image->structbytes= sizeof(BTexImageHeader);
	}
	else if(header->type == BTEX_TYPE_MESH) {
		mesh= &btex->btype.mesh;
		if(!fread(mesh, sizeof(BTexMeshHeader), 1, f))
			return 0;

		if(btex->switchendian) {
			SWITCH_INT(mesh->totgrid);
			SWITCH_INT(mesh->gridsize);
			SWITCH_INT(mesh->structbytes);
		}

		offset += mesh->structbytes;
		mesh->structbytes= sizeof(BTexMeshHeader);
	}

	if(fseek(f, offset, SEEK_SET) != 0)
		return 0;

	btex->layer= MEM_callocN(sizeof(BTexLayer)*header->totlayer, "BTexLayer");
	btex->totlayer= header->totlayer;

	for(a=0; a<header->totlayer; a++) {
		layer= &btex->layer[a];

		if(!fread(layer, sizeof(BTexLayer), 1, f))
			return 0;

		if(btex->switchendian) {
			SWITCH_INT(layer->type);
			SWITCH_INT(layer->datatype);
			SWITCH_INT(layer->datasize);
			SWITCH_INT(layer->structbytes);
		}

		if(layer->datatype != BTEX_DATA_FLOAT)
			return 0;

		offset += layer->structbytes;
		layer->structbytes= sizeof(BTexLayer);

		if(fseek(f, offset, SEEK_SET) != 0)
			return 0;
	}

	btex->dataoffset= offset;

	return 1;
}

static int btex_write_header(BTex *btex)
{
	BTexHeader *header;
	BTexImageHeader *image;
	BTexMeshHeader *mesh;
	BTexLayer *layer;
	FILE *f= btex->writef;
	int a;

	header= &btex->header;

	if(!fwrite(header, sizeof(BTexHeader), 1, f))
		return 0;
	
	if(header->type == BTEX_TYPE_IMAGE) {
		image= &btex->btype.image;
		if(!fwrite(image, sizeof(BTexImageHeader), 1, f))
			return 0;
	}
	else if(header->type == BTEX_TYPE_MESH) {
		mesh= &btex->btype.mesh;
		if(!fwrite(mesh, sizeof(BTexMeshHeader), 1, f))
			return 0;
	}

	for(a=0; a<header->totlayer; a++) {
		layer= &btex->layer[a];

		if(!fwrite(layer, sizeof(BTexLayer), 1, f))
			return 0;
	}

	return 1;
}

int btex_read_open(BTex *btex, char *filename)
{
	FILE *f;

	f= fopen(filename, "rb");
	if(!f)
		return 0;
	
	btex->readf= f;

	if(!btex_read_header(btex)) {
		btex_read_close(btex);
		return 0;
	}

	if(btex->header.type != btex->type) {
		btex_read_close(btex);
		return 0;
	}

	return 1;
}

int btex_read_layer(BTex *btex, BTexLayer *blay)
{
	size_t offset;
	int a;

	/* seek to right location in file */
	offset= btex->dataoffset;
	for(a=0; a<btex->totlayer; a++) {
		if(&btex->layer[a] == blay)
			break;
		else
			offset += btex->layer[a].datasize;
	}

	return (fseek(btex->readf, offset, SEEK_SET) == 0);
}

int btex_read_data(BTex *btex, int size, void *data)
{
	float *fdata;
	int a;
	
	/* read data */
	if(!fread(data, size, 1, btex->readf))
		return 0;

	/* switch endian if necessary */
	if(btex->switchendian) {
		fdata= data;

		for(a=0; a<size/sizeof(float); a++)
			SWITCH_INT(fdata[a])
	}

	return 1;
}

void btex_read_close(BTex *btex)
{
	if(btex->readf) {
		fclose(btex->readf);
		btex->readf= NULL;
	}
}

int btex_write_open(BTex *btex, char *filename)
{
	BTexHeader *header;
	BTexImageHeader *image;
	BTexMeshHeader *mesh;
	FILE *f;

	f= fopen(filename, "wb");
	if(!f)
		return 0;
	
	btex->writef= f;

	/* fill header */
	header= &btex->header;
	strcpy(header->ID, "BTEX");
	header->endian= btex_endian();
	header->version= BTEX_VERSION;
	header->subversion= BTEX_SUBVERSION;

	header->structbytes= sizeof(BTexHeader);
	header->type= btex->type;
	header->totlayer= btex->totlayer;

	if(btex->type == BTEX_TYPE_IMAGE) {
		/* fill image header */
		image= &btex->btype.image;
		image->structbytes= sizeof(BTexImageHeader);
		image->tile_size= BTEX_TILE_SIZE;
	}
	else if(btex->type == BTEX_TYPE_MESH) {
		/* fill mesh header */
		mesh= &btex->btype.mesh;
		mesh->structbytes= sizeof(BTexMeshHeader);
	}

	btex_write_header(btex);

	return 1;
}

int btex_write_layer(BTex *btex, BTexLayer *blay)
{
	return 1;
}

int btex_write_data(BTex *btex, int size, void *data)
{
	/* write data */
	if(!fwrite(data, size, 1, btex->writef))
		return 0;

	return 1;
}

void btex_write_close(BTex *btex)
{
	if(btex->writef) {
		fclose(btex->writef);
		btex->writef= NULL;
	}
}

void btex_remove(char *filename)
{
	BLI_delete(filename, 0, 0);
}

/********************************** Layers ***********************************/

BTexLayer *btex_layer_find(BTex *btex, int type, char *name)
{
	BTexLayer *layer;
	int a;

	for(a=0; a<btex->totlayer; a++) {
		layer= &btex->layer[a];

		if(layer->type == type && strcmp(layer->name, name) == 0)
			return layer;
	}
	
	return NULL;
}

BTexLayer *btex_layer_add(BTex *btex, int type, char *name)
{
	BTexLayer *newlayer, *layer;

	/* expand array */
	newlayer= MEM_callocN(sizeof(BTexLayer)*(btex->totlayer+1), "BTexLayer");
	memcpy(newlayer, btex->layer, sizeof(BTexLayer)*btex->totlayer);
	btex->layer= newlayer;

	btex->totlayer++;

	/* fill in new layer */
	layer= &btex->layer[btex->totlayer-1];
	layer->structbytes= sizeof(BTexLayer);
	layer->datatype= BTEX_DATA_FLOAT;
	layer->type= type;
	BLI_strncpy(layer->name, name, BTEX_LAYER_NAME_MAX);

	return layer;
}

void btex_layer_remove(BTex *btex, BTexLayer *layer)
{
	BTexLayer *newlayer;
	int index= layer - btex->layer;

	/* expand array */
	newlayer= MEM_callocN(sizeof(BTexLayer)*(btex->totlayer-1), "BTexLayer");
	if(index > 0)
		memcpy(newlayer, btex->layer, sizeof(BTexLayer)*index);
	if(index+1 < btex->totlayer)
		memcpy(newlayer+index, btex->layer+index+1, sizeof(BTexLayer)*(btex->totlayer-(index+1)));
	btex->layer= newlayer;

	btex->totlayer--;
}

/********************************* Mesh **************************************/

void btex_mesh_set_grids(BTex *btex, int totgrid, int gridsize, int datasize)
{
	BTexLayer *layer;
	int a;

	btex->btype.mesh.totgrid= totgrid;
	btex->btype.mesh.gridsize= gridsize;

	for(a=0; a<btex->totlayer; a++) {
		layer= &btex->layer[a];
		layer->datasize= datasize;
	}
}

