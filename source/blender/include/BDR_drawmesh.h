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
 */

#ifndef BDR_DRAWMESH_H
#define BDR_DRAWMESH_H

struct Image;
struct MTFace;
struct Object;
struct DerivedMesh;
struct Mesh;
struct EdgeHash;

/**
 * Enables or disable mipmapping for realtime images (textures).
 * Note that this will will destroy all texture bindings in OpenGL.
 * @see free_realtime_image()
 * @param mipmap Turn mipmapping on (mipmap!=0) or off (mipmap==0).
 */
void set_mipmap(int mipmap);

/**
 * Enables or disable linear mipmap setting for realtime images (textures).
 * Note that this will will destroy all texture bindings in OpenGL.
 * @see free_realtime_image()
 * @param mipmap Turn linear mipmapping on (linear!=0) or off (linear==0).
 */
void set_linear_mipmap(int linear);

/**
 * Returns the current setting for linear mipmapping.
 */
int get_linear_mipmap(void);

/**
 * Resets the realtime image cache variables.
 */
void clear_realtime_image_cache(void);


void update_realtime_image(struct Image *ima, int x, int y, int w, int h);
void free_realtime_image(struct Image *ima);
void free_all_realtime_images(void);
void make_repbind(struct Image *ima);
int set_tpage(struct MTFace *tface);

void texpaint_enable_mipmap(void);
void texpaint_disable_mipmap(void);

void draw_mesh_textured(struct Object *ob, struct DerivedMesh *dm, int facesel);
struct EdgeHash *get_tface_mesh_marked_edge_info(struct Mesh *me);
void init_realtime_GL(void); 

#endif /* BDR_DRAWMESH_H */

