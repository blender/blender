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
 * The Original Code is Copyright (C) 2014 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_data_transfer.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_remap.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"
#include "BKE_report.h"

#include "data_transfer_intern.h"

static CLG_LogRef LOG = {"bke.data_transfer"};

void BKE_object_data_transfer_dttypes_to_cdmask(const int dtdata_types,
                                                CustomData_MeshMasks *r_data_masks)
{
  for (int i = 0; i < DT_TYPE_MAX; i++) {
    const int dtdata_type = 1 << i;
    int cddata_type;

    if (!(dtdata_types & dtdata_type)) {
      continue;
    }

    cddata_type = BKE_object_data_transfer_dttype_to_cdtype(dtdata_type);
    if (!(cddata_type & CD_FAKE)) {
      if (DT_DATATYPE_IS_VERT(dtdata_type)) {
        r_data_masks->vmask |= 1LL << cddata_type;
      }
      else if (DT_DATATYPE_IS_EDGE(dtdata_type)) {
        r_data_masks->emask |= 1LL << cddata_type;
      }
      else if (DT_DATATYPE_IS_LOOP(dtdata_type)) {
        r_data_masks->lmask |= 1LL << cddata_type;
      }
      else if (DT_DATATYPE_IS_POLY(dtdata_type)) {
        r_data_masks->pmask |= 1LL << cddata_type;
      }
    }
    else if (cddata_type == CD_FAKE_MDEFORMVERT) {
      r_data_masks->vmask |= CD_MASK_MDEFORMVERT; /* Exception for vgroups :/ */
    }
    else if (cddata_type == CD_FAKE_UV) {
      r_data_masks->lmask |= CD_MASK_MLOOPUV;
    }
    else if (cddata_type == CD_FAKE_LNOR) {
      r_data_masks->vmask |= CD_MASK_NORMAL;
      r_data_masks->lmask |= CD_MASK_NORMAL | CD_MASK_CUSTOMLOOPNORMAL;
    }
  }
}

/* Check what can do each layer type (if it is actually handled by transferdata, if it supports advanced mixing... */
bool BKE_object_data_transfer_get_dttypes_capacity(const int dtdata_types,
                                                   bool *r_advanced_mixing,
                                                   bool *r_threshold)
{
  int i;
  bool ret = false;

  *r_advanced_mixing = false;
  *r_threshold = false;

  for (i = 0; (i < DT_TYPE_MAX) && !(ret && *r_advanced_mixing && *r_threshold); i++) {
    const int dtdata_type = 1 << i;

    if (!(dtdata_types & dtdata_type)) {
      continue;
    }

    switch (dtdata_type) {
      /* Vertex data */
      case DT_TYPE_MDEFORMVERT:
        *r_advanced_mixing = true;
        *r_threshold = true;
        ret = true;
        break;
      case DT_TYPE_SKIN:
        *r_threshold = true;
        ret = true;
        break;
      case DT_TYPE_BWEIGHT_VERT:
        ret = true;
        break;
      /* Edge data */
      case DT_TYPE_SHARP_EDGE:
        *r_threshold = true;
        ret = true;
        break;
      case DT_TYPE_SEAM:
        *r_threshold = true;
        ret = true;
        break;
      case DT_TYPE_CREASE:
        ret = true;
        break;
      case DT_TYPE_BWEIGHT_EDGE:
        ret = true;
        break;
      case DT_TYPE_FREESTYLE_EDGE:
        *r_threshold = true;
        ret = true;
        break;
      /* Loop/Poly data */
      case DT_TYPE_UV:
        ret = true;
        break;
      case DT_TYPE_VCOL:
        *r_advanced_mixing = true;
        *r_threshold = true;
        ret = true;
        break;
      case DT_TYPE_LNOR:
        *r_advanced_mixing = true;
        ret = true;
        break;
      case DT_TYPE_SHARP_FACE:
        *r_threshold = true;
        ret = true;
        break;
      case DT_TYPE_FREESTYLE_FACE:
        *r_threshold = true;
        ret = true;
        break;
    }
  }

  return ret;
}

int BKE_object_data_transfer_get_dttypes_item_types(const int dtdata_types)
{
  int i, ret = 0;

  for (i = 0; (i < DT_TYPE_MAX) && (ret ^ (ME_VERT | ME_EDGE | ME_LOOP | ME_POLY)); i++) {
    const int dtdata_type = 1 << i;

    if (!(dtdata_types & dtdata_type)) {
      continue;
    }

    if (DT_DATATYPE_IS_VERT(dtdata_type)) {
      ret |= ME_VERT;
    }
    if (DT_DATATYPE_IS_EDGE(dtdata_type)) {
      ret |= ME_EDGE;
    }
    if (DT_DATATYPE_IS_LOOP(dtdata_type)) {
      ret |= ME_LOOP;
    }
    if (DT_DATATYPE_IS_POLY(dtdata_type)) {
      ret |= ME_POLY;
    }
  }

  return ret;
}

int BKE_object_data_transfer_dttype_to_cdtype(const int dtdata_type)
{
  switch (dtdata_type) {
    case DT_TYPE_MDEFORMVERT:
      return CD_FAKE_MDEFORMVERT;
    case DT_TYPE_SHAPEKEY:
      return CD_FAKE_SHAPEKEY;
    case DT_TYPE_SKIN:
      return CD_MVERT_SKIN;
    case DT_TYPE_BWEIGHT_VERT:
      return CD_FAKE_BWEIGHT;

    case DT_TYPE_SHARP_EDGE:
      return CD_FAKE_SHARP;
    case DT_TYPE_SEAM:
      return CD_FAKE_SEAM;
    case DT_TYPE_CREASE:
      return CD_FAKE_CREASE;
    case DT_TYPE_BWEIGHT_EDGE:
      return CD_FAKE_BWEIGHT;
    case DT_TYPE_FREESTYLE_EDGE:
      return CD_FREESTYLE_EDGE;

    case DT_TYPE_UV:
      return CD_FAKE_UV;
    case DT_TYPE_SHARP_FACE:
      return CD_FAKE_SHARP;
    case DT_TYPE_FREESTYLE_FACE:
      return CD_FREESTYLE_FACE;

    case DT_TYPE_VCOL:
      return CD_MLOOPCOL;
    case DT_TYPE_LNOR:
      return CD_FAKE_LNOR;

    default:
      BLI_assert(0);
  }
  return 0; /* Should never be reached! */
}

int BKE_object_data_transfer_dttype_to_srcdst_index(const int dtdata_type)
{
  switch (dtdata_type) {
    case DT_TYPE_MDEFORMVERT:
      return DT_MULTILAYER_INDEX_MDEFORMVERT;
    case DT_TYPE_SHAPEKEY:
      return DT_MULTILAYER_INDEX_SHAPEKEY;
    case DT_TYPE_UV:
      return DT_MULTILAYER_INDEX_UV;
    case DT_TYPE_VCOL:
      return DT_MULTILAYER_INDEX_VCOL;
    default:
      return DT_MULTILAYER_INDEX_INVALID;
  }
}

/* ********** */

/* Generic pre/post processing, only used by custom loop normals currently. */

static void data_transfer_dtdata_type_preprocess(Mesh *me_src,
                                                 Mesh *me_dst,
                                                 const int dtdata_type,
                                                 const bool dirty_nors_dst)
{
  if (dtdata_type == DT_TYPE_LNOR) {
    /* Compute custom normals into regular loop normals, which will be used for the transfer. */
    MVert *verts_dst = me_dst->mvert;
    const int num_verts_dst = me_dst->totvert;
    MEdge *edges_dst = me_dst->medge;
    const int num_edges_dst = me_dst->totedge;
    MPoly *polys_dst = me_dst->mpoly;
    const int num_polys_dst = me_dst->totpoly;
    MLoop *loops_dst = me_dst->mloop;
    const int num_loops_dst = me_dst->totloop;
    CustomData *pdata_dst = &me_dst->pdata;
    CustomData *ldata_dst = &me_dst->ldata;

    const bool use_split_nors_dst = (me_dst->flag & ME_AUTOSMOOTH) != 0;
    const float split_angle_dst = me_dst->smoothresh;

    /* This should be ensured by cddata_masks we pass to code generating/giving us me_src now. */
    BLI_assert(CustomData_get_layer(&me_src->ldata, CD_NORMAL) != NULL);
    BLI_assert(CustomData_get_layer(&me_src->pdata, CD_NORMAL) != NULL);
    (void)me_src;

    float(*poly_nors_dst)[3];
    float(*loop_nors_dst)[3];
    short(*custom_nors_dst)[2] = CustomData_get_layer(ldata_dst, CD_CUSTOMLOOPNORMAL);

    /* Cache poly nors into a temp CDLayer. */
    poly_nors_dst = CustomData_get_layer(pdata_dst, CD_NORMAL);
    const bool do_poly_nors_dst = (poly_nors_dst == NULL);
    if (do_poly_nors_dst) {
      poly_nors_dst = CustomData_add_layer(pdata_dst, CD_NORMAL, CD_CALLOC, NULL, num_polys_dst);
      CustomData_set_layer_flag(pdata_dst, CD_NORMAL, CD_FLAG_TEMPORARY);
    }
    if (dirty_nors_dst || do_poly_nors_dst) {
      BKE_mesh_calc_normals_poly(verts_dst,
                                 NULL,
                                 num_verts_dst,
                                 loops_dst,
                                 polys_dst,
                                 num_loops_dst,
                                 num_polys_dst,
                                 poly_nors_dst,
                                 true);
    }
    /* Cache loop nors into a temp CDLayer. */
    loop_nors_dst = CustomData_get_layer(ldata_dst, CD_NORMAL);
    const bool do_loop_nors_dst = (loop_nors_dst == NULL);
    if (do_loop_nors_dst) {
      loop_nors_dst = CustomData_add_layer(ldata_dst, CD_NORMAL, CD_CALLOC, NULL, num_loops_dst);
      CustomData_set_layer_flag(ldata_dst, CD_NORMAL, CD_FLAG_TEMPORARY);
    }
    if (dirty_nors_dst || do_loop_nors_dst) {
      BKE_mesh_normals_loop_split(verts_dst,
                                  num_verts_dst,
                                  edges_dst,
                                  num_edges_dst,
                                  loops_dst,
                                  loop_nors_dst,
                                  num_loops_dst,
                                  polys_dst,
                                  (const float(*)[3])poly_nors_dst,
                                  num_polys_dst,
                                  use_split_nors_dst,
                                  split_angle_dst,
                                  NULL,
                                  custom_nors_dst,
                                  NULL);
    }
  }
}

static void data_transfer_dtdata_type_postprocess(Object *UNUSED(ob_src),
                                                  Object *UNUSED(ob_dst),
                                                  Mesh *UNUSED(me_src),
                                                  Mesh *me_dst,
                                                  const int dtdata_type,
                                                  const bool changed)
{
  if (dtdata_type == DT_TYPE_LNOR) {
    if (!changed) {
      return;
    }

    /* Bake edited destination loop normals into custom normals again. */
    MVert *verts_dst = me_dst->mvert;
    const int num_verts_dst = me_dst->totvert;
    MEdge *edges_dst = me_dst->medge;
    const int num_edges_dst = me_dst->totedge;
    MPoly *polys_dst = me_dst->mpoly;
    const int num_polys_dst = me_dst->totpoly;
    MLoop *loops_dst = me_dst->mloop;
    const int num_loops_dst = me_dst->totloop;
    CustomData *pdata_dst = &me_dst->pdata;
    CustomData *ldata_dst = &me_dst->ldata;

    const float(*poly_nors_dst)[3] = CustomData_get_layer(pdata_dst, CD_NORMAL);
    float(*loop_nors_dst)[3] = CustomData_get_layer(ldata_dst, CD_NORMAL);
    short(*custom_nors_dst)[2] = CustomData_get_layer(ldata_dst, CD_CUSTOMLOOPNORMAL);

    BLI_assert(poly_nors_dst);

    if (!custom_nors_dst) {
      custom_nors_dst = CustomData_add_layer(
          ldata_dst, CD_CUSTOMLOOPNORMAL, CD_CALLOC, NULL, num_loops_dst);
    }

    /* Note loop_nors_dst contains our custom normals as transferred from source... */
    BKE_mesh_normals_loop_custom_set(verts_dst,
                                     num_verts_dst,
                                     edges_dst,
                                     num_edges_dst,
                                     loops_dst,
                                     loop_nors_dst,
                                     num_loops_dst,
                                     polys_dst,
                                     poly_nors_dst,
                                     num_polys_dst,
                                     custom_nors_dst);
  }
}

/* ********** */

static MeshRemapIslandsCalc data_transfer_get_loop_islands_generator(const int cddata_type)
{
  switch (cddata_type) {
    case CD_FAKE_UV:
      return BKE_mesh_calc_islands_loop_poly_edgeseam;
    default:
      break;
  }
  return NULL;
}

float data_transfer_interp_float_do(const int mix_mode,
                                    const float val_dst,
                                    const float val_src,
                                    const float mix_factor)
{
  float val_ret;

  if (((mix_mode == CDT_MIX_REPLACE_ABOVE_THRESHOLD && (val_dst < mix_factor)) ||
       (mix_mode == CDT_MIX_REPLACE_BELOW_THRESHOLD && (val_dst > mix_factor)))) {
    return val_dst; /* Do not affect destination. */
  }

  switch (mix_mode) {
    case CDT_MIX_REPLACE_ABOVE_THRESHOLD:
    case CDT_MIX_REPLACE_BELOW_THRESHOLD:
      return val_src;
    case CDT_MIX_MIX:
      val_ret = (val_dst + val_src) * 0.5f;
      break;
    case CDT_MIX_ADD:
      val_ret = val_dst + val_src;
      break;
    case CDT_MIX_SUB:
      val_ret = val_dst - val_src;
      break;
    case CDT_MIX_MUL:
      val_ret = val_dst * val_src;
      break;
    case CDT_MIX_TRANSFER:
    default:
      val_ret = val_src;
      break;
  }
  return interpf(val_ret, val_dst, mix_factor);
}

static void data_transfer_interp_char(const CustomDataTransferLayerMap *laymap,
                                      void *dest,
                                      const void **sources,
                                      const float *weights,
                                      const int count,
                                      const float mix_factor)
{
  const char **data_src = (const char **)sources;
  char *data_dst = (char *)dest;

  const int mix_mode = laymap->mix_mode;
  float val_src = 0.0f;
  const float val_dst = (float)(*data_dst) / 255.0f;

  int i;

  for (i = count; i--;) {
    val_src += ((float)(*data_src[i]) / 255.0f) * weights[i];
  }

  val_src = data_transfer_interp_float_do(mix_mode, val_dst, val_src, mix_factor);

  CLAMP(val_src, 0.0f, 1.0f);

  *data_dst = (char)(val_src * 255.0f);
}

/* Helpers to match sources and destinations data layers (also handles 'conversions' in CD_FAKE cases). */

void data_transfer_layersmapping_add_item(ListBase *r_map,
                                          const int cddata_type,
                                          const int mix_mode,
                                          const float mix_factor,
                                          const float *mix_weights,
                                          const void *data_src,
                                          void *data_dst,
                                          const int data_src_n,
                                          const int data_dst_n,
                                          const size_t elem_size,
                                          const size_t data_size,
                                          const size_t data_offset,
                                          const uint64_t data_flag,
                                          cd_datatransfer_interp interp,
                                          void *interp_data)
{
  CustomDataTransferLayerMap *item = MEM_mallocN(sizeof(*item), __func__);

  BLI_assert(data_dst != NULL);

  item->data_type = cddata_type;
  item->mix_mode = mix_mode;
  item->mix_factor = mix_factor;
  item->mix_weights = mix_weights;

  item->data_src = data_src;
  item->data_dst = data_dst;
  item->data_src_n = data_src_n;
  item->data_dst_n = data_dst_n;
  item->elem_size = elem_size;

  item->data_size = data_size;
  item->data_offset = data_offset;
  item->data_flag = data_flag;

  item->interp = interp;
  item->interp_data = interp_data;

  BLI_addtail(r_map, item);
}

static void data_transfer_layersmapping_add_item_cd(ListBase *r_map,
                                                    const int cddata_type,
                                                    const int mix_mode,
                                                    const float mix_factor,
                                                    const float *mix_weights,
                                                    void *data_src,
                                                    void *data_dst,
                                                    cd_datatransfer_interp interp,
                                                    void *interp_data)
{
  uint64_t data_flag = 0;

  if (cddata_type == CD_FREESTYLE_EDGE) {
    data_flag = FREESTYLE_EDGE_MARK;
  }
  else if (cddata_type == CD_FREESTYLE_FACE) {
    data_flag = FREESTYLE_FACE_MARK;
  }

  data_transfer_layersmapping_add_item(r_map,
                                       cddata_type,
                                       mix_mode,
                                       mix_factor,
                                       mix_weights,
                                       data_src,
                                       data_dst,
                                       0,
                                       0,
                                       0,
                                       0,
                                       0,
                                       data_flag,
                                       interp,
                                       interp_data);
}

/* Note: All those layer mapping handlers return false *only* if they were given invalid parameters.
 *       This means that even if they do nothing, they will return true if all given parameters were OK.
 *       Also, r_map may be NULL, in which case they will 'only' create/delete destination layers according
 *       to given parameters.
 */

static bool data_transfer_layersmapping_cdlayers_multisrc_to_dst(ListBase *r_map,
                                                                 const int cddata_type,
                                                                 const int mix_mode,
                                                                 const float mix_factor,
                                                                 const float *mix_weights,
                                                                 const int num_elem_dst,
                                                                 const bool use_create,
                                                                 const bool use_delete,
                                                                 CustomData *cd_src,
                                                                 CustomData *cd_dst,
                                                                 const bool use_dupref_dst,
                                                                 const int tolayers,
                                                                 bool *use_layers_src,
                                                                 const int num_layers_src,
                                                                 cd_datatransfer_interp interp,
                                                                 void *interp_data)
{
  void *data_src, *data_dst = NULL;
  int idx_src = num_layers_src;
  int idx_dst, tot_dst = CustomData_number_of_layers(cd_dst, cddata_type);
  bool *data_dst_to_delete = NULL;

  if (!use_layers_src) {
    /* No source at all, we can only delete all dest if requested... */
    if (use_delete) {
      idx_dst = tot_dst;
      while (idx_dst--) {
        CustomData_free_layer(cd_dst, cddata_type, num_elem_dst, idx_dst);
      }
    }
    return true;
  }

  switch (tolayers) {
    case DT_LAYERS_INDEX_DST:
      idx_dst = tot_dst;

      /* Find last source actually used! */
      while (idx_src-- && !use_layers_src[idx_src]) {
        ;
      }
      idx_src++;

      if (idx_dst < idx_src) {
        if (use_create) {
          /* Create as much data layers as necessary! */
          for (; idx_dst < idx_src; idx_dst++) {
            CustomData_add_layer(cd_dst, cddata_type, CD_CALLOC, NULL, num_elem_dst);
          }
        }
        else {
          /* Otherwise, just try to map what we can with existing dst data layers. */
          idx_src = idx_dst;
        }
      }
      else if (use_delete && idx_dst > idx_src) {
        while (idx_dst-- > idx_src) {
          CustomData_free_layer(cd_dst, cddata_type, num_elem_dst, idx_dst);
        }
      }
      if (r_map) {
        while (idx_src--) {
          if (!use_layers_src[idx_src]) {
            continue;
          }
          data_src = CustomData_get_layer_n(cd_src, cddata_type, idx_src);
          /* If dest is a evaluated mesh (fro; ;odifier), we do not want to overwrite cdlayers of orig mesh! */
          if (use_dupref_dst) {
            data_dst = CustomData_duplicate_referenced_layer_n(
                cd_dst, cddata_type, idx_src, num_elem_dst);
          }
          else {
            data_dst = CustomData_get_layer_n(cd_dst, cddata_type, idx_src);
          }
          data_transfer_layersmapping_add_item_cd(r_map,
                                                  cddata_type,
                                                  mix_mode,
                                                  mix_factor,
                                                  mix_weights,
                                                  data_src,
                                                  data_dst,
                                                  interp,
                                                  interp_data);
        }
      }
      break;
    case DT_LAYERS_NAME_DST:
      if (use_delete) {
        if (tot_dst) {
          data_dst_to_delete = MEM_mallocN(sizeof(*data_dst_to_delete) * (size_t)tot_dst,
                                           __func__);
          memset(data_dst_to_delete, true, sizeof(*data_dst_to_delete) * (size_t)tot_dst);
        }
      }

      while (idx_src--) {
        const char *name;

        if (!use_layers_src[idx_src]) {
          continue;
        }

        name = CustomData_get_layer_name(cd_src, cddata_type, idx_src);
        data_src = CustomData_get_layer_n(cd_src, cddata_type, idx_src);

        if ((idx_dst = CustomData_get_named_layer(cd_dst, cddata_type, name)) == -1) {
          if (use_create) {
            CustomData_add_layer_named(cd_dst, cddata_type, CD_CALLOC, NULL, num_elem_dst, name);
            idx_dst = CustomData_get_named_layer(cd_dst, cddata_type, name);
          }
          else {
            /* If we are not allowed to create missing dst data layers, just skip matching src one. */
            continue;
          }
        }
        else if (data_dst_to_delete) {
          data_dst_to_delete[idx_dst] = false;
        }
        if (r_map) {
          /* If dest is a evaluated mesh (from modifier), we do not want to overwrite cdlayers of orig mesh! */
          if (use_dupref_dst) {
            data_dst = CustomData_duplicate_referenced_layer_n(
                cd_dst, cddata_type, idx_dst, num_elem_dst);
          }
          else {
            data_dst = CustomData_get_layer_n(cd_dst, cddata_type, idx_dst);
          }
          data_transfer_layersmapping_add_item_cd(r_map,
                                                  cddata_type,
                                                  mix_mode,
                                                  mix_factor,
                                                  mix_weights,
                                                  data_src,
                                                  data_dst,
                                                  interp,
                                                  interp_data);
        }
      }

      if (data_dst_to_delete) {
        /* Note: This won't affect newly created layers, if any, since tot_dst has not been updated!
         *       Also, looping backward ensures us we do not suffer from index shifting when deleting a layer.
         */
        for (idx_dst = tot_dst; idx_dst--;) {
          if (data_dst_to_delete[idx_dst]) {
            CustomData_free_layer(cd_dst, cddata_type, num_elem_dst, idx_dst);
          }
        }

        MEM_freeN(data_dst_to_delete);
      }
      break;
    default:
      return false;
  }

  return true;
}

static bool data_transfer_layersmapping_cdlayers(ListBase *r_map,
                                                 const int cddata_type,
                                                 const int mix_mode,
                                                 const float mix_factor,
                                                 const float *mix_weights,
                                                 const int num_elem_dst,
                                                 const bool use_create,
                                                 const bool use_delete,
                                                 CustomData *cd_src,
                                                 CustomData *cd_dst,
                                                 const bool use_dupref_dst,
                                                 const int fromlayers,
                                                 const int tolayers,
                                                 cd_datatransfer_interp interp,
                                                 void *interp_data)
{
  int idx_src, idx_dst;
  void *data_src, *data_dst = NULL;

  if (CustomData_layertype_is_singleton(cddata_type)) {
    if (!(data_src = CustomData_get_layer(cd_src, cddata_type))) {
      if (use_delete) {
        CustomData_free_layer(cd_dst, cddata_type, num_elem_dst, 0);
      }
      return true;
    }

    data_dst = CustomData_get_layer(cd_dst, cddata_type);
    if (!data_dst) {
      if (!use_create) {
        return true;
      }
      data_dst = CustomData_add_layer(cd_dst, cddata_type, CD_CALLOC, NULL, num_elem_dst);
    }
    else if (use_dupref_dst && r_map) {
      /* If dest is a evaluated mesh (from modifier), we do not want to overwrite cdlayers of orig mesh! */
      data_dst = CustomData_duplicate_referenced_layer(cd_dst, cddata_type, num_elem_dst);
    }

    if (r_map) {
      data_transfer_layersmapping_add_item_cd(r_map,
                                              cddata_type,
                                              mix_mode,
                                              mix_factor,
                                              mix_weights,
                                              data_src,
                                              data_dst,
                                              interp,
                                              interp_data);
    }
  }
  else if (fromlayers == DT_LAYERS_ACTIVE_SRC || fromlayers >= 0) {
    /* Note: use_delete has not much meaning in this case, ignored. */

    if (fromlayers >= 0) { /* Real-layer index */
      idx_src = fromlayers;
    }
    else {
      if ((idx_src = CustomData_get_active_layer(cd_src, cddata_type)) == -1) {
        return true;
      }
    }
    data_src = CustomData_get_layer_n(cd_src, cddata_type, idx_src);
    if (!data_src) {
      return true;
    }

    if (tolayers >= 0) { /* Real-layer index */
      idx_dst = tolayers;
      /* If dest is a evaluated mesh (from modifier), we do not want to overwrite cdlayers of orig mesh! */
      if (use_dupref_dst && r_map) {
        data_dst = CustomData_duplicate_referenced_layer_n(
            cd_dst, cddata_type, idx_dst, num_elem_dst);
      }
      else {
        data_dst = CustomData_get_layer_n(cd_dst, cddata_type, idx_dst);
      }
    }
    else if (tolayers == DT_LAYERS_ACTIVE_DST) {
      if ((idx_dst = CustomData_get_active_layer(cd_dst, cddata_type)) == -1) {
        if (!use_create) {
          return true;
        }
        data_dst = CustomData_add_layer(cd_dst, cddata_type, CD_CALLOC, NULL, num_elem_dst);
      }
      else {
        /* If dest is a evaluated mesh (from modifier), we do not want to overwrite cdlayers of orig mesh! */
        if (use_dupref_dst && r_map) {
          data_dst = CustomData_duplicate_referenced_layer_n(
              cd_dst, cddata_type, idx_dst, num_elem_dst);
        }
        else {
          data_dst = CustomData_get_layer_n(cd_dst, cddata_type, idx_dst);
        }
      }
    }
    else if (tolayers == DT_LAYERS_INDEX_DST) {
      int num = CustomData_number_of_layers(cd_dst, cddata_type);
      idx_dst = idx_src;
      if (num <= idx_dst) {
        if (!use_create) {
          return true;
        }
        /* Create as much data layers as necessary! */
        for (; num <= idx_dst; num++) {
          CustomData_add_layer(cd_dst, cddata_type, CD_CALLOC, NULL, num_elem_dst);
        }
      }
      /* If dest is a evaluated mesh (from modifier), we do not want to overwrite cdlayers of orig mesh! */
      if (use_dupref_dst && r_map) {
        data_dst = CustomData_duplicate_referenced_layer_n(
            cd_dst, cddata_type, idx_dst, num_elem_dst);
      }
      else {
        data_dst = CustomData_get_layer_n(cd_dst, cddata_type, idx_dst);
      }
    }
    else if (tolayers == DT_LAYERS_NAME_DST) {
      const char *name = CustomData_get_layer_name(cd_src, cddata_type, idx_src);
      if ((idx_dst = CustomData_get_named_layer(cd_dst, cddata_type, name)) == -1) {
        if (!use_create) {
          return true;
        }
        CustomData_add_layer_named(cd_dst, cddata_type, CD_CALLOC, NULL, num_elem_dst, name);
        idx_dst = CustomData_get_named_layer(cd_dst, cddata_type, name);
      }
      /* If dest is a evaluated mesh (from modifier), we do not want to overwrite cdlayers of orig mesh! */
      if (use_dupref_dst && r_map) {
        data_dst = CustomData_duplicate_referenced_layer_n(
            cd_dst, cddata_type, idx_dst, num_elem_dst);
      }
      else {
        data_dst = CustomData_get_layer_n(cd_dst, cddata_type, idx_dst);
      }
    }
    else {
      return false;
    }

    if (!data_dst) {
      return false;
    }

    if (r_map) {
      data_transfer_layersmapping_add_item_cd(r_map,
                                              cddata_type,
                                              mix_mode,
                                              mix_factor,
                                              mix_weights,
                                              data_src,
                                              data_dst,
                                              interp,
                                              interp_data);
    }
  }
  else if (fromlayers == DT_LAYERS_ALL_SRC) {
    int num_src = CustomData_number_of_layers(cd_src, cddata_type);
    bool *use_layers_src = num_src ?
                               MEM_mallocN(sizeof(*use_layers_src) * (size_t)num_src, __func__) :
                               NULL;
    bool ret;

    if (use_layers_src) {
      memset(use_layers_src, true, sizeof(*use_layers_src) * num_src);
    }

    ret = data_transfer_layersmapping_cdlayers_multisrc_to_dst(r_map,
                                                               cddata_type,
                                                               mix_mode,
                                                               mix_factor,
                                                               mix_weights,
                                                               num_elem_dst,
                                                               use_create,
                                                               use_delete,
                                                               cd_src,
                                                               cd_dst,
                                                               use_dupref_dst,
                                                               tolayers,
                                                               use_layers_src,
                                                               num_src,
                                                               interp,
                                                               interp_data);

    if (use_layers_src) {
      MEM_freeN(use_layers_src);
    }
    return ret;
  }
  else {
    return false;
  }

  return true;
}

static bool data_transfer_layersmapping_generate(ListBase *r_map,
                                                 Object *ob_src,
                                                 Object *ob_dst,
                                                 Mesh *me_src,
                                                 Mesh *me_dst,
                                                 const int elem_type,
                                                 int cddata_type,
                                                 int mix_mode,
                                                 float mix_factor,
                                                 const float *mix_weights,
                                                 const int num_elem_dst,
                                                 const bool use_create,
                                                 const bool use_delete,
                                                 const int fromlayers,
                                                 const int tolayers,
                                                 SpaceTransform *space_transform)
{
  CustomData *cd_src, *cd_dst;

  cd_datatransfer_interp interp = NULL;
  void *interp_data = NULL;

  if (elem_type == ME_VERT) {
    if (!(cddata_type & CD_FAKE)) {
      cd_src = &me_src->vdata;
      cd_dst = &me_dst->vdata;

      if (!data_transfer_layersmapping_cdlayers(r_map,
                                                cddata_type,
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                num_elem_dst,
                                                use_create,
                                                use_delete,
                                                cd_src,
                                                cd_dst,
                                                me_dst != ob_dst->data,
                                                fromlayers,
                                                tolayers,
                                                interp,
                                                interp_data)) {
        /* We handle specific source selection cases here. */
        return false;
      }
      return true;
    }
    else if (cddata_type == CD_FAKE_BWEIGHT) {
      const size_t elem_size = sizeof(*((MVert *)NULL));
      const size_t data_size = sizeof(((MVert *)NULL)->bweight);
      const size_t data_offset = offsetof(MVert, bweight);
      const uint64_t data_flag = 0;

      if (!(me_src->cd_flag & ME_CDFLAG_VERT_BWEIGHT)) {
        if (use_delete) {
          me_dst->cd_flag &= ~ME_CDFLAG_VERT_BWEIGHT;
        }
        return true;
      }
      me_dst->cd_flag |= ME_CDFLAG_VERT_BWEIGHT;
      if (r_map) {
        data_transfer_layersmapping_add_item(r_map,
                                             cddata_type,
                                             mix_mode,
                                             mix_factor,
                                             mix_weights,
                                             me_src->mvert,
                                             me_dst->mvert,
                                             me_src->totvert,
                                             me_dst->totvert,
                                             elem_size,
                                             data_size,
                                             data_offset,
                                             data_flag,
                                             data_transfer_interp_char,
                                             interp_data);
      }
      return true;
    }
    else if (cddata_type == CD_FAKE_MDEFORMVERT) {
      bool ret;

      cd_src = &me_src->vdata;
      cd_dst = &me_dst->vdata;

      ret = data_transfer_layersmapping_vgroups(r_map,
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                num_elem_dst,
                                                use_create,
                                                use_delete,
                                                ob_src,
                                                ob_dst,
                                                cd_src,
                                                cd_dst,
                                                me_dst != ob_dst->data,
                                                fromlayers,
                                                tolayers);

      /* Mesh stores its dvert in a specific pointer too. :( */
      me_dst->dvert = CustomData_get_layer(&me_dst->vdata, CD_MDEFORMVERT);
      return ret;
    }
    else if (cddata_type == CD_FAKE_SHAPEKEY) {
      /* TODO: leaving shapekeys aside for now, quite specific case, since we can't access them from MVert :/ */
      return false;
    }
  }
  else if (elem_type == ME_EDGE) {
    if (!(cddata_type & CD_FAKE)) { /* Unused for edges, currently... */
      cd_src = &me_src->edata;
      cd_dst = &me_dst->edata;

      if (!data_transfer_layersmapping_cdlayers(r_map,
                                                cddata_type,
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                num_elem_dst,
                                                use_create,
                                                use_delete,
                                                cd_src,
                                                cd_dst,
                                                me_dst != ob_dst->data,
                                                fromlayers,
                                                tolayers,
                                                interp,
                                                interp_data)) {
        /* We handle specific source selection cases here. */
        return false;
      }
      return true;
    }
    else if (cddata_type == CD_FAKE_CREASE) {
      const size_t elem_size = sizeof(*((MEdge *)NULL));
      const size_t data_size = sizeof(((MEdge *)NULL)->crease);
      const size_t data_offset = offsetof(MEdge, crease);
      const uint64_t data_flag = 0;

      if (!(me_src->cd_flag & ME_CDFLAG_EDGE_CREASE)) {
        if (use_delete && !me_dst) {
          me_dst->cd_flag &= ~ME_CDFLAG_EDGE_CREASE;
        }
        return true;
      }
      me_dst->cd_flag |= ME_CDFLAG_EDGE_CREASE;
      if (r_map) {
        data_transfer_layersmapping_add_item(r_map,
                                             cddata_type,
                                             mix_mode,
                                             mix_factor,
                                             mix_weights,
                                             me_src->medge,
                                             me_dst->medge,
                                             me_src->totedge,
                                             me_dst->totedge,
                                             elem_size,
                                             data_size,
                                             data_offset,
                                             data_flag,
                                             data_transfer_interp_char,
                                             interp_data);
      }
      return true;
    }
    else if (cddata_type == CD_FAKE_BWEIGHT) {
      const size_t elem_size = sizeof(*((MEdge *)NULL));
      const size_t data_size = sizeof(((MEdge *)NULL)->bweight);
      const size_t data_offset = offsetof(MEdge, bweight);
      const uint64_t data_flag = 0;

      if (!(me_src->cd_flag & ME_CDFLAG_EDGE_BWEIGHT)) {
        if (use_delete && !me_dst) {
          me_dst->cd_flag &= ~ME_CDFLAG_EDGE_BWEIGHT;
        }
        return true;
      }
      me_dst->cd_flag |= ME_CDFLAG_EDGE_BWEIGHT;
      if (r_map) {
        data_transfer_layersmapping_add_item(r_map,
                                             cddata_type,
                                             mix_mode,
                                             mix_factor,
                                             mix_weights,
                                             me_src->medge,
                                             me_dst->medge,
                                             me_src->totedge,
                                             me_dst->totedge,
                                             elem_size,
                                             data_size,
                                             data_offset,
                                             data_flag,
                                             data_transfer_interp_char,
                                             interp_data);
      }
      return true;
    }
    else if (r_map && ELEM(cddata_type, CD_FAKE_SHARP, CD_FAKE_SEAM)) {
      const size_t elem_size = sizeof(*((MEdge *)NULL));
      const size_t data_size = sizeof(((MEdge *)NULL)->flag);
      const size_t data_offset = offsetof(MEdge, flag);
      const uint64_t data_flag = (cddata_type == CD_FAKE_SHARP) ? ME_SHARP : ME_SEAM;

      data_transfer_layersmapping_add_item(r_map,
                                           cddata_type,
                                           mix_mode,
                                           mix_factor,
                                           mix_weights,
                                           me_src->medge,
                                           me_dst->medge,
                                           me_src->totedge,
                                           me_dst->totedge,
                                           elem_size,
                                           data_size,
                                           data_offset,
                                           data_flag,
                                           NULL,
                                           interp_data);
      return true;
    }
    else {
      return false;
    }
  }
  else if (elem_type == ME_LOOP) {
    if (cddata_type == CD_FAKE_UV) {
      cddata_type = CD_MLOOPUV;
    }
    else if (cddata_type == CD_FAKE_LNOR) {
      /* Preprocess should have generated it, Postprocess will convert it back to CD_CUSTOMLOOPNORMAL. */
      cddata_type = CD_NORMAL;
      interp_data = space_transform;
      interp = customdata_data_transfer_interp_normal_normals;
    }

    if (!(cddata_type & CD_FAKE)) {
      cd_src = &me_src->ldata;
      cd_dst = &me_dst->ldata;

      if (!data_transfer_layersmapping_cdlayers(r_map,
                                                cddata_type,
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                num_elem_dst,
                                                use_create,
                                                use_delete,
                                                cd_src,
                                                cd_dst,
                                                me_dst != ob_dst->data,
                                                fromlayers,
                                                tolayers,
                                                interp,
                                                interp_data)) {
        /* We handle specific source selection cases here. */
        return false;
      }
      return true;
    }
    else {
      return false;
    }
  }
  else if (elem_type == ME_POLY) {
    if (cddata_type == CD_FAKE_UV) {
      cddata_type = CD_MLOOPUV;
    }

    if (!(cddata_type & CD_FAKE)) {
      cd_src = &me_src->pdata;
      cd_dst = &me_dst->pdata;

      if (!data_transfer_layersmapping_cdlayers(r_map,
                                                cddata_type,
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                num_elem_dst,
                                                use_create,
                                                use_delete,
                                                cd_src,
                                                cd_dst,
                                                me_dst != ob_dst->data,
                                                fromlayers,
                                                tolayers,
                                                interp,
                                                interp_data)) {
        /* We handle specific source selection cases here. */
        return false;
      }
      return true;
    }
    else if (r_map && cddata_type == CD_FAKE_SHARP) {
      const size_t elem_size = sizeof(*((MPoly *)NULL));
      const size_t data_size = sizeof(((MPoly *)NULL)->flag);
      const size_t data_offset = offsetof(MPoly, flag);
      const uint64_t data_flag = ME_SMOOTH;

      data_transfer_layersmapping_add_item(r_map,
                                           cddata_type,
                                           mix_mode,
                                           mix_factor,
                                           mix_weights,
                                           me_src->mpoly,
                                           me_dst->mpoly,
                                           me_src->totpoly,
                                           me_dst->totpoly,
                                           elem_size,
                                           data_size,
                                           data_offset,
                                           data_flag,
                                           NULL,
                                           interp_data);
      return true;
    }
    else {
      return false;
    }
  }

  return false;
}

/**
 * Transfer data *layout* of selected types from source to destination object.
 * By default, it only creates new data layers if needed on \a ob_dst.
 * If \a use_delete is true, it will also delete data layers on \a ob_dst that do not match those from \a ob_src,
 * to get (as much as possible) exact copy of source data layout.
 */
void BKE_object_data_transfer_layout(struct Depsgraph *depsgraph,
                                     Scene *scene,
                                     Object *ob_src,
                                     Object *ob_dst,
                                     const int data_types,
                                     const bool use_delete,
                                     const int fromlayers_select[DT_MULTILAYER_INDEX_MAX],
                                     const int tolayers_select[DT_MULTILAYER_INDEX_MAX])
{
  Mesh *me_src;
  Mesh *me_dst;
  int i;

  const bool use_create = true; /* We always create needed layers here. */

  CustomData_MeshMasks me_src_mask = CD_MASK_BAREMESH;

  BLI_assert((ob_src != ob_dst) && (ob_src->type == OB_MESH) && (ob_dst->type == OB_MESH));

  me_dst = ob_dst->data;

  /* Get source evaluated mesh.*/
  BKE_object_data_transfer_dttypes_to_cdmask(data_types, &me_src_mask);
  me_src = mesh_get_eval_final(depsgraph, scene, ob_src, &me_src_mask);
  if (!me_src) {
    return;
  }

  /* Check all possible data types. */
  for (i = 0; i < DT_TYPE_MAX; i++) {
    const int dtdata_type = 1 << i;
    int cddata_type;
    int fromlayers, tolayers, fromto_idx;

    if (!(data_types & dtdata_type)) {
      continue;
    }

    cddata_type = BKE_object_data_transfer_dttype_to_cdtype(dtdata_type);

    fromto_idx = BKE_object_data_transfer_dttype_to_srcdst_index(dtdata_type);
    if (fromto_idx != DT_MULTILAYER_INDEX_INVALID) {
      fromlayers = fromlayers_select[fromto_idx];
      tolayers = tolayers_select[fromto_idx];
    }
    else {
      fromlayers = tolayers = 0;
    }

    if (DT_DATATYPE_IS_VERT(dtdata_type)) {
      const int num_elem_dst = me_dst->totvert;

      data_transfer_layersmapping_generate(NULL,
                                           ob_src,
                                           ob_dst,
                                           me_src,
                                           me_dst,
                                           ME_VERT,
                                           cddata_type,
                                           0,
                                           0.0f,
                                           NULL,
                                           num_elem_dst,
                                           use_create,
                                           use_delete,
                                           fromlayers,
                                           tolayers,
                                           NULL);
    }
    if (DT_DATATYPE_IS_EDGE(dtdata_type)) {
      const int num_elem_dst = me_dst->totedge;

      data_transfer_layersmapping_generate(NULL,
                                           ob_src,
                                           ob_dst,
                                           me_src,
                                           me_dst,
                                           ME_EDGE,
                                           cddata_type,
                                           0,
                                           0.0f,
                                           NULL,
                                           num_elem_dst,
                                           use_create,
                                           use_delete,
                                           fromlayers,
                                           tolayers,
                                           NULL);
    }
    if (DT_DATATYPE_IS_LOOP(dtdata_type)) {
      const int num_elem_dst = me_dst->totloop;

      data_transfer_layersmapping_generate(NULL,
                                           ob_src,
                                           ob_dst,
                                           me_src,
                                           me_dst,
                                           ME_LOOP,
                                           cddata_type,
                                           0,
                                           0.0f,
                                           NULL,
                                           num_elem_dst,
                                           use_create,
                                           use_delete,
                                           fromlayers,
                                           tolayers,
                                           NULL);
    }
    if (DT_DATATYPE_IS_POLY(dtdata_type)) {
      const int num_elem_dst = me_dst->totpoly;

      data_transfer_layersmapping_generate(NULL,
                                           ob_src,
                                           ob_dst,
                                           me_src,
                                           me_dst,
                                           ME_POLY,
                                           cddata_type,
                                           0,
                                           0.0f,
                                           NULL,
                                           num_elem_dst,
                                           use_create,
                                           use_delete,
                                           fromlayers,
                                           tolayers,
                                           NULL);
    }
  }
}

bool BKE_object_data_transfer_ex(struct Depsgraph *depsgraph,
                                 Scene *scene,
                                 Object *ob_src,
                                 Object *ob_dst,
                                 Mesh *me_dst,
                                 const int data_types,
                                 bool use_create,
                                 const int map_vert_mode,
                                 const int map_edge_mode,
                                 const int map_loop_mode,
                                 const int map_poly_mode,
                                 SpaceTransform *space_transform,
                                 const bool auto_transform,
                                 const float max_distance,
                                 const float ray_radius,
                                 const float islands_handling_precision,
                                 const int fromlayers_select[DT_MULTILAYER_INDEX_MAX],
                                 const int tolayers_select[DT_MULTILAYER_INDEX_MAX],
                                 const int mix_mode,
                                 const float mix_factor,
                                 const char *vgroup_name,
                                 const bool invert_vgroup,
                                 ReportList *reports)
{
#define VDATA 0
#define EDATA 1
#define LDATA 2
#define PDATA 3
#define DATAMAX 4

  SpaceTransform auto_space_transform;

  Mesh *me_src;
  bool dirty_nors_dst =
      true; /* Assumed always true if not using an evaluated mesh as destination. */
  int i;

  MDeformVert *mdef = NULL;
  int vg_idx = -1;
  float *weights[DATAMAX] = {NULL};

  MeshPairRemap geom_map[DATAMAX] = {{0}};
  bool geom_map_init[DATAMAX] = {0};
  ListBase lay_map = {NULL};
  bool changed = false;
  bool is_modifier = false;

  const bool use_delete = false; /* We never delete data layers from destination here. */

  CustomData_MeshMasks me_src_mask = CD_MASK_BAREMESH;

  BLI_assert((ob_src != ob_dst) && (ob_src->type == OB_MESH) && (ob_dst->type == OB_MESH));

  if (me_dst) {
    dirty_nors_dst = (me_dst->runtime.cd_dirty_vert & CD_NORMAL) != 0;
    /* Never create needed custom layers on passed destination mesh
     * (assumed to *not* be ob_dst->data, aka modifier case). */
    use_create = false;
    is_modifier = true;
  }
  else {
    me_dst = ob_dst->data;
  }

  if (vgroup_name) {
    mdef = CustomData_get_layer(&me_dst->vdata, CD_MDEFORMVERT);
    if (mdef) {
      vg_idx = defgroup_name_index(ob_dst, vgroup_name);
    }
  }

  /* Get source evaluated mesh.*/
  BKE_object_data_transfer_dttypes_to_cdmask(data_types, &me_src_mask);
  BKE_mesh_remap_calc_source_cddata_masks_from_map_modes(
      map_vert_mode, map_edge_mode, map_loop_mode, map_poly_mode, &me_src_mask);
  if (is_modifier) {
    me_src = BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob_src, false);

    if (me_src == NULL ||
        !CustomData_MeshMasks_are_matching(&ob_src->runtime.last_data_mask, &me_src_mask)) {
      CLOG_WARN(&LOG, "Data Transfer: source mesh data is not ready - dependency cycle?");
      return changed;
    }
  }
  else {
    me_src = mesh_get_eval_final(depsgraph, scene, ob_src, &me_src_mask);
  }
  if (!me_src) {
    return changed;
  }

  if (auto_transform) {
    if (space_transform == NULL) {
      space_transform = &auto_space_transform;
    }

    BKE_mesh_remap_find_best_match_from_mesh(
        me_dst->mvert, me_dst->totvert, me_src, space_transform);
  }

  /* Check all possible data types.
   * Note item mappings and dest mix weights are cached. */
  for (i = 0; i < DT_TYPE_MAX; i++) {
    const int dtdata_type = 1 << i;
    int cddata_type;
    int fromlayers, tolayers, fromto_idx;

    if (!(data_types & dtdata_type)) {
      continue;
    }

    data_transfer_dtdata_type_preprocess(me_src, me_dst, dtdata_type, dirty_nors_dst);

    cddata_type = BKE_object_data_transfer_dttype_to_cdtype(dtdata_type);

    fromto_idx = BKE_object_data_transfer_dttype_to_srcdst_index(dtdata_type);
    if (fromto_idx != DT_MULTILAYER_INDEX_INVALID) {
      fromlayers = fromlayers_select[fromto_idx];
      tolayers = tolayers_select[fromto_idx];
    }
    else {
      fromlayers = tolayers = 0;
    }

    if (DT_DATATYPE_IS_VERT(dtdata_type)) {
      MVert *verts_dst = me_dst->mvert;
      const int num_verts_dst = me_dst->totvert;

      if (!geom_map_init[VDATA]) {
        const int num_verts_src = me_src->totvert;

        if ((map_vert_mode == MREMAP_MODE_TOPOLOGY) && (num_verts_dst != num_verts_src)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source and destination meshes do not have the same amount of vertices, "
                     "'Topology' mapping cannot be used in this case");
          continue;
        }
        if ((map_vert_mode & MREMAP_USE_EDGE) && (me_src->totedge == 0)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source mesh doesn't have any edges, "
                     "None of the 'Edge' mappings can be used in this case");
          continue;
        }
        if ((map_vert_mode & MREMAP_USE_POLY) && (me_src->totpoly == 0)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source mesh doesn't have any faces, "
                     "None of the 'Face' mappings can be used in this case");
          continue;
        }
        if (ELEM(0, num_verts_dst, num_verts_src)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source or destination meshes do not have any vertices, cannot transfer "
                     "vertex data");
          continue;
        }

        BKE_mesh_remap_calc_verts_from_mesh(map_vert_mode,
                                            space_transform,
                                            max_distance,
                                            ray_radius,
                                            verts_dst,
                                            num_verts_dst,
                                            dirty_nors_dst,
                                            me_src,
                                            &geom_map[VDATA]);
        geom_map_init[VDATA] = true;
      }

      if (mdef && vg_idx != -1 && !weights[VDATA]) {
        weights[VDATA] = MEM_mallocN(sizeof(*(weights[VDATA])) * (size_t)num_verts_dst, __func__);
        BKE_defvert_extract_vgroup_to_vertweights(
            mdef, vg_idx, num_verts_dst, weights[VDATA], invert_vgroup);
      }

      if (data_transfer_layersmapping_generate(&lay_map,
                                               ob_src,
                                               ob_dst,
                                               me_src,
                                               me_dst,
                                               ME_VERT,
                                               cddata_type,
                                               mix_mode,
                                               mix_factor,
                                               weights[VDATA],
                                               num_verts_dst,
                                               use_create,
                                               use_delete,
                                               fromlayers,
                                               tolayers,
                                               space_transform)) {
        CustomDataTransferLayerMap *lay_mapit;

        changed = (lay_map.first != NULL);

        for (lay_mapit = lay_map.first; lay_mapit; lay_mapit = lay_mapit->next) {
          CustomData_data_transfer(&geom_map[VDATA], lay_mapit);
        }

        BLI_freelistN(&lay_map);
      }
    }
    if (DT_DATATYPE_IS_EDGE(dtdata_type)) {
      MVert *verts_dst = me_dst->mvert;
      const int num_verts_dst = me_dst->totvert;
      MEdge *edges_dst = me_dst->medge;
      const int num_edges_dst = me_dst->totedge;

      if (!geom_map_init[EDATA]) {
        const int num_edges_src = me_src->totedge;

        if ((map_edge_mode == MREMAP_MODE_TOPOLOGY) && (num_edges_dst != num_edges_src)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source and destination meshes do not have the same amount of edges, "
                     "'Topology' mapping cannot be used in this case");
          continue;
        }
        if ((map_edge_mode & MREMAP_USE_POLY) && (me_src->totpoly == 0)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source mesh doesn't have any faces, "
                     "None of the 'Face' mappings can be used in this case");
          continue;
        }
        if (ELEM(0, num_edges_dst, num_edges_src)) {
          BKE_report(
              reports,
              RPT_ERROR,
              "Source or destination meshes do not have any edges, cannot transfer edge data");
          continue;
        }

        BKE_mesh_remap_calc_edges_from_mesh(map_edge_mode,
                                            space_transform,
                                            max_distance,
                                            ray_radius,
                                            verts_dst,
                                            num_verts_dst,
                                            edges_dst,
                                            num_edges_dst,
                                            dirty_nors_dst,
                                            me_src,
                                            &geom_map[EDATA]);
        geom_map_init[EDATA] = true;
      }

      if (mdef && vg_idx != -1 && !weights[EDATA]) {
        weights[EDATA] = MEM_mallocN(sizeof(*weights[EDATA]) * (size_t)num_edges_dst, __func__);
        BKE_defvert_extract_vgroup_to_edgeweights(
            mdef, vg_idx, num_verts_dst, edges_dst, num_edges_dst, weights[EDATA], invert_vgroup);
      }

      if (data_transfer_layersmapping_generate(&lay_map,
                                               ob_src,
                                               ob_dst,
                                               me_src,
                                               me_dst,
                                               ME_EDGE,
                                               cddata_type,
                                               mix_mode,
                                               mix_factor,
                                               weights[EDATA],
                                               num_edges_dst,
                                               use_create,
                                               use_delete,
                                               fromlayers,
                                               tolayers,
                                               space_transform)) {
        CustomDataTransferLayerMap *lay_mapit;

        changed = (lay_map.first != NULL);

        for (lay_mapit = lay_map.first; lay_mapit; lay_mapit = lay_mapit->next) {
          CustomData_data_transfer(&geom_map[EDATA], lay_mapit);
        }

        BLI_freelistN(&lay_map);
      }
    }
    if (DT_DATATYPE_IS_LOOP(dtdata_type)) {
      MVert *verts_dst = me_dst->mvert;
      const int num_verts_dst = me_dst->totvert;
      MEdge *edges_dst = me_dst->medge;
      const int num_edges_dst = me_dst->totedge;
      MPoly *polys_dst = me_dst->mpoly;
      const int num_polys_dst = me_dst->totpoly;
      MLoop *loops_dst = me_dst->mloop;
      const int num_loops_dst = me_dst->totloop;
      CustomData *pdata_dst = &me_dst->pdata;
      CustomData *ldata_dst = &me_dst->ldata;

      MeshRemapIslandsCalc island_callback = data_transfer_get_loop_islands_generator(cddata_type);

      if (!geom_map_init[LDATA]) {
        const int num_loops_src = me_src->totloop;

        if ((map_loop_mode == MREMAP_MODE_TOPOLOGY) && (num_loops_dst != num_loops_src)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source and destination meshes do not have the same amount of face corners, "
                     "'Topology' mapping cannot be used in this case");
          continue;
        }
        if ((map_loop_mode & MREMAP_USE_EDGE) && (me_src->totedge == 0)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source mesh doesn't have any edges, "
                     "None of the 'Edge' mappings can be used in this case");
          continue;
        }
        if (ELEM(0, num_loops_dst, num_loops_src)) {
          BKE_report(
              reports,
              RPT_ERROR,
              "Source or destination meshes do not have any faces, cannot transfer corner data");
          continue;
        }

        BKE_mesh_remap_calc_loops_from_mesh(map_loop_mode,
                                            space_transform,
                                            max_distance,
                                            ray_radius,
                                            verts_dst,
                                            num_verts_dst,
                                            edges_dst,
                                            num_edges_dst,
                                            loops_dst,
                                            num_loops_dst,
                                            polys_dst,
                                            num_polys_dst,
                                            ldata_dst,
                                            pdata_dst,
                                            (me_dst->flag & ME_AUTOSMOOTH) != 0,
                                            me_dst->smoothresh,
                                            dirty_nors_dst,
                                            me_src,
                                            island_callback,
                                            islands_handling_precision,
                                            &geom_map[LDATA]);
        geom_map_init[LDATA] = true;
      }

      if (mdef && vg_idx != -1 && !weights[LDATA]) {
        weights[LDATA] = MEM_mallocN(sizeof(*weights[LDATA]) * (size_t)num_loops_dst, __func__);
        BKE_defvert_extract_vgroup_to_loopweights(
            mdef, vg_idx, num_verts_dst, loops_dst, num_loops_dst, weights[LDATA], invert_vgroup);
      }

      if (data_transfer_layersmapping_generate(&lay_map,
                                               ob_src,
                                               ob_dst,
                                               me_src,
                                               me_dst,
                                               ME_LOOP,
                                               cddata_type,
                                               mix_mode,
                                               mix_factor,
                                               weights[LDATA],
                                               num_loops_dst,
                                               use_create,
                                               use_delete,
                                               fromlayers,
                                               tolayers,
                                               space_transform)) {
        CustomDataTransferLayerMap *lay_mapit;

        changed = (lay_map.first != NULL);

        for (lay_mapit = lay_map.first; lay_mapit; lay_mapit = lay_mapit->next) {
          CustomData_data_transfer(&geom_map[LDATA], lay_mapit);
        }

        BLI_freelistN(&lay_map);
      }
    }
    if (DT_DATATYPE_IS_POLY(dtdata_type)) {
      MVert *verts_dst = me_dst->mvert;
      const int num_verts_dst = me_dst->totvert;
      MPoly *polys_dst = me_dst->mpoly;
      const int num_polys_dst = me_dst->totpoly;
      MLoop *loops_dst = me_dst->mloop;
      const int num_loops_dst = me_dst->totloop;
      CustomData *pdata_dst = &me_dst->pdata;

      if (!geom_map_init[PDATA]) {
        const int num_polys_src = me_src->totpoly;

        if ((map_poly_mode == MREMAP_MODE_TOPOLOGY) && (num_polys_dst != num_polys_src)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source and destination meshes do not have the same amount of faces, "
                     "'Topology' mapping cannot be used in this case");
          continue;
        }
        if ((map_poly_mode & MREMAP_USE_EDGE) && (me_src->totedge == 0)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source mesh doesn't have any edges, "
                     "None of the 'Edge' mappings can be used in this case");
          continue;
        }
        if (ELEM(0, num_polys_dst, num_polys_src)) {
          BKE_report(
              reports,
              RPT_ERROR,
              "Source or destination meshes do not have any faces, cannot transfer face data");
          continue;
        }

        BKE_mesh_remap_calc_polys_from_mesh(map_poly_mode,
                                            space_transform,
                                            max_distance,
                                            ray_radius,
                                            verts_dst,
                                            num_verts_dst,
                                            loops_dst,
                                            num_loops_dst,
                                            polys_dst,
                                            num_polys_dst,
                                            pdata_dst,
                                            dirty_nors_dst,
                                            me_src,
                                            &geom_map[PDATA]);
        geom_map_init[PDATA] = true;
      }

      if (mdef && vg_idx != -1 && !weights[PDATA]) {
        weights[PDATA] = MEM_mallocN(sizeof(*weights[PDATA]) * (size_t)num_polys_dst, __func__);
        BKE_defvert_extract_vgroup_to_polyweights(mdef,
                                                  vg_idx,
                                                  num_verts_dst,
                                                  loops_dst,
                                                  num_loops_dst,
                                                  polys_dst,
                                                  num_polys_dst,
                                                  weights[PDATA],
                                                  invert_vgroup);
      }

      if (data_transfer_layersmapping_generate(&lay_map,
                                               ob_src,
                                               ob_dst,
                                               me_src,
                                               me_dst,
                                               ME_POLY,
                                               cddata_type,
                                               mix_mode,
                                               mix_factor,
                                               weights[PDATA],
                                               num_polys_dst,
                                               use_create,
                                               use_delete,
                                               fromlayers,
                                               tolayers,
                                               space_transform)) {
        CustomDataTransferLayerMap *lay_mapit;

        changed = (lay_map.first != NULL);

        for (lay_mapit = lay_map.first; lay_mapit; lay_mapit = lay_mapit->next) {
          CustomData_data_transfer(&geom_map[PDATA], lay_mapit);
        }

        BLI_freelistN(&lay_map);
      }
    }

    data_transfer_dtdata_type_postprocess(ob_src, ob_dst, me_src, me_dst, dtdata_type, changed);
  }

  for (i = 0; i < DATAMAX; i++) {
    BKE_mesh_remap_free(&geom_map[i]);
    MEM_SAFE_FREE(weights[i]);
  }

  return changed;

#undef VDATA
#undef EDATA
#undef LDATA
#undef PDATA
#undef DATAMAX
}

bool BKE_object_data_transfer_mesh(struct Depsgraph *depsgraph,
                                   Scene *scene,
                                   Object *ob_src,
                                   Object *ob_dst,
                                   const int data_types,
                                   const bool use_create,
                                   const int map_vert_mode,
                                   const int map_edge_mode,
                                   const int map_loop_mode,
                                   const int map_poly_mode,
                                   SpaceTransform *space_transform,
                                   const bool auto_transform,
                                   const float max_distance,
                                   const float ray_radius,
                                   const float islands_handling_precision,
                                   const int fromlayers_select[DT_MULTILAYER_INDEX_MAX],
                                   const int tolayers_select[DT_MULTILAYER_INDEX_MAX],
                                   const int mix_mode,
                                   const float mix_factor,
                                   const char *vgroup_name,
                                   const bool invert_vgroup,
                                   ReportList *reports)
{
  return BKE_object_data_transfer_ex(depsgraph,
                                     scene,
                                     ob_src,
                                     ob_dst,
                                     NULL,
                                     data_types,
                                     use_create,
                                     map_vert_mode,
                                     map_edge_mode,
                                     map_loop_mode,
                                     map_poly_mode,
                                     space_transform,
                                     auto_transform,
                                     max_distance,
                                     ray_radius,
                                     islands_handling_precision,
                                     fromlayers_select,
                                     tolayers_select,
                                     mix_mode,
                                     mix_factor,
                                     vgroup_name,
                                     invert_vgroup,
                                     reports);
}
