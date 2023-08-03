/* SPDX-FileCopyrightText: 2014 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_customdata.h"
#include "BKE_data_transfer.h"
#include "BKE_deform.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_mesh_remap.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"
#include "BKE_report.h"

#include "DEG_depsgraph_query.h"

#include "data_transfer_intern.h"

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
      else if (DT_DATATYPE_IS_FACE(dtdata_type)) {
        r_data_masks->pmask |= 1LL << cddata_type;
      }
    }
    else if (cddata_type == CD_FAKE_MDEFORMVERT) {
      r_data_masks->vmask |= CD_MASK_MDEFORMVERT; /* Exception for vgroups :/ */
    }
    else if (cddata_type == CD_FAKE_UV) {
      r_data_masks->lmask |= CD_MASK_PROP_FLOAT2;
    }
    else if (cddata_type == CD_FAKE_LNOR) {
      r_data_masks->lmask |= CD_MASK_NORMAL | CD_MASK_CUSTOMLOOPNORMAL;
    }
  }
}

bool BKE_object_data_transfer_get_dttypes_capacity(const int dtdata_types,
                                                   bool *r_advanced_mixing,
                                                   bool *r_threshold)
{
  bool ret = false;

  *r_advanced_mixing = false;
  *r_threshold = false;

  for (int i = 0; (i < DT_TYPE_MAX) && !(ret && *r_advanced_mixing && *r_threshold); i++) {
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
      case DT_TYPE_MPROPCOL_VERT:
      case DT_TYPE_MLOOPCOL_VERT:
      case DT_TYPE_MPROPCOL_LOOP:
      case DT_TYPE_MLOOPCOL_LOOP:
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
    if (DT_DATATYPE_IS_FACE(dtdata_type)) {
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
    case DT_TYPE_LNOR:
      return CD_FAKE_LNOR;
    case DT_TYPE_MLOOPCOL_VERT:
    case DT_TYPE_MLOOPCOL_LOOP:
      return CD_PROP_BYTE_COLOR;
    case DT_TYPE_MPROPCOL_VERT:
    case DT_TYPE_MPROPCOL_LOOP:
      return CD_PROP_COLOR;
    default:
      BLI_assert_unreachable();
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
    case DT_TYPE_MPROPCOL_VERT:
    case DT_TYPE_MLOOPCOL_VERT:
    case DT_TYPE_MPROPCOL_VERT | DT_TYPE_MLOOPCOL_VERT:
      return DT_MULTILAYER_INDEX_VCOL_VERT;
    case DT_TYPE_MPROPCOL_LOOP:
    case DT_TYPE_MLOOPCOL_LOOP:
    case DT_TYPE_MPROPCOL_LOOP | DT_TYPE_MLOOPCOL_LOOP:
      return DT_MULTILAYER_INDEX_VCOL_LOOP;
    default:
      return DT_MULTILAYER_INDEX_INVALID;
  }
}

/* ********** */

/**
 * When transferring color attributes, also transfer the active color attribute string.
 * If a match can't be found, use the first color layer that can be found (to ensure a valid string
 * is set).
 */
static void data_transfer_mesh_attributes_transfer_active_color_string(
    Mesh *mesh_dst, const Mesh *mesh_src, const eAttrDomainMask mask_domain, const int data_type)
{
  if (mesh_dst->active_color_attribute) {
    return;
  }

  const char *active_color_src = BKE_id_attributes_active_color_name(&mesh_src->id);

  if ((data_type == CD_PROP_COLOR) && !BKE_id_attribute_search(&const_cast<ID &>(mesh_src->id),
                                                               active_color_src,
                                                               CD_MASK_PROP_COLOR,
                                                               ATTR_DOMAIN_MASK_COLOR))
  {
    return;
  }
  else if ((data_type == CD_PROP_BYTE_COLOR) &&
           !BKE_id_attribute_search(&const_cast<ID &>(mesh_src->id),
                                    active_color_src,
                                    CD_MASK_PROP_BYTE_COLOR,
                                    ATTR_DOMAIN_MASK_COLOR))
  {
    return;
  }

  if ((data_type == CD_PROP_COLOR) &&
      BKE_id_attribute_search(
          &mesh_dst->id, active_color_src, CD_MASK_PROP_COLOR, ATTR_DOMAIN_MASK_COLOR))
  {
    mesh_dst->active_color_attribute = BLI_strdup(active_color_src);
  }
  else if ((data_type == CD_PROP_BYTE_COLOR) &&
           BKE_id_attribute_search(
               &mesh_dst->id, active_color_src, CD_MASK_PROP_BYTE_COLOR, ATTR_DOMAIN_MASK_COLOR))
  {
    mesh_dst->active_color_attribute = BLI_strdup(active_color_src);
  }
  else {
    CustomDataLayer *first_color_layer = BKE_id_attribute_from_index(
        &mesh_dst->id, 0, mask_domain, CD_MASK_COLOR_ALL);
    if (first_color_layer != nullptr) {
      mesh_dst->active_color_attribute = BLI_strdup(first_color_layer->name);
    }
  }
}

/**
 * When transferring color attributes, also transfer the default color attribute string.
 * If a match cant be found, use the first color layer that can be found (to ensure a valid string
 * is set).
 */
static void data_transfer_mesh_attributes_transfer_default_color_string(
    Mesh *mesh_dst, const Mesh *mesh_src, const eAttrDomainMask mask_domain, const int data_type)
{
  if (mesh_dst->default_color_attribute) {
    return;
  }

  const char *default_color_src = BKE_id_attributes_default_color_name(&mesh_src->id);

  if ((data_type == CD_PROP_COLOR) && !BKE_id_attribute_search(&const_cast<ID &>(mesh_src->id),
                                                               default_color_src,
                                                               CD_MASK_PROP_COLOR,
                                                               ATTR_DOMAIN_MASK_COLOR))
  {
    return;
  }
  else if ((data_type == CD_PROP_BYTE_COLOR) &&
           !BKE_id_attribute_search(&const_cast<ID &>(mesh_src->id),
                                    default_color_src,
                                    CD_MASK_PROP_BYTE_COLOR,
                                    ATTR_DOMAIN_MASK_COLOR))
  {
    return;
  }

  if ((data_type == CD_PROP_COLOR) &&
      BKE_id_attribute_search(
          &mesh_dst->id, default_color_src, CD_MASK_PROP_COLOR, ATTR_DOMAIN_MASK_COLOR))
  {
    mesh_dst->default_color_attribute = BLI_strdup(default_color_src);
  }
  else if ((data_type == CD_PROP_BYTE_COLOR) &&
           BKE_id_attribute_search(
               &mesh_dst->id, default_color_src, CD_MASK_PROP_BYTE_COLOR, ATTR_DOMAIN_MASK_COLOR))
  {
    mesh_dst->default_color_attribute = BLI_strdup(default_color_src);
  }
  else {
    CustomDataLayer *first_color_layer = BKE_id_attribute_from_index(
        &mesh_dst->id, 0, mask_domain, CD_MASK_COLOR_ALL);
    if (first_color_layer != nullptr) {
      mesh_dst->default_color_attribute = BLI_strdup(first_color_layer->name);
    }
  }
}

/* ********** */

/* Generic pre/post processing, only used by custom loop normals currently. */

static void data_transfer_dtdata_type_preprocess(const Mesh *me_src,
                                                 Mesh *me_dst,
                                                 const int dtdata_type,
                                                 const bool dirty_nors_dst)
{
  if (dtdata_type == DT_TYPE_LNOR) {
    /* Compute custom normals into regular loop normals, which will be used for the transfer. */
    CustomData *ldata_dst = &me_dst->loop_data;

    const bool use_split_nors_dst = (me_dst->flag & ME_AUTOSMOOTH) != 0;
    const float split_angle_dst = me_dst->smoothresh;

    /* This should be ensured by cddata_masks we pass to code generating/giving us me_src now. */
    BLI_assert(CustomData_get_layer(&me_src->loop_data, CD_NORMAL) != nullptr);
    (void)me_src;

    const blender::short2 *custom_nors_dst = static_cast<const blender::short2 *>(
        CustomData_get_layer(ldata_dst, CD_CUSTOMLOOPNORMAL));

    /* Cache loop nors into a temp CDLayer. */
    blender::float3 *loop_nors_dst = static_cast<blender::float3 *>(
        CustomData_get_layer_for_write(ldata_dst, CD_NORMAL, me_dst->totloop));
    const bool do_loop_nors_dst = (loop_nors_dst == nullptr);
    if (do_loop_nors_dst) {
      loop_nors_dst = static_cast<blender::float3 *>(
          CustomData_add_layer(ldata_dst, CD_NORMAL, CD_SET_DEFAULT, me_dst->totloop));
      CustomData_set_layer_flag(ldata_dst, CD_NORMAL, CD_FLAG_TEMPORARY);
    }
    if (dirty_nors_dst || do_loop_nors_dst) {
      const bool *sharp_edges = static_cast<const bool *>(
          CustomData_get_layer_named(&me_dst->edge_data, CD_PROP_BOOL, "sharp_edge"));
      const bool *sharp_faces = static_cast<const bool *>(
          CustomData_get_layer_named(&me_dst->face_data, CD_PROP_BOOL, "sharp_face"));
      blender::bke::mesh::normals_calc_loop(me_dst->vert_positions(),
                                            me_dst->edges(),
                                            me_dst->faces(),
                                            me_dst->corner_verts(),
                                            me_dst->corner_edges(),
                                            {},
                                            me_dst->vert_normals(),
                                            me_dst->face_normals(),
                                            sharp_edges,
                                            sharp_faces,
                                            custom_nors_dst,
                                            use_split_nors_dst,
                                            split_angle_dst,
                                            nullptr,
                                            {loop_nors_dst, me_dst->totloop});
    }
  }
}

static void data_transfer_dtdata_type_postprocess(Mesh *me_dst,
                                                  const int dtdata_type,
                                                  const bool changed)
{
  using namespace blender;
  if (dtdata_type == DT_TYPE_LNOR) {
    if (!changed) {
      return;
    }
    /* Bake edited destination loop normals into custom normals again. */
    CustomData *ldata_dst = &me_dst->loop_data;

    blender::float3 *loop_nors_dst = static_cast<blender::float3 *>(
        CustomData_get_layer_for_write(ldata_dst, CD_NORMAL, me_dst->totloop));
    blender::short2 *custom_nors_dst = static_cast<blender::short2 *>(
        CustomData_get_layer_for_write(ldata_dst, CD_CUSTOMLOOPNORMAL, me_dst->totloop));

    if (!custom_nors_dst) {
      custom_nors_dst = static_cast<blender::short2 *>(
          CustomData_add_layer(ldata_dst, CD_CUSTOMLOOPNORMAL, CD_SET_DEFAULT, me_dst->totloop));
    }

    bke::MutableAttributeAccessor attributes = me_dst->attributes_for_write();
    bke::SpanAttributeWriter<bool> sharp_edges = attributes.lookup_or_add_for_write_span<bool>(
        "sharp_edge", ATTR_DOMAIN_EDGE);
    const bool *sharp_faces = static_cast<const bool *>(
        CustomData_get_layer_named(&me_dst->face_data, CD_PROP_BOOL, "sharp_face"));
    /* Note loop_nors_dst contains our custom normals as transferred from source... */
    blender::bke::mesh::normals_loop_custom_set(me_dst->vert_positions(),
                                                me_dst->edges(),
                                                me_dst->faces(),
                                                me_dst->corner_verts(),
                                                me_dst->corner_edges(),
                                                me_dst->vert_normals(),
                                                me_dst->face_normals(),
                                                sharp_faces,
                                                sharp_edges.span,
                                                {loop_nors_dst, me_dst->totloop},
                                                {custom_nors_dst, me_dst->totloop});
    sharp_edges.finish();
  }
}

/* ********** */

static MeshRemapIslandsCalc data_transfer_get_loop_islands_generator(const int cddata_type)
{
  switch (cddata_type) {
    case CD_FAKE_UV:
      return BKE_mesh_calc_islands_loop_face_edgeseam;
    default:
      break;
  }
  return nullptr;
}

float data_transfer_interp_float_do(const int mix_mode,
                                    const float val_dst,
                                    const float val_src,
                                    const float mix_factor)
{
  float val_ret;

  if ((mix_mode == CDT_MIX_REPLACE_ABOVE_THRESHOLD && (val_dst < mix_factor)) ||
      (mix_mode == CDT_MIX_REPLACE_BELOW_THRESHOLD && (val_dst > mix_factor)))
  {
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

/* Helpers to match sources and destinations data layers
 * (also handles 'conversions' in CD_FAKE cases). */

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
  CustomDataTransferLayerMap *item = MEM_new<CustomDataTransferLayerMap>(__func__);

  BLI_assert(data_dst != nullptr);

  item->data_type = eCustomDataType(cddata_type);
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
                                                    const void *data_src,
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

/**
 * \note
 * All those layer mapping handlers return false *only* if they were given invalid parameters.
 * This means that even if they do nothing, they will return true if all given parameters were OK.
 * Also, r_map may be nullptr, in which case they will 'only' create/delete destination layers
 * according to given parameters.
 */
static bool data_transfer_layersmapping_cdlayers_multisrc_to_dst(ListBase *r_map,
                                                                 const eCustomDataType cddata_type,
                                                                 const int mix_mode,
                                                                 const float mix_factor,
                                                                 const float *mix_weights,
                                                                 const int num_elem_dst,
                                                                 const bool use_create,
                                                                 const bool use_delete,
                                                                 const CustomData *cd_src,
                                                                 CustomData *cd_dst,
                                                                 const int tolayers,
                                                                 const bool *use_layers_src,
                                                                 const int num_layers_src,
                                                                 cd_datatransfer_interp interp,
                                                                 void *interp_data)
{
  const void *data_src;
  void *data_dst = nullptr;
  int idx_src = num_layers_src;
  int idx_dst, tot_dst = CustomData_number_of_layers(cd_dst, cddata_type);
  bool *data_dst_to_delete = nullptr;

  if (!use_layers_src) {
    /* No source at all, we can only delete all destination if requested. */
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
        /* pass */
      }
      idx_src++;

      if (idx_dst < idx_src) {
        if (use_create) {
          /* Create as much data layers as necessary! */
          for (; idx_dst < idx_src; idx_dst++) {
            CustomData_add_layer(
                cd_dst, eCustomDataType(cddata_type), CD_SET_DEFAULT, num_elem_dst);
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
          data_dst = CustomData_get_layer_n_for_write(cd_dst, cddata_type, idx_src, num_elem_dst);
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
          data_dst_to_delete = static_cast<bool *>(
              MEM_mallocN(sizeof(*data_dst_to_delete) * size_t(tot_dst), __func__));
          memset(data_dst_to_delete, true, sizeof(*data_dst_to_delete) * size_t(tot_dst));
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
            CustomData_add_layer_named(
                cd_dst, eCustomDataType(cddata_type), CD_SET_DEFAULT, num_elem_dst, name);
            idx_dst = CustomData_get_named_layer(cd_dst, cddata_type, name);
          }
          else {
            /* If we are not allowed to create missing dst data layers,
             * just skip matching src one. */
            continue;
          }
        }
        else if (data_dst_to_delete) {
          data_dst_to_delete[idx_dst] = false;
        }
        if (r_map) {
          data_dst = CustomData_get_layer_n_for_write(cd_dst, cddata_type, idx_dst, num_elem_dst);
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
        /* NOTE:
         * This won't affect newly created layers, if any, since tot_dst has not been updated!
         * Also, looping backward ensures us we do not suffer
         * from index shifting when deleting a layer. */
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
                                                 const eCustomDataType cddata_type,
                                                 const int mix_mode,
                                                 const float mix_factor,
                                                 const float *mix_weights,
                                                 const int num_elem_dst,
                                                 const bool use_create,
                                                 const bool use_delete,
                                                 const CustomData *cd_src,
                                                 CustomData *cd_dst,
                                                 const int fromlayers,
                                                 const int tolayers,
                                                 cd_datatransfer_interp interp,
                                                 void *interp_data)
{
  int idx_src, idx_dst;
  const void *data_src;
  void *data_dst = nullptr;

  if (CustomData_layertype_is_singleton(cddata_type)) {
    if (!(data_src = CustomData_get_layer(cd_src, cddata_type))) {
      if (use_delete) {
        CustomData_free_layer(cd_dst, cddata_type, num_elem_dst, 0);
      }
      return true;
    }

    data_dst = CustomData_get_layer_for_write(cd_dst, cddata_type, num_elem_dst);
    if (!data_dst) {
      if (!use_create) {
        return true;
      }
      data_dst = CustomData_add_layer(
          cd_dst, eCustomDataType(cddata_type), CD_SET_DEFAULT, num_elem_dst);
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
    /* NOTE: use_delete has not much meaning in this case, ignored. */

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
      data_dst = CustomData_get_layer_n_for_write(cd_dst, cddata_type, idx_dst, num_elem_dst);
    }
    else if (tolayers == DT_LAYERS_ACTIVE_DST) {
      if ((idx_dst = CustomData_get_active_layer(cd_dst, cddata_type)) == -1) {
        if (!use_create) {
          return true;
        }
        data_dst = CustomData_add_layer(
            cd_dst, eCustomDataType(cddata_type), CD_SET_DEFAULT, num_elem_dst);
      }
      else {
        data_dst = CustomData_get_layer_n_for_write(cd_dst, cddata_type, idx_dst, num_elem_dst);
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
          CustomData_add_layer(cd_dst, eCustomDataType(cddata_type), CD_SET_DEFAULT, num_elem_dst);
        }
      }
      data_dst = CustomData_get_layer_n_for_write(cd_dst, cddata_type, idx_dst, num_elem_dst);
    }
    else if (tolayers == DT_LAYERS_NAME_DST) {
      const char *name = CustomData_get_layer_name(cd_src, cddata_type, idx_src);
      if ((idx_dst = CustomData_get_named_layer(cd_dst, cddata_type, name)) == -1) {
        if (!use_create) {
          return true;
        }
        CustomData_add_layer_named(
            cd_dst, eCustomDataType(cddata_type), CD_SET_DEFAULT, num_elem_dst, name);
        idx_dst = CustomData_get_named_layer(cd_dst, cddata_type, name);
      }
      data_dst = CustomData_get_layer_n_for_write(cd_dst, cddata_type, idx_dst, num_elem_dst);
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
    int num_src = CustomData_number_of_layers(cd_src, eCustomDataType(cddata_type));
    bool *use_layers_src = num_src ? static_cast<bool *>(MEM_mallocN(
                                         sizeof(*use_layers_src) * size_t(num_src), __func__)) :
                                     nullptr;
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
                                                 const Mesh *me_src,
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
  const CustomData *cd_src;
  CustomData *cd_dst;

  cd_datatransfer_interp interp = nullptr;
  void *interp_data = nullptr;

  if (elem_type == ME_VERT) {
    if (!(cddata_type & CD_FAKE)) {
      cd_src = &me_src->vert_data;
      cd_dst = &me_dst->vert_data;

      if (!data_transfer_layersmapping_cdlayers(r_map,
                                                eCustomDataType(cddata_type),
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                num_elem_dst,
                                                use_create,
                                                use_delete,
                                                cd_src,
                                                cd_dst,
                                                fromlayers,
                                                tolayers,
                                                interp,
                                                interp_data))
      {
        /* We handle specific source selection cases here. */
        return false;
      }
      return true;
    }
    if (cddata_type == CD_FAKE_MDEFORMVERT) {
      bool ret;

      cd_src = &me_src->vert_data;
      cd_dst = &me_dst->vert_data;

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
      return ret;
    }
    if (cddata_type == CD_FAKE_SHAPEKEY) {
      /* TODO: leaving shape-keys aside for now, quite specific case,
       * since we can't access them from mesh vertices :/ */
      return false;
    }
    if (r_map && cddata_type == CD_FAKE_BWEIGHT) {
      if (!CustomData_get_layer_named(&me_dst->vert_data, CD_PROP_FLOAT, "bevel_weight_vert")) {
        CustomData_add_layer_named(&me_dst->vert_data,
                                   CD_PROP_FLOAT,
                                   CD_SET_DEFAULT,
                                   me_dst->totvert,
                                   "bevel_weight_vert");
      }
      data_transfer_layersmapping_add_item_cd(
          r_map,
          CD_PROP_FLOAT,
          mix_mode,
          mix_factor,
          mix_weights,
          CustomData_get_layer_named(&me_src->vert_data, CD_PROP_FLOAT, "bevel_weight_vert"),
          CustomData_get_layer_named_for_write(
              &me_dst->vert_data, CD_PROP_FLOAT, "bevel_weight_vert", me_dst->totvert),
          interp,
          interp_data);
      return true;
    }
  }
  else if (elem_type == ME_EDGE) {
    if (!(cddata_type & CD_FAKE)) { /* Unused for edges, currently... */
      cd_src = &me_src->edge_data;
      cd_dst = &me_dst->edge_data;

      if (!data_transfer_layersmapping_cdlayers(r_map,
                                                eCustomDataType(cddata_type),
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                num_elem_dst,
                                                use_create,
                                                use_delete,
                                                cd_src,
                                                cd_dst,
                                                fromlayers,
                                                tolayers,
                                                interp,
                                                interp_data))
      {
        /* We handle specific source selection cases here. */
        return false;
      }
      return true;
    }
    if (r_map && cddata_type == CD_FAKE_SEAM) {
      if (!CustomData_has_layer_named(&me_dst->edge_data, CD_PROP_BOOL, ".uv_seam")) {
        CustomData_add_layer_named(
            &me_dst->edge_data, CD_PROP_BOOL, CD_SET_DEFAULT, me_dst->totedge, ".uv_seam");
      }
      data_transfer_layersmapping_add_item_cd(
          r_map,
          CD_PROP_BOOL,
          mix_mode,
          mix_factor,
          mix_weights,
          CustomData_get_layer_named(&me_src->edge_data, CD_PROP_BOOL, ".uv_seam"),
          CustomData_get_layer_named_for_write(
              &me_dst->edge_data, CD_PROP_BOOL, ".uv_seam", me_dst->totedge),
          interp,
          interp_data);
      return true;
    }
    if (r_map && cddata_type == CD_FAKE_SHARP) {
      if (!CustomData_has_layer_named(&me_dst->edge_data, CD_PROP_BOOL, "sharp_edge")) {
        CustomData_add_layer_named(
            &me_dst->edge_data, CD_PROP_BOOL, CD_SET_DEFAULT, me_dst->totedge, "sharp_edge");
      }
      data_transfer_layersmapping_add_item_cd(
          r_map,
          CD_PROP_BOOL,
          mix_mode,
          mix_factor,
          mix_weights,
          CustomData_get_layer_named(&me_src->edge_data, CD_PROP_BOOL, "sharp_edge"),
          CustomData_get_layer_named_for_write(
              &me_dst->edge_data, CD_PROP_BOOL, "sharp_edge", me_dst->totedge),
          interp,
          interp_data);
      return true;
    }
    if (r_map && cddata_type == CD_FAKE_BWEIGHT) {
      if (!CustomData_get_layer_named(&me_dst->edge_data, CD_PROP_FLOAT, "bevel_weight_edge")) {
        CustomData_add_layer_named(&me_dst->edge_data,
                                   CD_PROP_FLOAT,
                                   CD_SET_DEFAULT,
                                   me_dst->totedge,
                                   "bevel_weight_edge");
      }
      data_transfer_layersmapping_add_item_cd(
          r_map,
          CD_PROP_FLOAT,
          mix_mode,
          mix_factor,
          mix_weights,
          CustomData_get_layer_named(&me_src->edge_data, CD_PROP_FLOAT, "bevel_weight_edge"),
          CustomData_get_layer_named_for_write(
              &me_dst->edge_data, CD_PROP_FLOAT, "bevel_weight_edge", me_dst->totedge),
          interp,
          interp_data);
      return true;
    }

    return false;
  }
  else if (elem_type == ME_LOOP) {
    if (cddata_type == CD_FAKE_UV) {
      cddata_type = CD_PROP_FLOAT2;
    }
    else if (cddata_type == CD_FAKE_LNOR) {
      /* Pre-process should have generated it,
       * Post-process will convert it back to CD_CUSTOMLOOPNORMAL. */
      cddata_type = CD_NORMAL;
      interp_data = space_transform;
      interp = customdata_data_transfer_interp_normal_normals;
    }

    if (!(cddata_type & CD_FAKE)) {
      cd_src = &me_src->loop_data;
      cd_dst = &me_dst->loop_data;

      if (!data_transfer_layersmapping_cdlayers(r_map,
                                                eCustomDataType(cddata_type),
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                num_elem_dst,
                                                use_create,
                                                use_delete,
                                                cd_src,
                                                cd_dst,
                                                fromlayers,
                                                tolayers,
                                                interp,
                                                interp_data))
      {
        /* We handle specific source selection cases here. */
        return false;
      }
      return true;
    }

    return false;
  }
  else if (elem_type == ME_POLY) {
    if (cddata_type == CD_FAKE_UV) {
      cddata_type = CD_PROP_FLOAT2;
    }

    if (!(cddata_type & CD_FAKE)) {
      cd_src = &me_src->face_data;
      cd_dst = &me_dst->face_data;

      if (!data_transfer_layersmapping_cdlayers(r_map,
                                                eCustomDataType(cddata_type),
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                num_elem_dst,
                                                use_create,
                                                use_delete,
                                                cd_src,
                                                cd_dst,
                                                fromlayers,
                                                tolayers,
                                                interp,
                                                interp_data))
      {
        /* We handle specific source selection cases here. */
        return false;
      }
      return true;
    }
    if (r_map && cddata_type == CD_FAKE_SHARP) {
      if (!CustomData_has_layer_named(&me_dst->face_data, CD_PROP_BOOL, "sharp_face")) {
        CustomData_add_layer_named(
            &me_dst->face_data, CD_PROP_BOOL, CD_SET_DEFAULT, me_dst->faces_num, "sharp_face");
      }
      data_transfer_layersmapping_add_item_cd(
          r_map,
          CD_PROP_BOOL,
          mix_mode,
          mix_factor,
          mix_weights,
          CustomData_get_layer_named(&me_src->face_data, CD_PROP_BOOL, "sharp_face"),
          CustomData_get_layer_named_for_write(
              &me_dst->face_data, CD_PROP_BOOL, "sharp_face", num_elem_dst),
          interp,
          interp_data);
      return true;
    }

    return false;
  }

  return false;
}

void BKE_object_data_transfer_layout(Depsgraph *depsgraph,
                                     Object *ob_src,
                                     Object *ob_dst,
                                     const int data_types,
                                     const bool use_delete,
                                     const int fromlayers_select[DT_MULTILAYER_INDEX_MAX],
                                     const int tolayers_select[DT_MULTILAYER_INDEX_MAX])
{
  Mesh *me_dst;

  const bool use_create = true; /* We always create needed layers here. */

  BLI_assert((ob_src != ob_dst) && (ob_src->type == OB_MESH) && (ob_dst->type == OB_MESH));

  me_dst = static_cast<Mesh *>(ob_dst->data);

  /* Get source evaluated mesh. */
  const Object *ob_src_eval = DEG_get_evaluated_object(depsgraph, ob_src);
  const Mesh *me_src = BKE_object_get_evaluated_mesh(ob_src_eval);
  if (!me_src) {
    return;
  }

  /* Check all possible data types. */
  for (int i = 0; i < DT_TYPE_MAX; i++) {
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

      data_transfer_layersmapping_generate(nullptr,
                                           ob_src,
                                           ob_dst,
                                           me_src,
                                           me_dst,
                                           ME_VERT,
                                           cddata_type,
                                           0,
                                           0.0f,
                                           nullptr,
                                           num_elem_dst,
                                           use_create,
                                           use_delete,
                                           fromlayers,
                                           tolayers,
                                           nullptr);
      /* Make sure we have active/default color layers if none existed before.
       * Use the active/default from src (if it was transferred), otherwise the first. */
      if (ELEM(cddata_type, CD_PROP_COLOR, CD_PROP_BYTE_COLOR)) {
        data_transfer_mesh_attributes_transfer_active_color_string(
            me_dst, me_src, ATTR_DOMAIN_MASK_POINT, cddata_type);
        data_transfer_mesh_attributes_transfer_default_color_string(
            me_dst, me_src, ATTR_DOMAIN_MASK_POINT, cddata_type);
      }
    }
    if (DT_DATATYPE_IS_EDGE(dtdata_type)) {
      const int num_elem_dst = me_dst->totedge;

      data_transfer_layersmapping_generate(nullptr,
                                           ob_src,
                                           ob_dst,
                                           me_src,
                                           me_dst,
                                           ME_EDGE,
                                           cddata_type,
                                           0,
                                           0.0f,
                                           nullptr,
                                           num_elem_dst,
                                           use_create,
                                           use_delete,
                                           fromlayers,
                                           tolayers,
                                           nullptr);
    }
    if (DT_DATATYPE_IS_LOOP(dtdata_type)) {
      const int num_elem_dst = me_dst->totloop;

      data_transfer_layersmapping_generate(nullptr,
                                           ob_src,
                                           ob_dst,
                                           me_src,
                                           me_dst,
                                           ME_LOOP,
                                           cddata_type,
                                           0,
                                           0.0f,
                                           nullptr,
                                           num_elem_dst,
                                           use_create,
                                           use_delete,
                                           fromlayers,
                                           tolayers,
                                           nullptr);
      /* Make sure we have active/default color layers if none existed before.
       * Use the active/default from src (if it was transferred), otherwise the first. */
      if (ELEM(cddata_type, CD_PROP_COLOR, CD_PROP_BYTE_COLOR)) {
        data_transfer_mesh_attributes_transfer_active_color_string(
            me_dst, me_src, ATTR_DOMAIN_MASK_CORNER, cddata_type);
        data_transfer_mesh_attributes_transfer_default_color_string(
            me_dst, me_src, ATTR_DOMAIN_MASK_CORNER, cddata_type);
      }
    }
    if (DT_DATATYPE_IS_FACE(dtdata_type)) {
      const int num_elem_dst = me_dst->faces_num;

      data_transfer_layersmapping_generate(nullptr,
                                           ob_src,
                                           ob_dst,
                                           me_src,
                                           me_dst,
                                           ME_POLY,
                                           cddata_type,
                                           0,
                                           0.0f,
                                           nullptr,
                                           num_elem_dst,
                                           use_create,
                                           use_delete,
                                           fromlayers,
                                           tolayers,
                                           nullptr);
    }
  }
}

bool BKE_object_data_transfer_ex(Depsgraph *depsgraph,
                                 Object *ob_src,
                                 Object *ob_dst,
                                 Mesh *me_dst,
                                 const int data_types,
                                 bool use_create,
                                 const int map_vert_mode,
                                 const int map_edge_mode,
                                 const int map_loop_mode,
                                 const int map_face_mode,
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

  const Mesh *me_src;
  /* Assumed always true if not using an evaluated mesh as destination. */
  bool dirty_nors_dst = true;

  const MDeformVert *mdef = nullptr;
  int vg_idx = -1;
  float *weights[DATAMAX] = {nullptr};

  MeshPairRemap geom_map[DATAMAX] = {{0}};
  bool geom_map_init[DATAMAX] = {false};
  ListBase lay_map = {nullptr};
  bool changed = false;
  bool is_modifier = false;

  const bool use_delete = false; /* We never delete data layers from destination here. */

  BLI_assert((ob_src != ob_dst) && (ob_src->type == OB_MESH) && (ob_dst->type == OB_MESH));

  if (me_dst) {
    dirty_nors_dst = BKE_mesh_vert_normals_are_dirty(me_dst);
    /* Never create needed custom layers on passed destination mesh
     * (assumed to *not* be ob_dst->data, aka modifier case). */
    use_create = false;
    is_modifier = true;
  }
  else {
    me_dst = static_cast<Mesh *>(ob_dst->data);
  }

  if (vgroup_name) {
    mdef = static_cast<const MDeformVert *>(
        CustomData_get_layer(&me_dst->vert_data, CD_MDEFORMVERT));
    if (mdef) {
      vg_idx = BKE_id_defgroup_name_index(&me_dst->id, vgroup_name);
    }
  }

  /* Get source evaluated mesh. */
  if (is_modifier) {
    me_src = BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob_src);
  }
  else {
    const Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob_src);
    me_src = BKE_object_get_evaluated_mesh(ob_eval);
  }
  if (!me_src) {
    return changed;
  }
  BKE_mesh_wrapper_ensure_mdata(const_cast<Mesh *>(me_src));

  if (auto_transform) {
    if (space_transform == nullptr) {
      space_transform = &auto_space_transform;
    }

    BKE_mesh_remap_find_best_match_from_mesh(
        reinterpret_cast<const float(*)[3]>(me_dst->vert_positions().data()),
        me_dst->totvert,
        me_src,
        space_transform);
  }

  /* Check all possible data types.
   * Note item mappings and destination mix weights are cached. */
  for (int i = 0; i < DT_TYPE_MAX; i++) {
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
      blender::MutableSpan<blender::float3> positions_dst = me_dst->vert_positions_for_write();
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
        if ((map_vert_mode & MREMAP_USE_POLY) && (me_src->faces_num == 0)) {
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

        BKE_mesh_remap_calc_verts_from_mesh(
            map_vert_mode,
            space_transform,
            max_distance,
            ray_radius,
            reinterpret_cast<const float(*)[3]>(positions_dst.data()),
            num_verts_dst,
            dirty_nors_dst,
            me_src,
            me_dst,
            &geom_map[VDATA]);
        geom_map_init[VDATA] = true;
      }

      if (mdef && vg_idx != -1 && !weights[VDATA]) {
        weights[VDATA] = static_cast<float *>(
            MEM_mallocN(sizeof(*(weights[VDATA])) * size_t(num_verts_dst), __func__));
        BKE_defvert_extract_vgroup_to_vertweights(
            mdef, vg_idx, num_verts_dst, invert_vgroup, weights[VDATA]);
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
                                               space_transform))
      {
        CustomDataTransferLayerMap *lay_mapit;

        changed |= (lay_map.first != nullptr);

        for (lay_mapit = static_cast<CustomDataTransferLayerMap *>(lay_map.first); lay_mapit;
             lay_mapit = lay_mapit->next)
        {
          CustomData_data_transfer(&geom_map[VDATA], lay_mapit);
        }

        BLI_freelistN(&lay_map);
      }
    }
    if (DT_DATATYPE_IS_EDGE(dtdata_type)) {
      blender::MutableSpan<blender::float3> positions_dst = me_dst->vert_positions_for_write();

      const int num_verts_dst = me_dst->totvert;
      const blender::Span<blender::int2> edges_dst = me_dst->edges();

      if (!geom_map_init[EDATA]) {
        const int num_edges_src = me_src->totedge;

        if ((map_edge_mode == MREMAP_MODE_TOPOLOGY) && (edges_dst.size() != num_edges_src)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source and destination meshes do not have the same amount of edges, "
                     "'Topology' mapping cannot be used in this case");
          continue;
        }
        if ((map_edge_mode & MREMAP_USE_POLY) && (me_src->faces_num == 0)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source mesh doesn't have any faces, "
                     "None of the 'Face' mappings can be used in this case");
          continue;
        }
        if (ELEM(0, edges_dst.size(), num_edges_src)) {
          BKE_report(
              reports,
              RPT_ERROR,
              "Source or destination meshes do not have any edges, cannot transfer edge data");
          continue;
        }

        BKE_mesh_remap_calc_edges_from_mesh(
            map_edge_mode,
            space_transform,
            max_distance,
            ray_radius,
            reinterpret_cast<const float(*)[3]>(positions_dst.data()),
            num_verts_dst,
            edges_dst.data(),
            edges_dst.size(),
            dirty_nors_dst,
            me_src,
            me_dst,
            &geom_map[EDATA]);
        geom_map_init[EDATA] = true;
      }

      if (mdef && vg_idx != -1 && !weights[EDATA]) {
        weights[EDATA] = static_cast<float *>(
            MEM_mallocN(sizeof(*weights[EDATA]) * size_t(edges_dst.size()), __func__));
        BKE_defvert_extract_vgroup_to_edgeweights(mdef,
                                                  vg_idx,
                                                  num_verts_dst,
                                                  edges_dst.data(),
                                                  edges_dst.size(),
                                                  invert_vgroup,
                                                  weights[EDATA]);
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
                                               edges_dst.size(),
                                               use_create,
                                               use_delete,
                                               fromlayers,
                                               tolayers,
                                               space_transform))
      {
        CustomDataTransferLayerMap *lay_mapit;

        changed |= (lay_map.first != nullptr);

        for (lay_mapit = static_cast<CustomDataTransferLayerMap *>(lay_map.first); lay_mapit;
             lay_mapit = lay_mapit->next)
        {
          CustomData_data_transfer(&geom_map[EDATA], lay_mapit);
        }

        BLI_freelistN(&lay_map);
      }
    }
    if (DT_DATATYPE_IS_LOOP(dtdata_type)) {
      const blender::Span<blender::float3> positions_dst = me_dst->vert_positions();
      const int num_verts_dst = me_dst->totvert;
      const blender::Span<blender::int2> edges_dst = me_dst->edges();
      const blender::OffsetIndices faces_dst = me_dst->faces();
      const blender::Span<int> corner_verts_dst = me_dst->corner_verts();
      const blender::Span<int> corner_edges_dst = me_dst->corner_edges();
      CustomData *ldata_dst = &me_dst->loop_data;

      MeshRemapIslandsCalc island_callback = data_transfer_get_loop_islands_generator(cddata_type);

      if (!geom_map_init[LDATA]) {
        const int num_loops_src = me_src->totloop;

        if ((map_loop_mode == MREMAP_MODE_TOPOLOGY) && (corner_verts_dst.size() != num_loops_src))
        {
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
        if (ELEM(0, corner_verts_dst.size(), num_loops_src)) {
          BKE_report(
              reports,
              RPT_ERROR,
              "Source or destination meshes do not have any faces, cannot transfer corner data");
          continue;
        }

        BKE_mesh_remap_calc_loops_from_mesh(
            map_loop_mode,
            space_transform,
            max_distance,
            ray_radius,
            me_dst,
            reinterpret_cast<const float(*)[3]>(positions_dst.data()),
            num_verts_dst,
            edges_dst.data(),
            edges_dst.size(),
            corner_verts_dst.data(),
            corner_edges_dst.data(),
            corner_verts_dst.size(),
            faces_dst,
            ldata_dst,
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
        weights[LDATA] = static_cast<float *>(
            MEM_mallocN(sizeof(*weights[LDATA]) * size_t(corner_verts_dst.size()), __func__));
        BKE_defvert_extract_vgroup_to_loopweights(mdef,
                                                  vg_idx,
                                                  num_verts_dst,
                                                  corner_verts_dst.data(),
                                                  corner_verts_dst.size(),
                                                  invert_vgroup,
                                                  weights[LDATA]);
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
                                               corner_verts_dst.size(),
                                               use_create,
                                               use_delete,
                                               fromlayers,
                                               tolayers,
                                               space_transform))
      {
        CustomDataTransferLayerMap *lay_mapit;

        changed |= (lay_map.first != nullptr);

        for (lay_mapit = static_cast<CustomDataTransferLayerMap *>(lay_map.first); lay_mapit;
             lay_mapit = lay_mapit->next)
        {
          CustomData_data_transfer(&geom_map[LDATA], lay_mapit);
        }

        BLI_freelistN(&lay_map);
      }
    }
    if (DT_DATATYPE_IS_FACE(dtdata_type)) {
      const blender::Span<blender::float3> positions_dst = me_dst->vert_positions();
      const int num_verts_dst = me_dst->totvert;
      const blender::OffsetIndices faces_dst = me_dst->faces();
      const blender::Span<int> corner_verts_dst = me_dst->corner_verts();

      if (!geom_map_init[PDATA]) {
        const int num_faces_src = me_src->faces_num;

        if ((map_face_mode == MREMAP_MODE_TOPOLOGY) && (faces_dst.size() != num_faces_src)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source and destination meshes do not have the same amount of faces, "
                     "'Topology' mapping cannot be used in this case");
          continue;
        }
        if ((map_face_mode & MREMAP_USE_EDGE) && (me_src->totedge == 0)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source mesh doesn't have any edges, "
                     "None of the 'Edge' mappings can be used in this case");
          continue;
        }
        if (ELEM(0, faces_dst.size(), num_faces_src)) {
          BKE_report(
              reports,
              RPT_ERROR,
              "Source or destination meshes do not have any faces, cannot transfer face data");
          continue;
        }

        BKE_mesh_remap_calc_faces_from_mesh(
            map_face_mode,
            space_transform,
            max_distance,
            ray_radius,
            me_dst,
            reinterpret_cast<const float(*)[3]>(positions_dst.data()),
            num_verts_dst,
            corner_verts_dst.data(),
            faces_dst,
            me_src,
            &geom_map[PDATA]);
        geom_map_init[PDATA] = true;
      }

      if (mdef && vg_idx != -1 && !weights[PDATA]) {
        weights[PDATA] = static_cast<float *>(
            MEM_mallocN(sizeof(*weights[PDATA]) * faces_dst.size(), __func__));
        BKE_defvert_extract_vgroup_to_faceweights(mdef,
                                                  vg_idx,
                                                  num_verts_dst,
                                                  corner_verts_dst.data(),
                                                  corner_verts_dst.size(),
                                                  faces_dst,
                                                  invert_vgroup,
                                                  weights[PDATA]);
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
                                               faces_dst.size(),
                                               use_create,
                                               use_delete,
                                               fromlayers,
                                               tolayers,
                                               space_transform))
      {
        CustomDataTransferLayerMap *lay_mapit;

        changed |= (lay_map.first != nullptr);

        for (lay_mapit = static_cast<CustomDataTransferLayerMap *>(lay_map.first); lay_mapit;
             lay_mapit = lay_mapit->next)
        {
          CustomData_data_transfer(&geom_map[PDATA], lay_mapit);
        }

        BLI_freelistN(&lay_map);
      }
    }

    data_transfer_dtdata_type_postprocess(me_dst, dtdata_type, changed);
  }

  for (int i = 0; i < DATAMAX; i++) {
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

bool BKE_object_data_transfer_mesh(Depsgraph *depsgraph,
                                   Object *ob_src,
                                   Object *ob_dst,
                                   const int data_types,
                                   const bool use_create,
                                   const int map_vert_mode,
                                   const int map_edge_mode,
                                   const int map_loop_mode,
                                   const int map_face_mode,
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
                                     ob_src,
                                     ob_dst,
                                     nullptr,
                                     data_types,
                                     use_create,
                                     map_vert_mode,
                                     map_edge_mode,
                                     map_loop_mode,
                                     map_face_mode,
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
