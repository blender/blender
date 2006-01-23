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

/* blender.c */
void freeAllRad(void);
void free_editText(void);
void free_vertexpaint(void);

/* readfile.c */
struct PluginSeq;
void open_plugin_seq(struct PluginSeq *pis, char *seqname);
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

struct ID;
struct Script;
struct Text;
void BPY_do_pyscript (struct ID *id, short int event);
void BPY_clear_script (struct Script *script);
void BPY_free_compiled_text (struct Text *text);
void BPY_free_screen_spacehandlers (struct bScreen *sc);

/* writefile.c */
struct Oops;
void free_oops(struct Oops *oops);
void error(char *str, ...);

/* anim.c */
extern struct ListBase editNurb;

void mainqenter (unsigned short event, short val);
void waitcursor(int);
void allqueue(unsigned short event, short val);
#define REDRAWVIEW3D	0x4010
#define REDRAWBUTSEDIT	0x4019
struct Material;
extern struct Material defmaterial;


/* exotic.c */
void load_editMesh(void);
void make_editMesh(void);
struct EditMesh;
void free_editMesh(struct EditMesh *);
void free_editArmature(void);
void docentre_new(void);
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

struct DispListMesh;
struct Object;
struct DispListMesh *NewBooleanMeshDLM(struct Object *ob, struct Object *ob_select, int int_op_type);

#endif

