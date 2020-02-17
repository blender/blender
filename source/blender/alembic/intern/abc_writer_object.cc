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

#include "abc_writer_object.h"

extern "C" {
#include "DNA_object_types.h"

#include "BKE_object.h"
}

AbcObjectWriter::AbcObjectWriter(Object *ob,
                                 uint32_t time_sampling,
                                 ExportSettings &settings,
                                 AbcObjectWriter *parent)
    : m_object(ob), m_settings(settings), m_time_sampling(time_sampling), m_first_frame(true)
{
  m_name = get_id_name(m_object) + "Shape";

  if (parent) {
    parent->addChild(this);
  }
}

AbcObjectWriter::~AbcObjectWriter()
{
}

void AbcObjectWriter::addChild(AbcObjectWriter *child)
{
  m_children.push_back(child);
}

Imath::Box3d AbcObjectWriter::bounds()
{
  BoundBox *bb = BKE_object_boundbox_get(this->m_object);

  if (!bb) {
    if (this->m_object->type != OB_CAMERA) {
      ABC_LOG(m_settings.logger) << "Bounding box is null!\n";
    }

    return Imath::Box3d();
  }

  /* Convert Z-up to Y-up. This also changes which vector goes into which min/max property. */
  this->m_bounds.min.x = bb->vec[0][0];
  this->m_bounds.min.y = bb->vec[0][2];
  this->m_bounds.min.z = -bb->vec[6][1];

  this->m_bounds.max.x = bb->vec[6][0];
  this->m_bounds.max.y = bb->vec[6][2];
  this->m_bounds.max.z = -bb->vec[0][1];

  return this->m_bounds;
}

void AbcObjectWriter::write()
{
  do_write();
  m_first_frame = false;
}
