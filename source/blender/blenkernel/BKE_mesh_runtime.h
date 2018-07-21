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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_MESH_RUNTIME_H__
#define __BKE_MESH_RUNTIME_H__

/** \file BKE_mesh_runtime.h
 *  \ingroup bke
 *
 * This file contains access functions for the Mesh.runtime struct.
 */

#include "BKE_customdata.h"  /* for CustomDataMask */

struct ColorBand;
struct CustomData;
struct Depsgraph;
struct KeyBlock;
struct Mesh;
struct MLoop;
struct MLoopTri;
struct MVertTri;
struct Object;
struct Scene;

/* Undefine to hide DerivedMesh-based function declarations */
#define USE_DERIVEDMESH

#ifdef USE_DERIVEDMESH
struct DerivedMesh;
#endif

void BKE_mesh_runtime_reset(struct Mesh *mesh);
int BKE_mesh_runtime_looptri_len(const struct Mesh *mesh);
void BKE_mesh_runtime_looptri_recalc(struct Mesh *mesh);
const struct MLoopTri *BKE_mesh_runtime_looptri_ensure(struct Mesh *mesh);
bool BKE_mesh_runtime_ensure_edit_data(struct Mesh *mesh);
bool BKE_mesh_runtime_clear_edit_data(struct Mesh *mesh);
void BKE_mesh_runtime_clear_geometry(struct Mesh *mesh);
void BKE_mesh_runtime_clear_cache(struct Mesh *mesh);

void BKE_mesh_runtime_verttri_from_looptri(
        struct MVertTri *r_verttri,
        const struct MLoop *mloop, const struct MLoopTri *looptri, int looptri_num);

/* NOTE: the functions below are defined in DerivedMesh.c, and are intended to be moved
 * to a more suitable location when that file is removed. */
#ifdef USE_DERIVEDMESH
struct DerivedMesh *mesh_get_derived_final(
        struct Depsgraph *depsgraph, struct Scene *scene,
        struct Object *ob, CustomDataMask dataMask);
#endif
struct Mesh *mesh_get_eval_final(
        struct Depsgraph *depsgraph, struct Scene *scene, struct Object *ob, CustomDataMask dataMask);

#ifdef USE_DERIVEDMESH
struct DerivedMesh *mesh_get_derived_deform(
        struct Depsgraph *depsgraph, struct Scene *scene,
        struct Object *ob, CustomDataMask dataMask);
#endif
struct Mesh *mesh_get_eval_deform(
        struct Depsgraph *depsgraph, struct Scene *scene,
        struct Object *ob, CustomDataMask dataMask);

#ifdef USE_DERIVEDMESH
struct DerivedMesh *mesh_create_derived_index_render(
        struct Depsgraph *depsgraph, struct Scene *scene,
        struct Object *ob, CustomDataMask dataMask, int index);
#endif
struct Mesh *mesh_create_eval_final_index_render(
        struct Depsgraph *depsgraph, struct Scene *scene,
        struct Object *ob, CustomDataMask dataMask, int index);

#ifdef USE_DERIVEDMESH
struct DerivedMesh *mesh_create_derived_view(
        struct Depsgraph *depsgraph, struct Scene *scene,
        struct Object *ob, CustomDataMask dataMask);
#endif
struct Mesh *mesh_create_eval_final_view(
        struct Depsgraph *depsgraph, struct Scene *scene,
        struct Object *ob, CustomDataMask dataMask);

void BKE_mesh_runtime_eval_to_meshkey(struct Mesh *me_deformed, struct Mesh *me, struct KeyBlock *kb);

/* Temporary? A function to give a colorband to derivedmesh for vertexcolor ranges */
void BKE_mesh_runtime_color_band_store(const struct ColorBand *coba, const char alert_color[4]);


#ifndef NDEBUG
char *BKE_mesh_runtime_debug_info(struct Mesh *me_eval);
void BKE_mesh_runtime_debug_print(struct Mesh *me_eval);
void BKE_mesh_runtime_debug_print_cdlayers(struct CustomData *data);
bool BKE_mesh_runtime_is_valid(struct Mesh *me_eval);
#endif  /* NDEBUG */

#endif /* __BKE_MESH_RUNTIME_H__ */
