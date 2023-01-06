/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

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

/** #Attribute.domain */
typedef enum eAttrDomain {
  ATTR_DOMAIN_AUTO = -1,    /* Use for nodes to choose automatically based on other data. */
  ATTR_DOMAIN_POINT = 0,    /* Mesh, Curve or Point Cloud Point */
  ATTR_DOMAIN_EDGE = 1,     /* Mesh Edge */
  ATTR_DOMAIN_FACE = 2,     /* Mesh Face */
  ATTR_DOMAIN_CORNER = 3,   /* Mesh Corner */
  ATTR_DOMAIN_CURVE = 4,    /* A single curve in a larger curve data-block */
  ATTR_DOMAIN_INSTANCE = 5, /* Instance */
} eAttrDomain;
#define ATTR_DOMAIN_NUM 6

typedef enum eAttrDomainMask {
  ATTR_DOMAIN_MASK_POINT = (1 << 0),
  ATTR_DOMAIN_MASK_EDGE = (1 << 1),
  ATTR_DOMAIN_MASK_FACE = (1 << 2),
  ATTR_DOMAIN_MASK_CORNER = (1 << 3),
  ATTR_DOMAIN_MASK_CURVE = (1 << 4),
  ATTR_DOMAIN_MASK_ALL = (1 << 5) - 1
} eAttrDomainMask;

#define ATTR_DOMAIN_AS_MASK(domain) ((eAttrDomainMask)((1 << (int)(domain))))

/* All domains that support color attributes. */
#define ATTR_DOMAIN_MASK_COLOR \
  ((eAttrDomainMask)((ATTR_DOMAIN_MASK_POINT | ATTR_DOMAIN_MASK_CORNER)))

/* Attributes. */

bool BKE_id_attributes_supported(const struct ID *id);
bool BKE_attribute_allow_procedural_access(const char *attribute_name);

/**
 * Create a new attribute layer.
 */
struct CustomDataLayer *BKE_id_attribute_new(
    struct ID *id, const char *name, int type, eAttrDomain domain, struct ReportList *reports);
bool BKE_id_attribute_remove(struct ID *id, const char *name, struct ReportList *reports);

/**
 * Creates a duplicate attribute layer.
 */
struct CustomDataLayer *BKE_id_attribute_duplicate(struct ID *id,
                                                   const char *name,
                                                   struct ReportList *reports);

struct CustomDataLayer *BKE_id_attribute_find(const struct ID *id,
                                              const char *name,
                                              int type,
                                              eAttrDomain domain);

struct CustomDataLayer *BKE_id_attribute_search(struct ID *id,
                                                const char *name,
                                                eCustomDataMask type,
                                                eAttrDomainMask domain_mask);

eAttrDomain BKE_id_attribute_domain(const struct ID *id, const struct CustomDataLayer *layer);
int BKE_id_attribute_data_length(struct ID *id, struct CustomDataLayer *layer);
bool BKE_id_attribute_required(const struct ID *id, const char *name);
bool BKE_id_attribute_rename(struct ID *id,
                             const char *old_name,
                             const char *new_name,
                             struct ReportList *reports);

int BKE_id_attributes_length(const struct ID *id,
                             eAttrDomainMask domain_mask,
                             eCustomDataMask mask);

struct CustomDataLayer *BKE_id_attributes_active_get(struct ID *id);
void BKE_id_attributes_active_set(struct ID *id, const char *name);
int *BKE_id_attributes_active_index_p(struct ID *id);

CustomData *BKE_id_attributes_iterator_next_domain(struct ID *id, struct CustomDataLayer *layers);
CustomDataLayer *BKE_id_attribute_from_index(struct ID *id,
                                             int lookup_index,
                                             eAttrDomainMask domain_mask,
                                             eCustomDataMask layer_mask);

/** Layer is allowed to be nullptr; if so -1 (layer not found) will be returned. */
int BKE_id_attribute_to_index(const struct ID *id,
                              const CustomDataLayer *layer,
                              eAttrDomainMask domain_mask,
                              eCustomDataMask layer_mask);

/**
 * Sets up a temporary ID with arbitrary CustomData domains. `r_id` will
 * be zero initialized with ID type id_type and any non-nullptr
 * CustomData parameter will be copied into the appropriate struct members.
 *
 * \param r_id: Pointer to storage sufficient for ID type-code id_type.
 */
void BKE_id_attribute_copy_domains_temp(short id_type,
                                        const struct CustomData *vdata,
                                        const struct CustomData *edata,
                                        const struct CustomData *ldata,
                                        const struct CustomData *pdata,
                                        const struct CustomData *cdata,
                                        struct ID *r_id);

const char *BKE_id_attributes_active_color_name(const struct ID *id);
const char *BKE_id_attributes_default_color_name(const struct ID *id);

struct CustomDataLayer *BKE_id_attributes_active_color_get(const struct ID *id);
void BKE_id_attributes_active_color_set(struct ID *id, const char *name);
struct CustomDataLayer *BKE_id_attributes_default_color_get(const struct ID *id);
void BKE_id_attributes_default_color_set(struct ID *id, const char *name);
struct CustomDataLayer *BKE_id_attributes_color_find(const struct ID *id, const char *name);

bool BKE_id_attribute_calc_unique_name(struct ID *id, const char *name, char *outname);

#ifdef __cplusplus
}
#endif
