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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Stubs to reduce linking time for shader_builder.
 */

#include "BLI_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_subdiv_ccg.h"

#include "DNA_userdef_types.h"

#include "DRW_engine.h"

#include "bmesh.h"

#include "UI_resources.h"

extern "C" {

Global G;
UserDef U;

/* -------------------------------------------------------------------- */
/** \name Stubs of BLI_imbuf_types.h
 * \{ */

void IMB_freeImBuf(ImBuf *UNUSED(ibuf))
{
  BLI_assert_unreachable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of UI_resources.h
 * \{ */

void UI_GetThemeColor4fv(int UNUSED(colorid), float UNUSED(col[4]))
{
  BLI_assert_unreachable();
}

void UI_GetThemeColor3fv(int UNUSED(colorid), float UNUSED(col[3]))
{
  BLI_assert_unreachable();
}

void UI_GetThemeColorShade4fv(int UNUSED(colorid), int UNUSED(offset), float UNUSED(col[4]))
{
  BLI_assert_unreachable();
}

void UI_GetThemeColorShadeAlpha4fv(int UNUSED(colorid),
                                   int UNUSED(coloffset),
                                   int UNUSED(alphaoffset),
                                   float UNUSED(col[4]))
{
  BLI_assert_unreachable();
}
void UI_GetThemeColorBlendShade4fv(int UNUSED(colorid1),
                                   int UNUSED(colorid2),
                                   float UNUSED(fac),
                                   int UNUSED(offset),
                                   float UNUSED(col[4]))
{
  BLI_assert_unreachable();
}

void UI_GetThemeColorBlend3ubv(int UNUSED(colorid1),
                               int UNUSED(colorid2),
                               float UNUSED(fac),
                               unsigned char UNUSED(col[3]))
{
  BLI_assert_unreachable();
}

void UI_GetThemeColorShadeAlpha4ubv(int UNUSED(colorid),
                                    int UNUSED(coloffset),
                                    int UNUSED(alphaoffset),
                                    unsigned char UNUSED(col[4]))
{
  BLI_assert_unreachable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_paint.h
 * \{ */
bool paint_is_face_hidden(const struct MLoopTri *UNUSED(lt),
                          const struct MVert *UNUSED(mvert),
                          const struct MLoop *UNUSED(mloop))
{
  BLI_assert_unreachable();
  return false;
}

void BKE_paint_face_set_overlay_color_get(const int UNUSED(face_set),
                                          const int UNUSED(seed),
                                          uchar UNUSED(r_color[4]))
{
  BLI_assert_unreachable();
}

bool paint_is_grid_face_hidden(const unsigned int *UNUSED(grid_hidden),
                               int UNUSED(gridsize),
                               int UNUSED(x),
                               int UNUSED(y))
{
  BLI_assert_unreachable();
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_mesh.h
 * \{ */
void BKE_mesh_calc_poly_normal(const struct MPoly *UNUSED(mpoly),
                               const struct MLoop *UNUSED(loopstart),
                               const struct MVert *UNUSED(mvarray),
                               float UNUSED(r_no[3]))
{
  BLI_assert_unreachable();
}

void BKE_mesh_looptri_get_real_edges(const struct Mesh *UNUSED(mesh),
                                     const struct MLoopTri *UNUSED(looptri),
                                     int UNUSED(r_edges[3]))
{
  BLI_assert_unreachable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_material.h
 * \{ */

void BKE_material_defaults_free_gpu(void)
{
  /* This function is reachable via GPU_exit. */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_customdata.h
 * \{ */

int CustomData_get_offset(const struct CustomData *UNUSED(data), int UNUSED(type))
{
  BLI_assert_unreachable();
  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_pbvh.h
 * \{ */

int BKE_pbvh_count_grid_quads(BLI_bitmap **UNUSED(grid_hidden),
                              const int *UNUSED(grid_indices),
                              int UNUSED(totgrid),
                              int UNUSED(gridsize))
{
  BLI_assert_unreachable();
  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_subdiv_ccg.h
 * \{ */
int BKE_subdiv_ccg_grid_to_face_index(const SubdivCCG *UNUSED(subdiv_ccg),
                                      const int UNUSED(grid_index))
{
  BLI_assert_unreachable();
  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of BKE_node.h
 * \{ */
void ntreeGPUMaterialNodes(struct bNodeTree *UNUSED(localtree),
                           struct GPUMaterial *UNUSED(mat),
                           bool *UNUSED(has_surface_output),
                           bool *UNUSED(has_volume_output))
{
  BLI_assert_unreachable();
}

struct bNodeTree *ntreeLocalize(struct bNodeTree *UNUSED(ntree))
{
  BLI_assert_unreachable();
  return nullptr;
}

void ntreeFreeLocalTree(struct bNodeTree *UNUSED(ntree))
{
  BLI_assert_unreachable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of bmesh.h
 * \{ */
void BM_face_as_array_vert_tri(BMFace *UNUSED(f), BMVert *UNUSED(r_verts[3]))
{
  BLI_assert_unreachable();
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Stubs of DRW_engine.h
 * \{ */
void DRW_deferred_shader_remove(struct GPUMaterial *UNUSED(mat))
{
  BLI_assert_unreachable();
}

/** \} */
}
