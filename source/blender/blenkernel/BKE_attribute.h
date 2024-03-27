/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 * \brief Generic geometry attributes built on CustomData.
 */

#pragma once

#include <string>

#include "BLI_sys_types.h"

#include "DNA_customdata_types.h"

namespace blender::bke {
enum class AttrDomain : int8_t;
}
struct CustomData;
struct CustomDataLayer;
struct ID;
struct ReportList;

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

#define ATTR_DOMAIN_AS_MASK(domain) ((AttrDomainMask)((1 << (int)(domain))))

/* All domains that support color attributes. */
#define ATTR_DOMAIN_MASK_COLOR \
  ((AttrDomainMask)((ATTR_DOMAIN_MASK_POINT | ATTR_DOMAIN_MASK_CORNER)))

/* Attributes. */

bool BKE_id_attributes_supported(const struct ID *id);
bool BKE_attribute_allow_procedural_access(const char *attribute_name);

/**
 * Create a new attribute layer.
 */
struct CustomDataLayer *BKE_id_attribute_new(struct ID *id,
                                             const char *name,
                                             eCustomDataType type,
                                             blender::bke::AttrDomain domain,
                                             struct ReportList *reports);
bool BKE_id_attribute_remove(struct ID *id, const char *name, struct ReportList *reports);

/**
 * Creates a duplicate attribute layer.
 */
struct CustomDataLayer *BKE_id_attribute_duplicate(struct ID *id,
                                                   const char *name,
                                                   struct ReportList *reports);

struct CustomDataLayer *BKE_id_attribute_find(const struct ID *id,
                                              const char *name,
                                              eCustomDataType type,
                                              blender::bke::AttrDomain domain);

const struct CustomDataLayer *BKE_id_attribute_search(const struct ID *id,
                                                      const char *name,
                                                      eCustomDataMask type,
                                                      AttrDomainMask domain_mask);

struct CustomDataLayer *BKE_id_attribute_search_for_write(struct ID *id,
                                                          const char *name,
                                                          eCustomDataMask type,
                                                          AttrDomainMask domain_mask);

blender::bke::AttrDomain BKE_id_attribute_domain(const struct ID *id,
                                                 const struct CustomDataLayer *layer);
int BKE_id_attribute_data_length(struct ID *id, struct CustomDataLayer *layer);
bool BKE_id_attribute_required(const struct ID *id, const char *name);
bool BKE_id_attribute_rename(struct ID *id,
                             const char *old_name,
                             const char *new_name,
                             struct ReportList *reports);

int BKE_id_attributes_length(const struct ID *id,
                             AttrDomainMask domain_mask,
                             eCustomDataMask mask);

struct CustomDataLayer *BKE_id_attributes_active_get(struct ID *id);
void BKE_id_attributes_active_set(struct ID *id, const char *name);
int *BKE_id_attributes_active_index_p(struct ID *id);

CustomData *BKE_id_attributes_iterator_next_domain(struct ID *id, struct CustomDataLayer *layers);
CustomDataLayer *BKE_id_attribute_from_index(struct ID *id,
                                             int lookup_index,
                                             AttrDomainMask domain_mask,
                                             eCustomDataMask layer_mask);

/** Layer is allowed to be nullptr; if so -1 (layer not found) will be returned. */
int BKE_id_attribute_to_index(const struct ID *id,
                              const CustomDataLayer *layer,
                              AttrDomainMask domain_mask,
                              eCustomDataMask layer_mask);

const char *BKE_id_attributes_active_color_name(const struct ID *id);
const char *BKE_id_attributes_default_color_name(const struct ID *id);
void BKE_id_attributes_active_color_set(struct ID *id, const char *name);
void BKE_id_attributes_default_color_set(struct ID *id, const char *name);

const struct CustomDataLayer *BKE_id_attributes_color_find(const struct ID *id, const char *name);
bool BKE_color_attribute_supported(const struct Mesh &mesh, const blender::StringRef name);

std::string BKE_id_attribute_calc_unique_name(const struct ID &id, const blender::StringRef name);

const char *BKE_uv_map_vert_select_name_get(const char *uv_map_name, char *buffer);
const char *BKE_uv_map_edge_select_name_get(const char *uv_map_name, char *buffer);
const char *BKE_uv_map_pin_name_get(const char *uv_map_name, char *buffer);
