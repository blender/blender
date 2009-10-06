/**
 * blenlib/DNA_space_types.h (mar-2001 nzc)
 *	
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef DNA_SPACE_TYPES_H
#define DNA_SPACE_TYPES_H

#include "DNA_listBase.h"
#include "DNA_vec_types.h"
#include "DNA_outliner_types.h"		/* for TreeStoreElem */
#include "DNA_image_types.h"	/* ImageUser */
/* Hum ... Not really nice... but needed for spacebuts. */
#include "DNA_view2d_types.h"

struct ID;
struct Text;
struct Script;
struct ImBuf;
struct Image;
struct SpaceIpo;
struct BlendHandle;
struct RenderInfo;
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

	/**
	 * The base structure all the other spaces
	 * are derived (implicitly) from. Would be
	 * good to make this explicit.
	 */

typedef struct SpaceLink {
	struct SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;			/* XXX depricate this */
	short blockhandler[8];		/* XXX depricate this */
} SpaceLink;

typedef struct SpaceInfo {
	SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;

	short blockhandler[8];		/* XXX depricate this */
	
	struct bScreen *screen;		/* browse screen */
	struct Scene *scene;		/* browse scene */
	
} SpaceInfo;

/* 'Graph' Editor (formerly known as the IPO Editor) */
/* XXX for now, we keep all old data... */
typedef struct SpaceIpo {
	SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;

	short blockhandler[8];
	View2D v2d; /* deprecated, copied to region */
	
	struct bDopeSheet *ads;	/* settings for filtering animation data (NOTE: we use a pointer due to code-linking issues) */
	
	ListBase ghostCurves;	/* sampled snapshots of F-Curves used as in-session guides */
	
	short mode;				/* mode for the Graph editor (eGraphEdit_Mode) */
	short flag;				/* settings for Graph editor */
	short autosnap;			/* time-transform autosnapping settings for Graph editor (eAnimEdit_AutoSnap in DNA_action_types.h) */
	char pin, lock;			// XXX old, unused vars that are probably going to be depreceated soon...
} SpaceIpo;

typedef struct SpaceButs {
	SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;
	
	short blockhandler[8];
	
	struct RenderInfo *ri;

	View2D v2d;						/* deprecated, copied to region */
	
	short mainb, mainbo, mainbuser;	/* context tabs */
	short re_align, align;			/* align for panels */
	short preview;					/* preview is signal to refresh */
	char flag, pad[3];
	
	void *path;				/* runtime */
	int pathflag, dataicon;	/* runtime */
	ID *pinid;
} SpaceButs;

typedef struct SpaceSeq {
	SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;

	short blockhandler[8];

	View2D v2d; /* deprecated, copied to region */
	
	float xof, yof;	/* offset for drawing the image preview */
	short mainb;	/* weird name for the sequencer subtype (seq, image, luma... etc) */
	short render_size;
	short chanshown;
	short zebra;
	int flag;
	float zoom;
	
	struct bGPdata *gpd;		/* grease-pencil data */
} SpaceSeq;

typedef struct FileSelectParams {
	char title[24]; /* title, also used for the text of the execute button */
	char dir[240]; /* directory */
	char file[80]; /* file */
	char renamefile[80];

	short type; /* XXXXX for now store type here, should be moved to the operator */
	short flag; /* settings for filter, hiding dots files,...  */
	short sort; /* sort order */
	short display; /* display mode flag */
	short filter; /* filter when (flags & FILE_FILTER) is true */

	/* XXX - temporary, better move to filelist */
	short active_bookmark;
	int	active_file;
	int selstate;

	/* short */
	/* XXX --- still unused -- */
	short f_fp; /* show font preview */
	short menu; /* currently selected option in pupmenu */
	char fp_str[8]; /* string to use for font preview */
	char *pupmenu; /* allows menu for save options - result stored in menup */
	/* XXX --- end unused -- */
} FileSelectParams;


typedef struct SpaceFile {
	SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	int pad;

	struct FileSelectParams *params; /* config and input for file select */
	
	struct FileList *files; /* holds the list of files to show */

	ListBase *folders_prev; /* holds the list of previous directories to show */
	ListBase *folders_next; /* holds the list of next directories (pushed from previous) to show */

	/* operator that is invoking fileselect 
	   op->exec() will be called on the 'Load' button.
	   if operator provides op->cancel(), then this will be invoked
	   on the cancel button.
	*/
	struct wmOperator *op; 

	struct wmTimer *loadimage_timer;

	struct FileLayout *layout;
	
	short recentnr, bookmarknr;
	short systemnr, pad2;
} SpaceFile;

typedef struct SpaceOops {
	SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;

	short blockhandler[8];

	View2D v2d; /* deprecated, copied to region */
	
	ListBase tree;
	struct TreeStore *treestore;
	
	/* search stuff */
	char search_string[32];
	struct TreeStoreElem search_tse;
	int search_flags, do_;
	
	short flag, outlinevis, storeflag, pad;
} SpaceOops;

typedef struct SpaceImage {
	SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;

	float blockscale;
	short blockhandler[8];
	
	struct Image *image;
	struct ImageUser iuser;
	
	struct CurveMapping *cumap;
	short menunr, imanr, pad2;
	short curtile; /* the currently active tile of the image when tile is enabled, is kept in sync with the active faces tile */
	int flag;
	short imtypenr, lock;
	short pin, pad3;
	char dt_uv; /* UV draw type */
	char sticky; /* sticky selection type */
	char dt_uvstretch;
	char around;
	
	float xof, yof;					/* user defined offset, image is centered */
	float zoom, pad4;				/* user defined zoom level */
	float centx, centy;				/* storage for offset while render drawing */
	
	struct bGPdata *gpd;			/* grease pencil data */
} SpaceImage;

typedef struct SpaceNla {
	struct SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;

	short blockhandler[8];

	short autosnap;			/* this uses the same settings as autosnap for Action Editor */
	short flag;
	int pad;
	
	struct bDopeSheet *ads;
	View2D v2d;	 /* deprecated, copied to region */
} SpaceNla;

typedef struct SpaceText {
	SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;

	short blockhandler[8];

	struct Text *text;	

	int top, viewlines;
	short flags, menunr;	

	short lheight;		/* user preference */
	char cwidth, linenrs_tot;		/* runtime computed, character width and the number of chars to use when showing line numbers */
	int left;
	int showlinenrs;
	int tabnumber;

	int showsyntax;
	short overwrite;
	short live_edit; /* run python while editing, evil */
	float pix_per_line;

	struct rcti txtscroll, txtbar;

	int wordwrap, doplugins;

	char findstr[256];		/* ST_MAX_FIND_STR */
	char replacestr[256];	/* ST_MAX_FIND_STR */
} SpaceText;

typedef struct Script {
	ID id;

	void *py_draw;
	void *py_event;
	void *py_button;
	void *py_browsercallback;
	void *py_globaldict;

	int flags, lastspace;
	char scriptname[256]; /* store the script file here so we can re-run it on loading blender, if "Enable Scripts" is on */
	char scriptarg[256];
} Script;
#define SCRIPT_SET_NULL(_script) _script->py_draw = _script->py_event = _script->py_button = _script->py_browsercallback = _script->py_globaldict = NULL; _script->flags = 0;
#define SCRIPT_RUNNING	0x01
#define SCRIPT_GUI		0x02
#define SCRIPT_FILESEL	0x04

typedef struct SpaceScript {
	SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;
	struct Script *script;

	short flags, menunr;
	int pad1;
	
	void *but_refs;
} SpaceScript;

typedef struct SpaceTime {
	SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;
	
	View2D v2d; /* deprecated, copied to region */
	
	int flag, redraws;
	
} SpaceTime;

typedef struct SpaceNode {
	SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;
	
	short blockhandler[8];
	
	View2D v2d; /* deprecated, copied to region */
	
	struct ID *id, *from;		/* context, no need to save in file? well... pinning... */
	short flag, menunr;			/* menunr: browse id block in header */
	float aspect;
	void *curfont;
	
	float xof, yof;	/* offset for drawing the backdrop */
	float mx, my;	/* mousepos for drawing socketless link */
	
	struct bNodeTree *nodetree, *edittree;
	int treetype;			/* treetype: as same nodetree->type */
	short texfrom, pad;		/* texfrom object, world or brush */
	
	struct bGPdata *gpd;		/* grease-pencil data */
} SpaceNode;

/* snode->flag */
#define SNODE_BACKDRAW		2
#define SNODE_DISPGP		4

/* snode->texfrom */
#define SNODE_TEX_OBJECT	0
#define SNODE_TEX_WORLD		1
#define SNODE_TEX_BRUSH		2

typedef struct SpaceLogic {
	SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;
	
	short blockhandler[8];
	
	short flag, scaflag;
	int pad;
	
	struct bGPdata *gpd;		/* grease-pencil data */
} SpaceLogic;


typedef struct SpaceImaSel {
	SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;
	
	short blockhandler[8];

	View2D v2d; /* deprecated, copied to region */

	struct FileList *files;

	/* specific stuff for drawing */
	char title[24];
	char dir[240];
	char file[80];

	short type, menu, flag, sort;

	void *curfont;
	int	active_file;

	int numtilesx;
	int numtilesy;

	int selstate;

	struct rcti viewrect;
	struct rcti bookmarkrect;

	float scrollpos; /* current position of scrollhandle */
	float scrollheight; /* height of the scrollhandle */
	float scrollarea; /* scroll region, scrollpos is from 0 to scrollarea */

	float aspect;
	unsigned short retval;		/* event */

	short ipotype;
	
	short filter;
	short active_bookmark;
	short pad, pad1;

	/* view settings */
	short prv_w;
	short prv_h;

	/* one day we'll add unions to dna */
	void (*returnfunc)(char *);
	void (*returnfunc_event)(unsigned short);
	void (*returnfunc_args)(char *, void *, void *);
	
	void *arg1, *arg2;
	short *menup;	/* pointer to menu result or ID browsing */
	char *pupmenu;	/* optional menu in header */

	struct ImBuf *img;
} SpaceImaSel;


typedef struct ConsoleLine {
	struct ConsoleLine *next, *prev;
	
	/* keep these 3 vars so as to share free, realloc funcs */
	int len_alloc;	/* allocated length */
	int len;	/* real len - strlen() */
	char *line; 
	
	int cursor;
	int type; /* only for use when in the 'scrollback' listbase */
} ConsoleLine;

/* ConsoleLine.type */
enum {
	CONSOLE_LINE_OUTPUT=0,
	CONSOLE_LINE_INPUT,
	CONSOLE_LINE_INFO, /* autocomp feedback */
	CONSOLE_LINE_ERROR
};

/* SpaceConsole.rpt_mask */
enum {
	CONSOLE_TYPE_PYTHON=0,
	CONSOLE_TYPE_REPORT,
};

/* SpaceConsole.type see BKE_report.h */
enum {
	CONSOLE_RPT_DEBUG	= 1<<0,
	CONSOLE_RPT_INFO	= 1<<1,
	CONSOLE_RPT_OP		= 1<<2,
	CONSOLE_RPT_WARN	= 1<<3,
	CONSOLE_RPT_ERR		= 1<<4,
};

typedef struct SpaceConsole {
	SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;			// XXX are these needed?
	
	short blockhandler[8];		// XXX are these needed?
	
	/* space vars */
	int type; /* console/report/..? */
	int rpt_mask; /* which reports to display */
	int flag, lheight;

	ListBase scrollback; /* ConsoleLine; output */
	ListBase history; /* ConsoleLine; command history, current edited line is the first */
	char prompt[8];
	
} SpaceConsole;

typedef struct SpaceUserPref {
	SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;

	int pad;
} SpaceUserPref;

/* view3d  Now in DNA_view3d_types.h */



/* **************** SPACE DEFINES ********************* */

/* button defines (deprecated) */
/* warning: the values of these defines are used in sbuts->tabs[8] */
/* sbuts->mainb new */
#define CONTEXT_SCENE	0
#define CONTEXT_OBJECT	1
#define CONTEXT_TYPES	2
#define CONTEXT_SHADING	3
#define CONTEXT_EDITING	4
#define CONTEXT_SCRIPT	5
#define CONTEXT_LOGIC	6

/* sbuts->mainb old (deprecated) */
#define BUTS_VIEW			0
#define BUTS_LAMP			1
#define BUTS_MAT			2
#define BUTS_TEX			3
#define BUTS_ANIM			4
#define BUTS_WORLD			5
#define BUTS_RENDER			6
#define BUTS_EDIT			7
#define BUTS_GAME			8
#define BUTS_FPAINT			9
#define BUTS_RADIO			10
#define BUTS_SCRIPT			11
#define BUTS_SOUND			12
#define BUTS_CONSTRAINT		13
#define BUTS_EFFECTS		14

/* sbuts->tab new (deprecated) */
#define TAB_SHADING_MAT 	0
#define TAB_SHADING_TEX 	1
#define TAB_SHADING_RAD 	2
#define TAB_SHADING_WORLD	3
#define TAB_SHADING_LAMP	4

#define TAB_OBJECT_OBJECT	0
#define TAB_OBJECT_PHYSICS 	1
#define TAB_OBJECT_PARTICLE	2

#define TAB_SCENE_RENDER	0
#define TAB_SCENE_WORLD     	1
#define TAB_SCENE_ANIM		2
#define TAB_SCENE_SOUND		3
#define TAB_SCENE_SEQUENCER	4


/* buts->mainb new */
#define BCONTEXT_SCENE			0
#define BCONTEXT_WORLD				1
#define BCONTEXT_OBJECT				2
#define BCONTEXT_DATA				3
#define BCONTEXT_MATERIAL			4
#define BCONTEXT_TEXTURE			5
#define BCONTEXT_PARTICLE			6
#define BCONTEXT_PHYSICS			7
#define BCONTEXT_BONE				9
#define BCONTEXT_MODIFIER			10
#define BCONTEXT_CONSTRAINT			12
#define BCONTEXT_BONE_CONSTRAINT	13
#define BCONTEXT_TOT				14

/* sbuts->flag */
#define SB_PRV_OSA			1
#define SB_PIN_CONTEXT		2
#define SB_WORLD_TEX		4
#define SB_BRUSH_TEX		8

/* sbuts->align */
#define BUT_FREE  		0
#define BUT_HORIZONTAL  1
#define BUT_VERTICAL    2
#define BUT_AUTO		3

/* sbuts->scaflag */		
#define BUTS_SENS_SEL		1
#define BUTS_SENS_ACT		2
#define BUTS_SENS_LINK		4
#define BUTS_CONT_SEL		8
#define BUTS_CONT_ACT		16
#define BUTS_CONT_LINK		32
#define BUTS_ACT_SEL		64
#define BUTS_ACT_ACT		128
#define BUTS_ACT_LINK		256
#define BUTS_SENS_STATE		512
#define BUTS_ACT_STATE		1024

/* FileSelectParams.display */
enum FileDisplayTypeE {
	FILE_SHORTDISPLAY = 1,
	FILE_LONGDISPLAY,
	FILE_IMGDISPLAY
};

/* FileSelectParams.sort */
enum FileSortTypeE {
	FILE_SORT_NONE = 0,
	FILE_SORT_ALPHA = 1,
	FILE_SORT_EXTENSION,
	FILE_SORT_TIME,
	FILE_SORT_SIZE
};

/* these values need to be hardcoded in structs, dna does not recognize defines */
/* also defined in BKE */
#define FILE_MAXDIR			160
#define FILE_MAXFILE		80
#define FILE_MAX			240

/* filesel types */
#define FILE_UNIX			8
#define FILE_BLENDER		8 /* dont display relative paths */
#define FILE_SPECIAL		9

#define FILE_LOADLIB		1
#define FILE_MAIN			2
#define FILE_LOADFONT		3

/* sfile->flag and simasel->flag */
#define FILE_SHOWSHORT		1
#define FILE_STRINGCODE		2
#define FILE_LINK			4
#define FILE_HIDE_DOT		8
#define FILE_AUTOSELECT		16
#define FILE_ACTIVELAY		32
#define FILE_ATCURSOR		64
#define FILE_SYNCPOSE		128
#define FILE_FILTER			256
#define FILE_BOOKMARKS		512

/* files in filesel list: 2=ACTIVE  */
#define EDITING				1
#define ACTIVE				2
#define BLENDERFILE			4
#define PSXFILE				8
#define IMAGEFILE			16
#define MOVIEFILE			32
#define PYSCRIPTFILE		64
#define FTFONTFILE			128
#define SOUNDFILE			256
#define TEXTFILE			512
#define MOVIEFILE_ICON		1024 /* movie file that preview can't load */
#define FOLDERFILE			2048 /* represents folders for filtering */

/* SpaceImage->dt_uv */
#define SI_UVDT_OUTLINE	0
#define SI_UVDT_DASH	1
#define SI_UVDT_BLACK	2
#define SI_UVDT_WHITE	3

/* SpaceImage->dt_uvstretch */
#define SI_UVDT_STRETCH_ANGLE	0
#define SI_UVDT_STRETCH_AREA	1

/* SpaceImage->sticky
 * Note DISABLE should be 0, however would also need to re-arrange icon order,
 * also, sticky loc is the default mode so this means we dont need to 'do_versons' */
#define SI_STICKY_LOC		0
#define SI_STICKY_DISABLE	1
#define SI_STICKY_VERTEX	2

/* SpaceImage->flag */
#define SI_BE_SQUARE	1<<0
#define SI_EDITTILE		1<<1
#define SI_CLIP_UV		1<<2
#define SI_DRAWTOOL		1<<3
#define SI_DEPRECATED1  1<<4	/* stick UVs to others in the same location */
#define SI_DRAWSHADOW   1<<5
#define SI_SELACTFACE   1<<6	/* deprecated */
#define SI_DEPRECATED2	1<<7
#define SI_DEPRECATED3  1<<8	/* stick UV selection to mesh vertex (UVs wont always be touching) */
#define SI_COORDFLOATS  1<<9
#define SI_PIXELSNAP	1<<10
#define SI_LIVE_UNWRAP	1<<11
#define SI_USE_ALPHA	1<<12
#define SI_SHOW_ALPHA	1<<13
#define SI_SHOW_ZBUF	1<<14
		/* next two for render window dislay */
#define SI_PREVSPACE	1<<15
#define SI_FULLWINDOW	1<<16
#define SI_DEPRECATED4	1<<17
#define SI_DEPRECATED5	1<<18
		/* this means that the image is drawn until it reaches the view edge,
		 * in the image view, its unrelated to the 'tile' mode for texface */
#define SI_DRAW_TILE	1<<19 
#define SI_SMOOTH_UV	1<<20
#define SI_DRAW_STRETCH	1<<21
#define SI_DISPGP		1<<22
#define SI_DRAW_OTHER	1<<23

#define SI_COLOR_CORRECTION	1<<24

/* SpaceIpo->flag (Graph Editor Settings) */
#define SIPO_LOCK_VIEW			(1<<0)
#define SIPO_NOTRANSKEYCULL		(1<<1)
#define SIPO_NOHANDLES			(1<<2)
#define SIPO_NODRAWCFRANUM		(1<<3)
#define SIPO_DRAWTIME			(1<<4)
#define SIPO_SELCUVERTSONLY		(1<<5)
#define SIPO_DRAWNAMES			(1<<6)
#define SIPO_SLIDERS			(1<<7)

/* SpaceIpo->mode (Graph Editor Mode) */
enum {
		/* all animation curves (from all over Blender) */
	SIPO_MODE_ANIMATION	= 0,
		/* drivers only */
	SIPO_MODE_DRIVERS,
} eGraphEdit_Mode;

/* SpaceText flags (moved from DNA_text_types.h) */

#define ST_SCROLL_SELECT        0x0001 // scrollable
#define ST_CLEAR_NAMESPACE      0x0010 // clear namespace after script
                                       // execution (see BPY_main.c)
#define	ST_FIND_WRAP			0x0020
#define	ST_FIND_ALL				0x0040


/* stext->findstr/replacestr */
#define ST_MAX_FIND_STR		256

/* SpaceOops->flag */
#define SO_TESTBLOCKS	1
#define SO_NEWSELECTED	2
#define SO_HIDE_RESTRICTCOLS		4
#define SO_HIDE_KEYINGSETINFO		8

/* SpaceOops->outlinevis */
#define SO_ALL_SCENES	0
#define SO_CUR_SCENE	1
#define SO_VISIBLE		2
#define SO_SELECTED		3
#define SO_ACTIVE		4
#define SO_SAME_TYPE	5
#define SO_GROUPS		6
#define SO_LIBRARIES	7
#define SO_VERSE_SESSION	8
#define SO_VERSE_MS		9
#define SO_SEQUENCE		10
#define SO_DATABLOCKS	11
#define SO_USERDEF		12
#define SO_KEYMAP		13

/* SpaceOops->storeflag */
#define SO_TREESTORE_CLEANUP	1
		/* if set, it allows redraws. gets set for some allqueue events */
#define SO_TREESTORE_REDRAW		2

/* headerbuttons: 450-499 */

#define B_IMASELHOME		451
#define B_IMASELREMOVEBIP	452

#define C_BACK  0xBAAAAA
#define C_DARK  0x665656
#define C_DERK  0x766666
#define C_HI	0xCBBBBB
#define C_LO	0x544444

/* queue settings */
#define IMS_KNOW_WIN        1
#define IMS_KNOW_BIP        2
#define IMS_KNOW_DIR        4
#define IMS_DOTHE_INF		8
#define IMS_KNOW_INF	   16
#define IMS_DOTHE_IMA	   32
#define IMS_KNOW_IMA	   64
#define IMS_FOUND_BIP	  128
#define IMS_DOTHE_BIP	  256
#define IMS_WRITE_NO_BIP  512

/* imasel->mode */
#define IMS_NOIMA			0
#define IMS_IMA				1
#define IMS_ANIM			2
#define IMS_DIR				4
#define IMS_FILE			8
#define IMS_STRINGCODE		16

#define IMS_INDIR			1
#define IMS_INDIRSLI		2
#define IMS_INFILE			3
#define IMS_INFILESLI		4

/* nla->flag */
	// depreceated
#define SNLA_ALLKEYED		(1<<0)
	// depreceated
#define SNLA_ACTIVELAYERS	(1<<1)

#define SNLA_DRAWTIME		(1<<2)
#define SNLA_NOTRANSKEYCULL	(1<<3)
#define SNLA_NODRAWCFRANUM	(1<<4)
#define SNLA_NOSTRIPCURVES	(1<<5)

/* time->flag */
	/* show timing in frames instead of in seconds */
#define TIME_DRAWFRAMES		1
	/* temporary flag set when scrubbing time */
#define TIME_CFRA_NUM		2
	/* only keyframes from active/selected channels get shown */
#define TIME_ONLYACTSEL		4

/* time->redraws */
#define TIME_REGION				1
#define TIME_ALL_3D_WIN			2
#define TIME_ALL_ANIM_WIN		4
#define TIME_ALL_BUTS_WIN		8
#define TIME_WITH_SEQ_AUDIO		16		// deprecated
#define TIME_SEQ				32
#define TIME_ALL_IMAGE_WIN		64
#define TIME_CONTINUE_PHYSICS	128
#define TIME_NODES				256

/* sseq->mainb */
#define SEQ_DRAW_SEQUENCE         0
#define SEQ_DRAW_IMG_IMBUF        1
#define SEQ_DRAW_IMG_WAVEFORM     2
#define SEQ_DRAW_IMG_VECTORSCOPE  3
#define SEQ_DRAW_IMG_HISTOGRAM    4

/* sseq->flag */
#define SEQ_DRAWFRAMES   1
#define SEQ_MARKER_TRANS 2
#define SEQ_DRAW_COLOR_SEPERATED     4
#define SEQ_DRAW_SAFE_MARGINS        8
#define SEQ_DRAW_GPENCIL			16

/* space types, moved from DNA_screen_types.h */
enum {
	SPACE_EMPTY,
	SPACE_VIEW3D,
	SPACE_IPO,
	SPACE_OUTLINER,
	SPACE_BUTS,
	SPACE_FILE,
	SPACE_IMAGE,		
	SPACE_INFO,
	SPACE_SEQ,
	SPACE_TEXT,
	SPACE_IMASEL,
	SPACE_SOUND,
	SPACE_ACTION,
	SPACE_NLA,
	SPACE_SCRIPT,
	SPACE_TIME,
	SPACE_NODE,
	SPACE_LOGIC,
	SPACE_CONSOLE,
	SPACE_USERPREF,
	SPACEICONMAX = SPACE_USERPREF
};

#endif
