/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup bke
 * Implementation of generic geometry attributes management. This is built
 * on top of CustomData, which manages individual domains.
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_curves_types.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_index_range.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"

#include "BKE_attribute.h"
#include "BKE_attribute_access.hh"
#include "BKE_curves.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
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

static CustomData *attribute_customdata_find(ID *id, CustomDataLayer *layer)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    CustomData *customdata = info[domain].customdata;
    if (customdata &&
        ARRAY_HAS_ITEM((CustomDataLayer *)layer, customdata->layers, customdata->totlayer)) {
      return customdata;
    }
  }

  return nullptr;
}

bool BKE_id_attributes_supported(ID *id)
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

bool BKE_id_attribute_rename(ID *id,
                             CustomDataLayer *layer,
                             const char *new_name,
                             ReportList *reports)
{
  if (BKE_id_attribute_required(id, layer)) {
    BLI_assert_msg(0, "Required attribute name is not editable");
    return false;
  }

  CustomData *customdata = attribute_customdata_find(id, layer);
  if (customdata == nullptr) {
    BKE_report(reports, RPT_ERROR, "Attribute is not part of this geometry");
    return false;
  }

  BLI_strncpy_utf8(layer->name, new_name, sizeof(layer->name));
  CustomData_set_layer_unique_name(customdata, layer - customdata->layers);
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

    CustomData *cdata = info[domain].customdata;
    for (int i = 0; i < cdata->totlayer; i++) {
      CustomDataLayer *layer = cdata->layers + i;

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

  BLI_strncpy_utf8(outname, name, MAX_CUSTOMDATA_LAYER_NAME);

  return BLI_uniquename_cb(
      unique_name_cb, &data, nullptr, '.', outname, MAX_CUSTOMDATA_LAYER_NAME);
}

CustomDataLayer *BKE_id_attribute_new(
    ID *id, const char *name, const int type, const AttributeDomain domain, ReportList *reports)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  CustomData *customdata = info[domain].customdata;
  if (customdata == nullptr) {
    BKE_report(reports, RPT_ERROR, "Attribute domain not supported by this geometry type");
    return nullptr;
  }

  char uniquename[MAX_CUSTOMDATA_LAYER_NAME];
  BKE_id_attribute_calc_unique_name(id, name, uniquename);

  switch (GS(id->name)) {
    case ID_ME: {
      Mesh *me = (Mesh *)id;
      BMEditMesh *em = me->edit_mesh;
      if (em != nullptr) {
        BM_data_layer_add_named(em->bm, customdata, type, uniquename);
      }
      else {
        CustomData_add_layer_named(
            customdata, type, CD_DEFAULT, nullptr, info[domain].length, uniquename);
      }
      break;
    }
    default: {
      CustomData_add_layer_named(
          customdata, type, CD_DEFAULT, nullptr, info[domain].length, uniquename);
      break;
    }
  }

  const int index = CustomData_get_named_layer_index(customdata, type, uniquename);
  return (index == -1) ? nullptr : &(customdata->layers[index]);
}

bool BKE_id_attribute_remove(ID *id, CustomDataLayer *layer, ReportList *reports)
{
  CustomData *customdata = attribute_customdata_find(id, layer);
  const int index = (customdata) ?
                        CustomData_get_named_layer_index(customdata, layer->type, layer->name) :
                        -1;

  if (index == -1) {
    BKE_report(reports, RPT_ERROR, "Attribute is not part of this geometry");
    return false;
  }

  if (BKE_id_attribute_required(id, layer)) {
    BKE_report(reports, RPT_ERROR, "Attribute is required and can't be removed");
    return false;
  }

  switch (GS(id->name)) {
    case ID_ME: {
      Mesh *me = (Mesh *)id;
      BMEditMesh *em = me->edit_mesh;
      if (em != nullptr) {
        BM_data_layer_free(em->bm, customdata, layer->type);
      }
      else {
        const int length = BKE_id_attribute_data_length(id, layer);
        CustomData_free_layer(customdata, layer->type, length, index);
      }
      break;
    }
    default: {
      const int length = BKE_id_attribute_data_length(id, layer);
      CustomData_free_layer(customdata, layer->type, length, index);
      break;
    }
  }

  return true;
}

CustomDataLayer *BKE_id_attribute_find(const ID *id,
                                       const char *name,
                                       const int type,
                                       const AttributeDomain domain)
{
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

int BKE_id_attributes_length(const ID *id, AttributeDomainMask domain_mask, CustomDataMask mask)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  int length = 0;

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    CustomData *customdata = info[domain].customdata;

    if (customdata && ((1 << (int)domain) & domain_mask)) {
      length += CustomData_number_of_layers_typemask(customdata, mask);
    }
  }

  return length;
}

AttributeDomain BKE_id_attribute_domain(const ID *id, const CustomDataLayer *layer)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    CustomData *customdata = info[domain].customdata;
    if (customdata &&
        ARRAY_HAS_ITEM((CustomDataLayer *)layer, customdata->layers, customdata->totlayer)) {
      return static_cast<AttributeDomain>(domain);
    }
  }

  BLI_assert_msg(0, "Custom data layer not found in geometry");
  return static_cast<AttributeDomain>(ATTR_DOMAIN_POINT);
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
    }
    default:
      break;
  }

  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    CustomData *customdata = info[domain].customdata;
    if (customdata &&
        ARRAY_HAS_ITEM((CustomDataLayer *)layer, customdata->layers, customdata->totlayer)) {
      return info[domain].length;
    }
  }

  BLI_assert_msg(0, "Custom data layer not found in geometry");
  return 0;
}

bool BKE_id_attribute_required(ID *id, CustomDataLayer *layer)
{
  switch (GS(id->name)) {
    case ID_PT: {
      return BKE_pointcloud_customdata_required((PointCloud *)id, layer);
    }
    case ID_CV: {
      return BKE_curves_customdata_required((Curves *)id, layer);
    }
    default:
      return false;
  }
}

CustomDataLayer *BKE_id_attributes_active_get(ID *id)
{
  int active_index = *BKE_id_attributes_active_index_p(id);
  if (active_index > BKE_id_attributes_length(id, ATTR_DOMAIN_MASK_ALL, CD_MASK_PROP_ALL)) {
    active_index = 0;
  }

  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  int index = 0;

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    CustomData *customdata = info[domain].customdata;
    if (customdata) {
      for (int i = 0; i < customdata->totlayer; i++) {
        CustomDataLayer *layer = &customdata->layers[i];
        if (CD_MASK_PROP_ALL & CD_TYPE_AS_MASK(layer->type)) {
          if (index == active_index) {
            return layer;
          }
          index++;
        }
      }
    }
  }

  return nullptr;
}

void BKE_id_attributes_active_set(ID *id, CustomDataLayer *active_layer)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  int index = 0;

  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    CustomData *customdata = info[domain].customdata;
    if (customdata) {
      for (int i = 0; i < customdata->totlayer; i++) {
        CustomDataLayer *layer = &customdata->layers[i];
        if (layer == active_layer) {
          *BKE_id_attributes_active_index_p(id) = index;
          return;
        }
        if (CD_MASK_PROP_ALL & CD_TYPE_AS_MASK(layer->type)) {
          index++;
        }
      }
    }
  }
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
    if (customdata && customdata->layers && customdata->totlayer) {
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
                                             AttributeDomainMask domain_mask,
                                             CustomDataMask layer_mask)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  int index = 0;
  for (const int domain : IndexRange(ATTR_DOMAIN_NUM)) {
    CustomData *customdata = info[domain].customdata;

    if (!customdata || !((1 << (int)domain) & domain_mask)) {
      continue;
    }

    for (int i = 0; i < customdata->totlayer; i++) {
      if (!(layer_mask & CD_TYPE_AS_MASK(customdata->layers[i].type)) ||
          (customdata->layers[i].flag & CD_FLAG_TEMPORARY)) {
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

/** Get list of domain types but with ATTR_DOMAIN_FACE and
 * ATTR_DOMAIN_CORNER swapped.
 */
static void get_domains_types(AttributeDomain domains[ATTR_DOMAIN_NUM])
{
  for (const int i : IndexRange(ATTR_DOMAIN_NUM)) {
    domains[i] = static_cast<AttributeDomain>(i);
  }

  /* Swap corner and face. */
  SWAP(AttributeDomain, domains[ATTR_DOMAIN_FACE], domains[ATTR_DOMAIN_CORNER]);
}

int BKE_id_attribute_to_index(const ID *id,
                              const CustomDataLayer *layer,
                              AttributeDomainMask domain_mask,
                              CustomDataMask layer_mask)
{
  if (!layer) {
    return -1;
  }

  DomainInfo info[ATTR_DOMAIN_NUM];
  AttributeDomain domains[ATTR_DOMAIN_NUM];
  get_domains_types(domains);
  get_domains(id, info);

  int index = 0;
  for (int i = 0; i < ATTR_DOMAIN_NUM; i++) {
    if (!(domain_mask & (1 << domains[i])) || !info[domains[i]].customdata) {
      continue;
    }

    CustomData *cdata = info[domains[i]].customdata;
    for (int j = 0; j < cdata->totlayer; j++) {
      CustomDataLayer *layer_iter = cdata->layers + j;

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

CustomDataLayer *BKE_id_attribute_subset_active_get(const ID *id,
                                                    int active_flag,
                                                    AttributeDomainMask domain_mask,
                                                    CustomDataMask mask)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  AttributeDomain domains[ATTR_DOMAIN_NUM];

  get_domains_types(domains);
  get_domains(id, info);

  CustomDataLayer *candidate = nullptr;
  for (int i = 0; i < ARRAY_SIZE(domains); i++) {
    if (!((1 << domains[i]) & domain_mask) || !info[domains[i]].customdata) {
      continue;
    }

    CustomData *cdata = info[domains[i]].customdata;

    for (int j = 0; j < cdata->totlayer; j++) {
      CustomDataLayer *layer = cdata->layers + j;

      if (!(CD_TYPE_AS_MASK(layer->type) & mask) || (layer->flag & CD_FLAG_TEMPORARY)) {
        continue;
      }

      if (layer->flag & active_flag) {
        return layer;
      }

      candidate = layer;
    }
  }

  return candidate;
}

void BKE_id_attribute_subset_active_set(ID *id,
                                        CustomDataLayer *layer,
                                        int active_flag,
                                        AttributeDomainMask domain_mask,
                                        CustomDataMask mask)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  AttributeDomain domains[ATTR_DOMAIN_NUM];

  get_domains_types(domains);
  get_domains(id, info);

  for (int i = 0; i < ATTR_DOMAIN_NUM; i++) {
    AttributeDomainMask domain_mask2 = (AttributeDomainMask)(1 << domains[i]);

    if (!(domain_mask2 & domain_mask) || !info[domains[i]].customdata) {
      continue;
    }

    CustomData *cdata = info[domains[i]].customdata;

    for (int j = 0; j < cdata->totlayer; j++) {
      CustomDataLayer *layer_iter = cdata->layers + j;

      if (!(CD_TYPE_AS_MASK(layer_iter->type) & mask) || (layer_iter->flag & CD_FLAG_TEMPORARY)) {
        continue;
      }

      layer_iter->flag &= ~active_flag;
    }
  }

  layer->flag |= active_flag;
}

CustomDataLayer *BKE_id_attributes_active_color_get(const ID *id)
{
  return BKE_id_attribute_subset_active_get(
      id, CD_FLAG_COLOR_ACTIVE, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);
}

void BKE_id_attributes_active_color_set(ID *id, CustomDataLayer *active_layer)
{
  BKE_id_attribute_subset_active_set(
      id, active_layer, CD_FLAG_COLOR_ACTIVE, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);
}

CustomDataLayer *BKE_id_attributes_render_color_get(const ID *id)
{
  return BKE_id_attribute_subset_active_get(
      id, CD_FLAG_COLOR_RENDER, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);
}

void BKE_id_attributes_render_color_set(ID *id, CustomDataLayer *active_layer)
{
  BKE_id_attribute_subset_active_set(
      id, active_layer, CD_FLAG_COLOR_RENDER, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);
}

CustomDataLayer *BKE_id_attributes_color_find(const ID *id, const char *name)
{
  CustomDataLayer *layer = BKE_id_attribute_find(id, name, CD_PROP_COLOR, ATTR_DOMAIN_POINT);
  if (layer == nullptr) {
    layer = BKE_id_attribute_find(id, name, CD_PROP_COLOR, ATTR_DOMAIN_CORNER);
  }
  if (layer == nullptr) {
    layer = BKE_id_attribute_find(id, name, CD_PROP_BYTE_COLOR, ATTR_DOMAIN_POINT);
  }
  if (layer == nullptr) {
    layer = BKE_id_attribute_find(id, name, CD_PROP_BYTE_COLOR, ATTR_DOMAIN_CORNER);
  }
  return layer;
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

      me->edit_mesh = nullptr;

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
