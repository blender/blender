/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BCSampleData.h"
#include "collada_utils.h"

#include "MEM_guardedalloc.h"

#include "BKE_armature.hh"
#include "BKE_fcurve.hh"
#include "BKE_material.h"

#include "ED_anim_api.hh"
#include "ED_keyframes_edit.hh"

#include "ANIM_fcurve.hh"

typedef float(TangentPoint)[2];

typedef std::set<float> BCFrameSet;
typedef std::vector<float> BCFrames;
typedef std::vector<float> BCValues;
typedef std::vector<float> BCTimes;
typedef std::map<int, float> BCValueMap;

typedef enum BC_animation_type {
  BC_ANIMATION_TYPE_OBJECT,
  BC_ANIMATION_TYPE_BONE,
  BC_ANIMATION_TYPE_CAMERA,
  BC_ANIMATION_TYPE_MATERIAL,
  BC_ANIMATION_TYPE_LIGHT,
} BC_animation_type;

class BCCurveKey {
 private:
  BC_animation_type key_type;
  std::string rna_path;
  int curve_array_index;
  int curve_subindex; /* only needed for materials */

 public:
  BCCurveKey();
  BCCurveKey(const BC_animation_type type,
             const std::string path,
             int array_index,
             int subindex = -1);
  void operator=(const BCCurveKey &other);
  std::string get_full_path() const;
  std::string get_path() const;
  int get_array_index() const;
  int get_subindex() const;
  void set_object_type(BC_animation_type object_type);
  BC_animation_type get_animation_type() const;
  bool operator<(const BCCurveKey &other) const;
};

class BCBezTriple {
 public:
  BezTriple &bezt;

  BCBezTriple(BezTriple &bezt);
  float get_frame() const;
  float get_time(Scene *scene) const;
  float get_value() const;
  float get_angle() const;
  void get_in_tangent(Scene *scene, float point[2], bool as_angle) const;
  void get_out_tangent(Scene *scene, float point[2], bool as_angle) const;
  void get_tangent(Scene *scene, float point[2], bool as_angle, int index) const;
};

class BCAnimationCurve {
 private:
  BCCurveKey curve_key;
  float min = 0;
  float max = 0;

  bool curve_is_local_copy = false;
  FCurve *fcurve;
  PointerRNA id_ptr;
  void init_pointer_rna(Object *ob);
  void delete_fcurve(FCurve *fcu);
  FCurve *create_fcurve(int array_index, const char *rna_path);
  void create_bezt(float frame, float output);
  void update_range(float val);
  void init_range(float val);

 public:
  BCAnimationCurve();
  BCAnimationCurve(const BCAnimationCurve &other);
  BCAnimationCurve(const BCCurveKey &key, Object *ob);
  BCAnimationCurve(BCCurveKey key, Object *ob, FCurve *fcu);
  ~BCAnimationCurve();

  bool is_of_animation_type(BC_animation_type type) const;
  int get_interpolation_type(float sample_frame) const;
  bool is_animated();
  bool is_transform_curve() const;
  bool is_rotation_curve() const;
  bool is_keyframe(int frame);
  void adjust_range(int frame);

  std::string get_animation_name(Object *ob) const; /* XXX: this is COLLADA specific. */
  std::string get_channel_target() const;
  std::string get_channel_type() const;
  std::string get_channel_posebone() const; /* returns "" if channel is not a bone channel */

  int get_channel_index() const;
  int get_subindex() const;
  std::string get_rna_path() const;
  FCurve *get_fcurve() const;
  int sample_count() const;

  float get_value(float frame);
  void get_values(BCValues &values) const;
  void get_value_map(BCValueMap &value_map);

  void get_frames(BCFrames &frames) const;

  /* Curve edit functions create a copy of the underlying #FCurve. */
  FCurve *get_edit_fcurve();
  bool add_value_from_rna(int frame);
  bool add_value_from_matrix(const BCSample &sample, int frame);
  void add_value(float val, int frame);
  void clean_handles();

  /* experimental stuff */
  int closest_index_above(float sample_frame, int start_at) const;
  int closest_index_below(float sample_frame) const;
};

typedef std::map<BCCurveKey, BCAnimationCurve *> BCAnimationCurveMap;
