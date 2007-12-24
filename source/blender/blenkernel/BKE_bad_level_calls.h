/**
 * blenlib/BKE_bad_level_calls.h (mar-2001 nzc)
 *	
 * Stuff that definitely does not belong in the kernel! These will all
 * have to be removed in order to restore sanity.
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
#ifndef BKE_BAD_LEVEL_CALLS_H
#define BKE_BAD_LEVEL_CALLS_H

extern ListBase editNurb;

#include "radio.h"
#include "BIF_editmesh.h"
#include "BIF_editmesh.h"
#include "BIF_editfont.h"
#include "BIF_editarmature.h"
#include "BIF_toolbox.h"
#include "BIF_interface.h"
#include "BIF_screen.h"

#include "BDR_editcurve.h"
#include "BDR_vpaint.h"

#include "BSE_sequence.h"



<<<<<<< .mine
=======
void mainqenter (unsigned short event, short val);
void waitcursor(int);
void allqueue(unsigned short event, short val);
#define REDRAWVIEW3D		0x4010
#define REDRAWBUTSOBJECT	0x4018
#define REDRAWBUTSEDIT		0x4019
struct Material;
extern struct Material defmaterial;


/* exotic.c */
void load_editMesh(void);
void make_editMesh(void);
struct EditMesh;
void free_editMesh(struct EditMesh *);
void free_editArmature(void);
void docenter_new(void);
int saveover(char *str);

/* image.c */
#include "DNA_image_types.h"
void free_realtime_image(Image *ima); // has to become a callback, opengl stuff

/* ipo.c */
void copy_view3d_lock(short val);	// was a hack, to make scene layer ipo's possible

/* library.c */
void allspace(unsigned short event, short val) ;
#define OOPS_TEST             2

/* mball.c */
extern struct ListBase editelems;

/* object.c */
struct ScriptLink;
void BPY_free_scriptlink(struct ScriptLink *slink);
void BPY_copy_scriptlink(struct ScriptLink *scriptlink);
float *give_cursor(void);  // become a callback or argument
void exit_posemode(int freedata);

/* packedFile.c */
short pupmenu(char *instr);  // will be general callback

/* sca.c */
#define LEFTMOUSE    0x001	// because of mouse sensor

/* scene.c */
#include "DNA_sequence_types.h"
void free_editing(struct Editing *ed);	// scenes and sequences problem...
void BPY_do_all_scripts (short int event);
int BPY_call_importloader(char *name);


extern char texstr[20][12];	/* buttons.c */


/* editsca.c */
void make_unique_prop_names(char *str);

/* DerivedMesh.c */
void bglBegin(int mode);
void bglVertex3fv(float *vec);
void bglVertex3f(float x, float y, float z);
void bglEnd(void);

struct Object;

/* booleanops.c */
struct DerivedMesh *NewBooleanDerivedMesh(struct Object *ob,
                                struct Object *ob_select, int int_op_type);

/* verse_*.c */
struct VerseVert;
struct VerseFace;
struct VerseSession;
struct VNode;
struct VTag;
struct VTagGroup;
struct VBitmapLayer;
struct VLink;
struct VLayer;
struct Mesh;

void post_vertex_create(struct VerseVert *vvert);
void post_vertex_set_xyz(struct VerseVert *vvert);
void post_vertex_delete(struct VerseVert *vvert);
void post_vertex_free_constraint(struct VerseVert *vvert);
void post_polygon_create(struct VerseFace *vface);
void post_polygon_set_corner(struct VerseFace *vface);
void post_polygon_delete(struct VerseFace *vface);
void post_polygon_free_constraint(struct VerseFace *vface);
void post_polygon_set_uint8(struct VerseFace *vface);
void post_node_create(struct VNode *vnode);
void post_node_destroy(struct VNode *vnode);
void post_node_name_set(struct VNode *vnode);
void post_tag_change(struct VTag *vtag);
void post_taggroup_create(struct VTagGroup *vtaggroup);
char *verse_client_name(void);
void post_transform(struct VNode *vnode);
void post_transform_pos(struct VNode *vnode);
void post_transform_rot(struct VNode *vnode);
void post_transform_scale(struct VNode *vnode);
void post_object_free_constraint(struct VNode *vnode);
void post_link_set(struct VLink *vlink);
void post_link_destroy(struct VLink *vlink);
void post_connect_accept(struct VerseSession *session);
void post_connect_terminated(struct VerseSession *session);
void post_connect_update(struct VerseSession *session);
void add_screenhandler(struct bScreen *sc, short eventcode, short val);
void post_bitmap_dimension_set(struct VNode *vnode);
void post_bitmap_layer_create(struct VBitmapLayer *vblayer);
void post_bitmap_layer_destroy(struct VBitmapLayer *vblayer);
void post_bitmap_tile_set(struct VBitmapLayer *vblayer, unsigned int xs, unsigned int ys);
void create_meshdata_from_geom_node(struct Mesh *me, struct VNode *vnode);
void post_geometry_free_constraint(struct VNode *vnode);
void post_layer_create(struct VLayer *vlayer);
void post_layer_destroy(struct VLayer *vlayer);
void post_server_add(void);

/* multires.c */
struct Multires;
struct MultiresLevel;
struct MultiresLevel *multires_level_n(struct Multires *mr, int n);
void multires_free(struct Multires *mr);
void multires_set_level(struct Object *ob, struct Mesh *me, const int render);
void multires_update_levels(struct Mesh *me, const int render);
void multires_calc_level_maps(struct MultiresLevel *lvl);
struct Multires *multires_copy(struct Multires *orig);
/* sculptmode.c */
struct SculptData;
void sculpt_reset_curve(struct SculptData *sd);
void sculptmode_free_all(struct Scene *sce);
void sculptmode_init(struct Scene *sce);

/* zbuf.c */
void antialias_tagbuf(int xsize, int ysize, char *rectmove);

/* imagetexture.c */
void ibuf_sample(struct ImBuf *ibuf, float fx, float fy, float dx, float dy, float *result);

/* modifier.c */
struct MeshDeformModifierData;

void harmonic_coordinates_bind(struct MeshDeformModifierData *mmd,
	float (*vertexcos)[3], int totvert, float cagemat[][4]);

/* particle.c */
struct ParticleSystem;

void PE_free_particle_edit(struct ParticleSystem *psys);
void PE_get_colors(char sel[4], char nosel[4]);
void PE_recalc_world_cos(struct Object *ob, struct ParticleSystem *psys);

>>>>>>> .r12991
#endif

