/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <functional>
#include <optional>

#include "BLI_enum_flags.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "UI_interface_types.hh"

namespace blender {

struct bContext;
struct bContextStore;
struct EnumPropertyItem;
struct IDProperty;
struct uiList;
struct uiStyle;
struct MenuType;
struct PanelType;
struct Panel;
struct PointerRNA;
struct PropertyRNA;
struct StructRNA;
struct wmOperatorType;

/* Layout
 *
 * More automated layout of buttons. Has three levels:
 * - Layout: contains a number templates, within a bounded width or height.
 * - Template: predefined layouts for buttons with a number of slots, each
 *   slot can contain multiple items.
 * - Item: item to put in a template slot, being either an RNA property,
 *   operator, label or menu. Also regular buttons can be used when setting
 *   uiBlockCurLayout. */

namespace ui {
enum class ItemType : int8_t;
enum class ItemInternalFlag : uint8_t;
enum class EmbossType : uint8_t;
enum class LayoutAlign : int8_t;
enum class ButProgressType : int8_t;
enum class LayoutDirection : int8_t;
enum class AlertIcon : int8_t;

struct ItemInternal;
struct LayoutInternal;
struct Layout;
struct LayoutRoot;
}  // namespace ui

namespace wm {
enum class OpCallContext : int8_t;
}

namespace ui {

struct PanelLayout {
  Layout *header;
  Layout *body;
};

struct Item {

  Item(ItemType type);
  Item(const Item &) = default;
  virtual ~Item() = default;

  [[nodiscard]] bool fixed_size() const;
  void fixed_size_set(bool fixed_size);

  [[nodiscard]] ItemType type() const;

  [[nodiscard]] int2 size() const;
  [[nodiscard]] int2 offset() const;

 protected:
  ItemInternalFlag flag_ = {};
  ItemType type_ = {};

  friend struct ItemInternal;
};

enum eUI_Item_Flag : uint16_t;

enum class LayoutSeparatorType : int8_t {
  Auto,
  Space,
  Line,
};

enum class NodeAssetMenuOperatorType : int8_t {
  Add,
  Swap,
};

enum class EnumTabExpand {
  Default = 0,
  Row,
};

struct Layout : public Item, NonCopyable, NonMovable {
 protected:
  LayoutRoot *root_ = nullptr;
  bContextStore *context_ = nullptr;
  Layout *parent_ = nullptr;
  std::string heading_;

  Vector<Item *> items_;

  /** Sub layout to add child items, if not the layout itself. */
  Layout *child_items_layout_ = nullptr;

  int x_ = 0, y_ = 0, w_ = 0, h_ = 0;

  short space_ = 0;

  float scale_[2] = {0.0f, 0.0f};
  bool align_ = false;
  bool active_ = false;
  bool active_default_ = false;
  bool activate_init_ = false;
  bool enabled_ = false;
  bool redalert_ = false;
  /** For layouts inside grid-flow, they and their items shall never have a fixed maximal size. */
  bool variable_size_ = false;
  LayoutAlign alignment_ = {};
  EmbossType emboss_ = {};
  /** for fixed width or height to avoid UI size changes */
  float units_[2] = {0.0f, 0.0f};
  /** Is copied to uiButs created in this layout. */
  float search_weight_ = 0.0f;

 public:
  Layout(ItemType type, LayoutRoot *root);

  [[nodiscard]] bool active() const;
  /**
   * Sets the active state of the layout and its items.
   * When false the layout and its buttons are grayed out, user can still interact with them but
   * generally they will not have an active use.
   */
  void active_set(bool active);

  [[nodiscard]] bool active_default() const;
  /**
   * When set to true the next operator button added in the layout will be highlighted as default
   * action when pressing return, in popup dialogs this overrides default confirmation buttons.
   */
  void active_default_set(bool active_default);

  [[nodiscard]] bool activate_init() const;
  /**
   * When set to true, the next button added in the layout will be activated on first display.
   * Only for popups dialogs and only the first button in the popup with this flag will be
   * activated.
   */
  void activate_init_set(bool activate_init);

  [[nodiscard]] LayoutAlign alignment() const;
  void alignment_set(LayoutAlign alignment);

  [[nodiscard]] Block *block() const;

  void context_copy(const bContextStore *context);

  [[nodiscard]] const PointerRNA *context_ptr_get(const StringRef name,
                                                  const StructRNA *type) const;
  void context_ptr_set(StringRef name, const PointerRNA *ptr);

  [[nodiscard]] std::optional<StringRefNull> context_string_get(const StringRef name) const;
  void context_string_set(StringRef name, StringRef value);

  [[nodiscard]] std::optional<int64_t> context_int_get(const StringRef name) const;
  void context_int_set(StringRef name, int64_t value);

  /** Only for convenience. */
  void context_set_from_but(const Button *but);

  [[nodiscard]] bContextStore *context_store() const;

  [[nodiscard]] bool enabled() const;
  /**
   * Sets the enabled state of the layout and its items.
   * When false the layout and its buttons are grayed out, user can't interaction with them, only
   * buttons tooltips are available on hovering.
   */
  void enabled_set(bool enabled);

  [[nodiscard]] EmbossType emboss() const;
  void emboss_set(EmbossType emboss);

  [[nodiscard]] LayoutDirection local_direction() const;

  [[nodiscard]] wm::OpCallContext operator_context() const;
  /** Sets the default call context for new operator buttons added in any #root_ sub-layout. */
  void operator_context_set(wm::OpCallContext opcontext);

  [[nodiscard]] bool red_alert() const;
  /**
   * When set to true new items added in the layout are highlighted with the error state
   * color #TH_REDALERT.
   */
  void red_alert_set(bool red_alert);

  [[nodiscard]] Panel *root_panel() const;

  [[nodiscard]] float scale_x() const;
  void scale_x_set(float scale);

  [[nodiscard]] float scale_y() const;
  void scale_y_set(float scale);

  [[nodiscard]] float search_weight() const;
  void search_weight_set(float weight);

  [[nodiscard]] float ui_units_x() const;
  /** Sets a fixed width size for this layout. */
  void ui_units_x_set(float width);

  [[nodiscard]] float ui_units_y() const;
  /** Sets a fixed height size for this layout. */
  void ui_units_y_set(float height);

  [[nodiscard]] bool use_property_split() const;
  /**
   * Sets when to split property's label into a separate button when adding new property buttons.
   */
  void use_property_split_set(bool value);

  [[nodiscard]] bool use_property_decorate() const;
  /**
   * Sets when to add an extra button to insert keyframes next to new property buttons added in the
   * layout.
   */
  void use_property_decorate_set(bool is_sep);

  [[nodiscard]] int width() const;

  /** Sub-layout items. */

  Layout &absolute(bool align = false);

  /**
   * Add a new box sub-layout, items placed in this sub-layout are added vertically one under
   * each other in a column and are surrounded by a box.
   */
  Layout &box();
  /**
   * Add a new column sub-layout, items placed in this sub-layout are added vertically one under
   * each other in a column.
   */
  Layout &column(bool align);
  /**
   * Add a new column sub-layout, items placed in this sub-layout are added vertically one under
   * each other in a column.
   * \param heading: Heading label to set to the first child element added in the sub-layout
   * through #Layout::prop. When property split is used, this heading label is set in the split
   * label column when there is no label defined.
   */
  Layout &column(bool align, StringRef heading);

  /**
   * Add a new row sub-layout, items placed in this sub-layout are added horizontally next to each
   * other in row.
   */
  Layout &row(bool align);
  /**
   * Add a new row sub-layout, items placed in this sub-layout are added horizontally next to each
   * other in row.
   * \param heading: Heading label to set to the first child element added in the sub-layout
   * through #Layout::prop. When property split is used, this heading label is set in the split
   * label column when there is no label defined.
   */
  Layout &row(bool align, StringRef heading);

  /**
   * Add a new column flow sub-layout, items placed in this sub-layout would be evenly distributed
   * in columns.
   * \param number: the number of columns in which items are distributed.
   */
  Layout &column_flow(int number, bool align);
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
  Layout &grid_flow(
      bool row_major, int columns_len, bool even_columns, bool even_rows, bool align);

  /** Add a new list box sub-layout. */
  Layout &list_box(uiList *ui_list, PointerRNA *actptr, PropertyRNA *actprop);

  /**
   * Add a pie menu layout, buttons are arranged around a center.
   * Only one pie menu per layout root can be added, if it's already initialized it will be
   * returned instead of adding a new one.
   */
  Layout &menu_pie();

  /** Add a new overlap sub-layout. */
  Layout &overlap();

  /**
   * Create a "layout panel" which is a panel that is defined as part of the `Layout`. This
   * allows creating expandable sections which can also be nested.
   *
   * The open-state of the panel is defined by an RNA property which is passed in as a pointer +
   * property name pair. This gives the caller flexibility to decide who should own the open-state.
   *
   * \param C: The context is necessary because sometimes the panel may be forced to be open by the
   * context even of the open-property is `false`. This can happen with e.g. property search.
   * \param layout: The `Layout` that should contain the sub-panel.
   * Only layouts that span the full width of the region are supported for now.
   * \param open_prop_owner: Data that contains the open-property.
   * \param open_prop_name: Name of the open-property in `open_prop_owner`.
   *
   * \return A #PanelLayout containing layouts for both the header row and the panel body. If the
   * panel is closed and should not be drawn, the body layout will be NULL.
   */
  PanelLayout panel_prop(const bContext *C,
                         PointerRNA *open_prop_owner,
                         StringRefNull open_prop_name);
  /**
   * Variant of #panel_prop that automatically creates the header row with the
   * given label and only returns the body layout.
   *
   * \param label: Text that's shown in the panel header. It should already be translated.
   *
   * \return NULL if the panel is closed and should not be drawn, otherwise the layout where the
   * sub-panel should be inserted into.
   */
  Layout *panel_prop(const bContext *C,
                     PointerRNA *open_prop_owner,
                     StringRefNull open_prop_name,
                     StringRef label);
  PanelLayout panel_prop_with_bool_header(const bContext *C,
                                          PointerRNA *open_prop_owner,
                                          StringRefNull open_prop_name,
                                          PointerRNA *bool_prop_owner,
                                          StringRefNull bool_prop_name,
                                          std::optional<StringRef> label);
  /**
   * Variant of #panel_prop that automatically stores the open-close-state in the root
   * panel. When a dynamic number of panels is required, it's recommended to use #panel_prop
   * instead of passing in generated id names.
   *
   * \param idname: String that identifies the open-close-state in the root panel.
   */
  PanelLayout panel(const bContext *C, StringRef idname, bool default_closed);

  /**
   * Variant of #panel that automatically creates the header row with the given label and
   * only returns the body layout.
   *
   * \param label:  Text that's shown in the panel header. It should already be translated.
   *
   * \return NULL if the panel is closed and should not be drawn, otherwise the layout where the
   * sub-panel should be inserted into.
   */
  Layout *panel(const bContext *C, StringRef idname, bool default_closed, StringRef label);

  /**
   * Add a new split sub-layout, items placed in this sub-layout are added horizontally next to
   * each other in row, but width is splitted between the first item and remaining items.
   * \param percentage: Width percent to split.
   */
  Layout &split(float percentage, bool align);

  /** Items. */

  /**
   * Insert a decorator item for a button with the same property as \a prop.
   * To force inserting a blank dummy element, nullptr can be passed for \a or and \a prop.
   */
  void decorator(PointerRNA *ptr, PropertyRNA *prop, int index);
  /**
   * Insert a decorator item for a button with the same property as \a prop.
   * To force inserting a blank dummy element, nullptr can be passed for \a ptr or `std::nullopt`
   * for \a propname.
   */
  void decorator(PointerRNA *ptr, std::optional<StringRefNull> propname, int index);

  /** Adds a label item that will display text and/or icon in the layout. */
  void label(StringRef name, int icon);

  /**
   * Adds a menu item, which is a button that when active will display a menu.
   * If menu fails to poll with `WM_menutype_poll` it will not be added into the layout.
   */
  void menu(MenuType *mt, std::optional<StringRef> name, int icon);
  /**
   * Adds a menu item, which is a button that when active will display a menu.
   * If menu fails to poll with `WM_menutype_poll` it will not be added into the layout.
   */
  void menu(StringRef menuname, std::optional<StringRef> name, int icon);

  /** Adds the menu content into this layout. */
  void menu_contents(StringRef menuname);

  /**
   * Adds a menu item, which is a button that when active will display a menu.
   * \param name: Label to show in the menu button.
   * \param func: Function that generates the menu layout.
   * \param arg: Pointer to data used as last argument in \a func.
   */
  void menu_fn(StringRefNull name, int icon, MenuCreateFunc func, void *arg);
  /**
   * Adds a menu item, which is a button that when active will display a menu.
   * \param name: Label to show in the menu button.
   * \param func: Function that generates the menu layout.
   * \param argN: Pointer to data used as last argument in \a func, it will be
   * freed with the menu button.
   */
  void menu_fn_argN_free(StringRefNull name, int icon, MenuCreateFunc func, void *argN);
  /**
   * Adds a operator item, places a button in the layout to call the operator.
   * \param ot: Operator to add.
   * \param name: Text to show in the layout.
   * \param context: Operator call context for #WM_operator_name_call.
   * \returns Operator pointer to write properties.
   */
  PointerRNA op(wmOperatorType *ot,
                std::optional<StringRef> name,
                int icon,
                wm::OpCallContext context,
                eUI_Item_Flag flag);

  /**
   * Adds a operator item, places a button in the layout to call the operator.
   * \param ot: Operator to add.
   * \param name: Text to show in the layout.
   * \returns Operator pointer to write properties.
   */
  PointerRNA op(wmOperatorType *ot, std::optional<StringRef> name, int icon);

  /**
   * Adds a operator item, places a button in the layout to call the operator.
   * \param opname: Operator id name.
   * \param name: Text to show in the layout.
   * \returns Operator pointer to write properties, might be #PointerRNA_NULL if operator does not
   * exists.
   */
  PointerRNA op(StringRefNull opname, std::optional<StringRef> name, int icon);

  /**
   * Adds a operator item, places a button in the layout to call the operator.
   * \param opname: Operator id name.
   * \param name: Text to show in the layout.
   * \param context: Operator call context for #WM_operator_name_call.
   * \returns Operator pointer to write properties, might be #PointerRNA_NULL if operator does not
   * exists.
   */
  PointerRNA op(StringRefNull opname,
                std::optional<StringRef> name,
                int icon,
                wm::OpCallContext context,
                eUI_Item_Flag flag);
  /**
   * Expands and sets each enum property value as an operator button.
   * \param propname: Name of the operator's enum property.
   * \param properties: Extra operator properties values to set.
   * \param active: an optional item to highlight.
   */
  void op_enum(StringRefNull opname,
               StringRefNull propname,
               IDProperty *properties,
               wm::OpCallContext context,
               eUI_Item_Flag flag,
               const int active = -1);

  /**
   * Expands and sets each enum property value as an operator button.
   * \param propname: Name of the operator's enum property.
   */
  void op_enum(StringRefNull opname, StringRefNull propname);
  /**
   * Expands and sets each enum property value as an operator button.
   * \param prop: Operator's enum property.
   * \param properties: Extra operator properties values to set.
   * \param item_array: Precalculated item array, could be a subset of the enum property values.
   * \param active: an optional item to highlight.
   */
  void op_enum_items(wmOperatorType *ot,
                     const PointerRNA &ptr,
                     PropertyRNA *prop,
                     IDProperty *properties,
                     wm::OpCallContext context,
                     eUI_Item_Flag flag,
                     const EnumPropertyItem *item_array,
                     int totitem,
                     int active = -1);

  /**
   * Adds a #op_enum menu.
   * \returns Operator pointer to write extra properties to set when menu buttons are
   * displayed, might be #PointerRNA_NULL if operator does not exist.
   */
  PointerRNA op_menu_enum(const bContext *C,
                          wmOperatorType *ot,
                          StringRefNull propname,
                          std::optional<StringRefNull> name,
                          int icon);
  /**
   * Adds a #op_enum menu.
   * \returns Operator pointer to write extra properties to set when menu buttons are
   * displayed, might be #PointerRNA_NULL if operator does not exist.
   */
  PointerRNA op_menu_enum(const bContext *C,
                          StringRefNull opname,
                          StringRefNull propname,
                          StringRefNull name,
                          int icon);
  /**
   * Adds a operator item, places a button in the layout to call the operator, if the button is
   * held down, a menu will be displayed instead.
   * \param ot: Operator to add.
   * \param name: Text to show in the layout.
   * \param context: Operator call context for #WM_operator_name_call.
   * \param menu_id: menu to show on held down.
   * \returns Operator pointer to write properties, might be #PointerRNA_NULL if operator does not
   * exists.
   */
  PointerRNA op_menu_hold(wmOperatorType *ot,
                          std::optional<StringRef> name,
                          int icon,
                          wm::OpCallContext context,
                          eUI_Item_Flag flag,
                          const char *menu_id);

  void progress_indicator(const char *text, float factor, ButProgressType progress_type);

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
            std::optional<StringRef> name_opt,
            int icon,
            std::optional<StringRef> placeholder = std::nullopt);
  /** Adds a RNA property item, and exposes it into the layout. */
  void prop(PointerRNA *ptr,
            StringRefNull propname,
            eUI_Item_Flag flag,
            std::optional<StringRef> name,
            int icon);

  void popover(const bContext *C, PanelType *pt, std::optional<StringRef> name_opt, int icon);
  void popover(const bContext *C,
               StringRef panel_type,
               std::optional<StringRef> name_opt,
               int icon);
  void popover_group(
      bContext *C, int space_id, int region_id, const char *context, const char *category);

  /**
   * Add a enum property value item. This button acts like a radio button that are used to chose
   * a single enum value from a set of the enum property value items.
   */
  void prop_enum(
      PointerRNA *ptr, PropertyRNA *prop, int value, std::optional<StringRefNull> name, int icon);
  /**
   * Add a enum property value item. This button acts like a radio button that are used to chose
   * a single enum value from a set of the enum property value items.
   */
  void prop_enum(PointerRNA *ptr,
                 PropertyRNA *prop,
                 const char *value,
                 std::optional<StringRefNull> name,
                 int icon);
  /**
   * Add a enum property value item. This button acts like a radio button that are used to chose
   * a single enum value from a set of the enum property value items.
   */
  void prop_enum(PointerRNA *ptr,
                 StringRefNull propname,
                 const char *value,
                 std::optional<StringRefNull> name,
                 int icon);

  /** Add a enum property item, and exposes its value throw a radio button menu. */
  void prop_menu_enum(PointerRNA *ptr,
                      PropertyRNA *prop,
                      std::optional<StringRefNull> name,
                      int icon);

  /** Expands enum property value items as tabs buttons. */
  void prop_tabs_enum(bContext *C,
                      PointerRNA *ptr,
                      PropertyRNA *prop,
                      PointerRNA *ptr_highlight,
                      PropertyRNA *prop_highlight,
                      bool icon_only,
                      EnumTabExpand expand_as = EnumTabExpand::Default);

  /** Expands enum property value items as radio buttons. */
  void props_enum(PointerRNA *ptr, StringRefNull propname);

  /**
   * Adds a RNA enum/pointer/string/ property item, and exposes it into the layout. Button input
   * would suggest values from the search property collection.
   * \param searchprop: Collection property in \a searchptr from where to take input values.
   * \param results_are_suggestions: Allow inputs that not match any suggested value.
   * \param item_searchpropname: The name of the string property in the collection items to use for
   *        searching (if unset, code will use RNA_struc.
   */
  void prop_search(PointerRNA *ptr,
                   PropertyRNA *prop,
                   PointerRNA *searchptr,
                   PropertyRNA *searchprop,
                   PropertyRNA *item_searchpropname,
                   std::optional<StringRefNull> name,
                   int icon,
                   bool results_are_suggestions);
  /**
   * Adds a RNA enum/pointer/string/ property item, and exposes it into the layout. Button input
   * would suggest values from the search property collection, input must match a suggested value.
   * \param searchprop: Collection property in \a searchptr from where to take input values.
   */
  void prop_search(PointerRNA *ptr,
                   StringRefNull propname,
                   PointerRNA *searchptr,
                   StringRefNull searchpropname,
                   std::optional<StringRefNull> name,
                   int icon);

  /**
   * Adds a RNA property item, and sets a custom popover to expose its value.
   */
  void prop_with_popover(PointerRNA *ptr,
                         PropertyRNA *prop,
                         int index,
                         int value,
                         eUI_Item_Flag flag,
                         std::optional<StringRefNull> name,
                         int icon,
                         const char *panel_type);

  /**
   * Adds a RNA property item, and sets a custom menu to expose its value.
   */
  void prop_with_menu(PointerRNA *ptr,
                      PropertyRNA *prop,
                      int index,
                      int value,
                      eUI_Item_Flag flag,
                      std::optional<StringRefNull> name,
                      int icon,
                      const char *menu_type);

  /** Simple button executing \a func on click. */
  Button *button(StringRef name,
                 int icon,
                 std::function<void(bContext &)> func,
                 std::optional<StringRef> tooltip = std::nullopt);

  /** Adds a separator item, that adds empty space between items. */
  void separator(float factor = 1.0f, LayoutSeparatorType type = LayoutSeparatorType::Auto);

  /** Adds a spacer item that inserts empty horizontal space between other items in the layout. */
  void separator_spacer();

  friend struct LayoutInternal;

  [[nodiscard]] LayoutRoot *root() const;
  [[nodiscard]] const bContextStore *context() const;
  [[nodiscard]] Layout *parent() const;
  [[nodiscard]] StringRef heading() const;
  void heading_reset();
  [[nodiscard]] Span<Item *> items() const;
  [[nodiscard]] bool align() const;
  [[nodiscard]] bool variable_size() const;
  [[nodiscard]] EmbossType emboss_or_undefined() const;
  [[nodiscard]] int2 size() const;
  [[nodiscard]] int2 offset() const;

 protected:
  void estimate();
  virtual void estimate_impl();
  void resolve();
  virtual void resolve_impl();
};

inline bool Layout::active() const
{
  return active_;
}
inline void Layout::active_set(bool active)
{
  active_ = active;
}

inline bool Layout::active_default() const
{
  return active_default_;
}
inline void Layout::active_default_set(bool active_default)
{
  active_default_ = active_default;
}

inline bool Layout::activate_init() const
{
  return activate_init_;
}
inline void Layout::activate_init_set(bool activate_init)
{
  activate_init_ = activate_init;
}

inline LayoutAlign Layout::alignment() const
{
  return alignment_;
}

inline void Layout::alignment_set(LayoutAlign alignment)
{
  alignment_ = alignment;
}

inline bContextStore *Layout::context_store() const
{
  return context_;
}

inline bool Layout::enabled() const
{
  return enabled_;
}
inline void Layout::enabled_set(bool enabled)
{
  enabled_ = enabled;
}

inline bool Layout::red_alert() const
{
  return redalert_;
}
inline void Layout::red_alert_set(bool red_alert)
{
  redalert_ = red_alert;
}

inline float Layout::search_weight() const
{
  return search_weight_;
}
inline void Layout::search_weight_set(float weight)
{
  search_weight_ = weight;
}

inline float Layout::scale_x() const
{
  return scale_[0];
}
inline void Layout::scale_x_set(float scale)
{
  scale_[0] = scale;
}

inline float Layout::scale_y() const
{
  return scale_[1];
}
inline void Layout::scale_y_set(float scale)
{
  scale_[1] = scale;
}

inline float Layout::ui_units_x() const
{
  return units_[0];
}
inline void Layout::ui_units_x_set(float width)
{
  units_[0] = width;
}

inline float Layout::ui_units_y() const
{
  return units_[1];
}
inline void Layout::ui_units_y_set(float height)
{
  units_[1] = height;
}

inline int Layout::width() const
{
  return this->w_;
}

enum class LayoutDirection : int8_t {
  Horizontal = 0,
  Vertical = 1,
};

enum class LayoutType : int8_t {
  Panel = 0,
  Header = 1,
  Menu = 2,
  Toolbar = 3,
  PieMenu = 4,
  VerticalBar = 5,
};

enum class LayoutAlign : int8_t {
  Expand = 0,
  Left = 1,
  Center = 2,
  Right = 3,
};
enum class ButProgressType : int8_t {
  Bar = 0,
  Ring = 1,
};

Layout &block_layout(Block *block,
                     LayoutDirection direction,
                     LayoutType type,
                     int x,
                     int y,
                     int size,
                     int em,
                     int padding,
                     const uiStyle *style);
int2 block_layout_resolve(Block *block);

void block_layout_set_current(Block *block, Layout *layout);
bool block_layout_needs_resolving(const Block *block);
/**
 * Used for property search when the layout process needs to be cancelled in order to avoid
 * computing the locations for buttons, but the layout items created while adding the buttons
 * must still be freed.
 */
void block_layout_free(Block *block);

enum eUI_Item_Flag : uint16_t {
  /* ITEM_O_RETURN_PROPS = 1 << 0, */ /* UNUSED */
  ITEM_R_EXPAND = 1 << 1,
  ITEM_R_SLIDER = 1 << 2,
  /**
   * Use for booleans, causes the button to draw with an outline (emboss),
   * instead of text with a checkbox.
   * This is implied when toggle buttons have an icon
   * unless #ITEM_R_ICON_NEVER flag is set.
   */
  ITEM_R_TOGGLE = 1 << 3,
  /**
   * Don't attempt to use an icon when the icon is set to #ICON_NONE.
   *
   * Use for booleans, causes the buttons to always show as a checkbox
   * even when there is an icon (which would normally show the button as a toggle).
   */
  ITEM_R_ICON_NEVER = 1 << 4,
  ITEM_R_ICON_ONLY = 1 << 5,
  ITEM_R_EVENT = 1 << 6,
  ITEM_R_FULL_EVENT = 1 << 7,
  ITEM_R_NO_BG = 1 << 8,
  ITEM_R_IMMEDIATE = 1 << 9,
  ITEM_O_DEPRESS = 1 << 10,
  ITEM_R_COMPACT = 1 << 11,
  ITEM_R_CHECKBOX_INVERT = 1 << 12,
  /** Don't add a real decorator item, just blank space. */
  ITEM_R_FORCE_BLANK_DECORATE = 1 << 13,
  /* Even create the property split layout if there's no name to show there. */
  ITEM_R_SPLIT_EMPTY_NAME = 1 << 14,
  /**
   * Only for text buttons (for now): Force the button as active in a semi-modal state (capturing
   * text input while leaving the remaining UI interactive).
   */
  ITEM_R_TEXT_BUT_FORCE_SEMI_MODAL_ACTIVE = 1 << 15,
};
ENUM_OPERATORS(eUI_Item_Flag)
#define UI_ITEM_NONE ui::eUI_Item_Flag(0)

/**
 * Apply property search behavior, setting panel flags and deactivating buttons that don't match.
 *
 * \note Must not be run after #block_layout_resolve.
 */
bool block_apply_search_filter(Block *block, const char *search_filter);

void uiLayoutSetFunc(Layout *layout, MenuHandleFunc handlefunc, void *argv);

/**
 * Set tooltip function for all buttons in the layout.
 * func, arg and free_arg are passed on to button_func_tooltip_set, so their meaning is the same.
 *
 * \param func: The callback function that gets called to get tooltip content
 * \param arg: An optional opaque pointer that gets passed to func
 * \param free_arg: An optional callback for freeing arg (can be set to e.g. MEM_freeN)
 * \param copy_arg: An optional callback for duplicating arg in case button_func_tooltip_set
 * is being called on multiple buttons (can be set to e.g. MEM_dupallocN). If set to NULL, arg will
 * be passed as-is to all buttons.
 */
void uiLayoutSetTooltipFunc(
    Layout *layout, ButtonToolTipFunc func, void *arg, CopyArgFunc copy_arg, FreeArgFunc free_arg);

/**
 * Same as above but should be used when building a fully custom tooltip instead of just
 * generating a description.
 */
void uiLayoutSetTooltipCustomFunc(Layout *layout,
                                  ButtonToolTipCustomFunc func,
                                  void *arg,
                                  CopyArgFunc copy_arg,
                                  FreeArgFunc free_arg);

void menutype_draw(bContext *C, MenuType *mt, Layout *layout);

/**
 * Used for popup panels only.
 */
void UI_paneltype_draw(bContext *C, PanelType *pt, Layout *layout);

int uiLayoutListItemPaddingWidth();
void uiLayoutListItemAddPadding(Layout *layout);

/* Layout create functions. */

bool uiLayoutEndsWithPanelHeader(const Layout &layout);

struct PropertySplitWrapper {
  Layout *label_column;
  Layout *property_row;
  /**
   * Column for decorators. Note that this may be null, see #uiItemPropertySplitWrapperCreate().
   */
  Layout *decorate_column;
};

/**
 * Normally, we handle the split layout in #Layout::prop(), but there are other cases where the
 * logic is needed. Ideally, #Layout::prop() could just call this, but it currently has too many
 * special needs.
 *
 * The returned #uiPropertySplitWrapper.decorator_column may be null when decorators are disabled
 * (#uiLayoutGetPropDecorate() returns false).
 */
PropertySplitWrapper uiItemPropertySplitWrapperCreate(Layout *parent_layout);

Button *uiItemL_ex(Layout *layout, StringRef name, int icon, bool highlight, bool redalert);
/**
 * Helper to add a label using a property split layout if needed. After calling this the
 * active layout will be the one to place the labeled items in. An additional layout may be
 * returned to place decorator buttons in.
 *
 * \return the layout to place decorators in, if #UI_ITEM_PROP_SEP is enabled. Otherwise null.
 */
Layout *uiItemL_respect_property_split(Layout *layout, StringRef text, int icon);
/**
 * Label icon for dragging.
 */
void uiItemLDrag(Layout *layout, PointerRNA *ptr, StringRef name, int icon);

/* Only for testing, inspecting layouts. */
/**
 * Evaluate layout items as a Python dictionary.
 */
std::string layout_introspect(Layout *layout);

/**
 * Helpers to add a big icon and create a split layout for alert popups.
 * Returns the layout to place further items into the alert box.
 */
Layout *uiItemsAlertBox(Block *block,
                        const uiStyle *style,
                        const int dialog_width,
                        const AlertIcon icon,
                        const int icon_size);
Layout *uiItemsAlertBox(Block *block, const int size, const AlertIcon icon);

}  // namespace ui
}  // namespace blender
