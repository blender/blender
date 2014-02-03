/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
/** \file RE_shader_ext.h
 *  \ingroup render
 */


#ifndef __RE_SHADER_EXT_H__
#define __RE_SHADER_EXT_H__

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* this include is for shading and texture exports            */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* localized texture result data */
/* note; tr tg tb ta has to remain in this order */
typedef struct TexResult {
	float tin, tr, tg, tb, ta;
	int talpha;
	float *nor;
} TexResult;

/* localized shade result data */
typedef struct ShadeResult {
	float combined[4];
	float col[4];
	float alpha, mist, z;
	float emit[3];
	float diff[3];		/* no ramps, shadow, etc */
	float spec[3];
	float shad[4];		/* shad[3] is shadow intensity */
	float ao[3];
	float env[3];
	float indirect[3];
	float refl[3];
	float refr[3];
	float nor[3];
	float winspeed[4];
	float rayhits[4];
} ShadeResult;

/* only here for quick copy */
struct ShadeInputCopy {
	
	struct Material *mat;
	struct VlakRen *vlr;
	struct StrandRen *strand;
	struct ObjectInstanceRen *obi;
	struct ObjectRen *obr;
	int facenr;
	float facenor[3];				/* copy from face */
	short flippednor;				/* is facenor flipped? */
	struct VertRen *v1, *v2, *v3;	/* vertices can be in any order for quads... */
	short i1, i2, i3;				/* original vertex indices */
	short puno;
	short osatex;
	float vn[3], vno[3];			/* actual render normal, and a copy to restore it */
	float n1[3], n2[3], n3[3];		/* vertex normals, corrected */
	int mode;						/* base material mode (OR-ed result of entire node tree) */
};

typedef struct ShadeInputUV {
	float dxuv[3], dyuv[3], uv[3];
	char *name;
} ShadeInputUV;

typedef struct ShadeInputCol {
	float col[4];
	char *name;
} ShadeInputCol;

/* localized renderloop data */
typedef struct ShadeInput {
	/* copy from face, also to extract tria from quad */
	/* note it mirrors a struct above for quick copy */
	
	struct Material *mat;
	struct VlakRen *vlr;
	struct StrandRen *strand;
	struct ObjectInstanceRen *obi;
	struct ObjectRen *obr;
	int facenr;
	float facenor[3];				/* copy from face */
	short flippednor;				/* is facenor flipped? */
	struct VertRen *v1, *v2, *v3;	/* vertices can be in any order for quads... */
	short i1, i2, i3;				/* original vertex indices */
	short puno;
	short osatex;
	float vn[3], vno[3];			/* actual render normal, and a copy to restore it */
	float n1[3], n2[3], n3[3];		/* vertex normals, corrected */
	int mode;						/* base material mode (OR-ed result of entire node tree) */
	
	/* internal face coordinates */
	float u, v, dx_u, dx_v, dy_u, dy_v;
	float co[3], view[3], camera_co[3];
	
	/* copy from material, keep synced so we can do memcopy */
	/* current size: 23*4 */
	float r, g, b;
	float specr, specg, specb;
	float mirr, mirg, mirb;
	float ambr, ambb, ambg;
	
	float amb, emit, ang, spectra, ray_mirror;
	float alpha, refl, spec, zoffs, add;
	float translucency;
	/* end direct copy from material */
	
	/* individual copies: */
	int har; /* hardness */
	
	/* texture coordinates */
	float lo[3], gl[3], ref[3], orn[3], winco[3], vcol[4];
	float refcol[4], displace[3];
	float strandco, tang[3], nmapnorm[3], nmaptang[4], stress, winspeed[4];
	float duplilo[3], dupliuv[3];

	ShadeInputUV uv[8];   /* 8 = MAX_MTFACE */
	ShadeInputCol col[8]; /* 8 = MAX_MCOL */
	int totuv, totcol, actuv, actcol;
	
	/* dx/dy OSA coordinates */
	float dxco[3], dyco[3];
	float dxlo[3], dylo[3], dxgl[3], dygl[3];
	float dxref[3], dyref[3], dxorn[3], dyorn[3];
	float dxno[3], dyno[3], dxview, dyview;
	float dxlv[3], dylv[3];
	float dxwin[3], dywin[3];
	float dxrefract[3], dyrefract[3];
	float dxstrand, dystrand;
	
	/* AO is a pre-process now */
	float ao[3], indirect[3], env[3];
	
	int xs, ys;				/* pixel to be rendered */
	int mask;				/* subsample mask */
	float scanco[3];		/* original scanline coordinate without jitter */
	
	int samplenr;			/* sample counter, to detect if we should do shadow again */
	int depth;				/* 1 or larger on raytrace shading */
	int volume_depth;		/* number of intersections through volumes */
	
	/* for strand shading, normal at the surface */
	float surfnor[3], surfdist;

	/* from initialize, part or renderlayer */
	bool do_preview;		/* for nodes, in previewrender */
	bool do_manage;			/* color management flag */
	short thread, sample;	/* sample: ShadeSample array index */
	short nodes;			/* indicate node shading, temp hack to prevent recursion */
	
	unsigned int lay;
	int layflag, passflag, combinedflag;
	struct Group *light_override;
	struct Material *mat_override;
	
#ifdef RE_RAYCOUNTER
	RayCounter raycounter;
#endif
	
} ShadeInput;

typedef struct BakeImBufuserData {
	float *displacement_buffer;
	char *mask_buffer;
} BakeImBufuserData;

/* node shaders... */
struct Tex;
struct MTex;
struct ImBuf;
struct ImagePool;
struct Object;

/* this one uses nodes */
int	multitex_ext(struct Tex *tex, float texvec[3], float dxt[3], float dyt[3], int osatex, struct TexResult *texres, struct ImagePool *pool, bool scene_color_manage);
/* nodes disabled */
int multitex_ext_safe(struct Tex *tex, float texvec[3], struct TexResult *texres, struct ImagePool *pool, bool scene_color_manage);
/* only for internal node usage */
int multitex_nodes(struct Tex *tex, float texvec[3], float dxt[3], float dyt[3], int osatex, struct TexResult *texres,
                   const short thread, short which_output, struct ShadeInput *shi, struct MTex *mtex,
                   struct ImagePool *pool);
float RE_lamp_get_data(struct ShadeInput *shi, struct Object *lamp_obj, float col[4], float lv[3], float *dist, float shadow[4]);

/* shaded view and bake */
struct Render;
struct Image;

int RE_bake_shade_all_selected(struct Render *re, int type, struct Object *actob, short *do_update, float *progress);
struct Image *RE_bake_shade_get_image(void);
void RE_bake_ibuf_filter(struct ImBuf *ibuf, char *mask, const int filter);
void RE_bake_ibuf_normalize_displacement(struct ImBuf *ibuf, float *displacement, char *mask, float displacement_min, float displacement_max);
float RE_bake_make_derivative(struct ImBuf *ibuf, float *heights_buffer, const char *mask,
                              const float height_min, const float height_max,
                              const float fmult);

#define BAKE_RESULT_OK			0
#define BAKE_RESULT_NO_OBJECTS		1
#define BAKE_RESULT_FEEDBACK_LOOP	2

#endif /* __RE_SHADER_EXT_H__ */
