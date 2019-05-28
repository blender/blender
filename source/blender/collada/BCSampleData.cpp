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

#include "BCSampleData.h"
#include "collada_utils.h"

BCSample::~BCSample()
{
  BCBoneMatrixMap::iterator it;
  for (it = bonemats.begin(); it != bonemats.end(); ++it) {
    delete it->second;
  }
}

void BCSample::add_bone_matrix(Bone *bone, Matrix &mat)
{
  BCMatrix *matrix;
  BCBoneMatrixMap::const_iterator it = bonemats.find(bone);
  if (it != bonemats.end()) {
    throw std::invalid_argument("bone " + std::string(bone->name) + " already defined before");
  }
  matrix = new BCMatrix(mat);
  bonemats[bone] = matrix;
}

/* Get channel value */
const bool BCSample::get_value(std::string channel_target, const int array_index, float *val) const
{
  if (channel_target == "location") {
    *val = obmat.location()[array_index];
  }
  else if (channel_target == "scale") {
    *val = obmat.scale()[array_index];
  }
  else if (channel_target == "rotation" || channel_target == "rotation_euler") {
    *val = obmat.rotation()[array_index];
  }
  else if (channel_target == "rotation_quat") {
    *val = obmat.quat()[array_index];
  }
  else {
    *val = 0;
    return false;
  }

  return true;
}

const BCMatrix *BCSample::get_matrix(Bone *bone) const
{
  BCBoneMatrixMap::const_iterator it = bonemats.find(bone);
  if (it == bonemats.end()) {
    return NULL;
  }
  return it->second;
}

const BCMatrix &BCSample::get_matrix() const
{
  return obmat;
}
