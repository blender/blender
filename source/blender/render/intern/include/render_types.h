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
 * Contributor(s): (c) 2006 Blender Foundation, full refactor
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef RENDER_TYPES_H
#define RENDER_TYPES_H

/* ------------------------------------------------------------------------- */
/* exposed internal in render module only! */
/* ------------------------------------------------------------------------- */

#include "DNA_color_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "DNA_object_types.h"
#include "DNA_vec_types.h"

#include "BLI_threads.h"

#include "RE_pipeline.h"
#include "RE_shader_ext.h"	/* TexResult, ShadeResult, ShadeInput */
#include "sunsky.h"

#include "BLO_sys_types.h" // for intptr_t support

struct Object;
struct MemArena;
struct VertTableNode;
struct VlakTableNode;
struct GHash;
struct RenderBuckets;
struct ObjectInstanceRen;
struct RayObject;
struct RayFace;

#define TABLEINITSIZE 1024
#define LAMPINITSIZE 256

typedef struct SampleTables
{
	float centLut[16];
	float *fmask1[9], *fmask2[9];
	char cmask[256], *centmask;
	
} SampleTables;

typedef struct QMCSampler
{
	struct QMCSampler *next, *prev;
	int type;
	int tot;
	int used;
	double *samp2d;
	double offs[BLENDER_MAX_THREADS][2];
} QMCSampler;

#define SAMP_TYPE_JITTERED		0
#define SAMP_TYPE_HALTON		1
#define SAMP_TYPE_HAMMERSLEY	2

/* this is handed over to threaded hiding/passes/shading engine */
typedef struct RenderPart
{
	struct RenderPart *next, *prev;
	
	RenderResult *result;			/* result of part rendering */
	ListBase fullresult;			/* optional full sample buffers */
	
	int *recto;						/* object table for objects */
	int *rectp;						/* polygon index table */
	int *rectz;						/* zbuffer */
	int *rectmask;					/* negative zmask */
	intptr_t *rectdaps;					/* delta acum buffer for pixel structs */
	int *rectbacko;					/* object table for backside sss */
	int *rectbackp;					/* polygon index table for backside sss */
	int *rectbackz;					/* zbuffer for backside sss */
	intptr_t *rectall;					/* buffer for all faces for sss */

	rcti disprect;					/* part coordinates within total picture */
	int rectx, recty;				/* the size */
	short crop, ready;				/* crop is amount of pixels we crop, for filter */
	short sample, nr;				/* sample can be used by zbuffers, nr is partnr */
	short thread;					/* thread id */
	
	char *clipflag;					/* clipflags for part zbuffering */
} RenderPart;

/* controls state of render, everything that's read-only during render stage */
struct Render
{
	struct Render *next, *prev;
	char name[RE_MAXNAME];
	int slot;
	
	/* state settings */
	short flag, osa, ok, result_ok;
	
	/* result of rendering */
	RenderResult *result;
	/* if render with single-layer option, other rendered layers are stored here */
	RenderResult *pushedresult;
	/* a list of RenderResults, for fullsample */
	ListBase fullresult;	
	/* read/write mutex, all internal code that writes to re->result must use a
	   write lock, all external code must use a read lock. internal code is assumed
	   to not conflict with writes, so no lock used for that */
	ThreadRWMutex resultmutex;
	
	/* window size, display rect, viewplane */
	int winx, winy;
	rcti disprect;			/* part within winx winy */
	rctf viewplane;			/* mapped on winx winy */
	float viewdx, viewdy;	/* size of 1 pixel */
	float clipcrop;			/* 2 pixel boundary to prevent clip when filter used */
	
	/* final picture width and height (within disprect) */
	int rectx, recty;
	
	/* real maximum amount of xparts/yparts after correction for minimum */
	int xparts, yparts;
	/* real maximum size of parts after correction for minimum 
	   partx*xparts can be larger than rectx, in that case last part is smaller */
	int partx, party;
	
	/* values for viewing */
	float lens, ycor, viewfac;
	float panophi, panosi, panoco, panodxp, panodxv;
	
	/* Matrices */
	float grvec[3];			/* for world */
	float imat[3][3];		/* copy of viewinv */
	float viewmat[4][4], viewinv[4][4];
	float winmat[4][4];
	
	/* clippping */
	float clipsta;
	float clipend;
	
	/* samples */
	SampleTables *samples;
	float jit[32][2];
	float mblur_jit[32][2];
	ListBase *qmcsamplers;
	
	/* shadow counter, detect shadow-reuse for shaders */
	int shadowsamplenr[BLENDER_MAX_THREADS];
	
	/* scene, and its full copy of renderdata and world */
	Scene *scene;
	RenderData r;
	World wrld;
	
	ListBase parts;
	
	/* octree tables and variables for raytrace */
	struct RayObject *raytree;
	struct RayFace *rayfaces;
	struct VlakPrimitive *rayprimitives;
	float maxdist; /* needed for keeping an incorrect behaviour of SUN and HEMI lights (avoid breaking old scenes) */

	/* occlusion tree */
	void *occlusiontree;
	ListBase strandsurface;
	
	/* use this instead of R.r.cfra */
	float cfra;	
	
	/* render database */
	int totvlak, totvert, tothalo, totstrand, totlamp;
	struct HaloRen **sortedhalos;

	ListBase lights;	/* GroupObject pointers */
	ListBase lampren;	/* storage, for free */
	
	ListBase objecttable;

	struct ObjectInstanceRen *objectinstance;
	ListBase instancetable;
	int totinstance;

	struct Image *backbuf, *bakebuf;
	
	struct GHash *orco_hash;

	struct GHash *sss_hash;
	ListBase *sss_points;
	struct Material *sss_mat;

	ListBase customdata_names;

	struct Object *excludeob;
	ListBase render_volumes_inside;
	ListBase volumes;
	ListBase volume_precache_parts;

	/* arena for allocating data for use during render, for
		* example dynamic TFaces to go in the VlakRen structure.
		*/
	struct MemArena *memArena;
	
	/* callbacks */
	void (*display_init)(void *handle, RenderResult *rr);
	void *dih;
	void (*display_clear)(void *handle, RenderResult *rr);
	void *dch;
	void (*display_draw)(void *handle, RenderResult *rr, volatile rcti *rect);
	void *ddh;
	
	void (*stats_draw)(void *handle, RenderStats *ri);
	void *sdh;
	void (*timecursor)(void *handle, int i);
	void *tch;
	
	int (*test_break)(void *handle);
	void *tbh;
	
	void (*error)(void *handle, char *str);
	void *erh;
	
	RenderStats i;
};

/* ------------------------------------------------------------------------- */

struct ISBData;

typedef struct DeepSample {
	int z;
	float v;
} DeepSample;
 
typedef struct ShadSampleBuf {
	struct ShadSampleBuf *next, *prev;
	intptr_t *zbuf;
	char *cbuf;
	DeepSample **deepbuf;
	int *totbuf;
} ShadSampleBuf;

typedef struct ShadBuf {
	/* regular shadowbuffer */
	short samp, shadhalostep, totbuf;
	float persmat[4][4];
	float viewmat[4][4];
	float winmat[4][4];
	float *jit, *weight;
	float d, clipend, pixsize, soft, compressthresh;
	int co[3];
	int size, bias;
	ListBase buffers;
	
	/* irregular shadowbufer, result stored per thread */
	struct ISBData *isb_result[BLENDER_MAX_THREADS];
} ShadBuf;

/* ------------------------------------------------------------------------- */

typedef struct ObjectRen {
	struct ObjectRen *next, *prev;
	struct Object *ob, *par;
	struct Scene *sce;
	int index, psysindex, flag, lay;

	float boundbox[2][3];

	int totvert, totvlak, totstrand, tothalo;
	int vertnodeslen, vlaknodeslen, strandnodeslen, blohalen;
	struct VertTableNode *vertnodes;
	struct VlakTableNode *vlaknodes;
	struct StrandTableNode *strandnodes;
	struct HaloRen **bloha;
	struct StrandBuffer *strandbuf;

	char (*mtface)[32];
	char (*mcol)[32];
	int  actmtface, actmcol, bakemtface;

	float obmat[4][4];	/* only used in convertblender.c, for instancing */

	/* used on makeraytree */
	struct RayObject *raytree;
	struct RayFace *rayfaces;
	struct VlakPrimitive *rayprimitives;
	struct ObjectInstanceRen *rayobi;
	
} ObjectRen;

typedef struct ObjectInstanceRen {
	struct ObjectInstanceRen *next, *prev;

	ObjectRen *obr;
	Object *ob, *par;
	int index, psysindex, lay;

	float mat[4][4], nmat[3][3]; /* nmat is inverse mat tranposed */
	short flag;

	float dupliorco[3], dupliuv[2];
	float (*duplitexmat)[4];
	
	struct VolumePrecache *volume_precache;
	
	float *vectors;
	int totvector;
	
	/* used on makeraytree */
	struct RayObject *raytree;
	int transform_primitives;

} ObjectInstanceRen;

/* ------------------------------------------------------------------------- */

typedef struct VertRen
{
	float co[3];
	float n[3];
	float *orco;
	short clip;
	unsigned short flag;		/* in use for clipping zbuffer parts, temp setting stuff in convertblender.c */
	float accum;		/* accum for radio weighting, and for strand texco static particles */
	int index;			/* index allows extending vertren with any property */
} VertRen;

/* ------------------------------------------------------------------------- */

struct halosort {
	struct HaloRen *har;
	int z;
};

/* ------------------------------------------------------------------------- */
struct Material;
struct MTFace;

typedef struct RadFace {
	float unshot[3], totrad[3];
	float norm[3], cent[3], area;
	int flag;
} RadFace;

typedef struct VlakRen {
	struct VertRen *v1, *v2, *v3, *v4;	/* keep in order for ** addressing */
	float n[3];
	struct Material *mat;
	char puno;
	char flag, ec;
	int index;
} VlakRen;

typedef struct HaloRen
{	
    short miny, maxy;
    float alfa, xs, ys, rad, radsq, sin, cos, co[3], no[3];
	float hard, b, g, r;
    int zs, zd;
    int zBufDist;	/* depth in the z-buffer coordinate system */
    char starpoints, type, add, tex;
    char linec, ringc, seed;
	short flarec; /* used to be a char. why ?*/
    float hasize;
    int pixels;
    unsigned int lay;
    struct Material *mat;
} HaloRen;

/* ------------------------------------------------------------------------- */

typedef struct StrandVert {
	float co[3];
	float strandco;
} StrandVert;

typedef struct StrandSurface {
	struct StrandSurface *next, *prev;
	ObjectRen obr;
	int (*face)[4];
	float (*co)[3];
	/* for occlusion caching */
	float (*ao)[3];
	float (*env)[3];
	float (*indirect)[3];
	/* for speedvectors */
	float (*prevco)[3], (*nextco)[3];
	int totvert, totface;
} StrandSurface;

typedef struct StrandBound {
	int start, end;
	float boundbox[2][3];
} StrandBound;

typedef struct StrandBuffer {
	struct StrandBuffer *next, *prev;
	struct StrandVert *vert;
	struct StrandBound *bound;
	int totvert, totbound;

	struct ObjectRen *obr;
	struct Material *ma;
	struct StrandSurface *surface;
	unsigned int lay;
	int overrideuv;
	int flag, maxdepth;
	float adaptcos, minwidth, widthfade;

	float winmat[4][4];
	int winx, winy;
} StrandBuffer;

typedef struct StrandRen {
	StrandVert *vert;
	StrandBuffer *buffer;
	int totvert, flag;
	int clip, index;
	float orco[3];
} StrandRen;

/* ------------------------------------------------------------------------- */

typedef struct VolumeOb
{
	struct VolumeOb *next, *prev;
	struct Material *ma;
	struct ObjectRen *obr;
} VolumeOb;

typedef struct MatInside {
	struct MatInside *next, *prev;
	struct Material *ma;
	struct ObjectInstanceRen *obi;
} MatInside;

typedef struct VolPrecachePart
{
	struct VolPrecachePart *next, *prev;
	struct RayObject *tree;
	struct ShadeInput *shi;
	struct ObjectInstanceRen *obi;
	int num;
	int minx, maxx;
	int miny, maxy;
	int minz, maxz;
	int res[3];
	float bbmin[3];
	float voxel[3];
	int working, done;
} VolPrecachePart;

typedef struct VolumePrecache
{
	int res[3];
	float *data_r;
	float *data_g;
	float *data_b;
} VolumePrecache;

/* ------------------------------------------------------------------------- */

struct LampRen;
struct MTex;

/**
 * For each lamp in a scene, a LampRen is created. It determines the
 * properties of a lightsource.
 */

typedef struct LampShadowSubSample {
	int samplenr;
	float shadfac[4];	/* rgba shadow */
} LampShadowSubSample;

typedef struct LampShadowSample {
	LampShadowSubSample s[16];	/* MAX OSA */
} LampShadowSample;

typedef struct LampRen {
	struct LampRen *next, *prev;
	
	float xs, ys, dist;
	float co[3];
	short type;
	int mode;
	float r, g, b, k;
	float shdwr, shdwg, shdwb;
	float energy, haint;
	int lay;
	float spotsi,spotbl;
	float vec[3];
	float xsp, ysp, distkw, inpr;
	float halokw, halo;
	
	short falloff_type;
	float ld1,ld2;
	struct CurveMapping *curfalloff;

	/* copied from Lamp, to decouple more rendering stuff */
	/** Size of the shadowbuffer */
	short bufsize;
	/** Number of samples for the shadows */
	short samp;
	/** Softness factor for shadow */
	float soft;
	/** amount of subsample buffers and type of filter for sampling */
	short buffers, filtertype;
	/** shadow buffer type (regular, irregular) */
	short buftype;
	/** autoclip */
	short bufflag;
	/** shadow plus halo: detail level */
	short shadhalostep;
	/** Near clip of the lamp */
	float clipsta;
	/** Far clip of the lamp */
	float clipend;
	/** A small depth offset to prevent self-shadowing. */
	float bias;
	/* Compression threshold for deep shadow maps */
	float compressthresh;
	
	short ray_samp, ray_sampy, ray_sampz, ray_samp_method, ray_samp_type, area_shape, ray_totsamp;
	short xold[BLENDER_MAX_THREADS], yold[BLENDER_MAX_THREADS];	/* last jitter table for area lights */
	float area_size, area_sizey, area_sizez;
	float adapt_thresh;

	/* sun/sky */
	struct SunSky *sunsky;
	
	struct ShadBuf *shb;
	float *jitter;
	
	float imat[3][3];
	float spottexfac;
	float sh_invcampos[3], sh_zfac;	/* sh_= spothalo */
	
	float mat[3][3];	/* 3x3 part from lampmat x viewmat */
	float area[8][3], areasize;
	
	/* passes & node shader support: all shadow info for a pixel */
	LampShadowSample *shadsamp;
		
	/* yafray: photonlight params */
	int YF_numphotons, YF_numsearch;
	short YF_phdepth, YF_useqmc, YF_bufsize;
	float YF_causticblur, YF_ltradius;
	float YF_glowint, YF_glowofs;
	short YF_glowtype;
	
	/* ray optim */
	struct RayObject *last_hit[BLENDER_MAX_THREADS];
	
	struct MTex *mtex[MAX_MTEX];

	/* threading */
	int thread_assigned;
	int thread_ready;
} LampRen;

/* **************** defines ********************* */

/* R.r.mode flag is same as for renderdata */

/* R.flag */
#define R_ZTRA			1
#define R_HALO			2
#define R_SEC_FIELD		4
#define R_LAMPHALO		8
#define R_GLOB_NOPUNOFLIP	16
#define R_NEED_TANGENT	32
#define R_BAKE_TRACE	128
#define R_BAKING		256

/* vlakren->flag (vlak = face in dutch) char!!! */
#define R_SMOOTH		1
#define R_HIDDEN		2
/* strand flag, means special handling */
#define R_STRAND		4
#define R_NOPUNOFLIP	8
#define R_FULL_OSA		16
#define R_FACE_SPLIT	32
/* Tells render to divide face other way. */
#define R_DIVIDE_24		64	
/* vertex normals are tangent or view-corrected vector, for hair strands */
#define R_TANGENT		128		

/* strandbuffer->flag */
#define R_STRAND_BSPLINE	1
#define R_STRAND_B_UNITS	2

/* objectren->flag */
#define R_INSTANCEABLE		1

/* objectinstance->flag */
#define R_DUPLI_TRANSFORMED	1
#define R_ENV_TRANSFORMED	2
#define R_TRANSFORMED		(1|2)
#define R_NEED_VECTORS		4

#endif /* RENDER_TYPES_H */

