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

#include "BlenderContext.h"
#include "BKE_scene.h"

BlenderContext::BlenderContext(bContext *C)
{
	context = C;
	main = CTX_data_main(C);
	scene = CTX_data_scene(C);
	view_layer = CTX_data_view_layer(C);
	depsgraph = nullptr; // create only when needed
}

bContext *BlenderContext::get_context()
{
	return context;
}

Depsgraph *BlenderContext::get_depsgraph()
{
	if (!depsgraph) {
		depsgraph = BKE_scene_get_depsgraph(scene, view_layer, true);
	}
	return depsgraph;
}

Scene *BlenderContext::get_scene()
{
	return scene;
}

Scene *BlenderContext::get_evaluated_scene()
{
	Scene *scene_eval = DEG_get_evaluated_scene(get_depsgraph());
	return scene_eval;
}

Object *BlenderContext::get_evaluated_object(Object *ob)
{
	Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
	return ob_eval;
}

ViewLayer *BlenderContext::get_view_layer()
{
	return view_layer;
}

Main *BlenderContext::get_main()
{
	return main;
}
