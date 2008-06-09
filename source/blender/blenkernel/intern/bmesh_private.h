/**
 * BME_private.h    jan 2007
 *
 *	low level, 'private' function prototypes for bmesh kernel.
 *
 * $Id: BKE_bmesh.h,v 1.00 2007/01/17 17:42:01 Briggs Exp $
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BMESH_PRIVATE
#define BMESH_PRIVATE

#include "BKE_bmesh.h"

struct BME_mempool *BME_mempool_create(int esize, int tote, int pchunk);
void BME_mempool_destroy(struct BME_mempool *pool);
void *BME_mempool_alloc(struct BME_mempool *pool);
void BME_mempool_free(struct BME_mempool *pool, void *address);

/*ALLOCATION/DEALLOCATION*/
struct BME_Vert *BME_addvertlist(struct BME_Mesh *bm, struct BME_Vert *example);
struct BME_Edge *BME_addedgelist(struct BME_Mesh *bm, struct BME_Vert *v1, struct BME_Vert *v2, struct BME_Edge *example);
struct BME_Poly *BME_addpolylist(struct BME_Mesh *bm, struct BME_Poly *example); 
struct BME_Loop *BME_create_loop(struct BME_Mesh *bm, struct BME_Vert *v, struct BME_Edge *e, struct BME_Poly *f, struct BME_Loop *example);

void BME_free_vert(struct BME_Mesh *bm, struct BME_Vert *v);
void BME_free_edge(struct BME_Mesh *bm, struct BME_Edge *e);
void BME_free_poly(struct BME_Mesh *bm, struct BME_Poly *f);
void BME_free_loop(struct BME_Mesh *bm, struct BME_Loop *l);
//void BME_delete_loop(struct BME_Mesh *bm, struct BME_Loop *l);

/*DOUBLE CIRCULAR LINKED LIST FUNCTIONS*/
void BME_cycle_append(void *h, void *nt);
int BME_cycle_remove(void *h, void *remn);
int BME_cycle_validate(int len, void *h);
/*DISK CYCLE MANAGMENT*/
int BME_disk_append_edge(struct BME_Edge *e, struct BME_Vert *v);
void BME_disk_remove_edge(struct BME_Edge *e, struct BME_Vert *v);
/*RADIAL CYCLE MANAGMENT*/
void BME_radial_append(struct BME_Edge *e, struct BME_Loop *l);
void BME_radial_remove_loop(struct BME_Loop *l, struct BME_Edge *e);

/*MISC FUNCTIONS*/
int BME_edge_swapverts(struct BME_Edge *e, struct BME_Vert *orig, struct BME_Vert *new); /*relink edge*/
int BME_disk_hasedge(struct BME_Vert *v, struct BME_Edge *e);

/*Error reporting. Shouldnt be called by tools ever.*/
void BME_error(void);
#endif
