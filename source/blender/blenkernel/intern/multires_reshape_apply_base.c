/*
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "multires_reshape.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math_vector.h"

#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_subdiv_eval.h"

void multires_reshape_apply_base_update_mesh_coords(MultiresReshapeContext *reshape_context)
{
  Mesh *base_mesh = reshape_context->base_mesh;
  const int grid_size = reshape_context->top.grid_size;
  const int grid_index = grid_size * grid_size - 1;
  for (int i = 0; i < base_mesh->totloop; ++i) {
    MDisps *displacement_grid = &reshape_context->mdisps[i];
    const MLoop *loop = &base_mesh->mloop[i];
    MVert *vert = &base_mesh->mvert[loop->v];
    copy_v3_v3(vert->co, displacement_grid->disps[grid_index]);
  }
}

/* Assumes no is normalized; return value's sign is negative if v is on the other side of the
 * plane. */
static float v3_dist_from_plane(float v[3], float center[3], float no[3])
{
  float s[3];
  sub_v3_v3v3(s, v, center);
  return dot_v3v3(s, no);
}

void multires_reshape_apply_base_refit_base_mesh(MultiresReshapeContext *reshape_context)
{
  Mesh *base_mesh = reshape_context->base_mesh;

  MeshElemMap *pmap;
  int *pmap_mem;
  BKE_mesh_vert_poly_map_create(&pmap,
                                &pmap_mem,
                                base_mesh->mpoly,
                                base_mesh->mloop,
                                base_mesh->totvert,
                                base_mesh->totpoly,
                                base_mesh->totloop);

  float(*origco)[3] = MEM_calloc_arrayN(
      base_mesh->totvert, 3 * sizeof(float), "multires apply base origco");
  for (int i = 0; i < base_mesh->totvert; i++) {
    copy_v3_v3(origco[i], base_mesh->mvert[i].co);
  }

  for (int i = 0; i < base_mesh->totvert; i++) {
    float avg_no[3] = {0, 0, 0}, center[3] = {0, 0, 0}, push[3];

    /* Don't adjust vertices not used by at least one poly. */
    if (!pmap[i].count) {
      continue;
    }

    /* Find center. */
    int tot = 0;
    for (int j = 0; j < pmap[i].count; j++) {
      const MPoly *p = &base_mesh->mpoly[pmap[i].indices[j]];

      /* This double counts, not sure if that's bad or good. */
      for (int k = 0; k < p->totloop; k++) {
        const int vndx = base_mesh->mloop[p->loopstart + k].v;
        if (vndx != i) {
          add_v3_v3(center, origco[vndx]);
          tot++;
        }
      }
    }
    mul_v3_fl(center, 1.0f / tot);

    /* Find normal. */
    for (int j = 0; j < pmap[i].count; j++) {
      const MPoly *p = &base_mesh->mpoly[pmap[i].indices[j]];
      MPoly fake_poly;
      MLoop *fake_loops;
      float(*fake_co)[3];
      float no[3];

      /* Set up poly, loops, and coords in order to call BKE_mesh_calc_poly_normal_coords(). */
      fake_poly.totloop = p->totloop;
      fake_poly.loopstart = 0;
      fake_loops = MEM_malloc_arrayN(p->totloop, sizeof(MLoop), "fake_loops");
      fake_co = MEM_malloc_arrayN(p->totloop, 3 * sizeof(float), "fake_co");

      for (int k = 0; k < p->totloop; k++) {
        const int vndx = base_mesh->mloop[p->loopstart + k].v;

        fake_loops[k].v = k;

        if (vndx == i) {
          copy_v3_v3(fake_co[k], center);
        }
        else {
          copy_v3_v3(fake_co[k], origco[vndx]);
        }
      }

      BKE_mesh_calc_poly_normal_coords(&fake_poly, fake_loops, (const float(*)[3])fake_co, no);
      MEM_freeN(fake_loops);
      MEM_freeN(fake_co);

      add_v3_v3(avg_no, no);
    }
    normalize_v3(avg_no);

    /* Push vertex away from the plane. */
    const float dist = v3_dist_from_plane(base_mesh->mvert[i].co, center, avg_no);
    copy_v3_v3(push, avg_no);
    mul_v3_fl(push, dist);
    add_v3_v3(base_mesh->mvert[i].co, push);
  }

  MEM_freeN(origco);
  MEM_freeN(pmap);
  MEM_freeN(pmap_mem);

  /* Vertices were moved around, need to update normals after all the vertices are updated
   * Probably this is possible to do in the loop above, but this is rather tricky because
   * we don't know all needed vertices' coordinates there yet. */
  BKE_mesh_calc_normals(base_mesh);
}

void multires_reshape_apply_base_refine_subdiv(MultiresReshapeContext *reshape_context)
{
  BKE_subdiv_eval_update_from_mesh(reshape_context->subdiv, reshape_context->base_mesh, NULL);
}
