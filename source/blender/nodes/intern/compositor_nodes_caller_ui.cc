/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.hh"

#include "BLI_array.hh"
#include "BLI_listbase.h"
#include "BLI_listbase_iterator.hh"
#include "BLI_span.hh"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "DNA_node_tree_interface_types.h"
#include "DNA_node_types.h"
#include "DNA_sequence_types.h"

#include "NOD_caller_ui.hh"
#include "NOD_composite.hh"
#include "NOD_compositor_nodes_caller_ui.hh"
#include "NOD_compositor_nodes_srna.hh"
#include "NOD_socket_usage_inference.hh"

#include "SEQ_iterator.hh"
#include "SEQ_modifier.hh"
#include "SEQ_modifiertypes.hh"
#include "SEQ_sequencer.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"

namespace blender::nodes {

namespace {

struct DrawGroupInputsContext {
  const bContext &C;
  bNodeTree *tree;
  PointerRNA *properties_ptr;
  PointerRNA *bmain_ptr;

  Array<nodes::socket_usage_inference::SocketUsage> input_usages;
  Array<nodes::socket_usage_inference::SocketUsage> output_usages;

  bool input_is_visible(const bNodeTreeInterfaceSocket &socket) const
  {
    return this->input_usages[this->tree->interface_input_index(socket)].is_visible;
  }

  bool input_is_active(const bNodeTreeInterfaceSocket &socket) const
  {
    return this->input_usages[this->tree->interface_input_index(socket)].is_used;
  }
};
};  // namespace

/* Drawing the properties manually with #ui::Layout::prop instead of #uiDefAutoButsRNA allows using
 * the node socket identifier for the property names, since they are unique, but also having
 * the correct label displayed in the UI. */
static void draw_property_for_socket(DrawGroupInputsContext &ctx,
                                     ui::Layout &layout,
                                     const bNodeTreeInterfaceSocket &socket,
                                     PointerRNA *socket_props_ptr,
                                     const std::optional<StringRef> parent_name = std::nullopt)
{
  if (!ctx.input_is_visible(socket)) {
    /* The input is not used currently, but it would be used if any menu input is changed.
     * By convention, the input is hidden in this case instead of just grayed out. */
    return;
  }

  ui::Layout &row = layout.row(true);
  row.use_property_decorate_set(true);
  row.active_set(ctx.input_is_active(socket));

  /* Use #ui::Layout::prop_search to draw pointer properties because #ui::Layout::prop would not
   * have enough information about what type of ID to select for editing the values. This is
   * because pointer IDProperties contain no information about their type. */
  const bke::bNodeSocketType *typeinfo = socket.socket_typeinfo();
  const eNodeSocketDatatype type = typeinfo ? typeinfo->type : SOCK_CUSTOM;

  /* Check #composite_node_tree_socket_type_valid for which socket types are valid and should be
   * drawn. */
  if (!ELEM(type,
            SOCK_FLOAT,
            SOCK_INT,
            SOCK_BOOLEAN,
            SOCK_VECTOR,
            SOCK_INT_VECTOR,
            SOCK_RGBA,
            SOCK_MATRIX,
            SOCK_MENU,
            SOCK_STRING,
            SOCK_OBJECT))
  {
    return;
  }

  std::string name = socket.name ? IFACE_(socket.name) : "";

  /* If the property has a prefix that's the same string as the name of the panel it's in, remove
   * the prefix so it appears less verbose. */
  if (parent_name.has_value()) {
    const StringRef prefix_to_remove = *parent_name;
    const int prefix_size = prefix_to_remove.size();
    const int pos = name.find(prefix_to_remove);
    if (pos == 0 && name.size() > prefix_size && name[prefix_size] == ' ') {
      name = name.substr(prefix_size + 1);
    }
  }

  switch (type) {
    case SOCK_OBJECT: {
      row.prop_search(socket_props_ptr, "value", ctx.bmain_ptr, "objects", name, ICON_OBJECT_DATA);
      break;
    }
    case SOCK_MENU: {
      if (socket.flag & NODE_INTERFACE_SOCKET_MENU_EXPANDED) {
        /* Use a single space when the name is empty to work around a bug with expanded enums. Also
         * see #ui_item_enum_expand_exec. */
        row.prop(socket_props_ptr,
                 "value",
                 ui::ITEM_R_EXPAND,
                 StringRef(name).is_empty() ? " " : name,
                 ICON_NONE);
      }
      else {
        row.prop(socket_props_ptr, "value", UI_ITEM_NONE, name, ICON_NONE);
      }
      break;
    }
    default: {
      row.prop(socket_props_ptr, "value", UI_ITEM_NONE, name, ICON_NONE);
      break;
    }
  }
}

static void draw_interface_root_panel_content(DrawGroupInputsContext &ctx,
                                              ui::Layout &layout,
                                              const bNodeTreeInterfacePanel &interface_panel,
                                              const bool is_mask_input_used)
{
  bool found_image_input = false;
  bool found_mask_input = false;
  for (const bNodeTreeInterfaceItem *item : interface_panel.items()) {
    switch (eNodeTreeInterfaceItemType(item->item_type)) {
      case NODE_INTERFACE_PANEL: {
        const auto &sub_interface_panel = *reinterpret_cast<const bNodeTreeInterfacePanel *>(item);
        draw_interface_panel_as_panel(
            ctx.C,
            layout,
            ctx.properties_ptr,
            sub_interface_panel,
            [&](const bNodeTreeInterfaceSocket &socket) { return ctx.input_is_visible(socket); },
            [&](const bNodeTreeInterfaceSocket &socket) { return ctx.input_is_active(socket); },
            [&](ui::Layout &layout,
                const bNodeTreeInterfaceSocket &socket,
                PointerRNA *socket_props_ptr,
                const std::optional<StringRef> parent_name) {
              draw_property_for_socket(ctx, layout, socket, socket_props_ptr, parent_name);
            });
        break;
      }
      case NODE_INTERFACE_SOCKET: {
        const auto &interface_socket = *reinterpret_cast<const bNodeTreeInterfaceSocket *>(item);
        const bke::bNodeSocketType *typeinfo = interface_socket.socket_typeinfo();
        const eNodeSocketDatatype socket_type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
        if (interface_socket.flag & NODE_INTERFACE_SOCKET_INPUT) {
          /* Don't draw the first color input. It's the strip input. */
          if (!found_image_input && socket_type == SOCK_RGBA) {
            found_image_input = true;
          }
          /* Don't draw the second color input if the mask input is used. */
          else if (is_mask_input_used && !found_mask_input && socket_type == SOCK_RGBA) {
            found_mask_input = true;
          }
          else if (!(interface_socket.flag & NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER)) {
            PointerRNA inputs_ptr = RNA_pointer_get(ctx.properties_ptr, "inputs");
            PointerRNA socket_props_ptr = RNA_pointer_get(&inputs_ptr,
                                                          interface_socket.identifier);
            draw_property_for_socket(
                ctx, layout, interface_socket, &socket_props_ptr, std::nullopt);
          }
        }
        break;
      }
    }
  }
}

static void draw_mask_input_type_settings(const bContext &C, ui::Layout &layout, PointerRNA *ptr)
{
  Scene *sequencer_scene = CTX_data_sequencer_scene(&C);
  Editing *ed = seq::editing_get(sequencer_scene);

  const int input_mask_type = RNA_enum_get(ptr, "input_mask_type");

  layout.use_property_split_set(true);

  ui::Layout &col = layout.column(false);
  ui::Layout *row = &col.row(true);
  row->prop(ptr, "input_mask_type", ui::ITEM_R_EXPAND, IFACE_("Type"), ICON_NONE);

  if (input_mask_type == STRIP_MASK_INPUT_STRIP) {
    PointerRNA sequences_object = RNA_pointer_create_discrete(
        &sequencer_scene->id, RNA_SequenceEditor, ed);
    col.prop_search(
        ptr, "input_mask_strip", &sequences_object, "strips_all", IFACE_("Mask"), ICON_NONE);
  }
  else {
    col.prop(ptr, "input_mask_id", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    row = &col.row(true);
    row->prop(ptr, "mask_time", ui::ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  }
}

static void draw_error_message(const bNodeTree &tree, ui::Layout &layout, const bool is_mask_used)
{
  const Span<const bNodeTreeInterfaceSocket *> interface_inputs = tree.interface_inputs();
  const Span<const bNodeTreeInterfaceSocket *> interface_ouputs = tree.interface_outputs();

  if (interface_inputs.size() > 0) {
    const bke::bNodeSocketType *typeinfo = interface_inputs[0]->socket_typeinfo();
    const eNodeSocketDatatype socket_type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
    if (socket_type != SOCK_RGBA) {
      ui::Layout &row = layout.row(false);
      row.label(RPT_("The first node group input must have the Color type"), ICON_ERROR);
    }
  }

  if (is_mask_used) {
    if (interface_inputs.size() < 1) {
      ui::Layout &row = layout.row(false);
      row.label(RPT_("Node group must have at least two inputs to use the mask input"),
                ICON_ERROR);
    }
    const bke::bNodeSocketType *typeinfo = interface_inputs[1]->socket_typeinfo();
    const eNodeSocketDatatype socket_type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
    if (socket_type != SOCK_RGBA) {
      ui::Layout &row = layout.row(false);
      row.label(RPT_("The second node group input must have the Color type"), ICON_ERROR);
    }
  }

  if (interface_ouputs.is_empty()) {
    ui::Layout &row = layout.row(false);
    row.label(RPT_("Node group must have an output"), ICON_ERROR);
  }
  else {
    const bke::bNodeSocketType *typeinfo = interface_ouputs[0]->socket_typeinfo();
    const eNodeSocketDatatype socket_type = typeinfo ? typeinfo->type : SOCK_CUSTOM;
    if (socket_type != SOCK_RGBA) {
      ui::Layout &row = layout.row(false);
      row.label(RPT_("The first node group output must have the Color type"), ICON_ERROR);
    }
  }
}

void draw_compositor_nodes_modifier_ui(const bContext &C,
                                       PointerRNA *modifier_ptr,
                                       ui::Layout &layout)
{
  Main *bmain = CTX_data_main(&C);
  PointerRNA bmain_ptr = RNA_main_pointer_create(bmain);
  SequencerCompositorModifierData &cmd = *modifier_ptr->data_as<SequencerCompositorModifierData>();
  PointerRNA properties_ptr = RNA_pointer_get(modifier_ptr, "properties");
  DrawGroupInputsContext ctx{C, cmd.node_group, &properties_ptr, &bmain_ptr};

  layout.use_property_split_set(true);

  if ((cmd.flag & SEQ_COMP_MOD_HIDE_DATABLOCK_SELECTOR) == 0) {
    const char *newop = (cmd.node_group == nullptr) ?
                            "node.new_compositor_sequencer_node_group" :
                            "node.duplicate_compositing_modifier_node_group";
    template_id(&layout, &C, modifier_ptr, "node_group", newop, nullptr, nullptr);
  }

  const StripModifierData &smd = cmd.modifier;
  const bool is_mask_used = smd.mask_input_type == STRIP_MASK_INPUT_STRIP ?
                                smd.mask_strip != nullptr :
                                smd.mask_id != nullptr;

  if (cmd.node_group != nullptr && !(ID_MISSING(cmd.node_group))) {
    bNodeTree &tree = *cmd.node_group;
    tree.ensure_interface_cache();
    draw_error_message(tree, layout, is_mask_used);

    ctx.input_usages.reinitialize(tree.interface_inputs().size());
    ctx.output_usages.reinitialize(tree.interface_outputs().size());
    nodes::socket_usage_inference::infer_group_interface_inputs_usage(
        tree, *ctx.properties_ptr, ctx.input_usages, ctx.output_usages);
    draw_interface_root_panel_content(ctx, layout, tree.tree_interface.root_panel, is_mask_used);
  }

  if (ui::Layout *mask_input_layout = layout.panel_prop(
          &C, modifier_ptr, "open_mask_input_panel", IFACE_("Mask Input")))
  {
    draw_mask_input_type_settings(C, *mask_input_layout, modifier_ptr);
  }
}

};  // namespace blender::nodes
