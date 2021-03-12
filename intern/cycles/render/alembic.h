/*
 * Copyright 2011-2018 Blender Foundation
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

#pragma once

#include "graph/node.h"
#include "render/attribute.h"
#include "render/procedural.h"
#include "util/util_set.h"
#include "util/util_transform.h"
#include "util/util_vector.h"

#ifdef WITH_ALEMBIC

#  include <Alembic/AbcCoreFactory/All.h>
#  include <Alembic/AbcGeom/All.h>

CCL_NAMESPACE_BEGIN

class AlembicProcedural;
class Geometry;
class Object;
class Progress;
class Shader;

using MatrixSampleMap = std::map<Alembic::Abc::chrono_t, Alembic::Abc::M44d>;

struct MatrixSamplesData {
  MatrixSampleMap *samples = nullptr;
  Alembic::AbcCoreAbstract::TimeSamplingPtr time_sampling;
};

/* Helpers to detect if some type is a ccl::array. */
template<typename> struct is_array : public std::false_type {
};

template<typename T> struct is_array<array<T>> : public std::true_type {
};

/* Holds the data for a cache lookup at a given time, as well as informations to
 * help disambiguate successes or failures to get data from the cache. */
template<typename T> class CacheLookupResult {
  enum class State {
    NEW_DATA,
    ALREADY_LOADED,
    NO_DATA_FOR_TIME,
  };

  T *data;
  State state;

 protected:
  /* Prevent default construction outside of the class: for a valid result, we
   * should use the static functions below. */
  CacheLookupResult() = default;

 public:
  static CacheLookupResult new_data(T *data_)
  {
    CacheLookupResult result;
    result.data = data_;
    result.state = State::NEW_DATA;
    return result;
  }

  static CacheLookupResult no_data_found_for_time()
  {
    CacheLookupResult result;
    result.data = nullptr;
    result.state = State::NO_DATA_FOR_TIME;
    return result;
  }

  static CacheLookupResult already_loaded()
  {
    CacheLookupResult result;
    result.data = nullptr;
    result.state = State::ALREADY_LOADED;
    return result;
  }

  /* This should only be call if new data is available. */
  const T &get_data() const
  {
    assert(state == State::NEW_DATA);
    assert(data != nullptr);
    return *data;
  }

  T *get_data_or_null() const
  {
    // data_ should already be null if there is no new data so no need to check
    return data;
  }

  bool has_new_data() const
  {
    return state == State::NEW_DATA;
  }

  bool has_already_loaded() const
  {
    return state == State::ALREADY_LOADED;
  }

  bool has_no_data_for_time() const
  {
    return state == State::NO_DATA_FOR_TIME;
  }
};

/* Store the data set for an animation at every time points, or at the beginning of the animation
 * for constant data.
 *
 * The data is supposed to be stored in chronological order, and is looked up using the current
 * animation time in seconds using the TimeSampling from the Alembic property. */
template<typename T> class DataStore {
  struct DataTimePair {
    double time = 0;
    T data{};
  };

  vector<DataTimePair> data{};
  Alembic::AbcCoreAbstract::TimeSampling time_sampling{};

  double last_loaded_time = std::numeric_limits<double>::max();

 public:
  void set_time_sampling(Alembic::AbcCoreAbstract::TimeSampling time_sampling_)
  {
    time_sampling = time_sampling_;
  }

  Alembic::AbcCoreAbstract::TimeSampling get_time_sampling() const
  {
    return time_sampling;
  }

  /* Get the data for the specified time.
   * Return nullptr if there is no data or if the data for this time was already loaded. */
  CacheLookupResult<T> data_for_time(double time)
  {
    if (size() == 0) {
      return CacheLookupResult<T>::no_data_found_for_time();
    }

    std::pair<size_t, Alembic::Abc::chrono_t> index_pair;
    index_pair = time_sampling.getNearIndex(time, data.size());
    DataTimePair &data_pair = data[index_pair.first];

    if (last_loaded_time == data_pair.time) {
      return CacheLookupResult<T>::already_loaded();
    }

    last_loaded_time = data_pair.time;

    return CacheLookupResult<T>::new_data(&data_pair.data);
  }

  /* get the data for the specified time, but do not check if the data was already loaded for this
   * time return nullptr if there is no data */
  CacheLookupResult<T> data_for_time_no_check(double time)
  {
    if (size() == 0) {
      return CacheLookupResult<T>::no_data_found_for_time();
    }

    std::pair<size_t, Alembic::Abc::chrono_t> index_pair;
    index_pair = time_sampling.getNearIndex(time, data.size());
    DataTimePair &data_pair = data[index_pair.first];
    return CacheLookupResult<T>::new_data(&data_pair.data);
  }

  void add_data(T &data_, double time)
  {
    if constexpr (is_array<T>::value) {
      data.emplace_back();
      data.back().data.steal_data(data_);
      data.back().time = time;
      return;
    }

    data.push_back({time, data_});
  }

  bool is_constant() const
  {
    return data.size() <= 1;
  }

  size_t size() const
  {
    return data.size();
  }

  void clear()
  {
    invalidate_last_loaded_time();
    data.clear();
  }

  void invalidate_last_loaded_time()
  {
    last_loaded_time = std::numeric_limits<double>::max();
  }

  /* Copy the data for the specified time to the node's socket. If there is no
   * data for this time or it was already loaded, do nothing. */
  void copy_to_socket(double time, Node *node, const SocketType *socket)
  {
    CacheLookupResult<T> result = data_for_time(time);

    if (!result.has_new_data()) {
      return;
    }

    /* TODO(kevindietrich): arrays are emptied when passed to the sockets, so for now we copy the
     * arrays to avoid reloading the data */
    T value = result.get_data();
    node->set(*socket, value);
  }
};

/* Actual cache for the stored data.
 * This caches the topological, transformation, and attribute data for a Mesh node or a Hair node
 * inside of DataStores.
 */
struct CachedData {
  DataStore<Transform> transforms{};

  /* mesh data */
  DataStore<array<float3>> vertices;
  DataStore<array<int3>> triangles{};
  /* triangle "loops" are the polygons' vertices indices used for indexing face varying attributes
   * (like UVs) */
  DataStore<array<int3>> triangles_loops{};
  DataStore<array<int>> shader{};

  /* subd data */
  DataStore<array<int>> subd_start_corner;
  DataStore<array<int>> subd_num_corners;
  DataStore<array<bool>> subd_smooth;
  DataStore<array<int>> subd_ptex_offset;
  DataStore<array<int>> subd_face_corners;
  DataStore<int> num_ngons;
  DataStore<array<int>> subd_creases_edge;
  DataStore<array<float>> subd_creases_weight;

  /* hair data */
  DataStore<array<float3>> curve_keys;
  DataStore<array<float>> curve_radius;
  DataStore<array<int>> curve_first_key;
  DataStore<array<int>> curve_shader;

  struct CachedAttribute {
    AttributeStandard std;
    AttributeElement element;
    TypeDesc type_desc;
    ustring name;
    DataStore<array<char>> data{};
  };

  vector<CachedAttribute> attributes{};

  void clear();

  CachedAttribute &add_attribute(const ustring &name,
                                 const Alembic::Abc::TimeSampling &time_sampling);

  bool is_constant() const;

  void invalidate_last_loaded_time(bool attributes_only = false);

  void set_time_sampling(Alembic::AbcCoreAbstract::TimeSampling time_sampling);
};

/* Representation of an Alembic object for the AlembicProcedural.
 *
 * The AlembicObject holds the path to the Alembic IObject inside of the archive that is desired
 * for rendering, as well as the list of shaders that it is using.
 *
 * The names of the shaders should correspond to the names of the FaceSets inside of the Alembic
 * archive for per-triangle shader association. If there is no FaceSets, or the names do not
 * match, the first shader is used for rendering for all triangles.
 */
class AlembicObject : public Node {
 public:
  NODE_DECLARE

  /* Path to the IObject inside of the archive. */
  NODE_SOCKET_API(ustring, path)

  /* Shaders used for rendering. */
  NODE_SOCKET_API_ARRAY(array<Node *>, used_shaders)

  /* Maximum number of subdivisions for ISubD objects. */
  NODE_SOCKET_API(int, subd_max_level)

  /* Finest level of detail (in pixels) for the subdivision. */
  NODE_SOCKET_API(float, subd_dicing_rate)

  /* Scale the radius of points and curves. */
  NODE_SOCKET_API(float, radius_scale)

  AlembicObject();
  ~AlembicObject();

 private:
  friend class AlembicProcedural;

  void set_object(Object *object);
  Object *get_object();

  void load_all_data(AlembicProcedural *proc,
                     Alembic::AbcGeom::IPolyMeshSchema &schema,
                     Progress &progress);
  void load_all_data(AlembicProcedural *proc,
                     Alembic::AbcGeom::ISubDSchema &schema,
                     Progress &progress);
  void load_all_data(AlembicProcedural *proc,
                     const Alembic::AbcGeom::ICurvesSchema &schema,
                     Progress &progress,
                     float default_radius);

  bool has_data_loaded() const;

  /* Enumeration used to speed up the discrimination of an IObject as IObject::matches() methods
   * are too expensive and show up in profiles. */
  enum AbcSchemaType {
    INVALID,
    POLY_MESH,
    SUBD,
    CURVES,
  };

  bool need_shader_update = true;

  AlembicObject *instance_of = nullptr;

  Alembic::AbcCoreAbstract::TimeSamplingPtr xform_time_sampling;
  MatrixSampleMap xform_samples;
  Alembic::AbcGeom::IObject iobject;

  /* Set if the path points to a valid IObject whose type is supported. */
  AbcSchemaType schema_type;

  CachedData &get_cached_data()
  {
    return cached_data;
  }

  bool is_constant() const
  {
    return cached_data.is_constant();
  }

  Object *object = nullptr;

  bool data_loaded = false;

  CachedData cached_data;

  void update_shader_attributes(const Alembic::AbcGeom::ICompoundProperty &arb_geom_params,
                                Progress &progress);

  void read_attribute(const Alembic::AbcGeom::ICompoundProperty &arb_geom_params,
                      const ustring &attr_name,
                      Progress &progress);

  template<typename SchemaType>
  void read_face_sets(SchemaType &schema,
                      array<int> &polygon_to_shader,
                      Alembic::AbcGeom::ISampleSelector sample_sel);

  void setup_transform_cache(float scale);

  AttributeRequestSet get_requested_attributes();
};

/* Procedural to render objects from a single Alembic archive.
 *
 * Every object desired to be rendered should be passed as an AlembicObject through the objects
 * socket.
 *
 * This procedural will load the data set for the entire animation in memory on the first frame,
 * and directly set the data for the new frames on the created Nodes if needed. This allows for
 * faster updates between frames as it avoids reseeking the data on disk.
 */
class AlembicProcedural : public Procedural {
  Alembic::AbcGeom::IArchive archive;
  bool objects_loaded;
  Scene *scene_;

 public:
  NODE_DECLARE

  /* The file path to the Alembic archive */
  NODE_SOCKET_API(ustring, filepath)

  /* The current frame to render. */
  NODE_SOCKET_API(float, frame)

  /* The first frame to load data for. */
  NODE_SOCKET_API(float, start_frame)

  /* The last frame to load data for. */
  NODE_SOCKET_API(float, end_frame)

  /* Subtracted to the current frame. */
  NODE_SOCKET_API(float, frame_offset)

  /* The frame rate used for rendering in units of frames per second. */
  NODE_SOCKET_API(float, frame_rate)

  /* List of AlembicObjects to render. */
  NODE_SOCKET_API_ARRAY(array<Node *>, objects)

  /* Set the default radius to use for curves when the Alembic Curves Schemas do not have radius
   * information. */
  NODE_SOCKET_API(float, default_radius)

  /* Multiplier to account for differences in default units for measuring objects in various
   * software. */
  NODE_SOCKET_API(float, scale)

  AlembicProcedural();
  ~AlembicProcedural();

  /* Populates the Cycles scene with Nodes for every contained AlembicObject on the first
   * invocation, and updates the data on subsequent invocations if the frame changed. */
  void generate(Scene *scene, Progress &progress);

  /* Add an object to our list of objects, and tag the socket as modified. */
  void add_object(AlembicObject *object);

  /* Tag for an update only if something was modified. */
  void tag_update(Scene *scene);

  /* Returns a pointer to an existing or a newly created AlembicObject for the given path. */
  AlembicObject *get_or_create_object(const ustring &path);

 private:
  /* Load the data for all the objects whose data has not yet been loaded. */
  void load_objects(Progress &progress);

  /* Traverse the Alembic hierarchy to lookup the IObjects for the AlembicObjects that were
   * specified in our objects socket, and accumulate all of the transformations samples along the
   * way for each IObject. */
  void walk_hierarchy(Alembic::AbcGeom::IObject parent,
                      const Alembic::AbcGeom::ObjectHeader &ohead,
                      MatrixSamplesData matrix_samples_data,
                      const unordered_map<string, AlembicObject *> &object_map,
                      Progress &progress);

  /* Read the data for an IPolyMesh at the specified frame_time. Creates corresponding Geometry and
   * Object Nodes in the Cycles scene if none exist yet. */
  void read_mesh(AlembicObject *abc_object, Alembic::AbcGeom::Abc::chrono_t frame_time);

  /* Read the data for an ICurves at the specified frame_time. Creates corresponding Geometry and
   * Object Nodes in the Cycles scene if none exist yet. */
  void read_curves(AlembicObject *abc_object, Alembic::AbcGeom::Abc::chrono_t frame_time);

  /* Read the data for an ISubD at the specified frame_time. Creates corresponding Geometry and
   * Object Nodes in the Cycles scene if none exist yet. */
  void read_subd(AlembicObject *abc_object, Alembic::AbcGeom::Abc::chrono_t frame_time);

  void build_caches(Progress &progress);
};

CCL_NAMESPACE_END

#endif
