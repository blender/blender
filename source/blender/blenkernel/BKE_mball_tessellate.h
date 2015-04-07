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
#ifndef __BKE_MBALL_TESSELLATE_H__
#define __BKE_MBALL_TESSELLATE_H__

/** \file BKE_mball_tessellate.h
 *  \ingroup bke
 */
struct EvaluationContext;
struct Object;
struct Scene;

void BKE_mball_polygonize(
        struct EvaluationContext *eval_ctx, struct Scene *scene,
        struct Object *ob, struct ListBase *dispbase);

void BKE_mball_cubeTable_free(void);

#endif  /* __BKE_MBALL_TESSELLATE_H__ */
