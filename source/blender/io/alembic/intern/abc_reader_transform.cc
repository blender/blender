/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_reader_transform.h"
#include "abc_util.h"

#include "DNA_object_types.h"

#include "BLT_translation.hh"

#include "BKE_object.hh"

namespace blender {

using Alembic::Abc::ISampleSelector;

namespace io::alembic {

AbcEmptyReader::AbcEmptyReader(const AbcReaderConstructorArgs &args) : AbcObjectReader(args)
{
  /* Empties have no data. It makes the import of Alembic files easier to
   * understand when we name the empty after its name in Alembic. */
  m_object_name = m_iobject.getName();

  Alembic::AbcGeom::IXform xform(m_iobject, Alembic::AbcGeom::kWrapExisting);
  m_schema = xform.getSchema();
}

bool AbcEmptyReader::valid() const
{
  return m_schema.valid();
}

bool AbcEmptyReader::accepts_object_type(
    const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
    const Object *const ob,
    const char **r_err_str) const
{
  if (!Alembic::AbcGeom::IXform::matches(alembic_header)) {
    *r_err_str = RPT_(
        "Object type mismatch, Alembic object path pointed to XForm when importing, but not any "
        "more");
    return false;
  }

  if (ob->type != OB_EMPTY) {
    *r_err_str = RPT_("Object type mismatch, Alembic object path points to XForm");
    return false;
  }

  return true;
}

void AbcEmptyReader::readObjectData(Main *bmain, const ISampleSelector & /*sample_sel*/)
{
  m_object = BKE_object_add_only_object(bmain, OB_EMPTY, m_object_name.c_str());
  m_object->data = nullptr;
}

}  // namespace io::alembic
}  // namespace blender
