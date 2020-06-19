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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup Alembic
 */

#pragma once

#include "ABC_alembic.h"
#include "IO_abstract_hierarchy_iterator.h"

#include <Alembic/Abc/OArchive.h>
#include <Alembic/Abc/OTypedScalarProperty.h>

#include <fstream>
#include <set>
#include <string>

struct Main;
struct Scene;

namespace blender {
namespace io {
namespace alembic {

/* Container for an Alembic archive and time sampling info.
 *
 * Constructor arguments are used to create the correct output stream and to set the archive's
 * metadata. */
class ABCArchive {
 public:
  typedef std::set<double> Frames;

  Alembic::Abc::OArchive *archive;

  ABCArchive(const Main *bmain,
             const Scene *scene,
             AlembicExportParams params,
             std::string filename);
  ~ABCArchive();

  uint32_t time_sampling_index_transforms() const;
  uint32_t time_sampling_index_shapes() const;

  Frames::const_iterator frames_begin() const;
  Frames::const_iterator frames_end() const;
  size_t total_frame_count() const;

  bool is_xform_frame(double frame) const;
  bool is_shape_frame(double frame) const;

  ExportSubset export_subset_for_frame(double frame) const;

  void update_bounding_box(const Imath::Box3d &bounds);

 private:
  std::ofstream abc_ostream_;
  uint32_t time_sampling_index_transforms_;
  uint32_t time_sampling_index_shapes_;

  Frames xform_frames_;
  Frames shape_frames_;
  Frames export_frames_;

  Alembic::Abc::OBox3dProperty abc_archive_bbox_;
};

}  // namespace alembic
}  // namespace io
}  // namespace blender
