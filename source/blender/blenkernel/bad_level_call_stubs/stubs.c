
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
 * BKE_bad_level_calls function stubs
 */

#include <stdlib.h>

#include "BKE_bad_level_calls.h"
#include "BLI_blenlib.h"
#include "BPI_script.h"
#include "DNA_texture_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "RE_render_ext.h"
#include "RE_shader_ext.h"
#include "RE_pipeline.h"

int winqueue_break= 0;

char bprogname[1];

struct IpoCurve;
struct FluidsimSettings;
struct Render;
struct RenderResult;

char *getIpoCurveName( struct IpoCurve * icu );
void insert_vert_ipo(struct IpoCurve *icu, float x, float y);
struct IpoCurve *verify_ipocurve(struct ID *id, short a, char *b, char *d, int e);
void elbeemDebugOut(char *msg);
void fluidsimSettingsFree(struct FluidsimSettings* sb);


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

void insert_vert_ipo(struct IpoCurve *icu, float x, float y)
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
void BPY_free_screen_spacehandlers (struct bScreen *sc){}

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
void docentre_new(void){}
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

struct DispListMesh *NewBooleanMeshDLM(struct Object *ob, struct Object *ob_select, int int_op_type) { return 0; }

// bobj read/write debug messages
void elbeemDebugOut(char *msg) {}
void fluidsimSettingsFree(struct FluidsimSettings* sb) {}

/*new render funcs */
void     externtex(struct MTex *mtex, float *vec, float *tin, float *tr, float *tg, float *tb, float *ta) {}
int		multitex(struct Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, struct TexResult *texres) {return 0;}
struct Render *RE_GetRender(const char *name) {return (struct Render *)NULL;}
struct RenderResult *RE_GetResult(Render *re) {return (struct RenderResult *)NULL;}
float *RE_RenderLayerGetPass(RenderLayer *rl, int passtype) {return NULL;}
float RE_filter_value(int type, float x) {return 0.0f;}

/* node_composite.c */
void RE_zbuf_accumulate_vecblur(struct NodeBlurData *nd, int xsize, int ysize, float *newrect, float *imgrect, float *vecbufrect, float *zbufrect) {}
