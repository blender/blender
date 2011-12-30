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

/** \file DNA_scene_types.h
 *  \ingroup DNA
 */

#ifndef DNA_SCENE_TYPES_H
#define DNA_SCENE_TYPES_H

#include "DNA_defs.h"

// XXX, temp feature - campbell
#define DURIAN_CAMERA_SWITCH

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_vec_types.h"
#include "DNA_listBase.h"
#include "DNA_ID.h"

struct Object;
struct Brush;
struct World;
struct Scene;
struct Image;
struct Group;
struct Text;
struct bNodeTree;
struct AnimData;
struct Editing;
struct SceneStats;
struct bGPdata;
struct MovieClip;

typedef struct Base {
	struct Base *next, *prev;
	unsigned int lay, selcol;
	int flag;
	short sx, sy;
	struct Object *object;
} Base;

typedef struct AviCodecData {
	void			*lpFormat;  /* save format */
	void			*lpParms;   /* compressor options */
	unsigned int	cbFormat;	    /* size of lpFormat buffer */
	unsigned int	cbParms;	    /* size of lpParms buffer */

	unsigned int	fccType;            /* stream type, for consistency */
	unsigned int	fccHandler;         /* compressor */
	unsigned int	dwKeyFrameEvery;    /* keyframe rate */
	unsigned int	dwQuality;          /* compress quality 0-10,000 */
	unsigned int	dwBytesPerSecond;   /* bytes per second */
	unsigned int	dwFlags;            /* flags... see below */
	unsigned int	dwInterleaveEvery;  /* for non-video streams only */
	unsigned int	pad;

	char			avicodecname[128];
} AviCodecData;

typedef struct QuicktimeCodecData {
	/*Old quicktime implementation compatibility fields, read only in 2.5 - deprecated*/
	void			*cdParms;   /* codec/compressor options */
	void			*pad;	    /* padding */

	unsigned int	cdSize;		    /* size of cdParms buffer */
	unsigned int	pad2;		    /* padding */

	char			qtcodecname[128];
} QuicktimeCodecData;
	
typedef struct QuicktimeCodecSettings {
	/* Codec settings detailed for 2.5 implementation*/
	int codecType; /* Types defined in quicktime_export.h */
	int	codecSpatialQuality; /* in 0-100 scale, to be translated in 0-1024 for qt use */

	/* Settings not available in current QTKit API */
	int	codec;
	int	codecFlags;
	int	colorDepth;
	int	codecTemporalQuality; /* in 0-100 scale, to be translated in 0-1024 for qt use */
	int	minSpatialQuality; /* in 0-100 scale, to be translated in 0-1024 for qt use */
	int	minTemporalQuality; /* in 0-100 scale, to be translated in 0-1024 for qt use */
	int	keyFrameRate;
	int	bitRate;	/* bitrate in bps */
	
	/* Audio Codec settings */
	int audiocodecType;
	int audioSampleRate;
	short audioBitDepth;
	short audioChannels;
	int audioCodecFlags;
	int audioBitRate;
	int pad1;
} QuicktimeCodecSettings;

typedef struct FFMpegCodecData {
	int type;
	int codec;
	int audio_codec;
	int video_bitrate;
	int audio_bitrate;
	int audio_mixrate;
	int audio_channels;
	int audio_pad;
	float audio_volume;
	int gop_size;
	int flags;

	int rc_min_rate;
	int rc_max_rate;
	int rc_buffer_size;
	int mux_packet_size;
	int mux_rate;
	IDProperty *properties;
} FFMpegCodecData;


typedef struct AudioData {
	int mixrate; // 2.5: now in FFMpegCodecData: audio_mixrate
	float main; // 2.5: now in FFMpegCodecData: audio_volume
	float speed_of_sound;
	float doppler_factor;
	int distance_model;
	short flag;
	short pad;
	float volume;
	float pad2;
} AudioData;

typedef struct SceneRenderLayer {
	struct SceneRenderLayer *next, *prev;
	
	char name[32];
	
	struct Material *mat_override;
	struct Group *light_override;
	
	unsigned int lay;		/* scene->lay itself has priority over this */
	unsigned int lay_zmask;	/* has to be after lay, this is for Z-masking */
	int layflag;
	
	int pad;
	
	int passflag;			/* pass_xor has to be after passflag */
	int pass_xor;
} SceneRenderLayer;

/* srl->layflag */
#define SCE_LAY_SOLID	1
#define SCE_LAY_ZTRA	2
#define SCE_LAY_HALO	4
#define SCE_LAY_EDGE	8
#define SCE_LAY_SKY		16
#define SCE_LAY_STRAND	32
	/* flags between 32 and 0x8000 are set to 1 already, for future options */

#define SCE_LAY_ALL_Z		0x8000
#define SCE_LAY_XOR			0x10000
#define SCE_LAY_DISABLE		0x20000
#define SCE_LAY_ZMASK		0x40000
#define SCE_LAY_NEG_ZMASK	0x80000

/* srl->passflag */
#define SCE_PASS_COMBINED		(1<<0)
#define SCE_PASS_Z				(1<<1)
#define SCE_PASS_RGBA			(1<<2)
#define SCE_PASS_DIFFUSE		(1<<3)
#define SCE_PASS_SPEC			(1<<4)
#define SCE_PASS_SHADOW			(1<<5)
#define SCE_PASS_AO				(1<<6)
#define SCE_PASS_REFLECT		(1<<7)
#define SCE_PASS_NORMAL			(1<<8)
#define SCE_PASS_VECTOR			(1<<9)
#define SCE_PASS_REFRACT		(1<<10)
#define SCE_PASS_INDEXOB		(1<<11)
#define SCE_PASS_UV				(1<<12)
#define SCE_PASS_INDIRECT		(1<<13)
#define SCE_PASS_MIST			(1<<14)
#define SCE_PASS_RAYHITS		(1<<15)
#define SCE_PASS_EMIT			(1<<16)
#define SCE_PASS_ENVIRONMENT	(1<<17)
#define SCE_PASS_INDEXMA	(1<<18)

/* note, srl->passflag is treestore element 'nr' in outliner, short still... */


/* Generic image format settings,
 * this is used for NodeImageFile and IMAGE_OT_save_as operator too.
 *
 * note: its a bit strange that even though this is an image format struct
 *  the imtype can still be used to select video formats.
 *  RNA ensures these enum's are only selectable for render output.
 */
typedef struct ImageFormatData {
	char imtype;   /* R_IMF_IMTYPE_PNG, R_... */
	               /* note, video types should only ever be set from this
	                * structure when used from RenderData */
	char depth;    /* bits per channel, R_IMF_CHAN_DEPTH_8 -> 32,
	                * not a flag, only set 1 at a time */

	char planes  ; /* - R_IMF_PLANES_BW, R_IMF_PLANES_RGB, R_IMF_PLANES_RGBA */
	char flag;     /* generic options for all image types, alpha zbuffer */

	char quality;  /* (0 - 100), eg: jpeg quality */
	char compress; /* (0 - 100), eg: png compression */


	/* --- format specific --- */

	/* OpenEXR */
	char  exr_codec;

	/* Cineon */
	char  cineon_flag;
	short cineon_white, cineon_black;
	float cineon_gamma;

	/* Jpeg2000 */
	char  jp2_flag;

	char pad[7];

} ImageFormatData;


/* ImageFormatData.imtype */
#define R_IMF_IMTYPE_TARGA           0
#define R_IMF_IMTYPE_IRIS            1
/* #define R_HAMX                    2 */ /* hamx is nomore */
/* #define R_FTYPE                   3 */ /* ftype is nomore */
#define R_IMF_IMTYPE_JPEG90          4
/* #define R_MOVIE                   5 */ /* movie is nomore */
#define R_IMF_IMTYPE_IRIZ            7
#define R_IMF_IMTYPE_RAWTGA         14
#define R_IMF_IMTYPE_AVIRAW         15
#define R_IMF_IMTYPE_AVIJPEG        16
#define R_IMF_IMTYPE_PNG            17
#define R_IMF_IMTYPE_AVICODEC       18
#define R_IMF_IMTYPE_QUICKTIME      19
#define R_IMF_IMTYPE_BMP            20
#define R_IMF_IMTYPE_RADHDR         21
#define R_IMF_IMTYPE_TIFF           22
#define R_IMF_IMTYPE_OPENEXR        23
#define R_IMF_IMTYPE_FFMPEG         24
#define R_IMF_IMTYPE_FRAMESERVER    25
#define R_IMF_IMTYPE_CINEON         26
#define R_IMF_IMTYPE_DPX            27
#define R_IMF_IMTYPE_MULTILAYER     28
#define R_IMF_IMTYPE_DDS            29
#define R_IMF_IMTYPE_JP2            30
#define R_IMF_IMTYPE_H264           31
#define R_IMF_IMTYPE_XVID           32
#define R_IMF_IMTYPE_THEORA         33

#define R_IMF_IMTYPE_INVALID        255

/* ImageFormatData.flag */
#define R_IMF_FLAG_ZBUF         (1<<0)   /* was R_OPENEXR_ZBUF */
#define R_IMF_FLAG_PREVIEW_JPG  (1<<1)   /* was R_PREVIEW_JPG */

/* return values from BKE_imtype_valid_depths, note this is depts per channel */
#define R_IMF_CHAN_DEPTH_1  (1<<0) /* 1bits  (unused) */
#define R_IMF_CHAN_DEPTH_8  (1<<1) /* 8bits  (default) */
#define R_IMF_CHAN_DEPTH_12 (1<<2) /* 12bits (uncommon, jp2 supports) */
#define R_IMF_CHAN_DEPTH_16 (1<<3) /* 16bits (tiff, halff float exr) */
#define R_IMF_CHAN_DEPTH_24 (1<<4) /* 24bits (unused) */
#define R_IMF_CHAN_DEPTH_32 (1<<5) /* 32bits (full float exr) */

/* ImageFormatData.planes */
#define R_IMF_PLANES_RGB   24
#define R_IMF_PLANES_RGBA  32
#define R_IMF_PLANES_BW    8

/* ImageFormatData.exr_codec */
#define R_IMF_EXR_CODEC_NONE  0
#define R_IMF_EXR_CODEC_PXR24 1
#define R_IMF_EXR_CODEC_ZIP   2
#define R_IMF_EXR_CODEC_PIZ   3
#define R_IMF_EXR_CODEC_RLE   4

/* ImageFormatData.jp2_flag */
#define R_IMF_JP2_FLAG_YCC          (1<<0)  /* when disabled use RGB */ /* was R_JPEG2K_YCC */
#define R_IMF_JP2_FLAG_CINE_PRESET  (1<<1)  /* was R_JPEG2K_CINE_PRESET */
#define R_IMF_JP2_FLAG_CINE_48      (1<<2)  /* was R_JPEG2K_CINE_48FPS */

/* ImageFormatData.cineon_flag */
#define R_IMF_CINEON_FLAG_LOG (1<<0)  /* was R_CINEON_LOG */

typedef struct RenderData {
	struct ImageFormatData im_format;
	
	struct AviCodecData *avicodecdata;
	struct QuicktimeCodecData *qtcodecdata;
	struct QuicktimeCodecSettings qtcodecsettings;
	struct FFMpegCodecData ffcodecdata;

	int cfra, sfra, efra;	/* frames as in 'images' */
	float subframe;			/* subframe offset from cfra, in 0.0-1.0 */
	int psfra, pefra;		/* start+end frames of preview range */

	int images, framapto;
	short flag, threads;

	float framelen, blurfac;

	/** For UR edge rendering: give the edges this color */
	float edgeR, edgeG, edgeB;


	/* standalone player */  //  XXX deprecated since 2.5
	short fullscreen  DNA_DEPRECATED, xplay  DNA_DEPRECATED, yplay  DNA_DEPRECATED;
	short freqplay  DNA_DEPRECATED;
	/* standalone player */  //  XXX deprecated since 2.5
	short depth  DNA_DEPRECATED, attrib  DNA_DEPRECATED;


	int frame_step;		/* frames to jump during render/playback */

	short stereomode  DNA_DEPRECATED;	/* standalone player stereo settings */  //  XXX deprecated since 2.5
	
	short dimensionspreset;		/* for the dimensions presets menu */

	short filtertype;	/* filter is box, tent, gauss, mitch, etc */

	short size, maximsize;	/* size in %, max in Kb */
	/* from buttons: */
	/**
	 * The desired number of pixels in the x direction
	 */
	short xsch;
	/**
	 * The desired number of pixels in the y direction
	 */
	short ysch;
	/**
	 * The number of part to use in the x direction
	 */
	short xparts;
	/**
	 * The number of part to use in the y direction
	 */
	short yparts;

	short planes  DNA_DEPRECATED, imtype  DNA_DEPRECATED, subimtype  DNA_DEPRECATED, quality  DNA_DEPRECATED; /*deprecated!*/
	
	/**
	 * Render to image editor, fullscreen or to new window.
	 */
	short displaymode;

	/**
	 * Flags for render settings. Use bit-masking to access the settings.
	 */
	int scemode;

	/**
	 * Flags for render settings. Use bit-masking to access the settings.
	 */
	int mode;

	/**
	 * Flags for raytrace settings. Use bit-masking to access the settings.
	 */
	int raytrace_options;
	
	/**
	 * Raytrace acceleration structure
	 */
	short raytrace_structure;

	short pad1;

	/* octree resolution */
	short ocres;
	short pad4;
	
	/**
	 * What to do with the sky/background. Picks sky/premul/key
	 * blending for the background
	 */
	short alphamode;

	/**
	 * The number of samples to use per pixel.
	 */
	short osa;

	short frs_sec, edgeint;

	
	/* safety, border and display rect */
	rctf safety, border;
	rcti disprect;
	
	/* information on different layers to be rendered */
	ListBase layers;
	short actlay;
	
	/* number of mblur samples */
	short mblur_samples;
	
	/**
	 * Adjustment factors for the aspect ratio in the x direction, was a short in 2.45
	 */
	float xasp, yasp;

	float frs_sec_base;
	
	/**
	 * Value used to define filter size for all filter options  */
	float gauss;
	
	
	/* color management settings - color profiles, gamma correction, etc */
	int color_mgt_flag;
	
	/** post-production settings. deprecated, but here for upwards compat (initialized to 1) */
	float postgamma, posthue, postsat;	 
	
	 /* Dither noise intensity */
	float dither_intensity;
	
	/* Bake Render options */
	short bake_osa, bake_filter, bake_mode, bake_flag;
	short bake_normal_space, bake_quad_split;
	float bake_maxdist, bake_biasdist, bake_pad;

	/* path to render output */
	char pic[240];

	/* stamps flags. */
	int stamp;
	short stamp_font_id, pad3; /* select one of blenders bitmap fonts */

	/* stamp info user data. */
	char stamp_udata[160];

	/* foreground/background color. */
	float fg_stamp[4];
	float bg_stamp[4];

	/* sequencer options */
	char seq_prev_type;
	char seq_rend_type;
	char seq_flag; /* flag use for sequence render/draw */
	char pad5[5];

	/* render simplify */
	int simplify_flag;
	short simplify_subsurf;
	short simplify_shadowsamples;
	float simplify_particles;
	float simplify_aosss;

	/* cineon */
	short cineonwhite  DNA_DEPRECATED, cineonblack  DNA_DEPRECATED;  /*deprecated*/
	float cineongamma  DNA_DEPRECATED;  /*deprecated*/
	
	/* jpeg2000 */
	short jp2_preset  DNA_DEPRECATED, jp2_depth  DNA_DEPRECATED;  /*deprecated*/
	int rpad3;

	/* Dome variables */ //  XXX deprecated since 2.5
	short domeres  DNA_DEPRECATED, domemode  DNA_DEPRECATED;	//  XXX deprecated since 2.5
	short domeangle  DNA_DEPRECATED, dometilt  DNA_DEPRECATED;	//  XXX deprecated since 2.5
	float domeresbuf  DNA_DEPRECATED;	//  XXX deprecated since 2.5
	float pad2;
	struct Text *dometext  DNA_DEPRECATED;	//  XXX deprecated since 2.5

	/* render engine */
	char engine[32];
} RenderData;

/* control render convert and shading engine */
typedef struct RenderProfile {
	struct RenderProfile *next, *prev;
	char name[32];
	
	short particle_perc;
	short subsurf_max;
	short shadbufsample_max;
	short pad1;
	
	float ao_error, pad2;
	
} RenderProfile;

typedef struct GameDome {
	short res, mode;
	short angle, tilt;
	float resbuf, pad2;
	struct Text *warptext;
} GameDome;

#define DOME_FISHEYE			1
#define DOME_TRUNCATED_FRONT	2
#define DOME_TRUNCATED_REAR		3
#define DOME_ENVMAP				4
#define DOME_PANORAM_SPH		5
#define DOME_NUM_MODES			6

typedef struct GameFraming {
	float col[3];
	char type, pad1, pad2, pad3;
} GameFraming;

#define SCE_GAMEFRAMING_BARS   0
#define SCE_GAMEFRAMING_EXTEND 1
#define SCE_GAMEFRAMING_SCALE  2

typedef struct RecastData {
	float cellsize;
	float cellheight;
	float agentmaxslope;
	float agentmaxclimb;
	float agentheight;
	float agentradius;
	float edgemaxlen;
	float edgemaxerror;
	float regionminsize;
	float regionmergesize;
	int vertsperpoly;
	float detailsampledist;
	float detailsamplemaxerror;
	short pad1, pad2;
} RecastData;

typedef struct GameData {

	/*  standalone player */
	struct GameFraming framing;
	short fullscreen, xplay, yplay, freqplay;
	short depth, attrib, rt1, rt2;

	/* stereo/dome mode */
	struct GameDome dome;
	short stereoflag, stereomode;
	float eyeseparation;
	RecastData recastData;


	/* physics (it was in world)*/
	float gravity; /*Gravitation constant for the game world*/

	/*
	 * Radius of the activity bubble, in Manhattan length. Objects
	 * outside the box are activity-culled. */
	float activityBoxRadius;

	/*
	 * bit 3: (gameengine): Activity culling is enabled.
	 * bit 5: (gameengine) : enable Bullet DBVT tree for view frustrum culling
	*/
	int flag;
	short mode, matmode;
	short occlusionRes;		/* resolution of occlusion Z buffer in pixel */
	short physicsEngine;
	short exitkey, pad;
	short ticrate, maxlogicstep, physubstep, maxphystep;
	short obstacleSimulation, pad1;
	float levelHeight;
} GameData;

#define STEREO_NOSTEREO		1
#define STEREO_ENABLED		2
#define STEREO_DOME			3

//#define STEREO_NOSTEREO		 1
#define STEREO_QUADBUFFERED 2
#define STEREO_ABOVEBELOW	 3
#define STEREO_INTERLACED	 4
#define STEREO_ANAGLYPH		5
#define STEREO_SIDEBYSIDE	6
#define STEREO_VINTERLACE	7
//#define STEREO_DOME		8

/* physicsEngine */
#define WOPHY_NONE		0
#define WOPHY_ENJI		1
#define WOPHY_SUMO		2
#define WOPHY_DYNAMO	3
#define WOPHY_ODE		4
#define WOPHY_BULLET	5

/* obstacleSimulation */
#define OBSTSIMULATION_NONE		0
#define OBSTSIMULATION_TOI_rays		1
#define OBSTSIMULATION_TOI_cells	2

/* GameData.flag */
#define GAME_RESTRICT_ANIM_UPDATES			(1 << 0)
#define GAME_ENABLE_ALL_FRAMES				(1 << 1)
#define GAME_SHOW_DEBUG_PROPS				(1 << 2)
#define GAME_SHOW_FRAMERATE					(1 << 3)
#define GAME_SHOW_PHYSICS					(1 << 4)
#define GAME_DISPLAY_LISTS					(1 << 5)
#define GAME_GLSL_NO_LIGHTS					(1 << 6)
#define GAME_GLSL_NO_SHADERS				(1 << 7)
#define GAME_GLSL_NO_SHADOWS				(1 << 8)
#define GAME_GLSL_NO_RAMPS					(1 << 9)
#define GAME_GLSL_NO_NODES					(1 << 10)
#define GAME_GLSL_NO_EXTRA_TEX				(1 << 11)
#define GAME_IGNORE_DEPRECATION_WARNINGS	(1 << 12)
#define GAME_ENABLE_ANIMATION_RECORD		(1 << 13)
#define GAME_SHOW_MOUSE						(1 << 14)
#define GAME_GLSL_NO_COLOR_MANAGEMENT		(1 << 15)
#define GAME_SHOW_OBSTACLE_SIMULATION		(1 << 16)
/* Note: GameData.flag is now an int (max 32 flags). A short could only take 16 flags */

/* GameData.matmode */
#define GAME_MAT_TEXFACE	0
#define GAME_MAT_MULTITEX	1
#define GAME_MAT_GLSL		2

typedef struct TimeMarker {
	struct TimeMarker *next, *prev;
	int frame;
	char name[64];
	unsigned int flag;
	struct Object *camera;
} TimeMarker;

typedef struct Paint {
	struct Brush *brush;
	
	/* WM Paint cursor */
	void *paint_cursor;
	unsigned char paint_cursor_col[4];

	int flags;
} Paint;

typedef struct ImagePaintSettings {
	Paint paint;

	short flag, pad;
	
	/* for projection painting only */
	short seam_bleed, normal_angle;
	short screen_grab_size[2]; /* capture size for re-projection */

	int pad1;

	void *paintcursor;			/* wm handle */
} ImagePaintSettings;

typedef struct ParticleBrushData {
	short size;						/* common setting */
	short step, invert, count;		/* for specific brushes only */
	int flag;
	float strength;
} ParticleBrushData;

typedef struct ParticleEditSettings {
	short flag;
	short totrekey;
	short totaddkey;
	short brushtype;

	ParticleBrushData brush[7]; /* 7 = PE_TOT_BRUSH */
	void *paintcursor;			/* runtime */

	float emitterdist, rt;

	int selectmode;
	int edittype;

	int draw_step, fade_frames;

	struct Scene *scene;
	struct Object *object;
} ParticleEditSettings;

typedef struct TransformOrientation {
	struct TransformOrientation *next, *prev;
	char name[36];
	float mat[3][3];
} TransformOrientation;

typedef struct Sculpt {
	Paint paint;

	/* For rotating around a pivot point */
	//float pivot[3]; XXX not used?
	int flags;

	/* Control tablet input */
	//char tablet_size, tablet_strength; XXX not used?
	int radial_symm[3];

	// all this below is used to communicate with the cursor drawing routine

	/* record movement of mouse so that rake can start at an intuitive angle */
	float last_x, last_y;
	float last_angle;

	int draw_anchored;
	int   anchored_size;
	float anchored_location[3];
	float anchored_initial_mouse[2];

	int draw_pressure;
	float pressure_value;

	float special_rotation;

	int pad;
} Sculpt;

typedef struct VPaint {
	Paint paint;

	short flag, pad;
	int tot;							/* allocation size of prev buffers */
	unsigned int *vpaint_prev;			/* previous mesh colors */
	struct MDeformVert *wpaint_prev;	/* previous vertex weights */
	
	void *paintcursor;					/* wm handle */
} VPaint;

/* VPaint flag */
#define VP_COLINDEX	1
#define VP_AREA		2

#define VP_NORMALS	8
#define VP_SPRAY	16
// #define VP_MIRROR_X	32 // deprecated in 2.5x use (me->editflag & ME_EDIT_MIRROR_X)
#define VP_ONLYVGROUP	128


typedef struct ToolSettings {
	VPaint *vpaint;		/* vertex paint */
	VPaint *wpaint;		/* weight paint */
	Sculpt *sculpt;
	
	/* Vertex groups */
	float vgroup_weight;

	/* Subdivide Settings */
	short cornertype;
	short editbutflag;
	/*Triangle to Quad conversion threshold*/
	float jointrilimit;
	/* Editmode Tools */
	float degr; 
	short step;
	short turn; 
	
	float extr_offs; 	/* extrude offset */
	float doublimit;	/* remove doubles limit */
	float normalsize;	/* size of normals */
	short automerge;

	/* Selection Mode for Mesh */
	short selectmode;

	/* Primitive Settings */
	/* UV Sphere */
	short segments;
	short rings;
	
	/* Cylinder - Tube - Circle */
	short vertices;

	/* UV Calculation */
	short unwrapper;
	float uvcalc_radius;
	float uvcalc_cubesize;
	float uvcalc_margin;
	short uvcalc_mapdir;
	short uvcalc_mapalign;
	short uvcalc_flag;
	short uv_flag, uv_selectmode;
	short uv_pad;
	
	/* Grease Pencil */
	short gpencil_flags;
	
	/* Auto-IK */
	short autoik_chainlen;

	/* Image Paint (8 byttse aligned please!) */
	struct ImagePaintSettings imapaint;

	/* Particle Editing */
	struct ParticleEditSettings particle;
	
	/* Transform Proportional Area of Effect */
	float proportional_size;

	/* Select Group Threshold */
	float select_thresh;
	
	/* Graph Editor */
	float clean_thresh;

	/* Auto-Keying Mode */
	short autokey_mode, autokey_flag;	/* defines in DNA_userdef_types.h */
	
	/* Retopo */
	char retopo_mode;
	char retopo_paint_tool;
	char line_div, ellipse_div, retopo_hotspot;

	/* Multires */
	char multires_subdiv_type;
	
	/* Skeleton generation */
	short skgen_resolution;
	float skgen_threshold_internal;
	float skgen_threshold_external;
	float skgen_length_ratio;
	float skgen_length_limit;
	float skgen_angle_limit;
	float skgen_correlation_limit;
	float skgen_symmetry_limit;
	float skgen_retarget_angle_weight;
	float skgen_retarget_length_weight;
	float skgen_retarget_distance_weight;
	short skgen_options;
	char  skgen_postpro;
	char  skgen_postpro_passes;
	char  skgen_subdivisions[3];
	char  skgen_multi_level;
	
	/* Skeleton Sketching */
	struct Object *skgen_template;
	char bone_sketching;
	char bone_sketching_convert;
	char skgen_subdivision_number;
	char skgen_retarget_options;
	char skgen_retarget_roll;
	char skgen_side_string[8];
	char skgen_num_string[8];
	
	/* Alt+RMB option */
	char edge_mode;
	char edge_mode_live_unwrap;

	/* Transform */
	char snap_mode;
	short snap_flag, snap_target;
	short proportional, prop_mode;
	char proportional_objects; /* proportional edit, object mode */
	char pad[5];

	char auto_normalize; /*auto normalizing mode in wpaint*/
	char multipaint; /* paint multiple bones in wpaint */

	short sculpt_paint_settings; /* user preferences for sculpt and paint */
	short pad1;
	int sculpt_paint_unified_size; /* unified radius of brush in pixels */
	float sculpt_paint_unified_unprojected_radius;/* unified radius of brush in Blender units */
	float sculpt_paint_unified_alpha; /* unified strength of brush */
} ToolSettings;

typedef struct bStats {
	/* scene totals for visible layers */
	int totobj, totlamp, totobjsel, totcurve, totmesh, totarmature;
	int totvert, totface;
} bStats;

typedef struct UnitSettings {
	/* Display/Editing unit options for each scene */
	float scale_length; /* maybe have other unit conversions? */
	char system; /* imperial, metric etc */
	char system_rotation; /* not implimented as a propper unit system yet */
	short flag;
	
} UnitSettings;

typedef struct PhysicsSettings {
	float gravity[3];
	int flag, quick_cache_step, rt;
} PhysicsSettings;

typedef struct Scene {
	ID id;
	struct AnimData *adt;	/* animation data (must be immediately after id for utilities to use it) */ 
	
	struct Object *camera;
	struct World *world;
	
	struct Scene *set;
	
	ListBase base;
	struct Base *basact;		/* active base */
	struct Object *obedit;		/* name replaces old G.obedit */
	
	float cursor[3];			/* 3d cursor location */
	float twcent[3];			/* center for transform widget */
	float twmin[3], twmax[3];	/* boundbox of selection for transform widget */
	
	unsigned int lay;			/* bitflags for layer visibility */
	int layact;		/* active layer */
	unsigned int lay_updated;       /* runtime flag, has layer ever been updated since load? */
	
	short flag;								/* various settings */
	
	short use_nodes;
	
	struct bNodeTree *nodetree;	
	
	struct Editing *ed;								/* sequence editor data is allocated here */
	
	struct ToolSettings *toolsettings;		/* default allocated now */
	struct SceneStats *stats;				/* default allocated now */

	/* migrate or replace? depends on some internal things... */
	/* no, is on the right place (ton) */
	struct RenderData r;
	struct AudioData audio;
	
	ListBase markers;
	ListBase transform_spaces;
	
	void *sound_scene;
	void *sound_scene_handle;
	void *sound_scrub_handle;
	void *speaker_handles;
	
	void *fps_info;					/* (runtime) info/cache used for presenting playback framerate info to the user */
	
	/* none of the dependancy graph  vars is mean to be saved */
	struct  DagForest *theDag;
	short dagisvalid, dagflags;
	short recalc;				/* recalc = counterpart of ob->recalc */

	short pad6;
	int pad5;

	/* User-Defined KeyingSets */
	int active_keyingset;			/* index of the active KeyingSet. first KeyingSet has index 1, 'none' active is 0, 'add new' is -1 */
	ListBase keyingsets;			/* KeyingSets for the given frame */
	
	/* Game Settings */
	struct GameFraming framing  DNA_DEPRECATED; // XXX  deprecated since 2.5
	struct GameData gm;

	/* Units */
	struct UnitSettings unit;
	
	/* Grease Pencil */
	struct bGPdata *gpd;

	/* Physics simulation settings */
	struct PhysicsSettings physics_settings;

	/* Movie Tracking */
	struct MovieClip *clip;			/* active movie clip */

	uint64_t customdata_mask;	/* XXX. runtime flag for drawing, actually belongs in the window, only used by object_handle_update() */
	uint64_t customdata_mask_modal; /* XXX. same as above but for temp operator use (gl renders) */
} Scene;


/* **************** RENDERDATA ********************* */

/* flag */
	/* use preview range */
#define SCER_PRV_RANGE	(1<<0)

/* mode (int now) */
#define R_OSA			0x0001
#define R_SHADOW		0x0002
#define R_GAMMA			0x0004
#define R_ORTHO			0x0008
#define R_ENVMAP		0x0010
#define R_EDGE			0x0020
#define R_FIELDS		0x0040
#define R_FIELDSTILL	0x0080
#define R_RADIO			0x0100
#define R_BORDER		0x0200
#define R_PANORAMA		0x0400	/* deprecated as scene option, still used in renderer */
#define R_CROP			0x0800
/*#define R_COSMO			0x1000 deprecated */
#define R_ODDFIELD		0x2000
#define R_MBLUR			0x4000
		/* unified was here */
#define R_RAYTRACE      0x10000
		/* R_GAUSS is obsolete, but used to retrieve setting from old files */
#define R_GAUSS      	0x20000
		/* fbuf obsolete... */
/*#define R_FBUF			0x40000*/
		/* threads obsolete... is there for old files, now use for autodetect threads */
#define R_THREADS		0x80000
		/* Use the same flag for autothreads */
#define R_FIXED_THREADS		0x80000 

#define R_SPEED			0x100000
#define R_SSS			0x200000
#define R_NO_OVERWRITE	0x400000 /* skip existing files */
#define R_TOUCH			0x800000 /* touch files before rendering */
#define R_SIMPLIFY		0x1000000

/* seq_flag */
#define R_SEQ_GL_PREV 1
#define R_SEQ_GL_REND 2

/* displaymode */

#define R_OUTPUT_SCREEN	0
#define R_OUTPUT_AREA	1
#define R_OUTPUT_WINDOW	2
#define R_OUTPUT_NONE	3
/*#define R_OUTPUT_FORKED	4*/

/* filtertype */
#define R_FILTER_BOX	0
#define R_FILTER_TENT	1
#define R_FILTER_QUAD	2
#define R_FILTER_CUBIC	3
#define R_FILTER_CATROM	4
#define R_FILTER_GAUSS	5
#define R_FILTER_MITCH	6
#define R_FILTER_FAST_GAUSS	7 /* note, this is only used for nodes at the moment */

/* raytrace structure */
#define R_RAYSTRUCTURE_AUTO				0
#define R_RAYSTRUCTURE_OCTREE			1
#define R_RAYSTRUCTURE_BLIBVH			2
#define R_RAYSTRUCTURE_VBVH				3
#define R_RAYSTRUCTURE_SIMD_SVBVH		4	/* needs SIMD */
#define R_RAYSTRUCTURE_SIMD_QBVH		5	/* needs SIMD */

/* raytrace_options */
#define R_RAYTRACE_USE_LOCAL_COORDS		0x0001
#define R_RAYTRACE_USE_INSTANCES		0x0002

/* scemode (int now) */
#define R_DOSEQ				0x0001
#define R_BG_RENDER			0x0002
		/* passepartout is camera option now, keep this for backward compatibility */
#define R_PASSEPARTOUT		0x0004
#define R_PREVIEWBUTS		0x0008
#define R_EXTENSION			0x0010
#define R_MATNODE_PREVIEW	0x0020
#define R_DOCOMP			0x0040
#define R_COMP_CROP			0x0080
#define R_FREE_IMAGE		0x0100
#define R_SINGLE_LAYER		0x0200
#define R_EXR_TILE_FILE		0x0400
#define R_COMP_FREE			0x0800
#define R_NO_IMAGE_LOAD		0x1000
#define R_NO_TEX			0x2000
#define R_NO_FRAME_UPDATE	0x4000
#define R_FULL_SAMPLE		0x8000
/* #define R_DEPRECATED		0x10000 */
/* #define R_RECURS_PROTECTION	0x20000 */
#define R_TEXNODE_PREVIEW	0x40000

/* r->stamp */
#define R_STAMP_TIME 	0x0001
#define R_STAMP_FRAME	0x0002
#define R_STAMP_DATE	0x0004
#define R_STAMP_CAMERA	0x0008
#define R_STAMP_SCENE	0x0010
#define R_STAMP_NOTE	0x0020
#define R_STAMP_DRAW	0x0040 /* draw in the image */
#define R_STAMP_MARKER	0x0080
#define R_STAMP_FILENAME	0x0100
#define R_STAMP_SEQSTRIP	0x0200
#define R_STAMP_RENDERTIME	0x0400
#define R_STAMP_CAMERALENS	0x0800
#define R_STAMP_ALL (R_STAMP_TIME|R_STAMP_FRAME|R_STAMP_DATE|R_STAMP_CAMERA|R_STAMP_SCENE| \
                     R_STAMP_NOTE|R_STAMP_MARKER|R_STAMP_FILENAME|R_STAMP_SEQSTRIP|        \
                     R_STAMP_RENDERTIME|R_STAMP_CAMERALENS)

/* alphamode */
#define R_ADDSKY		0
#define R_ALPHAPREMUL	1
#define R_ALPHAKEY		2

/* color_mgt_flag */
#define R_COLOR_MANAGEMENT              (1 << 0)
#define R_COLOR_MANAGEMENT_PREDIVIDE    (1 << 1)

/* subimtype, flag options for imtype */
#define R_OPENEXR_HALF    1                                      /*deprecated*/
#define R_OPENEXR_ZBUF    2                                      /*deprecated*/
#define R_PREVIEW_JPG    4                                       /*deprecated*/
#define R_CINEON_LOG     8                                       /*deprecated*/
#define R_TIFF_16BIT    16                                       /*deprecated*/

#define R_JPEG2K_12BIT    32 /* Jpeg2000 */                      /*deprecated*/
#define R_JPEG2K_16BIT    64                                     /*deprecated*/
#define R_JPEG2K_YCC    128 /* when disabled use RGB */          /*deprecated*/
#define R_JPEG2K_CINE_PRESET    256                              /*deprecated*/
#define R_JPEG2K_CINE_48FPS        512                           /*deprecated*/

/* bake_mode: same as RE_BAKE_xxx defines */
/* bake_flag: */
#define R_BAKE_CLEAR		1
#define R_BAKE_OSA			2
#define R_BAKE_TO_ACTIVE	4
#define R_BAKE_NORMALIZE	8
#define R_BAKE_MULTIRES		16
#define R_BAKE_LORES_MESH	32

/* bake_normal_space */
#define R_BAKE_SPACE_CAMERA	 0
#define R_BAKE_SPACE_WORLD	 1
#define R_BAKE_SPACE_OBJECT	 2
#define R_BAKE_SPACE_TANGENT 3

/* simplify_flag */
#define R_SIMPLE_NO_TRIANGULATE		1

/* sequencer seq_prev_type seq_rend_type */


/* **************** SCENE ********************* */

/* for general use */
#define MAXFRAME	300000
#define MAXFRAMEF	300000.0f

#define MINFRAME	0
#define MINFRAMEF	0.0f

/* (minimum frame number for current-frame) */
#define MINAFRAME	-300000
#define MINAFRAMEF	-300000.0f

/* depricate this! */
#define TESTBASE(v3d, base)  (                                                \
	((base)->flag & SELECT) &&                                                \
	((base)->lay & v3d->lay) &&                                               \
	(((base)->object->restrictflag & OB_RESTRICT_VIEW)==0)  )
#define TESTBASELIB(v3d, base)  (                                             \
	((base)->flag & SELECT) &&                                                \
	((base)->lay & v3d->lay) &&                                               \
	((base)->object->id.lib==NULL) &&                                         \
	(((base)->object->restrictflag & OB_RESTRICT_VIEW)==0)  )
#define TESTBASELIB_BGMODE(v3d, scene, base)  (                               \
	((base)->flag & SELECT) &&                                                \
	((base)->lay & (v3d ? v3d->lay : scene->lay)) &&                          \
	((base)->object->id.lib==NULL) &&                                         \
	(((base)->object->restrictflag & OB_RESTRICT_VIEW)==0)  )
#define BASE_EDITABLE_BGMODE(v3d, scene, base)  (                             \
	((base)->lay & (v3d ? v3d->lay : scene->lay)) &&                          \
	((base)->object->id.lib==NULL) &&                                         \
	(((base)->object->restrictflag & OB_RESTRICT_VIEW)==0))
#define BASE_SELECTABLE(v3d, base)  (                                         \
	(base->lay & v3d->lay) &&                                                 \
	(base->object->restrictflag & (OB_RESTRICT_SELECT|OB_RESTRICT_VIEW))==0  )
#define BASE_VISIBLE(v3d, base)  (                                            \
	(base->lay & v3d->lay) &&                                                 \
	(base->object->restrictflag & OB_RESTRICT_VIEW)==0  )

#define FIRSTBASE		scene->base.first
#define LASTBASE		scene->base.last
#define BASACT			(scene->basact)
#define OBACT			(BASACT? BASACT->object: NULL)

#define V3D_CAMERA_LOCAL(v3d) ((!(v3d)->scenelock && (v3d)->camera) ? (v3d)->camera : NULL)
#define V3D_CAMERA_SCENE(scene, v3d) ((!(v3d)->scenelock && (v3d)->camera) ? (v3d)->camera : (scene)->camera)

#define ID_NEW(a)		if( (a) && (a)->id.newid ) (a)= (void *)(a)->id.newid
#define ID_NEW_US(a)	if( (a)->id.newid) {(a)= (void *)(a)->id.newid; (a)->id.us++;}
#define ID_NEW_US2(a)	if( ((ID *)a)->newid) {(a)= ((ID *)a)->newid; ((ID *)a)->us++;}
#define	CFRA			(scene->r.cfra)
#define SUBFRA			(scene->r.subframe)
#define	SFRA			(scene->r.sfra)
#define	EFRA			(scene->r.efra)
#define PRVRANGEON		(scene->r.flag & SCER_PRV_RANGE)
#define PSFRA			((PRVRANGEON)? (scene->r.psfra): (scene->r.sfra))
#define PEFRA			((PRVRANGEON)? (scene->r.pefra): (scene->r.efra))
#define FRA2TIME(a)           ((((double) scene->r.frs_sec_base) * (double)(a)) / (double)scene->r.frs_sec)
#define TIME2FRA(a)           ((((double) scene->r.frs_sec) * (double)(a)) / (double)scene->r.frs_sec_base)
#define FPS                     (((double) scene->r.frs_sec) / (double)scene->r.frs_sec_base)

#define RAD_PHASE_PATCHES	1
#define RAD_PHASE_FACES		2

/* base->flag is in DNA_object_types.h */

/* toolsettings->snap_flag */
#define SCE_SNAP				1
#define SCE_SNAP_ROTATE			2
#define SCE_SNAP_PEEL_OBJECT	4
#define SCE_SNAP_PROJECT		8
#define SCE_SNAP_NO_SELF		16
/* toolsettings->snap_target */
#define SCE_SNAP_TARGET_CLOSEST	0
#define SCE_SNAP_TARGET_CENTER	1
#define SCE_SNAP_TARGET_MEDIAN	2
#define SCE_SNAP_TARGET_ACTIVE	3
/* toolsettings->snap_mode */
#define SCE_SNAP_MODE_INCREMENT	0
#define SCE_SNAP_MODE_VERTEX	1
#define SCE_SNAP_MODE_EDGE		2
#define SCE_SNAP_MODE_FACE		3
#define SCE_SNAP_MODE_VOLUME	4

/* toolsettings->selectmode */
#define SCE_SELECT_VERTEX	1 /* for mesh */
#define SCE_SELECT_EDGE		2
#define SCE_SELECT_FACE		4

/* toolsettings->particle.selectmode for particles */
#define SCE_SELECT_PATH		1
#define SCE_SELECT_POINT	2
#define SCE_SELECT_END		4

/* sce->recalc (now in use by previewrender) */
#define SCE_PRV_CHANGED		1

/* toolsettings->prop_mode (proportional falloff) */
#define PROP_SMOOTH            0
#define PROP_SPHERE            1
#define PROP_ROOT              2
#define PROP_SHARP             3
#define PROP_LIN               4
#define PROP_CONST             5
#define PROP_RANDOM            6
#define PROP_MODE_MAX          7

/* toolsettings->proportional */
#define PROP_EDIT_OFF			0
#define PROP_EDIT_ON			1
#define PROP_EDIT_CONNECTED	2

/* sce->flag */
#define SCE_DS_SELECTED			(1<<0)
#define SCE_DS_COLLAPSED		(1<<1)
#define SCE_NLA_EDIT_ON			(1<<2)
#define SCE_FRAME_DROP			(1<<3)


	/* return flag next_object function */
#define F_ERROR			-1
#define F_START			0
#define F_SCENE			1
#define F_DUPLI			3

/* audio->flag */
#define AUDIO_MUTE                (1<<0)
#define AUDIO_SYNC                (1<<1)
#define AUDIO_SCRUB		          (1<<2)
#define AUDIO_VOLUME_ANIMATED     (1<<3)

#define FFMPEG_MULTIPLEX_AUDIO  1 /* deprecated, you can choose none as audiocodec now */
#define FFMPEG_AUTOSPLIT_OUTPUT 2

/* Paint.flags */
typedef enum {
	PAINT_SHOW_BRUSH = (1<<0),
	PAINT_FAST_NAVIGATE = (1<<1),
	PAINT_SHOW_BRUSH_ON_SURFACE = (1<<2),
} PaintFlags;

/* Sculpt.flags */
/* These can eventually be moved to paint flags? */
typedef enum SculptFlags {
	SCULPT_SYMM_X = (1<<0),
	SCULPT_SYMM_Y = (1<<1),
	SCULPT_SYMM_Z = (1<<2),
	SCULPT_LOCK_X = (1<<3),
	SCULPT_LOCK_Y = (1<<4),
	SCULPT_LOCK_Z = (1<<5),
	SCULPT_SYMMETRY_FEATHER = (1<<6),
	SCULPT_USE_OPENMP = (1<<7),
	SCULPT_ONLY_DEFORM = (1<<8),
} SculptFlags;

/* sculpt_paint_settings */
#define SCULPT_PAINT_USE_UNIFIED_SIZE        (1<<0)
#define SCULPT_PAINT_USE_UNIFIED_ALPHA       (1<<1)
#define SCULPT_PAINT_UNIFIED_LOCK_BRUSH_SIZE (1<<2)
#define SCULPT_PAINT_UNIFIED_SIZE_PRESSURE   (1<<3)
#define SCULPT_PAINT_UNIFIED_ALPHA_PRESSURE  (1<<4)

/* ImagePaintSettings.flag */
#define IMAGEPAINT_DRAWING				1
// #define IMAGEPAINT_DRAW_TOOL			2 // deprecated
// #define IMAGEPAINT_DRAW_TOOL_DRAWING	4 // deprecated

/* projection painting only */
#define IMAGEPAINT_PROJECT_DISABLE		8	/* Non projection 3D painting */
#define IMAGEPAINT_PROJECT_XRAY			16
#define IMAGEPAINT_PROJECT_BACKFACE		32
#define IMAGEPAINT_PROJECT_FLAT			64
#define IMAGEPAINT_PROJECT_LAYER_CLONE	128
#define IMAGEPAINT_PROJECT_LAYER_STENCIL	256
#define IMAGEPAINT_PROJECT_LAYER_STENCIL_INV	512

/* toolsettings->uvcalc_flag */
#define UVCALC_FILLHOLES			1
#define UVCALC_NO_ASPECT_CORRECT	2	/* would call this UVCALC_ASPECT_CORRECT, except it should be default with old file */
#define UVCALC_TRANSFORM_CORRECT	4	/* adjust UV's while transforming to avoid distortion */

/* toolsettings->uv_flag */
#define UV_SYNC_SELECTION	1
#define UV_SHOW_SAME_IMAGE	2

/* toolsettings->uv_selectmode */
#define UV_SELECT_VERTEX	1
#define UV_SELECT_EDGE		2
#define UV_SELECT_FACE		4
#define UV_SELECT_ISLAND	8

/* toolsettings->edge_mode */
#define EDGE_MODE_SELECT				0
#define EDGE_MODE_TAG_SEAM				1
#define EDGE_MODE_TAG_SHARP				2
#define EDGE_MODE_TAG_CREASE			3
#define EDGE_MODE_TAG_BEVEL				4

/* toolsettings->gpencil_flags */
#define GP_TOOL_FLAG_PAINTSESSIONS_ON	(1<<0)

/* toolsettings->particle flag */
#define PE_KEEP_LENGTHS			1
#define PE_LOCK_FIRST			2
#define PE_DEFLECT_EMITTER		4
#define PE_INTERPOLATE_ADDED	8
#define PE_DRAW_PART			16
/* #define PE_X_MIRROR			64 */	/* deprecated */
#define PE_FADE_TIME			128
#define PE_AUTO_VELOCITY		256

/* toolsetting->particle brushtype */
#define PE_BRUSH_NONE		-1
#define PE_BRUSH_COMB		0
#define PE_BRUSH_CUT		1
#define PE_BRUSH_LENGTH		2
#define PE_BRUSH_PUFF		3
#define PE_BRUSH_ADD		4
#define PE_BRUSH_SMOOTH		5
#define PE_BRUSH_WEIGHT		6

/* this must equal ParticleEditSettings.brush array size */
#define PE_TOT_BRUSH		6

/* ParticleBrushData->flag */
#define PE_BRUSH_DATA_PUFF_VOLUME 1

/* tooksettings->particle edittype */
#define PE_TYPE_PARTICLES	0
#define PE_TYPE_SOFTBODY	1
#define PE_TYPE_CLOTH		2

/* toolsettings->retopo_mode */
#define RETOPO 1
#define RETOPO_PAINT 2

/* toolsettings->retopo_paint_tool */ /*UNUSED*/
/* #define RETOPO_PEN 1 */
/* #define RETOPO_LINE 2 */
/* #define RETOPO_ELLIPSE 4 */

/* toolsettings->skgen_options */
#define SKGEN_FILTER_INTERNAL	(1 << 0)
#define SKGEN_FILTER_EXTERNAL	(1 << 1)
#define	SKGEN_SYMMETRY			(1 << 2)
#define	SKGEN_CUT_LENGTH		(1 << 3)
#define	SKGEN_CUT_ANGLE			(1 << 4)
#define	SKGEN_CUT_CORRELATION	(1 << 5)
#define	SKGEN_HARMONIC			(1 << 6)
#define	SKGEN_STICK_TO_EMBEDDING	(1 << 7)
#define	SKGEN_ADAPTIVE_DISTANCE		(1 << 8)
#define SKGEN_FILTER_SMART		(1 << 9)
#define SKGEN_DISP_LENGTH		(1 << 10)
#define SKGEN_DISP_WEIGHT		(1 << 11)
#define SKGEN_DISP_ORIG			(1 << 12)
#define SKGEN_DISP_EMBED		(1 << 13)
#define SKGEN_DISP_INDEX		(1 << 14)

#define	SKGEN_SUB_LENGTH		0
#define	SKGEN_SUB_ANGLE			1
#define	SKGEN_SUB_CORRELATION	2
#define	SKGEN_SUB_TOTAL			3

/* toolsettings->skgen_postpro */
#define SKGEN_SMOOTH			0
#define SKGEN_AVERAGE			1
#define SKGEN_SHARPEN			2

/* toolsettings->bone_sketching */
#define BONE_SKETCHING			1
#define BONE_SKETCHING_QUICK	2
#define BONE_SKETCHING_ADJUST	4

/* toolsettings->bone_sketching_convert */
#define	SK_CONVERT_CUT_FIXED			0
#define	SK_CONVERT_CUT_LENGTH			1
#define	SK_CONVERT_CUT_ADAPTATIVE		2
#define	SK_CONVERT_RETARGET				3

/* toolsettings->skgen_retarget_options */
#define	SK_RETARGET_AUTONAME			1

/* toolsettings->skgen_retarget_roll */
#define	SK_RETARGET_ROLL_NONE			0
#define	SK_RETARGET_ROLL_VIEW			1
#define	SK_RETARGET_ROLL_JOINT			2

/* physics_settings->flag */
#define PHYS_GLOBAL_GRAVITY		1

/* UnitSettings */

/* UnitSettings->system */
#define	USER_UNIT_NONE			0
#define	USER_UNIT_METRIC		1
#define	USER_UNIT_IMPERIAL		2
/* UnitSettings->flag */
#define	USER_UNIT_OPT_SPLIT		1
#define USER_UNIT_ROT_RADIANS	2

#ifdef __cplusplus
}
#endif

#endif
