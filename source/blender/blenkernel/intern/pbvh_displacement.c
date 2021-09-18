#if 0
#  include "MEM_guardedalloc.h"

#  include "BLI_alloca.h"
#  include "BLI_array.h"
#  include "BLI_compiler_attrs.h"
#  include "BLI_compiler_compat.h"
#  include "BLI_ghash.h"
#  include "BLI_linklist.h"
#  include "BLI_math.h"
#  include "BLI_memarena.h"
#  include "BLI_memblock.h"
#  include "BLI_mempool.h"
#  include "BLI_utildefines.h"

#  include "BLI_hash.h"

#  include "BKE_context.h"
#  include "BKE_global.h"
#  include "BKE_image.h"
#  include "BKE_mesh.h"
#  include "BKE_multires.h"
#  include "BKE_object.h"
#  include "BKE_pbvh.h"
#  include "BKE_scene.h"

#  include "BLI_bitmap.h"
#  include "DNA_customdata_types.h"
#  include "DNA_image_types.h"
#  include "DNA_material_types.h"
#  include "DNA_mesh_types.h"
#  include "DNA_meshdata_types.h"
#  include "DNA_object_types.h"
#  include "DNA_scene_types.h"

#  include "pbvh_intern.h"

#  include "bmesh.h"

void *BKE_pbvh_get_tex_settings(PBVH *pbvh, PBVHNode *node, TexLayerRef vdm)
{
  return NULL;  // implement me!
}

void *BKE_pbvh_get_tex_data(PBVH *pbvh, PBVHNode *node, TexPointRef vdm)
{
  return NULL;  // implement me!
}

typedef union TexelKey {
  struct {
    int idx;     // index in image
    int co_key;  // used to differentiate same texel used in different 3d points in space
  } key;
  intptr_t i;
} TexelKey;

BLI_INLINE int calc_co_key(const float *co)
{
  const int mul = 65535;
  const int mask = 65535;

  int x = (int)co[0] + (((int)co[0] * mul) & mask);
  int y = (int)co[0] + (((int)co[0] * mul) & mask);
  int z = (int)co[0] + (((int)co[0] * mul) & mask);

  return BLI_hash_int_3d(x, y, z);
}

typedef struct TextureVDMSettings {
  ImageUser ima_user;
  ID *image;
  bool tangent_space;

  char uv_layer[64];

  // used by texture_vdm_get_points
  // BLI_bitmap *texel_used_map;
  GSet *texel_used_map;

  int width, height;
  bool invalid;
} TextureVDMSettings;

typedef struct TextureNodeData {
  TexPointRef *point_ids;
  float (*point_cos)[3];
  float (*point_uvs)[2];
  float **point_cos_ptrs;
  int totpoint;
} TextureNodeData;

void texture_vdm_begin(PBVH *pbvh, PBVHNode *node, TexLayerRef vdm)
{
  TextureVDMSettings *settings = BKE_pbvh_get_tex_settings(pbvh, node, vdm);

  if (!settings->image) {
    return;
  }

  Image *image = (Image *)settings->image;

  int w = 0, h = 0;
  BKE_image_get_size(image, &settings->ima_user, &w, &h);

  // Image *image = settings->image.
  settings->width = w;
  settings->height = h;
  // settings->texel_used_map = BLI_BITMAP_NEW(w * h, "texel_used_map");
  settings->texel_used_map = BLI_gset_ptr_new("texel_used_map");
}

void texture_vdm_build_points(PBVH *pbvh, PBVHNode *node, TexLayerRef vdm)
{
  TextureVDMSettings *settings = BKE_pbvh_get_tex_settings(pbvh, node, vdm);
  TextureNodeData *data = BKE_pbvh_get_tex_data(pbvh, node, vdm);

  int idx;

  if (!settings->uv_layer[0]) {
    idx = CustomData_get_layer_index(&pbvh->bm->ldata, CD_MLOOPUV);
  }
  else {
    idx = CustomData_get_named_layer_index(&pbvh->bm->ldata, CD_MLOOPUV, settings->uv_layer);
  }

  if (idx < 0) {
    settings->invalid = true;
    return;
  }

  const int cd_uv = pbvh->bm->ldata.layers[idx].offset;
  const int w = settings->width, h = settings->height;

  float **point_cos_ptrs = NULL;
  float *uvs = NULL;
  float *cos = NULL;
  TexPointRef *ids = NULL;

  BLI_array_declare(point_cos_ptrs);
  BLI_array_declare(uvs);
  BLI_array_declare(cos);
  BLI_array_declare(ids);

  for (int i = 0; i < node->tribuf->tottri; i++) {
    PBVHTri *tri = node->tribuf->tris + i;

    BMLoop *ls[3] = {(BMLoop *)tri->l[0], (BMLoop *)tri->l[1], (BMLoop *)tri->l[2]};
    float min[2] = {FLT_MAX, FLT_MAX}, max[2] = {FLT_MIN, FLT_MIN};

    float tricos[3][3];

    copy_v3_v3(tricos[0], ls[0]->v->co);
    copy_v3_v3(tricos[1], ls[1]->v->co);
    copy_v3_v3(tricos[2], ls[2]->v->co);

    for (int j = 0; j < 3; j++) {
      MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(ls[j], cd_uv);
      minmax_v2v2_v2(min, max, luv->uv);
    }

    int dw = (int)((max[0] - min[0]) * (float)w + 0.000001f);
    int dh = (int)((max[1] - min[1]) * (float)h + 0.000001f);

    dw = MAX2(dw, 1);
    dh = MAX2(dh, 1);

    float du = (max[0] - min[0]) / dw;
    float dv = (max[1] - min[1]) / dh;

    float u = min[0], v = min[1];
    for (int y = 0; y < dh; y++, v += dv) {
      u = min[0];

      for (int x = 0; x < dw; x++, u += du) {
        int idx = y * w + x;
        float co[3];

        interp_barycentric_tri_v3(tricos, u, v, co);

        TexelKey key;
        key.key.idx = idx;
        key.key.co_key = calc_co_key(co);

        if (BLI_gset_haskey(settings->texel_used_map, (void *)key.i)) {
          continue;
        }

        BLI_gset_insert(settings->texel_used_map, (void *)key.i);

        BLI_array_append(uvs, u);
        BLI_array_append(uvs, v);

        BLI_array_append(cos, co[0]);
        BLI_array_append(cos, co[1]);
        BLI_array_append(cos, co[2]);
        BLI_array_append(ids, (TexPointRef)key.i);
      }
    }
  }

  settings->invalid = false;
  MEM_SAFE_FREE(data->point_cos);
  MEM_SAFE_FREE(data->point_ids);
  MEM_SAFE_FREE(data->point_uvs);
  MEM_SAFE_FREE(data->point_cos_ptrs);

  int totpoint = BLI_array_len(ids);

  data->totpoint = totpoint;

  data->point_cos_ptrs = MEM_malloc_arrayN(totpoint, sizeof(void *), "point_cos_ptrs");

  // dumb casting trick
  union {
    float *cos;
    float (*cos3)[3];
  } castcos;

  union {
    float *uvs;
    float (*uvs2)[2];
  } castuvs;

  castcos.cos = cos;
  castuvs.uvs = uvs;

  data->point_cos = castcos.cos3;
  data->point_ids = ids;
  data->point_uvs = castuvs.uvs2;

  for (int i = 0; i < totpoint; i++) {
    data->point_cos_ptrs[i] = cos + i * 3;
  }
}

void texture_vdm_get_points(PBVH *pbvh,
                            PBVHNode *node,
                            TexLayerRef vdm,
                            TexPointRef **r_ids,
                            float ***r_cos,
                            float ***r_nos,
                            int *r_totpoint)
{
  TextureVDMSettings *settings = BKE_pbvh_get_tex_settings(pbvh, node, vdm);
  TextureNodeData *data = BKE_pbvh_get_tex_data(pbvh, node, vdm);

  if (r_totpoint) {
    *r_totpoint = data->totpoint;
  }

  if (r_cos) {
    *r_cos = data->point_cos_ptrs;
  }

  if (r_ids) {
    *r_ids = data->point_ids;
  }
}

static SculptDisplacementDef texture_vdm = {
    .type = SCULPT_TEXTURE_UV,
    .settings_size = sizeof(TextureNodeData),
    .getPointsFromNode = texture_vdm_get_points,
};
#endif
