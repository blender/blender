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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 */

#ifndef OPENVDB_TRANSFORM_H
#define OPENVDB_TRANSFORM_H

#include <openvdb/openvdb.h>

struct OpenVDBTransform {
 private:
  openvdb::math::Transform::Ptr transform;

 public:
  OpenVDBTransform();
  ~OpenVDBTransform();
  void create_linear_transform(double voxel_size);
  const openvdb::math::Transform::Ptr &get_transform();
  void set_transform(const openvdb::math::Transform::Ptr &transform);
};

#endif  // OPENVDB_TRANSFORM_H
