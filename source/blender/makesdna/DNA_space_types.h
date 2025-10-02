/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/** \file
 * \ingroup DNA
 *
 * Structs for each of space type in the user interface.
 */

#pragma once

#include "DNA_asset_types.h"
#include "DNA_color_types.h" /* for Histogram */
#include "DNA_defs.h"
#include "DNA_image_types.h" /* ImageUser */
#include "DNA_listBase.h"
#include "DNA_movieclip_types.h" /* MovieClipUser */
#include "DNA_node_types.h"      /* for bNodeInstanceKey */
#include "DNA_outliner_types.h"  /* for TreeStoreElem */
#include "DNA_space_enums.h"
/* Hum ... Not really nice... but needed for spacebuts. */
#include "DNA_view2d_types.h"
#include "DNA_viewer_path_types.h"

struct BLI_mempool;
struct FileLayout;
struct FileList;
struct FileSelectParams;
struct Histogram;
struct ID;
struct Image;
struct Mask;
struct MovieClip;
struct MovieClipScopes;
struct Scopes;
struct Script;
struct SpaceGraph;
struct Text;
struct bDopeSheet;
struct bGPdata;
struct bNodeTree;
struct wmOperator;
struct wmTimer;

#ifdef __cplusplus
namespace blender::asset_system {
class AssetRepresentation;
}
using AssetRepresentationHandle = blender::asset_system::AssetRepresentation;
#else
typedef struct AssetRepresentationHandle AssetRepresentationHandle;
#endif

/** Defined in `buttons_intern.hh`. */
typedef struct SpaceProperties_Runtime SpaceProperties_Runtime;

#ifdef __cplusplus
namespace blender::ed::space_node {
struct SpaceNode_Runtime;
}  // namespace blender::ed::space_node
using SpaceNode_Runtime = blender::ed::space_node::SpaceNode_Runtime;

namespace blender::ed::outliner {
struct SpaceOutliner_Runtime;
}  // namespace blender::ed::outliner
using SpaceOutliner_Runtime = blender::ed::outliner::SpaceOutliner_Runtime;

namespace blender::ed::vse {
struct SpaceSeq_Runtime;
}  // namespace blender::ed::vse
using SpaceSeq_Runtime = blender::ed::vse::SpaceSeq_Runtime;

namespace blender::ed::text {
struct SpaceText_Runtime;
}  // namespace blender::ed::text
using SpaceText_Runtime = blender::ed::text::SpaceText_Runtime;

namespace blender::ed::spreadsheet {
struct SpaceSpreadsheet_Runtime;
struct SpreadsheetColumnRuntime;
}  // namespace blender::ed::spreadsheet
using SpaceSpreadsheet_Runtime = blender::ed::spreadsheet::SpaceSpreadsheet_Runtime;
using SpreadsheetColumnRuntime = blender::ed::spreadsheet::SpreadsheetColumnRuntime;
#else
typedef struct SpaceNode_Runtime SpaceNode_Runtime;
typedef struct SpaceOutliner_Runtime SpaceOutliner_Runtime;
typedef struct SpaceSeq_Runtime SpaceSeq_Runtime;
typedef struct SpaceText_Runtime SpaceText_Runtime;
typedef struct SpaceSpreadsheet_Runtime SpaceSpreadsheet_Runtime;
typedef struct SpreadsheetColumnRuntime SpreadsheetColumnRuntime;
#endif

/** Defined in `file_intern.hh`. */
typedef struct SpaceFile_Runtime SpaceFile_Runtime;

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
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
} SpaceLink;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space Info
 * \{ */

/** Info Header. */
typedef struct SpaceInfo {
  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  char rpt_mask;
  char _pad[7];
} SpaceInfo;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Properties Editor
 * \{ */

/** Properties Editor. */
typedef struct SpaceProperties {
  DNA_DEFINE_CXX_METHODS(SpaceProperties)

  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  /** Deprecated, copied to region. */
  View2D v2d DNA_DEPRECATED;

  /* For different kinds of property editors (exposed in the space type selector). */
  short space_subtype;

  /** Context tabs. */
  short mainb, mainbo, mainbuser;
  uint32_t visible_tabs;
  /** Preview is signal to refresh. */
  short preview;
  char flag;

  /* eSpaceButtons_OutlinerSync */
  char outliner_sync;

  /** Runtime. */
  void *path;
  /** Runtime. */
  int pathflag, dataicon;
  ID *pinid;

  void *texuser;

  /* Doesn't necessarily need to be a pointer, but runtime structs are still written to files. */
  struct SpaceProperties_Runtime *runtime;
} SpaceProperties;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Outliner
 * \{ */

/** Outliner */
typedef struct SpaceOutliner {
  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  /** Deprecated, copied to region. */
  View2D v2d DNA_DEPRECATED;

  ListBase tree;

  /**
   * Treestore is an ordered list of TreeStoreElem's from outliner tree;
   * Note that treestore may contain duplicate elements if element
   * is used multiple times in outliner tree (e. g. linked objects)
   * Also note that BLI_mempool can not be read/written in DNA directly,
   * therefore `readfile.cc` / `writefile.cc` linearize treestore into #TreeStore structure.
   */
  struct BLI_mempool *treestore;

  char search_string[64];

  short flag;
  short outlinevis;
  short lib_override_view_mode;
  short storeflag;
  char search_flags;
  char _pad[6];

  /** Selection syncing flag (#WM_OUTLINER_SYNC_SELECT_FROM_OBJECT and similar flags). */
  char sync_select_dirty;

  int filter;
  char filter_state;
  char show_restrict_flags;
  short filter_id_type;

  SpaceOutliner_Runtime *runtime;
} SpaceOutliner;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Graph Editor
 * \{ */

typedef struct SpaceGraph_Runtime {
  /** #eGraphEdit_Runtime_Flag */
  char flag;
  char _pad[7];
  /** Sampled snapshots of F-Curves used as in-session guides */
  ListBase ghost_curves;
} SpaceGraph_Runtime;

/** 'Graph' Editor (formerly known as the IPO Editor). */
typedef struct SpaceGraph {
  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  /** Deprecated, copied to region. */
  View2D v2d DNA_DEPRECATED;

  /** Settings for filtering animation data
   * \note we use a pointer due to code-linking issues. */
  struct bDopeSheet *ads;

  /** Mode for the Graph editor (eGraphEdit_Mode). */
  short mode;
  /* Snapping now lives on the Scene. */
  short autosnap DNA_DEPRECATED;
  /** Settings for Graph editor (eGraphEdit_Flag). */
  int flag;

  /** Time value for cursor (when in drivers mode; animation uses current frame). */
  float cursorTime;
  /** Cursor value (y-value, x-value is current frame). */
  float cursorVal;
  /** Pivot point for transforms. */
  int around;
  char _pad[4];

  SpaceGraph_Runtime runtime;
} SpaceGraph;

/** \} */

/* -------------------------------------------------------------------- */
/** \name NLA Editor
 * \{ */

/** NLA Editor */
typedef struct SpaceNla {
  struct SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  /* Snapping now lives on the Scene. */
  short autosnap DNA_DEPRECATED;
  short flag;
  char _pad[4];

  struct bDopeSheet *ads;
  /** Deprecated, copied to region. */
  View2D v2d DNA_DEPRECATED;
} SpaceNla;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sequence Editor
 * \{ */

typedef struct SequencerPreviewOverlay {
  int flag;
  char _pad0[4];
} SequencerPreviewOverlay;

typedef struct SequencerTimelineOverlay {
  int flag;
  char _pad0[4];
} SequencerTimelineOverlay;

typedef struct SequencerCacheOverlay {
  int flag;
  char _pad0[4];
} SequencerCacheOverlay;

/** Sequencer. */
typedef struct SpaceSeq {
  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  /** Deprecated, copied to region. */
  View2D v2d DNA_DEPRECATED;

  /** Deprecated: offset for drawing the image preview. */
  float xof DNA_DEPRECATED, yof DNA_DEPRECATED;
  short mainb; /* eSpaceSeq_RegionType; strange name for view type (image, histogram, ...). */
  short render_size; /* eSpaceSeq_Proxy_RenderSize. */
  short chanshown;
  short zebra; /* Show overexposed. 0=disabled; otherwise as percentage of "pure white". */
  int flag;
  /** Deprecated, handled by View2D now. */
  float zoom DNA_DEPRECATED;
  /** See SEQ_VIEW_* below. */
  char view;
  char overlay_frame_type;
  /** Overlay an image of the editing on below the strips. */
  char draw_flag;
  char gizmo_flag;
  char _pad[4];

  /** 2D cursor for transform. */
  float cursor[2];

  /** Grease-pencil data. */
  struct bGPdata *gpd;

  struct SequencerPreviewOverlay preview_overlay;
  struct SequencerTimelineOverlay timeline_overlay;
  struct SequencerCacheOverlay cache_overlay;

  /** Multi-view current eye - for internal use. */
  char multiview_eye;
  char _pad2[7];

  SpaceSeq_Runtime *runtime;
} SpaceSeq;

typedef struct MaskSpaceInfo {
  /* **** mask editing **** */
  struct Mask *mask;
  /* draw options */
  char draw_flag;
  char draw_type;
  char overlay_mode;
  char _pad3[1];
  float blend_factor;
} MaskSpaceInfo;

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Selector
 * \{ */

/** Config and Input for File Selector. */
typedef struct FileSelectParams {
  /** Title, also used for the text of the execute button. */
  char title[96];
  /**
   * Directory.
   *
   * \note #FILE_MAX_LIBEXTRA == `1024 + 258`, this is for extreme case when 1023 length path
   * needs to be linked in, where `foo.blend/Armature` need adding.
   */
  char dir[/*FILE_MAX_LIBEXTRA*/ 1282];
  char file[/*FILE_MAXFILE*/ 256];

  char renamefile[/*FILE_MAXFILE*/ 256];
  short rename_flag;
  char _pad[4];
  /** An ID that was just renamed. Used to identify a renamed asset file over re-reads, similar to
   * `renamefile` but for local IDs (takes precedence). Don't keep this stored across handlers!
   * Would break on undo. */
  const ID *rename_id;
  void *_pad3;

  /** List of file-types to filter. */
  char filter_glob[/*FILE_MAXFILE*/ 256];

  /** Text items name must match to be shown. */
  char filter_search[64];
  /** Same as filter, but for ID types (aka library groups). */
  uint64_t filter_id;

  /** Active file used for keyboard navigation. -1 means no active file (cleared e.g. after
   * directory change or search update). */
  int active_file;
  /** File under cursor. */
  int highlight_file;
  int sel_first;
  int sel_last;
  unsigned short thumbnail_size;
  unsigned short list_thumbnail_size;
  unsigned short list_column_size;

  /* short */
  /** XXX: for now store type here, should be moved to the operator. */
  short type; /* eFileSelectType */
  /** Settings for filter, hiding dots files. */
  short flag;
  /** Sort order. */
  short sort;
  /** Display mode flag. */
  short display;
  /** Details toggles (file size, creation date, etc.) */
  char details_flags;
  char _pad1;

  /** Filter when (flags & FILE_FILTER) is true. */
  int filter;

  /** Max number of levels in directory tree to show at once, 0 to disable recursion. */
  short recursion_level;

  char _pad2[2];
} FileSelectParams;

/**
 * File selection parameters for asset browsing mode, with #FileSelectParams as base.
 */
typedef struct FileAssetSelectParams {
  FileSelectParams base_params;

  AssetLibraryReference asset_library_ref;
  short asset_catalog_visibility; /* eFileSel_Params_AssetCatalogVisibility */
  char _pad[6];
  /** If #asset_catalog_visibility is #FILE_SHOW_ASSETS_FROM_CATALOG, this sets the ID of the
   * catalog to show. */
  bUUID catalog_id;

  short import_method; /* eFileAssetImportMethod */
  short import_flags;  /* eFileImportFlags */
  char _pad2[4];
} FileAssetSelectParams;

/**
 * A wrapper to store previous and next folder lists (#FolderList) for a specific browse mode
 * (#eFileBrowse_Mode).
 */
typedef struct FileFolderHistory {
  struct FileFolderLists *next, *prev;

  /** The browse mode this prev/next folder-lists are created for. */
  char browse_mode; /* eFileBrowse_Mode */
  char _pad[7];

  /** Holds the list of previous directories to show. */
  ListBase folders_prev;
  /** Holds the list of next directories (pushed from previous) to show. */
  ListBase folders_next;
} FileFolderHistory;

/** File Browser. */
typedef struct SpaceFile {
  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  /** Is this a File Browser or an Asset Browser? */
  char browse_mode; /* eFileBrowse_Mode */
  char _pad1[1];

  short tags;

  int scroll_offset;

  /** Config and input for file select. One for each browse-mode, to keep them independent. */
  FileSelectParams *params;
  FileAssetSelectParams *asset_params;

  void *_pad2;

  /**
   * Holds the list of files to show.
   * Currently recreated when browse-mode changes. Could be per browse-mode to avoid refreshes.
   */
  struct FileList *files;

  /**
   * Holds the list of previous directories to show. Owned by `folder_histories` below.
   */
  ListBase *folders_prev;
  /**
   * Holds the list of next directories (pushed from previous) to show. Owned by
   * `folder_histories` below.
   */
  ListBase *folders_next;

  /**
   * This actually owns the prev/next folder-lists above. On browse-mode change, the lists of the
   * new mode get assigned to the above.
   */
  ListBase folder_histories; /* FileFolderHistory */

  /**
   * The operator that is invoking file-select `op->exec()` will be called on the 'Load' button.
   * if operator provides op->cancel(), then this will be invoked on the cancel button.
   */
  struct wmOperator *op;

  struct wmTimer *smoothscroll_timer;
  struct wmTimer *previews_timer;

  struct FileLayout *layout;

  short recentnr, bookmarknr;
  short systemnr, system_bookmarknr;

  SpaceFile_Runtime *runtime;
} SpaceFile;

/* ***** Related to file browser, but never saved in DNA, only here to help with RNA. ***** */

#
#
typedef struct FileDirEntry {
  struct FileDirEntry *next, *prev;

  uint32_t uid; /* FileUID */
  /* Name needs freeing if FILE_ENTRY_NAME_FREE is set. Otherwise this is a direct pointer to a
   * name buffer. */
  const char *name;

  uint64_t size;
  int64_t time;

  struct {
    /* Temp caching of UI-generated strings. */
    char size_str[16];
    char datetime_str[16 + 8];
  } draw_data;

  /** #eFileSel_File_Types. */
  int typeflag;
  /** ID type, in case typeflag has FILE_TYPE_BLENDERLIB set. */
  int blentype;

  /* Path to item that is relative to current folder root. To get the full path, use
   * #filelist_file_get_full_path() */
  char *relpath;
  /** Optional argument for shortcuts, aliases etc. */
  char *redirection_path;

  /** When showing local IDs (FILE_MAIN, FILE_MAIN_ASSET), ID this file represents. Note comment
   * for FileListInternEntry.local_data, the same applies here! */
  ID *id;
  /** If this file represents an asset, its asset data is here. Note that we may show assets of
   * external files in which case this is set but not the id above.
   * Note comment for FileListInternEntry.local_data, the same applies here! */
  AssetRepresentationHandle *asset;

  /* The icon_id for the preview image. */
  int preview_icon_id;

  short flags;
  /* eFileAttributes defined in BLI_fileops.h */
  int attributes;
} FileDirEntry;

/**
 * Array of directory entries.
 *
 * Stores the total number of available entries, the number of visible (filtered) entries, and a
 * subset of those in 'entries' ListBase, from idx_start (included) to idx_end (excluded).
 */
#
#
typedef struct FileDirEntryArr {
  ListBase entries;
  int entries_num;
  int entries_filtered_num;

  char root[/*FILE_MAX*/ 1024];
} FileDirEntryArr;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image/UV Editor
 * \{ */

/* Image/UV Editor */

typedef struct SpaceImageOverlay {
  int flag;
  float passepartout_alpha;
} SpaceImageOverlay;

typedef struct SpaceImage {
  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  struct Image *image;
  struct ImageUser iuser;

  /** Histogram waveform and vector-scope. */
  struct Scopes scopes;
  /** Sample line histogram. */
  struct Histogram sample_line_hist;

  /** Grease pencil data. */
  struct bGPdata *gpd;

  /** UV editor 2d cursor. */
  float cursor[2];
  /** User defined offset, image is centered. */
  float xof, yof;
  /** User defined zoom level. */
  float zoom;
  /** Storage for offset while render drawing. */
  float centx, centy;

  /** View/paint/mask. */
  char mode;
  /* Storage for sub-space types. */
  char mode_prev;

  char pin;

  char pixel_round_mode;

  char lock;
  /** UV draw type. */
  char dt_uv;
  /** Sticky selection type. */
  char dt_uvstretch;
  char around;

  char gizmo_flag;

  char grid_shape_source;
  char _pad1[6];

  int flag;

  float uv_opacity;
  float uv_face_opacity;
  char _pad2[4];

  float stretch_opacity;

  int tile_grid_shape[2];
  /**
   * UV editor custom-grid. Value of `{M,N}` will produce `MxN` grid.
   * Use when `custom_grid_shape == SI_GRID_SHAPE_FIXED`.
   */
  int custom_grid_subdiv[2];

  MaskSpaceInfo mask_info;
  SpaceImageOverlay overlay;
} SpaceImage;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Editor
 * \{ */

/** Text Editor. */
typedef struct SpaceText {
  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  struct Text *text;

  /** Determines at what line the top of the text is displayed. */
  int top;

  /** Determines the horizontal scroll (in columns). */
  int left;
  char _pad1[4];

  short flags;

  /** User preference, is font_size! */
  short lheight;

  int tabnumber;

  /* Booleans */
  char wordwrap;
  char doplugins;
  char showlinenrs;
  char showsyntax;
  char line_hlight;
  char overwrite;
  /** Run python while editing, evil. */
  char live_edit;
  char _pad2[1];

  char findstr[/*ST_MAX_FIND_STR*/ 256];
  char replacestr[/*ST_MAX_FIND_STR*/ 256];

  /** Column number to show right margin at. */
  short margin_column;
  char _pad3[2];

  /** Keep last. */
  SpaceText_Runtime *runtime;
} SpaceText;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Script View (Obsolete)
 * \{ */

/** Script Runtime Data - Obsolete (pre 2.5). */
typedef struct Script {
  ID id;

  void *py_draw;
  void *py_event;
  void *py_button;
  void *py_browsercallback;
  void *py_globaldict;

  int flags, lastspace;
  /**
   * Store the script file here so we can re-run it on loading blender,
   * if "Enable Scripts" is on
   */
  char scriptname[/*FILE_MAX*/ 1024];
  char scriptarg[/*FILE_MAXFILE*/ 256];
} Script;
#define SCRIPT_SET_NULL(_script) \
  _script->py_draw = _script->py_event = _script->py_button = _script->py_browsercallback = \
      _script->py_globaldict = NULL; \
  _script->flags = 0

/** Script View - Obsolete (pre 2.5). */
typedef struct SpaceScript {
  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  struct Script *script;

  short flags, menunr;
  char _pad1[4];

  void *but_refs;
} SpaceScript;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Nodes Editor
 * \{ */

typedef struct bNodeTreePath {
  struct bNodeTreePath *next, *prev;

  struct bNodeTree *nodetree;
  /** Base key for nodes in this tree instance. */
  bNodeInstanceKey parent_key;
  char _pad[4];
  /** V2d center point, so node trees can have different offsets in editors. */
  float view_center[2];

  char node_name[/*MAX_NAME*/ 64];
  char display_name[/*MAX_NAME*/ 64];
} bNodeTreePath;

typedef struct SpaceNodeOverlay {
  /* eSpaceNodeOverlay_Flag */
  int flag;
  /* eSpaceNodeOverlay_preview_shape */
  int preview_shape;
} SpaceNodeOverlay;

typedef struct SpaceNode {
  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  /** Deprecated, copied to region. */
  View2D v2d DNA_DEPRECATED;

  /** Context, no need to save in file? well... pinning... */
  struct ID *id, *from;

  short flag;

  /** Direction for offsetting nodes on insertion. */
  char insert_ofs_dir;
  char _pad1;

  /** Offset for drawing the backdrop. */
  float xof, yof;
  /** Zoom for backdrop. */
  float zoom;

  /**
   * XXX nodetree pointer info is all in the path stack now,
   * remove later on and use bNodeTreePath instead.
   * For now these variables are set when pushing/popping
   * from path stack, to avoid having to update all the functions and operators.
   * Can be done when design is accepted and everything is properly tested.
   */
  ListBase treepath;

  /* The tree farthest down in the group hierarchy. */
  struct bNodeTree *edittree;

  struct bNodeTree *nodetree;

  /* tree type for the current node tree */
  char tree_idname[64];
  /** Same as #bNodeTree::type (deprecated). */
  int treetype DNA_DEPRECATED;

  /** Texture-from object, world or brush (#eSpaceNode_TexFrom). */
  short texfrom;
  /** Shader from object or world (#eSpaceNode_ShaderFrom). */
  char shaderfrom;
  /**
   * The sub type of the node tree being edited.
   * #SpaceNodeGeometryNodesType or #SpaceNodeCompositorNodesType.
   */
  char node_tree_sub_type;

  /**
   * Used as the editor's top-level node group for node trees that are not part of the context and
   * thus needs to be stored in the node editor. For instance #SNODE_GEOMETRY_MODIFIER is part of
   * the context since it is stored on the active modifier, while #SNODE_GEOMETRY_TOOL is not part
   * of the context.
   */
  struct bNodeTree *selected_node_group;

  /** Grease-pencil data. */
  struct bGPdata *gpd;

  char gizmo_flag;
  char _pad2[7];

  SpaceNodeOverlay overlay;

  SpaceNode_Runtime *runtime;
} SpaceNode;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Console
 * \{ */

/** Console content. */
typedef struct ConsoleLine {
  struct ConsoleLine *next, *prev;

  /* Keep these 3 vars so as to share free, realloc functions. */
  /** Allocated length. */
  int len_alloc;
  /** Real length: `strlen()`. */
  int len;
  char *line;

  int cursor;
  /** Only for use when in the 'scrollback' listbase. */
  int type;
} ConsoleLine;

/** Console View. */
typedef struct SpaceConsole {
  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  /* Space variables. */

  /** ConsoleLine; output. */
  ListBase scrollback;
  /** ConsoleLine; command history, current edited line is the first. */
  ListBase history;
  char prompt[256];
  /** Multiple consoles are possible, not just python. */
  char language[32];

  int lheight;

  /** Index into history of most recent up/down arrow keys. */
  int history_index;

  /** Selection offset in bytes. */
  int sel_start;
  int sel_end;
} SpaceConsole;

/** \} */

/* -------------------------------------------------------------------- */
/** \name User Preferences
 * \{ */

typedef struct SpaceUserPref {
  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  char _pad1[7];
  char filter_type;
  /** Search term for filtering in the UI. */
  char filter[64];
} SpaceUserPref;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Motion Tracking
 * \{ */

typedef struct SpaceClipOverlay {
  /* eSpaceClipOverlay_Flag */
  int flag;
  char _pad0[4];
} SpaceClipOverlay;

/** Clip Editor. */
typedef struct SpaceClip {
  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  char gizmo_flag;
  char _pad1[3];

  /** User defined offset, image is centered. */
  float xof, yof;
  /** User defined offset from locked position. */
  float xlockof, ylockof;
  /** User defined zoom level. */
  float zoom;

  /** User of clip. */
  struct MovieClipUser user;
  /** Clip data. */
  struct MovieClip *clip;
  /** Different scoped displayed in space panels. */
  struct MovieClipScopes scopes;

  /** Flags. */
  int flag;
  /** Editor mode (editing context being displayed). */
  short mode;
  /** Type of the clip editor view. */
  short view;

  /** Length of displaying path, in frames. */
  int path_length;

  /* current stabilization data */
  /** Pre-composed stabilization data. */
  float loc[2], scale, angle;
  char _pad[4];
  /**
   * Current stabilization matrix and the same matrix in unified space,
   * defined when drawing and used for mouse position calculation.
   */
  float stabmat[4][4], unistabmat[4][4];

  /** Movie postprocessing. */
  int postproc_flag;

  /* grease pencil */
  short gpencil_src;
  char _pad2[2];

  /** Pivot point for transforms. */
  int around;
  char _pad4[4];

  /** Mask editor 2d cursor. */
  float cursor[2];

  MaskSpaceInfo mask_info;
  struct SpaceClipOverlay overlay;
} SpaceClip;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Top Bar
 * \{ */

typedef struct SpaceTopBar {
  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */
} SpaceTopBar;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Status Bar
 * \{ */

typedef struct SpaceStatusBar {
  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */
} SpaceStatusBar;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Spreadsheet
 * \{ */

typedef struct SpreadsheetColumnID {
  char *name;
} SpreadsheetColumnID;

typedef struct SpreadsheetColumn {
  /**
   * Identifies the data in the column.
   * This is a pointer instead of a struct to make it easier if we want to "subclass"
   * #SpreadsheetColumnID in the future for different kinds of ids.
   */
  SpreadsheetColumnID *id;

  /**
   * An indicator of the type of values in the column, set at runtime.
   * #eSpreadsheetColumnValueType.
   */
  uint8_t data_type;
  char _pad0[3];
  /** #eSpreadsheetColumnFlag. */
  uint32_t flag;
  /** Width in SPREADSHEET_WIDTH_UNIT. */
  float width;

  /**
   * A logical time set when the column is used. This is used to be able to remove long-unused
   * columns when there are too many. This is set from #SpreadsheetTable.column_use_clock.
   */
  uint32_t last_used;

  /**
   * The final column name generated by the data source, also just
   * cached at runtime when the data source columns are generated.
   */
  char *display_name;

  SpreadsheetColumnRuntime *runtime;

#ifdef __cplusplus
  bool is_available() const
  {
    return !(flag & SPREADSHEET_COLUMN_FLAG_UNAVAILABLE);
  }
#endif
} SpreadsheetColumn;

typedef struct SpreadsheetInstanceID {
  int reference_index;

#ifdef __cplusplus
  uint64_t hash() const;
  friend bool operator==(const SpreadsheetInstanceID &a, const SpreadsheetInstanceID &b);
  friend bool operator!=(const SpreadsheetInstanceID &a, const SpreadsheetInstanceID &b);
#endif
} SpreadsheetInstanceID;

typedef struct SpreadsheetTableID {
  /** #eSpreadsheetTableIDType. */
  int type;
} SpreadsheetTableID;

typedef struct SpreadsheetBundlePathElem {
  char *identifier;
#ifdef __cplusplus
  friend bool operator==(const SpreadsheetBundlePathElem &a, const SpreadsheetBundlePathElem &b);
  friend bool operator!=(const SpreadsheetBundlePathElem &a, const SpreadsheetBundlePathElem &b);
#endif
} SpreadsheetBundlePathElem;

typedef enum SpreadsheetClosureInputOutput {
  SPREADSHEET_CLOSURE_NONE = 0,
  SPREADSHEET_CLOSURE_INPUT = 1,
  SPREADSHEET_CLOSURE_OUTPUT = 2,
} SpreadsheetClosureInputOutput;

typedef struct SpreadsheetTableIDGeometry {
  SpreadsheetTableID base;
  char _pad0[4];
  /**
   * Context that is displayed in the editor. This is usually a either a single object (in
   * original/evaluated mode) or path to a viewer node. This is retrieved from the workspace but
   * can be pinned so that it stays constant even when the active node changes.
   */
  ViewerPath viewer_path;

  int viewer_item_identifier;

  int bundle_path_num;
  SpreadsheetBundlePathElem *bundle_path;

  /** #SpreadsheetClosureInputOutput. */
  int8_t closure_input_output;

  char _pad3[7];

  /**
   * The "path" to the currently active instance reference. This is needed when viewing nested
   * instances.
   */
  SpreadsheetInstanceID *instance_ids;
  int instance_ids_num;
  /** #GeometryComponent::Type. */
  uint8_t geometry_component_type;
  /** #AttrDomain. */
  uint8_t attribute_domain;
  /** #eSpaceSpreadsheet_ObjectEvalState. */
  uint8_t object_eval_state;
  char _pad1[5];
  /** Grease Pencil layer index for grease pencil component. */
  int layer_index;
} SpreadsheetTableIDGeometry;

typedef struct SpreadsheetTable {
  SpreadsheetTableID *id;
  /** All the columns in the table. */
  SpreadsheetColumn **columns;
  int num_columns;
  /** #eSpreadsheetTableFlag. */
  uint32_t flag;
  /**
   * A logical time set when the table is used. This is used to be able to remove long-unused
   * tables when there are too many. This is set from #SpaceSpreadsheet.table_use_clock.
   */
  uint32_t last_used;

  /**
   * This is increased whenever a new column is used. It allows for some garbage collection of
   * long-unused columns when there are too many.
   */
  uint32_t column_use_clock;
} SpreadsheetTable;

typedef struct SpaceSpreadsheet {
  SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  /** The current table and persisted state of previously displayed tables. */
  SpreadsheetTable **tables;
  int num_tables;
  char _pad1[3];

  /** #eSpaceSpreadsheet_FilterFlag. */
  uint8_t filter_flag;

  /** #SpreadsheetRowFilter. */
  ListBase row_filters;

  /** The currently active geometry data. This is used to look up the active table from #tables. */
  SpreadsheetTableIDGeometry geometry_id;

  /* eSpaceSpreadsheet_Flag. */
  uint32_t flag;
  /**
   * This is increased whenever a new table is used. It allows for some garbage collection of
   * long-unused tables when there are too many.
   */
  uint32_t table_use_clock;

  /** Index of the active viewer path element in the Data Source panel. */
  int active_viewer_path_index;
  char _pad2[4];

  SpaceSpreadsheet_Runtime *runtime;
} SpaceSpreadsheet;

typedef struct SpreadsheetRowFilter {
  struct SpreadsheetRowFilter *next, *prev;

  char column_name[/*MAX_NAME*/ 64];

  /* eSpreadsheetFilterOperation. */
  uint8_t operation;
  /* eSpaceSpreadsheet_RowFilterFlag. */
  uint8_t flag;

  char _pad0[6];

  int value_int;
  int value_int2[2];
  int value_int3[3];
  char *value_string;
  float value_float;
  float threshold;
  float value_float2[2];
  float value_float3[3];
  float value_color[4];
  char _pad1[4];
} SpreadsheetRowFilter;

/** \} */
