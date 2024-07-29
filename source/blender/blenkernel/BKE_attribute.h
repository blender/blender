/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 * \brief Generic geometry attributes built on CustomData.
 */

#pragma once

#include <string>

#include "BLI_string_ref.hh"
#include "BLI_sys_types.h"

#include "DNA_customdata_types.h"

namespace blender::bke {
enum class AttrDomain : int8_t;
class AttributeAccessor;
}  // namespace blender::bke
struct CustomData;
struct CustomDataLayer;
struct ID;
struct ReportList;
struct Mesh;
struct PointCloud;
struct Curves;
struct GreasePencil;
struct GreasePencilDrawing;

typedef enum AttrDomainMask {
  ATTR_DOMAIN_MASK_POINT = (1 << 0),
  ATTR_DOMAIN_MASK_EDGE = (1 << 1),
  ATTR_DOMAIN_MASK_FACE = (1 << 2),
  ATTR_DOMAIN_MASK_CORNER = (1 << 3),
  ATTR_DOMAIN_MASK_CURVE = (1 << 4),
  ATTR_DOMAIN_MASK_GREASE_PENCIL_LAYER = (1 << 6),
  ATTR_DOMAIN_MASK_ALL = (1 << 7) - 1
} AttrDomainMask;
ENUM_OPERATORS(AttrDomainMask, ATTR_DOMAIN_MASK_ALL);

enum class AttributeOwnerType {
  Mesh,
  PointCloud,
  Curves,
  GreasePencil,
  GreasePencilDrawing,
};

class AttributeOwner {
  AttributeOwnerType type_;
  void *ptr_ = nullptr;

 public:
  AttributeOwner(){};
  AttributeOwner(AttributeOwnerType type, void *ptr) : type_(type), ptr_(ptr){};

  static AttributeOwner from_id(ID *id);

  AttributeOwnerType type() const;
  bool is_valid() const;

  Mesh *get_mesh() const;
  PointCloud *get_pointcloud() const;
  Curves *get_curves() const;
  GreasePencil *get_grease_pencil() const;
  GreasePencilDrawing *get_grease_pencil_drawing() const;
};

#define ATTR_DOMAIN_AS_MASK(domain) ((AttrDomainMask)((1 << (int)(domain))))

/* All domains that support color attributes. */
#define ATTR_DOMAIN_MASK_COLOR \
  ((AttrDomainMask)((ATTR_DOMAIN_MASK_POINT | ATTR_DOMAIN_MASK_CORNER)))

/* Attributes. */

/**
 * Create a new attribute layer.
 */
struct CustomDataLayer *BKE_attribute_new(AttributeOwner &owner,
                                          const char *name,
                                          eCustomDataType type,
                                          blender::bke::AttrDomain domain,
                                          struct ReportList *reports);
bool BKE_attribute_remove(AttributeOwner &owner, const char *name, struct ReportList *reports);

/**
 * Creates a duplicate attribute layer.
 */
struct CustomDataLayer *BKE_attribute_duplicate(AttributeOwner &owner,
                                                const char *name,
                                                struct ReportList *reports);

struct CustomDataLayer *BKE_attribute_find(const AttributeOwner &owner,
                                           const char *name,
                                           eCustomDataType type,
                                           blender::bke::AttrDomain domain);

const struct CustomDataLayer *BKE_attribute_search(const AttributeOwner &owner,
                                                   const char *name,
                                                   eCustomDataMask type,
                                                   AttrDomainMask domain_mask);

struct CustomDataLayer *BKE_attribute_search_for_write(AttributeOwner &owner,
                                                       const char *name,
                                                       eCustomDataMask type,
                                                       AttrDomainMask domain_mask);

blender::bke::AttrDomain BKE_attribute_domain(const AttributeOwner &owner,
                                              const struct CustomDataLayer *layer);
int BKE_attribute_domain_size(const AttributeOwner &owner, int domain);
int BKE_attribute_data_length(AttributeOwner &owner, struct CustomDataLayer *layer);
bool BKE_attribute_required(const AttributeOwner &owner, const char *name);
bool BKE_attribute_rename(AttributeOwner &owner,
                          const char *old_name,
                          const char *new_name,
                          struct ReportList *reports);

int BKE_attributes_length(const AttributeOwner &owner,
                          AttrDomainMask domain_mask,
                          eCustomDataMask mask);

struct CustomDataLayer *BKE_attributes_active_get(AttributeOwner &owner);
void BKE_attributes_active_set(AttributeOwner &owner, const char *name);
int *BKE_attributes_active_index_p(AttributeOwner &owner);

CustomData *BKE_attributes_iterator_next_domain(AttributeOwner &owner,
                                                struct CustomDataLayer *layers);
CustomDataLayer *BKE_attribute_from_index(AttributeOwner &owner,
                                          int lookup_index,
                                          AttrDomainMask domain_mask,
                                          eCustomDataMask layer_mask);

/** Layer is allowed to be nullptr; if so -1 (layer not found) will be returned. */
int BKE_attribute_to_index(const AttributeOwner &owner,
                           const CustomDataLayer *layer,
                           AttrDomainMask domain_mask,
                           eCustomDataMask layer_mask);

const char *BKE_id_attributes_active_color_name(const struct ID *id);
const char *BKE_id_attributes_default_color_name(const struct ID *id);
void BKE_id_attributes_active_color_set(struct ID *id, const char *name);
void BKE_id_attributes_default_color_set(struct ID *id, const char *name);

const struct CustomDataLayer *BKE_id_attributes_color_find(const struct ID *id, const char *name);
bool BKE_color_attribute_supported(const struct Mesh &mesh, const blender::StringRef name);

std::string BKE_attribute_calc_unique_name(const AttributeOwner &owner,
                                           const blender::StringRef name);

const char *BKE_uv_map_vert_select_name_get(const char *uv_map_name, char *buffer);
const char *BKE_uv_map_edge_select_name_get(const char *uv_map_name, char *buffer);
const char *BKE_uv_map_pin_name_get(const char *uv_map_name, char *buffer);
