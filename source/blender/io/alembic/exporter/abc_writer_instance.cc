/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_writer_instance.h"
#include "abc_hierarchy_iterator.h"

#include "BLI_assert.h"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.alembic"};

namespace blender::io::alembic {

using Alembic::Abc::OObject;

ABCInstanceWriter::ABCInstanceWriter(const ABCWriterConstructorArgs &args)
    : ABCAbstractWriter(args)
{
}

void ABCInstanceWriter::create_alembic_objects(const HierarchyContext *context)
{
  OObject original = args_.hierarchy_iterator->get_alembic_object(context->original_export_path);
  OObject abc_parent = args_.abc_parent;
  if (!abc_parent.addChildInstance(original, args_.abc_name)) {
    CLOG_WARN(&LOG, "unable to export %s as instance", args_.abc_path.c_str());
    return;
  }
  CLOG_INFO(&LOG, 2, "exporting instance %s", args_.abc_path.c_str());
}

void ABCInstanceWriter::ensure_custom_properties_exporter(const HierarchyContext & /*context*/)
{
  /* Intentionally do nothing. Instances should not have their own custom properties. */
}

Alembic::Abc::OCompoundProperty ABCInstanceWriter::abc_prop_for_custom_props()
{
  return Alembic::Abc::OCompoundProperty();
}

OObject ABCInstanceWriter::get_alembic_object() const
{
  /* There is no OObject for an instance. */
  BLI_assert_msg(0, "ABCInstanceWriter cannot return its Alembic OObject");
  return OObject();
}

bool ABCInstanceWriter::is_supported(const HierarchyContext *context) const
{
  return context->is_instance();
}

void ABCInstanceWriter::do_write(HierarchyContext & /*context*/)
{
  /* Instances don't have data to be written. Just creating them is enough. */
}

}  // namespace blender::io::alembic
