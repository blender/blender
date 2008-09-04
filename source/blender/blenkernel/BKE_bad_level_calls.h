/**
 * blenlib/BKE_bad_level_calls.h (mar-2001 nzc)
 *	
 * Stuff that definitely does not belong in the kernel! These will all
 * have to be removed in order to restore sanity.
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_BAD_LEVEL_CALLS_H
#define BKE_BAD_LEVEL_CALLS_H

/* blender.c */
void freeAllRad(void);
void free_editText(void);
void free_vertexpaint(void);

/* readfile.c */
struct SpaceButs;
void set_rects_butspace(struct SpaceButs *buts);
struct SpaceImaSel;
void check_imasel_copy(struct SpaceImaSel *simasel);
struct ScrArea;
struct bScreen;
void unlink_screen(struct bScreen *sc);
void setscreen(struct bScreen *sc);
void force_draw_all(int);
  /* otherwise the WHILE_SEQ doesn't work */
struct Sequence;
struct ListBase;
void build_seqar(struct ListBase *seqbase, struct Sequence  ***seqar, int *totseq);

/* BPython API */
struct ID;
struct Script;
struct Text;
struct IpoDriver; /* DNA_curve_types.h */
struct Object;
struct PyObject;
struct Node_Type;
struct BPy_Node;
struct bNode;
struct bNodeStack;
struct ShadeInput;
struct bPythonConstraint;
struct bConstraintOb;
struct bConstraintTarget;
void BPY_do_pyscript (struct ID *id, short int event);
void BPY_clear_script (struct Script *script);
void BPY_free_compiled_text (struct Text *text);
/* pydrivers */
struct Object **BPY_pydriver_get_objects(struct IpoDriver *driver);
float BPY_pydriver_eval(struct IpoDriver *driver);
void BPY_pydriver_update(void);
/* button python evaluation */
int BPY_button_eval(char *expr, double *value);
/* pyconstraints */
void BPY_pyconstraint_eval(struct bPythonConstraint *con, struct bConstraintOb *cob, struct ListBase *targets);
void BPY_pyconstraint_targets(struct bPythonConstraint *con, struct bConstraintTarget *ct);
/* pynodes */
int EXPP_dict_set_item_str(struct PyObject *dict, char *key, struct PyObject *value);
void Node_SetStack(struct BPy_Node *self, struct bNodeStack **stack, int type);
void InitNode(struct BPy_Node *self, struct bNode *node);
void Node_SetShi(struct BPy_Node *self, struct ShadeInput *shi);
struct BPy_NodeSockets *Node_CreateSocketLists(struct bNode *node);
int pytype_is_pynode(struct PyObject *pyob);
/* writefile.c */
struct Oops;
void free_oops(struct Oops *oops);
void error(char *str, ...);

/* anim.c */
extern struct ListBase editNurb;

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

#endif

