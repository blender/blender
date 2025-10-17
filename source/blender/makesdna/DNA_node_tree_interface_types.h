/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_enum_flags.hh"

#ifdef __cplusplus
#  include "BLI_color_types.hh"
#  include "BLI_function_ref.hh"
#  include "BLI_span.hh"
#  include "BLI_string_ref.hh"

#  include <memory>
#endif

#ifdef __cplusplus
namespace blender::bke {
class bNodeTreeInterfaceRuntime;
struct bNodeSocketType;
}  // namespace blender::bke
using bNodeTreeInterfaceRuntimeHandle = blender::bke::bNodeTreeInterfaceRuntime;
using bNodeSocketTypeHandle = blender::bke::bNodeSocketType;
#else
typedef struct bNodeTreeInterfaceRuntimeHandle bNodeTreeInterfaceRuntimeHandle;
typedef struct bNodeSocketTypeHandle bNodeSocketTypeHandle;
#endif

struct bNodeSocket;
struct bNodeTreeInterfaceItem;
struct bNodeTreeInterfacePanel;
struct bNodeTreeInterfaceSocket;
struct ID;
struct IDProperty;
struct LibraryForeachIDData;
struct BlendWriter;
struct BlendDataReader;

/** Type of interface item. */
typedef enum NodeTreeInterfaceItemType {
  NODE_INTERFACE_PANEL = 0,
  NODE_INTERFACE_SOCKET = 1,
} eNodeTreeInterfaceItemType;

/** Describes a socket and all necessary details for a node declaration. */
typedef struct bNodeTreeInterfaceItem {
  /* eNodeTreeInterfaceItemType */
  char item_type;
  char _pad[7];
} bNodeTreeInterfaceItem;

/* Socket interface flags */
typedef enum NodeTreeInterfaceSocketFlag {
  NODE_INTERFACE_SOCKET_INPUT = 1 << 0,
  NODE_INTERFACE_SOCKET_OUTPUT = 1 << 1,
  NODE_INTERFACE_SOCKET_HIDE_VALUE = 1 << 2,
  NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER = 1 << 3,
  NODE_INTERFACE_SOCKET_COMPACT = 1 << 4,
  /* To be deprecated when structure types are moved out of experimental. */
  NODE_INTERFACE_SOCKET_SINGLE_VALUE_ONLY_LEGACY = 1 << 5,
  NODE_INTERFACE_SOCKET_LAYER_SELECTION = 1 << 6,
  /* INSPECT is used by Connect to Output operator to ensure socket that exits from node group. */
  NODE_INTERFACE_SOCKET_INSPECT = 1 << 7,
  /* Socket is used in the panel header as a toggle. */
  NODE_INTERFACE_SOCKET_PANEL_TOGGLE = 1 << 8,
  /* Menu socket should be drawn expanded instead of as drop-down menu. */
  NODE_INTERFACE_SOCKET_MENU_EXPANDED = 1 << 9,
  /**
   * Indicates that drawing code may decide not to draw the label if that would result in a
   * cleaner UI.
   */
  NODE_INTERFACE_SOCKET_OPTIONAL_LABEL = 1 << 10,
} NodeTreeInterfaceSocketFlag;
ENUM_OPERATORS(NodeTreeInterfaceSocketFlag);

typedef enum NodeSocketInterfaceStructureType {
  NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO = 0,
  NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_SINGLE = 1,
  NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_DYNAMIC = 2,
  NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_FIELD = 3,
  NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_GRID = 4,
  NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_LIST = 5,
} NodeSocketInterfaceStructureType;

// TODO: Move out of DNA.
#ifdef __cplusplus
namespace blender::nodes {
enum class StructureType : int8_t {
  Single = NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_SINGLE,
  Dynamic = NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_DYNAMIC,
  Field = NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_FIELD,
  Grid = NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_GRID,
  List = NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_LIST,
};
}
#endif

typedef struct bNodeTreeInterfaceSocket {
  bNodeTreeInterfaceItem item;

  /* UI name of the socket. */
  char *name;
  char *description;
  /* Type idname of the socket to generate, e.g. "NodeSocketFloat". */
  char *socket_type;
  /* NodeTreeInterfaceSocketFlag */
  int flag;

  /* AttrDomain */
  int16_t attribute_domain;
  /** NodeDefaultInputType. */
  int16_t default_input;
  char *default_attribute_name;

  /* Unique identifier for generated sockets. */
  char *identifier;
  /* Socket default value and associated data, e.g. bNodeSocketValueFloat. */
  void *socket_data;

  struct IDProperty *properties;

  /** #NodeSocketInterfaceStructureType. */
  int8_t structure_type;
  char _pad[7];

#ifdef __cplusplus
  bNodeSocketTypeHandle *socket_typeinfo() const;
  blender::ColorGeometry4f socket_color() const;

  /**
   * Set the \a socket_type and replace the \a socket_data.
   * \param new_socket_type: Socket type idname, e.g. "NodeSocketFloat"
   */
  bool set_socket_type(blender::StringRef new_socket_type);

  /**
   * Use an existing socket to define an interface socket.
   * Replaces the current \a socket_type and any existing \a socket data if called again.
   */
  void init_from_socket_instance(const bNodeSocket *socket);
#endif
} bNodeTreeInterfaceSocket;

/* Panel interface flags */
typedef enum NodeTreeInterfacePanelFlag {
  /* Panel starts closed on new node instances. */
  NODE_INTERFACE_PANEL_DEFAULT_CLOSED = 1 << 0,
  /* In the past, not all panels allowed child panels. Now all allow them. */
  NODE_INTERFACE_PANEL_ALLOW_CHILD_PANELS_LEGACY = 1 << 1,
  /* Allow adding sockets after panels. */
  NODE_INTERFACE_PANEL_ALLOW_SOCKETS_AFTER_PANELS = 1 << 2,
  /* Whether the panel is collapsed in the node group interface tree view. */
  NODE_INTERFACE_PANEL_IS_COLLAPSED = 1 << 3,
} NodeTreeInterfacePanelFlag;
ENUM_OPERATORS(NodeTreeInterfacePanelFlag);

typedef enum NodeDefaultInputType {
  NODE_DEFAULT_INPUT_VALUE = 0,
  NODE_DEFAULT_INPUT_INDEX_FIELD = 1,
  NODE_DEFAULT_INPUT_ID_INDEX_FIELD = 2,
  NODE_DEFAULT_INPUT_NORMAL_FIELD = 3,
  NODE_DEFAULT_INPUT_POSITION_FIELD = 4,
  NODE_DEFAULT_INPUT_INSTANCE_TRANSFORM_FIELD = 5,
  NODE_DEFAULT_INPUT_HANDLE_LEFT_FIELD = 6,
  NODE_DEFAULT_INPUT_HANDLE_RIGHT_FIELD = 7,
} NodeDefaultInputType;

typedef struct bNodeTreeInterfacePanel {
  bNodeTreeInterfaceItem item;

  /* UI name of the panel. */
  char *name;
  char *description;
  /* NodeTreeInterfacePanelFlag */
  int flag;
  char _pad[4];

  bNodeTreeInterfaceItem **items_array;
  int items_num;

  /* Internal unique identifier for validating panel states. */
  int identifier;

#ifdef __cplusplus
  blender::IndexRange items_range() const;
  blender::Span<const bNodeTreeInterfaceItem *> items() const;
  blender::MutableSpan<bNodeTreeInterfaceItem *> items();

  /**
   * Check if the item is a direct child of the panel.
   */
  bool contains(const bNodeTreeInterfaceItem &item) const;
  /**
   * Search for an item in the interface.
   * \return True if the item was found.
   */
  bool contains_recursive(const bNodeTreeInterfaceItem &item) const;
  /**
   * Get the position of an item in this panel.
   * \return Position relative to the start of the panel items or -1 if the item is not in the
   * panel.
   */
  int item_position(const bNodeTreeInterfaceItem &item) const;
  /**
   * Get the index of the item in the interface.
   * \return Index if the item was found or -1 otherwise.
   */
  int item_index(const bNodeTreeInterfaceItem &item) const;
  /**
   * Get the item at the given index of the interface draw list.
   */
  const bNodeTreeInterfaceItem *item_at_index(int index) const;
  /**
   * Find the panel containing the item among this panel and all children.
   * \return Parent panel containing the item.
   */
  bNodeTreeInterfacePanel *find_parent_recursive(const bNodeTreeInterfaceItem &item);

  /** Remove all items from the panel. */
  void clear(bool do_id_user);

  /**
   * Add item at the end of the panel.
   * \note Takes ownership of the item.
   */
  void add_item(bNodeTreeInterfaceItem &item);
  /**
   * Insert an item at the given position.
   * \note Takes ownership of the item.
   */
  void insert_item(bNodeTreeInterfaceItem &item, int position);
  /**
   * Remove item from the panel.
   * \param free: Destruct and deallocate the item.
   * \return True if the item was found.
   */
  bool remove_item(bNodeTreeInterfaceItem &item, bool free);
  /**
   * Move item to a new position within the panel.
   * \return True if the item was found.
   */
  bool move_item(bNodeTreeInterfaceItem &item, int new_position);

  /**
   * Apply a function to every item in the panel, including child panels.
   * \note The items are visited in drawing order from top to bottom.
   *
   * \param fn: Function to execute for each item, iterations stops if false is returned.
   * \param include_self: Include the panel itself in the iteration.
   */
  void foreach_item(blender::FunctionRef<bool(bNodeTreeInterfaceItem &item)> fn,
                    bool include_self = false);
  /** Same as above but for a const interface. */
  void foreach_item(blender::FunctionRef<bool(const bNodeTreeInterfaceItem &item)> fn,
                    bool include_self = false) const;

  /** Get the socket that is part of the panel header if available. */
  const bNodeTreeInterfaceSocket *header_toggle_socket() const;
  bNodeTreeInterfaceSocket *header_toggle_socket();

 private:
  /** Find a valid position for inserting in the items span. */
  int find_valid_insert_position_for_item(const bNodeTreeInterfaceItem &item,
                                          int initial_position) const;
#endif
} bNodeTreeInterfacePanel;

typedef struct bNodeTreeInterface {
  bNodeTreeInterfacePanel root_panel;

  /* Global index of the active item. */
  int active_index;
  int next_uid;

  bNodeTreeInterfaceRuntimeHandle *runtime;

#ifdef __cplusplus

  /** Initialize data of new interface instance. */
  void init_data();
  /** Copy data from another interface.
   *  \param flag: ID creation/copying flags, e.g. LIB_ID_CREATE_NO_MAIN.
   */
  void copy_data(const bNodeTreeInterface &src, int flag);
  /** Free data before the owning data block is freed.
   * \note Does not decrement ID user counts, this has to be done by the caller.
   */
  void free_data();

  /** Read/write blend file data. */
  void write(BlendWriter *writer);
  void read_data(BlendDataReader *reader);

  bNodeTreeInterfaceItem *active_item();
  const bNodeTreeInterfaceItem *active_item() const;
  void active_item_set(bNodeTreeInterfaceItem *item);

  /**
   * Get the position of the item in its parent panel.
   * \return Position if the item was found or -1 otherwise.
   */
  int find_item_position(const bNodeTreeInterfaceItem &item) const
  {
    /* const_cast to avoid a const version of #find_parent_recursive. */
    const bNodeTreeInterfacePanel *parent =
        const_cast<bNodeTreeInterfacePanel &>(root_panel).find_parent_recursive(item);
    if (parent == nullptr || parent == &root_panel) {
      /* Panel is the root panel. */
      return root_panel.item_position(item);
    }
    return parent->item_position(item);
  }
  /**
   * Get the global index of the item in the interface.
   * \return Index if the item was found or -1 otherwise.
   */
  int find_item_index(const bNodeTreeInterfaceItem &item) const
  {
    return root_panel.item_index(item);
  }
  /**
   * Search for an item in the interface.
   * \return True if the item was found.
   */
  bool find_item(const bNodeTreeInterfaceItem &item) const
  {
    return root_panel.contains_recursive(item);
  }
  /**
   * Get the item at the given index of the interface draw list.
   */
  const bNodeTreeInterfaceItem *get_item_at_index(int index) const
  {
    return root_panel.item_at_index(index);
  }
  /**
   * Find the panel containing the item.
   * \param include_root: Allow #root_panel as a return value,
   *                      otherwise return nullptr for root items.
   * \return Parent panel containing the item.
   */
  bNodeTreeInterfacePanel *find_item_parent(const bNodeTreeInterfaceItem &item,
                                            bool include_root = false)
  {
    bNodeTreeInterfacePanel *parent = root_panel.find_parent_recursive(item);
    /* Return nullptr instead the root panel. */
    if (!include_root && parent == &root_panel) {
      return nullptr;
    }
    return parent;
  }

  /**
   * Add a new socket at the end of the items list.
   * \param parent: Panel in which to add the socket. If parent is null the socket is added in the
   * root panel.
   */
  bNodeTreeInterfaceSocket *add_socket(blender::StringRef name,
                                       blender::StringRef description,
                                       blender::StringRef socket_type,
                                       NodeTreeInterfaceSocketFlag flag,
                                       bNodeTreeInterfacePanel *parent);
  /**
   * Insert a new socket.
   * \param parent: Panel in which to add the socket. If parent is null the socket is added in the
   * root panel.
   * \param position: Position of the socket within the parent panel.
   */
  bNodeTreeInterfaceSocket *insert_socket(blender::StringRef name,
                                          blender::StringRef description,
                                          blender::StringRef socket_type,
                                          NodeTreeInterfaceSocketFlag flag,
                                          bNodeTreeInterfacePanel *parent,
                                          int position);

  /**
   * Add a new panel at the end of the items list.
   * \param parent: Panel in which the new panel is added as a child. If parent is null the new
   * panel is made a child of the root panel.
   */
  bNodeTreeInterfacePanel *add_panel(blender::StringRef name,
                                     blender::StringRef description,
                                     NodeTreeInterfacePanelFlag flag,
                                     bNodeTreeInterfacePanel *parent);
  /**
   * Insert a new panel.
   * \param parent: Panel in which the new panel is added as a child. If parent is null the new
   * panel is made a child of the root panel.
   * \param position: Position of the child panel within the parent panel.
   */
  bNodeTreeInterfacePanel *insert_panel(blender::StringRef name,
                                        blender::StringRef description,
                                        NodeTreeInterfacePanelFlag flag,
                                        bNodeTreeInterfacePanel *parent,
                                        int position);

  /**
   * Add a copy of an item at the end of the items list.
   * \param parent: Add the item inside the parent panel. If parent is null the item is made a
   * child of the root panel.
   */
  bNodeTreeInterfaceItem *add_item_copy(const bNodeTreeInterfaceItem &item,
                                        bNodeTreeInterfacePanel *parent);
  /**
   * Insert a copy of an item.
   * \param parent: Add the item inside the parent panel. If parent is null the item is made a
   * child of the root panel.
   * \param position: Position of the item within the parent panel.
   */
  bNodeTreeInterfaceItem *insert_item_copy(const bNodeTreeInterfaceItem &item,
                                           bNodeTreeInterfacePanel *parent,
                                           int position);

  /**
   * Remove an item from the interface.
   * \param move_content_to_parent: If the item is a panel, move the contents to the parent instead
   * of deleting it.
   * \return True if the item was found and successfully removed.
   */
  bool remove_item(bNodeTreeInterfaceItem &item, bool move_content_to_parent = true);
  void clear_items();

  /**
   * Move an item to a new position.
   * \param new_position: New position of the item in the parent panel.
   */
  bool move_item(bNodeTreeInterfaceItem &item, int new_position);
  /**
   * Move an item to a new panel and/or position.
   * \param new_parent: Panel that the item is moved to. If null the item is added to the root
   * panel.
   * \param new_position: New position of the item in the parent panel.
   */
  bool move_item_to_parent(bNodeTreeInterfaceItem &item,
                           bNodeTreeInterfacePanel *new_parent,
                           int new_position);

  /**
   * Apply a function to every item in the interface.
   * \note The items are visited in drawing order from top to bottom.
   *
   * \param fn: Function to execute for each item, iterations stops if false is returned.
   * \param include_root: Include the root panel in the iteration.
   */
  void foreach_item(blender::FunctionRef<bool(bNodeTreeInterfaceItem &item)> fn,
                    bool include_root = false)
  {
    root_panel.foreach_item(fn, /*include_self=*/include_root);
  }
  /**
   * Apply a function to every item in the interface.
   * \note The items are visited in drawing order from top to bottom.
   *
   * \param fn: Function to execute for each item, iterations stops if false is returned.
   * \param include_root: Include the root panel in the iteration.
   */
  void foreach_item(blender::FunctionRef<bool(const bNodeTreeInterfaceItem &item)> fn,
                    bool include_root = false) const
  {
    root_panel.foreach_item(fn, /*include_self=*/include_root);
  }

  /** Callback for every ID pointer in the interface data. */
  void foreach_id(LibraryForeachIDData *cb);

  /** True if the items cache is ready to use. */
  bool items_cache_is_available() const;

  /** Ensure the items cache can be accessed. */
  void ensure_items_cache() const;

  /** True if any trees and nodes depending on the interface require updates. */
  bool requires_dependent_tree_updates() const;

  /** Call after changing the items list. */
  void tag_items_changed();
  /** Call after generic user changes through the API. */
  void tag_items_changed_generic();
  /** Call after changing an item property. */
  void tag_item_property_changed();

  /**
   * Reset flag to indicate that dependent trees have been updated.
   * Should only be called by #NodeTreeMainUpdater.
   */
  void reset_interface_changed();

 private:
  /** Tag after interface changes that require updates to dependent trees. */
  void tag_interface_changed();
  /** Invalidate caches and force full tree update after loading DNA. */
  void tag_missing_runtime_data();

#endif
} bNodeTreeInterface;
