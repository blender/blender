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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef RENDERDATABASE_H
#define RENDERDATABASE_H

struct Object;
struct VlakRen;
struct VertRen;
struct HaloRen;
struct Material;
struct Render;
struct MCol;
struct MTFace;
struct CustomData;
struct StrandBuffer;
struct StrandRen;
struct ObjectInstanceRen;
struct RadFace;

#define RE_QUAD_MASK	0x7FFFFFF
#define RE_QUAD_OFFS	0x8000000

/* render allocates totvert/256 of these nodes, for lookup and quick alloc */
typedef struct VertTableNode {
	struct VertRen *vert;
	float *rad;
	float *sticky;
	float *strand;
	float *tangent;
	float *stress;
	float *winspeed;
} VertTableNode;

typedef struct VlakTableNode {
	struct VlakRen *vlak;
	struct MTFace *mtface;
	struct MCol *mcol;
	int totmtface, totmcol;
	float *surfnor;
	float *tangent;
	struct RadFace **radface;
} VlakTableNode;

typedef struct StrandTableNode {
	struct StrandRen *strand;
	float *winspeed;
	float *surfnor;
	float *simplify;
	int *face;
	struct MCol *mcol;
	float *uv;
	int totuv, totmcol;
} StrandTableNode;

/* renderdatabase.c */
void free_renderdata_tables(struct Render *re);
void free_renderdata_vertnodes(struct VertTableNode *vertnodes);
void free_renderdata_vlaknodes(struct VlakTableNode *vlaknodes);

void set_normalflags(struct Render *re, struct ObjectRen *obr);
void project_renderdata(struct Render *re, void (*projectfunc)(float *, float mat[][4], float *),  int do_pano, float xoffs, int do_buckets);
int clip_render_object(float boundbox[][3], float *bounds, float mat[][4]);

/* functions are not exported... so wrong names */

struct VlakRen *RE_findOrAddVlak(struct ObjectRen *obr, int nr);
struct VertRen *RE_findOrAddVert(struct ObjectRen *obr, int nr);
struct StrandRen *RE_findOrAddStrand(struct ObjectRen *obr, int nr);
struct HaloRen *RE_findOrAddHalo(struct ObjectRen *obr, int nr);
struct HaloRen *RE_inithalo(struct Render *re, struct ObjectRen *obr, struct Material *ma, float *vec, float *vec1, float *orco, float hasize,  float vectsize, int seed);
struct HaloRen *RE_inithalo_particle(struct Render *re, struct ObjectRen *obr, struct DerivedMesh *dm, struct Material *ma,   float *vec,   float *vec1, float *orco, float *uvco, float hasize, float vectsize, int seed);
struct StrandBuffer *RE_addStrandBuffer(struct ObjectRen *obr, int totvert);

struct ObjectRen *RE_addRenderObject(struct Render *re, struct Object *ob, struct Object *par, int index, int psysindex, int lay);
struct ObjectInstanceRen *RE_addRenderInstance(struct Render *re, struct ObjectRen *obr, struct Object *ob, struct Object *par, int index, int psysindex, float mat[][4], int lay);
void RE_makeRenderInstances(struct Render *re);
void RE_instanceTransformNormal(struct ObjectInstanceRen *obi, float *nor, float *tnor);

float *RE_vertren_get_sticky(struct ObjectRen *obr, struct VertRen *ver, int verify);
float *RE_vertren_get_stress(struct ObjectRen *obr, struct VertRen *ver, int verify);
float *RE_vertren_get_rad(struct ObjectRen *obr, struct VertRen *ver, int verify);
float *RE_vertren_get_strand(struct ObjectRen *obr, struct VertRen *ver, int verify);
float *RE_vertren_get_tangent(struct ObjectRen *obr, struct VertRen *ver, int verify);
float *RE_vertren_get_winspeed(struct ObjectInstanceRen *obi, struct VertRen *ver, int verify);

struct MTFace *RE_vlakren_get_tface(struct ObjectRen *obr, VlakRen *ren, int n, char **name, int verify);
struct MCol *RE_vlakren_get_mcol(struct ObjectRen *obr, VlakRen *ren, int n, char **name, int verify);
float *RE_vlakren_get_surfnor(struct ObjectRen *obr, VlakRen *ren, int verify);
float *RE_vlakren_get_nmap_tangent(struct ObjectRen *obr, VlakRen *ren, int verify);
RadFace **RE_vlakren_get_radface(struct ObjectRen *obr, VlakRen *ren, int verify);
int RE_vlakren_get_normal(struct Render *re, struct ObjectInstanceRen *obi, struct VlakRen *vlr, float *nor);

float *RE_strandren_get_surfnor(struct ObjectRen *obr, struct StrandRen *strand, int verify);
float *RE_strandren_get_uv(struct ObjectRen *obr, struct StrandRen *strand, int n, char **name, int verify);
struct MCol *RE_strandren_get_mcol(struct ObjectRen *obr, struct StrandRen *strand, int n, char **name, int verify);
float *RE_strandren_get_simplify(struct ObjectRen *obr, struct StrandRen *strand, int verify);
int *RE_strandren_get_face(struct ObjectRen *obr, struct StrandRen *strand, int verify);
float *RE_strandren_get_winspeed(struct ObjectInstanceRen *obi, struct StrandRen *strand, int verify);

struct VertRen *RE_vertren_copy(struct ObjectRen *obr, struct VertRen *ver);
struct VlakRen *RE_vlakren_copy(struct ObjectRen *obr, struct VlakRen *vlr);

void RE_set_customdata_names(struct ObjectRen *obr, struct CustomData *data);

/* haloren->type: flags */
#define HA_ONLYSKY		1
#define HA_VECT			2
#define HA_XALPHA		4
#define HA_FLARECIRC	8

/* convertblender.c */
void init_render_world(Render *re);
void RE_Database_FromScene_Vectors(Render *re, struct Scene *sce);


#endif /* RENDERDATABASE_H */

