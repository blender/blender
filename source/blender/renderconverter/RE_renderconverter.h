/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * Interface to transform the Blender scene into renderable data.
 *
 * @mainpage RE - Blender RE-converter external interface
 *
 * @section about About the RE-converter module
 *
 * The converter takes a Blender scene, and transforms the data into
 * renderer-specific data.
 *
 * Conversions:
 * 
 * halos: (world settings) stars ->
 *        some particle effects  ->
 *        meshes with halo prop  ->  
 *                                  HaloRen (made by inithalo)
 *
 *
 *                                  VlakRen (face render data)
 * Each vlakren needs several VertRens to make sense.
 *                                  VertRen (vertex render data)
 *
 * @section issues Known issues with RE-converter
 *
 *
 * @section dependencies Dependencies
 *
 *
 * */

#ifndef RE_RENDERCONVERTER_H
#define RE_RENDERCONVERTER_H

#ifdef __cplusplus
extern "C" {
#endif

	struct LampRen;
	struct Object;
	struct Lamp;
	
	/** Transform a blender scene to render data. */
	void RE_rotateBlenderScene(void);

	/** Free all memory used for the conversion. */
	void RE_freeRotateBlenderScene(void);

	/**
	 * Used by the preview renderer.
	 */
	void RE_add_render_lamp(struct Object *ob, int doshadbuf);

	/**
	 * Strange support for star rendering for drawview.c... For
	 * rendering purposes, these function pointers should be NULL.
	 */
	void RE_make_stars(void (*initfunc)(void),
					   void (*vertexfunc)(float*),
					   void (*termfunc)(void));
		
#ifdef __cplusplus
}
#endif

#endif

