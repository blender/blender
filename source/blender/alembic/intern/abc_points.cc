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

#include "abc_points.h"

#include "abc_mesh.h"
#include "abc_transform.h"
#include "abc_util.h"

extern "C" {
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"

#include "BLI_math.h"
}

using Alembic::AbcGeom::kVertexScope;
using Alembic::AbcGeom::kWrapExisting;
using Alembic::AbcGeom::P3fArraySamplePtr;
using Alembic::AbcGeom::N3fArraySamplePtr;

using Alembic::AbcGeom::ICompoundProperty;
using Alembic::AbcGeom::IN3fArrayProperty;
using Alembic::AbcGeom::IPoints;
using Alembic::AbcGeom::IPointsSchema;
using Alembic::AbcGeom::ISampleSelector;

using Alembic::AbcGeom::OPoints;
using Alembic::AbcGeom::OPointsSchema;

/* ************************************************************************** */

AbcPointsWriter::AbcPointsWriter(Scene *scene,
                                 Object *ob,
                                 AbcTransformWriter *parent,
                                 uint32_t time_sampling,
                                 ExportSettings &settings,
                                 ParticleSystem *psys)
    : AbcObjectWriter(scene, ob, time_sampling, settings, parent)
{
	m_psys = psys;

	OPoints points(parent->alembicXform(), psys->name, m_time_sampling);
	m_schema = points.getSchema();
}

void AbcPointsWriter::do_write()
{
	if (!m_psys) {
		return;
	}

	std::vector<Imath::V3f> points;
	std::vector<Imath::V3f> velocities;
	std::vector<float> widths;
	std::vector<uint64_t> ids;

	ParticleKey state;

	ParticleSimulationData sim;
	sim.scene = m_scene;
	sim.ob = m_object;
	sim.psys = m_psys;

	m_psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

	uint64_t index = 0;
	for (int p = 0; p < m_psys->totpart; p++) {
		float pos[3], vel[3];

		if (m_psys->particles[p].flag & (PARS_NO_DISP | PARS_UNEXIST)) {
			continue;
		}

		state.time = BKE_scene_frame_get(m_scene);

		if (psys_get_particle_state(&sim, p, &state, 0) == 0) {
			continue;
		}

		/* location */
		mul_v3_m4v3(pos, m_object->imat, state.co);

		/* velocity */
		sub_v3_v3v3(vel, state.co, m_psys->particles[p].prev_state.co);

		/* Convert Z-up to Y-up. */
		points.push_back(Imath::V3f(pos[0], pos[2], -pos[1]));
		velocities.push_back(Imath::V3f(vel[0], vel[2], -vel[1]));
		widths.push_back(m_psys->particles[p].size);
		ids.push_back(index++);
	}

	if (m_psys->lattice_deform_data) {
		end_latt_deform(m_psys->lattice_deform_data);
		m_psys->lattice_deform_data = NULL;
	}

	Alembic::Abc::P3fArraySample psample(points);
	Alembic::Abc::UInt64ArraySample idsample(ids);
	Alembic::Abc::V3fArraySample vsample(velocities);
	Alembic::Abc::FloatArraySample wsample_array(widths);
	Alembic::AbcGeom::OFloatGeomParam::Sample wsample(wsample_array, kVertexScope);

	m_sample = OPointsSchema::Sample(psample, idsample, vsample, wsample);
	m_sample.setSelfBounds(bounds());

	m_schema.set(m_sample);
}

/* ************************************************************************** */

AbcPointsReader::AbcPointsReader(const Alembic::Abc::IObject &object, ImportSettings &settings)
    : AbcObjectReader(object, settings)
{
	IPoints ipoints(m_iobject, kWrapExisting);
	m_schema = ipoints.getSchema();
	get_min_max_time(m_iobject, m_schema, m_min_time, m_max_time);
}

bool AbcPointsReader::valid() const
{
	return m_schema.valid();
}

bool AbcPointsReader::accepts_object_type(const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
                                          const Object *const ob,
                                          const char **err_str) const
{
	if (!Alembic::AbcGeom::IPoints::matches(alembic_header)) {
		*err_str = "Object type mismatch, Alembic object path pointed to Points when importing, but not any more.";
		return false;
	}

	if (ob->type != OB_MESH) {
		*err_str = "Object type mismatch, Alembic object path points to Points.";
		return false;
	}

	return true;
}

void AbcPointsReader::readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel)
{
	Mesh *mesh = BKE_mesh_add(bmain, m_data_name.c_str());

	DerivedMesh *dm = CDDM_from_mesh(mesh);
	DerivedMesh *ndm = this->read_derivedmesh(dm, sample_sel, 0, NULL);

	if (ndm != dm) {
		dm->release(dm);
	}

	DM_to_mesh(ndm, mesh, m_object, CD_MASK_MESH, true);

	if (m_settings->validate_meshes) {
		BKE_mesh_validate(mesh, false, false);
	}

	m_object = BKE_object_add_only_object(bmain, OB_MESH, m_object_name.c_str());
	m_object->data = mesh;

	if (has_animations(m_schema, m_settings)) {
		addCacheModifier();
	}
}

void read_points_sample(const IPointsSchema &schema,
                        const ISampleSelector &selector,
                        CDStreamConfig &config)
{
	Alembic::AbcGeom::IPointsSchema::Sample sample = schema.getValue(selector);

	const P3fArraySamplePtr &positions = sample.getPositions();

	ICompoundProperty prop = schema.getArbGeomParams();
	N3fArraySamplePtr vnormals;

	if (has_property(prop, "N")) {
		const Alembic::Util::uint32_t itime = static_cast<Alembic::Util::uint32_t>(selector.getRequestedTime());
		const IN3fArrayProperty &normals_prop = IN3fArrayProperty(prop, "N", itime);

		if (normals_prop) {
			vnormals = normals_prop.getValue(selector);
		}
	}

	read_mverts(config.mvert, positions, vnormals);
}

DerivedMesh *AbcPointsReader::read_derivedmesh(DerivedMesh *dm,
                                               const ISampleSelector &sample_sel,
                                               int /*read_flag*/,
                                               const char ** /*err_str*/)
{
	const IPointsSchema::Sample sample = m_schema.getValue(sample_sel);

	const P3fArraySamplePtr &positions = sample.getPositions();

	DerivedMesh *new_dm = NULL;

	if (dm->getNumVerts(dm) != positions->size()) {
		new_dm = CDDM_new(positions->size(), 0, 0, 0, 0);
	}

	CDStreamConfig config = get_config(new_dm ? new_dm : dm);
	read_points_sample(m_schema, sample_sel, config);

	return new_dm ? new_dm : dm;
}
