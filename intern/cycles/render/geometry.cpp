/*
 * Copyright 2011-2020 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bvh/bvh.h"
#include "bvh/bvh2.h"

#include "device/device.h"

#include "render/attribute.h"
#include "render/camera.h"
#include "render/geometry.h"
#include "render/hair.h"
#include "render/light.h"
#include "render/mesh.h"
#include "render/nodes.h"
#include "render/object.h"
#include "render/scene.h"
#include "render/shader.h"
#include "render/stats.h"
#include "render/volume.h"

#include "subd/subd_patch_table.h"
#include "subd/subd_split.h"

#include "kernel/osl/osl_globals.h"

#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_progress.h"
#include "util/util_task.h"

CCL_NAMESPACE_BEGIN

/* Geometry */

NODE_ABSTRACT_DEFINE(Geometry)
{
  NodeType *type = NodeType::add("geometry_base", NULL);

  SOCKET_UINT(motion_steps, "Motion Steps", 3);
  SOCKET_BOOLEAN(use_motion_blur, "Use Motion Blur", false);
  SOCKET_NODE_ARRAY(used_shaders, "Shaders", Shader::get_node_type());

  return type;
}

Geometry::Geometry(const NodeType *node_type, const Type type)
    : Node(node_type), geometry_type(type), attributes(this, ATTR_PRIM_GEOMETRY)
{
  need_update_rebuild = false;
  need_update_bvh_for_offset = false;

  transform_applied = false;
  transform_negative_scaled = false;
  transform_normal = transform_identity();
  bounds = BoundBox::empty;

  has_volume = false;
  has_surface_bssrdf = false;

  bvh = NULL;
  attr_map_offset = 0;
  optix_prim_offset = 0;
  prim_offset = 0;
}

Geometry::~Geometry()
{
  dereference_all_used_nodes();
  delete bvh;
}

void Geometry::clear(bool preserve_shaders)
{
  if (!preserve_shaders)
    used_shaders.clear();

  transform_applied = false;
  transform_negative_scaled = false;
  transform_normal = transform_identity();
  tag_modified();
}

bool Geometry::need_attribute(Scene *scene, AttributeStandard std)
{
  if (std == ATTR_STD_NONE)
    return false;

  if (scene->need_global_attribute(std))
    return true;

  foreach (Node *node, used_shaders) {
    Shader *shader = static_cast<Shader *>(node);
    if (shader->attributes.find(std))
      return true;
  }

  return false;
}

bool Geometry::need_attribute(Scene * /*scene*/, ustring name)
{
  if (name == ustring())
    return false;

  foreach (Node *node, used_shaders) {
    Shader *shader = static_cast<Shader *>(node);
    if (shader->attributes.find(name))
      return true;
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

float Geometry::motion_time(int step) const
{
  return (motion_steps > 1) ? 2.0f * step / (motion_steps - 1) - 1.0f : 0.0f;
}

int Geometry::motion_step(float time) const
{
  if (motion_steps > 1) {
    int attr_step = 0;

    for (int step = 0; step < motion_steps; step++) {
      float step_time = motion_time(step);
      if (step_time == time) {
        return attr_step;
      }

      /* Center step is stored in a separate attribute. */
      if (step != motion_steps / 2) {
        attr_step++;
      }
    }
  }

  return -1;
}

bool Geometry::need_build_bvh(BVHLayout layout) const
{
  return is_instanced() || layout == BVH_LAYOUT_OPTIX || layout == BVH_LAYOUT_MULTI_OPTIX ||
         layout == BVH_LAYOUT_MULTI_OPTIX_EMBREE;
}

bool Geometry::is_instanced() const
{
  /* Currently we treat subsurface objects as instanced.
   *
   * While it might be not very optimal for ray traversal, it avoids having
   * duplicated BVH in the memory, saving quite some space.
   */
  return !transform_applied || has_surface_bssrdf;
}

bool Geometry::has_true_displacement() const
{
  foreach (Node *node, used_shaders) {
    Shader *shader = static_cast<Shader *>(node);
    if (shader->has_displacement && shader->get_displacement_method() != DISPLACE_BUMP) {
      return true;
    }
  }

  return false;
}

void Geometry::compute_bvh(
    Device *device, DeviceScene *dscene, SceneParams *params, Progress *progress, int n, int total)
{
  if (progress->get_cancel())
    return;

  compute_bounds();

  const BVHLayout bvh_layout = BVHParams::best_bvh_layout(params->bvh_layout,
                                                          device->get_bvh_layout_mask());
  if (need_build_bvh(bvh_layout)) {
    string msg = "Updating Geometry BVH ";
    if (name.empty())
      msg += string_printf("%u/%u", (uint)(n + 1), (uint)total);
    else
      msg += string_printf("%s %u/%u", name.c_str(), (uint)(n + 1), (uint)total);

    Object object;
    object.set_geometry(this);

    vector<Geometry *> geometry;
    geometry.push_back(this);
    vector<Object *> objects;
    objects.push_back(&object);

    if (bvh && !need_update_rebuild) {
      progress->set_status(msg, "Refitting BVH");

      bvh->geometry = geometry;
      bvh->objects = objects;

      device->build_bvh(bvh, *progress, true);
    }
    else {
      progress->set_status(msg, "Building BVH");

      BVHParams bparams;
      bparams.use_spatial_split = params->use_bvh_spatial_split;
      bparams.bvh_layout = bvh_layout;
      bparams.use_unaligned_nodes = dscene->data.bvh.have_curves &&
                                    params->use_bvh_unaligned_nodes;
      bparams.num_motion_triangle_steps = params->num_bvh_time_steps;
      bparams.num_motion_curve_steps = params->num_bvh_time_steps;
      bparams.bvh_type = params->bvh_type;
      bparams.curve_subdivisions = params->curve_subdivisions();

      delete bvh;
      bvh = BVH::create(bparams, geometry, objects, device);
      MEM_GUARDED_CALL(progress, device->build_bvh, bvh, *progress, false);
    }
  }

  need_update_rebuild = false;
  need_update_bvh_for_offset = false;
}

bool Geometry::has_motion_blur() const
{
  return (use_motion_blur && attributes.find(ATTR_STD_MOTION_VERTEX_POSITION));
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

void Geometry::tag_update(Scene *scene, bool rebuild)
{
  if (rebuild) {
    need_update_rebuild = true;
    scene->light_manager->tag_update(scene, LightManager::MESH_NEED_REBUILD);
  }
  else {
    foreach (Node *node, used_shaders) {
      Shader *shader = static_cast<Shader *>(node);
      if (shader->has_surface_emission) {
        scene->light_manager->tag_update(scene, LightManager::EMISSIVE_MESH_MODIFIED);
        break;
      }
    }
  }

  scene->geometry_manager->tag_update(scene, GeometryManager::GEOMETRY_MODIFIED);
}

void Geometry::tag_bvh_update(bool rebuild)
{
  tag_modified();

  if (rebuild) {
    need_update_rebuild = true;
  }
}

/* Geometry Manager */

GeometryManager::GeometryManager()
{
  update_flags = UPDATE_ALL;
  need_flags_update = true;
}

GeometryManager::~GeometryManager()
{
}

void GeometryManager::update_osl_attributes(Device *device,
                                            Scene *scene,
                                            vector<AttributeRequestSet> &geom_attributes)
{
#ifdef WITH_OSL
  /* for OSL, a hash map is used to lookup the attribute by name. */
  OSLGlobals *og = (OSLGlobals *)device->osl_memory();

  og->object_name_map.clear();
  og->attribute_map.clear();
  og->object_names.clear();

  og->attribute_map.resize(scene->objects.size() * ATTR_PRIM_TYPES);

  for (size_t i = 0; i < scene->objects.size(); i++) {
    /* set object name to object index map */
    Object *object = scene->objects[i];
    og->object_name_map[object->name] = i;
    og->object_names.push_back(object->name);

    /* set object attributes */
    foreach (ParamValue &attr, object->attributes) {
      OSLGlobals::Attribute osl_attr;

      osl_attr.type = attr.type();
      osl_attr.desc.element = ATTR_ELEMENT_OBJECT;
      osl_attr.value = attr;
      osl_attr.desc.offset = 0;
      osl_attr.desc.flags = 0;

      og->attribute_map[i * ATTR_PRIM_TYPES + ATTR_PRIM_GEOMETRY][attr.name()] = osl_attr;
      og->attribute_map[i * ATTR_PRIM_TYPES + ATTR_PRIM_SUBD][attr.name()] = osl_attr;
    }

    /* find geometry attributes */
    size_t j = object->geometry->index;
    assert(j < scene->geometry.size() && scene->geometry[j] == object->geometry);

    AttributeRequestSet &attributes = geom_attributes[j];

    /* set mesh attributes */
    foreach (AttributeRequest &req, attributes.requests) {
      OSLGlobals::Attribute osl_attr;

      if (req.desc.element != ATTR_ELEMENT_NONE) {
        osl_attr.desc = req.desc;

        if (req.type == TypeDesc::TypeFloat)
          osl_attr.type = TypeDesc::TypeFloat;
        else if (req.type == TypeDesc::TypeMatrix)
          osl_attr.type = TypeDesc::TypeMatrix;
        else if (req.type == TypeFloat2)
          osl_attr.type = TypeFloat2;
        else if (req.type == TypeRGBA)
          osl_attr.type = TypeRGBA;
        else
          osl_attr.type = TypeDesc::TypeColor;

        if (req.std != ATTR_STD_NONE) {
          /* if standard attribute, add lookup by geom: name convention */
          ustring stdname(string("geom:") + string(Attribute::standard_name(req.std)));
          og->attribute_map[i * ATTR_PRIM_TYPES + ATTR_PRIM_GEOMETRY][stdname] = osl_attr;
        }
        else if (req.name != ustring()) {
          /* add lookup by geometry attribute name */
          og->attribute_map[i * ATTR_PRIM_TYPES + ATTR_PRIM_GEOMETRY][req.name] = osl_attr;
        }
      }

      if (req.subd_desc.element != ATTR_ELEMENT_NONE) {
        osl_attr.desc = req.subd_desc;

        if (req.subd_type == TypeDesc::TypeFloat)
          osl_attr.type = TypeDesc::TypeFloat;
        else if (req.subd_type == TypeDesc::TypeMatrix)
          osl_attr.type = TypeDesc::TypeMatrix;
        else if (req.subd_type == TypeFloat2)
          osl_attr.type = TypeFloat2;
        else if (req.subd_type == TypeRGBA)
          osl_attr.type = TypeRGBA;
        else
          osl_attr.type = TypeDesc::TypeColor;

        if (req.std != ATTR_STD_NONE) {
          /* if standard attribute, add lookup by geom: name convention */
          ustring stdname(string("geom:") + string(Attribute::standard_name(req.std)));
          og->attribute_map[i * ATTR_PRIM_TYPES + ATTR_PRIM_SUBD][stdname] = osl_attr;
        }
        else if (req.name != ustring()) {
          /* add lookup by geometry attribute name */
          og->attribute_map[i * ATTR_PRIM_TYPES + ATTR_PRIM_SUBD][req.name] = osl_attr;
        }
      }
    }
  }
#else
  (void)device;
  (void)scene;
  (void)geom_attributes;
#endif
}

/* Generate a normal attribute map entry from an attribute descriptor. */
static void emit_attribute_map_entry(
    uint4 *attr_map, int index, uint id, TypeDesc type, const AttributeDescriptor &desc)
{
  attr_map[index].x = id;
  attr_map[index].y = desc.element;
  attr_map[index].z = as_uint(desc.offset);

  if (type == TypeDesc::TypeFloat)
    attr_map[index].w = NODE_ATTR_FLOAT;
  else if (type == TypeDesc::TypeMatrix)
    attr_map[index].w = NODE_ATTR_MATRIX;
  else if (type == TypeFloat2)
    attr_map[index].w = NODE_ATTR_FLOAT2;
  else if (type == TypeFloat4)
    attr_map[index].w = NODE_ATTR_FLOAT4;
  else if (type == TypeRGBA)
    attr_map[index].w = NODE_ATTR_RGBA;
  else
    attr_map[index].w = NODE_ATTR_FLOAT3;

  attr_map[index].w |= desc.flags << 8;
}

/* Generate an attribute map end marker, optionally including a link to another map.
 * Links are used to connect object attribute maps to mesh attribute maps. */
static void emit_attribute_map_terminator(uint4 *attr_map, int index, bool chain, uint chain_link)
{
  for (int j = 0; j < ATTR_PRIM_TYPES; j++) {
    attr_map[index + j].x = ATTR_STD_NONE;
    attr_map[index + j].y = chain;                      /* link is valid flag */
    attr_map[index + j].z = chain ? chain_link + j : 0; /* link to the correct sub-entry */
    attr_map[index + j].w = 0;
  }
}

/* Generate all necessary attribute map entries from the attribute request. */
static void emit_attribute_mapping(
    uint4 *attr_map, int index, Scene *scene, AttributeRequest &req, Geometry *geom)
{
  uint id;

  if (req.std == ATTR_STD_NONE)
    id = scene->shader_manager->get_attribute_id(req.name);
  else
    id = scene->shader_manager->get_attribute_id(req.std);

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
  int attr_map_size = 0;

  for (size_t i = 0; i < scene->geometry.size(); i++) {
    Geometry *geom = scene->geometry[i];
    geom->attr_map_offset = attr_map_size;
    attr_map_size += (geom_attributes[i].size() + 1) * ATTR_PRIM_TYPES;
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

  if (attr_map_size == 0)
    return;

  if (!dscene->attributes_map.need_realloc()) {
    return;
  }

  /* create attribute map */
  uint4 *attr_map = dscene->attributes_map.alloc(attr_map_size);
  memset(attr_map, 0, dscene->attributes_map.size() * sizeof(uint));

  for (size_t i = 0; i < scene->geometry.size(); i++) {
    Geometry *geom = scene->geometry[i];
    AttributeRequestSet &attributes = geom_attributes[i];

    /* set geometry attributes */
    int index = geom->attr_map_offset;

    foreach (AttributeRequest &req, attributes.requests) {
      emit_attribute_mapping(attr_map, index, scene, req, geom);
      index += ATTR_PRIM_TYPES;
    }

    emit_attribute_map_terminator(attr_map, index, false, 0);
  }

  for (size_t i = 0; i < scene->objects.size(); i++) {
    Object *object = scene->objects[i];
    AttributeRequestSet &attributes = object_attributes[i];

    /* set object attributes */
    if (attributes.size() > 0) {
      int index = object->attr_map_offset;

      foreach (AttributeRequest &req, attributes.requests) {
        emit_attribute_mapping(attr_map, index, scene, req, object->geometry);
        index += ATTR_PRIM_TYPES;
      }

      emit_attribute_map_terminator(attr_map, index, true, object->geometry->attr_map_offset);
    }
  }

  /* copy to device */
  dscene->attributes_map.copy_to_device();
}

static void update_attribute_element_size(Geometry *geom,
                                          Attribute *mattr,
                                          AttributePrimitive prim,
                                          size_t *attr_float_size,
                                          size_t *attr_float2_size,
                                          size_t *attr_float3_size,
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
      *attr_float3_size += size * 4;
    }
    else {
      *attr_float3_size += size;
    }
  }
}

void GeometryManager::update_attribute_element_offset(Geometry *geom,
                                                      device_vector<float> &attr_float,
                                                      size_t &attr_float_offset,
                                                      device_vector<float2> &attr_float2,
                                                      size_t &attr_float2_offset,
                                                      device_vector<float4> &attr_float3,
                                                      size_t &attr_float3_offset,
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
      }
      attr_float2_offset += size;
    }
    else if (mattr->type == TypeDesc::TypeMatrix) {
      Transform *tfm = mattr->data_transform();
      offset = attr_float3_offset;

      assert(attr_float3.size() >= offset + size * 3);
      if (mattr->modified) {
        for (size_t k = 0; k < size * 3; k++) {
          attr_float3[offset + k] = (&tfm->x)[k];
        }
      }
      attr_float3_offset += size * 3;
    }
    else {
      float4 *data = mattr->data_float4();
      offset = attr_float3_offset;

      assert(attr_float3.size() >= offset + size);
      if (mattr->modified) {
        for (size_t k = 0; k < size; k++) {
          attr_float3[offset + k] = data[k];
        }
      }
      attr_float3_offset += size;
    }

    /* mesh vertex/curve index is global, not per object, so we sneak
     * a correction for that in here */
    if (geom->is_mesh()) {
      Mesh *mesh = static_cast<Mesh *>(geom);
      if (mesh->subdivision_type == Mesh::SUBDIVISION_CATMULL_CLARK &&
          desc.flags & ATTR_SUBDIVIDED) {
        /* indices for subdivided attributes are retrieved
         * from patch table so no need for correction here*/
      }
      else if (element == ATTR_ELEMENT_VERTEX)
        offset -= mesh->vert_offset;
      else if (element == ATTR_ELEMENT_VERTEX_MOTION)
        offset -= mesh->vert_offset;
      else if (element == ATTR_ELEMENT_FACE) {
        if (prim == ATTR_PRIM_GEOMETRY)
          offset -= mesh->prim_offset;
        else
          offset -= mesh->face_offset;
      }
      else if (element == ATTR_ELEMENT_CORNER || element == ATTR_ELEMENT_CORNER_BYTE) {
        if (prim == ATTR_PRIM_GEOMETRY)
          offset -= 3 * mesh->prim_offset;
        else
          offset -= mesh->corner_offset;
      }
    }
    else if (geom->is_hair()) {
      Hair *hair = static_cast<Hair *>(geom);
      if (element == ATTR_ELEMENT_CURVE)
        offset -= hair->prim_offset;
      else if (element == ATTR_ELEMENT_CURVE_KEY)
        offset -= hair->curvekey_offset;
      else if (element == ATTR_ELEMENT_CURVE_KEY_MOTION)
        offset -= hair->curvekey_offset;
    }
  }
  else {
    /* attribute not found */
    desc.element = ATTR_ELEMENT_NONE;
    desc.offset = 0;
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
                                    &attr_uchar4_size);
    }
  }

  dscene->attributes_float.alloc(attr_float_size);
  dscene->attributes_float2.alloc(attr_float2_size);
  dscene->attributes_float3.alloc(attr_float3_size);
  dscene->attributes_uchar4.alloc(attr_uchar4_size);

  const bool copy_all_data = dscene->attributes_float.need_realloc() ||
                             dscene->attributes_float2.need_realloc() ||
                             dscene->attributes_float3.need_realloc() ||
                             dscene->attributes_uchar4.need_realloc();

  size_t attr_float_offset = 0;
  size_t attr_float2_offset = 0;
  size_t attr_float3_offset = 0;
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
        attr->modified |= copy_all_data;
      }

      update_attribute_element_offset(geom,
                                      dscene->attributes_float,
                                      attr_float_offset,
                                      dscene->attributes_float2,
                                      attr_float2_offset,
                                      dscene->attributes_float3,
                                      attr_float3_offset,
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
          subd_attr->modified |= copy_all_data;
        }

        update_attribute_element_offset(mesh,
                                        dscene->attributes_float,
                                        attr_float_offset,
                                        dscene->attributes_float2,
                                        attr_float2_offset,
                                        dscene->attributes_float3,
                                        attr_float3_offset,
                                        dscene->attributes_uchar4,
                                        attr_uchar4_offset,
                                        subd_attr,
                                        ATTR_PRIM_SUBD,
                                        req.subd_type,
                                        req.subd_desc);
      }

      if (progress.get_cancel())
        return;
    }
  }

  for (size_t i = 0; i < scene->objects.size(); i++) {
    Object *object = scene->objects[i];
    AttributeRequestSet &attributes = object_attributes[i];
    AttributeSet &values = object_attribute_values[i];

    foreach (AttributeRequest &req, attributes.requests) {
      Attribute *attr = values.find(req);

      update_attribute_element_offset(object->geometry,
                                      dscene->attributes_float,
                                      attr_float_offset,
                                      dscene->attributes_float2,
                                      attr_float2_offset,
                                      dscene->attributes_float3,
                                      attr_float3_offset,
                                      dscene->attributes_uchar4,
                                      attr_uchar4_offset,
                                      attr,
                                      ATTR_PRIM_GEOMETRY,
                                      req.type,
                                      req.desc);

      /* object attributes don't care about subdivision */
      req.subd_type = req.type;
      req.subd_desc = req.desc;

      if (progress.get_cancel())
        return;
    }
  }

  /* create attribute lookup maps */
  if (scene->shader_manager->use_osl())
    update_osl_attributes(device, scene, geom_attributes);

  update_svm_attributes(device, dscene, scene, geom_attributes, object_attributes);

  if (progress.get_cancel())
    return;

  /* copy to device */
  progress.set_status("Updating Mesh", "Copying Attributes to device");

  dscene->attributes_float.copy_to_device();
  dscene->attributes_float2.copy_to_device();
  dscene->attributes_float3.copy_to_device();
  dscene->attributes_uchar4.copy_to_device();

  if (progress.get_cancel())
    return;

  /* After mesh attributes and patch tables have been copied to device memory,
   * we need to update offsets in the objects. */
  scene->object_manager->device_update_mesh_offsets(device, dscene, scene);
}

void GeometryManager::mesh_calc_offset(Scene *scene, BVHLayout bvh_layout)
{
  size_t vert_size = 0;
  size_t tri_size = 0;

  size_t curve_key_size = 0;
  size_t curve_size = 0;

  size_t patch_size = 0;
  size_t face_size = 0;
  size_t corner_size = 0;

  size_t optix_prim_size = 0;

  foreach (Geometry *geom, scene->geometry) {
    if (geom->optix_prim_offset != optix_prim_size) {
      /* Need to rebuild BVH in OptiX, since refit only allows modified mesh data there */
      const bool has_optix_bvh = bvh_layout == BVH_LAYOUT_OPTIX ||
                                 bvh_layout == BVH_LAYOUT_MULTI_OPTIX ||
                                 bvh_layout == BVH_LAYOUT_MULTI_OPTIX_EMBREE;
      geom->need_update_rebuild |= has_optix_bvh;
      geom->need_update_bvh_for_offset = true;
    }

    if (geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::VOLUME) {
      Mesh *mesh = static_cast<Mesh *>(geom);

      mesh->vert_offset = vert_size;
      mesh->prim_offset = tri_size;

      mesh->patch_offset = patch_size;
      mesh->face_offset = face_size;
      mesh->corner_offset = corner_size;

      vert_size += mesh->verts.size();
      tri_size += mesh->num_triangles();

      if (mesh->get_num_subd_faces()) {
        Mesh::SubdFace last = mesh->get_subd_face(mesh->get_num_subd_faces() - 1);
        patch_size += (last.ptex_offset + last.num_ptex_faces()) * 8;

        /* patch tables are stored in same array so include them in patch_size */
        if (mesh->patch_table) {
          mesh->patch_table_offset = patch_size;
          patch_size += mesh->patch_table->total_size();
        }
      }

      face_size += mesh->get_num_subd_faces();
      corner_size += mesh->subd_face_corners.size();

      mesh->optix_prim_offset = optix_prim_size;
      optix_prim_size += mesh->num_triangles();
    }
    else if (geom->is_hair()) {
      Hair *hair = static_cast<Hair *>(geom);

      hair->curvekey_offset = curve_key_size;
      hair->prim_offset = curve_size;

      curve_key_size += hair->get_curve_keys().size();
      curve_size += hair->num_curves();

      hair->optix_prim_offset = optix_prim_size;
      optix_prim_size += hair->num_segments();
    }
  }
}

void GeometryManager::device_update_mesh(
    Device *, DeviceScene *dscene, Scene *scene, bool for_displacement, Progress &progress)
{
  /* Count. */
  size_t vert_size = 0;
  size_t tri_size = 0;

  size_t curve_key_size = 0;
  size_t curve_size = 0;

  size_t patch_size = 0;

  foreach (Geometry *geom, scene->geometry) {
    if (geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::VOLUME) {
      Mesh *mesh = static_cast<Mesh *>(geom);

      vert_size += mesh->verts.size();
      tri_size += mesh->num_triangles();

      if (mesh->get_num_subd_faces()) {
        Mesh::SubdFace last = mesh->get_subd_face(mesh->get_num_subd_faces() - 1);
        patch_size += (last.ptex_offset + last.num_ptex_faces()) * 8;

        /* patch tables are stored in same array so include them in patch_size */
        if (mesh->patch_table) {
          mesh->patch_table_offset = patch_size;
          patch_size += mesh->patch_table->total_size();
        }
      }
    }
    else if (geom->is_hair()) {
      Hair *hair = static_cast<Hair *>(geom);

      curve_key_size += hair->get_curve_keys().size();
      curve_size += hair->num_curves();
    }
  }

  /* Create mapping from triangle to primitive triangle array. */
  vector<uint> tri_prim_index(tri_size);
  if (for_displacement) {
    /* For displacement kernels we do some trickery to make them believe
     * we've got all required data ready. However, that data is different
     * from final render kernels since we don't have BVH yet, so can't
     * really use same semantic of arrays.
     */
    foreach (Geometry *geom, scene->geometry) {
      if (geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::VOLUME) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        for (size_t i = 0; i < mesh->num_triangles(); ++i) {
          tri_prim_index[i + mesh->prim_offset] = 3 * (i + mesh->prim_offset);
        }
      }
    }
  }
  else {
    for (size_t i = 0; i < dscene->prim_index.size(); ++i) {
      if ((dscene->prim_type[i] & PRIMITIVE_ALL_TRIANGLE) != 0) {
        tri_prim_index[dscene->prim_index[i]] = dscene->prim_tri_index[i];
      }
    }
  }

  /* Fill in all the arrays. */
  if (tri_size != 0) {
    /* normals */
    progress.set_status("Updating Mesh", "Computing normals");

    uint *tri_shader = dscene->tri_shader.alloc(tri_size);
    float4 *vnormal = dscene->tri_vnormal.alloc(vert_size);
    uint4 *tri_vindex = dscene->tri_vindex.alloc(tri_size);
    uint *tri_patch = dscene->tri_patch.alloc(tri_size);
    float2 *tri_patch_uv = dscene->tri_patch_uv.alloc(vert_size);

    const bool copy_all_data = dscene->tri_shader.need_realloc() ||
                               dscene->tri_vindex.need_realloc() ||
                               dscene->tri_vnormal.need_realloc() ||
                               dscene->tri_patch.need_realloc() ||
                               dscene->tri_patch_uv.need_realloc();

    foreach (Geometry *geom, scene->geometry) {
      if (geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::VOLUME) {
        Mesh *mesh = static_cast<Mesh *>(geom);

        if (mesh->shader_is_modified() || mesh->smooth_is_modified() ||
            mesh->triangles_is_modified() || copy_all_data) {
          mesh->pack_shaders(scene, &tri_shader[mesh->prim_offset]);
        }

        if (mesh->verts_is_modified() || copy_all_data) {
          mesh->pack_normals(&vnormal[mesh->vert_offset]);
        }

        if (mesh->triangles_is_modified() || mesh->vert_patch_uv_is_modified() || copy_all_data) {
          mesh->pack_verts(tri_prim_index,
                           &tri_vindex[mesh->prim_offset],
                           &tri_patch[mesh->prim_offset],
                           &tri_patch_uv[mesh->vert_offset],
                           mesh->vert_offset,
                           mesh->prim_offset);
        }

        if (progress.get_cancel())
          return;
      }
    }

    /* vertex coordinates */
    progress.set_status("Updating Mesh", "Copying Mesh to device");

    dscene->tri_shader.copy_to_device_if_modified();
    dscene->tri_vnormal.copy_to_device_if_modified();
    dscene->tri_vindex.copy_to_device_if_modified();
    dscene->tri_patch.copy_to_device_if_modified();
    dscene->tri_patch_uv.copy_to_device_if_modified();
  }

  if (curve_size != 0) {
    progress.set_status("Updating Mesh", "Copying Strands to device");

    float4 *curve_keys = dscene->curve_keys.alloc(curve_key_size);
    float4 *curves = dscene->curves.alloc(curve_size);

    const bool copy_all_data = dscene->curve_keys.need_realloc() || dscene->curves.need_realloc();

    foreach (Geometry *geom, scene->geometry) {
      if (geom->is_hair()) {
        Hair *hair = static_cast<Hair *>(geom);

        bool curve_keys_co_modified = hair->curve_radius_is_modified() ||
                                      hair->curve_keys_is_modified();
        bool curve_data_modified = hair->curve_shader_is_modified() ||
                                   hair->curve_first_key_is_modified();

        if (!curve_keys_co_modified && !curve_data_modified && !copy_all_data) {
          continue;
        }

        hair->pack_curves(scene,
                          &curve_keys[hair->curvekey_offset],
                          &curves[hair->prim_offset],
                          hair->curvekey_offset);
        if (progress.get_cancel())
          return;
      }
    }

    dscene->curve_keys.copy_to_device_if_modified();
    dscene->curves.copy_to_device_if_modified();
  }

  if (patch_size != 0 && dscene->patches.need_realloc()) {
    progress.set_status("Updating Mesh", "Copying Patches to device");

    uint *patch_data = dscene->patches.alloc(patch_size);

    foreach (Geometry *geom, scene->geometry) {
      if (geom->is_mesh()) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        mesh->pack_patches(&patch_data[mesh->patch_offset],
                           mesh->vert_offset,
                           mesh->face_offset,
                           mesh->corner_offset);

        if (mesh->patch_table) {
          mesh->patch_table->copy_adjusting_offsets(&patch_data[mesh->patch_table_offset],
                                                    mesh->patch_table_offset);
        }

        if (progress.get_cancel())
          return;
      }
    }

    dscene->patches.copy_to_device();
  }

  if (for_displacement) {
    float4 *prim_tri_verts = dscene->prim_tri_verts.alloc(tri_size * 3);
    foreach (Geometry *geom, scene->geometry) {
      if (geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::VOLUME) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        for (size_t i = 0; i < mesh->num_triangles(); ++i) {
          Mesh::Triangle t = mesh->get_triangle(i);
          size_t offset = 3 * (i + mesh->prim_offset);
          prim_tri_verts[offset + 0] = float3_to_float4(mesh->verts[t.v[0]]);
          prim_tri_verts[offset + 1] = float3_to_float4(mesh->verts[t.v[1]]);
          prim_tri_verts[offset + 2] = float3_to_float4(mesh->verts[t.v[2]]);
        }
      }
    }
    dscene->prim_tri_verts.copy_to_device();
  }
}

void GeometryManager::device_update_bvh(Device *device,
                                        DeviceScene *dscene,
                                        Scene *scene,
                                        Progress &progress)
{
  /* bvh build */
  progress.set_status("Updating Scene BVH", "Building");

  BVHParams bparams;
  bparams.top_level = true;
  bparams.bvh_layout = BVHParams::best_bvh_layout(scene->params.bvh_layout,
                                                  device->get_bvh_layout_mask());
  bparams.use_spatial_split = scene->params.use_bvh_spatial_split;
  bparams.use_unaligned_nodes = dscene->data.bvh.have_curves &&
                                scene->params.use_bvh_unaligned_nodes;
  bparams.num_motion_triangle_steps = scene->params.num_bvh_time_steps;
  bparams.num_motion_curve_steps = scene->params.num_bvh_time_steps;
  bparams.bvh_type = scene->params.bvh_type;
  bparams.curve_subdivisions = scene->params.curve_subdivisions();

  VLOG(1) << "Using " << bvh_layout_name(bparams.bvh_layout) << " layout.";

  const bool can_refit = scene->bvh != nullptr &&
                         (bparams.bvh_layout == BVHLayout::BVH_LAYOUT_OPTIX);
  const bool pack_all = scene->bvh == nullptr;

  BVH *bvh = scene->bvh;
  if (!scene->bvh) {
    bvh = scene->bvh = BVH::create(bparams, scene->geometry, scene->objects, device);
  }

  device->build_bvh(bvh, progress, can_refit);

  if (progress.get_cancel()) {
    return;
  }

  const bool has_bvh2_layout = (bparams.bvh_layout == BVH_LAYOUT_BVH2);

  PackedBVH pack;
  if (has_bvh2_layout) {
    pack = std::move(static_cast<BVH2 *>(bvh)->pack);
  }
  else {
    progress.set_status("Updating Scene BVH", "Packing BVH primitives");

    size_t num_prims = 0;
    size_t num_tri_verts = 0;
    foreach (Geometry *geom, scene->geometry) {
      if (geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::VOLUME) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        num_prims += mesh->num_triangles();
        num_tri_verts += 3 * mesh->num_triangles();
      }
      else if (geom->is_hair()) {
        Hair *hair = static_cast<Hair *>(geom);
        num_prims += hair->num_segments();
      }
    }

    pack.root_index = -1;

    if (!pack_all) {
      /* if we do not need to recreate the BVH, then only the vertices are updated, so we can
       * safely retake the memory */
      dscene->prim_tri_verts.give_data(pack.prim_tri_verts);
    }
    else {
      /* It is not strictly necessary to skip those resizes we if do not have to repack, as the OS
       * will not allocate pages if we do not touch them, however it does help catching bugs. */
      pack.prim_tri_index.resize(num_prims);
      pack.prim_tri_verts.resize(num_tri_verts);
      pack.prim_type.resize(num_prims);
      pack.prim_index.resize(num_prims);
      pack.prim_object.resize(num_prims);
      pack.prim_visibility.resize(num_prims);
    }

    // Merge visibility flags of all objects and find object index for non-instanced geometry
    unordered_map<const Geometry *, pair<int, uint>> geometry_to_object_info;
    geometry_to_object_info.reserve(scene->geometry.size());
    foreach (Object *ob, scene->objects) {
      const Geometry *const geom = ob->get_geometry();
      pair<int, uint> &info = geometry_to_object_info[geom];
      info.second |= ob->visibility_for_tracing();
      if (!geom->is_instanced()) {
        info.first = ob->get_device_index();
      }
    }

    TaskPool pool;
    // Iterate over scene mesh list instead of objects, since 'optix_prim_offset' was calculated
    // based on that list, which may be ordered differently from the object list.
    foreach (Geometry *geom, scene->geometry) {
      if (!pack_all && !geom->is_modified()) {
        continue;
      }

      const pair<int, uint> &info = geometry_to_object_info[geom];
      pool.push(function_bind(
          &Geometry::pack_primitives, geom, &pack, info.first, info.second, pack_all));
    }
    pool.wait_work();
  }

  /* copy to device */
  progress.set_status("Updating Scene BVH", "Copying BVH to device");

  /* When using BVH2, we always have to copy/update the data as its layout is dependent on the
   * BVH's leaf nodes which may be different when the objects or vertices move. */

  if (pack.nodes.size()) {
    dscene->bvh_nodes.steal_data(pack.nodes);
    dscene->bvh_nodes.copy_to_device();
  }
  if (pack.leaf_nodes.size()) {
    dscene->bvh_leaf_nodes.steal_data(pack.leaf_nodes);
    dscene->bvh_leaf_nodes.copy_to_device();
  }
  if (pack.object_node.size()) {
    dscene->object_node.steal_data(pack.object_node);
    dscene->object_node.copy_to_device();
  }
  if (pack.prim_tri_index.size() && (dscene->prim_tri_index.need_realloc() || has_bvh2_layout)) {
    dscene->prim_tri_index.steal_data(pack.prim_tri_index);
    dscene->prim_tri_index.copy_to_device();
  }
  if (pack.prim_tri_verts.size()) {
    dscene->prim_tri_verts.steal_data(pack.prim_tri_verts);
    dscene->prim_tri_verts.copy_to_device();
  }
  if (pack.prim_type.size() && (dscene->prim_type.need_realloc() || has_bvh2_layout)) {
    dscene->prim_type.steal_data(pack.prim_type);
    dscene->prim_type.copy_to_device();
  }
  if (pack.prim_visibility.size() && (dscene->prim_visibility.need_realloc() || has_bvh2_layout)) {
    dscene->prim_visibility.steal_data(pack.prim_visibility);
    dscene->prim_visibility.copy_to_device();
  }
  if (pack.prim_index.size() && (dscene->prim_index.need_realloc() || has_bvh2_layout)) {
    dscene->prim_index.steal_data(pack.prim_index);
    dscene->prim_index.copy_to_device();
  }
  if (pack.prim_object.size() && (dscene->prim_object.need_realloc() || has_bvh2_layout)) {
    dscene->prim_object.steal_data(pack.prim_object);
    dscene->prim_object.copy_to_device();
  }
  if (pack.prim_time.size() && (dscene->prim_time.need_realloc() || has_bvh2_layout)) {
    dscene->prim_time.steal_data(pack.prim_time);
    dscene->prim_time.copy_to_device();
  }

  dscene->data.bvh.root = pack.root_index;
  dscene->data.bvh.use_bvh_steps = (scene->params.num_bvh_time_steps != 0);
  dscene->data.bvh.curve_subdivisions = scene->params.curve_subdivisions();
  /* The scene handle is set in 'CPUDevice::const_copy_to' and 'OptiXDevice::const_copy_to' */
  dscene->data.bvh.scene = 0;
}

/* Set of flags used to help determining what data has been modified or needs reallocation, so we
 * can decide which device data to free or update. */
enum {
  DEVICE_CURVE_DATA_MODIFIED = (1 << 0),
  DEVICE_MESH_DATA_MODIFIED = (1 << 1),

  ATTR_FLOAT_MODIFIED = (1 << 2),
  ATTR_FLOAT2_MODIFIED = (1 << 3),
  ATTR_FLOAT3_MODIFIED = (1 << 4),
  ATTR_UCHAR4_MODIFIED = (1 << 5),

  CURVE_DATA_NEED_REALLOC = (1 << 6),
  MESH_DATA_NEED_REALLOC = (1 << 7),

  ATTR_FLOAT_NEEDS_REALLOC = (1 << 8),
  ATTR_FLOAT2_NEEDS_REALLOC = (1 << 9),
  ATTR_FLOAT3_NEEDS_REALLOC = (1 << 10),
  ATTR_UCHAR4_NEEDS_REALLOC = (1 << 11),

  ATTRS_NEED_REALLOC = (ATTR_FLOAT_NEEDS_REALLOC | ATTR_FLOAT2_NEEDS_REALLOC |
                        ATTR_FLOAT3_NEEDS_REALLOC | ATTR_UCHAR4_NEEDS_REALLOC),
  DEVICE_MESH_DATA_NEEDS_REALLOC = (MESH_DATA_NEED_REALLOC | ATTRS_NEED_REALLOC),
  DEVICE_CURVE_DATA_NEEDS_REALLOC = (CURVE_DATA_NEED_REALLOC | ATTRS_NEED_REALLOC),
};

static void update_device_flags_attribute(uint32_t &device_update_flags,
                                          const AttributeSet &attributes)
{
  foreach (const Attribute &attr, attributes.attributes) {
    if (!attr.modified) {
      continue;
    }

    if (attr.element == ATTR_ELEMENT_CORNER) {
      device_update_flags |= ATTR_UCHAR4_MODIFIED;
    }
    else if (attr.type == TypeDesc::TypeFloat) {
      device_update_flags |= ATTR_FLOAT_MODIFIED;
    }
    else if (attr.type == TypeFloat2) {
      device_update_flags |= ATTR_FLOAT2_MODIFIED;
    }
    else if (attr.type == TypeDesc::TypeMatrix) {
      device_update_flags |= ATTR_FLOAT3_MODIFIED;
    }
    else if (attr.element != ATTR_ELEMENT_VOXEL) {
      device_update_flags |= ATTR_FLOAT3_MODIFIED;
    }
  }
}

void GeometryManager::device_update_preprocess(Device *device, Scene *scene, Progress &progress)
{
  if (!need_update() && !need_flags_update) {
    return;
  }

  uint32_t device_update_flags = 0;

  scoped_callback_timer timer([scene](double time) {
    if (scene->update_stats) {
      scene->update_stats->geometry.times.add_entry({"device_update_preprocess", time});
    }
  });

  progress.set_status("Updating Meshes Flags");

  /* Update flags. */
  bool volume_images_updated = false;

  foreach (Geometry *geom, scene->geometry) {
    geom->has_volume = false;

    if (geom->attributes.modified) {
      device_update_flags |= ATTRS_NEED_REALLOC;
    }

    if (geom->is_mesh()) {
      Mesh *mesh = static_cast<Mesh *>(geom);

      if (mesh->subd_attributes.modified) {
        device_update_flags |= ATTRS_NEED_REALLOC;
      }
    }

    foreach (Node *node, geom->get_used_shaders()) {
      Shader *shader = static_cast<Shader *>(node);
      if (shader->has_volume) {
        geom->has_volume = true;
      }

      if (shader->has_surface_bssrdf) {
        geom->has_surface_bssrdf = true;
      }

      if (shader->need_update_uvs) {
        device_update_flags |= ATTR_FLOAT2_NEEDS_REALLOC;

        /* Attributes might need to be tessellated if added. */
        if (geom->is_mesh()) {
          Mesh *mesh = static_cast<Mesh *>(geom);
          if (mesh->need_tesselation()) {
            mesh->tag_modified();
          }
        }
      }

      if (shader->need_update_attribute) {
        device_update_flags |= ATTRS_NEED_REALLOC;

        /* Attributes might need to be tessellated if added. */
        if (geom->is_mesh()) {
          Mesh *mesh = static_cast<Mesh *>(geom);
          if (mesh->need_tesselation()) {
            mesh->tag_modified();
          }
        }
      }

      if (shader->need_update_displacement) {
        /* tag displacement related sockets as modified */
        if (geom->is_mesh()) {
          Mesh *mesh = static_cast<Mesh *>(geom);
          mesh->tag_verts_modified();
          mesh->tag_subd_dicing_rate_modified();
          mesh->tag_subd_max_level_modified();
          mesh->tag_subd_objecttoworld_modified();

          device_update_flags |= ATTRS_NEED_REALLOC;
        }
      }
    }

    /* only check for modified attributes if we do not need to reallocate them already */
    if ((device_update_flags & ATTRS_NEED_REALLOC) == 0) {
      update_device_flags_attribute(device_update_flags, geom->attributes);
      /* don't check for subd_attributes, as if they were modified, we would need to reallocate
       * anyway */
    }

    /* Re-create volume mesh if we will rebuild or refit the BVH. Note we
     * should only do it in that case, otherwise the BVH and mesh can go
     * out of sync. */
    if (geom->is_modified() && geom->geometry_type == Geometry::VOLUME) {
      /* Create volume meshes if there is voxel data. */
      if (!volume_images_updated) {
        progress.set_status("Updating Meshes Volume Bounds");
        device_update_volume_images(device, scene, progress);
        volume_images_updated = true;
      }

      Volume *volume = static_cast<Volume *>(geom);
      create_volume_mesh(volume, progress);

      /* always reallocate when we have a volume, as we need to rebuild the BVH */
      device_update_flags |= DEVICE_MESH_DATA_NEEDS_REALLOC;
    }

    if (geom->is_hair()) {
      /* Set curve shape, still a global scene setting for now. */
      Hair *hair = static_cast<Hair *>(geom);
      hair->curve_shape = scene->params.hair_shape;

      if (hair->need_update_rebuild) {
        device_update_flags |= DEVICE_CURVE_DATA_NEEDS_REALLOC;
      }
      else if (hair->is_modified()) {
        device_update_flags |= DEVICE_CURVE_DATA_MODIFIED;
      }
    }

    if (geom->is_mesh()) {
      Mesh *mesh = static_cast<Mesh *>(geom);

      if (mesh->need_update_rebuild) {
        device_update_flags |= DEVICE_MESH_DATA_NEEDS_REALLOC;
      }
      else if (mesh->is_modified()) {
        device_update_flags |= DEVICE_MESH_DATA_MODIFIED;
      }
    }
  }

  if (update_flags & (MESH_ADDED | MESH_REMOVED)) {
    device_update_flags |= DEVICE_MESH_DATA_NEEDS_REALLOC;
  }

  if (update_flags & (HAIR_ADDED | HAIR_REMOVED)) {
    device_update_flags |= DEVICE_CURVE_DATA_NEEDS_REALLOC;
  }

  /* tag the device arrays for reallocation or modification */
  DeviceScene *dscene = &scene->dscene;

  if (device_update_flags & (DEVICE_MESH_DATA_NEEDS_REALLOC | DEVICE_CURVE_DATA_NEEDS_REALLOC)) {
    delete scene->bvh;
    scene->bvh = nullptr;

    dscene->bvh_nodes.tag_realloc();
    dscene->bvh_leaf_nodes.tag_realloc();
    dscene->object_node.tag_realloc();
    dscene->prim_tri_verts.tag_realloc();
    dscene->prim_tri_index.tag_realloc();
    dscene->prim_type.tag_realloc();
    dscene->prim_visibility.tag_realloc();
    dscene->prim_index.tag_realloc();
    dscene->prim_object.tag_realloc();
    dscene->prim_time.tag_realloc();

    if (device_update_flags & DEVICE_MESH_DATA_NEEDS_REALLOC) {
      dscene->tri_vnormal.tag_realloc();
      dscene->tri_vindex.tag_realloc();
      dscene->tri_patch.tag_realloc();
      dscene->tri_patch_uv.tag_realloc();
      dscene->tri_shader.tag_realloc();
      dscene->patches.tag_realloc();
    }

    if (device_update_flags & DEVICE_CURVE_DATA_NEEDS_REALLOC) {
      dscene->curves.tag_realloc();
      dscene->curve_keys.tag_realloc();
    }
  }

  if (device_update_flags & ATTR_FLOAT_NEEDS_REALLOC) {
    dscene->attributes_map.tag_realloc();
    dscene->attributes_float.tag_realloc();
  }
  else if (device_update_flags & ATTR_FLOAT_MODIFIED) {
    dscene->attributes_float.tag_modified();
  }

  if (device_update_flags & ATTR_FLOAT2_NEEDS_REALLOC) {
    dscene->attributes_map.tag_realloc();
    dscene->attributes_float2.tag_realloc();
  }
  else if (device_update_flags & ATTR_FLOAT2_MODIFIED) {
    dscene->attributes_float2.tag_modified();
  }

  if (device_update_flags & ATTR_FLOAT3_NEEDS_REALLOC) {
    dscene->attributes_map.tag_realloc();
    dscene->attributes_float3.tag_realloc();
  }
  else if (device_update_flags & ATTR_FLOAT3_MODIFIED) {
    dscene->attributes_float3.tag_modified();
  }

  if (device_update_flags & ATTR_UCHAR4_NEEDS_REALLOC) {
    dscene->attributes_map.tag_realloc();
    dscene->attributes_uchar4.tag_realloc();
  }
  else if (device_update_flags & ATTR_UCHAR4_MODIFIED) {
    dscene->attributes_uchar4.tag_modified();
  }

  if (device_update_flags & DEVICE_MESH_DATA_MODIFIED) {
    /* if anything else than vertices or shaders are modified, we would need to reallocate, so
     * these are the only arrays that can be updated */
    dscene->tri_vnormal.tag_modified();
    dscene->tri_shader.tag_modified();
  }

  if (device_update_flags & DEVICE_CURVE_DATA_MODIFIED) {
    dscene->curve_keys.tag_modified();
    dscene->curves.tag_modified();
  }

  need_flags_update = false;
}

void GeometryManager::device_update_displacement_images(Device *device,
                                                        Scene *scene,
                                                        Progress &progress)
{
  progress.set_status("Updating Displacement Images");
  TaskPool pool;
  ImageManager *image_manager = scene->image_manager;
  set<int> bump_images;
  foreach (Geometry *geom, scene->geometry) {
    if (geom->is_modified()) {
      foreach (Node *node, geom->get_used_shaders()) {
        Shader *shader = static_cast<Shader *>(node);
        if (!shader->has_displacement || shader->get_displacement_method() == DISPLACE_BUMP) {
          continue;
        }
        foreach (ShaderNode *node, shader->graph->nodes) {
          if (node->special_type != SHADER_SPECIAL_TYPE_IMAGE_SLOT) {
            continue;
          }

          ImageSlotTextureNode *image_node = static_cast<ImageSlotTextureNode *>(node);
          for (int i = 0; i < image_node->handle.num_tiles(); i++) {
            const int slot = image_node->handle.svm_slot(i);
            if (slot != -1) {
              bump_images.insert(slot);
            }
          }
        }
      }
    }
  }
  foreach (int slot, bump_images) {
    pool.push(function_bind(
        &ImageManager::device_update_slot, image_manager, device, scene, slot, &progress));
  }
  pool.wait_work();
}

void GeometryManager::device_update_volume_images(Device *device, Scene *scene, Progress &progress)
{
  progress.set_status("Updating Volume Images");
  TaskPool pool;
  ImageManager *image_manager = scene->image_manager;
  set<int> volume_images;

  foreach (Geometry *geom, scene->geometry) {
    if (!geom->is_modified()) {
      continue;
    }

    foreach (Attribute &attr, geom->attributes.attributes) {
      if (attr.element != ATTR_ELEMENT_VOXEL) {
        continue;
      }

      ImageHandle &handle = attr.data_voxel();
      /* We can build directly from OpenVDB data structures, no need to
       * load such images early. */
      if (!handle.vdb_loader()) {
        const int slot = handle.svm_slot();
        if (slot != -1) {
          volume_images.insert(slot);
        }
      }
    }
  }

  foreach (int slot, volume_images) {
    pool.push(function_bind(
        &ImageManager::device_update_slot, image_manager, device, scene, slot, &progress));
  }
  pool.wait_work();
}

void GeometryManager::device_update(Device *device,
                                    DeviceScene *dscene,
                                    Scene *scene,
                                    Progress &progress)
{
  if (!need_update())
    return;

  VLOG(1) << "Total " << scene->geometry.size() << " meshes.";

  bool true_displacement_used = false;
  size_t total_tess_needed = 0;

  {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->geometry.times.add_entry({"device_update (normals)", time});
      }
    });

    foreach (Geometry *geom, scene->geometry) {
      if (geom->is_modified() &&
          (geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::VOLUME)) {
        Mesh *mesh = static_cast<Mesh *>(geom);

        /* Update normals. */
        mesh->add_face_normals();
        mesh->add_vertex_normals();

        if (mesh->need_attribute(scene, ATTR_STD_POSITION_UNDISPLACED)) {
          mesh->add_undisplaced();
        }

        /* Test if we need tessellation. */
        if (mesh->need_tesselation()) {
          total_tess_needed++;
        }

        /* Test if we need displacement. */
        if (mesh->has_true_displacement()) {
          true_displacement_used = true;
        }

        if (progress.get_cancel()) {
          return;
        }
      }
    }
  }

  if (progress.get_cancel()) {
    return;
  }

  /* Tessellate meshes that are using subdivision */
  if (total_tess_needed) {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->geometry.times.add_entry(
            {"device_update (adaptive subdivision)", time});
      }
    });

    Camera *dicing_camera = scene->dicing_camera;
    dicing_camera->set_screen_size_and_resolution(
        dicing_camera->get_full_width(), dicing_camera->get_full_height(), 1);
    dicing_camera->update(scene);

    size_t i = 0;
    foreach (Geometry *geom, scene->geometry) {
      if (!(geom->is_modified() && geom->is_mesh())) {
        continue;
      }

      Mesh *mesh = static_cast<Mesh *>(geom);
      if (mesh->need_tesselation()) {
        string msg = "Tessellating ";
        if (mesh->name == "")
          msg += string_printf("%u/%u", (uint)(i + 1), (uint)total_tess_needed);
        else
          msg += string_printf(
              "%s %u/%u", mesh->name.c_str(), (uint)(i + 1), (uint)total_tess_needed);

        progress.set_status("Updating Mesh", msg);

        mesh->subd_params->camera = dicing_camera;
        DiagSplit dsplit(*mesh->subd_params);
        mesh->tessellate(&dsplit);

        i++;

        if (progress.get_cancel()) {
          return;
        }
      }
    }

    if (progress.get_cancel()) {
      return;
    }
  }

  /* Update images needed for true displacement. */
  bool old_need_object_flags_update = false;
  if (true_displacement_used) {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->geometry.times.add_entry(
            {"device_update (displacement: load images)", time});
      }
    });
    device_update_displacement_images(device, scene, progress);
    old_need_object_flags_update = scene->object_manager->need_flags_update;
    scene->object_manager->device_update_flags(device, dscene, scene, progress, false);
  }

  /* Device update. */
  device_free(device, dscene, false);

  const BVHLayout bvh_layout = BVHParams::best_bvh_layout(scene->params.bvh_layout,
                                                          device->get_bvh_layout_mask());
  mesh_calc_offset(scene, bvh_layout);
  if (true_displacement_used) {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->geometry.times.add_entry(
            {"device_update (displacement: copy meshes to device)", time});
      }
    });
    device_update_mesh(device, dscene, scene, true, progress);
  }
  if (progress.get_cancel()) {
    return;
  }

  {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->geometry.times.add_entry({"device_update (attributes)", time});
      }
    });
    device_update_attributes(device, dscene, scene, progress);
    if (progress.get_cancel()) {
      return;
    }
  }

  /* Update displacement. */
  bool displacement_done = false;
  size_t num_bvh = 0;

  {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->geometry.times.add_entry({"device_update (displacement)", time});
      }
    });

    foreach (Geometry *geom, scene->geometry) {
      if (geom->is_modified()) {
        if (geom->is_mesh()) {
          Mesh *mesh = static_cast<Mesh *>(geom);
          if (displace(device, dscene, scene, mesh, progress)) {
            displacement_done = true;
          }
        }
      }

      if (geom->is_modified() || geom->need_update_bvh_for_offset) {
        if (geom->need_build_bvh(bvh_layout)) {
          num_bvh++;
        }
      }

      if (progress.get_cancel()) {
        return;
      }
    }
  }

  if (progress.get_cancel()) {
    return;
  }

  /* Device re-update after displacement. */
  if (displacement_done) {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->geometry.times.add_entry(
            {"device_update (displacement: attributes)", time});
      }
    });
    device_free(device, dscene, false);

    device_update_attributes(device, dscene, scene, progress);
    if (progress.get_cancel()) {
      return;
    }
  }

  /* Update the BVH even when there is no geometry so the kernel's BVH data is still valid,
   * especially when removing all of the objects during interactive renders.
   * Also update the BVH if the transformations change, we cannot rely on tagging the Geometry
   * as modified in this case, as we may accumulate displacement if the vertices do not also
   * change. */
  bool need_update_scene_bvh = (scene->bvh == nullptr || (update_flags & TRANSFORM_MODIFIED) != 0);
  {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->geometry.times.add_entry({"device_update (build object BVHs)", time});
      }
    });
    TaskPool pool;

    size_t i = 0;
    foreach (Geometry *geom, scene->geometry) {
      if (geom->is_modified() || geom->need_update_bvh_for_offset) {
        need_update_scene_bvh = true;
        pool.push(function_bind(
            &Geometry::compute_bvh, geom, device, dscene, &scene->params, &progress, i, num_bvh));
        if (geom->need_build_bvh(bvh_layout)) {
          i++;
        }
      }
    }

    TaskPool::Summary summary;
    pool.wait_work(&summary);
    VLOG(2) << "Objects BVH build pool statistics:\n" << summary.full_report();
  }

  foreach (Shader *shader, scene->shaders) {
    shader->need_update_uvs = false;
    shader->need_update_attribute = false;
    shader->need_update_displacement = false;
  }

  Scene::MotionType need_motion = scene->need_motion();
  bool motion_blur = need_motion == Scene::MOTION_BLUR;

  /* Update objects. */
  {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->geometry.times.add_entry({"device_update (compute bounds)", time});
      }
    });
    foreach (Object *object, scene->objects) {
      object->compute_bounds(motion_blur);
    }
  }

  if (progress.get_cancel()) {
    return;
  }

  if (need_update_scene_bvh) {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->geometry.times.add_entry({"device_update (build scene BVH)", time});
      }
    });
    device_update_bvh(device, dscene, scene, progress);
    if (progress.get_cancel()) {
      return;
    }
  }

  /* Always set BVH layout again after displacement where it was set to none,
   * to avoid ray-tracing at that stage. */
  dscene->data.bvh.bvh_layout = BVHParams::best_bvh_layout(scene->params.bvh_layout,
                                                           device->get_bvh_layout_mask());

  {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->geometry.times.add_entry(
            {"device_update (copy meshes to device)", time});
      }
    });
    device_update_mesh(device, dscene, scene, false, progress);
    if (progress.get_cancel()) {
      return;
    }
  }

  if (true_displacement_used) {
    /* Re-tag flags for update, so they're re-evaluated
     * for meshes with correct bounding boxes.
     *
     * This wouldn't cause wrong results, just true
     * displacement might be less optimal ot calculate.
     */
    scene->object_manager->need_flags_update = old_need_object_flags_update;
  }

  /* unset flags */

  foreach (Geometry *geom, scene->geometry) {
    geom->clear_modified();
    geom->attributes.clear_modified();

    if (geom->is_mesh()) {
      Mesh *mesh = static_cast<Mesh *>(geom);
      mesh->subd_attributes.clear_modified();
    }
  }

  update_flags = UPDATE_NONE;

  dscene->bvh_nodes.clear_modified();
  dscene->bvh_leaf_nodes.clear_modified();
  dscene->object_node.clear_modified();
  dscene->prim_tri_verts.clear_modified();
  dscene->prim_tri_index.clear_modified();
  dscene->prim_type.clear_modified();
  dscene->prim_visibility.clear_modified();
  dscene->prim_index.clear_modified();
  dscene->prim_object.clear_modified();
  dscene->prim_time.clear_modified();
  dscene->tri_shader.clear_modified();
  dscene->tri_vindex.clear_modified();
  dscene->tri_patch.clear_modified();
  dscene->tri_vnormal.clear_modified();
  dscene->tri_patch_uv.clear_modified();
  dscene->curves.clear_modified();
  dscene->curve_keys.clear_modified();
  dscene->patches.clear_modified();
  dscene->attributes_map.clear_modified();
  dscene->attributes_float.clear_modified();
  dscene->attributes_float2.clear_modified();
  dscene->attributes_float3.clear_modified();
  dscene->attributes_uchar4.clear_modified();
}

void GeometryManager::device_free(Device *device, DeviceScene *dscene, bool force_free)
{
  dscene->bvh_nodes.free_if_need_realloc(force_free);
  dscene->bvh_leaf_nodes.free_if_need_realloc(force_free);
  dscene->object_node.free_if_need_realloc(force_free);
  dscene->prim_tri_verts.free_if_need_realloc(force_free);
  dscene->prim_tri_index.free_if_need_realloc(force_free);
  dscene->prim_type.free_if_need_realloc(force_free);
  dscene->prim_visibility.free_if_need_realloc(force_free);
  dscene->prim_index.free_if_need_realloc(force_free);
  dscene->prim_object.free_if_need_realloc(force_free);
  dscene->prim_time.free_if_need_realloc(force_free);
  dscene->tri_shader.free_if_need_realloc(force_free);
  dscene->tri_vnormal.free_if_need_realloc(force_free);
  dscene->tri_vindex.free_if_need_realloc(force_free);
  dscene->tri_patch.free_if_need_realloc(force_free);
  dscene->tri_patch_uv.free_if_need_realloc(force_free);
  dscene->curves.free_if_need_realloc(force_free);
  dscene->curve_keys.free_if_need_realloc(force_free);
  dscene->patches.free_if_need_realloc(force_free);
  dscene->attributes_map.free_if_need_realloc(force_free);
  dscene->attributes_float.free_if_need_realloc(force_free);
  dscene->attributes_float2.free_if_need_realloc(force_free);
  dscene->attributes_float3.free_if_need_realloc(force_free);
  dscene->attributes_uchar4.free_if_need_realloc(force_free);

  /* Signal for shaders like displacement not to do ray tracing. */
  dscene->data.bvh.bvh_layout = BVH_LAYOUT_NONE;

#ifdef WITH_OSL
  OSLGlobals *og = (OSLGlobals *)device->osl_memory();

  if (og) {
    og->object_name_map.clear();
    og->attribute_map.clear();
    og->object_names.clear();
  }
#else
  (void)device;
#endif
}

void GeometryManager::tag_update(Scene *scene, uint32_t flag)
{
  update_flags |= flag;

  /* do not tag the object manager for an update if it is the one who tagged us */
  if ((flag & OBJECT_MANAGER) == 0) {
    scene->object_manager->tag_update(scene, ObjectManager::GEOMETRY_MANAGER);
  }
}

bool GeometryManager::need_update() const
{
  return update_flags != UPDATE_NONE;
}

void GeometryManager::collect_statistics(const Scene *scene, RenderStats *stats)
{
  foreach (Geometry *geometry, scene->geometry) {
    stats->mesh.geometry.add_entry(
        NamedSizeEntry(string(geometry->name.c_str()), geometry->get_total_size_in_bytes()));
  }
}

CCL_NAMESPACE_END
