/**
 * blenlib/BKE_mesh.h (mar-2001 nzc)
 *	
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
#ifndef BKE_MESH_H
#define BKE_MESH_H

/***/

struct BoundBox;
struct DispList;
struct ListBase;
struct MDeformVert;
struct Mesh;
struct MFace;
struct MVert;
struct MCol;
struct Object;
struct TFace;
struct VecNor;

#ifdef __cplusplus
extern "C" {
#endif

void unlink_mesh(struct Mesh *me);
void free_mesh(struct Mesh *me);
struct Mesh *add_mesh(void);
struct Mesh *copy_mesh(struct Mesh *me);
void make_local_tface(struct Mesh *me);
void make_local_mesh(struct Mesh *me);
void boundbox_mesh(struct Mesh *me, float *loc, float *size);
void tex_space_mesh(struct Mesh *me);
float *mesh_create_orco_render(struct Object *ob);
float *mesh_create_orco(struct Object *ob);
void test_index_face(struct MFace *mface, struct MCol *mc, struct TFace *tface, int nr);
struct Mesh *get_mesh(struct Object *ob);
void set_mesh(struct Object *ob, struct Mesh *me);
void mball_to_mesh(struct ListBase *lb, struct Mesh *me);
void nurbs_to_mesh(struct Object *ob);
void mcol_to_tface(struct Mesh *me, int freedata);
struct MCol *tface_to_mcol_p(struct TFace *tface, int totface);
void tface_to_mcol(struct Mesh *me);
void free_dverts(struct MDeformVert *dvert, int totvert);
void copy_dverts(struct MDeformVert *dst, struct MDeformVert *src, int totvert); /* __NLA */
int update_realtime_texture(struct TFace *tface, double time);
void mesh_delete_material_index(struct Mesh *me, int index);
void mesh_set_smooth_flag(struct Object *meshOb, int enableSmooth);

struct BoundBox *mesh_get_bb(struct Mesh *me);
void mesh_get_texspace(struct Mesh *me, float *loc_r, float *rot_r, float *size_r);

void make_edges(struct Mesh *me);
void mesh_strip_loose_faces(struct Mesh *me);

	/* Calculate vertex and face normals, face normals are returned in *faceNors_r if non-NULL
	 * and vertex normals are stored in actual mverts.
	 */
void mesh_calc_normals(struct MVert *mverts, int numVerts, struct MFace *mfaces, int numFaces, float **faceNors_r);

	/* Return a newly MEM_malloc'd array of all the mesh vertex locations
	 * (_numVerts_r_ may be NULL) */
float (*mesh_getVertexCos(struct Mesh *me, int *numVerts_r))[3];

#ifdef __cplusplus
}
#endif

#endif

