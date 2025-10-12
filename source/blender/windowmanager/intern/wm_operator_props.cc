/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Generic re-usable property definitions and accessors for operators to share.
 * (`WM_operator_properties_*` functions).
 */

#include "DNA_ID_enums.h"
#include "DNA_space_types.h"

#include "BKE_lib_id.hh"
#include "BKE_main.hh"

#include "BLI_math_base.h"
#include "BLI_rect.h"

#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "ED_select_utils.hh"

#include "WM_api.hh"
#include "WM_types.hh"

void WM_operator_properties_confirm_or_exec(wmOperatorType *ot)
{
  PropertyRNA *prop;

  prop = RNA_def_boolean(ot->srna, "confirm", true, "Confirm", "Prompt for confirmation");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/**
 * Extends rna_enum_fileselect_params_sort_items with a default item for operators to use.
 */
static const EnumPropertyItem *wm_operator_properties_filesel_sort_items_itemf(
    bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free)
{
  EnumPropertyItem *items;
  const EnumPropertyItem default_item = {
      FILE_SORT_DEFAULT,
      "DEFAULT",
      0,
      "Default",
      "Automatically determine sort method for files",
  };
  int totitem = 0;

  RNA_enum_item_add(&items, &totitem, &default_item);
  RNA_enum_items_add(&items, &totitem, rna_enum_fileselect_params_sort_items);
  RNA_enum_item_end(&items, &totitem);
  *r_free = true;

  return items;
}

void WM_operator_properties_filesel(wmOperatorType *ot,
                                    const int filter,
                                    const short type,
                                    const eFileSel_Action action,
                                    const eFileSel_Flag flag,
                                    const short display,
                                    const short sort)
{
  PropertyRNA *prop;

  static const EnumPropertyItem file_display_items[] = {
      {FILE_DEFAULTDISPLAY,
       "DEFAULT",
       0,
       "Default",
       "Automatically determine display type for files"},
      {FILE_VERTICALDISPLAY,
       "LIST_VERTICAL",
       ICON_SHORTDISPLAY, /* Name of deprecated short list. */
       "Short List",
       "Display files as short list"},
      {FILE_HORIZONTALDISPLAY,
       "LIST_HORIZONTAL",
       ICON_LONGDISPLAY, /* Name of deprecated long list. */
       "Long List",
       "Display files as a detailed list"},
      {FILE_IMGDISPLAY, "THUMBNAIL", ICON_IMGDISPLAY, "Thumbnails", "Display files as thumbnails"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  if (flag & WM_FILESEL_FILEPATH) {
    prop = RNA_def_string_file_path(
        ot->srna, "filepath", nullptr, FILE_MAX, "File Path", "Path to file");
    RNA_def_property_flag(prop, PROP_SKIP_PRESET);
  }

  if (flag & WM_FILESEL_DIRECTORY) {
    prop = RNA_def_string_dir_path(
        ot->srna, "directory", nullptr, FILE_MAX, "Directory", "Directory of the file");
    RNA_def_property_flag(prop, PROP_SKIP_PRESET);
  }

  if (flag & WM_FILESEL_FILENAME) {
    prop = RNA_def_string_file_name(
        ot->srna, "filename", nullptr, FILE_MAX, "File Name", "Name of the file");
    RNA_def_property_flag(prop, PROP_SKIP_PRESET);
  }

  if (flag & WM_FILESEL_FILES) {
    prop = RNA_def_collection_runtime(
        ot->srna, "files", &RNA_OperatorFileListElement, "Files", "");
    RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE | PROP_SKIP_PRESET);
  }

  if ((flag & WM_FILESEL_SHOW_PROPS) == 0) {
    prop = RNA_def_boolean(ot->srna,
                           "hide_props_region",
                           true,
                           "Hide Operator Properties",
                           "Collapse the region displaying the operator settings");
    RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  }

  /* NOTE: this is only used to check if we should highlight the filename area red when the
   * filepath is an existing file. */
  prop = RNA_def_boolean(ot->srna,
                         "check_existing",
                         action == FILE_SAVE,
                         "Check Existing",
                         "Check and warn on overwriting existing files");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna, "filter_blender", (filter & FILE_TYPE_BLENDER) != 0, "Filter .blend files", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "filter_backup",
                         (filter & FILE_TYPE_BLENDER_BACKUP) != 0,
                         "Filter .blend files",
                         "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "filter_image", (filter & FILE_TYPE_IMAGE) != 0, "Filter image files", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "filter_movie", (filter & FILE_TYPE_MOVIE) != 0, "Filter movie files", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "filter_python", (filter & FILE_TYPE_PYSCRIPT) != 0, "Filter Python files", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "filter_font", (filter & FILE_TYPE_FTFONT) != 0, "Filter font files", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "filter_sound", (filter & FILE_TYPE_SOUND) != 0, "Filter sound files", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "filter_text", (filter & FILE_TYPE_TEXT) != 0, "Filter text files", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "filter_archive", (filter & FILE_TYPE_ARCHIVE) != 0, "Filter archive files", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "filter_btx", (filter & FILE_TYPE_BTX) != 0, "Filter btx files", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "filter_alembic", (filter & FILE_TYPE_ALEMBIC) != 0, "Filter Alembic files", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "filter_usd", (filter & FILE_TYPE_USD) != 0, "Filter USD files", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "filter_obj", (filter & FILE_TYPE_OBJECT_IO) != 0, "Filter OBJ files", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "filter_volume",
                         (filter & FILE_TYPE_VOLUME) != 0,
                         "Filter OpenVDB volume files",
                         "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "filter_folder", (filter & FILE_TYPE_FOLDER) != 0, "Filter folders", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "filter_blenlib", (filter & FILE_TYPE_BLENDERLIB) != 0, "Filter Blender IDs", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  /* TODO: asset only filter? */

  prop = RNA_def_int(
      ot->srna,
      "filemode",
      type,
      FILE_LOADLIB,
      FILE_SPECIAL,
      "File Browser Mode",
      "The setting for the file browser mode to load a .blend file, a library or a special file",
      FILE_LOADLIB,
      FILE_SPECIAL);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  if (flag & WM_FILESEL_RELPATH) {
    RNA_def_boolean(ot->srna,
                    "relative_path",
                    true,
                    "Relative Path",
                    "Select the file relative to the blend file");
  }

  if ((filter & FILE_TYPE_IMAGE) || (filter & FILE_TYPE_MOVIE)) {
    prop = RNA_def_boolean(ot->srna, "show_multiview", false, "Enable Multi-View", "");
    RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
    prop = RNA_def_boolean(ot->srna, "use_multiview", false, "Use Multi-View", "");
    RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  }

  prop = RNA_def_enum(ot->srna, "display_type", file_display_items, display, "Display Type", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_enum(
      ot->srna, "sort_method", rna_enum_dummy_NULL_items, sort, "File sorting mode", "");
  RNA_def_enum_funcs(prop, wm_operator_properties_filesel_sort_items_itemf);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

void WM_operator_properties_id_lookup_set_from_id(PointerRNA *ptr, const ID *id)
{
  PropertyRNA *prop_session_uid = RNA_struct_find_property(ptr, "session_uid");
  PropertyRNA *prop_name = RNA_struct_find_property(ptr, "name");

  if (prop_session_uid) {
    RNA_int_set(ptr, "session_uid", int(id->session_uid));
  }
  else if (prop_name) {
    RNA_string_set(ptr, "name", id->name + 2);
  }
  else {
    BLI_assert_unreachable();
  }
}

ID *WM_operator_properties_id_lookup_from_name_or_session_uid(Main *bmain,
                                                              PointerRNA *ptr,
                                                              const ID_Type type)
{
  PropertyRNA *prop_session_uid = RNA_struct_find_property(ptr, "session_uid");
  if (prop_session_uid && RNA_property_is_set(ptr, prop_session_uid)) {
    const uint32_t session_uid = uint32_t(RNA_property_int_get(ptr, prop_session_uid));
    return BKE_libblock_find_session_uid(bmain, type, session_uid);
  }

  PropertyRNA *prop_name = RNA_struct_find_property(ptr, "name");
  if (prop_name && RNA_property_is_set(ptr, prop_name)) {
    char name[MAX_ID_NAME - 2];
    RNA_property_string_get(ptr, prop_name, name);
    return BKE_libblock_find_name(bmain, type, name);
  }

  return nullptr;
}

bool WM_operator_properties_id_lookup_is_set(PointerRNA *ptr)
{
  return RNA_struct_property_is_set(ptr, "session_uid") || RNA_struct_property_is_set(ptr, "name");
}

void WM_operator_properties_id_lookup(wmOperatorType *ot, const bool add_name_prop)
{
  PropertyRNA *prop;

  if (add_name_prop) {
    prop = RNA_def_string(ot->srna,
                          "name",
                          nullptr,
                          MAX_ID_NAME - 2,
                          "Name",
                          "Name of the data-block to use by the operator");
    RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  }

  prop = RNA_def_int(ot->srna,
                     "session_uid",
                     0,
                     INT32_MIN,
                     INT32_MAX,
                     "Session UID",
                     "Session UID of the data-block to use by the operator",
                     INT32_MIN,
                     INT32_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

static void wm_operator_properties_select_action_ex(wmOperatorType *ot,
                                                    int default_action,
                                                    const EnumPropertyItem *select_actions,
                                                    bool hide_gui)
{
  PropertyRNA *prop;
  prop = RNA_def_enum(
      ot->srna, "action", select_actions, default_action, "Action", "Selection action to execute");

  if (hide_gui) {
    RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  }
}

void WM_operator_properties_select_action(wmOperatorType *ot, int default_action, bool hide_gui)
{
  static const EnumPropertyItem select_actions[] = {
      {SEL_TOGGLE, "TOGGLE", 0, "Toggle", "Toggle selection for all elements"},
      {SEL_SELECT, "SELECT", 0, "Select", "Select all elements"},
      {SEL_DESELECT, "DESELECT", 0, "Deselect", "Deselect all elements"},
      {SEL_INVERT, "INVERT", 0, "Invert", "Invert selection of all elements"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wm_operator_properties_select_action_ex(ot, default_action, select_actions, hide_gui);
}

void WM_operator_properties_select_action_simple(wmOperatorType *ot,
                                                 int default_action,
                                                 bool hide_gui)
{
  static const EnumPropertyItem select_actions[] = {
      {SEL_SELECT, "SELECT", 0, "Select", "Select all elements"},
      {SEL_DESELECT, "DESELECT", 0, "Deselect", "Deselect all elements"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wm_operator_properties_select_action_ex(ot, default_action, select_actions, hide_gui);
}

void WM_operator_properties_select_random(wmOperatorType *ot)
{
  RNA_def_float_factor(ot->srna,
                       "ratio",
                       0.5f,
                       0.0f,
                       1.0f,
                       "Ratio",
                       "Portion of items to select randomly",
                       0.0f,
                       1.0f);
  RNA_def_int(ot->srna,
              "seed",
              0,
              0,
              INT_MAX,
              "Random Seed",
              "Seed for the random number generator",
              0,
              255);

  WM_operator_properties_select_action_simple(ot, SEL_SELECT, false);
}

int WM_operator_properties_select_random_seed_increment_get(wmOperator *op)
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "seed");
  int value = RNA_property_int_get(op->ptr, prop);

  if (op->flag & OP_IS_INVOKE) {
    if (!RNA_property_is_set(op->ptr, prop)) {
      value += 1;
      RNA_property_int_set(op->ptr, prop, value);
    }
  }
  return value;
}

void WM_operator_properties_select_all(wmOperatorType *ot)
{
  WM_operator_properties_select_action(ot, SEL_TOGGLE, true);
}

void WM_operator_properties_border(wmOperatorType *ot)
{
  PropertyRNA *prop;

  prop = RNA_def_int(ot->srna, "xmin", 0, INT_MIN, INT_MAX, "X Min", "", INT_MIN, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_int(ot->srna, "xmax", 0, INT_MIN, INT_MAX, "X Max", "", INT_MIN, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_int(ot->srna, "ymin", 0, INT_MIN, INT_MAX, "Y Min", "", INT_MIN, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_int(ot->srna, "ymax", 0, INT_MIN, INT_MAX, "Y Max", "", INT_MIN, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna, "wait_for_input", true, "Wait for Input", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

void WM_operator_properties_border_to_rcti(wmOperator *op, rcti *r_rect)
{
  r_rect->xmin = RNA_int_get(op->ptr, "xmin");
  r_rect->ymin = RNA_int_get(op->ptr, "ymin");
  r_rect->xmax = RNA_int_get(op->ptr, "xmax");
  r_rect->ymax = RNA_int_get(op->ptr, "ymax");
}

void WM_operator_properties_border_to_rctf(wmOperator *op, rctf *r_rect)
{
  rcti rect_i;
  WM_operator_properties_border_to_rcti(op, &rect_i);
  BLI_rctf_rcti_copy(r_rect, &rect_i);
}

blender::Bounds<blender::int2> WM_operator_properties_border_to_bounds(wmOperator *op)
{
  using namespace blender;
  return Bounds<int2>({RNA_int_get(op->ptr, "xmin"), RNA_int_get(op->ptr, "ymin")},
                      {RNA_int_get(op->ptr, "xmax"), RNA_int_get(op->ptr, "ymax")});
}

void WM_operator_properties_gesture_box_ex(wmOperatorType *ot, bool deselect, bool extend)
{
  PropertyRNA *prop;

  WM_operator_properties_border(ot);

  if (deselect) {
    prop = RNA_def_boolean(
        ot->srna, "deselect", false, "Deselect", "Deselect rather than select items");
    RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  }
  if (extend) {
    prop = RNA_def_boolean(ot->srna,
                           "extend",
                           true,
                           "Extend",
                           "Extend selection instead of deselecting everything first");
    RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  }
}

void WM_operator_properties_use_cursor_init(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_boolean(ot->srna,
                                      "use_cursor_init",
                                      true,
                                      "Use Mouse Position",
                                      "Allow the initial mouse position to be used");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

void WM_operator_properties_gesture_box_select(wmOperatorType *ot)
{
  WM_operator_properties_gesture_box_ex(ot, true, true);
}
void WM_operator_properties_gesture_box(wmOperatorType *ot)
{
  WM_operator_properties_gesture_box_ex(ot, false, false);
}

void WM_operator_properties_select_operation(wmOperatorType *ot)
{
  static const EnumPropertyItem select_mode_items[] = {
      {SEL_OP_SET, "SET", ICON_SELECT_SET, "Set", "Set a new selection"},
      {SEL_OP_ADD, "ADD", ICON_SELECT_EXTEND, "Extend", "Extend existing selection"},
      {SEL_OP_SUB, "SUB", ICON_SELECT_SUBTRACT, "Subtract", "Subtract existing selection"},
      {SEL_OP_XOR, "XOR", ICON_SELECT_DIFFERENCE, "Difference", "Invert existing selection"},
      {SEL_OP_AND, "AND", ICON_SELECT_INTERSECT, "Intersect", "Intersect existing selection"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  PropertyRNA *prop = RNA_def_enum(ot->srna, "mode", select_mode_items, SEL_OP_SET, "Mode", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

void WM_operator_properties_select_operation_simple(wmOperatorType *ot)
{
  static const EnumPropertyItem select_mode_items[] = {
      {SEL_OP_SET, "SET", ICON_SELECT_SET, "Set", "Set a new selection"},
      {SEL_OP_ADD, "ADD", ICON_SELECT_EXTEND, "Extend", "Extend existing selection"},
      {SEL_OP_SUB, "SUB", ICON_SELECT_SUBTRACT, "Subtract", "Subtract existing selection"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  PropertyRNA *prop = RNA_def_enum(ot->srna, "mode", select_mode_items, SEL_OP_SET, "Mode", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

void WM_operator_properties_select_walk_direction(wmOperatorType *ot)
{
  static const EnumPropertyItem direction_items[] = {
      {UI_SELECT_WALK_UP, "UP", 0, "Previous", ""},
      {UI_SELECT_WALK_DOWN, "DOWN", 0, "Next", ""},
      {UI_SELECT_WALK_LEFT, "LEFT", 0, "Left", ""},
      {UI_SELECT_WALK_RIGHT, "RIGHT", 0, "Right", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  PropertyRNA *prop;
  prop = RNA_def_enum(ot->srna,
                      "direction",
                      direction_items,
                      0,
                      "Walk Direction",
                      "Select/Deselect element in this direction");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

void WM_operator_properties_generic_select(wmOperatorType *ot)
{
  /* On the initial mouse press, this is set by #WM_generic_select_modal() to let the select
   * operator exec callback know that it should not __yet__ deselect other items when clicking on
   * an already selected one. Instead should make sure the operator executes modal then (see
   * #WM_generic_select_modal()), so that the exec callback can be called a second time on the
   * mouse release event to do this part. */
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "wait_to_deselect_others", false, "Wait to Deselect Others", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  /* Force the selection to act on mouse click, not press.
   * Necessary for some cases, but isn't used much. */
  prop = RNA_def_boolean(ot->srna,
                         "use_select_on_click",
                         false,
                         "Act on Click",
                         "Instead of selecting on mouse press, wait to see if there's drag event. "
                         "Otherwise select on mouse release");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  RNA_def_int(ot->srna, "mouse_x", 0, INT_MIN, INT_MAX, "Mouse X", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "mouse_y", 0, INT_MIN, INT_MAX, "Mouse Y", "", INT_MIN, INT_MAX);
}

void WM_operator_properties_gesture_box_zoom(wmOperatorType *ot)
{
  WM_operator_properties_border(ot);

  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna, "zoom_out", false, "Zoom Out", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

void WM_operator_properties_gesture_lasso(wmOperatorType *ot)
{
  PropertyRNA *prop;
  prop = RNA_def_collection_runtime(ot->srna, "path", &RNA_OperatorMousePath, "Path", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "use_smooth_stroke",
                         false,
                         "Stabilize Stroke",
                         "Selection lags behind mouse and follows a smoother path");
  prop = RNA_def_float(ot->srna,
                       "smooth_stroke_factor",
                       0.75f,
                       0.5f,
                       0.99f,
                       "Smooth Stroke Factor",
                       "Higher values gives a smoother stroke",
                       0.5f,
                       0.99f);
  prop = RNA_def_int(ot->srna,
                     "smooth_stroke_radius",
                     35,
                     10,
                     200,
                     "Smooth Stroke Radius",
                     "Minimum distance from last point before selection continues",
                     10,
                     200);
  RNA_def_property_subtype(prop, PROP_PIXEL);
}

void WM_operator_properties_gesture_polyline(wmOperatorType *ot)
{
  PropertyRNA *prop;
  prop = RNA_def_collection_runtime(ot->srna, "path", &RNA_OperatorMousePath, "Path", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

void WM_operator_properties_gesture_straightline(wmOperatorType *ot, int cursor)
{
  PropertyRNA *prop;

  prop = RNA_def_int(ot->srna, "xstart", 0, INT_MIN, INT_MAX, "X Start", "", INT_MIN, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_int(ot->srna, "xend", 0, INT_MIN, INT_MAX, "X End", "", INT_MIN, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_int(ot->srna, "ystart", 0, INT_MIN, INT_MAX, "Y Start", "", INT_MIN, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_int(ot->srna, "yend", 0, INT_MIN, INT_MAX, "Y End", "", INT_MIN, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "flip", false, "Flip", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  if (cursor) {
    prop = RNA_def_int(ot->srna,
                       "cursor",
                       cursor,
                       0,
                       INT_MAX,
                       "Cursor",
                       "Mouse cursor style to use during the modal operator",
                       0,
                       INT_MAX);
    RNA_def_property_flag(prop, PROP_HIDDEN);
  }
}

void WM_operator_properties_gesture_circle(wmOperatorType *ot)
{
  PropertyRNA *prop;
  const int radius_default = 25;

  prop = RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  RNA_def_int(ot->srna, "radius", radius_default, 1, INT_MAX, "Radius", "", 1, INT_MAX);

  prop = RNA_def_boolean(ot->srna, "wait_for_input", true, "Wait for Input", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

void WM_operator_properties_mouse_select(wmOperatorType *ot)
{
  PropertyRNA *prop;

  prop = RNA_def_boolean(ot->srna,
                         "extend",
                         false,
                         "Extend",
                         "Extend selection instead of deselecting everything first");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "deselect", false, "Deselect", "Remove from selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "toggle", false, "Toggle Selection", "Toggle the selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "deselect_all",
                         false,
                         "Deselect On Nothing",
                         "Deselect all when nothing under the cursor");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* TODO: currently only used for the 3D viewport. */
  prop = RNA_def_boolean(ot->srna,
                         "select_passthrough",
                         false,
                         "Only Select Unselected",
                         "Ignore the select action when the element is already selected");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

void WM_operator_properties_checker_interval(wmOperatorType *ot, bool nth_can_disable)
{
  const int nth_default = nth_can_disable ? 0 : 1;
  const int nth_min = min_ii(nth_default, 1);
  RNA_def_int(ot->srna,
              "skip",
              nth_default,
              nth_min,
              INT_MAX,
              "Deselected",
              "Number of deselected elements in the repetitive sequence",
              nth_min,
              100);
  RNA_def_int(ot->srna,
              "nth",
              1,
              1,
              INT_MAX,
              "Selected",
              "Number of selected elements in the repetitive sequence",
              1,
              100);
  RNA_def_int(ot->srna,
              "offset",
              0,
              INT_MIN,
              INT_MAX,
              "Offset",
              "Offset from the starting point",
              -100,
              100);
}

void WM_operator_properties_checker_interval_from_op(wmOperator *op,
                                                     CheckerIntervalParams *op_params)
{
  const int nth = RNA_int_get(op->ptr, "nth");
  const int skip = RNA_int_get(op->ptr, "skip");
  int offset = RNA_int_get(op->ptr, "offset");

  op_params->nth = nth;
  op_params->skip = skip;

  /* So input of offset zero ends up being (nth - 1). */
  op_params->offset = mod_i(offset, nth + skip);
}

bool WM_operator_properties_checker_interval_test(const CheckerIntervalParams *op_params,
                                                  int depth)
{
  return ((op_params->skip == 0) ||
          ((op_params->offset + depth) % (op_params->skip + op_params->nth) >= op_params->skip));
}
