/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "bvh/bvh.h"

#include "device/device.h"

#include "scene/attribute.h"
#include "scene/camera.h"
#include "scene/geometry.h"
#include "scene/hair.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_nodes.h"

#include "util/progress.h"

CCL_NAMESPACE_BEGIN

bool Geometry::need_attribute(Scene *scene, AttributeStandard std)
{
  if (std == ATTR_STD_NONE) {
    return false;
  }

  if (scene->need_global_attribute(std)) {
    return true;
  }

  for (Node *node : used_shaders) {
    Shader *shader = static_cast<Shader *>(node);
    if (shader->attributes.find(std)) {
      return true;
    }
  }

  return false;
}

bool Geometry::need_attribute(Scene * /*scene*/, ustring name)
{
  if (name.empty()) {
    return false;
  }

  for (Node *node : used_shaders) {
    Shader *shader = static_cast<Shader *>(node);
    if (shader->attributes.find(name)) {
      return true;
    }
  }

  return false;
}

AttributeRequestSet Geometry::needed_attributes()
{
  AttributeRequestSet result;

  for (Node *node : used_shaders) {
    Shader *shader = static_cast<Shader *>(node);
    result.add(shader->attributes);
  }

  return result;
}

bool Geometry::has_voxel_attributes() const
{
  for (const Attribute &attr : attributes.attributes) {
    if (attr.element & ATTR_ELEMENT_VOXEL) {
      return true;
    }
  }

  return false;
}

/* Generate a normal attribute map entry from an attribute descriptor. */
static void emit_attribute_map_entry(AttributeMap *attr_map,
                                     const size_t index,
                                     const uint64_t id,
                                     const TypeDesc type,
                                     const AttributeDescriptor &desc)
{
  attr_map[index].id = id;
  attr_map[index].element = desc.element;
  attr_map[index].offset = as_uint(desc.offset);

  if (type == TypeFloat) {
    attr_map[index].type = NODE_ATTR_FLOAT;
  }
  else if (type == TypeMatrix) {
    attr_map[index].type = NODE_ATTR_MATRIX;
  }
  else if (type == TypeFloat2) {
    attr_map[index].type = NODE_ATTR_FLOAT2;
  }
  else if (type == TypeFloat4) {
    attr_map[index].type = NODE_ATTR_FLOAT4;
  }
  else if (type == TypeRGBA) {
    attr_map[index].type = NODE_ATTR_RGBA;
  }
  else {
    attr_map[index].type = NODE_ATTR_FLOAT3;
  }
}

/* Generate an attribute map end marker, optionally including a link to another map.
 * Links are used to connect object attribute maps to mesh attribute maps. */
static void emit_attribute_map_terminator(AttributeMap *attr_map,
                                          const size_t index,
                                          const bool chain,
                                          const uint chain_link)
{
  for (int j = 0; j < ATTR_PRIM_TYPES; j++) {
    attr_map[index + j].id = ATTR_STD_NONE;
    attr_map[index + j].element = chain;                     /* link is valid flag */
    attr_map[index + j].offset = chain ? chain_link + j : 0; /* link to the correct sub-entry */
    attr_map[index + j].type = 0;
  }
}

/* Generate all necessary attribute map entries from the attribute request. */
static void emit_attribute_mapping(AttributeMap *attr_map,
                                   const size_t index,
                                   const uint64_t id,
                                   AttributeRequest &req)
{
  emit_attribute_map_entry(attr_map, index, id, req.type, req.desc);
}

void GeometryManager::update_svm_attributes(Device * /*unused*/,
                                            DeviceScene *dscene,
                                            Scene *scene,
                                            vector<AttributeRequestSet> &geom_attributes,
                                            vector<AttributeRequestSet> &object_attributes)
{
  /* for SVM, the attributes_map table is used to lookup the offset of an
   * attribute, based on a unique shader attribute id. */
  const bool use_osl = scene->shader_manager->use_osl();

  /* compute array stride */
  size_t attr_map_size = 0;

  for (size_t i = 0; i < scene->geometry.size(); i++) {
    Geometry *geom = scene->geometry[i];
    geom->attr_map_offset = attr_map_size;

    size_t attr_count = 0;
    if (use_osl) {
      for (const AttributeRequest &req : geom_attributes[i].requests) {
        if (req.std != ATTR_STD_NONE &&
            scene->shader_manager->get_attribute_id(req.std) != (uint64_t)req.std)
        {
          attr_count += 2;
        }
        else {
          attr_count += 1;
        }
      }
    }
    else {
      attr_count = geom_attributes[i].size();
    }

    attr_map_size += (attr_count + 1) * ATTR_PRIM_TYPES;
  }

  for (size_t i = 0; i < scene->objects.size(); i++) {
    Object *object = scene->objects[i];

    /* only allocate a table for the object if it actually has attributes */
    if (object_attributes[i].size() == 0) {
      object->attr_map_offset = 0;
    }
    else {
      object->attr_map_offset = attr_map_size;
      attr_map_size += (object_attributes[i].size() + 1) * ATTR_PRIM_TYPES;
    }
  }

  if (attr_map_size == 0) {
    return;
  }

  if ((attr_map_size == dscene->attributes_map.size()) && !dscene->attributes_map.need_realloc()) {
    return;
  }

  /* create attribute map */
  AttributeMap *attr_map = dscene->attributes_map.alloc(attr_map_size);
  memset(attr_map, 0, dscene->attributes_map.size() * sizeof(*attr_map));

  for (size_t i = 0; i < scene->geometry.size(); i++) {
    Geometry *geom = scene->geometry[i];
    AttributeRequestSet &attributes = geom_attributes[i];

    /* set geometry attributes */
    size_t index = geom->attr_map_offset;

    for (AttributeRequest &req : attributes.requests) {
      uint64_t id;
      if (req.std == ATTR_STD_NONE) {
        id = scene->shader_manager->get_attribute_id(req.name);
      }
      else {
        id = scene->shader_manager->get_attribute_id(req.std);
      }

      emit_attribute_mapping(attr_map, index, id, req);
      index += ATTR_PRIM_TYPES;

      if (use_osl) {
        /* Some standard attributes are explicitly referenced via their standard ID, so add those
         * again in case they were added under a different attribute ID. */
        if (req.std != ATTR_STD_NONE && id != (uint64_t)req.std) {
          emit_attribute_mapping(attr_map, index, (uint64_t)req.std, req);
          index += ATTR_PRIM_TYPES;
        }
      }
    }

    emit_attribute_map_terminator(attr_map, index, false, 0);
  }

  for (size_t i = 0; i < scene->objects.size(); i++) {
    Object *object = scene->objects[i];
    AttributeRequestSet &attributes = object_attributes[i];

    /* set object attributes */
    if (attributes.size() > 0) {
      size_t index = object->attr_map_offset;

      for (AttributeRequest &req : attributes.requests) {
        uint64_t id;
        if (req.std == ATTR_STD_NONE) {
          id = scene->shader_manager->get_attribute_id(req.name);
        }
        else {
          id = scene->shader_manager->get_attribute_id(req.std);
        }

        emit_attribute_mapping(attr_map, index, id, req);
        index += ATTR_PRIM_TYPES;
      }

      emit_attribute_map_terminator(attr_map, index, true, object->geometry->attr_map_offset);
    }
  }

  /* copy to device */
  dscene->attributes_map.copy_to_device();
}

template<typename T> struct AttributeTableEntry {
  device_vector<T> &data;
  size_t offset;
  size_t size;

  void reserve(const size_t attr_size)
  {
    size += attr_size;
  }

  /* Templated on U since we'll want to assign float3 values to a packed_float3 device_vector. */
  template<typename U> size_t add(const U *attr_data, const size_t attr_size, const bool modified)
  {
    assert(data.size() >= offset + attr_size);
    size_t start_offset = offset;
    if (modified) {
      for (size_t k = 0; k < attr_size; k++) {
        data[offset + k] = attr_data[k];
      }
      data.tag_modified();
    }
    offset += attr_size;
    return start_offset;
  }

  void alloc()
  {
    data.alloc(size);
  }
};

class AttributeTableBuilder {
 public:
  AttributeTableBuilder(DeviceScene *dscene)
      : attr_float{dscene->attributes_float, 0, 0},
        attr_float2{dscene->attributes_float2, 0, 0},
        attr_float3{dscene->attributes_float3, 0, 0},
        attr_float4{dscene->attributes_float4, 0, 0},
        attr_uchar4{dscene->attributes_uchar4, 0, 0},
        attr_normal{dscene->attributes_normal, 0, 0}
  {
  }

  AttributeTableEntry<float> attr_float;
  AttributeTableEntry<float2> attr_float2;
  AttributeTableEntry<packed_float3> attr_float3;
  AttributeTableEntry<float4> attr_float4;
  AttributeTableEntry<uchar4> attr_uchar4;
  AttributeTableEntry<packed_normal> attr_normal;

  void add(Geometry *geom,
           Attribute *mattr,
           AttributePrimitive prim,
           TypeDesc &type,
           AttributeDescriptor &desc)
  {
    if (mattr == nullptr) {
      /* attribute not found */
      desc.element = ATTR_ELEMENT_NONE;
      desc.offset = 0;
      return;
    }

    /* store element and type */
    desc.element = mattr->element;
    type = mattr->type;

    /* store attribute data in arrays */
    const size_t size = mattr->element_size(geom, prim);

    const AttributeElement &element = desc.element;
    int &offset = desc.offset;

    if (mattr->element & ATTR_ELEMENT_VOXEL) {
      /* store slot in offset value */
      const ImageHandle &handle = mattr->data_voxel();
      offset = handle.svm_image_texture_id();
    }
    else if (mattr->element & ATTR_ELEMENT_IS_BYTE) {
      offset = attr_uchar4.add(mattr->data_uchar4(), size, mattr->modified);
    }
    else if (mattr->element & ATTR_ELEMENT_IS_NORMAL) {
      offset = attr_normal.add(mattr->data_normal(), size, mattr->modified);
    }
    else if (mattr->type == TypeFloat) {
      offset = attr_float.add(mattr->data_float(), size, mattr->modified);
    }
    else if (mattr->type == TypeFloat2) {
      offset = attr_float2.add(mattr->data_float2(), size, mattr->modified);
    }
    else if (mattr->type == TypeMatrix) {
      offset = attr_float4.add((float4 *)mattr->data_transform(), size * 3, mattr->modified);
    }
    else if (mattr->type == TypeFloat4 || mattr->type == TypeRGBA) {
      offset = attr_float4.add(mattr->data_float4(), size, mattr->modified);
    }
    else {
      offset = attr_float3.add(mattr->data_float3(), size, mattr->modified);
    }

    /* mesh vertex/curve index is global, not per object, so we sneak
     * a correction for that in here */
    if (geom->is_mesh()) {
      Mesh *mesh = static_cast<Mesh *>(geom);
      if (element & ATTR_ELEMENT_VERTEX) {
        offset -= mesh->vert_offset;
      }
      else if (element & ATTR_ELEMENT_FACE) {
        offset -= mesh->prim_offset;
      }
      else if (element & ATTR_ELEMENT_CORNER) {
        offset -= 3 * mesh->prim_offset;
      }
    }
    else if (geom->is_hair()) {
      Hair *hair = static_cast<Hair *>(geom);
      if (element & ATTR_ELEMENT_CURVE) {
        offset -= hair->prim_offset;
      }
      else if (element & ATTR_ELEMENT_CURVE_KEY) {
        offset -= hair->curve_key_offset;
      }
    }
    else if (geom->is_pointcloud()) {
      if (element & ATTR_ELEMENT_VERTEX) {
        offset -= geom->prim_offset;
      }
    }
  }

  void reserve(Geometry *geom, Attribute *mattr, AttributePrimitive prim)
  {
    if (mattr == nullptr) {
      return;
    }

    const size_t size = mattr->element_size(geom, prim);

    if (mattr->element & ATTR_ELEMENT_VOXEL) {
      /* pass */
    }
    else if (mattr->element & ATTR_ELEMENT_IS_BYTE) {
      attr_uchar4.reserve(size);
    }
    else if (mattr->element & ATTR_ELEMENT_IS_NORMAL) {
      attr_normal.reserve(size);
    }
    else if (mattr->type == TypeFloat) {
      attr_float.reserve(size);
    }
    else if (mattr->type == TypeFloat2) {
      attr_float2.reserve(size);
    }
    else if (mattr->type == TypeMatrix) {
      attr_float4.reserve(size * 3);
    }
    else if (mattr->type == TypeFloat4 || mattr->type == TypeRGBA) {
      attr_float4.reserve(size);
    }
    else {
      attr_float3.reserve(size);
    }
  }

  void alloc()
  {
    attr_float.alloc();
    attr_float2.alloc();
    attr_float3.alloc();
    attr_float4.alloc();
    attr_uchar4.alloc();
    attr_normal.alloc();
  }

  void copy_to_device_if_modified()
  {
    attr_float.data.copy_to_device_if_modified();
    attr_float2.data.copy_to_device_if_modified();
    attr_float3.data.copy_to_device_if_modified();
    attr_float4.data.copy_to_device_if_modified();
    attr_uchar4.data.copy_to_device_if_modified();
    attr_normal.data.copy_to_device_if_modified();
  }
};

void GeometryManager::device_update_attributes(Device *device,
                                               DeviceScene *dscene,
                                               Scene *scene,
                                               Progress &progress)
{
  progress.set_status("Updating Mesh", "Computing attributes");

  /* gather per mesh requested attributes. as meshes may have multiple
   * shaders assigned, this merges the requested attributes that have
   * been set per shader by the shader manager */
  vector<AttributeRequestSet> geom_attributes(scene->geometry.size());
  AttributeRequestSet global_attributes;
  scene->need_global_attributes(global_attributes);

  for (size_t i = 0; i < scene->geometry.size(); i++) {
    Geometry *geom = scene->geometry[i];

    geom->index = i;
    geom_attributes[i].add(global_attributes);

    for (Node *node : geom->get_used_shaders()) {
      Shader *shader = static_cast<Shader *>(node);
      geom_attributes[i].add(shader->attributes);
    }

    for (const Attribute &attr : geom->attributes.attributes) {
      switch (attr.std) {
        case ATTR_STD_VERTEX_NORMAL:
        case ATTR_STD_MOTION_VERTEX_NORMAL:
        case ATTR_STD_CORNER_NORMAL:
        case ATTR_STD_MOTION_CORNER_NORMAL:
        case ATTR_STD_SHADOW_TRANSPARENCY:
          geom_attributes[i].add(attr.std);
          break;
        default:
          break;
      }
    }
  }

  /* convert object attributes to use the same data structures as geometry ones */
  vector<AttributeRequestSet> object_attributes(scene->objects.size());
  vector<AttributeSet> object_attribute_values;

  object_attribute_values.reserve(scene->objects.size());

  for (size_t i = 0; i < scene->objects.size(); i++) {
    Object *object = scene->objects[i];
    Geometry *geom = object->geometry;
    const size_t geom_idx = geom->index;

    assert(geom_idx < scene->geometry.size() && scene->geometry[geom_idx] == geom);

    object_attribute_values.push_back(AttributeSet(geom, ATTR_PRIM_GEOMETRY));

    AttributeRequestSet &geom_requests = geom_attributes[geom_idx];
    AttributeRequestSet &attributes = object_attributes[i];
    AttributeSet &values = object_attribute_values[i];

    for (size_t j = 0; j < object->attributes.size(); j++) {
      const ParamValue &param = object->attributes[j];

      /* add attributes that are requested and not already handled by the mesh */
      if (geom_requests.find(param.name()) && !geom->attributes.find(param.name())) {
        attributes.add(param.name());

        Attribute *attr = values.add(param.name(), param.type(), ATTR_ELEMENT_OBJECT);
        assert(param.datasize() == attr->buffer.size());
        memcpy(attr->buffer.data(), param.data(), param.datasize());
      }
    }
  }

  /* mesh attribute are stored in a single array per data type. here we fill
   * those arrays, and set the offset and element type to create attribute
   * maps next */

  /* Pre-allocate attributes to avoid arrays re-allocation which would
   * take 2x of overall attribute memory usage.
   */
  AttributeTableBuilder builder(dscene);

  for (size_t i = 0; i < scene->geometry.size(); i++) {
    Geometry *geom = scene->geometry[i];
    AttributeRequestSet &attributes = geom_attributes[i];
    for (AttributeRequest &req : attributes.requests) {
      Attribute *attr = geom->attributes.find(req);
      builder.reserve(geom, attr, ATTR_PRIM_GEOMETRY);
    }
  }

  for (size_t i = 0; i < scene->objects.size(); i++) {
    Object *object = scene->objects[i];

    for (Attribute &attr : object_attribute_values[i].attributes) {
      builder.reserve(object->geometry, &attr, ATTR_PRIM_GEOMETRY);
    }
  }

  builder.alloc();

  /* The order of those flags needs to match that of AttrKernelDataType. */
  const bool attributes_need_realloc[AttrKernelDataType::NUM] = {
      dscene->attributes_float.need_realloc(),
      dscene->attributes_float2.need_realloc(),
      dscene->attributes_float3.need_realloc(),
      dscene->attributes_float4.need_realloc(),
      dscene->attributes_uchar4.need_realloc(),
      dscene->attributes_normal.need_realloc(),
  };

  /* Fill in attributes. */
  for (size_t i = 0; i < scene->geometry.size(); i++) {
    Geometry *geom = scene->geometry[i];
    AttributeRequestSet &attributes = geom_attributes[i];

    /* todo: we now store std and name attributes from requests even if
     * they actually refer to the same mesh attributes, optimize */
    for (AttributeRequest &req : attributes.requests) {
      Attribute *attr = geom->attributes.find(req);

      if (attr) {
        /* force a copy if we need to reallocate all the data */
        attr->modified |= attributes_need_realloc[Attribute::kernel_type(*attr)];
      }

      builder.add(geom, attr, ATTR_PRIM_GEOMETRY, req.type, req.desc);

      if (progress.get_cancel()) {
        return;
      }
    }
  }

  for (size_t i = 0; i < scene->objects.size(); i++) {
    Object *object = scene->objects[i];
    AttributeRequestSet &attributes = object_attributes[i];
    AttributeSet &values = object_attribute_values[i];

    for (AttributeRequest &req : attributes.requests) {
      Attribute *attr = values.find(req);

      if (attr) {
        attr->modified |= attributes_need_realloc[Attribute::kernel_type(*attr)];
      }

      builder.add(object->geometry, attr, ATTR_PRIM_GEOMETRY, req.type, req.desc);

      if (progress.get_cancel()) {
        return;
      }
    }
  }

  /* create attribute lookup maps */
  if (scene->shader_manager->use_osl()) {
    update_osl_globals(device, scene);
  }

  update_svm_attributes(device, dscene, scene, geom_attributes, object_attributes);

  if (progress.get_cancel()) {
    return;
  }

  /* copy to device */
  progress.set_status("Updating Mesh", "Copying Attributes to device");

  builder.copy_to_device_if_modified();

  if (progress.get_cancel()) {
    return;
  }

  /* After mesh attributes and patch tables have been copied to device memory,
   * we need to update offsets in the objects. */
  scene->object_manager->device_update_geom_offsets(device, dscene, scene);
}

CCL_NAMESPACE_END
