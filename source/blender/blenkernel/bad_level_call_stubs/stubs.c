
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
 * BKE_bad_level_calls function stubs
 */

#include <stdlib.h>

#include "BKE_bad_level_calls.h"
#include "BLI_blenlib.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "RE_render_ext.h"
#include "RE_shader_ext.h"
#include "RE_pipeline.h"

int winqueue_break= 0;

char bprogname[1];
char btempdir[1];

struct IpoCurve;
struct FluidsimSettings;
struct Render;
struct RenderResult;
struct Object;
struct bPythonConstraint;
struct bConstraintOb;
struct bConstraintTarget;
struct ListBase;
struct EditFace;

char *getIpoCurveName( struct IpoCurve * icu );
void insert_vert_icu(struct IpoCurve *icu, float x, float y, short fast);
struct IpoCurve *verify_ipocurve(struct ID *id, short a, char *b, char *d, int e);
void elbeemDebugOut(char *msg);
void fluidsimSettingsFree(struct FluidsimSettings* sb);
void fluidsimSettingsCopy(struct FluidsimSettings* sb);


/* readfile.c */
	/* struct SpaceButs; */
void set_rects_butspace(struct SpaceButs *buts){}
	/* struct SpaceImaSel; */
void check_imasel_copy(struct SpaceImaSel *simasel){}
	/* struct ScrArea; */
void unlink_screen(struct bScreen *sc){}
void freeAllRad(void){}
void free_editText(void){}
void free_editArmature(void){}
void free_vertexpaint(void){}

char *getIpoCurveName( struct IpoCurve * icu ) 
{
	return 0;
}

void insert_vert_icu(struct IpoCurve *icu, float x, float y, short fast)
{
}


struct IpoCurve *verify_ipocurve(struct ID *id, short a, char *b, char *d, int e)
{
	return 0;
}


void setscreen(struct bScreen *sc){}
void force_draw_all(int header){}
  /* otherwise the WHILE_SEQ doesn't work */
	/* struct Sequence; */

/* MAART: added "seqar = 0; totseq = 0" because the loader will crash without it. */ 
void build_seqar(struct ListBase *seqbase, struct Sequence  ***seqar, int *totseq)
{
	*seqar = 0;
	*totseq = 0;
}

/* blender.c */
void mainqenter (unsigned short event, short val){}

void BPY_do_pyscript(ID *id, short int event){}
void BPY_clear_script(Script *script){}
void BPY_free_compiled_text(struct Text *text){}
void BPY_pydriver_update(void){}
float BPY_pydriver_eval(struct IpoDriver *driver)
{
	return 0;
}
int EXPP_dict_set_item_str(struct PyObject *dict, char *key, struct PyObject *value)
{
	return 0;
}
void Node_SetStack(struct BPy_Node *self, struct bNodeStack **stack, int type){}
void InitNode(struct BPy_Node *self, struct bNode *node){}
void Node_SetShi(struct BPy_Node *self, struct ShadeInput *shi){}
struct BPy_NodeSockets *Node_CreateSocketLists(struct bNode *node)
{
	return 0;
}
int pytype_is_pynode(struct PyObject *pyob)
{
	return 0;
}
/* depsgraph.c: */
struct Object **BPY_pydriver_get_objects(struct IpoDriver *driver)
{
	return 0;
}
int BPY_button_eval(char *expr, double *value)
{
	return 0;
}

/* PyConstraints - BPY_interface.c */
void BPY_pyconstraint_eval(struct bPythonConstraint *con, struct bConstraintOb *cob, struct ListBase *targets)
{
}
void BPY_pyconstraint_target(struct bPythonConstraint *con, struct bConstraintTarget *ct)
{
}


/* writefile.c */
	/* struct Oops; */
void free_oops(struct Oops *oops){}
void exit_posemode(int freedata){}
void error(char *str, ...){}

/* anim.c */
ListBase editNurb;

void waitcursor(int val){}
void allqueue(unsigned short event, short val){}
#define REDRAWVIEW3D	0x4010
Material defmaterial;

/* exotic.c */
void load_editMesh(void){}
void make_editMesh(void){}
void free_editMesh(struct EditMesh *em){}
void docenter_new(void){}
int saveover(char *str){ return 0;}

/* image.c */
#include "DNA_image_types.h"
void free_realtime_image(Image *ima){} // has to become a callback, opengl stuff

/* ipo.c */
void copy_view3d_lock(short val){}	// was a hack, to make scene layer ipo's possible

/* library.c */
void allspace(unsigned short event, short val){}
#define OOPS_TEST             2

/* mball.c */
ListBase editelems;

/* object.c */
void BPY_free_scriptlink(ScriptLink *slink){}
void BPY_copy_scriptlink(ScriptLink *scriptlink){}
float *give_cursor(void){ return 0;}  // become a callback or argument


/* packedFile.c */
short pupmenu(char *instr){ return 0;}  // will be general callback

/* sca.c */
#define LEFTMOUSE    0x001	// because of mouse sensor

/* scene.c */
#include "DNA_sequence_types.h"
void free_editing(struct Editing *ed){}	// scenes and sequences problem...
void BPY_do_all_scripts (short int event){}

/*editmesh_lib.c*/
void EM_select_face(struct EditFace *efa, int sel) {}
void EM_select_edge(struct EditEdge *eed, int sel) {}

/*editmesh.c*/
struct EditVert *addvertlist(float *vec, struct EditVert *example) { return 0;}
struct EditEdge *addedgelist(struct EditVert *v1, struct EditVert *v2, struct EditEdge *example) { return 0;}
struct EditFace *addfacelist(struct EditVert *v1, struct EditVert *v2, struct EditVert *v3, struct EditVert *v4, struct EditFace *example, struct EditFace *exampleEdges) { return 0;}
struct EditEdge *findedgelist(struct EditVert *v1, struct EditVert *v2)  { return 0;}
/*edit.c*/

void countall(void) {}


/* IKsolver stubs */
#include "IK_solver.h"

IK_Segment *IK_CreateSegment(int flag) { return 0; }
void IK_FreeSegment(IK_Segment *seg) {}

void IK_SetParent(IK_Segment *seg, IK_Segment *parent) {}
void IK_SetTransform(IK_Segment *seg, float start[3], float rest_basis[][3], float basis[][3], float length) {}
void IK_GetBasisChange(IK_Segment *seg, float basis_change[][3]) {}
void IK_GetTranslationChange(IK_Segment *seg, float *translation_change) {};
void IK_SetLimit(IK_Segment *seg, IK_SegmentAxis axis, float lower, float upper) {};
void IK_SetStiffness(IK_Segment *seg, IK_SegmentAxis axis, float stiffness) {};

IK_Solver *IK_CreateSolver(IK_Segment *root) { return 0; }
void IK_FreeSolver(IK_Solver *solver) {};

void IK_SolverAddGoal(IK_Solver *solver, IK_Segment *tip, float goal[3], float weight) {}
void IK_SolverAddGoalOrientation(IK_Solver *solver, IK_Segment *tip, float goal[][3], float weight) {}
void IK_SolverSetPoleVectorConstraint(IK_Solver *solver, IK_Segment *tip, float goal[3], float polegoal[3], float poleangle, int getangle) {}
float IK_SolverGetPoleAngle(IK_Solver *solver) { return 0.0f; }

int IK_Solve(IK_Solver *solver, float tolerance, int max_iterations) { return 0; }

/* exotic.c */
int BPY_call_importloader(char *name)
{
	return 0;
}


/* texture.c */
#define FLO 128
#define INT	96


char texstr[20][12];	/* buttons.c */

/* editsca.c */
void make_unique_prop_names(char *str) {}

/* DerivedMesh.c */
void bglBegin(int mode) {}
void bglVertex3fv(float *vec) {}
void bglVertex3f(float x, float y, float z) {}
void bglEnd(void) {}

/* booleanops.c */
struct DerivedMesh *NewBooleanDerivedMesh(struct Object *ob, struct Object *ob_select, int int_op_type) { return 0; }

// bobj read/write debug messages
void elbeemDebugOut(char *msg) {}
void fluidsimSettingsFree(struct FluidsimSettings* sb) {}
void fluidsimSettingsCopy(struct FluidsimSettings* sb) {}

/*new render funcs */
int     externtex(struct MTex *mtex, float *vec, float *tin, float *tr, float *tg, float *tb, float *ta) { return 0; }
void texture_rgb_blend(float *in, float *tex, float *out, float fact, float facg, int blendtype) {}
float texture_value_blend(float tex, float out, float fact, float facg, int blendtype, int flip) { return 0; }

void RE_FreeRenderResult(struct RenderResult *rr) {}
void RE_GetResultImage(struct Render *re, struct RenderResult *rr) {}
struct RenderResult *RE_MultilayerConvert(void *exrhandle, int rectx, int recty){return NULL;}
struct Render *RE_GetRender(const char *name) {return (struct Render *)NULL;}
struct RenderResult *RE_GetResult(Render *re) {return (struct RenderResult *)NULL;}
float *RE_RenderLayerGetPass(RenderLayer *rl, int passtype) {return NULL;}
float RE_filter_value(int type, float x) {return 0.0f;}
struct RenderLayer *RE_GetRenderLayer(RenderResult *rr, const char *name) {return (struct RenderLayer *)NULL;}
void RE_Database_Free (struct Render *re) {}
void RE_FreeRender(Render *re) {}
void RE_shade_external(Render *re, ShadeInput *shi, ShadeResult *shr) {}
void RE_DataBase_GetView(Render *re, float mat[][4]) {}
struct Render *RE_NewRender(const char *name) {return (struct Render *)NULL;}
void RE_Database_Baking(struct Render *re, struct Scene *scene, int type, struct Object *actob) {};


/* node_composite.c */
void RE_zbuf_accumulate_vecblur(struct NodeBlurData *nd, int xsize, int ysize, float *newrect, float *imgrect, float *vecbufrect, float *zbufrect) {}

int multitex_ext(Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, TexResult *texres)
{
	return 1969;
}

/* verse */

void post_vertex_create(struct VerseVert *vvert) {}
void post_vertex_set_xyz(struct VerseVert *vvert) {}
void post_vertex_delete(struct VerseVert *vvert) {}
void post_vertex_free_constraint(struct VerseVert *vvert) {}
void post_polygon_create(struct VerseFace *vface) {}
void post_polygon_set_corner(struct VerseFace *vface) {}
void post_polygon_delete(struct VerseFace *vface) {}
void post_polygon_free_constraint(struct VerseFace *vface) {}
void post_polygon_set_uint8(struct VerseFace *vface) {}
void post_node_create(struct VNode *vnode) {}
void post_node_destroy(struct VNode *vnode) {}
void post_node_name_set(struct VNode *vnode) {}
void post_tag_change(struct VTag *vtag) {}
void post_taggroup_create(struct VTagGroup *vtaggroup) {}
char *verse_client_name(void) { return NULL; }
void post_transform(struct VNode *vnode) {}
void post_transform_pos(struct VNode *vnode) {}
void post_transform_rot(struct VNode *vnode) {}
void post_transform_scale(struct VNode *vnode) {}
void post_object_free_constraint(struct VNode *vnode) {}
void post_link_set(struct VLink *vlink) {}
void post_link_destroy(struct VLink *vlink) {}
void post_connect_accept(struct VerseSession *session) {}
void post_connect_terminated(struct VerseSession *session) {}
void post_connect_update(struct VerseSession *session) {}
void add_screenhandler(struct bScreen *sc, short eventcode, short val) {}
void post_bitmap_dimension_set(struct VNode *vnode) {}
void post_bitmap_layer_create(struct VBitmapLayer *vblayer) {}
void post_bitmap_layer_destroy(struct VBitmapLayer *vblayer) {}
void post_bitmap_tile_set(struct VBitmapLayer *vblayer, unsigned int xs, unsigned int ys) {}
void create_meshdata_from_geom_node(struct Mesh *me, struct VNode *vnode) {}
void post_geometry_free_constraint(struct VNode *vnode) {}
void post_layer_create(struct VLayer *vlayer) {}
void post_layer_destroy(struct VLayer *vlayer) {}
void post_server_add(void) {}

/* zbuf.c stub */
void antialias_tagbuf(int xsize, int ysize, char *rectmove) {}

/* imagetexture.c stub */
void ibuf_sample(struct ImBuf *ibuf, float fx, float fy, float dx, float dy, float *result) {}

void update_for_newframe() {}

struct FileList;
void BIF_filelist_freelib(struct FileList* filelist) {};

/* edittime.c stub */
TimeMarker *get_frame_marker(int frame){return 0;};

/* editseq.c */
Sequence *get_forground_frame_seq(int frame){return 0;};
void set_last_seq(Sequence *seq){};

/* modifier.c stub */
void harmonic_coordinates_bind(struct MeshDeformModifierData *mmd,
	float (*vertexcos)[3], int totvert, float cagemat[][4]) {}

/* particle.c */
void PE_free_particle_edit(struct ParticleSystem *psys) {}
void PE_get_colors(char sel[4], char nosel[4]) {}
void PE_recalc_world_cos(struct Object *ob, struct ParticleSystem *psys) {}

/* text.c */
void txt_copy_clipboard (struct Text *text){}

char stipple_quarttone[1];

