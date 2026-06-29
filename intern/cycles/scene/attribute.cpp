/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/attribute.h"
#include "scene/hair.h"
#include "scene/image.h"
#include "scene/mesh.h"
#include "scene/pointcloud.h"

#include "util/guarded_allocator.h"
#include "util/log.h"
#include "util/transform.h"

CCL_NAMESPACE_BEGIN

/* Attribute */

Attribute::Attribute(ustring name,
                     const TypeDesc type,
                     AttributeElement element,
                     Geometry *geom,
                     AttributePrimitive prim)
    : name(name), std(ATTR_STD_NONE), type(type), element(element), flags(0), modified(true)
{
  /* string and matrix not supported! */
  assert(type == TypeFloat || type == TypeColor || type == TypePoint || type == TypeVector ||
         type == TypeNormal || type == TypeMatrix || type == TypeFloat2 || type == TypeFloat4 ||
         type == TypeRGBA);

  if (element & ATTR_ELEMENT_VOXEL) {
    auto *data = GuardedAllocator<ImageHandle>().allocate(1);
    new (data) ImageHandle();
    center.data = data;
    size = Attribute::element_size(geom, element, prim);
  }
  else {
    resize(geom, prim);
  }
}

Attribute::Attribute(ustring name,
                     const TypeDesc type,
                     AttributeElement element,
                     const void *data,
                     const int size,
                     ImplicitSharingInfo sharing_info)
    : name(name),
      std(ATTR_STD_NONE),
      type(type),
      size(size),
      element(element),
      flags(0),
      modified(true)
{
  assert((element & ATTR_ELEMENT_VOXEL) == 0);
  center.data = data;
  /* Implicit sharing function pointers should be set if shared attributes are created. */
  assert(g_implicit_sharing_user_add_fn);
  assert(g_implicit_sharing_user_remove_fn);
  g_implicit_sharing_user_add_fn(sharing_info);
  center.sharing_info = sharing_info;
}

static size_t attribute_alloc_bytes(const size_t element_size, const size_t size)
{
  /* rtcSetSharedGeometryBuffer is documented as requiring 4 bytes past the
   * end of a float3 for 16-byte SSE loads, so we add that for all attributes. */
  static constexpr size_t ATTRIBUTE_BUFFER_PADDING = 4;
  return element_size * size + ATTRIBUTE_BUFFER_PADDING;
}

static void free_step_buffer(Attribute::Buffer &buf,
                             const AttributeElement element,
                             const size_t data_sizeof,
                             const size_t size)
{
  if (element & ATTR_ELEMENT_VOXEL) {
    if (buf.data) {
      auto *image = static_cast<ImageHandle *>(const_cast<void *>(buf.data));
      image->~ImageHandle();
      GuardedAllocator<ImageHandle>().deallocate(image, 1);
    }
  }
  else if (buf.sharing_info) {
    g_implicit_sharing_user_remove_fn(buf.sharing_info);
  }
  else if (buf.data) {
    GuardedAllocator<char>().deallocate(static_cast<char *>(const_cast<void *>(buf.data)),
                                        attribute_alloc_bytes(data_sizeof, size));
  }
  buf.data = nullptr;
  buf.sharing_info = nullptr;
}

Attribute::Attribute(Attribute &&other)
    : name(other.name),
      std(other.std),
      type(other.type),
      element(other.element),
      modified(other.modified)
{
  set_data_from(std::move(other));
}

void Attribute::free_data()
{
  const size_t element_size = data_sizeof();
  free_step_buffer(center, element, element_size, size);
  for (Buffer &buf : motion) {
    free_step_buffer(buf, element, element_size, size);
  }
  motion.clear();
}

Attribute::~Attribute()
{
  free_data();
}

void Attribute::resize(Geometry *geom, AttributePrimitive prim)
{
  if (!(element & ATTR_ELEMENT_VOXEL)) {
    resize(Attribute::element_size(geom, element, prim));
  }
}

void Attribute::resize(const size_t num_elements)
{
  if (element & ATTR_ELEMENT_VOXEL) {
    return;
  }
  if (num_elements == size_t(size)) {
    return;
  }
  const size_t element_size = data_sizeof();
  const size_t copy_elems = std::min(num_elements, size_t(size));
  const size_t alloc_bytes = attribute_alloc_bytes(element_size, num_elements);

  /* Allocate and copy center step. */
  Buffer new_center;
  new_center.data = GuardedAllocator<char>().allocate(alloc_bytes);
  if (center.data) {
    memcpy(const_cast<void *>(new_center.data), center.data, copy_elems * element_size);
  }

  /* Allocate and copy motion steps. */
  vector<Buffer> new_motion(motion.size());
  for (size_t i = 0; i < motion.size(); i++) {
    new_motion[i].data = GuardedAllocator<char>().allocate(alloc_bytes);
    if (motion[i].data) {
      memcpy(const_cast<void *>(new_motion[i].data), motion[i].data, copy_elems * element_size);
    }
  }

  free_data();
  center = new_center;
  motion = std::move(new_motion);
  size = num_elements;
}

void Attribute::add_motion(const Geometry *geom)
{
  const int motion_steps = geom->get_motion_steps();
  if (motion_steps <= 0) {
    return;
  }

  const int motion_size = geom->get_motion_steps() - 1;
  if (motion_size == motion.size()) {
    return;
  }
  const size_t element_size = data_sizeof();

  if (motion_size < motion.size()) {
    for (size_t i = motion_size; i < motion.size(); i++) {
      free_step_buffer(motion[i], element, element_size, size);
    }
    motion.resize(motion_size);
  }
  else {
    motion.reserve(motion_size);
    while (motion.size() < motion_size) {
      Buffer buf;
      if (size > 0) {
        /* Left uninitialized, callers fill in the motion data for every step. */
        buf.data = GuardedAllocator<char>().allocate(attribute_alloc_bytes(element_size, size));
      }
      motion.push_back(buf);
    }
  }

  modified = true;
}

void Attribute::remove_motion()
{
  if (!has_motion()) {
    return;
  }
  const size_t element_size = data_sizeof();
  for (Buffer &buf : motion) {
    free_step_buffer(buf, element, element_size, size);
  }
  motion.clear();
  modified = true;
}

void Attribute::set_motion_step_shared(const int step,
                                       const void *data,
                                       const int new_size,
                                       ImplicitSharingInfo sharing_info)
{
  assert(step >= 1 && size_t(step - 1) < motion.size());
  assert(new_size == size);
  (void)new_size;

  Buffer &buf = motion[step - 1];
  free_step_buffer(buf, element, data_sizeof(), size);

  buf.data = data;
  assert(g_implicit_sharing_user_add_fn);
  g_implicit_sharing_user_add_fn(sharing_info);
  buf.sharing_info = sharing_info;

  modified = true;
}

void Attribute::take_motion_from(Attribute &other)
{
  assert(other.type == type && other.element == element && other.size == size);
  remove_motion();
  motion = std::move(other.motion);
  other.motion.clear();
  other.modified = true;
  modified = true;
}

static char *buffer_for_write(Attribute::Buffer &buf, const size_t element_size, const size_t size)
{
  if (!buf.data) {
    return nullptr;
  }
  if (buf.sharing_info) {
    /* Here we assume that the sharing info is not mutable. With the addition of another sharing
     * info callback function pointer we could check the user count to avoid unnecessary copies.
     * For now that isn't expected to happen in practice though. */
    auto *new_data = GuardedAllocator<char>().allocate(attribute_alloc_bytes(element_size, size));
    memcpy(new_data, buf.data, element_size * size);
    g_implicit_sharing_user_remove_fn(buf.sharing_info);
    buf.sharing_info = nullptr;
    buf.data = new_data;
  }
  return const_cast<char *>(reinterpret_cast<const char *>(buf.data));
}

char *Attribute::data_for_write_buffer(const int step)
{
  if (step == 0) {
    if (!center.data) {
      assert(size == 0);
      return nullptr;
    }
    return buffer_for_write(center, data_sizeof(), size);
  }
  assert(step >= 1 && step <= int(motion.size()));
  return buffer_for_write(motion[step - 1], data_sizeof(), size);
}

void Attribute::set_data_from(Attribute &&other)
{
  assert(other.std == std);
  assert(other.type == type);
  assert(other.element == element);

  flags = other.flags;

  const size_t element_size = data_sizeof();

  /* If topology or motion steps differ, take all data. */
  if (size != other.size || motion.size() != other.motion.size()) {
    free_data();
    center = other.center;
    motion = std::move(other.motion);
    size = other.size;
    other.center = Buffer();
    other.size = 0;
    modified = true;
    return;
  }

  /* Compare each step independently. */
  const auto take_step = [&](Buffer &dst, Buffer &src, const AttributeElement step_element) {
    free_step_buffer(dst, step_element, element_size, size);
    dst = src;
    src = Buffer();
    modified = true;
  };

  const auto step_equals = [&](const Buffer &a, const Buffer &b) {
    if (a.data == b.data) {
      /* Same buffer, e.g. shared through implicit sharing. */
      return true;
    }
    if (a.sharing_info != b.sharing_info) {
      return false;
    }
    if (size == 0) {
      return true;
    }
    return memcmp(a.data, b.data, element_size * size) == 0;
  };

  if (!step_equals(center, other.center)) {
    take_step(center, other.center, element);
  }
  for (size_t i = 0; i < motion.size(); i++) {
    if (!step_equals(motion[i], other.motion[i])) {
      take_step(motion[i], other.motion[i], element);
    }
  }
}

size_t Attribute::data_sizeof() const
{
  if (element & ATTR_ELEMENT_VOXEL) {
    return sizeof(ImageHandle);
  }
  if (element & ATTR_ELEMENT_IS_BYTE) {
    return sizeof(uchar4);
  }
  if (element & ATTR_ELEMENT_IS_NORMAL) {
    return sizeof(packed_normal);
  }
  if (type == TypeFloat) {
    return sizeof(float);
  }
  if (type == TypeFloat2) {
    return sizeof(float2);
  }
  if (type == TypeMatrix) {
    return sizeof(Transform);
    // The float3 type is not interchangeable with float4
    // as it is now a packed type.
  }
  if (type == TypeFloat4) {
    return sizeof(float4);
  }
  if (type == TypeRGBA) {
    return sizeof(float4);
  }
  return sizeof(packed_float3);
}

size_t Attribute::element_size(Geometry *geom,
                               const AttributeElement element,
                               AttributePrimitive prim)
{
  size_t size = 0;

  switch (element) {
    case ATTR_ELEMENT_OBJECT:
    case ATTR_ELEMENT_MESH:
    case ATTR_ELEMENT_VOXEL:
      size = 1;
      break;
    case ATTR_ELEMENT_VERTEX:
    case ATTR_ELEMENT_VERTEX_NORMAL:
      if (geom->is_mesh() || geom->is_volume()) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        if (prim == ATTR_PRIM_SUBD) {
          size = mesh->get_num_subd_base_verts();
        }
        else {
          size = mesh->num_verts();
        }
      }
      else if (geom->is_pointcloud()) {
        PointCloud *pointcloud = static_cast<PointCloud *>(geom);
        size = pointcloud->num_points();
      }
      break;

    case ATTR_ELEMENT_FACE:
      if (geom->is_mesh() || geom->is_volume()) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        if (prim == ATTR_PRIM_SUBD) {
          size = mesh->get_num_subd_faces();
        }
        else {
          size = mesh->num_triangles();
        }
      }
      break;
    case ATTR_ELEMENT_CORNER:
    case ATTR_ELEMENT_CORNER_BYTE:
    case ATTR_ELEMENT_CORNER_NORMAL:
      if (geom->is_mesh()) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        if (prim == ATTR_PRIM_SUBD) {
          size = mesh->get_subd_face_corners().size();
        }
        else {
          size = mesh->num_triangles() * 3;
        }
      }
      break;
    case ATTR_ELEMENT_CURVE:
      if (geom->is_hair()) {
        Hair *hair = static_cast<Hair *>(geom);
        size = hair->num_curves();
      }
      break;
    case ATTR_ELEMENT_CURVE_KEY:
    case ATTR_ELEMENT_CURVE_KEY_NORMAL:
      if (geom->is_hair()) {
        Hair *hair = static_cast<Hair *>(geom);
        size = hair->num_keys();
      }
      break;
    default:
      break;
  }

  return size;
}

size_t Attribute::buffer_size(Geometry *geom, AttributePrimitive prim) const
{
  /* Size of a single step buffer, as returned by data() for one step. */
  return Attribute::element_size(geom, element, prim) * data_sizeof();
}

bool Attribute::same_storage(const TypeDesc a, const TypeDesc b)
{
  if (a == b) {
    return true;
  }

  if (a == TypeColor || a == TypePoint || a == TypeVector || a == TypeNormal) {
    if (b == TypeColor || b == TypePoint || b == TypeVector || b == TypeNormal) {
      return true;
    }
  }
  return false;
}

void Attribute::zero_data(void *dst)
{
  memset(dst, 0, data_sizeof());
}

const char *Attribute::standard_name(AttributeStandard std)
{
  switch (std) {
    case ATTR_STD_POSITION:
      return "P";
    case ATTR_STD_RADIUS:
      return "radius";
    case ATTR_STD_VERTEX_NORMAL:
    case ATTR_STD_CORNER_NORMAL:
      return "N";
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
    case ATTR_STD_UV_TANGENT_UNDISPLACED:
      return "undisplaced_tangent";
    case ATTR_STD_UV_TANGENT_SIGN_UNDISPLACED:
      return "undisplaced_tangent_sign";
    case ATTR_STD_VERTEX_COLOR:
      return "vertex_color";
    case ATTR_STD_POSITION_UNDEFORMED:
      return "undeformed";
    case ATTR_STD_POSITION_UNDISPLACED:
      return "undisplaced";
    case ATTR_STD_NORMAL_UNDISPLACED:
      return "undisplaced_N";
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
  if (attr.element & ATTR_ELEMENT_IS_BYTE) {
    return AttrKernelDataType::UCHAR4;
  }

  if (attr.element & ATTR_ELEMENT_IS_NORMAL) {
    return AttrKernelDataType::NORMAL;
  }

  if (attr.type == TypeFloat) {
    return AttrKernelDataType::FLOAT;
  }

  if (attr.type == TypeFloat2) {
    return AttrKernelDataType::FLOAT2;
  }

  if (attr.type == TypeFloat4 || attr.type == TypeRGBA || attr.type == TypeMatrix) {
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

  const int num = Attribute::element_size(geom, element, prim);
  const float2 *uv = data<float2>();
  for (int i = 0; i < num; i++, uv++) {
    const float u = uv->x;
    const float v = uv->y;
    int x = (int)u;
    int y = (int)v;

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

AttributeSet::~AttributeSet() = default;

Attribute *AttributeSet::add(ustring name, const TypeDesc type, AttributeElement element)
{
  Attribute *attr = find(name);

  if (attr) {
    /* return if same already exists */
    if (attr->type == type && attr->element == element) {
      return attr;
    }

    /* overwrite attribute with same name but different type/element */
    remove(name);
  }

  attributes.emplace_back(name, type, element, geometry, prim);
  tag_modified(attributes.back());
  return &attributes.back();
}

Attribute *AttributeSet::add_shared(ustring name,
                                    const TypeDesc type,
                                    AttributeElement element,
                                    const void *data,
                                    const int size,
                                    ImplicitSharingInfo sharing_info)
{
  Attribute *attr = find(name);

  if (attr) {
    /* overwrite attribute with same name but different type/element */
    remove(name);
  }

  attributes.emplace_back(name, type, element, data, size, sharing_info);
  tag_modified(attributes.back());
  return &attributes.back();
}

Attribute *AttributeSet::add_from(Attribute &&other)
{
  Attribute *attr = find(other.name);
  if (attr) {
    if (attr->type == other.type && attr->element == other.element) {
      attr->std = other.std;
      attr->set_data_from(std::move(other));
      return attr;
    }

    /* Overwrite attribute with the same name but different type/element. */
    remove(other.name);
  }

  attributes.emplace_back(std::move(other));
  tag_modified(attributes.back());
  return &attributes.back();
}

Attribute *AttributeSet::find(ustring name) const
{
  for (const Attribute &attr : attributes) {
    if (attr.name == name) {
      return (Attribute *)&attr;
    }
  }

  return nullptr;
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

static TypeDesc find_type_from_geometry_std(Geometry *geometry, AttributeStandard std)
{
  if (geometry->is_mesh()) {
    switch (std) {
      case ATTR_STD_POSITION:
        return TypePoint;
      case ATTR_STD_VERTEX_NORMAL:
        return TypeNormal;
      case ATTR_STD_NORMAL_UNDISPLACED:
        return TypeNormal;
      case ATTR_STD_UV:
        return TypeFloat2;
      case ATTR_STD_UV_TANGENT:
      case ATTR_STD_UV_TANGENT_UNDISPLACED:
        return TypeVector;
      case ATTR_STD_UV_TANGENT_SIGN:
      case ATTR_STD_UV_TANGENT_SIGN_UNDISPLACED:
        return TypeFloat;
      case ATTR_STD_VERTEX_COLOR:
        return TypeRGBA;
      case ATTR_STD_GENERATED:
      case ATTR_STD_POSITION_UNDEFORMED:
      case ATTR_STD_POSITION_UNDISPLACED:
        return TypePoint;
      case ATTR_STD_CORNER_NORMAL:
        return TypeNormal;
      case ATTR_STD_PTEX_FACE_ID:
        return TypeFloat;
      case ATTR_STD_PTEX_UV:
        return TypeFloat2;
      case ATTR_STD_GENERATED_TRANSFORM:
        return TypeMatrix;
      case ATTR_STD_POINTINESS:
        return TypeFloat;
      case ATTR_STD_RANDOM_PER_ISLAND:
        return TypeFloat;
      default:
        assert(0);
        break;
    }
  }
  else if (geometry->is_pointcloud()) {
    switch (std) {
      case ATTR_STD_POSITION:
        return TypePoint;
      case ATTR_STD_RADIUS:
        return TypeFloat;
      case ATTR_STD_UV:
        return TypeFloat2;
      case ATTR_STD_GENERATED:
        return TypePoint;
      case ATTR_STD_POINT_RANDOM:
        return TypeFloat;
      case ATTR_STD_GENERATED_TRANSFORM:
        return TypeMatrix;
      default:
        assert(0);
        break;
    }
  }
  else if (geometry->is_volume()) {
    switch (std) {
      case ATTR_STD_POSITION:
        return TypePoint;
      case ATTR_STD_VERTEX_NORMAL:
        return TypeNormal;
      case ATTR_STD_CORNER_NORMAL:
        return TypeNormal;
      case ATTR_STD_VOLUME_DENSITY:
      case ATTR_STD_VOLUME_FLAME:
      case ATTR_STD_VOLUME_HEAT:
      case ATTR_STD_VOLUME_TEMPERATURE:
      case ATTR_STD_VOLUME_VELOCITY_X:
      case ATTR_STD_VOLUME_VELOCITY_Y:
      case ATTR_STD_VOLUME_VELOCITY_Z:
        return TypeFloat;
      case ATTR_STD_VOLUME_COLOR:
        return TypeColor;
      case ATTR_STD_VOLUME_VELOCITY:
        return TypeVector;
      case ATTR_STD_GENERATED_TRANSFORM:
        return TypeMatrix;
      default:
        assert(0);
        break;
    }
  }
  else if (geometry->is_hair()) {
    switch (std) {
      case ATTR_STD_POSITION:
        return TypePoint;
      case ATTR_STD_RADIUS:
        return TypeFloat;
      case ATTR_STD_VERTEX_NORMAL:
        return TypeNormal;
      case ATTR_STD_UV:
        return TypeFloat2;
      case ATTR_STD_GENERATED:
        return TypePoint;
      case ATTR_STD_CURVE_INTERCEPT:
        return TypeFloat;
      case ATTR_STD_CURVE_LENGTH:
        return TypeFloat;
      case ATTR_STD_CURVE_RANDOM:
        return TypeFloat;
      case ATTR_STD_GENERATED_TRANSFORM:
        return TypeMatrix;
      case ATTR_STD_POINTINESS:
        return TypeFloat;
      case ATTR_STD_RANDOM_PER_ISLAND:
        return TypeFloat;
      case ATTR_STD_SHADOW_TRANSPARENCY:
        return TypeFloat;
      default:
        assert(0);
        break;
    }
  }
  assert(0);
  return TypeFloat;
}

static AttributeElement find_element_from_geometry_std(Geometry *geometry, AttributeStandard std)
{
  if (geometry->is_mesh()) {
    switch (std) {
      case ATTR_STD_POSITION:
        return ATTR_ELEMENT_VERTEX;
      case ATTR_STD_VERTEX_NORMAL:
        return ATTR_ELEMENT_VERTEX_NORMAL;
      case ATTR_STD_NORMAL_UNDISPLACED:
        return ATTR_ELEMENT_VERTEX_NORMAL;
      case ATTR_STD_UV:
        return ATTR_ELEMENT_CORNER;
      case ATTR_STD_UV_TANGENT:
      case ATTR_STD_UV_TANGENT_UNDISPLACED:
        return ATTR_ELEMENT_CORNER;
      case ATTR_STD_UV_TANGENT_SIGN:
      case ATTR_STD_UV_TANGENT_SIGN_UNDISPLACED:
        return ATTR_ELEMENT_CORNER;
      case ATTR_STD_VERTEX_COLOR:
        return ATTR_ELEMENT_CORNER_BYTE;
      case ATTR_STD_GENERATED:
      case ATTR_STD_POSITION_UNDEFORMED:
      case ATTR_STD_POSITION_UNDISPLACED:
        return ATTR_ELEMENT_VERTEX;
      case ATTR_STD_CORNER_NORMAL:
        return ATTR_ELEMENT_CORNER_NORMAL;
      case ATTR_STD_PTEX_FACE_ID:
        return ATTR_ELEMENT_FACE;
      case ATTR_STD_PTEX_UV:
        return ATTR_ELEMENT_CORNER;
      case ATTR_STD_GENERATED_TRANSFORM:
        return ATTR_ELEMENT_MESH;
      case ATTR_STD_POINTINESS:
        return ATTR_ELEMENT_VERTEX;
      case ATTR_STD_RANDOM_PER_ISLAND:
        return ATTR_ELEMENT_FACE;
      default:
        assert(0);
        break;
    }
  }
  else if (geometry->is_pointcloud()) {
    switch (std) {
      case ATTR_STD_POSITION:
        return ATTR_ELEMENT_VERTEX;
      case ATTR_STD_RADIUS:
        return ATTR_ELEMENT_VERTEX;
      case ATTR_STD_UV:
        return ATTR_ELEMENT_VERTEX;
      case ATTR_STD_GENERATED:
        return ATTR_ELEMENT_VERTEX;
      case ATTR_STD_POINT_RANDOM:
        return ATTR_ELEMENT_VERTEX;
      case ATTR_STD_GENERATED_TRANSFORM:
        return ATTR_ELEMENT_MESH;
      default:
        assert(0);
        break;
    }
  }
  else if (geometry->is_volume()) {
    switch (std) {
      case ATTR_STD_POSITION:
        return ATTR_ELEMENT_VERTEX;
      case ATTR_STD_VERTEX_NORMAL:
        return ATTR_ELEMENT_VERTEX_NORMAL;
      case ATTR_STD_CORNER_NORMAL:
        return ATTR_ELEMENT_CORNER_NORMAL;
      case ATTR_STD_VOLUME_DENSITY:
      case ATTR_STD_VOLUME_FLAME:
      case ATTR_STD_VOLUME_HEAT:
      case ATTR_STD_VOLUME_TEMPERATURE:
      case ATTR_STD_VOLUME_VELOCITY_X:
      case ATTR_STD_VOLUME_VELOCITY_Y:
      case ATTR_STD_VOLUME_VELOCITY_Z:
        return ATTR_ELEMENT_VOXEL;
      case ATTR_STD_VOLUME_COLOR:
        return ATTR_ELEMENT_VOXEL;
      case ATTR_STD_VOLUME_VELOCITY:
        return ATTR_ELEMENT_VOXEL;
      case ATTR_STD_GENERATED_TRANSFORM:
        return ATTR_ELEMENT_MESH;
      default:
        assert(0);
        break;
    }
  }
  else if (geometry->is_hair()) {
    switch (std) {
      case ATTR_STD_POSITION:
        return ATTR_ELEMENT_CURVE_KEY;
      case ATTR_STD_RADIUS:
        return ATTR_ELEMENT_CURVE_KEY;
      case ATTR_STD_VERTEX_NORMAL:
        return ATTR_ELEMENT_CURVE_KEY_NORMAL;
      case ATTR_STD_UV:
        return ATTR_ELEMENT_CURVE;
      case ATTR_STD_GENERATED:
        return ATTR_ELEMENT_CURVE;
      case ATTR_STD_CURVE_INTERCEPT:
        return ATTR_ELEMENT_CURVE_KEY;
      case ATTR_STD_CURVE_LENGTH:
        return ATTR_ELEMENT_CURVE;
      case ATTR_STD_CURVE_RANDOM:
        return ATTR_ELEMENT_CURVE;
      case ATTR_STD_GENERATED_TRANSFORM:
        return ATTR_ELEMENT_MESH;
      case ATTR_STD_POINTINESS:
        return ATTR_ELEMENT_VERTEX;
      case ATTR_STD_RANDOM_PER_ISLAND:
        return ATTR_ELEMENT_FACE;
      case ATTR_STD_SHADOW_TRANSPARENCY:
        return ATTR_ELEMENT_CURVE_KEY;
      default:
        assert(0);
        break;
    }
  }
  assert(0);
  return ATTR_ELEMENT_NONE;
}

Attribute *AttributeSet::add(AttributeStandard std, ustring name)
{
  Attribute *attr = nullptr;

  if (name.empty()) {
    name = Attribute::standard_name(std);
  }

  attr = add(name,
             find_type_from_geometry_std(geometry, std),
             find_element_from_geometry_std(geometry, std));

  attr->std = std;

  return attr;
}

Attribute *AttributeSet::add_shared(AttributeStandard std,
                                    ustring name,
                                    const void *data,
                                    const int size,
                                    ImplicitSharingInfo sharing_info)
{
  Attribute *attr = nullptr;

  if (name.empty()) {
    name = Attribute::standard_name(std);
  }

  attr = add_shared(name,
                    find_type_from_geometry_std(geometry, std),
                    find_element_from_geometry_std(geometry, std),
                    data,
                    size,
                    sharing_info);

  attr->std = std;

  return attr;
}

Attribute &AttributeSet::copy(const Attribute &attr)
{
  Attribute &copy_attr = *add(attr.name, attr.type, attr.element);
  copy_attr.std = attr.std;
  if (attr.has_motion()) {
    copy_attr.add_motion(geometry);
  }
  return copy_attr;
}

Attribute *AttributeSet::find(AttributeStandard std) const
{
  for (const Attribute &attr : attributes) {
    if (attr.std == std) {
      return (Attribute *)&attr;
    }
  }

  return nullptr;
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
  if (req.std == ATTR_STD_NONE) {
    return find(req.name);
  }
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

void AttributeSet::resize()
{
  for (Attribute &attr : attributes) {
    attr.resize(geometry, prim);
  }
}

void AttributeSet::clear(bool preserve_voxel_data)
{
  if (preserve_voxel_data) {
    list<Attribute>::iterator it;

    for (it = attributes.begin(); it != attributes.end();) {
      if ((it->element & ATTR_ELEMENT_VOXEL) || it->std == ATTR_STD_GENERATED_TRANSFORM) {
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
  for (Attribute &attr : new_attributes.attributes) {
    const Attribute *new_attr = add_from(std::move(attr));

    /* Tag geometry as modified so BVH updates when attributes affecting it change. */
    if (new_attr->modified &&
        (new_attr->std == ATTR_STD_POSITION || new_attr->std == ATTR_STD_RADIUS))
    {
      geometry->tag_modified();
    }
  }

  /* If all attributes were replaced, transform is no longer applied. */
  geometry->transform_applied = false;
}

void AttributeSet::clear_modified()
{
  for (Attribute &attr : attributes) {
    attr.modified = false;
  }

  modified_flag = 0;
}

void AttributeSet::tag_modified(const Attribute &attr)
{
  const AttrKernelDataType kernel_type = Attribute::kernel_type(attr);
  modified_flag |= (1u << kernel_type);
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

  type = TypeFloat;
  desc.element = ATTR_ELEMENT_NONE;
  desc.offset = 0;
  desc.type = NODE_ATTR_FLOAT;
}

AttributeRequest::AttributeRequest(AttributeStandard std_)
{
  name = ustring();
  std = std_;

  type = TypeFloat;
  desc.element = ATTR_ELEMENT_NONE;
  desc.offset = 0;
  desc.type = NODE_ATTR_FLOAT;
}

/* AttributeRequestSet */

AttributeRequestSet::AttributeRequestSet() = default;

AttributeRequestSet::~AttributeRequestSet() = default;

bool AttributeRequestSet::modified(const AttributeRequestSet &other) const
{
  if (requests.size() != other.requests.size()) {
    return true;
  }

  for (size_t i = 0; i < requests.size(); i++) {
    bool found = false;

    for (size_t j = 0; j < requests.size() && !found; j++) {
      if (requests[i].name == other.requests[j].name && requests[i].std == other.requests[j].std) {
        found = true;
      }
    }

    if (!found) {
      return true;
    }
  }

  return false;
}

void AttributeRequestSet::add(ustring name)
{
  for (const AttributeRequest &req : requests) {
    if (req.name == name) {
      return;
    }
  }

  requests.push_back(AttributeRequest(name));
}

void AttributeRequestSet::add(AttributeStandard std)
{
  for (const AttributeRequest &req : requests) {
    if (req.std == std) {
      return;
    }
  }

  requests.push_back(AttributeRequest(std));
}

void AttributeRequestSet::add(const AttributeRequestSet &reqs)
{
  for (const AttributeRequest &req : reqs.requests) {
    if (req.std == ATTR_STD_NONE) {
      add(req.name);
    }
    else {
      add(req.std);
    }
  }
}

void AttributeRequestSet::add_standard(ustring name)
{
  if (name.empty()) {
    return;
  }

  const AttributeStandard std = Attribute::name_standard(name.c_str());

  if (std) {
    add(std);
  }
  else {
    add(name);
  }
}

bool AttributeRequestSet::find(const ustring name) const
{
  for (const AttributeRequest &req : requests) {
    if (req.name == name) {
      return true;
    }
  }

  return false;
}

bool AttributeRequestSet::find(const AttributeStandard std) const
{
  for (const AttributeRequest &req : requests) {
    if (req.std == std) {
      return true;
    }
  }

  return false;
}

size_t AttributeRequestSet::size() const
{
  return requests.size();
}

void AttributeRequestSet::clear()
{
  requests.clear();
}

CCL_NAMESPACE_END
