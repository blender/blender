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
 *
 * Implementation of generic geometry attributes management. This is built
 * on top of CustomData, which manages individual domains.
 */

/** \file
 * \ingroup bke
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_customdata_types.h"
#include "DNA_hair_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BKE_attribute.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_hair.h"
#include "BKE_pointcloud.h"
#include "BKE_report.h"

#include "RNA_access.h"

typedef struct DomainInfo {
  CustomData *customdata;
  int length;
} DomainInfo;

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
      if (em != NULL) {
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
    case ID_HA: {
      Hair *hair = (Hair *)id;
      info[ATTR_DOMAIN_POINT].customdata = &hair->pdata;
      info[ATTR_DOMAIN_POINT].length = hair->totpoint;
      info[ATTR_DOMAIN_CURVE].customdata = &hair->cdata;
      info[ATTR_DOMAIN_CURVE].length = hair->totcurve;
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

  for (AttributeDomain domain = 0; domain < ATTR_DOMAIN_NUM; domain++) {
    CustomData *customdata = info[domain].customdata;
    if (customdata && ARRAY_HAS_ITEM(layer, customdata->layers, customdata->totlayer)) {
      return customdata;
    }
  }

  return NULL;
}

bool BKE_id_attributes_supported(struct ID *id)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);
  for (AttributeDomain domain = 0; domain < ATTR_DOMAIN_NUM; domain++) {
    if (info[domain].customdata) {
      return true;
    }
  }
  return false;
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
  if (customdata == NULL) {
    BKE_report(reports, RPT_ERROR, "Attribute is not part of this geometry");
    return false;
  }

  BLI_strncpy_utf8(layer->name, new_name, sizeof(layer->name));
  CustomData_set_layer_unique_name(customdata, layer - customdata->layers);
  return true;
}

typedef struct AttrUniqueData {
  ID *id;
  CustomDataMask mask;
} AttrUniqueData;

static bool unique_name_cb(void *arg, const char *name)
{
  AttrUniqueData *data = (AttrUniqueData *)arg;

  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(data->id, info);

  for (AttributeDomain domain = ATTR_DOMAIN_POINT; domain < ATTR_DOMAIN_NUM; domain++) {
    if (!info[domain].customdata) {
      continue;
    }

    CustomData *cdata = info[domain].customdata;
    for (int i = 0; i < cdata->totlayer; i++) {
      CustomDataLayer *layer = cdata->layers + i;

      if ((CD_TYPE_AS_MASK(layer->type) & data->mask) && STREQ(layer->name, name)) {
        return true;
      }
    }
  }

  return false;
}

bool BKE_id_attribute_find_unique_name(ID *id,
                                       const char *name,
                                       char *outname,
                                       CustomDataMask mask)
{
  AttrUniqueData data = {.id = id, .mask = mask};

  BLI_strncpy_utf8(outname, name, MAX_CUSTOMDATA_LAYER_NAME);

  return BLI_uniquename_cb(unique_name_cb, &data, NULL, '.', outname, MAX_CUSTOMDATA_LAYER_NAME);
}

CustomDataLayer *BKE_id_attribute_new(ID *id,
                                      const char *name,
                                      const int type,
                                      CustomDataMask list_mask,
                                      const AttributeDomain domain,
                                      ReportList *reports)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  CustomData *customdata = info[domain].customdata;
  if (customdata == NULL) {
    BKE_report(reports, RPT_ERROR, "Attribute domain not supported by this geometry type");
    return NULL;
  }

  char uniquename[MAX_CUSTOMDATA_LAYER_NAME];

  BKE_id_attribute_find_unique_name(id, name, uniquename, list_mask);

  switch (GS(id->name)) {
    case ID_ME: {
      Mesh *me = (Mesh *)id;
      BMEditMesh *em = me->edit_mesh;
      if (em != NULL) {
        BM_data_layer_add_named(em->bm, customdata, type, uniquename);
      }
      else {
        CustomData_add_layer_named(
            customdata, type, CD_DEFAULT, NULL, info[domain].length, uniquename);
      }
      break;
    }
    default: {
      CustomData_add_layer_named(
          customdata, type, CD_DEFAULT, NULL, info[domain].length, uniquename);
      break;
    }
  }

  const int index = CustomData_get_named_layer_index(customdata, type, uniquename);
  return (index == -1) ? NULL : &(customdata->layers[index]);
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
      if (em != NULL) {
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
  if (customdata == NULL) {
    return NULL;
  }

  for (int i = 0; i < customdata->totlayer; i++) {
    CustomDataLayer *layer = &customdata->layers[i];
    if (layer->type == type && STREQ(layer->name, name)) {
      return layer;
    }
  }

  return NULL;
}

CustomDataLayer *BKE_id_attribute_from_index(const ID *id,
                                             int lookup_index,
                                             const AttributeDomainMask domain_mask)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  int index = 0;
  for (AttributeDomain domain = 0; domain < ATTR_DOMAIN_NUM; domain++) {
    CustomData *customdata = info[domain].customdata;

    if (!customdata || !((1 << (int)domain) & domain_mask)) {
      continue;
    }

    for (int i = 0; i < customdata->totlayer; i++) {
      if (CD_MASK_PROP_ALL & CD_TYPE_AS_MASK(customdata->layers[i].type)) {
        if (index == lookup_index) {
          return customdata->layers + i;
        }

        index++;
      }
    }
  }

  return NULL;
}

int BKE_id_attributes_length(const ID *id,
                             const AttributeDomainMask domain_mask,
                             const CustomDataMask mask)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  int length = 0;

  for (AttributeDomain domain = 0; domain < ATTR_DOMAIN_NUM; domain++) {
    CustomData *customdata = info[domain].customdata;

    if (customdata && ((1 << (int)domain) & domain_mask)) {
      length += CustomData_number_of_layers_typemask(customdata, mask);
    }
  }

  return length;
}

AttributeDomain BKE_id_attribute_domain(ID *id, CustomDataLayer *layer)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  for (AttributeDomain domain = 0; domain < ATTR_DOMAIN_NUM; domain++) {
    CustomData *customdata = info[domain].customdata;
    if (customdata && ARRAY_HAS_ITEM(layer, customdata->layers, customdata->totlayer)) {
      return domain;
    }
  }

  BLI_assert_msg(0, "Custom data layer not found in geometry");
  return ATTR_DOMAIN_NUM;
}

int BKE_id_attribute_data_length(ID *id, CustomDataLayer *layer)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  for (AttributeDomain domain = 0; domain < ATTR_DOMAIN_NUM; domain++) {
    CustomData *customdata = info[domain].customdata;
    if (customdata && ARRAY_HAS_ITEM(layer, customdata->layers, customdata->totlayer)) {
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
    case ID_HA: {
      return BKE_hair_customdata_required((Hair *)id, layer);
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

  for (AttributeDomain domain = 0; domain < ATTR_DOMAIN_NUM; domain++) {
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

  return NULL;
}

void BKE_id_attributes_active_set(ID *id, CustomDataLayer *active_layer)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  int index = 0;

  for (AttributeDomain domain = 0; domain < ATTR_DOMAIN_NUM; domain++) {
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
    case ID_HA: {
      return &((Hair *)id)->attributes_active_index;
    }
    default:
      return NULL;
  }
}

CustomDataLayer *BKE_id_attributes_active_color_get(ID *id)
{
  AttributeRef *ref = BKE_id_attributes_active_color_ref_p(id);

  if (!ref) {
    fprintf(stderr, "%s: vertex colors not supported for this type\n", __func__);
    return NULL;
  }

  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  int idx = CustomData_get_named_layer_index(info[ref->domain].customdata, ref->type, ref->name);

  return idx != -1 ? info[ref->domain].customdata->layers + idx : NULL;
}

void BKE_id_attributes_active_color_set(ID *id, CustomDataLayer *active_layer)
{
  AttributeRef *ref = BKE_id_attributes_active_color_ref_p(id);

  if (!ref || !ref->type) {
    fprintf(stderr, "%s: vertex colors not supported for this type\n", __func__);
    return;
  }

  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  if (!active_layer || !ELEM(active_layer->type, CD_PROP_COLOR, CD_MLOOPCOL)) {
    fprintf(stderr,
            "bad active color layer %p; type was %d\n",
            active_layer,
            active_layer ? active_layer->type : -1);
    return;
  }

  for (AttributeDomain domain = 0; domain < ATTR_DOMAIN_NUM; domain++) {
    CustomData *customdata = info[domain].customdata;

    if (customdata) {
      for (int i = 0; i < customdata->totlayer; i++) {
        CustomDataLayer *layer = &customdata->layers[i];

        if (layer == active_layer && ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CORNER)) {
          ref->type = layer->type;
          ref->domain = domain;
          BLI_strncpy_utf8(ref->name, layer->name, MAX_CUSTOMDATA_LAYER_NAME);
          return;
        }
      }
    }
  }
}

AttributeRef *BKE_id_attributes_active_color_ref_p(ID *id)
{
  switch (GS(id->name)) {
    case ID_ME: {
      return &((Mesh *)id)->attr_color_active;
    }
    default:
      return NULL;
  }
}

CustomDataLayer *BKE_id_attributes_render_color_get(ID *id)
{
  AttributeRef *ref = BKE_id_attributes_render_color_ref_p(id);

  if (!ref || !ref->type) {
    fprintf(stderr, "%s: vertex colors not supported for this type\n", __func__);
    return NULL;
  }

  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  int idx = CustomData_get_named_layer_index(info[ref->domain].customdata, ref->type, ref->name);

  return idx != -1 ? info[ref->domain].customdata->layers + idx : NULL;
}

void BKE_id_attributes_render_color_set(ID *id, CustomDataLayer *active_layer)
{
  AttributeRef *ref = BKE_id_attributes_render_color_ref_p(id);

  if (!ref) {
    fprintf(stderr, "%s: vertex colors not supported for this type\n", __func__);
    return;
  }

  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  if (!active_layer || !ELEM(active_layer->type, CD_PROP_COLOR, CD_MLOOPCOL)) {
    fprintf(stderr,
            "bad active color layer %p; type was %d\n",
            active_layer,
            active_layer ? active_layer->type : -1);
    return;
  }

  for (AttributeDomain domain = 0; domain < ATTR_DOMAIN_NUM; domain++) {
    CustomData *customdata = info[domain].customdata;

    if (customdata) {
      for (int i = 0; i < customdata->totlayer; i++) {
        CustomDataLayer *layer = &customdata->layers[i];

        if (layer == active_layer && ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CORNER)) {
          ref->type = layer->type;
          ref->domain = domain;
          BLI_strncpy_utf8(ref->name, layer->name, MAX_CUSTOMDATA_LAYER_NAME);
          return;
        }
      }
    }
  }
}

int BKE_id_attribute_index_from_ref(ID *id,
                                    AttributeRef *ref,
                                    AttributeDomainMask domain_mask,
                                    CustomDataMask type_filter)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  int index = 0;

  for (AttributeDomain domain = ATTR_DOMAIN_POINT; domain < ATTR_DOMAIN_NUM; domain++) {
    CustomData *data = info[domain].customdata;

    if (!((1 << (int)domain) & domain_mask) || !data) {
      continue;
    }

    for (int i = 0; i < data->totlayer; i++) {
      CustomDataLayer *layer = data->layers + i;

      if (layer->type == ref->type && STREQ(layer->name, ref->name)) {
        return index;
      }

      if (CD_TYPE_AS_MASK(layer->type) & type_filter) {
        index++;
      }
    }
  }

  return -1;
}

bool BKE_id_attribute_ref_from_index(ID *id,
                                     int attr_index,
                                     AttributeDomainMask domain_mask,
                                     CustomDataMask type_filter,
                                     AttributeRef *r_ref)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  int index = 0;

  for (AttributeDomain domain = ATTR_DOMAIN_POINT; domain < ATTR_DOMAIN_NUM; domain++) {
    CustomData *data = info[domain].customdata;

    if (!data || !((1 << (int)domain) & domain_mask)) {
      continue;
    }

    for (int i = 0; i < data->totlayer; i++) {
      CustomDataLayer *layer = data->layers + i;

      if (CD_TYPE_AS_MASK(layer->type) & type_filter) {
        if (index == attr_index) {
          r_ref->domain = domain;
          r_ref->type = layer->type;
          BLI_strncpy_utf8(r_ref->name, layer->name, MAX_CUSTOMDATA_LAYER_NAME);

          return true;
        }

        index++;
      }
    }
  }

  return false;
}

AttributeRef *BKE_id_attributes_render_color_ref_p(ID *id)
{
  switch (GS(id->name)) {
    case ID_ME: {
      return &((Mesh *)id)->attr_color_render;
    }
    default:
      return NULL;
  }
}

CustomData *BKE_id_attributes_iterator_next_domain(ID *id, CustomDataLayer *layers)
{
  DomainInfo info[ATTR_DOMAIN_NUM];
  get_domains(id, info);

  bool use_next = (layers == NULL);

  for (AttributeDomain domain = 0; domain < ATTR_DOMAIN_NUM; domain++) {
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

  return NULL;
}
