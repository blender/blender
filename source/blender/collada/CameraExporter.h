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

/** \file CameraExporter.h
 *  \ingroup collada
 */

#ifndef __CAMERAEXPORTER_H__
#define __CAMERAEXPORTER_H__

#include "COLLADASWStreamWriter.h"
#include "COLLADASWLibraryCameras.h"

extern "C" {
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
}

#include "ExportSettings.h"

class CamerasExporter: COLLADASW::LibraryCameras
{
public:
	CamerasExporter(COLLADASW::StreamWriter *sw, const ExportSettings *export_settings);
	void exportCameras(Scene *sce);
	void operator()(Object *ob, Scene *sce);
private:
	const ExportSettings *export_settings;
};

#endif
