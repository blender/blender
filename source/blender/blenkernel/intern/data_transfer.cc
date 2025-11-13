/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_attribute_legacy_convert.hh"
#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math_base.h"
#include "BLI_math_matrix.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_data_transfer.h"
#include "BKE_deform.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_mesh_remap.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph_query.hh"

#include "data_transfer_intern.hh"

using blender::StringRef;
using blender::Vector;

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
      return CD_FAKE_FREESTYLE_EDGE;

    case DT_TYPE_UV:
      return CD_FAKE_UV;
    case DT_TYPE_SHARP_FACE:
      return CD_FAKE_SHARP;
    case DT_TYPE_FREESTYLE_FACE:
      return CD_FAKE_FREESTYLE_FACE;
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
static void transfer_active_color_string(Mesh *mesh_dst, const Mesh *mesh_src)
{
  using namespace blender;
  if (mesh_dst->active_color_attribute) {
    return;
  }

  const StringRef name = mesh_src->active_color_attribute;
  const bke::AttributeAccessor attributes_src = mesh_src->attributes();
  const bke::AttributeAccessor attributes_dst = mesh_dst->attributes();

  if (!bke::mesh::is_color_attribute(attributes_src.lookup_meta_data(name))) {
    return;
  }

  if (bke::mesh::is_color_attribute(attributes_dst.lookup_meta_data(name))) {
    mesh_dst->active_color_attribute = BLI_strdupn(name.data(), name.size());
  }
  else {
    mesh_dst->attributes().foreach_attribute([&](const bke::AttributeIter &iter) {
      if (mesh_dst->active_color_attribute) {
        return;
      }
      if (!bke::mesh::is_color_attribute({iter.domain, iter.data_type})) {
        return;
      }
      mesh_dst->active_color_attribute = BLI_strdupn(iter.name.data(), iter.name.size());
    });
  }
}

/**
 * When transferring color attributes, also transfer the default color attribute string.
 * If a match cant be found, use the first color layer that can be found (to ensure a valid string
 * is set).
 */
static void transfer_default_color_string(Mesh *mesh_dst, const Mesh *mesh_src)
{
  using namespace blender;
  if (mesh_dst->default_color_attribute) {
    return;
  }

  const StringRef name = mesh_src->default_color_attribute;
  const bke::AttributeAccessor attributes_src = mesh_src->attributes();
  const bke::AttributeAccessor attributes_dst = mesh_dst->attributes();

  if (!bke::mesh::is_color_attribute(attributes_src.lookup_meta_data(name))) {
    return;
  }

  if (bke::mesh::is_color_attribute(attributes_dst.lookup_meta_data(name))) {
    mesh_dst->default_color_attribute = BLI_strdupn(name.data(), name.size());
  }
  else {
    mesh_dst->attributes().foreach_attribute([&](const bke::AttributeIter &iter) {
      if (mesh_dst->default_color_attribute) {
        return;
      }
      if (!bke::mesh::is_color_attribute({iter.domain, iter.data_type})) {
        return;
      }
      mesh_dst->default_color_attribute = BLI_strdupn(iter.name.data(), iter.name.size());
    });
  }
}

/* ********** */

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
    CustomData *ldata_dst = &me_dst->corner_data;

    blender::float3 *loop_nors_dst = static_cast<blender::float3 *>(
        CustomData_get_layer_for_write(ldata_dst, CD_NORMAL, me_dst->corners_num));

    bke::MutableAttributeAccessor attributes = me_dst->attributes_for_write();
    bke::SpanAttributeWriter custom_nors_dst = attributes.lookup_or_add_for_write_span<short2>(
        "custom_normal", bke::AttrDomain::Corner);
    if (!custom_nors_dst) {
      return;
    }
    bke::SpanAttributeWriter<bool> sharp_edges = attributes.lookup_or_add_for_write_span<bool>(
        "sharp_edge", bke::AttrDomain::Edge);
    const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face", bke::AttrDomain::Face);
    /* Note loop_nors_dst contains our custom normals as transferred from source... */
    blender::bke::mesh::normals_corner_custom_set(me_dst->vert_positions(),
                                                  me_dst->faces(),
                                                  me_dst->corner_verts(),
                                                  me_dst->corner_edges(),
                                                  me_dst->vert_to_face_map(),
                                                  me_dst->vert_normals(),
                                                  me_dst->face_normals_true(),
                                                  sharp_faces,
                                                  sharp_edges.span,
                                                  {loop_nors_dst, me_dst->corners_num},
                                                  custom_nors_dst.span);
    custom_nors_dst.finish();
    sharp_edges.finish();
    CustomData_free_layers(ldata_dst, CD_NORMAL);
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

static void data_transfer_layersmapping_add_item(
    Vector<CustomDataTransferLayerMap> *r_map,
    const int cddata_type,
    const int mix_mode,
    const float mix_factor,
    const float *mix_weights,
    std::variant<const void *, blender::GVArray> data_src,
    std::variant<void *, blender::bke::GSpanAttributeWriter> data_dst,
    const int data_src_n,
    const int data_dst_n,
    const size_t elem_size,
    const size_t data_size,
    const size_t data_offset,
    cd_datatransfer_interp interp,
    void *interp_data)
{
  CustomDataTransferLayerMap item{};

  BLI_assert(std::visit([](const auto &value) { return bool(value); }, data_dst));

  item.data_type = cddata_type;
  item.mix_mode = mix_mode;
  item.mix_factor = mix_factor;
  item.mix_weights = mix_weights;

  item.data_src = std::move(data_src);
  if (auto *attr = std::get_if<blender::bke::GSpanAttributeWriter>(&data_dst)) {
    item.data_dst = std::move(attr->span);
    item.tag_modified_fn = std::move(attr->tag_modified_fn);
  }
  else {
    item.data_dst = std::get<void *>(data_dst);
  }
  item.data_src_n = data_src_n;
  item.data_dst_n = data_dst_n;
  item.elem_size = elem_size;

  item.data_size = data_size;
  item.data_offset = data_offset;

  item.interp = interp;
  item.interp_data = interp_data;

  r_map->append(std::move(item));
}

void data_transfer_layersmapping_add_item(Vector<CustomDataTransferLayerMap> *r_map,
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
                                          cd_datatransfer_interp interp,
                                          void *interp_data)
{
  data_transfer_layersmapping_add_item(
      r_map,
      cddata_type,
      mix_mode,
      mix_factor,
      mix_weights,
      std::variant<const void *, blender::GVArray>(data_src),
      std::variant<void *, blender::bke::GSpanAttributeWriter>(data_dst),
      data_src_n,
      data_dst_n,
      elem_size,
      data_size,
      data_offset,
      interp,
      interp_data);
}

static void data_transfer_layersmapping_add_item_cd(
    Vector<CustomDataTransferLayerMap> *r_map,
    const int cddata_type,
    const int mix_mode,
    const float mix_factor,
    const float *mix_weights,
    std::variant<const void *, blender::GVArray> data_src,
    std::variant<void *, blender::bke::GSpanAttributeWriter> data_dst,
    cd_datatransfer_interp interp,
    void *interp_data)
{
  data_transfer_layersmapping_add_item(r_map,
                                       cddata_type,
                                       mix_mode,
                                       mix_factor,
                                       mix_weights,
                                       std::move(data_src),
                                       std::move(data_dst),
                                       0,
                                       0,
                                       0,
                                       0,
                                       0,
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
static bool data_transfer_layersmapping_cdlayers_multisrc_to_dst(
    Vector<CustomDataTransferLayerMap> *r_map,
    const eCustomDataType cddata_type,
    const blender::bke::AttrDomain domain,
    const int mix_mode,
    const float mix_factor,
    const float *mix_weights,
    const bool use_create,
    const bool use_delete,
    const Mesh &mesh_src,
    Mesh &mesh_dst,
    const blender::Span<std::string> src_names,
    const blender::Span<std::string> dst_names,
    const int tolayers,
    const bool *use_layers_src,
    const int num_layers_src)
{
  using namespace blender;
  bke::AttributeAccessor src_attributes = mesh_src.attributes();
  bke::MutableAttributeAccessor dst_attributes = mesh_dst.attributes_for_write();
  const bke::AttrType attr_type = *bke::custom_data_type_to_attr_type(cddata_type);
  std::variant<const void *, blender::GVArray> data_src;
  std::variant<void *, blender::bke::GSpanAttributeWriter> data_dst = nullptr;
  int idx_src = num_layers_src;
  int idx_dst, tot_dst = dst_names.size();

  if (!use_layers_src) {
    /* No source at all, we can only delete all destination if requested. */
    if (use_delete) {
      idx_dst = tot_dst;
      while (idx_dst--) {
        dst_attributes.remove(dst_names[idx_dst]);
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
            dst_attributes.add(
                src_names[idx_dst], domain, attr_type, bke::AttributeInitDefaultValue());
          }
        }
        else {
          /* Otherwise, just try to map what we can with existing dst data layers. */
          idx_src = idx_dst;
        }
      }
      else if (use_delete && idx_dst > idx_src) {
        while (idx_dst-- > idx_src) {
          dst_attributes.remove(dst_names[idx_dst]);
        }
      }
      if (r_map) {
        while (idx_src--) {
          if (!use_layers_src[idx_src]) {
            continue;
          }
          data_src = *src_attributes.lookup(src_names[idx_src], domain, attr_type);
          data_dst = dst_attributes.lookup_for_write_span(dst_names[idx_src]);
          data_transfer_layersmapping_add_item_cd(r_map,
                                                  cddata_type,
                                                  mix_mode,
                                                  mix_factor,
                                                  mix_weights,
                                                  std::move(data_src),
                                                  std::move(data_dst),
                                                  nullptr,
                                                  nullptr);
        }
      }
      break;
    case DT_LAYERS_NAME_DST: {
      Vector<std::string> data_dst_to_delete;

      while (idx_src--) {
        const char *name;

        if (!use_layers_src[idx_src]) {
          continue;
        }

        name = src_names[idx_src].c_str();
        data_src = *src_attributes.lookup(src_names[idx_src], domain, attr_type);
        data_dst = dst_attributes.lookup_for_write_span(name);
        if (!std::get<bke::GSpanAttributeWriter>(data_dst)) {
          if (use_create) {
            dst_attributes.add(name, domain, attr_type, bke::AttributeInitDefaultValue());
            data_dst = dst_attributes.lookup_for_write_span(name);
          }
          else {
            /* If we are not allowed to create missing dst data layers,
             * just skip matching src one. */
            continue;
          }
        }
        else if (use_delete) {
          data_dst_to_delete.append(name);
        }
        if (r_map) {
          data_transfer_layersmapping_add_item_cd(r_map,
                                                  cddata_type,
                                                  mix_mode,
                                                  mix_factor,
                                                  mix_weights,
                                                  std::move(data_src),
                                                  std::move(data_dst),
                                                  nullptr,
                                                  nullptr);
        }
      }

      /* NOTE:
       * This won't affect newly created layers, if any, since tot_dst has not been updated!
       * Also, looping backward ensures us we do not suffer
       * from index shifting when deleting a layer. */
      for (const StringRef name : data_dst_to_delete) {
        dst_attributes.remove(name);
      }

      break;
    }
    default:
      return false;
  }

  return true;
}

static bool data_transfer_layersmapping_cdlayers(Vector<CustomDataTransferLayerMap> *r_map,
                                                 const eCustomDataType cddata_type,
                                                 const blender::bke::AttrDomain domain,
                                                 const int mix_mode,
                                                 const float mix_factor,
                                                 const float *mix_weights,
                                                 const bool use_create,
                                                 const bool use_delete,
                                                 const Mesh &mesh_src,
                                                 Mesh &mesh_dst,
                                                 const int fromlayers,
                                                 const int tolayers)
{
  using namespace blender;
  bke::AttributeAccessor src_attributes = mesh_src.attributes();
  bke::MutableAttributeAccessor dst_attributes = mesh_dst.attributes_for_write();
  const bke::AttrType attr_type = *bke::custom_data_type_to_attr_type(cddata_type);

  Vector<std::string> src_names;
  mesh_src.attributes().foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain == domain && iter.data_type == attr_type) {
      src_names.append(iter.name);
    }
  });
  Vector<std::string> dst_names;
  mesh_dst.attributes().foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain == domain && iter.data_type == attr_type) {
      dst_names.append(iter.name);
    }
  });

  std::variant<void *, bke::GSpanAttributeWriter> data_dst = nullptr;

  if (fromlayers == DT_LAYERS_ACTIVE_SRC || fromlayers >= 0) {
    /* NOTE: use_delete has not much meaning in this case, ignored. */

    std::string name_src;
    if (fromlayers >= 0) { /* Real-layer index */
      name_src = src_names[fromlayers];
    }
    else {
      name_src = [&]() -> StringRef {
        switch (cddata_type) {
          case CD_PROP_FLOAT2:
            return mesh_src.active_uv_map_name();
          case CD_PROP_COLOR:
          case CD_PROP_BYTE_COLOR:
            return StringRef(mesh_src.active_color_attribute);
          default:
            BLI_assert_unreachable();
            return "";
        }
      }();
      if (name_src.empty()) {
        return true;
      }
    }
    const int idx_src = src_names.first_index_of(name_src);

    GVArray data_src = *src_attributes.lookup(name_src, domain, attr_type);
    if (!data_src) {
      return true;
    }

    std::string name_dst;
    int idx_dst;
    if (tolayers >= 0) { /* Real-layer index */
      name_dst = dst_names[tolayers];
      data_dst = dst_attributes.lookup_for_write_span(name_dst);
    }
    else if (tolayers == DT_LAYERS_ACTIVE_DST) {
      name_dst = [&]() -> StringRef {
        switch (cddata_type) {
          case CD_PROP_FLOAT2:
            return mesh_src.active_uv_map_name();
          case CD_PROP_COLOR:
          case CD_PROP_BYTE_COLOR:
            return StringRef(mesh_src.active_color_attribute);
          default:
            BLI_assert_unreachable();
            return "";
        }
      }();
      if (name_dst.empty()) {
        if (!use_create) {
          return true;
        }
        name_dst = name_src;
        dst_attributes.add(name_dst, domain, attr_type, bke::AttributeInitDefaultValue());
        data_dst = dst_attributes.lookup_for_write_span(name_dst);
      }
      else {
        data_dst = dst_attributes.lookup_for_write_span(name_dst);
      }
    }
    else if (tolayers == DT_LAYERS_INDEX_DST) {
      int num = dst_names.size();
      idx_dst = idx_src;
      if (num <= idx_dst) {
        if (!use_create) {
          return true;
        }
        /* Create as much data layers as necessary! */
        for (; num <= idx_dst; num++) {
          dst_attributes.add(src_names[num], domain, attr_type, bke::AttributeInitDefaultValue());
        }
      }

      data_dst = dst_attributes.lookup_for_write_span(name_dst);
    }
    else if (tolayers == DT_LAYERS_NAME_DST) {
      const char *name = name_src.c_str();
      if (mesh_dst.attributes().lookup_meta_data(name) !=
          bke::AttributeMetaData{domain, attr_type})
      {
        if (!use_create) {
          return true;
        }

        dst_attributes.add(name, domain, attr_type, bke::AttributeInitDefaultValue());
      }
      data_dst = dst_attributes.lookup_for_write_span(name);
    }
    else {
      return false;
    }

    if (!std::visit([&](const auto &value) { return bool(value); }, data_dst)) {
      return false;
    }

    if (r_map) {
      data_transfer_layersmapping_add_item_cd(r_map,
                                              cddata_type,
                                              mix_mode,
                                              mix_factor,
                                              mix_weights,
                                              data_src,
                                              std::move(data_dst),
                                              nullptr,
                                              nullptr);
    }
  }
  else if (fromlayers == DT_LAYERS_ALL_SRC) {
    int num_src = src_names.size();
    bool *use_layers_src = num_src ? MEM_malloc_arrayN<bool>(size_t(num_src), __func__) : nullptr;
    bool ret;

    if (use_layers_src) {
      memset(use_layers_src, true, sizeof(*use_layers_src) * num_src);
    }

    ret = data_transfer_layersmapping_cdlayers_multisrc_to_dst(r_map,
                                                               cddata_type,
                                                               domain,
                                                               mix_mode,
                                                               mix_factor,
                                                               mix_weights,
                                                               use_create,
                                                               use_delete,
                                                               mesh_src,
                                                               mesh_dst,
                                                               src_names,
                                                               dst_names,
                                                               tolayers,
                                                               use_layers_src,
                                                               num_src);

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

static void data_transfer_layersmapping_add_item_attr(Vector<CustomDataTransferLayerMap> *r_map,
                                                      const eCustomDataType cddata_type,
                                                      const blender::bke::AttrDomain domain,
                                                      const blender::StringRef name,
                                                      const int mix_mode,
                                                      const float mix_factor,
                                                      const float *mix_weights,
                                                      const bool use_create,
                                                      const bool use_delete,
                                                      const Mesh &mesh_src,
                                                      Mesh &mesh_dst)
{
  using namespace blender;
  bke::AttributeAccessor src_attributes = mesh_src.attributes();
  bke::MutableAttributeAccessor dst_attributes = mesh_dst.attributes_for_write();
  const bke::AttrType attr_type = *bke::custom_data_type_to_attr_type(cddata_type);

  const GVArray data_src = *src_attributes.lookup(name, domain, attr_type);
  if (data_src) {
    bke::GSpanAttributeWriter data_dst = dst_attributes.lookup_for_write_span(name);
    if (!data_dst && use_create) {
      data_dst = dst_attributes.lookup_or_add_for_write_span(name, domain, attr_type);
    }
    if (r_map && data_dst && data_dst.domain == domain &&
        bke::cpp_type_to_attribute_type(data_dst.span.type()) == attr_type)
    {
      data_transfer_layersmapping_add_item_cd(r_map,
                                              cddata_type,
                                              mix_mode,
                                              mix_factor,
                                              mix_weights,
                                              std::move(data_src),
                                              std::move(data_dst),
                                              nullptr,
                                              nullptr);
    }
  }
  else {
    if (use_delete) {
      dst_attributes.remove(name);
    }
  }
}

static bool data_transfer_layersmapping_generate(Vector<CustomDataTransferLayerMap> *r_map,
                                                 Object *ob_src,
                                                 Object *ob_dst,
                                                 const Mesh *me_src,
                                                 Mesh *me_dst,
                                                 const int elem_type,
                                                 const int cddata_type,
                                                 int mix_mode,
                                                 float mix_factor,
                                                 const float *mix_weights,
                                                 const bool use_create,
                                                 const bool use_delete,
                                                 const int fromlayers,
                                                 const int tolayers,
                                                 SpaceTransform *space_transform)
{
  using namespace blender;

  if (elem_type == ME_VERT) {
    if (cddata_type == CD_MVERT_SKIN) {
      const void *data_src = CustomData_get_layer(&me_src->vert_data, CD_MVERT_SKIN);
      if (data_src) {
        void *data_dst = CustomData_get_layer_for_write(
            &me_dst->vert_data, CD_MVERT_SKIN, me_dst->verts_num);
        if (!data_dst && use_create) {
          data_dst = CustomData_add_layer(
              &me_dst->vert_data, CD_MVERT_SKIN, CD_SET_DEFAULT, me_dst->verts_num);
        }

        if (r_map && data_dst) {
          data_transfer_layersmapping_add_item_cd(r_map,
                                                  CD_MVERT_SKIN,
                                                  mix_mode,
                                                  mix_factor,
                                                  mix_weights,
                                                  data_src,
                                                  data_dst,
                                                  nullptr,
                                                  nullptr);
        }
      }
      else {
        if (use_delete) {
          CustomData_free_layer(&me_dst->vert_data, CD_MVERT_SKIN, 0);
        }
      }
    }
    else if (cddata_type == CD_PROP_BYTE_COLOR) {
      if (!data_transfer_layersmapping_cdlayers(r_map,
                                                CD_PROP_BYTE_COLOR,
                                                bke::AttrDomain::Point,
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                use_create,
                                                use_delete,
                                                *me_src,
                                                *me_dst,
                                                fromlayers,
                                                tolayers))
      {
        return false;
      }
      return true;
    }
    else if (cddata_type == CD_PROP_COLOR) {
      if (!data_transfer_layersmapping_cdlayers(r_map,
                                                CD_PROP_COLOR,
                                                bke::AttrDomain::Point,
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                use_create,
                                                use_delete,
                                                *me_src,
                                                *me_dst,
                                                fromlayers,
                                                tolayers))
      {
        return false;
      }
      return true;
    }
    if (cddata_type == CD_FAKE_MDEFORMVERT) {
      return data_transfer_layersmapping_vgroups(r_map,
                                                 mix_mode,
                                                 mix_factor,
                                                 mix_weights,
                                                 use_create,
                                                 use_delete,
                                                 ob_src,
                                                 ob_dst,
                                                 *me_src,
                                                 *me_dst,
                                                 me_dst != ob_dst->data,
                                                 fromlayers,
                                                 tolayers);
    }
    if (cddata_type == CD_FAKE_BWEIGHT) {
      data_transfer_layersmapping_add_item_attr(r_map,
                                                CD_PROP_FLOAT,
                                                bke::AttrDomain::Point,
                                                "bevel_weight_vert",
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                use_create,
                                                use_delete,
                                                *me_src,
                                                *me_dst);
      return true;
    }
  }
  else if (elem_type == ME_EDGE) {
    if (cddata_type == CD_FAKE_SEAM) {
      data_transfer_layersmapping_add_item_attr(r_map,
                                                CD_PROP_BOOL,
                                                bke::AttrDomain::Edge,
                                                "uv_seam",
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                use_create,
                                                use_delete,
                                                *me_src,
                                                *me_dst);
      return true;
    }
    if (cddata_type == CD_FAKE_SHARP) {
      data_transfer_layersmapping_add_item_attr(r_map,
                                                CD_PROP_BOOL,
                                                bke::AttrDomain::Edge,
                                                "sharp_edge",
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                use_create,
                                                use_delete,
                                                *me_src,
                                                *me_dst);
      return true;
    }
    if (cddata_type == CD_FAKE_BWEIGHT) {
      data_transfer_layersmapping_add_item_attr(r_map,
                                                CD_PROP_FLOAT,
                                                bke::AttrDomain::Edge,
                                                "bevel_weight_edge",
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                use_create,
                                                use_delete,
                                                *me_src,
                                                *me_dst);
      return true;
    }
    if (cddata_type == CD_FAKE_CREASE) {
      data_transfer_layersmapping_add_item_attr(r_map,
                                                CD_PROP_FLOAT,
                                                bke::AttrDomain::Edge,
                                                "crease_edge",
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                use_create,
                                                use_delete,
                                                *me_src,
                                                *me_dst);
      return true;
    }
    if (r_map && cddata_type == CD_FAKE_FREESTYLE_EDGE) {
      data_transfer_layersmapping_add_item_attr(r_map,
                                                CD_PROP_BOOL,
                                                bke::AttrDomain::Edge,
                                                "freestyle_edge",
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                use_create,
                                                use_delete,
                                                *me_src,
                                                *me_dst);
      return true;
    }

    return false;
  }
  else if (elem_type == ME_LOOP) {
    if (cddata_type == CD_FAKE_UV) {
      if (!data_transfer_layersmapping_cdlayers(r_map,
                                                CD_PROP_FLOAT2,
                                                bke::AttrDomain::Corner,
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                use_create,
                                                use_delete,
                                                *me_src,
                                                *me_dst,
                                                fromlayers,
                                                tolayers))
      {
        return false;
      }
      return true;
    }
    if (cddata_type == CD_FAKE_LNOR) {
      if (r_map) {
        /* Use #CD_NORMAL as a temporary storage for custom normals in 3D vector form.
         * A post-process step will convert this layer to "custom_normal". */
        float3 *dst_data = static_cast<float3 *>(
            CustomData_get_layer_for_write(&me_dst->corner_data, CD_NORMAL, me_dst->corners_num));
        if (!dst_data) {
          dst_data = static_cast<float3 *>(CustomData_add_layer(
              &me_dst->corner_data, CD_NORMAL, CD_SET_DEFAULT, me_dst->corners_num));
        }
        if (mix_factor != 1.0f || mix_weights) {
          MutableSpan(dst_data, me_dst->corners_num).copy_from(me_dst->corner_normals());
        }
        /* Post-process will convert it back to "custom_normal". */
        data_transfer_layersmapping_add_item_cd(r_map,
                                                CD_NORMAL,
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                me_src->corner_normals().data(),
                                                dst_data,
                                                customdata_data_transfer_interp_normal_normals,
                                                space_transform);
      }
      return true;
    }
    if (cddata_type == CD_PROP_BYTE_COLOR) {
      if (!data_transfer_layersmapping_cdlayers(r_map,
                                                CD_PROP_BYTE_COLOR,
                                                bke::AttrDomain::Corner,
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                use_create,
                                                use_delete,
                                                *me_src,
                                                *me_dst,
                                                fromlayers,
                                                tolayers))
      {
        return false;
      }
      return true;
    }
    if (cddata_type == CD_PROP_COLOR) {
      if (!data_transfer_layersmapping_cdlayers(r_map,
                                                CD_PROP_COLOR,
                                                bke::AttrDomain::Corner,
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                use_create,
                                                use_delete,
                                                *me_src,
                                                *me_dst,
                                                fromlayers,
                                                tolayers))
      {
        return false;
      }
      return true;
    }
    return false;
  }
  else if (elem_type == ME_POLY) {
    if (cddata_type == CD_FAKE_SHARP) {
      data_transfer_layersmapping_add_item_attr(r_map,
                                                CD_PROP_BOOL,
                                                bke::AttrDomain::Face,
                                                "sharp_face",
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                use_create,
                                                use_delete,
                                                *me_src,
                                                *me_dst);
      return true;
    }
    if (cddata_type == CD_FAKE_FREESTYLE_FACE) {
      data_transfer_layersmapping_add_item_attr(r_map,
                                                CD_PROP_BOOL,
                                                bke::AttrDomain::Face,
                                                "freestyle_face",
                                                mix_mode,
                                                mix_factor,
                                                mix_weights,
                                                use_create,
                                                use_delete,
                                                *me_src,
                                                *me_dst);
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
  const Object *ob_src_eval = DEG_get_evaluated(depsgraph, ob_src);
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
                                           use_create,
                                           use_delete,
                                           fromlayers,
                                           tolayers,
                                           nullptr);
      /* Make sure we have active/default color layers if none existed before.
       * Use the active/default from src (if it was transferred), otherwise the first. */
      if (ELEM(cddata_type, CD_PROP_COLOR, CD_PROP_BYTE_COLOR)) {
        transfer_active_color_string(me_dst, me_src);
        transfer_default_color_string(me_dst, me_src);
      }
    }
    if (DT_DATATYPE_IS_EDGE(dtdata_type)) {
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
                                           use_create,
                                           use_delete,
                                           fromlayers,
                                           tolayers,
                                           nullptr);
    }
    if (DT_DATATYPE_IS_LOOP(dtdata_type)) {
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
                                           use_create,
                                           use_delete,
                                           fromlayers,
                                           tolayers,
                                           nullptr);
      /* Make sure we have active/default color layers if none existed before.
       * Use the active/default from src (if it was transferred), otherwise the first. */
      if (ELEM(cddata_type, CD_PROP_COLOR, CD_PROP_BYTE_COLOR)) {
        transfer_active_color_string(me_dst, me_src);
        transfer_default_color_string(me_dst, me_src);
      }
    }
    if (DT_DATATYPE_IS_FACE(dtdata_type)) {
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

  const MDeformVert *mdef = nullptr;
  int vg_idx = -1;
  float *weights[DATAMAX] = {nullptr};

  MeshPairRemap geom_map[DATAMAX] = {{0}};
  bool geom_map_init[DATAMAX] = {false};
  Vector<CustomDataTransferLayerMap> lay_map;
  bool changed = false;
  bool is_modifier = false;

  const bool use_delete = false; /* We never delete data layers from destination here. */

  BLI_assert((ob_src != ob_dst) && (ob_src->type == OB_MESH) && (ob_dst->type == OB_MESH));

  if (me_dst) {
    /* Never create needed custom layers on passed destination mesh
     * (assumed to *not* be ob_dst->data, aka modifier case). */
    use_create = false;
    is_modifier = true;
  }
  else {
    me_dst = static_cast<Mesh *>(ob_dst->data);
  }

  if (vgroup_name) {
    mdef = me_dst->deform_verts().data();
    if (mdef) {
      vg_idx = BKE_id_defgroup_name_index(&me_dst->id, vgroup_name);
    }
  }

  /* Get source evaluated mesh. */
  if (is_modifier) {
    me_src = BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob_src);
  }
  else {
    const Object *ob_eval = DEG_get_evaluated(depsgraph, ob_src);
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

    BKE_mesh_remap_find_best_match_from_mesh(me_dst->vert_positions(), me_src, space_transform);
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
      const int num_verts_dst = me_dst->verts_num;

      if (!geom_map_init[VDATA]) {
        const int num_verts_src = me_src->verts_num;

        if ((map_vert_mode == MREMAP_MODE_TOPOLOGY) && (num_verts_dst != num_verts_src)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source and destination meshes do not have the same number of vertices, "
                     "'Topology' mapping cannot be used in this case");
          continue;
        }
        if ((map_vert_mode & MREMAP_USE_EDGE) && (me_src->edges_num == 0)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source mesh does not have any edges, "
                     "none of the 'Edge' mappings can be used in this case");
          continue;
        }
        if ((map_vert_mode & MREMAP_USE_POLY) && (me_src->faces_num == 0)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source mesh does not have any faces, "
                     "none of the 'Face' mappings can be used in this case");
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
                                            positions_dst,
                                            me_src,
                                            me_dst,
                                            &geom_map[VDATA]);
        geom_map_init[VDATA] = true;
      }

      if (mdef && vg_idx != -1 && !weights[VDATA]) {
        weights[VDATA] = MEM_malloc_arrayN<float>(size_t(num_verts_dst), __func__);
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
                                               use_create,
                                               use_delete,
                                               fromlayers,
                                               tolayers,
                                               space_transform))
      {
        changed |= !lay_map.is_empty();

        for (CustomDataTransferLayerMap &lay_mapit : lay_map) {
          CustomData_data_transfer(&geom_map[VDATA], &lay_mapit);
        }

        lay_map.clear();
      }
    }
    if (DT_DATATYPE_IS_EDGE(dtdata_type)) {
      blender::MutableSpan<blender::float3> positions_dst = me_dst->vert_positions_for_write();

      const int num_verts_dst = me_dst->verts_num;
      const blender::Span<blender::int2> edges_dst = me_dst->edges();

      if (!geom_map_init[EDATA]) {
        const int num_edges_src = me_src->edges_num;

        if ((map_edge_mode == MREMAP_MODE_TOPOLOGY) && (edges_dst.size() != num_edges_src)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source and destination meshes do not have the same number of edges, "
                     "'Topology' mapping cannot be used in this case");
          continue;
        }
        if ((map_edge_mode & MREMAP_USE_POLY) && (me_src->faces_num == 0)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source mesh does not have any faces, "
                     "none of the 'Face' mappings can be used in this case");
          continue;
        }
        if (ELEM(0, edges_dst.size(), num_edges_src)) {
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
                                            positions_dst,
                                            edges_dst,
                                            me_src,
                                            me_dst,
                                            &geom_map[EDATA]);
        geom_map_init[EDATA] = true;
      }

      if (mdef && vg_idx != -1 && !weights[EDATA]) {
        weights[EDATA] = MEM_malloc_arrayN<float>(size_t(edges_dst.size()), __func__);
        BKE_defvert_extract_vgroup_to_edgeweights(
            mdef, vg_idx, num_verts_dst, edges_dst, invert_vgroup, weights[EDATA]);
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
                                               use_create,
                                               use_delete,
                                               fromlayers,
                                               tolayers,
                                               space_transform))
      {
        changed |= !lay_map.is_empty();

        for (CustomDataTransferLayerMap &lay_mapit : lay_map) {
          CustomData_data_transfer(&geom_map[EDATA], &lay_mapit);
        }

        lay_map.clear();
      }
    }
    if (DT_DATATYPE_IS_LOOP(dtdata_type)) {
      const blender::Span<blender::float3> positions_dst = me_dst->vert_positions();
      const int num_verts_dst = me_dst->verts_num;
      const blender::OffsetIndices faces_dst = me_dst->faces();
      const blender::Span<int> corner_verts_dst = me_dst->corner_verts();

      MeshRemapIslandsCalc island_callback = data_transfer_get_loop_islands_generator(cddata_type);

      if (!geom_map_init[LDATA]) {
        const int num_loops_src = me_src->corners_num;

        if ((map_loop_mode == MREMAP_MODE_TOPOLOGY) && (corner_verts_dst.size() != num_loops_src))
        {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source and destination meshes do not have the same number of face corners, "
                     "'Topology' mapping cannot be used in this case");
          continue;
        }
        if ((map_loop_mode & MREMAP_USE_EDGE) && (me_src->edges_num == 0)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source mesh does not have any edges, "
                     "none of the 'Edge' mappings can be used in this case");
          continue;
        }
        if (ELEM(0, corner_verts_dst.size(), num_loops_src)) {
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
                                            me_dst,
                                            positions_dst,
                                            corner_verts_dst,
                                            faces_dst,
                                            me_src,
                                            island_callback,
                                            islands_handling_precision,
                                            &geom_map[LDATA]);
        geom_map_init[LDATA] = true;
      }

      if (mdef && vg_idx != -1 && !weights[LDATA]) {
        weights[LDATA] = MEM_malloc_arrayN<float>(size_t(corner_verts_dst.size()), __func__);
        BKE_defvert_extract_vgroup_to_loopweights(
            mdef, vg_idx, num_verts_dst, corner_verts_dst, invert_vgroup, weights[LDATA]);
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
                                               use_create,
                                               use_delete,
                                               fromlayers,
                                               tolayers,
                                               space_transform))
      {
        changed |= !lay_map.is_empty();

        for (CustomDataTransferLayerMap &lay_mapit : lay_map) {
          CustomData_data_transfer(&geom_map[LDATA], &lay_mapit);
        }

        lay_map.clear();
      }
    }
    if (DT_DATATYPE_IS_FACE(dtdata_type)) {
      const blender::Span<blender::float3> positions_dst = me_dst->vert_positions();
      const int num_verts_dst = me_dst->verts_num;
      const blender::OffsetIndices faces_dst = me_dst->faces();
      const blender::Span<int> corner_verts_dst = me_dst->corner_verts();

      if (!geom_map_init[PDATA]) {
        const int num_faces_src = me_src->faces_num;

        if ((map_face_mode == MREMAP_MODE_TOPOLOGY) && (faces_dst.size() != num_faces_src)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source and destination meshes do not have the same number of faces, "
                     "'Topology' mapping cannot be used in this case");
          continue;
        }
        if ((map_face_mode & MREMAP_USE_EDGE) && (me_src->edges_num == 0)) {
          BKE_report(reports,
                     RPT_ERROR,
                     "Source mesh does not have any edges, "
                     "none of the 'Edge' mappings can be used in this case");
          continue;
        }
        if (ELEM(0, faces_dst.size(), num_faces_src)) {
          BKE_report(
              reports,
              RPT_ERROR,
              "Source or destination meshes do not have any faces, cannot transfer face data");
          continue;
        }

        BKE_mesh_remap_calc_faces_from_mesh(map_face_mode,
                                            space_transform,
                                            max_distance,
                                            ray_radius,
                                            me_dst,
                                            positions_dst,
                                            corner_verts_dst,
                                            faces_dst,
                                            me_src,
                                            &geom_map[PDATA]);
        geom_map_init[PDATA] = true;
      }

      if (mdef && vg_idx != -1 && !weights[PDATA]) {
        weights[PDATA] = MEM_malloc_arrayN<float>(size_t(faces_dst.size()), __func__);
        BKE_defvert_extract_vgroup_to_faceweights(mdef,
                                                  vg_idx,
                                                  num_verts_dst,
                                                  corner_verts_dst,
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
                                               use_create,
                                               use_delete,
                                               fromlayers,
                                               tolayers,
                                               space_transform))
      {
        changed |= !lay_map.is_empty();

        for (CustomDataTransferLayerMap &lay_mapit : lay_map) {
          CustomData_data_transfer(&geom_map[PDATA], &lay_mapit);
        }

        lay_map.clear();
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
