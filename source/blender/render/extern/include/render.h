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
 * Interface to transform the Blender scene into renderable data.
 */

#ifndef RENDER_H
#define RENDER_H "$Id$"

/* ------------------------------------------------------------------------- */
/* This little preamble might be moved to a separate include. It contains    */
/* some defines that should become functions, and some platform dependency   */
/* fixes. I think it is risky to always include it...                        */
/* ------------------------------------------------------------------------- */

/* fix for OSA and defmaterial extern */
#include "BKE_osa_types.h"
#include "DNA_material_types.h"

#ifdef __cplusplus
extern "C" { 
#endif


/* For #undefs of stupid windows defines */
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

/* ------------------------------------------------------------------------- */
/* Types                                                                     */
/* Both external and internal types can be placed here. Make sure there are  */
/* no dirty extras in the type files so they can be included without         */
/* problems. If possible, make a note why the include is needed.             */
/* ------------------------------------------------------------------------- */

#include "render_types.h"

/* ------------------------------------------------------------------------- */
/* Global variables                                                          */
/* These variable are global to the render module, and also externally       */
/* visible. The file where they are defined must be added.                   */
/* ------------------------------------------------------------------------- */

extern RE_Render         R;           /* rendercore.c */
extern Osa               O;           /* rendercore.c */
extern Material          defmaterial; /* initrender.c */
extern unsigned short   *igamtab1;    /* initrender.c */
extern unsigned short   *gamtab;      /* initrender.c */

struct View3D;

/* ------------------------------------------------------------------------- */
/* Function definitions                                                      */
/*                                                                           */
/* All functions that need to be externally visible must be declared here.   */
/* Currently, this interface contains 29 functions and 11 callbacks.         */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* Needed for the outside world referring to shadowbuffers                   */
/* ------------------------------------------------------------------------- */

#ifndef RE_SHADOWBUFFERHANDLE
#define RE_SHADOWBUFFERHANDLE
#define RE_DECLARE_HANDLE(name) typedef struct name##__ { int unused; } *name
	RE_DECLARE_HANDLE(RE_ShadowBufferHandle);
#endif
		
	/**
	 * Create a new, empty shadow buffer with certain settings.
	 *
	 * @param mode 0 is a dummy buffer, 1 is the old buffer for
	 * c-based shadowing, 2 is the old buffer with c++ refit , 2 is a
	 * deep buffer
	 */
	extern RE_ShadowBufferHandle RE_createShadowBuffer(struct LampRen *lar,
													   float mat[][4],
													   int mode);

	/**
	 * Delete a shadow buffer.
	 * @param shb handle to the buffer to be released
	 */
	extern void RE_deleteShadowBuffer(RE_ShadowBufferHandle shb);


	
/* ------------------------------------------------------------------------- */
/* initrender (14)                                                           */
/* ------------------------------------------------------------------------- */

struct View3D;

/**
 * Guarded call to frame renderer? Tests several limits and boundary
 * conditions. 
 * 
 * @param ogl_render_area The View3D area to use for OpenGL rendering
 * (can be NULL unless render R_OGL flag is set)
 */
void    RE_initrender(struct View3D *ogl_render_view3d);

/**
 *
 */
void    RE_setwindowclip(int mode, int jmode);

/*
 * @param ogl_render_area The View3D area to use for OpenGL rendering
 * (can be NULL unless render R_OGL flag is set)
 */
void    RE_animrender(struct View3D *ogl_render_view3d);
void    RE_free_render_data(void);
void    RE_free_filt_mask(void);
void    RE_holoview(void);
void    RE_init_filt_mask(void);
void    RE_init_render_data(void);
void    RE_jitterate1(float *jit1, float *jit2, int num, float rad1);
void    RE_jitterate2(float *jit1, float *jit2, int num, float rad2);
void    RE_make_existing_file(char *name);

/* ------------------------------------------------------------------------- */
/* zbuf (2)                                                                  */
/* ------------------------------------------------------------------------- */

/**
 * Converts a world coordinate into a homogenous coordinate in view
 * coordinates. (WCS -> HCS)
 * Also called in: shadbuf.c render.c radfactors.c
 *                 initrender.c envmap.c editmesh.c
 * @param v1  [3 floats] the world coordinate
 * @param adr [4 floats] the homogenous view coordinate
 */
void    RE_projectverto(float *v1,float *adr);

/**
 * Something about doing radiosity z buffering?
 * (called in radfactors.c), hope the RadView is defined already... 
 * Also called in: radfactors.c
 * Note: Uses globals.
 * @param radview radiosity view definition
 */
	struct RadView;
	struct RNode;
void    RE_zbufferall_radio(struct RadView *vw, struct RNode **rg_elem, int rg_totelem);


/* ------------------------------------------------------------------------- */
/* envmap (5)                                                                   */
/* ------------------------------------------------------------------------- */
	struct EnvMap;
	struct Tex;
void    RE_free_envmapdata(struct EnvMap *env);
void    RE_free_envmap(struct EnvMap *env);
struct EnvMap *RE_add_envmap(void);
/* these two maybe not external? yes, they are, for texture.c */
struct EnvMap *RE_copy_envmap(struct EnvMap *env);
/* (used in texture.c) */
int     RE_envmaptex(struct Tex *tex, float *texvec, float *dxt, float *dyt);

	/* --------------------------------------------------------------------- */
	/* rendercore (2)                                                        */
	/* --------------------------------------------------------------------- */
	float   RE_Spec(float inp, int hard);

	/* maybe not external */
	void    RE_calc_R_ref(void);

	/* --------------------------------------------------------------------- */
	/* renderdatabase (3)                                                    */
	/* --------------------------------------------------------------------- */
	struct VlakRen *RE_findOrAddVlak(int nr);
	struct VertRen *RE_findOrAddVert(int nr);
	struct HaloRen *RE_findOrAddHalo(int nr);
	HaloRen *RE_inithalo(Material *ma, 
					  float *vec, 
					  float *vec1, 
					  float *orco, 
					  float hasize, 
					  float vectsize);
  

	/**
	 * callbacks (11):
	 *
	 * If the callbacks aren't set, rendering will still proceed as
	 * desired, but the concerning functionality will not be enabled.
	 *
	 * There need to be better uncoupling between the renderer and
	 * these functions still!
	 * */
	
	void RE_set_test_break_callback(int (*f)(void));

	void RE_set_timecursor_callback(void (*f)(int));

	void RE_set_renderdisplay_callback(void (*f)(int, int, int, int, unsigned int *));
	void RE_set_initrenderdisplay_callback(void (*f)(void));
	void RE_set_clearrenderdisplay_callback(void (*f)(short));
	
	void RE_set_printrenderinfo_callback(void (*f)(double,int));

	void RE_set_getrenderdata_callback(void (*f)(void));
	void RE_set_freerenderdata_callback(void (*f)(void));
	

	/*from renderhelp, should disappear!!! */ 
	/** Recalculate all normals on renderdata. */
	void set_normalflags(void);
	/**
	 * On loan from zbuf.h:
	 * Tests whether the first three coordinates should be clipped
	 * wrt. the fourth component. Bits 1 and 2 test on x, 3 and 4 test on
	 * y, 5 and 6 test on z:
	 * xyz >  test => set first bit   (01),
	 * xyz < -test => set second bit  (10),
	 * xyz == test => reset both bits (00).
	 * Note: functionality is duplicated from an internal function
	 * Also called in: initrender.c, radfactors.c
	 * @param  v [4 floats] a coordinate 
	 * @return a vector of bitfields
	 */
	int RE_testclip(float *v); 

	/* patch for the external if, to support the split for the ui */
	void RE_addalphaAddfac(char *doel, char *bron, char addfac);
	void RE_sky(char *col); 
	void RE_renderflare(struct HaloRen *har); 
	/**
	 * Shade the pixel at xn, yn for halo har, and write the result to col. 
	 * Also called in: previewrender.c
	 * @param har    The halo to be rendered on this location
	 * @param col    [unsigned int 3] The destination colour vector 
	 * @param zz     Some kind of distance
	 * @param dist   Square of the distance of this coordinate to the halo's center
	 * @param x      [f] Pixel x relative to center
	 * @param y      [f] Pixel y relative to center
	 * @param flarec Flare counter? Always har->flarec...
	 */
	void RE_shadehalo(struct HaloRen *har,
				   char *col,
				   unsigned int zz,
				   float dist,
				   float x,
				   float y,
				   short flarec); 

/***/

/* haloren->type: flags */

#define HA_ONLYSKY		1
#define HA_VECT			2
#define HA_XALPHA		4
#define HA_FLARECIRC	8

#ifdef __cplusplus
}
#endif

#endif /* RENDER_H */

