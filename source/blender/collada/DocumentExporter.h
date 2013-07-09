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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DocumentExporter.h
 *  \ingroup collada
 */

#ifndef __DOCUMENTEXPORTER_H__
#define __DOCUMENTEXPORTER_H__

#include "ExportSettings.h"

extern "C" {
#include "DNA_customdata_types.h"
}

struct Scene;

class DocumentExporter
{
 public:
	DocumentExporter(const ExportSettings *export_settings);
	void exportCurrentScene(Scene *sce);
	void exportScenes(const char *filename);
private:
	const ExportSettings *export_settings;
};

#endif
