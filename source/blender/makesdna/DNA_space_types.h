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
#include "DNA_mask_types.h"
#include "DNA_movieclip_types.h" /* MovieClipUser */
#include "DNA_node_types.h"      /* for bNodeInstanceKey */
#include "DNA_outliner_types.h"  /* for TreeStoreElem */
#include "DNA_space_enums.h"
/* Hum ... Not really nice... but needed for spacebuts. */
#include "DNA_vec_defaults.h"
#include "DNA_view2d_types.h"
#include "DNA_viewer_path_types.h"

namespace blender {

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
struct SpaceUserPref_Runtime;

namespace asset_system {
class AssetRepresentation;
}

/** Defined in `buttons_intern.hh`. */
struct SpaceProperties_Runtime;

namespace ed::space_node {
struct SpaceNode_Runtime;
}  // namespace ed::space_node

namespace ed::outliner {
struct SpaceOutliner_Runtime;
}  // namespace ed::outliner

namespace ed::vse {
struct SpaceSeq_Runtime;
}  // namespace ed::vse

namespace ed::text {

struct SpaceText_Runtime;
}  // namespace ed::text

namespace ed::spreadsheet {
struct SpaceSpreadsheet_Runtime;
struct SpreadsheetColumnRuntime;
}  // namespace ed::spreadsheet

namespace ed::outliner {
struct TreeElement;
}

/** Defined in `file_intern.hh`. */
struct SpaceFile_Runtime;

/* -------------------------------------------------------------------- */
/** \name SpaceLink (Base)
 * \{ */

/**
 * The base structure all the other spaces
 * are derived (implicitly) from. Would be
 * good to make this explicit.
 */
struct SpaceLink {
  struct SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space Info
 * \{ */

/** Info Header. */
struct SpaceInfo {
  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  char rpt_mask = 0;
  char _pad[7] = {};
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Properties Editor
 * \{ */

/** Properties Editor. */
struct SpaceProperties {
  DNA_DEFINE_CXX_METHODS(SpaceProperties)

  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  /** Deprecated, copied to region. */
  DNA_DEPRECATED View2D v2d;

  /* For different kinds of property editors (exposed in the space type selector). */
  short space_subtype = 0;

  /** Context tabs. */
  short mainb = 0, mainbo = 0, mainbuser = 0;
  uint32_t visible_tabs = 0;
  /** Preview is signal to refresh. */
  short preview = 0;
  char flag = 0;

  /* eSpaceButtons_OutlinerSync */
  char outliner_sync = 0;

  /** Runtime. */
  void *path = nullptr;
  /** Runtime. */
  int pathflag = 0, dataicon = 0;
  ID *pinid = nullptr;

  void *texuser = nullptr;

  /* Doesn't necessarily need to be a pointer, but runtime structs are still written to files. */
  struct SpaceProperties_Runtime *runtime = nullptr;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Outliner
 * \{ */

/** Outliner */
struct SpaceOutliner {
  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  /** Deprecated, copied to region. */
  DNA_DEPRECATED View2D v2d;

  ListBaseT<ed::outliner::TreeElement> tree = {nullptr, nullptr};

  /**
   * Treestore is an ordered list of TreeStoreElem's from outliner tree;
   * Note that treestore may contain duplicate elements if element
   * is used multiple times in outliner tree (e. g. linked objects)
   * Also note that BLI_mempool can not be read/written in DNA directly,
   * therefore `readfile.cc` / `writefile.cc` linearize treestore into #TreeStore structure.
   */
  struct BLI_mempool *treestore = nullptr;

  char search_string[64] = "";

  short flag = 0;
  short outlinevis = 0;
  short lib_override_view_mode = 0;
  short storeflag = 0;
  char search_flags = 0;
  char _pad[6] = {};

  /** Selection syncing flag (#WM_OUTLINER_SYNC_SELECT_FROM_OBJECT and similar flags). */
  char sync_select_dirty = 0;

  int filter = 0;
  char filter_state = 0;
  char show_restrict_flags = 0;
  short filter_id_type = 0;

  ed::outliner::SpaceOutliner_Runtime *runtime = nullptr;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Graph Editor
 * \{ */

struct SpaceGraph_Runtime {
  /** #eGraphEdit_Runtime_Flag */
  char flag = 0;
  char _pad[7] = {};
  /** Sampled snapshots of F-Curves used as in-session guides */
  ListBaseT<FCurve> ghost_curves = {nullptr, nullptr};
};

/** 'Graph' Editor (formerly known as the IPO Editor). */
struct SpaceGraph {
  DNA_DEFINE_CXX_METHODS(SpaceGraph)

  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  /** Deprecated, copied to region. */
  DNA_DEPRECATED View2D v2d;

  /** Settings for filtering animation data
   * \note we use a pointer due to code-linking issues. */
  struct bDopeSheet *ads = nullptr;

  /** Mode for the Graph editor (eGraphEdit_Mode). */
  short mode = 0;
  /* Snapping now lives on the Scene. */
  DNA_DEPRECATED short autosnap = 0;
  /** Settings for Graph editor (eGraphEdit_Flag). */
  int flag = 0;

  /** Time value for cursor (when in drivers mode; animation uses current frame). */
  float cursorTime = 0;
  /** Cursor value (y-value, x-value is current frame). */
  float cursorVal = 0;
  /** Pivot point for transforms. */
  int around = 0;
  char _pad[4] = {};

  SpaceGraph_Runtime runtime;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name NLA Editor
 * \{ */

/** NLA Editor */
struct SpaceNla {
  DNA_DEFINE_CXX_METHODS(SpaceNla)

  struct SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  /* Snapping now lives on the Scene. */
  DNA_DEPRECATED short autosnap = 0;
  short flag = 0;
  char _pad[4] = {};

  struct bDopeSheet *ads = nullptr;
  /** Deprecated, copied to region. */
  DNA_DEPRECATED View2D v2d;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sequence Editor
 * \{ */

struct SequencerPreviewOverlay {
  int flag = 0;
  char _pad0[4] = {};
};

struct SequencerTimelineOverlay {
  int flag = 0;
  char _pad0[4] = {};
};

struct SequencerCacheOverlay {
  int flag = 0;
  char _pad0[4] = {};
};

/** Sequencer. */
struct SpaceSeq {
  DNA_DEFINE_CXX_METHODS(SpaceSeq)

  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  /** Deprecated, copied to region. */
  DNA_DEPRECATED View2D v2d;

  /** Deprecated: offset for drawing the image preview. */
  DNA_DEPRECATED float xof = 0;
  DNA_DEPRECATED float yof = 0;
  short mainb = 0; /* eSpaceSeq_RegionType; strange name for view type (image, histogram, ...). */
  short render_size = 0; /* eSpaceSeq_Proxy_RenderSize. */
  short chanshown = 0;
  short zebra = 0; /* Show overexposed. 0=disabled; otherwise as percentage of "pure white". */
  int flag = 0;
  /** Deprecated, handled by View2D now. */
  DNA_DEPRECATED float zoom = 0;
  /** See SEQ_VIEW_* below. */
  char view = 0;
  char overlay_frame_type = 0;
  /** Overlay an image of the editing on below the strips. */
  char draw_flag = 0;
  char gizmo_flag = 0;
  char _pad[4] = {};

  /** 2D cursor for transform. */
  float cursor[2] = {};

  /** Grease-pencil data. */
  struct bGPdata *gpd = nullptr;

  struct SequencerPreviewOverlay preview_overlay;
  struct SequencerTimelineOverlay timeline_overlay;
  struct SequencerCacheOverlay cache_overlay;

  /** Multi-view current eye - for internal use. */
  char multiview_eye = 0;
  char _pad2[7] = {};

  ed::vse::SpaceSeq_Runtime *runtime = nullptr;
};

struct MaskSpaceInfo {
  /* **** mask editing **** */
  struct Mask *mask = nullptr;
  /* draw options */
  char draw_flag = MASK_DRAWFLAG_SPLINE;         /* MaskDrawFlag */
  char draw_type = MASK_DT_OUTLINE;              /* MaskDrawType */
  char overlay_mode = MASK_OVERLAY_ALPHACHANNEL; /* MaskOverlayMode */
  char _pad3[1] = {};
  float blend_factor = 0.7f;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name File Selector
 * \{ */

/** Config and Input for File Selector. */
struct FileSelectParams {
  /** Title, also used for the text of the execute button. */
  char title[96] = "";
  /**
   * Directory.
   *
   * \note #FILE_MAX_LIBEXTRA == `1024 + 258`, this is for extreme case when 1023 length path
   * needs to be linked in, where `foo.blend/Armature` need adding.
   */
  char dir[/*FILE_MAX_LIBEXTRA*/ 1282] = "";
  char file[/*FILE_MAXFILE*/ 256] = "";

  char renamefile[/*FILE_MAXFILE*/ 256] = "";
  short rename_flag = 0;
  char _pad[4] = {};
  /** An ID that was just renamed. Used to identify a renamed asset file over re-reads, similar to
   * `renamefile` but for local IDs (takes precedence). Don't keep this stored across handlers!
   * Would break on undo. */
  const ID *rename_id = nullptr;
  void *_pad3 = nullptr;

  /** List of file-types to filter. */
  char filter_glob[/*FILE_MAXFILE*/ 256] = "";

  /** Text items name must match to be shown. */
  char filter_search[64] = "";
  /** Same as filter, but for ID types (aka library groups). */
  uint64_t filter_id = 0;

  /** Active file used for keyboard navigation. -1 means no active file (cleared e.g. after
   * directory change or search update). */
  int active_file = 0;
  /** File under cursor. */
  int highlight_file = 0;
  int sel_first = 0;
  int sel_last = 0;
  unsigned short thumbnail_size = 0;
  unsigned short list_thumbnail_size = 0;
  unsigned short list_column_size = 0;

  /* short */
  /** XXX: for now store type here, should be moved to the operator. */
  short type = 0; /* eFileSelectType */
  /** Settings for filter, hiding dots files. */
  short flag = 0;
  /** Sort order. */
  short sort = 0;
  /** Display mode flag. */
  short display = 0;
  /** Details toggles (file size, creation date, etc.) */
  char details_flags = 0;
  char _pad1 = {};

  /** Filter when (flags & FILE_FILTER) is true. */
  int filter = 0;

  /** Max number of levels in directory tree to show at once, 0 to disable recursion. */
  short recursion_level = 0;

  char _pad2[2] = {};
};

/**
 * File selection parameters for asset browsing mode, with #FileSelectParams as base.
 */
struct FileAssetSelectParams {
  FileSelectParams base_params;

  AssetLibraryReference asset_library_ref;
  short asset_catalog_visibility = 0; /* eFileSel_Params_AssetCatalogVisibility */
  char _pad[6] = {};
  /** If #asset_catalog_visibility is #FILE_SHOW_ASSETS_FROM_CATALOG, this sets the ID of the
   * catalog to show. */
  bUUID catalog_id;

  short import_method = 0; /* eFileAssetImportMethod */
  short import_flags = 0;  /* eFileImportFlags */
  char _pad2[4] = {};
};

/**
 * A wrapper to store previous and next folder lists (#FolderList) for a specific browse mode
 * (#eFileBrowse_Mode).
 */
struct FileFolderHistory {
  struct FileFolderLists *next = nullptr, *prev = nullptr;

  /** The browse mode this prev/next folder-lists are created for. */
  char browse_mode = 0; /* eFileBrowse_Mode */
  char _pad[7] = {};

  /** Holds the list of previous directories to show. */
  ListBaseT<struct FolderList> folders_prev = {nullptr, nullptr};
  /** Holds the list of next directories (pushed from previous) to show. */
  ListBaseT<struct FolderList> folders_next = {nullptr, nullptr};
};

/** File Browser. */
struct SpaceFile {
  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  /** Is this a File Browser or an Asset Browser? */
  char browse_mode = 0; /* eFileBrowse_Mode */
  char _pad1[1] = {};

  short tags = 0;

  int scroll_offset = 0;

  /** Config and input for file select. One for each browse-mode, to keep them independent. */
  FileSelectParams *params = nullptr;
  FileAssetSelectParams *asset_params = nullptr;

  void *_pad2 = nullptr;

  /**
   * Holds the list of files to show.
   * Currently recreated when browse-mode changes. Could be per browse-mode to avoid refreshes.
   */
  struct FileList *files = nullptr;

  /**
   * Holds the list of previous directories to show. Owned by `folder_histories` below.
   */
  ListBaseT<struct FolderList> *folders_prev = nullptr;
  /**
   * Holds the list of next directories (pushed from previous) to show. Owned by
   * `folder_histories` below.
   */
  ListBaseT<struct FolderList> *folders_next = nullptr;

  /**
   * This actually owns the prev/next folder-lists above. On browse-mode change, the lists of the
   * new mode get assigned to the above.
   */
  ListBaseT<FileFolderHistory> folder_histories = {nullptr, nullptr};

  /**
   * The operator that is invoking file-select `op->exec()` will be called on the 'Load' button.
   * if operator provides op->cancel(), then this will be invoked on the cancel button.
   */
  struct wmOperator *op = nullptr;

  struct wmTimer *smoothscroll_timer = nullptr;
  struct wmTimer *previews_timer = nullptr;

  struct FileLayout *layout = nullptr;

  short recentnr = 0, bookmarknr = 0;
  short systemnr = 0, system_bookmarknr = 0;

  SpaceFile_Runtime *runtime = nullptr;
};

/* ***** Related to file browser, but never saved in DNA, only here to help with RNA. ***** */

#
#
struct FileDirEntry {
  struct FileDirEntry *next = nullptr, *prev = nullptr;

  uint32_t uid = 0; /* FileUID */
  /* Name needs freeing if FILE_ENTRY_NAME_FREE is set. Otherwise this is a direct pointer to a
   * name buffer. */
  const char *name = nullptr;

  uint64_t size = 0;
  int64_t time = 0;

  struct {
    /* Temp caching of UI-generated strings. */
    char size_str[16] = "";
    char datetime_str[16 + 8] = "";
  } draw_data;

  /** #eFileSel_File_Types. */
  int typeflag = 0;
  /** ID type, in case typeflag has FILE_TYPE_BLENDERLIB set. */
  int blentype = 0;

  /* Path to item that is relative to current folder root. To get the full path, use
   * #filelist_file_get_full_path() */
  char *relpath = nullptr;
  /** Optional argument for shortcuts, aliases etc. */
  char *redirection_path = nullptr;

  /** When showing local IDs (FILE_MAIN, FILE_MAIN_ASSET), ID this file represents. Note comment
   * for FileListInternEntry.local_data, the same applies here! */
  ID *id = nullptr;
  /** If this file represents an asset, its asset data is here. Note that we may show assets of
   * external files in which case this is set but not the id above.
   * Note comment for FileListInternEntry.local_data, the same applies here! */
  asset_system::AssetRepresentation *asset = nullptr;

  /* The icon_id for the preview image. */
  int preview_icon_id = 0;

  short flags = 0;
  /* eFileAttributes defined in BLI_fileops.h */
  int attributes = 0;
};

/**
 * Array of directory entries.
 *
 * Stores the total number of available entries, the number of visible (filtered) entries, and a
 * subset of those in 'entries' ListBase, from idx_start (included) to idx_end (excluded).
 */
#
#
struct FileDirEntryArr {
  ListBaseT<struct FileListInternEntry> entries = {nullptr, nullptr};
  int entries_num = 0;
  int entries_filtered_num = 0;

  char root[/*FILE_MAX*/ 1024] = "";
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image/UV Editor
 * \{ */

/* Image/UV Editor */

struct SpaceImageOverlay {
  int flag = 0;
  float passepartout_alpha = 0;
};

struct SpaceImage {
  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  struct Image *image = nullptr;
  struct ImageUser iuser;

  /** Histogram waveform and vector-scope. */
  struct Scopes scopes;
  /** Sample line histogram. */
  struct Histogram sample_line_hist;

  /** Grease pencil data. */
  struct bGPdata *gpd = nullptr;

  /** UV editor 2d cursor. */
  float cursor[2] = {};
  /** User defined offset, image is centered. */
  float xof = 0, yof = 0;
  /** User defined zoom level. */
  float zoom = 0;
  /** Storage for offset while render drawing. */
  float centx = 0, centy = 0;

  /** View/paint/mask. */
  char mode = 0;
  /* Storage for sub-space types. */
  char mode_prev = 0;

  char pin = 0;

  char pixel_round_mode = 0;

  char lock = 0;
  /** UV draw type. */
  char dt_uv = 0;
  /** Sticky selection type. */
  char dt_uvstretch = 0;
  char around = 0;

  char gizmo_flag = 0;

  char grid_shape_source = 0;
  char _pad1[6] = {};

  int flag = 0;

  float uv_opacity = 0;
  float uv_face_opacity = 0;
  float uv_edge_opacity = 0;

  float stretch_opacity = 0;

  int tile_grid_shape[2] = {};
  /**
   * UV editor custom-grid. Value of `{M,N}` will produce `MxN` grid.
   * Use when `custom_grid_shape == SI_GRID_SHAPE_FIXED`.
   */
  int custom_grid_subdiv[2] = {};

  MaskSpaceInfo mask_info;
  SpaceImageOverlay overlay;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Editor
 * \{ */

/** Text Editor. */
struct SpaceText {
  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  struct Text *text = nullptr;

  /** Determines at what line the top of the text is displayed. */
  int top = 0;

  /** Determines the horizontal scroll (in columns). */
  int left = 0;
  char _pad1[4] = {};

  short flags = 0;

  /** User preference, is font_size! */
  short lheight = 0;

  int tabnumber = 0;

  /* Booleans */
  char wordwrap = 0;
  char doplugins = 0;
  char showlinenrs = 0;
  char showsyntax = 0;
  char line_hlight = 0;
  char overwrite = 0;
  /** Run python while editing, evil. */
  char live_edit = 0;
  char _pad2[1] = {};

  char findstr[/*ST_MAX_FIND_STR*/ 256] = "";
  char replacestr[/*ST_MAX_FIND_STR*/ 256] = "";

  /** Column number to show right margin at. */
  short margin_column = 0;
  char _pad3[2] = {};

  /** Keep last. */
  ed::text::SpaceText_Runtime *runtime = nullptr;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Script View (Obsolete)
 * \{ */

/** Script Runtime Data - Obsolete (pre 2.5). */
struct Script {
  ID id;

  void *py_draw = nullptr;
  void *py_event = nullptr;
  void *py_button = nullptr;
  void *py_browsercallback = nullptr;
  void *py_globaldict = nullptr;

  int flags = 0, lastspace = 0;
  /**
   * Store the script file here so we can re-run it on loading blender,
   * if "Enable Scripts" is on
   */
  char scriptname[/*FILE_MAX*/ 1024] = "";
  char scriptarg[/*FILE_MAXFILE*/ 256] = "";
};
#define SCRIPT_SET_NULL(_script) \
  _script->py_draw = _script->py_event = _script->py_button = _script->py_browsercallback = \
      _script->py_globaldict = nullptr; \
  _script->flags = 0

/** Script View - Obsolete (pre 2.5). */
struct SpaceScript {
  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  struct Script *script = nullptr;

  short flags = 0, menunr = 0;
  char _pad1[4] = {};

  void *but_refs = nullptr;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Nodes Editor
 * \{ */

struct bNodeTreePath {
  struct bNodeTreePath *next = nullptr, *prev = nullptr;

  struct bNodeTree *nodetree = nullptr;
  /** Base key for nodes in this tree instance. */
  bNodeInstanceKey parent_key;
  char _pad[4] = {};
  /** V2d center point, so node trees can have different offsets in editors. */
  float view_center[2] = {};

  char node_name[/*MAX_NAME*/ 64] = "";
  char display_name[/*MAX_NAME*/ 64] = "";
};

struct SpaceNodeOverlay {
  /* eSpaceNodeOverlay_Flag */
  int flag = 0;
  /* eSpaceNodeOverlay_preview_shape */
  int preview_shape = 0;
};

struct SpaceNode {
  DNA_DEFINE_CXX_METHODS(SpaceNode)

  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  /** Deprecated, copied to region. */
  DNA_DEPRECATED View2D v2d;

  /** Context, no need to save in file? well... pinning... */
  struct ID *id = nullptr, *from = nullptr;

  short flag = 0;

  /** Direction for offsetting nodes on insertion. */
  char insert_ofs_dir = 0;
  char _pad1 = {};

  /** Offset for drawing the backdrop. */
  float xof = 0, yof = 0;
  /** Zoom for backdrop. */
  float zoom = 0;

  /**
   * XXX nodetree pointer info is all in the path stack now,
   * remove later on and use bNodeTreePath instead.
   * For now these variables are set when pushing/popping
   * from path stack, to avoid having to update all the functions and operators.
   * Can be done when design is accepted and everything is properly tested.
   */
  ListBaseT<bNodeTreePath> treepath = {nullptr, nullptr};

  /* The tree farthest down in the group hierarchy. */
  struct bNodeTree *edittree = nullptr;

  struct bNodeTree *nodetree = nullptr;

  /* tree type for the current node tree */
  char tree_idname[64] = "";
  /** Same as #bNodeTree::type (deprecated). */
  DNA_DEPRECATED int treetype = 0;

  /** Texture-from object, world or brush (#eSpaceNode_TexFrom). */
  short texfrom = 0;
  /** Shader from object or world (#eSpaceNode_ShaderFrom). */
  char shaderfrom = 0;
  /**
   * The sub type of the node tree being edited.
   * #SpaceNodeGeometryNodesType or #SpaceNodeCompositorNodesType.
   */
  char node_tree_sub_type = 0;

  /**
   * Used as the editor's top-level node group for node trees that are not part of the context and
   * thus needs to be stored in the node editor. For instance #SNODE_GEOMETRY_MODIFIER is part of
   * the context since it is stored on the active modifier, while #SNODE_GEOMETRY_TOOL is not part
   * of the context.
   */
  struct bNodeTree *selected_node_group = nullptr;

  /** Grease-pencil data. */
  struct bGPdata *gpd = nullptr;

  char gizmo_flag = 0;
  char _pad2[7] = {};

  SpaceNodeOverlay overlay;

  ed::space_node::SpaceNode_Runtime *runtime = nullptr;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Console
 * \{ */

/** Console content. */
struct ConsoleLine {
  struct ConsoleLine *next = nullptr, *prev = nullptr;

  /* Keep these 3 vars so as to share free, realloc functions. */
  /** Allocated length. */
  int len_alloc = 0;
  /** Real length: `strlen()`. */
  int len = 0;
  char *line = nullptr;

  int cursor = 0;
  /** Only for use when in the 'scrollback' listbase. */
  int type = 0;
};

/** Console View. */
struct SpaceConsole {
  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  /* Space variables. */

  /** ConsoleLine; output. */
  ListBaseT<ConsoleLine> scrollback = {nullptr, nullptr};
  /** ConsoleLine; command history, current edited line is the first. */
  ListBaseT<ConsoleLine> history = {nullptr, nullptr};
  char prompt[256] = "";
  /** Multiple consoles are possible, not just python. */
  char language[32] = "";

  int lheight = 0;

  /** Index into history of most recent up/down arrow keys. */
  int history_index = 0;

  /** Selection offset in bytes. */
  int sel_start = 0;
  int sel_end = 0;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name User Preferences
 * \{ */

struct SpaceUserPref {
  DNA_DEFINE_CXX_METHODS(SpaceUserPref)
  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  char _pad1[7] = {};
  char filter_type = 0;
  /** Search term for filtering in the UI. */
  char filter[64] = "";
  SpaceUserPref_Runtime *runtime = nullptr;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Motion Tracking
 * \{ */

struct SpaceClipOverlay {
  /* eSpaceClipOverlay_Flag */
  int flag = SC_SHOW_OVERLAYS | SC_SHOW_CURSOR;
  char _pad0[4] = {};
};

/** Clip Editor. */
struct SpaceClip {
  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = SPACE_CLIP;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  char gizmo_flag = 0;
  char _pad1[3] = {};

  /** User defined offset, image is centered. */
  float xof = 0, yof = 0;
  /** User defined offset from locked position. */
  float xlockof = 0, ylockof = 0;
  /** User defined zoom level. */
  float zoom = 1.0f;

  /** User of clip. */
  struct MovieClipUser user;
  /** Clip data. */
  struct MovieClip *clip = nullptr;
  /** Different scoped displayed in space panels. */
  struct MovieClipScopes scopes;

  /** Flags. */
  int flag = SC_SHOW_MARKER_PATTERN | SC_SHOW_TRACK_PATH | SC_SHOW_GRAPH_TRACKS_MOTION |
             SC_SHOW_GRAPH_FRAMES | SC_SHOW_ANNOTATION;
  /** Editor mode (editing context being displayed). */
  short mode = SC_MODE_TRACKING;
  /** Type of the clip editor view. */
  short view = SC_VIEW_CLIP;

  /** Length of displaying path, in frames. */
  int path_length = 20;

  /* current stabilization data */
  /** Pre-composed stabilization data. */
  float loc[2] = {0, 0}, scale = 0, angle = 0;
  char _pad[4] = {};
  /**
   * Current stabilization matrix and the same matrix in unified space,
   * defined when drawing and used for mouse position calculation.
   */
  float stabmat[4][4] = _DNA_DEFAULT_UNIT_M4;
  float unistabmat[4][4] = _DNA_DEFAULT_UNIT_M4;

  /** Movie postprocessing (#MovieClipPostprocFlag). */
  int postproc_flag = 0;

  /* grease pencil */
  short gpencil_src = SC_GPENCIL_SRC_CLIP;
  char _pad2[2] = {};

  /** Pivot point for transforms. */
  int around = V3D_AROUND_CENTER_MEDIAN;
  char _pad4[4] = {};

  /** Mask editor 2d cursor. */
  float cursor[2] = {0, 0};

  MaskSpaceInfo mask_info;
  struct SpaceClipOverlay overlay;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Top Bar
 * \{ */

struct SpaceTopBar {
  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Status Bar
 * \{ */

struct SpaceStatusBar {
  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Spreadsheet
 * \{ */

enum SpreadsheetClosureInputOutput {
  SPREADSHEET_CLOSURE_NONE = 0,
  SPREADSHEET_CLOSURE_INPUT = 1,
  SPREADSHEET_CLOSURE_OUTPUT = 2,
};

struct SpreadsheetColumnID {
  char *name = nullptr;
};

struct SpreadsheetColumn {
  /**
   * Identifies the data in the column.
   * This is a pointer instead of a struct to make it easier if we want to "subclass"
   * #SpreadsheetColumnID in the future for different kinds of ids.
   */
  SpreadsheetColumnID *id = nullptr;

  /**
   * An indicator of the type of values in the column, set at runtime.
   * #eSpreadsheetColumnValueType.
   */
  uint8_t data_type = 0;
  char _pad0[3] = {};
  /** #eSpreadsheetColumnFlag. */
  uint32_t flag = 0;
  /** Width in SPREADSHEET_WIDTH_UNIT. */
  float width = 0;

  /**
   * A logical time set when the column is used. This is used to be able to remove long-unused
   * columns when there are too many. This is set from #SpreadsheetTable.column_use_clock.
   */
  uint32_t last_used = 0;

  /**
   * The final column name generated by the data source, also just
   * cached at runtime when the data source columns are generated.
   */
  char *display_name = nullptr;

  ed::spreadsheet::SpreadsheetColumnRuntime *runtime = nullptr;

#ifdef __cplusplus
  bool is_available() const
  {
    return !(flag & SPREADSHEET_COLUMN_FLAG_UNAVAILABLE);
  }
#endif
};

struct SpreadsheetInstanceID {
  int reference_index = 0;

#ifdef __cplusplus
  uint64_t hash() const;
  friend bool operator==(const SpreadsheetInstanceID &a, const SpreadsheetInstanceID &b);
  friend bool operator!=(const SpreadsheetInstanceID &a, const SpreadsheetInstanceID &b);
#endif
};

struct SpreadsheetTableID {
  /** #eSpreadsheetTableIDType. */
  int type = 0;
};

struct SpreadsheetBundlePathElem {
  char *identifier = nullptr;
#ifdef __cplusplus
  friend bool operator==(const SpreadsheetBundlePathElem &a, const SpreadsheetBundlePathElem &b);
  friend bool operator!=(const SpreadsheetBundlePathElem &a, const SpreadsheetBundlePathElem &b);
#endif
};

struct SpreadsheetTableIDGeometry {
  SpreadsheetTableID base;
  char _pad0[4] = {};
  /**
   * Context that is displayed in the editor. This is usually a either a single object (in
   * original/evaluated mode) or path to a viewer node. This is retrieved from the workspace but
   * can be pinned so that it stays constant even when the active node changes.
   */
  ViewerPath viewer_path;

  int viewer_item_identifier = 0;

  int bundle_path_num = 0;
  SpreadsheetBundlePathElem *bundle_path = nullptr;

  /** #SpreadsheetClosureInputOutput. */
  int8_t closure_input_output = 0;

  char _pad3[7] = {};

  /**
   * The "path" to the currently active instance reference. This is needed when viewing nested
   * instances.
   */
  SpreadsheetInstanceID *instance_ids = nullptr;
  int instance_ids_num = 0;
  /** #GeometryComponent::Type. */
  uint8_t geometry_component_type = 0;
  /** #AttrDomain. */
  uint8_t attribute_domain = 0;
  /** #eSpaceSpreadsheet_ObjectEvalState. */
  uint8_t object_eval_state = 0;
  char _pad1[5] = {};
  /** Grease Pencil layer index for grease pencil component. */
  int layer_index = 0;
};

struct SpreadsheetTable {
  SpreadsheetTableID *id = nullptr;
  /** All the columns in the table. */
  SpreadsheetColumn **columns = nullptr;
  int num_columns = 0;
  /** #eSpreadsheetTableFlag. */
  uint32_t flag = 0;
  /**
   * A logical time set when the table is used. This is used to be able to remove long-unused
   * tables when there are too many. This is set from #SpaceSpreadsheet.table_use_clock.
   */
  uint32_t last_used = 0;

  /**
   * This is increased whenever a new column is used. It allows for some garbage collection of
   * long-unused columns when there are too many.
   */
  uint32_t column_use_clock = 0;
};

struct SpaceSpreadsheet {
  SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  /** The current table and persisted state of previously displayed tables. */
  SpreadsheetTable **tables = nullptr;
  int num_tables = 0;
  char _pad1[3] = {};

  /** #eSpaceSpreadsheet_FilterFlag. */
  uint8_t filter_flag = 0;

  ListBaseT<struct SpreadsheetRowFilter> row_filters = {nullptr, nullptr};

  /** The currently active geometry data. This is used to look up the active table from #tables. */
  SpreadsheetTableIDGeometry geometry_id;

  /* eSpaceSpreadsheet_Flag. */
  uint32_t flag = 0;
  /**
   * This is increased whenever a new table is used. It allows for some garbage collection of
   * long-unused tables when there are too many.
   */
  uint32_t table_use_clock = 0;

  /** Index of the active viewer path element in the Data Source panel. */
  int active_viewer_path_index = 0;
  char _pad2[4] = {};

  ed::spreadsheet::SpaceSpreadsheet_Runtime *runtime = nullptr;
};

struct SpreadsheetRowFilter {
  struct SpreadsheetRowFilter *next = nullptr, *prev = nullptr;

  char column_name[/*MAX_NAME*/ 64] = "";

  /* eSpreadsheetFilterOperation. */
  uint8_t operation = 0;
  /* eSpaceSpreadsheet_RowFilterFlag. */
  uint8_t flag = 0;

  char _pad0[6] = {};

  int value_int = 0;
  int value_int2[2] = {};
  int value_int3[3] = {};
  char *value_string = nullptr;
  float value_float = 0;
  float threshold = 0;
  float value_float2[2] = {};
  float value_float3[3] = {};
  float value_color[4] = {};
  char _pad1[4] = {};
};

/** \} */

}  // namespace blender
