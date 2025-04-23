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

enum class LayoutSuppressFlag : uint8_t;

/**
 * NOTE: `uiItem` properties should be considered private outside `interface_layout.cc`,
 * incoming refactors would remove public access and add public read/write function methods.
 * Meanwhile keep using `uiLayout*` functions to read/write this properties.
 */
struct uiItem {
  blender::ui::ItemType type;
  blender::ui::ItemInternalFlag flag;

  uiItem() = default;
  uiItem(const uiItem &) = default;
  virtual ~uiItem() = default;
};
/**
 * NOTE: `uiLayout` properties should be considered private outside `interface_layout.cc`,
 * incoming refactors would remove public access and add public read/write function methods.
 * Meanwhile keep using `uiLayout*` functions to read/write this properties.
 */
struct uiLayout : uiItem {
  // protected:
  uiLayoutRoot *root;
  bContextStore *context;
  uiLayout *parent;
  blender::Vector<uiItem *> items;

  char heading[UI_MAX_NAME_STR];

  /** Sub layout to add child items, if not the layout itself. */
  uiLayout *child_items_layout;

  int x, y, w, h;
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

  LayoutSuppressFlag suppress_flag_;
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

enum eUI_Item_Flag {
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

void uiLayoutSetOperatorContext(uiLayout *layout, wmOperatorCallContext opcontext);
void uiLayoutSetActive(uiLayout *layout, bool active);
void uiLayoutSetActiveDefault(uiLayout *layout, bool active_default);
void uiLayoutSetActivateInit(uiLayout *layout, bool activate_init);
void uiLayoutSetEnabled(uiLayout *layout, bool enabled);
void uiLayoutSetRedAlert(uiLayout *layout, bool redalert);
void uiLayoutSetAlignment(uiLayout *layout, char alignment);
void uiLayoutSetFixedSize(uiLayout *layout, bool fixed_size);
void uiLayoutSetKeepAspect(uiLayout *layout, bool keepaspect);
void uiLayoutSetScaleX(uiLayout *layout, float scale);
void uiLayoutSetScaleY(uiLayout *layout, float scale);
void uiLayoutSetUnitsX(uiLayout *layout, float unit);
void uiLayoutSetUnitsY(uiLayout *layout, float unit);
void uiLayoutSetEmboss(uiLayout *layout, blender::ui::EmbossType emboss);
void uiLayoutSetPropSep(uiLayout *layout, bool is_sep);
void uiLayoutSetPropDecorate(uiLayout *layout, bool is_sep);
int uiLayoutGetLocalDir(const uiLayout *layout);
void uiLayoutSetSearchWeight(uiLayout *layout, float weight);

wmOperatorCallContext uiLayoutGetOperatorContext(uiLayout *layout);
bool uiLayoutGetActive(uiLayout *layout);
bool uiLayoutGetActiveDefault(uiLayout *layout);
bool uiLayoutGetActivateInit(uiLayout *layout);
bool uiLayoutGetEnabled(uiLayout *layout);
bool uiLayoutGetRedAlert(uiLayout *layout);
int uiLayoutGetAlignment(uiLayout *layout);
bool uiLayoutGetFixedSize(uiLayout *layout);
bool uiLayoutGetKeepAspect(uiLayout *layout);
int uiLayoutGetWidth(uiLayout *layout);
float uiLayoutGetScaleX(uiLayout *layout);
float uiLayoutGetScaleY(uiLayout *layout);
float uiLayoutGetUnitsX(uiLayout *layout);
float uiLayoutGetUnitsY(uiLayout *layout);
blender::ui::EmbossType uiLayoutGetEmboss(uiLayout *layout);
bool uiLayoutGetPropSep(uiLayout *layout);
bool uiLayoutGetPropDecorate(uiLayout *layout);
Panel *uiLayoutGetRootPanel(uiLayout *layout);
float uiLayoutGetSearchWeight(uiLayout *layout);

int uiLayoutListItemPaddingWidth();
void uiLayoutListItemAddPadding(uiLayout *layout);

/** Support suppressing checks typically performed to communicate issues to users. */
enum class LayoutSuppressFlag : uint8_t {
  PathSupportsBlendFileRelative = 1 << 0,
};
ENUM_OPERATORS(LayoutSuppressFlag, LayoutSuppressFlag::PathSupportsBlendFileRelative)

LayoutSuppressFlag uiLayoutSuppressFlagGet(const uiLayout *layout);
void uiLayoutSuppressFlagSet(uiLayout *layout, LayoutSuppressFlag flag);
void uiLayoutSuppressFlagClear(uiLayout *layout, LayoutSuppressFlag flag);

/* Layout create functions. */

uiLayout *uiLayoutRow(uiLayout *layout, bool align);

struct PanelLayout {
  uiLayout *header;
  uiLayout *body;
};

/**
 * Create a "layout panel" which is a panel that is defined as part of the `uiLayout`. This allows
 * creating expandable sections which can also be nested.
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
PanelLayout uiLayoutPanelProp(const bContext *C,
                              uiLayout *layout,
                              PointerRNA *open_prop_owner,
                              blender::StringRefNull open_prop_name);
PanelLayout uiLayoutPanelPropWithBoolHeader(const bContext *C,
                                            uiLayout *layout,
                                            PointerRNA *open_prop_owner,
                                            blender::StringRefNull open_prop_name,
                                            PointerRNA *bool_prop_owner,
                                            blender::StringRefNull bool_prop_name,
                                            std::optional<blender::StringRefNull> label);

/**
 * Variant of #uiLayoutPanelProp that automatically creates the header row with the
 * given label and only returns the body layout.
 *
 * \param label: Text that's shown in the panel header. It should already be translated.
 *
 * \return NULL if the panel is closed and should not be drawn, otherwise the layout where the
 * sub-panel should be inserted into.
 */
uiLayout *uiLayoutPanelProp(const bContext *C,
                            uiLayout *layout,
                            PointerRNA *open_prop_owner,
                            blender::StringRefNull open_prop_name,
                            blender::StringRef label);

/**
 * Variant of #uiLayoutPanelProp that automatically stores the open-close-state in the root
 * panel. When a dynamic number of panels is required, it's recommended to use #uiLayoutPanelProp
 * instead of passing in generated id names.
 *
 * \param idname: String that identifies the open-close-state in the root panel.
 */
PanelLayout uiLayoutPanel(const bContext *C,
                          uiLayout *layout,
                          blender::StringRef idname,
                          bool default_closed);

/**
 * Variant of #uiLayoutPanel that automatically creates the header row with the given label and
 * only returns the body layout.
 *
 * \param label:  Text that's shown in the panel header. It should already be translated.
 *
 * \return NULL if the panel is closed and should not be drawn, otherwise the layout where the
 * sub-panel should be inserted into.
 */
uiLayout *uiLayoutPanel(const bContext *C,
                        uiLayout *layout,
                        blender::StringRef idname,
                        bool default_closed,
                        blender::StringRef label);

bool uiLayoutEndsWithPanelHeader(const uiLayout &layout);

/**
 * See #uiLayoutColumnWithHeading().
 */
uiLayout *uiLayoutRowWithHeading(uiLayout *layout, bool align, blender::StringRef heading);
uiLayout *uiLayoutColumn(uiLayout *layout, bool align);
/**
 * Variant of #uiLayoutColumn() that sets a heading label for the layout if the first item is
 * added through #uiItemFullR(). If split layout is used and the item has no string to add to the
 * first split-column, the heading is added there instead. Otherwise the heading inserted with a
 * new row.
 */
uiLayout *uiLayoutColumnWithHeading(uiLayout *layout, bool align, blender::StringRef heading);
uiLayout *uiLayoutColumnFlow(uiLayout *layout, int number, bool align);
uiLayout *uiLayoutGridFlow(uiLayout *layout,
                           bool row_major,
                           int columns_len,
                           bool even_columns,
                           bool even_rows,
                           bool align);
uiLayout *uiLayoutBox(uiLayout *layout);
uiLayout *uiLayoutListBox(uiLayout *layout,
                          uiList *ui_list,
                          PointerRNA *actptr,
                          PropertyRNA *actprop);
uiLayout *uiLayoutAbsolute(uiLayout *layout, bool align);
uiLayout *uiLayoutSplit(uiLayout *layout, float percentage, bool align);
uiLayout *uiLayoutOverlap(uiLayout *layout);
uiBlock *uiLayoutAbsoluteBlock(uiLayout *layout);
/** Pie menu layout: Buttons are arranged around a center. */
uiLayout *uiLayoutRadial(uiLayout *layout);

enum class LayoutSeparatorType : int8_t {
  Auto,
  Space,
  Line,
};

/* items */
void uiItemO(uiLayout *layout,
             std::optional<blender::StringRef> name,
             int icon,
             blender::StringRefNull opname);
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
void uiItemBooleanO(uiLayout *layout,
                    std::optional<blender::StringRef> name,
                    int icon,
                    blender::StringRefNull opname,
                    blender::StringRefNull propname,
                    int value);
void uiItemIntO(uiLayout *layout,
                std::optional<blender::StringRef> name,
                int icon,
                blender::StringRefNull opname,
                blender::StringRefNull propname,
                int value);
void uiItemFloatO(uiLayout *layout,
                  std::optional<blender::StringRef> name,
                  int icon,
                  blender::StringRefNull opname,
                  blender::StringRefNull propname,
                  float value);
void uiItemStringO(uiLayout *layout,
                   std::optional<blender::StringRef> name,
                   int icon,
                   blender::StringRefNull opname,
                   blender::StringRefNull propname,
                   const char *value);

void uiItemFullO_ptr(uiLayout *layout,
                     wmOperatorType *ot,
                     std::optional<blender::StringRef> name,
                     int icon,
                     IDProperty *properties,
                     wmOperatorCallContext context,
                     eUI_Item_Flag flag,
                     PointerRNA *r_opptr);
void uiItemFullO(uiLayout *layout,
                 blender::StringRefNull opname,
                 std::optional<blender::StringRef> name,
                 int icon,
                 IDProperty *properties,
                 wmOperatorCallContext context,
                 eUI_Item_Flag flag,
                 PointerRNA *r_opptr);
void uiItemFullOMenuHold_ptr(uiLayout *layout,
                             wmOperatorType *ot,
                             std::optional<blender::StringRef> name,
                             int icon,
                             IDProperty *properties,
                             wmOperatorCallContext context,
                             eUI_Item_Flag flag,
                             const char *menu_id, /* extra menu arg. */
                             PointerRNA *r_opptr);

void uiItemR(uiLayout *layout,
             PointerRNA *ptr,
             blender::StringRefNull propname,
             eUI_Item_Flag flag,
             std::optional<blender::StringRefNull> name,
             int icon);
void uiItemFullR(uiLayout *layout,
                 PointerRNA *ptr,
                 PropertyRNA *prop,
                 int index,
                 int value,
                 eUI_Item_Flag flag,
                 std::optional<blender::StringRefNull> name_opt,
                 int icon,
                 std::optional<blender::StringRefNull> placeholder = std::nullopt);
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
 * Normally, we handle the split layout in #uiItemFullR(), but there are other cases where the
 * logic is needed. Ideally, #uiItemFullR() could just call this, but it currently has too many
 * special needs.
 *
 * The returned #uiPropertySplitWrapper.decorator_column may be null when decorators are disabled
 * (#uiLayoutGetPropDecorate() returns false).
 */
uiPropertySplitWrapper uiItemPropertySplitWrapperCreate(uiLayout *parent_layout);

void uiItemL(uiLayout *layout, blender::StringRef name, int icon); /* label */
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
 * Menu.
 */
void uiItemM_ptr(uiLayout *layout, MenuType *mt, std::optional<blender::StringRef> name, int icon);
void uiItemM(uiLayout *layout,
             blender::StringRef menuname,
             std::optional<blender::StringRef> name,
             int icon);
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
/** Separator item */
void uiItemS(uiLayout *layout);
/** Separator item */
void uiItemS_ex(uiLayout *layout,
                float factor,
                LayoutSeparatorType type = LayoutSeparatorType::Auto);
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
void uiItemMenuF(
    uiLayout *layout, blender::StringRefNull, int icon, uiMenuCreateFunc func, void *arg);
/**
 * Version of #uiItemMenuF that free's `argN`.
 */
void uiItemMenuFN(
    uiLayout *layout, blender::StringRefNull, int icon, uiMenuCreateFunc func, void *argN);
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
