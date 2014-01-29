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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_windowmanager_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_WINDOWMANAGER_TYPES_H__
#define __DNA_WINDOWMANAGER_TYPES_H__

#include "DNA_listBase.h"
#include "DNA_vec_types.h"

#include "DNA_ID.h"

/* defined here: */
struct wmWindowManager;
struct wmWindow;

struct wmEvent;
struct wmGesture;
struct wmOperatorType;
struct wmOperator;
struct wmKeyMap;
struct wmKeyConfig;

/* forwards */
struct bContext;
struct wmLocal;
struct bScreen;
struct uiBlock;
struct wmSubWindow;
struct wmTimer;
struct StructRNA;
struct PointerRNA;
struct ReportList;
struct Report;
struct uiLayout;

#define OP_MAX_TYPENAME 64
#define KMAP_MAX_NAME   64

/* keep in sync with 'wm_report_items' in wm_rna.c */
typedef enum ReportType {
	RPT_DEBUG                   = (1 << 0),
	RPT_INFO                    = (1 << 1),
	RPT_OPERATOR                = (1 << 2),
	RPT_PROPERTY                = (1 << 3),
	RPT_WARNING                 = (1 << 4),
	RPT_ERROR                   = (1 << 5),
	RPT_ERROR_INVALID_INPUT     = (1 << 6),
	RPT_ERROR_INVALID_CONTEXT   = (1 << 7),
	RPT_ERROR_OUT_OF_MEMORY     = (1 << 8),
} ReportType;

#define RPT_DEBUG_ALL       (RPT_DEBUG)
#define RPT_INFO_ALL        (RPT_INFO)
#define RPT_OPERATOR_ALL    (RPT_OPERATOR)
#define RPT_PROPERTY_ALL    (RPT_PROPERTY)
#define RPT_WARNING_ALL     (RPT_WARNING)
#define RPT_ERROR_ALL       (RPT_ERROR|RPT_ERROR_INVALID_INPUT|RPT_ERROR_INVALID_CONTEXT|RPT_ERROR_OUT_OF_MEMORY)

enum ReportListFlags {
	RPT_PRINT   = (1 << 0),
	RPT_STORE   = (1 << 1),
	RPT_FREE    = (1 << 2),
	RPT_OP_HOLD = (1 << 3), /* don't move them into the operator global list (caller will use) */
};

/* These two Lines with # tell makesdna this struct can be excluded. */
#
#
typedef struct Report {
	struct Report *next, *prev;
	short type;  /* ReportType */
	short flag;
	int len;  /* strlen(message), saves some time calculating the word wrap  */
	const char *typestr;
	const char *message;
} Report;

/* saved in the wm, don't remove */
typedef struct ReportList {
	ListBase list;
	int printlevel;  /* ReportType */
	int storelevel;  /* ReportType */
	int flag, pad;
	struct wmTimer *reporttimer;
} ReportList;

/* timer customdata to control reports display */
/* These two Lines with # tell makesdna this struct can be excluded. */
#
#
typedef struct ReportTimerInfo {
	float col[3];
	float grayscale;
	float widthfac;
} ReportTimerInfo;

/* reports need to be before wmWindowManager */


/* windowmanager is saved, tag WMAN */
typedef struct wmWindowManager {
	ID id;

	struct wmWindow *windrawable, *winactive;  /* separate active from drawable */
	ListBase windows;

	int initialized;                  /* set on file read */
	short file_saved;                 /* indicator whether data was saved */
	short op_undo_depth;              /* operator stack depth to avoid nested undo pushes */

	ListBase operators;               /* operator registry */

	ListBase queue;                   /* refresh/redraw wmNotifier structs */

	struct ReportList reports;        /* information and error reports */

	ListBase jobs;                    /* threaded jobs manager */

	ListBase paintcursors;            /* extra overlay cursors to draw, like circles */

	ListBase drags;                   /* active dragged items */

	ListBase keyconfigs;              /* known key configurations */
	struct wmKeyConfig *defaultconf;  /* default configuration */
	struct wmKeyConfig *addonconf;    /* addon configuration */
	struct wmKeyConfig *userconf;     /* user configuration */

	ListBase timers;                  /* active timers */
	struct wmTimer *autosavetimer;    /* timer for auto save */

	char is_interface_locked;		/* indicates whether interface is locked for user interaction */
	char par[7];
} wmWindowManager;

/* wmWindowManager.initialized */
enum {
	WM_INIT_WINDOW = (1<<0),
	WM_INIT_KEYMAP = (1<<1),
};

/* the savable part, rest of data is local in ghostwinlay */
typedef struct wmWindow {
	struct wmWindow *next, *prev;

	void *ghostwin;             /* don't want to include ghost.h stuff */

	int winid;                  /* winid also in screens, is for retrieving this window after read */

	short grabcursor;           /* cursor grab mode */
	short pad;

	struct bScreen *screen;     /* active screen */
	struct bScreen *newscreen;  /* temporary when switching */
	char screenname[64];        /* MAX_ID_NAME for matching window with active screen after file read */

	short posx, posy, sizex, sizey;  /* window coords */
	short windowstate;  /* borderless, full */
	short monitor;      /* multiscreen... no idea how to store yet */
	short active;       /* set to 1 if an active window, for quick rejects */
	short cursor;       /* current mouse cursor type */
	short lastcursor;   /* previous cursor when setting modal one */
	short modalcursor;  /* the current modal cursor */
	short addmousemove; /* internal: tag this for extra mousemove event, makes cursors/buttons active on UI switching */
	short pad2;

	struct wmEvent *eventstate;   /* storage for event system */

	struct wmSubWindow *curswin;  /* internal for wm_subwindow.c only */

	struct wmGesture *tweak;      /* internal for wm_operators.c */

	int drawmethod, drawfail;     /* internal for wm_draw.c only */
	void *drawdata;               /* internal for wm_draw.c only */

	ListBase queue;               /* all events (ghost level events were handled) */
	ListBase handlers;            /* window+screen handlers, handled last */
	ListBase modalhandlers;       /* priority handlers, handled first */

	ListBase subwindows;          /* opengl stuff for sub windows, see notes in wm_subwindow.c */
	ListBase gesture;             /* gesture stuff */
} wmWindow;

/* These two Lines with # tell makesdna this struct can be excluded. */
/* should be something like DNA_EXCLUDE 
 * but the preprocessor first removes all comments, spaces etc */
#
#
typedef struct wmOperatorTypeMacro {
	struct wmOperatorTypeMacro *next, *prev;

	/* operator id */
	char idname[64];
	/* rna pointer to access properties, like keymap */
	struct IDProperty *properties;  /* operator properties, assigned to ptr->data and can be written to a file */
	struct PointerRNA *ptr;
} wmOperatorTypeMacro;

/* partial copy of the event, for matching by eventhandler */
typedef struct wmKeyMapItem {
	struct wmKeyMapItem *next, *prev;

	/* operator */
	char idname[64];                /* used to retrieve operator type pointer */
	IDProperty *properties;         /* operator properties, assigned to ptr->data and can be written to a file */

	/* modal */
	char propvalue_str[64];         /* runtime temporary storage for loading */
	short propvalue;                /* if used, the item is from modal map */

	/* event */
	short type;                     /* event code itself */
	short val;                      /* KM_ANY, KM_PRESS, KM_NOTHING etc */
	short shift, ctrl, alt, oskey;  /* oskey is apple or windowskey, value denotes order of pressed */
	short keymodifier;              /* rawkey modifier */

	/* flag: inactive, expanded */
	short flag;

	/* runtime */
	short maptype;                  /* keymap editor */
	short id;                       /* unique identifier. Positive for kmi that override builtins, negative otherwise */
	short pad;
	struct PointerRNA *ptr;         /* rna pointer to access properties */
} wmKeyMapItem;

/* used instead of wmKeyMapItem for diff keymaps */
typedef struct wmKeyMapDiffItem {
	struct wmKeyMapDiffItem *next, *prev;

	wmKeyMapItem *remove_item;
	wmKeyMapItem *add_item;
} wmKeyMapDiffItem;

/* wmKeyMapItem.flag */
enum {
	KMI_INACTIVE       = (1 << 0),
	KMI_EXPANDED       = (1 << 1),
	KMI_USER_MODIFIED  = (1 << 2),
	KMI_UPDATE         = (1 << 3),
};

/* wmKeyMapItem.maptype */
enum {
	KMI_TYPE_KEYBOARD   = 0,
	KMI_TYPE_MOUSE      = 1,
	KMI_TYPE_TWEAK      = 2,
	KMI_TYPE_TEXTINPUT  = 3,
	KMI_TYPE_TIMER      = 4,
	KMI_TYPE_NDOF       = 5,
};

/* stored in WM, the actively used keymaps */
typedef struct wmKeyMap {
	struct wmKeyMap *next, *prev;

	ListBase items;
	ListBase diff_items;

	char idname[64];  /* global editor keymaps, or for more per space/region */
	short spaceid;    /* same IDs as in DNA_space_types.h */
	short regionid;   /* see above */

	short flag;       /* general flags */
	short kmi_id;     /* last kmi id */

	/* runtime */
	int (*poll)(struct bContext *);  /* verify if enabled in the current context */
	void *modal_items;               /* for modal, EnumPropertyItem for now */
} wmKeyMap;

/* wmKeyMap.flag */
enum {
	KEYMAP_MODAL              = (1 << 0),  /* modal map, not using operatornames */
	KEYMAP_USER               = (1 << 1),  /* user keymap */
	KEYMAP_EXPANDED           = (1 << 2),
	KEYMAP_CHILDREN_EXPANDED  = (1 << 3),
	KEYMAP_DIFF               = (1 << 4),  /* diff keymap for user preferences */
	KEYMAP_USER_MODIFIED      = (1 << 5),  /* keymap has user modifications */
	KEYMAP_UPDATE             = (1 << 6),
};

typedef struct wmKeyConfig {
	struct wmKeyConfig *next, *prev;

	char idname[64];    /* unique name */
	char basename[64];  /* idname of configuration this is derives from, "" if none */

	ListBase keymaps;
	int actkeymap, flag;
} wmKeyConfig;

/* wmKeyConfig.flag */
enum {
	KEYCONF_USER          = (1 << 1),  /* And what about (1 << 0)? */
	KEYCONF_INIT_DEFAULT  = (1 << 2),
};

/* this one is the operator itself, stored in files for macros etc */
/* operator + operatortype should be able to redo entirely, but for different contextes */
typedef struct wmOperator {
	struct wmOperator *next, *prev;

	/* saved */
	char idname[64];              /* used to retrieve type pointer */
	IDProperty *properties;       /* saved, user-settable properties */

	/* runtime */
	struct wmOperatorType *type;  /* operator type definition from idname */
	void *customdata;             /* custom storage, only while operator runs */
	void *py_instance;            /* python stores the class instance here */

	struct PointerRNA *ptr;       /* rna pointer to access properties */
	struct ReportList *reports;   /* errors and warnings storage */

	ListBase macro;               /* list of operators, can be a tree */
	struct wmOperator *opm;       /* current running macro, not saved */
	struct uiLayout *layout;      /* runtime for drawing */
	short flag, pad[3];

} wmOperator;

/* operator type return flags: exec(), invoke() modal(), return values */
enum {
	OPERATOR_RUNNING_MODAL  = (1 << 0),
	OPERATOR_CANCELLED      = (1 << 1),
	OPERATOR_FINISHED       = (1 << 2),
/* add this flag if the event should pass through */
	OPERATOR_PASS_THROUGH   = (1 << 3),
/* in case operator got executed outside WM code... like via fileselect */
	OPERATOR_HANDLED        = (1 << 4),
};
#define OPERATOR_FLAGS_ALL    (OPERATOR_RUNNING_MODAL | OPERATOR_CANCELLED | OPERATOR_FINISHED | \
                               OPERATOR_PASS_THROUGH | OPERATOR_HANDLED)

/* sanity checks for debug mode only */
#define OPERATOR_RETVAL_CHECK(ret) (void)ret, BLI_assert(ret != 0 && (ret & OPERATOR_FLAGS_ALL) == ret)

/* wmOperator flag */
enum {
	OP_GRAB_POINTER    = (1 << 0),
	/* low level flag so exec() operators can tell if they were invoked, use with care.
	 * typically this shouldn't make any difference, but it rare cases its needed (see smooth-view) */
	OP_IS_INVOKE       = (1 << 1),
};

#endif /* __DNA_WINDOWMANAGER_TYPES_H__ */
