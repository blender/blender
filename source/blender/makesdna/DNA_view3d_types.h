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
struct GPUViewport;

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_gpu_types.h"

/* ******************************** */

/* The near/far thing is a Win EXCEPTION, caused by indirect includes from <windows.h>.
 * Thus, leave near/far in the code, and undef for windows. */
#ifdef _WIN32
#  undef near
#  undef far
#endif

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


	/* transform gizmo matrix */
	float twmat[4][4];
	/* min/max dot product on twmat xyz axis. */
	float tw_axis_min[3], tw_axis_max[3];
	float tw_axis_matrix[3][3];

	float gridview;

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

	short twdrawflag; /* XXX can easily get rid of this (Julian) */
	short rflag;


	/* last view (use when switching out of camera view) */
	float lviewquat[4];
	short lpersp, lview; /* lpersp can never be set to 'RV3D_CAMOB' */

	/* active rotation from NDOF or elsewhere */
	float rot_angle;
	float rot_axis[3];
} RegionView3D;

typedef struct View3DCursor {
	float location[3];
	float rotation[4];
	char _pad[4];
} View3DCursor;

/* 3D Viewport Shading setings */
typedef struct View3DShading {
	short type;        /* Shading type (VIEW3D_SHADE_SOLID, ..) */
	short prev_type;   /* Runtime, for toggle between rendered viewport. */

	short flag;
	short color_type;

	short light;
	short pad[3];
	char studio_light[256]; /* FILE_MAXFILE */
	char matcap[256]; /* FILE_MAXFILE */

	float shadow_intensity;
	float single_color[3];

	float studiolight_rot_z;
	float studiolight_background;

	float object_outline_color[3];
	float xray_alpha;

	float cavity_valley_factor;
	float cavity_ridge_factor;
} View3DShading;

/* 3D Viewport Overlay setings */
typedef struct View3DOverlay {
	int flag;

	/* Edit mode settings */
	int edit_flag;
	float normals_length;
	float backwire_opacity;

	/* Paint mode settings */
	int paint_flag;

	/* Alpha for texture, weight, vertex paint overlay */
	float texture_paint_mode_opacity;
	float vertex_paint_mode_opacity;
	float weight_paint_mode_opacity;

	/* Armature edit/pose mode settings */
	int arm_flag;
	float bone_select_alpha;

	/* Other settings */
	float wireframe_threshold;
	char _pad0[4];

} View3DOverlay;

/* 3D ViewPort Struct */
typedef struct View3D {
	struct SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */

	float viewquat[4]  DNA_DEPRECATED;
	float dist         DNA_DEPRECATED;

	float bundle_size;			/* size of bundles in reconstructed data */
	char bundle_drawtype;		/* display style for bundle */
	char pad[3];

	unsigned int lay_prev; /* for active layer toggle */
	unsigned int lay_used; /* used while drawing */

	int object_type_exclude_viewport;
	int object_type_exclude_select;

	short persp  DNA_DEPRECATED;
	short view   DNA_DEPRECATED;

	struct Object *camera, *ob_centre;
	rctf render_border;

	struct View3D *localvd; /* allocated backup of its self while in localview */

	char ob_centre_bone[64];		/* optional string for armature bone to define center, MAXBONENAME */

	unsigned int lay;
	int layact;

	short ob_centre_cursor;		/* optional bool for 3d cursor to define center */
	short scenelock, _pad1;
	short flag, flag2, pad2;

	float lens, grid;
	float near, far;
	float ofs[3]  DNA_DEPRECATED;			/* XXX deprecated */

	View3DCursor cursor;

	char _pad[4];

	short matcap_icon;			/* icon id */

	short gridlines;
	short gridsubdiv;	/* Number of subdivisions in the grid between each highlighted grid line */
	char gridflag;

	/* transform gizmo info */
	char _pad5[2], gizmo_flag;

	short flag3;

	/* drawflags, denoting state */
	char _pad2;
	char transp, xray;

	char multiview_eye;				/* multiview current eye - for internal use */

	char pad3[4];

	/* note, 'fx_settings.dof' is currently _not_ allocated,
	 * instead set (temporarily) from camera */
	struct GPUFXSettings fx_settings;

	void *properties_storage;		/* Nkey panel stores stuff here (runtime only!) */

	/* XXX deprecated? */
	struct bGPdata *gpd  DNA_DEPRECATED;		/* Grease-Pencil Data (annotation layers) */

	/* Stereoscopy settings */
	short stereo3d_flag;
	char stereo3d_camera;
	char pad4;
	float stereo3d_convergence_factor;
	float stereo3d_volume_alpha;
	float stereo3d_convergence_alpha;

	/* Display settings */
	short drawtype DNA_DEPRECATED;
	short pad5[3];

	View3DShading shading;
	View3DOverlay overlay;
} View3D;


/* View3D->stereo_flag (short) */
#define V3D_S3D_DISPCAMERAS		(1 << 0)
#define V3D_S3D_DISPPLANE		(1 << 1)
#define V3D_S3D_DISPVOLUME		(1 << 2)

/* View3D->flag (short) */
/*#define V3D_FLAG_DEPRECATED_1 (1 << 0) */ /*UNUSED */
/*#define V3D_FLAG_DEPRECATED_2 (1 << 1) */ /* UNUSED */
#define V3D_HIDE_HELPLINES	4
#define V3D_INVALID_BACKBUF	8

/* #define V3D_FLAG_DEPRECATED_10 (1 << 10) */ /* UNUSED */
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
/*#define RV3D_IS_GAME_ENGINE			32 *//* UNUSED */
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
#define V3D_SHOW_MODE_SHADE_OVERRIDE (1 << 15) /* XXX: DNA deprecated */


/* View3d->flag3 (short) */
#define V3D_SHOW_WORLD			(1 << 0)

/* View3DShading->light */
enum {
	V3D_LIGHTING_FLAT   = 0,
	V3D_LIGHTING_STUDIO = 1,
	V3D_LIGHTING_MATCAP = 2,
};

/* View3DShading->flag */
enum {
	V3D_SHADING_OBJECT_OUTLINE      = (1 << 0),
	V3D_SHADING_XRAY                = (1 << 1),
	V3D_SHADING_SHADOW              = (1 << 2),
	V3D_SHADING_SCENE_LIGHTS        = (1 << 3),
	V3D_SHADING_SPECULAR_HIGHLIGHT  = (1 << 4),
	V3D_SHADING_CAVITY              = (1 << 5),
	V3D_SHADING_MATCAP_FLIP_X       = (1 << 6),
	V3D_SHADING_SCENE_WORLD         = (1 << 7),
};

/* View3DShading->color_type */
enum {
	V3D_SHADING_MATERIAL_COLOR = 0,
	V3D_SHADING_RANDOM_COLOR   = 1,
	V3D_SHADING_SINGLE_COLOR   = 2,
	V3D_SHADING_TEXTURE_COLOR  = 3,
};

/* View3DOverlay->flag */
enum {
	V3D_OVERLAY_FACE_ORIENTATION  = (1 << 0),
	V3D_OVERLAY_HIDE_CURSOR       = (1 << 1),
	V3D_OVERLAY_BONE_SELECT       = (1 << 2),
	V3D_OVERLAY_LOOK_DEV          = (1 << 3),
	V3D_OVERLAY_WIREFRAMES        = (1 << 4),
	V3D_OVERLAY_HIDE_TEXT         = (1 << 5),
	V3D_OVERLAY_HIDE_MOTION_PATHS = (1 << 6),
	V3D_OVERLAY_ONION_SKINS       = (1 << 7),
	V3D_OVERLAY_HIDE_BONES        = (1 << 8),
	V3D_OVERLAY_HIDE_OBJECT_XTRAS = (1 << 9),
	V3D_OVERLAY_HIDE_OBJECT_ORIGINS = (1 << 10),
};

/* View3DOverlay->edit_flag */
enum {
	V3D_OVERLAY_EDIT_VERT_NORMALS = (1 << 0),
	V3D_OVERLAY_EDIT_LOOP_NORMALS = (1 << 1),
	V3D_OVERLAY_EDIT_FACE_NORMALS = (1 << 2),

	V3D_OVERLAY_EDIT_OCCLUDE_WIRE = (1 << 3),

	V3D_OVERLAY_EDIT_WEIGHT       = (1 << 4),
};

/* View3DOverlay->arm_flag */
enum {
	V3D_OVERLAY_ARM_TRANSP_BONES  = (1 << 0),
};

/* View3DOverlay->paint_flag */
enum {
	V3D_OVERLAY_PAINT_WIRE        = (1 << 0),
};

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

/* Scene.orientation_type */
#define V3D_MANIP_GLOBAL		0
#define V3D_MANIP_LOCAL			1
#define V3D_MANIP_NORMAL		2
#define V3D_MANIP_VIEW			3
#define V3D_MANIP_GIMBAL		4
#define V3D_MANIP_CURSOR		5
#define V3D_MANIP_CUSTOM		1024

/* View3d.mpr_flag (also) */
enum {
	/** All gizmos. */
	V3D_GIZMO_HIDE                = (1 << 0),
	V3D_GIZMO_HIDE_NAVIGATE       = (1 << 1),
	V3D_GIZMO_HIDE_CONTEXT        = (1 << 2),
	V3D_GIZMO_HIDE_TOOL           = (1 << 3),
};

#define RV3D_CAMZOOM_MIN -30
#define RV3D_CAMZOOM_MAX 600

/* #BKE_screen_view3d_zoom_to_fac() values above */
#define RV3D_CAMZOOM_MIN_FACTOR  0.1657359312880714853f
#define RV3D_CAMZOOM_MAX_FACTOR 44.9852813742385702928f

#endif
