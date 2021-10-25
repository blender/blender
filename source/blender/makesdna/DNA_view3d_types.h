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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_view3d_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_VIEW3D_TYPES_H__
#define __DNA_VIEW3D_TYPES_H__

struct ViewDepths;
struct Object;
struct Image;
struct SpaceLink;
struct BoundBox;
struct MovieClip;
struct MovieClipUser;
struct RenderEngine;
struct bGPdata;
struct SmoothView3DStore;
struct wmTimer;
struct Material;
struct GPUFX;

/* This is needed to not let VC choke on near and far... old
 * proprietary MS extensions... */
#ifdef WIN32
#undef near
#undef far
#define near clipsta
#define far clipend
#endif

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_image_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_gpu_types.h"

/* ******************************** */

/* The near/far thing is a Win EXCEPTION. Thus, leave near/far in the
 * code, and patch for windows. */
 
/* Background Picture in 3D-View */
typedef struct BGpic {
	struct BGpic *next, *prev;

	struct Image *ima;
	struct ImageUser iuser;
	struct MovieClip *clip;
	struct MovieClipUser cuser;
	float xof, yof, size, blend, rotation;
	short view;
	short flag;
	short source;
	char pad[6];
} BGpic;

/* ********************************* */

typedef struct RegionView3D {
	
	float winmat[4][4];			/* GL_PROJECTION matrix */
	float viewmat[4][4];		/* GL_MODELVIEW matrix */
	float viewinv[4][4];		/* inverse of viewmat */
	float persmat[4][4];		/* viewmat*winmat */
	float persinv[4][4];		/* inverse of persmat */
	float viewcamtexcofac[4];	/* offset/scale for camera glsl texcoords */

	/* viewmat/persmat multiplied with object matrix, while drawing and selection */
	float viewmatob[4][4];
	float persmatob[4][4];

	/* user defined clipping planes */
	float clip[6][4];
	float clip_local[6][4]; /* clip in object space, means we can test for clipping in editmode without first going into worldspace */
	struct BoundBox *clipbb;

	struct RegionView3D *localvd; /* allocated backup of its self while in localview */
	struct RenderEngine *render_engine;
	struct ViewDepths *depths;
	void *gpuoffscreen;

	/* animated smooth view */
	struct SmoothView3DStore *sms;
	struct wmTimer *smooth_timer;


	/* transform widget matrix */
	float twmat[4][4];

	float viewquat[4];			/* view rotation, must be kept normalized */
	float dist;					/* distance from 'ofs' along -viewinv[2] vector, where result is negative as is 'ofs' */
	float camdx, camdy;			/* camera view offsets, 1.0 = viewplane moves entire width/height */
	float pixsize;				/* runtime only */
	float ofs[3];				/* view center & orbit pivot, negative of worldspace location,
								 * also matches -viewinv[3][0:3] in ortho mode.*/
	float camzoom;				/* viewport zoom on the camera frame, see BKE_screen_view3d_zoom_to_fac */
	char is_persp;				/* check if persp/ortho view, since 'persp' cant be used for this since
								 * it can have cameras assigned as well. (only set in view3d_winmatrix_set) */
	char persp;
	char view;
	char viewlock;
	char viewlock_quad;			/* options for quadview (store while out of quad view) */
	char pad[3];
	float ofs_lock[2];			/* normalized offset for locked view: (-1, -1) bottom left, (1, 1) upper right */

	short twdrawflag;
	short rflag;


	/* last view (use when switching out of camera view) */
	float lviewquat[4];
	short lpersp, lview; /* lpersp can never be set to 'RV3D_CAMOB' */

	float gridview;
	float tw_idot[3];  /* manipulator runtime: (1 - dot) product with view vector (used to check view alignment) */


	/* active rotation from NDOF or elsewhere */
	float rot_angle;
	float rot_axis[3];

	struct GPUFX *compositor;
} RegionView3D;

/* 3D ViewPort Struct */
typedef struct View3D {
	struct SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;
	short blockhandler[8];

	float viewquat[4]  DNA_DEPRECATED;
	float dist         DNA_DEPRECATED;

	float bundle_size;			/* size of bundles in reconstructed data */
	char bundle_drawtype;		/* display style for bundle */
	char pad[3];

	unsigned int lay_prev; /* for active layer toggle */
	unsigned int lay_used; /* used while drawing */
	
	short persp  DNA_DEPRECATED;
	short view   DNA_DEPRECATED;
	
	struct Object *camera, *ob_centre;
	rctf render_border;

	struct ListBase bgpicbase;
	struct BGpic *bgpic  DNA_DEPRECATED; /* deprecated, use bgpicbase, only kept for do_versions(...) */

	struct View3D *localvd; /* allocated backup of its self while in localview */
	
	char ob_centre_bone[64];		/* optional string for armature bone to define center, MAXBONENAME */
	
	unsigned int lay;
	int layact;
	
	/**
	 * The drawing mode for the 3d display. Set to OB_BOUNDBOX, OB_WIRE, OB_SOLID,
	 * OB_TEXTURE, OB_MATERIAL or OB_RENDER */
	short drawtype;
	short ob_centre_cursor;		/* optional bool for 3d cursor to define center */
	short scenelock, around;
	short flag, flag2;
	
	float lens, grid;
	float near, far;
	float ofs[3]  DNA_DEPRECATED;			/* XXX deprecated */
	float cursor[3];

	short matcap_icon;			/* icon id */

	short gridlines;
	short gridsubdiv;	/* Number of subdivisions in the grid between each highlighted grid line */
	char gridflag;

	/* transform widget info */
	char twtype, twmode, twflag;
	
	short flag3;
	
	/* afterdraw, for xray & transparent */
	struct ListBase afterdraw_transp;
	struct ListBase afterdraw_xray;
	struct ListBase afterdraw_xraytransp;

	/* drawflags, denoting state */
	char zbuf, transp, xray;

	char multiview_eye;				/* multiview current eye - for internal use */

	/* built-in shader effects (eGPUFXFlags) */
	char pad3[4];

	/* note, 'fx_settings.dof' is currently _not_ allocated,
	 * instead set (temporarily) from camera */
	struct GPUFXSettings fx_settings;

	void *properties_storage;		/* Nkey panel stores stuff here (runtime only!) */
	/* Allocated per view, not library data (used by matcap). */
	struct Material *defmaterial;

	/* XXX deprecated? */
	struct bGPdata *gpd  DNA_DEPRECATED;		/* Grease-Pencil Data (annotation layers) */

	 /* multiview - stereo 3d */
	short stereo3d_flag;
	char stereo3d_camera;
	char pad4;
	float stereo3d_convergence_factor;
	float stereo3d_volume_alpha;
	float stereo3d_convergence_alpha;

	/* Previous viewport draw type.
	 * Runtime-only, set in the rendered viewport toggle operator.
	 */
	short prev_drawtype;
	short pad1;
	float pad2;
} View3D;


/* View3D->stereo_flag (short) */
#define V3D_S3D_DISPCAMERAS		(1 << 0)
#define V3D_S3D_DISPPLANE		(1 << 1)
#define V3D_S3D_DISPVOLUME		(1 << 2)

/* View3D->flag (short) */
/*#define V3D_DISPIMAGE		1*/ /*UNUSED*/
#define V3D_DISPBGPICS		2
#define V3D_HIDE_HELPLINES	4
#define V3D_INVALID_BACKBUF	8

#define V3D_ALIGN			1024
#define V3D_SELECT_OUTLINE	2048
#define V3D_ZBUF_SELECT		4096
#define V3D_GLOBAL_STATS	8192
#define V3D_DRAW_CENTERS	32768

/* RegionView3d->persp */
#define RV3D_ORTHO				0
#define RV3D_PERSP				1
#define RV3D_CAMOB				2

/* RegionView3d->rflag */
#define RV3D_CLIPPING				4
#define RV3D_NAVIGATING				8
#define RV3D_GPULIGHT_UPDATE		16
#define RV3D_IS_GAME_ENGINE			32  /* runtime flag, used to check if LoD's should be used */
/**
 * Disable zbuffer offset, skip calls to #ED_view3d_polygon_offset.
 * Use when precise surface depth is needed and picking bias isn't, see T45434).
 */
#define RV3D_ZOFFSET_DISABLED		64

/* RegionView3d->viewlock */
#define RV3D_LOCKED			(1 << 0)
#define RV3D_BOXVIEW		(1 << 1)
#define RV3D_BOXCLIP		(1 << 2)
/* RegionView3d->viewlock_quad */
#define RV3D_VIEWLOCK_INIT	(1 << 7)

/* RegionView3d->view */
#define RV3D_VIEW_USER			 0
#define RV3D_VIEW_FRONT			 1
#define RV3D_VIEW_BACK			 2
#define RV3D_VIEW_LEFT			 3
#define RV3D_VIEW_RIGHT			 4
#define RV3D_VIEW_TOP			 5
#define RV3D_VIEW_BOTTOM		 6
#define RV3D_VIEW_CAMERA		 8

#define RV3D_VIEW_IS_AXIS(view) \
	(((view) >= RV3D_VIEW_FRONT) && ((view) <= RV3D_VIEW_BOTTOM))

/* View3d->flag2 (short) */
#define V3D_RENDER_OVERRIDE		(1 << 2)
#define V3D_SOLID_TEX			(1 << 3)
#define V3D_SHOW_GPENCIL		(1 << 4)
#define V3D_LOCK_CAMERA			(1 << 5)
#define V3D_RENDER_SHADOW		(1 << 6)		/* This is a runtime only flag that's used to tell draw_mesh_object() that we're doing a shadow pass instead of a regular draw */
#define V3D_SHOW_RECONSTRUCTION	(1 << 7)
#define V3D_SHOW_CAMERAPATH		(1 << 8)
#define V3D_SHOW_BUNDLENAME		(1 << 9)
#define V3D_BACKFACE_CULLING	(1 << 10)
#define V3D_RENDER_BORDER		(1 << 11)
#define V3D_SOLID_MATCAP		(1 << 12)	/* user flag */
#define V3D_SHOW_SOLID_MATCAP	(1 << 13)	/* runtime flag */
#define V3D_OCCLUDE_WIRE		(1 << 14)
#define V3D_SHADELESS_TEX		(1 << 15)


/* View3d->flag3 (short) */
#define V3D_SHOW_WORLD			(1 << 0)

/* View3D->around */
enum {
	/* center of the bounding box */
	V3D_AROUND_CENTER_BOUNDS	= 0,
	/* center from the sum of all points divided by the total */
	V3D_AROUND_CENTER_MEAN		= 3,
	/* pivot around the 2D/3D cursor */
	V3D_AROUND_CURSOR			= 1,
	/* pivot around each items own origin */
	V3D_AROUND_LOCAL_ORIGINS	= 2,
	/* pivot around the active items origin */
	V3D_AROUND_ACTIVE			= 4,
};

/*View3D types (only used in tools, not actually saved)*/
#define V3D_VIEW_STEPLEFT		 1
#define V3D_VIEW_STEPRIGHT		 2
#define V3D_VIEW_STEPDOWN		 3
#define V3D_VIEW_STEPUP		 4
#define V3D_VIEW_PANLEFT		 5
#define V3D_VIEW_PANRIGHT		 6
#define V3D_VIEW_PANDOWN		 7
#define V3D_VIEW_PANUP			 8

/* View3d->gridflag */
#define V3D_SHOW_FLOOR			1
#define V3D_SHOW_X				2
#define V3D_SHOW_Y				4
#define V3D_SHOW_Z				8

/* View3d->twtype (bits, we can combine them) */
#define V3D_MANIP_TRANSLATE		1
#define V3D_MANIP_ROTATE		2
#define V3D_MANIP_SCALE			4

/* View3d->twmode */
#define V3D_MANIP_GLOBAL		0
#define V3D_MANIP_LOCAL			1
#define V3D_MANIP_NORMAL		2
#define V3D_MANIP_VIEW			3
#define V3D_MANIP_GIMBAL		4
#define V3D_MANIP_CUSTOM		5 /* anything of value 5 or higher is custom */

/* View3d->twflag */
   /* USE = user setting, DRAW = based on selection */
#define V3D_USE_MANIPULATOR		1
#define V3D_DRAW_MANIPULATOR	2
/* #define V3D_CALC_MANIPULATOR	4 */ /*UNUSED*/

/* BGPic->flag */
/* may want to use 1 for select ? */
enum {
	V3D_BGPIC_EXPANDED      = (1 << 1),
	V3D_BGPIC_CAMERACLIP    = (1 << 2),
	V3D_BGPIC_DISABLED      = (1 << 3),
	V3D_BGPIC_FOREGROUND    = (1 << 4),

	/* Camera framing options */
	V3D_BGPIC_CAMERA_ASPECT = (1 << 5),  /* don't stretch to fit the camera view  */
	V3D_BGPIC_CAMERA_CROP   = (1 << 6),  /* crop out the image */

	/* Axis flip options */
	V3D_BGPIC_FLIP_X        = (1 << 7),
	V3D_BGPIC_FLIP_Y        = (1 << 8),
};

#define V3D_BGPIC_EXPANDED (V3D_BGPIC_EXPANDED | V3D_BGPIC_CAMERACLIP)

/* BGPic->source */
/* may want to use 1 for select ?*/
#define V3D_BGPIC_IMAGE		0
#define V3D_BGPIC_MOVIE		1

#define RV3D_CAMZOOM_MIN -30
#define RV3D_CAMZOOM_MAX 600

/* #BKE_screen_view3d_zoom_to_fac() values above */
#define RV3D_CAMZOOM_MIN_FACTOR  0.1657359312880714853f
#define RV3D_CAMZOOM_MAX_FACTOR 44.9852813742385702928f

#endif

