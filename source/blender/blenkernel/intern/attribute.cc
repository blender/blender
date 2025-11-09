/* SPDX-FileCopyrightText: 2006 Blender Authors
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
#include "DNA_pointcloud_types.h"

#include "BLI_index_range.hh"
#include "BLI_string.h"
#include "BLI_string_utils.hh"

#include "BLT_translation.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.hh"
#include "BKE_report.hh"

#include <fmt/format.h>

using blender::IndexRange;
using blender::StringRef;
using blender::bke::AttrDomain;

AttributeOwner AttributeOwner::from_id(ID *id)
{
  if (id == nullptr) {
    return {};
  }
  switch (GS(id->name)) {
    case ID_ME:
      return AttributeOwner(AttributeOwnerType::Mesh, id);
    case ID_PT:
      return AttributeOwner(AttributeOwnerType::PointCloud, id);
    case ID_CV:
      return AttributeOwner(AttributeOwnerType::Curves, id);
    case ID_GP:
      return AttributeOwner(AttributeOwnerType::GreasePencil, id);
    default:
      return {};
  }
}

AttributeOwnerType AttributeOwner::type() const
{
  return type_;
}

bool AttributeOwner::is_valid() const
{
  return ptr_ != nullptr;
}

Mesh *AttributeOwner::get_mesh() const
{
  BLI_assert(this->is_valid());
  BLI_assert(type_ == AttributeOwnerType::Mesh);
  return reinterpret_cast<Mesh *>(ptr_);
}

PointCloud *AttributeOwner::get_pointcloud() const
{
  BLI_assert(this->is_valid());
  BLI_assert(type_ == AttributeOwnerType::PointCloud);
  return reinterpret_cast<PointCloud *>(ptr_);
}

Curves *AttributeOwner::get_curves() const
{
  BLI_assert(this->is_valid());
  BLI_assert(type_ == AttributeOwnerType::Curves);
  return reinterpret_cast<Curves *>(ptr_);
}

GreasePencil *AttributeOwner::get_grease_pencil() const
{
  BLI_assert(this->is_valid());
  BLI_assert(type_ == AttributeOwnerType::GreasePencil);
  return reinterpret_cast<GreasePencil *>(ptr_);
}

GreasePencilDrawing *AttributeOwner::get_grease_pencil_drawing() const
{
  BLI_assert(this->is_valid());
  BLI_assert(type_ == AttributeOwnerType::GreasePencilDrawing);
  return reinterpret_cast<GreasePencilDrawing *>(ptr_);
}

blender::bke::AttributeStorage *AttributeOwner::get_storage() const
{
  switch (type_) {
    case AttributeOwnerType::Mesh:
      return &this->get_mesh()->attribute_storage.wrap();
    case AttributeOwnerType::PointCloud:
      return &this->get_pointcloud()->attribute_storage.wrap();
    case AttributeOwnerType::Curves:
      return &this->get_curves()->geometry.attribute_storage.wrap();
    case AttributeOwnerType::GreasePencil:
      return &this->get_grease_pencil()->attribute_storage.wrap();
    case AttributeOwnerType::GreasePencilDrawing:
      return &this->get_grease_pencil_drawing()->geometry.attribute_storage.wrap();
  }
  BLI_assert(false);
  return nullptr;
}

std::optional<blender::bke::MutableAttributeAccessor> AttributeOwner::get_accessor() const
{
  switch (type_) {
    case AttributeOwnerType::Mesh:
      /* The attribute API isn't implemented for BMesh, so edit mode meshes are not supported. */
      BLI_assert(this->get_mesh()->runtime->edit_mesh == nullptr);
      return this->get_mesh()->attributes_for_write();
    case AttributeOwnerType::PointCloud:
      return this->get_pointcloud()->attributes_for_write();
    case AttributeOwnerType::Curves:
      return this->get_curves()->geometry.wrap().attributes_for_write();
    case AttributeOwnerType::GreasePencil:
      return this->get_grease_pencil()->attributes_for_write();
    case AttributeOwnerType::GreasePencilDrawing:
      return this->get_grease_pencil_drawing()->geometry.wrap().attributes_for_write();
  }
  BLI_assert(false);
  return std::nullopt;
}

struct DomainInfo {
  CustomData *customdata = nullptr;
  int length = 0;
};

static std::array<DomainInfo, ATTR_DOMAIN_NUM> get_domains(const AttributeOwner &owner)
{
  std::array<DomainInfo, ATTR_DOMAIN_NUM> info;

  switch (owner.type()) {
    case AttributeOwnerType::Curves:
    case AttributeOwnerType::GreasePencil:
    case AttributeOwnerType::GreasePencilDrawing:
    case AttributeOwnerType::PointCloud: {
      /* This should be implemented with #AttributeStorage instead. */
      BLI_assert_unreachable();
      break;
    }
    case AttributeOwnerType::Mesh: {
      Mesh *mesh = owner.get_mesh();
      if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
        BMesh *bm = em->bm;
        info[int(AttrDomain::Point)].customdata = &bm->vdata;
        info[int(AttrDomain::Point)].length = bm->totvert;
        info[int(AttrDomain::Edge)].customdata = &bm->edata;
        info[int(AttrDomain::Edge)].length = bm->totedge;
        info[int(AttrDomain::Corner)].customdata = &bm->ldata;
        info[int(AttrDomain::Corner)].length = bm->totloop;
        info[int(AttrDomain::Face)].customdata = &bm->pdata;
        info[int(AttrDomain::Face)].length = bm->totface;
      }
      else {
        info[int(AttrDomain::Point)].customdata = &mesh->vert_data;
        info[int(AttrDomain::Point)].length = mesh->verts_num;
        info[int(AttrDomain::Edge)].customdata = &mesh->edge_data;
        info[int(AttrDomain::Edge)].length = mesh->edges_num;
        info[int(AttrDomain::Corner)].customdata = &mesh->corner_data;
        info[int(AttrDomain::Corner)].length = mesh->corners_num;
        info[int(AttrDomain::Face)].customdata = &mesh->face_data;
        info[int(AttrDomain::Face)].length = mesh->faces_num;
      }
      break;
    }
  }

  return info;
}

static bool bke_attribute_rename_if_exists(AttributeOwner &owner,
                                           const StringRef old_name,
                                           const StringRef new_name,
                                           ReportList *reports)
{
  CustomDataLayer *layer = BKE_attribute_search_for_write(
      owner, old_name, CD_MASK_PROP_ALL, ATTR_DOMAIN_MASK_ALL);
  if (layer == nullptr) {
    return false;
  }
  return BKE_attribute_rename(owner, old_name, new_name, reports);
}

static bool name_valid_for_builtin_domain_and_type(
    const blender::bke::AttributeAccessor attributes,
    const StringRef name,
    const AttrDomain domain,
    const blender::bke::AttrType data_type,
    ReportList *reports)
{
  if (const std::optional metadata = attributes.get_builtin_domain_and_type(name)) {
    if (domain != metadata->domain) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Domain unsupported for \"%s\" attribute",
                  std::string(name).c_str());
      return false;
    }
    if (data_type != metadata->data_type) {
      BKE_reportf(
          reports, RPT_ERROR, "Type unsupported for \"%s\" attribute", std::string(name).c_str());
      return false;
    }
  }
  return true;
}

static bool mesh_attribute_valid(const Mesh &mesh,
                                 const StringRef name,
                                 const AttrDomain domain,
                                 const blender::bke::AttrType data_type,
                                 ReportList *reports)
{
  using namespace blender;
  if (mesh.runtime->edit_mesh) {
    if (BM_attribute_stored_in_bmesh_builtin(name)) {
      BKE_report(reports, RPT_ERROR, "Unable to create attribute in edit mode");
      return false;
    }
  }
  if (!name_valid_for_builtin_domain_and_type(mesh.attributes(), name, domain, data_type, reports))
  {
    return false;
  }
  return true;
}

bool BKE_attribute_rename(AttributeOwner &owner,
                          const StringRef old_name,
                          const StringRef new_name,
                          ReportList *reports)
{
  using namespace blender;
  if (BKE_attribute_required(owner, old_name)) {
    BLI_assert_msg(0, "Required attribute name is not editable");
    return false;
  }
  if (new_name.is_empty()) {
    BKE_report(reports, RPT_ERROR, "Attribute name cannot be empty");
    return false;
  }

  if (owner.type() == AttributeOwnerType::Mesh) {
    Mesh *mesh = owner.get_mesh();
    /* NOTE: Checking if the new name matches the old name only makes sense when the name
     * is clamped to its maximum length, otherwise assigning an over-long name multiple times
     * will add `.001` suffix unnecessarily. */
    {
      const int new_name_maxncpy = CustomData_name_maxncpy_calc(new_name);
      /* NOTE: A function that performs a clamped comparison without copying would be handy. */
      char new_name_clamped[MAX_CUSTOMDATA_LAYER_NAME];
      new_name.copy_utf8_truncated(new_name_clamped, new_name_maxncpy);
      if (old_name == new_name_clamped) {
        return false;
      }
    }

    CustomDataLayer *layer = BKE_attribute_search_for_write(
        owner, old_name, CD_MASK_PROP_ALL, ATTR_DOMAIN_MASK_ALL);
    if (layer == nullptr) {
      BKE_report(reports, RPT_ERROR, "Attribute is not part of this geometry");
      return false;
    }

    if (!mesh_attribute_valid(*mesh,
                              new_name,
                              BKE_attribute_domain(owner, layer),
                              *bke::custom_data_type_to_attr_type(eCustomDataType(layer->type)),
                              reports))
    {
      return false;
    }

    std::string result_name = BKE_attribute_calc_unique_name(owner, new_name);

    if (layer->type == CD_PROP_FLOAT2) {
      /* Rename UV sub-attributes. */
      char buffer_src[MAX_CUSTOMDATA_LAYER_NAME];
      char buffer_dst[MAX_CUSTOMDATA_LAYER_NAME];
      bke_attribute_rename_if_exists(owner,
                                     BKE_uv_map_pin_name_get(layer->name, buffer_src),
                                     BKE_uv_map_pin_name_get(result_name, buffer_dst),
                                     reports);
    }

    if (old_name == BKE_id_attributes_active_color_name(&mesh->id)) {
      BKE_id_attributes_active_color_set(&mesh->id, result_name);
    }
    if (old_name == BKE_id_attributes_default_color_name(&mesh->id)) {
      BKE_id_attributes_default_color_set(&mesh->id, result_name);
    }

    StringRef(result_name).copy_utf8_truncated(layer->name);

    return true;
  }

  bke::AttributeStorage &attributes = *owner.get_storage();
  bke::Attribute *attr = attributes.lookup(old_name);
  if (!attr) {
    BKE_report(reports, RPT_ERROR, "Attribute is not part of this geometry");
    return false;
  }

  if (owner.type() == AttributeOwnerType::Curves) {
    Curves *curves = owner.get_curves();
    if (!name_valid_for_builtin_domain_and_type(curves->geometry.wrap().attributes(),
                                                new_name,
                                                attr->domain(),
                                                attr->data_type(),
                                                reports))
    {
      return false;
    }
  }

  attributes.rename(old_name, new_name);
  return true;
}

static bool attribute_name_exists(const AttributeOwner &owner, const StringRef name)
{
  const std::array<DomainInfo, ATTR_DOMAIN_NUM> info = get_domains(owner);

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    if (!info[domain].customdata) {
      continue;
    }

    const CustomData *cdata = info[domain].customdata;
    for (int i = 0; i < cdata->totlayer; i++) {
      const CustomDataLayer *layer = cdata->layers + i;

      if (layer->name == name) {
        return true;
      }
    }
  }

  return false;
}

std::string BKE_attribute_calc_unique_name(const AttributeOwner &owner, const StringRef name)
{
  if (owner.type() == AttributeOwnerType::Mesh) {
    return BLI_uniquename_cb(
        [&](const StringRef new_name) { return attribute_name_exists(owner, new_name); },
        '.',
        name.is_empty() ? DATA_("Attribute") : name);
  }

  blender::bke::AttributeStorage &storage = *owner.get_storage();
  return storage.unique_name_calc(name);
}

CustomDataLayer *BKE_attribute_new(AttributeOwner &owner,
                                   const StringRef name,
                                   const eCustomDataType type,
                                   const AttrDomain domain,
                                   ReportList *reports)
{
  using namespace blender::bke;
  const std::array<DomainInfo, ATTR_DOMAIN_NUM> info = get_domains(owner);

  CustomData *customdata = info[int(domain)].customdata;
  if (customdata == nullptr) {
    BKE_report(reports, RPT_ERROR, "Attribute domain not supported by this geometry type");
    return nullptr;
  }

  std::string uniquename = BKE_attribute_calc_unique_name(owner, name);

  if (owner.type() == AttributeOwnerType::Mesh) {
    Mesh *mesh = owner.get_mesh();
    if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
      if (!mesh_attribute_valid(
              *mesh, name, domain, *custom_data_type_to_attr_type(type), reports))
      {
        return nullptr;
      }
      BM_data_layer_add_named(em->bm, customdata, type, uniquename.c_str());
      const int index = CustomData_get_named_layer_index(customdata, type, uniquename);
      return (index == -1) ? nullptr : &(customdata->layers[index]);
    }
  }

  std::optional<MutableAttributeAccessor> attributes = owner.get_accessor();
  if (!attributes) {
    return nullptr;
  }

  attributes->add(
      uniquename, domain, *custom_data_type_to_attr_type(type), AttributeInitDefaultValue());

  const int index = CustomData_get_named_layer_index(customdata, type, uniquename);
  if (index == -1) {
    BKE_reportf(reports, RPT_WARNING, "Layer '%s' could not be created", uniquename.c_str());
  }

  return (index == -1) ? nullptr : &(customdata->layers[index]);
}

static void bke_attribute_copy_if_exists(AttributeOwner &owner,
                                         const StringRef srcname,
                                         const StringRef dstname)
{
  using namespace blender::bke;

  std::optional<MutableAttributeAccessor> attributes = owner.get_accessor();
  if (!attributes) {
    return;
  }

  GAttributeReader src = attributes->lookup(srcname);
  if (!src) {
    return;
  }

  attributes->add(dstname,
                  src.domain,
                  cpp_type_to_attribute_type(src.varray.type()),
                  AttributeInitVArray(src.varray));
}

CustomDataLayer *BKE_attribute_duplicate(AttributeOwner &owner,
                                         const StringRef name,
                                         ReportList *reports)
{
  using namespace blender::bke;
  std::string uniquename = BKE_attribute_calc_unique_name(owner, name);

  if (owner.type() == AttributeOwnerType::Mesh) {
    Mesh *mesh = owner.get_mesh();
    if (mesh->runtime->edit_mesh) {
      BLI_assert_unreachable();
      return nullptr;
    }
  }

  std::optional<MutableAttributeAccessor> attributes = owner.get_accessor();
  if (!attributes) {
    return nullptr;
  }

  GAttributeReader src = attributes->lookup(name);
  if (!src) {
    BKE_report(reports, RPT_ERROR, "Attribute is not part of this geometry");
    return nullptr;
  }

  const AttrType type = cpp_type_to_attribute_type(src.varray.type());
  attributes->add(uniquename, src.domain, type, AttributeInitVArray(src.varray));

  if (owner.type() == AttributeOwnerType::Mesh && type == AttrType::Float2) {
    /* Duplicate UV sub-attributes. */
    char buffer_src[MAX_CUSTOMDATA_LAYER_NAME];
    char buffer_dst[MAX_CUSTOMDATA_LAYER_NAME];
    bke_attribute_copy_if_exists(owner,
                                 BKE_uv_map_pin_name_get(name, buffer_src),
                                 BKE_uv_map_pin_name_get(uniquename, buffer_dst));
  }

  return BKE_attribute_search_for_write(owner, uniquename, CD_MASK_PROP_ALL, ATTR_DOMAIN_MASK_ALL);
}

static int color_name_to_index(AttributeOwner &owner, const StringRef name)
{
  const CustomDataLayer *layer = BKE_attribute_search(
      owner, name, CD_MASK_COLOR_ALL, ATTR_DOMAIN_MASK_COLOR);
  return BKE_attribute_to_index(owner, layer, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);
}

static int color_clamp_index(AttributeOwner &owner, int index)
{
  const int length = BKE_attributes_length(owner, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);
  return min_ii(index, length - 1);
}

static StringRef color_name_from_index(AttributeOwner &owner, int index)
{
  const CustomDataLayer *layer = BKE_attribute_from_index(
      owner, index, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);
  return layer ? layer->name : "";
}

bool BKE_attribute_remove(AttributeOwner &owner, const StringRef name, ReportList *reports)
{
  using namespace blender;
  using namespace blender::bke;
  if (name.is_empty()) {
    BKE_report(reports, RPT_ERROR, "The attribute name must not be empty");
    return false;
  }
  if (BKE_attribute_required(owner, name)) {
    BKE_report(reports, RPT_ERROR, "Attribute is required and cannot be removed");
    return false;
  }

  if (owner.type() == AttributeOwnerType::Mesh) {
    const std::array<DomainInfo, ATTR_DOMAIN_NUM> info = get_domains(owner);
    Mesh *mesh = owner.get_mesh();
    if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
      for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
        if (CustomData *data = info[domain].customdata) {
          const std::string name_copy = name;
          const int layer_index = CustomData_get_named_layer_index_notype(data, name_copy);
          if (layer_index == -1) {
            continue;
          }

          const eCustomDataType type = eCustomDataType(data->layers[layer_index].type);
          const bool is_active_color_attribute = name_copy.c_str() ==
                                                 StringRef(mesh->active_color_attribute);
          const bool is_default_color_attribute = name_copy.c_str() ==
                                                  StringRef(mesh->default_color_attribute);
          const int active_color_index = color_name_to_index(owner, mesh->active_color_attribute);
          const int default_color_index = color_name_to_index(owner,
                                                              mesh->default_color_attribute);

          if (!BM_data_layer_free_named(em->bm, data, name_copy.c_str())) {
            BLI_assert_unreachable();
          }

          if (is_active_color_attribute) {
            BKE_id_attributes_active_color_set(
                &mesh->id,
                color_name_from_index(owner, color_clamp_index(owner, active_color_index)));
          }
          if (is_default_color_attribute) {
            BKE_id_attributes_default_color_set(
                &mesh->id,
                color_name_from_index(owner, color_clamp_index(owner, default_color_index)));
          }

          if (type == CD_PROP_FLOAT2 && domain == int(AttrDomain::Corner)) {
            char buffer[MAX_CUSTOMDATA_LAYER_NAME];
            BM_data_layer_free_named(em->bm, data, BKE_uv_map_pin_name_get(name_copy, buffer));
          }
          return true;
        }
      }
      return false;
    }
  }

  std::optional<MutableAttributeAccessor> attributes = owner.get_accessor();
  if (!attributes) {
    return false;
  }

  if (owner.type() == AttributeOwnerType::Mesh) {
    const std::string name_copy = name;
    std::optional<blender::bke::AttributeMetaData> metadata = attributes->lookup_meta_data(
        name_copy);
    if (!metadata) {
      return false;
    }
    /* Update active and default color attributes. */
    Mesh *mesh = owner.get_mesh();
    const bool is_active_color_attribute = name_copy == StringRef(mesh->active_color_attribute);
    const bool is_default_color_attribute = name_copy == StringRef(mesh->default_color_attribute);
    const int active_color_index = color_name_to_index(owner, mesh->active_color_attribute);
    const int default_color_index = color_name_to_index(owner, mesh->default_color_attribute);

    if (!attributes->remove(name_copy)) {
      BLI_assert_unreachable();
    }

    if (is_active_color_attribute) {
      BKE_id_attributes_active_color_set(
          &mesh->id, color_name_from_index(owner, color_clamp_index(owner, active_color_index)));
    }
    if (is_default_color_attribute) {
      BKE_id_attributes_default_color_set(
          &mesh->id, color_name_from_index(owner, color_clamp_index(owner, default_color_index)));
    }

    if (bke::mesh::is_uv_map(metadata)) {
      char buffer[MAX_CUSTOMDATA_LAYER_NAME];
      attributes->remove(BKE_uv_map_pin_name_get(name_copy, buffer));
    }
    return true;
  }

  return attributes->remove(name);
}

const CustomDataLayer *BKE_attribute_search(const AttributeOwner &owner,
                                            const StringRef name,
                                            const eCustomDataMask type_mask,
                                            const AttrDomainMask domain_mask)
{
  if (name.is_empty()) {
    return nullptr;
  }
  const std::array<DomainInfo, ATTR_DOMAIN_NUM> info = get_domains(owner);

  for (AttrDomain domain = AttrDomain::Point; int(domain) < ATTR_DOMAIN_NUM;
       domain = AttrDomain(int(domain) + 1))
  {
    if (!(domain_mask & ATTR_DOMAIN_AS_MASK(domain))) {
      continue;
    }

    CustomData *customdata = info[int(domain)].customdata;
    if (customdata == nullptr) {
      continue;
    }

    for (int i = 0; i < customdata->totlayer; i++) {
      CustomDataLayer *layer = &customdata->layers[i];
      if ((CD_TYPE_AS_MASK(eCustomDataType(layer->type)) & type_mask) && layer->name == name) {
        return layer;
      }
    }
  }

  return nullptr;
}

CustomDataLayer *BKE_attribute_search_for_write(AttributeOwner &owner,
                                                const StringRef name,
                                                const eCustomDataMask type_mask,
                                                const AttrDomainMask domain_mask)
{
  /* Reuse the implementation of the const version.
   * Implicit sharing for the layer's data is handled below. */
  CustomDataLayer *layer = const_cast<CustomDataLayer *>(
      BKE_attribute_search(owner, name, type_mask, domain_mask));
  if (!layer) {
    return nullptr;
  }

  const std::array<DomainInfo, ATTR_DOMAIN_NUM> info = get_domains(owner);

  const AttrDomain domain = BKE_attribute_domain(owner, layer);
  CustomData_ensure_data_is_mutable(layer, info[int(domain)].length);

  return layer;
}

int BKE_attributes_length(const AttributeOwner &owner,
                          AttrDomainMask domain_mask,
                          eCustomDataMask mask)
{
  const std::array<DomainInfo, ATTR_DOMAIN_NUM> info = get_domains(owner);

  int length = 0;

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    const CustomData *customdata = info[domain].customdata;
    if (customdata == nullptr) {
      continue;
    }

    if ((1 << int(domain)) & domain_mask) {
      length += CustomData_number_of_layers_typemask(customdata, mask);
    }
  }

  return length;
}

AttrDomain BKE_attribute_domain(const AttributeOwner &owner, const CustomDataLayer *layer)
{
  const std::array<DomainInfo, ATTR_DOMAIN_NUM> info = get_domains(owner);

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    const CustomData *customdata = info[domain].customdata;
    if (customdata == nullptr) {
      continue;
    }
    if (blender::Span(customdata->layers, customdata->totlayer).contains_ptr(layer)) {
      return AttrDomain(domain);
    }
  }

  BLI_assert_msg(0, "Custom data layer not found in geometry");
  return AttrDomain(AttrDomain::Point);
}

int BKE_attribute_domain_size(const AttributeOwner &owner, const int domain)
{
  const std::array<DomainInfo, ATTR_DOMAIN_NUM> info = get_domains(owner);
  return info[domain].length;
}

int BKE_attribute_data_length(AttributeOwner &owner, CustomDataLayer *layer)
{
  /* When in mesh editmode, attributes point to bmesh customdata layers, the attribute data is
   * empty since custom data is stored per element instead of a single array there (same es UVs
   * etc.), see D11998. */
  if (owner.type() == AttributeOwnerType::Mesh) {
    Mesh *mesh = owner.get_mesh();
    if (mesh->runtime->edit_mesh != nullptr) {
      return 0;
    }
  }

  const std::array<DomainInfo, ATTR_DOMAIN_NUM> info = get_domains(owner);

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    const CustomData *customdata = info[domain].customdata;
    if (customdata == nullptr) {
      continue;
    }
    if (blender::Span(customdata->layers, customdata->totlayer).contains_ptr(layer)) {
      return info[domain].length;
    }
  }

  BLI_assert_msg(0, "Custom data layer not found in geometry");
  return 0;
}

bool BKE_attribute_required(const AttributeOwner &owner, const StringRef name)
{
  switch (owner.type()) {
    case AttributeOwnerType::PointCloud:
      return BKE_pointcloud_attribute_required(owner.get_pointcloud(), name);
    case AttributeOwnerType::Curves:
      return BKE_curves_attribute_required(owner.get_curves(), name);
    case AttributeOwnerType::Mesh:
      return BKE_mesh_attribute_required(name);
    case AttributeOwnerType::GreasePencil:
      return false;
    case AttributeOwnerType::GreasePencilDrawing:
      return BKE_grease_pencil_drawing_attribute_required(owner.get_grease_pencil_drawing(), name);
  }
  return false;
}

std::optional<blender::StringRefNull> BKE_attributes_active_name_get(AttributeOwner &owner)
{
  using namespace blender;
  using namespace blender::bke;
  int active_index = *BKE_attributes_active_index_p(owner);
  if (active_index == -1) {
    return std::nullopt;
  }
  if (owner.type() == AttributeOwnerType::Mesh) {
    if (active_index > BKE_attributes_length(owner, ATTR_DOMAIN_MASK_ALL, CD_MASK_PROP_ALL)) {
      active_index = 0;
    }
    const std::array<DomainInfo, ATTR_DOMAIN_NUM> info = get_domains(owner);
    int index = 0;
    for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
      CustomData *customdata = info[domain].customdata;
      if (customdata == nullptr) {
        continue;
      }
      for (int i = 0; i < customdata->totlayer; i++) {
        CustomDataLayer *layer = &customdata->layers[i];
        if (CD_MASK_PROP_ALL & CD_TYPE_AS_MASK(eCustomDataType(layer->type))) {
          if (index == active_index) {
            if (blender::bke::allow_procedural_attribute_access(layer->name)) {
              return layer->name;
            }
            return std::nullopt;
          }
          index++;
        }
      }
    }
    return std::nullopt;
  }

  bke::AttributeStorage &storage = *owner.get_storage();
  if (!IndexRange(storage.count()).contains(active_index)) {
    return std::nullopt;
  }
  return storage.at_index(active_index).name();
}

void BKE_attributes_active_set(AttributeOwner &owner, const StringRef name)
{
  using namespace blender;
  if (owner.type() == AttributeOwnerType::Mesh) {
    const CustomDataLayer *layer = BKE_attribute_search(
        owner, name, CD_MASK_PROP_ALL, ATTR_DOMAIN_MASK_ALL);
    BLI_assert(layer != nullptr);

    const int index = BKE_attribute_to_index(owner, layer, ATTR_DOMAIN_MASK_ALL, CD_MASK_PROP_ALL);
    *BKE_attributes_active_index_p(owner) = index;
    return;
  }

  bke::AttributeStorage &attributes = *owner.get_storage();
  *BKE_attributes_active_index_p(owner) = attributes.index_of(name);
}

void BKE_attributes_active_clear(AttributeOwner &owner)
{
  *BKE_attributes_active_index_p(owner) = -1;
}

int *BKE_attributes_active_index_p(AttributeOwner &owner)
{
  switch (owner.type()) {
    case AttributeOwnerType::PointCloud: {
      return &owner.get_pointcloud()->attributes_active_index;
    }
    case AttributeOwnerType::Mesh: {
      return &owner.get_mesh()->attributes_active_index;
    }
    case AttributeOwnerType::Curves: {
      return &owner.get_curves()->geometry.attributes_active_index;
    }
    case AttributeOwnerType::GreasePencil: {
      return &owner.get_grease_pencil()->attributes_active_index;
    }
    case AttributeOwnerType::GreasePencilDrawing: {
      return &owner.get_grease_pencil_drawing()->geometry.attributes_active_index;
    }
  }
  return nullptr;
}

CustomData *BKE_attributes_iterator_next_domain(AttributeOwner &owner, CustomDataLayer *layers)
{
  const std::array<DomainInfo, ATTR_DOMAIN_NUM> info = get_domains(owner);

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

CustomDataLayer *BKE_attribute_from_index(AttributeOwner &owner,
                                          int lookup_index,
                                          AttrDomainMask domain_mask,
                                          eCustomDataMask layer_mask)
{
  const std::array<DomainInfo, ATTR_DOMAIN_NUM> info = get_domains(owner);

  int index = 0;
  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    CustomData *customdata = info[domain].customdata;

    if (!customdata || !((1 << int(domain)) & domain_mask)) {
      continue;
    }

    for (int i = 0; i < customdata->totlayer; i++) {
      if (!(layer_mask & CD_TYPE_AS_MASK(eCustomDataType(customdata->layers[i].type))) ||
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

int BKE_attribute_to_index(const AttributeOwner &owner,
                           const CustomDataLayer *layer,
                           AttrDomainMask domain_mask,
                           eCustomDataMask layer_mask)
{
  if (!layer) {
    return -1;
  }

  const std::array<DomainInfo, ATTR_DOMAIN_NUM> info = get_domains(owner);

  int index = 0;
  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    const CustomData *customdata = info[domain].customdata;

    if (!customdata || !((1 << int(domain)) & domain_mask)) {
      continue;
    }

    for (int i = 0; i < customdata->totlayer; i++) {
      const CustomDataLayer *layer_iter = customdata->layers + i;
      if (!(layer_mask & CD_TYPE_AS_MASK(eCustomDataType(layer_iter->type))) ||
          (layer_iter->flag & CD_FLAG_TEMPORARY))
      {
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

std::optional<StringRef> BKE_id_attributes_active_color_name(const ID *id)
{
  if (GS(id->name) == ID_ME) {
    return reinterpret_cast<const Mesh *>(id)->active_color_attribute;
  }
  return std::nullopt;
}

std::optional<StringRef> BKE_id_attributes_default_color_name(const ID *id)
{
  if (GS(id->name) == ID_ME) {
    return reinterpret_cast<const Mesh *>(id)->default_color_attribute;
  }
  return std::nullopt;
}

void BKE_id_attributes_active_color_set(ID *id, const std::optional<StringRef> name)
{
  switch (GS(id->name)) {
    case ID_ME: {
      Mesh *mesh = reinterpret_cast<Mesh *>(id);
      MEM_SAFE_FREE(mesh->active_color_attribute);
      if (name) {
        mesh->active_color_attribute = BLI_strdupn(name->data(), name->size());
      }
      break;
    }
    default:
      break;
  }
}

void BKE_id_attributes_active_color_clear(ID *id)
{
  switch (GS(id->name)) {
    case ID_ME: {
      Mesh *mesh = reinterpret_cast<Mesh *>(id);
      MEM_SAFE_FREE(mesh->active_color_attribute);
      break;
    }
    default:
      break;
  }
}

void BKE_id_attributes_default_color_set(ID *id, const std::optional<StringRef> name)
{
  switch (GS(id->name)) {
    case ID_ME: {
      Mesh *mesh = reinterpret_cast<Mesh *>(id);
      MEM_SAFE_FREE(mesh->default_color_attribute);
      if (name) {
        mesh->default_color_attribute = BLI_strdupn(name->data(), name->size());
      }
      break;
    }
    default:
      break;
  }
}

const CustomDataLayer *BKE_id_attributes_color_find(const ID *id, const StringRef name)
{
  AttributeOwner owner = AttributeOwner::from_id(const_cast<ID *>(id));
  return BKE_attribute_search(owner, name, CD_MASK_COLOR_ALL, ATTR_DOMAIN_MASK_COLOR);
}

bool BKE_color_attribute_supported(const Mesh &mesh, const StringRef name)
{
  return blender::bke::mesh::is_color_attribute(mesh.attributes().lookup_meta_data(name));
}

StringRef BKE_uv_map_pin_name_get(const StringRef uv_map_name, char *buffer)
{
  BLI_assert(strlen(UV_PINNED_NAME) == 2);
  BLI_assert(uv_map_name.size() < MAX_CUSTOMDATA_LAYER_NAME - 4);
  const auto result = fmt::format_to_n(
      buffer, MAX_CUSTOMDATA_LAYER_NAME, ".{}.{}", UV_PINNED_NAME, uv_map_name);
  return StringRef(buffer, result.size);
}
