/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2012 Blender Foundation */

/** \file
 * \ingroup render
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_threads.h"

#include "BKE_DerivedMesh.h"
#include "BKE_ccg.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_tangent.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_subsurf.h"

#include "DEG_depsgraph.h"

#include "RE_multires_bake.h"
#include "RE_pipeline.h"
#include "RE_texture.h"
#include "RE_texture_margin.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

using MPassKnownData = void (*)(blender::Span<blender::float3> vert_positions,
                                blender::Span<blender::float3> vert_normals,
                                blender::OffsetIndices<int> polys,
                                blender::Span<int> corner_verts,
                                blender::Span<MLoopTri> looptris,
                                blender::Span<int> looptri_polys,
                                blender::Span<blender::float2> uv_map,
                                DerivedMesh *hires_dm,
                                void *thread_data,
                                void *bake_data,
                                ImBuf *ibuf,
                                const int face_index,
                                const int lvl,
                                const float st[2],
                                float tangmat[3][3],
                                const int x,
                                const int y);

using MInitBakeData = void *(*)(MultiresBakeRender *bkr, ImBuf *ibuf);
using MFreeBakeData = void (*)(void *bake_data);

struct MultiresBakeResult {
  float height_min, height_max;
};

struct MResolvePixelData {
  /* Data from low-resolution mesh. */
  blender::Span<blender::float3> vert_positions;
  blender::OffsetIndices<int> polys;
  blender::Span<int> corner_verts;
  blender::Span<MLoopTri> looptris;
  blender::Span<int> looptri_polys;
  blender::Span<blender::float3> vert_normals;
  blender::Span<blender::float3> poly_normals;

  blender::Span<blender::float2> uv_map;

  /* May be null. */
  const int *material_indices;
  const bool *sharp_faces;

  float uv_offset[2];
  float *pvtangent;
  int w, h;
  int tri_index;

  DerivedMesh *hires_dm;

  int lvl;
  void *thread_data;
  void *bake_data;
  ImBuf *ibuf;
  MPassKnownData pass_data;
  /* material aligned UV array */
  Image **image_array;
};

using MFlushPixel = void (*)(const MResolvePixelData *data, const int x, const int y);

struct MBakeRast {
  int w, h;
  char *texels;
  const MResolvePixelData *data;
  MFlushPixel flush_pixel;
  bool *do_update;
};

struct MHeightBakeData {
  float *heights;
  DerivedMesh *ssdm;
  const int *orig_index_mp_to_orig;
};

struct MNormalBakeData {
  const int *orig_index_mp_to_orig;
};

struct BakeImBufuserData {
  float *displacement_buffer;
  char *mask_buffer;
};

static void multiresbake_get_normal(const MResolvePixelData *data,
                                    const int tri_num,
                                    const int vert_index,
                                    float r_normal[3])
{
  const int poly_index = data->looptri_polys[tri_num];
  const bool smoothnormal = !(data->sharp_faces && data->sharp_faces[poly_index]);

  if (smoothnormal) {
    const int vi = data->corner_verts[data->looptris[tri_num].tri[vert_index]];
    copy_v3_v3(r_normal, data->vert_normals[vi]);
  }
  else {
    copy_v3_v3(r_normal, data->poly_normals[poly_index]);
  }
}

static void init_bake_rast(MBakeRast *bake_rast,
                           const ImBuf *ibuf,
                           const MResolvePixelData *data,
                           MFlushPixel flush_pixel,
                           bool *do_update)
{
  BakeImBufuserData *userdata = (BakeImBufuserData *)ibuf->userdata;

  memset(bake_rast, 0, sizeof(MBakeRast));

  bake_rast->texels = userdata->mask_buffer;
  bake_rast->w = ibuf->x;
  bake_rast->h = ibuf->y;
  bake_rast->data = data;
  bake_rast->flush_pixel = flush_pixel;
  bake_rast->do_update = do_update;
}

static void flush_pixel(const MResolvePixelData *data, const int x, const int y)
{
  const float st[2] = {(x + 0.5f) / data->w + data->uv_offset[0],
                       (y + 0.5f) / data->h + data->uv_offset[1]};
  const float *st0, *st1, *st2;
  const float *tang0, *tang1, *tang2;
  float no0[3], no1[3], no2[3];
  float fUV[2], from_tang[3][3], to_tang[3][3];
  float u, v, w, sign;
  int r;

  st0 = data->uv_map[data->looptris[data->tri_index].tri[0]];
  st1 = data->uv_map[data->looptris[data->tri_index].tri[1]];
  st2 = data->uv_map[data->looptris[data->tri_index].tri[2]];

  multiresbake_get_normal(data, data->tri_index, 0, no0); /* can optimize these 3 into one call */
  multiresbake_get_normal(data, data->tri_index, 1, no1);
  multiresbake_get_normal(data, data->tri_index, 2, no2);

  resolve_tri_uv_v2(fUV, st, st0, st1, st2);

  u = fUV[0];
  v = fUV[1];
  w = 1 - u - v;

  if (data->pvtangent) {
    tang0 = data->pvtangent + data->looptris[data->tri_index].tri[0] * 4;
    tang1 = data->pvtangent + data->looptris[data->tri_index].tri[1] * 4;
    tang2 = data->pvtangent + data->looptris[data->tri_index].tri[2] * 4;

    /* the sign is the same at all face vertices for any non degenerate face.
     * Just in case we clamp the interpolated value though. */
    sign = (tang0[3] * u + tang1[3] * v + tang2[3] * w) < 0 ? (-1.0f) : 1.0f;

    /* this sequence of math is designed specifically as is with great care
     * to be compatible with our shader. Please don't change without good reason. */
    for (r = 0; r < 3; r++) {
      from_tang[0][r] = tang0[r] * u + tang1[r] * v + tang2[r] * w;
      from_tang[2][r] = no0[r] * u + no1[r] * v + no2[r] * w;
    }

    cross_v3_v3v3(from_tang[1], from_tang[2], from_tang[0]); /* `B = sign * cross(N, T)` */
    mul_v3_fl(from_tang[1], sign);
    invert_m3_m3(to_tang, from_tang);
  }
  else {
    zero_m3(to_tang);
  }

  data->pass_data(data->vert_positions,
                  data->vert_normals,
                  data->polys,
                  data->corner_verts,
                  data->looptris,
                  data->looptri_polys,
                  data->uv_map,
                  data->hires_dm,
                  data->thread_data,
                  data->bake_data,
                  data->ibuf,
                  data->tri_index,
                  data->lvl,
                  st,
                  to_tang,
                  x,
                  y);
}

static void set_rast_triangle(const MBakeRast *bake_rast, const int x, const int y)
{
  const int w = bake_rast->w;
  const int h = bake_rast->h;

  if (x >= 0 && x < w && y >= 0 && y < h) {
    if ((bake_rast->texels[y * w + x]) == 0) {
      bake_rast->texels[y * w + x] = FILTER_MASK_USED;
      flush_pixel(bake_rast->data, x, y);
      if (bake_rast->do_update) {
        *bake_rast->do_update = true;
      }
    }
  }
}

static void rasterize_half(const MBakeRast *bake_rast,
                           const float s0_s,
                           const float t0_s,
                           const float s1_s,
                           const float t1_s,
                           const float s0_l,
                           const float t0_l,
                           const float s1_l,
                           const float t1_l,
                           const int y0_in,
                           const int y1_in,
                           const int is_mid_right)
{
  const int s_stable = fabsf(t1_s - t0_s) > FLT_EPSILON ? 1 : 0;
  const int l_stable = fabsf(t1_l - t0_l) > FLT_EPSILON ? 1 : 0;
  const int w = bake_rast->w;
  const int h = bake_rast->h;
  int y, y0, y1;

  if (y1_in <= 0 || y0_in >= h) {
    return;
  }

  y0 = y0_in < 0 ? 0 : y0_in;
  y1 = y1_in >= h ? h : y1_in;

  for (y = y0; y < y1; y++) {
    /*-b(x-x0) + a(y-y0) = 0 */
    int iXl, iXr, x;
    float x_l = s_stable != 0 ? (s0_s + (((s1_s - s0_s) * (y - t0_s)) / (t1_s - t0_s))) : s0_s;
    float x_r = l_stable != 0 ? (s0_l + (((s1_l - s0_l) * (y - t0_l)) / (t1_l - t0_l))) : s0_l;

    if (is_mid_right != 0) {
      std::swap(x_l, x_r);
    }

    iXl = int(ceilf(x_l));
    iXr = int(ceilf(x_r));

    if (iXr > 0 && iXl < w) {
      iXl = iXl < 0 ? 0 : iXl;
      iXr = iXr >= w ? w : iXr;

      for (x = iXl; x < iXr; x++) {
        set_rast_triangle(bake_rast, x, y);
      }
    }
  }
}

static void bake_rasterize(const MBakeRast *bake_rast,
                           const float st0_in[2],
                           const float st1_in[2],
                           const float st2_in[2])
{
  const int w = bake_rast->w;
  const int h = bake_rast->h;
  float slo = st0_in[0] * w - 0.5f;
  float tlo = st0_in[1] * h - 0.5f;
  float smi = st1_in[0] * w - 0.5f;
  float tmi = st1_in[1] * h - 0.5f;
  float shi = st2_in[0] * w - 0.5f;
  float thi = st2_in[1] * h - 0.5f;
  int is_mid_right = 0, ylo, yhi, yhi_beg;

  /* skip degenerates */
  if ((slo == smi && tlo == tmi) || (slo == shi && tlo == thi) || (smi == shi && tmi == thi)) {
    return;
  }

  /* sort by T */
  if (tlo > tmi && tlo > thi) {
    std::swap(shi, slo);
    std::swap(thi, tlo);
  }
  else if (tmi > thi) {
    std::swap(shi, smi);
    std::swap(thi, tmi);
  }

  if (tlo > tmi) {
    std::swap(slo, smi);
    std::swap(tlo, tmi);
  }

  /* check if mid point is to the left or to the right of the lo-hi edge */
  is_mid_right = (-(shi - slo) * (tmi - thi) + (thi - tlo) * (smi - shi)) > 0 ? 1 : 0;
  ylo = int(ceilf(tlo));
  yhi_beg = int(ceilf(tmi));
  yhi = int(ceilf(thi));

  // if (fTmi>ceilf(fTlo))
  rasterize_half(bake_rast, slo, tlo, smi, tmi, slo, tlo, shi, thi, ylo, yhi_beg, is_mid_right);
  rasterize_half(bake_rast, smi, tmi, shi, thi, slo, tlo, shi, thi, yhi_beg, yhi, is_mid_right);
}

static int multiresbake_test_break(MultiresBakeRender *bkr)
{
  if (!bkr->stop) {
    /* this means baker is executed outside from job system */
    return 0;
  }

  return *bkr->stop || G.is_break;
}

/* **** Threading routines **** */

struct MultiresBakeQueue {
  int cur_tri;
  int tot_tri;
  SpinLock spin;
};

struct MultiresBakeThread {
  /* this data is actually shared between all the threads */
  MultiresBakeQueue *queue;
  MultiresBakeRender *bkr;
  Image *image;
  void *bake_data;
  int num_total_faces;

  /* thread-specific data */
  MBakeRast bake_rast;
  MResolvePixelData data;

  /* displacement-specific data */
  float height_min, height_max;
};

static int multires_bake_queue_next_tri(MultiresBakeQueue *queue)
{
  int face = -1;

  /* TODO: it could worth making it so thread will handle neighbor faces
   *       for better memory cache utilization
   */

  BLI_spin_lock(&queue->spin);
  if (queue->cur_tri < queue->tot_tri) {
    face = queue->cur_tri;
    queue->cur_tri++;
  }
  BLI_spin_unlock(&queue->spin);

  return face;
}

static void *do_multires_bake_thread(void *data_v)
{
  MultiresBakeThread *handle = (MultiresBakeThread *)data_v;
  MResolvePixelData *data = &handle->data;
  MBakeRast *bake_rast = &handle->bake_rast;
  MultiresBakeRender *bkr = handle->bkr;
  int tri_index;

  while ((tri_index = multires_bake_queue_next_tri(handle->queue)) >= 0) {
    const MLoopTri *lt = &data->looptris[tri_index];
    const int poly_i = data->looptri_polys[tri_index];
    const short mat_nr = data->material_indices == nullptr ? 0 : data->material_indices[poly_i];

    if (multiresbake_test_break(bkr)) {
      break;
    }

    Image *tri_image = mat_nr < bkr->ob_image.len ? bkr->ob_image.array[mat_nr] : nullptr;
    if (tri_image != handle->image) {
      continue;
    }

    data->tri_index = tri_index;

    float uv[3][2];
    sub_v2_v2v2(uv[0], data->uv_map[lt->tri[0]], data->uv_offset);
    sub_v2_v2v2(uv[1], data->uv_map[lt->tri[1]], data->uv_offset);
    sub_v2_v2v2(uv[2], data->uv_map[lt->tri[2]], data->uv_offset);

    bake_rasterize(bake_rast, uv[0], uv[1], uv[2]);

    /* tag image buffer for refresh */
    if (data->ibuf->rect_float) {
      data->ibuf->userflags |= IB_RECT_INVALID;
    }

    data->ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;

    /* update progress */
    BLI_spin_lock(&handle->queue->spin);
    bkr->baked_faces++;

    if (bkr->do_update) {
      *bkr->do_update = true;
    }

    if (bkr->progress) {
      *bkr->progress = (float(bkr->baked_objects) +
                        float(bkr->baked_faces) / handle->num_total_faces) /
                       bkr->tot_obj;
    }
    BLI_spin_unlock(&handle->queue->spin);
  }

  return nullptr;
}

/* some of arrays inside ccgdm are lazy-initialized, which will generally
 * require lock around accessing such data
 * this function will ensure all arrays are allocated before threading started
 */
static void init_ccgdm_arrays(DerivedMesh *dm)
{
  CCGElem **grid_data;
  CCGKey key;
  int grid_size;
  const int *grid_offset;

  grid_size = dm->getGridSize(dm);
  grid_data = dm->getGridData(dm);
  grid_offset = dm->getGridOffset(dm);
  dm->getGridKey(dm, &key);

  (void)grid_size;
  (void)grid_data;
  (void)grid_offset;
}

static void do_multires_bake(MultiresBakeRender *bkr,
                             Image *ima,
                             ImageTile *tile,
                             ImBuf *ibuf,
                             bool require_tangent,
                             MPassKnownData passKnownData,
                             MInitBakeData initBakeData,
                             MFreeBakeData freeBakeData,
                             MultiresBakeResult *result)
{
  DerivedMesh *dm = bkr->lores_dm;
  const int lvl = bkr->lvl;
  if (dm->getNumPolys(dm) == 0) {
    return;
  }

  MultiresBakeQueue queue;

  const blender::Span<blender::float2> uv_map(
      reinterpret_cast<const blender::float2 *>(dm->getLoopDataArray(dm, CD_PROP_FLOAT2)),
      dm->getNumLoops(dm));

  float *pvtangent = nullptr;

  ListBase threads;
  int i, tot_thread = bkr->threads > 0 ? bkr->threads : BLI_system_thread_count();

  void *bake_data = nullptr;

  Mesh *temp_mesh = BKE_mesh_new_nomain(
      dm->getNumVerts(dm), dm->getNumEdges(dm), dm->getNumPolys(dm), dm->getNumLoops(dm));
  temp_mesh->vert_positions_for_write().copy_from(
      {reinterpret_cast<const blender::float3 *>(dm->getVertArray(dm)), temp_mesh->totvert});
  temp_mesh->edges_for_write().copy_from(
      {reinterpret_cast<const blender::int2 *>(dm->getEdgeArray(dm)), temp_mesh->totedge});
  temp_mesh->poly_offsets_for_write().copy_from({dm->getPolyArray(dm), temp_mesh->totpoly + 1});
  temp_mesh->corner_verts_for_write().copy_from({dm->getCornerVertArray(dm), temp_mesh->totloop});
  temp_mesh->corner_edges_for_write().copy_from({dm->getCornerEdgeArray(dm), temp_mesh->totloop});

  const blender::Span<blender::float3> positions = temp_mesh->vert_positions();
  const blender::OffsetIndices polys = temp_mesh->polys();
  const blender::Span<int> corner_verts = temp_mesh->corner_verts();
  const blender::Span<blender::float3> vert_normals = temp_mesh->vert_normals();
  const blender::Span<blender::float3> poly_normals = temp_mesh->poly_normals();
  const blender::Span<MLoopTri> looptris = temp_mesh->looptris();
  const blender::Span<int> looptri_polys = temp_mesh->looptri_polys();

  if (require_tangent) {
    if (CustomData_get_layer_index(&dm->loopData, CD_TANGENT) == -1) {
      BKE_mesh_calc_loop_tangent_ex(
          reinterpret_cast<const float(*)[3]>(positions.data()),
          polys,
          dm->getCornerVertArray(dm),
          looptris.data(),
          looptri_polys.data(),
          looptris.size(),
          static_cast<const bool *>(
              CustomData_get_layer_named(&dm->polyData, CD_PROP_BOOL, "sharp_face")),
          &dm->loopData,
          true,
          nullptr,
          0,
          reinterpret_cast<const float(*)[3]>(vert_normals.data()),
          reinterpret_cast<const float(*)[3]>(poly_normals.data()),
          (const float(*)[3])dm->getLoopDataArray(dm, CD_NORMAL),
          (const float(*)[3])dm->getVertDataArray(dm, CD_ORCO), /* May be nullptr. */
          /* result */
          &dm->loopData,
          dm->getNumLoops(dm),
          &dm->tangent_mask);
    }

    pvtangent = static_cast<float *>(DM_get_loop_data_layer(dm, CD_TANGENT));
  }

  /* all threads shares the same custom bake data */
  if (initBakeData) {
    bake_data = initBakeData(bkr, ibuf);
  }

  if (tot_thread > 1) {
    BLI_threadpool_init(&threads, do_multires_bake_thread, tot_thread);
  }

  blender::Array<MultiresBakeThread> handles(tot_thread);

  init_ccgdm_arrays(bkr->hires_dm);

  /* faces queue */
  queue.cur_tri = 0;
  queue.tot_tri = looptris.size();
  BLI_spin_init(&queue.spin);

  /* fill in threads handles */
  for (i = 0; i < tot_thread; i++) {
    MultiresBakeThread *handle = &handles[i];

    handle->bkr = bkr;
    handle->image = ima;
    handle->num_total_faces = queue.tot_tri * BLI_listbase_count(&ima->tiles);
    handle->queue = &queue;

    handle->data.vert_positions = positions;
    handle->data.polys = polys;
    handle->data.corner_verts = corner_verts;
    handle->data.looptris = looptris;
    handle->data.looptri_polys = looptri_polys;
    handle->data.vert_normals = vert_normals;
    handle->data.poly_normals = poly_normals;
    handle->data.material_indices = static_cast<const int *>(
        CustomData_get_layer_named(&dm->polyData, CD_PROP_INT32, "material_index"));
    handle->data.sharp_faces = static_cast<const bool *>(
        CustomData_get_layer_named(&dm->polyData, CD_PROP_BOOL, "sharp_face"));
    handle->data.uv_map = uv_map;
    BKE_image_get_tile_uv(ima, tile->tile_number, handle->data.uv_offset);
    handle->data.pvtangent = pvtangent;
    handle->data.w = ibuf->x;
    handle->data.h = ibuf->y;
    handle->data.hires_dm = bkr->hires_dm;
    handle->data.lvl = lvl;
    handle->data.pass_data = passKnownData;
    handle->data.thread_data = handle;
    handle->data.bake_data = bake_data;
    handle->data.ibuf = ibuf;

    handle->height_min = FLT_MAX;
    handle->height_max = -FLT_MAX;

    init_bake_rast(&handle->bake_rast, ibuf, &handle->data, flush_pixel, bkr->do_update);

    if (tot_thread > 1) {
      BLI_threadpool_insert(&threads, handle);
    }
  }

  /* run threads */
  if (tot_thread > 1) {
    BLI_threadpool_end(&threads);
  }
  else {
    do_multires_bake_thread(&handles[0]);
  }

  for (i = 0; i < tot_thread; i++) {
    result->height_min = min_ff(result->height_min, handles[i].height_min);
    result->height_max = max_ff(result->height_max, handles[i].height_max);
  }

  BLI_spin_end(&queue.spin);

  /* finalize baking */
  if (freeBakeData) {
    freeBakeData(bake_data);
  }

  BKE_id_free(nullptr, temp_mesh);
}

/* mode = 0: interpolate normals,
 * mode = 1: interpolate coord */
static void interp_bilinear_grid(
    CCGKey *key, CCGElem *grid, float crn_x, float crn_y, int mode, float res[3])
{
  int x0, x1, y0, y1;
  float u, v;
  float data[4][3];

  x0 = int(crn_x);
  x1 = x0 >= (key->grid_size - 1) ? (key->grid_size - 1) : (x0 + 1);

  y0 = int(crn_y);
  y1 = y0 >= (key->grid_size - 1) ? (key->grid_size - 1) : (y0 + 1);

  u = crn_x - x0;
  v = crn_y - y0;

  if (mode == 0) {
    copy_v3_v3(data[0], CCG_grid_elem_no(key, grid, x0, y0));
    copy_v3_v3(data[1], CCG_grid_elem_no(key, grid, x1, y0));
    copy_v3_v3(data[2], CCG_grid_elem_no(key, grid, x1, y1));
    copy_v3_v3(data[3], CCG_grid_elem_no(key, grid, x0, y1));
  }
  else {
    copy_v3_v3(data[0], CCG_grid_elem_co(key, grid, x0, y0));
    copy_v3_v3(data[1], CCG_grid_elem_co(key, grid, x1, y0));
    copy_v3_v3(data[2], CCG_grid_elem_co(key, grid, x1, y1));
    copy_v3_v3(data[3], CCG_grid_elem_co(key, grid, x0, y1));
  }

  interp_bilinear_quad_v3(data, u, v, res);
}

static void get_ccgdm_data(const blender::OffsetIndices<int> lores_polys,
                           DerivedMesh *hidm,
                           const int *index_mp_to_orig,
                           const int lvl,
                           const int poly_index,
                           const float u,
                           const float v,
                           float co[3],
                           float n[3])
{
  CCGElem **grid_data;
  CCGKey key;
  float crn_x, crn_y;
  int grid_size, S, face_side;
  int *grid_offset, g_index;

  grid_size = hidm->getGridSize(hidm);
  grid_data = hidm->getGridData(hidm);
  grid_offset = hidm->getGridOffset(hidm);
  hidm->getGridKey(hidm, &key);

  if (lvl == 0) {
    face_side = (grid_size << 1) - 1;

    g_index = grid_offset[poly_index];
    S = mdisp_rot_face_to_crn(lores_polys[poly_index].size(),
                              face_side,
                              u * (face_side - 1),
                              v * (face_side - 1),
                              &crn_x,
                              &crn_y);
  }
  else {
    /* number of faces per grid side */
    int polys_per_grid_side = (1 << (lvl - 1));
    /* get the original cage face index */
    int cage_face_index = index_mp_to_orig ? index_mp_to_orig[poly_index] : poly_index;
    /* local offset in total cage face grids
     * `(1 << (2 * lvl))` is number of all polys for one cage face */
    int loc_cage_poly_ofs = poly_index % (1 << (2 * lvl));
    /* local offset in the vertex grid itself */
    int cell_index = loc_cage_poly_ofs % (polys_per_grid_side * polys_per_grid_side);
    int cell_side = (grid_size - 1) / polys_per_grid_side;
    /* row and column based on grid side */
    int row = cell_index / polys_per_grid_side;
    int col = cell_index % polys_per_grid_side;

    /* S is the vertex whose grid we are examining */
    S = poly_index / (1 << (2 * (lvl - 1))) - grid_offset[cage_face_index];
    /* get offset of grid data for original cage face */
    g_index = grid_offset[cage_face_index];

    crn_y = (row * cell_side) + u * cell_side;
    crn_x = (col * cell_side) + v * cell_side;
  }

  CLAMP(crn_x, 0.0f, grid_size);
  CLAMP(crn_y, 0.0f, grid_size);

  if (n != nullptr) {
    interp_bilinear_grid(&key, grid_data[g_index + S], crn_x, crn_y, 0, n);
  }

  if (co != nullptr) {
    interp_bilinear_grid(&key, grid_data[g_index + S], crn_x, crn_y, 1, co);
  }
}

/* mode = 0: interpolate normals,
 * mode = 1: interpolate coord */

static void interp_bilinear_mpoly(const blender::Span<blender::float3> vert_positions,
                                  const blender::Span<blender::float3> vert_normals,
                                  const blender::Span<int> corner_verts,
                                  const blender::IndexRange poly,
                                  const float u,
                                  const float v,
                                  const int mode,
                                  float res[3])
{
  float data[4][3];

  if (mode == 0) {
    copy_v3_v3(data[0], vert_normals[corner_verts[poly[0]]]);
    copy_v3_v3(data[1], vert_normals[corner_verts[poly[1]]]);
    copy_v3_v3(data[2], vert_normals[corner_verts[poly[2]]]);
    copy_v3_v3(data[3], vert_normals[corner_verts[poly[3]]]);
  }
  else {
    copy_v3_v3(data[0], vert_positions[corner_verts[poly[0]]]);
    copy_v3_v3(data[1], vert_positions[corner_verts[poly[1]]]);
    copy_v3_v3(data[2], vert_positions[corner_verts[poly[2]]]);
    copy_v3_v3(data[3], vert_positions[corner_verts[poly[3]]]);
  }

  interp_bilinear_quad_v3(data, u, v, res);
}

static void interp_barycentric_mlooptri(const blender::Span<blender::float3> vert_positions,
                                        const blender::Span<blender::float3> vert_normals,
                                        const blender::Span<int> corner_verts,
                                        const MLoopTri *lt,
                                        const float u,
                                        const float v,
                                        const int mode,
                                        float res[3])
{
  float data[3][3];

  if (mode == 0) {
    copy_v3_v3(data[0], vert_normals[corner_verts[lt->tri[0]]]);
    copy_v3_v3(data[1], vert_normals[corner_verts[lt->tri[1]]]);
    copy_v3_v3(data[2], vert_normals[corner_verts[lt->tri[2]]]);
  }
  else {
    copy_v3_v3(data[0], vert_positions[corner_verts[lt->tri[0]]]);
    copy_v3_v3(data[1], vert_positions[corner_verts[lt->tri[1]]]);
    copy_v3_v3(data[2], vert_positions[corner_verts[lt->tri[2]]]);
  }

  interp_barycentric_tri_v3(data, u, v, res);
}

/* **************** Displacement Baker **************** */

static void *init_heights_data(MultiresBakeRender *bkr, ImBuf *ibuf)
{
  MHeightBakeData *height_data;
  DerivedMesh *lodm = bkr->lores_dm;
  BakeImBufuserData *userdata = static_cast<BakeImBufuserData *>(ibuf->userdata);

  if (userdata->displacement_buffer == nullptr) {
    userdata->displacement_buffer = MEM_cnew_array<float>(ibuf->x * ibuf->y,
                                                          "MultiresBake heights");
  }

  height_data = MEM_cnew<MHeightBakeData>("MultiresBake heightData");

  height_data->heights = userdata->displacement_buffer;

  if (!bkr->use_lores_mesh) {
    SubsurfModifierData smd = {{nullptr}};
    int ss_lvl = bkr->tot_lvl - bkr->lvl;

    CLAMP(ss_lvl, 0, 6);

    if (ss_lvl > 0) {
      smd.levels = smd.renderLevels = ss_lvl;
      smd.uv_smooth = SUBSURF_UV_SMOOTH_PRESERVE_BOUNDARIES;
      smd.quality = 3;

      height_data->ssdm = subsurf_make_derived_from_derived(
          bkr->lores_dm, &smd, bkr->scene, nullptr, SubsurfFlags(0));
      init_ccgdm_arrays(height_data->ssdm);
    }
  }

  height_data->orig_index_mp_to_orig = static_cast<const int *>(
      lodm->getPolyDataArray(lodm, CD_ORIGINDEX));

  return (void *)height_data;
}

static void free_heights_data(void *bake_data)
{
  MHeightBakeData *height_data = (MHeightBakeData *)bake_data;

  if (height_data->ssdm) {
    height_data->ssdm->release(height_data->ssdm);
  }

  MEM_freeN(height_data);
}

/* MultiresBake callback for heights baking
 * general idea:
 *   - find coord of point with specified UV in hi-res mesh (let's call it p1)
 *   - find coord of point and normal with specified UV in lo-res mesh (or subdivided lo-res
 *     mesh to make texture smoother) let's call this point p0 and n.
 *   - height wound be dot(n, p1-p0) */
static void apply_heights_callback(const blender::Span<blender::float3> vert_positions,
                                   const blender::Span<blender::float3> vert_normals,
                                   const blender::OffsetIndices<int> polys,
                                   const blender::Span<int> corner_verts,
                                   const blender::Span<MLoopTri> looptris,
                                   const blender::Span<int> looptri_polys,
                                   const blender::Span<blender::float2> uv_map,
                                   DerivedMesh *hires_dm,
                                   void *thread_data_v,
                                   void *bake_data,
                                   ImBuf *ibuf,
                                   const int tri_index,
                                   const int lvl,
                                   const float st[2],
                                   float /*tangmat*/[3][3],
                                   const int x,
                                   const int y)
{
  const MLoopTri *lt = &looptris[tri_index];
  const int poly_i = looptri_polys[tri_index];
  const blender::IndexRange poly = polys[poly_i];
  MHeightBakeData *height_data = (MHeightBakeData *)bake_data;
  MultiresBakeThread *thread_data = (MultiresBakeThread *)thread_data_v;
  float uv[2];
  const float *st0, *st1, *st2, *st3;
  int pixel = ibuf->x * y + x;
  float vec[3], p0[3], p1[3], n[3], len;

  /* ideally we would work on triangles only, however, we rely on quads to get orthogonal
   * coordinates for use in grid space (triangle barycentric is not orthogonal) */
  if (poly.size() == 4) {
    st0 = uv_map[poly[0]];
    st1 = uv_map[poly[1]];
    st2 = uv_map[poly[2]];
    st3 = uv_map[poly[3]];
    resolve_quad_uv_v2(uv, st, st0, st1, st2, st3);
  }
  else {
    st0 = uv_map[lt->tri[0]];
    st1 = uv_map[lt->tri[1]];
    st2 = uv_map[lt->tri[2]];
    resolve_tri_uv_v2(uv, st, st0, st1, st2);
  }

  clamp_v2(uv, 0.0f, 1.0f);

  get_ccgdm_data(
      polys, hires_dm, height_data->orig_index_mp_to_orig, lvl, poly_i, uv[0], uv[1], p1, nullptr);

  if (height_data->ssdm) {
    get_ccgdm_data(polys,
                   height_data->ssdm,
                   height_data->orig_index_mp_to_orig,
                   0,
                   poly_i,
                   uv[0],
                   uv[1],
                   p0,
                   n);
  }
  else {
    if (poly.size() == 4) {
      interp_bilinear_mpoly(vert_positions, vert_normals, corner_verts, poly, uv[0], uv[1], 1, p0);
      interp_bilinear_mpoly(vert_positions, vert_normals, corner_verts, poly, uv[0], uv[1], 0, n);
    }
    else {
      interp_barycentric_mlooptri(
          vert_positions, vert_normals, corner_verts, lt, uv[0], uv[1], 1, p0);
      interp_barycentric_mlooptri(
          vert_positions, vert_normals, corner_verts, lt, uv[0], uv[1], 0, n);
    }
  }

  sub_v3_v3v3(vec, p1, p0);
  len = dot_v3v3(n, vec);

  height_data->heights[pixel] = len;

  thread_data->height_min = min_ff(thread_data->height_min, len);
  thread_data->height_max = max_ff(thread_data->height_max, len);

  if (ibuf->rect_float) {
    float *rrgbf = ibuf->rect_float + pixel * 4;
    rrgbf[0] = rrgbf[1] = rrgbf[2] = len;
    rrgbf[3] = 1.0f;
  }
  else {
    char *rrgb = (char *)ibuf->rect + pixel * 4;
    rrgb[0] = rrgb[1] = rrgb[2] = unit_float_to_uchar_clamp(len);
    rrgb[3] = 255;
  }
}

/* **************** Normal Maps Baker **************** */

static void *init_normal_data(MultiresBakeRender *bkr, ImBuf * /*ibuf*/)
{
  MNormalBakeData *normal_data;
  DerivedMesh *lodm = bkr->lores_dm;

  normal_data = MEM_cnew<MNormalBakeData>("MultiresBake normalData");

  normal_data->orig_index_mp_to_orig = static_cast<const int *>(
      lodm->getPolyDataArray(lodm, CD_ORIGINDEX));

  return (void *)normal_data;
}

static void free_normal_data(void *bake_data)
{
  MNormalBakeData *normal_data = (MNormalBakeData *)bake_data;

  MEM_freeN(normal_data);
}

/**
 * MultiresBake callback for normals' baking.
 *
 * General idea:
 * - Find coord and normal of point with specified UV in hi-res mesh.
 * - Multiply it by tangmat.
 * - Vector in color space would be `norm(vec) / 2 + (0.5, 0.5, 0.5)`.
 */
static void apply_tangmat_callback(const blender::Span<blender::float3> /*vert_positions*/,
                                   const blender::Span<blender::float3> /*vert_normals*/,
                                   const blender::OffsetIndices<int> polys,
                                   const blender::Span<int> /*corner_verts*/,
                                   const blender::Span<MLoopTri> looptris,
                                   const blender::Span<int> looptri_polys,
                                   const blender::Span<blender::float2> uv_map,
                                   DerivedMesh *hires_dm,
                                   void * /*thread_data*/,
                                   void *bake_data,
                                   ImBuf *ibuf,
                                   const int tri_index,
                                   const int lvl,
                                   const float st[2],
                                   float tangmat[3][3],
                                   const int x,
                                   const int y)
{
  const MLoopTri *lt = &looptris[tri_index];
  const int poly_i = looptri_polys[tri_index];
  const blender::IndexRange poly = polys[poly_i];
  MNormalBakeData *normal_data = (MNormalBakeData *)bake_data;
  float uv[2];
  const float *st0, *st1, *st2, *st3;
  int pixel = ibuf->x * y + x;
  float n[3], vec[3], tmp[3] = {0.5, 0.5, 0.5};

  /* ideally we would work on triangles only, however, we rely on quads to get orthogonal
   * coordinates for use in grid space (triangle barycentric is not orthogonal) */
  if (poly.size() == 4) {
    st0 = uv_map[poly[0]];
    st1 = uv_map[poly[1]];
    st2 = uv_map[poly[2]];
    st3 = uv_map[poly[3]];
    resolve_quad_uv_v2(uv, st, st0, st1, st2, st3);
  }
  else {
    st0 = uv_map[lt->tri[0]];
    st1 = uv_map[lt->tri[1]];
    st2 = uv_map[lt->tri[2]];
    resolve_tri_uv_v2(uv, st, st0, st1, st2);
  }

  clamp_v2(uv, 0.0f, 1.0f);

  get_ccgdm_data(
      polys, hires_dm, normal_data->orig_index_mp_to_orig, lvl, poly_i, uv[0], uv[1], nullptr, n);

  mul_v3_m3v3(vec, tangmat, n);
  normalize_v3_length(vec, 0.5);
  add_v3_v3(vec, tmp);

  if (ibuf->rect_float) {
    float *rrgbf = ibuf->rect_float + pixel * 4;
    rrgbf[0] = vec[0];
    rrgbf[1] = vec[1];
    rrgbf[2] = vec[2];
    rrgbf[3] = 1.0f;
  }
  else {
    uchar *rrgb = (uchar *)ibuf->rect + pixel * 4;
    rgb_float_to_uchar(rrgb, vec);
    rrgb[3] = 255;
  }
}

/* TODO: restore ambient occlusion baking support, using BLI BVH? */
#if 0
/* **************** Ambient Occlusion Baker **************** */

/* Must be a power of two. */
#  define MAX_NUMBER_OF_AO_RAYS 1024

static ushort ao_random_table_1[MAX_NUMBER_OF_AO_RAYS];
static ushort ao_random_table_2[MAX_NUMBER_OF_AO_RAYS];

static void init_ao_random(void)
{
  int i;

  for (i = 0; i < MAX_NUMBER_OF_AO_RAYS; i++) {
    ao_random_table_1[i] = rand() & 0xffff;
    ao_random_table_2[i] = rand() & 0xffff;
  }
}

static ushort get_ao_random1(const int i)
{
  return ao_random_table_1[i & (MAX_NUMBER_OF_AO_RAYS - 1)];
}

static ushort get_ao_random2(const int i)
{
  return ao_random_table_2[i & (MAX_NUMBER_OF_AO_RAYS - 1)];
}

static void build_permutation_table(ushort permutation[],
                                    ushort temp_permutation[],
                                    const int number_of_rays,
                                    const int is_first_perm_table)
{
  int i, k;

  for (i = 0; i < number_of_rays; i++) {
    temp_permutation[i] = i;
  }

  for (i = 0; i < number_of_rays; i++) {
    const uint nr_entries_left = number_of_rays - i;
    ushort rnd = is_first_perm_table != false ? get_ao_random1(i) : get_ao_random2(i);
    const ushort entry = rnd % nr_entries_left;

    /* pull entry */
    permutation[i] = temp_permutation[entry];

    /* delete entry */
    for (k = entry; k < nr_entries_left - 1; k++) {
      temp_permutation[k] = temp_permutation[k + 1];
    }
  }

  /* verify permutation table
   * every entry must appear exactly once
   */
#  if 0
  for (i = 0; i < number_of_rays; i++) temp_permutation[i] = 0;
  for (i = 0; i < number_of_rays; i++) ++temp_permutation[permutation[i]];
  for (i = 0; i < number_of_rays; i++) BLI_assert(temp_permutation[i] == 1);
#  endif
}

static void create_ao_raytree(MultiresBakeRender *bkr, MAOBakeData *ao_data)
{
  DerivedMesh *hidm = bkr->hires_dm;
  RayObject *raytree;
  RayFace *face;
  CCGElem **grid_data;
  CCGKey key;
  int grids_num, grid_size /*, face_side */, faces_num;
  int i;

  grids_num = hidm->getNumGrids(hidm);
  grid_size = hidm->getGridSize(hidm);
  grid_data = hidm->getGridData(hidm);
  hidm->getGridKey(hidm, &key);

  /* face_side = (grid_size << 1) - 1; */ /* UNUSED */
  faces_num = grids_num * (grid_size - 1) * (grid_size - 1);

  raytree = ao_data->raytree = RE_rayobject_create(
      bkr->raytrace_structure, faces_num, bkr->octree_resolution);
  face = ao_data->rayfaces = (RayFace *)MEM_callocN(faces_num * sizeof(RayFace),
                                                    "ObjectRen faces");

  for (i = 0; i < grids_num; i++) {
    int x, y;
    for (x = 0; x < grid_size - 1; x++) {
      for (y = 0; y < grid_size - 1; y++) {
        float co[4][3];

        copy_v3_v3(co[0], CCG_grid_elem_co(&key, grid_data[i], x, y));
        copy_v3_v3(co[1], CCG_grid_elem_co(&key, grid_data[i], x, y + 1));
        copy_v3_v3(co[2], CCG_grid_elem_co(&key, grid_data[i], x + 1, y + 1));
        copy_v3_v3(co[3], CCG_grid_elem_co(&key, grid_data[i], x + 1, y));

        RE_rayface_from_coords(face, ao_data, face, co[0], co[1], co[2], co[3]);
        RE_rayobject_add(raytree, RE_rayobject_unalignRayFace(face));

        face++;
      }
    }
  }

  RE_rayobject_done(raytree);
}

static void *init_ao_data(MultiresBakeRender *bkr, ImBuf */*ibuf*/)
{
  MAOBakeData *ao_data;
  DerivedMesh *lodm = bkr->lores_dm;
  ushort *temp_permutation_table;
  size_t permutation_size;

  init_ao_random();

  ao_data = MEM_callocN(sizeof(MAOBakeData), "MultiresBake aoData");

  ao_data->number_of_rays = bkr->number_of_rays;
  ao_data->bias = bkr->bias;

  ao_data->orig_index_mp_to_orig = lodm->getPolyDataArray(lodm, CD_ORIGINDEX);

  create_ao_raytree(bkr, ao_data);

  /* initialize permutation tables */
  permutation_size = sizeof(ushort) * bkr->number_of_rays;
  ao_data->permutation_table_1 = MEM_callocN(permutation_size, "multires AO baker perm1");
  ao_data->permutation_table_2 = MEM_callocN(permutation_size, "multires AO baker perm2");
  temp_permutation_table = MEM_callocN(permutation_size, "multires AO baker temp perm");

  build_permutation_table(
      ao_data->permutation_table_1, temp_permutation_table, bkr->number_of_rays, 1);
  build_permutation_table(
      ao_data->permutation_table_2, temp_permutation_table, bkr->number_of_rays, 0);

  MEM_freeN(temp_permutation_table);

  return (void *)ao_data;
}

static void free_ao_data(void *bake_data)
{
  MAOBakeData *ao_data = (MAOBakeData *)bake_data;

  RE_rayobject_free(ao_data->raytree);
  MEM_freeN(ao_data->rayfaces);

  MEM_freeN(ao_data->permutation_table_1);
  MEM_freeN(ao_data->permutation_table_2);

  MEM_freeN(ao_data);
}

/* builds an X and a Y axis from the given Z axis */
static void build_coordinate_frame(float axisX[3], float axisY[3], const float axisZ[3])
{
  const float faX = fabsf(axisZ[0]);
  const float faY = fabsf(axisZ[1]);
  const float faZ = fabsf(axisZ[2]);

  if (faX <= faY && faX <= faZ) {
    const float len = sqrtf(axisZ[1] * axisZ[1] + axisZ[2] * axisZ[2]);
    axisY[0] = 0;
    axisY[1] = axisZ[2] / len;
    axisY[2] = -axisZ[1] / len;
    cross_v3_v3v3(axisX, axisY, axisZ);
  }
  else if (faY <= faZ) {
    const float len = sqrtf(axisZ[0] * axisZ[0] + axisZ[2] * axisZ[2]);
    axisX[0] = axisZ[2] / len;
    axisX[1] = 0;
    axisX[2] = -axisZ[0] / len;
    cross_v3_v3v3(axisY, axisZ, axisX);
  }
  else {
    const float len = sqrtf(axisZ[0] * axisZ[0] + axisZ[1] * axisZ[1]);
    axisX[0] = axisZ[1] / len;
    axisX[1] = -axisZ[0] / len;
    axisX[2] = 0;
    cross_v3_v3v3(axisY, axisZ, axisX);
  }
}

/* return false if nothing was hit and true otherwise */
static int trace_ao_ray(MAOBakeData *ao_data, float ray_start[3], float ray_direction[3])
{
  Isect isect = {{0}};

  isect.dist = RE_RAYTRACE_MAXDIST;
  copy_v3_v3(isect.start, ray_start);
  copy_v3_v3(isect.dir, ray_direction);
  isect.lay = -1;

  normalize_v3(isect.dir);

  return RE_rayobject_raycast(ao_data->raytree, &isect);
}

static void apply_ao_callback(DerivedMesh *lores_dm,
                              DerivedMesh *hires_dm,
                              void */*thread_data*/,
                              void *bake_data,
                              ImBuf *ibuf,
                              const int tri_index,
                              const int lvl,
                              const float st[2],
                              float /*tangmat[3][3]*/,
                              const int x,
                              const int y)
{
  const MLoopTri *lt = lores_dm->getLoopTriArray(lores_dm) + tri_index;
  float (*mloopuv)[2] = lores_dm->getLoopDataArray(lores_dm, CD_PROP_FLOAT2);
  MAOBakeData *ao_data = (MAOBakeData *)bake_data;

  int i, k, perm_ofs;
  float pos[3], nrm[3];
  float cen[3];
  float axisX[3], axisY[3], axisZ[3];
  float shadow = 0;
  float value;
  int pixel = ibuf->x * y + x;
  float uv[2], *st0, *st1, *st2, *st3;

  /* ideally we would work on triangles only, however, we rely on quads to get orthogonal
   * coordinates for use in grid space (triangle barycentric is not orthogonal) */
  if (poly.size() == 4) {
    st0 = mloopuv[poly[0]];
    st1 = mloopuv[poly[1]];
    st2 = mloopuv[poly[2]];
    st3 = mloopuv[poly[3]];
    resolve_quad_uv_v2(uv, st, st0, st1, st2, st3);
  }
  else {
    st0 = mloopuv[lt->tri[0]];
    st1 = mloopuv[lt->tri[1]];
    st2 = mloopuv[lt->tri[2]];
    resolve_tri_uv_v2(uv, st, st0, st1, st2);
  }

  clamp_v2(uv, 0.0f, 1.0f);

  get_ccgdm_data(
      lores_dm, hires_dm, ao_data->orig_index_mp_to_orig, lvl, lt, uv[0], uv[1], pos, nrm);

  /* offset ray origin by user bias along normal */
  for (i = 0; i < 3; i++) {
    cen[i] = pos[i] + ao_data->bias * nrm[i];
  }

  /* build tangent frame */
  for (i = 0; i < 3; i++) {
    axisZ[i] = nrm[i];
  }

  build_coordinate_frame(axisX, axisY, axisZ);

  /* static noise */
  perm_ofs = (get_ao_random2(get_ao_random1(x) + y)) & (MAX_NUMBER_OF_AO_RAYS - 1);

  /* importance sample shadow rays (cosine weighted) */
  for (i = 0; i < ao_data->number_of_rays; i++) {
    int hit_something;

    /* use N-Rooks to distribute our N ray samples across
     * a multi-dimensional domain (2D)
     */
    const ushort I =
        ao_data->permutation_table_1[(i + perm_ofs) % ao_data->number_of_rays];
    const ushort J = ao_data->permutation_table_2[i];

    const float JitPh = (get_ao_random2(I + perm_ofs) & (MAX_NUMBER_OF_AO_RAYS - 1)) /
                        float(MAX_NUMBER_OF_AO_RAYS);
    const float JitTh = (get_ao_random1(J + perm_ofs) & (MAX_NUMBER_OF_AO_RAYS - 1)) /
                        float(MAX_NUMBER_OF_AO_RAYS);
    const float SiSqPhi = (I + JitPh) / ao_data->number_of_rays;
    const float Theta = float(2 * M_PI) * ((J + JitTh) / ao_data->number_of_rays);

    /* this gives results identical to the so-called cosine
     * weighted distribution relative to the north pole.
     */
    float SiPhi = sqrtf(SiSqPhi);
    float CoPhi = SiSqPhi < 1.0f ? sqrtf(1.0f - SiSqPhi) : 0;
    float CoThe = cosf(Theta);
    float SiThe = sinf(Theta);

    const float dx = CoThe * CoPhi;
    const float dy = SiThe * CoPhi;
    const float dz = SiPhi;

    /* transform ray direction out of tangent frame */
    float dv[3];
    for (k = 0; k < 3; k++) {
      dv[k] = axisX[k] * dx + axisY[k] * dy + axisZ[k] * dz;
    }

    hit_something = trace_ao_ray(ao_data, cen, dv);

    if (hit_something != 0) {
      shadow += 1;
    }
  }

  value = 1.0f - (shadow / ao_data->number_of_rays);

  if (ibuf->rect_float) {
    float *rrgbf = ibuf->rect_float + pixel * 4;
    rrgbf[0] = rrgbf[1] = rrgbf[2] = value;
    rrgbf[3] = 1.0f;
  }
  else {
    uchar *rrgb = (uchar *)ibuf->rect + pixel * 4;
    rrgb[0] = rrgb[1] = rrgb[2] = unit_float_to_uchar_clamp(value);
    rrgb[3] = 255;
  }
}
#endif

/* ******$***************** Post processing ************************* */

static void bake_ibuf_filter(ImBuf *ibuf,
                             char *mask,
                             const int margin,
                             const char margin_type,
                             DerivedMesh *dm,
                             const float uv_offset[2])
{
  /* must check before filtering */
  const bool is_new_alpha = (ibuf->planes != R_IMF_PLANES_RGBA) && BKE_imbuf_alpha_test(ibuf);

  if (margin) {
    switch (margin_type) {
      case R_BAKE_ADJACENT_FACES:
        RE_generate_texturemargin_adjacentfaces_dm(ibuf, mask, margin, dm, uv_offset);
        break;
      default:
      /* fall through */
      case R_BAKE_EXTEND:
        IMB_filter_extend(ibuf, mask, margin);
        break;
    }
  }

  /* if the bake results in new alpha then change the image setting */
  if (is_new_alpha) {
    ibuf->planes = R_IMF_PLANES_RGBA;
  }
  else {
    if (margin && ibuf->planes != R_IMF_PLANES_RGBA) {
      /* clear alpha added by filtering */
      IMB_rectfill_alpha(ibuf, 1.0f);
    }
  }
}

static void bake_ibuf_normalize_displacement(ImBuf *ibuf,
                                             const float *displacement,
                                             const char *mask,
                                             float displacement_min,
                                             float displacement_max)
{
  int i;
  const float *current_displacement = displacement;
  const char *current_mask = mask;
  float max_distance;

  max_distance = max_ff(fabsf(displacement_min), fabsf(displacement_max));

  for (i = 0; i < ibuf->x * ibuf->y; i++) {
    if (*current_mask == FILTER_MASK_USED) {
      float normalized_displacement;

      if (max_distance > 1e-5f) {
        normalized_displacement = (*current_displacement + max_distance) / (max_distance * 2);
      }
      else {
        normalized_displacement = 0.5f;
      }

      if (ibuf->rect_float) {
        /* currently baking happens to RGBA only */
        float *fp = ibuf->rect_float + i * 4;
        fp[0] = fp[1] = fp[2] = normalized_displacement;
        fp[3] = 1.0f;
      }

      if (ibuf->rect) {
        uchar *cp = (uchar *)(ibuf->rect + i);
        cp[0] = cp[1] = cp[2] = unit_float_to_uchar_clamp(normalized_displacement);
        cp[3] = 255;
      }
    }

    current_displacement++;
    current_mask++;
  }
}

/* **************** Common functions public API relates on **************** */

static void count_images(MultiresBakeRender *bkr)
{
  BLI_listbase_clear(&bkr->image);
  bkr->tot_image = 0;

  for (int i = 0; i < bkr->ob_image.len; i++) {
    Image *ima = bkr->ob_image.array[i];
    if (ima) {
      ima->id.tag &= ~LIB_TAG_DOIT;
    }
  }

  for (int i = 0; i < bkr->ob_image.len; i++) {
    Image *ima = bkr->ob_image.array[i];
    if (ima) {
      if ((ima->id.tag & LIB_TAG_DOIT) == 0) {
        LinkData *data = BLI_genericNodeN(ima);
        BLI_addtail(&bkr->image, data);
        bkr->tot_image++;
        ima->id.tag |= LIB_TAG_DOIT;
      }
    }
  }

  for (int i = 0; i < bkr->ob_image.len; i++) {
    Image *ima = bkr->ob_image.array[i];
    if (ima) {
      ima->id.tag &= ~LIB_TAG_DOIT;
    }
  }
}

static void bake_images(MultiresBakeRender *bkr, MultiresBakeResult *result)
{
  LinkData *link;

  /* construct bake result */
  result->height_min = FLT_MAX;
  result->height_max = -FLT_MAX;

  for (link = static_cast<LinkData *>(bkr->image.first); link; link = link->next) {
    Image *ima = (Image *)link->data;

    LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
      ImageUser iuser;
      BKE_imageuser_default(&iuser);
      iuser.tile = tile->tile_number;

      ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);

      if (ibuf->x > 0 && ibuf->y > 0) {
        BakeImBufuserData *userdata = MEM_cnew<BakeImBufuserData>("MultiresBake userdata");
        userdata->mask_buffer = MEM_cnew_array<char>(ibuf->y * ibuf->x, "MultiresBake imbuf mask");
        ibuf->userdata = userdata;

        switch (bkr->mode) {
          case RE_BAKE_NORMALS:
            do_multires_bake(bkr,
                             ima,
                             tile,
                             ibuf,
                             true,
                             apply_tangmat_callback,
                             init_normal_data,
                             free_normal_data,
                             result);
            break;
          case RE_BAKE_DISPLACEMENT:
            do_multires_bake(bkr,
                             ima,
                             tile,
                             ibuf,
                             false,
                             apply_heights_callback,
                             init_heights_data,
                             free_heights_data,
                             result);
            break;
            /* TODO: restore ambient occlusion baking support. */
#if 0
          case RE_BAKE_AO:
            do_multires_bake(bkr, ima, tile, ibuf, false, apply_ao_callback, init_ao_data, free_ao_data, result);
            break;
#endif
        }
      }

      BKE_image_release_ibuf(ima, ibuf, nullptr);
    }

    ima->id.tag |= LIB_TAG_DOIT;
  }
}

static void finish_images(MultiresBakeRender *bkr, MultiresBakeResult *result)
{
  LinkData *link;
  bool use_displacement_buffer = bkr->mode == RE_BAKE_DISPLACEMENT;

  for (link = static_cast<LinkData *>(bkr->image.first); link; link = link->next) {
    Image *ima = (Image *)link->data;

    LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
      ImageUser iuser;
      BKE_imageuser_default(&iuser);
      iuser.tile = tile->tile_number;

      ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);
      BakeImBufuserData *userdata = (BakeImBufuserData *)ibuf->userdata;

      if (ibuf->x <= 0 || ibuf->y <= 0) {
        continue;
      }

      if (use_displacement_buffer) {
        bake_ibuf_normalize_displacement(ibuf,
                                         userdata->displacement_buffer,
                                         userdata->mask_buffer,
                                         result->height_min,
                                         result->height_max);
      }

      float uv_offset[2];
      BKE_image_get_tile_uv(ima, tile->tile_number, uv_offset);

      bake_ibuf_filter(ibuf,
                       userdata->mask_buffer,
                       bkr->bake_margin,
                       bkr->bake_margin_type,
                       bkr->lores_dm,
                       uv_offset);

      ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
      BKE_image_mark_dirty(ima, ibuf);

      if (ibuf->rect_float) {
        ibuf->userflags |= IB_RECT_INVALID;
      }

      if (ibuf->mipmap[0]) {
        ibuf->userflags |= IB_MIPMAP_INVALID;
        imb_freemipmapImBuf(ibuf);
      }

      if (ibuf->userdata) {
        if (userdata->displacement_buffer) {
          MEM_freeN(userdata->displacement_buffer);
        }

        MEM_freeN(userdata->mask_buffer);
        MEM_freeN(userdata);
        ibuf->userdata = nullptr;
      }

      BKE_image_release_ibuf(ima, ibuf, nullptr);
      DEG_id_tag_update(&ima->id, 0);
    }
  }
}

void RE_multires_bake_images(MultiresBakeRender *bkr)
{
  MultiresBakeResult result;

  count_images(bkr);
  bake_images(bkr, &result);
  finish_images(bkr, &result);
}
