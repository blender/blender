/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2016 Kévin Dietrich.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#ifndef __ABC_CURVES_H__
#define __ABC_CURVES_H__

#include "abc_object.h"

struct Curve;

/* ************************************************************************** */

class AbcCurveWriter : public AbcObjectWriter {
	Alembic::AbcGeom::OCurvesSchema m_schema;
	Alembic::AbcGeom::OCurvesSchema::Sample m_sample;

public:
	AbcCurveWriter(Scene *scene,
	               Object *ob,
	               AbcTransformWriter *parent,
	               uint32_t time_sampling,
	               ExportSettings &settings);

	void do_write();
};

/* ************************************************************************** */

class AbcCurveReader : public AbcObjectReader {
	Alembic::AbcGeom::ICurvesSchema m_curves_schema;

public:
	AbcCurveReader(const Alembic::Abc::IObject &object, ImportSettings &settings);

	bool valid() const;
	bool accepts_object_type(const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
	                         const Object *const ob,
	                         const char **err_str) const;

	void readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel);
	DerivedMesh *read_derivedmesh(DerivedMesh *dm,
	                              const Alembic::Abc::ISampleSelector &sample_sel,
	                              int read_flag,
	                              const char **err_str);
};

/* ************************************************************************** */

void read_curve_sample(Curve *cu,
                       const Alembic::AbcGeom::ICurvesSchema &schema,
                       const Alembic::Abc::ISampleSelector &sample_selector);

#endif  /* __ABC_CURVES_H__ */
