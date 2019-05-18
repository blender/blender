/*
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

/** \file
 * \ingroup collada
 */

#include <string>

#include "COLLADASWCamera.h"

extern "C" {
#include "DNA_camera_types.h"
}
#include "CameraExporter.h"

#include "collada_internal.h"

CamerasExporter::CamerasExporter(COLLADASW::StreamWriter *sw,
                                 const ExportSettings *export_settings)
    : COLLADASW::LibraryCameras(sw), export_settings(export_settings)
{
}

template<class Functor>
void forEachCameraObjectInExportSet(Scene *sce, Functor &f, LinkNode *export_set)
{
  LinkNode *node;
  for (node = export_set; node; node = node->next) {
    Object *ob = (Object *)node->link;

    if (ob->type == OB_CAMERA && ob->data) {
      f(ob, sce);
    }
  }
}

void CamerasExporter::exportCameras(Scene *sce)
{
  openLibrary();

  forEachCameraObjectInExportSet(sce, *this, this->export_settings->export_set);

  closeLibrary();
}
void CamerasExporter::operator()(Object *ob, Scene *sce)
{
  Camera *cam = (Camera *)ob->data;
  std::string cam_id(get_camera_id(ob));
  std::string cam_name(id_name(cam));

  switch (cam->type) {
    case CAM_PANO:
    case CAM_PERSP: {
      COLLADASW::PerspectiveOptic persp(mSW);
      persp.setXFov(RAD2DEGF(focallength_to_fov(cam->lens, cam->sensor_x)), "xfov");
      persp.setAspectRatio((float)(sce->r.xsch) / (float)(sce->r.ysch), false, "aspect_ratio");
      persp.setZFar(cam->clip_end, false, "zfar");
      persp.setZNear(cam->clip_start, false, "znear");
      COLLADASW::Camera ccam(mSW, &persp, cam_id, cam_name);
      exportBlenderProfile(ccam, cam);
      addCamera(ccam);

      break;
    }
    case CAM_ORTHO:
    default: {
      COLLADASW::OrthographicOptic ortho(mSW);
      ortho.setXMag(cam->ortho_scale / 2, "xmag");
      ortho.setAspectRatio((float)(sce->r.xsch) / (float)(sce->r.ysch), false, "aspect_ratio");
      ortho.setZFar(cam->clip_end, false, "zfar");
      ortho.setZNear(cam->clip_start, false, "znear");
      COLLADASW::Camera ccam(mSW, &ortho, cam_id, cam_name);
      exportBlenderProfile(ccam, cam);
      addCamera(ccam);
      break;
    }
  }
}
bool CamerasExporter::exportBlenderProfile(COLLADASW::Camera &cm, Camera *cam)
{
  cm.addExtraTechniqueParameter("blender", "shiftx", cam->shiftx);
  cm.addExtraTechniqueParameter("blender", "shifty", cam->shifty);
  cm.addExtraTechniqueParameter("blender", "dof_distance", cam->dof.focus_distance);
  return true;
}
