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
 */

/** \file
 * \ingroup balembic
 */

#include "abc_reader_transform.h"
#include "abc_util.h"

#include "DNA_object_types.h"

#include "BLI_utildefines.h"

#include "BKE_object.h"

using Alembic::Abc::ISampleSelector;

namespace blender {
namespace io {
namespace alembic {

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
    *err_str =
        "Object type mismatch, Alembic object path pointed to XForm when importing, but not any "
        "more.";
    return false;
  }

  if (ob->type != OB_EMPTY) {
    *err_str = "Object type mismatch, Alembic object path points to XForm.";
    return false;
  }

  return true;
}

void AbcEmptyReader::readObjectData(Main *bmain, const ISampleSelector &UNUSED(sample_sel))
{
  m_object = BKE_object_add_only_object(bmain, OB_EMPTY, m_object_name.c_str());
  m_object->data = NULL;
}

}  // namespace alembic
}  // namespace io
}  // namespace blender
