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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/customdata_file.c
 *  \ingroup bke
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_fileops.h"
#include "BLI_string.h"
#include "BLI_endian_switch.h"

#include "BKE_customdata_file.h"
#include "BKE_global.h"


/************************* File Format Definitions ***************************/

#define CDF_ENDIAN_LITTLE   0
#define CDF_ENDIAN_BIG      1

#define CDF_DATA_FLOAT  0

typedef struct CDataFileHeader {
	char ID[4];                 /* "BCDF" */
	char endian;                /* little, big */
	char version;               /* non-compatible versions */
	char subversion;            /* compatible sub versions */
	char pad;                   /* padding */

	int structbytes;            /* size of this struct in bytes */
	int type;                   /* image, mesh */
	int totlayer;               /* number of layers in the file */
} CDataFileHeader;

typedef struct CDataFileImageHeader {
	int structbytes;            /* size of this struct in bytes */
	int width;                  /* image width */
	int height;                 /* image height */
	int tile_size;              /* tile size (required power of 2) */
} CDataFileImageHeader;

typedef struct CDataFileMeshHeader {
	int structbytes;            /* size of this struct in bytes */
} CDataFileMeshHeader;

struct CDataFileLayer {
	int structbytes;                /* size of this struct in bytes */
	int datatype;                   /* only float for now */
	uint64_t datasize;              /* size of data in layer */
	int type;                       /* layer type */
	char name[CDF_LAYER_NAME_MAX];  /* layer name */
};

/**************************** Other Definitions ******************************/

#define CDF_VERSION         0
#define CDF_SUBVERSION      0
#define CDF_TILE_SIZE       64

struct CDataFile {
	int type;

	CDataFileHeader header;
	union {
		CDataFileImageHeader image;
		CDataFileMeshHeader mesh;
	} btype;

	CDataFileLayer *layer;
	int totlayer;

	FILE *readf;
	FILE *writef;
	int switchendian;
	size_t dataoffset;
};

/********************************* Create/Free *******************************/

static int cdf_endian(void)
{
	if (ENDIAN_ORDER == L_ENDIAN)
		return CDF_ENDIAN_LITTLE;
	else
		return CDF_ENDIAN_BIG;
}

#if 0
static int cdf_data_type_size(int datatype)
{
	if (datatype == CDF_DATA_FLOAT)
		return sizeof(float);
	
	return 0;
}
#endif

CDataFile *cdf_create(int type)
{
	CDataFile *cdf = MEM_callocN(sizeof(CDataFile), "CDataFile");

	cdf->type = type;

	return cdf;
}

void cdf_free(CDataFile *cdf)
{
	cdf_read_close(cdf);
	cdf_write_close(cdf);

	if (cdf->layer)
		MEM_freeN(cdf->layer);

	MEM_freeN(cdf);
}

/********************************* Read/Write ********************************/

static int cdf_read_header(CDataFile *cdf)
{
	CDataFileHeader *header;
	CDataFileImageHeader *image;
	CDataFileMeshHeader *mesh;
	CDataFileLayer *layer;
	FILE *f = cdf->readf;
	size_t offset = 0;
	int a;

	header = &cdf->header;

	if (!fread(header, sizeof(CDataFileHeader), 1, cdf->readf))
		return 0;
	
	if (memcmp(header->ID, "BCDF", sizeof(header->ID)) != 0)
		return 0;
	if (header->version > CDF_VERSION)
		return 0;

	cdf->switchendian = header->endian != cdf_endian();
	header->endian = cdf_endian();

	if (cdf->switchendian) {
		BLI_endian_switch_int32(&header->type);
		BLI_endian_switch_int32(&header->totlayer);
		BLI_endian_switch_int32(&header->structbytes);
	}

	if (!ELEM(header->type, CDF_TYPE_IMAGE, CDF_TYPE_MESH))
		return 0;

	offset += header->structbytes;
	header->structbytes = sizeof(CDataFileHeader);

	if (fseek(f, offset, SEEK_SET) != 0)
		return 0;
	
	if (header->type == CDF_TYPE_IMAGE) {
		image = &cdf->btype.image;
		if (!fread(image, sizeof(CDataFileImageHeader), 1, f))
			return 0;

		if (cdf->switchendian) {
			BLI_endian_switch_int32(&image->width);
			BLI_endian_switch_int32(&image->height);
			BLI_endian_switch_int32(&image->tile_size);
			BLI_endian_switch_int32(&image->structbytes);
		}

		offset += image->structbytes;
		image->structbytes = sizeof(CDataFileImageHeader);
	}
	else if (header->type == CDF_TYPE_MESH) {
		mesh = &cdf->btype.mesh;
		if (!fread(mesh, sizeof(CDataFileMeshHeader), 1, f))
			return 0;

		if (cdf->switchendian)
			BLI_endian_switch_int32(&mesh->structbytes);

		offset += mesh->structbytes;
		mesh->structbytes = sizeof(CDataFileMeshHeader);
	}

	if (fseek(f, offset, SEEK_SET) != 0)
		return 0;

	cdf->layer = MEM_callocN(sizeof(CDataFileLayer) * header->totlayer, "CDataFileLayer");
	cdf->totlayer = header->totlayer;

	for (a = 0; a < header->totlayer; a++) {
		layer = &cdf->layer[a];

		if (!fread(layer, sizeof(CDataFileLayer), 1, f))
			return 0;

		if (cdf->switchendian) {
			BLI_endian_switch_int32(&layer->type);
			BLI_endian_switch_int32(&layer->datatype);
			BLI_endian_switch_uint64(&layer->datasize);
			BLI_endian_switch_int32(&layer->structbytes);
		}

		if (layer->datatype != CDF_DATA_FLOAT)
			return 0;

		offset += layer->structbytes;
		layer->structbytes = sizeof(CDataFileLayer);

		if (fseek(f, offset, SEEK_SET) != 0)
			return 0;
	}

	cdf->dataoffset = offset;

	return 1;
}

static int cdf_write_header(CDataFile *cdf)
{
	CDataFileHeader *header;
	CDataFileImageHeader *image;
	CDataFileMeshHeader *mesh;
	CDataFileLayer *layer;
	FILE *f = cdf->writef;
	int a;

	header = &cdf->header;

	if (!fwrite(header, sizeof(CDataFileHeader), 1, f))
		return 0;
	
	if (header->type == CDF_TYPE_IMAGE) {
		image = &cdf->btype.image;
		if (!fwrite(image, sizeof(CDataFileImageHeader), 1, f))
			return 0;
	}
	else if (header->type == CDF_TYPE_MESH) {
		mesh = &cdf->btype.mesh;
		if (!fwrite(mesh, sizeof(CDataFileMeshHeader), 1, f))
			return 0;
	}

	for (a = 0; a < header->totlayer; a++) {
		layer = &cdf->layer[a];

		if (!fwrite(layer, sizeof(CDataFileLayer), 1, f))
			return 0;
	}

	return 1;
}

int cdf_read_open(CDataFile *cdf, const char *filename)
{
	FILE *f;

	f = BLI_fopen(filename, "rb");
	if (!f)
		return 0;
	
	cdf->readf = f;

	if (!cdf_read_header(cdf)) {
		cdf_read_close(cdf);
		return 0;
	}

	if (cdf->header.type != cdf->type) {
		cdf_read_close(cdf);
		return 0;
	}

	return 1;
}

int cdf_read_layer(CDataFile *cdf, CDataFileLayer *blay)
{
	size_t offset;
	int a;

	/* seek to right location in file */
	offset = cdf->dataoffset;
	for (a = 0; a < cdf->totlayer; a++) {
		if (&cdf->layer[a] == blay)
			break;
		else
			offset += cdf->layer[a].datasize;
	}

	return (fseek(cdf->readf, offset, SEEK_SET) == 0);
}

int cdf_read_data(CDataFile *cdf, unsigned int size, void *data)
{
	/* read data */
	if (!fread(data, size, 1, cdf->readf))
		return 0;

	/* switch endian if necessary */
	if (cdf->switchendian) {
		BLI_endian_switch_float_array(data, size / sizeof(float));
	}

	return 1;
}

void cdf_read_close(CDataFile *cdf)
{
	if (cdf->readf) {
		fclose(cdf->readf);
		cdf->readf = NULL;
	}
}

int cdf_write_open(CDataFile *cdf, const char *filename)
{
	CDataFileHeader *header;
	CDataFileImageHeader *image;
	CDataFileMeshHeader *mesh;
	FILE *f;

	f = BLI_fopen(filename, "wb");
	if (!f)
		return 0;
	
	cdf->writef = f;

	/* fill header */
	header = &cdf->header;
	/* strcpy(, "BCDF"); // terminator out of range */
	header->ID[0] = 'B'; header->ID[1] = 'C'; header->ID[2] = 'D'; header->ID[3] = 'F';
	header->endian = cdf_endian();
	header->version = CDF_VERSION;
	header->subversion = CDF_SUBVERSION;

	header->structbytes = sizeof(CDataFileHeader);
	header->type = cdf->type;
	header->totlayer = cdf->totlayer;

	if (cdf->type == CDF_TYPE_IMAGE) {
		/* fill image header */
		image = &cdf->btype.image;
		image->structbytes = sizeof(CDataFileImageHeader);
		image->tile_size = CDF_TILE_SIZE;
	}
	else if (cdf->type == CDF_TYPE_MESH) {
		/* fill mesh header */
		mesh = &cdf->btype.mesh;
		mesh->structbytes = sizeof(CDataFileMeshHeader);
	}

	cdf_write_header(cdf);

	return 1;
}

int cdf_write_layer(CDataFile *UNUSED(cdf), CDataFileLayer *UNUSED(blay))
{
	return 1;
}

int cdf_write_data(CDataFile *cdf, unsigned int size, void *data)
{
	/* write data */
	if (!fwrite(data, size, 1, cdf->writef))
		return 0;

	return 1;
}

void cdf_write_close(CDataFile *cdf)
{
	if (cdf->writef) {
		fclose(cdf->writef);
		cdf->writef = NULL;
	}
}

void cdf_remove(const char *filename)
{
	BLI_delete(filename, false, false);
}

/********************************** Layers ***********************************/

CDataFileLayer *cdf_layer_find(CDataFile *cdf, int type, const char *name)
{
	CDataFileLayer *layer;
	int a;

	for (a = 0; a < cdf->totlayer; a++) {
		layer = &cdf->layer[a];

		if (layer->type == type && strcmp(layer->name, name) == 0)
			return layer;
	}
	
	return NULL;
}

CDataFileLayer *cdf_layer_add(CDataFile *cdf, int type, const char *name, size_t datasize)
{
	CDataFileLayer *newlayer, *layer;

	/* expand array */
	newlayer = MEM_callocN(sizeof(CDataFileLayer) * (cdf->totlayer + 1), "CDataFileLayer");
	memcpy(newlayer, cdf->layer, sizeof(CDataFileLayer) * cdf->totlayer);
	cdf->layer = newlayer;

	cdf->totlayer++;

	/* fill in new layer */
	layer = &cdf->layer[cdf->totlayer - 1];
	layer->structbytes = sizeof(CDataFileLayer);
	layer->datatype = CDF_DATA_FLOAT;
	layer->datasize = datasize;
	layer->type = type;
	BLI_strncpy(layer->name, name, CDF_LAYER_NAME_MAX);

	return layer;
}

