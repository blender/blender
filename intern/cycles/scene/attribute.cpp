/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "scene/attribute.h"
#include "scene/hair.h"
#include "scene/image.h"
#include "scene/mesh.h"
#include "scene/pointcloud.h"

#include "util/foreach.h"
#include "util/log.h"
#include "util/transform.h"

CCL_NAMESPACE_BEGIN

/* Attribute */

Attribute::Attribute(
    ustring name, TypeDesc type, AttributeElement element, Geometry *geom, AttributePrimitive prim)
    : name(name), std(ATTR_STD_NONE), type(type), element(element), flags(0), modified(true)
{
  /* string and matrix not supported! */
  assert(type == TypeDesc::TypeFloat || type == TypeDesc::TypeColor ||
         type == TypeDesc::TypePoint || type == TypeDesc::TypeVector ||
         type == TypeDesc::TypeNormal || type == TypeDesc::TypeMatrix || type == TypeFloat2 ||
         type == TypeFloat4 || type == TypeRGBA);

  if (element == ATTR_ELEMENT_VOXEL) {
    buffer.resize(sizeof(ImageHandle));
    new (buffer.data()) ImageHandle();
  }
  else {
    resize(geom, prim, false);
  }
}

Attribute::~Attribute()
{
  /* For voxel data, we need to free the image handle. */
  if (element == ATTR_ELEMENT_VOXEL && buffer.size()) {
    ImageHandle &handle = data_voxel();
    handle.~ImageHandle();
  }
}

void Attribute::resize(Geometry *geom, AttributePrimitive prim, bool reserve_only)
{
  if (element != ATTR_ELEMENT_VOXEL) {
    if (reserve_only) {
      buffer.reserve(buffer_size(geom, prim));
    }
    else {
      buffer.resize(buffer_size(geom, prim), 0);
    }
  }
}

void Attribute::resize(size_t num_elements)
{
  if (element != ATTR_ELEMENT_VOXEL) {
    buffer.resize(num_elements * data_sizeof(), 0);
  }
}

void Attribute::add(const float &f)
{
  assert(data_sizeof() == sizeof(float));

  char *data = (char *)&f;
  size_t size = sizeof(f);

  for (size_t i = 0; i < size; i++)
    buffer.push_back(data[i]);

  modified = true;
}

void Attribute::add(const uchar4 &f)
{
  assert(data_sizeof() == sizeof(uchar4));

  char *data = (char *)&f;
  size_t size = sizeof(f);

  for (size_t i = 0; i < size; i++)
    buffer.push_back(data[i]);

  modified = true;
}

void Attribute::add(const float2 &f)
{
  assert(data_sizeof() == sizeof(float2));

  char *data = (char *)&f;
  size_t size = sizeof(f);

  for (size_t i = 0; i < size; i++)
    buffer.push_back(data[i]);

  modified = true;
}

void Attribute::add(const float3 &f)
{
  assert(data_sizeof() == sizeof(float3));

  char *data = (char *)&f;
  size_t size = sizeof(f);

  for (size_t i = 0; i < size; i++)
    buffer.push_back(data[i]);

  modified = true;
}

void Attribute::add(const Transform &f)
{
  assert(data_sizeof() == sizeof(Transform));

  char *data = (char *)&f;
  size_t size = sizeof(f);

  for (size_t i = 0; i < size; i++)
    buffer.push_back(data[i]);

  modified = true;
}

void Attribute::add(const char *data)
{
  size_t size = data_sizeof();

  for (size_t i = 0; i < size; i++)
    buffer.push_back(data[i]);

  modified = true;
}

void Attribute::set_data_from(Attribute &&other)
{
  assert(other.std == std);
  assert(other.type == type);
  assert(other.element == element);

  this->flags = other.flags;

  if (this->buffer.size() != other.buffer.size()) {
    this->buffer = std::move(other.buffer);
    modified = true;
  }
  else if (memcmp(this->data(), other.data(), other.buffer.size()) != 0) {
    this->buffer = std::move(other.buffer);
    modified = true;
  }
}

size_t Attribute::data_sizeof() const
{
  if (element == ATTR_ELEMENT_VOXEL)
    return sizeof(ImageHandle);
  else if (element == ATTR_ELEMENT_CORNER_BYTE)
    return sizeof(uchar4);
  else if (type == TypeDesc::TypeFloat)
    return sizeof(float);
  else if (type == TypeFloat2)
    return sizeof(float2);
  else if (type == TypeDesc::TypeMatrix)
    return sizeof(Transform);
  // The float3 type is not interchangeable with float4
  // as it is now a packed type.
  else if (type == TypeDesc::TypeFloat4)
    return sizeof(float4);
  else if (type == TypeRGBA)
    return sizeof(float4);
  else
    return sizeof(float3);
}

size_t Attribute::element_size(Geometry *geom, AttributePrimitive prim) const
{
  if (flags & ATTR_FINAL_SIZE) {
    return buffer.size() / data_sizeof();
  }

  size_t size = 0;

  switch (element) {
    case ATTR_ELEMENT_OBJECT:
    case ATTR_ELEMENT_MESH:
    case ATTR_ELEMENT_VOXEL:
      size = 1;
      break;
    case ATTR_ELEMENT_VERTEX:
      if (geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::VOLUME) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        size = mesh->get_verts().size() + mesh->get_num_ngons();
        if (prim == ATTR_PRIM_SUBD) {
          size -= mesh->get_num_subd_verts();
        }
      }
      else if (geom->geometry_type == Geometry::POINTCLOUD) {
        PointCloud *pointcloud = static_cast<PointCloud *>(geom);
        size = pointcloud->num_points();
      }
      break;
    case ATTR_ELEMENT_VERTEX_MOTION:
      if (geom->geometry_type == Geometry::MESH) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        DCHECK_GT(mesh->get_motion_steps(), 0);
        size = (mesh->get_verts().size() + mesh->get_num_ngons()) * (mesh->get_motion_steps() - 1);
        if (prim == ATTR_PRIM_SUBD) {
          size -= mesh->get_num_subd_verts() * (mesh->get_motion_steps() - 1);
        }
      }
      else if (geom->geometry_type == Geometry::POINTCLOUD) {
        PointCloud *pointcloud = static_cast<PointCloud *>(geom);
        size = pointcloud->num_points() * (pointcloud->get_motion_steps() - 1);
      }
      break;
    case ATTR_ELEMENT_FACE:
      if (geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::VOLUME) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        if (prim == ATTR_PRIM_GEOMETRY) {
          size = mesh->num_triangles();
        }
        else {
          size = mesh->get_num_subd_faces() + mesh->get_num_ngons();
        }
      }
      break;
    case ATTR_ELEMENT_CORNER:
    case ATTR_ELEMENT_CORNER_BYTE:
      if (geom->geometry_type == Geometry::MESH) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        if (prim == ATTR_PRIM_GEOMETRY) {
          size = mesh->num_triangles() * 3;
        }
        else {
          size = mesh->get_subd_face_corners().size() + mesh->get_num_ngons();
        }
      }
      break;
    case ATTR_ELEMENT_CURVE:
      if (geom->geometry_type == Geometry::HAIR) {
        Hair *hair = static_cast<Hair *>(geom);
        size = hair->num_curves();
      }
      break;
    case ATTR_ELEMENT_CURVE_KEY:
      if (geom->geometry_type == Geometry::HAIR) {
        Hair *hair = static_cast<Hair *>(geom);
        size = hair->get_curve_keys().size();
      }
      break;
    case ATTR_ELEMENT_CURVE_KEY_MOTION:
      if (geom->geometry_type == Geometry::HAIR) {
        Hair *hair = static_cast<Hair *>(geom);
        DCHECK_GT(hair->get_motion_steps(), 0);
        size = hair->get_curve_keys().size() * (hair->get_motion_steps() - 1);
      }
      break;
    default:
      break;
  }

  return size;
}

size_t Attribute::buffer_size(Geometry *geom, AttributePrimitive prim) const
{
  return element_size(geom, prim) * data_sizeof();
}

bool Attribute::same_storage(TypeDesc a, TypeDesc b)
{
  if (a == b)
    return true;

  if (a == TypeDesc::TypeColor || a == TypeDesc::TypePoint || a == TypeDesc::TypeVector ||
      a == TypeDesc::TypeNormal)
  {
    if (b == TypeDesc::TypeColor || b == TypeDesc::TypePoint || b == TypeDesc::TypeVector ||
        b == TypeDesc::TypeNormal)
    {
      return true;
    }
  }
  return false;
}

void Attribute::zero_data(void *dst)
{
  memset(dst, 0, data_sizeof());
}

void Attribute::add_with_weight(void *dst, void *src, float weight)
{
  if (element == ATTR_ELEMENT_CORNER_BYTE) {
    for (int i = 0; i < 4; i++) {
      ((uchar *)dst)[i] += uchar(((uchar *)src)[i] * weight);
    }
  }
  else if (same_storage(type, TypeDesc::TypeFloat)) {
    *((float *)dst) += *((float *)src) * weight;
  }
  else if (same_storage(type, TypeFloat2)) {
    *((float2 *)dst) += *((float2 *)src) * weight;
  }
  else if (same_storage(type, TypeDesc::TypeVector)) {
    // Points are float3s and not float4s
    *((float3 *)dst) += *((float3 *)src) * weight;
  }
  else {
    assert(!"not implemented for this type");
  }
}

const char *Attribute::standard_name(AttributeStandard std)
{
  switch (std) {
    case ATTR_STD_VERTEX_NORMAL:
      return "N";
    case ATTR_STD_FACE_NORMAL:
      return "Ng";
    case ATTR_STD_UV:
      return "uv";
    case ATTR_STD_GENERATED:
      return "generated";
    case ATTR_STD_GENERATED_TRANSFORM:
      return "generated_transform";
    case ATTR_STD_UV_TANGENT:
      return "tangent";
    case ATTR_STD_UV_TANGENT_SIGN:
      return "tangent_sign";
    case ATTR_STD_VERTEX_COLOR:
      return "vertex_color";
    case ATTR_STD_POSITION_UNDEFORMED:
      return "undeformed";
    case ATTR_STD_POSITION_UNDISPLACED:
      return "undisplaced";
    case ATTR_STD_MOTION_VERTEX_POSITION:
      return "motion_P";
    case ATTR_STD_MOTION_VERTEX_NORMAL:
      return "motion_N";
    case ATTR_STD_PARTICLE:
      return "particle";
    case ATTR_STD_CURVE_INTERCEPT:
      return "curve_intercept";
    case ATTR_STD_CURVE_LENGTH:
      return "curve_length";
    case ATTR_STD_CURVE_RANDOM:
      return "curve_random";
    case ATTR_STD_POINT_RANDOM:
      return "point_random";
    case ATTR_STD_PTEX_FACE_ID:
      return "ptex_face_id";
    case ATTR_STD_PTEX_UV:
      return "ptex_uv";
    case ATTR_STD_VOLUME_DENSITY:
      return "density";
    case ATTR_STD_VOLUME_COLOR:
      return "color";
    case ATTR_STD_VOLUME_FLAME:
      return "flame";
    case ATTR_STD_VOLUME_HEAT:
      return "heat";
    case ATTR_STD_VOLUME_TEMPERATURE:
      return "temperature";
    case ATTR_STD_VOLUME_VELOCITY:
      return "velocity";
    case ATTR_STD_VOLUME_VELOCITY_X:
      return "velocity_x";
    case ATTR_STD_VOLUME_VELOCITY_Y:
      return "velocity_y";
    case ATTR_STD_VOLUME_VELOCITY_Z:
      return "velocity_z";
    case ATTR_STD_POINTINESS:
      return "pointiness";
    case ATTR_STD_RANDOM_PER_ISLAND:
      return "random_per_island";
    case ATTR_STD_SHADOW_TRANSPARENCY:
      return "shadow_transparency";
    case ATTR_STD_NOT_FOUND:
    case ATTR_STD_NONE:
    case ATTR_STD_NUM:
      return "";
  }

  return "";
}

AttributeStandard Attribute::name_standard(const char *name)
{
  if (name) {
    for (int std = ATTR_STD_NONE; std < ATTR_STD_NUM; std++) {
      if (strcmp(name, Attribute::standard_name((AttributeStandard)std)) == 0) {
        return (AttributeStandard)std;
      }
    }
  }

  return ATTR_STD_NONE;
}

AttrKernelDataType Attribute::kernel_type(const Attribute &attr)
{
  if (attr.element == ATTR_ELEMENT_CORNER) {
    return AttrKernelDataType::UCHAR4;
  }

  if (attr.type == TypeDesc::TypeFloat) {
    return AttrKernelDataType::FLOAT;
  }

  if (attr.type == TypeFloat2) {
    return AttrKernelDataType::FLOAT2;
  }

  if (attr.type == TypeFloat4 || attr.type == TypeRGBA || attr.type == TypeDesc::TypeMatrix) {
    return AttrKernelDataType::FLOAT4;
  }

  return AttrKernelDataType::FLOAT3;
}

void Attribute::get_uv_tiles(Geometry *geom,
                             AttributePrimitive prim,
                             unordered_set<int> &tiles) const
{
  if (type != TypeFloat2) {
    return;
  }

  const int num = element_size(geom, prim);
  const float2 *uv = data_float2();
  for (int i = 0; i < num; i++, uv++) {
    float u = uv->x, v = uv->y;
    int x = (int)u, y = (int)v;

    if (x < 0 || y < 0 || x >= 10) {
      continue;
    }

    /* Be conservative in corners - precisely touching the right or upper edge of a tile
     * should not load its right/upper neighbor as well. */
    if (x > 0 && (u < x + 1e-6f)) {
      x--;
    }
    if (y > 0 && (v < y + 1e-6f)) {
      y--;
    }

    tiles.insert(1001 + 10 * y + x);
  }
}

/* Attribute Set */

AttributeSet::AttributeSet(Geometry *geometry, AttributePrimitive prim)
    : modified_flag(~0u), geometry(geometry), prim(prim)
{
}

AttributeSet::~AttributeSet() {}

Attribute *AttributeSet::add(ustring name, TypeDesc type, AttributeElement element)
{
  Attribute *attr = find(name);

  if (attr) {
    /* return if same already exists */
    if (attr->type == type && attr->element == element)
      return attr;

    /* overwrite attribute with same name but different type/element */
    remove(name);
  }

  Attribute new_attr(name, type, element, geometry, prim);
  attributes.emplace_back(std::move(new_attr));
  tag_modified(attributes.back());
  return &attributes.back();
}

Attribute *AttributeSet::find(ustring name) const
{
  foreach (const Attribute &attr, attributes)
    if (attr.name == name)
      return (Attribute *)&attr;

  return NULL;
}

void AttributeSet::remove(ustring name)
{
  Attribute *attr = find(name);

  if (attr) {
    list<Attribute>::iterator it;

    for (it = attributes.begin(); it != attributes.end(); it++) {
      if (&*it == attr) {
        remove(it);
        return;
      }
    }
  }
}

Attribute *AttributeSet::add(AttributeStandard std, ustring name)
{
  Attribute *attr = NULL;

  if (name == ustring())
    name = Attribute::standard_name(std);

  if (geometry->geometry_type == Geometry::MESH) {
    switch (std) {
      case ATTR_STD_VERTEX_NORMAL:
        attr = add(name, TypeDesc::TypeNormal, ATTR_ELEMENT_VERTEX);
        break;
      case ATTR_STD_FACE_NORMAL:
        attr = add(name, TypeDesc::TypeNormal, ATTR_ELEMENT_FACE);
        break;
      case ATTR_STD_UV:
        attr = add(name, TypeFloat2, ATTR_ELEMENT_CORNER);
        break;
      case ATTR_STD_UV_TANGENT:
        attr = add(name, TypeDesc::TypeVector, ATTR_ELEMENT_CORNER);
        break;
      case ATTR_STD_UV_TANGENT_SIGN:
        attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_CORNER);
        break;
      case ATTR_STD_VERTEX_COLOR:
        attr = add(name, TypeRGBA, ATTR_ELEMENT_CORNER_BYTE);
        break;
      case ATTR_STD_GENERATED:
      case ATTR_STD_POSITION_UNDEFORMED:
      case ATTR_STD_POSITION_UNDISPLACED:
        attr = add(name, TypeDesc::TypePoint, ATTR_ELEMENT_VERTEX);
        break;
      case ATTR_STD_MOTION_VERTEX_POSITION:
        attr = add(name, TypeDesc::TypePoint, ATTR_ELEMENT_VERTEX_MOTION);
        break;
      case ATTR_STD_MOTION_VERTEX_NORMAL:
        attr = add(name, TypeDesc::TypeNormal, ATTR_ELEMENT_VERTEX_MOTION);
        break;
      case ATTR_STD_PTEX_FACE_ID:
        attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_FACE);
        break;
      case ATTR_STD_PTEX_UV:
        attr = add(name, TypeDesc::TypePoint, ATTR_ELEMENT_VERTEX);
        break;
      case ATTR_STD_GENERATED_TRANSFORM:
        attr = add(name, TypeDesc::TypeMatrix, ATTR_ELEMENT_MESH);
        break;
      case ATTR_STD_POINTINESS:
        attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_VERTEX);
        break;
      case ATTR_STD_RANDOM_PER_ISLAND:
        attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_FACE);
        break;
      default:
        assert(0);
        break;
    }
  }
  else if (geometry->geometry_type == Geometry::POINTCLOUD) {
    switch (std) {
      case ATTR_STD_UV:
        attr = add(name, TypeFloat2, ATTR_ELEMENT_VERTEX);
        break;
      case ATTR_STD_GENERATED:
        attr = add(name, TypeDesc::TypePoint, ATTR_ELEMENT_VERTEX);
        break;
      case ATTR_STD_MOTION_VERTEX_POSITION:
        attr = add(name, TypeDesc::TypeFloat4, ATTR_ELEMENT_VERTEX_MOTION);
        break;
      case ATTR_STD_POINT_RANDOM:
        attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_VERTEX);
        break;
      case ATTR_STD_GENERATED_TRANSFORM:
        attr = add(name, TypeDesc::TypeMatrix, ATTR_ELEMENT_MESH);
        break;
      default:
        assert(0);
        break;
    }
  }
  else if (geometry->geometry_type == Geometry::VOLUME) {
    switch (std) {
      case ATTR_STD_VERTEX_NORMAL:
        attr = add(name, TypeDesc::TypeNormal, ATTR_ELEMENT_VERTEX);
        break;
      case ATTR_STD_FACE_NORMAL:
        attr = add(name, TypeDesc::TypeNormal, ATTR_ELEMENT_FACE);
        break;
      case ATTR_STD_VOLUME_DENSITY:
      case ATTR_STD_VOLUME_FLAME:
      case ATTR_STD_VOLUME_HEAT:
      case ATTR_STD_VOLUME_TEMPERATURE:
      case ATTR_STD_VOLUME_VELOCITY_X:
      case ATTR_STD_VOLUME_VELOCITY_Y:
      case ATTR_STD_VOLUME_VELOCITY_Z:
        attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_VOXEL);
        break;
      case ATTR_STD_VOLUME_COLOR:
        attr = add(name, TypeDesc::TypeColor, ATTR_ELEMENT_VOXEL);
        break;
      case ATTR_STD_VOLUME_VELOCITY:
        attr = add(name, TypeDesc::TypeVector, ATTR_ELEMENT_VOXEL);
        break;
      default:
        assert(0);
        break;
    }
  }
  else if (geometry->geometry_type == Geometry::HAIR) {
    switch (std) {
      case ATTR_STD_UV:
        attr = add(name, TypeFloat2, ATTR_ELEMENT_CURVE);
        break;
      case ATTR_STD_GENERATED:
        attr = add(name, TypeDesc::TypePoint, ATTR_ELEMENT_CURVE);
        break;
      case ATTR_STD_MOTION_VERTEX_POSITION:
        attr = add(name, TypeDesc::TypeFloat4, ATTR_ELEMENT_CURVE_KEY_MOTION);
        break;
      case ATTR_STD_CURVE_INTERCEPT:
        attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_CURVE_KEY);
        break;
      case ATTR_STD_CURVE_LENGTH:
        attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_CURVE);
        break;
      case ATTR_STD_CURVE_RANDOM:
        attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_CURVE);
        break;
      case ATTR_STD_GENERATED_TRANSFORM:
        attr = add(name, TypeDesc::TypeMatrix, ATTR_ELEMENT_MESH);
        break;
      case ATTR_STD_POINTINESS:
        attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_VERTEX);
        break;
      case ATTR_STD_RANDOM_PER_ISLAND:
        attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_FACE);
        break;
      case ATTR_STD_SHADOW_TRANSPARENCY:
        attr = add(name, TypeDesc::TypeFloat, ATTR_ELEMENT_CURVE_KEY);
        break;
      default:
        assert(0);
        break;
    }
  }

  attr->std = std;

  return attr;
}

Attribute *AttributeSet::find(AttributeStandard std) const
{
  foreach (const Attribute &attr, attributes)
    if (attr.std == std)
      return (Attribute *)&attr;

  return NULL;
}

Attribute *AttributeSet::find_matching(const Attribute &other)
{
  for (Attribute &attr : attributes) {
    if (attr.name != other.name) {
      continue;
    }
    if (attr.std != other.std) {
      continue;
    }
    if (attr.type != other.type) {
      continue;
    }
    if (attr.element != other.element) {
      continue;
    }
    return &attr;
  }
  return nullptr;
}

void AttributeSet::remove(AttributeStandard std)
{
  Attribute *attr = find(std);

  if (attr) {
    list<Attribute>::iterator it;

    for (it = attributes.begin(); it != attributes.end(); it++) {
      if (&*it == attr) {
        remove(it);
        return;
      }
    }
  }
}

Attribute *AttributeSet::find(AttributeRequest &req)
{
  if (req.std == ATTR_STD_NONE)
    return find(req.name);
  else
    return find(req.std);
}

void AttributeSet::remove(Attribute *attribute)
{
  if (attribute->std == ATTR_STD_NONE) {
    remove(attribute->name);
  }
  else {
    remove(attribute->std);
  }
}

void AttributeSet::remove(list<Attribute>::iterator it)
{
  tag_modified(*it);
  attributes.erase(it);
}

void AttributeSet::resize(bool reserve_only)
{
  foreach (Attribute &attr, attributes) {
    attr.resize(geometry, prim, reserve_only);
  }
}

void AttributeSet::clear(bool preserve_voxel_data)
{
  if (preserve_voxel_data) {
    list<Attribute>::iterator it;

    for (it = attributes.begin(); it != attributes.end();) {
      if (it->element == ATTR_ELEMENT_VOXEL || it->std == ATTR_STD_GENERATED_TRANSFORM) {
        it++;
      }
      else {
        attributes.erase(it++);
      }
    }
  }
  else {
    attributes.clear();
  }
}

void AttributeSet::update(AttributeSet &&new_attributes)
{
  /* Remove any attributes not on new_attributes. */
  list<Attribute>::iterator it;
  for (it = attributes.begin(); it != attributes.end();) {
    const Attribute &old_attr = *it;
    if (new_attributes.find_matching(old_attr) == nullptr) {
      remove(it++);
      continue;
    }
    it++;
  }

  /* Add or update old_attributes based on the new_attributes. */
  foreach (Attribute &attr, new_attributes.attributes) {
    Attribute *nattr = add(attr.name, attr.type, attr.element);
    nattr->std = attr.std;
    nattr->set_data_from(std::move(attr));
  }

  /* If all attributes were replaced, transform is no longer applied. */
  geometry->transform_applied = false;
}

void AttributeSet::clear_modified()
{
  foreach (Attribute &attr, attributes) {
    attr.modified = false;
  }

  modified_flag = 0;
}

void AttributeSet::tag_modified(const Attribute &attr)
{
  /* Some attributes are not stored in the various kernel attribute arrays
   * (DeviceScene::attribute_*), so the modified flags are only set if the associated standard
   * corresponds to an attribute which will be stored in the kernel's attribute arrays. */
  const bool modifies_device_array = (attr.std != ATTR_STD_FACE_NORMAL &&
                                      attr.std != ATTR_STD_VERTEX_NORMAL);

  if (modifies_device_array) {
    AttrKernelDataType kernel_type = Attribute::kernel_type(attr);
    modified_flag |= (1u << kernel_type);
  }
}

bool AttributeSet::modified(AttrKernelDataType kernel_type) const
{
  return (modified_flag & (1u << kernel_type)) != 0;
}

/* AttributeRequest */

AttributeRequest::AttributeRequest(ustring name_)
{
  name = name_;
  std = ATTR_STD_NONE;

  type = TypeDesc::TypeFloat;
  desc.element = ATTR_ELEMENT_NONE;
  desc.offset = 0;
  desc.type = NODE_ATTR_FLOAT;

  subd_type = TypeDesc::TypeFloat;
  subd_desc.element = ATTR_ELEMENT_NONE;
  subd_desc.offset = 0;
  subd_desc.type = NODE_ATTR_FLOAT;
}

AttributeRequest::AttributeRequest(AttributeStandard std_)
{
  name = ustring();
  std = std_;

  type = TypeDesc::TypeFloat;
  desc.element = ATTR_ELEMENT_NONE;
  desc.offset = 0;
  desc.type = NODE_ATTR_FLOAT;

  subd_type = TypeDesc::TypeFloat;
  subd_desc.element = ATTR_ELEMENT_NONE;
  subd_desc.offset = 0;
  subd_desc.type = NODE_ATTR_FLOAT;
}

/* AttributeRequestSet */

AttributeRequestSet::AttributeRequestSet() {}

AttributeRequestSet::~AttributeRequestSet() {}

bool AttributeRequestSet::modified(const AttributeRequestSet &other)
{
  if (requests.size() != other.requests.size())
    return true;

  for (size_t i = 0; i < requests.size(); i++) {
    bool found = false;

    for (size_t j = 0; j < requests.size() && !found; j++)
      if (requests[i].name == other.requests[j].name && requests[i].std == other.requests[j].std) {
        found = true;
      }

    if (!found) {
      return true;
    }
  }

  return false;
}

void AttributeRequestSet::add(ustring name)
{
  foreach (AttributeRequest &req, requests) {
    if (req.name == name) {
      return;
    }
  }

  requests.push_back(AttributeRequest(name));
}

void AttributeRequestSet::add(AttributeStandard std)
{
  foreach (AttributeRequest &req, requests)
    if (req.std == std)
      return;

  requests.push_back(AttributeRequest(std));
}

void AttributeRequestSet::add(AttributeRequestSet &reqs)
{
  foreach (AttributeRequest &req, reqs.requests) {
    if (req.std == ATTR_STD_NONE)
      add(req.name);
    else
      add(req.std);
  }
}

void AttributeRequestSet::add_standard(ustring name)
{
  if (name.empty()) {
    return;
  }

  AttributeStandard std = Attribute::name_standard(name.c_str());

  if (std) {
    add(std);
  }
  else {
    add(name);
  }
}

bool AttributeRequestSet::find(ustring name)
{
  foreach (AttributeRequest &req, requests)
    if (req.name == name)
      return true;

  return false;
}

bool AttributeRequestSet::find(AttributeStandard std)
{
  foreach (AttributeRequest &req, requests)
    if (req.std == std)
      return true;

  return false;
}

size_t AttributeRequestSet::size()
{
  return requests.size();
}

void AttributeRequestSet::clear()
{
  requests.clear();
}

CCL_NAMESPACE_END
