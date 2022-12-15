/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <stdlib.h>

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_curves_types.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_attribute.h"
#include "BKE_customdata.h"

#include "BLT_translation.h"

#include "WM_types.h"

const EnumPropertyItem rna_enum_attribute_type_items[] = {
    {CD_PROP_FLOAT, "FLOAT", 0, "Float", "Floating-point value"},
    {CD_PROP_INT32, "INT", 0, "Integer", "32-bit integer"},
    {CD_PROP_FLOAT3, "FLOAT_VECTOR", 0, "Vector", "3D vector with floating-point values"},
    {CD_PROP_COLOR, "FLOAT_COLOR", 0, "Color", "RGBA color with 32-bit floating-point values"},
    {CD_PROP_BYTE_COLOR,
     "BYTE_COLOR",
     0,
     "Byte Color",
     "RGBA color with 8-bit positive integer values"},
    {CD_PROP_STRING, "STRING", 0, "String", "Text string"},
    {CD_PROP_BOOL, "BOOLEAN", 0, "Boolean", "True or false"},
    {CD_PROP_FLOAT2, "FLOAT2", 0, "2D Vector", "2D vector with floating-point values"},
    {CD_PROP_INT8, "INT8", 0, "8-Bit Integer", "Smaller integer with a range from -128 to 127"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_color_attribute_type_items[] = {
    {CD_PROP_COLOR, "FLOAT_COLOR", 0, "Color", "RGBA color 32-bit floating-point values"},
    {CD_PROP_BYTE_COLOR,
     "BYTE_COLOR",
     0,
     "Byte Color",
     "RGBA color with 8-bit positive integer values"},
    {0, NULL, 0, NULL, NULL}};

const EnumPropertyItem rna_enum_attribute_type_with_auto_items[] = {
    {CD_AUTO_FROM_NAME, "AUTO", 0, "Auto", ""},
    {CD_PROP_FLOAT, "FLOAT", 0, "Float", "Floating-point value"},
    {CD_PROP_INT32, "INT", 0, "Integer", "32-bit integer"},
    {CD_PROP_FLOAT3, "FLOAT_VECTOR", 0, "Vector", "3D vector with floating-point values"},
    {CD_PROP_COLOR, "FLOAT_COLOR", 0, "Color", "RGBA color with 32-bit floating-point values"},
    {CD_PROP_BYTE_COLOR,
     "BYTE_COLOR",
     0,
     "Byte Color",
     "RGBA color with 8-bit positive integer values"},
    {CD_PROP_STRING, "STRING", 0, "String", "Text string"},
    {CD_PROP_BOOL, "BOOLEAN", 0, "Boolean", "True or false"},
    {CD_PROP_FLOAT2, "FLOAT2", 0, "2D Vector", "2D vector with floating-point values"},
    {CD_PROP_INT8, "INT8", 0, "8-Bit Integer", "Smaller integer with a range from -128 to 127"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_attribute_domain_items[] = {
    /* Not implement yet */
    // {ATTR_DOMAIN_GEOMETRY, "GEOMETRY", 0, "Geometry", "Attribute on (whole) geometry"},
    {ATTR_DOMAIN_POINT, "POINT", 0, "Point", "Attribute on point"},
    {ATTR_DOMAIN_EDGE, "EDGE", 0, "Edge", "Attribute on mesh edge"},
    {ATTR_DOMAIN_FACE, "FACE", 0, "Face", "Attribute on mesh faces"},
    {ATTR_DOMAIN_CORNER, "CORNER", 0, "Face Corner", "Attribute on mesh face corner"},
    /* Not implement yet */
    // {ATTR_DOMAIN_GRIDS, "GRIDS", 0, "Grids", "Attribute on mesh multires grids"},
    {ATTR_DOMAIN_CURVE, "CURVE", 0, "Spline", "Attribute on spline"},
    {ATTR_DOMAIN_INSTANCE, "INSTANCE", 0, "Instance", "Attribute on instance"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_attribute_domain_only_mesh_items[] = {
    {ATTR_DOMAIN_POINT, "POINT", 0, "Point", "Attribute on point"},
    {ATTR_DOMAIN_EDGE, "EDGE", 0, "Edge", "Attribute on mesh edge"},
    {ATTR_DOMAIN_FACE, "FACE", 0, "Face", "Attribute on mesh faces"},
    {ATTR_DOMAIN_CORNER, "CORNER", 0, "Face Corner", "Attribute on mesh face corner"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_attribute_domain_without_corner_items[] = {
    {ATTR_DOMAIN_POINT, "POINT", 0, "Point", "Attribute on point"},
    {ATTR_DOMAIN_EDGE, "EDGE", 0, "Edge", "Attribute on mesh edge"},
    {ATTR_DOMAIN_FACE, "FACE", 0, "Face", "Attribute on mesh faces"},
    {ATTR_DOMAIN_CURVE, "CURVE", 0, "Spline", "Attribute on spline"},
    {ATTR_DOMAIN_INSTANCE, "INSTANCE", 0, "Instance", "Attribute on instance"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_attribute_domain_with_auto_items[] = {
    {ATTR_DOMAIN_AUTO, "AUTO", 0, "Auto", ""},
    {ATTR_DOMAIN_POINT, "POINT", 0, "Point", "Attribute on point"},
    {ATTR_DOMAIN_EDGE, "EDGE", 0, "Edge", "Attribute on mesh edge"},
    {ATTR_DOMAIN_FACE, "FACE", 0, "Face", "Attribute on mesh faces"},
    {ATTR_DOMAIN_CORNER, "CORNER", 0, "Face Corner", "Attribute on mesh face corner"},
    {ATTR_DOMAIN_CURVE, "CURVE", 0, "Spline", "Attribute on spline"},
    {ATTR_DOMAIN_INSTANCE, "INSTANCE", 0, "Instance", "Attribute on instance"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_color_attribute_domain_items[] = {
    {ATTR_DOMAIN_POINT, "POINT", 0, "Vertex", ""},
    {ATTR_DOMAIN_CORNER, "CORNER", 0, "Face Corner", ""},
    {0, NULL, 0, NULL, NULL}};

const EnumPropertyItem rna_enum_attribute_curves_domain_items[] = {
    {ATTR_DOMAIN_POINT, "POINT", 0, "Control Point", ""},
    {ATTR_DOMAIN_CURVE, "CURVE", 0, "Curve", ""},
    {0, NULL, 0, NULL, NULL}};

#ifdef RNA_RUNTIME

#  include "BLI_math.h"

#  include "DEG_depsgraph.h"

#  include "BLT_translation.h"

#  include "WM_api.h"

/* Attribute */

static char *rna_Attribute_path(const PointerRNA *ptr)
{
  const CustomDataLayer *layer = ptr->data;
  return BLI_sprintfN("attributes['%s']", layer->name);
}

static StructRNA *srna_by_custom_data_layer_type(const eCustomDataType type)
{
  switch (type) {
    case CD_PROP_FLOAT:
      return &RNA_FloatAttribute;
    case CD_PROP_INT32:
      return &RNA_IntAttribute;
    case CD_PROP_FLOAT3:
      return &RNA_FloatVectorAttribute;
    case CD_PROP_COLOR:
      return &RNA_FloatColorAttribute;
    case CD_PROP_BYTE_COLOR:
      return &RNA_ByteColorAttribute;
    case CD_PROP_STRING:
      return &RNA_StringAttribute;
    case CD_PROP_BOOL:
      return &RNA_BoolAttribute;
    case CD_PROP_FLOAT2:
      return &RNA_Float2Attribute;
    case CD_PROP_INT8:
      return &RNA_ByteIntAttribute;
    default:
      return NULL;
  }
}

static StructRNA *rna_Attribute_refine(PointerRNA *ptr)
{
  CustomDataLayer *layer = ptr->data;
  return srna_by_custom_data_layer_type(layer->type);
}

static void rna_Attribute_name_set(PointerRNA *ptr, const char *value)
{
  const CustomDataLayer *layer = (const CustomDataLayer *)ptr->data;
  BKE_id_attribute_rename(ptr->owner_id, layer->name, value, NULL);
}

static int rna_Attribute_name_editable(PointerRNA *ptr, const char **r_info)
{
  CustomDataLayer *layer = ptr->data;
  if (BKE_id_attribute_required(ptr->owner_id, layer->name)) {
    *r_info = N_("Cannot modify name of required geometry attribute");
    return false;
  }

  return true;
}

static int rna_Attribute_type_get(PointerRNA *ptr)
{
  CustomDataLayer *layer = ptr->data;
  return layer->type;
}

const EnumPropertyItem *rna_enum_attribute_domain_itemf(ID *id,
                                                        bool include_instances,
                                                        bool *r_free)
{
  EnumPropertyItem *item = NULL;
  const EnumPropertyItem *domain_item = NULL;
  const ID_Type id_type = GS(id->name);
  int totitem = 0, a;

  static EnumPropertyItem mesh_vertex_domain_item = {
      ATTR_DOMAIN_POINT, "POINT", 0, N_("Vertex"), N_("Attribute per point/vertex")};

  for (a = 0; rna_enum_attribute_domain_items[a].identifier; a++) {
    domain_item = &rna_enum_attribute_domain_items[a];

    if (id_type == ID_PT && !ELEM(domain_item->value, ATTR_DOMAIN_POINT)) {
      continue;
    }
    if (id_type == ID_CV && !ELEM(domain_item->value, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CURVE)) {
      continue;
    }
    if (id_type == ID_ME && ELEM(domain_item->value, ATTR_DOMAIN_CURVE)) {
      continue;
    }
    if (!include_instances && domain_item->value == ATTR_DOMAIN_INSTANCE) {
      continue;
    }

    if (domain_item->value == ATTR_DOMAIN_POINT && id_type == ID_ME) {
      RNA_enum_item_add(&item, &totitem, &mesh_vertex_domain_item);
    }
    else {
      RNA_enum_item_add(&item, &totitem, domain_item);
    }
  }
  RNA_enum_item_end(&item, &totitem);

  *r_free = true;
  return item;
}

static const EnumPropertyItem *rna_Attribute_domain_itemf(bContext *UNUSED(C),
                                                          PointerRNA *ptr,
                                                          PropertyRNA *UNUSED(prop),
                                                          bool *r_free)
{
  return rna_enum_attribute_domain_itemf(ptr->owner_id, true, r_free);
}

static int rna_Attribute_domain_get(PointerRNA *ptr)
{
  return BKE_id_attribute_domain(ptr->owner_id, ptr->data);
}

static bool rna_Attribute_is_internal_get(PointerRNA *ptr)
{
  const CustomDataLayer *layer = (const CustomDataLayer *)ptr->data;
  return !BKE_attribute_allow_procedural_access(layer->name);
}

static void rna_Attribute_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;

  int length = BKE_id_attribute_data_length(id, layer);
  size_t struct_size;

  switch (layer->type) {
    case CD_PROP_FLOAT:
      struct_size = sizeof(MFloatProperty);
      break;
    case CD_PROP_INT32:
      struct_size = sizeof(MIntProperty);
      break;
    case CD_PROP_FLOAT3:
      struct_size = sizeof(float[3]);
      break;
    case CD_PROP_COLOR:
      struct_size = sizeof(MPropCol);
      break;
    case CD_PROP_BYTE_COLOR:
      struct_size = sizeof(MLoopCol);
      break;
    case CD_PROP_STRING:
      struct_size = sizeof(MStringProperty);
      break;
    case CD_PROP_BOOL:
      struct_size = sizeof(MBoolProperty);
      break;
    case CD_PROP_FLOAT2:
      struct_size = sizeof(float[2]);
      break;
    case CD_PROP_INT8:
      struct_size = sizeof(int8_t);
      break;
    default:
      struct_size = 0;
      length = 0;
      break;
  }

  rna_iterator_array_begin(iter, layer->data, struct_size, length, 0, NULL);
}

static int rna_Attribute_data_length(PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
  return BKE_id_attribute_data_length(id, layer);
}

static void rna_Attribute_update_data(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  /* cheating way for importers to avoid slow updates */
  if (id->us > 0) {
    DEG_id_tag_update(id, 0);
    WM_main_add_notifier(NC_GEOM | ND_DATA, id);
  }
}

/* Color Attribute */

static void rna_ByteColorAttributeValue_color_get(PointerRNA *ptr, float *values)
{
  MLoopCol *mlcol = (MLoopCol *)ptr->data;
  srgb_to_linearrgb_uchar4(values, &mlcol->r);
}

static void rna_ByteColorAttributeValue_color_set(PointerRNA *ptr, const float *values)
{
  MLoopCol *mlcol = (MLoopCol *)ptr->data;
  linearrgb_to_srgb_uchar4(&mlcol->r, values);
}

static void rna_ByteColorAttributeValue_color_srgb_get(PointerRNA *ptr, float *values)
{
  MLoopCol *col = (MLoopCol *)ptr->data;
  values[0] = col->r / 255.0f;
  values[1] = col->g / 255.0f;
  values[2] = col->b / 255.0f;
  values[3] = col->a / 255.0f;
}

static void rna_ByteColorAttributeValue_color_srgb_set(PointerRNA *ptr, const float *values)
{
  MLoopCol *col = (MLoopCol *)ptr->data;
  col->r = round_fl_to_uchar_clamp(values[0] * 255.0f);
  col->g = round_fl_to_uchar_clamp(values[1] * 255.0f);
  col->b = round_fl_to_uchar_clamp(values[2] * 255.0f);
  col->a = round_fl_to_uchar_clamp(values[3] * 255.0f);
}

static void rna_FloatColorAttributeValue_color_srgb_get(PointerRNA *ptr, float *values)
{
  MPropCol *col = (MPropCol *)ptr->data;
  linearrgb_to_srgb_v4(values, col->color);
}

static void rna_FloatColorAttributeValue_color_srgb_set(PointerRNA *ptr, const float *values)
{
  MPropCol *col = (MPropCol *)ptr->data;
  srgb_to_linearrgb_v4(col->color, values);
}

/* Int8 Attribute. */

static int rna_ByteIntAttributeValue_get(PointerRNA *ptr)
{
  int8_t *value = (int8_t *)ptr->data;
  return (int)(*value);
}

static void rna_ByteIntAttributeValue_set(PointerRNA *ptr, const int new_value)
{
  int8_t *value = (int8_t *)ptr->data;
  if (new_value > INT8_MAX) {
    *value = INT8_MAX;
  }
  else if (new_value < INT8_MIN) {
    *value = INT8_MIN;
  }
  else {
    *value = (int8_t)new_value;
  }
}

/* Attribute Group */

static PointerRNA rna_AttributeGroup_new(
    ID *id, ReportList *reports, const char *name, const int type, const int domain)
{
  CustomDataLayer *layer = BKE_id_attribute_new(id, name, type, domain, reports);
  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);

  PointerRNA ptr;
  RNA_pointer_create(id, &RNA_Attribute, layer, &ptr);
  return ptr;
}

static void rna_AttributeGroup_remove(ID *id, ReportList *reports, PointerRNA *attribute_ptr)
{
  const CustomDataLayer *layer = (const CustomDataLayer *)attribute_ptr->data;
  BKE_id_attribute_remove(id, layer->name, reports);
  RNA_POINTER_INVALIDATE(attribute_ptr);

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);
}

static int rna_Attributes_layer_skip(CollectionPropertyIterator *UNUSED(iter), void *data)
{
  CustomDataLayer *layer = (CustomDataLayer *)data;
  return !(CD_TYPE_AS_MASK(layer->type) & CD_MASK_PROP_ALL);
}

static int rna_Attributes_noncolor_layer_skip(CollectionPropertyIterator *iter, void *data)
{
  CustomDataLayer *layer = (CustomDataLayer *)data;

  /* Check valid domain here, too, keep in line with rna_AttributeGroup_color_length(). */
  ID *id = iter->parent.owner_id;
  eAttrDomain domain = BKE_id_attribute_domain(id, layer);
  if (!ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CORNER)) {
    return 1;
  }

  return !(CD_TYPE_AS_MASK(layer->type) & CD_MASK_COLOR_ALL) || (layer->flag & CD_FLAG_TEMPORARY);
}

/* Attributes are spread over multiple domains in separate CustomData, we use repeated
 * array iterators to loop over all. */
static void rna_AttributeGroup_next_domain(ID *id,
                                           CollectionPropertyIterator *iter,
                                           int(skip)(CollectionPropertyIterator *iter, void *data))
{
  do {
    CustomDataLayer *prev_layers = (iter->internal.array.endptr == NULL) ?
                                       NULL :
                                       (CustomDataLayer *)iter->internal.array.endptr -
                                           iter->internal.array.length;
    CustomData *customdata = BKE_id_attributes_iterator_next_domain(id, prev_layers);
    if (customdata == NULL) {
      return;
    }
    rna_iterator_array_begin(
        iter, customdata->layers, sizeof(CustomDataLayer), customdata->totlayer, false, skip);
  } while (!iter->valid);
}

void rna_AttributeGroup_iterator_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  memset(&iter->internal.array, 0, sizeof(iter->internal.array));
  rna_AttributeGroup_next_domain(ptr->owner_id, iter, rna_Attributes_layer_skip);
}

void rna_AttributeGroup_iterator_next(CollectionPropertyIterator *iter)
{
  rna_iterator_array_next(iter);

  if (!iter->valid) {
    ID *id = iter->parent.owner_id;
    rna_AttributeGroup_next_domain(id, iter, rna_Attributes_layer_skip);
  }
}

PointerRNA rna_AttributeGroup_iterator_get(CollectionPropertyIterator *iter)
{
  /* Refine to the proper type. */
  CustomDataLayer *layer = rna_iterator_array_get(iter);
  StructRNA *type = srna_by_custom_data_layer_type(layer->type);
  if (type == NULL) {
    return PointerRNA_NULL;
  }
  return rna_pointer_inherit_refine(&iter->parent, type, layer);
}

void rna_AttributeGroup_color_iterator_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  memset(&iter->internal.array, 0, sizeof(iter->internal.array));
  rna_AttributeGroup_next_domain(ptr->owner_id, iter, rna_Attributes_noncolor_layer_skip);
}

void rna_AttributeGroup_color_iterator_next(CollectionPropertyIterator *iter)
{
  rna_iterator_array_next(iter);

  if (!iter->valid) {
    ID *id = iter->parent.owner_id;
    rna_AttributeGroup_next_domain(id, iter, rna_Attributes_noncolor_layer_skip);
  }
}

PointerRNA rna_AttributeGroup_color_iterator_get(CollectionPropertyIterator *iter)
{
  /* Refine to the proper type. */
  CustomDataLayer *layer = rna_iterator_array_get(iter);
  StructRNA *type = srna_by_custom_data_layer_type(layer->type);
  if (type == NULL) {
    return PointerRNA_NULL;
  }
  return rna_pointer_inherit_refine(&iter->parent, type, layer);
}

int rna_AttributeGroup_color_length(PointerRNA *ptr)
{
  return BKE_id_attributes_length(ptr->owner_id,
                                  ATTR_DOMAIN_MASK_POINT | ATTR_DOMAIN_MASK_CORNER,
                                  CD_MASK_PROP_COLOR | CD_MASK_PROP_BYTE_COLOR);
}

int rna_AttributeGroup_length(PointerRNA *ptr)
{
  return BKE_id_attributes_length(ptr->owner_id, ATTR_DOMAIN_MASK_ALL, CD_MASK_PROP_ALL);
}

static int rna_AttributeGroup_active_index_get(PointerRNA *ptr)
{
  return *BKE_id_attributes_active_index_p(ptr->owner_id);
}

static PointerRNA rna_AttributeGroup_active_get(PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  CustomDataLayer *layer = BKE_id_attributes_active_get(id);

  PointerRNA attribute_ptr;
  RNA_pointer_create(id, &RNA_Attribute, layer, &attribute_ptr);
  return attribute_ptr;
}

static void rna_AttributeGroup_active_set(PointerRNA *ptr,
                                          PointerRNA attribute_ptr,
                                          ReportList *UNUSED(reports))
{
  ID *id = ptr->owner_id;
  CustomDataLayer *layer = attribute_ptr.data;
  BKE_id_attributes_active_set(id, layer->name);
}

static void rna_AttributeGroup_active_index_set(PointerRNA *ptr, int value)
{
  *BKE_id_attributes_active_index_p(ptr->owner_id) = value;
}

static void rna_AttributeGroup_active_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  *min = 0;
  *max = BKE_id_attributes_length(ptr->owner_id, ATTR_DOMAIN_MASK_ALL, CD_MASK_PROP_ALL);

  *softmin = *min;
  *softmax = *max;
}

static void rna_AttributeGroup_update_active(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Attribute_update_data(bmain, scene, ptr);
}

static PointerRNA rna_AttributeGroup_active_color_get(PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  CustomDataLayer *layer = BKE_id_attribute_search(ptr->owner_id,
                                                   BKE_id_attributes_active_color_name(id),
                                                   CD_MASK_COLOR_ALL,
                                                   ATTR_DOMAIN_MASK_COLOR);

  PointerRNA attribute_ptr;
  RNA_pointer_create(id, &RNA_Attribute, layer, &attribute_ptr);
  return attribute_ptr;
}

static void rna_AttributeGroup_active_color_set(PointerRNA *ptr,
                                                PointerRNA attribute_ptr,
                                                ReportList *UNUSED(reports))
{
  ID *id = ptr->owner_id;
  CustomDataLayer *layer = attribute_ptr.data;
  BKE_id_attributes_active_color_set(id, layer->name);
}

static int rna_AttributeGroup_active_color_index_get(PointerRNA *ptr)
{
  const CustomDataLayer *layer = BKE_id_attribute_search(
      ptr->owner_id,
      BKE_id_attributes_active_color_name(ptr->owner_id),
      CD_MASK_COLOR_ALL,
      ATTR_DOMAIN_MASK_COLOR);

  return BKE_id_attribute_to_index(
      ptr->owner_id, layer, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);
}

static void rna_AttributeGroup_active_color_index_set(PointerRNA *ptr, int value)
{
  CustomDataLayer *layer = BKE_id_attribute_from_index(
      ptr->owner_id, value, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);

  if (!layer) {
    fprintf(stderr, "%s: error setting active color index to %d\n", __func__, value);
    return;
  }

  BKE_id_attributes_active_color_set(ptr->owner_id, layer->name);
}

static void rna_AttributeGroup_active_color_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  *min = 0;
  *max = BKE_id_attributes_length(ptr->owner_id, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);

  *softmin = *min;
  *softmax = *max;
}

static void rna_AttributeGroup_update_active_color(Main *UNUSED(bmain),
                                                   Scene *UNUSED(scene),
                                                   PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  /* Cheating way for importers to avoid slow updates. */
  if (id->us > 0) {
    DEG_id_tag_update(id, 0);
    WM_main_add_notifier(NC_GEOM | ND_DATA, id);
  }
}

static int rna_AttributeGroup_render_color_index_get(PointerRNA *ptr)
{
  const CustomDataLayer *layer = BKE_id_attributes_color_find(
      ptr->owner_id, BKE_id_attributes_default_color_name(ptr->owner_id));

  return BKE_id_attribute_to_index(
      ptr->owner_id, layer, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);
}

static void rna_AttributeGroup_render_color_index_set(PointerRNA *ptr, int value)
{
  CustomDataLayer *layer = BKE_id_attribute_from_index(
      ptr->owner_id, value, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);

  if (!layer) {
    fprintf(stderr, "%s: error setting render color index to %d\n", __func__, value);
    return;
  }

  BKE_id_attributes_default_color_set(ptr->owner_id, layer->name);
}

static void rna_AttributeGroup_render_color_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  *min = 0;
  *max = BKE_id_attributes_length(ptr->owner_id, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);

  *softmin = *min;
  *softmax = *max;
}

static void rna_AttributeGroup_default_color_name_get(PointerRNA *ptr, char *value)
{
  const ID *id = ptr->owner_id;
  const char *name = BKE_id_attributes_default_color_name(id);
  if (!name) {
    value[0] = '\0';
    return;
  }
  BLI_strncpy(value, name, MAX_CUSTOMDATA_LAYER_NAME);
}

static int rna_AttributeGroup_default_color_name_length(PointerRNA *ptr)
{
  const ID *id = ptr->owner_id;
  const char *name = BKE_id_attributes_default_color_name(id);
  return name ? strlen(name) : 0;
}

static void rna_AttributeGroup_default_color_name_set(PointerRNA *ptr, const char *value)
{
  ID *id = ptr->owner_id;
  if (GS(id->name) == ID_ME) {
    Mesh *mesh = (Mesh *)id;
    MEM_SAFE_FREE(mesh->default_color_attribute);
    if (value[0]) {
      mesh->default_color_attribute = BLI_strdup(value);
    }
  }
}

static void rna_AttributeGroup_active_color_name_get(PointerRNA *ptr, char *value)
{
  const ID *id = ptr->owner_id;
  const char *name = BKE_id_attributes_active_color_name(id);
  if (!name) {
    value[0] = '\0';
    return;
  }
  BLI_strncpy(value, name, MAX_CUSTOMDATA_LAYER_NAME);
}

static int rna_AttributeGroup_active_color_name_length(PointerRNA *ptr)
{
  const ID *id = ptr->owner_id;
  const char *name = BKE_id_attributes_active_color_name(id);
  return name ? strlen(name) : 0;
}

static void rna_AttributeGroup_active_color_name_set(PointerRNA *ptr, const char *value)
{
  ID *id = ptr->owner_id;
  if (GS(id->name) == ID_ME) {
    Mesh *mesh = (Mesh *)id;
    MEM_SAFE_FREE(mesh->default_color_attribute);
    if (value[0]) {
      mesh->default_color_attribute = BLI_strdup(value);
    }
  }
}

#else

static void rna_def_attribute_float(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "FloatAttribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(
      srna, "Float Attribute", "Geometry attribute that stores floating-point values");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "FloatAttributeValue");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    NULL,
                                    NULL,
                                    NULL);

  srna = RNA_def_struct(brna, "FloatAttributeValue", NULL);
  RNA_def_struct_sdna(srna, "MFloatProperty");
  RNA_def_struct_ui_text(
      srna, "Float Attribute Value", "Floating-point value in geometry attribute");
  prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "f");
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");
}

static void rna_def_attribute_float_vector(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* Float Vector Attribute */
  srna = RNA_def_struct(brna, "FloatVectorAttribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(
      srna, "Float Vector Attribute", "Geometry attribute that stores floating-point 3D vectors");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "FloatVectorAttributeValue");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    NULL,
                                    NULL,
                                    NULL);

  /* Float Vector Attribute Value */
  srna = RNA_def_struct(brna, "FloatVectorAttributeValue", NULL);
  RNA_def_struct_sdna(srna, "vec3f");
  RNA_def_struct_ui_text(
      srna, "Float Vector Attribute Value", "Vector value in geometry attribute");

  prop = RNA_def_property(srna, "vector", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_ui_text(prop, "Vector", "3D vector");
  RNA_def_property_float_sdna(prop, NULL, "x");
  RNA_def_property_array(prop, 3);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");
}

static void rna_def_attribute_float_color(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* Float Color Attribute */
  srna = RNA_def_struct(brna, "FloatColorAttribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(srna,
                         "Float Color Attribute",
                         "Geometry attribute that stores RGBA colors as floating-point values "
                         "using 32-bits per channel");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "FloatColorAttributeValue");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    NULL,
                                    NULL,
                                    NULL);

  /* Float Color Attribute Value */
  srna = RNA_def_struct(brna, "FloatColorAttributeValue", NULL);
  RNA_def_struct_sdna(srna, "MPropCol");
  RNA_def_struct_ui_text(srna, "Float Color Attribute Value", "Color value in geometry attribute");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_ui_text(prop, "Color", "RGBA color in scene linear color space");
  RNA_def_property_float_sdna(prop, NULL, "color");
  RNA_def_property_array(prop, 4);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  prop = RNA_def_property(srna, "color_srgb", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_ui_text(prop, "Color", "RGBA color in sRGB color space");
  RNA_def_property_float_sdna(prop, NULL, "color");
  RNA_def_property_array(prop, 4);
  RNA_def_property_float_funcs(prop,
                               "rna_FloatColorAttributeValue_color_srgb_get",
                               "rna_FloatColorAttributeValue_color_srgb_set",
                               NULL);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");
}

static void rna_def_attribute_byte_color(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* Byte Color Attribute */
  srna = RNA_def_struct(brna, "ByteColorAttribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(srna,
                         "Byte Color Attribute",
                         "Geometry attribute that stores RGBA colors as positive integer values "
                         "using 8-bits per channel");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "ByteColorAttributeValue");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    NULL,
                                    NULL,
                                    NULL);

  /* Byte Color Attribute Value */
  srna = RNA_def_struct(brna, "ByteColorAttributeValue", NULL);
  RNA_def_struct_sdna(srna, "MLoopCol");
  RNA_def_struct_ui_text(srna, "Byte Color Attribute Value", "Color value in geometry attribute");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_funcs(prop,
                               "rna_ByteColorAttributeValue_color_get",
                               "rna_ByteColorAttributeValue_color_set",
                               NULL);
  RNA_def_property_ui_text(prop, "Color", "RGBA color in scene linear color space");
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  prop = RNA_def_property(srna, "color_srgb", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_funcs(prop,
                               "rna_ByteColorAttributeValue_color_srgb_get",
                               "rna_ByteColorAttributeValue_color_srgb_set",
                               NULL);
  RNA_def_property_ui_text(prop, "Color", "RGBA color in sRGB color space");
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");
}

static void rna_def_attribute_int(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "IntAttribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(
      srna, "Integer Attribute", "Geometry attribute that stores integer values");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "IntAttributeValue");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    NULL,
                                    NULL,
                                    NULL);

  srna = RNA_def_struct(brna, "IntAttributeValue", NULL);
  RNA_def_struct_sdna(srna, "MIntProperty");
  RNA_def_struct_ui_text(srna, "Integer Attribute Value", "Integer value in geometry attribute");
  prop = RNA_def_property(srna, "value", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "i");
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");
}

static void rna_def_attribute_string(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "StringAttribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(srna, "String Attribute", "Geometry attribute that stores strings");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "StringAttributeValue");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    NULL,
                                    NULL,
                                    NULL);

  srna = RNA_def_struct(brna, "StringAttributeValue", NULL);
  RNA_def_struct_sdna(srna, "MStringProperty");
  RNA_def_struct_ui_text(srna, "String Attribute Value", "String value in geometry attribute");
  prop = RNA_def_property(srna, "value", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "s");
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");
}

static void rna_def_attribute_bool(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BoolAttribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(srna, "Bool Attribute", "Geometry attribute that stores booleans");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "BoolAttributeValue");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    NULL,
                                    NULL,
                                    NULL);

  srna = RNA_def_struct(brna, "BoolAttributeValue", NULL);
  RNA_def_struct_sdna(srna, "MBoolProperty");
  RNA_def_struct_ui_text(srna, "Bool Attribute Value", "Bool value in geometry attribute");
  prop = RNA_def_property(srna, "value", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "b", 0x01);
}

static void rna_def_attribute_int8(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ByteIntAttribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(
      srna, "8-bit Integer Attribute", "Geometry attribute that stores 8-bit integers");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "ByteIntAttributeValue");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    NULL,
                                    NULL,
                                    NULL);

  srna = RNA_def_struct(brna, "ByteIntAttributeValue", NULL);
  RNA_def_struct_sdna(srna, "MInt8Property");
  RNA_def_struct_ui_text(
      srna, "8-bit Integer Attribute Value", "8-bit value in geometry attribute");
  prop = RNA_def_property(srna, "value", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(
      prop, "rna_ByteIntAttributeValue_get", "rna_ByteIntAttributeValue_set", NULL);
}

static void rna_def_attribute_float2(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* Float2 Attribute */
  srna = RNA_def_struct(brna, "Float2Attribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(
      srna, "Float2 Attribute", "Geometry attribute that stores floating-point 2D vectors");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Float2AttributeValue");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    NULL,
                                    NULL,
                                    NULL);

  /* Float2 Attribute Value */
  srna = RNA_def_struct(brna, "Float2AttributeValue", NULL);
  RNA_def_struct_sdna(srna, "vec2f");
  RNA_def_struct_ui_text(srna, "Float2 Attribute Value", "2D Vector value in geometry attribute");

  prop = RNA_def_property(srna, "vector", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_ui_text(prop, "Vector", "2D vector");
  RNA_def_property_float_sdna(prop, NULL, "x");
  RNA_def_property_array(prop, 2);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");
}

static void rna_def_attribute(BlenderRNA *brna)
{
  PropertyRNA *prop;
  StructRNA *srna;

  srna = RNA_def_struct(brna, "Attribute", NULL);
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(srna, "Attribute", "Geometry attribute");
  RNA_def_struct_path_func(srna, "rna_Attribute_path");
  RNA_def_struct_refine_func(srna, "rna_Attribute_refine");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Attribute_name_set");
  RNA_def_property_editable_func(prop, "rna_Attribute_name_editable");
  RNA_def_property_ui_text(prop, "Name", "Name of the Attribute");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "data_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "type");
  RNA_def_property_enum_items(prop, rna_enum_attribute_type_items);
  RNA_def_property_enum_funcs(prop, "rna_Attribute_type_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Data Type", "Type of data stored in attribute");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_domain_items);
  RNA_def_property_enum_funcs(
      prop, "rna_Attribute_domain_get", NULL, "rna_Attribute_domain_itemf");
  RNA_def_property_ui_text(prop, "Domain", "Domain of the Attribute");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_internal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Attribute_is_internal_get", NULL);
  RNA_def_property_ui_text(
      prop, "Is Internal", "The attribute is meant for internal use by Blender");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* types */
  rna_def_attribute_float(brna);
  rna_def_attribute_float_vector(brna);
  rna_def_attribute_float_color(brna);
  rna_def_attribute_byte_color(brna);
  rna_def_attribute_int(brna);
  rna_def_attribute_string(brna);
  rna_def_attribute_bool(brna);
  rna_def_attribute_float2(brna);
  rna_def_attribute_int8(brna);
}

/* Mesh/PointCloud/Curves.attributes */
static void rna_def_attribute_group(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "AttributeGroup", NULL);
  RNA_def_struct_ui_text(srna, "Attribute Group", "Group of geometry attributes");
  RNA_def_struct_sdna(srna, "ID");

  /* API */
  func = RNA_def_function(srna, "new", "rna_AttributeGroup_new");
  RNA_def_function_ui_description(func, "Add attribute to geometry");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "name", "Attribute", 0, "Name", "Name of geometry attribute");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  parm = RNA_def_enum(
      func, "type", rna_enum_attribute_type_items, CD_PROP_FLOAT, "Type", "Attribute type");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  parm = RNA_def_enum(func,
                      "domain",
                      rna_enum_attribute_domain_items,
                      ATTR_DOMAIN_POINT,
                      "Domain",
                      "Type of element that attribute is stored on");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  parm = RNA_def_pointer(func, "attribute", "Attribute", "", "New geometry attribute");
  RNA_def_parameter_flags(parm, 0, PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_AttributeGroup_remove");
  RNA_def_function_ui_description(func, "Remove attribute from geometry");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "attribute", "Attribute", "", "Geometry Attribute");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

  /* Active */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Attribute");
  RNA_def_property_ui_text(prop, "Active Attribute", "Active attribute");
  RNA_def_property_pointer_funcs(
      prop, "rna_AttributeGroup_active_get", "rna_AttributeGroup_active_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_update(prop, 0, "rna_AttributeGroup_update_active");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Active Attribute Index", "Active attribute index");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_AttributeGroup_active_index_get",
                             "rna_AttributeGroup_active_index_set",
                             "rna_AttributeGroup_active_index_range");
  RNA_def_property_update(prop, 0, "rna_AttributeGroup_update_active");

  prop = RNA_def_property(srna, "active_color", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Attribute");
  RNA_def_property_ui_text(prop, "Active Color", "Active color attribute for display and editing");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_AttributeGroup_active_color_get",
                                 "rna_AttributeGroup_active_color_set",
                                 NULL,
                                 NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_update(prop, 0, "rna_AttributeGroup_update_active_color");

  prop = RNA_def_property(srna, "active_color_index", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Active Color Index", "Active color attribute index");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_AttributeGroup_active_color_index_get",
                             "rna_AttributeGroup_active_color_index_set",
                             "rna_AttributeGroup_active_color_index_range");
  RNA_def_property_update(prop, 0, "rna_AttributeGroup_update_active_color");

  prop = RNA_def_property(srna, "render_color_index", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Active Render Color Index",
                           "The index of the color attribute used as a fallback for rendering");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_AttributeGroup_render_color_index_get",
                             "rna_AttributeGroup_render_color_index_set",
                             "rna_AttributeGroup_render_color_index_range");
  RNA_def_property_update(prop, 0, "rna_AttributeGroup_update_active_color");

  prop = RNA_def_property(srna, "default_color_name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_maxlength(prop, MAX_CUSTOMDATA_LAYER_NAME);
  RNA_def_property_string_funcs(prop,
                                "rna_AttributeGroup_default_color_name_get",
                                "rna_AttributeGroup_default_color_name_length",
                                "rna_AttributeGroup_default_color_name_set");
  RNA_def_property_ui_text(
      prop,
      "Default Color Attribute",
      "The name of the default color attribute used as a fallback for rendering");

  prop = RNA_def_property(srna, "active_color_name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_maxlength(prop, MAX_CUSTOMDATA_LAYER_NAME);
  RNA_def_property_string_funcs(prop,
                                "rna_AttributeGroup_active_color_name_get",
                                "rna_AttributeGroup_active_color_name_length",
                                "rna_AttributeGroup_active_color_name_set");
  RNA_def_property_ui_text(prop,
                           "Active Color Attribute",
                           "The name of the active color attribute for display and editing");
}

void rna_def_attributes_common(StructRNA *srna)
{
  PropertyRNA *prop;

  /* Attributes */
  prop = RNA_def_property(srna, "attributes", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_AttributeGroup_iterator_begin",
                                    "rna_AttributeGroup_iterator_next",
                                    "rna_iterator_array_end",
                                    "rna_AttributeGroup_iterator_get",
                                    "rna_AttributeGroup_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "Attribute");
  RNA_def_property_ui_text(prop, "Attributes", "Geometry attributes");
  RNA_def_property_srna(prop, "AttributeGroup");

  prop = RNA_def_property(srna, "color_attributes", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_AttributeGroup_color_iterator_begin",
                                    "rna_AttributeGroup_color_iterator_next",
                                    "rna_iterator_array_end",
                                    "rna_AttributeGroup_color_iterator_get",
                                    "rna_AttributeGroup_color_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "Attribute");
  RNA_def_property_ui_text(prop, "Color Attributes", "Geometry color attributes");
  RNA_def_property_srna(prop, "AttributeGroup");
}

void RNA_def_attribute(BlenderRNA *brna)
{
  rna_def_attribute(brna);
  rna_def_attribute_group(brna);
}
#endif
