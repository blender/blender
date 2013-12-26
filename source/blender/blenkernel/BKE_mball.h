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
struct EvaluationContext;
struct Main;
struct MetaBall;
struct Object;
struct Scene;
struct MetaElem;

void BKE_mball_unlink(struct MetaBall *mb);
void BKE_mball_free(struct MetaBall *mb);
struct MetaBall *BKE_mball_add(struct Main *bmain, const char *name);
struct MetaBall *BKE_mball_copy(struct MetaBall *mb);

void BKE_mball_make_local(struct MetaBall *mb);

void BKE_mball_cubeTable_free(void);

void BKE_mball_polygonize(struct EvaluationContext *eval_ctx, struct Scene *scene, struct Object *ob, struct ListBase *dispbase);
bool BKE_mball_is_basis_for(struct Object *ob1, struct Object *ob2);
bool BKE_mball_is_basis(struct Object *ob);
struct Object *BKE_mball_basis_find(struct Scene *scene, struct Object *ob);

void BKE_mball_texspace_calc(struct Object *ob);
float *BKE_mball_make_orco(struct Object *ob, struct ListBase *dispbase);

void BKE_mball_properties_copy(struct Scene *scene, struct Object *active_object);

bool BKE_mball_minmax(struct MetaBall *mb, float min[3], float max[3]);
bool BKE_mball_minmax_ex(struct MetaBall *mb, float min[3], float max[3],
                         float obmat[4][4], const short flag);
bool BKE_mball_center_median(struct MetaBall *mb, float r_cent[3]);
bool BKE_mball_center_bounds(struct MetaBall *mb, float r_cent[3]);
void BKE_mball_translate(struct MetaBall *mb, const float offset[3]);

struct MetaElem *BKE_mball_element_add(struct MetaBall *mb, const int type);

void BKE_mball_select_all(struct MetaBall *mb);
void BKE_mball_deselect_all(struct MetaBall *mb);
void BKE_mball_select_swap(struct MetaBall *mb);

#endif
