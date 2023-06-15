/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 * Implementation of generic geometry attributes management. This is built
 * on top of CustomData, which manages individual domains.
 */

#include <cstring>
#include <optional>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_curves_types.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_index_range.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"

#include "BLT_translation.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.h"
#include "BKE_report.h"

#include "RNA_access.h"

using blender::IndexRange;

struct DomainInfo {
  CustomData *customdata;
  int length;
};

static void get_domains(const ID *id, DomainInfo info[ATTR_DOMAIN_NUM])
{
  memset(info, 0, sizeof(DomainInfo) * ATTR_DOMAIN_NUM);

  switch (GS(id->name)) {
    case ID_PT: {
      PointCloud *pointcloud = (PointCloud *)id;
      info[ATTR_DOMAIN_POINT].customdata = &pointcloud->pdata;
      info[ATTR_DOMAIN_POINT].length = pointcloud->totpoint;
      break;
    }
    case ID_ME: {
      Mesh *mesh = (Mesh *)id;
      BMEditMesh *em = mesh->edit_mesh;
      if (em != nullptr) {
        BMesh *bm = em->bm;
        info[ATTR_DOMAIN_POINT].customdata = &bm->vdata;
        info[ATTR_DOMAIN_POINT].length = bm->totvert;
        info[ATTR_DOMAIN_EDGE].customdata = &bm->edata;
        info[ATTR_DOMAIN_EDGE].length = bm->totedge;
        info[ATTR_DOMAIN_CORNER].customdata = &bm->ldata;
        info[ATTR_DOMAIN_CORNER].length = bm->totloop;
        info[ATTR_DOMAIN_FACE].customdata = &bm->pdata;
        info[ATTR_DOMAIN_FACE].length = bm->totface;
      }
      else {
        info[ATTR_DOMAIN_POINT].customdata = &mesh->vdata;
        info[ATTR_DOMAIN_POINT].length = mesh->totvert;
        info[ATTR_DOMAIN_EDGE].customdata = &mesh->edata;
        info[ATTR_DOMAIN_EDGE].length = mesh->totedge;
        info[ATTR_DOMAIN_CORNER].customdata = &mesh->ldata;
        info[ATTR_DOMAIN_CORNER].length = mesh->totloop;
        info[ATTR_DOMAIN_FACE].customdata = &mesh->pdata;
        info[ATTR_DOMAIN_FACE].length = mesh->totpoly;
      }
      break;
    }
    case ID_CV: {
      Curves *curves = (Curves *)id;
      info[ATTR_DOMAIN_POINT].customdata = &curves->geometry.point_data;
      info[ATTR_DOMAIN_POINT].length = curves->geometry.point_num;
      info[ATTR_DOMAIN_CURVE].customdata = &curves->geometry.curve_data;
      info[ATTR_DOMAIN_CURVE].length = curves->geometry.curve_num;
      break;
    }
    default:
      break;
  }
}

namespace blender::bke {

static std::optional<blender::bke::MutableAttributeAccessor> get_attribute_accessor_for_write(
    ID &id)
{
  switch (GS(id.name)) {
    case ID_ME: {
      Mesh &mesh = reinterpret_cast<Mesh &>(id);
      /* The attribute API isn't implemented for BMesh, so edit mode meshes are not supported. */
      BLI_assert(mesh.edit_mesh == nullptr);
      return mesh.attributes_for_write();
    }
    case ID_PT: {
      PointCloud &pointcloud = reinterpret_cast<PointCloud &>(id);
      return pointcloud.attributes_for_write();
    }
    case ID_CV: {
      Curves &curves_id = reinterpret_cast<Curves &>(id);
      CurvesGeometry &curves = curves_id.geometry.wrap();
      return curves.attributes_for_write();
    }
    default: {
      BLI_assert_unreachable();
      return {};
    }
  }
}

}  // namespace blender::bke

bool BKE_id_attributes_supported(const ID *id)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);
  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    if (info[domain].customdata) {
      return true;
    }
  }
  return false;
}

bool BKE_attribute_allow_procedural_access(const char *attribute_name)
{
  return blender::bke::allow_procedural_attribute_access(attribute_name);
}

static bool bke_id_attribute_rename_if_exists(ID *id,
                                              const char *old_name,
                                              const char *new_name,
                                              ReportList *reports)
{
  CustomDataLayer *layer = BKE_id_attribute_search(
      id, old_name, CD_MASK_PROP_ALL, ATTR_DOMAIN_MASK_ALL);
  if (layer == nullptr) {
    return false;
  }
  return BKE_id_attribute_rename(id, old_name, new_name, reports);
}

bool BKE_id_attribute_rename(ID *id,
                             const char *old_name,
                             const char *new_name,
                             ReportList *reports)
{
  using namespace blender;
  if (BKE_id_attribute_required(id, old_name)) {
    BLI_assert_msg(0, "Required attribute name is not editable");
    return false;
  }
  if (STREQ(new_name, "")) {
    BKE_report(reports, RPT_ERROR, "Attribute name can not be empty");
    return false;
  }

  /* NOTE: Checking if the new name matches the old name only makes sense when the name
   * is clamped to it's maximum length, otherwise assigning an over-long name multiple times
   * will add `.001` suffix unnecessarily. */
  {
    const int new_name_maxncpy = CustomData_name_maxncpy_calc(new_name);
    /* NOTE: A function that performs a clamped comparison without copying would be handy here. */
    char new_name_clamped[MAX_CUSTOMDATA_LAYER_NAME];
    BLI_strncpy_utf8(new_name_clamped, new_name, new_name_maxncpy);
    if (STREQ(old_name, new_name_clamped)) {
      return false;
    }
  }

  CustomDataLayer *layer = BKE_id_attribute_search(
      id, old_name, CD_MASK_PROP_ALL, ATTR_DOMAIN_MASK_ALL);
  if (layer == nullptr) {
    BKE_report(reports, RPT_ERROR, "Attribute is not part of this geometry");
    return false;
  }

  char result_name[MAX_CUSTOMDATA_LAYER_NAME];
  BKE_id_attribute_calc_unique_name(id, new_name, result_name);

  if (layer->type == CD_PROP_FLOAT2 && GS(id->name) == ID_ME) {
    /* Rename UV sub-attributes. */
    char buffer_src[MAX_CUSTOMDATA_LAYER_NAME];
    char buffer_dst[MAX_CUSTOMDATA_LAYER_NAME];

    bke_id_attribute_rename_if_exists(id,
                                      BKE_uv_map_vert_select_name_get(layer->name, buffer_src),
                                      BKE_uv_map_vert_select_name_get(result_name, buffer_dst),
                                      reports);
    bke_id_attribute_rename_if_exists(id,
                                      BKE_uv_map_edge_select_name_get(layer->name, buffer_src),
                                      BKE_uv_map_edge_select_name_get(result_name, buffer_dst),
                                      reports);
    bke_id_attribute_rename_if_exists(id,
                                      BKE_uv_map_pin_name_get(layer->name, buffer_src),
                                      BKE_uv_map_pin_name_get(result_name, buffer_dst),
                                      reports);
  }
  if (StringRef(old_name) == BKE_id_attributes_active_color_name(id)) {
    BKE_id_attributes_active_color_set(id, result_name);
  }
  if (StringRef(old_name) == BKE_id_attributes_default_color_name(id)) {
    BKE_id_attributes_default_color_set(id, result_name);
  }

  STRNCPY_UTF8(layer->name, result_name);

  return true;
}

struct AttrUniqueData {
  ID *id;
};

static bool unique_name_cb(void *arg, const char *name)
{
  AttrUniqueData *data = (AttrUniqueData *)arg;

  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(data->id, info);

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    if (!info[domain].customdata) {
      continue;
    }

    const CustomData *cdata = info[domain].customdata;
    for (int i = 0; i < cdata->totlayer; i++) {
      const CustomDataLayer *layer = cdata->layers + i;

      if (STREQ(layer->name, name)) {
        return true;
      }
    }
  }

  return false;
}

bool BKE_id_attribute_calc_unique_name(ID *id, const char *name, char *outname)
{
  AttrUniqueData data{id};
  const int name_maxncpy = CustomData_name_maxncpy_calc(name);

  /* Set default name if none specified.
   * NOTE: We only call IFACE_() if needed to avoid locale lookup overhead. */
  BLI_strncpy_utf8(outname, (name && name[0]) ? name : IFACE_("Attribute"), name_maxncpy);

  const char *defname = ""; /* Dummy argument, never used as `name` is never zero length. */
  return BLI_uniquename_cb(unique_name_cb, &data, defname, '.', outname, name_maxncpy);
}

CustomDataLayer *BKE_id_attribute_new(ID *id,
                                      const char *name,
                                      const eCustomDataType type,
                                      const eAttrDomain domain,
                                      ReportList *reports)
{
  using namespace blender::bke;
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  CustomData *customdata = info[domain].customdata;
  if (customdata == nullptr) {
    BKE_report(reports, RPT_ERROR, "Attribute domain not supported by this geometry type");
    return nullptr;
  }

  char uniquename[MAX_CUSTOMDATA_LAYER_NAME];
  BKE_id_attribute_calc_unique_name(id, name, uniquename);

  if (GS(id->name) == ID_ME) {
    Mesh *mesh = reinterpret_cast<Mesh *>(id);
    if (BMEditMesh *em = mesh->edit_mesh) {
      BM_data_layer_add_named(em->bm, customdata, type, uniquename);
      const int index = CustomData_get_named_layer_index(customdata, type, uniquename);
      return (index == -1) ? nullptr : &(customdata->layers[index]);
    }
  }

  std::optional<MutableAttributeAccessor> attributes = get_attribute_accessor_for_write(*id);
  if (!attributes) {
    return nullptr;
  }

  attributes->add(uniquename, domain, eCustomDataType(type), AttributeInitDefaultValue());

  const int index = CustomData_get_named_layer_index(customdata, type, uniquename);
  return (index == -1) ? nullptr : &(customdata->layers[index]);
}

static void bke_id_attribute_copy_if_exists(ID *id, const char *srcname, const char *dstname)
{
  using namespace blender::bke;

  std::optional<MutableAttributeAccessor> attributes = get_attribute_accessor_for_write(*id);
  if (!attributes) {
    return;
  }

  GAttributeReader src = attributes->lookup(srcname);
  if (!src) {
    return;
  }

  const eCustomDataType type = cpp_type_to_custom_data_type(src.varray.type());
  attributes->add(dstname, src.domain, type, AttributeInitVArray(src.varray));
}

CustomDataLayer *BKE_id_attribute_duplicate(ID *id, const char *name, ReportList *reports)
{
  using namespace blender::bke;
  char uniquename[MAX_CUSTOMDATA_LAYER_NAME];
  BKE_id_attribute_calc_unique_name(id, name, uniquename);

  if (GS(id->name) == ID_ME) {
    Mesh *mesh = reinterpret_cast<Mesh *>(id);
    if (BMEditMesh *em = mesh->edit_mesh) {
      BLI_assert_unreachable();
      UNUSED_VARS(em);
      return nullptr;
    }
  }

  std::optional<MutableAttributeAccessor> attributes = get_attribute_accessor_for_write(*id);
  if (!attributes) {
    return nullptr;
  }

  GAttributeReader src = attributes->lookup(name);
  if (!src) {
    BKE_report(reports, RPT_ERROR, "Attribute is not part of this geometry");
    return nullptr;
  }

  const eCustomDataType type = cpp_type_to_custom_data_type(src.varray.type());
  attributes->add(uniquename, src.domain, type, AttributeInitVArray(src.varray));

  if (GS(id->name) == ID_ME && type == CD_PROP_FLOAT2) {
    /* Duplicate UV sub-attributes. */
    char buffer_src[MAX_CUSTOMDATA_LAYER_NAME];
    char buffer_dst[MAX_CUSTOMDATA_LAYER_NAME];

    bke_id_attribute_copy_if_exists(id,
                                    BKE_uv_map_vert_select_name_get(name, buffer_src),
                                    BKE_uv_map_vert_select_name_get(uniquename, buffer_dst));
    bke_id_attribute_copy_if_exists(id,
                                    BKE_uv_map_edge_select_name_get(name, buffer_src),
                                    BKE_uv_map_edge_select_name_get(uniquename, buffer_dst));
    bke_id_attribute_copy_if_exists(id,
                                    BKE_uv_map_pin_name_get(name, buffer_src),
                                    BKE_uv_map_pin_name_get(uniquename, buffer_dst));
  }

  return BKE_id_attribute_search(id, uniquename, CD_MASK_PROP_ALL, ATTR_DOMAIN_MASK_ALL);
}

static int color_name_to_index(ID *id, const char *name)
{
  const CustomDataLayer *layer = BKE_id_attribute_search(
      id, name, CD_MASK_COLOR_ALL, ATTR_DOMAIN_MASK_COLOR);
  return BKE_id_attribute_to_index(id, layer, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);
}

static int color_clamp_index(ID *id, int index)
{
  const int length = BKE_id_attributes_length(id, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL, true);
  return min_ii(index, length - 1);
}

static const char *color_name_from_index(ID *id, int index)
{
  const CustomDataLayer *layer = BKE_id_attribute_from_index(
      id, index, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);
  return layer ? layer->name : nullptr;
}

bool BKE_id_attribute_remove(ID *id, const char *name, ReportList *reports)
{
  using namespace blender;
  using namespace blender::bke;
  if (!name || name[0] == '\0') {
    BKE_report(reports, RPT_ERROR, "The attribute name must not be empty");
    return false;
  }
  if (BKE_id_attribute_required(id, name)) {
    BKE_report(reports, RPT_ERROR, "Attribute is required and can't be removed");
    return false;
  }

  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  if (GS(id->name) == ID_ME) {
    Mesh *mesh = reinterpret_cast<Mesh *>(id);
    if (BMEditMesh *em = mesh->edit_mesh) {
      for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
        if (CustomData *data = info[domain].customdata) {
          const std::string name_copy = name;
          const int layer_index = CustomData_get_named_layer_index_notype(data, name_copy.c_str());
          if (layer_index == -1) {
            continue;
          }

          const eCustomDataType type = eCustomDataType(data->layers[layer_index].type);
          const bool is_active_color_attribute = name_copy.c_str() ==
                                                 StringRef(mesh->active_color_attribute);
          const bool is_default_color_attribute = name_copy.c_str() ==
                                                  StringRef(mesh->default_color_attribute);
          const int active_color_index = color_name_to_index(id, mesh->active_color_attribute);
          const int default_color_index = color_name_to_index(id, mesh->default_color_attribute);

          if (!BM_data_layer_free_named(em->bm, data, name_copy.c_str())) {
            BLI_assert_unreachable();
          }

          if (is_active_color_attribute) {
            BKE_id_attributes_active_color_set(
                id, color_name_from_index(id, color_clamp_index(id, active_color_index)));
          }
          if (is_default_color_attribute) {
            BKE_id_attributes_default_color_set(
                id, color_name_from_index(id, color_clamp_index(id, default_color_index)));
          }

          if (type == CD_PROP_FLOAT2 && domain == ATTR_DOMAIN_CORNER) {
            char buffer[MAX_CUSTOMDATA_LAYER_NAME];
            BM_data_layer_free_named(
                em->bm, data, BKE_uv_map_vert_select_name_get(name_copy.c_str(), buffer));
            BM_data_layer_free_named(
                em->bm, data, BKE_uv_map_edge_select_name_get(name_copy.c_str(), buffer));
            BM_data_layer_free_named(
                em->bm, data, BKE_uv_map_pin_name_get(name_copy.c_str(), buffer));
          }
          return true;
        }
      }
      return false;
    }
  }

  std::optional<MutableAttributeAccessor> attributes = get_attribute_accessor_for_write(*id);
  if (!attributes) {
    return false;
  }

  if (GS(id->name) == ID_ME) {
    const std::string name_copy = name;
    std::optional<blender::bke::AttributeMetaData> metadata = attributes->lookup_meta_data(
        name_copy);
    if (!metadata) {
      return false;
    }
    /* Update active and default color attributes. */
    Mesh *mesh = reinterpret_cast<Mesh *>(id);
    const bool is_active_color_attribute = name_copy == StringRef(mesh->active_color_attribute);
    const bool is_default_color_attribute = name_copy == StringRef(mesh->default_color_attribute);
    const int active_color_index = color_name_to_index(id, mesh->active_color_attribute);
    const int default_color_index = color_name_to_index(id, mesh->default_color_attribute);

    if (!attributes->remove(name_copy)) {
      BLI_assert_unreachable();
    }

    if (is_active_color_attribute) {
      BKE_id_attributes_active_color_set(
          id, color_name_from_index(id, color_clamp_index(id, active_color_index)));
    }
    if (is_default_color_attribute) {
      BKE_id_attributes_default_color_set(
          id, color_name_from_index(id, color_clamp_index(id, default_color_index)));
    }

    if (metadata->data_type == CD_PROP_FLOAT2 && metadata->domain == ATTR_DOMAIN_CORNER) {
      char buffer[MAX_CUSTOMDATA_LAYER_NAME];
      attributes->remove(BKE_uv_map_vert_select_name_get(name_copy.c_str(), buffer));
      attributes->remove(BKE_uv_map_edge_select_name_get(name_copy.c_str(), buffer));
      attributes->remove(BKE_uv_map_pin_name_get(name_copy.c_str(), buffer));
    }
    return true;
  }

  return attributes->remove(name);
}

CustomDataLayer *BKE_id_attribute_find(const ID *id,
                                       const char *name,
                                       const eCustomDataType type,
                                       const eAttrDomain domain)
{
  if (!name) {
    return nullptr;
  }
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  CustomData *customdata = info[domain].customdata;
  if (customdata == nullptr) {
    return nullptr;
  }

  for (int i = 0; i < customdata->totlayer; i++) {
    CustomDataLayer *layer = &customdata->layers[i];
    if (layer->type == type && STREQ(layer->name, name)) {
      return layer;
    }
  }

  return nullptr;
}

CustomDataLayer *BKE_id_attribute_search(ID *id,
                                         const char *name,
                                         const eCustomDataMask type_mask,
                                         const eAttrDomainMask domain_mask)
{
  if (!name) {
    return nullptr;
  }
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  for (eAttrDomain domain = ATTR_DOMAIN_POINT; domain < ATTR_DOMAIN_NUM;
       domain = static_cast<eAttrDomain>(int(domain) + 1))
  {
    if (!(domain_mask & ATTR_DOMAIN_AS_MASK(domain))) {
      continue;
    }

    CustomData *customdata = info[domain].customdata;
    if (customdata == nullptr) {
      continue;
    }

    for (int i = 0; i < customdata->totlayer; i++) {
      CustomDataLayer *layer = &customdata->layers[i];
      if ((CD_TYPE_AS_MASK(layer->type) & type_mask) && STREQ(layer->name, name)) {
        return layer;
      }
    }
  }

  return nullptr;
}

int BKE_id_attributes_length(const ID *id,
                             eAttrDomainMask domain_mask,
                             eCustomDataMask mask,
                             bool skip_temporary)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  int length = 0;

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    const CustomData *customdata = info[domain].customdata;
    if (customdata == nullptr) {
      continue;
    }

    if ((1 << (int)domain) & domain_mask) {
      length += CustomData_number_of_layers_typemask(customdata, mask, skip_temporary);
    }
  }

  return length;
}

eAttrDomain BKE_id_attribute_domain(const ID *id, const CustomDataLayer *layer)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    const CustomData *customdata = info[domain].customdata;
    if (customdata == nullptr) {
      continue;
    }
    if (ARRAY_HAS_ITEM((CustomDataLayer *)layer, customdata->layers, customdata->totlayer)) {
      return static_cast<eAttrDomain>(domain);
    }
  }

  BLI_assert_msg(0, "Custom data layer not found in geometry");
  return static_cast<eAttrDomain>(ATTR_DOMAIN_POINT);
}

int BKE_id_attribute_data_length(ID *id, CustomDataLayer *layer)
{
  /* When in mesh editmode, attributes point to bmesh customdata layers, the attribute data is
   * empty since custom data is stored per element instead of a single array there (same es UVs
   * etc.), see D11998. */
  switch (GS(id->name)) {
    case ID_ME: {
      Mesh *mesh = (Mesh *)id;
      if (mesh->edit_mesh != nullptr) {
        return 0;
      }
      break;
    }
    default:
      break;
  }

  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    const CustomData *customdata = info[domain].customdata;
    if (customdata == nullptr) {
      continue;
    }
    if (ARRAY_HAS_ITEM((CustomDataLayer *)layer, customdata->layers, customdata->totlayer)) {
      return info[domain].length;
    }
  }

  BLI_assert_msg(0, "Custom data layer not found in geometry");
  return 0;
}

bool BKE_id_attribute_required(const ID *id, const char *name)
{
  switch (GS(id->name)) {
    case ID_PT:
      return BKE_pointcloud_attribute_required((const PointCloud *)id, name);
    case ID_CV:
      return BKE_curves_attribute_required((const Curves *)id, name);
    case ID_ME:
      return BKE_mesh_attribute_required(name);
    default:
      return false;
  }
}

CustomDataLayer *BKE_id_attributes_active_get(ID *id)
{
  int active_index = *BKE_id_attributes_active_index_p(id);
  if (active_index > BKE_id_attributes_length(id, ATTR_DOMAIN_MASK_ALL, CD_MASK_PROP_ALL, false)) {
    active_index = 0;
  }

  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  int index = 0;

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    CustomData *customdata = info[domain].customdata;
    if (customdata == nullptr) {
      continue;
    }
    for (int i = 0; i < customdata->totlayer; i++) {
      CustomDataLayer *layer = &customdata->layers[i];
      if (CD_MASK_PROP_ALL & CD_TYPE_AS_MASK(layer->type)) {
        if (index == active_index) {
          if (BKE_attribute_allow_procedural_access(layer->name)) {
            return layer;
          }
          return nullptr;
        }
        index++;
      }
    }
  }

  return nullptr;
}

void BKE_id_attributes_active_set(ID *id, const char *name)
{
  const CustomDataLayer *layer = BKE_id_attribute_search(
      id, name, CD_MASK_PROP_ALL, ATTR_DOMAIN_MASK_ALL);
  BLI_assert(layer != nullptr);

  const int index = BKE_id_attribute_to_index(id, layer, ATTR_DOMAIN_MASK_ALL, CD_MASK_PROP_ALL);
  *BKE_id_attributes_active_index_p(id) = index;
}

int *BKE_id_attributes_active_index_p(ID *id)
{
  switch (GS(id->name)) {
    case ID_PT: {
      return &((PointCloud *)id)->attributes_active_index;
    }
    case ID_ME: {
      return &((Mesh *)id)->attributes_active_index;
    }
    case ID_CV: {
      return &((Curves *)id)->attributes_active_index;
    }
    default:
      return nullptr;
  }
}

CustomData *BKE_id_attributes_iterator_next_domain(ID *id, CustomDataLayer *layers)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  bool use_next = (layers == nullptr);

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    CustomData *customdata = info[domain].customdata;
    if (customdata == nullptr) {
      continue;
    }
    if (customdata->layers && customdata->totlayer) {
      if (customdata->layers == layers) {
        use_next = true;
      }
      else if (use_next) {
        return customdata;
      }
    }
  }

  return nullptr;
}

CustomDataLayer *BKE_id_attribute_from_index(ID *id,
                                             int lookup_index,
                                             eAttrDomainMask domain_mask,
                                             eCustomDataMask layer_mask)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  int index = 0;
  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    CustomData *customdata = info[domain].customdata;

    if (!customdata || !((1 << int(domain)) & domain_mask)) {
      continue;
    }

    for (int i = 0; i < customdata->totlayer; i++) {
      if (!(layer_mask & CD_TYPE_AS_MASK(customdata->layers[i].type)) ||
          (customdata->layers[i].flag & CD_FLAG_TEMPORARY))
      {
        continue;
      }

      if (index == lookup_index) {
        return customdata->layers + i;
      }

      index++;
    }
  }

  return nullptr;
}

/**
 * Get list of domain types but with ATTR_DOMAIN_FACE and
 * ATTR_DOMAIN_CORNER swapped.
 */
static void get_domains_types(eAttrDomain domains[ATTR_DOMAIN_NUM])
{
  for (const int i : IndexRange(ATTR_DOMAIN_NUM)) {
    domains[i] = static_cast<eAttrDomain>(i);
  }

  /* Swap corner and face. */
  std::swap(domains[ATTR_DOMAIN_FACE], domains[ATTR_DOMAIN_CORNER]);
}

int BKE_id_attribute_to_index(const ID *id,
                              const CustomDataLayer *layer,
                              eAttrDomainMask domain_mask,
                              eCustomDataMask layer_mask)
{
  if (!layer) {
    return -1;
  }

  DomainInfo info[ATTR_DOMAIN_NUM];
  eAttrDomain domains[ATTR_DOMAIN_NUM];
  get_domains_types(domains);
  get_domains(id, info);

  int index = 0;
  for (int i = 0; i < ATTR_DOMAIN_NUM; i++) {
    if (!(domain_mask & (1 << domains[i])) || !info[domains[i]].customdata) {
      continue;
    }

    const CustomData *cdata = info[domains[i]].customdata;
    for (int j = 0; j < cdata->totlayer; j++) {
      const CustomDataLayer *layer_iter = cdata->layers + j;

      if (!(CD_TYPE_AS_MASK(layer_iter->type) & layer_mask) ||
          (layer_iter->flag & CD_FLAG_TEMPORARY)) {
        continue;
      }

      if (layer == layer_iter) {
        return index;
      }

      index++;
    }
  }

  return -1;
}

const char *BKE_id_attributes_active_color_name(const ID *id)
{
  if (GS(id->name) == ID_ME) {
    return reinterpret_cast<const Mesh *>(id)->active_color_attribute;
  }
  return nullptr;
}

const char *BKE_id_attributes_default_color_name(const ID *id)
{
  if (GS(id->name) == ID_ME) {
    return reinterpret_cast<const Mesh *>(id)->default_color_attribute;
  }
  return nullptr;
}

void BKE_id_attributes_active_color_set(ID *id, const char *name)
{
  switch (GS(id->name)) {
    case ID_ME: {
      Mesh *mesh = reinterpret_cast<Mesh *>(id);
      MEM_SAFE_FREE(mesh->active_color_attribute);
      if (name) {
        mesh->active_color_attribute = BLI_strdup(name);
      }
      break;
    }
    default:
      break;
  }
}

void BKE_id_attributes_default_color_set(ID *id, const char *name)
{
  switch (GS(id->name)) {
    case ID_ME: {
      Mesh *mesh = reinterpret_cast<Mesh *>(id);
      MEM_SAFE_FREE(mesh->default_color_attribute);
      if (name) {
        mesh->default_color_attribute = BLI_strdup(name);
      }
      break;
    }
    default:
      break;
  }
}

CustomDataLayer *BKE_id_attributes_color_find(const ID *id, const char *name)
{
  if (CustomDataLayer *layer = BKE_id_attribute_find(id, name, CD_PROP_COLOR, ATTR_DOMAIN_POINT)) {
    return layer;
  }
  if (CustomDataLayer *layer = BKE_id_attribute_find(id, name, CD_PROP_COLOR, ATTR_DOMAIN_CORNER))
  {
    return layer;
  }
  if (CustomDataLayer *layer = BKE_id_attribute_find(
          id, name, CD_PROP_BYTE_COLOR, ATTR_DOMAIN_POINT))
  {
    return layer;
  }
  if (CustomDataLayer *layer = BKE_id_attribute_find(
          id, name, CD_PROP_BYTE_COLOR, ATTR_DOMAIN_CORNER))
  {
    return layer;
  }
  return nullptr;
}

const char *BKE_uv_map_vert_select_name_get(const char *uv_map_name, char *buffer)
{
  BLI_assert(strlen(UV_VERTSEL_NAME) == 2);
  BLI_assert(strlen(uv_map_name) < MAX_CUSTOMDATA_LAYER_NAME - 4);
  BLI_snprintf(buffer, MAX_CUSTOMDATA_LAYER_NAME, ".%s.%s", UV_VERTSEL_NAME, uv_map_name);
  return buffer;
}

const char *BKE_uv_map_edge_select_name_get(const char *uv_map_name, char *buffer)
{
  BLI_assert(strlen(UV_EDGESEL_NAME) == 2);
  BLI_assert(strlen(uv_map_name) < MAX_CUSTOMDATA_LAYER_NAME - 4);
  BLI_snprintf(buffer, MAX_CUSTOMDATA_LAYER_NAME, ".%s.%s", UV_EDGESEL_NAME, uv_map_name);
  return buffer;
}

const char *BKE_uv_map_pin_name_get(const char *uv_map_name, char *buffer)
{
  BLI_assert(strlen(UV_PINNED_NAME) == 2);
  BLI_assert(strlen(uv_map_name) < MAX_CUSTOMDATA_LAYER_NAME - 4);
  BLI_snprintf(buffer, MAX_CUSTOMDATA_LAYER_NAME, ".%s.%s", UV_PINNED_NAME, uv_map_name);
  return buffer;
}

void BKE_id_attribute_copy_domains_temp(short id_type,
                                        const CustomData *vdata,
                                        const CustomData *edata,
                                        const CustomData *ldata,
                                        const CustomData *pdata,
                                        const CustomData *cdata,
                                        ID *r_id)
{
  CustomData reset;

  CustomData_reset(&reset);

  switch (id_type) {
    case ID_ME: {
      Mesh *me = (Mesh *)r_id;
      memset((void *)me, 0, sizeof(*me));

      me->edit_mesh = NULL;

      me->vdata = vdata ? *vdata : reset;
      me->edata = edata ? *edata : reset;
      me->ldata = ldata ? *ldata : reset;
      me->pdata = pdata ? *pdata : reset;

      break;
    }
    case ID_PT: {
      PointCloud *pointcloud = (PointCloud *)r_id;

      memset((void *)pointcloud, 0, sizeof(*pointcloud));

      pointcloud->pdata = vdata ? *vdata : reset;
      break;
    }
    case ID_CV: {
      Curves *curves = (Curves *)r_id;

      memset((void *)curves, 0, sizeof(*curves));

      curves->geometry.point_data = vdata ? *vdata : reset;
      curves->geometry.curve_data = cdata ? *cdata : reset;
      break;
    }
    default:
      break;
  }

  *((short *)r_id->name) = id_type;
}
