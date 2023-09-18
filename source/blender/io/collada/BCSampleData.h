/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <algorithm>
#include <map>
#include <string>

#include "BCMath.h"
#include "BCSampleData.h"
#include "ExportSettings.h"

#include "BKE_object.h"

#include "BLI_math_rotation.h"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

typedef std::map<Bone *, BCMatrix *> BCBoneMatrixMap;

class BCSample {
 private:
  BCMatrix obmat;
  BCBoneMatrixMap bonemats; /* For Armature animation */

 public:
  BCSample(Object *ob) : obmat(ob) {}

  ~BCSample();

  void add_bone_matrix(Bone *bone, Matrix &mat);

  /** Get channel value. */
  bool get_value(std::string channel_target, int array_index, float *val) const;
  const BCMatrix &get_matrix() const;
  const BCMatrix *get_matrix(Bone *bone) const; /* returns NULL if bone is not animated */
};

typedef std::map<Object *, BCSample *> BCSampleMap;
typedef std::map<int, const BCSample *> BCFrameSampleMap;
typedef std::map<int, const BCMatrix *> BCMatrixSampleMap;
