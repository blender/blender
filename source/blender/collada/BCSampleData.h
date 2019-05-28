/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

#ifndef __BC_SAMPLE_H__
#define __BC_SAMPLE_H__

#include <string>
#include <map>
#include <algorithm>

#include "ExportSettings.h"
#include "BCSampleData.h"
#include "BCMath.h"

extern "C" {
#include "BKE_object.h"
#include "BLI_math_rotation.h"
#include "DNA_object_types.h"
#include "DNA_armature_types.h"
#include "DNA_material_types.h"
#include "DNA_light_types.h"
#include "DNA_camera_types.h"
}

typedef std::map<Bone *, BCMatrix *> BCBoneMatrixMap;

class BCSample {
 private:
  BCMatrix obmat;
  BCBoneMatrixMap bonemats; /* For Armature animation */

 public:
  BCSample(Object *ob) : obmat(ob)
  {
  }

  ~BCSample();

  void add_bone_matrix(Bone *bone, Matrix &mat);

  const bool get_value(std::string channel_target, const int array_index, float *val) const;
  const BCMatrix &get_matrix() const;
  const BCMatrix *get_matrix(Bone *bone) const;  // returns NULL if bone is not animated
};

typedef std::map<Object *, BCSample *> BCSampleMap;
typedef std::map<int, const BCSample *> BCFrameSampleMap;
typedef std::map<int, const BCMatrix *> BCMatrixSampleMap;

#endif
