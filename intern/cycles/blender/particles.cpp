/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/particles.h"
#include "scene/mesh.h"
#include "scene/object.h"

#include "blender/sync.h"
#include "blender/util.h"

#include "DNA_particle_types.h"

#include "BKE_scene.hh"

#include "DEG_depsgraph_query.hh"

CCL_NAMESPACE_BEGIN

/* Utilities */

bool BlenderSync::sync_dupli_particle(::Object &b_parent,
                                      ::DEGObjectIterData &b_deg_iter_data,
                                      ::Object &b_ob,
                                      Object *object)
{
  /* Test if this dupli was generated from a particle system. */
  ::ParticleSystem *b_psys = b_deg_iter_data.dupli_object_current->particle_system;
  if (!b_psys) {
    return false;
  }

  object->set_hide_on_missing_motion(true);

  /* test if we need particle data */
  if (!object->get_geometry()->need_attribute(scene, ATTR_STD_PARTICLE)) {
    return false;
  }

  /* don't handle child particles yet */
  const int *persistent_id = b_deg_iter_data.dupli_object_current->persistent_id;

  if (persistent_id[0] >= b_psys->totpart) {
    return false;
  }

  /* find particle system */
  const ParticleSystemKey key(&b_parent, persistent_id);
  ParticleSystem *psys;

  const bool first_use = !particle_system_map.is_used(key);
  const bool need_update = particle_system_map.add_or_update(&psys, &b_parent.id, &b_ob.id, key);

  /* no update needed? */
  if (!need_update && !object->get_geometry()->is_modified() &&
      !scene->object_manager->need_update())
  {
    return true;
  }

  /* first time used in this sync loop? clear and tag update */
  if (first_use) {
    psys->particles.clear();
    psys->tag_update(scene);
  }

  /* add particle */
  ::ParticleData &b_pa = b_psys->particles[persistent_id[0]];
  Particle pa;

  pa.index = persistent_id[0];
  pa.age = BKE_scene_frame_to_ctime(b_scene, b_scene->r.cfra) - b_pa.time;
  pa.lifetime = b_pa.lifetime;
  pa.location = make_float3(b_pa.state.co[0], b_pa.state.co[1], b_pa.state.co[2]);
  pa.rotation = make_float4(
      b_pa.state.rot[0], b_pa.state.rot[1], b_pa.state.rot[2], b_pa.state.rot[3]);
  pa.size = b_pa.size;
  pa.velocity = make_float3(b_pa.state.vel[0], b_pa.state.vel[1], b_pa.state.vel[2]);
  pa.angular_velocity = make_float3(b_pa.state.ave[0], b_pa.state.ave[1], b_pa.state.ave[2]);

  psys->particles.push_back_slow(pa);

  object->set_particle_system(psys);
  object->set_particle_index(psys->particles.size() - 1);

  if (object->particle_index_is_modified()) {
    scene->object_manager->tag_update(scene, ObjectManager::PARTICLE_MODIFIED);
  }

  /* return that this object has particle data */
  return true;
}

CCL_NAMESPACE_END
