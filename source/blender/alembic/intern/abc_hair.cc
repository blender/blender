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

#include "abc_hair.h"

#include <cstdio>

#include "abc_transform.h"
#include "abc_util.h"

extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_modifier_types.h"

#include "BLI_listbase.h"
#include "BLI_math_geom.h"

#include "BKE_DerivedMesh.h"
#include "BKE_object.h"
#include "BKE_particle.h"
}

using Alembic::Abc::P3fArraySamplePtr;

using Alembic::AbcGeom::OCurves;
using Alembic::AbcGeom::OCurvesSchema;
using Alembic::AbcGeom::ON3fGeomParam;
using Alembic::AbcGeom::OV2fGeomParam;

/* ************************************************************************** */

AbcHairWriter::AbcHairWriter(Scene *scene,
                             Object *ob,
                             AbcTransformWriter *parent,
                             uint32_t time_sampling,
                             ExportSettings &settings,
                             ParticleSystem *psys)
    : AbcObjectWriter(scene, ob, time_sampling, settings, parent)
    , m_uv_warning_shown(false)
{
	m_psys = psys;

	OCurves curves(parent->alembicXform(), psys->name, m_time_sampling);
	m_schema = curves.getSchema();
}

void AbcHairWriter::do_write()
{
	if (!m_psys) {
		return;
	}

	ParticleSystemModifierData *psmd = psys_get_modifier(m_object, m_psys);

	if (!psmd->dm_final) {
		return;
	}

	DerivedMesh *dm = mesh_create_derived_render(m_scene, m_object, CD_MASK_MESH);
	DM_ensure_tessface(dm);

	std::vector<Imath::V3f> verts;
	std::vector<int32_t> hvertices;
	std::vector<Imath::V2f> uv_values;
	std::vector<Imath::V3f> norm_values;

	if (m_psys->pathcache) {
		ParticleSettings *part = m_psys->part;

		write_hair_sample(dm, part, verts, norm_values, uv_values, hvertices);

		if (m_settings.export_child_hairs && m_psys->childcache) {
			write_hair_child_sample(dm, part, verts, norm_values, uv_values, hvertices);
		}
	}

	dm->release(dm);

	Alembic::Abc::P3fArraySample iPos(verts);
	m_sample = OCurvesSchema::Sample(iPos, hvertices);
	m_sample.setBasis(Alembic::AbcGeom::kNoBasis);
	m_sample.setType(Alembic::AbcGeom::kLinear);
	m_sample.setWrap(Alembic::AbcGeom::kNonPeriodic);

	if (!uv_values.empty()) {
		OV2fGeomParam::Sample uv_smp;
		uv_smp.setVals(uv_values);
		m_sample.setUVs(uv_smp);
	}

	if (!norm_values.empty()) {
		ON3fGeomParam::Sample norm_smp;
		norm_smp.setVals(norm_values);
		m_sample.setNormals(norm_smp);
	}

	m_sample.setSelfBounds(bounds());
	m_schema.set(m_sample);
}

void AbcHairWriter::write_hair_sample(DerivedMesh *dm,
                                      ParticleSettings *part,
                                      std::vector<Imath::V3f> &verts,
                                      std::vector<Imath::V3f> &norm_values,
                                      std::vector<Imath::V2f> &uv_values,
                                      std::vector<int32_t> &hvertices)
{
	/* Get untransformed vertices, there's a xform under the hair. */
	float inv_mat[4][4];
	invert_m4_m4_safe(inv_mat, m_object->obmat);

	MTFace *mtface = static_cast<MTFace *>(CustomData_get_layer(&dm->faceData, CD_MTFACE));
	MFace *mface = dm->getTessFaceArray(dm);
	MVert *mverts = dm->getVertArray(dm);

	if ((!mtface || !mface) && !m_uv_warning_shown) {
		std::fprintf(stderr, "Warning, no UV set found for underlying geometry of %s.\n",
		             m_object->id.name + 2);
		m_uv_warning_shown = true;
	}

	ParticleData *pa = m_psys->particles;
	int k;

	ParticleCacheKey **cache = m_psys->pathcache;
	ParticleCacheKey *path;
	float normal[3];
	Imath::V3f tmp_nor;

	for (int p = 0; p < m_psys->totpart; ++p, ++pa) {
		/* underlying info for faces-only emission */
		path = cache[p];

		if (part->from == PART_FROM_FACE && mtface) {
			const int num = pa->num_dmcache >= 0 ? pa->num_dmcache : pa->num;

			if (num < dm->getNumTessFaces(dm)) {
				MFace *face = static_cast<MFace *>(dm->getTessFaceData(dm, num, CD_MFACE));
				MTFace *tface = mtface + num;

				if (mface) {
					float r_uv[2], mapfw[4], vec[3];

					psys_interpolate_uvs(tface, face->v4, pa->fuv, r_uv);
					uv_values.push_back(Imath::V2f(r_uv[0], r_uv[1]));

					psys_interpolate_face(mverts, face, tface, NULL, mapfw, vec, normal, NULL, NULL, NULL, NULL);

					copy_yup_from_zup(tmp_nor.getValue(), normal);
					norm_values.push_back(tmp_nor);
				}
			}
			else {
				std::fprintf(stderr, "Particle to faces overflow (%d/%d)\n", num, dm->getNumTessFaces(dm));
			}
		}
		else if (part->from == PART_FROM_VERT && mtface) {
			/* vertex id */
			const int num = (pa->num_dmcache >= 0) ? pa->num_dmcache : pa->num;

			/* iterate over all faces to find a corresponding underlying UV */
			for (int n = 0; n < dm->getNumTessFaces(dm); ++n) {
				MFace *face  = static_cast<MFace *>(dm->getTessFaceData(dm, n, CD_MFACE));
				MTFace *tface = mtface + n;
				unsigned int vtx[4];
				vtx[0] = face->v1;
				vtx[1] = face->v2;
				vtx[2] = face->v3;
				vtx[3] = face->v4;
				bool found = false;

				for (int o = 0; o < 4; ++o) {
					if (o > 2 && vtx[o] == 0) {
						break;
					}

					if (vtx[o] == num) {
						uv_values.push_back(Imath::V2f(tface->uv[o][0], tface->uv[o][1]));

						MVert *mv = mverts + vtx[o];

						normal_short_to_float_v3(normal, mv->no);
						copy_yup_from_zup(tmp_nor.getValue(), normal);
						norm_values.push_back(tmp_nor);
						found = true;
						break;
					}
				}

				if (found) {
					break;
				}
			}
		}

		int steps = path->segments + 1;
		hvertices.push_back(steps);

		for (k = 0; k < steps; ++k) {
			float vert[3];
			copy_v3_v3(vert, path->co);
			mul_m4_v3(inv_mat, vert);

			/* Convert Z-up to Y-up. */
			verts.push_back(Imath::V3f(vert[0], vert[2], -vert[1]));

			++path;
		}
	}
}

void AbcHairWriter::write_hair_child_sample(DerivedMesh *dm,
                                            ParticleSettings *part,
                                            std::vector<Imath::V3f> &verts,
                                            std::vector<Imath::V3f> &norm_values,
                                            std::vector<Imath::V2f> &uv_values,
                                            std::vector<int32_t> &hvertices)
{
	/* Get untransformed vertices, there's a xform under the hair. */
	float inv_mat[4][4];
	invert_m4_m4_safe(inv_mat, m_object->obmat);

	MTFace *mtface = static_cast<MTFace *>(CustomData_get_layer(&dm->faceData, CD_MTFACE));
	MVert *mverts = dm->getVertArray(dm);

	ParticleCacheKey **cache = m_psys->childcache;
	ParticleCacheKey *path;

	ChildParticle *pc = m_psys->child;

	for (int p = 0; p < m_psys->totchild; ++p, ++pc) {
		path = cache[p];

		if (part->from == PART_FROM_FACE &&
		    part->childtype != PART_CHILD_PARTICLES &&
		    mtface)
		{
			const int num = pc->num;
			if (num < 0) {
				ABC_LOG(m_settings.logger)
				        << "Warning, child particle of hair system " << m_psys->name
				        << " has unknown face index of geometry of "<< (m_object->id.name + 2)
				        << ", skipping child hair." << std::endl;
				continue;
			}

			MFace *face = static_cast<MFace *>(dm->getTessFaceData(dm, num, CD_MFACE));
			MTFace *tface = mtface + num;

			float r_uv[2], tmpnor[3], mapfw[4], vec[3];

			psys_interpolate_uvs(tface, face->v4, pc->fuv, r_uv);
			uv_values.push_back(Imath::V2f(r_uv[0], r_uv[1]));

			psys_interpolate_face(mverts, face, tface, NULL, mapfw, vec, tmpnor, NULL, NULL, NULL, NULL);

			/* Convert Z-up to Y-up. */
			norm_values.push_back(Imath::V3f(tmpnor[0], tmpnor[2], -tmpnor[1]));
		}
		else {
			if (uv_values.size()) {
				uv_values.push_back(uv_values[pc->parent]);
			}
			if (norm_values.size()) {
				norm_values.push_back(norm_values[pc->parent]);
			}
		}

		int steps = path->segments + 1;
		hvertices.push_back(steps);

		for (int k = 0; k < steps; ++k) {
			float vert[3];
			copy_v3_v3(vert, path->co);
			mul_m4_v3(inv_mat, vert);

			/* Convert Z-up to Y-up. */
			verts.push_back(Imath::V3f(vert[0], vert[2], -vert[1]));

			++path;
		}
	}
}
