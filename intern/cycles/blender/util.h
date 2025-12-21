/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "BKE_colorband.hh"
#include "BKE_colortools.hh"
#include "BKE_main.hh"
#include "DNA_fluid_types.h"
#include "DNA_text_types.h"
#include "RE_engine.h"
#include "RNA_access.hh"

#include "scene/mesh.h"
#include "scene/scene.h"

#include "util/algorithm.h"
#include "util/array.h"
#include "util/path.h"
#include "util/set.h"
#include "util/transform.h"
#include "util/types.h"

#include "BLI_listbase.h"

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"

#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.h"
#include "BKE_mesh_types.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_object.hh"

CCL_NAMESPACE_BEGIN

static inline ::ID *object_get_data(const ::Object &b_ob, const bool use_adaptive_subdivision)
{
  if (!use_adaptive_subdivision && b_ob.type == OB_MESH) {
    return &BKE_mesh_wrapper_ensure_subdivision(static_cast<::Mesh *>(b_ob.data))->id;
  }

  return reinterpret_cast<ID *>(b_ob.data);
}

struct BObjectInfo {
  /* Object directly provided by the depsgraph iterator. This object is only valid during one
   * iteration and must not be accessed afterwards. Transforms and visibility should be checked on
   * this object. */
  ::Object *iter_object;

  /* This object remains alive even after the object iterator is done. It corresponds to one
   * original object. It is the object that owns the object data below. */
  ::Object *real_object;

  /* The object-data referenced by the iter object. This is still valid after the depsgraph
   * iterator is done. It might have a different type compared to object_get_data(real_object). */
  ::ID *object_data;

  /* Object will use adaptive subdivision. */
  bool use_adaptive_subdivision;

  /* True when the current geometry is the data of the referenced object. False when it is a
   * geometry instance that does not have a 1-to-1 relationship with an object. */
  bool is_real_object_data() const
  {
    return object_get_data(*real_object, use_adaptive_subdivision) == object_data;
  }
};

static inline ::Mesh *object_copy_mesh_data(const BObjectInfo &b_ob_info)
{
  ::Mesh *mesh = BKE_mesh_new_from_object(
      nullptr, b_ob_info.real_object, false, false, !b_ob_info.use_adaptive_subdivision);
  return mesh;
}

int blender_attribute_name_split_type(ustring name, string *r_real_name);

void python_thread_state_save(void **python_thread_state);
void python_thread_state_restore(void **python_thread_state);

static bool mesh_use_corner_normals(const BObjectInfo &b_ob_info, ::Mesh *mesh)
{
  return mesh && !b_ob_info.use_adaptive_subdivision &&
         (mesh->normals_domain(true) == blender::bke::MeshNormalDomain::Corner);
}

void mesh_split_edges_for_corner_normals(::Mesh &mesh);

static inline ::Mesh *object_to_mesh(BObjectInfo &b_ob_info)
{
  ::Mesh *mesh = (GS(b_ob_info.object_data->name) == ID_ME) ?
                     blender::id_cast<::Mesh *>(b_ob_info.object_data) :
                     nullptr;

  bool use_corner_normals = false;

  if (b_ob_info.is_real_object_data()) {
    if (mesh) {
      if (mesh->runtime->edit_mesh) {
        /* Flush edit-mesh to mesh, including all data layers. */
        mesh = object_copy_mesh_data(b_ob_info);
        use_corner_normals = mesh_use_corner_normals(b_ob_info, mesh);
      }
      else if (mesh_use_corner_normals(b_ob_info, mesh)) {
        /* Make a copy to split faces. */
        mesh = object_copy_mesh_data(b_ob_info);
        use_corner_normals = true;
      }
    }
    else {
      mesh = object_copy_mesh_data(b_ob_info);
      use_corner_normals = mesh_use_corner_normals(b_ob_info, mesh);
    }
  }
  else {
    /* TODO: what to do about non-mesh geometry instances? */
    use_corner_normals = mesh_use_corner_normals(b_ob_info, mesh);
  }

  if (mesh) {
    if (use_corner_normals) {
      mesh_split_edges_for_corner_normals(*mesh);
    }

    if (b_ob_info.use_adaptive_subdivision) {
      mesh->corner_tris();
    }
  }

  return mesh;
}

static inline void free_object_to_mesh(BObjectInfo &b_ob_info, ::Mesh &mesh)
{
  if (!b_ob_info.is_real_object_data()) {
    return;
  }
  /* Free mesh if we didn't just use the existing one. */
  ::Object *object = b_ob_info.real_object;
  if (object_get_data(*object, b_ob_info.use_adaptive_subdivision) != &mesh.id) {
    BKE_id_free(nullptr, &mesh.id);
  }
}

static inline void colorramp_to_array(const ::ColorBand &ramp,
                                      array<float3> &ramp_color,
                                      array<float> &ramp_alpha,
                                      const int size)
{
  const int full_size = size + 1;
  ramp_color.resize(full_size);
  ramp_alpha.resize(full_size);

  for (int i = 0; i < full_size; i++) {
    float color[4];

    BKE_colorband_evaluate(&ramp, float(i) / float(size), color);
    ramp_color[i] = make_float3(color[0], color[1], color[2]);
    ramp_alpha[i] = color[3];
  }
}

static inline void curvemap_minmax_curve(const ::CurveMap &curve, float *min_x, float *max_x)
{
  const blender::Span<::CurveMapPoint> points(curve.curve, curve.totpoint);
  *min_x = min(*min_x, points.first().x);
  *max_x = max(*max_x, points.last().x);
}

static inline void curvemapping_minmax(const ::CurveMapping &cumap,
                                       const int num_curves,
                                       float *min_x,
                                       float *max_x)
{
  // const int num_curves = cumap.curves.length(); /* Gives linking error so far. */
  *min_x = FLT_MAX;
  *max_x = -FLT_MAX;
  for (int i = 0; i < num_curves; ++i) {
    const ::CurveMap &map(cumap.cm[i]);
    curvemap_minmax_curve(map, min_x, max_x);
  }
}

static inline void curvemapping_to_array(const ::CurveMapping &cumap,
                                         array<float> &data,
                                         const int size)
{
  BKE_curvemapping_changed_all(&const_cast<::CurveMapping &>(cumap));
  const ::CurveMap &curve = cumap.cm[0];
  const int full_size = size + 1;
  data.resize(full_size);
  if (!curve.table) {
    BKE_curvemapping_init(&const_cast<::CurveMapping &>(cumap));
  }
  for (int i = 0; i < full_size; i++) {
    const float t = float(i) / float(size);
    data[i] = BKE_curvemap_evaluateF(&cumap, &curve, t);
  }
}

static inline void curvemapping_float_to_array(const ::CurveMapping &cumap,
                                               array<float> &data,
                                               const int size)
{
  float min = 0.0f;
  float max = 1.0f;

  curvemapping_minmax(cumap, 1, &min, &max);

  const float range = max - min;

  BKE_curvemapping_changed_all(&const_cast<::CurveMapping &>(cumap));

  const ::CurveMap &map = cumap.cm[0];

  const int full_size = size + 1;
  data.resize(full_size);
  if (!map.table) {
    BKE_curvemapping_init(&const_cast<::CurveMapping &>(cumap));
  }
  for (int i = 0; i < full_size; i++) {
    const float t = min + float(i) / float(size) * range;
    data[i] = BKE_curvemap_evaluateF(&cumap, &map, t);
  }
}

static inline void curvemapping_color_to_array(const ::CurveMapping &cumap,
                                               array<float3> &data,
                                               const int size,
                                               bool rgb_curve)
{
  float min_x = 0.0f;
  float max_x = 1.0f;

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

  BKE_curvemapping_changed_all(&const_cast<::CurveMapping &>(cumap));

  const ::CurveMap &mapR = cumap.cm[0];
  const ::CurveMap &mapG = cumap.cm[1];
  const ::CurveMap &mapB = cumap.cm[2];
  if (!mapR.table || !mapG.table || !mapB.table) {
    BKE_curvemapping_init(&const_cast<::CurveMapping &>(cumap));
  }

  const int full_size = size + 1;
  data.resize(full_size);

  if (rgb_curve) {
    const ::CurveMap &mapI = cumap.cm[3];
    if (!mapR.table || !mapG.table || !mapB.table || !mapI.table) {
      BKE_curvemapping_init(&const_cast<::CurveMapping &>(cumap));
    }

    for (int i = 0; i < full_size; i++) {
      const float t = min_x + float(i) / float(size) * range_x;
      data[i] = make_float3(
          BKE_curvemap_evaluateF(&cumap, &mapR, BKE_curvemap_evaluateF(&cumap, &mapI, t)),
          BKE_curvemap_evaluateF(&cumap, &mapG, BKE_curvemap_evaluateF(&cumap, &mapI, t)),
          BKE_curvemap_evaluateF(&cumap, &mapB, BKE_curvemap_evaluateF(&cumap, &mapI, t)));
    }
  }
  else {
    if (!mapR.table || !mapG.table || !mapB.table) {
      BKE_curvemapping_init(&const_cast<::CurveMapping &>(cumap));
    }

    for (int i = 0; i < full_size; i++) {
      const float t = min_x + float(i) / float(size) * range_x;
      data[i] = make_float3(BKE_curvemap_evaluateF(&cumap, &mapR, t),
                            BKE_curvemap_evaluateF(&cumap, &mapG, t),
                            BKE_curvemap_evaluateF(&cumap, &mapB, t));
    }
  }
}

static inline bool BKE_object_is_deform_modified(BObjectInfo &self, ::Scene &scene, bool preview)
{
  if (!self.is_real_object_data()) {
    /* Comes from geometry nodes, can't use heuristic to guess if it's animated. */
    return true;
  }

  /* Use heuristic to quickly check if object is potentially animated. */
  const int settings = preview ? eModifierMode_Realtime : eModifierMode_Render;
  return (::BKE_object_is_deform_modified(&scene, self.real_object) & settings) != 0;
}

static inline int render_resolution_x(const ::RenderData &b_render)
{
  return b_render.xsch * b_render.size / 100;
}

static inline int render_resolution_y(const ::RenderData &b_render)
{
  return b_render.ysch * b_render.size / 100;
}

static inline string image_user_file_path(::Main &data,
                                          ::ImageUser &iuser,
                                          ::Image &ima,
                                          const int cfra)
{
  char filepath[1024];
  BKE_image_user_frame_calc(&ima, &iuser, cfra);
  BKE_image_user_file_path_ex(&data, &iuser, &ima, filepath, false, true);

  return string(filepath);
}

static inline int image_user_frame_number(::ImageUser &iuser, ::Image &ima, const int cfra)
{
  BKE_image_user_frame_calc(&ima, &iuser, cfra);
  return iuser.framenr;
}

static inline bool image_is_builtin(::Image &ima, ::RenderEngine &engine)
{
  const eImageSource image_source = eImageSource(ima.source);
  if (image_source == IMA_SRC_TILED) {
    /* If any tile is marked as generated, then treat the entire Image as built-in. */
    LISTBASE_FOREACH (::ImageTile *, tile, &ima.tiles) {
      if (tile->gen_flag & IMA_GEN_TILE) {
        return true;
      }
    }
  }

  return BKE_image_has_packedfile(&ima) || image_source == IMA_SRC_GENERATED ||
         image_source == IMA_SRC_MOVIE ||
         ((engine.flag & RE_ENGINE_PREVIEW) != 0 && image_source != IMA_SRC_SEQUENCE);
}

static inline void render_add_metadata(::RenderResult &b_rr, string name, string value)
{
  BKE_render_result_stamp_data(&b_rr, name.c_str(), value.c_str());
}

/* Utilities */

static inline Transform get_transform(const blender::float4x4 &matrix)
{
  /* Convert from Blender column major to Cycles row major, assume it's an affine transform that
   * does not need the last row. */
  const float *ptr = matrix.base_ptr();
  return make_transform(ptr[0],
                        ptr[4],
                        ptr[8],
                        ptr[12],

                        ptr[1],
                        ptr[5],
                        ptr[9],
                        ptr[13],

                        ptr[2],
                        ptr[6],
                        ptr[10],
                        ptr[14]);
}

static inline float3 get_float3(PointerRNA &ptr, const char *name)
{
  float3 f;
  RNA_float_get_array(&ptr, name, &f.x);
  return f;
}

static inline void set_float3(PointerRNA &ptr, const char *name, const float3 value)
{
  RNA_float_set_array(&ptr, name, &value.x);
}

static inline float4 get_float4(PointerRNA &ptr, const char *name)
{
  float4 f;
  RNA_float_get_array(&ptr, name, &f.x);
  return f;
}

static inline void set_float4(PointerRNA &ptr, const char *name, const float4 value)
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

static inline void set_float(PointerRNA &ptr, const char *name, const float value)
{
  RNA_float_set(&ptr, name, value);
}

static inline int get_int(PointerRNA &ptr, const char *name)
{
  return RNA_int_get(&ptr, name);
}

static inline void set_int(PointerRNA &ptr, const char *name, const int value)
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
  const int value = RNA_property_enum_get(&ptr, prop);

  RNA_property_enum_identifier(nullptr, &ptr, prop, value, &identifier);

  return string(identifier);
}

static inline void set_enum(PointerRNA &ptr, const char *name, const int value)
{
  RNA_enum_set(&ptr, name, value);
}

static inline void set_enum(PointerRNA &ptr, const char *name, const string &identifier)
{
  RNA_enum_set_identifier(nullptr, &ptr, name, identifier.c_str());
}

static inline string get_string(PointerRNA &ptr, const char *name)
{
  return RNA_string_get(&ptr, name);
}

static inline void set_string(PointerRNA &ptr, const char *name, const string &value)
{
  RNA_string_set(&ptr, name, value.c_str());
}

/* Relative Paths */

static inline string blender_absolute_path(::Main &b_data, ::ID &b_id, const string &path)
{
  if (path.size() >= 2 && path[0] == '/' && path[1] == '/') {
    string dirname;

    if (b_id.lib) {
      dirname = blender_absolute_path(b_data, b_id.lib->id, b_id.lib->filepath);
    }
    else {
      dirname = b_data.filepath;
    }

    return path_join(path_dirname(dirname), path.substr(2));
  }

  return path;
}

static inline string get_text_datablock_content(const ::ID *id)
{
  if (id == nullptr) {
    return "";
  }
  if (GS(id->name) != ID_TXT) {
    return "";
  }
  const auto &text = *blender::id_cast<const ::Text *>(id);

  string content;
  LISTBASE_FOREACH (::TextLine *, line, &text.lines) {
    content += line->line ? line->line : "";
    content += "\n";
  }

  return content;
}

/* Texture Space */

static inline void mesh_texture_space(const ::Mesh &b_mesh, float3 &loc, float3 &size)
{
  float texspace_location[3];
  float texspace_size[3];
  BKE_mesh_texspace_get(const_cast<::Mesh *>(&b_mesh), texspace_location, texspace_size);

  loc = make_float3(texspace_location[0], texspace_location[1], texspace_location[2]);
  size = make_float3(texspace_size[0], texspace_size[1], texspace_size[2]);

  if (size.x != 0.0f) {
    size.x = 0.5f / size.x;
  }
  if (size.y != 0.0f) {
    size.y = 0.5f / size.y;
  }
  if (size.z != 0.0f) {
    size.z = 0.5f / size.z;
  }

  loc = loc * size - make_float3(0.5f, 0.5f, 0.5f);
}

/* Object motion steps, returns 0 if no motion blur needed. */
static inline uint object_motion_steps(::Object &b_parent,
                                       ::Object &b_ob,
                                       const int max_steps = INT_MAX)
{
  /* Get motion enabled and steps from object itself. */
  PointerRNA object_rna_ptr = RNA_id_pointer_create(&b_ob.id);
  PointerRNA cobject = RNA_pointer_get(&object_rna_ptr, "cycles");
  bool use_motion = get_boolean(cobject, "use_motion_blur");
  if (!use_motion) {
    return 0;
  }

  int steps = max(1, get_int(cobject, "motion_steps"));

  /* Also check parent object, so motion blur and steps can be
   * controlled by dupli-group duplicator for linked groups. */
  if (&b_parent != &b_ob) {
    PointerRNA parent_rna_ptr = RNA_id_pointer_create(&b_parent.id);
    PointerRNA parent_cobject = RNA_pointer_get(&parent_rna_ptr, "cycles");
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
static inline bool object_use_deform_motion(::Object &b_parent, ::Object &b_ob)
{
  PointerRNA b_ob_rna_ptr = RNA_id_pointer_create(&b_ob.id);
  PointerRNA cobject = RNA_pointer_get(&b_ob_rna_ptr, "cycles");
  bool use_deform_motion = get_boolean(cobject, "use_deform_motion");
  /* If motion blur is enabled for the object we also check
   * whether it's enabled for the parent object as well.
   *
   * This way we can control motion blur from the dupli-group
   * duplicator much easier. */
  if (use_deform_motion && &b_parent != &b_ob) {
    PointerRNA b_parent_rna_ptr = RNA_id_pointer_create(&b_parent.id);
    PointerRNA parent_cobject = RNA_pointer_get(&b_parent_rna_ptr, "cycles");
    use_deform_motion &= get_boolean(parent_cobject, "use_deform_motion");
  }
  return use_deform_motion;
}

static inline ::FluidDomainSettings *object_fluid_gas_domain_find(::Object &b_ob)
{
  LISTBASE_FOREACH (::ModifierData *, b_mod, &b_ob.modifiers) {
    if (b_mod->type == eModifierType_Fluid) {
      auto *b_mmd = reinterpret_cast<::FluidModifierData *>(b_mod);

      if (b_mmd->type == MOD_FLUID_TYPE_DOMAIN && b_mmd->domain->type == FLUID_DOMAIN_TYPE_GAS) {
        return b_mmd->domain;
      }
    }
  }

  return nullptr;
}

static ::SubsurfModifierData *object_subdivision_modifier(::Object &b_ob, const bool preview)
{
  ModifierData *md = static_cast<ModifierData *>(b_ob.modifiers.last);
  if (!md) {
    return nullptr;
  }
  if (md->type != eModifierType_Subsurf) {
    return nullptr;
  }
  const ModifierMode enabled_mode = preview ? eModifierMode_Render : eModifierMode_Realtime;
  if ((md->mode & enabled_mode) == 0) {
    return nullptr;
  }
  SubsurfModifierData *subsurf = reinterpret_cast<SubsurfModifierData *>(md);
  if ((subsurf->flags & eSubsurfModifierFlag_UseAdaptiveSubdivision) == 0) {
    return nullptr;
  }
  return subsurf;
}

static inline Mesh::SubdivisionType object_subdivision_type(::Object &b_ob,
                                                            const bool preview,
                                                            const bool use_adaptive_subdivision)
{
  if (!use_adaptive_subdivision) {
    return Mesh::SUBDIVISION_NONE;
  }

  ::SubsurfModifierData *subsurf = object_subdivision_modifier(b_ob, preview);

  if (subsurf) {
    if (subsurf->subdivType == SUBSURF_TYPE_CATMULL_CLARK) {
      return Mesh::SUBDIVISION_CATMULL_CLARK;
    }
    return Mesh::SUBDIVISION_LINEAR;
  }

  return Mesh::SUBDIVISION_NONE;
}

static inline void object_subdivision_to_mesh(::Object &b_ob,
                                              Mesh &mesh,
                                              const bool preview,
                                              const bool use_adaptive_subdivision)
{
  if (!use_adaptive_subdivision) {
    mesh.set_subdivision_type(Mesh::SUBDIVISION_NONE);
    return;
  }

  ::SubsurfModifierData *subsurf = object_subdivision_modifier(b_ob, preview);

  if (!subsurf) {
    mesh.set_subdivision_type(Mesh::SUBDIVISION_NONE);
    return;
  }

  if (subsurf->subdivType != SUBSURF_TYPE_CATMULL_CLARK) {
    mesh.set_subdivision_type(Mesh::SUBDIVISION_LINEAR);
    return;
  }

  mesh.set_subdivision_type(Mesh::SUBDIVISION_CATMULL_CLARK);

  switch (subsurf->boundary_smooth) {
    case SUBSURF_BOUNDARY_SMOOTH_PRESERVE_CORNERS:
      mesh.set_subdivision_boundary_interpolation(Mesh::SUBDIVISION_BOUNDARY_EDGE_AND_CORNER);
      break;
    case SUBSURF_BOUNDARY_SMOOTH_ALL:
      mesh.set_subdivision_boundary_interpolation(Mesh::SUBDIVISION_BOUNDARY_EDGE_ONLY);
      break;
  }

  switch (subsurf->uv_smooth) {
    case SUBSURF_UV_SMOOTH_NONE:
      mesh.set_subdivision_fvar_interpolation(Mesh::SUBDIVISION_FVAR_LINEAR_ALL);
      break;
    case SUBSURF_UV_SMOOTH_PRESERVE_CORNERS:
      mesh.set_subdivision_fvar_interpolation(Mesh::SUBDIVISION_FVAR_LINEAR_CORNERS_ONLY);
      break;
    case SUBSURF_UV_SMOOTH_PRESERVE_CORNERS_AND_JUNCTIONS:
      mesh.set_subdivision_fvar_interpolation(Mesh::SUBDIVISION_FVAR_LINEAR_CORNERS_PLUS1);
      break;
    case SUBSURF_UV_SMOOTH_PRESERVE_CORNERS_JUNCTIONS_AND_CONCAVE:
      mesh.set_subdivision_fvar_interpolation(Mesh::SUBDIVISION_FVAR_LINEAR_CORNERS_PLUS2);
      break;
    case SUBSURF_UV_SMOOTH_PRESERVE_BOUNDARIES:
      mesh.set_subdivision_fvar_interpolation(Mesh::SUBDIVISION_FVAR_LINEAR_BOUNDARIES);
      break;
    case SUBSURF_UV_SMOOTH_ALL:
      mesh.set_subdivision_fvar_interpolation(Mesh::SUBDIVISION_FVAR_LINEAR_NONE);
      break;
  }
}

static inline uint object_ray_visibility(::Object &b_ob)
{
  uint flag = 0;

  flag |= ((b_ob.visibility_flag & OB_HIDE_CAMERA) == 0) ? PATH_RAY_CAMERA : PathRayFlag(0);
  flag |= ((b_ob.visibility_flag & OB_HIDE_DIFFUSE) == 0) ? PATH_RAY_DIFFUSE : PathRayFlag(0);
  flag |= ((b_ob.visibility_flag & OB_HIDE_GLOSSY) == 0) ? PATH_RAY_GLOSSY : PathRayFlag(0);
  flag |= ((b_ob.visibility_flag & OB_HIDE_TRANSMISSION) == 0) ? PATH_RAY_TRANSMIT :
                                                                 PathRayFlag(0);
  flag |= ((b_ob.visibility_flag & OB_HIDE_SHADOW) == 0) ? PATH_RAY_SHADOW : PathRayFlag(0);
  flag |= ((b_ob.visibility_flag & OB_HIDE_VOLUME_SCATTER) == 0) ? PATH_RAY_VOLUME_SCATTER :
                                                                   PathRayFlag(0);

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
    PointerRNA b_ob_rna_ptr = RNA_id_pointer_create(&b_ob_info.real_object->id);
    PointerRNA cobject = RNA_pointer_get(&b_ob_rna_ptr, "cycles");
    const bool use_motion = get_boolean(cobject, "use_motion_blur");
    if (!use_motion) {
      return false;
    }
  }

  /* Motion pass which implies 3 motion steps, or motion blur which is not disabled on object
   * level. */
  return true;
}

static inline bool region_view3d_navigating_or_transforming(const ::RegionView3D *b_rv3d)
{
  return b_rv3d && ((b_rv3d->rflag & (RV3D_NAVIGATING | RV3D_PAINTING)) ||
                    (G.moving & (G_TRANSFORM_OBJ | G_TRANSFORM_EDIT)));
}

class EdgeMap {
 public:
  EdgeMap() = default;

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
