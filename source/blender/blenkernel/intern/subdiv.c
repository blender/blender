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
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "BKE_subdiv.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "subdiv_converter.h"

#include "opensubdiv_capi.h"
#include "opensubdiv_converter_capi.h"
#include "opensubdiv_evaluator_capi.h"
#include "opensubdiv_topology_refiner_capi.h"

/* =================----====--===== MODULE ==========================------== */

void BKE_subdiv_init()
{
  openSubdiv_init();
}

void BKE_subdiv_exit()
{
  openSubdiv_cleanup();
}

/* ========================== CONVERSION HELPERS ============================ */

eSubdivFVarLinearInterpolation BKE_subdiv_fvar_interpolation_from_uv_smooth(int uv_smooth)
{
  switch (uv_smooth) {
    case SUBSURF_UV_SMOOTH_NONE:
      return SUBDIV_FVAR_LINEAR_INTERPOLATION_ALL;
    case SUBSURF_UV_SMOOTH_PRESERVE_CORNERS:
      return SUBDIV_FVAR_LINEAR_INTERPOLATION_CORNERS_ONLY;
    case SUBSURF_UV_SMOOTH_PRESERVE_CORNERS_AND_JUNCTIONS:
      return SUBDIV_FVAR_LINEAR_INTERPOLATION_CORNERS_AND_JUNCTIONS;
    case SUBSURF_UV_SMOOTH_PRESERVE_CORNERS_JUNCTIONS_AND_CONCAVE:
      return SUBDIV_FVAR_LINEAR_INTERPOLATION_CORNERS_JUNCTIONS_AND_CONCAVE;
    case SUBSURF_UV_SMOOTH_PRESERVE_BOUNDARIES:
      return SUBDIV_FVAR_LINEAR_INTERPOLATION_BOUNDARIES;
    case SUBSURF_UV_SMOOTH_ALL:
      return SUBDIV_FVAR_LINEAR_INTERPOLATION_NONE;
  }
  BLI_assert(!"Unknown uv smooth flag");
  return SUBDIV_FVAR_LINEAR_INTERPOLATION_ALL;
}

/* ================================ SETTINGS ================================ */

static bool check_mesh_has_non_quad(const Mesh *mesh)
{
  for (int poly_index = 0; poly_index < mesh->totpoly; poly_index++) {
    const MPoly *poly = &mesh->mpoly[poly_index];
    if (poly->totloop != 4) {
      return true;
    }
  }
  return false;
}

void BKE_subdiv_settings_validate_for_mesh(SubdivSettings *settings, const Mesh *mesh)
{
  if (settings->level != 1) {
    return;
  }
  if (check_mesh_has_non_quad(mesh)) {
    settings->level = 2;
  }
}

bool BKE_subdiv_settings_equal(const SubdivSettings *settings_a, const SubdivSettings *settings_b)
{
  return (settings_a->is_simple == settings_b->is_simple &&
          settings_a->is_adaptive == settings_b->is_adaptive &&
          settings_a->level == settings_b->level &&
          settings_a->vtx_boundary_interpolation == settings_b->vtx_boundary_interpolation &&
          settings_a->fvar_linear_interpolation == settings_b->fvar_linear_interpolation);
}

/* ============================== CONSTRUCTION ============================== */

/* Creation from scratch. */

Subdiv *BKE_subdiv_new_from_converter(const SubdivSettings *settings,
                                      struct OpenSubdiv_Converter *converter)
{
  SubdivStats stats;
  BKE_subdiv_stats_init(&stats);
  BKE_subdiv_stats_begin(&stats, SUBDIV_STATS_TOPOLOGY_REFINER_CREATION_TIME);
  OpenSubdiv_TopologyRefinerSettings topology_refiner_settings;
  topology_refiner_settings.level = settings->level;
  topology_refiner_settings.is_adaptive = settings->is_adaptive;
  struct OpenSubdiv_TopologyRefiner *osd_topology_refiner = NULL;
  if (converter->getNumVertices(converter) != 0) {
    osd_topology_refiner = openSubdiv_createTopologyRefinerFromConverter(
        converter, &topology_refiner_settings);
  }
  else {
    /* TODO(sergey): Check whether original geometry had any vertices.
     * The thing here is: OpenSubdiv can only deal with faces, but our
     * side of subdiv also deals with loose vertices and edges. */
  }
  Subdiv *subdiv = MEM_callocN(sizeof(Subdiv), "subdiv from converetr");
  subdiv->settings = *settings;
  subdiv->topology_refiner = osd_topology_refiner;
  subdiv->evaluator = NULL;
  subdiv->displacement_evaluator = NULL;
  BKE_subdiv_stats_end(&stats, SUBDIV_STATS_TOPOLOGY_REFINER_CREATION_TIME);
  subdiv->stats = stats;
  return subdiv;
}

Subdiv *BKE_subdiv_new_from_mesh(const SubdivSettings *settings, const Mesh *mesh)
{
  if (mesh->totvert == 0) {
    return NULL;
  }
  OpenSubdiv_Converter converter;
  BKE_subdiv_converter_init_for_mesh(&converter, settings, mesh);
  Subdiv *subdiv = BKE_subdiv_new_from_converter(settings, &converter);
  BKE_subdiv_converter_free(&converter);
  return subdiv;
}

/* Creation with cached-aware semantic. */

Subdiv *BKE_subdiv_update_from_converter(Subdiv *subdiv,
                                         const SubdivSettings *settings,
                                         OpenSubdiv_Converter *converter)
{
  /* Check if the existing descriptor can be re-used. */
  bool can_reuse_subdiv = true;
  if (subdiv != NULL && subdiv->topology_refiner != NULL) {
    if (!BKE_subdiv_settings_equal(&subdiv->settings, settings)) {
      can_reuse_subdiv = false;
    }
    else {
      BKE_subdiv_stats_begin(&subdiv->stats, SUBDIV_STATS_TOPOLOGY_COMPARE);
      can_reuse_subdiv = openSubdiv_topologyRefinerCompareWithConverter(subdiv->topology_refiner,
                                                                        converter);
      BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_TOPOLOGY_COMPARE);
    }
  }
  else {
    can_reuse_subdiv = false;
  }
  if (can_reuse_subdiv) {
    return subdiv;
  }
  /* Create new subdiv. */
  if (subdiv != NULL) {
    BKE_subdiv_free(subdiv);
  }
  return BKE_subdiv_new_from_converter(settings, converter);
}

Subdiv *BKE_subdiv_update_from_mesh(Subdiv *subdiv,
                                    const SubdivSettings *settings,
                                    const Mesh *mesh)
{
  OpenSubdiv_Converter converter;
  BKE_subdiv_converter_init_for_mesh(&converter, settings, mesh);
  subdiv = BKE_subdiv_update_from_converter(subdiv, settings, &converter);
  BKE_subdiv_converter_free(&converter);
  return subdiv;
}

/* Memory release. */

void BKE_subdiv_free(Subdiv *subdiv)
{
  if (subdiv->evaluator != NULL) {
    openSubdiv_deleteEvaluator(subdiv->evaluator);
  }
  if (subdiv->topology_refiner != NULL) {
    openSubdiv_deleteTopologyRefiner(subdiv->topology_refiner);
  }
  BKE_subdiv_displacement_detach(subdiv);
  if (subdiv->cache_.face_ptex_offset != NULL) {
    MEM_freeN(subdiv->cache_.face_ptex_offset);
  }
  MEM_freeN(subdiv);
}

/* =========================== PTEX FACES AND GRIDS ========================= */

int *BKE_subdiv_face_ptex_offset_get(Subdiv *subdiv)
{
  if (subdiv->cache_.face_ptex_offset != NULL) {
    return subdiv->cache_.face_ptex_offset;
  }
  OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
  if (topology_refiner == NULL) {
    return NULL;
  }
  const int num_coarse_faces = topology_refiner->getNumFaces(topology_refiner);
  subdiv->cache_.face_ptex_offset = MEM_malloc_arrayN(
      num_coarse_faces, sizeof(int), "subdiv face_ptex_offset");
  int ptex_offset = 0;
  for (int face_index = 0; face_index < num_coarse_faces; face_index++) {
    const int num_ptex_faces = topology_refiner->getNumFacePtexFaces(topology_refiner, face_index);
    subdiv->cache_.face_ptex_offset[face_index] = ptex_offset;
    ptex_offset += num_ptex_faces;
  }
  return subdiv->cache_.face_ptex_offset;
}
