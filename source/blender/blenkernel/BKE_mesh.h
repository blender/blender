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

/* texflag */
#define AUTOSPACE               1

/* me->flag (let op alternatieven in blendpsx.h) */
#define ME_ISDONE               1
#define ME_NOPUNOFLIP   2
#define ME_TWOSIDED             4
#define ME_UVEFFECT             8
#define ME_VCOLEFFECT   16
#define ME_AUTOSMOOTH   32
#define ME_SMESH                64
#define ME_SUBSURF				128

/* puno (mface) */
#define ME_FLIPV1               1
#define ME_FLIPV2               2
#define ME_FLIPV3               4
#define ME_FLIPV4               8
#define ME_PROJXY               16
#define ME_PROJXZ               32
#define ME_PROJYZ               64

/* edcode (mface) */
#define ME_V1V2                 1
#define ME_V2V3                 2
#define ME_V3V1                 4
#define ME_V3V4                 4
#define ME_V4V1                 8

/* flag (mface) */
#define ME_SMOOTH               1

/***/

struct DispList;
struct ListBase;
struct MDeformVert;
struct Mesh;
struct MFace;
struct MVert;
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
void make_orco_displist_mesh(struct Object *ob, int subdivlvl);
void make_orco_mesh(struct Mesh *me);
void test_index_mface(struct MFace *mface, int nr);
void test_index_face(struct MFace *mface, struct TFace *tface, int nr);
void flipnorm_mesh(struct Mesh *me);
struct Mesh *get_mesh(struct Object *ob);
void set_mesh(struct Object *ob, struct Mesh *me);
void mball_to_mesh(struct ListBase *lb, struct Mesh *me);
void nurbs_to_mesh(struct Object *ob);
void edge_drawflags_mesh(struct Mesh *me);
void mcol_to_tface(struct Mesh *me, int freedata);
void tface_to_mcol(struct Mesh *me);
void free_dverts(struct MDeformVert *dvert, int totvert);
void copy_dverts(struct MDeformVert *dst, struct MDeformVert *src, int totvert); /* __NLA */
int mesh_uses_displist(struct Mesh *me);
int rendermesh_uses_displist(struct Mesh *me);
float get_mvert_weight (struct Object *ob, int vert, int defgroup);
int update_realtime_texture(struct TFace *tface, double time);

#ifdef __cplusplus
}
#endif

#endif
