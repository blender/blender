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
struct Image;
struct Scopes;
struct Histogram;
struct SpaceIpo;
struct bNodeTree;
struct FileList;
struct bGPdata;
struct bDopeSheet;
struct FileSelectParams;
struct FileLayout;
struct wmOperator;
struct wmTimer;
struct MovieClip;
struct MovieClipScopes;
struct Mask;
struct BLI_mempool;

/* TODO 2.8: We don't write the global areas to files currently. Uncomment
 * define to enable writing (should become the default in a bit). */
//#define WITH_GLOBAL_AREA_WRITING


/* -------------------------------------------------------------------- */
/** \name SpaceLink (Base)
 * \{ */

/**
 * The base structure all the other spaces
 * are derived (implicitly) from. Would be
 * good to make this explicit.
 */
typedef struct SpaceLink {
	struct SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
} SpaceLink;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space Info
 * \{ */

/* Info Header */
typedef struct SpaceInfo {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Properties Editor
 * \{ */

/* Properties Editor */
typedef struct SpaceButs {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */

	View2D v2d DNA_DEPRECATED;                      /* deprecated, copied to region */

	/* For different kinds of property editors (exposed in the space type selector). */
	short space_subtype;

	short mainb, mainbo, mainbuser; /* context tabs */
	short preview;                  /* preview is signal to refresh */
	short pad[2];
	char flag;
	char collection_context;

	void *path;                     /* runtime */
	int pathflag, dataicon;         /* runtime */
	ID *pinid;

	void *texuser;
} SpaceButs;

/* button defines (deprecated) */
#ifdef DNA_DEPRECATED_ALLOW
/* warning: the values of these defines are used in SpaceButs.tabs[8] */
/* SpaceButs.mainb new */
#define CONTEXT_SCENE   0
#define CONTEXT_OBJECT  1
// #define CONTEXT_TYPES   2
#define CONTEXT_SHADING 3
#define CONTEXT_EDITING 4
// #define CONTEXT_SCRIPT  5
// #define CONTEXT_LOGIC   6

/* SpaceButs.mainb old (deprecated) */
// #define BUTS_VIEW           0
#define BUTS_LAMP           1
#define BUTS_MAT            2
#define BUTS_TEX            3
#define BUTS_ANIM           4
#define BUTS_WORLD          5
#define BUTS_RENDER         6
#define BUTS_EDIT           7
// #define BUTS_GAME           8
#define BUTS_FPAINT         9
#define BUTS_RADIO          10
#define BUTS_SCRIPT         11
// #define BUTS_SOUND          12
#define BUTS_CONSTRAINT     13
// #define BUTS_EFFECTS        14
#endif /* DNA_DEPRECATED_ALLOW */

/* SpaceButs.mainb new */
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
	BCONTEXT_VIEW_LAYER = 13,
	BCONTEXT_TOOL = 14,
	BCONTEXT_WORKSPACE = 15,

	/* always as last... */
	BCONTEXT_TOT
} eSpaceButtons_Context;

/* SpaceButs.flag */
typedef enum eSpaceButtons_Flag {
	SB_PRV_OSA = (1 << 0),
	SB_PIN_CONTEXT = (1 << 1),
	/* SB_WORLD_TEX = (1 << 2), */ /* not used anymore */
	/* SB_BRUSH_TEX = (1 << 3), */ /* not used anymore */
	SB_TEX_USER_LIMITED = (1 << 3), /* Do not add materials, particles, etc. in TemplateTextureUser list. */
	SB_SHADING_CONTEXT = (1 << 4),
} eSpaceButtons_Flag;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Outliner
 * \{ */

/* Outliner */
typedef struct SpaceOops {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */

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
	char search_string[64];
	struct TreeStoreElem search_tse;

	short flag, outlinevis, storeflag, search_flags;
	int filter;
	char filter_state;
	char pad;
	short filter_id_type;

	/* pointers to treestore elements, grouped by (id, type, nr) in hashtable for faster searching */
	void *treehash;
} SpaceOops;


/* SpaceOops.flag */
typedef enum eSpaceOutliner_Flag {
	SO_TESTBLOCKS           = (1 << 0),
	SO_NEWSELECTED          = (1 << 1),
	SO_HIDE_RESTRICTCOLS    = (1 << 2),
	SO_HIDE_KEYINGSETINFO   = (1 << 3),
	SO_SKIP_SORT_ALPHA      = (1 << 4),
} eSpaceOutliner_Flag;

/* SpaceOops.filter */
typedef enum eSpaceOutliner_Filter {
	SO_FILTER_SEARCH           = (1 << 0), /* Run-time flag. */
	/* SO_FILTER_ENABLE           = (1 << 1), */ /* Deprecated */
	SO_FILTER_NO_OBJECT        = (1 << 2),
	SO_FILTER_NO_OB_CONTENT    = (1 << 3), /* Not only mesh, but modifiers, constraints, ... */
	SO_FILTER_NO_CHILDREN      = (1 << 4),

	/* SO_FILTER_OB_TYPE          = (1 << 5), */ /* Deprecated */
	SO_FILTER_NO_OB_MESH       = (1 << 6),
	SO_FILTER_NO_OB_ARMATURE   = (1 << 7),
	SO_FILTER_NO_OB_EMPTY      = (1 << 8),
	SO_FILTER_NO_OB_LAMP       = (1 << 9),
	SO_FILTER_NO_OB_CAMERA     = (1 << 10),
	SO_FILTER_NO_OB_OTHERS     = (1 << 11),

	/* SO_FILTER_OB_STATE          = (1 << 12), */ /* Deprecated */
	SO_FILTER_OB_STATE_VISIBLE  = (1 << 13), /* Not set via DNA. */
	SO_FILTER_OB_STATE_SELECTED = (1 << 14), /* Not set via DNA. */
	SO_FILTER_OB_STATE_ACTIVE   = (1 << 15), /* Not set via DNA. */
	SO_FILTER_NO_COLLECTION     = (1 << 16),

	SO_FILTER_ID_TYPE           = (1 << 17),
} eSpaceOutliner_Filter;

#define SO_FILTER_OB_TYPE (SO_FILTER_NO_OB_MESH | \
                           SO_FILTER_NO_OB_ARMATURE | \
                           SO_FILTER_NO_OB_EMPTY | \
                           SO_FILTER_NO_OB_LAMP | \
                           SO_FILTER_NO_OB_CAMERA | \
                           SO_FILTER_NO_OB_OTHERS)

#define SO_FILTER_OB_STATE (SO_FILTER_OB_STATE_VISIBLE | \
                            SO_FILTER_OB_STATE_SELECTED | \
                            SO_FILTER_OB_STATE_ACTIVE)

#define SO_FILTER_ANY (SO_FILTER_NO_OB_CONTENT | \
                       SO_FILTER_NO_CHILDREN | \
                       SO_FILTER_OB_TYPE | \
                       SO_FILTER_OB_STATE | \
                       SO_FILTER_NO_COLLECTION)

/* SpaceOops.filter_state */
typedef enum eSpaceOutliner_StateFilter {
	SO_FILTER_OB_ALL           = 0,
	SO_FILTER_OB_VISIBLE       = 1,
	SO_FILTER_OB_SELECTED      = 2,
	SO_FILTER_OB_ACTIVE        = 3,
} eSpaceOutliner_StateFilter;

/* SpaceOops.outlinevis */
typedef enum eSpaceOutliner_Mode {
	SO_SCENES            = 0,
	/* SO_CUR_SCENE      = 1, */  /* deprecated! */
	/* SO_VISIBLE        = 2, */  /* deprecated! */
	/* SO_SELECTED       = 3, */  /* deprecated! */
	/* SO_ACTIVE         = 4, */  /* deprecated! */
	/* SO_SAME_TYPE      = 5, */  /* deprecated! */
	/* SO_GROUPS         = 6, */  /* deprecated! */
	SO_LIBRARIES         = 7,
	/* SO_VERSE_SESSION  = 8, */  /* deprecated! */
	/* SO_VERSE_MS       = 9, */  /* deprecated! */
	SO_SEQUENCE          = 10,
	SO_DATA_API          = 11,
	/* SO_USERDEF        = 12, */  /* deprecated! */
	/* SO_KEYMAP         = 13, */  /* deprecated! */
	SO_ID_ORPHANS        = 14,
	SO_VIEW_LAYER        = 15,
} eSpaceOutliner_Mode;

/* SpaceOops.storeflag */
typedef enum eSpaceOutliner_StoreFlag {
	/* cleanup tree */
	SO_TREESTORE_CLEANUP    = (1 << 0),
	/* SO_TREESTORE_REDRAW     = (1 << 1), */ /* Deprecated */
	/* rebuild the tree, similar to cleanup,
	 * but defer a call to BKE_outliner_treehash_rebuild_from_treestore instead */
	SO_TREESTORE_REBUILD    = (1 << 2),
} eSpaceOutliner_StoreFlag;

/* outliner search flags (SpaceOops.search_flags) */
typedef enum eSpaceOutliner_Search_Flags {
	SO_FIND_CASE_SENSITIVE  = (1 << 0),
	SO_FIND_COMPLETE        = (1 << 1),
	SO_SEARCH_RECURSIVE     = (1 << 2),
} eSpaceOutliner_Search_Flags;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Graph Editor
 * \{ */

/* 'Graph' Editor (formerly known as the IPO Editor) */
typedef struct SpaceIpo {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */

	View2D v2d DNA_DEPRECATED;  /* deprecated, copied to region */

	struct bDopeSheet *ads; /* settings for filtering animation data (NOTE: we use a pointer due to code-linking issues) */

	ListBase ghostCurves;   /* sampled snapshots of F-Curves used as in-session guides */

	short mode;             /* mode for the Graph editor (eGraphEdit_Mode) */
	short autosnap;         /* time-transform autosnapping settings for Graph editor (eAnimEdit_AutoSnap in DNA_action_types.h) */
	int flag;               /* settings for Graph editor (eGraphEdit_Flag) */

	float cursorTime;       /* time value for cursor (when in drivers mode; animation uses current frame) */
	float cursorVal;        /* cursor value (y-value, x-value is current frame) */
	int around;             /* pivot point for transforms */
	int pad;
} SpaceIpo;


/* SpaceIpo.flag (Graph Editor Settings) */
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

/* SpaceIpo.mode (Graph Editor Mode) */
typedef enum eGraphEdit_Mode {
	/* all animation curves (from all over Blender) */
	SIPO_MODE_ANIMATION = 0,
	/* drivers only */
	SIPO_MODE_DRIVERS = 1,
} eGraphEdit_Mode;

/** \} */

/* -------------------------------------------------------------------- */
/** \name NLA Editor
 * \{ */

/* NLA Editor */
typedef struct SpaceNla {
	struct SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */

	short autosnap;         /* this uses the same settings as autosnap for Action Editor */
	short flag;
	int pad;

	struct bDopeSheet *ads;
	View2D v2d DNA_DEPRECATED;   /* deprecated, copied to region */
} SpaceNla;

/* SpaceNla.flag */
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
	/* don't show local strip marker indications */
	SNLA_NOLOCALMARKERS    = (1 << 7),
} eSpaceNla_Flag;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Timeline
 * \{ */

/* SpaceTime.redraws (now bScreen.redraws_flag) */
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

	TIME_FOLLOW            = (1 << 15),
} eScreen_Redraws_Flag;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sequence Editor
 * \{ */

/* Sequencer */
typedef struct SpaceSeq {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */

	View2D v2d DNA_DEPRECATED;  /* deprecated, copied to region */

	float xof DNA_DEPRECATED, yof DNA_DEPRECATED;   /* deprecated: offset for drawing the image preview */
	short mainb;    /* weird name for the sequencer subtype (seq, image, luma... etc) */
	short render_size;  /* eSpaceSeq_Proxy_RenderSize */
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

	char multiview_eye;				/* multiview current eye - for internal use */
	char pad2[7];

	struct GPUFX *compositor;
	void *pad3;
} SpaceSeq;


/* SpaceSeq.mainb */
typedef enum eSpaceSeq_RegionType {
	SEQ_DRAW_SEQUENCE = 0,
	SEQ_DRAW_IMG_IMBUF = 1,
	SEQ_DRAW_IMG_WAVEFORM = 2,
	SEQ_DRAW_IMG_VECTORSCOPE = 3,
	SEQ_DRAW_IMG_HISTOGRAM = 4,
} eSpaceSeq_RegionType;

/* SpaceSeq.draw_flag */
typedef enum eSpaceSeq_DrawFlag {
	SEQ_DRAW_BACKDROP              = (1 << 0),
	SEQ_DRAW_OFFSET_EXT            = (1 << 1),
} eSpaceSeq_DrawFlag;


/* SpaceSeq.flag */
typedef enum eSpaceSeq_Flag {
	SEQ_DRAWFRAMES              = (1 << 0),
	SEQ_MARKER_TRANS            = (1 << 1),
	SEQ_DRAW_COLOR_SEPARATED    = (1 << 2),
	SEQ_SHOW_SAFE_MARGINS       = (1 << 3),
	SEQ_SHOW_GPENCIL            = (1 << 4),
	SEQ_NO_DRAW_CFRANUM         = (1 << 5),
	SEQ_USE_ALPHA               = (1 << 6), /* use RGBA display mode for preview */
	SEQ_ALL_WAVEFORMS           = (1 << 7), /* draw all waveforms */
	SEQ_NO_WAVEFORMS            = (1 << 8), /* draw no waveforms */
	SEQ_SHOW_SAFE_CENTER        = (1 << 9),
	SEQ_SHOW_METADATA           = (1 << 10),
} eSpaceSeq_Flag;

/* SpaceSeq.view */
typedef enum eSpaceSeq_Displays {
	SEQ_VIEW_SEQUENCE = 1,
	SEQ_VIEW_PREVIEW = 2,
	SEQ_VIEW_SEQUENCE_PREVIEW = 3,
} eSpaceSeq_Dispays;

/* SpaceSeq.render_size */
typedef enum eSpaceSeq_Proxy_RenderSize {
	SEQ_PROXY_RENDER_SIZE_NONE      =  -1,
	SEQ_PROXY_RENDER_SIZE_SCENE     =   0,
	SEQ_PROXY_RENDER_SIZE_25        =  25,
	SEQ_PROXY_RENDER_SIZE_50        =  50,
	SEQ_PROXY_RENDER_SIZE_75        =  75,
	SEQ_PROXY_RENDER_SIZE_100       =  99,
	SEQ_PROXY_RENDER_SIZE_FULL      = 100
} eSpaceSeq_Proxy_RenderSize;

typedef struct MaskSpaceInfo {
	/* **** mask editing **** */
	struct Mask *mask;
	/* draw options */
	char draw_flag;
	char draw_type;
	char overlay_mode;
	char pad3[5];
} MaskSpaceInfo;

/* SpaceSeq.mainb */
typedef enum eSpaceSeq_OverlayType {
	SEQ_DRAW_OVERLAY_RECT = 0,
	SEQ_DRAW_OVERLAY_REFERENCE = 1,
	SEQ_DRAW_OVERLAY_CURRENT = 2
} eSpaceSeq_OverlayType;

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Selector
 * \{ */

/* Config and Input for File Selector */
typedef struct FileSelectParams {
	char title[96]; /* title, also used for the text of the execute button */
	char dir[1090]; /* directory, FILE_MAX_LIBEXTRA, 1024 + 66, this is for extreme case when 1023 length path
	                 * needs to be linked in, where foo.blend/Armature need adding  */
	char pad_c1[2];
	char file[256]; /* file */
	char renamefile[256];
	char renameedit[256]; /* annoying but the first is only used for initialization */

	char filter_glob[256]; /* FILE_MAXFILE */ /* list of filetypes to filter */

	char filter_search[64];  /* text items' name must match to be shown. */
	int filter_id;  /* same as filter, but for ID types (aka library groups). */

	int active_file;    /* active file used for keyboard navigation */
	int highlight_file; /* file under cursor */
	int sel_first;
	int sel_last;
	unsigned short thumbnail_size;
	short pad;

	/* short */
	short type; /* XXXXX for now store type here, should be moved to the operator */
	short flag; /* settings for filter, hiding dots files,...  */
	short sort; /* sort order */
	short display; /* display mode flag */
	int filter; /* filter when (flags & FILE_FILTER) is true */

	short recursion_level;  /* max number of levels in dirtree to show at once, 0 to disable recursion. */

	/* XXX --- still unused -- */
	short f_fp; /* show font preview */
	char fp_str[8]; /* string to use for font preview */

	/* XXX --- end unused -- */
} FileSelectParams;

/* File Browser */
typedef struct SpaceFile {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */

	char _pad1[4];
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
	struct wmTimer *previews_timer;

	struct FileLayout *layout;

	short recentnr, bookmarknr;
	short systemnr, system_bookmarknr;
} SpaceFile;

/* FileSelectParams.display */
enum eFileDisplayType {
	FILE_DEFAULTDISPLAY = 0,
	FILE_SHORTDISPLAY = 1,
	FILE_LONGDISPLAY = 2,
	FILE_IMGDISPLAY = 3
};

/* FileSelectParams.sort */
enum eFileSortType {
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
/* Note: short flag, also used as 16 lower bits of flags in link/append code
 *       (WM and BLO code area, see BLO_LibLinkFlags in BLO_readfile.h). */
typedef enum eFileSel_Params_Flag {
	FILE_SHOWSHORT      = (1 << 0),
	FILE_RELPATH        = (1 << 1), /* was FILE_STRINGCODE */
	FILE_LINK           = (1 << 2),
	FILE_HIDE_DOT       = (1 << 3),
	FILE_AUTOSELECT     = (1 << 4),
	FILE_ACTIVE_COLLECTION = (1 << 5),
/*  FILE_ATCURSOR       = (1 << 6), */ /* deprecated */
	FILE_DIRSEL_ONLY    = (1 << 7),
	FILE_FILTER         = (1 << 8),
	FILE_BOOKMARKS      = (1 << 9),
	FILE_GROUP_INSTANCE = (1 << 10),
} eFileSel_Params_Flag;


/* files in filesel list: file types
 * Note we could use mere values (instead of bitflags) for file types themselves,
 * but since we do not lack of bytes currently...
 */
typedef enum eFileSel_File_Types {
	FILE_TYPE_BLENDER           = (1 << 2),
	FILE_TYPE_BLENDER_BACKUP    = (1 << 3),
	FILE_TYPE_IMAGE             = (1 << 4),
	FILE_TYPE_MOVIE             = (1 << 5),
	FILE_TYPE_PYSCRIPT          = (1 << 6),
	FILE_TYPE_FTFONT            = (1 << 7),
	FILE_TYPE_SOUND             = (1 << 8),
	FILE_TYPE_TEXT              = (1 << 9),
	/* 1 << 10 was FILE_TYPE_MOVIE_ICON, got rid of this so free slot for future type... */
	FILE_TYPE_FOLDER            = (1 << 11), /* represents folders for filtering */
	FILE_TYPE_BTX               = (1 << 12),
	FILE_TYPE_COLLADA           = (1 << 13),
	FILE_TYPE_OPERATOR          = (1 << 14), /* from filter_glob operator property */
	FILE_TYPE_APPLICATIONBUNDLE = (1 << 15),
	FILE_TYPE_ALEMBIC           = (1 << 16),

	FILE_TYPE_DIR               = (1 << 30),  /* An FS directory (i.e. S_ISDIR on its path is true). */
	FILE_TYPE_BLENDERLIB        = (1u << 31),
} eFileSel_File_Types;

/* Selection Flags in filesel: struct direntry, unsigned char selflag */
typedef enum eDirEntry_SelectFlag {
/*	FILE_SEL_ACTIVE         = (1 << 1), */ /* UNUSED */
	FILE_SEL_HIGHLIGHTED    = (1 << 2),
	FILE_SEL_SELECTED       = (1 << 3),
	FILE_SEL_EDITING        = (1 << 4),
} eDirEntry_SelectFlag;

#define FILE_LIST_MAX_RECURSION 4

/* ***** Related to file browser, but never saved in DNA, only here to help with RNA. ***** */

/* About Unique identifier.
 * Stored in a CustomProps once imported.
 * Each engine is free to use it as it likes - it will be the only thing passed to it by blender to identify
 * asset/variant/version (concatenating the three into a single 48 bytes one).
 * Assumed to be 128bits, handled as four integers due to lack of real bytes proptype in RNA :|.
 */
#define ASSET_UUID_LENGTH     16

/* Used to communicate with asset engines outside of 'import' context. */
typedef struct AssetUUID {
	int uuid_asset[4];
	int uuid_variant[4];
	int uuid_revision[4];
} AssetUUID;

typedef struct AssetUUIDList {
	AssetUUID *uuids;
	int nbr_uuids, pad;
} AssetUUIDList;

/* Container for a revision, only relevant in asset context. */
typedef struct FileDirEntryRevision {
	struct FileDirEntryRevision *next, *prev;

	char *comment;
	void *pad;

	int uuid[4];

	uint64_t size;
	int64_t time;
	/* Temp caching of UI-generated strings... */
	char    size_str[16];
	char    time_str[8];
	char    date_str[16];
} FileDirEntryRevision;

/* Container for a variant, only relevant in asset context.
 * In case there are no variants, a single one shall exist, with NULL name/description. */
typedef struct FileDirEntryVariant {
	struct FileDirEntryVariant *next, *prev;

	int uuid[4];
	char *name;
	char *description;

	ListBase revisions;
	int nbr_revisions;
	int act_revision;
} FileDirEntryVariant;

/* Container for mere direntry, with additional asset-related data. */
typedef struct FileDirEntry {
	struct FileDirEntry *next, *prev;

	int uuid[4];
	char *name;
	char *description;

	/* Either point to active variant/revision if available, or own entry (in mere filebrowser case). */
	FileDirEntryRevision *entry;

	int typeflag;  /* eFileSel_File_Types */
	int blentype;  /* ID type, in case typeflag has FILE_TYPE_BLENDERLIB set. */

	char *relpath;

	void *poin;  /* TODO: make this a real ID pointer? */
	struct ImBuf *image;

	/* Tags are for info only, most of filtering is done in asset engine. */
	char **tags;
	int nbr_tags;

	short status;
	short flags;

	ListBase variants;
	int nbr_variants;
	int act_variant;
} FileDirEntry;

/* Array of direntries. */
/* This struct is used in various, different contexts.
 * In Filebrowser UI, it stores the total number of available entries, the number of visible (filtered) entries,
 *                    and a subset of those in 'entries' ListBase, from idx_start (included) to idx_end (excluded).
 * In AssetEngine context (i.e. outside of 'browsing' context), entries contain all needed data, there is no filtering,
 *                        so nbr_entries_filtered, entry_idx_start and entry_idx_end should all be set to -1.
 */
typedef struct FileDirEntryArr {
	ListBase entries;
	int nbr_entries;
	int nbr_entries_filtered;
	int entry_idx_start, entry_idx_end;

	char root[1024];	 /* FILE_MAX */
} FileDirEntryArr;

/* FileDirEntry.status */
enum {
	ASSET_STATUS_LOCAL  = 1 << 0,  /* If active uuid is available locally/immediately. */
	ASSET_STATUS_LATEST = 1 << 1,  /* If active uuid is latest available version. */
};

/* FileDirEntry.flags */
enum {
	FILE_ENTRY_INVALID_PREVIEW = 1 << 0,  /* The preview for this entry could not be generated. */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image/UV Editor
 * \{ */

/* Image/UV Editor */
typedef struct SpaceImage {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */

	struct Image *image;
	struct ImageUser iuser;

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

	/* Filter settings when editor shows other object's UVs. */
	int other_uv_filter;

	int flag;

	MaskSpaceInfo mask_info;
} SpaceImage;


/* SpaceImage.dt_uv */
typedef enum eSpaceImage_UVDT {
	SI_UVDT_OUTLINE = 0,
	SI_UVDT_DASH = 1,
	SI_UVDT_BLACK = 2,
	SI_UVDT_WHITE = 3,
} eSpaceImage_UVDT;

/* SpaceImage.dt_uvstretch */
typedef enum eSpaceImage_UVDT_Stretch {
	SI_UVDT_STRETCH_ANGLE = 0,
	SI_UVDT_STRETCH_AREA = 1,
} eSpaceImage_UVDT_Stretch;

/* SpaceImage.mode */
typedef enum eSpaceImage_Mode {
	SI_MODE_VIEW  = 0,
	SI_MODE_PAINT = 1,
	SI_MODE_MASK  = 2   /* note: mesh edit mode overrides mask */
} eSpaceImage_Mode;

/* SpaceImage.sticky
 * Note DISABLE should be 0, however would also need to re-arrange icon order,
 * also, sticky loc is the default mode so this means we don't need to 'do_versions' */
typedef enum eSpaceImage_Sticky {
	SI_STICKY_LOC      = 0,
	SI_STICKY_DISABLE  = 1,
	SI_STICKY_VERTEX   = 2,
} eSpaceImage_Sticky;

/* SpaceImage.flag */
typedef enum eSpaceImage_Flag {
/*	SI_BE_SQUARE          = (1 << 0), */  /* deprecated */
/*	SI_EDITTILE           = (1 << 1), */  /* deprecated */
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
	 * in the image view, it's unrelated to the 'tile' mode for texface
	 */
	SI_DRAW_TILE          = (1 << 19),
	SI_SMOOTH_UV          = (1 << 20),
	SI_DRAW_STRETCH       = (1 << 21),
	SI_SHOW_GPENCIL       = (1 << 22),
	SI_DRAW_OTHER         = (1 << 23),

	SI_COLOR_CORRECTION   = (1 << 24),

	SI_NO_DRAW_TEXPAINT   = (1 << 25),
	SI_DRAW_METADATA      = (1 << 26),

	SI_SHOW_R             = (1 << 27),
	SI_SHOW_G             = (1 << 28),
	SI_SHOW_B             = (1 << 29),
} eSpaceImage_Flag;

/* SpaceImage.other_uv_filter */
typedef enum eSpaceImage_OtherUVFilter {
	SI_FILTER_SAME_IMAGE    = 0,
	SI_FILTER_ALL           = 1,
} eSpaceImage_OtherUVFilter;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Editor
 * \{ */

/* Text Editor */
typedef struct SpaceText {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */

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

/* SpaceText.findstr/replacestr */
#define ST_MAX_FIND_STR     256

/** \} */

/* -------------------------------------------------------------------- */
/** \name Script View (Obsolete)
 * \{ */

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
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */

	struct Script *script;

	short flags, menunr;
	int pad1;

	void *but_refs;
} SpaceScript;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Nodes Editor
 * \{ */

typedef struct bNodeTreePath {
	struct bNodeTreePath *next, *prev;

	struct bNodeTree *nodetree;
	bNodeInstanceKey parent_key;	/* base key for nodes in this tree instance */
	int pad;
	float view_center[2];			/* v2d center point, so node trees can have different offsets in editors */

	char node_name[64];		/* MAX_NAME */
} bNodeTreePath;

typedef struct SpaceNode {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */

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

	short texfrom;       /* texfrom object, world or brush */
	short shaderfrom;    /* shader from object or world */
	short recalc;        /* currently on 0/1, for auto compo */

	char insert_ofs_dir; /* direction for offsetting nodes on insertion */
	char pad4;

	ListBase linkdrag;   /* temporary data for modal linking operator */
	/* XXX hack for translate_attach op-macros to pass data from transform op to insert_offset op */
	struct NodeInsertOfsData *iofsd; /* temporary data for node insert offset (in UI called Auto-offset) */

	struct bGPdata *gpd;        /* grease-pencil data */
} SpaceNode;

/* SpaceNode.flag */
typedef enum eSpaceNode_Flag {
	SNODE_BACKDRAW       = (1 << 1),
	SNODE_SHOW_GPENCIL   = (1 << 2),
	SNODE_USE_ALPHA      = (1 << 3),
	SNODE_SHOW_ALPHA     = (1 << 4),
	SNODE_SHOW_R         = (1 << 7),
	SNODE_SHOW_G         = (1 << 8),
	SNODE_SHOW_B         = (1 << 9),
	SNODE_AUTO_RENDER    = (1 << 5),
//	SNODE_SHOW_HIGHLIGHT = (1 << 6), DNA_DEPRECATED
//	SNODE_USE_HIDDEN_PREVIEW = (1 << 10), DNA_DEPRECATED December2013
//	SNODE_NEW_SHADERS    = (1 << 11), DNA_DEPRECATED
	SNODE_PIN            = (1 << 12),
	SNODE_SKIP_INSOFFSET = (1 << 13), /* automatically offset following nodes in a chain on insertion */
} eSpaceNode_Flag;

/* SpaceNode.texfrom */
typedef enum eSpaceNode_TexFrom {
	/* SNODE_TEX_OBJECT   = 0, */
	SNODE_TEX_WORLD    = 1,
	SNODE_TEX_BRUSH    = 2,
	SNODE_TEX_LINESTYLE = 3,
} eSpaceNode_TexFrom;

/* SpaceNode.shaderfrom */
typedef enum eSpaceNode_ShaderFrom {
	SNODE_SHADER_OBJECT = 0,
	SNODE_SHADER_WORLD = 1,
	SNODE_SHADER_LINESTYLE = 2,
} eSpaceNode_ShaderFrom;

/* SpaceNode.insert_ofs_dir */
enum {
	SNODE_INSERTOFS_DIR_RIGHT = 0,
	SNODE_INSERTOFS_DIR_LEFT  = 1,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Console
 * \{ */

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
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */

	/* space vars */
	int lheight, pad;

	ListBase scrollback; /* ConsoleLine; output */
	ListBase history; /* ConsoleLine; command history, current edited line is the first */
	char prompt[256];
	char language[32]; /* multiple consoles are possible, not just python */

	int sel_start;
	int sel_end;
} SpaceConsole;

/** \} */

/* -------------------------------------------------------------------- */
/** \name User Preferences
 * \{ */

typedef struct SpaceUserPref {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */

	char _pad1[7];
	char filter_type;
	char filter[64];        /* search term for filtering in the UI */
} SpaceUserPref;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Motion Tracking
 * \{ */

/* Clip Editor */
typedef struct SpaceClip {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */

	char _pad1[4];

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

/* SpaceClip.flag */
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
	SC_SHOW_METADATA            = (1 << 22),
} eSpaceClip_Flag;

/* SpaceClip.mode */
typedef enum eSpaceClip_Mode {
	SC_MODE_TRACKING = 0,
	/*SC_MODE_RECONSTRUCTION = 1,*/  /* DEPRECATED */
	/*SC_MODE_DISTORTION = 2,*/  /* DEPRECATED */
	SC_MODE_MASKEDIT = 3,
} eSpaceClip_Mode;

/* SpaceClip.view */
typedef enum eSpaceClip_View {
	SC_VIEW_CLIP = 0,
	SC_VIEW_GRAPH = 1,
	SC_VIEW_DOPESHEET = 2,
} eSpaceClip_View;

/* SpaceClip.gpencil_src */
typedef enum eSpaceClip_GPencil_Source {
	SC_GPENCIL_SRC_CLIP = 0,
	SC_GPENCIL_SRC_TRACK = 1,
} eSpaceClip_GPencil_Source;


/** \} */

/* -------------------------------------------------------------------- */
/** \name Top Bar
 * \{ */

/* These two lines with # tell makesdna this struct can be excluded.
 * Should be: #ifndef WITH_GLOBAL_AREA_WRITING */
#
#
typedef struct SpaceTopBar {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */
} SpaceTopBar;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Status Bar
 * \{ */

/* These two lines with # tell makesdna this struct can be excluded.
 * Should be: #ifndef WITH_GLOBAL_AREA_WRITING */
#
#
typedef struct SpaceStatusBar {
	SpaceLink *next, *prev;
	ListBase regionbase;        /* storage of regions for inactive spaces */
	char spacetype;
	char link_flag;
	char _pad0[6];
	/* End 'SpaceLink' header. */
} SpaceStatusBar;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space Defines (eSpace_Type)
 * \{ */

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
#ifdef DNA_DEPRECATED
	SPACE_IMASEL   = 10, /* deprecated */
	SPACE_SOUND    = 11, /* Deprecated */
#endif
	SPACE_ACTION   = 12,
	SPACE_NLA      = 13,
	/* TODO: fully deprecate */
	SPACE_SCRIPT   = 14, /* Deprecated */
	SPACE_TIME     = 15, /* Deprecated */
	SPACE_NODE     = 16,
	SPACE_LOGIC    = 17, /* deprecated */
	SPACE_CONSOLE  = 18,
	SPACE_USERPREF = 19,
	SPACE_CLIP     = 20,
	SPACE_TOPBAR   = 21,
	SPACE_STATUSBAR = 22,

	SPACE_TYPE_LAST = SPACE_STATUSBAR
} eSpace_Type;

/* use for function args */
#define SPACE_TYPE_ANY -1

#define IMG_SIZE_FALLBACK 256

/** \} */

#endif  /* __DNA_SPACE_TYPES_H__ */
