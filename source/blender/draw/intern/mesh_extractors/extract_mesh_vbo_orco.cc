/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 */

#include "extract_mesh.h"

#include "draw_subdivision.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Orco
 * \{ */

struct MeshExtract_Orco_Data {
  float (*vbo_data)[4];
  const float (*orco)[3];
};

static void extract_orco_init(const MeshRenderData *mr,
                              struct MeshBatchCache *UNUSED(cache),
                              void *buf,
                              void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* FIXME(fclem): We use the last component as a way to differentiate from generic vertex
     * attributes. This is a substantial waste of video-ram and should be done another way.
     * Unfortunately, at the time of writing, I did not found any other "non disruptive"
     * alternative. */
    GPU_vertformat_attr_add(&format, "orco", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  CustomData *cd_vdata = &mr->me->vdata;

  MeshExtract_Orco_Data *data = static_cast<MeshExtract_Orco_Data *>(tls_data);
  data->vbo_data = (float(*)[4])GPU_vertbuf_get_data(vbo);
  data->orco = static_cast<const float(*)[3]>(CustomData_get_layer(cd_vdata, CD_ORCO));
  /* Make sure `orco` layer was requested only if needed! */
  BLI_assert(data->orco);
}

static void extract_orco_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                      const BMFace *f,
                                      const int UNUSED(f_index),
                                      void *data)
{
  MeshExtract_Orco_Data *orco_data = (MeshExtract_Orco_Data *)data;
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    float *loop_orco = orco_data->vbo_data[l_index];
    copy_v3_v3(loop_orco, orco_data->orco[BM_elem_index_get(l_iter->v)]);
    loop_orco[3] = 0.0; /* Tag as not a generic attribute. */
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_orco_iter_poly_mesh(const MeshRenderData *mr,
                                        const MPoly *mp,
                                        const int UNUSED(mp_index),
                                        void *data)
{
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];
    MeshExtract_Orco_Data *orco_data = (MeshExtract_Orco_Data *)data;
    float *loop_orco = orco_data->vbo_data[ml_index];
    copy_v3_v3(loop_orco, orco_data->orco[ml->v]);
    loop_orco[3] = 0.0; /* Tag as not a generic attribute. */
  }
}

static void extract_orco_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                     const MeshRenderData *mr,
                                     struct MeshBatchCache *UNUSED(cache),
                                     void *buffer,
                                     void *UNUSED(data))
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* FIXME(fclem): We use the last component as a way to differentiate from generic vertex
     * attributes. This is a substantial waste of video-ram and should be done another way.
     * Unfortunately, at the time of writing, I did not found any other "non disruptive"
     * alternative. */
    GPU_vertformat_attr_add(&format, "orco", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }

  GPUVertBuf *dst_buffer = static_cast<GPUVertBuf *>(buffer);
  GPU_vertbuf_init_build_on_device(dst_buffer, &format, subdiv_cache->num_subdiv_loops);

  GPUVertBuf *coarse_vbo = GPU_vertbuf_calloc();
  /* Dynamic as we upload and interpolate layers one at a time. */
  GPU_vertbuf_init_with_format_ex(coarse_vbo, &format, GPU_USAGE_DYNAMIC);
  GPU_vertbuf_data_alloc(coarse_vbo, mr->loop_len);

  float(*coarse_vbo_data)[4] = static_cast<float(*)[4]>(GPU_vertbuf_get_data(coarse_vbo));

  CustomData *cd_vdata = &mr->me->vdata;
  const float(*orco)[3] = static_cast<const float(*)[3]>(CustomData_get_layer(cd_vdata, CD_ORCO));

  if (mr->extract_type == MR_EXTRACT_MESH) {
    const MLoop *mloop = mr->mloop;
    const MPoly *mp = mr->mpoly;

    int ml_index = 0;
    for (int i = 0; i < mr->poly_len; i++, mp++) {
      const MLoop *ml = &mloop[mp->loopstart];

      for (int j = 0; j < mp->totloop; j++, ml++, ml_index++) {
        float *loop_orco = coarse_vbo_data[ml_index];
        copy_v3_v3(loop_orco, orco[ml->v]);
        loop_orco[3] = 0.0; /* Tag as not a generic attribute. */
      }
    }
  }
  else {
    BMIter iter;
    BMFace *f;
    BM_ITER_MESH (f, &iter, mr->bm, BM_FACES_OF_MESH) {
      BMLoop *l_iter;
      BMLoop *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        const int l_index = BM_elem_index_get(l_iter);
        float *loop_orco = coarse_vbo_data[l_index];
        copy_v3_v3(loop_orco, orco[BM_elem_index_get(l_iter->v)]);
        loop_orco[3] = 0.0; /* Tag as not a generic attribute. */
      } while ((l_iter = l_iter->next) != l_first);
    }
  }

  draw_subdiv_interp_custom_data(subdiv_cache, coarse_vbo, dst_buffer, 4, 0, false);

  GPU_vertbuf_discard(coarse_vbo);
}

constexpr MeshExtract create_extractor_orco()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_orco_init;
  extractor.iter_poly_bm = extract_orco_iter_poly_bm;
  extractor.iter_poly_mesh = extract_orco_iter_poly_mesh;
  extractor.init_subdiv = extract_orco_init_subdiv;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(MeshExtract_Orco_Data);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.orco);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_orco = blender::draw::create_extractor_orco();
}
