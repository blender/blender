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

#ifndef __DNA_SCENE_TYPES_H__
#define __DNA_SCENE_TYPES_H__

#include "DNA_defs.h"

/* XXX, temp feature - campbell */
#define DURIAN_CAMERA_SWITCH

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_color_types.h"  /* color management */
#include "DNA_vec_types.h"
#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_freestyle_types.h"
#include "DNA_gpu_types.h"
#include "DNA_userdef_types.h"

struct CurveMapping;
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
struct bGPDbrush;
struct MovieClip;
struct ColorSpace;

/* ************************************************************* */
/* Scene Data */

/* Base - Wrapper for referencing Objects in a Scene */
typedef struct Base {
	struct Base *next, *prev;
	unsigned int lay, selcol;
	int flag;
	short sx, sy;
	struct Object *object;
} Base;

/* ************************************************************* */
/* Output Format Data */

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

typedef enum FFMpegPreset {
	FFM_PRESET_NONE,
	FFM_PRESET_ULTRAFAST,
	FFM_PRESET_SUPERFAST,
	FFM_PRESET_VERYFAST,
	FFM_PRESET_FASTER,
	FFM_PRESET_FAST,
	FFM_PRESET_MEDIUM,
	FFM_PRESET_SLOW,
	FFM_PRESET_SLOWER,
	FFM_PRESET_VERYSLOW,
} FFMpegPreset;


/* Mapping from easily-understandable descriptions to CRF values.
 * Assumes we output 8-bit video. Needs to be remapped if 10-bit
 * is output.
 * We use a slightly wider than "subjectively sane range" according
 * to https://trac.ffmpeg.org/wiki/Encode/H.264#a1.ChooseaCRFvalue
 */
typedef enum FFMpegCrf {
	FFM_CRF_NONE = -1,
	FFM_CRF_LOSSLESS = 0,
	FFM_CRF_PERC_LOSSLESS = 17,
	FFM_CRF_HIGH = 20,
	FFM_CRF_MEDIUM = 23,
	FFM_CRF_LOW = 26,
	FFM_CRF_VERYLOW = 29,
	FFM_CRF_LOWEST = 32,
} FFMpegCrf;

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
	int max_b_frames; /* only used if FFMPEG_USE_MAX_B_FRAMES flag is set. */
	int flags;
	int constant_rate_factor;
	int ffmpeg_preset; /* see FFMpegPreset */

	int rc_min_rate;
	int rc_max_rate;
	int rc_buffer_size;
	int mux_packet_size;
	int mux_rate;
	int pad1;

	IDProperty *properties;
} FFMpegCodecData;

/* ************************************************************* */
/* Audio */

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

/* *************************************************************** */
/* Render Layers */

/* Render Layer */
typedef struct SceneRenderLayer {
	struct SceneRenderLayer *next, *prev;
	
	char name[64];	/* MAX_NAME */
	
	struct Material *mat_override;
	struct Group *light_override;
	
	unsigned int lay;		  /* scene->lay itself has priority over this */
	unsigned int lay_zmask;	  /* has to be after lay, this is for Z-masking */
	unsigned int lay_exclude; /* not used by internal, exclude */
	int layflag;
	
	int passflag;			/* pass_xor has to be after passflag */
	int pass_xor;

	int samples;
	float pass_alpha_threshold;

	IDProperty *prop;

	struct FreestyleConfig freestyleConfig;
} SceneRenderLayer;

/* srl->layflag */
#define SCE_LAY_SOLID	1
#define SCE_LAY_ZTRA	2
#define SCE_LAY_HALO	4
#define SCE_LAY_EDGE	8
#define SCE_LAY_SKY		16
#define SCE_LAY_STRAND	32
#define SCE_LAY_FRS		64
#define SCE_LAY_AO		128
	/* flags between 256 and 0x8000 are set to 1 already, for future options */

#define SCE_LAY_ALL_Z		0x8000
#define SCE_LAY_XOR			0x10000
#define SCE_LAY_DISABLE		0x20000
#define SCE_LAY_ZMASK		0x40000
#define SCE_LAY_NEG_ZMASK	0x80000

/* srl->passflag */
typedef enum ScenePassType {
	SCE_PASS_COMBINED                 = (1 << 0),
	SCE_PASS_Z                        = (1 << 1),
	SCE_PASS_RGBA                     = (1 << 2),
	SCE_PASS_DIFFUSE                  = (1 << 3),
	SCE_PASS_SPEC                     = (1 << 4),
	SCE_PASS_SHADOW                   = (1 << 5),
	SCE_PASS_AO                       = (1 << 6),
	SCE_PASS_REFLECT                  = (1 << 7),
	SCE_PASS_NORMAL                   = (1 << 8),
	SCE_PASS_VECTOR                   = (1 << 9),
	SCE_PASS_REFRACT                  = (1 << 10),
	SCE_PASS_INDEXOB                  = (1 << 11),
	SCE_PASS_UV                       = (1 << 12),
	SCE_PASS_INDIRECT                 = (1 << 13),
	SCE_PASS_MIST                     = (1 << 14),
	SCE_PASS_RAYHITS                  = (1 << 15),
	SCE_PASS_EMIT                     = (1 << 16),
	SCE_PASS_ENVIRONMENT              = (1 << 17),
	SCE_PASS_INDEXMA                  = (1 << 18),
	SCE_PASS_DIFFUSE_DIRECT           = (1 << 19),
	SCE_PASS_DIFFUSE_INDIRECT         = (1 << 20),
	SCE_PASS_DIFFUSE_COLOR            = (1 << 21),
	SCE_PASS_GLOSSY_DIRECT            = (1 << 22),
	SCE_PASS_GLOSSY_INDIRECT          = (1 << 23),
	SCE_PASS_GLOSSY_COLOR             = (1 << 24),
	SCE_PASS_TRANSM_DIRECT            = (1 << 25),
	SCE_PASS_TRANSM_INDIRECT          = (1 << 26),
	SCE_PASS_TRANSM_COLOR             = (1 << 27),
	SCE_PASS_SUBSURFACE_DIRECT        = (1 << 28),
	SCE_PASS_SUBSURFACE_INDIRECT      = (1 << 29),
	SCE_PASS_SUBSURFACE_COLOR         = (1 << 30),
} ScenePassType;

#define RE_PASSNAME_COMBINED "Combined"
#define RE_PASSNAME_Z "Depth"
#define RE_PASSNAME_VECTOR "Vector"
#define RE_PASSNAME_NORMAL "Normal"
#define RE_PASSNAME_UV "UV"
#define RE_PASSNAME_RGBA "Color"
#define RE_PASSNAME_EMIT "Emit"
#define RE_PASSNAME_DIFFUSE "Diffuse"
#define RE_PASSNAME_SPEC "Spec"
#define RE_PASSNAME_SHADOW "Shadow"

#define RE_PASSNAME_AO "AO"
#define RE_PASSNAME_ENVIRONMENT "Env"
#define RE_PASSNAME_INDIRECT "Indirect"
#define RE_PASSNAME_REFLECT "Reflect"
#define RE_PASSNAME_REFRACT "Refract"
#define RE_PASSNAME_INDEXOB "IndexOB"
#define RE_PASSNAME_INDEXMA "IndexMA"
#define RE_PASSNAME_MIST "Mist"

#define RE_PASSNAME_RAYHITS "RayHits"
#define RE_PASSNAME_DIFFUSE_DIRECT "DiffDir"
#define RE_PASSNAME_DIFFUSE_INDIRECT "DiffInd"
#define RE_PASSNAME_DIFFUSE_COLOR "DiffCol"
#define RE_PASSNAME_GLOSSY_DIRECT "GlossDir"
#define RE_PASSNAME_GLOSSY_INDIRECT "GlossInd"
#define RE_PASSNAME_GLOSSY_COLOR "GlossCol"
#define RE_PASSNAME_TRANSM_DIRECT "TransDir"
#define RE_PASSNAME_TRANSM_INDIRECT "TransInd"
#define RE_PASSNAME_TRANSM_COLOR "TransCol"

#define RE_PASSNAME_SUBSURFACE_DIRECT "SubsurfaceDir"
#define RE_PASSNAME_SUBSURFACE_INDIRECT "SubsurfaceInd"
#define RE_PASSNAME_SUBSURFACE_COLOR "SubsurfaceCol"

/* note, srl->passflag is treestore element 'nr' in outliner, short still... */

/* View - MultiView */
typedef struct SceneRenderView {
	struct SceneRenderView *next, *prev;

	char name[64];	/* MAX_NAME */
	char suffix[64];	/* MAX_NAME */

	int viewflag;
	int pad[2];
	char pad2[4];

} SceneRenderView;

/* srv->viewflag */
#define SCE_VIEW_DISABLE		(1<<0)

/* scene.render.views_format */
enum {
	SCE_VIEWS_FORMAT_STEREO_3D = 0,
	SCE_VIEWS_FORMAT_MULTIVIEW = 1,
};

/* ImageFormatData.views_output */
enum {
	R_IMF_VIEWS_INDIVIDUAL = 0,
	R_IMF_VIEWS_STEREO_3D  = 1,
	R_IMF_VIEWS_MULTIVIEW  = 2,
};

typedef struct Stereo3dFormat {
	short flag;
	char display_mode; /* encoding mode */
	char anaglyph_type; /* anaglyph scheme for the user display */
	char interlace_type;  /* interlace type for the user display */
	char pad[3];
} Stereo3dFormat;

/* Stereo3dFormat.display_mode */
typedef enum eStereoDisplayMode {
	S3D_DISPLAY_ANAGLYPH    = 0,
	S3D_DISPLAY_INTERLACE   = 1,
	S3D_DISPLAY_PAGEFLIP    = 2,
	S3D_DISPLAY_SIDEBYSIDE  = 3,
	S3D_DISPLAY_TOPBOTTOM   = 4,
} eStereoDisplayMode;

/* Stereo3dFormat.flag */
typedef enum eStereo3dFlag {
	S3D_INTERLACE_SWAP        = (1 << 0),
	S3D_SIDEBYSIDE_CROSSEYED  = (1 << 1),
	S3D_SQUEEZED_FRAME        = (1 << 2),
} eStereo3dFlag;

/* Stereo3dFormat.anaglyph_type */
typedef enum eStereo3dAnaglyphType {
	S3D_ANAGLYPH_REDCYAN      = 0,
	S3D_ANAGLYPH_GREENMAGENTA = 1,
	S3D_ANAGLYPH_YELLOWBLUE   = 2,
} eStereo3dAnaglyphType;

/* Stereo3dFormat.interlace_type */
typedef enum eStereo3dInterlaceType {
	S3D_INTERLACE_ROW          = 0,
	S3D_INTERLACE_COLUMN       = 1,
	S3D_INTERLACE_CHECKERBOARD = 2,
} eStereo3dInterlaceType;

/* *************************************************************** */

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

	char planes;   /* - R_IMF_PLANES_BW, R_IMF_PLANES_RGB, R_IMF_PLANES_RGBA */
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
	char jp2_codec;

	/* TIFF */
	char tiff_codec;

	char pad[4];

	/* Multiview */
	char views_format;
	Stereo3dFormat stereo3d_format;

	/* color management */
	ColorManagedViewSettings view_settings;
	ColorManagedDisplaySettings display_settings;
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
/* #define R_IMF_IMTYPE_AVICODEC    18 */ /* avicodec is nomore */
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
#define R_IMF_IMTYPE_PSD            34

#define R_IMF_IMTYPE_INVALID        255

/* ImageFormatData.flag */
#define R_IMF_FLAG_ZBUF         (1<<0)   /* was R_OPENEXR_ZBUF */
#define R_IMF_FLAG_PREVIEW_JPG  (1<<1)   /* was R_PREVIEW_JPG */

/* return values from BKE_imtype_valid_depths, note this is depts per channel */
#define R_IMF_CHAN_DEPTH_1  (1<<0) /* 1bits  (unused) */
#define R_IMF_CHAN_DEPTH_8  (1<<1) /* 8bits  (default) */
#define R_IMF_CHAN_DEPTH_10 (1<<2) /* 10bits (uncommon, Cineon/DPX support) */
#define R_IMF_CHAN_DEPTH_12 (1<<3) /* 12bits (uncommon, jp2/DPX support) */
#define R_IMF_CHAN_DEPTH_16 (1<<4) /* 16bits (tiff, halff float exr) */
#define R_IMF_CHAN_DEPTH_24 (1<<5) /* 24bits (unused) */
#define R_IMF_CHAN_DEPTH_32 (1<<6) /* 32bits (full float exr) */

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
#define R_IMF_EXR_CODEC_ZIPS  5
#define R_IMF_EXR_CODEC_B44   6
#define R_IMF_EXR_CODEC_B44A  7
#define R_IMF_EXR_CODEC_DWAA  8
#define R_IMF_EXR_CODEC_DWAB  9
#define R_IMF_EXR_CODEC_MAX  10

/* ImageFormatData.jp2_flag */
#define R_IMF_JP2_FLAG_YCC          (1<<0)  /* when disabled use RGB */ /* was R_JPEG2K_YCC */
#define R_IMF_JP2_FLAG_CINE_PRESET  (1<<1)  /* was R_JPEG2K_CINE_PRESET */
#define R_IMF_JP2_FLAG_CINE_48      (1<<2)  /* was R_JPEG2K_CINE_48FPS */

/* ImageFormatData.jp2_codec */
#define R_IMF_JP2_CODEC_JP2  0
#define R_IMF_JP2_CODEC_J2K  1

/* ImageFormatData.cineon_flag */
#define R_IMF_CINEON_FLAG_LOG (1<<0)  /* was R_CINEON_LOG */

/* ImageFormatData.tiff_codec */
enum {
	R_IMF_TIFF_CODEC_DEFLATE   = 0,
	R_IMF_TIFF_CODEC_LZW       = 1,
	R_IMF_TIFF_CODEC_PACKBITS  = 2,
	R_IMF_TIFF_CODEC_NONE      = 3,
};

typedef struct BakeData {
	struct ImageFormatData im_format;

	char filepath[1024]; /* FILE_MAX */

	short width, height;
	short margin, flag;

	float cage_extrusion;
	int pass_filter;

	char normal_swizzle[3];
	char normal_space;

	char save_mode;
	char pad[3];

	char cage[64];  /* MAX_NAME */
} BakeData;

/* (char) normal_swizzle */
typedef enum BakeNormalSwizzle {
	R_BAKE_POSX = 0,
	R_BAKE_POSY = 1,
	R_BAKE_POSZ = 2,
	R_BAKE_NEGX = 3,
	R_BAKE_NEGY = 4,
	R_BAKE_NEGZ = 5,
} BakeNormalSwizzle;

/* (char) save_mode */
typedef enum BakeSaveMode {
	R_BAKE_SAVE_INTERNAL = 0,
	R_BAKE_SAVE_EXTERNAL = 1,
} BakeSaveMode;

/* bake->pass_filter */
typedef enum BakePassFilter {
	R_BAKE_PASS_FILTER_NONE           = 0,
	R_BAKE_PASS_FILTER_AO             = (1 << 0),
	R_BAKE_PASS_FILTER_EMIT           = (1 << 1),
	R_BAKE_PASS_FILTER_DIFFUSE        = (1 << 2),
	R_BAKE_PASS_FILTER_GLOSSY         = (1 << 3),
	R_BAKE_PASS_FILTER_TRANSM         = (1 << 4),
	R_BAKE_PASS_FILTER_SUBSURFACE     = (1 << 5),
	R_BAKE_PASS_FILTER_DIRECT         = (1 << 6),
	R_BAKE_PASS_FILTER_INDIRECT       = (1 << 7),
	R_BAKE_PASS_FILTER_COLOR          = (1 << 8),
} BakePassFilter;

#define R_BAKE_PASS_FILTER_ALL (~0)

/* *************************************************************** */
/* Render Data */

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

	short size; /* size in % */
	
	short maximsize DNA_DEPRECATED; /* max in Kb */

	short pad6;

	/* from buttons: */
	/**
	 * The desired number of pixels in the x direction
	 */
	int xsch;
	/**
	 * The desired number of pixels in the y direction
	 */
	int ysch;

	/**
	 * The number of part to use in the x direction
	 */
	short xparts DNA_DEPRECATED;
	/**
	 * The number of part to use in the y direction
	 */
	short yparts DNA_DEPRECATED;

	/**
	 * render tile dimensions
	 */
	int tilex, tiley;

	short planes  DNA_DEPRECATED, imtype  DNA_DEPRECATED, subimtype  DNA_DEPRECATED, quality  DNA_DEPRECATED; /*deprecated!*/
	
	/**
	 * Render to image editor, fullscreen or to new window.
	 */
	short displaymode;
	char use_lock_interface;
	char pad7;

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
	float bake_maxdist, bake_biasdist;
	short bake_samples, bake_pad;
	float bake_user_scale, bake_pad1;

	/* path to render output */
	char pic[1024]; /* 1024 = FILE_MAX */

	/* stamps flags. */
	int stamp;
	short stamp_font_id, pad3; /* select one of blenders bitmap fonts */

	/* stamp info user data. */
	char stamp_udata[768];

	/* foreground/background color. */
	float fg_stamp[4];
	float bg_stamp[4];

	/* sequencer options */
	char seq_prev_type;
	char seq_rend_type;  /* UNUSED! */
	char seq_flag; /* flag use for sequence render/draw */
	char pad5[5];

	/* render simplify */
	int simplify_flag;
	short simplify_subsurf;
	short simplify_subsurf_render;
	short simplify_shadowsamples, pad9;
	float simplify_particles;
	float simplify_particles_render;
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

	/* Freestyle line thickness options */
	int line_thickness_mode;
	float unit_line_thickness; /* in pixels */

	/* render engine */
	char engine[32];

	/* Cycles baking */
	struct BakeData bake;

	int preview_start_resolution;

	/* Type of the debug pass to use.
	 * Only used when built with debug passes support.
	 */
	short debug_pass_type;

	short pad;

	/* MultiView */
	ListBase views;  /* SceneRenderView */
	short actview;
	short views_format;
	short pad8[2];

	/* Motion blur shutter */
	struct CurveMapping mblur_shutter_curve;
} RenderData;

/* *************************************************************** */
/* Render Conversion/Simplfication Settings */

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

/* *************************************************************** */
/* Game Engine - Dome */

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

/* *************************************************************** */
/* Game Engine */

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
	char partitioning;
	char pad1;
	short pad2;
} RecastData;

#define RC_PARTITION_WATERSHED 0
#define RC_PARTITION_MONOTONE 1
#define RC_PARTITION_LAYERS 2

typedef struct GameData {

	/* standalone player */
	struct GameFraming framing;
	short playerflag, xplay, yplay, freqplay;
	short depth, attrib, rt1, rt2;
	short aasamples, pad4[3];

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
	 * bit 5: (gameengine) : enable Bullet DBVT tree for view frustum culling
	 */
	int flag;
	short mode, matmode;
	short occlusionRes;		/* resolution of occlusion Z buffer in pixel */
	short physicsEngine;
	short exitkey;
	short vsync; /* Controls vsync: off, on, or adaptive (if supported) */
	short ticrate, maxlogicstep, physubstep, maxphystep;
	short obstacleSimulation;
	short raster_storage;
	float levelHeight;
	float deactivationtime, lineardeactthreshold, angulardeactthreshold;

	/* Scene LoD */
	short lodflag, pad2;
	int scehysteresis, pad5;

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
#define STEREO_3DTVTOPBOTTOM 9

/* physicsEngine */
#define WOPHY_NONE		0
#define WOPHY_BULLET	5

/* obstacleSimulation */
#define OBSTSIMULATION_NONE		0
#define OBSTSIMULATION_TOI_rays		1
#define OBSTSIMULATION_TOI_cells	2

/* Raster storage */
#define RAS_STORE_AUTO		0
#define RAS_STORE_IMMEDIATE	1
#define RAS_STORE_VA		2
#define RAS_STORE_VBO		3

/* vsync */
#define VSYNC_ON	0
#define VSYNC_OFF	1
#define VSYNC_ADAPTIVE	2

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
#define GAME_NO_MATERIAL_CACHING			(1 << 17)
#define GAME_GLSL_NO_ENV_LIGHTING			(1 << 18)
/* Note: GameData.flag is now an int (max 32 flags). A short could only take 16 flags */

/* GameData.playerflag */
#define GAME_PLAYER_FULLSCREEN				(1 << 0)
#define GAME_PLAYER_DESKTOP_RESOLUTION		(1 << 1)

/* GameData.matmode */
enum {
#ifdef DNA_DEPRECATED
	GAME_MAT_TEXFACE    = 0, /* deprecated */
#endif
	GAME_MAT_MULTITEX   = 1,
	GAME_MAT_GLSL       = 2,
};

/* GameData.lodflag */
#define SCE_LOD_USE_HYST		(1 << 0)

/* UV Paint */
#define UV_SCULPT_LOCK_BORDERS				1
#define UV_SCULPT_ALL_ISLANDS				2

#define UV_SCULPT_TOOL_PINCH				1
#define UV_SCULPT_TOOL_RELAX				2
#define UV_SCULPT_TOOL_GRAB					3

#define UV_SCULPT_TOOL_RELAX_LAPLACIAN	1
#define UV_SCULPT_TOOL_RELAX_HC			2

/* Stereo Flags */
#define STEREO_RIGHT_NAME "right"
#define STEREO_LEFT_NAME "left"
#define STEREO_RIGHT_SUFFIX "_R"
#define STEREO_LEFT_SUFFIX "_L"

typedef enum StereoViews {
	STEREO_LEFT_ID = 0,
	STEREO_RIGHT_ID = 1,
	STEREO_3D_ID = 2,
	STEREO_MONO_ID = 3,
} StereoViews;

/* *************************************************************** */
/* Markers */

typedef struct TimeMarker {	
	struct TimeMarker *next, *prev;
	int frame;
	char name[64];
	unsigned int flag;
	struct Object *camera;
} TimeMarker;

/* *************************************************************** */
/* Paint Mode/Tool Data */

#define PAINT_MAX_INPUT_SAMPLES 64

/* Paint Tool Base */
typedef struct Paint {
	struct Brush *brush;
	struct Palette *palette;
	struct CurveMapping *cavity_curve; /* cavity curve */

	/* WM Paint cursor */
	void *paint_cursor;
	unsigned char paint_cursor_col[4];

	/* enum PaintFlags */
	int flags;

	/* Paint stroke can use up to PAINT_MAX_INPUT_SAMPLES inputs to
	 * smooth the stroke */
	int num_input_samples;
	
	/* flags used for symmetry */
	int symmetry_flags;

	float tile_offset[3];
	int pad2;
} Paint;

/* ------------------------------------------- */
/* Image Paint */

/* Texture/Image Editor */
typedef struct ImagePaintSettings {
	Paint paint;

	short flag, missing_data;
	
	/* for projection painting only */
	short seam_bleed, normal_angle;
	short screen_grab_size[2]; /* capture size for re-projection */

	int mode;                  /* mode used for texture painting */

	void *paintcursor;		   /* wm handle */
	struct Image *stencil;     /* workaround until we support true layer masks */
	struct Image *clone;       /* clone layer for image mode for projective texture painting */
	struct Image *canvas;      /* canvas when the explicit system is used for painting */
	float stencil_col[3];
	float dither;              /* dither amount used when painting on byte images */
} ImagePaintSettings;

/* ------------------------------------------- */
/* Particle Edit */

/* Settings for a Particle Editing Brush */
typedef struct ParticleBrushData {
	short size;						/* common setting */
	short step, invert, count;		/* for specific brushes only */
	int flag;
	float strength;
} ParticleBrushData;

/* Particle Edit Mode Settings */
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
	struct Object *shape_object;
} ParticleEditSettings;

/* ------------------------------------------- */
/* Sculpt */

/* Sculpt */
typedef struct Sculpt {
	Paint paint;

	/* For rotating around a pivot point */
	//float pivot[3]; XXX not used?
	int flags;

	/* Control tablet input */
	//char tablet_size, tablet_strength; XXX not used?
	int radial_symm[3];

	/* Maximum edge length for dynamic topology sculpting (in pixels) */
	float detail_size;

	/* Direction used for SCULPT_OT_symmetrize operator */
	int symmetrize_direction;

	/* gravity factor for sculpting */
	float gravity_factor;

	/* scale for constant detail size */
	float constant_detail; /* Constant detail resolution (Blender unit / constant_detail) */
	float detail_percent;
	float pad;

	struct Object *gravity_object;
} Sculpt;

typedef struct UvSculpt {
	Paint paint;
} UvSculpt;

/* ------------------------------------------- */
/* Vertex Paint */

/* Vertex Paint */
typedef struct VPaint {
	Paint paint;

	short flag, pad;
	int tot;							/* allocation size of prev buffers */
	unsigned int *vpaint_prev;			/* previous mesh colors */
	struct MDeformVert *wpaint_prev;	/* previous vertex weights */
	
	void *paintcursor;					/* wm handle */
} VPaint;

/* VPaint.flag */
enum {
	// VP_COLINDEX  = (1 << 0),  /* only paint onto active material*/  /* deprecated since before 2.49 */
	// VP_AREA      = (1 << 1),  /* deprecated since 2.70 */
	VP_NORMALS      = (1 << 3),
	VP_SPRAY        = (1 << 4),
	// VP_MIRROR_X  = (1 << 5),  /* deprecated in 2.5x use (me->editflag & ME_EDIT_MIRROR_X) */
	VP_ONLYVGROUP   = (1 << 7)   /* weight paint only */
};

/* ------------------------------------------- */
/* GPencil Stroke Sculpting */

/* Brush types */
typedef enum eGP_EditBrush_Types {
	GP_EDITBRUSH_TYPE_SMOOTH    = 0,
	GP_EDITBRUSH_TYPE_THICKNESS = 1,
	GP_EDITBRUSH_TYPE_GRAB      = 2,
	GP_EDITBRUSH_TYPE_PUSH      = 3,
	GP_EDITBRUSH_TYPE_TWIST     = 4,
	GP_EDITBRUSH_TYPE_PINCH     = 5,
	GP_EDITBRUSH_TYPE_RANDOMIZE = 6,
	GP_EDITBRUSH_TYPE_SUBDIVIDE = 7,
	GP_EDITBRUSH_TYPE_SIMPLIFY  = 8,
	GP_EDITBRUSH_TYPE_CLONE     = 9,
	GP_EDITBRUSH_TYPE_STRENGTH  = 10,

	/* !!! Update GP_EditBrush_Data brush[###]; below !!! */
	TOT_GP_EDITBRUSH_TYPES
} eGP_EditBrush_Types;

/* Lock axis options */
typedef enum eGP_Lockaxis_Types {
	GP_LOCKAXIS_NONE = 0,
	GP_LOCKAXIS_X = 1,
	GP_LOCKAXIS_Y = 2,
	GP_LOCKAXIS_Z = 3
} eGP_Lockaxis_Types;

/* Settings for a GPencil Stroke Sculpting Brush */
typedef struct GP_EditBrush_Data {
	short size;             /* radius of brush */
	short flag;             /* eGP_EditBrush_Flag */
	float strength;         /* strength of effect */
} GP_EditBrush_Data;

/* GP_EditBrush_Data.flag */
typedef enum eGP_EditBrush_Flag {
	/* invert the effect of the brush */
	GP_EDITBRUSH_FLAG_INVERT       = (1 << 0),
	/* adjust strength using pen pressure */
	GP_EDITBRUSH_FLAG_USE_PRESSURE = (1 << 1),
	
	/* strength of brush falls off with distance from cursor */
	GP_EDITBRUSH_FLAG_USE_FALLOFF  = (1 << 2),
	
	/* smooth brush affects pressure values as well */
	GP_EDITBRUSH_FLAG_SMOOTH_PRESSURE  = (1 << 3)
} eGP_EditBrush_Flag;



/* GPencil Stroke Sculpting Settings */
typedef struct GP_BrushEdit_Settings {
	GP_EditBrush_Data brush[11];  /* TOT_GP_EDITBRUSH_TYPES */
	void *paintcursor;            /* runtime */
	
	int brushtype;                /* eGP_EditBrush_Types */
	int flag;                     /* eGP_BrushEdit_SettingsFlag */
	int lock_axis;                /* lock drawing to one axis */
	float alpha;                  /* alpha factor for selection color */
} GP_BrushEdit_Settings;

/* GP_BrushEdit_Settings.flag */
typedef enum eGP_BrushEdit_SettingsFlag {
	/* only affect selected points */
	GP_BRUSHEDIT_FLAG_SELECT_MASK = (1 << 0),
	/* apply brush to position */
	GP_BRUSHEDIT_FLAG_APPLY_POSITION = (1 << 1),
	/* apply brush to strength */
	GP_BRUSHEDIT_FLAG_APPLY_STRENGTH = (1 << 2),
	/* apply brush to thickness */
	GP_BRUSHEDIT_FLAG_APPLY_THICKNESS = (1 << 3),
} eGP_BrushEdit_SettingsFlag;


/* Settings for GP Interpolation Operators */
typedef struct GP_Interpolate_Settings {
	short flag;                        /* eGP_Interpolate_SettingsFlag */
	
	char type;                         /* eGP_Interpolate_Type - Interpolation Mode */ 
	char easing;                       /* eBezTriple_Easing - Easing mode (if easing equation used) */
	
	float back;                        /* BEZT_IPO_BACK */
	float amplitude, period;           /* BEZT_IPO_ELASTIC */
	
	struct CurveMapping *custom_ipo;   /* custom interpolation curve (for use with GP_IPO_CURVEMAP) */
} GP_Interpolate_Settings;

/* GP_Interpolate_Settings.flag */
typedef enum eGP_Interpolate_SettingsFlag {
	/* apply interpolation to all layers */
	GP_TOOLFLAG_INTERPOLATE_ALL_LAYERS    = (1 << 0),
	/* apply interpolation to only selected */
	GP_TOOLFLAG_INTERPOLATE_ONLY_SELECTED = (1 << 1),
} eGP_Interpolate_SettingsFlag;

/* GP_Interpolate_Settings.type */
typedef enum eGP_Interpolate_Type {
	/* Traditional Linear Interpolation */
	GP_IPO_LINEAR   = 0,
	
	/* CurveMap Defined Interpolation */
	GP_IPO_CURVEMAP = 1,
	
	/* Easing Equations */
	GP_IPO_BACK = 3,
	GP_IPO_BOUNCE = 4,
	GP_IPO_CIRC = 5,
	GP_IPO_CUBIC = 6,
	GP_IPO_ELASTIC = 7,
	GP_IPO_EXPO = 8,
	GP_IPO_QUAD = 9,
	GP_IPO_QUART = 10,
	GP_IPO_QUINT = 11,
	GP_IPO_SINE = 12,
} eGP_Interpolate_Type;


/* *************************************************************** */
/* Transform Orientations */

typedef struct TransformOrientation {
	struct TransformOrientation *next, *prev;
	char name[64];	/* MAX_NAME */
	float mat[3][3];
	int pad;
} TransformOrientation;

/* *************************************************************** */
/* Unified Paint Settings
 */

/* These settings can override the equivalent fields in the active
 * Brush for any paint mode; the flag field controls whether these
 * values are used */
typedef struct UnifiedPaintSettings {
	/* unified radius of brush in pixels */
	int size;

	/* unified radius of brush in Blender units */
	float unprojected_radius;

	/* unified strength of brush */
	float alpha;

	/* unified brush weight, [0, 1] */
	float weight;

	/* unified brush color */
	float rgb[3];
	/* unified brush secondary color */
	float secondary_rgb[3];

	/* user preferences for sculpt and paint */
	int flag;

	/* rake rotation */

	/* record movement of mouse so that rake can start at an intuitive angle */
	float last_rake[2];
	float last_rake_angle;
	
	int last_stroke_valid;
	float average_stroke_accum[3];
	int average_stroke_counter;
	

	float brush_rotation;
	float brush_rotation_sec;

	/*********************************************************************************
	 *  all data below are used to communicate with cursor drawing and tex sampling  *
	 *********************************************************************************/
	int anchored_size;

	float overlap_factor; /* normalization factor due to accumulated value of curve along spacing.
	                       * Calculated when brush spacing changes to dampen strength of stroke
	                       * if space attenuation is used*/
	char draw_inverted;
	/* check is there an ongoing stroke right now */
	char stroke_active;

	char draw_anchored;
	char do_linear_conversion;

	/* store last location of stroke or whether the mesh was hit. Valid only while stroke is active */
	float last_location[3];
	int last_hit;

	float anchored_initial_mouse[2];

	/* radius of brush, premultiplied with pressure.
	 * In case of anchored brushes contains the anchored radius */
	float pixel_radius;

	/* drawing pressure */
	float size_pressure_value;

	/* position of mouse, used to sample the texture */
	float tex_mouse[2];

	/* position of mouse, used to sample the mask texture */
	float mask_tex_mouse[2];

	/* ColorSpace cache to avoid locking up during sampling */
	struct ColorSpace *colorspace;
} UnifiedPaintSettings;

typedef enum {
	UNIFIED_PAINT_SIZE  = (1 << 0),
	UNIFIED_PAINT_ALPHA = (1 << 1),
	UNIFIED_PAINT_WEIGHT = (1 << 5),
	UNIFIED_PAINT_COLOR = (1 << 6),

	/* only used if unified size is enabled, mirrors the brush flags
	 * BRUSH_LOCK_SIZE and BRUSH_SIZE_PRESSURE */
	UNIFIED_PAINT_BRUSH_LOCK_SIZE = (1 << 2),
	UNIFIED_PAINT_BRUSH_SIZE_PRESSURE   = (1 << 3),

	/* only used if unified alpha is enabled, mirrors the brush flag
	 * BRUSH_ALPHA_PRESSURE */
	UNIFIED_PAINT_BRUSH_ALPHA_PRESSURE  = (1 << 4)
} UnifiedPaintSettingsFlags;


typedef struct CurvePaintSettings {
	char curve_type;
	char flag;
	char depth_mode;
	char surface_plane;
	char fit_method;
	char pad;
	short error_threshold;
	float radius_min, radius_max;
	float radius_taper_start, radius_taper_end;
	float surface_offset;
	float corner_angle;
} CurvePaintSettings;

/* CurvePaintSettings.flag */
enum {
	CURVE_PAINT_FLAG_CORNERS_DETECT             = (1 << 0),
	CURVE_PAINT_FLAG_PRESSURE_RADIUS            = (1 << 1),
	CURVE_PAINT_FLAG_DEPTH_STROKE_ENDPOINTS     = (1 << 2),
	CURVE_PAINT_FLAG_DEPTH_STROKE_OFFSET_ABS    = (1 << 3),
};

/* CurvePaintSettings.fit_method */
enum {
	CURVE_PAINT_FIT_METHOD_REFIT            = 0,
	CURVE_PAINT_FIT_METHOD_SPLIT            = 1,
};

/* CurvePaintSettings.depth_mode */
enum {
	CURVE_PAINT_PROJECT_CURSOR              = 0,
	CURVE_PAINT_PROJECT_SURFACE             = 1,
};

/* CurvePaintSettings.surface_plane */
enum {
	CURVE_PAINT_SURFACE_PLANE_NORMAL_VIEW           = 0,
	CURVE_PAINT_SURFACE_PLANE_NORMAL_SURFACE        = 1,
	CURVE_PAINT_SURFACE_PLANE_VIEW                  = 2,
};


/* *************************************************************** */
/* Stats */

/* Stats for Meshes */
typedef struct MeshStatVis {
	char type;
	char _pad1[2];

	/* overhang */
	char  overhang_axis;
	float overhang_min, overhang_max;

	/* thickness */
	float thickness_min, thickness_max;
	char thickness_samples;
	char _pad2[3];

	/* distort */
	float distort_min, distort_max;

	/* sharp */
	float sharp_min, sharp_max;
} MeshStatVis;


/* *************************************************************** */
/* Tool Settings */

typedef struct ToolSettings {
	VPaint *vpaint;		/* vertex paint */
	VPaint *wpaint;		/* weight paint */
	Sculpt *sculpt;
	UvSculpt *uvsculpt;	/* uv smooth */
	
	/* Vertex group weight - used only for editmode, not weight
	 * paint */
	float vgroup_weight;

	float doublimit;	/* remove doubles limit */
	float normalsize;	/* size of normals */
	short automerge;

	/* Selection Mode for Mesh */
	short selectmode;

	/* UV Calculation */
	char unwrapper;
	char uvcalc_flag;
	char uv_flag;
	char uv_selectmode;

	float uvcalc_margin;

	/* Auto-IK */
	short autoik_chainlen;  /* runtime only */

	/* Grease Pencil */
	char gpencil_flags;		/* flags/options for how the tool works */
	char gpencil_src;		/* for main 3D view Grease Pencil, where data comes from */

	char gpencil_v3d_align; /* stroke placement settings: 3D View */
	char gpencil_v2d_align; /*                          : General 2D Editor */
	char gpencil_seq_align; /*                          : Sequencer Preview */
	char gpencil_ima_align; /*                          : Image Editor */
	
	/* Grease Pencil Sculpt */
	struct GP_BrushEdit_Settings gp_sculpt;
	
	/* Grease Pencil Interpolation Tool(s) */
	struct GP_Interpolate_Settings gp_interpolate;
	
	/* Grease Pencil Drawing Brushes (bGPDbrush) */
	ListBase gp_brushes; 

	/* Image Paint (8 byttse aligned please!) */
	struct ImagePaintSettings imapaint;

	/* Particle Editing */
	struct ParticleEditSettings particle;
	
	/* Transform Proportional Area of Effect */
	float proportional_size;

	/* Select Group Threshold */
	float select_thresh;

	/* Auto-Keying Mode */
	short autokey_mode, autokey_flag;	/* defines in DNA_userdef_types.h */
	char keyframe_type;                 /* keyframe type (see DNA_curve_types.h) */

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
	char snap_mode, snap_node_mode;
	char snap_uv_mode;
	short snap_flag, snap_target;
	short proportional, prop_mode;
	char proportional_objects; /* proportional edit, object mode */
	char proportional_mask; /* proportional edit, mask editing */
	char proportional_action; /* proportional edit, action editor */
	char proportional_fcurve; /* proportional edit, graph editor */
	char lock_markers; /* lock marker editing */
	char pad4[5];

	char auto_normalize; /*auto normalizing mode in wpaint*/
	char multipaint; /* paint multiple bones in wpaint */
	char weightuser;
	char vgroupsubset; /* subset selection filter in wpaint */

	/* UV painting */
	int use_uv_sculpt;
	int uv_sculpt_settings;
	int uv_sculpt_tool;
	int uv_relax_method;
	/* XXX: these sculpt_paint_* fields are deprecated, use the
	 * unified_paint_settings field instead! */
	short sculpt_paint_settings DNA_DEPRECATED;	short pad5;
	int sculpt_paint_unified_size DNA_DEPRECATED;
	float sculpt_paint_unified_unprojected_radius DNA_DEPRECATED;
	float sculpt_paint_unified_alpha DNA_DEPRECATED;

	/* Unified Paint Settings */
	struct UnifiedPaintSettings unified_paint_settings;

	struct CurvePaintSettings curve_paint_settings;

	struct MeshStatVis statvis;
} ToolSettings;

/* *************************************************************** */
/* Assorted Scene Data */

/* ------------------------------------------- */
/* Stats (show in Info header) */

typedef struct bStats {
	/* scene totals for visible layers */
	int totobj, totlamp, totobjsel, totcurve, totmesh, totarmature;
	int totvert, totface;
} bStats;

/* ------------------------------------------- */
/* Unit Settings */

typedef struct UnitSettings {
	/* Display/Editing unit options for each scene */
	float scale_length; /* maybe have other unit conversions? */
	char system; /* imperial, metric etc */
	char system_rotation; /* not implemented as a proper unit system yet */
	short flag;
} UnitSettings;

/* ------------------------------------------- */
/* Global/Common Physics Settings */

typedef struct PhysicsSettings {
	float gravity[3];
	int flag, quick_cache_step, rt;
} PhysicsSettings;

/* ------------------------------------------- */
/* Safe Area options used in Camera View & VSE
 */
typedef struct DisplaySafeAreas {
	/* each value represents the (x,y) margins as a multiplier.
	 * 'center' in this context is just the name for a different kind of safe-area */

	float title[2];		/* Title Safe */
	float action[2];	/* Image/Graphics Safe */

	/* use for alternate aspect ratio */
	float title_center[2];
	float action_center[2];
} DisplaySafeAreas;

/* *************************************************************** */
/* Scene ID-Block */

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
	
	char use_nodes;
	char pad[1];
	
	struct bNodeTree *nodetree;
	
	struct Editing *ed;								/* sequence editor data is allocated here */
	
	struct ToolSettings *toolsettings;		/* default allocated now */
	struct SceneStats *stats;				/* default allocated now */
	struct DisplaySafeAreas safe_areas;

	/* migrate or replace? depends on some internal things... */
	/* no, is on the right place (ton) */
	struct RenderData r;
	struct AudioData audio;
	
	ListBase markers;
	ListBase transform_spaces;
	
	void *sound_scene;
	void *playback_handle;
	void *sound_scrub_handle;
	void *speaker_handles;
	
	void *fps_info;					/* (runtime) info/cache used for presenting playback framerate info to the user */
	
	/* none of the dependency graph  vars is mean to be saved */
	struct Depsgraph *depsgraph;
	void *pad1;
	struct  DagForest *theDag;
	short dagflags;
	short pad3;

	/* User-Defined KeyingSets */
	int active_keyingset;			/* index of the active KeyingSet. first KeyingSet has index 1, 'none' active is 0, 'add new' is -1 */
	ListBase keyingsets;			/* KeyingSets for this scene */
	
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

	uint64_t customdata_mask;	/* XXX. runtime flag for drawing, actually belongs in the window, only used by BKE_object_handle_update() */
	uint64_t customdata_mask_modal; /* XXX. same as above but for temp operator use (gl renders) */

	/* Color Management */
	ColorManagedViewSettings view_settings;
	ColorManagedDisplaySettings display_settings;
	ColorManagedColorspaceSettings sequencer_colorspace_settings;
	
	/* RigidBody simulation world+settings */
	struct RigidBodyWorld *rigidbody_world;

	struct PreviewImage *preview;
} Scene;

/* **************** RENDERDATA ********************* */

/* flag */
	/* use preview range */
#define SCER_PRV_RANGE	(1<<0)
#define SCER_LOCK_FRAME_SELECTION	(1<<1)
	/* show/use subframes (for checking motion blur) */
#define SCER_SHOW_SUBFRAME	(1<<3)

/* mode (int now) */
#define R_OSA			0x0001
#define R_SHADOW		0x0002
#define R_GAMMA			0x0004
#define R_ORTHO			0x0008
#define R_ENVMAP		0x0010
#define R_EDGE			0x0020
#define R_FIELDS		0x0040
#define R_FIELDSTILL	0x0080
/*#define R_RADIO			0x0100 */ /* deprecated */
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

#define R_SPEED				0x100000
#define R_SSS				0x200000
#define R_NO_OVERWRITE		0x400000  /* skip existing files */
#define R_TOUCH				0x800000  /* touch files before rendering */
#define R_SIMPLIFY			0x1000000
#define R_EDGE_FRS			0x2000000 /* R_EDGE reserved for Freestyle */
#define R_PERSISTENT_DATA	0x4000000 /* keep data around for re-render */
#define R_USE_WS_SHADING	0x8000000 /* use world space interpretation of lighting data */

/* seq_flag */
// #define R_SEQ_GL_PREV 1  // UNUSED, we just use setting from seq_prev_type now.
// #define R_SEQ_GL_REND 2  // UNUSED, opengl render has its own operator now.
#define R_SEQ_SOLID_TEX 4

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
#define R_RAYSTRUCTURE_BLIBVH			2	/* removed */
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
#define R_BUTS_PREVIEW		0x0008
#define R_EXTENSION			0x0010
#define R_MATNODE_PREVIEW	0x0020
#define R_DOCOMP			0x0040
#define R_COMP_CROP			0x0080
#define R_FREE_IMAGE		0x0100
#define R_SINGLE_LAYER		0x0200
#define R_EXR_TILE_FILE		0x0400
/* #define R_COMP_FREE			0x0800 */
#define R_NO_IMAGE_LOAD		0x1000
#define R_NO_TEX			0x2000
#define R_NO_FRAME_UPDATE	0x4000
#define R_FULL_SAMPLE		0x8000
/* #define R_DEPRECATED		0x10000 */
/* #define R_RECURS_PROTECTION	0x20000 */
#define R_TEXNODE_PREVIEW	0x40000
#define R_VIEWPORT_PREVIEW	0x80000
#define R_EXR_CACHE_FILE	0x100000
#define R_MULTIVIEW			0x200000

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
#define R_STAMP_STRIPMETA	0x1000
#define R_STAMP_MEMORY		0x2000
#define R_STAMP_HIDE_LABELS	0x4000
#define R_STAMP_ALL (R_STAMP_TIME|R_STAMP_FRAME|R_STAMP_DATE|R_STAMP_CAMERA|R_STAMP_SCENE| \
                     R_STAMP_NOTE|R_STAMP_MARKER|R_STAMP_FILENAME|R_STAMP_SEQSTRIP|        \
                     R_STAMP_RENDERTIME|R_STAMP_CAMERALENS|R_STAMP_MEMORY|                 \
                     R_STAMP_HIDE_LABELS)

/* alphamode */
#define R_ADDSKY		0
#define R_ALPHAPREMUL	1
/*#define R_ALPHAKEY		2*/ /* deprecated, shouldn't be used */

/* color_mgt_flag */
enum {
	R_COLOR_MANAGEMENT              = (1 << 0),  /* deprecated, should only be used in versioning code only */
	/*R_COLOR_MANAGEMENT_PREDIVIDE    = (1 << 1)*/  /* deprecated, shouldn't be used */
};

#ifdef DNA_DEPRECATED
/* subimtype, flag options for imtype */
enum {
	R_OPENEXR_HALF	= 1,  /*deprecated*/
	R_OPENEXR_ZBUF	= 2,  /*deprecated*/
	R_PREVIEW_JPG	= 4,  /*deprecated*/
	R_CINEON_LOG	= 8,  /*deprecated*/
	R_TIFF_16BIT	= 16, /*deprecated*/

	R_JPEG2K_12BIT			=     32,  /* Jpeg2000 */                    /*deprecated*/
	R_JPEG2K_16BIT			=     64,                                    /*deprecated*/
	R_JPEG2K_YCC			=     128,  /* when disabled use RGB */      /*deprecated*/
	R_JPEG2K_CINE_PRESET	=     256,                                   /*deprecated*/
	R_JPEG2K_CINE_48FPS		=     512,                                   /*deprecated*/
};
#endif

/* bake_mode: same as RE_BAKE_xxx defines */
/* bake_flag: */
#define R_BAKE_CLEAR		1
#define R_BAKE_OSA			2
#define R_BAKE_TO_ACTIVE	4
#define R_BAKE_NORMALIZE	8
#define R_BAKE_MULTIRES		16
#define R_BAKE_LORES_MESH	32
#define R_BAKE_VCOL			64
#define R_BAKE_USERSCALE	128
#define R_BAKE_CAGE			256
#define R_BAKE_SPLIT_MAT	512
#define R_BAKE_AUTO_NAME	1024

/* bake_normal_space */
#define R_BAKE_SPACE_CAMERA	 0
#define R_BAKE_SPACE_WORLD	 1
#define R_BAKE_SPACE_OBJECT	 2
#define R_BAKE_SPACE_TANGENT 3

/* simplify_flag */
#define R_SIMPLE_NO_TRIANGULATE		1

/* line_thickness_mode */
#define R_LINE_THICKNESS_ABSOLUTE 1
#define R_LINE_THICKNESS_RELATIVE 2

/* sequencer seq_prev_type seq_rend_type */

/* scene->r.engine (scene.c) */
extern const char *RE_engine_id_BLENDER_RENDER;
extern const char *RE_engine_id_BLENDER_GAME;
extern const char *RE_engine_id_CYCLES;

/* **************** SCENE ********************* */

/* note that much higher maxframes give imprecise sub-frames, see: T46859 */
/* Current precision is 16 for the sub-frames closer to MAXFRAME. */

/* for general use */
#define MAXFRAME	1048574
#define MAXFRAMEF	1048574.0f

#define MINFRAME	0
#define MINFRAMEF	0.0f

/* (minimum frame number for current-frame) */
#define MINAFRAME	-1048574
#define MINAFRAMEF	-1048574.0f

/* depricate this! */
#define TESTBASE(v3d, base)  (                                                \
	((base)->flag & SELECT) &&                                                \
	((base)->lay & v3d->lay) &&                                               \
	(((base)->object->restrictflag & OB_RESTRICT_VIEW) == 0))
#define TESTBASELIB(v3d, base)  (                                             \
	((base)->flag & SELECT) &&                                                \
	((base)->lay & v3d->lay) &&                                               \
	((base)->object->id.lib == NULL) &&                                       \
	(((base)->object->restrictflag & OB_RESTRICT_VIEW) == 0))
#define TESTBASELIB_BGMODE(v3d, scene, base)  (                               \
	((base)->flag & SELECT) &&                                                \
	((base)->lay & (v3d ? v3d->lay : scene->lay)) &&                          \
	((base)->object->id.lib == NULL) &&                                       \
	(((base)->object->restrictflag & OB_RESTRICT_VIEW) == 0))
#define BASE_EDITABLE_BGMODE(v3d, scene, base)  (                             \
	((base)->lay & (v3d ? v3d->lay : scene->lay)) &&                          \
	((base)->object->id.lib == NULL) &&                                       \
	(((base)->object->restrictflag & OB_RESTRICT_VIEW) == 0))
#define BASE_SELECTABLE(v3d, base)  (                                         \
	(base->lay & v3d->lay) &&                                                 \
	(base->object->restrictflag & (OB_RESTRICT_SELECT | OB_RESTRICT_VIEW)) == 0)
#define BASE_VISIBLE(v3d, base)  (                                            \
	(base->lay & v3d->lay) &&                                                 \
	(base->object->restrictflag & OB_RESTRICT_VIEW) == 0)
#define BASE_VISIBLE_BGMODE(v3d, scene, base)  (                              \
	(base->lay & (v3d ? v3d->lay : scene->lay)) &&                            \
	(base->object->restrictflag & OB_RESTRICT_VIEW) == 0)

#define FIRSTBASE		scene->base.first
#define LASTBASE		scene->base.last
#define BASACT			(scene->basact)
#define OBACT			(BASACT ? BASACT->object: NULL)

#define V3D_CAMERA_LOCAL(v3d) ((!(v3d)->scenelock && (v3d)->camera) ? (v3d)->camera : NULL)
#define V3D_CAMERA_SCENE(scene, v3d) ((!(v3d)->scenelock && (v3d)->camera) ? (v3d)->camera : (scene)->camera)

#define CFRA            (scene->r.cfra)
#define SUBFRA          (scene->r.subframe)
#define SFRA            (scene->r.sfra)
#define EFRA            (scene->r.efra)
#define PRVRANGEON      (scene->r.flag & SCER_PRV_RANGE)
#define PSFRA           ((PRVRANGEON) ? (scene->r.psfra) : (scene->r.sfra))
#define PEFRA           ((PRVRANGEON) ? (scene->r.pefra) : (scene->r.efra))
#define FRA2TIME(a)     ((((double) scene->r.frs_sec_base) * (double)(a)) / (double)scene->r.frs_sec)
#define TIME2FRA(a)     ((((double) scene->r.frs_sec) * (double)(a)) / (double)scene->r.frs_sec_base)
#define FPS              (((double) scene->r.frs_sec) / (double)scene->r.frs_sec_base)

/* base->flag is in DNA_object_types.h */

/* toolsettings->snap_flag */
#define SCE_SNAP				1
#define SCE_SNAP_ROTATE			2
#define SCE_SNAP_PEEL_OBJECT	4
#define SCE_SNAP_PROJECT		8
#define SCE_SNAP_NO_SELF		16
#define SCE_SNAP_ABS_GRID		32

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
#define SCE_SNAP_MODE_NODE_X	5
#define SCE_SNAP_MODE_NODE_Y	6
#define SCE_SNAP_MODE_NODE_XY	7
#define SCE_SNAP_MODE_GRID		8

/* toolsettings->selectmode */
#define SCE_SELECT_VERTEX	1 /* for mesh */
#define SCE_SELECT_EDGE		2
#define SCE_SELECT_FACE		4

/* toolsettings->statvis->type */
#define SCE_STATVIS_OVERHANG	0
#define SCE_STATVIS_THICKNESS	1
#define SCE_STATVIS_INTERSECT	2
#define SCE_STATVIS_DISTORT		3
#define SCE_STATVIS_SHARP		4

/* toolsettings->particle.selectmode for particles */
#define SCE_SELECT_PATH		1
#define SCE_SELECT_POINT	2
#define SCE_SELECT_END		4

/* toolsettings->prop_mode (proportional falloff) */
#define PROP_SMOOTH            0
#define PROP_SPHERE            1
#define PROP_ROOT              2
#define PROP_SHARP             3
#define PROP_LIN               4
#define PROP_CONST             5
#define PROP_RANDOM            6
#define PROP_INVSQUARE         7
#define PROP_MODE_MAX          8

/* toolsettings->proportional */
#define PROP_EDIT_OFF			0
#define PROP_EDIT_ON			1
#define PROP_EDIT_CONNECTED		2
#define PROP_EDIT_PROJECTED		3

/* toolsettings->weightuser */
enum {
	OB_DRAW_GROUPUSER_NONE      = 0,
	OB_DRAW_GROUPUSER_ACTIVE    = 1,
	OB_DRAW_GROUPUSER_ALL       = 2
};

/* toolsettings->vgroupsubset */
/* object_vgroup.c */
typedef enum eVGroupSelect {
	WT_VGROUP_ALL = 0,
	WT_VGROUP_ACTIVE = 1,
	WT_VGROUP_BONE_SELECT = 2,
	WT_VGROUP_BONE_DEFORM = 3,
	WT_VGROUP_BONE_DEFORM_OFF = 4
} eVGroupSelect;

#define WT_VGROUP_MASK_ALL \
	((1 << WT_VGROUP_ACTIVE) | \
	 (1 << WT_VGROUP_BONE_SELECT) | \
	 (1 << WT_VGROUP_BONE_DEFORM) | \
	 (1 << WT_VGROUP_BONE_DEFORM_OFF) | \
	 (1 << WT_VGROUP_ALL))


/* sce->flag */
#define SCE_DS_SELECTED			(1<<0)
#define SCE_DS_COLLAPSED		(1<<1)
#define SCE_NLA_EDIT_ON			(1<<2)
#define SCE_FRAME_DROP			(1<<3)
#define SCE_KEYS_NO_SELONLY	    (1<<4)


	/* return flag BKE_scene_base_iter_next functions */
/* #define F_ERROR			-1 */  /* UNUSED */
#define F_START			0
#define F_SCENE			1
#define F_DUPLI			3

/* audio->flag */
#define AUDIO_MUTE                (1<<0)
#define AUDIO_SYNC                (1<<1)
#define AUDIO_SCRUB		          (1<<2)
#define AUDIO_VOLUME_ANIMATED     (1<<3)

enum {
#ifdef DNA_DEPRECATED
	FFMPEG_MULTIPLEX_AUDIO  = 1,  /* deprecated, you can choose none as audiocodec now */
#endif
	FFMPEG_AUTOSPLIT_OUTPUT = 2,
	FFMPEG_LOSSLESS_OUTPUT  = 4,
	FFMPEG_USE_MAX_B_FRAMES = (1 << 3),
};

/* Paint.flags */
typedef enum {
	PAINT_SHOW_BRUSH = (1 << 0),
	PAINT_FAST_NAVIGATE = (1 << 1),
	PAINT_SHOW_BRUSH_ON_SURFACE = (1 << 2),
	PAINT_USE_CAVITY_MASK = (1 << 3)
} PaintFlags;

/* Paint.symmetry_flags
 * (for now just a duplicate of sculpt symmetry flags) */
typedef enum SymmetryFlags {
	PAINT_SYMM_X = (1 << 0),
	PAINT_SYMM_Y = (1 << 1),
	PAINT_SYMM_Z = (1 << 2),
	PAINT_SYMMETRY_FEATHER = (1 << 3),
	PAINT_TILE_X = (1 << 4),
	PAINT_TILE_Y = (1 << 5),
	PAINT_TILE_Z = (1 << 6),
} SymmetryFlags;

#define PAINT_SYMM_AXIS_ALL (PAINT_SYMM_X | PAINT_SYMM_Y | PAINT_SYMM_Z)

/* Sculpt.flags */
/* These can eventually be moved to paint flags? */
typedef enum SculptFlags {
#ifdef DNA_DEPRECATED
	/* deprecated, part of paint struct symmetry_flags now */
	SCULPT_SYMM_X = (1 << 0),
	SCULPT_SYMM_Y = (1 << 1),
	SCULPT_SYMM_Z = (1 << 2),
#endif

	SCULPT_LOCK_X = (1 << 3),
	SCULPT_LOCK_Y = (1 << 4),
	SCULPT_LOCK_Z = (1 << 5),
	/* deprecated, part of paint struct symmetry_flags now */
	SCULPT_SYMMETRY_FEATHER = (1 << 6),

	SCULPT_USE_OPENMP = (1 << 7),
	SCULPT_ONLY_DEFORM = (1 << 8),
	SCULPT_SHOW_DIFFUSE = (1 << 9),

	/* If set, the mesh will be drawn with smooth-shading in
	 * dynamic-topology mode */
	SCULPT_DYNTOPO_SMOOTH_SHADING = (1 << 10),

	/* If set, dynamic-topology brushes will subdivide short edges */
	SCULPT_DYNTOPO_SUBDIVIDE = (1 << 12),
	/* If set, dynamic-topology brushes will collapse short edges */
	SCULPT_DYNTOPO_COLLAPSE = (1 << 11),

	/* If set, dynamic-topology detail size will be constant in object space */
	SCULPT_DYNTOPO_DETAIL_CONSTANT = (1 << 13),
	SCULPT_DYNTOPO_DETAIL_BRUSH = (1 << 14),
} SculptFlags;

typedef enum ImagePaintMode {
	IMAGEPAINT_MODE_MATERIAL, /* detect texture paint slots from the material */
	IMAGEPAINT_MODE_IMAGE,    /* select texture paint image directly */
} ImagePaintMode;

/* ImagePaintSettings.flag */
#define IMAGEPAINT_DRAWING				1
// #define IMAGEPAINT_DRAW_TOOL			2 // deprecated
// #define IMAGEPAINT_DRAW_TOOL_DRAWING	4 // deprecated

/* projection painting only */
#define IMAGEPAINT_PROJECT_XRAY			(1 << 4)
#define IMAGEPAINT_PROJECT_BACKFACE		(1 << 5)
#define IMAGEPAINT_PROJECT_FLAT			(1 << 6)
#define IMAGEPAINT_PROJECT_LAYER_CLONE	(1 << 7)
#define IMAGEPAINT_PROJECT_LAYER_STENCIL	(1 << 8)
#define IMAGEPAINT_PROJECT_LAYER_STENCIL_INV	(1 << 9)


#define IMAGEPAINT_MISSING_UVS       (1 << 0)
#define IMAGEPAINT_MISSING_MATERIAL  (1 << 1)
#define IMAGEPAINT_MISSING_TEX       (1 << 2)
#define IMAGEPAINT_MISSING_STENCIL   (1 << 3)

/* toolsettings->uvcalc_flag */
#define UVCALC_FILLHOLES			1
#define UVCALC_NO_ASPECT_CORRECT	2	/* would call this UVCALC_ASPECT_CORRECT, except it should be default with old file */
#define UVCALC_TRANSFORM_CORRECT	4	/* adjust UV's while transforming to avoid distortion */
#define UVCALC_USESUBSURF			8	/* Use mesh data after subsurf to compute UVs*/

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
#define EDGE_MODE_TAG_FREESTYLE			5

/* toolsettings->gpencil_flags */
typedef enum eGPencil_Flags {
	/* "Continuous Drawing" - The drawing operator enters a mode where multiple strokes can be drawn */
	GP_TOOL_FLAG_PAINTSESSIONS_ON       = (1 << 0),
	/* When creating new frames, the last frame gets used as the basis for the new one */
	GP_TOOL_FLAG_RETAIN_LAST            = (1 << 1),
	/* Add the strokes below all strokes in the layer */
	GP_TOOL_FLAG_PAINT_ONBACK = (1 << 2)
} eGPencil_Flags;

/* toolsettings->gpencil_src */
typedef enum eGPencil_Source_3D {
	GP_TOOL_SOURCE_SCENE    = 0,
	GP_TOOL_SOURCE_OBJECT   = 1
} eGPencil_Source_3d;

/* toolsettings->gpencil_*_align - Stroke Placement mode flags */
typedef enum eGPencil_Placement_Flags {
	/* New strokes are added in viewport/data space (i.e. not screen space) */
	GP_PROJECT_VIEWSPACE    = (1 << 0),
	
	/* Viewport space, but relative to render canvas (Sequencer Preview Only) */
	GP_PROJECT_CANVAS       = (1 << 1),
	
	/* Project into the screen's Z values */
	GP_PROJECT_DEPTH_VIEW	= (1 << 2),
	GP_PROJECT_DEPTH_STROKE = (1 << 3),
	
	/* "Use Endpoints" */
	GP_PROJECT_DEPTH_STROKE_ENDPOINTS = (1 << 4),
} eGPencil_Placement_Flags;

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

#endif  /* __DNA_SCENE_TYPES_H__ */
