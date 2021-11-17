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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 * \brief Generic geometry attributes built on CustomData.
 */

#pragma once

#include "BLI_sys_types.h"

#include "BKE_customdata.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CustomData;
struct CustomDataLayer;
struct ID;
struct ReportList;
struct AttributeRef;

/* Attribute.domain */
/**
 * \warning Careful when changing existing items.
 * Arrays may be initialized from this (e.g. #DATASET_layout_hierarchy).
 */
typedef enum AttributeDomain {
  ATTR_DOMAIN_AUTO = -1,  /* Use for nodes to choose automatically based on other data. */
  ATTR_DOMAIN_POINT = 0,  /* Mesh, Hair or PointCloud Point */
  ATTR_DOMAIN_EDGE = 1,   /* Mesh Edge */
  ATTR_DOMAIN_FACE = 2,   /* Mesh Face */
  ATTR_DOMAIN_CORNER = 3, /* Mesh Corner */
  ATTR_DOMAIN_CURVE = 4,  /* Hair Curve */

  ATTR_DOMAIN_NUM
} AttributeDomain;

typedef enum {
  ATTR_DOMAIN_MASK_POINT = (1 << 0),
  ATTR_DOMAIN_MASK_EDGE = (1 << 1),
  ATTR_DOMAIN_MASK_FACE = (1 << 2),
  ATTR_DOMAIN_MASK_CORNER = (1 << 3),
  ATTR_DOMAIN_MASK_CURVE = (1 << 4),
  ATTR_DOMAIN_MASK_ALL = (1 << 5) - 1
} AttributeDomainMask;

/* Attributes */

bool BKE_id_attributes_supported(struct ID *id);

struct CustomDataLayer *BKE_id_attribute_new(struct ID *id,
                                             const char *name,
                                             const int type,
                                             CustomDataMask list_mask,
                                             const AttributeDomain domain,
                                             struct ReportList *reports);
bool BKE_id_attribute_remove(struct ID *id,
                             struct CustomDataLayer *layer,
                             struct ReportList *reports);

struct CustomDataLayer *BKE_id_attribute_find(const struct ID *id,
                                              const char *name,
                                              const int type,
                                              const AttributeDomain domain);

AttributeDomain BKE_id_attribute_domain(struct ID *id, struct CustomDataLayer *layer);
int BKE_id_attribute_data_length(struct ID *id, struct CustomDataLayer *layer);
bool BKE_id_attribute_required(struct ID *id, struct CustomDataLayer *layer);
bool BKE_id_attribute_rename(struct ID *id,
                             struct CustomDataLayer *layer,
                             const char *new_name,
                             struct ReportList *reports);

int BKE_id_attributes_length(const struct ID *id,
                             const AttributeDomainMask domain_mask,
                             const CustomDataMask mask);

struct CustomDataLayer *BKE_id_attributes_active_get(struct ID *id);
void BKE_id_attributes_active_set(struct ID *id, struct CustomDataLayer *layer);
int *BKE_id_attributes_active_index_p(struct ID *id);

CustomData *BKE_id_attributes_iterator_next_domain(struct ID *id, struct CustomDataLayer *layers);
CustomDataLayer *BKE_id_attribute_from_index(const struct ID *id, int lookup_index);

struct AttributeRef *BKE_id_attributes_active_color_ref_p(struct ID *id);
void BKE_id_attributes_active_color_set(struct ID *id, struct CustomDataLayer *active_layer);
struct CustomDataLayer *BKE_id_attributes_active_color_get(struct ID *id);

struct AttributeRef *BKE_id_attributes_render_color_ref_p(struct ID *id);
void BKE_id_attributes_render_color_set(struct ID *id, struct CustomDataLayer *active_layer);
CustomDataLayer *BKE_id_attributes_render_color_get(struct ID *id);

bool BKE_id_attribute_find_unique_name(struct ID *id,
                                       const char *name,
                                       char *outname,
                                       CustomDataMask mask);

int BKE_id_attribute_index_from_ref(struct ID *id,
                                    struct AttributeRef *ref,
                                    AttributeDomainMask domain_mask,
                                    CustomDataMask type_filter);

bool BKE_id_attribute_ref_from_index(struct ID *id,
                                     int attr_index,
                                     AttributeDomainMask domain_mask,
                                     CustomDataMask type_filter,
                                     struct AttributeRef *r_ref);

#ifdef __cplusplus
}
#endif
