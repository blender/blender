/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include <map>
#include <string>
#include <vector>

#include "COLLADAFWFileInfo.h"
#include "Math/COLLADABUMathMatrix4.h"

#include "BLI_linklist.h"
#include "DNA_armature_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

class UnitConverter {
 private:
  COLLADAFW::FileInfo::Unit unit;
  COLLADAFW::FileInfo::UpAxisType up_axis;

  float x_up_mat4[4][4];
  float y_up_mat4[4][4];
  float z_up_mat4[4][4];
  float scale_mat4[4][4];

 public:
  enum UnitSystem {
    None,
    Metric,
    Imperial,
  };

  /* Initialize with Z_UP, since Blender uses right-handed, z-up */
  UnitConverter();

  void read_asset(const COLLADAFW::FileInfo *asset);

  void convertVector3(COLLADABU::Math::Vector3 &vec, float *v);

  UnitConverter::UnitSystem isMetricSystem(void);

  float getLinearMeter(void);

  /* TODO: need also for angle conversion, time conversion... */

  static void dae_matrix_to_mat4_(float out[4][4], const COLLADABU::Math::Matrix4 &in);
  static void mat4_to_dae(float out[4][4], float in[4][4]);
  static void mat4_to_dae_double(double out[4][4], float in[4][4]);

  float (&get_rotation())[4][4];
  float (&get_scale())[4][4];
  void calculate_scale(Scene &sce);
};

extern void clear_global_id_map();
/** Look at documentation of translate_map */
extern std::string translate_id(const std::string &id);
/** Look at documentation of translate_map */
extern std::string translate_id(const char *idString);

extern std::string id_name(void *id);
extern std::string encode_xml(std::string xml);

extern std::string get_geometry_id(Object *ob);
extern std::string get_geometry_id(Object *ob, bool use_instantiation);

extern std::string get_light_id(Object *ob);

extern std::string get_joint_sid(Bone *bone);

extern std::string get_camera_id(Object *ob);
extern std::string get_morph_id(Object *ob);

extern std::string get_effect_id(Material *mat);
extern std::string get_material_id(Material *mat);
