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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Jan Diederich, Tod Liverseed,
 *                 Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/CameraExporter.cpp
 *  \ingroup collada
 */


#include <string>

#include "COLLADASWCamera.h"
#include "COLLADASWCameraOptic.h"

#include "DNA_camera_types.h"

#include "CameraExporter.h"

#include "collada_internal.h"

CamerasExporter::CamerasExporter(COLLADASW::StreamWriter *sw, const ExportSettings *export_settings): COLLADASW::LibraryCameras(sw), export_settings(export_settings) {}

template<class Functor>
void forEachCameraObjectInScene(Scene *sce, Functor &f, bool export_selected)
{
	Base *base = (Base*) sce->base.first;
	while (base) {
		Object *ob = base->object;

		if (ob->type == OB_CAMERA && ob->data && !(export_selected && !(ob->flag & SELECT))) {
			f(ob, sce);
		}
		base = base->next;
	}
}

void CamerasExporter::exportCameras(Scene *sce)
{
	openLibrary();
	
	forEachCameraObjectInScene(sce, *this, this->export_settings->selected);
	
	closeLibrary();
}
void CamerasExporter::operator()(Object *ob, Scene *sce)
{
	// TODO: shiftx, shifty, YF_dofdist
	Camera *cam = (Camera*)ob->data;
	std::string cam_id(get_camera_id(ob));
	std::string cam_name(id_name(cam));
	
	if (cam->type == CAM_PERSP) {
		COLLADASW::PerspectiveOptic persp(mSW);
		persp.setXFov(RAD2DEGF(focallength_to_fov(cam->lens, cam->sensor_x)), "xfov");
		persp.setAspectRatio((float)(sce->r.xsch)/(float)(sce->r.ysch), false, "aspect_ratio");
		persp.setZFar(cam->clipend, false, "zfar");
		persp.setZNear(cam->clipsta, false, "znear");
		COLLADASW::Camera ccam(mSW, &persp, cam_id, cam_name);
		addCamera(ccam);
	}
	else {
		COLLADASW::OrthographicOptic ortho(mSW);
		ortho.setXMag(cam->ortho_scale, "xmag");
		ortho.setAspectRatio((float)(sce->r.xsch)/(float)(sce->r.ysch), false, "aspect_ratio");
		ortho.setZFar(cam->clipend, false, "zfar");
		ortho.setZNear(cam->clipsta, false, "znear");
		COLLADASW::Camera ccam(mSW, &ortho, cam_id, cam_name);
		addCamera(ccam);
	}
}	
