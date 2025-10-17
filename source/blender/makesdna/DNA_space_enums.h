/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/** \file
 * \ingroup DNA
 *
 * Enums for space type in the user interface.
 */

#include "BLI_enum_flags.hh"

#pragma once

/* -------------------------------------------------------------------- */
/** \name SpaceLink (Base)
 * \{ */

/** #SpaceLink.link_flag */
enum {
  /**
   * The space is not a regular one opened through the editor menu (for example) but spawned by an
   * operator to fulfill some task and then disappear again.
   * Can typically be canceled using Escape, but that is handled on the editor level.
   */
  SPACE_FLAG_TYPE_TEMPORARY = (1 << 0),
  /**
   * Used to mark a space as active but "overlapped" by temporary full-screen spaces. Without this
   * we wouldn't be able to restore the correct active space after closing temp full-screens
   * reliably if the same space type is opened twice in a full-screen stack (see #19296). We don't
   * actually open the same space twice, we have to pretend it is by managing area order carefully.
   */
  SPACE_FLAG_TYPE_WAS_ACTIVE = (1 << 1),
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space Info
 * \{ */

/** #SpaceInfo.rpt_mask */
typedef enum eSpaceInfo_RptMask {
  INFO_RPT_DEBUG = (1 << 0),
  INFO_RPT_INFO = (1 << 1),
  INFO_RPT_OP = (1 << 2),
  INFO_RPT_WARN = (1 << 3),
  INFO_RPT_ERR = (1 << 4),
} eSpaceInfo_RptMask;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Properties Editor
 * \{ */

/* button defines (deprecated) */
#ifdef DNA_DEPRECATED_ALLOW
/* WARNING: the values of these defines are used in SpaceProperties.tabs[8] */
/** #SpaceProperties::mainb new */
enum {
  CONTEXT_SCENE = 0,
  CONTEXT_OBJECT = 1,
  // CONTEXT_TYPES = 2,
  CONTEXT_SHADING = 3,
  CONTEXT_EDITING = 4,
  // CONTEXT_SCRIPT = 5,
  // CONTEXT_LOGIC = 6,
};

/** #SpaceProperties::mainb old (deprecated) */
enum {
  // BUTS_VIEW = 0,
  BUTS_LAMP = 1,
  BUTS_MAT = 2,
  BUTS_TEX = 3,
  BUTS_ANIM = 4,
  BUTS_WORLD = 5,
  BUTS_RENDER = 6,
  BUTS_EDIT = 7,
  // BUTS_GAME = 8,
  BUTS_FPAINT = 9,
  BUTS_RADIO = 10,
  BUTS_SCRIPT = 11,
  // BUTS_SOUND = 12,
  BUTS_CONSTRAINT = 13,
  // BUTS_EFFECTS = 14,
};
#endif /* DNA_DEPRECATED_ALLOW */

/** #SpaceProperties.mainb new */
typedef enum eSpaceButtons_Context {
  BCONTEXT_SEPARATOR = -1,
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
  BCONTEXT_SHADERFX = 15,
  BCONTEXT_OUTPUT = 16,
  BCONTEXT_COLLECTION = 17,
  BCONTEXT_STRIP = 18,
  BCONTEXT_STRIP_MODIFIER = 19,

  /* Keep last. */
  BCONTEXT_TOT,
} eSpaceButtons_Context;

/** #SpaceProperties.flag */
typedef enum eSpaceButtons_Flag {
  /* SB_PRV_OSA = (1 << 0), */ /* UNUSED */
  SB_PIN_CONTEXT = (1 << 1),
  SB_FLAG_UNUSED_2 = (1 << 2),
  SB_FLAG_UNUSED_3 = (1 << 3),
  /** Do not add materials, particles, etc. in TemplateTextureUser list. */
  SB_TEX_USER_LIMITED = (1 << 3),
  SB_SHADING_CONTEXT = (1 << 4),
} eSpaceButtons_Flag;

/** #SpaceProperties.outliner_sync */
typedef enum eSpaceButtons_OutlinerSync {
  PROPERTIES_SYNC_AUTO = 0,
  PROPERTIES_SYNC_NEVER = 1,
  PROPERTIES_SYNC_ALWAYS = 2,
} eSpaceButtons_OutlinerSync;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Outliner
 * \{ */

/** #SpaceOutliner.flag */
typedef enum eSpaceOutliner_Flag {
  /* SO_TESTBLOCKS = (1 << 0), */         /* UNUSED */
  /* SO_NEWSELECTED = (1 << 1), */        /* UNUSED */
  SO_FLAG_UNUSED_1 = (1 << 2),            /* cleared */
  /* SO_HIDE_KEYINGSETINFO = (1 << 3), */ /* UNUSED */
  SO_SKIP_SORT_ALPHA = (1 << 4),
  SO_SYNC_SELECT = (1 << 5),
  SO_MODE_COLUMN = (1 << 6),
} eSpaceOutliner_Flag;

/** #SpaceOutliner.filter */
typedef enum eSpaceOutliner_Filter {
  SO_FILTER_SEARCH = (1 << 0), /* Run-time flag. */
  SO_FILTER_CLEARED_1 = (1 << 1),
  SO_FILTER_NO_OBJECT = (1 << 2),
  SO_FILTER_NO_OB_CONTENT = (1 << 3), /* Not only mesh, but modifiers, constraints, ... */
  SO_FILTER_NO_CHILDREN = (1 << 4),

  SO_FILTER_UNUSED_5 = (1 << 5), /* cleared */
  /** Show overrides that are defined/controlled by Blender. */
  SO_FILTER_SHOW_SYSTEM_OVERRIDES = SO_FILTER_UNUSED_5, /* re-use */
  SO_FILTER_NO_OB_MESH = (1 << 6),
  SO_FILTER_NO_OB_ARMATURE = (1 << 7),
  SO_FILTER_NO_OB_EMPTY = (1 << 8),
  SO_FILTER_NO_OB_LAMP = (1 << 9),
  SO_FILTER_NO_OB_CAMERA = (1 << 10),
  SO_FILTER_NO_OB_OTHERS = (1 << 11),

  SO_FILTER_OB_STATE_SELECTABLE = (1 << 12), /* Not set via DNA. */
  SO_FILTER_OB_STATE_VISIBLE = (1 << 13),    /* Not set via DNA. */
  SO_FILTER_OB_STATE_INVERSE = (1 << 14),
  SO_FILTER_OB_STATE_SELECTED = (1 << 15), /* Not set via DNA. */
  SO_FILTER_OB_STATE_ACTIVE = (1 << 16),   /* Not set via DNA. */
  SO_FILTER_NO_COLLECTION = (1 << 17),
  SO_FILTER_NO_VIEW_LAYERS = (1 << 18),

  SO_FILTER_ID_TYPE = (1 << 19),

  SO_FILTER_NO_OB_GREASE_PENCIL = (1 << 20),
} eSpaceOutliner_Filter;

#define SO_FILTER_OB_TYPE \
  (SO_FILTER_NO_OB_MESH | SO_FILTER_NO_OB_ARMATURE | SO_FILTER_NO_OB_EMPTY | \
   SO_FILTER_NO_OB_LAMP | SO_FILTER_NO_OB_CAMERA | SO_FILTER_NO_OB_GREASE_PENCIL | \
   SO_FILTER_NO_OB_OTHERS)

#define SO_FILTER_OB_STATE \
  (SO_FILTER_OB_STATE_VISIBLE | SO_FILTER_OB_STATE_SELECTED | SO_FILTER_OB_STATE_ACTIVE | \
   SO_FILTER_OB_STATE_SELECTABLE)

#define SO_FILTER_ANY \
  (SO_FILTER_NO_OB_CONTENT | SO_FILTER_NO_CHILDREN | SO_FILTER_OB_TYPE | SO_FILTER_OB_STATE | \
   SO_FILTER_NO_COLLECTION | SO_FILTER_NO_VIEW_LAYERS)

/** #SpaceOutliner.filter_state */
typedef enum eSpaceOutliner_StateFilter {
  SO_FILTER_OB_ALL = 0,
  SO_FILTER_OB_VISIBLE = 1,
  SO_FILTER_OB_HIDDEN = 2, /* deprecated */
  SO_FILTER_OB_SELECTED = 3,
  SO_FILTER_OB_ACTIVE = 4,
  SO_FILTER_OB_SELECTABLE = 5,
} eSpaceOutliner_StateFilter;

/** #SpaceOutliner.show_restrict_flags */
typedef enum eSpaceOutliner_ShowRestrictFlag {
  SO_RESTRICT_ENABLE = (1 << 0),
  SO_RESTRICT_SELECT = (1 << 1),
  SO_RESTRICT_HIDE = (1 << 2),
  SO_RESTRICT_VIEWPORT = (1 << 3),
  SO_RESTRICT_RENDER = (1 << 4),
  SO_RESTRICT_HOLDOUT = (1 << 5),
  SO_RESTRICT_INDIRECT_ONLY = (1 << 6),
} eSpaceOutliner_Restrict;

/** #SpaceOutliner.outlinevis */
typedef enum eSpaceOutliner_Mode {
  SO_SCENES = 0,
  /* SO_CUR_SCENE      = 1, */ /* deprecated! */
  /* SO_VISIBLE        = 2, */ /* deprecated! */
  /* SO_SELECTED       = 3, */ /* deprecated! */
  /* SO_ACTIVE         = 4, */ /* deprecated! */
  /* SO_SAME_TYPE      = 5, */ /* deprecated! */
  /* SO_GROUPS         = 6, */ /* deprecated! */
  SO_LIBRARIES = 7,
  /* SO_VERSE_SESSION  = 8, */ /* deprecated! */
  /* SO_VERSE_MS       = 9, */ /* deprecated! */
  SO_SEQUENCE = 10,
  SO_DATA_API = 11,
  /* SO_USERDEF        = 12, */ /* deprecated! */
  /* SO_KEYMAP         = 13, */ /* deprecated! */
  SO_ID_ORPHANS = 14,
  SO_VIEW_LAYER = 15,
  SO_OVERRIDES_LIBRARY = 16,
} eSpaceOutliner_Mode;

/** #SpaceOutliner.outlinevis */
typedef enum eSpaceOutliner_LibOverrideViewMode {
  /** View all overrides with RNA buttons to edit the overridden values. */
  SO_LIB_OVERRIDE_VIEW_PROPERTIES = 0,
  /** View entire override hierarchies (relationships between overridden data-blocks). */
  SO_LIB_OVERRIDE_VIEW_HIERARCHIES = 1,
} eSpaceOutliner_LibOverrideViewMode;

/** #SpaceOutliner.storeflag */
typedef enum eSpaceOutliner_StoreFlag {
  /* cleanup tree */
  SO_TREESTORE_CLEANUP = (1 << 0),
  SO_TREESTORE_UNUSED_1 = (1 << 1), /* cleared */
  /** Rebuild the tree, similar to cleanup, but defer a call to
   * bke::outliner::treehash::rebuild_from_treestore instead. */
  SO_TREESTORE_REBUILD = (1 << 2),
} eSpaceOutliner_StoreFlag;

/** Outliner search flags (#SpaceOutliner.search_flags) */
typedef enum eSpaceOutliner_Search_Flags {
  SO_FIND_CASE_SENSITIVE = (1 << 0),
  SO_FIND_COMPLETE = (1 << 1),
  SO_SEARCH_RECURSIVE = (1 << 2),
} eSpaceOutliner_Search_Flags;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Graph Editor
 * \{ */

/** #SpaceGraph.flag (Graph Editor Settings) */
typedef enum eGraphEdit_Flag {
  /* OLD DEPRECATED SETTING */
  /* SIPO_LOCK_VIEW            = (1 << 0), */

  /* don't merge keyframes on the same frame after a transform */
  SIPO_NOTRANSKEYCULL = (1 << 1),
  /* don't show any keyframe handles at all */
  SIPO_NOHANDLES = (1 << 2),
  /* Automatically lock the transform to whichever axis the cursor has moved the most. */
  SIPO_AUTOLOCK_AXIS = (1 << 3),
  /* show timing in seconds instead of frames */
  SIPO_DRAWTIME = (1 << 4),
  /* draw names of F-Curves beside the respective curves */
  /* NOTE: currently not used */
  /* SIPO_DRAWNAMES = (1 << 6), */ /* UNUSED */
  /* show sliders in channels list */
  SIPO_SLIDERS = (1 << 7),
  /* don't show the horizontal component of the cursor */
  SIPO_NODRAWCURSOR = (1 << 8),
  /* only show handles of selected keyframes */
  SIPO_SELVHANDLESONLY = (1 << 9),
  /* don't perform realtime updates */
  SIPO_NOREALTIMEUPDATES = (1 << 11),
  /* draw grouped channels with colors set in group */
  /* SIPO_NODRAWGCOLORS = (1 << 13), DEPRECATED */
  /* normalize curves on display */
  SIPO_NORMALIZE = (1 << 14),
  SIPO_NORMALIZE_FREEZE = (1 << 15),
  /* show markers region */
  SIPO_SHOW_MARKERS = (1 << 16),
  SIPO_NO_DRAW_EXTRAPOLATION = (1 << 17),
} eGraphEdit_Flag;

/** #SpaceGraph.mode (Graph Editor Mode) */
typedef enum eGraphEdit_Mode {
  /* all animation curves (from all over Blender) */
  SIPO_MODE_ANIMATION = 0,
  /* drivers only */
  SIPO_MODE_DRIVERS = 1,
} eGraphEdit_Mode;

typedef enum eGraphEdit_Runtime_Flag {
  /** Temporary flag to force channel selections to be synced with main. */
  SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC = (1 << 0),
  /** Temporary flag to force F-Curves to recalculate colors. */
  SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC_COLOR = (1 << 1),

  /**
   * These flags are for the mouse-select code to communicate with the transform code. Click
   * dragging (tweaking) a handle sets the according left/right flag which transform code uses then
   * to limit translation to this side. */
  SIPO_RUNTIME_FLAG_TWEAK_HANDLES_LEFT = (1 << 2),
  SIPO_RUNTIME_FLAG_TWEAK_HANDLES_RIGHT = (1 << 3),
} eGraphEdit_Runtime_Flag;

/** \} */

/* -------------------------------------------------------------------- */
/** \name NLA Editor
 * \{ */

/** #SpaceNla.flag */
typedef enum eSpaceNla_Flag {
  SNLA_FLAG_UNUSED_0 = (1 << 0),
  SNLA_FLAG_UNUSED_1 = (1 << 1),
  /* draw timing in seconds instead of frames */
  SNLA_DRAWTIME = (1 << 2),
  SNLA_FLAG_UNUSED_3 = (1 << 3),
  /* SNLA_NODRAWCFRANUM = (1 << 4), DEPRECATED */
  /* don't draw influence curves on strips */
  SNLA_NOSTRIPCURVES = (1 << 5),
  /* don't perform realtime updates */
  SNLA_NOREALTIMEUPDATES = (1 << 6),
  /* don't show local strip marker indications */
  SNLA_NOLOCALMARKERS = (1 << 7),
  /* show markers region */
  SNLA_SHOW_MARKERS = (1 << 8),
} eSpaceNla_Flag;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sequence Editor
 * \{ */

/** #SequencerPreviewOverlay.flag */
typedef enum eSpaceSeq_SequencerPreviewOverlay_Flag {
  SEQ_PREVIEW_SHOW_2D_CURSOR = (1 << 1),
  SEQ_PREVIEW_SHOW_OUTLINE_SELECTED = (1 << 2),
  SEQ_PREVIEW_SHOW_SAFE_MARGINS = (1 << 3),
  SEQ_PREVIEW_SHOW_GPENCIL = (1 << 4),
  SEQ_PREVIEW_SHOW_SAFE_CENTER = (1 << 9),
  SEQ_PREVIEW_SHOW_METADATA = (1 << 10),
} eSpaceSeq_SequencerPreviewOverlay_Flag;

/** #SequencerTimelineOverlay.flag */
typedef enum eSpaceSeq_SequencerTimelineOverlay_Flag {
  SEQ_TIMELINE_SHOW_STRIP_OFFSETS = (1 << 1),
  SEQ_TIMELINE_SHOW_THUMBNAILS = (1 << 2),
  /** Use #Strip::color_tag */
  SEQ_TIMELINE_SHOW_STRIP_COLOR_TAG = (1 << 3),
  SEQ_TIMELINE_SHOW_STRIP_RETIMING = (1 << 4),
  SEQ_TIMELINE_SHOW_FCURVES = (1 << 5),
  /** Draw all wave-forms. */
  SEQ_TIMELINE_ALL_WAVEFORMS = (1 << 7),
  /** Draw no wave-forms. */
  SEQ_TIMELINE_NO_WAVEFORMS = (1 << 8),
  /** Draw only upper part of the waveform, showing absolute signal value. */
  SEQ_TIMELINE_WAVEFORMS_HALF = (1 << 9),
  SEQ_TIMELINE_SHOW_STRIP_NAME = (1 << 14),
  SEQ_TIMELINE_SHOW_STRIP_SOURCE = (1 << 15),
  SEQ_TIMELINE_SHOW_STRIP_DURATION = (1 << 16),
  SEQ_TIMELINE_SHOW_GRID = (1 << 18),
} eSpaceSeq_SequencerTimelineOverlay_Flag;

/** #SequencerCacheOverlay.flag */
typedef enum eSpaceSeq_SequencerCacheOverlay_Flag {
  SEQ_CACHE_SHOW = (1 << 1),
  SEQ_CACHE_SHOW_RAW = (1 << 2),
  /* Was SEQ_CACHE_SHOW_PREPROCESSED = (1 << 3) */
  /* Was SEQ_CACHE_SHOW_COMPOSITE = (1 << 4) */
  SEQ_CACHE_SHOW_FINAL_OUT = (1 << 5),
} eSpaceSeq_SequencerCacheOverlay_Flag;

/** #SpaceSeq.mainb */
typedef enum eSpaceSeq_RegionType {
  SEQ_DRAW_IMG_IMBUF = 1,
  SEQ_DRAW_IMG_WAVEFORM = 2,
  SEQ_DRAW_IMG_VECTORSCOPE = 3,
  SEQ_DRAW_IMG_HISTOGRAM = 4,
  SEQ_DRAW_IMG_RGBPARADE = 5,
} eSpaceSeq_RegionType;

/** #SpaceSeq.draw_flag */
typedef enum eSpaceSeq_DrawFlag {
  SEQ_DRAW_UNUSED_0 = (1 << 0),
  SEQ_DRAW_UNUSED_1 = (1 << 1),
  SEQ_DRAW_TRANSFORM_PREVIEW = (1 << 2),
} eSpaceSeq_DrawFlag;

/** #SpaceSeq.flag */
typedef enum eSpaceSeq_Flag {
  SEQ_DRAWFRAMES = (1 << 0),
  SEQ_MARKER_TRANS = (1 << 1),
  SEQ_DRAW_COLOR_SEPARATED_UNUSED_2 = (1 << 2),
  SEQ_CLAMP_VIEW = (1 << 3),
  SPACE_SEQ_DESELECT_STRIP_HANDLE = (1 << 4),
  SPACE_SEQ_FLAG_UNUSED_5 = (1 << 5),
  SEQ_USE_ALPHA = (1 << 6), /* use RGBA display mode for preview */
  SPACE_SEQ_FLAG_UNUSED_10 = (1 << 10),
  SEQ_SHOW_MARKERS = (1 << 11), /* show markers region */
  SEQ_ZOOM_TO_FIT = (1 << 12),
  SEQ_SHOW_OVERLAY = (1 << 13),
  SPACE_SEQ_FLAG_UNUSED_14 = (1 << 14),
  SPACE_SEQ_FLAG_UNUSED_15 = (1 << 15),
  SPACE_SEQ_FLAG_UNUSED_16 = (1 << 16),
  SEQ_USE_PROXIES = (1 << 17),
  SEQ_SHOW_GRID = (1 << 18),
} eSpaceSeq_Flag;

/** #SpaceSeq.view */
typedef enum eSpaceSeq_Displays {
  SEQ_VIEW_SEQUENCE = 1,
  SEQ_VIEW_PREVIEW = 2,
  SEQ_VIEW_SEQUENCE_PREVIEW = 3,
} eSpaceSeq_Dispays;

/** #SpaceSeq.render_size */
typedef enum eSpaceSeq_Proxy_RenderSize {
  SEQ_RENDER_SIZE_NONE = -1,
  SEQ_RENDER_SIZE_SCENE = 0,
  SEQ_RENDER_SIZE_PROXY_25 = 25,
  SEQ_RENDER_SIZE_PROXY_50 = 50,
  SEQ_RENDER_SIZE_PROXY_75 = 75,
  SEQ_RENDER_SIZE_PROXY_100 = 99,
  SEQ_RENDER_SIZE_FULL_DEPRECATED = 100, /* deprecated, for versioning only */
} eSpaceSeq_Proxy_RenderSize;

/** #SpaceSeq.gizmo_flag */
enum {
  /** All gizmos. */
  SEQ_GIZMO_HIDE = (1 << 0),
  SEQ_GIZMO_HIDE_NAVIGATE = (1 << 1),
  SEQ_GIZMO_HIDE_CONTEXT = (1 << 2),
  SEQ_GIZMO_HIDE_TOOL = (1 << 3),
};

/** #SpaceSeq.mainb */
typedef enum eSpaceSeq_OverlayFrameType {
  SEQ_OVERLAY_FRAME_TYPE_RECT = 0,
  SEQ_OVERLAY_FRAME_TYPE_REFERENCE = 1,
  SEQ_OVERLAY_FRAME_TYPE_CURRENT = 2,
} eSpaceSeq_OverlayFrameType;

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Selector
 * \{ */

typedef enum eFileAssetImportMethod {
  /** Regular data-block linking. */
  FILE_ASSET_IMPORT_LINK = 0,
  /** Regular data-block appending (basically linking + "Make Local"). */
  FILE_ASSET_IMPORT_APPEND = 1,
  /** Append data-block with the #BLO_LIBLINK_APPEND_LOCAL_ID_REUSE flag enabled. Some typically
   * heavy data dependencies (e.g. the image data-blocks of a material, the mesh of an object) may
   * be reused from an earlier append. */
  FILE_ASSET_IMPORT_APPEND_REUSE = 2,
  /** Default: Follow the preference setting for this asset library. */
  FILE_ASSET_IMPORT_FOLLOW_PREFS = 3,
  /**
   * Link the data-block, but also pack it in the current file to keep it working even if the
   * source file is not available anymore.
   */
  FILE_ASSET_IMPORT_PACK = 4,
} eFileAssetImportMethod;

typedef enum eFileAssetImportFlags {
  FILE_ASSET_IMPORT_INSTANCE_COLLECTIONS_ON_LINK = (1 << 0),
  FILE_ASSET_IMPORT_INSTANCE_COLLECTIONS_ON_APPEND = (1 << 1),
} eFileAssetImportFlags;

/** #SpaceFile.browse_mode (File Space Browsing Mode). */
typedef enum eFileBrowse_Mode {
  /* Regular Blender File Browser */
  FILE_BROWSE_MODE_FILES = 0,
  /* Asset Browser */
  FILE_BROWSE_MODE_ASSETS = 1,
} eFileBrowse_Mode;

/** #FileSelectParams.display */
enum eFileDisplayType {
  /** Internal (not exposed to users): Keep whatever display type was used during the last File
   * Browser use, or the default if no such record is found. Use this unless there's a good reason
   * to set a specific display type. */
  FILE_DEFAULTDISPLAY = 0,

  /* User selectable choices. */

  FILE_VERTICALDISPLAY = 1,
  FILE_HORIZONTALDISPLAY = 2,
  FILE_IMGDISPLAY = 3,
};

/** #FileSelectParams.sort */
enum eFileSortType {
  /** Internal (not exposed to users): Sort by whatever was sorted by during the last File Browser
   * use, or the default if no such record is found. Use this unless there's a good reason to set a
   * specific sort order. */
  FILE_SORT_DEFAULT = 0,

  /* User selectable choices. */

  FILE_SORT_ALPHA = 1,
  FILE_SORT_EXTENSION = 2,
  FILE_SORT_TIME = 3,
  FILE_SORT_SIZE = 4,
  /* Assets: Sort by catalog. Within each catalog, assets will be sorted by name. */
  FILE_SORT_ASSET_CATALOG = 5,
};

/** #SpaceFile.tags */
enum eFileTags {
  /** Tag the space as having to update files representing or containing main data. Must be set
   * after file read and undo/redo. */
  FILE_TAG_REBUILD_MAIN_FILES = (1 << 0),
};

/** #FileSelectParams.details_flags */
enum eFileDetails {
  FILE_DETAILS_SIZE = (1 << 0),
  FILE_DETAILS_DATETIME = (1 << 1),
};

/* These values need to be hard-coded in structs, DNA does not recognize defines. */
/* also defined in BKE */
#define FILE_MAXDIR 768
#define FILE_MAXFILE 256
#define FILE_MAX 1024

#define FILE_MAX_LIBEXTRA (FILE_MAX + MAX_ID_NAME)

/**
 * Maximum level of recursions accepted for #FileSelectParams.recursion_level. Rather than a
 * completely arbitrary limit or none at all, make it just enough to support the most extreme case
 * where the maximal path length is used with single letter directory/file names only.
 */
#define FILE_SELECT_MAX_RECURSIONS (FILE_MAX_LIBEXTRA / 2)

/** File selector types. */
typedef enum eFileSelectType {
  FILE_LOADLIB = 1,
  FILE_MAIN = 2,
  /** Load assets from #Main. */
  FILE_MAIN_ASSET = 3,
  /** Load assets of an asset library containing external files. */
  FILE_ASSET_LIBRARY = 4,
  /** Load all asset libraries. */
  FILE_ASSET_LIBRARY_ALL = 5,

  FILE_UNIX = 8,
  FILE_BLENDER = 8, /* don't display relative paths */
  FILE_SPECIAL = 9,
} eFileSelectType;

/**
 * #FileSelectParams.flag / `sfile->params->flag`.
 * \note short flag, also used as 16 lower bits of flags in link/append code
 * (WM and BLO code area, see #eBLOLibLinkFlags in BLO_readfile.hh).
 */
typedef enum eFileSel_Params_Flag {
  FILE_PARAMS_FLAG_UNUSED_1 = (1 << 0),
  FILE_RELPATH = (1 << 1),
  FILE_LINK = (1 << 2),
  FILE_HIDE_DOT = (1 << 3),
  FILE_AUTOSELECT = (1 << 4),
  FILE_ACTIVE_COLLECTION = (1 << 5),
  FILE_PARAMS_FLAG_UNUSED_2 = (1 << 6),
  FILE_DIRSEL_ONLY = (1 << 7),
  FILE_FILTER = (1 << 8),
  FILE_PARAMS_FLAG_UNUSED_3 = (1 << 9),
  FILE_PATH_TOKENS_ALLOW = (1 << 10),
  FILE_SORT_INVERT = (1 << 11),
  FILE_HIDE_TOOL_PROPS = (1 << 12),
  FILE_CHECK_EXISTING = (1 << 13),
  FILE_ASSETS_ONLY = (1 << 14),
  /** Enables filtering by asset catalog. */
  FILE_FILTER_ASSET_CATALOG = (1 << 15),
} eFileSel_Params_Flag;
ENUM_OPERATORS(eFileSel_Params_Flag);

typedef enum eFileSel_Params_AssetCatalogVisibility {
  FILE_SHOW_ASSETS_ALL_CATALOGS,
  FILE_SHOW_ASSETS_FROM_CATALOG,
  FILE_SHOW_ASSETS_WITHOUT_CATALOG,
} eFileSel_Params_AssetCatalogVisibility;

/**
 * #FileSelectParams.rename_flag / `sfile->params->rename_flag`.
 * \note short flag. Defined as bit-flags, but currently only used as exclusive status markers.
 */
typedef enum eFileSel_Params_RenameFlag {
  /** Used when we only have the name of the entry we want to rename,
   * but not yet access to its matching file entry. */
  FILE_PARAMS_RENAME_PENDING = 1 << 0,
  /** We are actually renaming an entry. */
  FILE_PARAMS_RENAME_ACTIVE = 1 << 1,
  /** Used to scroll to newly renamed entry. */
  FILE_PARAMS_RENAME_POSTSCROLL_PENDING = 1 << 2,
  FILE_PARAMS_RENAME_POSTSCROLL_ACTIVE = 1 << 3,
} eFileSel_Params_RenameFlag;

/**
 * Files in the file selector list: file types
 * Note we could use mere values (instead of bit-flags) for file types themselves,
 * but since we do not lack of bytes currently.
 */
typedef enum eFileSel_File_Types {
  FILE_TYPE_BLENDER = (1 << 2),
  FILE_TYPE_BLENDER_BACKUP = (1 << 3),
  FILE_TYPE_IMAGE = (1 << 4),
  FILE_TYPE_MOVIE = (1 << 5),
  FILE_TYPE_PYSCRIPT = (1 << 6),
  FILE_TYPE_FTFONT = (1 << 7),
  FILE_TYPE_SOUND = (1 << 8),
  FILE_TYPE_TEXT = (1 << 9),
  FILE_TYPE_ARCHIVE = (1 << 10),
  /** represents folders for filtering */
  FILE_TYPE_FOLDER = (1 << 11),
  FILE_TYPE_BTX = (1 << 12),
  FILE_TYPE_UNUSED_13 = (1 << 13), /* Was FILE_TYPE_COLLADA */
  /** from filter_glob operator property */
  FILE_TYPE_OPERATOR = (1 << 14),
  FILE_TYPE_BUNDLE = (1 << 15),
  FILE_TYPE_ALEMBIC = (1 << 16),
  /** For all kinds of recognized import/export formats. No need for specialized types. */
  FILE_TYPE_OBJECT_IO = (1 << 17),
  FILE_TYPE_USD = (1 << 18),
  FILE_TYPE_VOLUME = (1 << 19),

  FILE_TYPE_ASSET = (1 << 28),
  /** An FS directory (i.e. S_ISDIR on its path is true). */
  FILE_TYPE_DIR = (1 << 30),
  FILE_TYPE_BLENDERLIB = (1u << 31),
} eFileSel_File_Types;
ENUM_OPERATORS(eFileSel_File_Types);

/** Selection Flags #FileList::selection_state. */
typedef enum eDirEntry_SelectFlag {
  // FILE_SEL_ACTIVE = (1 << 1), /* UNUSED */
  FILE_SEL_HIGHLIGHTED = (1 << 2),
  FILE_SEL_SELECTED = (1 << 3),
  FILE_SEL_EDITING = (1 << 4),
} eDirEntry_SelectFlag;
ENUM_OPERATORS(eDirEntry_SelectFlag);

/* ***** Related to file browser, but never saved in DNA, only here to help with RNA. ***** */

/** #FileDirEntry.flags */
enum {
  /* The preview for this entry could not be generated. */
  FILE_ENTRY_INVALID_PREVIEW = 1 << 0,
  /* The entry name needs to be freed when clearing file list. */
  FILE_ENTRY_NAME_FREE = 1 << 1,
  /* The preview for this entry is being loaded on another thread. */
  FILE_ENTRY_PREVIEW_LOADING = 1 << 2,
  /** For #FILE_TYPE_BLENDERLIB only: Denotes that the ID is known to not have a preview (none was
   * found in the .blend). Stored so we don't keep trying to find non-existent previews every time
   * we reload previews. When dealing with heavy files this can have quite an impact. */
  FILE_ENTRY_BLENDERLIB_NO_PREVIEW = 1 << 3,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image/UV Editor
 * \{ */

typedef enum eSpaceImage_GridShapeSource {
  SI_GRID_SHAPE_DYNAMIC = 0,
  SI_GRID_SHAPE_FIXED = 1,
  SI_GRID_SHAPE_PIXEL = 2,
} eSpaceImage_GridShapeSource;

/** #SpaceImage.dt_uv */
typedef enum eSpaceImage_UVDT {
  SI_UVDT_OUTLINE = 0,
  SI_UVDT_DASH = 1,
  SI_UVDT_BLACK = 2,
  SI_UVDT_WHITE = 3,
} eSpaceImage_UVDT;

/** #SpaceImage.dt_uvstretch */
typedef enum eSpaceImage_UVDT_Stretch {
  SI_UVDT_STRETCH_ANGLE = 0,
  SI_UVDT_STRETCH_AREA = 1,
} eSpaceImage_UVDT_Stretch;

/** #SpaceImage.pixel_round_mode */
typedef enum eSpaceImage_PixelRoundMode {
  SI_PIXEL_ROUND_DISABLED = 0,
  SI_PIXEL_ROUND_CENTER = 1,
  SI_PIXEL_ROUND_CORNER = 2,
} eSpaceImage_PixelRoundMode;

/** #SpaceImage.mode */
typedef enum eSpaceImage_Mode {
  SI_MODE_VIEW = 0,
  SI_MODE_PAINT = 1,
  SI_MODE_MASK = 2,
  SI_MODE_UV = 3,
} eSpaceImage_Mode;

/** #SpaceImage.flag */
typedef enum eSpaceImage_Flag {
  SI_FLAG_UNUSED_0 = (1 << 0), /* cleared */
  SI_FLAG_UNUSED_1 = (1 << 1), /* cleared */
  SI_CLIP_UV = (1 << 2),
  SI_FLAG_UNUSED_3 = (1 << 3), /* cleared */
  SI_NO_DRAWFACES = (1 << 4),
  SI_DRAWSHADOW = (1 << 5),
  SI_FLAG_UNUSED_6 = (1 << 6), /* cleared */
  SI_FLAG_UNUSED_7 = (1 << 7), /* cleared */
  SI_FLAG_UNUSED_8 = (1 << 8), /* cleared */
  SI_COORDFLOATS = (1 << 9),

  SI_FLAG_UNUSED_10 = (1 << 10),
  SI_LIVE_UNWRAP = (1 << 11),
  SI_USE_ALPHA = (1 << 12),
  SI_SHOW_ALPHA = (1 << 13),
  SI_SHOW_ZBUF = (1 << 14),

  /* next two for render window display */
  SI_PREVSPACE = (1 << 15),
  SI_FULLWINDOW = (1 << 16),

  SI_FLAG_UNUSED_17 = (1 << 17),
  SI_FLAG_UNUSED_18 = (1 << 18),

  /**
   * This means that the image is drawn until it reaches the view edge,
   * in the image view, it's unrelated to UDIM tiles.
   */
  SI_DRAW_TILE = (1 << 19),
  SI_FLAG_UNUSED_20 = (1 << 20), /* cleared */
  SI_DRAW_STRETCH = (1 << 21),
  SI_SHOW_GPENCIL = (1 << 22),
  SI_FLAG_UNUSED_23 = (1 << 23), /* cleared */

  SI_FLAG_UNUSED_24 = (1 << 24),

#ifdef DNA_DEPRECATED_ALLOW
  SI_NO_DRAW_TEXPAINT = (1 << 25), /* deprecated - use SI_NO_DRAW_UV_GUIDE instead, see #135102 */
#endif
  SI_DRAW_METADATA = (1 << 26),

  SI_SHOW_R = (1 << 27),
  SI_SHOW_G = (1 << 28),
  SI_SHOW_B = (1 << 29),

  SI_GRID_OVER_IMAGE = (1 << 30),

  SI_NO_DRAW_UV_GUIDE = (1 << 31),
} eSpaceImage_Flag;

typedef enum eSpaceImageOverlay_Flag {
  SI_OVERLAY_SHOW_OVERLAYS = (1 << 0),
  SI_OVERLAY_SHOW_GRID_BACKGROUND = (1 << 1),
  SI_OVERLAY_DRAW_RENDER_REGION = (1 << 2),
  SI_OVERLAY_DRAW_TEXT_INFO = (1 << 3),
} eSpaceImageOverlay_Flag;

/** #SpaceImage.gizmo_flag */
enum {
  /** All gizmos. */
  SI_GIZMO_HIDE = (1 << 0),
  SI_GIZMO_HIDE_NAVIGATE = (1 << 1),
};

/** Keep in sync with `STEPS_LEN` in `grid_frag.glsl`. */
#define SI_GRID_STEPS_LEN 8

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Editor
 * \{ */

/** SpaceText flags (moved from DNA_text_types.h). */
typedef enum eSpaceText_Flags {
  /* scrollable */
  ST_SCROLL_SELECT = (1 << 0),

  ST_FLAG_UNUSED_4 = (1 << 4), /* Cleared. */

  ST_FIND_WRAP = (1 << 5),
  ST_FIND_ALL = (1 << 6),
  ST_SHOW_MARGIN = (1 << 7),
  ST_MATCH_CASE = (1 << 8),

  ST_FLAG_UNUSED_9 = (1 << 9), /* Dirty. */

} eSpaceText_Flags;

/* SpaceText.findstr/replacestr */
#define ST_MAX_FIND_STR 256

/** \} */

/* -------------------------------------------------------------------- */
/** \name Nodes Editor
 * \{ */

typedef enum eSpaceNodeOverlay_Flag {
  SN_OVERLAY_SHOW_OVERLAYS = (1 << 1),
  SN_OVERLAY_SHOW_WIRE_COLORS = (1 << 2),
  SN_OVERLAY_SHOW_TIMINGS = (1 << 3),
  SN_OVERLAY_SHOW_PATH = (1 << 4),
  SN_OVERLAY_SHOW_NAMED_ATTRIBUTES = (1 << 5),
  SN_OVERLAY_SHOW_PREVIEWS = (1 << 6),
  /**
   * Display an automatic label on reroute nodes based on the user-defined labels
   * of connected reroute nodes.
   */
  SN_OVERLAY_SHOW_REROUTE_AUTO_LABELS = (1 << 7),
} eSpaceNodeOverlay_Flag;

typedef enum eSpaceNodeOverlay_preview_shape {
  SN_OVERLAY_PREVIEW_FLAT = 0,
  SN_OVERLAY_PREVIEW_3D = 1,
} eSpaceNodeOverlay_preview_shape;

/** #SpaceNode.flag */
typedef enum eSpaceNode_Flag {
  SNODE_BACKDRAW = (1 << 1),
  SNODE_SHOW_GPENCIL = (1 << 2),
  SNODE_USE_ALPHA = (1 << 3),
  SNODE_SHOW_ALPHA = (1 << 4),
  SNODE_SHOW_R = (1 << 7),
  SNODE_SHOW_G = (1 << 8),
  SNODE_SHOW_B = (1 << 9),
  SNODE_FLAG_UNUSED_5 = (1 << 5),   /* cleared */
  SNODE_FLAG_UNUSED_6 = (1 << 6),   /* cleared */
  SNODE_FLAG_UNUSED_10 = (1 << 10), /* cleared */
  SNODE_FLAG_UNUSED_11 = (1 << 11), /* cleared */
  SNODE_PIN = (1 << 12),
  SNODE_FLAG_UNUSED_12 = (1 << 13),
} eSpaceNode_Flag;

/** #SpaceNode.gizmo_flag */
enum {
  /** All gizmos. */
  SNODE_GIZMO_HIDE = (1 << 0),
  SNODE_GIZMO_HIDE_ACTIVE_NODE = (1 << 1),
};

/** #SpaceNode.texfrom */
typedef enum eSpaceNode_TexFrom {
  /* SNODE_TEX_OBJECT   = 0, */
  SNODE_TEX_WORLD = 1,
  SNODE_TEX_BRUSH = 2,
  SNODE_TEX_LINESTYLE = 3,
} eSpaceNode_TexFrom;

/** #SpaceNode.shaderfrom */
typedef enum eSpaceNode_ShaderFrom {
  SNODE_SHADER_OBJECT = 0,
  SNODE_SHADER_WORLD = 1,
  SNODE_SHADER_LINESTYLE = 2,
} eSpaceNode_ShaderFrom;

/** #SpaceNode.nodes_type */
typedef enum SpaceNodeGeometryNodesType {
  SNODE_GEOMETRY_MODIFIER = 0,
  SNODE_GEOMETRY_TOOL = 1,
} SpaceNodeGeometryNodesType;

/** #SpaceNode.nodes_type */
typedef enum SpaceNodeCompositorNodesType {
  SNODE_COMPOSITOR_SCENE = 0,
  SNODE_COMPOSITOR_SEQUENCER = 1,
} SpaceNodeCompositorNodesType;

/** #SpaceNode.insert_ofs_dir */
enum {
  SNODE_INSERTOFS_DIR_RIGHT = 0,
  SNODE_INSERTOFS_DIR_LEFT = 1,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Console
 * \{ */

/** #ConsoleLine.type */
typedef enum eConsoleLine_Type {
  CONSOLE_LINE_OUTPUT = 0,
  CONSOLE_LINE_INPUT = 1,
  /** Auto-completion feedback. */
  CONSOLE_LINE_INFO = 2,
  CONSOLE_LINE_ERROR = 3,
} eConsoleLine_Type;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Motion Tracking
 * \{ */

/** #SpaceClipOverlay.flag */
typedef enum eSpaceClipOverlay_Flag {
  SC_SHOW_OVERLAYS = (1 << 0),
  SC_SHOW_CURSOR = (1 << 1),
} eSpaceClipOverlay_Flag;

/** #SpaceClip.flag */
typedef enum eSpaceClip_Flag {
  SC_SHOW_MARKER_PATTERN = (1 << 0),
  SC_SHOW_MARKER_SEARCH = (1 << 1),
  SC_LOCK_SELECTION = (1 << 2),
  SC_SHOW_TINY_MARKER = (1 << 3),
  SC_SHOW_TRACK_PATH = (1 << 4),
  SC_SHOW_BUNDLES = (1 << 5),
  SC_MUTE_FOOTAGE = (1 << 6),
  SC_HIDE_DISABLED = (1 << 7),
  SC_SHOW_NAMES = (1 << 8),
  SC_SHOW_GRID = (1 << 9),
  SC_SHOW_STABLE = (1 << 10),
  SC_MANUAL_CALIBRATION = (1 << 11),
  SC_SHOW_ANNOTATION = (1 << 12),
  SC_SHOW_FILTERS = (1 << 13),
  SC_SHOW_GRAPH_FRAMES = (1 << 14),
  SC_SHOW_GRAPH_TRACKS_MOTION = (1 << 15),
  // SC_SHOW_PYRAMID_LEVELS = (1 << 16), /* UNUSED */
  SC_LOCK_TIMECURSOR = (1 << 17),
  SC_SHOW_SECONDS = (1 << 18),
  SC_SHOW_GRAPH_SEL_ONLY = (1 << 19),
  SC_SHOW_GRAPH_HIDDEN = (1 << 20),
  SC_SHOW_GRAPH_TRACKS_ERROR = (1 << 21),
  SC_SHOW_METADATA = (1 << 22),
} eSpaceClip_Flag;

/** #SpaceClip.mode */
typedef enum eSpaceClip_Mode {
  SC_MODE_TRACKING = 0,
  // SC_MODE_RECONSTRUCTION = 1, /* DEPRECATED */
  // SC_MODE_DISTORTION = 2,     /* DEPRECATED */
  SC_MODE_MASKEDIT = 3,
} eSpaceClip_Mode;

/** #SpaceClip.view */
typedef enum eSpaceClip_View {
  SC_VIEW_CLIP = 0,
  SC_VIEW_GRAPH = 1,
  SC_VIEW_DOPESHEET = 2,
} eSpaceClip_View;

/** #SpaceClip.gpencil_src */
typedef enum eSpaceClip_GPencil_Source {
  SC_GPENCIL_SRC_CLIP = 0,
  SC_GPENCIL_SRC_TRACK = 1,
} eSpaceClip_GPencil_Source;

/** #SpaceClip.gizmo_flag */
enum {
  /** All gizmos. */
  SCLIP_GIZMO_HIDE = (1 << 0),
  SCLIP_GIZMO_HIDE_NAVIGATE = (1 << 1),
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Spreadsheet
 * \{ */

typedef enum eSpaceSpreadsheet_Flag {
  /**
   * Spreadsheet context is pinned and does not does not change when the context changes (unless
   * the pinned context does not exist anymore).
   */
  SPREADSHEET_FLAG_PINNED = (1 << 0),
  /**
   * Not used anymore, just kept so the flag is not accidentally reused without clearing it in
   * versioning first.
   */
  SPREADSHEET_FLAG_CONTEXT_PATH_COLLAPSED_LEGACY = (1 << 1),
  SPREADSHEET_FLAG_SHOW_INTERNAL_ATTRIBUTES = (1 << 2),
} eSpaceSpreadsheet_Flag;

typedef enum eSpaceSpreadsheet_FilterFlag {
  SPREADSHEET_FILTER_SELECTED_ONLY = (1 << 0),
  SPREADSHEET_FILTER_ENABLE = (1 << 1),
} eSpaceSpreadsheet_FilterFlag;

typedef enum eSpaceSpreadsheet_RowFilterFlag {
  SPREADSHEET_ROW_FILTER_UI_EXPAND = (1 << 0),
  SPREADSHEET_ROW_FILTER_BOOL_VALUE = (1 << 1),
  SPREADSHEET_ROW_FILTER_ENABLED = (1 << 2),
} eSpaceSpreadsheet_RowFilterFlag;

typedef enum eSpreadsheetFilterOperation {
  SPREADSHEET_ROW_FILTER_EQUAL = 0,
  SPREADSHEET_ROW_FILTER_GREATER = 1,
  SPREADSHEET_ROW_FILTER_LESS = 2,
} eSpreadsheetFilterOperation;

typedef enum eSpaceSpreadsheet_ObjectEvalState {
  SPREADSHEET_OBJECT_EVAL_STATE_EVALUATED = 0,
  SPREADSHEET_OBJECT_EVAL_STATE_ORIGINAL = 1,
  SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE = 2,
} eSpaceSpreadsheet_Context;

typedef enum eSpaceSpreadsheet_ContextType {
  SPREADSHEET_CONTEXT_OBJECT = 0,
  SPREADSHEET_CONTEXT_MODIFIER = 1,
  SPREADSHEET_CONTEXT_NODE = 2,
} eSpaceSpreadsheet_ContextType;

typedef enum eSpreadsheetColumnValueType {
  SPREADSHEET_VALUE_TYPE_UNKNOWN = -1,
  SPREADSHEET_VALUE_TYPE_BOOL = 0,
  SPREADSHEET_VALUE_TYPE_INT32 = 1,
  SPREADSHEET_VALUE_TYPE_FLOAT = 2,
  SPREADSHEET_VALUE_TYPE_FLOAT2 = 3,
  SPREADSHEET_VALUE_TYPE_FLOAT3 = 4,
  SPREADSHEET_VALUE_TYPE_COLOR = 5,
  SPREADSHEET_VALUE_TYPE_INSTANCES = 6,
  SPREADSHEET_VALUE_TYPE_STRING = 7,
  SPREADSHEET_VALUE_TYPE_BYTE_COLOR = 8,
  SPREADSHEET_VALUE_TYPE_INT8 = 9,
  SPREADSHEET_VALUE_TYPE_INT32_2D = 10,
  SPREADSHEET_VALUE_TYPE_QUATERNION = 11,
  SPREADSHEET_VALUE_TYPE_FLOAT4X4 = 12,
  SPREADSHEET_VALUE_TYPE_BUNDLE_ITEM = 13,
  SPREADSHEET_VALUE_TYPE_INT64 = 14,
  SPREADSHEET_VALUE_TYPE_INT32_3D = 15,
} eSpreadsheetColumnValueType;

typedef enum eSpreadsheetColumnFlag {
  /**
   * There is no data for this column currently, so it's not displayed. However, it is still kept
   * around so that the column remembers its position and width when the data becomes available
   * again.
   */
  SPREADSHEET_COLUMN_FLAG_UNAVAILABLE = (1 << 0),
} eSpreadsheetColumnFlag;

typedef enum eSpreadsheetTableIDType {
  /** This table uses the #SpreadsheetTableIDGeometry key. */
  SPREADSHEET_TABLE_ID_TYPE_GEOMETRY = 0,
} eSpreadsheetTableType;

typedef enum eSpreadsheetTableFlag {
  /**
   * If a generated table has never been manually edited (e.g. changing column order), it can be
   * discarded when it's no longer displayed.
   */
  SPREADSHEET_TABLE_FLAG_MANUALLY_EDITED = (1 << 0),
} eSpreadsheetTableFlag;

/**
 * We can't just use UI_UNIT_X, because it does not take `widget.points` into account, which
 * modifies the width of text as well.
 */
#define SPREADSHEET_WIDTH_UNIT \
  (UI_UNIT_X * UI_style_get_dpi()->widget.points / (float)UI_DEFAULT_TEXT_POINTS)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space Defines (eSpace_Type)
 * \{ */

/**
 * Space types: #SpaceLink.spacetype & #ScrArea.spacetype.
 * \note Do NOT change order, append on end. types are hardcoded needed.
 */
typedef enum eSpace_Type {
  SPACE_EMPTY = 0,
  SPACE_VIEW3D = 1,
  SPACE_GRAPH = 2,
  SPACE_OUTLINER = 3,
  SPACE_PROPERTIES = 4,
  SPACE_FILE = 5,
  SPACE_IMAGE = 6,
  SPACE_INFO = 7,
  SPACE_SEQ = 8,
  SPACE_TEXT = 9,
#ifdef DNA_DEPRECATED_ALLOW
  SPACE_IMASEL = 10, /* Deprecated */
  SPACE_SOUND = 11,  /* Deprecated */
#endif
  SPACE_ACTION = 12,
  SPACE_NLA = 13,
  /* TODO: fully deprecate */
  SPACE_SCRIPT = 14, /* Deprecated */
#ifdef DNA_DEPRECATED_ALLOW
  SPACE_TIME = 15, /* Deprecated */
#endif
  SPACE_NODE = 16,
#ifdef DNA_DEPRECATED_ALLOW
  SPACE_LOGIC = 17, /* Deprecated */
#endif
  SPACE_CONSOLE = 18,
  SPACE_USERPREF = 19,
  SPACE_CLIP = 20,
  SPACE_TOPBAR = 21,
  SPACE_STATUSBAR = 22,
  SPACE_SPREADSHEET = 23

#define SPACE_TYPE_NUM (SPACE_SPREADSHEET + 1)
} eSpace_Type;

/* use for function args */
#define SPACE_TYPE_ANY -1

#define IMG_SIZE_FALLBACK 256

/** \} */
