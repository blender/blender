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
#include "DNA_oops_types.h"		/* for TreeStoreElem */
#include "DNA_image_types.h"	/* ImageUser */
/* Hum ... Not really nice... but needed for spacebuts. */
#include "DNA_view2d_types.h"

struct Ipo;
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

	/**
	 * The base structure all the other spaces
	 * are derived (implicitly) from. Would be
	 * good to make this explicit.
	 */
typedef struct SpaceLink SpaceLink;
struct SpaceLink {
	SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;
	short blockhandler[8];
};

typedef struct SpaceInfo {
	SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;

	short blockhandler[8];
} SpaceInfo;

typedef struct SpaceIpo {
	SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;

	short blockhandler[8];
	
	unsigned int rowbut, pad2; 
	View2D v2d;
	
	void *editipo;
	ListBase ipokey;
	
	/* the ipo context we need to store */
	struct Ipo *ipo;
	struct ID *from;
	char actname[32], constname[32], bonename[32];

	short totipo, pin;
	short butofs, channel;
	short showkey, blocktype;
	short menunr, lock;
	int flag;
	float median[3];
	rctf tot;
} SpaceIpo;

typedef struct SpaceButs {
	SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;
	struct RenderInfo *ri;

	short blockhandler[8];

	short cursens, curact;
	short align, tabo;		/* align for panels, tab is old tab */
	View2D v2d;
	
	short mainb, menunr;	/* texnr and menunr have to remain shorts */
	short pin, mainbo;	
	void *lockpoin;
	
	short texnr;
	char texfrom, showgroup;
	
	short modeltype;
	short scriptblock;
	short scaflag;
	short re_align;
	
	short oldkeypress;		/* for keeping track of the sub tab key cycling */
	char pad, flag;
	
	char texact, tab[7];	/* storing tabs for each context */
		
} SpaceButs;

typedef struct SpaceSeq {
	SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;

	short blockhandler[8];

	View2D v2d;
	
	float xof, yof;	/* offset for drawing the image preview */
	short mainb, pad;
	short chanshown;
	short zebra;
	int flag;
	float zoom;
} SpaceSeq;

typedef struct SpaceFile {
	SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;
	
	short blockhandler[8];

	struct direntry *filelist;
	int totfile;
	
	char title[24];
	char dir[240];
	char file[80];
	
	short type, ofs, flag, sort;
	short maxnamelen, collums, f_fp, pad1;
	int pad2;
	char fp_str[8];

	struct BlendHandle *libfiledata;
	
	unsigned short retval;		/* event */
	short menu, act, ipotype;
	
	/* one day we'll add unions to dna */
	void (*returnfunc)(char *);
	void (*returnfunc_event)(unsigned short);
	void (*returnfunc_args)(char *, void *, void *);
	
	void *arg1, *arg2;
	short *menup;	/* pointer to menu result or ID browsing */
	char *pupmenu;	/* optional menu in header */
} SpaceFile;

typedef struct SpaceOops {
	SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;

	short blockhandler[8];

	View2D v2d;
	
	ListBase oops;
	short pin, visiflag, flag, rt;
	void *lockpoin;
	
	ListBase tree;
	struct TreeStore *treestore;
	
	/* search stuff */
	char search_string[32];
	struct TreeStoreElem search_tse;
	int search_flags, do_;
	
	short type, outlinevis, storeflag;
	short deps_flags;
	
} SpaceOops;

typedef struct SpaceImage {
	SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;

	short blockhandler[8];

	View2D v2d;
	
	struct Image *image;
	struct ImageUser iuser;
	
	struct CurveMapping *cumap;
	short mode, menunr;
	short imanr;
	short curtile; /* the currently active tile of the image when tile is enabled, is kept in sync with the active faces tile */
	int flag;
	short selectmode;
	short imtypenr, lock;
	short pin;
	float zoom;
	char dt_uv; /* UV draw type */
	char sticky; /* sticky selection type */
	char dt_uvstretch;
	char pad[5];
	
	float xof, yof;					/* user defined offset, image is centered */
	float centx, centy;				/* storage for offset while render drawing */
	
} SpaceImage;

typedef struct SpaceNla {
	struct SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;

	short blockhandler[8];

	short menunr, lock;
	short autosnap;			/* this uses the same settings as autosnap for Action Editor */
	short flag;
	
	View2D v2d;	
} SpaceNla;

typedef struct SpaceText {
	SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;

	short blockhandler[8];

	struct Text *text;	

	int top, viewlines;
	short flags, menunr;
	
	int font_id;	
	int lheight;
	int left;
	int showlinenrs;
	
	int tabnumber;
	int currtab_set; 
	int showsyntax;
	int unused_padd;
	
	float pix_per_line;

	struct rcti txtscroll, txtbar;

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
	int spacetype;
	float blockscale;
	struct ScrArea *area;
	struct Script *script;

	short flags, menunr;
	int pad1;
	
	void *but_refs;
} SpaceScript;

typedef struct SpaceTime {
	SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;
	
	View2D v2d;
	
	int flag, redraws;
	
} SpaceTime;

typedef struct SpaceNode {
	SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;
	
	View2D v2d;
	
	struct ID *id, *from;		/* context, no need to save in file? well... pinning... */
	short flag, menunr;			/* menunr: browse id block in header */
	float aspect;
	void *curfont;
	
	float xof, yof;	/* offset for drawing the backdrop */
	
	struct bNodeTree *nodetree, *edittree;
	int treetype, pad;			/* treetype: as same nodetree->type */
	
} SpaceNode;

/* snode->flag */
#define SNODE_DO_PREVIEW	1
#define SNODE_BACKDRAW		2

typedef struct SpaceImaSel {
	SpaceLink *next, *prev;
	int spacetype;
	float blockscale;
	struct ScrArea *area;
	
	short blockhandler[8];

	View2D v2d;

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


/* **************** SPACE ********************* */


/* view3d  Now in DNA_view3d_types.h */

/* button defines in BIF_butspace.h */

/* sbuts->flag */
#define SB_PRV_OSA			1

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

/* sfile->sort */
#define FILE_SORTALPHA		0
#define FILE_SORTDATE		1
#define FILE_SORTSIZE		2
#define FILE_SORTEXTENS		3

/* files in filesel list: 2=ACTIVE  */
#define HILITE				1
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

#define SCROLLH	16			/* height scrollbar */
#define SCROLLB	16			/* width scrollbar */

/* SpaceImage->mode */
#define SI_TEXTURE		0
#define SI_SHOW			1

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

/* SpaceImage->selectmode */
#define SI_SELECT_VERTEX	0
#define SI_SELECT_EDGE		1 /* not implemented */
#define SI_SELECT_FACE		2
#define SI_SELECT_ISLAND	3

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
#define SI_SYNC_UVSEL	1<<17
#define SI_LOCAL_UV		1<<18
		/* this means that the image is drawn until it reaches the view edge,
		 * in the image view, its unrelated to the 'tile' mode for texface */
#define SI_DRAW_TILE	1<<19 
#define SI_SMOOTH_UV	1<<20
#define SI_DRAW_STRETCH	1<<21

/* SpaceIpo->flag */
#define SIPO_LOCK_VIEW			1<<0
#define SIPO_NOTRANSKEYCULL		1<<1

/* SpaceText flags (moved from DNA_text_types.h) */

#define ST_SCROLL_SELECT        0x0001 // scrollable
#define ST_CLEAR_NAMESPACE      0x0010 // clear namespace after script
                                       // execution (see BPY_main.c)

/* SpaceOops->type */
#define SO_OOPS			0
#define SO_OUTLINER		1
#define SO_DEPSGRAPH    2

/* SpaceOops->flag */
#define SO_TESTBLOCKS	1
#define SO_NEWSELECTED	2
#define SO_HIDE_RESTRICTCOLS		4

/* SpaceOops->visiflag */
#define OOPS_SCE	1
#define OOPS_OB		2
#define OOPS_ME		4
#define OOPS_CU		8
#define OOPS_MB		16
#define OOPS_LT		32
#define OOPS_LA		64
#define OOPS_MA		128
#define OOPS_TE		256
#define OOPS_IP		512
#define OOPS_LAY	1024
#define OOPS_LI		2048
#define OOPS_IM		4096
#define OOPS_AR		8192
#define OOPS_GR		16384
#define OOPS_CA		32768

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
#define SNLA_ALLKEYED		1
#define SNLA_ACTIVELAYERS	2
#define SNLA_DRAWTIME		4
#define SNLA_NOTRANSKEYCULL	8

/* time->flag */
	/* show timing in frames instead of in seconds */
#define TIME_DRAWFRAMES		1
	/* temporary flag set when scrubbing time */
#define TIME_CFRA_NUM		2
	/* only keyframes from active/selected channels get shown */
#define TIME_ONLYACTSEL		4

/* time->redraws */
#define TIME_LEFTMOST_3D_WIN	1
#define TIME_ALL_3D_WIN			2
#define TIME_ALL_ANIM_WIN		4
#define TIME_ALL_BUTS_WIN		8
#define TIME_WITH_SEQ_AUDIO		16
#define TIME_SEQ				32
#define TIME_ALL_IMAGE_WIN		64
#define TIME_CONTINUE_PHYSICS	128

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

/* space types, moved from DNA_screen_types.h */
enum {
	SPACE_EMPTY,
	SPACE_VIEW3D,
	SPACE_IPO,
	SPACE_OOPS,
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
	SPACEICONMAX = SPACE_NODE
/*	SPACE_LOGIC	*/
};

#endif
