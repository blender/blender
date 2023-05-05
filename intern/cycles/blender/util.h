/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __BLENDER_UTIL_H__
#define __BLENDER_UTIL_H__

#include "scene/mesh.h"
#include "scene/scene.h"

#include "util/algorithm.h"
#include "util/array.h"
#include "util/map.h"
#include "util/path.h"
#include "util/set.h"
#include "util/transform.h"
#include "util/types.h"
#include "util/vector.h"

/* Hacks to hook into Blender API
 * todo: clean this up ... */

extern "C" {
void BKE_image_user_frame_calc(void *ima, void *iuser, int cfra);
void BKE_image_user_file_path_ex(void *bmain,
                                 void *iuser,
                                 void *ima,
                                 char *filepath,
                                 bool resolve_udim,
                                 bool resolve_multiview);
unsigned char *BKE_image_get_pixels_for_frame(void *image, int frame, int tile);
float *BKE_image_get_float_pixels_for_frame(void *image, int frame, int tile);
}

CCL_NAMESPACE_BEGIN

struct BObjectInfo {
  /* Object directly provided by the depsgraph iterator. This object is only valid during one
   * iteration and must not be accessed afterwards. Transforms and visibility should be checked on
   * this object. */
  BL::Object iter_object;

  /* This object remains alive even after the object iterator is done. It corresponds to one
   * original object. It is the object that owns the object data below. */
  BL::Object real_object;

  /* The object-data referenced by the iter object. This is still valid after the depsgraph
   * iterator is done. It might have a different type compared to real_object.data(). */
  BL::ID object_data;

  /* True when the current geometry is the data of the referenced object. False when it is a
   * geometry instance that does not have a 1-to-1 relationship with an object. */
  bool is_real_object_data() const
  {
    return const_cast<BL::Object &>(real_object).data() == object_data;
  }
};

typedef BL::ShaderNodeAttribute::attribute_type_enum BlenderAttributeType;
BlenderAttributeType blender_attribute_name_split_type(ustring name, string *r_real_name);

void python_thread_state_save(void **python_thread_state);
void python_thread_state_restore(void **python_thread_state);

static inline BL::Mesh object_to_mesh(BL::BlendData & /*data*/,
                                      BObjectInfo &b_ob_info,
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

  BL::Mesh mesh = (b_ob_info.object_data.is_a(&RNA_Mesh)) ? BL::Mesh(b_ob_info.object_data) :
                                                            BL::Mesh(PointerRNA_NULL);

  if (b_ob_info.is_real_object_data()) {
    if (mesh) {
      /* Make a copy to split faces if we use autosmooth, otherwise not needed.
       * Also in edit mode do we need to make a copy, to ensure data layers like
       * UV are not empty. */
      if (mesh.is_editmode() ||
          (mesh.use_auto_smooth() && subdivision_type == Mesh::SUBDIVISION_NONE)) {
        BL::Depsgraph depsgraph(PointerRNA_NULL);
        mesh = b_ob_info.real_object.to_mesh(false, depsgraph);
      }
    }
    else {
      BL::Depsgraph depsgraph(PointerRNA_NULL);
      mesh = b_ob_info.real_object.to_mesh(false, depsgraph);
    }
  }
  else {
    /* TODO: what to do about non-mesh geometry instances? */
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
      mesh.calc_normals_split();
      mesh.split_faces(false);
    }

    mesh.calc_loop_triangles();
  }

  return mesh;
}

static inline void free_object_to_mesh(BL::BlendData & /*data*/,
                                       BObjectInfo &b_ob_info,
                                       BL::Mesh &mesh)
{
  if (!b_ob_info.is_real_object_data()) {
    return;
  }
  /* Free mesh if we didn't just use the existing one. */
  BL::Object object = b_ob_info.real_object;
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
                                       int num_curves,
                                       float *min_x,
                                       float *max_x)
{
  // const int num_curves = cumap.curves.length(); /* Gives linking error so far. */
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
    data[i] = cumap.evaluate(curve, t);
  }
}

static inline void curvemapping_float_to_array(BL::CurveMapping &cumap,
                                               array<float> &data,
                                               int size)
{
  float min = 0.0f, max = 1.0f;

  curvemapping_minmax(cumap, 1, &min, &max);

  const float range = max - min;

  cumap.update();

  BL::CurveMap map = cumap.curves[0];

  data.resize(size);

  for (int i = 0; i < size; i++) {
    float t = min + (float)i / (float)(size - 1) * range;
    data[i] = cumap.evaluate(map, t);
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
  const int num_curves = rgb_curve ? 4 : 3;
  curvemapping_minmax(cumap, num_curves, &min_x, &max_x);

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
      data[i] = make_float3(cumap.evaluate(mapR, cumap.evaluate(mapI, t)),
                            cumap.evaluate(mapG, cumap.evaluate(mapI, t)),
                            cumap.evaluate(mapB, cumap.evaluate(mapI, t)));
    }
  }
  else {
    for (int i = 0; i < size; i++) {
      float t = min_x + (float)i / (float)(size - 1) * range_x;
      data[i] = make_float3(
          cumap.evaluate(mapR, t), cumap.evaluate(mapG, t), cumap.evaluate(mapB, t));
    }
  }
}

static inline bool BKE_object_is_modified(BL::Object &self, BL::Scene &scene, bool preview)
{
  return self.is_modified(scene, (preview) ? (1 << 0) : (1 << 1)) ? true : false;
}

static inline bool BKE_object_is_deform_modified(BObjectInfo &self, BL::Scene &scene, bool preview)
{
  if (!self.is_real_object_data()) {
    /* Comes from geometry nodes, can't use heuristic to guess if it's animated. */
    return true;
  }

  /* Use heuristic to quickly check if object is potentially animated. */
  return self.real_object.is_deform_modified(scene, (preview) ? (1 << 0) : (1 << 1)) ? true :
                                                                                       false;
}

static inline int render_resolution_x(BL::RenderSettings &b_render)
{
  return b_render.resolution_x() * b_render.resolution_percentage() / 100;
}

static inline int render_resolution_y(BL::RenderSettings &b_render)
{
  return b_render.resolution_y() * b_render.resolution_percentage() / 100;
}

static inline string image_user_file_path(BL::BlendData &data,
                                          BL::ImageUser &iuser,
                                          BL::Image &ima,
                                          int cfra)
{
  char filepath[1024];
  iuser.tile(0);
  BKE_image_user_frame_calc(ima.ptr.data, iuser.ptr.data, cfra);
  BKE_image_user_file_path_ex(data.ptr.data, iuser.ptr.data, ima.ptr.data, filepath, false, true);

  return string(filepath);
}

static inline int image_user_frame_number(BL::ImageUser &iuser, BL::Image &ima, int cfra)
{
  BKE_image_user_frame_calc(ima.ptr.data, iuser.ptr.data, cfra);
  return iuser.frame_current();
}

static inline unsigned char *image_get_pixels_for_frame(BL::Image &image, int frame, int tile)
{
  return BKE_image_get_pixels_for_frame(image.ptr.data, frame, tile);
}

static inline float *image_get_float_pixels_for_frame(BL::Image &image, int frame, int tile)
{
  return BKE_image_get_float_pixels_for_frame(image.ptr.data, frame, tile);
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
  char *cstr = RNA_string_get_alloc(&ptr, name, cstrbuf, sizeof(cstrbuf), NULL);
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
static inline uint object_motion_steps(BL::Object &b_parent,
                                       BL::Object &b_ob,
                                       const int max_steps = INT_MAX)
{
  /* Get motion enabled and steps from object itself. */
  PointerRNA cobject = RNA_pointer_get(&b_ob.ptr, "cycles");
  bool use_motion = get_boolean(cobject, "use_motion_blur");
  if (!use_motion) {
    return 0;
  }

  int steps = max(1, get_int(cobject, "motion_steps"));

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
  return min((2 << (steps - 1)) + 1, max_steps);
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

static inline BL::FluidDomainSettings object_fluid_gas_domain_find(BL::Object &b_ob)
{
  for (BL::Modifier &b_mod : b_ob.modifiers) {
    if (b_mod.is_a(&RNA_FluidModifier)) {
      BL::FluidModifier b_mmd(b_mod);

      if (b_mmd.fluid_type() == BL::FluidModifier::fluid_type_DOMAIN &&
          b_mmd.domain_settings().domain_type() == BL::FluidDomainSettings::domain_type_GAS)
      {
        return b_mmd.domain_settings();
      }
    }
  }

  return BL::FluidDomainSettings(PointerRNA_NULL);
}

static inline BL::MeshSequenceCacheModifier object_mesh_cache_find(BL::Object &b_ob,
                                                                   bool *has_subdivision_modifier)
{
  for (int i = b_ob.modifiers.length() - 1; i >= 0; --i) {
    BL::Modifier b_mod = b_ob.modifiers[i];

    if (b_mod.type() == BL::Modifier::type_MESH_SEQUENCE_CACHE) {
      BL::MeshSequenceCacheModifier mesh_cache = BL::MeshSequenceCacheModifier(b_mod);
      return mesh_cache;
    }

    /* Skip possible particles system modifiers as they do not modify the geometry. */
    if (b_mod.type() == BL::Modifier::type_PARTICLE_SYSTEM) {
      continue;
    }

    if (b_mod.type() == BL::Modifier::type_SUBSURF) {
      if (has_subdivision_modifier) {
        *has_subdivision_modifier = true;
      }
      continue;
    }

    break;
  }

  return BL::MeshSequenceCacheModifier(PointerRNA_NULL);
}

static inline Mesh::SubdivisionType object_subdivision_type(BL::Object &b_ob,
                                                            bool preview,
                                                            bool experimental)
{
  PointerRNA cobj = RNA_pointer_get(&b_ob.ptr, "cycles");

  if (cobj.data && !b_ob.modifiers.empty() && experimental) {
    BL::Modifier mod = b_ob.modifiers[b_ob.modifiers.length() - 1];
    bool enabled = preview ? mod.show_viewport() : mod.show_render();

    if (enabled && mod.type() == BL::Modifier::type_SUBSURF &&
        RNA_boolean_get(&cobj, "use_adaptive_subdivision"))
    {
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

static inline uint object_ray_visibility(BL::Object &b_ob)
{
  uint flag = 0;

  flag |= b_ob.visible_camera() ? PATH_RAY_CAMERA : 0;
  flag |= b_ob.visible_diffuse() ? PATH_RAY_DIFFUSE : 0;
  flag |= b_ob.visible_glossy() ? PATH_RAY_GLOSSY : 0;
  flag |= b_ob.visible_transmission() ? PATH_RAY_TRANSMIT : 0;
  flag |= b_ob.visible_shadow() ? PATH_RAY_SHADOW : 0;
  flag |= b_ob.visible_volume_scatter() ? PATH_RAY_VOLUME_SCATTER : 0;

  return flag;
}

/* Check whether some of "built-in" motion-related attributes are needed to be exported (includes
 * things like velocity from cache modifier, fluid simulation).
 *
 * NOTE: This code is run prior to object motion blur initialization. so can not access properties
 * set by `sync_object_motion_init()`. */
static inline bool object_need_motion_attribute(BObjectInfo &b_ob_info, Scene *scene)
{
  const Scene::MotionType need_motion = scene->need_motion();
  if (need_motion == Scene::MOTION_NONE) {
    /* Simple case: neither motion pass nor motion blur is needed, no need in the motion related
     * attributes. */
    return false;
  }

  if (need_motion == Scene::MOTION_BLUR) {
    /* A bit tricky and implicit case:
     * - Motion blur is enabled in the scene, which implies specific number of time steps for
     *   objects.
     * - If the object has motion blur disabled on it, it will have 0 time steps.
     * - Motion attribute expects non-zero time steps.
     *
     * Avoid adding motion attributes if the motion blur will enforce 0 motion steps. */
    PointerRNA cobject = RNA_pointer_get(&b_ob_info.real_object.ptr, "cycles");
    const bool use_motion = get_boolean(cobject, "use_motion_blur");
    if (!use_motion) {
      return false;
    }
  }

  /* Motion pass which implies 3 motion steps, or motion blur which is not disabled on object
   * level. */
  return true;
}

class EdgeMap {
 public:
  EdgeMap() {}

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
