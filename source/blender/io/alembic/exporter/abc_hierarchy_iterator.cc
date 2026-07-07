/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "abc_hierarchy_iterator.h"
#include "abc_writer_abstract.h"
#include "abc_writer_camera.h"
#include "abc_writer_curves.h"
#include "abc_writer_hair.h"
#include "abc_writer_instance.h"
#include "abc_writer_mball.h"
#include "abc_writer_mesh.h"
#include "abc_writer_nurbs.h"
#include "abc_writer_points.h"
#include "abc_writer_transform.h"
#include "intern/abc_util.h"

#include <memory>
#include <string>

#include "BLI_assert.h"

#include "DNA_layer_types.h"
#include "DNA_object_types.h"

namespace blender::io::alembic {

ABCHierarchyIterator::ABCHierarchyIterator(Main *bmain,
                                           Depsgraph *depsgraph,
                                           ABCArchive *abc_archive,
                                           const AlembicExportParams &params)
    : AbstractHierarchyIterator(bmain, depsgraph), abc_archive_(abc_archive), params_(params)
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
    AbstractHierarchyWriter *abstract_writer = writers_.lookup(context->export_path);
    ABCAbstractWriter *abc_writer = static_cast<ABCAbstractWriter *>(abstract_writer);

    if (abc_writer != nullptr) {
      bounds.extendBy(abc_writer->bounding_box());
    }
  }

  ExportChildren *children = graph_children(context);
  if (!children) {
    return;
  }

  for (HierarchyContext *child_context : *children) {
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
  return get_valid_abc_name(name.c_str());
}

ObjectIdentifier ABCHierarchyIterator::determine_graph_index_object(
    const HierarchyContext *context)
{
  if (params_.flatten_hierarchy) {
    return ObjectIdentifier::for_graph_root();
  }

  return AbstractHierarchyIterator::determine_graph_index_object(context);
}

ObjectIdentifier ABCHierarchyIterator::determine_graph_index_dupli(
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

Alembic::Abc::OObject ABCHierarchyIterator::get_alembic_object(
    const std::string &export_path) const
{
  if (export_path.empty()) {
    return Alembic::Abc::OObject();
  }

  AbstractHierarchyWriter *writer = get_writer(export_path);
  if (writer == nullptr) {
    return Alembic::Abc::OObject();
  }

  ABCAbstractWriter *abc_writer = static_cast<ABCAbstractWriter *>(writer);
  return abc_writer->get_alembic_object();
}

Alembic::Abc::OObject ABCHierarchyIterator::get_alembic_parent(
    const HierarchyContext *context) const
{
  Alembic::Abc::OObject parent = get_alembic_object(context->higher_up_export_path);

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

  if (params_.use_instancing && context->is_instance()) {
    data_writer = new ABCInstanceWriter(writer_args);
  }
  else {
    data_writer = create_data_writer_for_object_type(context, writer_args);
  }

  if (data_writer == nullptr || !data_writer->is_supported(context)) {
    delete data_writer;
    return nullptr;
  }

  data_writer->create_alembic_objects(context);
  return data_writer;
}

ABCAbstractWriter *ABCHierarchyIterator::create_data_writer_for_object_type(
    const HierarchyContext *context, const ABCWriterConstructorArgs &writer_args)
{
  switch (context->object->type) {
    case OB_MESH:
      return new ABCMeshWriter(writer_args);
    case OB_CAMERA:
      return new ABCCameraWriter(writer_args);
    case OB_CURVES_LEGACY:
    case OB_CURVES:
      if (params_.curves_as_mesh) {
        return new ABCCurveMeshWriter(writer_args);
      }
      return new ABCCurveWriter(writer_args);
    case OB_SURF:
      if (params_.curves_as_mesh) {
        return new ABCCurveMeshWriter(writer_args);
      }
      return new ABCNurbsWriter(writer_args);
    case OB_MBALL:
      return new ABCMetaballWriter(writer_args);

    case OB_EMPTY:
    case OB_LAMP:
    case OB_FONT:
    case OB_SPEAKER:
    case OB_LIGHTPROBE:
    case OB_LATTICE:
    case OB_ARMATURE:
      return nullptr;
    case OB_TYPE_MAX:
      BLI_assert_msg(0, "OB_TYPE_MAX should not be used");
      return nullptr;
  }

  /* Just to please the compiler, all cases should be handled by the above switch. */
  return nullptr;
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

}  // namespace blender::io::alembic
