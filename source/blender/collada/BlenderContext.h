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
 * Contributor(s): Gaia Clary,
 *
 * ***** END GPL LICENSE BLOCK *****
 */

 /** \file CameraExporter.h
  *  \ingroup collada
  */

#ifndef __BLENDERCONTEXT_H__
#define __BLENDERCONTEXT_H__

extern "C" {
#include "DNA_object_types.h"
#include "BKE_context.h"
#include "BKE_main.h"
#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"
}

class BlenderContext
{
private:
	bContext *context;
	Depsgraph *depsgraph;
	Scene *scene;
	ViewLayer *view_layer;
	Main *main;

public:
	BlenderContext(bContext *C);
	bContext *get_context();
	Depsgraph *get_depsgraph();
	Scene *get_scene();
	ViewLayer *get_view_layer();
	Main *get_main();
};

#endif
