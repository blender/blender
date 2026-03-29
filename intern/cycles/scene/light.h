/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/types.h"

#include "graph/node.h"

#include "scene/geometry.h"

#include "util/ies.h"
#include "util/thread.h"
#include "util/types.h"
#include "util/unique_ptr.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Object;
class Progress;
class Scene;
class Shader;

class Light : public Geometry {
 public:
  NODE_ABSTRACT_DECLARE;

  Light(const NodeType *node_type, const Geometry::Type type);

  NODE_SOCKET_API(LightType, light_type)
  NODE_SOCKET_API(float3, strength)

  NODE_SOCKET_API(bool, cast_shadow)
  NODE_SOCKET_API(bool, use_mis)
  NODE_SOCKET_API(bool, use_caustics)

  NODE_SOCKET_API(bool, is_enabled)

  NODE_SOCKET_API(int, max_bounces)

  /* Normalize power by the surface area of the light. */
  NODE_SOCKET_API(bool, normalize)

  void tag_update(Scene *scene);

  /* Check whether the light has contribution the scene. */
  bool has_contribution(const Scene *scene, const Object *object);

  /* Shader */
  Shader *get_shader() const;

  virtual float area(const Transform &tfm) const = 0;

  /* Geometry */
  void compute_bounds() override;
  void apply_transform(const Transform &tfm, const bool apply_to_motion) override;
  void get_uv_tiles(ustring map, unordered_set<int> &tiles) override;
  PrimitiveType primitive_type() const override;

  void copy_to_kernel(KernelLight *klight,
                      const Scene *scene,
                      const Object *object,
                      const uint shader_flags) const;
  virtual void copy_to_kernel(KernelLight *klight,
                              const Scene *scene,
                              const Object *object) const = 0;

  virtual bool is_traceable() const = 0;

  bool is_spot_light() const;
  bool is_point_light() const;
  bool is_area_light() const;
  bool is_sun_light() const;
  bool is_background_light() const;
  bool is_distant_light() const;
  virtual bool is_portal_light() const
  {
    return false;
  }

  friend class LightManager;
  friend class LightTree;
};

class PointLight : public Light {
 public:
  NODE_DECLARE;

  PointLight();
  PointLight(const NodeType *node_type, const Geometry::Type type) : Light(node_type, type) {};

  float area(const Transform &tfm) const override;
  void copy_to_kernel(KernelLight *klight,
                      const Scene *scene,
                      const Object *object) const override;
  bool is_traceable() const override
  {
    return radius > 0.0f;
  };

  NODE_SOCKET_API(float, radius)
  NODE_SOCKET_API(bool, is_sphere)
};

class SpotLight : public PointLight {
 public:
  NODE_DECLARE;

  SpotLight();
  void copy_to_kernel(KernelLight *klight,
                      const Scene *scene,
                      const Object *object) const override;

  NODE_SOCKET_API(float, angle)
  NODE_SOCKET_API(float, smooth)
};

class AreaLight : public Light {
 public:
  NODE_DECLARE;

  AreaLight();

  float area(const Transform &tfm) const override;
  void copy_to_kernel(KernelLight *klight,
                      const Scene *scene,
                      const Object *object) const override;
  bool is_traceable() const override
  {
    return sizeu * sizev > 0.0f;
  };

  bool is_portal_light() const override
  {
    return is_portal;
  }

  NODE_SOCKET_API(float, sizeu)
  NODE_SOCKET_API(float, sizev)
  NODE_SOCKET_API(bool, ellipse)
  NODE_SOCKET_API(float, spread)
  NODE_SOCKET_API(bool, is_portal)
};

class SunLight : public Light {
 public:
  NODE_DECLARE;

  SunLight();

  float area(const Transform &tfm) const override;
  void copy_to_kernel(KernelLight *klight,
                      const Scene *scene,
                      const Object *object) const override;
  bool is_traceable() const override
  {
    return false;
  };

  NODE_SOCKET_API(float, angle)
};

class BackgroundLight : public Light {
 public:
  NODE_DECLARE;

  BackgroundLight();

  float area(const Transform &tfm) const override;
  void copy_to_kernel(KernelLight *klight,
                      const Scene *scene,
                      const Object *object) const override;
  bool is_traceable() const override
  {
    return false;
  };

  NODE_SOCKET_API(int, map_resolution)
  NODE_SOCKET_API(float, average_radiance)
};

class LightManager {
 public:
  enum : uint32_t {
    MESH_NEED_REBUILD = (1 << 0),
    EMISSIVE_MESH_MODIFIED = (1 << 1),
    LIGHT_MODIFIED = (1 << 2),
    LIGHT_ADDED = (1 << 3),
    LIGHT_REMOVED = (1 << 4),
    OBJECT_MANAGER = (1 << 5),
    SHADER_COMPILED = (1 << 6),
    SHADER_MODIFIED = (1 << 7),

    /* tag everything in the manager for an update */
    UPDATE_ALL = ~0u,

    UPDATE_NONE = 0u,
  };

  /* Need to update background (including multiple importance map) */
  bool need_update_background;

  LightManager();

  /* IES texture management */
  int add_ies(const string &content, const bool log_parsing_error);
  int add_ies_from_file(const string &filename);
  void remove_ies(const int slot);

  void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  void device_free(Device *device, DeviceScene *dscene, const bool free_background = true);

  void tag_update(Scene *scene, const uint32_t flag);

  bool need_update() const;

  /* Check whether there is a background light. */
  bool has_background_light(Scene *scene);

 protected:
  /* Optimization: disable light which is either unsupported or
   * which doesn't contribute to the scene or which is only used for MIS
   * and scene doesn't need MIS.
   */
  void test_enabled_lights(Scene *scene);
  /* Count lights in the scene. */
  void count_lights(KernelIntegrator *kintegrator, const Scene *scene);

  void device_update_lights(DeviceScene *dscene, Scene *scene);
  void device_update_distribution(Device *device,
                                  DeviceScene *dscene,
                                  Scene *scene,
                                  Progress &progress);
  void device_update_tree(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  void device_update_background(Device *device,
                                DeviceScene *dscene,
                                Scene *scene,
                                Progress &progress);
  void device_update_ies(DeviceScene *dscene);

  struct IESSlot {
    IESFile ies;
    uint hash;
    int users;
  };

  vector<unique_ptr<IESSlot>> ies_slots;
  thread_mutex ies_mutex;

  bool last_background_enabled;
  int last_background_resolution;

  uint32_t update_flags;
};

CCL_NAMESPACE_END
