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
 */

/** \file
 * \ingroup RNA
 */

#include <stdlib.h>

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_customdata_types.h"
#include "DNA_hair_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_attribute.h"
#include "BKE_customdata.h"

#include "WM_types.h"

const EnumPropertyItem rna_enum_attribute_type_items[] = {
    {CD_PROP_FLOAT, "FLOAT", 0, "Float", "Floating-point value"},
    {CD_PROP_INT32, "INT", 0, "Integer", "32-bit integer"},
    {CD_PROP_FLOAT3, "FLOAT_VECTOR", 0, "Vector", "3D vector with floating-point values"},
    {CD_PROP_COLOR, "FLOAT_COLOR", 0, "Color", "RGBA color with floating-point precisions"},
    {CD_MLOOPCOL, "BYTE_COLOR", 0, "Byte Color", "RGBA color with 8-bit precision"},
    {CD_PROP_STRING, "STRING", 0, "String", "Text string"},
    {CD_PROP_BOOL, "BOOLEAN", 0, "Boolean", "True or false"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_attribute_domain_items[] = {
    /* Not implement yet */
    // {ATTR_DOMAIN_GEOMETRY, "GEOMETRY", 0, "Geometry", "Attribute on (whole) geometry"},
    {ATTR_DOMAIN_POINT, "POINT", 0, "Point", "Attribute on point"},
    {ATTR_DOMAIN_EDGE, "EDGE", 0, "Edge", "Attribute on mesh edge"},
    {ATTR_DOMAIN_CORNER, "CORNER", 0, "Corner", "Attribute on mesh polygon corner"},
    {ATTR_DOMAIN_POLYGON, "POLYGON", 0, "Polygon", "Attribute on mesh polygons"},
    /* Not implement yet */
    // {ATTR_DOMAIN_GRIDS, "GRIDS", 0, "Grids", "Attribute on mesh multires grids"},
    {ATTR_DOMAIN_CURVE, "CURVE", 0, "Curve", "Attribute on hair curve"},
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

#  include "BLI_math.h"

#  include "DEG_depsgraph.h"

#  include "BLT_translation.h"

#  include "WM_api.h"

/* Attribute */

static char *rna_Attribute_path(PointerRNA *ptr)
{
  CustomDataLayer *layer = ptr->data;
  return BLI_sprintfN("attributes['%s']", layer->name);
}

static void rna_Attribute_name_set(PointerRNA *ptr, const char *value)
{
  BKE_id_attribute_rename(ptr->owner_id, ptr->data, value, NULL);
}

static int rna_Attribute_name_editable(PointerRNA *ptr, const char **r_info)
{
  CustomDataLayer *layer = ptr->data;
  if (BKE_id_attribute_required(ptr->owner_id, layer)) {
    *r_info = N_("Can't modify name of required geometry attribute");
    return false;
  }

  return true;
}

static int rna_Attribute_type_get(PointerRNA *ptr)
{
  CustomDataLayer *layer = ptr->data;
  return layer->type;
}

const EnumPropertyItem *rna_enum_attribute_domain_itemf(ID *id, bool *r_free)
{
  EnumPropertyItem *item = NULL;
  const EnumPropertyItem *domain_item = NULL;
  const ID_Type id_type = GS(id->name);
  int totitem = 0, a;

  for (a = 0; rna_enum_attribute_domain_items[a].identifier; a++) {
    domain_item = &rna_enum_attribute_domain_items[a];

    if (id_type == ID_PT && !ELEM(domain_item->value, ATTR_DOMAIN_POINT)) {
      continue;
    }
    if (id_type == ID_HA && !ELEM(domain_item->value, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CURVE)) {
      continue;
    }
    if (id_type == ID_ME && ELEM(domain_item->value, ATTR_DOMAIN_CURVE)) {
      continue;
    }

    RNA_enum_item_add(&item, &totitem, domain_item);
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
  return rna_enum_attribute_domain_itemf(ptr->owner_id, r_free);
}

static int rna_Attribute_domain_get(PointerRNA *ptr)
{
  return BKE_id_attribute_domain(ptr->owner_id, ptr->data);
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
    case CD_MLOOPCOL:
      struct_size = sizeof(MLoopCol);
      break;
    case CD_PROP_STRING:
      struct_size = sizeof(MStringProperty);
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
  CustomDataLayer *layer = (CustomDataLayer *)attribute_ptr->data;
  BKE_id_attribute_remove(id, layer, reports);
  RNA_POINTER_INVALIDATE(attribute_ptr);

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);
}

static int rna_Attributes_layer_skip(CollectionPropertyIterator *UNUSED(iter), void *data)
{
  CustomDataLayer *layer = (CustomDataLayer *)data;
  return !(CD_TYPE_AS_MASK(layer->type) & CD_MASK_PROP_ALL);
}

/* Attributes are spread over multiple domains in separate CustomData, we use repeated
 * array iterators to loop over all. */
static void rna_AttributeGroup_next_domain(ID *id,
                                           CollectionPropertyIterator *iter,
                                           int(skip)(CollectionPropertyIterator *iter, void *data))
{
  do {
    CustomDataLayer *prev_layers = (CustomDataLayer *)iter->internal.array.endptr -
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
  /* refine to the proper type */
  StructRNA *type;
  CustomDataLayer *layer = rna_iterator_array_get(iter);

  switch (layer->type) {
    case CD_PROP_FLOAT:
      type = &RNA_FloatAttribute;
      break;
    case CD_PROP_INT32:
      type = &RNA_IntAttribute;
      break;
    case CD_PROP_FLOAT3:
      type = &RNA_FloatVectorAttribute;
      break;
    case CD_PROP_COLOR:
      type = &RNA_FloatColorAttribute;
      break;
    case CD_MLOOPCOL:
      type = &RNA_ByteColorAttribute;
      break;
    case CD_PROP_STRING:
      type = &RNA_StringAttribute;
      break;
    default:
      return PointerRNA_NULL;
  }

  return rna_pointer_inherit_refine(&iter->parent, type, layer);
}

int rna_AttributeGroup_length(PointerRNA *ptr)
{
  return BKE_id_attributes_length(ptr->owner_id, CD_MASK_PROP_ALL);
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
  BKE_id_attributes_active_set(id, layer);
}

static void rna_AttributeGroup_active_index_set(PointerRNA *ptr, int value)
{
  *BKE_id_attributes_active_index_p(ptr->owner_id) = value;
}

static void rna_AttributeGroup_active_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  *min = 0;
  *max = BKE_id_attributes_length(ptr->owner_id, CD_MASK_PROP_ALL);

  *softmin = *min;
  *softmax = *max;
}

static void rna_AttributeGroup_update_active(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Attribute_update_data(bmain, scene, ptr);
}

#else

static void rna_def_attribute_float(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "FloatAttribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(srna, "Float Attribute", "Geometry attribute with floating-point values");

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
      srna, "Float Vector Attribute", "Vector geometry attribute, with floating-point precision");

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
  RNA_def_struct_ui_text(
      srna, "Float Color Attribute", "Color geometry attribute, with floating-point precision");

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
}

static void rna_def_attribute_byte_color(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* Byte Color Attribute */
  srna = RNA_def_struct(brna, "ByteColorAttribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(
      srna, "Byte Color Attribute", "Color geometry attribute, with 8-bit precision");

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
}

static void rna_def_attribute_int(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "IntAttribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(srna, "Int Attribute", "Integer geometry attribute");

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
  RNA_def_struct_ui_text(srna, "String Attribute", "String geometry attribute");

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

static void rna_def_attribute(BlenderRNA *brna)
{
  PropertyRNA *prop;
  StructRNA *srna;

  srna = RNA_def_struct(brna, "Attribute", NULL);
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(srna, "Attribute", "Geometry attribute");
  RNA_def_struct_path_func(srna, "rna_Attribute_path");

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

  /* types */
  rna_def_attribute_float(brna);
  rna_def_attribute_float_vector(brna);
  rna_def_attribute_float_color(brna);
  rna_def_attribute_byte_color(brna);
  rna_def_attribute_int(brna);
  rna_def_attribute_string(brna);
}

/* Mesh/PointCloud/Hair.attributes */
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
  RNA_def_function_ui_description(func, "Add an attribute");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "name", "Attribute", 0, "", "Attribute name");
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
  RNA_def_function_ui_description(func, "Remove an attribute");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "attribute", "Attribute", "", "Geometry Attribute");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

  /* Active */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Attribute");
  RNA_def_property_pointer_funcs(
      prop, "rna_AttributeGroup_active_get", "rna_AttributeGroup_active_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Active Attribute", "Active attribute");
  RNA_def_property_update(prop, 0, "rna_AttributeGroup_update_active");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_AttributeGroup_active_index_get",
                             "rna_AttributeGroup_active_index_set",
                             "rna_AttributeGroup_active_index_range");
  RNA_def_property_update(prop, 0, "rna_AttributeGroup_update_active");
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
}

void RNA_def_attribute(BlenderRNA *brna)
{
  rna_def_attribute(brna);
  rna_def_attribute_group(brna);
}
#endif
