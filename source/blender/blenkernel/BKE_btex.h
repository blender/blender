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

#ifndef BKE_BTEX_H
#define BKE_BTEX_H

#define BTEX_TYPE_IMAGE	0
#define BTEX_TYPE_MESH	1

#define BTEX_LAYER_NAME_MAX	64

typedef struct BTex BTex;
typedef struct BTexLayer BTexLayer;

/* Create/Free */

BTex *btex_create(int type);
void btex_free(BTex *btex);

/* File read/write/remove */

int btex_read_open(BTex *btex, char *filename);
int btex_read_layer(BTex *btex, BTexLayer *blay);
int btex_read_data(BTex *btex, int size, void *data);
void btex_read_close(BTex *btex);

int btex_write_open(BTex *btex, char *filename);
int btex_write_layer(BTex *btex, BTexLayer *blay);
int btex_write_data(BTex *btex, int size, void *data);
void btex_write_close(BTex *btex);

void btex_remove(char *filename);

/* Layers */

BTexLayer *btex_layer_find(BTex *btex, int type, char *name);
BTexLayer *btex_layer_add(BTex *btex, int type, char *name);
void btex_layer_remove(BTex *btex, BTexLayer *blay);

/* Mesh */

void btex_mesh_set_grids(BTex *btex, int totgrid, int gridsize, int datasize);

#endif /* BKE_BTEX_H */

