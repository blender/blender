/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

#define CDF_TYPE_IMAGE 0
#define CDF_TYPE_MESH 1

#define CDF_LAYER_NAME_MAX 64

typedef struct CDataFile CDataFile;
typedef struct CDataFileLayer CDataFileLayer;

/* Create/Free */

CDataFile *cdf_create(int type);
void cdf_free(CDataFile *cdf);

/* File read/write/remove */

bool cdf_read_open(CDataFile *cdf, const char *filepath);
bool cdf_read_layer(CDataFile *cdf, CDataFileLayer *blay);
bool cdf_read_data(CDataFile *cdf, unsigned int size, void *data);
void cdf_read_close(CDataFile *cdf);

bool cdf_write_open(CDataFile *cdf, const char *filepath);
bool cdf_write_layer(CDataFile *cdf, CDataFileLayer *blay);
bool cdf_write_data(CDataFile *cdf, unsigned int size, void *data);
void cdf_write_close(CDataFile *cdf);

void cdf_remove(const char *filepath);

/* Layers */

CDataFileLayer *cdf_layer_find(CDataFile *cdf, int type, const char *name);
CDataFileLayer *cdf_layer_add(CDataFile *cdf, int type, const char *name, size_t datasize);

#ifdef __cplusplus
}
#endif
