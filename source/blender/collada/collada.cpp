/**
 * $Id$
 *
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
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_context.h"

#include "DocumentExporter.h"
#include "DocumentImporter.h"

extern "C"
{
	int collada_import(bContext *C, const char *filepath)
	{
		DocumentImporter imp;
		imp.import(C, filepath);

		return 1;
	}

	int collada_export(Scene *sce, const char *filepath)
	{

		DocumentExporter exp;
		exp.exportCurrentScene(sce, filepath);

		return 1;
	}
}
