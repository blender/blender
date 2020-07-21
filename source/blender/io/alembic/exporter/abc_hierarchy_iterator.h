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
#pragma once

#include "ABC_alembic.h"
#include "abc_archive.h"

#include "IO_abstract_hierarchy_iterator.h"

#include <string>

#include <Alembic/Abc/OArchive.h>
#include <Alembic/Abc/OObject.h>

struct Depsgraph;
struct ID;
struct Object;

namespace blender {
namespace io {
namespace alembic {

class ABCHierarchyIterator;

struct ABCWriterConstructorArgs {
  Depsgraph *depsgraph;
  ABCArchive *abc_archive;
  Alembic::Abc::OObject abc_parent;
  std::string abc_name;
  std::string abc_path;
  const ABCHierarchyIterator *hierarchy_iterator;
  const AlembicExportParams *export_params;
};

class ABCHierarchyIterator : public AbstractHierarchyIterator {
 private:
  ABCArchive *abc_archive_;
  const AlembicExportParams &params_;

 public:
  ABCHierarchyIterator(Depsgraph *depsgraph,
                       ABCArchive *abc_archive_,
                       const AlembicExportParams &params);

  virtual void iterate_and_write() override;
  virtual std::string make_valid_name(const std::string &name) const override;

 protected:
  virtual bool mark_as_weak_export(const Object *object) const override;

  virtual ExportGraph::key_type determine_graph_index_object(
      const HierarchyContext *context) override;
  virtual AbstractHierarchyIterator::ExportGraph::key_type determine_graph_index_dupli(
      const HierarchyContext *context,
      const DupliObject *dupli_object,
      const DupliParentFinder &dupli_parent_finder) override;

  virtual AbstractHierarchyWriter *create_transform_writer(
      const HierarchyContext *context) override;
  virtual AbstractHierarchyWriter *create_data_writer(const HierarchyContext *context) override;
  virtual AbstractHierarchyWriter *create_hair_writer(const HierarchyContext *context) override;
  virtual AbstractHierarchyWriter *create_particle_writer(
      const HierarchyContext *context) override;

  virtual void release_writer(AbstractHierarchyWriter *writer) override;

 private:
  Alembic::Abc::OObject get_alembic_parent(const HierarchyContext *context) const;
  ABCWriterConstructorArgs writer_constructor_args(const HierarchyContext *context) const;
  void update_archive_bounding_box();
  void update_bounding_box_recursive(Imath::Box3d &bounds, const HierarchyContext *context);
};

}  // namespace alembic
}  // namespace io
}  // namespace blender
