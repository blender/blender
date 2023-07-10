/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_reader_transform.h"
#include "abc_util.h"

#include "DNA_object_types.h"

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_object.h"

using Alembic::Abc::ISampleSelector;

namespace blender::io::alembic {

AbcEmptyReader::AbcEmptyReader(const Alembic::Abc::IObject &object, ImportSettings &settings)
    : AbcObjectReader(object, settings)
{
  /* Empties have no data. It makes the import of Alembic files easier to
   * understand when we name the empty after its name in Alembic. */
  m_object_name = object.getName();

  Alembic::AbcGeom::IXform xform(object, Alembic::AbcGeom::kWrapExisting);
  m_schema = xform.getSchema();

  get_min_max_time(m_iobject, m_schema, m_min_time, m_max_time);
}

bool AbcEmptyReader::valid() const
{
  return m_schema.valid();
}

bool AbcEmptyReader::accepts_object_type(
    const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
    const Object *const ob,
    const char **err_str) const
{
  if (!Alembic::AbcGeom::IXform::matches(alembic_header)) {
    *err_str = TIP_(
        "Object type mismatch, Alembic object path pointed to XForm when importing, but not any "
        "more");
    return false;
  }

  if (ob->type != OB_EMPTY) {
    *err_str = TIP_("Object type mismatch, Alembic object path points to XForm");
    return false;
  }

  return true;
}

void AbcEmptyReader::readObjectData(Main *bmain, const ISampleSelector & /*sample_sel*/)
{
  m_object = BKE_object_add_only_object(bmain, OB_EMPTY, m_object_name.c_str());
  m_object->data = nullptr;
}

}  // namespace blender::io::alembic
