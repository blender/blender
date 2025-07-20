/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <map>
#include <string>

#include "BCMath.h"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"

using BCBoneMatrixMap = std::map<Bone *, BCMatrix *>;

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

using BCSampleMap = std::map<Object *, BCSample *>;
using BCFrameSampleMap = std::map<int, const BCSample *>;
using BCMatrixSampleMap = std::map<int, const BCMatrix *>;
