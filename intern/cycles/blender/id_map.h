/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __BLENDER_ID_MAP_H__
#define __BLENDER_ID_MAP_H__

#include <string.h>

#include "scene/geometry.h"
#include "scene/scene.h"

#include "util/map.h"
#include "util/set.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

/* ID Map
 *
 * Utility class to map between Blender datablocks and Cycles data structures,
 * and keep track of recalc tags from the dependency graph. */

template<typename K, typename T, typename Flags = uint> class id_map {
 public:
  id_map(Scene *scene_) : scene(scene_) {}

  ~id_map()
  {
    set<T *> nodes;

    typename map<K, T *>::iterator jt;
    for (jt = b_map.begin(); jt != b_map.end(); jt++) {
      nodes.insert(jt->second);
    }

    scene->delete_nodes(nodes);
  }

  T *find(const BL::ID &id)
  {
    return find(id.ptr.owner_id);
  }

  T *find(const K &key)
  {
    if (b_map.find(key) != b_map.end()) {
      T *data = b_map[key];
      return data;
    }

    return NULL;
  }

  void set_recalc(const BL::ID &id)
  {
    b_recalc.insert(id.ptr.data);
  }

  void set_recalc(void *id_ptr)
  {
    b_recalc.insert(id_ptr);
  }

  bool check_recalc(const BL::ID &id)
  {
    return id.ptr.data && b_recalc.find(id.ptr.data) != b_recalc.end();
  }

  bool has_recalc()
  {
    return !(b_recalc.empty());
  }

  void pre_sync()
  {
    used_set.clear();
  }

  /* Add new data. */
  void add(const K &key, T *data)
  {
    assert(find(key) == NULL);
    b_map[key] = data;
    used(data);
  }

  /* Update existing data. */
  bool update(T *data, const BL::ID &id)
  {
    return update(data, id, id);
  }
  bool update(T *data, const BL::ID &id, const BL::ID &parent)
  {
    bool recalc = (b_recalc.find(id.ptr.data) != b_recalc.end());
    if (parent.ptr.data && parent.ptr.data != id.ptr.data) {
      recalc = recalc || (b_recalc.find(parent.ptr.data) != b_recalc.end());
    }
    used(data);
    return recalc;
  }

  /* Combined add and update as needed. */
  bool add_or_update(T **r_data, const BL::ID &id)
  {
    return add_or_update(r_data, id, id, id.ptr.owner_id);
  }
  bool add_or_update(T **r_data, const BL::ID &id, const K &key)
  {
    return add_or_update(r_data, id, id, key);
  }
  bool add_or_update(T **r_data, const BL::ID &id, const BL::ID &parent, const K &key)
  {
    T *data = find(key);
    bool recalc;

    if (!data) {
      /* Add data if it didn't exist yet. */
      data = scene->create_node<T>();
      add(key, data);
      recalc = true;
    }
    else {
      /* check if updated needed. */
      recalc = update(data, id, parent);
    }

    *r_data = data;
    return recalc;
  }

  /* Combined add or update for convenience. */

  bool is_used(const K &key)
  {
    T *data = find(key);
    return (data) ? used_set.find(data) != used_set.end() : false;
  }

  void used(T *data)
  {
    /* tag data as still in use */
    used_set.insert(data);
  }

  void set_default(T *data)
  {
    b_map[NULL] = data;
  }

  void post_sync(bool do_delete = true)
  {
    map<K, T *> new_map;
    typedef pair<const K, T *> TMapPair;
    typename map<K, T *>::iterator jt;

    for (jt = b_map.begin(); jt != b_map.end(); jt++) {
      TMapPair &pair = *jt;

      if (do_delete && used_set.find(pair.second) == used_set.end()) {
        flags.erase(pair.second);
        scene->delete_node(pair.second);
      }
      else {
        new_map[pair.first] = pair.second;
      }
    }

    used_set.clear();
    b_recalc.clear();
    b_map = new_map;
  }

  const map<K, T *> &key_to_scene_data()
  {
    return b_map;
  }

  bool test_flag(T *data, Flags val)
  {
    typename map<T *, uint>::iterator it = flags.find(data);
    return it != flags.end() && (it->second & (1 << val)) != 0;
  }

  void set_flag(T *data, Flags val)
  {
    flags[data] |= (1 << val);
  }

  void clear_flag(T *data, Flags val)
  {
    typename map<T *, uint>::iterator it = flags.find(data);
    if (it != flags.end()) {
      it->second &= ~(1 << val);

      if (it->second == 0) {
        flags.erase(it);
      }
    }
  }

 protected:
  map<K, T *> b_map;
  set<T *> used_set;
  map<T *, uint> flags;
  set<void *> b_recalc;
  Scene *scene;
};

/* Object Key
 *
 * To uniquely identify instances, we use the parent, object and persistent instance ID.
 * We also export separate object for a mesh and its particle hair. */

enum { OBJECT_PERSISTENT_ID_SIZE = 8 /* MAX_DUPLI_RECUR in Blender. */ };

struct ObjectKey {
  void *parent;
  int id[OBJECT_PERSISTENT_ID_SIZE];
  void *ob;
  bool use_particle_hair;

  ObjectKey(void *parent_, int id_[OBJECT_PERSISTENT_ID_SIZE], void *ob_, bool use_particle_hair_)
      : parent(parent_), ob(ob_), use_particle_hair(use_particle_hair_)
  {
    if (id_)
      memcpy(id, id_, sizeof(id));
    else
      memset(id, 0, sizeof(id));
  }

  bool operator<(const ObjectKey &k) const
  {
    if (ob < k.ob) {
      return true;
    }
    else if (ob == k.ob) {
      if (parent < k.parent) {
        return true;
      }
      else if (parent == k.parent) {
        if (use_particle_hair < k.use_particle_hair) {
          return true;
        }
        else if (use_particle_hair == k.use_particle_hair) {
          return memcmp(id, k.id, sizeof(id)) < 0;
        }
      }
    }

    return false;
  }
};

/* Geometry Key
 *
 * We export separate geometry for a mesh and its particle hair, so key needs to
 * distinguish between them. */

struct GeometryKey {
  void *id;
  Geometry::Type geometry_type;

  GeometryKey(void *id, Geometry::Type geometry_type) : id(id), geometry_type(geometry_type) {}

  bool operator<(const GeometryKey &k) const
  {
    if (id < k.id) {
      return true;
    }
    else if (id == k.id) {
      if (geometry_type < k.geometry_type) {
        return true;
      }
    }

    return false;
  }
};

/* Particle System Key */

struct ParticleSystemKey {
  void *ob;
  int id[OBJECT_PERSISTENT_ID_SIZE];

  ParticleSystemKey(void *ob_, int id_[OBJECT_PERSISTENT_ID_SIZE]) : ob(ob_)
  {
    if (id_)
      memcpy(id, id_, sizeof(id));
    else
      memset(id, 0, sizeof(id));
  }

  bool operator<(const ParticleSystemKey &k) const
  {
    /* first id is particle index, we don't compare that */
    if (ob < k.ob)
      return true;
    else if (ob == k.ob)
      return memcmp(id + 1, k.id + 1, sizeof(int) * (OBJECT_PERSISTENT_ID_SIZE - 1)) < 0;

    return false;
  }
};

CCL_NAMESPACE_END

#endif /* __BLENDER_ID_MAP_H__ */
