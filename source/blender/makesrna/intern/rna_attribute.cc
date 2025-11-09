/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "DNA_customdata_types.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"

#include "BLT_translation.hh"

#include "WM_types.hh"

#include "UI_resources.hh"

using blender::bke::AttrDomain;

const EnumPropertyItem rna_enum_attribute_type_items[] = {
    {CD_PROP_FLOAT, "FLOAT", 0, "Float", "Floating-point value"},
    {CD_PROP_INT32, "INT", 0, "Integer", "32-bit integer"},
    {CD_PROP_BOOL, "BOOLEAN", 0, "Boolean", "True or false"},
    {CD_PROP_FLOAT3, "FLOAT_VECTOR", 0, "Vector", "3D vector with floating-point values"},
    {CD_PROP_COLOR, "FLOAT_COLOR", 0, "Color", "RGBA color with 32-bit floating-point values"},
    {CD_PROP_QUATERNION, "QUATERNION", 0, "Quaternion", "Floating point quaternion rotation"},
    {CD_PROP_FLOAT4X4, "FLOAT4X4", 0, "4x4 Matrix", "Floating point matrix"},
    {CD_PROP_STRING, "STRING", 0, "String", "Text string"},
    {CD_PROP_INT8, "INT8", 0, "8-Bit Integer", "Smaller integer with a range from -128 to 127"},
    {CD_PROP_INT16_2D, "INT16_2D", 0, "2D 16-Bit Integer Vector", "16-bit signed integer vector"},
    {CD_PROP_INT32_2D, "INT32_2D", 0, "2D Integer Vector", "32-bit signed integer vector"},
    {CD_PROP_FLOAT2, "FLOAT2", 0, "2D Vector", "2D vector with floating-point values"},
    {CD_PROP_BYTE_COLOR,
     "BYTE_COLOR",
     0,
     "Byte Color",
     "RGBA color with 8-bit positive integer values"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_color_attribute_type_items[] = {
    {CD_PROP_COLOR, "FLOAT_COLOR", 0, "Color", "RGBA color 32-bit floating-point values"},
    {CD_PROP_BYTE_COLOR,
     "BYTE_COLOR",
     0,
     "Byte Color",
     "RGBA color with 8-bit positive integer values"},
    {0, nullptr, 0, nullptr, nullptr}};

const EnumPropertyItem rna_enum_attribute_type_with_auto_items[] = {
    {CD_AUTO_FROM_NAME, "AUTO", 0, "Auto", ""},
    {CD_PROP_FLOAT, "FLOAT", 0, "Float", "Floating-point value"},
    {CD_PROP_INT32, "INT", 0, "Integer", "32-bit integer"},
    {CD_PROP_BOOL, "BOOLEAN", 0, "Boolean", "True or false"},
    {CD_PROP_FLOAT3, "FLOAT_VECTOR", 0, "Vector", "3D vector with floating-point values"},
    {CD_PROP_COLOR, "FLOAT_COLOR", 0, "Color", "RGBA color with 32-bit floating-point values"},
    {CD_PROP_QUATERNION, "QUATERNION", 0, "Quaternion", "Floating point quaternion rotation"},
    {CD_PROP_FLOAT4X4, "FLOAT4X4", 0, "4x4 Matrix", "Floating point matrix"},
    {CD_PROP_STRING, "STRING", 0, "String", "Text string"},
    {CD_PROP_INT8, "INT8", 0, "8-Bit Integer", "Smaller integer with a range from -128 to 127"},
    {CD_PROP_INT16_2D, "INT16_2D", 0, "2D 16-Bit Integer Vector", "16-bit signed integer vector"},
    {CD_PROP_INT32_2D, "INT32_2D", 0, "2D Integer Vector", "32-bit signed integer vector"},
    {CD_PROP_FLOAT2, "FLOAT2", 0, "2D Vector", "2D vector with floating-point values"},
    {CD_PROP_BYTE_COLOR,
     "BYTE_COLOR",
     0,
     "Byte Color",
     "RGBA color with 8-bit positive integer values"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_attr_storage_type[] = {
    {int(blender::bke::AttrStorageType::Array),
     "ARRAY",
     0,
     "Array",
     "Store a value for every element"},
    {int(blender::bke::AttrStorageType::Single),
     "SINGLE",
     0,
     "Single",
     "Store a single value for the entire domain"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_attribute_domain_items[] = {
    /* Not implement yet */
    // {ATTR_DOMAIN_GEOMETRY, "GEOMETRY", 0, "Geometry", "Attribute on (whole) geometry"},
    {int(AttrDomain::Point), "POINT", 0, "Point", "Attribute on point"},
    {int(AttrDomain::Edge), "EDGE", 0, "Edge", "Attribute on mesh edge"},
    {int(AttrDomain::Face), "FACE", 0, "Face", "Attribute on mesh faces"},
    {int(AttrDomain::Corner), "CORNER", 0, "Face Corner", "Attribute on mesh face corner"},
    /* Not implement yet */
    // {ATTR_DOMAIN_GRIDS, "GRIDS", 0, "Grids", "Attribute on mesh multires grids"},
    {int(AttrDomain::Curve), "CURVE", 0, "Spline", "Attribute on spline"},
    {int(AttrDomain::Instance), "INSTANCE", 0, "Instance", "Attribute on instance"},
    {int(AttrDomain::Layer), "LAYER", 0, "Layer", "Attribute on Grease Pencil layer"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_attribute_domain_only_mesh_items[] = {
    {int(AttrDomain::Point), "POINT", 0, "Point", "Attribute on point"},
    {int(AttrDomain::Edge), "EDGE", 0, "Edge", "Attribute on mesh edge"},
    {int(AttrDomain::Face), "FACE", 0, "Face", "Attribute on mesh faces"},
    {int(AttrDomain::Corner), "CORNER", 0, "Face Corner", "Attribute on mesh face corner"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_attribute_domain_only_mesh_no_edge_items[] = {
    {int(AttrDomain::Point), "POINT", 0, "Point", "Attribute on point"},
    {int(AttrDomain::Face), "FACE", 0, "Face", "Attribute on mesh faces"},
    {int(AttrDomain::Corner), "CORNER", 0, "Face Corner", "Attribute on mesh face corner"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_attribute_domain_only_mesh_no_corner_items[] = {
    {int(AttrDomain::Point), "POINT", 0, "Point", "Attribute on point"},
    {int(AttrDomain::Edge), "EDGE", 0, "Edge", "Attribute on mesh edge"},
    {int(AttrDomain::Face), "FACE", 0, "Face", "Attribute on mesh faces"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_attribute_domain_point_face_curve_items[] = {
    {int(AttrDomain::Point), "POINT", 0, "Point", "Attribute on point"},
    {int(AttrDomain::Face), "FACE", 0, "Face", "Attribute on mesh faces"},
    {int(AttrDomain::Curve), "CURVE", 0, "Spline", "Attribute on spline"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_attribute_domain_point_edge_face_curve_items[] = {
    {int(AttrDomain::Point), "POINT", 0, "Point", "Attribute on point"},
    {int(AttrDomain::Edge), "EDGE", 0, "Edge", "Attribute on mesh edge"},
    {int(AttrDomain::Face), "FACE", 0, "Face", "Attribute on mesh faces"},
    {int(AttrDomain::Curve), "CURVE", 0, "Spline", "Attribute on spline"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_attribute_domain_edge_face_items[] = {
    {int(AttrDomain::Edge), "EDGE", 0, "Edge", "Attribute on mesh edge"},
    {int(AttrDomain::Face), "FACE", 0, "Face", "Attribute on mesh faces"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_attribute_domain_without_corner_items[] = {
    {int(AttrDomain::Point), "POINT", 0, "Point", "Attribute on point"},
    {int(AttrDomain::Edge), "EDGE", 0, "Edge", "Attribute on mesh edge"},
    {int(AttrDomain::Face), "FACE", 0, "Face", "Attribute on mesh faces"},
    {int(AttrDomain::Curve), "CURVE", 0, "Spline", "Attribute on spline"},
    {int(AttrDomain::Instance), "INSTANCE", 0, "Instance", "Attribute on instance"},
    {int(AttrDomain::Layer), "LAYER", 0, "Layer", "Attribute on Grease Pencil layer"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_attribute_domain_with_auto_items[] = {
    {int(AttrDomain::Auto), "AUTO", 0, "Auto", ""},
    {int(AttrDomain::Point), "POINT", 0, "Point", "Attribute on point"},
    {int(AttrDomain::Edge), "EDGE", 0, "Edge", "Attribute on mesh edge"},
    {int(AttrDomain::Face), "FACE", 0, "Face", "Attribute on mesh faces"},
    {int(AttrDomain::Corner), "CORNER", 0, "Face Corner", "Attribute on mesh face corner"},
    {int(AttrDomain::Curve), "CURVE", 0, "Spline", "Attribute on spline"},
    {int(AttrDomain::Instance), "INSTANCE", 0, "Instance", "Attribute on instance"},
    {int(AttrDomain::Layer), "LAYER", 0, "Layer", "Attribute on Grease Pencil layer"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_color_attribute_domain_items[] = {
    {int(AttrDomain::Point), "POINT", 0, "Vertex", ""},
    {int(AttrDomain::Corner), "CORNER", 0, "Face Corner", ""},
    {0, nullptr, 0, nullptr, nullptr}};

const EnumPropertyItem rna_enum_attribute_curves_domain_items[] = {
    {int(AttrDomain::Point), "POINT", 0, "Control Point", ""},
    {int(AttrDomain::Curve), "CURVE", 0, "Curve", ""},
    {0, nullptr, 0, nullptr, nullptr}};

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "DNA_customdata_types.h"
#  include "DNA_grease_pencil_types.h"
#  include "DNA_mesh_types.h"
#  include "DNA_meshdata_types.h"
#  include "DNA_pointcloud_types.h"

#  include "BLI_math_color.h"
#  include "BLI_string.h"

#  include "BKE_attribute_legacy_convert.hh"
#  include "BKE_curves.hh"
#  include "BKE_customdata.hh"
#  include "BKE_report.hh"

#  include "RNA_prototypes.hh"

#  include "DEG_depsgraph.hh"

#  include "BLT_translation.hh"

#  include "IMB_colormanagement.hh"

#  include "WM_api.hh"

using blender::StringRef;

/* Attribute */

static bool find_attr_with_pointer(const blender::bke::AttributeStorage &storage,
                                   const blender::bke::Attribute &attr)
{
  bool found_attr = false;
  storage.foreach_with_stop([&](const blender::bke::Attribute &attr_iter) {
    if (&attr_iter == &attr) {
      found_attr = true;
      return false;
    }
    return true;
  });
  return found_attr;
}

static AttributeOwner owner_from_attribute_pointer_rna(PointerRNA *ptr)
{
  using namespace blender;
  ID *owner_id = ptr->owner_id;
  /* TODO: Because we don't know the path to the `ptr`, we need to look though all possible
   * candidates and search for the `layer` currently. This should be just a simple lookup. */
  if (GS(owner_id->name) == ID_GP) {
    bke::Attribute *attr = static_cast<bke::Attribute *>(ptr->data);
    GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(owner_id);

    /* First check the layer attributes. */
    if (find_attr_with_pointer(grease_pencil->attribute_storage.wrap(), *attr)) {
      return AttributeOwner(AttributeOwnerType::GreasePencil, grease_pencil);
    }

    /* Now check all the drawings. */
    for (GreasePencilDrawingBase *base : grease_pencil->drawings()) {
      if (base->type == GP_DRAWING) {
        GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(base);
        const bke::CurvesGeometry &curves = drawing->geometry.wrap();
        if (find_attr_with_pointer(curves.attribute_storage.wrap(), *attr)) {
          return AttributeOwner(AttributeOwnerType::GreasePencilDrawing, drawing);
        }
      }
    }
  }
  return AttributeOwner::from_id(owner_id);
}

static AttributeOwner owner_from_pointer_rna(PointerRNA *ptr)
{
  /* For non-ID attribute owners, check the `ptr->type` to derive the `AttributeOwnerType`
   * and construct an `AttributeOwner` from that type and `ptr->data`. */
  if (ptr->type == &RNA_GreasePencilDrawing) {
    return AttributeOwner(AttributeOwnerType::GreasePencilDrawing, ptr->data);
  }
  return AttributeOwner::from_id(ptr->owner_id);
}

static std::optional<std::string> rna_Attribute_path(const PointerRNA *ptr)
{
  using namespace blender;
  if (GS(ptr->owner_id->name) != ID_ME) {
    bke::Attribute *attr = ptr->data_as<bke::Attribute>();
    const std::string escaped_name = BLI_str_escape(attr->name().c_str());
    return fmt::format("attributes[\"{}\"]", escaped_name);
  }

  const CustomDataLayer *layer = static_cast<const CustomDataLayer *>(ptr->data);
  char layer_name_esc[sizeof(layer->name) * 2];
  BLI_str_escape(layer_name_esc, layer->name, sizeof(layer_name_esc));
  return fmt::format("attributes[\"{}\"]", layer_name_esc);
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
    case CD_PROP_INT16_2D:
      return &RNA_Short2Attribute;
    case CD_PROP_INT32_2D:
      return &RNA_Int2Attribute;
    case CD_PROP_QUATERNION:
      return &RNA_QuaternionAttribute;
    case CD_PROP_FLOAT4X4:
      return &RNA_Float4x4Attribute;
    default:
      return nullptr;
  }
}

static StructRNA *rna_Attribute_refine(PointerRNA *ptr)
{
  using namespace blender;
  if (GS(ptr->owner_id->name) != ID_ME) {
    bke::Attribute *attr = ptr->data_as<bke::Attribute>();
    const eCustomDataType data_type = *bke::attr_type_to_custom_data_type(attr->data_type());
    return srna_by_custom_data_layer_type(data_type);
  }

  CustomDataLayer *layer = static_cast<CustomDataLayer *>(ptr->data);
  return srna_by_custom_data_layer_type(eCustomDataType(layer->type));
}

static void rna_Attribute_name_get(PointerRNA *ptr, char *value)
{
  using namespace blender;
  AttributeOwner owner = owner_from_attribute_pointer_rna(ptr);
  if (owner.type() == AttributeOwnerType::Mesh) {
    strcpy(value, ptr->data_as<CustomDataLayer>()->name);
    return;
  }

  const bke::Attribute *attr = ptr->data_as<bke::Attribute>();
  attr->name().copy_unsafe(value);
}

static int rna_Attribute_name_length(PointerRNA *ptr)
{
  using namespace blender;
  AttributeOwner owner = owner_from_attribute_pointer_rna(ptr);
  const CustomDataLayer *layer = ptr->data_as<CustomDataLayer>();
  if (owner.type() == AttributeOwnerType::Mesh) {
    return strlen(layer->name);
  }

  const bke::Attribute *attr = ptr->data_as<bke::Attribute>();
  return attr->name().size();
}

static void rna_Attribute_name_set(PointerRNA *ptr, const char *value)
{
  using namespace blender;
  AttributeOwner owner = owner_from_attribute_pointer_rna(ptr);
  if (owner.type() == AttributeOwnerType::Mesh) {
    const CustomDataLayer *layer = (const CustomDataLayer *)ptr->data;
    BKE_attribute_rename(owner, layer->name, value, nullptr);
    return;
  }

  const bke::Attribute *attr = ptr->data_as<bke::Attribute>();
  BKE_attribute_rename(owner, attr->name(), value, nullptr);
}

static int rna_Attribute_name_editable(const PointerRNA *ptr, const char **r_info)
{
  using namespace blender;
  AttributeOwner owner = owner_from_attribute_pointer_rna(const_cast<PointerRNA *>(ptr));
  if (owner.type() == AttributeOwnerType::Mesh) {
    CustomDataLayer *layer = static_cast<CustomDataLayer *>(ptr->data);
    if (BKE_attribute_required(owner, layer->name)) {
      *r_info = N_("Cannot modify name of required geometry attribute");
      return false;
    }
    return true;
  }

  bke::Attribute *attr = ptr->data_as<bke::Attribute>();
  if (BKE_attribute_required(owner, attr->name())) {
    *r_info = N_("Cannot modify name of required geometry attribute");
    return false;
  }
  return true;
}

static int rna_Attribute_type_get(PointerRNA *ptr)
{
  using namespace blender;
  if (GS(ptr->owner_id->name) != ID_ME) {
    const bke::Attribute *attr = static_cast<const bke::Attribute *>(ptr->data);
    return *bke::attr_type_to_custom_data_type(attr->data_type());
  }

  CustomDataLayer *layer = static_cast<CustomDataLayer *>(ptr->data);
  return layer->type;
}

static int rna_Attribute_storage_type_get(PointerRNA *ptr)
{
  using namespace blender;
  if (GS(ptr->owner_id->name) != ID_ME) {
    const bke::Attribute *attr = static_cast<const bke::Attribute *>(ptr->data);
    return int(attr->storage_type());
  }

  return int(bke::AttrStorageType::Array);
}

const EnumPropertyItem *rna_enum_attribute_domain_itemf(const AttributeOwner &owner,
                                                        bool include_instances,
                                                        bool *r_free)
{
  EnumPropertyItem *item = nullptr;
  const EnumPropertyItem *domain_item = nullptr;
  int totitem = 0, a;

  static EnumPropertyItem mesh_vertex_domain_item = {
      int(AttrDomain::Point), "POINT", 0, N_("Vertex"), N_("Attribute per point/vertex")};

  for (a = 0; rna_enum_attribute_domain_items[a].identifier; a++) {
    domain_item = &rna_enum_attribute_domain_items[a];

    if (owner.type() == AttributeOwnerType::PointCloud &&
        !ELEM(domain_item->value, int(AttrDomain::Point)))
    {
      continue;
    }
    if (owner.type() == AttributeOwnerType::Curves &&
        !ELEM(domain_item->value, int(AttrDomain::Point), int(AttrDomain::Curve)))
    {
      continue;
    }
    if (owner.type() == AttributeOwnerType::Mesh && !ELEM(domain_item->value,
                                                          int(AttrDomain::Point),
                                                          int(AttrDomain::Edge),
                                                          int(AttrDomain::Face),
                                                          int(AttrDomain::Corner)))
    {
      continue;
    }
    if (owner.type() == AttributeOwnerType::GreasePencil &&
        !ELEM(domain_item->value, int(AttrDomain::Layer)))
    {
      continue;
    }
    if (!include_instances && domain_item->value == int(AttrDomain::Instance)) {
      continue;
    }

    if (domain_item->value == int(AttrDomain::Point) && owner.type() == AttributeOwnerType::Mesh) {
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

static const EnumPropertyItem *rna_Attribute_domain_itemf(bContext * /*C*/,
                                                          PointerRNA *ptr,
                                                          PropertyRNA * /*prop*/,
                                                          bool *r_free)
{
  AttributeOwner owner = owner_from_attribute_pointer_rna(ptr);
  return rna_enum_attribute_domain_itemf(owner, true, r_free);
}

static int rna_Attribute_domain_get(PointerRNA *ptr)
{
  using namespace blender;
  AttributeOwner owner = owner_from_attribute_pointer_rna(ptr);
  if (owner.type() == AttributeOwnerType::Mesh) {
    return int(BKE_attribute_domain(owner, static_cast<const CustomDataLayer *>(ptr->data)));
  }

  const bke::Attribute *attr = static_cast<const bke::Attribute *>(ptr->data);
  return int(attr->domain());
}

static bool rna_Attribute_is_internal_get(PointerRNA *ptr)
{
  using namespace blender;
  if (GS(ptr->owner_id->name) != ID_ME) {
    const bke::Attribute *attr = static_cast<const bke::Attribute *>(ptr->data);
    return !bke::allow_procedural_attribute_access(attr->name());
  }

  const CustomDataLayer *layer = (const CustomDataLayer *)ptr->data;
  return !blender::bke::allow_procedural_attribute_access(layer->name);
}

static bool rna_Attribute_is_required_get(PointerRNA *ptr)
{
  using namespace blender;
  AttributeOwner owner = owner_from_attribute_pointer_rna(ptr);
  if (owner.type() == AttributeOwnerType::Mesh) {
    const CustomDataLayer *layer = (const CustomDataLayer *)ptr->data;
    return BKE_attribute_required(owner, layer->name);
  }

  const bke::Attribute *attr = static_cast<const bke::Attribute *>(ptr->data);
  return BKE_attribute_required(owner, attr->name());
}

static void rna_Attribute_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  using namespace blender;
  AttributeOwner owner = owner_from_attribute_pointer_rna(ptr);
  if (owner.type() == AttributeOwnerType::Mesh) {
    CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
    if (!(CD_TYPE_AS_MASK(eCustomDataType(layer->type)) & CD_MASK_PROP_ALL)) {
      iter->valid = false;
    }

    const int length = BKE_attribute_data_length(owner, layer);
    const size_t struct_size = CustomData_get_elem_size(layer);
    CustomData_ensure_data_is_mutable(layer, length);

    rna_iterator_array_begin(iter, ptr, layer->data, struct_size, length, 0, nullptr);
    return;
  }

  bke::MutableAttributeAccessor accessor = *owner.get_accessor();

  bke::Attribute *attr = ptr->data_as<bke::Attribute>();
  const int domain_size = accessor.domain_size(attr->domain());
  const CPPType &type = bke::attribute_type_to_cpp_type(attr->data_type());
  switch (attr->storage_type()) {
    case bke::AttrStorageType::Array: {
      const auto &data = std::get<bke::Attribute::ArrayData>(attr->data_for_write());
      rna_iterator_array_begin(iter, ptr, data.data, type.size, domain_size, false, nullptr);
      break;
    }
    case bke::AttrStorageType::Single: {
      /* TODO: Access to single values is unimplemented for now. */
      iter->valid = false;
      break;
    }
  }
}

static int rna_Attribute_data_length(PointerRNA *ptr)
{
  using namespace blender;
  AttributeOwner owner = owner_from_attribute_pointer_rna(ptr);
  if (owner.type() == AttributeOwnerType::Mesh) {
    CustomDataLayer *layer = (CustomDataLayer *)ptr->data;
    return BKE_attribute_data_length(owner, layer);
  }

  const bke::Attribute *attr = ptr->data_as<bke::Attribute>();
  const bke::AttributeAccessor accessor = *owner.get_accessor();
  return accessor.domain_size(attr->domain());
}

static void rna_Attribute_update_data(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
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
  IMB_colormanagement_rec709_to_scene_linear(values, values);
}

static void rna_ByteColorAttributeValue_color_set(PointerRNA *ptr, const float *values)
{
  MLoopCol *mlcol = (MLoopCol *)ptr->data;
  float rec709[4];
  IMB_colormanagement_scene_linear_to_rec709(rec709, values);
  rec709[3] = values[3];
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
  IMB_colormanagement_scene_linear_to_srgb_v3(values, col->color);
  values[3] = col->color[3];
}

static void rna_FloatColorAttributeValue_color_srgb_set(PointerRNA *ptr, const float *values)
{
  MPropCol *col = (MPropCol *)ptr->data;
  IMB_colormanagement_srgb_to_scene_linear_v3(col->color, values);
  col->color[3] = values[3];
}

/* String Attribute */

static void rna_StringAttributeValue_s_get(PointerRNA *ptr, char *value)
{
  const MStringProperty *mstring = static_cast<const MStringProperty *>(ptr->data);
  const int len = std::min<int>(mstring->s_len, sizeof(mstring->s) - 1);
  memcpy(value, mstring->s, len);
  /* RNA accessors require this. */
  value[len] = '\0';
}

static int rna_StringAttributeValue_s_length(PointerRNA *ptr)
{
  const MStringProperty *mstring = static_cast<const MStringProperty *>(ptr->data);
  const int len = std::min<int>(mstring->s_len, sizeof(mstring->s) - 1);
  return len;
}

static void rna_StringAttributeValue_s_set(PointerRNA *ptr, const char *value)
{
  /* NOTE: RNA does not support byte-strings which contain null bytes.
   * If `PROP_BYTESTRING` supported this then a value & length could be passed in
   * and `MStringProperty` could be set with values to include null bytes. */
  MStringProperty *mstring = static_cast<MStringProperty *>(ptr->data);
  mstring->s_len = BLI_strnlen(value, sizeof(MStringProperty::s));
  memcpy(mstring->s, value, mstring->s_len);
}

/* Attribute Group */

static PointerRNA rna_AttributeGroupID_new(
    ID *id, ReportList *reports, const char *name, const int type, const int domain)
{
  using namespace blender;
  AttributeOwner owner = AttributeOwner::from_id(id);
  if (owner.type() == AttributeOwnerType::Mesh) {
    CustomDataLayer *layer = BKE_attribute_new(
        owner, name, eCustomDataType(type), AttrDomain(domain), reports);

    if (!layer) {
      return PointerRNA_NULL;
    }

    if ((GS(id->name) == ID_ME) && ELEM(layer->type, CD_PROP_COLOR, CD_PROP_BYTE_COLOR)) {
      Mesh *mesh = (Mesh *)id;
      if (!mesh->active_color_attribute) {
        mesh->active_color_attribute = BLI_strdup(layer->name);
      }
      if (!mesh->default_color_attribute) {
        mesh->default_color_attribute = BLI_strdup(layer->name);
      }
    }

    DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, id);

    PointerRNA ptr = RNA_pointer_create_discrete(id, &RNA_Attribute, layer);
    return ptr;
  }

  const bke::AttributeAccessor accessor = *owner.get_accessor();
  if (!accessor.domain_supported(AttrDomain(domain))) {
    BKE_report(reports, RPT_ERROR, "Attribute domain not supported by this geometry type");
    return PointerRNA_NULL;
  }
  const int domain_size = accessor.domain_size(AttrDomain(domain));

  bke::AttributeStorage &attributes = *owner.get_storage();
  const CPPType &cpp_type = *bke::custom_data_type_to_cpp_type(eCustomDataType(type));
  bke::Attribute &attr = attributes.add(
      attributes.unique_name_calc(name),
      AttrDomain(domain),
      *bke::custom_data_type_to_attr_type(eCustomDataType(type)),
      bke::Attribute::ArrayData::from_default_value(cpp_type, domain_size));

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);

  return RNA_pointer_create_discrete(id, &RNA_Attribute, &attr);
}

static void rna_AttributeGroupID_remove(ID *id, ReportList *reports, PointerRNA *attribute_ptr)
{
  using namespace blender;
  AttributeOwner owner = AttributeOwner::from_id(id);
  if (owner.type() == AttributeOwnerType::Mesh) {
    const CustomDataLayer *layer = (const CustomDataLayer *)attribute_ptr->data;
    BKE_attribute_remove(owner, layer->name, reports);
    attribute_ptr->invalidate();

    DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, id);
    return;
  }

  const bke::Attribute *attr = static_cast<const bke::Attribute *>(attribute_ptr->data);
  if (BKE_attribute_required(owner, attr->name())) {
    BKE_report(reports, RPT_ERROR, "Attribute is required and cannot be removed");
    return;
  }

  bke::MutableAttributeAccessor accessor = *owner.get_accessor();
  accessor.remove(attr->name());
  attribute_ptr->invalidate();

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);
}

static bool rna_Attributes_layer_skip(CollectionPropertyIterator * /*iter*/, void *data)
{
  CustomDataLayer *layer = (CustomDataLayer *)data;
  return !(CD_TYPE_AS_MASK(eCustomDataType(layer->type)) & CD_MASK_PROP_ALL);
}

static bool rna_Attributes_noncolor_layer_skip(CollectionPropertyIterator *iter, void *data)
{
  CustomDataLayer *layer = (CustomDataLayer *)data;

  /* Check valid domain here, too, keep in line with rna_AttributeGroup_color_length(). */
  PointerRNA attribute_pointer(iter->parent.owner_id, &RNA_Attribute, data);
  const AttributeOwner owner = owner_from_attribute_pointer_rna(&attribute_pointer);
  const AttrDomain domain = BKE_attribute_domain(owner, layer);
  if (!(ATTR_DOMAIN_AS_MASK(domain) & ATTR_DOMAIN_MASK_COLOR)) {
    return true;
  }

  return !(CD_TYPE_AS_MASK(eCustomDataType(layer->type)) & CD_MASK_COLOR_ALL) ||
         (layer->flag & CD_FLAG_TEMPORARY);
}

/* Attributes are spread over multiple domains in separate CustomData, we use repeated
 * array iterators to loop over all. */
static void rna_AttributeGroup_next_domain(AttributeOwner &owner,
                                           CollectionPropertyIterator *iter,
                                           PointerRNA *ptr,
                                           bool(skip)(CollectionPropertyIterator *iter,
                                                      void *data))
{
  do {
    CustomDataLayer *prev_layers = (iter->internal.array.endptr == nullptr) ?
                                       nullptr :
                                       (CustomDataLayer *)iter->internal.array.endptr -
                                           iter->internal.array.length;
    CustomData *customdata = BKE_attributes_iterator_next_domain(owner, prev_layers);
    if (customdata == nullptr) {
      return;
    }
    rna_iterator_array_begin(
        iter, ptr, customdata->layers, sizeof(CustomDataLayer), customdata->totlayer, false, skip);
  } while (!iter->valid);
}

void rna_AttributeGroup_iterator_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  using namespace blender;
  memset(&iter->internal.array, 0, sizeof(iter->internal.array));
  AttributeOwner owner = owner_from_pointer_rna(ptr);
  if (owner.type() == AttributeOwnerType::Mesh) {
    rna_AttributeGroup_next_domain(owner, iter, ptr, rna_Attributes_layer_skip);
    return;
  }

  bke::AttributeStorage &storage = *owner.get_storage();
  Vector<bke::Attribute *> attributes;
  storage.foreach([&](bke::Attribute &attr) { attributes.append(&attr); });
  VectorData data = attributes.release();
  rna_iterator_array_begin(
      iter, ptr, data.data, sizeof(bke::Attribute *), data.size, true, nullptr);
}

void rna_AttributeGroup_iterator_next(CollectionPropertyIterator *iter)
{
  rna_iterator_array_next(iter);
  AttributeOwner owner = owner_from_pointer_rna(&iter->parent);
  if (owner.type() == AttributeOwnerType::Mesh) {
    if (!iter->valid) {
      rna_AttributeGroup_next_domain(owner, iter, &iter->parent, rna_Attributes_layer_skip);
    }
    return;
  }
  /* Everything stored in #AttributeStorage is an attribute. */
}

PointerRNA rna_AttributeGroup_iterator_get(CollectionPropertyIterator *iter)
{
  using namespace blender;
  AttributeOwner owner = owner_from_pointer_rna(&iter->parent);
  if (owner.type() == AttributeOwnerType::Mesh) {
    CustomDataLayer *layer = static_cast<CustomDataLayer *>(rna_iterator_array_get(iter));
    StructRNA *type = srna_by_custom_data_layer_type(eCustomDataType(layer->type));
    if (type == nullptr) {
      return PointerRNA_NULL;
    }
    return RNA_pointer_create_with_parent(iter->parent, type, layer);
  }

  bke::Attribute *attr = *static_cast<bke::Attribute **>(rna_iterator_array_get(iter));
  const eCustomDataType data_type = *bke::attr_type_to_custom_data_type(attr->data_type());
  StructRNA *type = srna_by_custom_data_layer_type(data_type);
  return RNA_pointer_create_with_parent(iter->parent, type, attr);
}

void rna_AttributeGroup_color_iterator_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  using namespace blender;
  AttributeOwner owner = owner_from_pointer_rna(ptr);
  if (owner.type() == AttributeOwnerType::Mesh) {
    memset(&iter->internal.array, 0, sizeof(iter->internal.array));
    rna_AttributeGroup_next_domain(owner, iter, ptr, rna_Attributes_noncolor_layer_skip);
    return;
  }

  bke::AttributeStorage &storage = *owner.get_storage();
  Vector<bke::Attribute *> attributes;
  storage.foreach([&](bke::Attribute &attr) {
    if (!(ATTR_DOMAIN_AS_MASK(attr.domain()) & ATTR_DOMAIN_MASK_COLOR)) {
      return;
    }
    if (!(CD_TYPE_AS_MASK(*bke::attr_type_to_custom_data_type(attr.data_type())) &
          CD_MASK_COLOR_ALL))
    {
      return;
    }
    attributes.append(&attr);
  });
  VectorData data = attributes.release();
  rna_iterator_array_begin(
      iter, ptr, data.data, sizeof(bke::Attribute *), data.size, true, nullptr);
}

void rna_AttributeGroup_color_iterator_next(CollectionPropertyIterator *iter)
{
  using namespace blender;
  rna_iterator_array_next(iter);
  AttributeOwner owner = owner_from_pointer_rna(&iter->parent);
  if (owner.type() == AttributeOwnerType::Mesh) {
    if (!iter->valid) {
      AttributeOwner owner = owner_from_pointer_rna(&iter->parent);
      rna_AttributeGroup_next_domain(
          owner, iter, &iter->parent, rna_Attributes_noncolor_layer_skip);
    }
    return;
  }
  /* Not used for #AttributeStorage. */
}

PointerRNA rna_AttributeGroup_color_iterator_get(CollectionPropertyIterator *iter)
{
  using namespace blender;
  AttributeOwner owner = owner_from_pointer_rna(&iter->parent);
  if (owner.type() == AttributeOwnerType::Mesh) {
    CustomDataLayer *layer = static_cast<CustomDataLayer *>(rna_iterator_array_get(iter));
    StructRNA *type = srna_by_custom_data_layer_type(eCustomDataType(layer->type));
    if (type == nullptr) {
      return PointerRNA_NULL;
    }
    return RNA_pointer_create_with_parent(iter->parent, type, layer);
  }

  bke::Attribute *attr = *static_cast<bke::Attribute **>(rna_iterator_array_get(iter));
  const eCustomDataType data_type = *bke::attr_type_to_custom_data_type(attr->data_type());
  StructRNA *type = srna_by_custom_data_layer_type(data_type);
  return RNA_pointer_create_with_parent(iter->parent, type, attr);
}

int rna_AttributeGroup_color_length(PointerRNA *ptr)
{
  using namespace blender;
  AttributeOwner owner = owner_from_pointer_rna(ptr);
  if (owner.type() == AttributeOwnerType::Mesh) {
    return BKE_attributes_length(owner, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);
  }

  bke::AttributeStorage &storage = *owner.get_storage();
  int count = 0;
  storage.foreach([&](bke::Attribute &attr) {
    if (!(ATTR_DOMAIN_AS_MASK(attr.domain()) & ATTR_DOMAIN_MASK_COLOR)) {
      return;
    }
    if (!(CD_TYPE_AS_MASK(*bke::attr_type_to_custom_data_type(attr.data_type())) &
          CD_MASK_COLOR_ALL))
    {
      return;
    }
    count++;
  });
  return count;
}

int rna_AttributeGroup_length(PointerRNA *ptr)
{
  using namespace blender;
  AttributeOwner owner = owner_from_pointer_rna(ptr);
  if (owner.type() == AttributeOwnerType::Mesh) {
    return BKE_attributes_length(owner, ATTR_DOMAIN_MASK_ALL, CD_MASK_PROP_ALL);
  }

  bke::AttributeStorage &storage = *owner.get_storage();
  int count = 0;
  storage.foreach([&](bke::Attribute & /*attr*/) { count++; });
  return count;
}

bool rna_AttributeGroup_lookup_string(PointerRNA *ptr, const char *key, PointerRNA *r_ptr)
{
  using namespace blender;
  AttributeOwner owner = owner_from_pointer_rna(ptr);
  if (owner.type() == AttributeOwnerType::Mesh) {
    if (CustomDataLayer *layer = BKE_attribute_search_for_write(
            owner, key, CD_MASK_PROP_ALL, ATTR_DOMAIN_MASK_ALL))
    {
      rna_pointer_create_with_ancestors(*ptr, &RNA_Attribute, layer, *r_ptr);
      return true;
    }

    *r_ptr = PointerRNA_NULL;
    return false;
  }

  bke::AttributeStorage &storage = *owner.get_storage();
  bke::Attribute *attr = storage.lookup(key);
  if (!attr) {
    *r_ptr = PointerRNA_NULL;
    return false;
  }
  rna_pointer_create_with_ancestors(*ptr, &RNA_Attribute, attr, *r_ptr);
  return true;
}

static int rna_AttributeGroupID_active_index_get(PointerRNA *ptr)
{
  AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
  return *BKE_attributes_active_index_p(owner);
}

static PointerRNA rna_AttributeGroupID_active_get(PointerRNA *ptr)
{
  using namespace blender;
  AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
  const std::optional<blender::StringRef> name = BKE_attributes_active_name_get(owner);
  if (!name) {
    return PointerRNA_NULL;
  }
  if (owner.type() == AttributeOwnerType::Mesh) {
    CustomDataLayer *layer = BKE_attribute_search_for_write(
        owner, *name, CD_MASK_PROP_ALL, ATTR_DOMAIN_MASK_ALL);
    return RNA_pointer_create_with_parent(*ptr, &RNA_Attribute, layer);
  }

  bke::AttributeStorage &storage = *owner.get_storage();
  bke::Attribute *attr = storage.lookup(*name);
  return RNA_pointer_create_with_parent(*ptr, &RNA_Attribute, attr);
}

static void rna_AttributeGroupID_active_set(PointerRNA *ptr,
                                            PointerRNA attribute_ptr,
                                            ReportList * /*reports*/)
{
  using namespace blender;
  AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
  if (owner.type() == AttributeOwnerType::Mesh) {
    CustomDataLayer *layer = static_cast<CustomDataLayer *>(attribute_ptr.data);
    if (layer) {
      BKE_attributes_active_set(owner, layer->name);
    }
    else {
      BKE_attributes_active_clear(owner);
    }
    return;
  }

  bke::Attribute *attr = attribute_ptr.data_as<bke::Attribute>();
  BKE_attributes_active_set(owner, attr->name());
}

static void rna_AttributeGroupID_active_index_set(PointerRNA *ptr, int value)
{
  AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
  *BKE_attributes_active_index_p(owner) = std::max(-1, value);
}

static void rna_AttributeGroupID_active_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  *min = -1;
  *max = rna_AttributeGroup_length(ptr);

  *softmin = *min;
  *softmax = *max;
}

static void rna_AttributeGroup_update_active(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Attribute_update_data(bmain, scene, ptr);
}

static void rna_AttributeGroup_update_active_color(Main * /*bmain*/,
                                                   Scene * /*scene*/,
                                                   PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  /* Cheating way for importers to avoid slow updates. */
  if (id->us > 0) {
    DEG_id_tag_update(id, 0);
    WM_main_add_notifier(NC_GEOM | ND_DATA, id);
  }
}

static int rna_AttributeGroupID_domain_size(ID *id, const int domain)
{
  using namespace blender;
  AttributeOwner owner = AttributeOwner::from_id(id);
  if (owner.type() == AttributeOwnerType::Mesh) {
    return BKE_attribute_domain_size(owner, domain);
  }

  bke::AttributeAccessor attributes = *owner.get_accessor();
  return attributes.domain_size(bke::AttrDomain(domain));
}

static PointerRNA rna_AttributeGroupMesh_active_color_get(PointerRNA *ptr)
{
  AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
  CustomDataLayer *layer = BKE_attribute_search_for_write(
      owner,
      BKE_id_attributes_active_color_name(ptr->owner_id).value_or(""),
      CD_MASK_COLOR_ALL,
      ATTR_DOMAIN_MASK_COLOR);

  PointerRNA attribute_ptr = RNA_pointer_create_discrete(ptr->owner_id, &RNA_Attribute, layer);
  return attribute_ptr;
}

static void rna_AttributeGroupMesh_active_color_set(PointerRNA *ptr,
                                                    PointerRNA attribute_ptr,
                                                    ReportList * /*reports*/)
{
  ID *id = ptr->owner_id;
  CustomDataLayer *layer = static_cast<CustomDataLayer *>(attribute_ptr.data);
  if (layer) {
    BKE_id_attributes_active_color_set(id, layer->name);
  }
  else {
    BKE_id_attributes_active_color_clear(id);
  }
}

static int rna_AttributeGroupMesh_active_color_index_get(PointerRNA *ptr)
{
  AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
  const CustomDataLayer *layer = BKE_attribute_search(
      owner,
      BKE_id_attributes_active_color_name(ptr->owner_id).value_or(""),
      CD_MASK_COLOR_ALL,
      ATTR_DOMAIN_MASK_COLOR);

  return BKE_attribute_to_index(owner, layer, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);
}

static void rna_AttributeGroupMesh_active_color_index_set(PointerRNA *ptr, int value)
{
  AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
  CustomDataLayer *layer = BKE_attribute_from_index(
      owner, value, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);

  if (!layer) {
    fprintf(stderr, "%s: error setting active color index to %d\n", __func__, value);
    return;
  }

  BKE_id_attributes_active_color_set(ptr->owner_id, layer->name);
}

static void rna_AttributeGroupMesh_active_color_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
  *min = 0;
  *max = BKE_attributes_length(owner, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);

  *softmin = *min;
  *softmax = *max;
}

static int rna_AttributeGroupMesh_render_color_index_get(PointerRNA *ptr)
{
  AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
  const CustomDataLayer *layer = BKE_id_attributes_color_find(
      ptr->owner_id, BKE_id_attributes_default_color_name(ptr->owner_id).value_or(""));

  return BKE_attribute_to_index(owner, layer, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);
}

static void rna_AttributeGroupMesh_render_color_index_set(PointerRNA *ptr, int value)
{
  AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
  CustomDataLayer *layer = BKE_attribute_from_index(
      owner, value, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);

  if (!layer) {
    fprintf(stderr, "%s: error setting render color index to %d\n", __func__, value);
    return;
  }

  BKE_id_attributes_default_color_set(ptr->owner_id, layer->name);
}

static void rna_AttributeGroupMesh_render_color_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  AttributeOwner owner = AttributeOwner::from_id(ptr->owner_id);
  *min = 0;
  *max = BKE_attributes_length(owner, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL);

  *softmin = *min;
  *softmax = *max;
}

static void rna_AttributeGroupMesh_default_color_name_get(PointerRNA *ptr, char *value)
{
  const ID *id = ptr->owner_id;
  const StringRef name = BKE_id_attributes_default_color_name(id).value_or("");
  name.copy_unsafe(value);
}

static int rna_AttributeGroupMesh_default_color_name_length(PointerRNA *ptr)
{
  const ID *id = ptr->owner_id;
  const StringRef name = BKE_id_attributes_default_color_name(id).value_or("");
  return name.size();
}

static void rna_AttributeGroupMesh_default_color_name_set(PointerRNA *ptr, const char *value)
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

static void rna_AttributeGroupMesh_active_color_name_get(PointerRNA *ptr, char *value)
{
  const ID *id = ptr->owner_id;
  const StringRef name = BKE_id_attributes_active_color_name(id).value_or("");
  name.copy_unsafe(value);
}

static int rna_AttributeGroupMesh_active_color_name_length(PointerRNA *ptr)
{
  const ID *id = ptr->owner_id;
  const StringRef name = BKE_id_attributes_active_color_name(id).value_or("");
  return name.size();
}

static void rna_AttributeGroupMesh_active_color_name_set(PointerRNA *ptr, const char *value)
{
  ID *id = ptr->owner_id;
  if (GS(id->name) == ID_ME) {
    Mesh *mesh = (Mesh *)id;
    MEM_SAFE_FREE(mesh->active_color_attribute);
    if (value[0]) {
      mesh->active_color_attribute = BLI_strdup(value);
    }
  }
}

static PointerRNA rna_AttributeGroupGreasePencilDrawing_new(ID *grease_pencil_id,
                                                            GreasePencilDrawing *drawing,
                                                            ReportList *reports,
                                                            const char *name,
                                                            const int type,
                                                            const int domain)
{
  using namespace blender;
  AttributeOwner owner = AttributeOwner(AttributeOwnerType::GreasePencilDrawing, drawing);
  const bke::AttributeAccessor accessor = *owner.get_accessor();
  if (!accessor.domain_supported(AttrDomain(domain))) {
    BKE_report(reports, RPT_ERROR, "Attribute domain not supported by this geometry type");
    return PointerRNA_NULL;
  }
  const int domain_size = accessor.domain_size(AttrDomain(domain));

  bke::AttributeStorage &attributes = *owner.get_storage();
  const CPPType &cpp_type = *bke::custom_data_type_to_cpp_type(eCustomDataType(type));
  bke::Attribute &attr = attributes.add(
      attributes.unique_name_calc(name),
      AttrDomain(domain),
      *bke::custom_data_type_to_attr_type(eCustomDataType(type)),
      bke::Attribute::ArrayData::from_default_value(cpp_type, domain_size));

  DEG_id_tag_update(grease_pencil_id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, grease_pencil_id);

  return RNA_pointer_create_discrete(grease_pencil_id, &RNA_Attribute, &attr);
}

static void rna_AttributeGroupGreasePencilDrawing_remove(ID *grease_pencil_id,
                                                         GreasePencilDrawing *drawing,
                                                         ReportList *reports,
                                                         PointerRNA *attribute_ptr)
{
  using namespace blender;
  AttributeOwner owner = AttributeOwner(AttributeOwnerType::GreasePencilDrawing, drawing);
  const bke::Attribute *attr = static_cast<const bke::Attribute *>(attribute_ptr->data);
  if (BKE_attribute_required(owner, attr->name())) {
    BKE_report(reports, RPT_ERROR, "Attribute is required and cannot be removed");
    return;
  }

  bke::MutableAttributeAccessor accessor = *owner.get_accessor();
  accessor.remove(attr->name());
  attribute_ptr->invalidate();

  DEG_id_tag_update(grease_pencil_id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, grease_pencil_id);
}

static PointerRNA rna_AttributeGroupGreasePencilDrawing_active_get(PointerRNA *ptr)
{
  using namespace blender;
  GreasePencilDrawing *drawing = static_cast<GreasePencilDrawing *>(ptr->data);
  AttributeOwner owner = AttributeOwner(AttributeOwnerType::GreasePencilDrawing, drawing);
  const std::optional<blender::StringRef> name = BKE_attributes_active_name_get(owner);
  if (!name) {
    return PointerRNA_NULL;
  }
  bke::AttributeStorage &storage = *owner.get_storage();
  bke::Attribute *attr = storage.lookup(*name);
  return RNA_pointer_create_discrete(ptr->owner_id, &RNA_Attribute, attr);
}

static void rna_AttributeGroupGreasePencilDrawing_active_set(PointerRNA *ptr,
                                                             PointerRNA attribute_ptr,
                                                             ReportList * /*reports*/)
{
  GreasePencilDrawing *drawing = static_cast<GreasePencilDrawing *>(ptr->data);
  AttributeOwner owner = AttributeOwner(AttributeOwnerType::GreasePencilDrawing, drawing);
  CustomDataLayer *layer = static_cast<CustomDataLayer *>(attribute_ptr.data);
  if (layer) {
    BKE_attributes_active_set(owner, layer->name);
  }
  else {
    BKE_attributes_active_clear(owner);
  }
}

static bool rna_AttributeGroupGreasePencilDrawing_active_poll(PointerRNA *ptr,
                                                              const PointerRNA value)
{
  AttributeOwner owner = owner_from_attribute_pointer_rna(const_cast<PointerRNA *>(&value));
  return owner.is_valid() && owner.type() == AttributeOwnerType::GreasePencilDrawing &&
         owner.get_grease_pencil_drawing() == static_cast<GreasePencilDrawing *>(ptr->data);
}

static int rna_AttributeGroupGreasePencilDrawing_active_index_get(PointerRNA *ptr)
{
  GreasePencilDrawing *drawing = static_cast<GreasePencilDrawing *>(ptr->data);
  AttributeOwner owner = AttributeOwner(AttributeOwnerType::GreasePencilDrawing, drawing);
  return *BKE_attributes_active_index_p(owner);
}

static void rna_AttributeGroupGreasePencilDrawing_active_index_set(PointerRNA *ptr, int value)
{
  GreasePencilDrawing *drawing = static_cast<GreasePencilDrawing *>(ptr->data);
  AttributeOwner owner = AttributeOwner(AttributeOwnerType::GreasePencilDrawing, drawing);
  *BKE_attributes_active_index_p(owner) = std::max(-1, value);
}

static void rna_AttributeGroupGreasePencilDrawing_active_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  GreasePencilDrawing *drawing = static_cast<GreasePencilDrawing *>(ptr->data);
  AttributeOwner owner = AttributeOwner(AttributeOwnerType::GreasePencilDrawing, drawing);
  *min = -1;
  *max = owner.get_storage()->count();

  *softmin = *min;
  *softmax = *max;
}

static int rna_AttributeGroupGreasePencilDrawing_domain_size(GreasePencilDrawing *drawing,
                                                             const int domain)
{
  AttributeOwner owner = AttributeOwner(AttributeOwnerType::GreasePencilDrawing, drawing);
  return owner.get_accessor()->domain_size(blender::bke::AttrDomain(domain));
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
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  srna = RNA_def_struct(brna, "FloatAttributeValue", nullptr);
  RNA_def_struct_sdna(srna, "MFloatProperty");
  RNA_def_struct_ui_text(
      srna, "Float Attribute Value", "Floating-point value in geometry attribute");
  prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "f");
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
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  /* Float Vector Attribute Value */
  srna = RNA_def_struct(brna, "FloatVectorAttributeValue", nullptr);
  RNA_def_struct_sdna(srna, "vec3f");
  RNA_def_struct_ui_text(
      srna, "Float Vector Attribute Value", "Vector value in geometry attribute");

  prop = RNA_def_property(srna, "vector", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_ui_text(prop, "Vector", "3D vector");
  RNA_def_property_float_sdna(prop, nullptr, "x");
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
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  /* Float Color Attribute Value */
  srna = RNA_def_struct(brna, "FloatColorAttributeValue", nullptr);
  RNA_def_struct_sdna(srna, "MPropCol");
  RNA_def_struct_ui_text(srna, "Float Color Attribute Value", "Color value in geometry attribute");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_ui_text(prop, "Color", "RGBA color in scene linear color space");
  RNA_def_property_float_sdna(prop, nullptr, "color");
  RNA_def_property_array(prop, 4);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  prop = RNA_def_property(srna, "color_srgb", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_ui_text(prop, "Color", "RGBA color in sRGB color space");
  RNA_def_property_float_sdna(prop, nullptr, "color");
  RNA_def_property_array(prop, 4);
  RNA_def_property_float_funcs(prop,
                               "rna_FloatColorAttributeValue_color_srgb_get",
                               "rna_FloatColorAttributeValue_color_srgb_set",
                               nullptr);
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
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  /* Byte Color Attribute Value */
  srna = RNA_def_struct(brna, "ByteColorAttributeValue", nullptr);
  RNA_def_struct_sdna(srna, "MLoopCol");
  RNA_def_struct_ui_text(srna, "Byte Color Attribute Value", "Color value in geometry attribute");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_funcs(prop,
                               "rna_ByteColorAttributeValue_color_get",
                               "rna_ByteColorAttributeValue_color_set",
                               nullptr);
  RNA_def_property_ui_text(prop, "Color", "RGBA color in scene linear color space");
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  prop = RNA_def_property(srna, "color_srgb", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_funcs(prop,
                               "rna_ByteColorAttributeValue_color_srgb_get",
                               "rna_ByteColorAttributeValue_color_srgb_set",
                               nullptr);
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
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  srna = RNA_def_struct(brna, "IntAttributeValue", nullptr);
  RNA_def_struct_sdna(srna, "MIntProperty");
  RNA_def_struct_ui_text(srna, "Integer Attribute Value", "Integer value in geometry attribute");
  prop = RNA_def_property(srna, "value", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "i");
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
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  srna = RNA_def_struct(brna, "StringAttributeValue", nullptr);
  RNA_def_struct_sdna(srna, "MStringProperty");
  RNA_def_struct_ui_text(srna, "String Attribute Value", "String value in geometry attribute");
  prop = RNA_def_property(srna, "value", PROP_STRING, PROP_BYTESTRING);
  RNA_def_property_string_sdna(prop, nullptr, "s");
  RNA_def_property_string_funcs(prop,
                                "rna_StringAttributeValue_s_get",
                                "rna_StringAttributeValue_s_length",
                                "rna_StringAttributeValue_s_set");
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
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  srna = RNA_def_struct(brna, "BoolAttributeValue", nullptr);
  RNA_def_struct_sdna(srna, "MBoolProperty");
  RNA_def_struct_ui_text(srna, "Bool Attribute Value", "Bool value in geometry attribute");
  prop = RNA_def_property(srna, "value", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "b", 0);
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
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  srna = RNA_def_struct(brna, "ByteIntAttributeValue", nullptr);
  RNA_def_struct_sdna(srna, "MInt8Property");
  RNA_def_struct_ui_text(
      srna, "8-bit Integer Attribute Value", "8-bit value in geometry attribute");
  prop = RNA_def_property(srna, "value", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "i");
}

static void rna_def_attribute_short2(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Short2Attribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(srna,
                         "2D 16-Bit Integer Vector Attribute",
                         "Geometry attribute that stores 2D integer vectors");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Short2AttributeValue");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  srna = RNA_def_struct(brna, "Short2AttributeValue", nullptr);
  RNA_def_struct_sdna(srna, "vec2s");
  RNA_def_struct_ui_text(
      srna, "2D 16-Bit Integer Vector Attribute Value", "2D value in geometry attribute");

  prop = RNA_def_property(srna, "value", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Vector", "2D vector");
  RNA_def_property_int_sdna(prop, nullptr, "x");
  RNA_def_property_array(prop, 2);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");
}

static void rna_def_attribute_int2(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Int2Attribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(
      srna, "2D Integer Vector Attribute", "Geometry attribute that stores 2D integer vectors");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Int2AttributeValue");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  srna = RNA_def_struct(brna, "Int2AttributeValue", nullptr);
  RNA_def_struct_sdna(srna, "vec2i");
  RNA_def_struct_ui_text(
      srna, "2D Integer Vector Attribute Value", "2D value in geometry attribute");

  prop = RNA_def_property(srna, "value", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Vector", "2D vector");
  RNA_def_property_int_sdna(prop, nullptr, "x");
  RNA_def_property_array(prop, 2);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");
}

static void rna_def_attribute_quaternion(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "QuaternionAttribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(srna, "Quaternion Attribute", "Geometry attribute that stores rotation");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "QuaternionAttributeValue");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  srna = RNA_def_struct(brna, "QuaternionAttributeValue", nullptr);
  RNA_def_struct_sdna(srna, "vec4f");
  RNA_def_struct_ui_text(
      srna, "Quaternion Attribute Value", "Rotation value in geometry attribute");

  prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Value", "Quaternion");
  RNA_def_property_float_sdna(prop, nullptr, "x");
  RNA_def_property_array(prop, 4);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");
}

static void rna_def_attribute_float4x4(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Float4x4Attribute", "Attribute");
  RNA_def_struct_sdna(srna, "CustomDataLayer");
  RNA_def_struct_ui_text(
      srna, "4x4 Matrix Attribute", "Geometry attribute that stores a 4 by 4 float matrix");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Float4x4AttributeValue");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  srna = RNA_def_struct(brna, "Float4x4AttributeValue", nullptr);
  RNA_def_struct_sdna(srna, "mat4x4f");
  RNA_def_struct_ui_text(srna, "Matrix Attribute Value", "Matrix value in geometry attribute");

  prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_ui_text(prop, "Value", "Matrix");
  RNA_def_property_float_sdna(prop, nullptr, "value");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");
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
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Attribute_data_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Attribute_data_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");

  /* Float2 Attribute Value */
  srna = RNA_def_struct(brna, "Float2AttributeValue", nullptr);
  RNA_def_struct_sdna(srna, "vec2f");
  RNA_def_struct_ui_text(srna, "Float2 Attribute Value", "2D Vector value in geometry attribute");

  prop = RNA_def_property(srna, "vector", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_ui_text(prop, "Vector", "2D vector");
  RNA_def_property_float_sdna(prop, nullptr, "x");
  RNA_def_property_array(prop, 2);
  RNA_def_property_update(prop, 0, "rna_Attribute_update_data");
}

static void rna_def_attribute(BlenderRNA *brna)
{
  PropertyRNA *prop;
  StructRNA *srna;

  srna = RNA_def_struct(brna, "Attribute", nullptr);
  RNA_def_struct_ui_text(srna, "Attribute", "Geometry attribute");
  RNA_def_struct_path_func(srna, "rna_Attribute_path");
  RNA_def_struct_refine_func(srna, "rna_Attribute_refine");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_Attribute_name_get", "rna_Attribute_name_length", "rna_Attribute_name_set");
  RNA_def_property_editable_func(prop, "rna_Attribute_name_editable");
  RNA_def_property_ui_text(prop, "Name", "Name of the Attribute");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "data_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_type_items);
  RNA_def_property_enum_funcs(prop, "rna_Attribute_type_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Data Type", "Type of data stored in attribute");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "storage_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attr_storage_type);
  RNA_def_property_enum_funcs(prop, "rna_Attribute_storage_type_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Storage Type", "Method used to store the data");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_AMOUNT);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_domain_items);
  RNA_def_property_enum_funcs(
      prop, "rna_Attribute_domain_get", nullptr, "rna_Attribute_domain_itemf");
  RNA_def_property_ui_text(prop, "Domain", "Domain of the Attribute");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_internal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Attribute_is_internal_get", nullptr);
  RNA_def_property_ui_text(
      prop, "Is Internal", "The attribute is meant for internal use by Blender");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_required", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Attribute_is_required_get", nullptr);
  RNA_def_property_ui_text(prop, "Is Required", "Whether the attribute can be removed or renamed");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* types */
  rna_def_attribute_float(brna);
  rna_def_attribute_float_vector(brna);
  rna_def_attribute_float_color(brna);
  rna_def_attribute_byte_color(brna);
  rna_def_attribute_int(brna);
  rna_def_attribute_short2(brna);
  rna_def_attribute_int2(brna);
  rna_def_attribute_quaternion(brna);
  rna_def_attribute_float4x4(brna);
  rna_def_attribute_string(brna);
  rna_def_attribute_bool(brna);
  rna_def_attribute_float2(brna);
  rna_def_attribute_int8(brna);
}

static void rna_def_attribute_group_id_common(StructRNA *srna)
{
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  /* API */
  func = RNA_def_function(srna, "new", "rna_AttributeGroupID_new");
  RNA_def_function_ui_description(func, "Add attribute to geometry");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "name", "Attribute", 0, "Name", "Name of geometry attribute");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  parm = RNA_def_enum(
      func, "type", rna_enum_attribute_type_items, CD_PROP_FLOAT, "Type", "Attribute type");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  parm = RNA_def_enum(func,
                      "domain",
                      rna_enum_attribute_domain_items,
                      int(AttrDomain::Point),
                      "Domain",
                      "Type of element that attribute is stored on");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  parm = RNA_def_pointer(func, "attribute", "Attribute", "", "New geometry attribute");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_AttributeGroupID_remove");
  RNA_def_function_ui_description(func, "Remove attribute from geometry");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "attribute", "Attribute", "", "Geometry Attribute");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  /* Active */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Attribute");
  RNA_def_property_ui_text(prop, "Active Attribute", "Active attribute");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_AttributeGroupID_active_get",
                                 "rna_AttributeGroupID_active_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_update(prop, 0, "rna_AttributeGroup_update_active");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Active Attribute Index", "Active attribute index or -1 when none are active");
  RNA_def_property_range(prop, -1, INT_MAX);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_AttributeGroupID_active_index_get",
                             "rna_AttributeGroupID_active_index_set",
                             "rna_AttributeGroupID_active_index_range");
  RNA_def_property_update(prop, 0, "rna_AttributeGroup_update_active");

  /* Domain Size */
  func = RNA_def_function(srna, "domain_size", "rna_AttributeGroupID_domain_size");
  RNA_def_function_ui_description(func, "Get the size of a given domain");
  parm = RNA_def_enum(func,
                      "domain",
                      rna_enum_attribute_domain_items,
                      int(AttrDomain::Point),
                      "Domain",
                      "Type of element that attribute is stored on");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "size", 0, 0, INT_MAX, "Size", "Size of the domain", 0, INT_MAX);
  RNA_def_function_return(func, parm);
}

static void rna_def_attribute_group_mesh(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "AttributeGroupMesh", nullptr);
  RNA_def_struct_ui_text(srna, "Attribute Group", "Group of geometry attributes");
  /* Define `AttributeGroupMesh` to be of type `ID` so we can reuse the generic ID `AttributeGroup`
   * functions. */
  RNA_def_struct_sdna(srna, "ID");

  rna_def_attribute_group_id_common(srna);

  prop = RNA_def_property(srna, "active_color", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Attribute");
  RNA_def_property_ui_text(prop, "Active Color", "Active color attribute for display and editing");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_AttributeGroupMesh_active_color_get",
                                 "rna_AttributeGroupMesh_active_color_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_update(prop, 0, "rna_AttributeGroup_update_active_color");

  prop = RNA_def_property(srna, "active_color_index", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Active Color Index", "Active color attribute index");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_AttributeGroupMesh_active_color_index_get",
                             "rna_AttributeGroupMesh_active_color_index_set",
                             "rna_AttributeGroupMesh_active_color_index_range");
  RNA_def_property_update(prop, 0, "rna_AttributeGroup_update_active_color");

  prop = RNA_def_property(srna, "render_color_index", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Active Render Color Index",
                           "The index of the color attribute used as a fallback for rendering");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_AttributeGroupMesh_render_color_index_get",
                             "rna_AttributeGroupMesh_render_color_index_set",
                             "rna_AttributeGroupMesh_render_color_index_range");
  RNA_def_property_update(prop, 0, "rna_AttributeGroup_update_active_color");

  prop = RNA_def_property(srna, "default_color_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_maxlength(prop, MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX);
  RNA_def_property_string_funcs(prop,
                                "rna_AttributeGroupMesh_default_color_name_get",
                                "rna_AttributeGroupMesh_default_color_name_length",
                                "rna_AttributeGroupMesh_default_color_name_set");
  RNA_def_property_ui_text(
      prop,
      "Default Color Attribute",
      "The name of the default color attribute used as a fallback for rendering");

  prop = RNA_def_property(srna, "active_color_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_maxlength(prop, MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX);
  RNA_def_property_string_funcs(prop,
                                "rna_AttributeGroupMesh_active_color_name_get",
                                "rna_AttributeGroupMesh_active_color_name_length",
                                "rna_AttributeGroupMesh_active_color_name_set");
  RNA_def_property_ui_text(prop,
                           "Active Color Attribute",
                           "The name of the active color attribute for display and editing");
}

static void rna_def_attribute_group_pointcloud(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "AttributeGroupPointCloud", nullptr);
  RNA_def_struct_ui_text(srna, "Attribute Group", "Group of geometry attributes");
  RNA_def_struct_sdna(srna, "ID");

  rna_def_attribute_group_id_common(srna);
}

static void rna_def_attribute_group_curves(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "AttributeGroupCurves", nullptr);
  RNA_def_struct_ui_text(srna, "Attribute Group", "Group of geometry attributes");
  RNA_def_struct_sdna(srna, "ID");

  rna_def_attribute_group_id_common(srna);
}

static void rna_def_attribute_group_grease_pencil(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "AttributeGroupGreasePencil", nullptr);
  RNA_def_struct_ui_text(srna, "Attribute Group", "Group of geometry attributes");
  RNA_def_struct_sdna(srna, "ID");

  rna_def_attribute_group_id_common(srna);
}

static void rna_def_attribute_group_grease_pencil_drawing(BlenderRNA *brna)
{
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;
  StructRNA *srna;

  srna = RNA_def_struct(brna, "AttributeGroupGreasePencilDrawing", nullptr);
  RNA_def_struct_ui_text(srna, "Attribute Group", "Group of geometry attributes");
  RNA_def_struct_sdna(srna, "GreasePencilDrawing");

  /* API */
  func = RNA_def_function(srna, "new", "rna_AttributeGroupGreasePencilDrawing_new");
  RNA_def_function_ui_description(func, "Add attribute to geometry");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "name", "Attribute", 0, "Name", "Name of geometry attribute");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  parm = RNA_def_enum(
      func, "type", rna_enum_attribute_type_items, CD_PROP_FLOAT, "Type", "Attribute type");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  parm = RNA_def_enum(func,
                      "domain",
                      rna_enum_attribute_domain_items,
                      int(AttrDomain::Point),
                      "Domain",
                      "Type of element that attribute is stored on");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  parm = RNA_def_pointer(func, "attribute", "Attribute", "", "New geometry attribute");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_AttributeGroupGreasePencilDrawing_remove");
  RNA_def_function_ui_description(func, "Remove attribute from geometry");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "attribute", "Attribute", "", "Geometry Attribute");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  /* Active */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Attribute");
  RNA_def_property_ui_text(prop, "Active Attribute", "Active attribute");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_AttributeGroupGreasePencilDrawing_active_get",
                                 "rna_AttributeGroupGreasePencilDrawing_active_set",
                                 nullptr,
                                 "rna_AttributeGroupGreasePencilDrawing_active_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_update(prop, 0, "rna_AttributeGroup_update_active");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Active Attribute Index", "Active attribute index or -1 when none are active");
  RNA_def_property_range(prop, -1, INT_MAX);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_AttributeGroupGreasePencilDrawing_active_index_get",
                             "rna_AttributeGroupGreasePencilDrawing_active_index_set",
                             "rna_AttributeGroupGreasePencilDrawing_active_index_range");
  RNA_def_property_update(prop, 0, "rna_AttributeGroup_update_active");

  /* Domain Size */
  func = RNA_def_function(
      srna, "domain_size", "rna_AttributeGroupGreasePencilDrawing_domain_size");
  RNA_def_function_ui_description(func, "Get the size of a given domain");
  parm = RNA_def_enum(func,
                      "domain",
                      rna_enum_attribute_domain_items,
                      int(AttrDomain::Point),
                      "Domain",
                      "Type of element that attribute is stored on");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "size", 0, 0, INT_MAX, "Size", "Size of the domain", 0, INT_MAX);
  RNA_def_function_return(func, parm);
}

void rna_def_attributes_common(StructRNA *srna, const AttributeOwnerType type)
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
                                    nullptr,
                                    "rna_AttributeGroup_lookup_string",
                                    nullptr);
  RNA_def_property_struct_type(prop, "Attribute");
  RNA_def_property_ui_text(prop, "Attributes", "Geometry attributes");
  switch (type) {
    case AttributeOwnerType::Mesh:
      RNA_def_property_srna(prop, "AttributeGroupMesh");
      break;
    case AttributeOwnerType::PointCloud:
      RNA_def_property_srna(prop, "AttributeGroupPointCloud");
      break;
    case AttributeOwnerType::Curves:
      RNA_def_property_srna(prop, "AttributeGroupCurves");
      break;
    case AttributeOwnerType::GreasePencil:
      RNA_def_property_srna(prop, "AttributeGroupGreasePencil");
      break;
    case AttributeOwnerType::GreasePencilDrawing:
      RNA_def_property_srna(prop, "AttributeGroupGreasePencilDrawing");
      break;
  }

  prop = RNA_def_property(srna, "color_attributes", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_AttributeGroup_color_iterator_begin",
                                    "rna_AttributeGroup_color_iterator_next",
                                    "rna_iterator_array_end",
                                    "rna_AttributeGroup_color_iterator_get",
                                    "rna_AttributeGroup_color_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "Attribute");
  RNA_def_property_ui_text(prop, "Color Attributes", "Geometry color attributes");
  switch (type) {
    case AttributeOwnerType::Mesh:
      RNA_def_property_srna(prop, "AttributeGroupMesh");
      break;
    case AttributeOwnerType::PointCloud:
      RNA_def_property_srna(prop, "AttributeGroupPointCloud");
      break;
    case AttributeOwnerType::Curves:
      RNA_def_property_srna(prop, "AttributeGroupCurves");
      break;
    case AttributeOwnerType::GreasePencil:
      RNA_def_property_srna(prop, "AttributeGroupGreasePencil");
      break;
    case AttributeOwnerType::GreasePencilDrawing:
      RNA_def_property_srna(prop, "AttributeGroupGreasePencilDrawing");
      break;
  }
}

void RNA_def_attribute(BlenderRNA *brna)
{
  rna_def_attribute(brna);
  rna_def_attribute_group_mesh(brna);
  rna_def_attribute_group_pointcloud(brna);
  rna_def_attribute_group_curves(brna);
  rna_def_attribute_group_grease_pencil(brna);
  rna_def_attribute_group_grease_pencil_drawing(brna);
}
#endif
