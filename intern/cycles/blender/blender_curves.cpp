/*
 * Copyright 2011, Blender Foundation.
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
 */

#include "mesh.h"
#include "object.h"
#include "scene.h"
#include "curves.h"

#include "blender_sync.h"
#include "blender_util.h"

#include "subd_mesh.h"
#include "subd_patch.h"
#include "subd_split.h"

#include "util_foreach.h"

#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_meshdata_types.h"

CCL_NAMESPACE_BEGIN

/* Utilities */

/* Hair curve functions */

void curveinterp_v3_v3v3v3v3(float3 *p, float3 *v1, float3 *v2, float3 *v3, float3 *v4, const float w[4]);
void interp_weights(float t, float data[4], int type);
float shaperadius(float shape, float root, float tip, float time);
void InterpolateKeySegments(int seg, int segno, int key, int curve, float3 *keyloc, float *time, ParticleCurveData *CData, int interpolation);
bool ObtainParticleData(Mesh *mesh, BL::Mesh *b_mesh, BL::Object *b_ob, ParticleCurveData *CData);
bool ObtainCacheParticleData(Mesh *mesh, BL::Mesh *b_mesh, BL::Object *b_ob, ParticleCurveData *CData, bool use_parents);
void ExportCurveTrianglePlanes(Mesh *mesh, ParticleCurveData *CData, int interpolation, bool use_smooth, int segments, float3 RotCam);
void ExportCurveTriangleRibbons(Mesh *mesh, ParticleCurveData *CData, int interpolation, bool use_smooth, int segments);
void ExportCurveTriangleGeometry(Mesh *mesh, ParticleCurveData *CData, int interpolation, bool use_smooth, int resolution, int segments);
void ExportCurveSegments(Mesh *mesh, ParticleCurveData *CData, int interpolation, int segments);

ParticleCurveData::ParticleCurveData()
{
}

ParticleCurveData::~ParticleCurveData()
{
	psys_firstcurve.clear();
	psys_curvenum.clear();
	psys_shader.clear();
	psys_rootradius.clear();
	psys_tipradius.clear();
	psys_shape.clear();

	curve_firstkey.clear();
	curve_keynum.clear();
	curve_length.clear();
	curve_u.clear();
	curve_v.clear();

	curvekey_co.clear();
	curvekey_time.clear();
}

void interp_weights(float t, float data[4], int type)
{
	float t2, t3, fc;

	if (type == CURVE_LINEAR) {
		data[0] =          0.0f;
		data[1] = -t     + 1.0f;
		data[2] =  t;
		data[3] =          0.0f;
	}
	else if (type == CURVE_CARDINAL) {
		t2 = t * t;
		t3 = t2 * t;
		fc = 0.71f;

		data[0] = -fc          * t3  + 2.0f * fc          * t2 - fc * t;
		data[1] =  (2.0f - fc) * t3  + (fc - 3.0f)        * t2 + 1.0f;
		data[2] =  (fc - 2.0f) * t3  + (3.0f - 2.0f * fc) * t2 + fc * t;
		data[3] =  fc          * t3  - fc * t2;
	}
	else if (type == CURVE_BSPLINE) {
		t2 = t * t;
		t3 = t2 * t;

		data[0] = -0.16666666f * t3  + 0.5f * t2   - 0.5f * t    + 0.16666666f;
		data[1] =  0.5f        * t3  - t2                        + 0.66666666f;
		data[2] = -0.5f        * t3  + 0.5f * t2   + 0.5f * t    + 0.16666666f;
		data[3] =  0.16666666f * t3;
	}
}

void curveinterp_v3_v3v3v3v3(float3 *p, float3 *v1, float3 *v2, float3 *v3, float3 *v4, const float w[4])
{
	p->x = v1->x * w[0] + v2->x * w[1] + v3->x * w[2] + v4->x * w[3];
	p->y = v1->y * w[0] + v2->y * w[1] + v3->y * w[2] + v4->y * w[3];
	p->z = v1->z * w[0] + v2->z * w[1] + v3->z * w[2] + v4->z * w[3];
}

float shaperadius(float shape, float root, float tip, float time)
{
	float radius = 1.0f - time;
	if (shape != 0.0f) {
		if (shape < 0.0f)
			radius = (float)pow(1.0f - time, 1.f + shape);
		else
			radius = (float)pow(1.0f - time, 1.f / (1.f - shape));
	}
	return (radius * (root - tip)) + tip;
}

/* curve functions */

void InterpolateKeySegments(int seg, int segno, int key, int curve, float3 *keyloc, float *time, ParticleCurveData *CData, int interpolation)
{
	float3 ckey_loc1 = CData->curvekey_co[key];
	float3 ckey_loc2 = ckey_loc1;
	float3 ckey_loc3 = CData->curvekey_co[key+1];
	float3 ckey_loc4 = ckey_loc3;

	if (key > CData->curve_firstkey[curve])
		ckey_loc1 = CData->curvekey_co[key - 1];

	if (key < CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2)
		ckey_loc4 = CData->curvekey_co[key + 2];


	float time1 = CData->curvekey_time[key]/CData->curve_length[curve];
	float time2 = CData->curvekey_time[key + 1]/CData->curve_length[curve];

	float dfra = (time2 - time1) / (float)segno;

	if(time)
		*time = (dfra * seg) + time1;

	float t[4];

	interp_weights((float)seg / (float)segno, t, interpolation);

	if(keyloc)
		curveinterp_v3_v3v3v3v3(keyloc, &ckey_loc1, &ckey_loc2, &ckey_loc3, &ckey_loc4, t);
}

bool ObtainParticleData(Mesh *mesh, BL::Mesh *b_mesh, BL::Object *b_ob, ParticleCurveData *CData)
{

	int curvenum = 0;
	int keyno = 0;

	if(!(mesh && b_mesh && b_ob && CData))
		return false;

	BL::Object::modifiers_iterator b_mod;
	for(b_ob->modifiers.begin(b_mod); b_mod != b_ob->modifiers.end(); ++b_mod) {
		if ((b_mod->type() == b_mod->type_PARTICLE_SYSTEM) && (b_mod->show_viewport()) && (b_mod->show_render())) {

			BL::ParticleSystemModifier psmd(b_mod->ptr);

			BL::ParticleSystem b_psys((const PointerRNA)psmd.particle_system().ptr);

			BL::ParticleSettings b_part((const PointerRNA)b_psys.settings().ptr);

			if((b_psys.settings().render_type()==BL::ParticleSettings::render_type_PATH)&&(b_psys.settings().type()==BL::ParticleSettings::type_HAIR)) {

				int mi = clamp(b_psys.settings().material()-1, 0, mesh->used_shaders.size()-1);
				int shader = mesh->used_shaders[mi];

				int totcurves = b_psys.particles.length();

				if(totcurves == 0)
					continue;

				PointerRNA cpsys = RNA_pointer_get(&b_part.ptr, "cycles");

				CData->psys_firstcurve.push_back(curvenum);
				CData->psys_curvenum.push_back(totcurves);
				CData->psys_shader.push_back(shader);

				float radius = b_psys.settings().particle_size() * 0.5f;
	
				CData->psys_rootradius.push_back(radius * get_float(cpsys, "root_width"));
				CData->psys_tipradius.push_back(radius * get_float(cpsys, "tip_width"));
				CData->psys_shape.push_back(get_float(cpsys, "shape"));
				CData->psys_closetip.push_back(get_boolean(cpsys, "use_closetip"));

				BL::ParticleSystem::particles_iterator b_pa;
				for(b_psys.particles.begin(b_pa); b_pa != b_psys.particles.end(); ++b_pa) {
					CData->curve_firstkey.push_back(keyno);

					int keylength = b_pa->hair_keys.length();
					CData->curve_keynum.push_back(keylength);

					float curve_length = 0.0f;
					float3 pcKey;
					int step_no = 0;
					BL::Particle::hair_keys_iterator b_cKey;
					for(b_pa->hair_keys.begin(b_cKey); b_cKey != b_pa->hair_keys.end(); ++b_cKey) {
						float nco[3];
						b_cKey->co_object( *b_ob, psmd, *b_pa, nco);
						float3 cKey = make_float3(nco[0],nco[1],nco[2]);
						if (step_no > 0)
							curve_length += len(cKey - pcKey);
						CData->curvekey_co.push_back(cKey);
						CData->curvekey_time.push_back(curve_length);
						pcKey = cKey;
						keyno++;
						step_no++;
					}

					CData->curve_length.push_back(curve_length);
					/*add uvs*/
					BL::Mesh::tessface_uv_textures_iterator l;
					b_mesh->tessface_uv_textures.begin(l);

					float uvs[3] = {0,0};
					if(b_mesh->tessface_uv_textures.length())
						b_pa->uv_on_emitter(psmd,uvs);
					CData->curve_u.push_back(uvs[0]);
					CData->curve_v.push_back(uvs[1]);

					curvenum++;

				}
			}
		}
	}

	return true;

}

bool ObtainCacheParticleData(Mesh *mesh, BL::Mesh *b_mesh, BL::Object *b_ob, ParticleCurveData *CData, bool use_parents)
{

	int curvenum = 0;
	int keyno = 0;

	if(!(mesh && b_mesh && b_ob && CData))
		return false;

	Transform tfm = get_transform(b_ob->matrix_world());
	Transform itfm = transform_quick_inverse(tfm);

	BL::Object::modifiers_iterator b_mod;
	for(b_ob->modifiers.begin(b_mod); b_mod != b_ob->modifiers.end(); ++b_mod) {
		if ((b_mod->type() == b_mod->type_PARTICLE_SYSTEM) && (b_mod->show_viewport()) && (b_mod->show_render())) {
			BL::ParticleSystemModifier psmd((const PointerRNA)b_mod->ptr);

			BL::ParticleSystem b_psys((const PointerRNA)psmd.particle_system().ptr);

			BL::ParticleSettings b_part((const PointerRNA)b_psys.settings().ptr);

			if((b_psys.settings().render_type()==BL::ParticleSettings::render_type_PATH)&&(b_psys.settings().type()==BL::ParticleSettings::type_HAIR)) {

				int mi = clamp(b_psys.settings().material()-1, 0, mesh->used_shaders.size()-1);
				int shader = mesh->used_shaders[mi];
				int draw_step = b_psys.settings().draw_step();
				int ren_step = (int)pow((float)2.0f,(float)draw_step);
				/*b_psys.settings().render_step(draw_step);*/

				int totparts = b_psys.particles.length();
				int totchild = b_psys.child_particles.length() * b_psys.settings().draw_percentage() / 100;
				int totcurves = totchild;
				
				if (use_parents || b_psys.settings().child_type() == 0)
					totcurves += totparts;

				if (totcurves == 0)
					continue;

				PointerRNA cpsys = RNA_pointer_get(&b_part.ptr, "cycles");

				CData->psys_firstcurve.push_back(curvenum);
				CData->psys_curvenum.push_back(totcurves);
				CData->psys_shader.push_back(shader);

				float radius = b_psys.settings().particle_size() * 0.5f;
	
				CData->psys_rootradius.push_back(radius * get_float(cpsys, "root_width"));
				CData->psys_tipradius.push_back(radius * get_float(cpsys, "tip_width"));
				CData->psys_shape.push_back(get_float(cpsys, "shape"));
				CData->psys_closetip.push_back(get_boolean(cpsys, "use_closetip"));

				int pa_no = 0;
				if(!use_parents && !(b_psys.settings().child_type() == 0))
					pa_no = totparts;

				BL::ParticleSystem::particles_iterator b_pa;
				b_psys.particles.begin(b_pa);
				for(; pa_no < totparts+totchild; pa_no++) {

					CData->curve_firstkey.push_back(keyno);
					CData->curve_keynum.push_back(ren_step+1);
					
					float curve_length = 0.0f;
					float3 pcKey;
					for(int step_no = 0; step_no <= ren_step; step_no++) {
						float nco[3];
						b_psys.co_hair(*b_ob, psmd, pa_no, step_no, nco);
						float3 cKey = make_float3(nco[0],nco[1],nco[2]);
						cKey = transform_point(&itfm, cKey);
						if (step_no > 0)
							curve_length += len(cKey - pcKey);
						CData->curvekey_co.push_back(cKey);
						CData->curvekey_time.push_back(curve_length);
						pcKey = cKey;
						keyno++;
					}
					CData->curve_length.push_back(curve_length);

					/*add uvs*/
					BL::Mesh::tessface_uv_textures_iterator l;
					b_mesh->tessface_uv_textures.begin(l);

					float uvs[2] = {0,0};
					if(b_mesh->tessface_uv_textures.length())
						b_psys.uv_on_emitter(psmd, *b_pa, pa_no, uvs);
						

					if(pa_no < totparts && b_pa != b_psys.particles.end())
						++b_pa;

					CData->curve_u.push_back(uvs[0]);
					CData->curve_v.push_back(uvs[1]);

					curvenum++;

				}
			}

		}
	}

	return true;

}

void ExportCurveTrianglePlanes(Mesh *mesh, ParticleCurveData *CData, int interpolation, bool use_smooth, int segments, float3 RotCam)
{
	int vertexno = mesh->verts.size();
	int vertexindex = vertexno;

	for( int sys = 0; sys < CData->psys_firstcurve.size() ; sys++) {
		for( int curve = CData->psys_firstcurve[sys]; curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys] ; curve++) {

			for( int curvekey = CData->curve_firstkey[curve]; curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1; curvekey++) {

				int subv = 1;
				float3 xbasis;

				float3 v1;

				if (curvekey == CData->curve_firstkey[curve]) {
					subv = 0;
					v1 = CData->curvekey_co[curvekey+2] - CData->curvekey_co[curvekey];
				}
				else if (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2)
					v1 = CData->curvekey_co[curvekey] - CData->curvekey_co[curvekey - 2];
				else 
					v1 = CData->curvekey_co[curvekey + 1] - CData->curvekey_co[curvekey - 1];


				for (; subv <= segments; subv++) {

					float3 ickey_loc = make_float3(0.0f,0.0f,0.0f);
					float time = 0.0f;

					if ((interpolation == CURVE_BSPLINE) && (curvekey == CData->curve_firstkey[curve]) && (subv == 0))
						ickey_loc = CData->curvekey_co[curvekey];
					else
						InterpolateKeySegments(subv, segments, curvekey, curve, &ickey_loc, &time, CData , interpolation);

					float radius = shaperadius(CData->psys_shape[sys], CData->psys_rootradius[sys], CData->psys_tipradius[sys], time);

					if (CData->psys_closetip[sys] && (subv == segments) && (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2))
						radius = shaperadius(CData->psys_shape[sys], CData->psys_rootradius[sys], 0.0f, 0.95f);

					if ((curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2) && (subv == segments))
						radius = shaperadius(CData->psys_shape[sys], CData->psys_rootradius[sys], CData->psys_tipradius[sys], 0.95f);

					xbasis = normalize(cross(v1,RotCam - ickey_loc));
					float3 ickey_loc_shfl = ickey_loc - radius * xbasis;
					float3 ickey_loc_shfr = ickey_loc + radius * xbasis;
					mesh->verts.push_back(ickey_loc_shfl);
					mesh->verts.push_back(ickey_loc_shfr);
					if(subv!=0) {
						mesh->add_triangle(vertexindex-2, vertexindex, vertexindex-1, CData->psys_shader[sys], use_smooth);
						mesh->add_triangle(vertexindex+1, vertexindex-1, vertexindex, CData->psys_shader[sys], use_smooth);
					}
					vertexindex += 2;
				}
			}
		}
	}

	mesh->reserve(mesh->verts.size(), mesh->triangles.size());
	mesh->attributes.remove(ATTR_STD_VERTEX_NORMAL);
	mesh->attributes.remove(ATTR_STD_FACE_NORMAL);
	mesh->add_face_normals();
	mesh->add_vertex_normals();
	mesh->attributes.remove(ATTR_STD_FACE_NORMAL);

	/* texture coords still needed */

}

void ExportCurveTriangleRibbons(Mesh *mesh, ParticleCurveData *CData, int interpolation, bool use_smooth, int segments)
{
	int vertexno = mesh->verts.size();
	int vertexindex = vertexno;

	for( int sys = 0; sys < CData->psys_firstcurve.size() ; sys++) {
		for( int curve = CData->psys_firstcurve[sys]; curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys] ; curve++) {

			float3 firstxbasis = cross(make_float3(1.0f,0.0f,0.0f),CData->curvekey_co[CData->curve_firstkey[curve]+1] - CData->curvekey_co[CData->curve_firstkey[curve]]);
			if(len_squared(firstxbasis)!= 0.0f)
				firstxbasis = normalize(firstxbasis);
			else
				firstxbasis = normalize(cross(make_float3(0.0f,1.0f,0.0f),CData->curvekey_co[CData->curve_firstkey[curve]+1] - CData->curvekey_co[CData->curve_firstkey[curve]]));

			for( int curvekey = CData->curve_firstkey[curve]; curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1; curvekey++) {

				float3 xbasis = firstxbasis;
				float3 v1;
				float3 v2;

				if (curvekey == CData->curve_firstkey[curve]) {
					v1 = CData->curvekey_co[curvekey+2] - CData->curvekey_co[curvekey+1];
					v2 = CData->curvekey_co[curvekey+1] - CData->curvekey_co[curvekey];
				}
				else if (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2) {
					v1 = CData->curvekey_co[curvekey] - CData->curvekey_co[curvekey-1];
					v2 = CData->curvekey_co[curvekey-1] - CData->curvekey_co[curvekey-2];
				}
				else {
					v1 = CData->curvekey_co[curvekey+1] - CData->curvekey_co[curvekey];
					v2 = CData->curvekey_co[curvekey] - CData->curvekey_co[curvekey-1];
				}

				xbasis = cross(v1,v2);

				if(len_squared(xbasis) >= 0.05f * len_squared(v1) * len_squared(v2)) {
					firstxbasis = normalize(xbasis);
					break;
				}
			}

			for( int curvekey = CData->curve_firstkey[curve]; curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1; curvekey++) {

				int subv = 1;
				float3 v1;
				float3 v2;
				float3 xbasis;

				if (curvekey == CData->curve_firstkey[curve]) {
					subv = 0;
					v1 = CData->curvekey_co[curvekey+2] - CData->curvekey_co[curvekey+1];
					v2 = CData->curvekey_co[curvekey+1] - CData->curvekey_co[curvekey];
				}
				else if (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2) {
					v1 = CData->curvekey_co[curvekey] - CData->curvekey_co[curvekey-1];
					v2 = CData->curvekey_co[curvekey-1] - CData->curvekey_co[curvekey-2];
				}
				else {
					v1 = CData->curvekey_co[curvekey+1] - CData->curvekey_co[curvekey];
					v2 = CData->curvekey_co[curvekey] - CData->curvekey_co[curvekey-1];
				}

				xbasis = cross(v1,v2);

				if(len_squared(xbasis) >= 0.05f * len_squared(v1) * len_squared(v2)) {
					xbasis = normalize(xbasis);
					firstxbasis = xbasis;
				}
				else
					xbasis = firstxbasis;

				for (; subv <= segments; subv++) {

					float3 ickey_loc = make_float3(0.0f,0.0f,0.0f);
					float time = 0.0f;

					if ((interpolation == CURVE_BSPLINE) && (curvekey == CData->curve_firstkey[curve]) && (subv == 0))
						ickey_loc = CData->curvekey_co[curvekey];
					else
						InterpolateKeySegments(subv, segments, curvekey, curve, &ickey_loc, &time, CData , interpolation);

					float radius = shaperadius(CData->psys_shape[sys], CData->psys_rootradius[sys], CData->psys_tipradius[sys], time);

					if (CData->psys_closetip[sys] && (subv == segments) && (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2))
						radius = shaperadius(CData->psys_shape[sys], CData->psys_rootradius[sys], 0.0f, 0.95f);

					if ((curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2) && (subv == segments))
						radius = shaperadius(CData->psys_shape[sys], CData->psys_rootradius[sys], CData->psys_tipradius[sys], 0.95f);

					float3 ickey_loc_shfl = ickey_loc - radius * xbasis;
					float3 ickey_loc_shfr = ickey_loc + radius * xbasis;
					mesh->verts.push_back(ickey_loc_shfl);
					mesh->verts.push_back(ickey_loc_shfr);
					if(subv!=0) {
						mesh->add_triangle(vertexindex-2, vertexindex, vertexindex-1, CData->psys_shader[sys], use_smooth);
						mesh->add_triangle(vertexindex+1, vertexindex-1, vertexindex, CData->psys_shader[sys], use_smooth);
					}
					vertexindex += 2;
				}
			}
		}
	}

	mesh->reserve(mesh->verts.size(), mesh->triangles.size());
	mesh->attributes.remove(ATTR_STD_VERTEX_NORMAL);
	mesh->attributes.remove(ATTR_STD_FACE_NORMAL);
	mesh->add_face_normals();
	mesh->add_vertex_normals();
	mesh->attributes.remove(ATTR_STD_FACE_NORMAL);
	/* texture coords still needed */

}

void ExportCurveTriangleGeometry(Mesh *mesh, ParticleCurveData *CData, int interpolation, bool use_smooth, int resolution, int segments)
{
	int vertexno = mesh->verts.size();
	int vertexindex = vertexno;

	for( int sys = 0; sys < CData->psys_firstcurve.size() ; sys++) {
		for( int curve = CData->psys_firstcurve[sys]; curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys] ; curve++) {

			float3 firstxbasis = cross(make_float3(1.0f,0.0f,0.0f),CData->curvekey_co[CData->curve_firstkey[curve]+1] - CData->curvekey_co[CData->curve_firstkey[curve]]);
			if(len_squared(firstxbasis)!= 0.0f)
				firstxbasis = normalize(firstxbasis);
			else
				firstxbasis = normalize(cross(make_float3(0.0f,1.0f,0.0f),CData->curvekey_co[CData->curve_firstkey[curve]+1] - CData->curvekey_co[CData->curve_firstkey[curve]]));


			for( int curvekey = CData->curve_firstkey[curve]; curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1; curvekey++) {

				float3 xbasis = firstxbasis;
				float3 v1;
				float3 v2;

				if (curvekey == CData->curve_firstkey[curve]) {
					v1 = CData->curvekey_co[curvekey+2] - CData->curvekey_co[curvekey+1];
					v2 = CData->curvekey_co[curvekey+1] - CData->curvekey_co[curvekey];
				}
				else if (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2) {
					v1 = CData->curvekey_co[curvekey] - CData->curvekey_co[curvekey-1];
					v2 = CData->curvekey_co[curvekey-1] - CData->curvekey_co[curvekey-2];
				}
				else {
					v1 = CData->curvekey_co[curvekey+1] - CData->curvekey_co[curvekey];
					v2 = CData->curvekey_co[curvekey] - CData->curvekey_co[curvekey-1];
				}

				xbasis = cross(v1,v2);

				if(len_squared(xbasis) >= 0.05f * len_squared(v1) * len_squared(v2)) {
					firstxbasis = normalize(xbasis);
					break;
				}
			}

			for( int curvekey = CData->curve_firstkey[curve]; curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1; curvekey++) {

				int subv = 1;
				float3 xbasis;
				float3 ybasis;
				float3 v1;
				float3 v2;

				if (curvekey == CData->curve_firstkey[curve]) {
					subv = 0;
					v1 = CData->curvekey_co[curvekey+2] - CData->curvekey_co[curvekey+1];
					v2 = CData->curvekey_co[curvekey+1] - CData->curvekey_co[curvekey];
				}
				else if (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2) {
					v1 = CData->curvekey_co[curvekey] - CData->curvekey_co[curvekey-1];
					v2 = CData->curvekey_co[curvekey-1] - CData->curvekey_co[curvekey-2];
				}
				else {
					v1 = CData->curvekey_co[curvekey+1] - CData->curvekey_co[curvekey];
					v2 = CData->curvekey_co[curvekey] - CData->curvekey_co[curvekey-1];
				}

				xbasis = cross(v1,v2);

				if(len_squared(xbasis) >= 0.05f * len_squared(v1) * len_squared(v2)) {
					xbasis = normalize(xbasis);
					firstxbasis = xbasis;
				}
				else
					xbasis = firstxbasis;

				ybasis = normalize(cross(xbasis,v2));

				for (; subv <= segments; subv++) {

					float3 ickey_loc = make_float3(0.0f,0.0f,0.0f);
					float time = 0.0f;

					if ((interpolation == CURVE_BSPLINE) && (curvekey == CData->curve_firstkey[curve]) && (subv == 0))
						ickey_loc = CData->curvekey_co[curvekey];
					else
						InterpolateKeySegments(subv, segments, curvekey, curve, &ickey_loc, &time, CData , interpolation);

					float radius = shaperadius(CData->psys_shape[sys], CData->psys_rootradius[sys], CData->psys_tipradius[sys], time);

					if (CData->psys_closetip[sys] && (subv == segments) && (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2))
						radius = shaperadius(CData->psys_shape[sys], CData->psys_rootradius[sys], 0.0f, 0.95f);

					if ((curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2) && (subv == segments))
						radius = shaperadius(CData->psys_shape[sys], CData->psys_rootradius[sys], CData->psys_tipradius[sys], 0.95f);

					float angle = 2 * M_PI_F / (float)resolution;
					for(int section = 0 ; section < resolution; section++) {
						float3 ickey_loc_shf = ickey_loc + radius * (cosf(angle * section) * xbasis + sinf(angle * section) * ybasis);
						mesh->verts.push_back(ickey_loc_shf);
					}

					if(subv!=0) {
						for(int section = 0 ; section < resolution - 1; section++) {
							mesh->add_triangle(vertexindex - resolution + section, vertexindex + section, vertexindex - resolution + section + 1, CData->psys_shader[sys], use_smooth);
							mesh->add_triangle(vertexindex + section + 1, vertexindex - resolution + section + 1, vertexindex + section, CData->psys_shader[sys], use_smooth);
						}
						mesh->add_triangle(vertexindex-1, vertexindex + resolution - 1, vertexindex - resolution, CData->psys_shader[sys], use_smooth);
						mesh->add_triangle(vertexindex, vertexindex - resolution , vertexindex + resolution - 1, CData->psys_shader[sys], use_smooth);
					}
					vertexindex += resolution;
				}
			}
		}
	}

	mesh->reserve(mesh->verts.size(), mesh->triangles.size());
	mesh->attributes.remove(ATTR_STD_VERTEX_NORMAL);
	mesh->attributes.remove(ATTR_STD_FACE_NORMAL);
	mesh->add_face_normals();
	mesh->add_vertex_normals();
	mesh->attributes.remove(ATTR_STD_FACE_NORMAL);

	/* texture coords still needed */
}

void ExportCurveSegments(Mesh *mesh, ParticleCurveData *CData, int interpolation, int segments)
{
	int cks = 0;
	int curs = 0;
	int segs = 0;

	if(!(mesh->curve_segs.empty() && mesh->curve_keys.empty() && mesh->curve_attrib.empty()))
		return;

	for( int sys = 0; sys < CData->psys_firstcurve.size() ; sys++) {

		if(CData->psys_curvenum[sys] == 0)
			continue;

		for( int curve = CData->psys_firstcurve[sys]; curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys] ; curve++) {

			if(CData->curve_keynum[curve] <= 1)
				continue;

			for( int curvekey = CData->curve_firstkey[curve]; curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1; curvekey++) {

				int subv = 1;
				if (curvekey == CData->curve_firstkey[curve])
					subv = 0;

				for (; subv <= segments; subv++) {

					float3 ickey_loc = make_float3(0.0f,0.0f,0.0f);
					float time = 0.0f;

					if ((interpolation == CURVE_BSPLINE) && (curvekey == CData->curve_firstkey[curve]) && (subv == 0))
						ickey_loc = CData->curvekey_co[curvekey];
					else
						InterpolateKeySegments(subv, segments, curvekey, curve, &ickey_loc, &time, CData , interpolation);

					float radius = shaperadius(CData->psys_shape[sys], CData->psys_rootradius[sys], CData->psys_tipradius[sys], time);

					if (CData->psys_closetip[sys] && (subv == segments) && (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2))
						radius =0.0f;

					mesh->add_curvekey(ickey_loc, radius, time);

					if(subv != 0) {
						mesh->add_curve(cks - 1, cks, CData->psys_shader[sys], curs);
						segs++;
					}

					cks++;
				}
			}

			mesh->add_curveattrib(CData->curve_u[curve], CData->curve_v[curve]);
			curs++;

		}
	}

	/* check allocation*/
	if((mesh->curve_keys.size() !=  cks) || (mesh->curve_segs.size() !=  segs) || (mesh->curve_attrib.size() != curs)) {
		/* allocation failed -> clear data */
		mesh->curve_keys.clear();
		mesh->curve_segs.clear();
		mesh->curve_attrib.clear();
	}
}

/* Hair Curve Sync */

void BlenderSync::sync_curve_settings()
{
	PointerRNA csscene = RNA_pointer_get(&b_scene.ptr, "cycles_curves");
	
	int preset = get_enum(csscene, "preset");

	CurveSystemManager *curve_system_manager = scene->curve_system_manager;
	CurveSystemManager prev_curve_system_manager = *curve_system_manager;

	curve_system_manager->use_curves = get_boolean(csscene, "use_curves");

	if (preset == CURVE_CUSTOM) {
		/*custom properties*/
		curve_system_manager->primitive = get_enum(csscene, "primitive");
		curve_system_manager->line_method = get_enum(csscene, "line_method");
		curve_system_manager->interpolation = get_enum(csscene, "interpolation");
		curve_system_manager->triangle_method = get_enum(csscene, "triangle_method");
		curve_system_manager->resolution = get_int(csscene, "resolution");
		curve_system_manager->segments = get_int(csscene, "segments");
		curve_system_manager->use_smooth = get_boolean(csscene, "use_smooth");

		curve_system_manager->normalmix = get_float(csscene, "normalmix");
		curve_system_manager->encasing_ratio = get_float(csscene, "encasing_ratio");

		curve_system_manager->use_cache = get_boolean(csscene, "use_cache");
		curve_system_manager->use_parents = get_boolean(csscene, "use_parents");
		curve_system_manager->use_encasing = get_boolean(csscene, "use_encasing");
		curve_system_manager->use_backfacing = get_boolean(csscene, "use_backfacing");
		curve_system_manager->use_joined = get_boolean(csscene, "use_joined");
		curve_system_manager->use_tangent_normal = get_boolean(csscene, "use_tangent_normal");
		curve_system_manager->use_tangent_normal_geometry = get_boolean(csscene, "use_tangent_normal_geometry");
		curve_system_manager->use_tangent_normal_correction = get_boolean(csscene, "use_tangent_normal_correction");
	}
	else {
		curve_system_manager->primitive = CURVE_LINE_SEGMENTS;
		curve_system_manager->interpolation = CURVE_CARDINAL;
		curve_system_manager->normalmix = 1.0f;
		curve_system_manager->encasing_ratio = 1.01f;
		curve_system_manager->use_cache = true;
		curve_system_manager->use_parents = false;
		curve_system_manager->segments = 1;
		curve_system_manager->use_joined = false;

		switch(preset) {
			case CURVE_TANGENT_SHADING:
				/*tangent shading*/
				curve_system_manager->line_method = CURVE_UNCORRECTED;
				curve_system_manager->use_encasing = true;
				curve_system_manager->use_backfacing = false;
				curve_system_manager->use_tangent_normal = true;
				curve_system_manager->use_tangent_normal_geometry = true;
				curve_system_manager->use_tangent_normal_correction = false;
				break;
			case CURVE_TRUE_NORMAL:
				/*True Normal*/
				curve_system_manager->line_method = CURVE_CORRECTED;
				curve_system_manager->use_encasing = true;
				curve_system_manager->use_backfacing = false;
				curve_system_manager->use_tangent_normal = false;
				curve_system_manager->use_tangent_normal_geometry = false;
				curve_system_manager->use_tangent_normal_correction = false;
				break;
			case CURVE_ACCURATE_PRESET:
				/*Accurate*/
				curve_system_manager->line_method = CURVE_ACCURATE;
				curve_system_manager->use_encasing = false;
				curve_system_manager->use_backfacing = true;
				curve_system_manager->use_tangent_normal = false;
				curve_system_manager->use_tangent_normal_geometry = false;
				curve_system_manager->use_tangent_normal_correction = false;
				break;
		}
		
	}

	if(curve_system_manager->modified_mesh(prev_curve_system_manager))
	{
		BL::BlendData::objects_iterator b_ob;

		for(b_data.objects.begin(b_ob); b_ob != b_data.objects.end(); ++b_ob) {
			if(object_is_mesh(*b_ob)) {
				BL::Object::particle_systems_iterator b_psys;
				for(b_ob->particle_systems.begin(b_psys); b_psys != b_ob->particle_systems.end(); ++b_psys) {
					if((b_psys->settings().render_type()==BL::ParticleSettings::render_type_PATH)&&(b_psys->settings().type()==BL::ParticleSettings::type_HAIR)) {
						BL::ID key = BKE_object_is_modified(*b_ob)? *b_ob: b_ob->data();
						mesh_map.set_recalc(key);
						object_map.set_recalc(*b_ob);
					}
				}
			}
		}
	}

	if(curve_system_manager->modified(prev_curve_system_manager))
		curve_system_manager->tag_update(scene);

}

void BlenderSync::sync_curves(Mesh *mesh, BL::Mesh b_mesh, BL::Object b_ob, bool object_updated)
{
	/* Clear stored curve data */
	mesh->curve_attrib.clear();
	mesh->curve_keys.clear();
	mesh->curve_keysCD.clear();
	mesh->curve_segs.clear();

	/* obtain general settings */
	bool use_curves = scene->curve_system_manager->use_curves;

	if(use_curves && b_ob.mode() == b_ob.mode_OBJECT) {
	int primitive = scene->curve_system_manager->primitive;
	int interpolation = scene->curve_system_manager->interpolation;
	int triangle_method = scene->curve_system_manager->triangle_method;
	int resolution = scene->curve_system_manager->resolution;
	int segments = scene->curve_system_manager->segments;
	bool use_smooth = scene->curve_system_manager->use_smooth;
	bool use_cache = scene->curve_system_manager->use_cache;
	bool use_parents = scene->curve_system_manager->use_parents;
	bool export_tgs = scene->curve_system_manager->use_joined;

	/* extract particle hair data - should be combined with connecting to mesh later*/

	ParticleCurveData *CData = new ParticleCurveData();

	if (use_cache)
		ObtainCacheParticleData(mesh, &b_mesh, &b_ob, CData, use_parents);
	else
		ObtainParticleData(mesh, &b_mesh, &b_ob, CData);

	/* attach strands to mesh */
	BL::Object b_CamOb = b_scene.camera();
	float3 RotCam = make_float3(0.0f, 0.0f, 0.0f);
	if(b_CamOb) {
		Transform ctfm = get_transform(b_CamOb.matrix_world());
		Transform tfm = get_transform(b_ob.matrix_world());
		Transform itfm = transform_quick_inverse(tfm);
		RotCam = transform_point(&itfm, make_float3(ctfm.x.w, ctfm.y.w, ctfm.z.w));
	}

	if (primitive == CURVE_TRIANGLES){
		if (triangle_method == CURVE_CAMERA)
			ExportCurveTrianglePlanes(mesh, CData, interpolation, use_smooth, segments, RotCam);
		else if (triangle_method == CURVE_RIBBONS)
			ExportCurveTriangleRibbons(mesh, CData, interpolation, use_smooth, segments);
		else
			ExportCurveTriangleGeometry(mesh, CData, interpolation, use_smooth, resolution, segments);
	}
	else {
		ExportCurveSegments(mesh, CData, interpolation, segments);
		int ckey_num = mesh->curve_keys.size();

		/*export tangents or curve data? - not functional yet*/
		if (export_tgs && ckey_num > 1) {
			
			for(int ck = 0; ck < ckey_num; ck++) {
				Mesh::CurveData SCD;
				SCD.tg = normalize(normalize(mesh->curve_keys[min(ck + 1, ckey_num - 1)].loc - mesh->curve_keys[ck].loc) -
					normalize(mesh->curve_keys[max(ck - 1, 0)].loc - mesh->curve_keys[ck].loc));
				mesh->curve_keysCD.push_back(SCD);
			}
		}
	}


	delete CData;

	}

	mesh->compute_bounds();

}


CCL_NAMESPACE_END

