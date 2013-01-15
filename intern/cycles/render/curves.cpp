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

#include "device.h"
#include "curves.h"
#include "mesh.h"
#include "object.h"
#include "scene.h"

#include "util_foreach.h"
#include "util_map.h"
#include "util_progress.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

/* Curve functions */

void curvebounds(float *lower, float *upper, float3 *p, int dim)
{
	float *p0 = &p[0].x;
	float *p1 = &p[1].x;
	float *p2 = &p[2].x;
	float *p3 = &p[3].x;
	float fc = 0.71f;
	float curve_coef[4];
	curve_coef[0] = p1[dim];
	curve_coef[1] = -fc*p0[dim] + fc*p2[dim];
	curve_coef[2] = 2.0f * fc * p0[dim] + (fc - 3.0f) * p1[dim] + (3.0f - 2.0f * fc) * p2[dim] - fc * p3[dim];
	curve_coef[3] = -fc * p0[dim] + (2.0f - fc) * p1[dim] + (fc - 2.0f) * p2[dim] + fc * p3[dim];
	float discroot = curve_coef[2] * curve_coef[2] - 3 * curve_coef[3] * curve_coef[1];
	float ta = -1.0f;
	float tb = -1.0f;
	if(discroot >= 0) {
		discroot = sqrt(discroot);
		ta = (-curve_coef[2] - discroot) / (3 * curve_coef[3]);
		tb = (-curve_coef[2] + discroot) / (3 * curve_coef[3]);
		ta = (ta > 1.0f || ta < 0.0f) ? -1.0f : ta;
		tb = (tb > 1.0f || tb < 0.0f) ? -1.0f : tb;
	}

	*upper = max(p1[dim],p2[dim]);
	*lower = min(p1[dim],p2[dim]);
	float exa = p1[dim];
	float exb = p2[dim];
	float t2;
	float t3;
	if(ta >= 0.0f) {
		t2 = ta * ta;
		t3 = t2 * ta;
		exa = curve_coef[3] * t3 + curve_coef[2] * t2 + curve_coef[1] * ta + curve_coef[0];
	}
	if(tb >= 0.0f) {
		t2 = tb * tb;
		t3 = t2 * tb;
		exb = curve_coef[3] * t3 + curve_coef[2] * t2 + curve_coef[1] * tb + curve_coef[0];
	}
	*upper = max(*upper, max(exa,exb));
	*lower = min(*lower, min(exa,exb));
	
}

/* Hair System Manager */

CurveSystemManager::CurveSystemManager()
{
	primitive = CURVE_LINE_SEGMENTS;
	line_method = CURVE_CORRECTED;
	interpolation = CURVE_CARDINAL;
	triangle_method = CURVE_CAMERA_TRIANGLES;
	resolution = 3;
	segments = 1;
	subdivisions = 3;

	normalmix = 1.0f;
	encasing_ratio = 1.01f;

	use_curves = true;
	use_smooth = true;
	use_cache = true;
	use_parents = false;
	use_encasing = true;
	use_backfacing = false;
	use_joined = false;
	use_tangent_normal = false;
	use_tangent_normal_geometry = false;
	use_tangent_normal_correction = false;

	need_update = true;
	need_mesh_update = false;
}

CurveSystemManager::~CurveSystemManager()
{
}

void CurveSystemManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	if(!need_update)
		return;

	device_free(device, dscene);

	progress.set_status("Updating Hair settings", "Copying Hair settings to device");

	KernelCurves *kcurve= &dscene->data.curve_kernel_data;

	kcurve->curveflags = 0;

	if(use_curves) {
		if(primitive == CURVE_SEGMENTS || primitive == CURVE_RIBBONS)
			kcurve->curveflags |= CURVE_KN_INTERPOLATE;
		if(primitive == CURVE_RIBBONS)
			kcurve->curveflags |= CURVE_KN_RIBBONS;

		if(line_method == CURVE_ACCURATE)
			kcurve->curveflags |= CURVE_KN_ACCURATE;
		if(line_method == CURVE_CORRECTED)
			kcurve->curveflags |= CURVE_KN_INTERSECTCORRECTION;
		if(line_method == CURVE_POSTCORRECTED)
			kcurve->curveflags |= CURVE_KN_POSTINTERSECTCORRECTION;

		if(use_tangent_normal)
			kcurve->curveflags |= CURVE_KN_TANGENTGNORMAL;
		if(use_tangent_normal_correction)
			kcurve->curveflags |= CURVE_KN_NORMALCORRECTION;
		if(use_tangent_normal_geometry)
			kcurve->curveflags |= CURVE_KN_TRUETANGENTGNORMAL;
		if(use_joined)
			kcurve->curveflags |= CURVE_KN_CURVEDATA;
		if(use_backfacing)
			kcurve->curveflags |= CURVE_KN_BACKFACING;
		if(use_encasing)
			kcurve->curveflags |= CURVE_KN_ENCLOSEFILTER;

		kcurve->normalmix = normalmix;
		kcurve->encasing_ratio = encasing_ratio;
		kcurve->subdivisions = subdivisions;
	}

	if(progress.get_cancel()) return;

	need_update = false;
}

void CurveSystemManager::device_free(Device *device, DeviceScene *dscene)
{

}

bool CurveSystemManager::modified(const CurveSystemManager& CurveSystemManager)
{
	return !(line_method == CurveSystemManager.line_method &&
		interpolation == CurveSystemManager.interpolation &&
		primitive == CurveSystemManager.primitive &&
		use_encasing == CurveSystemManager.use_encasing &&
		use_tangent_normal == CurveSystemManager.use_tangent_normal &&
		use_tangent_normal_correction == CurveSystemManager.use_tangent_normal_correction &&
		use_tangent_normal_geometry == CurveSystemManager.use_tangent_normal_geometry &&
		encasing_ratio == CurveSystemManager.encasing_ratio &&
		use_backfacing == CurveSystemManager.use_backfacing &&
		normalmix == CurveSystemManager.normalmix &&
		use_cache == CurveSystemManager.use_cache &&
		use_smooth == CurveSystemManager.use_smooth &&
		triangle_method == CurveSystemManager.triangle_method &&
		resolution == CurveSystemManager.resolution &&
		use_curves == CurveSystemManager.use_curves &&
		use_joined == CurveSystemManager.use_joined &&
		segments == CurveSystemManager.segments &&
		use_parents == CurveSystemManager.use_parents &&
		subdivisions == CurveSystemManager.subdivisions);
}

bool CurveSystemManager::modified_mesh(const CurveSystemManager& CurveSystemManager)
{
	return !(primitive == CurveSystemManager.primitive &&
		interpolation == CurveSystemManager.interpolation &&
		use_parents == CurveSystemManager.use_parents &&
		use_smooth == CurveSystemManager.use_smooth &&
		triangle_method == CurveSystemManager.triangle_method &&
		resolution == CurveSystemManager.resolution &&
		use_curves == CurveSystemManager.use_curves &&
		use_joined == CurveSystemManager.use_joined &&
		segments == CurveSystemManager.segments &&
		use_cache == CurveSystemManager.use_cache);
}

void CurveSystemManager::tag_update(Scene *scene)
{
	need_update = true;
}

void CurveSystemManager::tag_update_mesh()
{
	need_mesh_update = true;
}
CCL_NAMESPACE_END

