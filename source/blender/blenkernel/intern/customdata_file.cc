/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_endian_defines.h"
#include "BLI_endian_switch.h"
#include "BLI_fileops.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_customdata_file.h"

/************************* File Format Definitions ***************************/

#define CDF_ENDIAN_LITTLE 0
#define CDF_ENDIAN_BIG 1

#define CDF_DATA_FLOAT 0

struct CDataFileHeader {
  char ID[4];      /* "BCDF" */
  char endian;     /* little, big */
  char version;    /* non-compatible versions */
  char subversion; /* compatible sub versions */
  char pad;        /* padding */

  int structbytes; /* size of this struct in bytes */
  int type;        /* image, mesh */
  int totlayer;    /* number of layers in the file */
};

struct CDataFileImageHeader {
  int structbytes; /* size of this struct in bytes */
  int width;       /* image width */
  int height;      /* image height */
  int tile_size;   /* tile size (required power of 2) */
};

struct CDataFileMeshHeader {
  int structbytes; /* size of this struct in bytes */
};

struct CDataFileLayer {
  int structbytes;               /* size of this struct in bytes */
  int datatype;                  /* only float for now */
  uint64_t datasize;             /* size of data in layer */
  int type;                      /* layer type */
  char name[CDF_LAYER_NAME_MAX]; /* layer name */
};

/**************************** Other Definitions ******************************/

#define CDF_VERSION 0
#define CDF_SUBVERSION 0
#define CDF_TILE_SIZE 64

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

static int cdf_endian()
{
  if (ENDIAN_ORDER == L_ENDIAN) {
    return CDF_ENDIAN_LITTLE;
  }

  return CDF_ENDIAN_BIG;
}

CDataFile *cdf_create(int type)
{
  CDataFile *cdf = static_cast<CDataFile *>(MEM_callocN(sizeof(CDataFile), "CDataFile"));

  cdf->type = type;

  return cdf;
}

void cdf_free(CDataFile *cdf)
{
  cdf_read_close(cdf);
  cdf_write_close(cdf);

  if (cdf->layer) {
    MEM_freeN(cdf->layer);
  }

  MEM_freeN(cdf);
}

/********************************* Read/Write ********************************/

static bool cdf_read_header(CDataFile *cdf)
{
  CDataFileHeader *header;
  CDataFileImageHeader *image;
  CDataFileMeshHeader *mesh;
  CDataFileLayer *layer;
  FILE *f = cdf->readf;
  size_t offset = 0;
  int a;

  header = &cdf->header;

  if (!fread(header, sizeof(CDataFileHeader), 1, cdf->readf)) {
    return false;
  }

  if (memcmp(header->ID, "BCDF", sizeof(header->ID)) != 0) {
    return false;
  }
  if (header->version > CDF_VERSION) {
    return false;
  }

  cdf->switchendian = header->endian != cdf_endian();
  header->endian = cdf_endian();

  if (cdf->switchendian) {
    BLI_endian_switch_int32(&header->type);
    BLI_endian_switch_int32(&header->totlayer);
    BLI_endian_switch_int32(&header->structbytes);
  }

  if (!ELEM(header->type, CDF_TYPE_IMAGE, CDF_TYPE_MESH)) {
    return false;
  }

  offset += header->structbytes;
  header->structbytes = sizeof(CDataFileHeader);

  if (BLI_fseek(f, offset, SEEK_SET) != 0) {
    return false;
  }

  if (header->type == CDF_TYPE_IMAGE) {
    image = &cdf->btype.image;
    if (!fread(image, sizeof(CDataFileImageHeader), 1, f)) {
      return false;
    }

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
    if (!fread(mesh, sizeof(CDataFileMeshHeader), 1, f)) {
      return false;
    }

    if (cdf->switchendian) {
      BLI_endian_switch_int32(&mesh->structbytes);
    }

    offset += mesh->structbytes;
    mesh->structbytes = sizeof(CDataFileMeshHeader);
  }

  if (BLI_fseek(f, offset, SEEK_SET) != 0) {
    return false;
  }

  cdf->layer = static_cast<CDataFileLayer *>(
      MEM_calloc_arrayN(header->totlayer, sizeof(CDataFileLayer), "CDataFileLayer"));
  cdf->totlayer = header->totlayer;

  if (!cdf->layer) {
    return false;
  }

  for (a = 0; a < header->totlayer; a++) {
    layer = &cdf->layer[a];

    if (!fread(layer, sizeof(CDataFileLayer), 1, f)) {
      return false;
    }

    if (cdf->switchendian) {
      BLI_endian_switch_int32(&layer->type);
      BLI_endian_switch_int32(&layer->datatype);
      BLI_endian_switch_uint64(&layer->datasize);
      BLI_endian_switch_int32(&layer->structbytes);
    }

    if (layer->datatype != CDF_DATA_FLOAT) {
      return false;
    }

    offset += layer->structbytes;
    layer->structbytes = sizeof(CDataFileLayer);

    if (BLI_fseek(f, offset, SEEK_SET) != 0) {
      return false;
    }
  }

  cdf->dataoffset = offset;

  return true;
}

static bool cdf_write_header(CDataFile *cdf)
{
  CDataFileHeader *header;
  CDataFileImageHeader *image;
  CDataFileMeshHeader *mesh;
  CDataFileLayer *layer;
  FILE *f = cdf->writef;
  int a;

  header = &cdf->header;

  if (!fwrite(header, sizeof(CDataFileHeader), 1, f)) {
    return false;
  }

  if (header->type == CDF_TYPE_IMAGE) {
    image = &cdf->btype.image;
    if (!fwrite(image, sizeof(CDataFileImageHeader), 1, f)) {
      return false;
    }
  }
  else if (header->type == CDF_TYPE_MESH) {
    mesh = &cdf->btype.mesh;
    if (!fwrite(mesh, sizeof(CDataFileMeshHeader), 1, f)) {
      return false;
    }
  }

  for (a = 0; a < header->totlayer; a++) {
    layer = &cdf->layer[a];

    if (!fwrite(layer, sizeof(CDataFileLayer), 1, f)) {
      return false;
    }
  }

  return true;
}

bool cdf_read_open(CDataFile *cdf, const char *filepath)
{
  FILE *f;

  f = BLI_fopen(filepath, "rb");
  if (!f) {
    return false;
  }

  cdf->readf = f;

  if (!cdf_read_header(cdf)) {
    cdf_read_close(cdf);
    return false;
  }

  if (cdf->header.type != cdf->type) {
    cdf_read_close(cdf);
    return false;
  }

  return true;
}

bool cdf_read_layer(CDataFile *cdf, const CDataFileLayer *blay)
{
  size_t offset;
  int a;

  /* seek to right location in file */
  offset = cdf->dataoffset;
  for (a = 0; a < cdf->totlayer; a++) {
    if (&cdf->layer[a] == blay) {
      break;
    }

    offset += cdf->layer[a].datasize;
  }

  return (BLI_fseek(cdf->readf, offset, SEEK_SET) == 0);
}

bool cdf_read_data(CDataFile *cdf, uint size, void *data)
{
  /* read data */
  if (!fread(data, size, 1, cdf->readf)) {
    return false;
  }

  /* switch endian if necessary */
  if (cdf->switchendian) {
    BLI_endian_switch_float_array(static_cast<float *>(data), size / sizeof(float));
  }

  return true;
}

void cdf_read_close(CDataFile *cdf)
{
  if (cdf->readf) {
    fclose(cdf->readf);
    cdf->readf = nullptr;
  }
}

bool cdf_write_open(CDataFile *cdf, const char *filepath)
{
  CDataFileHeader *header;
  CDataFileImageHeader *image;
  CDataFileMeshHeader *mesh;
  FILE *f;

  f = BLI_fopen(filepath, "wb");
  if (!f) {
    return false;
  }

  cdf->writef = f;

  /* Fill header. */
  header = &cdf->header;
  /* Copy "BCDF" (string terminator out of range). */
  header->ID[0] = 'B';
  header->ID[1] = 'C';
  header->ID[2] = 'D';
  header->ID[3] = 'F';
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

  return true;
}

bool cdf_write_layer(CDataFile * /*cdf*/, CDataFileLayer * /*blay*/)
{
  return true;
}

bool cdf_write_data(CDataFile *cdf, uint size, const void *data)
{
  /* write data */
  if (!fwrite(data, size, 1, cdf->writef)) {
    return false;
  }

  return true;
}

void cdf_write_close(CDataFile *cdf)
{
  if (cdf->writef) {
    fclose(cdf->writef);
    cdf->writef = nullptr;
  }
}

void cdf_remove(const char *filepath)
{
  BLI_delete(filepath, false, false);
}

/********************************** Layers ***********************************/

CDataFileLayer *cdf_layer_find(CDataFile *cdf, int type, const char *name)
{
  CDataFileLayer *layer;
  int a;

  for (a = 0; a < cdf->totlayer; a++) {
    layer = &cdf->layer[a];

    if (layer->type == type && STREQ(layer->name, name)) {
      return layer;
    }
  }

  return nullptr;
}

CDataFileLayer *cdf_layer_add(CDataFile *cdf, int type, const char *name, size_t datasize)
{
  CDataFileLayer *newlayer, *layer;

  /* expand array */
  newlayer = static_cast<CDataFileLayer *>(
      MEM_calloc_arrayN((cdf->totlayer + 1), sizeof(CDataFileLayer), "CDataFileLayer"));
  if (cdf->totlayer > 0) {
    memcpy(newlayer, cdf->layer, sizeof(CDataFileLayer) * cdf->totlayer);
  }
  cdf->layer = newlayer;

  cdf->totlayer++;

  /* fill in new layer */
  layer = &cdf->layer[cdf->totlayer - 1];
  layer->structbytes = sizeof(CDataFileLayer);
  layer->datatype = CDF_DATA_FLOAT;
  layer->datasize = datasize;
  layer->type = type;
  STRNCPY(layer->name, name);

  return layer;
}
