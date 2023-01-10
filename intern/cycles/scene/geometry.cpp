/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

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
         layout == BVH_LAYOUT_METAL || layout == BVH_LAYOUT_MULTI_OPTIX_EMBREE ||
         layout == BVH_LAYOUT_MULTI_METAL || layout == BVH_LAYOUT_MULTI_METAL_EMBREE;
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

void Geometry::compute_bvh(Device *device,
                           DeviceScene *dscene,
                           SceneParams *params,
                           Progress *progress,
                           size_t n,
                           size_t total)
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

    /* Ensure all visibility bits are set at the geometry level BVH. In
     * the object level BVH is where actual visibility is tested. */
    object.set_is_shadow_catcher(true);
    object.set_visibility(~0);

    object.set_geometry(this);

    vector<Geometry *> geometry;
    geometry.push_back(this);
    vector<Object *> objects;
    objects.push_back(&object);

    if (bvh && !need_update_rebuild) {
      progress->set_status(msg, "Refitting BVH");

      bvh->replace_geometry(geometry, objects);

      device->build_bvh(bvh, *progress, true);
    }
    else {
      progress->set_status(msg, "Building BVH");

      BVHParams bparams;
      bparams.use_spatial_split = params->use_bvh_spatial_split;
      bparams.use_compact_structure = params->use_bvh_compact_structure;
      bparams.bvh_layout = bvh_layout;
      bparams.use_unaligned_nodes = dscene->data.bvh.have_curves &&
                                    params->use_bvh_unaligned_nodes;
      bparams.num_motion_triangle_steps = params->num_bvh_time_steps;
      bparams.num_motion_curve_steps = params->num_bvh_time_steps;
      bparams.num_motion_point_steps = params->num_bvh_time_steps;
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
      if (shader->emission_sampling != EMISSION_SAMPLING_NONE) {
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

void GeometryManager::update_osl_globals(Device *device, Scene *scene)
{
#ifdef WITH_OSL
  OSLGlobals *og = (OSLGlobals *)device->get_cpu_osl_memory();

  og->object_name_map.clear();
  og->object_names.clear();

  for (size_t i = 0; i < scene->objects.size(); i++) {
    /* set object name to object index map */
    Object *object = scene->objects[i];
    og->object_name_map[object->name] = i;
    og->object_names.push_back(object->name);
  }
#else
  (void)device;
  (void)scene;
#endif
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

  if (type == TypeDesc::TypeFloat)
    attr_map[index].type = NODE_ATTR_FLOAT;
  else if (type == TypeDesc::TypeMatrix)
    attr_map[index].type = NODE_ATTR_MATRIX;
  else if (type == TypeFloat2)
    attr_map[index].type = NODE_ATTR_FLOAT2;
  else if (type == TypeFloat4)
    attr_map[index].type = NODE_ATTR_FLOAT4;
  else if (type == TypeRGBA)
    attr_map[index].type = NODE_ATTR_RGBA;
  else
    attr_map[index].type = NODE_ATTR_FLOAT3;

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

  if (attr_map_size == 0)
    return;

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
      if (req.std == ATTR_STD_NONE)
        id = scene->shader_manager->get_attribute_id(req.name);
      else
        id = scene->shader_manager->get_attribute_id(req.std);

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
        if (req.std == ATTR_STD_NONE)
          id = scene->shader_manager->get_attribute_id(req.name);
        else
          id = scene->shader_manager->get_attribute_id(req.std);

        emit_attribute_mapping(attr_map, index, id, req, object->geometry);
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
        offset -= hair->curve_key_offset;
      else if (element == ATTR_ELEMENT_CURVE_KEY_MOTION)
        offset -= hair->curve_key_offset;
    }
    else if (geom->is_pointcloud()) {
      if (element == ATTR_ELEMENT_VERTEX)
        offset -= geom->prim_offset;
      else if (element == ATTR_ELEMENT_VERTEX_MOTION)
        offset -= geom->prim_offset;
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

      if (progress.get_cancel())
        return;
    }
  }

  /* create attribute lookup maps */
  if (scene->shader_manager->use_osl())
    update_osl_globals(device, scene);

  update_svm_attributes(device, dscene, scene, geom_attributes, object_attributes);

  if (progress.get_cancel())
    return;

  /* copy to device */
  progress.set_status("Updating Mesh", "Copying Attributes to device");

  dscene->attributes_float.copy_to_device_if_modified();
  dscene->attributes_float2.copy_to_device_if_modified();
  dscene->attributes_float3.copy_to_device_if_modified();
  dscene->attributes_float4.copy_to_device_if_modified();
  dscene->attributes_uchar4.copy_to_device_if_modified();

  if (progress.get_cancel())
    return;

  /* After mesh attributes and patch tables have been copied to device memory,
   * we need to update offsets in the objects. */
  scene->object_manager->device_update_geom_offsets(device, dscene, scene);
}

void GeometryManager::geom_calc_offset(Scene *scene, BVHLayout bvh_layout)
{
  size_t vert_size = 0;
  size_t tri_size = 0;

  size_t curve_size = 0;
  size_t curve_key_size = 0;
  size_t curve_segment_size = 0;

  size_t point_size = 0;

  size_t patch_size = 0;
  size_t face_size = 0;
  size_t corner_size = 0;

  foreach (Geometry *geom, scene->geometry) {
    bool prim_offset_changed = false;

    if (geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::VOLUME) {
      Mesh *mesh = static_cast<Mesh *>(geom);

      prim_offset_changed = (mesh->prim_offset != tri_size);

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
    }
    else if (geom->is_hair()) {
      Hair *hair = static_cast<Hair *>(geom);

      prim_offset_changed = (hair->curve_segment_offset != curve_segment_size);
      hair->curve_key_offset = curve_key_size;
      hair->curve_segment_offset = curve_segment_size;
      hair->prim_offset = curve_size;

      curve_size += hair->num_curves();
      curve_key_size += hair->get_curve_keys().size();
      curve_segment_size += hair->num_segments();
    }
    else if (geom->is_pointcloud()) {
      PointCloud *pointcloud = static_cast<PointCloud *>(geom);

      prim_offset_changed = (pointcloud->prim_offset != point_size);

      pointcloud->prim_offset = point_size;
      point_size += pointcloud->num_points();
    }

    if (prim_offset_changed) {
      /* Need to rebuild BVH in OptiX, since refit only allows modified mesh data there */
      const bool has_optix_bvh = bvh_layout == BVH_LAYOUT_OPTIX ||
                                 bvh_layout == BVH_LAYOUT_MULTI_OPTIX ||
                                 bvh_layout == BVH_LAYOUT_MULTI_OPTIX_EMBREE;
      geom->need_update_rebuild |= has_optix_bvh;
      geom->need_update_bvh_for_offset = true;
    }
  }
}

void GeometryManager::device_update_mesh(Device *,
                                         DeviceScene *dscene,
                                         Scene *scene,
                                         Progress &progress)
{
  /* Count. */
  size_t vert_size = 0;
  size_t tri_size = 0;

  size_t curve_key_size = 0;
  size_t curve_size = 0;
  size_t curve_segment_size = 0;

  size_t point_size = 0;

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
      curve_segment_size += hair->num_segments();
    }
    else if (geom->is_pointcloud()) {
      PointCloud *pointcloud = static_cast<PointCloud *>(geom);
      point_size += pointcloud->num_points();
    }
  }

  /* Fill in all the arrays. */
  if (tri_size != 0) {
    /* normals */
    progress.set_status("Updating Mesh", "Computing normals");

    packed_float3 *tri_verts = dscene->tri_verts.alloc(tri_size * 3);
    uint *tri_shader = dscene->tri_shader.alloc(tri_size);
    packed_float3 *vnormal = dscene->tri_vnormal.alloc(vert_size);
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

        if (mesh->verts_is_modified() || mesh->triangles_is_modified() ||
            mesh->vert_patch_uv_is_modified() || copy_all_data) {
          mesh->pack_verts(&tri_verts[mesh->prim_offset * 3],
                           &tri_vindex[mesh->prim_offset],
                           &tri_patch[mesh->prim_offset],
                           &tri_patch_uv[mesh->vert_offset]);
        }

        if (progress.get_cancel())
          return;
      }
    }

    /* vertex coordinates */
    progress.set_status("Updating Mesh", "Copying Mesh to device");

    dscene->tri_verts.copy_to_device_if_modified();
    dscene->tri_shader.copy_to_device_if_modified();
    dscene->tri_vnormal.copy_to_device_if_modified();
    dscene->tri_vindex.copy_to_device_if_modified();
    dscene->tri_patch.copy_to_device_if_modified();
    dscene->tri_patch_uv.copy_to_device_if_modified();
  }

  if (curve_segment_size != 0) {
    progress.set_status("Updating Mesh", "Copying Curves to device");

    float4 *curve_keys = dscene->curve_keys.alloc(curve_key_size);
    KernelCurve *curves = dscene->curves.alloc(curve_size);
    KernelCurveSegment *curve_segments = dscene->curve_segments.alloc(curve_segment_size);

    const bool copy_all_data = dscene->curve_keys.need_realloc() ||
                               dscene->curves.need_realloc() ||
                               dscene->curve_segments.need_realloc();

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
                          &curve_keys[hair->curve_key_offset],
                          &curves[hair->prim_offset],
                          &curve_segments[hair->curve_segment_offset]);
        if (progress.get_cancel())
          return;
      }
    }

    dscene->curve_keys.copy_to_device_if_modified();
    dscene->curves.copy_to_device_if_modified();
    dscene->curve_segments.copy_to_device_if_modified();
  }

  if (point_size != 0) {
    progress.set_status("Updating Mesh", "Copying Point clouds to device");

    float4 *points = dscene->points.alloc(point_size);
    uint *points_shader = dscene->points_shader.alloc(point_size);

    foreach (Geometry *geom, scene->geometry) {
      if (geom->is_pointcloud()) {
        PointCloud *pointcloud = static_cast<PointCloud *>(geom);
        pointcloud->pack(
            scene, &points[pointcloud->prim_offset], &points_shader[pointcloud->prim_offset]);
        if (progress.get_cancel())
          return;
      }
    }

    dscene->points.copy_to_device();
    dscene->points_shader.copy_to_device();
  }

  if (patch_size != 0 && dscene->patches.need_realloc()) {
    progress.set_status("Updating Mesh", "Copying Patches to device");

    uint *patch_data = dscene->patches.alloc(patch_size);

    foreach (Geometry *geom, scene->geometry) {
      if (geom->is_mesh()) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        mesh->pack_patches(&patch_data[mesh->patch_offset]);

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
  bparams.num_motion_point_steps = scene->params.num_bvh_time_steps;
  bparams.bvh_type = scene->params.bvh_type;
  bparams.curve_subdivisions = scene->params.curve_subdivisions();

  VLOG_INFO << "Using " << bvh_layout_name(bparams.bvh_layout) << " layout.";

  const bool can_refit = scene->bvh != nullptr &&
                         (bparams.bvh_layout == BVHLayout::BVH_LAYOUT_OPTIX ||
                          bparams.bvh_layout == BVHLayout::BVH_LAYOUT_METAL);

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
    pack.root_index = -1;
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
  if (pack.prim_type.size()) {
    dscene->prim_type.steal_data(pack.prim_type);
    dscene->prim_type.copy_to_device();
  }
  if (pack.prim_visibility.size()) {
    dscene->prim_visibility.steal_data(pack.prim_visibility);
    dscene->prim_visibility.copy_to_device();
  }
  if (pack.prim_index.size()) {
    dscene->prim_index.steal_data(pack.prim_index);
    dscene->prim_index.copy_to_device();
  }
  if (pack.prim_object.size()) {
    dscene->prim_object.steal_data(pack.prim_object);
    dscene->prim_object.copy_to_device();
  }
  if (pack.prim_time.size()) {
    dscene->prim_time.steal_data(pack.prim_time);
    dscene->prim_time.copy_to_device();
  }

  dscene->data.bvh.root = pack.root_index;
  dscene->data.bvh.use_bvh_steps = (scene->params.num_bvh_time_steps != 0);
  dscene->data.bvh.curve_subdivisions = scene->params.curve_subdivisions();
  /* The scene handle is set in 'CPUDevice::const_copy_to' and 'OptiXDevice::const_copy_to' */
  dscene->data.device_bvh = 0;
}

/* Set of flags used to help determining what data has been modified or needs reallocation, so we
 * can decide which device data to free or update. */
enum {
  DEVICE_CURVE_DATA_MODIFIED = (1 << 0),
  DEVICE_MESH_DATA_MODIFIED = (1 << 1),
  DEVICE_POINT_DATA_MODIFIED = (1 << 2),

  ATTR_FLOAT_MODIFIED = (1 << 3),
  ATTR_FLOAT2_MODIFIED = (1 << 4),
  ATTR_FLOAT3_MODIFIED = (1 << 5),
  ATTR_FLOAT4_MODIFIED = (1 << 6),
  ATTR_UCHAR4_MODIFIED = (1 << 7),

  CURVE_DATA_NEED_REALLOC = (1 << 8),
  MESH_DATA_NEED_REALLOC = (1 << 9),
  POINT_DATA_NEED_REALLOC = (1 << 10),

  ATTR_FLOAT_NEEDS_REALLOC = (1 << 11),
  ATTR_FLOAT2_NEEDS_REALLOC = (1 << 12),
  ATTR_FLOAT3_NEEDS_REALLOC = (1 << 13),
  ATTR_FLOAT4_NEEDS_REALLOC = (1 << 14),

  ATTR_UCHAR4_NEEDS_REALLOC = (1 << 15),

  ATTRS_NEED_REALLOC = (ATTR_FLOAT_NEEDS_REALLOC | ATTR_FLOAT2_NEEDS_REALLOC |
                        ATTR_FLOAT3_NEEDS_REALLOC | ATTR_FLOAT4_NEEDS_REALLOC |
                        ATTR_UCHAR4_NEEDS_REALLOC),
  DEVICE_MESH_DATA_NEEDS_REALLOC = (MESH_DATA_NEED_REALLOC | ATTRS_NEED_REALLOC),
  DEVICE_POINT_DATA_NEEDS_REALLOC = (POINT_DATA_NEED_REALLOC | ATTRS_NEED_REALLOC),
  DEVICE_CURVE_DATA_NEEDS_REALLOC = (CURVE_DATA_NEED_REALLOC | ATTRS_NEED_REALLOC),
};

static void update_device_flags_attribute(uint32_t &device_update_flags,
                                          const AttributeSet &attributes)
{
  foreach (const Attribute &attr, attributes.attributes) {
    if (!attr.modified) {
      continue;
    }

    AttrKernelDataType kernel_type = Attribute::kernel_type(attr);

    switch (kernel_type) {
      case AttrKernelDataType::FLOAT: {
        device_update_flags |= ATTR_FLOAT_MODIFIED;
        break;
      }
      case AttrKernelDataType::FLOAT2: {
        device_update_flags |= ATTR_FLOAT2_MODIFIED;
        break;
      }
      case AttrKernelDataType::FLOAT3: {
        device_update_flags |= ATTR_FLOAT3_MODIFIED;
        break;
      }
      case AttrKernelDataType::FLOAT4: {
        device_update_flags |= ATTR_FLOAT4_MODIFIED;
        break;
      }
      case AttrKernelDataType::UCHAR4: {
        device_update_flags |= ATTR_UCHAR4_MODIFIED;
        break;
      }
      case AttrKernelDataType::NUM: {
        break;
      }
    }
  }
}

static void update_attribute_realloc_flags(uint32_t &device_update_flags,
                                           const AttributeSet &attributes)
{
  if (attributes.modified(AttrKernelDataType::FLOAT)) {
    device_update_flags |= ATTR_FLOAT_NEEDS_REALLOC;
  }
  if (attributes.modified(AttrKernelDataType::FLOAT2)) {
    device_update_flags |= ATTR_FLOAT2_NEEDS_REALLOC;
  }
  if (attributes.modified(AttrKernelDataType::FLOAT3)) {
    device_update_flags |= ATTR_FLOAT3_NEEDS_REALLOC;
  }
  if (attributes.modified(AttrKernelDataType::FLOAT4)) {
    device_update_flags |= ATTR_FLOAT4_NEEDS_REALLOC;
  }
  if (attributes.modified(AttrKernelDataType::UCHAR4)) {
    device_update_flags |= ATTR_UCHAR4_NEEDS_REALLOC;
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

    update_attribute_realloc_flags(device_update_flags, geom->attributes);

    if (geom->is_mesh()) {
      Mesh *mesh = static_cast<Mesh *>(geom);
      update_attribute_realloc_flags(device_update_flags, mesh->subd_attributes);
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
      create_volume_mesh(scene, volume, progress);

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

    if (geom->is_pointcloud()) {
      PointCloud *pointcloud = static_cast<PointCloud *>(geom);

      if (pointcloud->need_update_rebuild) {
        device_update_flags |= DEVICE_POINT_DATA_NEEDS_REALLOC;
      }
      else if (pointcloud->is_modified()) {
        device_update_flags |= DEVICE_POINT_DATA_MODIFIED;
      }
    }
  }

  if (update_flags & (MESH_ADDED | MESH_REMOVED)) {
    device_update_flags |= DEVICE_MESH_DATA_NEEDS_REALLOC;
  }

  if (update_flags & (HAIR_ADDED | HAIR_REMOVED)) {
    device_update_flags |= DEVICE_CURVE_DATA_NEEDS_REALLOC;
  }

  if (update_flags & (POINT_ADDED | POINT_REMOVED)) {
    device_update_flags |= DEVICE_POINT_DATA_NEEDS_REALLOC;
  }

  /* tag the device arrays for reallocation or modification */
  DeviceScene *dscene = &scene->dscene;

  if (device_update_flags & (DEVICE_MESH_DATA_NEEDS_REALLOC | DEVICE_CURVE_DATA_NEEDS_REALLOC |
                             DEVICE_POINT_DATA_NEEDS_REALLOC)) {
    delete scene->bvh;
    scene->bvh = nullptr;

    dscene->bvh_nodes.tag_realloc();
    dscene->bvh_leaf_nodes.tag_realloc();
    dscene->object_node.tag_realloc();
    dscene->prim_type.tag_realloc();
    dscene->prim_visibility.tag_realloc();
    dscene->prim_index.tag_realloc();
    dscene->prim_object.tag_realloc();
    dscene->prim_time.tag_realloc();

    if (device_update_flags & DEVICE_MESH_DATA_NEEDS_REALLOC) {
      dscene->tri_verts.tag_realloc();
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
      dscene->curve_segments.tag_realloc();
    }

    if (device_update_flags & DEVICE_POINT_DATA_NEEDS_REALLOC) {
      dscene->points.tag_realloc();
      dscene->points_shader.tag_realloc();
    }
  }

  if ((update_flags & VISIBILITY_MODIFIED) != 0) {
    dscene->prim_visibility.tag_modified();
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

  if (device_update_flags & ATTR_FLOAT4_NEEDS_REALLOC) {
    dscene->attributes_map.tag_realloc();
    dscene->attributes_float4.tag_realloc();
  }
  else if (device_update_flags & ATTR_FLOAT4_MODIFIED) {
    dscene->attributes_float4.tag_modified();
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
    dscene->tri_verts.tag_modified();
    dscene->tri_vnormal.tag_modified();
    dscene->tri_shader.tag_modified();
  }

  if (device_update_flags & DEVICE_CURVE_DATA_MODIFIED) {
    dscene->curve_keys.tag_modified();
    dscene->curves.tag_modified();
    dscene->curve_segments.tag_modified();
  }

  if (device_update_flags & DEVICE_POINT_DATA_MODIFIED) {
    dscene->points.tag_modified();
    dscene->points_shader.tag_modified();
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
      /* Geometry-level check for hair shadow transparency.
       * This matches the logic in the `Hair::update_shadow_transparency()`, avoiding access to
       * possible non-loaded images. */
      bool need_shadow_transparency = false;
      if (geom->geometry_type == Geometry::HAIR) {
        Hair *hair = static_cast<Hair *>(geom);
        need_shadow_transparency = hair->need_shadow_transparency();
      }

      foreach (Node *node, geom->get_used_shaders()) {
        Shader *shader = static_cast<Shader *>(node);
        const bool is_true_displacement = (shader->has_displacement &&
                                           shader->get_displacement_method() != DISPLACE_BUMP);
        if (!is_true_displacement && !need_shadow_transparency) {
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

  VLOG_INFO << "Total " << scene->geometry.size() << " meshes.";

  bool true_displacement_used = false;
  bool curve_shadow_transparency_used = false;
  size_t total_tess_needed = 0;

  {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->geometry.times.add_entry({"device_update (normals)", time});
      }
    });

    foreach (Geometry *geom, scene->geometry) {
      if (geom->is_modified()) {
        if ((geom->geometry_type == Geometry::MESH || geom->geometry_type == Geometry::VOLUME)) {
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
        }
        else if (geom->geometry_type == Geometry::HAIR) {
          Hair *hair = static_cast<Hair *>(geom);
          if (hair->need_shadow_transparency()) {
            curve_shadow_transparency_used = true;
          }
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
    dicing_camera->set_screen_size(dicing_camera->get_full_width(),
                                   dicing_camera->get_full_height());
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
  if (true_displacement_used || curve_shadow_transparency_used) {
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
  geom_calc_offset(scene, bvh_layout);
  if (true_displacement_used || curve_shadow_transparency_used) {
    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->geometry.times.add_entry(
            {"device_update (displacement: copy meshes to device)", time});
      }
    });
    device_update_mesh(device, dscene, scene, progress);
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

  /* Update displacement and hair shadow transparency. */
  bool displacement_done = false;
  bool curve_shadow_transparency_done = false;
  size_t num_bvh = 0;

  {
    /* Copy constant data needed by shader evaluation. */
    device->const_copy_to("data", &dscene->data, sizeof(dscene->data));

    scoped_callback_timer timer([scene](double time) {
      if (scene->update_stats) {
        scene->update_stats->geometry.times.add_entry({"device_update (displacement)", time});
      }
    });

    foreach (Geometry *geom, scene->geometry) {
      if (geom->is_modified()) {
        if (geom->is_mesh()) {
          Mesh *mesh = static_cast<Mesh *>(geom);
          if (displace(device, scene, mesh, progress)) {
            displacement_done = true;
          }
        }
        else if (geom->geometry_type == Geometry::HAIR) {
          Hair *hair = static_cast<Hair *>(geom);
          if (hair->update_shadow_transparency(device, scene, progress)) {
            curve_shadow_transparency_done = true;
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
  if (displacement_done || curve_shadow_transparency_done) {
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
  bool need_update_scene_bvh = (scene->bvh == nullptr ||
                                (update_flags & (TRANSFORM_MODIFIED | VISIBILITY_MODIFIED)) != 0);
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
    VLOG_WORK << "Objects BVH build pool statistics:\n" << summary.full_report();
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
    device_update_mesh(device, dscene, scene, progress);
    if (progress.get_cancel()) {
      return;
    }
  }

  if (true_displacement_used) {
    /* Re-tag flags for update, so they're re-evaluated
     * for meshes with correct bounding boxes.
     *
     * This wouldn't cause wrong results, just true
     * displacement might be less optimal to calculate.
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
  dscene->prim_type.clear_modified();
  dscene->prim_visibility.clear_modified();
  dscene->prim_index.clear_modified();
  dscene->prim_object.clear_modified();
  dscene->prim_time.clear_modified();
  dscene->tri_verts.clear_modified();
  dscene->tri_shader.clear_modified();
  dscene->tri_vindex.clear_modified();
  dscene->tri_patch.clear_modified();
  dscene->tri_vnormal.clear_modified();
  dscene->tri_patch_uv.clear_modified();
  dscene->curves.clear_modified();
  dscene->curve_keys.clear_modified();
  dscene->curve_segments.clear_modified();
  dscene->points.clear_modified();
  dscene->points_shader.clear_modified();
  dscene->patches.clear_modified();
  dscene->attributes_map.clear_modified();
  dscene->attributes_float.clear_modified();
  dscene->attributes_float2.clear_modified();
  dscene->attributes_float3.clear_modified();
  dscene->attributes_float4.clear_modified();
  dscene->attributes_uchar4.clear_modified();
}

void GeometryManager::device_free(Device *device, DeviceScene *dscene, bool force_free)
{
  dscene->bvh_nodes.free_if_need_realloc(force_free);
  dscene->bvh_leaf_nodes.free_if_need_realloc(force_free);
  dscene->object_node.free_if_need_realloc(force_free);
  dscene->prim_type.free_if_need_realloc(force_free);
  dscene->prim_visibility.free_if_need_realloc(force_free);
  dscene->prim_index.free_if_need_realloc(force_free);
  dscene->prim_object.free_if_need_realloc(force_free);
  dscene->prim_time.free_if_need_realloc(force_free);
  dscene->tri_verts.free_if_need_realloc(force_free);
  dscene->tri_shader.free_if_need_realloc(force_free);
  dscene->tri_vnormal.free_if_need_realloc(force_free);
  dscene->tri_vindex.free_if_need_realloc(force_free);
  dscene->tri_patch.free_if_need_realloc(force_free);
  dscene->tri_patch_uv.free_if_need_realloc(force_free);
  dscene->curves.free_if_need_realloc(force_free);
  dscene->curve_keys.free_if_need_realloc(force_free);
  dscene->curve_segments.free_if_need_realloc(force_free);
  dscene->points.free_if_need_realloc(force_free);
  dscene->points_shader.free_if_need_realloc(force_free);
  dscene->patches.free_if_need_realloc(force_free);
  dscene->attributes_map.free_if_need_realloc(force_free);
  dscene->attributes_float.free_if_need_realloc(force_free);
  dscene->attributes_float2.free_if_need_realloc(force_free);
  dscene->attributes_float3.free_if_need_realloc(force_free);
  dscene->attributes_float4.free_if_need_realloc(force_free);
  dscene->attributes_uchar4.free_if_need_realloc(force_free);

  /* Signal for shaders like displacement not to do ray tracing. */
  dscene->data.bvh.bvh_layout = BVH_LAYOUT_NONE;

#ifdef WITH_OSL
  OSLGlobals *og = (OSLGlobals *)device->get_cpu_osl_memory();

  if (og) {
    og->object_name_map.clear();
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
