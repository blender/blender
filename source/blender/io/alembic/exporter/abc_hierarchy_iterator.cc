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

#include "abc_hierarchy_iterator.h"
#include "abc_writer_abstract.h"
#include "abc_writer_camera.h"
#include "abc_writer_curves.h"
#include "abc_writer_hair.h"
#include "abc_writer_mball.h"
#include "abc_writer_mesh.h"
#include "abc_writer_nurbs.h"
#include "abc_writer_points.h"
#include "abc_writer_transform.h"

#include <memory>
#include <string>

#include "BLI_assert.h"

#include "DEG_depsgraph_query.h"

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"

namespace blender {
namespace io {
namespace alembic {

ABCHierarchyIterator::ABCHierarchyIterator(Depsgraph *depsgraph,
                                           ABCArchive *abc_archive,
                                           const AlembicExportParams &params)
    : AbstractHierarchyIterator(depsgraph), abc_archive_(abc_archive), params_(params)
{
}

void ABCHierarchyIterator::iterate_and_write()
{
  AbstractHierarchyIterator::iterate_and_write();
  update_archive_bounding_box();
}

void ABCHierarchyIterator::update_archive_bounding_box()
{
  Imath::Box3d bounds;
  update_bounding_box_recursive(bounds, HierarchyContext::root());
  abc_archive_->update_bounding_box(bounds);
}

void ABCHierarchyIterator::update_bounding_box_recursive(Imath::Box3d &bounds,
                                                         const HierarchyContext *context)
{
  if (context != nullptr) {
    AbstractHierarchyWriter *abstract_writer = writers_[context->export_path];
    ABCAbstractWriter *abc_writer = static_cast<ABCAbstractWriter *>(abstract_writer);

    if (abc_writer != nullptr) {
      bounds.extendBy(abc_writer->bounding_box());
    }
  }

  for (HierarchyContext *child_context : graph_children(context)) {
    update_bounding_box_recursive(bounds, child_context);
  }
}

bool ABCHierarchyIterator::mark_as_weak_export(const Object *object) const
{
  if (params_.selected_only && (object->base_flag & BASE_SELECTED) == 0) {
    return true;
  }
  /* TODO(Sybren): handle other flags too? */
  return false;
}

void ABCHierarchyIterator::release_writer(AbstractHierarchyWriter *writer)
{
  delete writer;
}

std::string ABCHierarchyIterator::make_valid_name(const std::string &name) const
{
  std::string abc_name(name);
  std::replace(abc_name.begin(), abc_name.end(), ' ', '_');
  std::replace(abc_name.begin(), abc_name.end(), '.', '_');
  std::replace(abc_name.begin(), abc_name.end(), ':', '_');
  return abc_name;
}

AbstractHierarchyIterator::ExportGraph::key_type ABCHierarchyIterator::
    determine_graph_index_object(const HierarchyContext *context)
{
  if (params_.flatten_hierarchy) {
    return ObjectIdentifier::for_graph_root();
  }

  return AbstractHierarchyIterator::determine_graph_index_object(context);
}

AbstractHierarchyIterator::ExportGraph::key_type ABCHierarchyIterator::determine_graph_index_dupli(
    const HierarchyContext *context,
    const DupliObject *dupli_object,
    const DupliParentFinder &dupli_parent_finder)
{
  if (params_.flatten_hierarchy) {
    return ObjectIdentifier::for_graph_root();
  }

  return AbstractHierarchyIterator::determine_graph_index_dupli(
      context, dupli_object, dupli_parent_finder);
}

Alembic::Abc::OObject ABCHierarchyIterator::get_alembic_parent(
    const HierarchyContext *context) const
{
  Alembic::Abc::OObject parent;

  if (!context->higher_up_export_path.empty()) {
    AbstractHierarchyWriter *writer = get_writer(context->higher_up_export_path);
    ABCAbstractWriter *abc_writer = static_cast<ABCAbstractWriter *>(writer);
    parent = abc_writer->get_alembic_object();
  }

  if (!parent.valid()) {
    /* An invalid parent object means "no parent", which should be translated to Alembic's top
     * archive object. */
    return abc_archive_->archive->getTop();
  }

  return parent;
}

ABCWriterConstructorArgs ABCHierarchyIterator::writer_constructor_args(
    const HierarchyContext *context) const
{
  ABCWriterConstructorArgs constructor_args;
  constructor_args.depsgraph = depsgraph_;
  constructor_args.abc_archive = abc_archive_;
  constructor_args.abc_parent = get_alembic_parent(context);
  constructor_args.abc_name = context->export_name;
  constructor_args.abc_path = context->export_path;
  constructor_args.hierarchy_iterator = this;
  constructor_args.export_params = &params_;
  return constructor_args;
}

AbstractHierarchyWriter *ABCHierarchyIterator::create_transform_writer(
    const HierarchyContext *context)
{
  ABCAbstractWriter *transform_writer = new ABCTransformWriter(writer_constructor_args(context));
  transform_writer->create_alembic_objects(context);
  return transform_writer;
}

AbstractHierarchyWriter *ABCHierarchyIterator::create_data_writer(const HierarchyContext *context)
{
  const ABCWriterConstructorArgs writer_args = writer_constructor_args(context);
  ABCAbstractWriter *data_writer = nullptr;

  switch (context->object->type) {
    case OB_MESH:
      data_writer = new ABCMeshWriter(writer_args);
      break;
    case OB_CAMERA:
      data_writer = new ABCCameraWriter(writer_args);
      break;
    case OB_CURVE:
      if (params_.curves_as_mesh) {
        data_writer = new ABCCurveMeshWriter(writer_args);
      }
      else {
        data_writer = new ABCCurveWriter(writer_args);
      }
      break;
    case OB_SURF:
      if (params_.curves_as_mesh) {
        data_writer = new ABCCurveMeshWriter(writer_args);
      }
      else {
        data_writer = new ABCNurbsWriter(writer_args);
      }
      break;
    case OB_MBALL:
      data_writer = new ABCMetaballWriter(writer_args);
      break;

    case OB_EMPTY:
    case OB_LAMP:
    case OB_FONT:
    case OB_SPEAKER:
    case OB_LIGHTPROBE:
    case OB_LATTICE:
    case OB_ARMATURE:
    case OB_GPENCIL:
      return nullptr;
    case OB_TYPE_MAX:
      BLI_assert(!"OB_TYPE_MAX should not be used");
      return nullptr;
  }

  if (!data_writer->is_supported(context)) {
    delete data_writer;
    return nullptr;
  }

  data_writer->create_alembic_objects(context);
  return data_writer;
}

AbstractHierarchyWriter *ABCHierarchyIterator::create_hair_writer(const HierarchyContext *context)
{
  if (!params_.export_hair) {
    return nullptr;
  }

  const ABCWriterConstructorArgs writer_args = writer_constructor_args(context);
  ABCAbstractWriter *hair_writer = new ABCHairWriter(writer_args);

  if (!hair_writer->is_supported(context)) {
    delete hair_writer;
    return nullptr;
  }

  hair_writer->create_alembic_objects(context);
  return hair_writer;
}

AbstractHierarchyWriter *ABCHierarchyIterator::create_particle_writer(
    const HierarchyContext *context)
{
  if (!params_.export_particles) {
    return nullptr;
  }

  const ABCWriterConstructorArgs writer_args = writer_constructor_args(context);
  std::unique_ptr<ABCPointsWriter> particle_writer(std::make_unique<ABCPointsWriter>(writer_args));

  if (!particle_writer->is_supported(context)) {
    return nullptr;
  }

  particle_writer->create_alembic_objects(context);
  return particle_writer.release();
}

}  // namespace alembic
}  // namespace io
}  // namespace blender
