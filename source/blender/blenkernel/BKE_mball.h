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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_MBALL_H__
#define __BKE_MBALL_H__

/** \file BKE_mball.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */
struct MetaBall;
struct Object;
struct Scene;
struct MetaElem;

void BKE_metaball_unlink(struct MetaBall *mb);
void BKE_metaball_free(struct MetaBall *mb);
struct MetaBall *BKE_metaball_add(const char *name);
struct MetaBall *BKE_metaball_copy(struct MetaBall *mb);

void BKE_metaball_make_local(struct MetaBall *mb);

void BKE_metaball_cubeTable_free(void);

void BKE_metaball_polygonize(struct Scene *scene, struct Object *ob, struct ListBase *dispbase);
int BKE_metaball_is_basis_for(struct Object *ob1, struct Object *ob2);
int BKE_metaball_is_basis(struct Object *ob);
struct Object *BKE_metaball_basis_find(struct Scene *scene, struct Object *ob);

void BKE_metaball_tex_space_calc(struct Object *ob);
float *BKE_metaball_make_orco(struct Object *ob, struct ListBase *dispbase);

void BKE_metaball_properties_copy(struct Scene *scene, struct Object *active_object);

int BKE_metaball_minmax(struct MetaBall *mb, float min[3], float max[3]);
int BKE_metaball_center_median(struct MetaBall *mb, float cent[3]);
int BKE_metaball_center_bounds(struct MetaBall *mb, float cent[3]);
void BKE_metaball_translate(struct MetaBall *mb, float offset[3]);

struct MetaElem *BKE_metaball_element_add(struct MetaBall *mb, const int type);

#endif
