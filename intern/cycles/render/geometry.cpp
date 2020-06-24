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
#include "bvh/bvh_build.h"
#include "bvh/bvh_embree.h"

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

#include "subd/subd_patch_table.h"
#include "subd/subd_split.h"

#include "kernel/osl/osl_globals.h"

#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_progress.h"

CCL_NAMESPACE_BEGIN

/* Geometry */

NODE_ABSTRACT_DEFINE(Geometry)
{
  NodeType *type = NodeType::add("geometry_base", NULL);

  SOCKET_UINT(motion_steps, "Motion Steps", 3);
  SOCKET_BOOLEAN(use_motion_blur, "Use Motion Blur", false);

  return type;
}

Geometry::Geometry(const NodeType *node_type, const Type type)
    : Node(node_type), type(type), attributes(this, ATTR_PRIM_GEOMETRY)
{
  need_update = true;
  need_update_rebuild = false;

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
  delete bvh;
}

void Geometry::clear()
{
  used_shaders.clear();
  transform_applied = false;
  transform_negative_scaled = false;
  transform_normal = transform_identity();
}

bool Geometry::need_attribute(Scene *scene, AttributeStandard std)
{
  if (std == ATTR_STD_NONE)
    return false;

  if (scene->need_global_attribute(std))
    return true;

  foreach (Shader *shader, used_shaders)
    if (shader->attributes.find(std))
      return true;

  return false;
}

bool Geometry::need_attribute(Scene * /*scene*/, ustring name)
{
  if (name == ustring())
    return false;

  foreach (Shader *shader, used_shaders)
    if (shader->attributes.find(name))
      return true;

  return false;
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
  return !transform_applied || has_surface_bssrdf || layout == BVH_LAYOUT_OPTIX;
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
  foreach (Shader *shader, used_shaders) {
    if (shader->has_displacement && shader->displacement_method != DISPLACE_BUMP) {
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
    object.geometry = this;

    vector<Geometry *> geometry;
    geometry.push_back(this);
    vector<Object *> objects;
    objects.push_back(&object);

    if (bvh && !need_update_rebuild) {
      progress->set_status(msg, "Refitting BVH");

      bvh->geometry = geometry;
      bvh->objects = objects;

      bvh->refit(*progress);
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
      bvh = BVH::create(bparams, geometry, objects);
      MEM_GUARDED_CALL(progress, bvh->build, *progress);
    }
  }

  need_update = false;
  need_update_rebuild = false;
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
  need_update = true;

  if (rebuild) {
    need_update_rebuild = true;
    scene->light_manager->need_update = true;
  }
  else {
    foreach (Shader *shader, used_shaders)
      if (shader->has_surface_emission)
        scene->light_manager->need_update = true;
  }

  scene->geometry_manager->need_update = true;
  scene->object_manager->need_update = true;
}

/* Geometry Manager */

GeometryManager::GeometryManager()
{
  need_update = true;
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
    size_t j;

    for (j = 0; j < scene->geometry.size(); j++)
      if (scene->geometry[j] == object->geometry)
        break;

    AttributeRequestSet &attributes = geom_attributes[j];

    /* set object attributes */
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

void GeometryManager::update_svm_attributes(Device *,
                                            DeviceScene *dscene,
                                            Scene *scene,
                                            vector<AttributeRequestSet> &geom_attributes)
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

  if (attr_map_size == 0)
    return;

  /* create attribute map */
  uint4 *attr_map = dscene->attributes_map.alloc(attr_map_size);
  memset(attr_map, 0, dscene->attributes_map.size() * sizeof(uint));

  for (size_t i = 0; i < scene->geometry.size(); i++) {
    Geometry *geom = scene->geometry[i];
    AttributeRequestSet &attributes = geom_attributes[i];

    /* set object attributes */
    int index = geom->attr_map_offset;

    foreach (AttributeRequest &req, attributes.requests) {
      uint id;

      if (req.std == ATTR_STD_NONE)
        id = scene->shader_manager->get_attribute_id(req.name);
      else
        id = scene->shader_manager->get_attribute_id(req.std);

      attr_map[index].x = id;
      attr_map[index].y = req.desc.element;
      attr_map[index].z = as_uint(req.desc.offset);

      if (req.type == TypeDesc::TypeFloat)
        attr_map[index].w = NODE_ATTR_FLOAT;
      else if (req.type == TypeDesc::TypeMatrix)
        attr_map[index].w = NODE_ATTR_MATRIX;
      else if (req.type == TypeFloat2)
        attr_map[index].w = NODE_ATTR_FLOAT2;
      else if (req.type == TypeRGBA)
        attr_map[index].w = NODE_ATTR_RGBA;
      else
        attr_map[index].w = NODE_ATTR_FLOAT3;

      attr_map[index].w |= req.desc.flags << 8;

      index++;

      if (geom->type == Geometry::MESH) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        if (mesh->subd_faces.size()) {
          attr_map[index].x = id;
          attr_map[index].y = req.subd_desc.element;
          attr_map[index].z = as_uint(req.subd_desc.offset);

          if (req.subd_type == TypeDesc::TypeFloat)
            attr_map[index].w = NODE_ATTR_FLOAT;
          else if (req.subd_type == TypeDesc::TypeMatrix)
            attr_map[index].w = NODE_ATTR_MATRIX;
          else if (req.subd_type == TypeFloat2)
            attr_map[index].w = NODE_ATTR_FLOAT2;
          else if (req.subd_type == TypeRGBA)
            attr_map[index].w = NODE_ATTR_RGBA;
          else
            attr_map[index].w = NODE_ATTR_FLOAT3;

          attr_map[index].w |= req.subd_desc.flags << 8;
        }
      }

      index++;
    }

    /* terminator */
    for (int j = 0; j < ATTR_PRIM_TYPES; j++) {
      attr_map[index].x = ATTR_STD_NONE;
      attr_map[index].y = 0;
      attr_map[index].z = 0;
      attr_map[index].w = 0;

      index++;
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

static void update_attribute_element_offset(Geometry *geom,
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
      for (size_t k = 0; k < size; k++) {
        attr_uchar4[offset + k] = data[k];
      }
      attr_uchar4_offset += size;
    }
    else if (mattr->type == TypeDesc::TypeFloat) {
      float *data = mattr->data_float();
      offset = attr_float_offset;

      assert(attr_float.size() >= offset + size);
      for (size_t k = 0; k < size; k++) {
        attr_float[offset + k] = data[k];
      }
      attr_float_offset += size;
    }
    else if (mattr->type == TypeFloat2) {
      float2 *data = mattr->data_float2();
      offset = attr_float2_offset;

      assert(attr_float2.size() >= offset + size);
      for (size_t k = 0; k < size; k++) {
        attr_float2[offset + k] = data[k];
      }
      attr_float2_offset += size;
    }
    else if (mattr->type == TypeDesc::TypeMatrix) {
      Transform *tfm = mattr->data_transform();
      offset = attr_float3_offset;

      assert(attr_float3.size() >= offset + size * 3);
      for (size_t k = 0; k < size * 3; k++) {
        attr_float3[offset + k] = (&tfm->x)[k];
      }
      attr_float3_offset += size * 3;
    }
    else {
      float4 *data = mattr->data_float4();
      offset = attr_float3_offset;

      assert(attr_float3.size() >= offset + size);
      for (size_t k = 0; k < size; k++) {
        attr_float3[offset + k] = data[k];
      }
      attr_float3_offset += size;
    }

    /* mesh vertex/curve index is global, not per object, so we sneak
     * a correction for that in here */
    if (geom->type == Geometry::MESH) {
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
    else if (geom->type == Geometry::HAIR) {
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

    scene->need_global_attributes(geom_attributes[i]);

    foreach (Shader *shader, geom->used_shaders) {
      geom_attributes[i].add(shader->attributes);
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

      if (geom->type == Geometry::MESH) {
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

  dscene->attributes_float.alloc(attr_float_size);
  dscene->attributes_float2.alloc(attr_float2_size);
  dscene->attributes_float3.alloc(attr_float3_size);
  dscene->attributes_uchar4.alloc(attr_uchar4_size);

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

      if (geom->type == Geometry::MESH) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        Attribute *subd_attr = mesh->subd_attributes.find(req);

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

  /* create attribute lookup maps */
  if (scene->shader_manager->use_osl())
    update_osl_attributes(device, scene, geom_attributes);

  update_svm_attributes(device, dscene, scene, geom_attributes);

  if (progress.get_cancel())
    return;

  /* copy to device */
  progress.set_status("Updating Mesh", "Copying Attributes to device");

  if (dscene->attributes_float.size()) {
    dscene->attributes_float.copy_to_device();
  }
  if (dscene->attributes_float2.size()) {
    dscene->attributes_float2.copy_to_device();
  }
  if (dscene->attributes_float3.size()) {
    dscene->attributes_float3.copy_to_device();
  }
  if (dscene->attributes_uchar4.size()) {
    dscene->attributes_uchar4.copy_to_device();
  }

  if (progress.get_cancel())
    return;

  /* After mesh attributes and patch tables have been copied to device memory,
   * we need to update offsets in the objects. */
  scene->object_manager->device_update_mesh_offsets(device, dscene, scene);
}

void GeometryManager::mesh_calc_offset(Scene *scene)
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
    if (geom->type == Geometry::MESH) {
      Mesh *mesh = static_cast<Mesh *>(geom);

      mesh->vert_offset = vert_size;
      mesh->prim_offset = tri_size;

      mesh->patch_offset = patch_size;
      mesh->face_offset = face_size;
      mesh->corner_offset = corner_size;

      vert_size += mesh->verts.size();
      tri_size += mesh->num_triangles();

      if (mesh->subd_faces.size()) {
        Mesh::SubdFace &last = mesh->subd_faces[mesh->subd_faces.size() - 1];
        patch_size += (last.ptex_offset + last.num_ptex_faces()) * 8;

        /* patch tables are stored in same array so include them in patch_size */
        if (mesh->patch_table) {
          mesh->patch_table_offset = patch_size;
          patch_size += mesh->patch_table->total_size();
        }
      }

      face_size += mesh->subd_faces.size();
      corner_size += mesh->subd_face_corners.size();

      mesh->optix_prim_offset = optix_prim_size;
      optix_prim_size += mesh->num_triangles();
    }
    else if (geom->type == Geometry::HAIR) {
      Hair *hair = static_cast<Hair *>(geom);

      hair->curvekey_offset = curve_key_size;
      hair->prim_offset = curve_size;

      curve_key_size += hair->curve_keys.size();
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
    if (geom->type == Geometry::MESH) {
      Mesh *mesh = static_cast<Mesh *>(geom);

      vert_size += mesh->verts.size();
      tri_size += mesh->num_triangles();

      if (mesh->subd_faces.size()) {
        Mesh::SubdFace &last = mesh->subd_faces[mesh->subd_faces.size() - 1];
        patch_size += (last.ptex_offset + last.num_ptex_faces()) * 8;

        /* patch tables are stored in same array so include them in patch_size */
        if (mesh->patch_table) {
          mesh->patch_table_offset = patch_size;
          patch_size += mesh->patch_table->total_size();
        }
      }
    }
    else if (geom->type == Geometry::HAIR) {
      Hair *hair = static_cast<Hair *>(geom);

      curve_key_size += hair->curve_keys.size();
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
      if (geom->type == Geometry::MESH) {
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

    foreach (Geometry *geom, scene->geometry) {
      if (geom->type == Geometry::MESH) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        mesh->pack_shaders(scene, &tri_shader[mesh->prim_offset]);
        mesh->pack_normals(&vnormal[mesh->vert_offset]);
        mesh->pack_verts(tri_prim_index,
                         &tri_vindex[mesh->prim_offset],
                         &tri_patch[mesh->prim_offset],
                         &tri_patch_uv[mesh->vert_offset],
                         mesh->vert_offset,
                         mesh->prim_offset);
        if (progress.get_cancel())
          return;
      }
    }

    /* vertex coordinates */
    progress.set_status("Updating Mesh", "Copying Mesh to device");

    dscene->tri_shader.copy_to_device();
    dscene->tri_vnormal.copy_to_device();
    dscene->tri_vindex.copy_to_device();
    dscene->tri_patch.copy_to_device();
    dscene->tri_patch_uv.copy_to_device();
  }

  if (curve_size != 0) {
    progress.set_status("Updating Mesh", "Copying Strands to device");

    float4 *curve_keys = dscene->curve_keys.alloc(curve_key_size);
    float4 *curves = dscene->curves.alloc(curve_size);

    foreach (Geometry *geom, scene->geometry) {
      if (geom->type == Geometry::HAIR) {
        Hair *hair = static_cast<Hair *>(geom);
        hair->pack_curves(scene,
                          &curve_keys[hair->curvekey_offset],
                          &curves[hair->prim_offset],
                          hair->curvekey_offset);
        if (progress.get_cancel())
          return;
      }
    }

    dscene->curve_keys.copy_to_device();
    dscene->curves.copy_to_device();
  }

  if (patch_size != 0) {
    progress.set_status("Updating Mesh", "Copying Patches to device");

    uint *patch_data = dscene->patches.alloc(patch_size);

    foreach (Geometry *geom, scene->geometry) {
      if (geom->type == Geometry::MESH) {
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
      if (geom->type == Geometry::MESH) {
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

  BVH *bvh = BVH::create(bparams, scene->geometry, scene->objects);
  bvh->build(progress, &device->stats);

  if (progress.get_cancel()) {
#ifdef WITH_EMBREE
    if (dscene->data.bvh.scene) {
      BVHEmbree::destroy(dscene->data.bvh.scene);
      dscene->data.bvh.scene = NULL;
    }
#endif
    delete bvh;
    return;
  }

  /* copy to device */
  progress.set_status("Updating Scene BVH", "Copying BVH to device");

  PackedBVH &pack = bvh->pack;

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
  if (pack.prim_tri_index.size()) {
    dscene->prim_tri_index.steal_data(pack.prim_tri_index);
    dscene->prim_tri_index.copy_to_device();
  }
  if (pack.prim_tri_verts.size()) {
    dscene->prim_tri_verts.steal_data(pack.prim_tri_verts);
    dscene->prim_tri_verts.copy_to_device();
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
  dscene->data.bvh.bvh_layout = bparams.bvh_layout;
  dscene->data.bvh.use_bvh_steps = (scene->params.num_bvh_time_steps != 0);
  dscene->data.bvh.curve_subdivisions = scene->params.curve_subdivisions();

  bvh->copy_to_device(progress, dscene);

  delete bvh;
}

void GeometryManager::device_update_preprocess(Device *device, Scene *scene, Progress &progress)
{
  if (!need_update && !need_flags_update) {
    return;
  }

  progress.set_status("Updating Meshes Flags");

  /* Update flags. */
  bool volume_images_updated = false;

  foreach (Geometry *geom, scene->geometry) {
    geom->has_volume = false;

    foreach (const Shader *shader, geom->used_shaders) {
      if (shader->has_volume) {
        geom->has_volume = true;
      }
      if (shader->has_surface_bssrdf) {
        geom->has_surface_bssrdf = true;
      }
    }

    if (need_update && geom->has_volume && geom->type == Geometry::MESH) {
      /* Create volume meshes if there is voxel data. */
      if (geom->has_voxel_attributes()) {
        if (!volume_images_updated) {
          progress.set_status("Updating Meshes Volume Bounds");
          device_update_volume_images(device, scene, progress);
          volume_images_updated = true;
        }

        Mesh *mesh = static_cast<Mesh *>(geom);
        create_volume_mesh(mesh, progress);
      }
    }

    if (geom->type == Geometry::HAIR) {
      /* Set curve shape, still a global scene setting for now. */
      Hair *hair = static_cast<Hair *>(geom);
      hair->curve_shape = scene->params.hair_shape;
    }
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
    if (geom->need_update) {
      foreach (Shader *shader, geom->used_shaders) {
        if (!shader->has_displacement || shader->displacement_method == DISPLACE_BUMP) {
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
    if (!geom->need_update) {
      continue;
    }

    foreach (Attribute &attr, geom->attributes.attributes) {
      if (attr.element != ATTR_ELEMENT_VOXEL) {
        continue;
      }

      ImageHandle &handle = attr.data_voxel();
      const int slot = handle.svm_slot();
      if (slot != -1) {
        volume_images.insert(slot);
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
  if (!need_update)
    return;

  VLOG(1) << "Total " << scene->geometry.size() << " meshes.";

  bool true_displacement_used = false;
  size_t total_tess_needed = 0;

  foreach (Geometry *geom, scene->geometry) {
    foreach (Shader *shader, geom->used_shaders) {
      if (shader->need_update_geometry)
        geom->need_update = true;
    }

    if (geom->need_update && geom->type == Geometry::MESH) {
      Mesh *mesh = static_cast<Mesh *>(geom);

      /* Update normals. */
      mesh->add_face_normals();
      mesh->add_vertex_normals();

      if (mesh->need_attribute(scene, ATTR_STD_POSITION_UNDISPLACED)) {
        mesh->add_undisplaced();
      }

      /* Test if we need tessellation. */
      if (mesh->subdivision_type != Mesh::SUBDIVISION_NONE && mesh->num_subd_verts == 0 &&
          mesh->subd_params) {
        total_tess_needed++;
      }

      /* Test if we need displacement. */
      if (mesh->has_true_displacement()) {
        true_displacement_used = true;
      }

      if (progress.get_cancel())
        return;
    }
  }

  /* Tessellate meshes that are using subdivision */
  if (total_tess_needed) {
    Camera *dicing_camera = scene->dicing_camera;
    dicing_camera->update(scene);

    size_t i = 0;
    foreach (Geometry *geom, scene->geometry) {
      if (!(geom->need_update && geom->type == Geometry::MESH)) {
        continue;
      }

      Mesh *mesh = static_cast<Mesh *>(geom);
      if (mesh->subdivision_type != Mesh::SUBDIVISION_NONE && mesh->num_subd_verts == 0 &&
          mesh->subd_params) {
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

        if (progress.get_cancel())
          return;
      }
    }
  }

  /* Update images needed for true displacement. */
  bool old_need_object_flags_update = false;
  if (true_displacement_used) {
    VLOG(1) << "Updating images used for true displacement.";
    device_update_displacement_images(device, scene, progress);
    old_need_object_flags_update = scene->object_manager->need_flags_update;
    scene->object_manager->device_update_flags(device, dscene, scene, progress, false);
  }

  /* Device update. */
  device_free(device, dscene);

  mesh_calc_offset(scene);
  if (true_displacement_used) {
    device_update_mesh(device, dscene, scene, true, progress);
  }
  if (progress.get_cancel())
    return;

  device_update_attributes(device, dscene, scene, progress);
  if (progress.get_cancel())
    return;

  /* Update displacement. */
  bool displacement_done = false;
  size_t num_bvh = 0;
  BVHLayout bvh_layout = BVHParams::best_bvh_layout(scene->params.bvh_layout,
                                                    device->get_bvh_layout_mask());

  foreach (Geometry *geom, scene->geometry) {
    if (geom->need_update) {
      if (geom->type == Geometry::MESH) {
        Mesh *mesh = static_cast<Mesh *>(geom);
        if (displace(device, dscene, scene, mesh, progress)) {
          displacement_done = true;
        }
      }

      if (geom->need_build_bvh(bvh_layout)) {
        num_bvh++;
      }
    }

    if (progress.get_cancel())
      return;
  }

  /* Device re-update after displacement. */
  if (displacement_done) {
    device_free(device, dscene);

    device_update_attributes(device, dscene, scene, progress);
    if (progress.get_cancel())
      return;
  }

  TaskPool pool;

  size_t i = 0;
  foreach (Geometry *geom, scene->geometry) {
    if (geom->need_update) {
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

  foreach (Shader *shader, scene->shaders) {
    shader->need_update_geometry = false;
  }

  Scene::MotionType need_motion = scene->need_motion();
  bool motion_blur = need_motion == Scene::MOTION_BLUR;

  /* Update objects. */
  vector<Object *> volume_objects;
  foreach (Object *object, scene->objects) {
    object->compute_bounds(motion_blur);
  }

  if (progress.get_cancel())
    return;

  device_update_bvh(device, dscene, scene, progress);
  if (progress.get_cancel())
    return;

  device_update_mesh(device, dscene, scene, false, progress);
  if (progress.get_cancel())
    return;

  need_update = false;

  if (true_displacement_used) {
    /* Re-tag flags for update, so they're re-evaluated
     * for meshes with correct bounding boxes.
     *
     * This wouldn't cause wrong results, just true
     * displacement might be less optimal ot calculate.
     */
    scene->object_manager->need_flags_update = old_need_object_flags_update;
  }
}

void GeometryManager::device_free(Device *device, DeviceScene *dscene)
{
#ifdef WITH_EMBREE
  if (dscene->data.bvh.bvh_layout == BVH_LAYOUT_EMBREE) {
    if (dscene->data.bvh.scene) {
      BVHEmbree::destroy(dscene->data.bvh.scene);
      dscene->data.bvh.scene = NULL;
    }
  }
#endif

  dscene->bvh_nodes.free();
  dscene->bvh_leaf_nodes.free();
  dscene->object_node.free();
  dscene->prim_tri_verts.free();
  dscene->prim_tri_index.free();
  dscene->prim_type.free();
  dscene->prim_visibility.free();
  dscene->prim_index.free();
  dscene->prim_object.free();
  dscene->prim_time.free();
  dscene->tri_shader.free();
  dscene->tri_vnormal.free();
  dscene->tri_vindex.free();
  dscene->tri_patch.free();
  dscene->tri_patch_uv.free();
  dscene->curves.free();
  dscene->curve_keys.free();
  dscene->patches.free();
  dscene->attributes_map.free();
  dscene->attributes_float.free();
  dscene->attributes_float2.free();
  dscene->attributes_float3.free();
  dscene->attributes_uchar4.free();

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

void GeometryManager::tag_update(Scene *scene)
{
  need_update = true;
  scene->object_manager->need_update = true;
}

void GeometryManager::collect_statistics(const Scene *scene, RenderStats *stats)
{
  foreach (Geometry *geometry, scene->geometry) {
    stats->mesh.geometry.add_entry(
        NamedSizeEntry(string(geometry->name.c_str()), geometry->get_total_size_in_bytes()));
  }
}

CCL_NAMESPACE_END
