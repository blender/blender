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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Esteban Tovagliari, Cedric Paille, Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __ABC_HAIR_H__
#define __ABC_HAIR_H__

#include "abc_object.h"

struct DerivedMesh;
struct ParticleSettings;
struct ParticleSystem;

/* ************************************************************************** */

class AbcHairWriter : public AbcObjectWriter {
	ParticleSystem *m_psys;

	Alembic::AbcGeom::OCurvesSchema m_schema;
	Alembic::AbcGeom::OCurvesSchema::Sample m_sample;

	bool m_uv_warning_shown;

public:
	AbcHairWriter(Scene *scene,
	              Object *ob,
	              AbcTransformWriter *parent,
	              uint32_t time_sampling,
	              ExportSettings &settings,
	              ParticleSystem *psys);

private:
	virtual void do_write();

	void write_hair_sample(DerivedMesh *dm,
	                       ParticleSettings *part,
	                       std::vector<Imath::V3f> &verts,
	                       std::vector<Imath::V3f> &norm_values,
	                       std::vector<Imath::V2f> &uv_values,
	                       std::vector<int32_t> &hvertices);

	void write_hair_child_sample(DerivedMesh *dm,
	                             ParticleSettings *part,
	                             std::vector<Imath::V3f> &verts,
	                             std::vector<Imath::V3f> &norm_values,
	                             std::vector<Imath::V2f> &uv_values,
	                             std::vector<int32_t> &hvertices);
};

#endif  /* __ABC_HAIR_H__ */
