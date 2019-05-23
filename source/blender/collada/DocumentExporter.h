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

#ifndef __DOCUMENTEXPORTER_H__
#define __DOCUMENTEXPORTER_H__

#include "collada.h"
#include "collada_utils.h"
#include "BlenderContext.h"

extern "C" {
#include "DNA_customdata_types.h"
}

class DocumentExporter {
 public:
  DocumentExporter(BlenderContext &blender_context, ExportSettings *export_settings);
  int exportCurrentScene();
  void exportScenes(const char *filename);

 private:
  BlenderContext &blender_context;
  BCExportSettings export_settings;
  KeyImageMap key_image_map;
};

#endif
