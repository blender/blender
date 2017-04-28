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

#ifndef __ABC_POINTS_H__
#define __ABC_POINTS_H__

#include "abc_object.h"
#include "abc_customdata.h"

struct ParticleSystem;

/* ************************************************************************** */

class AbcPointsWriter : public AbcObjectWriter {
	Alembic::AbcGeom::OPointsSchema m_schema;
	Alembic::AbcGeom::OPointsSchema::Sample m_sample;
	ParticleSystem *m_psys;

public:
	AbcPointsWriter(Scene *scene,
	                Object *ob,
	                AbcTransformWriter *parent,
	                uint32_t time_sampling,
	                ExportSettings &settings,
	                ParticleSystem *psys);

	void do_write();
};

/* ************************************************************************** */

class AbcPointsReader : public AbcObjectReader {
	Alembic::AbcGeom::IPointsSchema m_schema;
	Alembic::AbcGeom::IPointsSchema::Sample m_sample;

public:
	AbcPointsReader(const Alembic::Abc::IObject &object, ImportSettings &settings);

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

void read_points_sample(const Alembic::AbcGeom::IPointsSchema &schema,
                        const Alembic::AbcGeom::ISampleSelector &selector,
                        CDStreamConfig &config);

#endif  /* __ABC_POINTS_H__ */
