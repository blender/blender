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
  std::string bname = bc_string_before(channel_target, ".");
  std::string channel_type = bc_string_after(channel_target, ".");

  const BCMatrix *matrix = &obmat;
  if (bname != channel_target) {
    bname = bname.substr(2);
    bname = bc_string_before(bname, "\"");
    BCBoneMatrixMap::const_iterator it;
    for (it = bonemats.begin(); it != bonemats.end(); ++it) {
      Bone *bone = it->first;
      if (bname == bone->name) {
        matrix = it->second;
        break;
      }
    }
  }
  else {
    matrix = &obmat;
  }

  if (channel_type == "location") {
    *val = matrix->location()[array_index];
  }
  else if (channel_type == "scale") {
    *val = matrix->scale()[array_index];
  }
  else if (channel_type == "rotation" || channel_type == "rotation_euler") {
    *val = matrix->rotation()[array_index];
  }
  else if (channel_type == "rotation_quaternion") {
    *val = matrix->quat()[array_index];
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
