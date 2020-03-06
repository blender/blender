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

#ifndef __ABC_WRITER_TRANSFORM_H__
#define __ABC_WRITER_TRANSFORM_H__

#include "abc_writer_object.h"

#include <Alembic/AbcGeom/All.h>

class AbcTransformWriter : public AbcObjectWriter {
  Alembic::AbcGeom::OXform m_xform;
  Alembic::AbcGeom::OXformSchema m_schema;
  Alembic::AbcGeom::XformSample m_sample;
  Alembic::AbcGeom::OVisibilityProperty m_visibility;
  Alembic::Abc::M44d m_matrix;

  bool m_is_animated;
  bool m_inherits_xform;

 public:
  Object *m_proxy_from;

 public:
  AbcTransformWriter(Object *ob,
                     const Alembic::AbcGeom::OObject &abc_parent,
                     AbcTransformWriter *parent,
                     unsigned int time_sampling,
                     ExportSettings &settings);

  Alembic::AbcGeom::OXform &alembicXform()
  {
    return m_xform;
  }
  virtual Imath::Box3d bounds();

 private:
  virtual void do_write();

  bool hasAnimation(Object *ob) const;
};

#endif /* __ABC_WRITER_TRANSFORM_H__ */
