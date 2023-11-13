/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "bvh/bvh.h"
#include "bvh/bvh2.h"

#include "device/device.h"

#include "scene/attribute.h"
#include "scene/camera.h"
#include "scene/geometry.h"
#include "scene/hair.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/pointcloud.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_nodes.h"
#include "scene/stats.h"
#include "scene/volume.h"

#include "subd/patch_table.h"
#include "subd/split.h"

#include "kernel/osl/globals.h"

#include "util/foreach.h"
#include "util/log.h"
#include "util/progress.h"
#include "util/task.h"

CCL_NAMESPACE_BEGIN

bool Geometry::need_attribute(Scene *scene, AttributeStandard std)
{
  if (std == ATTR_STD_NONE) {
    return false;
  }

  if (scene->need_global_attribute(std)) {
    return true;
  }

  foreach (Node *node, used_shaders) {
    Shader *shader = static_cast<Shader *>(node);
    if (shader->attributes.find(std)) {
      return true;
    }
  }

  return false;
}

bool Geometry::need_attribute(Scene * /*scene*/, ustring name)
{
  if (name == ustring()) {
    return false;
  }

  foreach (Node *node, used_shaders) {
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

  foreach (Node *node, used_shaders) {
    Shader *shader = static_cast<Shader *>(node);
    result.add(shader->attributes);
  }

  return result;
}

bool Geometry::has_voxel_attributes() const
{
  foreach (const Attribute &attr, attributes.attributes) {
    if (attr.element == ATTR_ELEMENT_VOXEL) {
      return true;
    }
  }

  return false;
}

/* Generate a normal attribute map entry from an attribute descriptor. */
static void emit_attribute_map_entry(AttributeMap *attr_map,
                                     size_t index,
                                     uint64_t id,
                                     TypeDesc type,
                                     const AttributeDescriptor &desc)
{
  attr_map[index].id = id;
  attr_map[index].element = desc.element;
  attr_map[index].offset = as_uint(desc.offset);

  if (type == TypeDesc::TypeFloat) {
    attr_map[index].type = NODE_ATTR_FLOAT;
  }
  else if (type == TypeDesc::TypeMatrix) {
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

  attr_map[index].flags = desc.flags;
}

/* Generate an attribute map end marker, optionally including a link to another map.
 * Links are used to connect object attribute maps to mesh attribute maps. */
static void emit_attribute_map_terminator(AttributeMap *attr_map,
                                          size_t index,
                                          bool chain,
                                          uint chain_link)
{
  for (int j = 0; j < ATTR_PRIM_TYPES; j++) {
    attr_map[index + j].id = ATTR_STD_NONE;
    attr_map[index + j].element = chain;                     /* link is valid flag */
    attr_map[index + j].offset = chain ? chain_link + j : 0; /* link to the correct sub-entry */
    attr_map[index + j].type = 0;
    attr_map[index + j].flags = 0;
  }
}

/* Generate all necessary attribute map entries from the attribute request. */
static void emit_attribute_mapping(
    AttributeMap *attr_map, size_t index, uint64_t id, AttributeRequest &req, Geometry *geom)
{
  emit_attribute_map_entry(attr_map, index, id, req.type, req.desc);

  if (geom->is_mesh()) {
    Mesh *mesh = static_cast<Mesh *>(geom);
    if (mesh->get_num_subd_faces()) {
      emit_attribute_map_entry(attr_map, index + 1, id, req.subd_type, req.subd_desc);
    }
  }
}

void GeometryManager::update_svm_attributes(Device *,
                                            DeviceScene *dscene,
                                            Scene *scene,
                                            vector<AttributeRequestSet> &geom_attributes,
                                            vector<AttributeRequestSet> &object_attributes)
{
  /* for SVM, the attributes_map table is used to lookup the offset of an
   * attribute, based on a unique shader attribute id. */

  /* compute array stride */
  size_t attr_map_size = 0;

  for (size_t i = 0; i < scene->geometry.size(); i++) {
    Geometry *geom = scene->geometry[i];
    geom->attr_map_offset = attr_map_size;

#ifdef WITH_OSL
    size_t attr_count = 0;
    foreach (AttributeRequest &req, geom_attributes[i].requests) {
      if (req.std != ATTR_STD_NONE &&
          scene->shader_manager->get_attribute_id(req.std) != (uint64_t)req.std)
        attr_count += 2;
      else
        attr_count += 1;
    }
#else
    const size_t attr_count = geom_attributes[i].size();
#endif

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

  if (!dscene->attributes_map.need_realloc()) {
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

    foreach (AttributeRequest &req, attributes.requests) {
      uint64_t id;
      if (req.std == ATTR_STD_NONE) {
        id = scene->shader_manager->get_attribute_id(req.name);
      }
      else {
        id = scene->shader_manager->get_attribute_id(req.std);
      }

      emit_attribute_mapping(attr_map, index, id, req, geom);
      index += ATTR_PRIM_TYPES;

#ifdef WITH_OSL
      /* Some standard attributes are explicitly referenced via their standard ID, so add those
       * again in case they were added under a different attribute ID. */
      if (req.std != ATTR_STD_NONE && id != (uint64_t)req.std) {
        emit_attribute_mapping(attr_map, index, (uint64_t)req.std, req, geom);
        index += ATTR_PRIM_TYPES;
      }
#endif
    }

    emit_attribute_map_terminator(attr_map, index, false, 0);
  }

  for (size_t i = 0; i < scene->objects.size(); i++) {
    Object *object = scene->objects[i];
    AttributeRequestSet &attributes = object_attributes[i];

    /* set object attributes */
    if (attributes.size() > 0) {
      size_t index = object->attr_map_offset;

      foreach (AttributeRequest &req, attributes.requests) {
        uint64_t id;
        if (req.std == ATTR_STD_NONE) {
          id = scene->shader_manager->get_attribute_id(req.name);
        }
        else {
          id = scene->shader_manager->get_attribute_id(req.std);
        }

        emit_attribute_mapping(attr_map, index, id, req, object->geometry);
        index += ATTR_PRIM_TYPES;
      }

      emit_attribute_map_terminator(attr_map, index, true, object->geometry->attr_map_offset);
    }
  }

  /* copy to device */
  dscene->attributes_map.copy_to_device();
}

void GeometryManager::update_attribute_element_offset(Geometry *geom,
                                                      device_vector<float> &attr_float,
                                                      size_t &attr_float_offset,
                                                      device_vector<float2> &attr_float2,
                                                      size_t &attr_float2_offset,
                                                      device_vector<packed_float3> &attr_float3,
                                                      size_t &attr_float3_offset,
                                                      device_vector<float4> &attr_float4,
                                                      size_t &attr_float4_offset,
                                                      device_vector<uchar4> &attr_uchar4,
                                                      size_t &attr_uchar4_offset,
                                                      Attribute *mattr,
                                                      AttributePrimitive prim,
                                                      TypeDesc &type,
                                                      AttributeDescriptor &desc)
{
  if (mattr) {
    /* store element and type */
    desc.element = mattr->element;
    desc.flags = mattr->flags;
    type = mattr->type;

    /* store attribute data in arrays */
    size_t size = mattr->element_size(geom, prim);

    AttributeElement &element = desc.element;
    int &offset = desc.offset;

    if (mattr->element == ATTR_ELEMENT_VOXEL) {
      /* store slot in offset value */
      ImageHandle &handle = mattr->data_voxel();
      offset = handle.svm_slot();
    }
    else if (mattr->element == ATTR_ELEMENT_CORNER_BYTE) {
      uchar4 *data = mattr->data_uchar4();
      offset = attr_uchar4_offset;

      assert(attr_uchar4.size() >= offset + size);
      if (mattr->modified) {
        for (size_t k = 0; k < size; k++) {
          attr_uchar4[offset + k] = data[k];
        }
        attr_uchar4.tag_modified();
      }
      attr_uchar4_offset += size;
    }
    else if (mattr->type == TypeDesc::TypeFloat) {
      float *data = mattr->data_float();
      offset = attr_float_offset;

      assert(attr_float.size() >= offset + size);
      if (mattr->modified) {
        for (size_t k = 0; k < size; k++) {
          attr_float[offset + k] = data[k];
        }
        attr_float.tag_modified();
      }
      attr_float_offset += size;
    }
    else if (mattr->type == TypeFloat2) {
      float2 *data = mattr->data_float2();
      offset = attr_float2_offset;

      assert(attr_float2.size() >= offset + size);
      if (mattr->modified) {
        for (size_t k = 0; k < size; k++) {
          attr_float2[offset + k] = data[k];
        }
        attr_float2.tag_modified();
      }
      attr_float2_offset += size;
    }
    else if (mattr->type == TypeDesc::TypeMatrix) {
      Transform *tfm = mattr->data_transform();
      offset = attr_float4_offset;

      assert(attr_float4.size() >= offset + size * 3);
      if (mattr->modified) {
        for (size_t k = 0; k < size * 3; k++) {
          attr_float4[offset + k] = (&tfm->x)[k];
        }
        attr_float4.tag_modified();
      }
      attr_float4_offset += size * 3;
    }
    else if (mattr->type == TypeFloat4 || mattr->type == TypeRGBA) {
      float4 *data = mattr->data_float4();
      offset = attr_float4_offset;

      assert(attr_float4.size() >= offset + size);
      if (mattr->modified) {
        for (size_t k = 0; k < size; k++) {
          attr_float4[offset + k] = data[k];
        }
        attr_float4.tag_modified();
      }
      attr_float4_offset += size;
    }
    else {
      float3 *data = mattr->data_float3();
      offset = attr_float3_offset;

      assert(attr_float3.size() >= offset + size);
      if (mattr->modified) {
        for (size_t k = 0; k < size; k++) {
          attr_float3[offset + k] = data[k];
        }
        attr_float3.tag_modified();
      }
      attr_float3_offset += size;
    }

    /* mesh vertex/curve index is global, not per object, so we sneak
     * a correction for that in here */
    if (geom->is_mesh()) {
      Mesh *mesh = static_cast<Mesh *>(geom);
      if (mesh->subdivision_type == Mesh::SUBDIVISION_CATMULL_CLARK &&
          desc.flags & ATTR_SUBDIVIDED) {
        /* Indices for subdivided attributes are retrieved
         * from patch table so no need for correction here. */
      }
      else if (element == ATTR_ELEMENT_VERTEX) {
        offset -= mesh->vert_offset;
      }
      else if (element == ATTR_ELEMENT_VERTEX_MOTION) {
        offset -= mesh->vert_offset;
      }
      else if (element == ATTR_ELEMENT_FACE) {
        if (prim == ATTR_PRIM_GEOMETRY) {
          offset -= mesh->prim_offset;
        }
        else {
          offset -= mesh->face_offset;
        }
      }
      else if (element == ATTR_ELEMENT_CORNER || element == ATTR_ELEMENT_CORNER_BYTE) {
        if (prim == ATTR_PRIM_GEOMETRY) {
          offset -= 3 * mesh->prim_offset;
        }
        else {
          offset -= mesh->corner_offset;
        }
      }
    }
    else if (geom->is_hair()) {
      Hair *hair = static_cast<Hair *>(geom);
      if (element == ATTR_ELEMENT_CURVE) {
        offset -= hair->prim_offset;
      }
      else if (element == ATTR_ELEMENT_CURVE_KEY) {
        offset -= hair->curve_key_offset;
      }
      else if (element == ATTR_ELEMENT_CURVE_KEY_MOTION) {
        offset -= hair->curve_key_offset;
      }
    }
    else if (geom->is_pointcloud()) {
      if (element == ATTR_ELEMENT_VERTEX) {
        offset -= geom->prim_offset;
      }
      else if (element == ATTR_ELEMENT_VERTEX_MOTION) {
        offset -= geom->prim_offset;
      }
    }
  }
  else {
    /* attribute not found */
    desc.element = ATTR_ELEMENT_NONE;
    desc.offset = 0;
  }
}

static void update_attribute_element_size(Geometry *geom,
                                          Attribute *mattr,
                                          AttributePrimitive prim,
                                          size_t *attr_float_size,
                                          size_t *attr_float2_size,
                                          size_t *attr_float3_size,
                                          size_t *attr_float4_size,
                                          size_t *attr_uchar4_size)
{
  if (mattr) {
    size_t size = mattr->element_size(geom, prim);

    if (mattr->element == ATTR_ELEMENT_VOXEL) {
      /* pass */
    }
    else if (mattr->element == ATTR_ELEMENT_CORNER_BYTE) {
      *attr_uchar4_size += size;
    }
    else if (mattr->type == TypeDesc::TypeFloat) {
      *attr_float_size += size;
    }
    else if (mattr->type == TypeFloat2) {
      *attr_float2_size += size;
    }
    else if (mattr->type == TypeDesc::TypeMatrix) {
      *attr_float4_size += size * 4;
    }
    else if (mattr->type == TypeFloat4 || mattr->type == TypeRGBA) {
      *attr_float4_size += size;
    }
    else {
      *attr_float3_size += size;
    }
  }
}

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

  for (size_t i = 0; i < scene->geometry.size(); i++) {
    Geometry *geom = scene->geometry[i];

    geom->index = i;
    scene->need_global_attributes(geom_attributes[i]);

    foreach (Node *node, geom->get_used_shaders()) {
      Shader *shader = static_cast<Shader *>(node);
      geom_attributes[i].add(shader->attributes);
    }

    if (geom->is_hair() && static_cast<Hair *>(geom)->need_shadow_transparency()) {
      geom_attributes[i].add(ATTR_STD_SHADOW_TRANSPARENCY);
    }
  }

  /* convert object attributes to use the same data structures as geometry ones */
  vector<AttributeRequestSet> object_attributes(scene->objects.size());
  vector<AttributeSet> object_attribute_values;

  object_attribute_values.reserve(scene->objects.size());

  for (size_t i = 0; i < scene->objects.size(); i++) {
    Object *object = scene->objects[i];
    Geometry *geom = object->geometry;
    size_t geom_idx = geom->index;

    assert(geom_idx < scene->geometry.size() && scene->geometry[geom_idx] == geom);

    object_attribute_values.push_back(AttributeSet(geom, ATTR_PRIM_GEOMETRY));

    AttributeRequestSet &geom_requests = geom_attributes[geom_idx];
    AttributeRequestSet &attributes = object_attributes[i];
    AttributeSet &values = object_attribute_values[i];

    for (size_t j = 0; j < object->attributes.size(); j++) {
      ParamValue &param = object->attributes[j];

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
  size_t attr_float_size = 0;
  size_t attr_float2_size = 0;
  size_t attr_float3_size = 0;
  size_t attr_float4_size = 0;
  size_t attr_uchar4_size = 0;

  for (size_t i = 0; i < scene->geometry.size(); i++) {
    Geometry *geom = scene->geometry[i];
    AttributeRequestSet &attributes = geom_attributes[i];
    foreach (AttributeRequest &req, attributes.requests) {
      Attribute *attr = geom->attributes.find(req);

      update_attribute_element_size(geom,
                                    attr,
                                    ATTR_PRIM_GEOMETRY,
                                    &attr_float_size,
                                    &attr_float2_size,
                                    &attr_float3_size,
                                    &attr_float4_size,
                                    &attr_uchar4_size);

      if (geom->is_mesh()) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        Attribute *subd_attr = mesh->subd_attributes.find(req);

        update_attribute_element_size(mesh,
                                      subd_attr,
                                      ATTR_PRIM_SUBD,
                                      &attr_float_size,
                                      &attr_float2_size,
                                      &attr_float3_size,
                                      &attr_float4_size,
                                      &attr_uchar4_size);
      }
    }
  }

  for (size_t i = 0; i < scene->objects.size(); i++) {
    Object *object = scene->objects[i];

    foreach (Attribute &attr, object_attribute_values[i].attributes) {
      update_attribute_element_size(object->geometry,
                                    &attr,
                                    ATTR_PRIM_GEOMETRY,
                                    &attr_float_size,
                                    &attr_float2_size,
                                    &attr_float3_size,
                                    &attr_float4_size,
                                    &attr_uchar4_size);
    }
  }

  dscene->attributes_float.alloc(attr_float_size);
  dscene->attributes_float2.alloc(attr_float2_size);
  dscene->attributes_float3.alloc(attr_float3_size);
  dscene->attributes_float4.alloc(attr_float4_size);
  dscene->attributes_uchar4.alloc(attr_uchar4_size);

  /* The order of those flags needs to match that of AttrKernelDataType. */
  const bool attributes_need_realloc[AttrKernelDataType::NUM] = {
      dscene->attributes_float.need_realloc(),
      dscene->attributes_float2.need_realloc(),
      dscene->attributes_float3.need_realloc(),
      dscene->attributes_float4.need_realloc(),
      dscene->attributes_uchar4.need_realloc(),
  };

  size_t attr_float_offset = 0;
  size_t attr_float2_offset = 0;
  size_t attr_float3_offset = 0;
  size_t attr_float4_offset = 0;
  size_t attr_uchar4_offset = 0;

  /* Fill in attributes. */
  for (size_t i = 0; i < scene->geometry.size(); i++) {
    Geometry *geom = scene->geometry[i];
    AttributeRequestSet &attributes = geom_attributes[i];

    /* todo: we now store std and name attributes from requests even if
     * they actually refer to the same mesh attributes, optimize */
    foreach (AttributeRequest &req, attributes.requests) {
      Attribute *attr = geom->attributes.find(req);

      if (attr) {
        /* force a copy if we need to reallocate all the data */
        attr->modified |= attributes_need_realloc[Attribute::kernel_type(*attr)];
      }

      update_attribute_element_offset(geom,
                                      dscene->attributes_float,
                                      attr_float_offset,
                                      dscene->attributes_float2,
                                      attr_float2_offset,
                                      dscene->attributes_float3,
                                      attr_float3_offset,
                                      dscene->attributes_float4,
                                      attr_float4_offset,
                                      dscene->attributes_uchar4,
                                      attr_uchar4_offset,
                                      attr,
                                      ATTR_PRIM_GEOMETRY,
                                      req.type,
                                      req.desc);

      if (geom->is_mesh()) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        Attribute *subd_attr = mesh->subd_attributes.find(req);

        if (subd_attr) {
          /* force a copy if we need to reallocate all the data */
          subd_attr->modified |= attributes_need_realloc[Attribute::kernel_type(*subd_attr)];
        }

        update_attribute_element_offset(mesh,
                                        dscene->attributes_float,
                                        attr_float_offset,
                                        dscene->attributes_float2,
                                        attr_float2_offset,
                                        dscene->attributes_float3,
                                        attr_float3_offset,
                                        dscene->attributes_float4,
                                        attr_float4_offset,
                                        dscene->attributes_uchar4,
                                        attr_uchar4_offset,
                                        subd_attr,
                                        ATTR_PRIM_SUBD,
                                        req.subd_type,
                                        req.subd_desc);
      }

      if (progress.get_cancel()) {
        return;
      }
    }
  }

  for (size_t i = 0; i < scene->objects.size(); i++) {
    Object *object = scene->objects[i];
    AttributeRequestSet &attributes = object_attributes[i];
    AttributeSet &values = object_attribute_values[i];

    foreach (AttributeRequest &req, attributes.requests) {
      Attribute *attr = values.find(req);

      if (attr) {
        attr->modified |= attributes_need_realloc[Attribute::kernel_type(*attr)];
      }

      update_attribute_element_offset(object->geometry,
                                      dscene->attributes_float,
                                      attr_float_offset,
                                      dscene->attributes_float2,
                                      attr_float2_offset,
                                      dscene->attributes_float3,
                                      attr_float3_offset,
                                      dscene->attributes_float4,
                                      attr_float4_offset,
                                      dscene->attributes_uchar4,
                                      attr_uchar4_offset,
                                      attr,
                                      ATTR_PRIM_GEOMETRY,
                                      req.type,
                                      req.desc);

      /* object attributes don't care about subdivision */
      req.subd_type = req.type;
      req.subd_desc = req.desc;

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

  dscene->attributes_float.copy_to_device_if_modified();
  dscene->attributes_float2.copy_to_device_if_modified();
  dscene->attributes_float3.copy_to_device_if_modified();
  dscene->attributes_float4.copy_to_device_if_modified();
  dscene->attributes_uchar4.copy_to_device_if_modified();

  if (progress.get_cancel()) {
    return;
  }

  /* After mesh attributes and patch tables have been copied to device memory,
   * we need to update offsets in the objects. */
  scene->object_manager->device_update_geom_offsets(device, dscene, scene);
}

CCL_NAMESPACE_END
