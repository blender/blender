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


#include "BLI_blenlib.h"

#include "DNA_material_types.h"

#include "BKE_bad_level_calls.h"

int winqueue_break= 0;

/* readfile.c */
	/* struct PluginSeq; */
void open_plugin_seq(struct PluginSeq *pis, char *seqname){}
	/* struct SpaceButs; */
void set_rects_butspace(struct SpaceButs *buts){}
	/* struct SpaceImaSel; */
void check_imasel_copy(struct SpaceImaSel *simasel){}
	/* struct ScrArea; */
void unlink_screen(struct bScreen *sc){}
void freeAllRad(void){}
void free_editText(void){}
void free_editArmature(void){}


void setscreen(struct bScreen *sc){}
void force_draw_all(void){}
  /* otherwise the WHILE_SEQ doesn't work */
	/* struct Sequence; */

/* MAART: added "seqar = 0; totseq = 0" because the loader will crash without it. */ 
void build_seqar(ListBase *seqbase, struct Sequence  ***seqar, int *totseq)
{
	*seqar = 0;
	*totseq = 0;
}

void BPY_do_pyscript(ID *id, short int event){}
void BPY_free_compiled_text(struct Text *text) {};

/* writefile.c */
	/* struct Oops; */
void free_oops(struct Oops *oops){}
void exit_posemode(int freedata){}
void error(char *str, ...){}

/* anim.c */
ListBase editNurb;

/* displist.c */
#include "DNA_world_types.h"	/* for render_types */
#include "render_types.h"
struct RE_Render R;
float   RE_Spec(float inp, int hard){}
void waitcursor(int val){}
void allqueue(unsigned short event, short val){}
#define REDRAWVIEW3D	0x4010
Material defmaterial;

/* effect.c */
void    RE_jitterate1(float *jit1, float *jit2, int num, float rad1){}
void    RE_jitterate2(float *jit1, float *jit2, int num, float rad2){}

/* exotic.c */
void load_editMesh(void){}
void make_editMesh(void){}
void free_editMesh(void){}
void docentre_new(void){}
int saveover(char *str){}

/* image.c */
#include "DNA_image_types.h"
void free_realtime_image(Image *ima){} // has to become a callback, opengl stuff
void RE_make_existing_file(char *name){} // from render, but these funcs should be moved anyway 

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
float *give_cursor(void){}  // become a callback or argument


/* packedFile.c */
short pupmenu(char *instr){}  // will be general callback

/* sca.c */
#define LEFTMOUSE    0x001	// because of mouse sensor

/* scene.c */
#include "DNA_sequence_types.h"
void free_editing(struct Editing *ed){}	// scenes and sequences problem...
void BPY_do_all_scripts (short int event){}

/* IKsolver stubs */
#include "IK_solver.h"
extern int IK_LoadChain(IK_Chain_ExternPtr chain,IK_Segment_ExternPtr segments, int num_segs)
{
	return 0;
}

extern int IK_SolveChain(
	IK_Chain_ExternPtr chain,
	float goal[3],
	float tolerance,
	int max_iterations,
	float max_angle_change, 
	IK_Segment_ExternPtr output
	)
{
	return 0;
}

extern void IK_FreeChain(IK_Chain_ExternPtr chain)
{
	;
}


extern IK_Chain_ExternPtr IK_CreateChain(void)
{
	return 0;
}



/* texture.c */
#define FLO 128
#define INT	96
	/* struct EnvMap; */
	/* struct Tex; */
void    RE_free_envmap(struct EnvMap *env){}      
struct EnvMap *RE_copy_envmap(struct EnvMap *env){}
void    RE_free_envmapdata(struct EnvMap *env){}
int     RE_envmaptex(struct Tex *tex, float *texvec, float *dxt, float *dyt){}
void    RE_calc_R_ref(void){}
char texstr[15][8];	/* buttons.c */
Osa O;

/* editsca.c */
void make_unique_prop_names(char *str) {}
