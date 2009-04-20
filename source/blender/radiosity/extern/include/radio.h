/* *************************************** 



    radio.h	nov/dec 1992
	revised for Blender may 1999

   $Id$
  
  ***** BEGIN GPL LICENSE BLOCK *****
 
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
  The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
  All rights reserved.
 
  The Original Code is: all of this file.
 
  Contributor(s): none yet.
 
  ***** END GPL LICENSE BLOCK *****
 */

#ifndef RADIO_H
#define RADIO_H 
#define RADIO_H 

/* type include */
#include "radio_types.h"

extern RadGlobal RG;

/* radfactors.c */
extern float calcStokefactor(RPatch *shoot, RPatch *rp, RNode *rn, float *area);
extern void calcTopfactors(void);
void calcSidefactors(void);
extern void initradiosity(void);
extern void rad_make_hocos(RadView *vw);
extern void hemizbuf(RadView *vw);
extern int makeformfactors(RPatch *shoot);
extern void applyformfactors(RPatch *shoot);
extern RPatch *findshootpatch(void);
extern void setnodeflags(RNode *rn, int flag, int set);
extern void backface_test(RPatch *shoot);
extern void clear_backface_test(void);
extern void progressiverad(void);
extern void minmaxradelem(RNode *rn, float *min, float *max);
extern void minmaxradelemfilt(RNode *rn, float *min, float *max, float *errmin, float *errmax);
extern void subdivideshootElements(int it);
extern void subdivideshootPatches(int it);
extern void inithemiwindows(void);
extern void closehemiwindows(void);
void rad_init_energy(void);

/* radio.c */
void freeAllRad(void);
int rad_phase(void);
void rad_status_str(char *str);
void rad_printstatus(void);
void rad_setlimits(void);
void set_radglobal(void);
void add_radio(void);
void delete_radio(void);
int rad_go(void);
void rad_subdivshootpatch(void);
void rad_subdivshootelem(void);
void rad_limit_subdivide(void);     

/* radnode.c */
extern void setnodelimit(float limit);
extern float *mallocVert(void);
extern float *callocVert(void);
extern void freeVert(float *vert);
extern int totalRadVert(void);
extern RNode *mallocNode(void);
extern RNode *callocNode(void);
extern void freeNode(RNode *node);
extern void freeNode_recurs(RNode *node);
extern RPatch *mallocPatch(void);
extern RPatch *callocPatch(void);
extern void freePatch(RPatch *patch);
extern void replaceAllNode(RNode *, RNode *);
extern void replaceAllNodeInv(RNode *neighb, RNode *old);
extern void replaceAllNodeUp(RNode *neighb, RNode *old);
extern void replaceTestNode(RNode *, RNode **, RNode *, int , float *);
extern void free_fastAll(void);

/* radnode.c */
extern void start_fastmalloc(char *str);
extern int setvertexpointersNode(RNode *neighb, RNode *node, int level, float **v1, float **v2);
extern float edlen(float *v1, float *v2);
extern void deleteNodes(RNode *node);
extern void subdivideTriNode(RNode *node, RNode *edge);
extern void subdivideNode(RNode *node, RNode *edge);
extern int comparelevel(RNode *node, RNode *nb, int level);

/* radpreprocess.c */
extern void splitconnected(void);
extern int vergedge(const void *v1,const void *v2);
extern void addedge(float *v1, float *v2, EdSort *es);
extern void setedgepointers(void);
extern void rad_collect_meshes(void);
extern void countelem(RNode *rn);
extern void countglobaldata(void);
extern void addelem(RNode ***el, RNode *rn, RPatch *rp);
extern void makeGlobalElemArray(void);
extern void remakeGlobaldata(void);
extern void splitpatch(RPatch *old);
extern void addpatch(RPatch *old, RNode *rn);
extern void converttopatches(void);
extern void make_elements(void);
extern void subdividelamps(void);
extern void maxsizePatches(void);
extern void subdiv_elements(void);

/* radpostprocess.c */
void addaccu(register char *z, register char *t);
void addaccuweight(register char *z, register char *t, int w);
void triaweight(Face *face, int *w1, int *w2, int *w3);
void init_face_tab(void);
Face *addface(void);
Face *makeface(float *v1, float *v2, float *v3, float *v4, RNode *rn);
void anchorQuadface(RNode *rn, float *v1, float *v2, float *v3, float *v4, int flag);
void anchorTriface(RNode *rn, float *v1, float *v2, float *v3, int flag);
float *findmiddlevertex(RNode *node, RNode *nb, float *v1, float *v2);
void make_face_tab(void);
void filterFaces(void);
void calcfiltrad(RNode *rn, float *cd);
void filterNodes(void);
void removeEqualNodes(short limit);
void rad_addmesh(void);
void rad_replacemesh(void);         

/* raddisplay.c */
extern char calculatecolor(float col);
extern void make_node_display(void);
extern void drawnodeWire(RNode *rn);
extern void drawsingnodeWire(RNode *rn);
extern void drawnodeSolid(RNode *rn);
extern void drawnodeGour(RNode *rn);
extern void drawpatch(RPatch *patch, unsigned int col);
extern void drawfaceGour(Face *face);
extern void drawfaceSolid(Face *face);
extern void drawfaceWire(Face *face);
extern void drawsquare(float *cent, float size, short cox, short coy);
extern void drawlimits(void);
extern void setcolNode(RNode *rn, unsigned int *col);
extern void pseudoAmb(void);
extern void rad_forcedraw(void);
extern void drawpatch_ext(RPatch *patch, unsigned int col);
extern void RAD_drawall(int depth_is_on);

/* radrender.c */
struct Render;
extern void do_radio_render(struct Render *re);
void end_radio_render(void);

#endif /* RADIO_H */

