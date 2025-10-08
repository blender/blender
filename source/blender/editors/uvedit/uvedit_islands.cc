/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eduv
 *
 * Utilities for manipulating UV islands.
 *
 * \note This is similar to `GEO_uv_parametrizer.hh`,
 * however the data structures there don't support arbitrary topology
 * such as an edge with 3 or more faces using it.
 * This API uses #BMesh data structures and doesn't have limitations for manifold meshes.
 */

#include "BLI_listbase.h"
#include "BLI_math_vector.h"

#include "BKE_editmesh.hh"

#include "DNA_image_types.h"

#include "ED_uvedit.hh" /* Own include. */

/* -------------------------------------------------------------------- */
/** \name UDIM packing helper functions
 * \{ */

bool uv_coords_isect_udim(const Image *image, const int udim_grid[2], const float coords[2])
{
  const float coords_floor[2] = {floorf(coords[0]), floorf(coords[1])};
  const bool is_tiled_image = image && (image->source == IMA_SRC_TILED);

  if (coords[0] < udim_grid[0] && coords[0] > 0 && coords[1] < udim_grid[1] && coords[1] > 0) {
    return true;
  }
  /* Check if selection lies on a valid UDIM image tile. */
  if (is_tiled_image) {
    LISTBASE_FOREACH (const ImageTile *, tile, &image->tiles) {
      const int tile_index = tile->tile_number - 1001;
      const int target_x = (tile_index % 10);
      const int target_y = (tile_index / 10);
      if (coords_floor[0] == target_x && coords_floor[1] == target_y) {
        return true;
      }
    }
  }
  /* Probably not required since UDIM grid checks for 1001. */
  else if (image && !is_tiled_image) {
    if (is_zero_v2(coords_floor)) {
      return true;
    }
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Calculate UV Islands
 * \{ */

struct SharedUVLoopData {
  BMUVOffsets offsets;
  bool use_seams;
};

static bool bm_loop_uv_shared_edge_check(const BMLoop *l_a, const BMLoop *l_b, void *user_data)
{
  const SharedUVLoopData *data = static_cast<const SharedUVLoopData *>(user_data);

  if (data->use_seams) {
    if (BM_elem_flag_test(l_a->e, BM_ELEM_SEAM)) {
      return false;
    }
  }

  return BM_loop_uv_share_edge_check((BMLoop *)l_a, (BMLoop *)l_b, data->offsets.uv);
}

/**
 * Returns true if `efa` is able to be affected by a packing operation, given various parameters.
 *
 * Checks if it's (not) hidden, and optionally selected, and/or UV selected.
 *
 * Will eventually be superseded by `BM_uv_element_map_create()`.
 *
 * Loosely based on `uvedit_is_face_affected`, but "bug-compatible" with previous code.
 */
static bool uvedit_is_face_affected_for_calc_uv_islands(const Scene *scene,
                                                        const BMesh *bm,
                                                        BMFace *efa,
                                                        const bool only_selected_faces,
                                                        const bool only_selected_uvs)
{
  if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
    return false;
  }
  if (only_selected_faces) {
    if (only_selected_uvs) {
      return BM_elem_flag_test(efa, BM_ELEM_SELECT) && uvedit_face_select_test(scene, bm, efa);
    }
    return BM_elem_flag_test(efa, BM_ELEM_SELECT);
  }
  return true;
}

int bm_mesh_calc_uv_islands(const Scene *scene,
                            BMesh *bm,
                            ListBase *island_list,
                            const bool only_selected_faces,
                            const bool only_selected_uvs,
                            const bool use_seams,
                            const float aspect_y,
                            const BMUVOffsets &uv_offsets)
{
  BLI_assert(uv_offsets.uv >= 0);
  int island_added = 0;
  BM_mesh_elem_table_ensure(bm, BM_FACE);

  int *groups_array = MEM_malloc_arrayN<int>(bm->totface, __func__);

  int (*group_index)[2];

  /* Set the tag for `BM_mesh_calc_face_groups`. */
  BMFace *f;
  BMIter iter;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    const bool face_affected = uvedit_is_face_affected_for_calc_uv_islands(
        scene, bm, f, only_selected_faces, only_selected_uvs);
    BM_elem_flag_set(f, BM_ELEM_TAG, face_affected);
  }

  SharedUVLoopData user_data = {{0}};
  user_data.offsets = uv_offsets;
  user_data.use_seams = use_seams;

  const int group_len = BM_mesh_calc_face_groups(bm,
                                                 groups_array,
                                                 &group_index,
                                                 nullptr,
                                                 bm_loop_uv_shared_edge_check,
                                                 &user_data,
                                                 BM_ELEM_TAG,
                                                 BM_EDGE);

  for (int i = 0; i < group_len; i++) {
    const int faces_start = group_index[i][0];
    const int faces_len = group_index[i][1];
    BMFace **faces = MEM_malloc_arrayN<BMFace *>(faces_len, __func__);

    for (int j = 0; j < faces_len; j++) {
      faces[j] = BM_face_at_index(bm, groups_array[faces_start + j]);
    }

    FaceIsland *island = MEM_callocN<FaceIsland>(__func__);
    island->faces = faces;
    island->faces_len = faces_len;
    island->offsets = uv_offsets;
    island->aspect_y = aspect_y;
    BLI_addtail(island_list, island);
    island_added += 1;
  }

  MEM_freeN(groups_array);
  MEM_freeN(group_index);
  return island_added;
}

/** \} */
