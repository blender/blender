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

#ifndef __ABC_MESH_H__
#define __ABC_MESH_H__

#include "abc_customdata.h"
#include "abc_object.h"

struct Mesh;
struct ModifierData;

/* ************************************************************************** */

class AbcMeshWriter : public AbcObjectWriter {
	Alembic::AbcGeom::OPolyMeshSchema m_mesh_schema;
	Alembic::AbcGeom::OPolyMeshSchema::Sample m_mesh_sample;

	Alembic::AbcGeom::OSubDSchema m_subdiv_schema;
	Alembic::AbcGeom::OSubDSchema::Sample m_subdiv_sample;

	Alembic::Abc::OArrayProperty m_mat_indices;

	bool m_is_animated;
	ModifierData *m_subsurf_mod;

	CDStreamConfig m_custom_data_config;

	bool m_is_liquid;
	bool m_is_subd;

public:
	AbcMeshWriter(Object *ob,
	              AbcTransformWriter *parent,
	              uint32_t time_sampling,
	              ExportSettings &settings);

	~AbcMeshWriter();
	void setIsAnimated(bool is_animated);

private:
	virtual void do_write();

	bool isAnimated() const;

	void writeMesh(struct Mesh *mesh);
	void writeSubD(struct Mesh *mesh);

	void getMeshInfo(struct Mesh *mesh, std::vector<float> &points,
	                 std::vector<int32_t> &facePoints,
	                 std::vector<int32_t> &faceCounts,
	                 std::vector<int32_t> &creaseIndices,
	                 std::vector<int32_t> &creaseLengths,
	                 std::vector<float> &creaseSharpness);

	struct Mesh *getFinalMesh(bool &r_needsfree);

	void getMaterialIndices(struct Mesh *mesh, std::vector<int32_t> &indices);

	void writeArbGeoParams(struct Mesh *mesh);
	void getGeoGroups(struct Mesh *mesh, std::map<std::string, std::vector<int32_t> > &geoGroups);

	/* fluid surfaces support */
	void getVelocities(struct Mesh *mesh, std::vector<Imath::V3f> &vels);

	template <typename Schema>
	void writeFaceSets(struct Mesh *mesh, Schema &schema);
};

/* ************************************************************************** */

class AbcMeshReader : public AbcObjectReader {
	Alembic::AbcGeom::IPolyMeshSchema m_schema;

	CDStreamConfig m_mesh_data;

public:
	AbcMeshReader(const Alembic::Abc::IObject &object, ImportSettings &settings);

	bool valid() const;
	bool accepts_object_type(const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
	                       const Object *const ob,
	                       const char **err_str) const;
	void readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel);

	struct Mesh *read_mesh(struct Mesh *existing_mesh,
	                       const Alembic::Abc::ISampleSelector &sample_sel,
	                       int read_flag,
	                       const char **err_str);

private:
	void readFaceSetsSample(Main *bmain, Mesh *mesh, size_t poly_start,
	                        const Alembic::AbcGeom::ISampleSelector &sample_sel);

	void assign_facesets_to_mpoly(const Alembic::Abc::ISampleSelector &sample_sel,
	                              size_t poly_start,
	                              MPoly *mpoly, int totpoly,
	                              std::map<std::string, int> & r_mat_map);
};

/* ************************************************************************** */

class AbcSubDReader : public AbcObjectReader {
	Alembic::AbcGeom::ISubDSchema m_schema;

	CDStreamConfig m_mesh_data;

public:
	AbcSubDReader(const Alembic::Abc::IObject &object, ImportSettings &settings);

	bool valid() const;
	bool accepts_object_type(const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
	                         const Object *const ob,
	                         const char **err_str) const;
	void readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel);
	struct Mesh *read_mesh(struct Mesh *existing_mesh,
	                       const Alembic::Abc::ISampleSelector &sample_sel,
	                       int read_flag,
	                       const char **err_str);
};

/* ************************************************************************** */

void read_mverts(MVert *mverts,
                 const Alembic::AbcGeom::P3fArraySamplePtr &positions,
                 const Alembic::AbcGeom::N3fArraySamplePtr &normals);

CDStreamConfig get_config(struct Mesh *mesh);

#endif  /* __ABC_MESH_H__ */
