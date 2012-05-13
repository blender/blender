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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/uvproject.c
 *  \ingroup bli
 */

#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_uvproject.h"

typedef struct ProjCameraInfo {
	float camangle;
	float camsize;
	float xasp, yasp;
	float shiftx, shifty;
	float rotmat[4][4];
	float caminv[4][4];
	short do_persp, do_pano, do_rotmat;
} ProjCameraInfo;

void BLI_uvproject_from_camera(float target[2], float source[3], ProjCameraInfo *uci)
{
	float pv4[4];

	copy_v3_v3(pv4, source);
	pv4[3] = 1.0;

	/* rotmat is the object matrix in this case */
	if (uci->do_rotmat)
		mul_m4_v4(uci->rotmat, pv4);

	/* caminv is the inverse camera matrix */
	mul_m4_v4(uci->caminv, pv4);

	if (uci->do_pano) {
		float angle = atan2f(pv4[0], -pv4[2]) / ((float)M_PI * 2.0f); /* angle around the camera */
		if (uci->do_persp == 0) {
			target[0] = angle; /* no correct method here, just map to  0-1 */
			target[1] = pv4[1] / uci->camsize;
		}
		else {
			float vec2d[2]; /* 2D position from the camera */
			vec2d[0] = pv4[0];
			vec2d[1] = pv4[2];
			target[0] = angle * ((float)M_PI / uci->camangle);
			target[1] = pv4[1] / (len_v2(vec2d) * (uci->camsize * 2.0f));
		}
	}
	else {
		if (pv4[2] == 0.0f)
			pv4[2] = 0.00001f;  /* don't allow div by 0 */

		if (uci->do_persp == 0) {
			target[0] = (pv4[0] / uci->camsize);
			target[1] = (pv4[1] / uci->camsize);
		}
		else {
			target[0] = (-pv4[0] * ((1.0f / uci->camsize) / pv4[2])) / 2.0f;
			target[1] = (-pv4[1] * ((1.0f / uci->camsize) / pv4[2])) / 2.0f;
		}
	}

	target[0] *= uci->xasp;
	target[1] *= uci->yasp;

	/* adds camera shift + 0.5 */
	target[0] += uci->shiftx;
	target[1] += uci->shifty;
}

/* could rv3d->persmat */
void BLI_uvproject_from_view(float target[2], float source[3], float persmat[4][4], float rotmat[4][4], float winx, float winy)
{
	float pv[3], pv4[4], x = 0.0, y = 0.0;

	mul_v3_m4v3(pv, rotmat, source);

	copy_v3_v3(pv4, source);
	pv4[3] = 1.0;

	/* rotmat is the object matrix in this case */
	mul_m4_v4(rotmat, pv4);

	/* almost project_short */
	mul_m4_v4(persmat, pv4);
	if (fabsf(pv4[3]) > 0.00001f) { /* avoid division by zero */
		target[0] = winx / 2.0f + (winx / 2.0f) * pv4[0] / pv4[3];
		target[1] = winy / 2.0f + (winy / 2.0f) * pv4[1] / pv4[3];
	}
	else {
		/* scaling is lost but give a valid result */
		target[0] = winx / 2.0f + (winx / 2.0f) * pv4[0];
		target[1] = winy / 2.0f + (winy / 2.0f) * pv4[1];
	}

	/* v3d->persmat seems to do this funky scaling */
	if (winx > winy) {
		y = (winx - winy) / 2.0f;
		winy = winx;
	}
	else {
		x = (winy - winx) / 2.0f;
		winx = winy;
	}

	target[0] = (x + target[0]) / winx;
	target[1] = (y + target[1]) / winy;
}

/* 'rotmat' can be obedit->obmat when uv project is used.
 * 'winx' and 'winy' can be from scene->r.xsch/ysch */
ProjCameraInfo *BLI_uvproject_camera_info(Object *ob, float(*rotmat)[4], float winx, float winy)
{
	ProjCameraInfo uci;
	Camera *camera = ob->data;

	uci.do_pano = (camera->type == CAM_PANO);
	uci.do_persp = (camera->type == CAM_PERSP);

	uci.camangle = focallength_to_fov(camera->lens, camera->sensor_x) / 2.0f;
	uci.camsize = uci.do_persp ? tanf(uci.camangle) : camera->ortho_scale;

	/* account for scaled cameras */
	copy_m4_m4(uci.caminv, ob->obmat);
	normalize_m4(uci.caminv);

	if (invert_m4(uci.caminv)) {
		ProjCameraInfo *uci_pt;

		/* normal projection */
		if (rotmat) {
			copy_m4_m4(uci.rotmat, rotmat);
			uci.do_rotmat = 1;
		}
		else {
			uci.do_rotmat = 0;
		}

		/* also make aspect ratio adjustment factors */
		if (winx > winy) {
			uci.xasp = 1.0f;
			uci.yasp = winx / winy;
		}
		else {
			uci.xasp = winy / winx;
			uci.yasp = 1.0f;
		}

		/* include 0.5f here to move the UVs into the center */
		uci.shiftx = 0.5f - (camera->shiftx * uci.xasp);
		uci.shifty = 0.5f - (camera->shifty * uci.yasp);

		uci_pt = MEM_mallocN(sizeof(ProjCameraInfo), "ProjCameraInfo");
		*uci_pt = uci;
		return uci_pt;
	}

	return NULL;
}

void BLI_uvproject_from_view_ortho(float target[2], float source[3], float rotmat[4][4])
{
	float pv[3];

	mul_v3_m4v3(pv, rotmat, source);

	/* ortho projection */
	target[0] = -pv[0];
	target[1] = pv[2];
}

void BLI_uvproject_camera_info_scale(ProjCameraInfo *uci, float scale_x, float scale_y)
{
	uci->xasp *= scale_x;
	uci->yasp *= scale_y;
}
