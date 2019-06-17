/*
 * Copyright 2011-2013 Blender Foundation
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

#ifndef __BLENDER_UTIL_H__
#define __BLENDER_UTIL_H__

#include "render/mesh.h"

#include "util/util_algorithm.h"
#include "util/util_array.h"
#include "util/util_map.h"
#include "util/util_path.h"
#include "util/util_set.h"
#include "util/util_transform.h"
#include "util/util_types.h"
#include "util/util_vector.h"

/* Hacks to hook into Blender API
 * todo: clean this up ... */

extern "C" {
void BKE_image_user_frame_calc(void *ima, void *iuser, int cfra);
void BKE_image_user_file_path(void *iuser, void *ima, char *path);
unsigned char *BKE_image_get_pixels_for_frame(void *image, int frame);
float *BKE_image_get_float_pixels_for_frame(void *image, int frame);
}

CCL_NAMESPACE_BEGIN

void python_thread_state_save(void **python_thread_state);
void python_thread_state_restore(void **python_thread_state);

static inline BL::Mesh object_to_mesh(BL::BlendData & /*data*/,
                                      BL::Object &object,
                                      BL::Depsgraph & /*depsgraph*/,
                                      bool /*calc_undeformed*/,
                                      Mesh::SubdivisionType subdivision_type)
{
  /* TODO: make this work with copy-on-write, modifiers are already evaluated. */
#if 0
  bool subsurf_mod_show_render = false;
  bool subsurf_mod_show_viewport = false;

  if (subdivision_type != Mesh::SUBDIVISION_NONE) {
    BL::Modifier subsurf_mod = object.modifiers[object.modifiers.length() - 1];

    subsurf_mod_show_render = subsurf_mod.show_render();
    subsurf_mod_show_viewport = subsurf_mod.show_viewport();

    subsurf_mod.show_render(false);
    subsurf_mod.show_viewport(false);
  }
#endif

  BL::Mesh mesh(PointerRNA_NULL);
  if (object.type() == BL::Object::type_MESH) {
    /* TODO: calc_undeformed is not used. */
    mesh = BL::Mesh(object.data());

    /* Make a copy to split faces if we use autosmooth, otherwise not needed.
     * Also in edit mode do we need to make a copy, to ensure data layers like
     * UV are not empty. */
    if (mesh.is_editmode() ||
        (mesh.use_auto_smooth() && subdivision_type == Mesh::SUBDIVISION_NONE)) {
      BL::Depsgraph depsgraph(PointerRNA_NULL);
      mesh = object.to_mesh(false, depsgraph);
    }
  }
  else {
    BL::Depsgraph depsgraph(PointerRNA_NULL);
    mesh = object.to_mesh(false, depsgraph);
  }

#if 0
  if (subdivision_type != Mesh::SUBDIVISION_NONE) {
    BL::Modifier subsurf_mod = object.modifiers[object.modifiers.length() - 1];

    subsurf_mod.show_render(subsurf_mod_show_render);
    subsurf_mod.show_viewport(subsurf_mod_show_viewport);
  }
#endif

  if ((bool)mesh && subdivision_type == Mesh::SUBDIVISION_NONE) {
    if (mesh.use_auto_smooth()) {
      mesh.split_faces(false);
    }

    mesh.calc_loop_triangles();
  }

  return mesh;
}

static inline void free_object_to_mesh(BL::BlendData & /*data*/,
                                       BL::Object &object,
                                       BL::Mesh &mesh)
{
  /* Free mesh if we didn't just use the existing one. */
  if (object.data().ptr.data != mesh.ptr.data) {
    object.to_mesh_clear();
  }
}

static inline void colorramp_to_array(BL::ColorRamp &ramp,
                                      array<float3> &ramp_color,
                                      array<float> &ramp_alpha,
                                      int size)
{
  ramp_color.resize(size);
  ramp_alpha.resize(size);

  for (int i = 0; i < size; i++) {
    float color[4];

    ramp.evaluate((float)i / (float)(size - 1), color);
    ramp_color[i] = make_float3(color[0], color[1], color[2]);
    ramp_alpha[i] = color[3];
  }
}

static inline void curvemap_minmax_curve(/*const*/ BL::CurveMap &curve, float *min_x, float *max_x)
{
  *min_x = min(*min_x, curve.points[0].location()[0]);
  *max_x = max(*max_x, curve.points[curve.points.length() - 1].location()[0]);
}

static inline void curvemapping_minmax(/*const*/ BL::CurveMapping &cumap,
                                       bool rgb_curve,
                                       float *min_x,
                                       float *max_x)
{
  /* const int num_curves = cumap.curves.length(); */ /* Gives linking error so far. */
  const int num_curves = rgb_curve ? 4 : 3;
  *min_x = FLT_MAX;
  *max_x = -FLT_MAX;
  for (int i = 0; i < num_curves; ++i) {
    BL::CurveMap map(cumap.curves[i]);
    curvemap_minmax_curve(map, min_x, max_x);
  }
}

static inline void curvemapping_to_array(BL::CurveMapping &cumap, array<float> &data, int size)
{
  cumap.update();
  BL::CurveMap curve = cumap.curves[0];
  data.resize(size);
  for (int i = 0; i < size; i++) {
    float t = (float)i / (float)(size - 1);
    data[i] = curve.evaluate(t);
  }
}

static inline void curvemapping_color_to_array(BL::CurveMapping &cumap,
                                               array<float3> &data,
                                               int size,
                                               bool rgb_curve)
{
  float min_x = 0.0f, max_x = 1.0f;

  /* TODO(sergey): There is no easy way to automatically guess what is
   * the range to be used here for the case when mapping is applied on
   * top of another mapping (i.e. R curve applied on top of common
   * one).
   *
   * Using largest possible range form all curves works correct for the
   * cases like vector curves and should be good enough heuristic for
   * the color curves as well.
   *
   * There might be some better estimations here tho.
   */
  curvemapping_minmax(cumap, rgb_curve, &min_x, &max_x);

  const float range_x = max_x - min_x;

  cumap.update();

  BL::CurveMap mapR = cumap.curves[0];
  BL::CurveMap mapG = cumap.curves[1];
  BL::CurveMap mapB = cumap.curves[2];

  data.resize(size);

  if (rgb_curve) {
    BL::CurveMap mapI = cumap.curves[3];
    for (int i = 0; i < size; i++) {
      const float t = min_x + (float)i / (float)(size - 1) * range_x;
      data[i] = make_float3(mapR.evaluate(mapI.evaluate(t)),
                            mapG.evaluate(mapI.evaluate(t)),
                            mapB.evaluate(mapI.evaluate(t)));
    }
  }
  else {
    for (int i = 0; i < size; i++) {
      float t = min_x + (float)i / (float)(size - 1) * range_x;
      data[i] = make_float3(mapR.evaluate(t), mapG.evaluate(t), mapB.evaluate(t));
    }
  }
}

static inline bool BKE_object_is_modified(BL::Object &self, BL::Scene &scene, bool preview)
{
  return self.is_modified(scene, (preview) ? (1 << 0) : (1 << 1)) ? true : false;
}

static inline bool BKE_object_is_deform_modified(BL::Object &self, BL::Scene &scene, bool preview)
{
  return self.is_deform_modified(scene, (preview) ? (1 << 0) : (1 << 1)) ? true : false;
}

static inline int render_resolution_x(BL::RenderSettings &b_render)
{
  return b_render.resolution_x() * b_render.resolution_percentage() / 100;
}

static inline int render_resolution_y(BL::RenderSettings &b_render)
{
  return b_render.resolution_y() * b_render.resolution_percentage() / 100;
}

static inline string image_user_file_path(BL::ImageUser &iuser, BL::Image &ima, int cfra)
{
  char filepath[1024];
  BKE_image_user_frame_calc(NULL, iuser.ptr.data, cfra);
  BKE_image_user_file_path(iuser.ptr.data, ima.ptr.data, filepath);
  return string(filepath);
}

static inline int image_user_frame_number(BL::ImageUser &iuser, int cfra)
{
  BKE_image_user_frame_calc(NULL, iuser.ptr.data, cfra);
  return iuser.frame_current();
}

static inline unsigned char *image_get_pixels_for_frame(BL::Image &image, int frame)
{
  return BKE_image_get_pixels_for_frame(image.ptr.data, frame);
}

static inline float *image_get_float_pixels_for_frame(BL::Image &image, int frame)
{
  return BKE_image_get_float_pixels_for_frame(image.ptr.data, frame);
}

static inline void render_add_metadata(BL::RenderResult &b_rr, string name, string value)
{
  b_rr.stamp_data_add_field(name.c_str(), value.c_str());
}

/* Utilities */

static inline Transform get_transform(const BL::Array<float, 16> &array)
{
  ProjectionTransform projection;

  /* We assume both types to be just 16 floats, and transpose because blender
   * use column major matrix order while we use row major. */
  memcpy((void *)&projection, &array, sizeof(float) * 16);
  projection = projection_transpose(projection);

  /* Drop last row, matrix is assumed to be affine transform. */
  return projection_to_transform(projection);
}

static inline float2 get_float2(const BL::Array<float, 2> &array)
{
  return make_float2(array[0], array[1]);
}

static inline float3 get_float3(const BL::Array<float, 2> &array)
{
  return make_float3(array[0], array[1], 0.0f);
}

static inline float3 get_float3(const BL::Array<float, 3> &array)
{
  return make_float3(array[0], array[1], array[2]);
}

static inline float3 get_float3(const BL::Array<float, 4> &array)
{
  return make_float3(array[0], array[1], array[2]);
}

static inline float4 get_float4(const BL::Array<float, 4> &array)
{
  return make_float4(array[0], array[1], array[2], array[3]);
}

static inline int3 get_int3(const BL::Array<int, 3> &array)
{
  return make_int3(array[0], array[1], array[2]);
}

static inline int4 get_int4(const BL::Array<int, 4> &array)
{
  return make_int4(array[0], array[1], array[2], array[3]);
}

static inline float3 get_float3(PointerRNA &ptr, const char *name)
{
  float3 f;
  RNA_float_get_array(&ptr, name, &f.x);
  return f;
}

static inline void set_float3(PointerRNA &ptr, const char *name, float3 value)
{
  RNA_float_set_array(&ptr, name, &value.x);
}

static inline float4 get_float4(PointerRNA &ptr, const char *name)
{
  float4 f;
  RNA_float_get_array(&ptr, name, &f.x);
  return f;
}

static inline void set_float4(PointerRNA &ptr, const char *name, float4 value)
{
  RNA_float_set_array(&ptr, name, &value.x);
}

static inline bool get_boolean(PointerRNA &ptr, const char *name)
{
  return RNA_boolean_get(&ptr, name) ? true : false;
}

static inline void set_boolean(PointerRNA &ptr, const char *name, bool value)
{
  RNA_boolean_set(&ptr, name, (int)value);
}

static inline float get_float(PointerRNA &ptr, const char *name)
{
  return RNA_float_get(&ptr, name);
}

static inline void set_float(PointerRNA &ptr, const char *name, float value)
{
  RNA_float_set(&ptr, name, value);
}

static inline int get_int(PointerRNA &ptr, const char *name)
{
  return RNA_int_get(&ptr, name);
}

static inline void set_int(PointerRNA &ptr, const char *name, int value)
{
  RNA_int_set(&ptr, name, value);
}

/* Get a RNA enum value with sanity check: if the RNA value is above num_values
 * the function will return a fallback default value.
 *
 * NOTE: This function assumes that RNA enum values are a continuous sequence
 * from 0 to num_values-1. Be careful to use it with enums where some values are
 * deprecated!
 */
static inline int get_enum(PointerRNA &ptr,
                           const char *name,
                           int num_values = -1,
                           int default_value = -1)
{
  int value = RNA_enum_get(&ptr, name);
  if (num_values != -1 && value >= num_values) {
    assert(default_value != -1);
    value = default_value;
  }
  return value;
}

static inline string get_enum_identifier(PointerRNA &ptr, const char *name)
{
  PropertyRNA *prop = RNA_struct_find_property(&ptr, name);
  const char *identifier = "";
  int value = RNA_property_enum_get(&ptr, prop);

  RNA_property_enum_identifier(NULL, &ptr, prop, value, &identifier);

  return string(identifier);
}

static inline void set_enum(PointerRNA &ptr, const char *name, int value)
{
  RNA_enum_set(&ptr, name, value);
}

static inline void set_enum(PointerRNA &ptr, const char *name, const string &identifier)
{
  RNA_enum_set_identifier(NULL, &ptr, name, identifier.c_str());
}

static inline string get_string(PointerRNA &ptr, const char *name)
{
  char cstrbuf[1024];
  char *cstr = RNA_string_get_alloc(&ptr, name, cstrbuf, sizeof(cstrbuf));
  string str(cstr);
  if (cstr != cstrbuf)
    MEM_freeN(cstr);

  return str;
}

static inline void set_string(PointerRNA &ptr, const char *name, const string &value)
{
  RNA_string_set(&ptr, name, value.c_str());
}

/* Relative Paths */

static inline string blender_absolute_path(BL::BlendData &b_data, BL::ID &b_id, const string &path)
{
  if (path.size() >= 2 && path[0] == '/' && path[1] == '/') {
    string dirname;

    if (b_id.library()) {
      BL::ID b_library_id(b_id.library());
      dirname = blender_absolute_path(b_data, b_library_id, b_id.library().filepath());
    }
    else
      dirname = b_data.filepath();

    return path_join(path_dirname(dirname), path.substr(2));
  }

  return path;
}

static inline string get_text_datablock_content(const PointerRNA &ptr)
{
  if (ptr.data == NULL) {
    return "";
  }

  string content;
  BL::Text::lines_iterator iter;
  for (iter.begin(ptr); iter; ++iter) {
    content += iter->body() + "\n";
  }

  return content;
}

/* Texture Space */

static inline void mesh_texture_space(BL::Mesh &b_mesh, float3 &loc, float3 &size)
{
  loc = get_float3(b_mesh.texspace_location());
  size = get_float3(b_mesh.texspace_size());

  if (size.x != 0.0f)
    size.x = 0.5f / size.x;
  if (size.y != 0.0f)
    size.y = 0.5f / size.y;
  if (size.z != 0.0f)
    size.z = 0.5f / size.z;

  loc = loc * size - make_float3(0.5f, 0.5f, 0.5f);
}

/* Object motion steps, returns 0 if no motion blur needed. */
static inline uint object_motion_steps(BL::Object &b_parent, BL::Object &b_ob)
{
  /* Get motion enabled and steps from object itself. */
  PointerRNA cobject = RNA_pointer_get(&b_ob.ptr, "cycles");
  bool use_motion = get_boolean(cobject, "use_motion_blur");
  if (!use_motion) {
    return 0;
  }

  uint steps = max(1, get_int(cobject, "motion_steps"));

  /* Also check parent object, so motion blur and steps can be
   * controlled by dupligroup duplicator for linked groups. */
  if (b_parent.ptr.data != b_ob.ptr.data) {
    PointerRNA parent_cobject = RNA_pointer_get(&b_parent.ptr, "cycles");
    use_motion &= get_boolean(parent_cobject, "use_motion_blur");

    if (!use_motion) {
      return 0;
    }

    steps = max(steps, get_int(parent_cobject, "motion_steps"));
  }

  /* Use uneven number of steps so we get one keyframe at the current frame,
   * and use 2^(steps - 1) so objects with more/fewer steps still have samples
   * at the same times, to avoid sampling at many different times. */
  return (2 << (steps - 1)) + 1;
}

/* object uses deformation motion blur */
static inline bool object_use_deform_motion(BL::Object &b_parent, BL::Object &b_ob)
{
  PointerRNA cobject = RNA_pointer_get(&b_ob.ptr, "cycles");
  bool use_deform_motion = get_boolean(cobject, "use_deform_motion");
  /* If motion blur is enabled for the object we also check
   * whether it's enabled for the parent object as well.
   *
   * This way we can control motion blur from the dupligroup
   * duplicator much easier.
   */
  if (use_deform_motion && b_parent.ptr.data != b_ob.ptr.data) {
    PointerRNA parent_cobject = RNA_pointer_get(&b_parent.ptr, "cycles");
    use_deform_motion &= get_boolean(parent_cobject, "use_deform_motion");
  }
  return use_deform_motion;
}

static inline BL::SmokeDomainSettings object_smoke_domain_find(BL::Object &b_ob)
{
  BL::Object::modifiers_iterator b_mod;

  for (b_ob.modifiers.begin(b_mod); b_mod != b_ob.modifiers.end(); ++b_mod) {
    if (b_mod->is_a(&RNA_SmokeModifier)) {
      BL::SmokeModifier b_smd(*b_mod);

      if (b_smd.smoke_type() == BL::SmokeModifier::smoke_type_DOMAIN)
        return b_smd.domain_settings();
    }
  }

  return BL::SmokeDomainSettings(PointerRNA_NULL);
}

static inline BL::DomainFluidSettings object_fluid_domain_find(BL::Object b_ob)
{
  BL::Object::modifiers_iterator b_mod;

  for (b_ob.modifiers.begin(b_mod); b_mod != b_ob.modifiers.end(); ++b_mod) {
    if (b_mod->is_a(&RNA_FluidSimulationModifier)) {
      BL::FluidSimulationModifier b_fmd(*b_mod);
      BL::FluidSettings fss = b_fmd.settings();

      if (fss.type() == BL::FluidSettings::type_DOMAIN)
        return (BL::DomainFluidSettings)b_fmd.settings();
    }
  }

  return BL::DomainFluidSettings(PointerRNA_NULL);
}

static inline Mesh::SubdivisionType object_subdivision_type(BL::Object &b_ob,
                                                            bool preview,
                                                            bool experimental)
{
  PointerRNA cobj = RNA_pointer_get(&b_ob.ptr, "cycles");

  if (cobj.data && b_ob.modifiers.length() > 0 && experimental) {
    BL::Modifier mod = b_ob.modifiers[b_ob.modifiers.length() - 1];
    bool enabled = preview ? mod.show_viewport() : mod.show_render();

    if (enabled && mod.type() == BL::Modifier::type_SUBSURF &&
        RNA_boolean_get(&cobj, "use_adaptive_subdivision")) {
      BL::SubsurfModifier subsurf(mod);

      if (subsurf.subdivision_type() == BL::SubsurfModifier::subdivision_type_CATMULL_CLARK) {
        return Mesh::SUBDIVISION_CATMULL_CLARK;
      }
      else {
        return Mesh::SUBDIVISION_LINEAR;
      }
    }
  }

  return Mesh::SUBDIVISION_NONE;
}

/* ID Map
 *
 * Utility class to keep in sync with blender data.
 * Used for objects, meshes, lights and shaders. */

template<typename K, typename T> class id_map {
 public:
  id_map(vector<T *> *scene_data_)
  {
    scene_data = scene_data_;
  }

  T *find(const BL::ID &id)
  {
    return find(id.ptr.id.data);
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

  bool has_recalc()
  {
    return !(b_recalc.empty());
  }

  void pre_sync()
  {
    used_set.clear();
  }

  bool sync(T **r_data, const BL::ID &id)
  {
    return sync(r_data, id, id, id.ptr.id.data);
  }

  bool sync(T **r_data, const BL::ID &id, const BL::ID &parent, const K &key)
  {
    T *data = find(key);
    bool recalc;

    if (!data) {
      /* add data if it didn't exist yet */
      data = new T();
      scene_data->push_back(data);
      b_map[key] = data;
      recalc = true;
    }
    else {
      recalc = (b_recalc.find(id.ptr.data) != b_recalc.end());
      if (parent.ptr.data)
        recalc = recalc || (b_recalc.find(parent.ptr.data) != b_recalc.end());
    }

    used(data);

    *r_data = data;
    return recalc;
  }

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

  bool post_sync(bool do_delete = true)
  {
    /* remove unused data */
    vector<T *> new_scene_data;
    typename vector<T *>::iterator it;
    bool deleted = false;

    for (it = scene_data->begin(); it != scene_data->end(); it++) {
      T *data = *it;

      if (do_delete && used_set.find(data) == used_set.end()) {
        delete data;
        deleted = true;
      }
      else
        new_scene_data.push_back(data);
    }

    *scene_data = new_scene_data;

    /* update mapping */
    map<K, T *> new_map;
    typedef pair<const K, T *> TMapPair;
    typename map<K, T *>::iterator jt;

    for (jt = b_map.begin(); jt != b_map.end(); jt++) {
      TMapPair &pair = *jt;

      if (used_set.find(pair.second) != used_set.end())
        new_map[pair.first] = pair.second;
    }

    used_set.clear();
    b_recalc.clear();
    b_map = new_map;

    return deleted;
  }

  const map<K, T *> &key_to_scene_data()
  {
    return b_map;
  }

 protected:
  vector<T *> *scene_data;
  map<K, T *> b_map;
  set<T *> used_set;
  set<void *> b_recalc;
};

/* Object Key */

enum { OBJECT_PERSISTENT_ID_SIZE = 16 };

struct ObjectKey {
  void *parent;
  int id[OBJECT_PERSISTENT_ID_SIZE];
  void *ob;

  ObjectKey(void *parent_, int id_[OBJECT_PERSISTENT_ID_SIZE], void *ob_)
      : parent(parent_), ob(ob_)
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
      if (parent < k.parent)
        return true;
      else if (parent == k.parent)
        return memcmp(id, k.id, sizeof(id)) < 0;
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

class EdgeMap {
 public:
  EdgeMap()
  {
  }

  void clear()
  {
    edges_.clear();
  }

  void insert(int v0, int v1)
  {
    get_sorted_verts(v0, v1);
    edges_.insert(std::pair<int, int>(v0, v1));
  }

  bool exists(int v0, int v1)
  {
    get_sorted_verts(v0, v1);
    return edges_.find(std::pair<int, int>(v0, v1)) != edges_.end();
  }

 protected:
  void get_sorted_verts(int &v0, int &v1)
  {
    if (v0 > v1) {
      swap(v0, v1);
    }
  }

  set<std::pair<int, int>> edges_;
};

CCL_NAMESPACE_END

#endif /* __BLENDER_UTIL_H__ */
