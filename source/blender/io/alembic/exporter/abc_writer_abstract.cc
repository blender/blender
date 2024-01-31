/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "abc_writer_abstract.h"
#include "abc_hierarchy_iterator.h"

#include "BKE_animsys.h"
#include "BKE_key.hh"
#include "BKE_object.hh"

#include "DNA_modifier_types.h"

#include "DEG_depsgraph.hh"

#include <Alembic/AbcGeom/Visibility.h>

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.alembic"};

namespace blender::io::alembic {

using Alembic::Abc::OObject;
using Alembic::Abc::TimeSamplingPtr;

ABCAbstractWriter::ABCAbstractWriter(const ABCWriterConstructorArgs &args)
    : args_(args),
      frame_has_been_written_(false),
      is_animated_(false),
      timesample_index_(args_.abc_archive->time_sampling_index_shapes())
{
}

bool ABCAbstractWriter::is_supported(const HierarchyContext * /*context*/) const
{
  return true;
}

void ABCAbstractWriter::write(HierarchyContext &context)
{
  if (!frame_has_been_written_) {
    is_animated_ = (args_.export_params->frame_start != args_.export_params->frame_end) &&
                   check_is_animated(context);
    ensure_custom_properties_exporter(context);
  }
  else if (!is_animated_) {
    /* A frame has already been written, and without animation one frame is enough. */
    return;
  }

  do_write(context);

  if (custom_props_) {
    custom_props_->write_all(get_id_properties(context));
  }

  frame_has_been_written_ = true;
}

void ABCAbstractWriter::ensure_custom_properties_exporter(const HierarchyContext &context)
{
  if (!args_.export_params->export_custom_properties) {
    return;
  }

  if (custom_props_) {
    /* Custom properties exporter already created. */
    return;
  }

  /* Avoid creating a custom properties exporter if there are no custom properties to export. */
  const IDProperty *id_properties = get_id_properties(context);
  if (id_properties == nullptr || id_properties->len == 0) {
    return;
  }

  custom_props_ = std::make_unique<CustomPropertiesExporter>(this);
}

const IDProperty *ABCAbstractWriter::get_id_properties(const HierarchyContext &context) const
{
  Object *object = context.object;
  if (object->data == nullptr) {
    return nullptr;
  }

  /* Most subclasses write object data, so default to the object data's ID properties. */
  return static_cast<ID *>(object->data)->properties;
}

uint32_t ABCAbstractWriter::timesample_index() const
{
  return timesample_index_;
}

const Imath::Box3d &ABCAbstractWriter::bounding_box() const
{
  return bounding_box_;
}

void ABCAbstractWriter::update_bounding_box(Object *object)
{
  const std::optional<Bounds<float3>> bounds = BKE_object_boundbox_get(object);
  if (!bounds) {
    if (object->type != OB_CAMERA) {
      CLOG_WARN(&LOG, "Bounding box is null!");
    }
    bounding_box_.min.x = bounding_box_.min.y = bounding_box_.min.z = 0;
    bounding_box_.max.x = bounding_box_.max.y = bounding_box_.max.z = 0;
    return;
  }

  BoundBox bb;
  BKE_boundbox_init_from_minmax(&bb, bounds->min, bounds->max);

  /* Convert Z-up to Y-up. This also changes which vector goes into which min/max property. */
  bounding_box_.min.x = bb.vec[0][0];
  bounding_box_.min.y = bb.vec[0][2];
  bounding_box_.min.z = -bb.vec[6][1];

  bounding_box_.max.x = bb.vec[6][0];
  bounding_box_.max.y = bb.vec[6][2];
  bounding_box_.max.z = -bb.vec[0][1];
}

void ABCAbstractWriter::write_visibility(const HierarchyContext &context)
{
  const bool is_visible = context.is_object_visible(args_.export_params->evaluation_mode);
  Alembic::Abc::OObject abc_object = get_alembic_object();

  if (!abc_visibility_.valid()) {
    abc_visibility_ = Alembic::AbcGeom::CreateVisibilityProperty(abc_object, timesample_index_);
  }
  abc_visibility_.set(is_visible ? Alembic::AbcGeom::kVisibilityVisible :
                                   Alembic::AbcGeom::kVisibilityHidden);
}

}  // namespace blender::io::alembic
