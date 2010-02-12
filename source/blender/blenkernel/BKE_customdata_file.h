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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_CUSTOMDATA_FILE_H
#define BKE_CUSTOMDATA_FILE_H

#define CDF_TYPE_IMAGE	0
#define CDF_TYPE_MESH	1

#define CDF_LAYER_NAME_MAX	64

typedef struct CDataFile CDataFile;
typedef struct CDataFileLayer CDataFileLayer;

/* Create/Free */

CDataFile *cdf_create(int type);
void cdf_free(CDataFile *cdf);

/* File read/write/remove */

int cdf_read_open(CDataFile *cdf, char *filename);
int cdf_read_layer(CDataFile *cdf, CDataFileLayer *blay);
int cdf_read_data(CDataFile *cdf, int size, void *data);
void cdf_read_close(CDataFile *cdf);

int cdf_write_open(CDataFile *cdf, char *filename);
int cdf_write_layer(CDataFile *cdf, CDataFileLayer *blay);
int cdf_write_data(CDataFile *cdf, int size, void *data);
void cdf_write_close(CDataFile *cdf);

void cdf_remove(char *filename);

/* Layers */

CDataFileLayer *cdf_layer_find(CDataFile *cdf, int type, char *name);
CDataFileLayer *cdf_layer_add(CDataFile *cdf, int type, char *name, size_t datasize);

#endif /* BKE_CUSTOMDATA_FILE_H */

