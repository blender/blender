/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "UI_interface_icons.hh" /* `eAlertIcon` */
#include "UI_interface_types.hh"

#include "WM_types.hh" /* `wmOperatorCallContext` */

struct bContext;
struct bContextStore;
struct uiBlock;
struct uiBut;
struct uiLayoutRoot;
struct uiStyle;
struct MenuType;
struct PanelType;
struct PointerRNA;

/* Layout
 *
 * More automated layout of buttons. Has three levels:
 * - Layout: contains a number templates, within a bounded width or height.
 * - Template: predefined layouts for buttons with a number of slots, each
 *   slot can contain multiple items.
 * - Item: item to put in a template slot, being either an RNA property,
 *   operator, label or menu. Also regular buttons can be used when setting
 *   uiBlockCurLayout. */

namespace blender::ui {
enum class ItemType : int8_t;
enum class ItemInternalFlag : uint8_t;
enum class EmbossType : uint8_t;
}  // namespace blender::ui

struct PanelLayout {
  uiLayout *header;
  uiLayout *body;
};

/**
 * NOTE: `uiItem` properties should be considered private outside `interface_layout.cc`,
 * incoming refactors would remove public access and add public read/write function methods.
 * Meanwhile keep using `uiLayout*` functions to read/write this properties.
 */
struct uiItem {
  blender::ui::ItemType type_;
  blender::ui::ItemInternalFlag flag_;

  uiItem() = default;
  uiItem(const uiItem &) = default;
  virtual ~uiItem() = default;
};

enum eUI_Item_Flag : uint16_t;

enum class LayoutSeparatorType : int8_t {
  Auto,
  Space,
  Line,
};

/**
 * NOTE: `uiLayout` properties should be considered private outside `interface_layout.cc`,
 * incoming refactors would remove public access and add public read/write function methods.
 * Meanwhile keep using `uiLayout*` functions to read/write this properties.
 */
struct uiLayout : uiItem {
  // protected:
  uiLayoutRoot *root_;
  bContextStore *context_;
  uiLayout *parent_;
  blender::Vector<uiItem *> items_;

  char heading_[UI_MAX_NAME_STR];

  /** Sub layout to add child items, if not the layout itself. */
  uiLayout *child_items_layout_;

  int x_, y_, w_, h_;
  float scale_[2];
  short space_;
  bool align_;
  bool active_;
  bool active_default_;
  bool activate_init_;
  bool enabled_;
  bool redalert_;
  bool keepaspect_;
  /** For layouts inside grid-flow, they and their items shall never have a fixed maximal size. */
  bool variable_size_;
  char alignment_;
  blender::ui::EmbossType emboss_;
  /** for fixed width or height to avoid UI size changes */
  float units_[2];
  /** Is copied to uiButs created in this layout. */
  float search_weight_;

 public:
  bool active() const;
  /**
   * Sets the active state of the layout and its items.
   * When false the layout and its buttons are grayed out, user can still interact with them but
   * generally they will not have an active use.
   */
  void active_set(bool active);

  blender::ui::EmbossType emboss() const;
  void emboss_set(blender::ui::EmbossType emboss);

  wmOperatorCallContext operator_context() const;
  /** Sets the default call context for new operator buttons added in any #root_ sub-layout. */
  void operator_context_set(wmOperatorCallContext opcontext);

  float scale_x() const;
  void scale_x_set(float scale);

  float scale_y() const;
  void scale_y_set(float scale);

  float ui_units_x() const;
  /** Sets a fixed width size for this layout. */
  void ui_units_x_set(float width);

  float ui_units_y() const;
  /** Sets a fixed height size for this layout. */
  void ui_units_y_set(float height);

  /** Sub-layout items. */

  uiLayout &absolute(bool align);
  uiBlock *absolute_block();

  /**
   * Add a new box sub-layout, items placed in this sub-layout are added vertically one under
   * each other in a column and are surrounded by a box.
   */
  uiLayout &box();
  /**
   * Add a new column sub-layout, items placed in this sub-layout are added vertically one under
   * each other in a column.
   */
  uiLayout &column(bool align);
  /**
   * Add a new column sub-layout, items placed in this sub-layout are added vertically one under
   * each other in a column.
   * \param heading: Heading label to set to the first child element added in the sub-layout
   * through #uiLayout::prop. When property split is used, this heading label is set in the split
   * label column when there is no label defined.
   */
  uiLayout &column(bool align, blender::StringRef heading);

  /**
   * Add a new row sub-layout, items placed in this sub-layout are added horizontally next to each
   * other in row.
   */
  uiLayout &row(bool align);
  /**
   * Add a new row sub-layout, items placed in this sub-layout are added horizontally next to each
   * other in row.
   * \param heading: Heading label to set to the first child element added in the sub-layout
   * through #uiLayout::prop. When property split is used, this heading label is set in the split
   * label column when there is no label defined.
   */
  uiLayout &row(bool align, blender::StringRef heading);

  /**
   * Add a new column flow sub-layout, items placed in this sub-layout would be evenly distributed
   * in columns.
   * \param number: the number of columns in which items are distributed.
   */
  uiLayout &column_flow(int number, bool align);
  /**
   * Add a new grid flow sub-layout, items placed in this sub-layout would be distributed in a
   * grid.
   * \param row_major: When true items are distributed by rows, otherwise items are distributed by
   * columns.
   * \param columns_len: When positive is the fixed number of columns to show, when 0 its automatic
   * defined, when negative its an automatic stepped number of columns/rows to show (e.g. when \a
   * row_major is true -3 will automatically show (1,2,3,6,9,...) columns, or when \a row_major is
   * false -3 will automatically show (3,6,9,...) rows).
   * \param even_columns: All columns will have the same width.
   * \param even_rows: All rows will have the same height.
   */
  uiLayout &grid_flow(
      bool row_major, int columns_len, bool even_columns, bool even_rows, bool align);

  /** Add a new list box sub-layout. */
  uiLayout &list_box(uiList *ui_list, PointerRNA *actptr, PropertyRNA *actprop);

  /**
   * Add a pie menu layout, buttons are arranged around a center.
   * Only one pie menu per layout root can be added, if it's already initialized it will be
   * returned instead of adding a new one.
   */
  uiLayout &menu_pie();

  /** Add a new overlap sub-layout. */
  uiLayout &overlap();

  /**
   * Create a "layout panel" which is a panel that is defined as part of the `uiLayout`. This
   * allows creating expandable sections which can also be nested.
   *
   * The open-state of the panel is defined by an RNA property which is passed in as a pointer +
   * property name pair. This gives the caller flexibility to decide who should own the open-state.
   *
   * \param C: The context is necessary because sometimes the panel may be forced to be open by the
   * context even of the open-property is `false`. This can happen with e.g. property search.
   * \param layout: The `uiLayout` that should contain the sub-panel.
   * Only layouts that span the full width of the region are supported for now.
   * \param open_prop_owner: Data that contains the open-property.
   * \param open_prop_name: Name of the open-property in `open_prop_owner`.
   *
   * \return A #PanelLayout containing layouts for both the header row and the panel body. If the
   * panel is closed and should not be drawn, the body layout will be NULL.
   */
  PanelLayout panel_prop(const bContext *C,
                         PointerRNA *open_prop_owner,
                         blender::StringRefNull open_prop_name);
  /**
   * Variant of #panel_prop that automatically creates the header row with the
   * given label and only returns the body layout.
   *
   * \param label: Text that's shown in the panel header. It should already be translated.
   *
   * \return NULL if the panel is closed and should not be drawn, otherwise the layout where the
   * sub-panel should be inserted into.
   */
  uiLayout *panel_prop(const bContext *C,
                       PointerRNA *open_prop_owner,
                       blender::StringRefNull open_prop_name,
                       blender::StringRef label);
  PanelLayout panel_prop_with_bool_header(const bContext *C,
                                          PointerRNA *open_prop_owner,
                                          blender::StringRefNull open_prop_name,
                                          PointerRNA *bool_prop_owner,
                                          blender::StringRefNull bool_prop_name,
                                          std::optional<blender::StringRefNull> label);
  /**
   * Variant of #panel_prop that automatically stores the open-close-state in the root
   * panel. When a dynamic number of panels is required, it's recommended to use #panel_prop
   * instead of passing in generated id names.
   *
   * \param idname: String that identifies the open-close-state in the root panel.
   */
  PanelLayout panel(const bContext *C, blender::StringRef idname, bool default_closed);

  /**
   * Variant of #panel that automatically creates the header row with the given label and
   * only returns the body layout.
   *
   * \param label:  Text that's shown in the panel header. It should already be translated.
   *
   * \return NULL if the panel is closed and should not be drawn, otherwise the layout where the
   * sub-panel should be inserted into.
   */
  uiLayout *panel(const bContext *C,
                  blender::StringRef idname,
                  bool default_closed,
                  blender::StringRef label);

  /**
   * Add a new split sub-layout, items placed in this sub-layout are added horizontally next to
   * each other in row, but width is splitted between the first item and remaining items.
   * \param percentage: Width percent to split.
   */
  uiLayout &split(float percentage, bool align);

  /** Items. */

  /** Adds a label item that will display text and/or icon in the layout. */
  void label(blender::StringRef name, int icon);

  /**
   * Adds a menu item, which is a button that when active will display a menu.
   * If menu fails to poll with `WM_menutype_poll` it will not be added into the layout.
   */
  void menu(MenuType *mt, std::optional<blender::StringRef> name, int icon);
  /**
   * Adds a menu item, which is a button that when active will display a menu.
   * If menu fails to poll with `WM_menutype_poll` it will not be added into the layout.
   */
  void menu(blender::StringRef menuname, std::optional<blender::StringRef> name, int icon);

  /**
   * Adds a menu item, which is a button that when active will display a menu.
   * \param name: Label to show in the menu button.
   * \param func: Function that generates the menu layout.
   * \param arg: Pointer to data used as last argument in \a func.
   */
  void menu_fn(blender::StringRefNull name, int icon, uiMenuCreateFunc func, void *arg);
  /**
   * Adds a menu item, which is a button that when active will display a menu.
   * \param name: Label to show in the menu button.
   * \param func: Function that generates the menu layout.
   * \param argN: Pointer to data used as last argument in \a func, it will be
   * freed with the menu button.
   */
  void menu_fn_argN_free(blender::StringRefNull name, int icon, uiMenuCreateFunc func, void *argN);
  /**
   * Adds a operator item, places a button in the layout to call the operator.
   * \param ot: Operator to add.
   * \param name: Text to show in the layout.
   * \param context: Operator call context for #WM_operator_name_call.
   * \returns Operator pointer to write properties.
   */
  PointerRNA op(wmOperatorType *ot,
                std::optional<blender::StringRef> name,
                int icon,
                wmOperatorCallContext context,
                eUI_Item_Flag flag);

  /**
   * Adds a operator item, places a button in the layout to call the operator.
   * \param ot: Operator to add.
   * \param name: Text to show in the layout.
   * \returns Operator pointer to write properties.
   */
  PointerRNA op(wmOperatorType *ot, std::optional<blender::StringRef> name, int icon);

  /**
   * Adds a operator item, places a button in the layout to call the operator.
   * \param opname: Operator id name.
   * \param name: Text to show in the layout.
   * \returns Operator pointer to write properties, might be #PointerRNA_NULL if operator does not
   * exists.
   */
  PointerRNA op(blender::StringRefNull opname, std::optional<blender::StringRef> name, int icon);

  /**
   * Adds a operator item, places a button in the layout to call the operator.
   * \param opname: Operator id name.
   * \param name: Text to show in the layout.
   * \param context: Operator call context for #WM_operator_name_call.
   * \returns Operator pointer to write properties, might be #PointerRNA_NULL if operator does not
   * exists.
   */
  PointerRNA op(blender::StringRefNull opname,
                std::optional<blender::StringRef> name,
                int icon,
                wmOperatorCallContext context,
                eUI_Item_Flag flag);
  /**
   * Adds a RNA property item, and exposes it into the layout.
   * \param ptr: RNA pointer to the struct owner of \a prop.
   * \param prop: The property in \a ptr to add.
   * \param index: When \a prop is a array property, indicates what entry to expose through the
   * layout, #RNA_NO_INDEX (-1) means all.
   */
  void prop(PointerRNA *ptr,
            PropertyRNA *prop,
            int index,
            int value,
            eUI_Item_Flag flag,
            std::optional<blender::StringRef> name_opt,
            int icon,
            std::optional<blender::StringRef> placeholder = std::nullopt);
  /** Adds a RNA property item, and exposes it into the layout. */
  void prop(PointerRNA *ptr,
            blender::StringRefNull propname,
            eUI_Item_Flag flag,
            std::optional<blender::StringRef> name,
            int icon);

  /** Adds a separator item, that adds empty space between items. */
  void separator(float factor = 1.0f, LayoutSeparatorType type = LayoutSeparatorType::Auto);
};

inline bool uiLayout::active() const
{
  return active_;
}
inline void uiLayout::active_set(bool active)
{
  active_ = active;
}

inline float uiLayout::scale_x() const
{
  return scale_[0];
};
inline void uiLayout::scale_x_set(float scale)
{
  scale_[0] = scale;
};

inline float uiLayout::scale_y() const
{
  return scale_[1];
};
inline void uiLayout::scale_y_set(float scale)
{
  scale_[1] = scale;
};

inline float uiLayout::ui_units_x() const
{
  return units_[0];
};
inline void uiLayout::ui_units_x_set(float width)
{
  units_[0] = width;
};

inline float uiLayout::ui_units_y() const
{
  return units_[1];
};
inline void uiLayout::ui_units_y_set(float height)
{
  units_[1] = height;
};

enum {
  UI_LAYOUT_HORIZONTAL = 0,
  UI_LAYOUT_VERTICAL = 1,
};

enum {
  UI_LAYOUT_PANEL = 0,
  UI_LAYOUT_HEADER = 1,
  UI_LAYOUT_MENU = 2,
  UI_LAYOUT_TOOLBAR = 3,
  UI_LAYOUT_PIEMENU = 4,
  UI_LAYOUT_VERT_BAR = 5,
};

enum {
  UI_LAYOUT_ALIGN_EXPAND = 0,
  UI_LAYOUT_ALIGN_LEFT = 1,
  UI_LAYOUT_ALIGN_CENTER = 2,
  UI_LAYOUT_ALIGN_RIGHT = 3,
};

enum eUI_Item_Flag : uint16_t {
  /* UI_ITEM_O_RETURN_PROPS = 1 << 0, */ /* UNUSED */
  UI_ITEM_R_EXPAND = 1 << 1,
  UI_ITEM_R_SLIDER = 1 << 2,
  /**
   * Use for booleans, causes the button to draw with an outline (emboss),
   * instead of text with a checkbox.
   * This is implied when toggle buttons have an icon
   * unless #UI_ITEM_R_ICON_NEVER flag is set.
   */
  UI_ITEM_R_TOGGLE = 1 << 3,
  /**
   * Don't attempt to use an icon when the icon is set to #ICON_NONE.
   *
   * Use for booleans, causes the buttons to always show as a checkbox
   * even when there is an icon (which would normally show the button as a toggle).
   */
  UI_ITEM_R_ICON_NEVER = 1 << 4,
  UI_ITEM_R_ICON_ONLY = 1 << 5,
  UI_ITEM_R_EVENT = 1 << 6,
  UI_ITEM_R_FULL_EVENT = 1 << 7,
  UI_ITEM_R_NO_BG = 1 << 8,
  UI_ITEM_R_IMMEDIATE = 1 << 9,
  UI_ITEM_O_DEPRESS = 1 << 10,
  UI_ITEM_R_COMPACT = 1 << 11,
  UI_ITEM_R_CHECKBOX_INVERT = 1 << 12,
  /** Don't add a real decorator item, just blank space. */
  UI_ITEM_R_FORCE_BLANK_DECORATE = 1 << 13,
  /* Even create the property split layout if there's no name to show there. */
  UI_ITEM_R_SPLIT_EMPTY_NAME = 1 << 14,
  /**
   * Only for text buttons (for now): Force the button as active in a semi-modal state (capturing
   * text input while leaving the remaining UI interactive).
   */
  UI_ITEM_R_TEXT_BUT_FORCE_SEMI_MODAL_ACTIVE = 1 << 15,
};
ENUM_OPERATORS(eUI_Item_Flag, UI_ITEM_R_TEXT_BUT_FORCE_SEMI_MODAL_ACTIVE)
#define UI_ITEM_NONE eUI_Item_Flag(0)

uiLayout *UI_block_layout(uiBlock *block,
                          int dir,
                          int type,
                          int x,
                          int y,
                          int size,
                          int em,
                          int padding,
                          const uiStyle *style);
void UI_block_layout_set_current(uiBlock *block, uiLayout *layout);
void UI_block_layout_resolve(uiBlock *block, int *r_x, int *r_y);
bool UI_block_layout_needs_resolving(const uiBlock *block);
/**
 * Used for property search when the layout process needs to be cancelled in order to avoid
 * computing the locations for buttons, but the layout items created while adding the buttons
 * must still be freed.
 */
void UI_block_layout_free(uiBlock *block);

/**
 * Apply property search behavior, setting panel flags and deactivating buttons that don't match.
 *
 * \note Must not be run after #UI_block_layout_resolve.
 */
bool UI_block_apply_search_filter(uiBlock *block, const char *search_filter);

uiBlock *uiLayoutGetBlock(uiLayout *layout);

void uiLayoutSetFunc(uiLayout *layout, uiMenuHandleFunc handlefunc, void *argv);
void uiLayoutSetContextPointer(uiLayout *layout, blender::StringRef name, PointerRNA *ptr);
void uiLayoutSetContextString(uiLayout *layout, blender::StringRef name, blender::StringRef value);
void uiLayoutSetContextInt(uiLayout *layout, blender::StringRef name, int64_t value);
bContextStore *uiLayoutGetContextStore(uiLayout *layout);
void uiLayoutContextCopy(uiLayout *layout, const bContextStore *context);

/**
 * Set tooltip function for all buttons in the layout.
 * func, arg and free_arg are passed on to UI_but_func_tooltip_set, so their meaning is the same.
 *
 * \param func: The callback function that gets called to get tooltip content
 * \param arg: An optional opaque pointer that gets passed to func
 * \param free_arg: An optional callback for freeing arg (can be set to e.g. MEM_freeN)
 * \param copy_arg: An optional callback for duplicating arg in case UI_but_func_tooltip_set
 * is being called on multiple buttons (can be set to e.g. MEM_dupallocN). If set to NULL, arg will
 * be passed as-is to all buttons.
 */
void uiLayoutSetTooltipFunc(uiLayout *layout,
                            uiButToolTipFunc func,
                            void *arg,
                            uiCopyArgFunc copy_arg,
                            uiFreeArgFunc free_arg);

void UI_menutype_draw(bContext *C, MenuType *mt, uiLayout *layout);

/**
 * Used for popup panels only.
 */
void UI_paneltype_draw(bContext *C, PanelType *pt, uiLayout *layout);

/* Only for convenience. */
void uiLayoutSetContextFromBut(uiLayout *layout, uiBut *but);

void uiLayoutSetActiveDefault(uiLayout *layout, bool active_default);
void uiLayoutSetActivateInit(uiLayout *layout, bool activate_init);
void uiLayoutSetEnabled(uiLayout *layout, bool enabled);
void uiLayoutSetRedAlert(uiLayout *layout, bool redalert);
void uiLayoutSetAlignment(uiLayout *layout, char alignment);
void uiLayoutSetFixedSize(uiLayout *layout, bool fixed_size);
void uiLayoutSetKeepAspect(uiLayout *layout, bool keepaspect);
void uiLayoutSetPropSep(uiLayout *layout, bool is_sep);
void uiLayoutSetPropDecorate(uiLayout *layout, bool is_sep);
int uiLayoutGetLocalDir(const uiLayout *layout);
void uiLayoutSetSearchWeight(uiLayout *layout, float weight);

bool uiLayoutGetActiveDefault(uiLayout *layout);
bool uiLayoutGetActivateInit(uiLayout *layout);
bool uiLayoutGetEnabled(uiLayout *layout);
bool uiLayoutGetRedAlert(uiLayout *layout);
int uiLayoutGetAlignment(uiLayout *layout);
bool uiLayoutGetFixedSize(uiLayout *layout);
bool uiLayoutGetKeepAspect(uiLayout *layout);
int uiLayoutGetWidth(uiLayout *layout);
bool uiLayoutGetPropSep(uiLayout *layout);
bool uiLayoutGetPropDecorate(uiLayout *layout);
Panel *uiLayoutGetRootPanel(uiLayout *layout);
float uiLayoutGetSearchWeight(uiLayout *layout);

int uiLayoutListItemPaddingWidth();
void uiLayoutListItemAddPadding(uiLayout *layout);

/* Layout create functions. */

bool uiLayoutEndsWithPanelHeader(const uiLayout &layout);

/* items */

void uiItemEnumO_ptr(uiLayout *layout,
                     wmOperatorType *ot,
                     std::optional<blender::StringRef> name,
                     int icon,
                     blender::StringRefNull propname,
                     int value);
void uiItemEnumO(uiLayout *layout,
                 blender::StringRefNull opname,
                 std::optional<blender::StringRef> name,
                 int icon,
                 blender::StringRefNull propname,
                 int value);
/**
 * For use in cases where we have.
 */
void uiItemEnumO_value(uiLayout *layout,
                       blender::StringRefNull name,
                       int icon,
                       blender::StringRefNull opname,
                       blender::StringRefNull propname,
                       int value);
void uiItemEnumO_string(uiLayout *layout,
                        blender::StringRef name,
                        int icon,
                        blender::StringRefNull opname,
                        blender::StringRefNull propname,
                        const char *value_str);
void uiItemsEnumO(uiLayout *layout,
                  blender::StringRefNull opname,
                  blender::StringRefNull propname);

void uiItemFullOMenuHold_ptr(uiLayout *layout,
                             wmOperatorType *ot,
                             std::optional<blender::StringRef> name,
                             int icon,
                             wmOperatorCallContext context,
                             eUI_Item_Flag flag,
                             const char *menu_id, /* extra menu arg. */
                             PointerRNA *r_opptr);

/**
 * Use a wrapper function since re-implementing all the logic in this function would be messy.
 */
void uiItemFullR_with_popover(uiLayout *layout,
                              PointerRNA *ptr,
                              PropertyRNA *prop,
                              int index,
                              int value,
                              eUI_Item_Flag flag,
                              std::optional<blender::StringRefNull> name,
                              int icon,
                              const char *panel_type);
void uiItemFullR_with_menu(uiLayout *layout,
                           PointerRNA *ptr,
                           PropertyRNA *prop,
                           int index,
                           int value,
                           eUI_Item_Flag flag,
                           std::optional<blender::StringRefNull> name,
                           int icon,
                           const char *menu_type);
void uiItemEnumR_prop(uiLayout *layout,
                      std::optional<blender::StringRefNull> name,
                      int icon,
                      PointerRNA *ptr,
                      PropertyRNA *prop,
                      int value);
void uiItemEnumR_string_prop(uiLayout *layout,
                             PointerRNA *ptr,
                             PropertyRNA *prop,
                             const char *value,
                             std::optional<blender::StringRefNull> name,
                             int icon);
void uiItemEnumR_string(uiLayout *layout,
                        PointerRNA *ptr,
                        blender::StringRefNull propname,
                        const char *value,
                        std::optional<blender::StringRefNull> name,
                        int icon);
void uiItemsEnumR(uiLayout *layout, PointerRNA *ptr, blender::StringRefNull propname);
void uiItemPointerR_prop(uiLayout *layout,
                         PointerRNA *ptr,
                         PropertyRNA *prop,
                         PointerRNA *searchptr,
                         PropertyRNA *searchprop,
                         std::optional<blender::StringRefNull> name,
                         int icon,
                         bool results_are_suggestions);
void uiItemPointerR(uiLayout *layout,
                    PointerRNA *ptr,
                    blender::StringRefNull propname,
                    PointerRNA *searchptr,
                    blender::StringRefNull searchpropname,
                    std::optional<blender::StringRefNull> name,
                    int icon);

/**
 * Create a list of enum items.
 *
 * \param active: an optional item to highlight.
 */
void uiItemsFullEnumO(uiLayout *layout,
                      blender::StringRefNull opname,
                      blender::StringRefNull propname,
                      IDProperty *properties,
                      wmOperatorCallContext context,
                      eUI_Item_Flag flag,
                      const int active = -1);
/**
 * Create UI items for enum items in \a item_array.
 *
 * A version of #uiItemsFullEnumO that takes pre-calculated item array.
 * \param active: if not -1, will highlight that item.
 */
void uiItemsFullEnumO_items(uiLayout *layout,
                            wmOperatorType *ot,
                            const PointerRNA &ptr,
                            PropertyRNA *prop,
                            IDProperty *properties,
                            wmOperatorCallContext context,
                            eUI_Item_Flag flag,
                            const EnumPropertyItem *item_array,
                            int totitem,
                            int active = -1);

struct uiPropertySplitWrapper {
  uiLayout *label_column;
  uiLayout *property_row;
  /**
   * Column for decorators. Note that this may be null, see #uiItemPropertySplitWrapperCreate().
   */
  uiLayout *decorate_column;
};

/**
 * Normally, we handle the split layout in #uiLayout::prop(), but there are other cases where the
 * logic is needed. Ideally, #uiLayout::prop() could just call this, but it currently has too many
 * special needs.
 *
 * The returned #uiPropertySplitWrapper.decorator_column may be null when decorators are disabled
 * (#uiLayoutGetPropDecorate() returns false).
 */
uiPropertySplitWrapper uiItemPropertySplitWrapperCreate(uiLayout *parent_layout);

uiBut *uiItemL_ex(
    uiLayout *layout, blender::StringRef name, int icon, bool highlight, bool redalert);
/**
 * Helper to add a label using a property split layout if needed. After calling this the
 * active layout will be the one to place the labeled items in. An additional layout may be
 * returned to place decorator buttons in.
 *
 * \return the layout to place decorators in, if #UI_ITEM_PROP_SEP is enabled. Otherwise null.
 */
uiLayout *uiItemL_respect_property_split(uiLayout *layout, blender::StringRef text, int icon);
/**
 * Label icon for dragging.
 */
void uiItemLDrag(uiLayout *layout, PointerRNA *ptr, blender::StringRef name, int icon);
/**
 * Menu contents.
 */
void uiItemMContents(uiLayout *layout, blender::StringRef menuname);

/* Decorators. */

/**
 * Insert a decorator item for a button with the same property as \a prop.
 * To force inserting a blank dummy element, NULL can be passed for \a ptr and \a prop.
 */
void uiItemDecoratorR_prop(uiLayout *layout, PointerRNA *ptr, PropertyRNA *prop, int index);
/**
 * Insert a decorator item for a button with the same property as \a prop.
 * To force inserting a blank dummy element, NULL can be passed for \a ptr and \a propname.
 */
void uiItemDecoratorR(uiLayout *layout,
                      PointerRNA *ptr,
                      std::optional<blender::StringRefNull> propname,
                      int index);

/** Flexible spacing. */
void uiItemSpacer(uiLayout *layout);

enum eButProgressType {
  UI_BUT_PROGRESS_TYPE_BAR = 0,
  UI_BUT_PROGRESS_TYPE_RING = 1,
};

void uiItemProgressIndicator(uiLayout *layout,
                             const char *text,
                             float factor,
                             eButProgressType progress_type);

/* popover */
void uiItemPopoverPanel_ptr(uiLayout *layout,
                            const bContext *C,
                            PanelType *pt,
                            std::optional<blender::StringRef> name_opt,
                            int icon);
void uiItemPopoverPanel(uiLayout *layout,
                        const bContext *C,
                        blender::StringRef panel_type,
                        std::optional<blender::StringRef> name_opt,
                        int icon);
void uiItemPopoverPanelFromGroup(uiLayout *layout,
                                 bContext *C,
                                 int space_id,
                                 int region_id,
                                 const char *context,
                                 const char *category);

/**
 * Level items.
 */
void uiItemMenuEnumFullO_ptr(uiLayout *layout,
                             const bContext *C,
                             wmOperatorType *ot,
                             blender::StringRefNull propname,
                             std::optional<blender::StringRefNull> name,
                             int icon,
                             PointerRNA *r_opptr);
void uiItemMenuEnumFullO(uiLayout *layout,
                         const bContext *C,
                         blender::StringRefNull opname,
                         blender::StringRefNull propname,
                         blender::StringRefNull name,
                         int icon,
                         PointerRNA *r_opptr);
void uiItemMenuEnumO(uiLayout *layout,
                     const bContext *C,
                     blender::StringRefNull opname,
                     blender::StringRefNull propname,
                     blender::StringRefNull name,
                     int icon);
void uiItemMenuEnumR_prop(uiLayout *layout,
                          PointerRNA *ptr,
                          PropertyRNA *prop,
                          std::optional<blender::StringRefNull>,
                          int icon);
void uiItemTabsEnumR_prop(uiLayout *layout,
                          bContext *C,
                          PointerRNA *ptr,
                          PropertyRNA *prop,
                          PointerRNA *ptr_highlight,
                          PropertyRNA *prop_highlight,
                          bool icon_only);

/* Only for testing, inspecting layouts. */
/**
 * Evaluate layout items as a Python dictionary.
 */
const char *UI_layout_introspect(uiLayout *layout);

/**
 * Helpers to add a big icon and create a split layout for alert popups.
 * Returns the layout to place further items into the alert box.
 */
uiLayout *uiItemsAlertBox(uiBlock *block,
                          const uiStyle *style,
                          const int dialog_width,
                          const eAlertIcon icon,
                          const int icon_size);
uiLayout *uiItemsAlertBox(uiBlock *block, const int size, const eAlertIcon icon);
