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
/** \file DNA_space_types.h
 *  \ingroup DNA
 *  \since mar-2001
 *  \author nzc
 *
 * Structs for each of space type in the user interface.
 */

#ifndef __DNA_SPACE_TYPES_H__
#define __DNA_SPACE_TYPES_H__

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_color_types.h"        /* for Histogram */
#include "DNA_vec_types.h"
#include "DNA_outliner_types.h"     /* for TreeStoreElem */
#include "DNA_image_types.h"        /* ImageUser */
#include "DNA_movieclip_types.h"    /* MovieClipUser */
#include "DNA_sequence_types.h"     /* SequencerScopes */
#include "DNA_node_types.h"         /* for bNodeInstanceKey */
/* Hum ... Not really nice... but needed for spacebuts. */
#include "DNA_view2d_types.h"

struct ID;
struct Text;
struct Script;
struct bSound;
struct ImBuf;
struct Image;
struct Scopes;
struct Histogram;
struct SpaceIpo;
struct BlendHandle;
struct bNodeTree;
struct uiBlock;
struct FileList;
struct bGPdata;
struct bDopeSheet;
struct FileSelectParams;
struct FileLayout;
struct bScreen;
struct Scene;
struct wmOperator;
struct wmTimer;
struct MovieClip;
struct MovieClipScopes;
struct Mask;
struct GHash;
struct BLI_mempool;


/* SpaceLink (Base) ==================================== */

/**
 * The base structure all the other spaces
 * are derived (implicitly) from. Would be
 * good to make this explicit.
 */
typedef struct SpaceLink {
	struct SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;
	float blockscale DNA_DEPRECATED;        /* XXX make deprecated */
	short blockhandler[8]  DNA_DEPRECATED;  /* XXX make deprecated */
} SpaceLink;


/* Space Info ========================================== */

/* Info Header */
typedef struct SpaceInfo {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;
	float blockscale DNA_DEPRECATED;
	short blockhandler[8]  DNA_DEPRECATED;      /* XXX make deprecated */
	
	char rpt_mask;
	char pad[7];
} SpaceInfo;

/* SpaceInfo.rpt_mask */
typedef enum eSpaceInfo_RptMask {
	INFO_RPT_DEBUG  = (1 << 0),
	INFO_RPT_INFO   = (1 << 1),
	INFO_RPT_OP     = (1 << 2),
	INFO_RPT_WARN   = (1 << 3),
	INFO_RPT_ERR    = (1 << 4),
} eSpaceInfo_RptMask;


/* Properties Editor ==================================== */

/* Properties Editor */
typedef struct SpaceButs {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;
	float blockscale DNA_DEPRECATED;
	
	short blockhandler[8]  DNA_DEPRECATED;

	View2D v2d DNA_DEPRECATED;                      /* deprecated, copied to region */
	
	short mainb, mainbo, mainbuser; /* context tabs */
	short re_align, align;          /* align for panels */
	short preview;                  /* preview is signal to refresh */
	/* texture context selector (material, lamp, particles, world, other)*/
	short texture_context, texture_context_prev;
	char flag, pad[7];
	
	void *path;                     /* runtime */
	int pathflag, dataicon;         /* runtime */
	ID *pinid;

	void *texuser;
} SpaceButs;

/* button defines (deprecated) */
/* warning: the values of these defines are used in sbuts->tabs[8] */
/* sbuts->mainb new */
#define CONTEXT_SCENE   0
#define CONTEXT_OBJECT  1
#define CONTEXT_TYPES   2
#define CONTEXT_SHADING 3
#define CONTEXT_EDITING 4
#define CONTEXT_SCRIPT  5
#define CONTEXT_LOGIC   6

/* sbuts->mainb old (deprecated) */
#define BUTS_VIEW           0
#define BUTS_LAMP           1
#define BUTS_MAT            2
#define BUTS_TEX            3
#define BUTS_ANIM           4
#define BUTS_WORLD          5
#define BUTS_RENDER         6
#define BUTS_EDIT           7
#define BUTS_GAME           8
#define BUTS_FPAINT         9
#define BUTS_RADIO          10
#define BUTS_SCRIPT         11
#define BUTS_SOUND          12
#define BUTS_CONSTRAINT     13
#define BUTS_EFFECTS        14

/* buts->mainb new */
typedef enum eSpaceButtons_Context {
	BCONTEXT_RENDER = 0,
	BCONTEXT_SCENE = 1,
	BCONTEXT_WORLD = 2,
	BCONTEXT_OBJECT = 3,
	BCONTEXT_DATA = 4,
	BCONTEXT_MATERIAL = 5,
	BCONTEXT_TEXTURE = 6,
	BCONTEXT_PARTICLE = 7,
	BCONTEXT_PHYSICS = 8,
	BCONTEXT_BONE = 9,
	BCONTEXT_MODIFIER = 10,
	BCONTEXT_CONSTRAINT = 11,
	BCONTEXT_BONE_CONSTRAINT = 12,
	BCONTEXT_RENDER_LAYER = 13,
	
	/* always as last... */
	BCONTEXT_TOT
} eSpaceButtons_Context;

/* sbuts->flag */
typedef enum eSpaceButtons_Flag {
	SB_PRV_OSA = (1 << 0),
	SB_PIN_CONTEXT = (1 << 1),
	/* SB_WORLD_TEX = (1 << 2), */ /* not used anymore */
	/* SB_BRUSH_TEX = (1 << 3), */ /* not used anymore */
	SB_TEX_USER_LIMITED = (1 << 3), /* Do not add materials, particles, etc. in TemplateTextureUser list. */
	SB_SHADING_CONTEXT = (1 << 4),
} eSpaceButtons_Flag;

/* sbuts->texture_context */
typedef enum eSpaceButtons_Texture_Context {
	SB_TEXC_MATERIAL = 0,
	SB_TEXC_WORLD = 1,
	SB_TEXC_LAMP = 2,
	SB_TEXC_PARTICLES = 3,
	SB_TEXC_OTHER = 4,
	SB_TEXC_LINESTYLE = 5,
} eSpaceButtons_Texture_Context;

/* sbuts->align */
typedef enum eSpaceButtons_Align {
	BUT_FREE = 0,
	BUT_HORIZONTAL = 1,
	BUT_VERTICAL = 2,
	BUT_AUTO = 3,
} eSpaceButtons_Align;

/* sbuts->scaflag */
#define BUTS_SENS_SEL           1
#define BUTS_SENS_ACT           2
#define BUTS_SENS_LINK          4
#define BUTS_CONT_SEL           8
#define BUTS_CONT_ACT           16
#define BUTS_CONT_LINK          32
#define BUTS_ACT_SEL            64
#define BUTS_ACT_ACT            128
#define BUTS_ACT_LINK           256
#define BUTS_SENS_STATE         512
#define BUTS_ACT_STATE          1024
#define BUTS_CONT_INIT_STATE    2048


/* Outliner =============================================== */

/* Outliner */
typedef struct SpaceOops {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;
	float blockscale DNA_DEPRECATED;
	short blockhandler[8]  DNA_DEPRECATED;

	View2D v2d DNA_DEPRECATED;  /* deprecated, copied to region */
	
	ListBase tree;
	
	/* treestore is an ordered list of TreeStoreElem's from outliner tree;
	 * Note that treestore may contain duplicate elements if element
	 * is used multiple times in outliner tree (e. g. linked objects)
	 * Also note that BLI_mempool can not be read/written in DNA directly,
	 * therefore readfile.c/writefile.c linearize treestore into TreeStore structure
	 */
	struct BLI_mempool *treestore;
	
	/* search stuff */
	char search_string[32];
	struct TreeStoreElem search_tse;

	short flag, outlinevis, storeflag, search_flags;
	
	/* pointers to treestore elements, grouped by (id, type, nr) in hashtable for faster searching */
	void *treehash;
} SpaceOops;


/* SpaceOops->flag */
typedef enum eSpaceOutliner_Flag {
	SO_TESTBLOCKS           = (1 << 0),
	SO_NEWSELECTED          = (1 << 1),
	SO_HIDE_RESTRICTCOLS    = (1 << 2),
	SO_HIDE_KEYINGSETINFO   = (1 << 3),
} eSpaceOutliner_Flag;

/* SpaceOops->outlinevis */
typedef enum eSpaceOutliner_Mode {
	SO_ALL_SCENES = 0,
	SO_CUR_SCENE = 1,
	SO_VISIBLE = 2,
	SO_SELECTED = 3,
	SO_ACTIVE = 4,
	SO_SAME_TYPE = 5,
	SO_GROUPS = 6,
	SO_LIBRARIES = 7,
	/* SO_VERSE_SESSION = 8, */  /* deprecated! */
	/* SO_VERSE_MS = 9, */       /* deprecated! */
	SO_SEQUENCE = 10,
	SO_DATABLOCKS = 11,
	SO_USERDEF = 12,
	/* SO_KEYMAP = 13, */        /* deprecated! */
} eSpaceOutliner_Mode;

/* SpaceOops->storeflag */
typedef enum eSpaceOutliner_StoreFlag {
	/* rebuild tree */
	SO_TREESTORE_CLEANUP    = (1 << 0),
	/* if set, it allows redraws. gets set for some allqueue events */
	SO_TREESTORE_REDRAW     = (1 << 1),
} eSpaceOutliner_StoreFlag;

/* outliner search flags (SpaceOops->search_flags) */
typedef enum eSpaceOutliner_Search_Flags {
	SO_FIND_CASE_SENSITIVE  = (1 << 0),
	SO_FIND_COMPLETE        = (1 << 1),
	SO_SEARCH_RECURSIVE     = (1 << 2),
} eSpaceOutliner_Search_Flags;


/* Graph Editor ========================================= */

/* 'Graph' Editor (formerly known as the IPO Editor) */
typedef struct SpaceIpo {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;
	float blockscale DNA_DEPRECATED;
	short blockhandler[8]  DNA_DEPRECATED;
	
	View2D v2d DNA_DEPRECATED;  /* deprecated, copied to region */
	
	struct bDopeSheet *ads; /* settings for filtering animation data (NOTE: we use a pointer due to code-linking issues) */
	
	ListBase ghostCurves;   /* sampled snapshots of F-Curves used as in-session guides */
	
	short mode;             /* mode for the Graph editor (eGraphEdit_Mode) */
	short autosnap;         /* time-transform autosnapping settings for Graph editor (eAnimEdit_AutoSnap in DNA_action_types.h) */
	int flag;               /* settings for Graph editor (eGraphEdit_Flag) */
	
	float cursorVal;        /* cursor value (y-value, x-value is current frame) */
	int around;             /* pivot point for transforms */
} SpaceIpo;


/* SpaceIpo->flag (Graph Editor Settings) */
typedef enum eGraphEdit_Flag {
	/* OLD DEPRECEATED SETTING */
	/* SIPO_LOCK_VIEW            = (1 << 0), */
	
	/* don't merge keyframes on the same frame after a transform */
	SIPO_NOTRANSKEYCULL       = (1 << 1),
	/* don't show any keyframe handles at all */
	SIPO_NOHANDLES            = (1 << 2),
	/* don't show current frame number beside indicator line */
	SIPO_NODRAWCFRANUM        = (1 << 3),
	/* show timing in seconds instead of frames */
	SIPO_DRAWTIME             = (1 << 4),
	/* only show keyframes for selected F-Curves */
	SIPO_SELCUVERTSONLY       = (1 << 5),
	/* draw names of F-Curves beside the respective curves */
	/* NOTE: currently not used */
	SIPO_DRAWNAMES            = (1 << 6),
	/* show sliders in channels list */
	SIPO_SLIDERS              = (1 << 7),
	/* don't show the horizontal component of the cursor */
	SIPO_NODRAWCURSOR         = (1 << 8),
	/* only show handles of selected keyframes */
	SIPO_SELVHANDLESONLY      = (1 << 9),
	/* temporary flag to force channel selections to be synced with main */
	SIPO_TEMP_NEEDCHANSYNC    = (1 << 10),
	/* don't perform realtime updates */
	SIPO_NOREALTIMEUPDATES    = (1 << 11),
	/* don't draw curves with AA ("beauty-draw") for performance */
	SIPO_BEAUTYDRAW_OFF       = (1 << 12),
	/* draw grouped channels with colors set in group */
	SIPO_NODRAWGCOLORS        = (1 << 13),
	/* normalize curves on display */
	SIPO_NORMALIZE            = (1 << 14),
	SIPO_NORMALIZE_FREEZE     = (1 << 15),
} eGraphEdit_Flag;

/* SpaceIpo->mode (Graph Editor Mode) */
typedef enum eGraphEdit_Mode {
	/* all animation curves (from all over Blender) */
	SIPO_MODE_ANIMATION = 0,
	/* drivers only */
	SIPO_MODE_DRIVERS = 1,
} eGraphEdit_Mode;


/* NLA Editor ============================================= */

/* NLA Editor */
typedef struct SpaceNla {
	struct SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;
	float blockscale DNA_DEPRECATED;
	short blockhandler[8]  DNA_DEPRECATED;

	short autosnap;         /* this uses the same settings as autosnap for Action Editor */
	short flag;
	int pad;
	
	struct bDopeSheet *ads;
	View2D v2d DNA_DEPRECATED;   /* deprecated, copied to region */
} SpaceNla;

/* nla->flag */
typedef enum eSpaceNla_Flag {
	/* flags (1<<0), (1<<1), and (1<<3) are deprecated flags from old verisons */

	/* draw timing in seconds instead of frames */
	SNLA_DRAWTIME          = (1 << 2),
	/* don't draw frame number beside frame indicator */
	SNLA_NODRAWCFRANUM     = (1 << 4),
	/* don't draw influence curves on strips */
	SNLA_NOSTRIPCURVES     = (1 << 5),
	/* don't perform realtime updates */
	SNLA_NOREALTIMEUPDATES = (1 << 6),
} eSpaceNla_Flag;


/* Timeline =============================================== */

/* Pointcache drawing data */
# /* Only store the data array in the cache to avoid constant reallocation. */
# /* No need to store when saved. */
typedef struct SpaceTimeCache {
	struct SpaceTimeCache *next, *prev;
	float *array;
} SpaceTimeCache;

/* Timeline View */
typedef struct SpaceTime {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;
	float blockscale DNA_DEPRECATED;
	
	View2D v2d DNA_DEPRECATED;  /* deprecated, copied to region */

	ListBase caches;

	int cache_display;
	int flag;
} SpaceTime;


/* time->flag */
typedef enum eTimeline_Flag {
	/* show timing in frames instead of in seconds */
	TIME_DRAWFRAMES    = (1 << 0),
	/* show time indicator box beside the frame number */
	TIME_CFRA_NUM      = (1 << 1),
	/* only keyframes from active/selected channels get shown */
	TIME_ONLYACTSEL    = (1 << 2),
} eTimeline_Flag;

/* time->redraws (now screen->redraws_flag) */
typedef enum eScreen_Redraws_Flag {
	TIME_REGION            = (1 << 0),
	TIME_ALL_3D_WIN        = (1 << 1),
	TIME_ALL_ANIM_WIN      = (1 << 2),
	TIME_ALL_BUTS_WIN      = (1 << 3),
	// TIME_WITH_SEQ_AUDIO    = (1 << 4), /* DEPRECATED */
	TIME_SEQ               = (1 << 5),
	TIME_ALL_IMAGE_WIN     = (1 << 6),
	// TIME_CONTINUE_PHYSICS  = (1 << 7), /* UNUSED */
	TIME_NODES             = (1 << 8),
	TIME_CLIPS             = (1 << 9),
} eScreen_Redraws_Flag;

/* time->cache */
typedef enum eTimeline_Cache_Flag {
	TIME_CACHE_DISPLAY       = (1 << 0),
	TIME_CACHE_SOFTBODY      = (1 << 1),
	TIME_CACHE_PARTICLES     = (1 << 2),
	TIME_CACHE_CLOTH         = (1 << 3),
	TIME_CACHE_SMOKE         = (1 << 4),
	TIME_CACHE_DYNAMICPAINT  = (1 << 5),
	TIME_CACHE_RIGIDBODY     = (1 << 6),
} eTimeline_Cache_Flag;


/* Sequence Editor ======================================= */

/* Sequencer */
typedef struct SpaceSeq {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;
	float blockscale DNA_DEPRECATED;

	short blockhandler[8]  DNA_DEPRECATED;

	View2D v2d DNA_DEPRECATED;  /* deprecated, copied to region */
	
	float xof DNA_DEPRECATED, yof DNA_DEPRECATED;   /* deprecated: offset for drawing the image preview */
	short mainb;    /* weird name for the sequencer subtype (seq, image, luma... etc) */
	short render_size;
	short chanshown;
	short zebra;
	int flag;
	float zoom DNA_DEPRECATED;  /* deprecated, handled by View2D now */
	int view; /* see SEQ_VIEW_* below */
	int overlay_type;
	int draw_flag; /* overlay an image of the editing on below the strips */
	int pad;

	struct bGPdata *gpd;        /* grease-pencil data */

	struct SequencerScopes scopes;  /* different scoped displayed in space */
} SpaceSeq;


/* sseq->mainb */
typedef enum eSpaceSeq_RegionType {
	SEQ_DRAW_SEQUENCE = 0,
	SEQ_DRAW_IMG_IMBUF = 1,
	SEQ_DRAW_IMG_WAVEFORM = 2,
	SEQ_DRAW_IMG_VECTORSCOPE = 3,
	SEQ_DRAW_IMG_HISTOGRAM = 4,
} eSpaceSeq_RegionType;

/* sseq->draw_flag */
typedef enum eSpaceSeq_DrawFlag {
	SEQ_DRAW_BACKDROP              = (1 << 0),
} eSpaceSeq_DrawFlag;


/* sseq->flag */
typedef enum eSpaceSeq_Flag {
	SEQ_DRAWFRAMES              = (1 << 0),
	SEQ_MARKER_TRANS            = (1 << 1),
	SEQ_DRAW_COLOR_SEPARATED    = (1 << 2),
	SEQ_DRAW_SAFE_MARGINS       = (1 << 3),
	SEQ_SHOW_GPENCIL            = (1 << 4),
	SEQ_NO_DRAW_CFRANUM         = (1 << 5),
	SEQ_USE_ALPHA               = (1 << 6), /* use RGBA display mode for preview */
	SEQ_ALL_WAVEFORMS           = (1 << 7), /* draw all waveforms */
	SEQ_NO_WAVEFORMS            = (1 << 8), /* draw no waveforms */
} eSpaceSeq_Flag;

/* sseq->view */
typedef enum eSpaceSeq_Displays {
	SEQ_VIEW_SEQUENCE = 1,
	SEQ_VIEW_PREVIEW = 2,
	SEQ_VIEW_SEQUENCE_PREVIEW = 3,
} eSpaceSeq_Dispays;

/* sseq->render_size */
typedef enum eSpaceSeq_Proxy_RenderSize {
	SEQ_PROXY_RENDER_SIZE_NONE      =  -1,
	SEQ_PROXY_RENDER_SIZE_SCENE     =   0,
	SEQ_PROXY_RENDER_SIZE_25        =  25,
	SEQ_PROXY_RENDER_SIZE_50        =  50,
	SEQ_PROXY_RENDER_SIZE_75        =  75,
	SEQ_PROXY_RENDER_SIZE_100       =  99,
	SEQ_PROXY_RENDER_SIZE_FULL      = 100
} eSpaceSeq_Proxy_RenderSize;

typedef struct MaskSpaceInfo
{
	/* **** mask editing **** */
	struct Mask *mask;
	/* draw options */
	char draw_flag;
	char draw_type;
	char overlay_mode;
	char pad3[5];
} MaskSpaceInfo;

/* sseq->mainb */
typedef enum eSpaceSeq_OverlayType {
	SEQ_DRAW_OVERLAY_RECT = 0,
	SEQ_DRAW_OVERLAY_REFERENCE = 1,
	SEQ_DRAW_OVERLAY_CURRENT = 2
} eSpaceSeq_OverlayType;

/* File Selector ========================================== */

/* Config and Input for File Selector */
typedef struct FileSelectParams {
	char title[96]; /* title, also used for the text of the execute button */
	char dir[1090]; /* directory, FILE_MAX_LIBEXTRA, 1024 + 66, this is for extreme case when 1023 length path
	                 * needs to be linked in, where foo.blend/Armature need adding  */
	char pad_c1[2];
	char file[256]; /* file */
	char renamefile[256];
	char renameedit[256]; /* annoying but the first is only used for initialization */

	char filter_glob[64]; /* list of filetypes to filter */

	int active_file;
	int sel_first;
	int sel_last;

	/* short */
	short type; /* XXXXX for now store type here, should be moved to the operator */
	short flag; /* settings for filter, hiding dots files,...  */
	short sort; /* sort order */
	short display; /* display mode flag */
	short filter; /* filter when (flags & FILE_FILTER) is true */

	/* XXX --- still unused -- */
	short f_fp; /* show font preview */
	char fp_str[8]; /* string to use for font preview */

	/* XXX --- end unused -- */
} FileSelectParams;

/* File Browser */
typedef struct SpaceFile {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;
	
	int scroll_offset;

	struct FileSelectParams *params; /* config and input for file select */
	
	struct FileList *files; /* holds the list of files to show */

	ListBase *folders_prev; /* holds the list of previous directories to show */
	ListBase *folders_next; /* holds the list of next directories (pushed from previous) to show */

	/* operator that is invoking fileselect 
	 * op->exec() will be called on the 'Load' button.
	 * if operator provides op->cancel(), then this will be invoked
	 * on the cancel button.
	 */
	struct wmOperator *op; 

	struct wmTimer *smoothscroll_timer;

	struct FileLayout *layout;
	
	short recentnr, bookmarknr;
	short systemnr, pad2;
} SpaceFile;


/* FileSelectParams.display */
enum FileDisplayTypeE {
	FILE_DEFAULTDISPLAY = 0,
	FILE_SHORTDISPLAY = 1,
	FILE_LONGDISPLAY = 2,
	FILE_IMGDISPLAY = 3
};

/* FileSelectParams.sort */
enum FileSortTypeE {
	FILE_SORT_NONE = 0,
	FILE_SORT_ALPHA = 1,
	FILE_SORT_EXTENSION = 2,
	FILE_SORT_TIME = 3,
	FILE_SORT_SIZE = 4
};

/* these values need to be hardcoded in structs, dna does not recognize defines */
/* also defined in BKE */
#define FILE_MAXDIR         768
#define FILE_MAXFILE        256
#define FILE_MAX            1024

#define FILE_MAX_LIBEXTRA   (FILE_MAX + MAX_ID_NAME)

/* filesel types */
#define FILE_UNIX           8
#define FILE_BLENDER        8 /* don't display relative paths */
#define FILE_SPECIAL        9

#define FILE_LOADLIB        1
#define FILE_MAIN           2
#define FILE_LOADFONT       3

/* filesel op property -> action */
typedef enum eFileSel_Action {
	FILE_OPENFILE = 0,
	FILE_SAVE = 1,
} eFileSel_Action;

/* sfile->params->flag and simasel->flag */
typedef enum eFileSel_Params_Flag {
	FILE_SHOWSHORT      = (1 << 0),
	FILE_RELPATH        = (1 << 1), /* was FILE_STRINGCODE */
	FILE_LINK           = (1 << 2),
	FILE_HIDE_DOT       = (1 << 3),
	FILE_AUTOSELECT     = (1 << 4),
	FILE_ACTIVELAY      = (1 << 5),
/*  FILE_ATCURSOR       = (1 << 6), */ /* deprecated */
	FILE_DIRSEL_ONLY    = (1 << 7),
	FILE_FILTER         = (1 << 8),
	FILE_BOOKMARKS      = (1 << 9),
	FILE_GROUP_INSTANCE = (1 << 10),
} eFileSel_Params_Flag;


/* files in filesel list: file types */
typedef enum eFileSel_File_Types {
	BLENDERFILE         = (1 << 2),
	BLENDERFILE_BACKUP  = (1 << 3),
	IMAGEFILE           = (1 << 4),
	MOVIEFILE           = (1 << 5),
	PYSCRIPTFILE        = (1 << 6),
	FTFONTFILE          = (1 << 7),
	SOUNDFILE           = (1 << 8),
	TEXTFILE            = (1 << 9),
	MOVIEFILE_ICON      = (1 << 10), /* movie file that preview can't load */
	FOLDERFILE          = (1 << 11), /* represents folders for filtering */
	BTXFILE             = (1 << 12),
	COLLADAFILE         = (1 << 13),
	OPERATORFILE        = (1 << 14), /* from filter_glob operator property */
	APPLICATIONBUNDLE   = (1 << 15),
} eFileSel_File_Types;

/* Selection Flags in filesel: struct direntry, unsigned char selflag */
typedef enum eDirEntry_SelectFlag {
/*  ACTIVE_FILE         = (1 << 1), */ /* UNUSED */
	HILITED_FILE        = (1 << 2),
	SELECTED_FILE       = (1 << 3),
	EDITING_FILE        = (1 << 4),
} eDirEntry_SelectFlag;

/* Image/UV Editor ======================================== */

/* Image/UV Editor */
typedef struct SpaceImage {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;

	int flag;

	struct Image *image;
	struct ImageUser iuser;

	struct CurveMapping *cumap DNA_DEPRECATED;  /* was switched to scene's color management settings */

	struct Scopes scopes;           /* histogram waveform and vectorscope */
	struct Histogram sample_line_hist;  /* sample line histogram */

	struct bGPdata *gpd;            /* grease pencil data */

	float cursor[2];                /* UV editor 2d cursor */
	float xof, yof;                 /* user defined offset, image is centered */
	float zoom;                     /* user defined zoom level */
	float centx, centy;             /* storage for offset while render drawing */

	char  mode;                     /* view/paint/mask */
	char  pin;
	short pad;
	short curtile; /* the currently active tile of the image when tile is enabled, is kept in sync with the active faces tile */
	short lock;
	char dt_uv; /* UV draw type */
	char sticky; /* sticky selection type */
	char dt_uvstretch;
	char around;

	MaskSpaceInfo mask_info;
} SpaceImage;


/* SpaceImage->dt_uv */
typedef enum eSpaceImage_UVDT {
	SI_UVDT_OUTLINE = 0,
	SI_UVDT_DASH = 1,
	SI_UVDT_BLACK = 2,
	SI_UVDT_WHITE = 3,
} eSpaceImage_UVDT;

/* SpaceImage->dt_uvstretch */
typedef enum eSpaceImage_UVDT_Stretch {
	SI_UVDT_STRETCH_ANGLE = 0,
	SI_UVDT_STRETCH_AREA = 1,
} eSpaceImage_UVDT_Stretch;

/* SpaceImage->mode */
typedef enum eSpaceImage_Mode {
	SI_MODE_VIEW  = 0,
	SI_MODE_PAINT = 1,
	SI_MODE_MASK  = 2   /* note: mesh edit mode overrides mask */
} eSpaceImage_Mode;

/* SpaceImage->sticky
 * Note DISABLE should be 0, however would also need to re-arrange icon order,
 * also, sticky loc is the default mode so this means we don't need to 'do_versons' */
typedef enum eSpaceImage_Sticky {
	SI_STICKY_LOC      = 0,
	SI_STICKY_DISABLE  = 1,
	SI_STICKY_VERTEX   = 2,
} eSpaceImage_Sticky;

/* SpaceImage->flag */
typedef enum eSpaceImage_Flag {
/*	SI_BE_SQUARE          = (1 << 0), */  /* deprecated */
	SI_EDITTILE           = (1 << 1),     /* XXX - not used but should be? */
	SI_CLIP_UV            = (1 << 2),
/*	SI_DRAWTOOL           = (1 << 3), */  /* deprecated */
	SI_NO_DRAWFACES       = (1 << 4),
	SI_DRAWSHADOW         = (1 << 5),
/*	SI_SELACTFACE         = (1 << 6), */  /* deprecated */
/*	SI_DEPRECATED2        = (1 << 7), */  /* deprecated */
/*	SI_DEPRECATED3        = (1 << 8), */  /* deprecated */
	SI_COORDFLOATS        = (1 << 9),
	SI_PIXELSNAP          = (1 << 10),
	SI_LIVE_UNWRAP        = (1 << 11),
	SI_USE_ALPHA          = (1 << 12),
	SI_SHOW_ALPHA         = (1 << 13),
	SI_SHOW_ZBUF          = (1 << 14),
	
	/* next two for render window display */
	SI_PREVSPACE          = (1 << 15),
	SI_FULLWINDOW         = (1 << 16),
	
/*	SI_DEPRECATED4        = (1 << 17), */  /* deprecated */
/*	SI_DEPRECATED5        = (1 << 18), */  /* deprecated */
	
	/* this means that the image is drawn until it reaches the view edge,
	 * in the image view, its unrelated to the 'tile' mode for texface
	 */
	SI_DRAW_TILE          = (1 << 19),
	SI_SMOOTH_UV          = (1 << 20),
	SI_DRAW_STRETCH       = (1 << 21),
	SI_SHOW_GPENCIL       = (1 << 22),
	SI_DRAW_OTHER         = (1 << 23),

	SI_COLOR_CORRECTION   = (1 << 24),

	SI_NO_DRAW_TEXPAINT   = (1 << 25),
} eSpaceImage_Flag;

/* Text Editor ============================================ */

/* Text Editor */
typedef struct SpaceText {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;
	float blockscale DNA_DEPRECATED;
	short blockhandler[8]  DNA_DEPRECATED;

	struct Text *text;

	int top, viewlines;
	short flags, menunr;

	short lheight;      /* user preference, is font_size! */
	char cwidth, linenrs_tot;       /* runtime computed, character width and the number of chars to use when showing line numbers */
	int left;
	int showlinenrs;
	int tabnumber;

	short showsyntax;
	short line_hlight;
	short overwrite;
	short live_edit; /* run python while editing, evil */
	float pix_per_line;

	struct rcti txtscroll, txtbar;

	int wordwrap, doplugins;

	char findstr[256];      /* ST_MAX_FIND_STR */
	char replacestr[256];   /* ST_MAX_FIND_STR */

	short margin_column;	/* column number to show right margin at */
	short lheight_dpi;		/* actual lineheight, dpi controlled */
	char pad[4];

	void *drawcache; /* cache for faster drawing */

	float scroll_accum[2]; /* runtime, for scroll increments smaller than a line */
} SpaceText;


/* SpaceText flags (moved from DNA_text_types.h) */
typedef enum eSpaceText_Flags {
	/* scrollable */
	ST_SCROLL_SELECT        = (1 << 0),
	/* clear namespace after script execution (BPY_main.c) */
	ST_CLEAR_NAMESPACE      = (1 << 4), 
	
	ST_FIND_WRAP            = (1 << 5),
	ST_FIND_ALL             = (1 << 6),
	ST_SHOW_MARGIN          = (1 << 7),
	ST_MATCH_CASE           = (1 << 8),
	
	ST_FIND_ACTIVATE		= (1 << 9),
} eSpaceText_Flags;

/* stext->findstr/replacestr */
#define ST_MAX_FIND_STR     256

/* Script View (Obsolete) ================================== */

/* Script Runtime Data - Obsolete (pre 2.5) */
typedef struct Script {
	ID id;

	void *py_draw;
	void *py_event;
	void *py_button;
	void *py_browsercallback;
	void *py_globaldict;

	int flags, lastspace;
	/* store the script file here so we can re-run it on loading blender, if "Enable Scripts" is on */
	char scriptname[1024]; /* 1024 = FILE_MAX */
	char scriptarg[256]; /* 1024 = FILE_MAX */
} Script;
#define SCRIPT_SET_NULL(_script) _script->py_draw = _script->py_event = _script->py_button = _script->py_browsercallback = _script->py_globaldict = NULL; _script->flags = 0

/* Script View - Obsolete (pre 2.5) */
typedef struct SpaceScript {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;
	float blockscale DNA_DEPRECATED;
	struct Script *script;

	short flags, menunr;
	int pad1;
	
	void *but_refs;
} SpaceScript;

/* Nodes Editor =========================================== */

/* Node Editor */

typedef struct bNodeTreePath {
	struct bNodeTreePath *next, *prev;
	
	struct bNodeTree *nodetree;
	bNodeInstanceKey parent_key;	/* base key for nodes in this tree instance */
	int pad;
	float view_center[2];			/* v2d center point, so node trees can have different offsets in editors */
	/* XXX this is not automatically updated when node names are changed! */
	char node_name[64];		/* MAX_NAME */
} bNodeTreePath;

typedef struct SpaceNode {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;
	float blockscale DNA_DEPRECATED;
	short blockhandler[8]  DNA_DEPRECATED;
	
	View2D v2d DNA_DEPRECATED;  /* deprecated, copied to region */
	
	struct ID *id, *from;       /* context, no need to save in file? well... pinning... */
	short flag, pad1;           /* menunr: browse id block in header */
	float aspect, pad2;	/* internal state variables */
	
	float xof, yof;     /* offset for drawing the backdrop */
	float zoom;   /* zoom for backdrop */
	float cursor[2];    /* mouse pos for drawing socketless link and adding nodes */
	
	/* XXX nodetree pointer info is all in the path stack now,
	 * remove later on and use bNodeTreePath instead. For now these variables are set when pushing/popping
	 * from path stack, to avoid having to update all the functions and operators. Can be done when
	 * design is accepted and everything is properly tested.
	 */
	ListBase treepath;
	
	struct bNodeTree *nodetree, *edittree;
	
	/* tree type for the current node tree */
	char tree_idname[64];
	int treetype DNA_DEPRECATED; /* treetype: as same nodetree->type */
	int pad3;
	
	short texfrom;      /* texfrom object, world or brush */
	short shaderfrom;   /* shader from object or world */
	short recalc;       /* currently on 0/1, for auto compo */
	short pad4;
	ListBase linkdrag;  /* temporary data for modal linking operator */
	
	struct bGPdata *gpd;        /* grease-pencil data */
} SpaceNode;

/* snode->flag */
typedef enum eSpaceNode_Flag {
	SNODE_BACKDRAW       = (1 << 1),
	SNODE_SHOW_GPENCIL   = (1 << 2),
	SNODE_USE_ALPHA      = (1 << 3),
	SNODE_SHOW_ALPHA     = (1 << 4),
	SNODE_SHOW_R         = (1 << 7),
	SNODE_SHOW_G         = (1 << 8),
	SNODE_SHOW_B         = (1 << 9),
	SNODE_AUTO_RENDER    = (1 << 5),
	SNODE_SHOW_HIGHLIGHT = (1 << 6),
//	SNODE_USE_HIDDEN_PREVIEW = (1 << 10), DNA_DEPRECATED December2013 
	SNODE_NEW_SHADERS = (1 << 11),
	SNODE_PIN            = (1 << 12),
} eSpaceNode_Flag;

/* snode->texfrom */
typedef enum eSpaceNode_TexFrom {
	SNODE_TEX_OBJECT   = 0,
	SNODE_TEX_WORLD    = 1,
	SNODE_TEX_BRUSH    = 2,
	SNODE_TEX_LINESTYLE = 3,
} eSpaceNode_TexFrom;

/* snode->shaderfrom */
typedef enum eSpaceNode_ShaderFrom {
	SNODE_SHADER_OBJECT = 0,
	SNODE_SHADER_WORLD = 1,
	SNODE_SHADER_LINESTYLE = 2,
} eSpaceNode_ShaderFrom;

/* Game Logic Editor ===================================== */

/* Logic Editor */
typedef struct SpaceLogic {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;
	float blockscale DNA_DEPRECATED;
	
	short blockhandler[8]  DNA_DEPRECATED;
	
	short flag, scaflag;
	int pad;
	
	struct bGPdata *gpd;        /* grease-pencil data */
} SpaceLogic;

/* Console ================================================ */

/* Console content */
typedef struct ConsoleLine {
	struct ConsoleLine *next, *prev;
	
	/* keep these 3 vars so as to share free, realloc funcs */
	int len_alloc;  /* allocated length */
	int len;    /* real len - strlen() */
	char *line; 
	
	int cursor;
	int type; /* only for use when in the 'scrollback' listbase */
} ConsoleLine;

/* ConsoleLine.type */
typedef enum eConsoleLine_Type {
	CONSOLE_LINE_OUTPUT = 0,
	CONSOLE_LINE_INPUT = 1,
	CONSOLE_LINE_INFO = 2, /* autocomp feedback */
	CONSOLE_LINE_ERROR = 3
} eConsoleLine_Type;


/* Console View */
typedef struct SpaceConsole {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;
	float blockscale DNA_DEPRECATED;            // XXX are these needed?
	short blockhandler[8]  DNA_DEPRECATED;      // XXX are these needed?
	
	/* space vars */
	int lheight, pad;

	ListBase scrollback; /* ConsoleLine; output */
	ListBase history; /* ConsoleLine; command history, current edited line is the first */
	char prompt[256];
	char language[32]; /* multiple consoles are possible, not just python */
	
	int sel_start;
	int sel_end;
} SpaceConsole;


/* User Preferences ======================================= */

/* User Preferences View */
typedef struct SpaceUserPref {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;
	
	char pad[3];
	char filter_type;
	char filter[64];        /* search term for filtering in the UI */
} SpaceUserPref;

/* Motion Tracking ======================================== */

/* Clip Editor */
typedef struct SpaceClip {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	int spacetype;

	float xof, yof;             /* user defined offset, image is centered */
	float xlockof, ylockof;     /* user defined offset from locked position */
	float zoom;                 /* user defined zoom level */

	struct MovieClipUser user;      /* user of clip */
	struct MovieClip *clip;         /* clip data */
	struct MovieClipScopes scopes;  /* different scoped displayed in space panels */

	int flag;                   /* flags */
	short mode;                 /* editor mode (editing context being displayed) */
	short view;                 /* type of the clip editor view */

	int path_length;            /* length of displaying path, in frames */

	/* current stabilization data */
	float loc[2], scale, angle; /* pre-composed stabilization data */
	int pad;
	float stabmat[4][4], unistabmat[4][4];  /* current stabilization matrix and the same matrix in unified space,
	                                         * defined when drawing and used for mouse position calculation */

	/* movie postprocessing */
	int postproc_flag;

	/* grease pencil */
	short gpencil_src, pad2;

	int around, pad4;             /* pivot point for transforms */

	float cursor[2];              /* Mask editor 2d cursor */

	MaskSpaceInfo mask_info;
} SpaceClip;

/* SpaceClip->flag */
typedef enum eSpaceClip_Flag {
	SC_SHOW_MARKER_PATTERN      = (1 << 0),
	SC_SHOW_MARKER_SEARCH       = (1 << 1),
	SC_LOCK_SELECTION           = (1 << 2),
	SC_SHOW_TINY_MARKER         = (1 << 3),
	SC_SHOW_TRACK_PATH          = (1 << 4),
	SC_SHOW_BUNDLES             = (1 << 5),
	SC_MUTE_FOOTAGE             = (1 << 6),
	SC_HIDE_DISABLED            = (1 << 7),
	SC_SHOW_NAMES               = (1 << 8),
	SC_SHOW_GRID                = (1 << 9),
	SC_SHOW_STABLE              = (1 << 10),
	SC_MANUAL_CALIBRATION       = (1 << 11),
	SC_SHOW_GPENCIL             = (1 << 12),
	SC_SHOW_FILTERS             = (1 << 13),
	SC_SHOW_GRAPH_FRAMES        = (1 << 14),
	SC_SHOW_GRAPH_TRACKS_MOTION = (1 << 15),
/*	SC_SHOW_PYRAMID_LEVELS      = (1 << 16), */	/* UNUSED */
	SC_LOCK_TIMECURSOR          = (1 << 17),
	SC_SHOW_SECONDS             = (1 << 18),
	SC_SHOW_GRAPH_SEL_ONLY      = (1 << 19),
	SC_SHOW_GRAPH_HIDDEN        = (1 << 20),
	SC_SHOW_GRAPH_TRACKS_ERROR  = (1 << 21),
} eSpaceClip_Flag;

/* SpaceClip->mode */
typedef enum eSpaceClip_Mode {
	SC_MODE_TRACKING = 0,
	/*SC_MODE_RECONSTRUCTION = 1,*/  /* DEPRECATED */
	/*SC_MODE_DISTORTION = 2,*/  /* DEPRECATED */
	SC_MODE_MASKEDIT = 3,
} eSpaceClip_Mode;

/* SpaceClip->view */
typedef enum eSpaceClip_View {
	SC_VIEW_CLIP = 0,
	SC_VIEW_GRAPH = 1,
	SC_VIEW_DOPESHEET = 2,
} eSpaceClip_View;

/* SpaceClip->gpencil_src */
typedef enum eSpaceClip_GPencil_Source {
	SC_GPENCIL_SRC_CLIP = 0,
	SC_GPENCIL_SRC_TRACK = 1,
} eSpaceClip_GPencil_Source;

/* **************** SPACE DEFINES ********************* */

/* space types, moved from DNA_screen_types.h */
/* Do NOT change order, append on end. types are hardcoded needed */
typedef enum eSpace_Type {
	SPACE_EMPTY    = 0,
	SPACE_VIEW3D   = 1,
	SPACE_IPO      = 2,
	SPACE_OUTLINER = 3,
	SPACE_BUTS     = 4,
	SPACE_FILE     = 5,
	SPACE_IMAGE    = 6,
	SPACE_INFO     = 7,
	SPACE_SEQ      = 8,
	SPACE_TEXT     = 9,
	SPACE_IMASEL   = 10, /* deprecated */
	SPACE_SOUND    = 11, /* Deprecated */
	SPACE_ACTION   = 12,
	SPACE_NLA      = 13,
	SPACE_SCRIPT   = 14, /* Deprecated */
	SPACE_TIME     = 15,
	SPACE_NODE     = 16,
	SPACE_LOGIC    = 17,
	SPACE_CONSOLE  = 18,
	SPACE_USERPREF = 19,
	SPACE_CLIP     = 20,
	
	SPACEICONMAX = SPACE_CLIP
} eSpace_Type;

// TODO: SPACE_SCRIPT
#if (DNA_DEPRECATED_GCC_POISON == 1)
#pragma GCC poison SPACE_IMASEL SPACE_SOUND
#endif

#define IMG_SIZE_FALLBACK 256

#endif  /* __DNA_SPACE_TYPES_H__ */
