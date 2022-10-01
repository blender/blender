/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 *
 * PBVH drawing.
 * Embeds GPU meshes inside of PBVH nodes, used by mesh sculpt mode.
 */

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_ghash.h"
#include "BLI_math_color.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_attribute.h"
#include "BKE_ccg.h"
#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_subdiv_ccg.h"

#include "GPU_batch.h"

#include "DRW_engine.h"
#include "DRW_pbvh.h"

#include "bmesh.h"
#include "draw_pbvh.h"
#include "gpu_private.h"

#define MAX_PBVH_BATCH_KEY 512
#define MAX_PBVH_VBOS 16

#include "BLI_index_range.hh"
#include "BLI_map.hh"
#include "BLI_math_vec_types.hh"
#include "BLI_vector.hh"
#include <vector>

#include <algorithm>
#include <string>

using blender::char3;
using blender::float2;
using blender::float3;
using blender::float4;
using blender::IndexRange;
using blender::Map;
using blender::short3;
using blender::uchar3;
using blender::ushort3;
using blender::ushort4;
using blender::Vector;

using string = std::string;

struct PBVHVbo {
  uint64_t type;
  eAttrDomain domain;
  string name;
  GPUVertBuf *vert_buf = nullptr;
  string key;

  PBVHVbo(eAttrDomain _domain, uint64_t _type, string _name)
      : type(_type), domain(_domain), name(_name)
  {
  }

  void clear_data()
  {
    GPU_vertbuf_clear(vert_buf);
  }

  string build_key()
  {
    char buf[512];

    sprintf(buf, "%d:%d:%s", int(type), int(domain), name.c_str());

    key = string(buf);
    return key;
  }
};

struct PBVHBatch {
  Vector<int> vbos;
  string key;
  GPUBatch *tris = nullptr, *lines = nullptr;
  int tris_count = 0, lines_count = 0;

  void sort_vbos(Vector<PBVHVbo> &master_vbos)
  {
    struct cmp {
      Vector<PBVHVbo> &master_vbos;

      cmp(Vector<PBVHVbo> &_master_vbos) : master_vbos(_master_vbos)
      {
      }

      bool operator()(const int &a, const int &b)
      {
        return master_vbos[a].key < master_vbos[b].key;
      }
    };

    std::sort(vbos.begin(), vbos.end(), cmp(master_vbos));
  }

  string build_key(Vector<PBVHVbo> &master_vbos)
  {
    key = "";

    sort_vbos(master_vbos);

    for (int vbo_i : vbos) {
      key += master_vbos[vbo_i].key + ":";
    }

    return key;
  }
};

static CustomData *get_cdata(eAttrDomain domain, PBVH_GPU_Args *args)
{
  switch (domain) {
    case ATTR_DOMAIN_POINT:
      return args->vdata;
    case ATTR_DOMAIN_CORNER:
      return args->ldata;
    case ATTR_DOMAIN_FACE:
      return args->pdata;
    default:
      return nullptr;
  }
}

struct PBVHBatches {
  Vector<PBVHVbo> vbos;
  Map<string, PBVHBatch> batches;
  GPUIndexBuf *tri_index = nullptr;
  GPUIndexBuf *lines_index = nullptr;
  int faces_count = 0; /* Used by PBVH_BMESH and PBVH_GRIDS */
  int tris_count = 0, lines_count = 0;
  bool needs_tri_index = false;

  int material_index = 0;

  int count_faces(PBVH_GPU_Args *args)
  {
    int count = 0;

    switch (args->pbvh_type) {
      case PBVH_FACES: {
        for (int i = 0; i < args->totprim; i++) {
          int face_index = args->mlooptri[args->prim_indices[i]].poly;

          if (args->hide_poly && args->hide_poly[face_index]) {
            continue;
          }

          count++;
        }
        break;
      }
      case PBVH_GRIDS: {
        count = BKE_pbvh_count_grid_quads((BLI_bitmap **)args->grid_hidden,
                                          args->grid_indices,
                                          args->totprim,
                                          args->ccg_key.grid_size);

        break;
      }
      case PBVH_BMESH: {
        GSET_FOREACH_BEGIN (BMFace *, f, args->bm_faces) {
          if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
            count++;
          }
        }
        GSET_FOREACH_END();
      }
    }

    return count;
  }

  PBVHBatches(PBVH_GPU_Args *args)
  {
    faces_count = count_faces(args);

    if (args->pbvh_type == PBVH_BMESH) {
      tris_count = faces_count;
    }
  }

  ~PBVHBatches()
  {
    for (PBVHBatch &batch : batches.values()) {
      GPU_BATCH_DISCARD_SAFE(batch.tris);
      GPU_BATCH_DISCARD_SAFE(batch.lines);
    }

    for (PBVHVbo &vbo : vbos) {
      GPU_vertbuf_discard(vbo.vert_buf);
    }

    GPU_INDEXBUF_DISCARD_SAFE(tri_index);
    GPU_INDEXBUF_DISCARD_SAFE(lines_index);
  }

  string build_key(PBVHAttrReq *attrs, int attrs_num)
  {
    string key;
    PBVHBatch batch;
    Vector<PBVHVbo> vbos;

    for (int i : IndexRange(attrs_num)) {
      PBVHAttrReq *attr = attrs + i;

      PBVHVbo vbo(attr->domain, attr->type, string(attr->name));
      vbo.build_key();

      vbos.append(vbo);
      batch.vbos.append(i);
    }

    batch.build_key(vbos);
    return batch.key;
  }

  bool has_vbo(eAttrDomain domain, int type, string name)
  {
    for (PBVHVbo &vbo : vbos) {
      if (vbo.domain == domain && vbo.type == type && vbo.name == name) {
        return true;
      }
    }

    return false;
  }

  int get_vbo_index(PBVHVbo *vbo)
  {
    for (int i : IndexRange(vbos.size())) {
      if (vbo == &vbos[i]) {
        return i;
      }
    }

    return -1;
  }

  PBVHVbo *get_vbo(eAttrDomain domain, int type, string name)
  {
    for (PBVHVbo &vbo : vbos) {
      if (vbo.domain == domain && vbo.type == type && vbo.name == name) {
        return &vbo;
      }
    }

    return nullptr;
  }

  bool has_batch(PBVHAttrReq *attrs, int attrs_num)
  {
    return batches.contains(build_key(attrs, attrs_num));
  }

  PBVHBatch &ensure_batch(PBVHAttrReq *attrs, int attrs_num, PBVH_GPU_Args *args)
  {
    if (!has_batch(attrs, attrs_num)) {
      create_batch(attrs, attrs_num, args);
    }

    return batches.lookup(build_key(attrs, attrs_num));
  }

  void fill_vbo_normal_faces(
      PBVHVbo & /*vbo*/,
      PBVH_GPU_Args *args,
      std::function<void(std::function<void(int, int, int, const MLoopTri *)> callback)>
          foreach_faces,
      GPUVertBufRaw *access)
  {
    float fno[3];
    short no[3];
    int last_poly = -1;
    bool smooth = false;

    foreach_faces([&](int /*buffer_i*/, int /*tri_i*/, int vertex_i, const MLoopTri *tri) {
      const MPoly *mp = args->mpoly + tri->poly;

      if (tri->poly != last_poly) {
        last_poly = tri->poly;

        if (!(mp->flag & ME_SMOOTH)) {
          smooth = true;
          BKE_mesh_calc_poly_normal(mp, args->mloop + mp->loopstart, args->mvert, fno);
          normal_float_to_short_v3(no, fno);
        }
        else {
          smooth = false;
        }
      }

      if (!smooth) {
        normal_float_to_short_v3(no, args->vert_normals[vertex_i]);
      }

      *static_cast<short3 *>(GPU_vertbuf_raw_step(access)) = no;
    });
  }

  void fill_vbo_grids_intern(
      PBVHVbo &vbo,
      PBVH_GPU_Args *args,
      std::function<
          void(std::function<void(int x, int y, int grid_index, CCGElem *elems[4], int i)> func)>
          foreach_grids)
  {
    uint vert_per_grid = square_i(args->ccg_key.grid_size - 1) * 4;
    uint vert_count = args->totprim * vert_per_grid;

    int existing_num = GPU_vertbuf_get_vertex_len(vbo.vert_buf);
    void *existing_data = GPU_vertbuf_get_data(vbo.vert_buf);

    if (existing_data == nullptr || existing_num != vert_count) {
      /* Allocate buffer if not allocated yet or size changed. */
      GPU_vertbuf_data_alloc(vbo.vert_buf, vert_count);
    }

    GPUVertBufRaw access;
    GPU_vertbuf_attr_get_raw_data(vbo.vert_buf, 0, &access);

    switch (vbo.type) {
      case CD_PROP_COLOR:
      case CD_PROP_BYTE_COLOR: {
        /* TODO: Implement color support for multires similar to the mesh cache
         * extractor code. For now just upload white.
         */
        const ushort4 white(USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX);

        foreach_grids(
            [&](int /*x*/, int /*y*/, int /*grid_index*/, CCGElem * /*elems*/[4], int /*i*/) {
              *static_cast<ushort4 *>(GPU_vertbuf_raw_step(&access)) = white;
            });
        break;
      }
      case CD_PBVH_CO_TYPE:
        foreach_grids([&](int /*x*/, int /*y*/, int /*grid_index*/, CCGElem *elems[4], int i) {
          float *co = CCG_elem_co(&args->ccg_key, elems[i]);

          *static_cast<float3 *>(GPU_vertbuf_raw_step(&access)) = co;
        });
        break;

      case CD_PBVH_NO_TYPE:
        foreach_grids([&](int /*x*/, int /*y*/, int grid_index, CCGElem *elems[4], int /*i*/) {
          float3 no(0.0f, 0.0f, 0.0f);

          const bool smooth = args->grid_flag_mats[grid_index].flag & ME_SMOOTH;

          if (smooth) {
            no = CCG_elem_no(&args->ccg_key, elems[0]);
          }
          else {
            for (int j = 0; j < 4; j++) {
              no += CCG_elem_no(&args->ccg_key, elems[j]);
            }
          }

          normalize_v3(no);
          short sno[3];

          normal_float_to_short_v3(sno, no);

          *static_cast<short3 *>(GPU_vertbuf_raw_step(&access)) = sno;
        });
        break;

      case CD_PBVH_MASK_TYPE:
        foreach_grids([&](int /*x*/, int /*y*/, int /*grid_index*/, CCGElem *elems[4], int i) {
          float *mask = CCG_elem_mask(&args->ccg_key, elems[i]);

          *static_cast<uchar *>(GPU_vertbuf_raw_step(&access)) = mask ? uchar(*mask * 255.0f) :
                                                                        255;
        });
        break;

      case CD_PBVH_FSET_TYPE: {
        int *face_sets = args->face_sets;

        if (!face_sets) {
          uchar white[3] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};

          foreach_grids(
              [&](int /*x*/, int /*y*/, int /*grid_index*/, CCGElem * /*elems*/[4], int /*i*/) {
                *static_cast<uchar3 *>(GPU_vertbuf_raw_step(&access)) = white;
              });
        }
        else {
          foreach_grids(
              [&](int /*x*/, int /*y*/, int grid_index, CCGElem * /*elems*/[4], int /*i*/) {
                uchar face_set_color[4] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};

                if (face_sets) {
                  const int face_index = BKE_subdiv_ccg_grid_to_face_index(args->subdiv_ccg,
                                                                           grid_index);
                  const int fset = face_sets[face_index];

                  /* Skip for the default color Face Set to render it white. */
                  if (fset != args->face_sets_color_default) {
                    BKE_paint_face_set_overlay_color_get(
                        fset, args->face_sets_color_seed, face_set_color);
                  }
                }

                *static_cast<uchar3 *>(GPU_vertbuf_raw_step(&access)) = face_set_color;
              });
        }
        break;
      }
    }
  }

  void fill_vbo_grids(PBVHVbo &vbo, PBVH_GPU_Args *args)
  {
    int gridsize = args->ccg_key.grid_size;

    uint totgrid = args->totprim;

    auto foreach_solid =
        [&](std::function<void(int x, int y, int grid_index, CCGElem *elems[4], int i)> func) {
          for (int i = 0; i < totgrid; i++) {
            const int grid_index = args->grid_indices[i];

            CCGElem *grid = args->grids[grid_index];

            for (int y = 0; y < gridsize - 1; y++) {
              for (int x = 0; x < gridsize - 1; x++) {
                CCGElem *elems[4] = {
                    CCG_grid_elem(&args->ccg_key, grid, x, y),
                    CCG_grid_elem(&args->ccg_key, grid, x + 1, y),
                    CCG_grid_elem(&args->ccg_key, grid, x + 1, y + 1),
                    CCG_grid_elem(&args->ccg_key, grid, x, y + 1),
                };

                func(x, y, grid_index, elems, 0);
                func(x + 1, y, grid_index, elems, 1);
                func(x + 1, y + 1, grid_index, elems, 2);
                func(x, y + 1, grid_index, elems, 3);
              }
            }
          }
        };

    auto foreach_indexed =
        [&](std::function<void(int x, int y, int grid_index, CCGElem *elems[4], int i)> func) {
          for (int i = 0; i < totgrid; i++) {
            const int grid_index = args->grid_indices[i];

            CCGElem *grid = args->grids[grid_index];

            for (int y = 0; y < gridsize; y++) {
              for (int x = 0; x < gridsize; x++) {
                CCGElem *elems[4] = {
                    CCG_grid_elem(&args->ccg_key, grid, x, y),
                    CCG_grid_elem(&args->ccg_key, grid, x + 1, y),
                    CCG_grid_elem(&args->ccg_key, grid, x + 1, y + 1),
                    CCG_grid_elem(&args->ccg_key, grid, x, y + 1),
                };

                func(x, y, grid_index, elems, 0);
              }
            }
          }
        };

    if (needs_tri_index) {
      fill_vbo_grids_intern(vbo, args, foreach_indexed);
    }
    else {
      fill_vbo_grids_intern(vbo, args, foreach_solid);
    }
  }

  void fill_vbo_faces(PBVHVbo &vbo, PBVH_GPU_Args *args)
  {
    auto foreach_faces =
        [&](std::function<void(int buffer_i, int tri_i, int vertex_i, const MLoopTri *tri)> func) {
          int buffer_i = 0;
          const MLoop *mloop = args->mloop;

          for (int i : IndexRange(args->totprim)) {
            int face_index = args->mlooptri[args->prim_indices[i]].poly;

            if (args->hide_poly && args->hide_poly[face_index]) {
              continue;
            }

            const MLoopTri *tri = args->mlooptri + args->prim_indices[i];

            for (int j : IndexRange(3)) {
              func(buffer_i, j, mloop[tri->tri[j]].v, tri);
              buffer_i++;
            }
          }
        };

    int totvert = 0;
    foreach_faces([&totvert](int, int, int, const MLoopTri *) { totvert++; });

    int existing_num = GPU_vertbuf_get_vertex_len(vbo.vert_buf);
    void *existing_data = GPU_vertbuf_get_data(vbo.vert_buf);

    if (existing_data == nullptr || existing_num != totvert) {
      /* Allocate buffer if not allocated yet or size changed. */
      GPU_vertbuf_data_alloc(vbo.vert_buf, totvert);
    }

    GPUVertBufRaw access;
    GPU_vertbuf_attr_get_raw_data(vbo.vert_buf, 0, &access);

    switch (vbo.type) {
      case CD_PBVH_CO_TYPE:
        foreach_faces(
            [&](int /*buffer_i*/, int /*tri_i*/, int vertex_i, const MLoopTri * /*tri*/) {
              *static_cast<float3 *>(GPU_vertbuf_raw_step(&access)) = args->mvert[vertex_i].co;
            });
        break;
      case CD_PBVH_NO_TYPE:
        fill_vbo_normal_faces(vbo, args, foreach_faces, &access);
        break;
      case CD_PBVH_MASK_TYPE: {
        float *mask = static_cast<float *>(CustomData_get_layer(args->vdata, CD_PAINT_MASK));

        if (mask) {
          foreach_faces(
              [&](int /*buffer_i*/, int /*tri_i*/, int vertex_i, const MLoopTri * /*tri*/) {
                *static_cast<uchar *>(GPU_vertbuf_raw_step(&access)) = uchar(mask[vertex_i] *
                                                                             255.0f);
              });
        }
        else {
          foreach_faces(
              [&](int /*buffer_i*/, int /*tri_i*/, int /*vertex_i*/, const MLoopTri * /*tri*/) {
                *static_cast<uchar *>(GPU_vertbuf_raw_step(&access)) = 0;
              });
        }
        break;
      }
      case CD_PBVH_FSET_TYPE: {
        int *face_sets = static_cast<int *>(
            CustomData_get_layer_named(args->pdata, CD_PROP_INT32, ".sculpt_face_set"));

        if (face_sets) {
          int last_poly = -1;
          uchar fset_color[4] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};

          foreach_faces(
              [&](int /*buffer_i*/, int /*tri_i*/, int /*vertex_i*/, const MLoopTri *tri) {
                if (last_poly != tri->poly) {
                  last_poly = tri->poly;

                  const int fset = face_sets[tri->poly];

                  if (fset != args->face_sets_color_default) {
                    BKE_paint_face_set_overlay_color_get(
                        fset, args->face_sets_color_seed, fset_color);
                  }
                  else {
                    /* Skip for the default color face set to render it white. */
                    fset_color[0] = fset_color[1] = fset_color[2] = UCHAR_MAX;
                  }
                }

                *static_cast<uchar3 *>(GPU_vertbuf_raw_step(&access)) = fset_color;
              });
        }
        else {
          uchar fset_color[4] = {255, 255, 255, 255};

          foreach_faces(
              [&](int /*buffer_i*/, int /*tri_i*/, int /*vertex_i*/, const MLoopTri * /*tri*/) {
                *static_cast<uchar3 *>(GPU_vertbuf_raw_step(&access)) = fset_color;
              });
        }

        break;
      }
      case CD_MLOOPUV: {
        MLoopUV *mloopuv = static_cast<MLoopUV *>(
            CustomData_get_layer_named(args->ldata, CD_MLOOPUV, vbo.name.c_str()));

        foreach_faces([&](int /*buffer_i*/, int tri_i, int /*vertex_i*/, const MLoopTri *tri) {
          *static_cast<float2 *>(GPU_vertbuf_raw_step(&access)) = mloopuv[tri->tri[tri_i]].uv;
        });
        break;
      }
      case CD_PROP_COLOR:
        if (vbo.domain == ATTR_DOMAIN_POINT) {
          MPropCol *mpropcol = static_cast<MPropCol *>(
              CustomData_get_layer_named(args->vdata, CD_PROP_COLOR, vbo.name.c_str()));

          foreach_faces(
              [&](int /*buffer_i*/, int /*tri_i*/, int vertex_i, const MLoopTri * /*tri*/) {
                ushort color[4];
                MPropCol *col = mpropcol + vertex_i;

                color[0] = unit_float_to_ushort_clamp(col->color[0]);
                color[1] = unit_float_to_ushort_clamp(col->color[1]);
                color[2] = unit_float_to_ushort_clamp(col->color[2]);
                color[3] = unit_float_to_ushort_clamp(col->color[3]);

                *static_cast<ushort4 *>(GPU_vertbuf_raw_step(&access)) = color;
              });
        }
        else if (vbo.domain == ATTR_DOMAIN_CORNER) {
          MPropCol *mpropcol = static_cast<MPropCol *>(
              CustomData_get_layer_named(args->ldata, CD_PROP_COLOR, vbo.name.c_str()));

          foreach_faces([&](int /*buffer_i*/, int tri_i, int /*vertex_i*/, const MLoopTri *tri) {
            ushort color[4];
            MPropCol *col = mpropcol + tri->tri[tri_i];

            color[0] = unit_float_to_ushort_clamp(col->color[0]);
            color[1] = unit_float_to_ushort_clamp(col->color[1]);
            color[2] = unit_float_to_ushort_clamp(col->color[2]);
            color[3] = unit_float_to_ushort_clamp(col->color[3]);

            *static_cast<ushort4 *>(GPU_vertbuf_raw_step(&access)) = color;
          });
        }
        break;
      case CD_PROP_BYTE_COLOR:
        if (vbo.domain == ATTR_DOMAIN_POINT) {
          MLoopCol *mbytecol = static_cast<MLoopCol *>(
              CustomData_get_layer_named(args->vdata, CD_PROP_BYTE_COLOR, vbo.name.c_str()));

          foreach_faces(
              [&](int /*buffer_i*/, int /*tri_i*/, int vertex_i, const MLoopTri * /*tri*/) {
                ushort color[4];
                MLoopCol *col = mbytecol + vertex_i;

                color[0] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[col->r]);
                color[1] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[col->g]);
                color[2] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[col->b]);
                color[3] = col->a * 257;

                *static_cast<ushort4 *>(GPU_vertbuf_raw_step(&access)) = color;
              });
        }
        else if (vbo.domain == ATTR_DOMAIN_CORNER) {
          MLoopCol *mbytecol = static_cast<MLoopCol *>(
              CustomData_get_layer_named(args->ldata, CD_PROP_BYTE_COLOR, vbo.name.c_str()));

          foreach_faces([&](int /*buffer_i*/, int tri_i, int /*vertex_i*/, const MLoopTri *tri) {
            ushort color[4];
            MLoopCol *col = mbytecol + tri->tri[tri_i];

            color[0] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[col->r]);
            color[1] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[col->g]);
            color[2] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[col->b]);
            color[3] = col->a * 257;

            *static_cast<ushort4 *>(GPU_vertbuf_raw_step(&access)) = color;
          });
        }
        break;
    }
  }

  void gpu_flush()
  {
    for (PBVHVbo &vbo : vbos) {
      if (vbo.vert_buf && GPU_vertbuf_get_data(vbo.vert_buf)) {
        GPU_vertbuf_use(vbo.vert_buf);
      }
    }
  }

  void update(PBVH_GPU_Args *args)
  {
    check_index_buffers(args);

    for (PBVHVbo &vbo : vbos) {
      fill_vbo(vbo, args);
    }
  }

  void fill_vbo_bmesh(PBVHVbo &vbo, PBVH_GPU_Args *args)
  {
    auto foreach_bmesh = [&](std::function<void(BMLoop * l)> callback) {
      GSET_FOREACH_BEGIN (BMFace *, f, args->bm_faces) {
        if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
          continue;
        }

        BMLoop *l = f->l_first;
        do {
          callback(l);
        } while ((l = l->next) != f->l_first);
      }
      GSET_FOREACH_END();
    };

    faces_count = 0;
    GSET_FOREACH_BEGIN (BMFace *, f, args->bm_faces) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }

      BMLoop *l = f->l_first;
      do {
        faces_count++;
      } while ((l = l->next) != f->l_first);
    }
    GSET_FOREACH_END();
    tris_count = faces_count;

    int existing_num = GPU_vertbuf_get_vertex_len(vbo.vert_buf);
    void *existing_data = GPU_vertbuf_get_data(vbo.vert_buf);

    int vert_count = tris_count * 3;

    if (existing_data == nullptr || existing_num != vert_count) {
      /* Allocate buffer if not allocated yet or size changed. */
      GPU_vertbuf_data_alloc(vbo.vert_buf, vert_count);
    }

    GPUVertBufRaw access;
    GPU_vertbuf_attr_get_raw_data(vbo.vert_buf, 0, &access);

    switch (vbo.type) {
      case CD_PBVH_CO_TYPE:
        foreach_bmesh(
            [&](BMLoop *l) { *static_cast<float3 *>(GPU_vertbuf_raw_step(&access)) = l->v->co; });
        break;

      case CD_PBVH_NO_TYPE:
        foreach_bmesh([&](BMLoop *l) {
          short no[3];
          bool smooth = BM_elem_flag_test(l->f, BM_ELEM_SMOOTH);

          normal_float_to_short_v3(no, smooth ? l->v->no : l->f->no);
          *static_cast<short3 *>(GPU_vertbuf_raw_step(&access)) = no;
        });
        break;

      case CD_PBVH_MASK_TYPE: {
        int cd_mask = args->cd_mask_layer;

        if (cd_mask == -1) {
          foreach_bmesh(
              [&](BMLoop * /*l*/) { *static_cast<float *>(GPU_vertbuf_raw_step(&access)) = 0; });
        }
        else {
          foreach_bmesh([&](BMLoop *l) {
            float mask = BM_ELEM_CD_GET_FLOAT(l->v, cd_mask);

            *static_cast<uchar *>(GPU_vertbuf_raw_step(&access)) = uchar(mask * 255.0f);
          });
        }
        break;
      }
      case CD_PBVH_FSET_TYPE: {
        uchar3 white(UCHAR_MAX, UCHAR_MAX, UCHAR_MAX);

        foreach_bmesh([&](BMLoop * /*l*/) {
          *static_cast<uchar3 *>(GPU_vertbuf_raw_step(&access)) = white;
        });
      }
    }
  }

  void fill_vbo(PBVHVbo &vbo, PBVH_GPU_Args *args)
  {
    switch (args->pbvh_type) {
      case PBVH_FACES:
        fill_vbo_faces(vbo, args);
        break;
      case PBVH_GRIDS:
        fill_vbo_grids(vbo, args);
        break;
      case PBVH_BMESH:
        fill_vbo_bmesh(vbo, args);
        break;
    }
  }

  void create_vbo(eAttrDomain domain, const uint32_t type, string name, PBVH_GPU_Args *args)
  {
    PBVHVbo vbo(domain, type, name);
    GPUVertFormat format;

    bool need_aliases = !ELEM(
        type, CD_PBVH_CO_TYPE, CD_PBVH_NO_TYPE, CD_PBVH_FSET_TYPE, CD_PBVH_MASK_TYPE);

    GPU_vertformat_clear(&format);

    switch (type) {
      case CD_PBVH_CO_TYPE:
        GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
        break;
      case CD_PROP_FLOAT3:
        GPU_vertformat_attr_add(&format, "a", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
        need_aliases = true;
        break;
      case CD_PBVH_NO_TYPE:
        GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I16, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
        break;
      case CD_PROP_FLOAT2:
        GPU_vertformat_attr_add(&format, "a", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
        need_aliases = true;
        break;
      case CD_MLOOPUV:
        GPU_vertformat_attr_add(&format, "uvs", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
        need_aliases = true;
        break;
      case CD_PBVH_FSET_TYPE:
        GPU_vertformat_attr_add(&format, "fset", GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
        break;
      case CD_PBVH_MASK_TYPE:
        GPU_vertformat_attr_add(&format, "msk", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
        break;
      case CD_PROP_FLOAT:
        GPU_vertformat_attr_add(&format, "f", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
        need_aliases = true;
        break;
      case CD_PROP_COLOR:
      case CD_PROP_BYTE_COLOR: {
        GPU_vertformat_attr_add(&format, "c", GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
        need_aliases = true;
        break;
      }
      default:
        BLI_assert(0);
        printf("%s: error\n", __func__);

        break;
    }

    if (need_aliases) {
      CustomData *cdata = get_cdata(domain, args);
      int layer_i = cdata ? CustomData_get_named_layer_index(cdata, type, name.c_str()) : -1;
      CustomDataLayer *layer = layer_i != -1 ? cdata->layers + layer_i : nullptr;

      if (layer) {
        bool is_render, is_active;
        const char *prefix = "a";

        if (ELEM(type, CD_PROP_COLOR, CD_PROP_BYTE_COLOR)) {
          prefix = "c";

          CustomDataLayer *render = BKE_id_attributes_render_color_get(&args->me->id);
          CustomDataLayer *active = BKE_id_attributes_active_color_get(&args->me->id);

          is_render = render && layer && STREQ(render->name, layer->name);
          is_active = active && layer && STREQ(active->name, layer->name);
        }
        else {
          switch (type) {
            case CD_MLOOPUV:
              prefix = "u";
              break;
            default:
              break;
          }

          const char *active_name = CustomData_get_active_layer_name(cdata, type);
          const char *render_name = CustomData_get_render_layer_name(cdata, type);

          is_active = active_name && STREQ(layer->name, active_name);
          is_render = render_name && STREQ(layer->name, render_name);
        }

        DRW_cdlayer_attr_aliases_add(&format, prefix, cdata, layer, is_render, is_active);
      }
      else {
        printf("%s: error looking up attribute %s\n", __func__, name.c_str());
      }
    }

    vbo.vert_buf = GPU_vertbuf_create_with_format_ex(&format, GPU_USAGE_STATIC);
    vbo.build_key();
    fill_vbo(vbo, args);

    vbos.append(vbo);
  }

  void update_pre(PBVH_GPU_Args *args)
  {
    if (args->pbvh_type == PBVH_BMESH) {
      int count = count_faces(args);

      if (faces_count != count) {
        for (PBVHVbo &vbo : vbos) {
          vbo.clear_data();
        }

        GPU_INDEXBUF_DISCARD_SAFE(tri_index);
        GPU_INDEXBUF_DISCARD_SAFE(lines_index);

        tri_index = lines_index = nullptr;
        faces_count = tris_count = count;
      }
    }
  }

  void create_index_faces(PBVH_GPU_Args *args)
  {
    /* Calculate number of edges*/
    int edge_count = 0;
    for (int i = 0; i < args->totprim; i++) {
      const MLoopTri *lt = args->mlooptri + args->prim_indices[i];

      if (args->hide_poly && args->hide_poly[lt->poly]) {
        continue;
      }
      int r_edges[3];
      BKE_mesh_looptri_get_real_edges(args->me, lt, r_edges);

      if (r_edges[0] != -1) {
        edge_count++;
      }
      if (r_edges[1] != -1) {
        edge_count++;
      }
      if (r_edges[2] != -1) {
        edge_count++;
      }
    }

    GPUIndexBufBuilder elb_lines;
    GPU_indexbuf_init(&elb_lines, GPU_PRIM_LINES, edge_count * 2, INT_MAX);

    int vertex_i = 0;
    for (int i = 0; i < args->totprim; i++) {
      const MLoopTri *lt = args->mlooptri + args->prim_indices[i];

      if (args->hide_poly && args->hide_poly[lt->poly]) {
        continue;
      }

      int r_edges[3];
      BKE_mesh_looptri_get_real_edges(args->me, lt, r_edges);

      if (r_edges[0] != -1) {
        GPU_indexbuf_add_line_verts(&elb_lines, vertex_i, vertex_i + 1);
      }
      if (r_edges[1] != -1) {
        GPU_indexbuf_add_line_verts(&elb_lines, vertex_i + 1, vertex_i + 2);
      }
      if (r_edges[2] != -1) {
        GPU_indexbuf_add_line_verts(&elb_lines, vertex_i + 2, vertex_i);
      }

      vertex_i += 3;
    }

    lines_index = GPU_indexbuf_build(&elb_lines);
  }

  void create_index_bmesh(PBVH_GPU_Args *args)
  {
    GPUIndexBufBuilder elb_lines;
    GPU_indexbuf_init(&elb_lines, GPU_PRIM_LINES, tris_count * 3 * 2, INT_MAX);

    int v_index = 0;
    lines_count = 0;

    GSET_FOREACH_BEGIN (BMFace *, f, args->bm_faces) {
      if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        continue;
      }

      GPU_indexbuf_add_line_verts(&elb_lines, v_index, v_index + 1);
      GPU_indexbuf_add_line_verts(&elb_lines, v_index + 1, v_index + 2);
      GPU_indexbuf_add_line_verts(&elb_lines, v_index + 2, v_index);

      lines_count += 3;
      v_index += 3;
    }
    GSET_FOREACH_END();

    lines_index = GPU_indexbuf_build(&elb_lines);
  }

  void create_index_grids(PBVH_GPU_Args *args)
  {
    needs_tri_index = true;
    int gridsize = args->ccg_key.grid_size;
    int totgrid = args->totprim;

    for (int i : IndexRange(args->totprim)) {
      int grid_index = args->grid_indices[i];
      bool smooth = args->grid_flag_mats[grid_index].flag & ME_SMOOTH;
      BLI_bitmap *gh = args->grid_hidden[grid_index];

      for (int y = 0; y < gridsize - 1; y++) {
        for (int x = 0; x < gridsize - 1; x++) {
          if (gh && paint_is_grid_face_hidden(gh, gridsize, x, y)) {
            /* Skip hidden faces by just setting smooth to true. */
            smooth = true;
            goto outer_loop_break;
          }
        }
      }

    outer_loop_break:

      if (!smooth) {
        needs_tri_index = false;
        break;
      }
    }

    GPUIndexBufBuilder elb, elb_lines;

    CCGKey *key = &args->ccg_key;

    uint visible_quad_len = BKE_pbvh_count_grid_quads(
        (BLI_bitmap **)args->grid_hidden, args->grid_indices, totgrid, key->grid_size);

    GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, 2 * visible_quad_len, INT_MAX);
    GPU_indexbuf_init(
        &elb_lines, GPU_PRIM_LINES, 2 * totgrid * gridsize * (gridsize - 1), INT_MAX);

    if (needs_tri_index) {
      uint offset = 0;
      const uint grid_vert_len = gridsize * gridsize;
      for (int i = 0; i < totgrid; i++, offset += grid_vert_len) {
        uint v0, v1, v2, v3;
        bool grid_visible = false;

        BLI_bitmap *gh = args->grid_hidden[args->grid_indices[i]];

        for (int j = 0; j < gridsize - 1; j++) {
          for (int k = 0; k < gridsize - 1; k++) {
            /* Skip hidden grid face */
            if (gh && paint_is_grid_face_hidden(gh, gridsize, k, j)) {
              continue;
            }
            /* Indices in a Clockwise QUAD disposition. */
            v0 = offset + j * gridsize + k;
            v1 = v0 + 1;
            v2 = v1 + gridsize;
            v3 = v2 - 1;

            GPU_indexbuf_add_tri_verts(&elb, v0, v2, v1);
            GPU_indexbuf_add_tri_verts(&elb, v0, v3, v2);

            GPU_indexbuf_add_line_verts(&elb_lines, v0, v1);
            GPU_indexbuf_add_line_verts(&elb_lines, v0, v3);

            if (j + 2 == gridsize) {
              GPU_indexbuf_add_line_verts(&elb_lines, v2, v3);
            }
            grid_visible = true;
          }

          if (grid_visible) {
            GPU_indexbuf_add_line_verts(&elb_lines, v1, v2);
          }
        }
      }
    }
    else {
      uint offset = 0;
      const uint grid_vert_len = square_uint(gridsize - 1) * 4;
      for (int i = 0; i < totgrid; i++, offset += grid_vert_len) {
        bool grid_visible = false;
        BLI_bitmap *gh = args->grid_hidden[args->grid_indices[i]];

        uint v0, v1, v2, v3;
        for (int j = 0; j < gridsize - 1; j++) {
          for (int k = 0; k < gridsize - 1; k++) {
            /* Skip hidden grid face */
            if (gh && paint_is_grid_face_hidden(gh, gridsize, k, j)) {
              continue;
            }
            /* VBO data are in a Clockwise QUAD disposition. */
            v0 = offset + (j * (gridsize - 1) + k) * 4;
            v1 = v0 + 1;
            v2 = v0 + 2;
            v3 = v0 + 3;

            GPU_indexbuf_add_tri_verts(&elb, v0, v2, v1);
            GPU_indexbuf_add_tri_verts(&elb, v0, v3, v2);

            GPU_indexbuf_add_line_verts(&elb_lines, v0, v1);
            GPU_indexbuf_add_line_verts(&elb_lines, v0, v3);

            if (j + 2 == gridsize) {
              GPU_indexbuf_add_line_verts(&elb_lines, v2, v3);
            }
            grid_visible = true;
          }

          if (grid_visible) {
            GPU_indexbuf_add_line_verts(&elb_lines, v1, v2);
          }
        }
      }
    }

    tri_index = GPU_indexbuf_build(&elb);
    lines_index = GPU_indexbuf_build(&elb_lines);
  }

  void create_index(PBVH_GPU_Args *args)
  {
    switch (args->pbvh_type) {
      case PBVH_FACES:
        create_index_faces(args);
        break;
      case PBVH_BMESH:
        create_index_bmesh(args);
        break;
      case PBVH_GRIDS:
        create_index_grids(args);
        break;
    }

    for (PBVHBatch &batch : batches.values()) {
      GPU_batch_elembuf_set(batch.tris, tri_index, false);
      GPU_batch_elembuf_set(batch.lines, lines_index, false);
    }
  }

  void check_index_buffers(PBVH_GPU_Args *args)
  {
    if (!lines_index) {
      create_index(args);
    }
  }

  void create_batch(PBVHAttrReq *attrs, int attrs_num, PBVH_GPU_Args *args)
  {
    check_index_buffers(args);

    PBVHBatch batch;

    batch.tris = GPU_batch_create(GPU_PRIM_TRIS,
                                  nullptr,
                                  /* can be nullptr if buffer is empty */
                                  tri_index);
    batch.tris_count = tris_count;

    if (lines_index) {
      batch.lines = GPU_batch_create(GPU_PRIM_LINES, nullptr, lines_index);
      batch.lines_count = lines_count;
    }

    for (int i : IndexRange(attrs_num)) {
      PBVHAttrReq *attr = attrs + i;

      if (!has_vbo(attr->domain, int(attr->type), attr->name)) {
        create_vbo(attr->domain, uint32_t(attr->type), attr->name, args);
      }

      PBVHVbo *vbo = get_vbo(attr->domain, uint32_t(attr->type), attr->name);
      int vbo_i = get_vbo_index(vbo);

      batch.vbos.append(vbo_i);
      GPU_batch_vertbuf_add_ex(batch.tris, vbo->vert_buf, false);

      if (batch.lines) {
        GPU_batch_vertbuf_add_ex(batch.lines, vbo->vert_buf, false);
      }
    }

    batch.build_key(vbos);
    batches.add(batch.key, batch);
  }
};

void DRW_pbvh_node_update(PBVHBatches *batches, PBVH_GPU_Args *args)
{
  batches->update(args);
}

void DRW_pbvh_node_gpu_flush(PBVHBatches *batches)
{
  batches->gpu_flush();
}

PBVHBatches *DRW_pbvh_node_create(PBVH_GPU_Args *args)
{
  PBVHBatches *batches = new PBVHBatches(args);
  return batches;
}

void DRW_pbvh_node_free(PBVHBatches *batches)
{
  delete batches;
}

GPUBatch *DRW_pbvh_tris_get(PBVHBatches *batches,
                            PBVHAttrReq *attrs,
                            int attrs_num,
                            PBVH_GPU_Args *args,
                            int *r_prim_count)
{
  PBVHBatch &batch = batches->ensure_batch(attrs, attrs_num, args);

  *r_prim_count = batch.tris_count;

  return batch.tris;
}

GPUBatch *DRW_pbvh_lines_get(PBVHBatches *batches,
                             PBVHAttrReq *attrs,
                             int attrs_num,
                             PBVH_GPU_Args *args,
                             int *r_prim_count)
{
  PBVHBatch &batch = batches->ensure_batch(attrs, attrs_num, args);

  *r_prim_count = batch.lines_count;

  return batch.lines;
}

void DRW_pbvh_update_pre(struct PBVHBatches *batches, struct PBVH_GPU_Args *args)
{
  batches->update_pre(args);
}

int drw_pbvh_material_index_get(struct PBVHBatches *batches)
{
  return batches->material_index;
}
