/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Template for building the panel layout for the scene compositor effects.
 */

#include <fmt/format.h>

#include "BLI_listbase.hh"
#include "BLI_string.hh"
#include "BLI_string_utf8.hh"

#include "BLT_translation.hh"

#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "WM_api.hh"

#include "BKE_compute_contexts.hh"
#include "BKE_context.hh"
#include "BKE_scene_runtime.hh"
#include "BKE_screen.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "NOD_caller_ui.hh"
#include "NOD_eval_log.hh"
#include "NOD_socket_usage_inference.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::ui {

static void draw_effect_extra_menu(bContext *C, ui::Layout *layout, void *effect_v)
{
  Scene *scene = CTX_data_scene(C);
  SceneCompositorEffect *effect = static_cast<SceneCompositorEffect *>(effect_v);

  {
    PointerRNA operator_ptr = layout->op("SCENE_OT_duplicate_compositor_effect",
                                         CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Duplicate"),
                                         ICON_DUPLICATE);
    RNA_string_set(&operator_ptr, "name", effect->name);
  }

  layout->separator();

  {
    ui::Layout &row = layout->row(false);
    PointerRNA operator_ptr = row.op("SCENE_OT_move_compositor_effect_to_index",
                                     IFACE_("Move to First"),
                                     ICON_TRIA_UP,
                                     wm::OpCallContext::InvokeDefault,
                                     UI_ITEM_NONE);
    RNA_string_set(&operator_ptr, "name", effect->name);
    RNA_int_set(&operator_ptr, "index", 0);
    row.enabled_set(effect->previous != nullptr);
  }

  {
    ui::Layout &row = layout->row(false);
    PointerRNA operator_ptr = row.op("SCENE_OT_move_compositor_effect_to_index",
                                     IFACE_("Move to Last"),
                                     ICON_TRIA_DOWN,
                                     wm::OpCallContext::InvokeDefault,
                                     UI_ITEM_NONE);
    RNA_string_set(&operator_ptr, "name", effect->name);
    RNA_int_set(&operator_ptr, "index", scene->compositor_effects.count() - 1);
    row.enabled_set(effect->next != nullptr);
  }

  layout->separator();

  PointerRNA effect_ptr = RNA_pointer_create_discrete(
      &scene->id, RNA_SceneCompositorEffect, effect);
  layout->prop(&effect_ptr, "show_node_group_selector", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void draw_effect_panel_header(const bContext * /*C*/, Panel *panel)
{
  ui::Layout &layout = *panel->layout;

  PointerRNA *effect_ptr = ui::panel_custom_data_get(panel);
  ui::panel_context_pointer_set(panel, "effect", effect_ptr);
  SceneCompositorEffect *effect = effect_ptr->data_as<SceneCompositorEffect>();

  ui::Layout &icon_row = layout.row(true);
  icon_row.emboss_set(ui::EmbossType::None);
  if (!effect->node_group || ID_MISSING(effect->node_group)) {
    icon_row.red_alert_set(true);
  }
  PointerRNA set_active_operator_ptr = icon_row.op(
      "SCENE_OT_set_active_compositor_effect", "", ICON_NODE_COMPOSITING);
  RNA_string_set(&set_active_operator_ptr, "name", effect->name);

  ui::Layout &buttons_row = layout.row(true);
  ui::Layout &name_row = buttons_row.row(true);

  constexpr int number_of_buttons = 3;
  const int available_space_for_name = (panel->sizex / UI_UNIT_X) - number_of_buttons;
  const bool is_panel_drawn_for_first_time = panel->sizex == 0;
  if (is_panel_drawn_for_first_time || available_space_for_name > 5) {
    name_row.prop(effect_ptr, "name", UI_ITEM_NONE, "", ICON_NONE);
  }
  else {
    buttons_row.alignment_set(ui::LayoutAlign::Right);
  }

  ui::Layout &enable_for_preview_row = buttons_row.row(true);
  enable_for_preview_row.prop(effect_ptr, "enable_for_preview", UI_ITEM_NONE, "", ICON_NONE);

  ui::Layout &enable_for_render_row = buttons_row.row(true);
  enable_for_render_row.prop(effect_ptr, "enable_for_render", UI_ITEM_NONE, "", ICON_NONE);

  buttons_row.menu_fn("", ICON_DOWNARROW_HLT, draw_effect_extra_menu, effect);

  ui::Layout &remove_row = buttons_row.row(false);
  remove_row.emboss_set(ui::EmbossType::None);
  PointerRNA remove_operator_ptr = remove_row.op("SCENE_OT_remove_compositor_effect", "", ICON_X);
  RNA_string_set(&remove_operator_ptr, "name", effect->name);

  layout.separator();
}

static void reorder_effect(bContext *C, Panel *panel, const int new_index)
{
  PointerRNA *effect_ptr = ui::panel_custom_data_get(panel);
  SceneCompositorEffect *effect = effect_ptr->data_as<SceneCompositorEffect>();

  wmOperatorType *operator_type = WM_operatortype_find("SCENE_OT_move_compositor_effect_to_index",
                                                       false);
  PointerRNA properties_ptr = WM_operator_properties_create_ptr(operator_type);
  RNA_string_set(&properties_ptr, "name", effect->name);
  RNA_int_set(&properties_ptr, "index", new_index);
  WM_operator_name_call_ptr(
      C, operator_type, wm::OpCallContext::InvokeDefault, &properties_ptr, nullptr);
  WM_operator_properties_free(&properties_ptr);
}

static short get_effect_expand_flag(const bContext * /*C*/, Panel *panel)
{
  PointerRNA *effect_ptr = ui::panel_custom_data_get(panel);
  SceneCompositorEffect *effect = effect_ptr->data_as<SceneCompositorEffect>();
  return effect->ui_panel_data_expansion;
}

static void set_effect_expand_flag(const bContext * /*C*/, Panel *panel, short expand_flag)
{
  PointerRNA *effect_ptr = ui::panel_custom_data_get(panel);
  SceneCompositorEffect *effect = effect_ptr->data_as<SceneCompositorEffect>();
  effect->ui_panel_data_expansion = uiPanelDataExpansion(expand_flag);
}

/* Drawing the properties manually with #ui::Layout::prop instead of #uiDefAutoButsRNA allows using
 * the node socket identifier for the property names, since they are unique, but also having
 * the correct label displayed in the UI. */
static void draw_property_for_socket(
    const bContext &C,
    ui::Layout &layout,
    const bNodeTreeInterfaceSocket &socket,
    PointerRNA &input_ptr,
    const bNodeTree &node_group,
    Array<nodes::socket_usage_inference::SocketUsage> &input_usages)
{
  if (!input_usages[node_group.interface_input_index(socket)].is_visible) {
    /* The input is not used currently, but it would be used if any menu input is changed.
     * By convention, the input is hidden in this case instead of just grayed out. */
    return;
  }

  ui::Layout &row = layout.row(true);
  row.use_property_decorate_set(true);
  row.active_set(input_usages[node_group.interface_input_index(socket)].is_used);

  const bke::bNodeSocketType *typeinfo = socket.socket_typeinfo();
  const eNodeSocketDatatype type = typeinfo ? typeinfo->type : SOCK_CUSTOM;

  if (!typeinfo->make_scene_compositor_effect_input_srna) {
    return;
  }

  std::string name = socket.name ? IFACE_(socket.name) : "";

  switch (type) {
    case SOCK_OBJECT: {
      /* Use #ui::Layout::prop_search to draw pointer properties because #ui::Layout::prop would
       * not have enough information about what type of ID to select for editing the values. This
       * is because pointer IDProperties contain no information about their type. */
      Main *bmain = CTX_data_main(&C);
      PointerRNA bmain_ptr = RNA_main_pointer_create(bmain);
      row.prop_search(&input_ptr, "value", &bmain_ptr, "objects", name, ICON_OBJECT_DATA);
      break;
    }
    case SOCK_MENU: {
      if (socket.flag & NODE_INTERFACE_SOCKET_MENU_EXPANDED) {
        /* Use a single space when the name is empty to work around a bug with expanded enums. Also
         * see #ui_item_enum_expand_exec. */
        row.prop(&input_ptr,
                 "value",
                 ui::ITEM_R_EXPAND,
                 StringRef(name).is_empty() ? " " : name,
                 ICON_NONE);
      }
      else {
        row.prop(&input_ptr, "value", UI_ITEM_NONE, name, ICON_NONE);
      }
      break;
    }
    case SOCK_FONT: {
      template_id(&row,
                  &C,
                  &input_ptr,
                  "value",
                  nullptr,
                  "FONT_OT_open",
                  "FONT_OT_unlink",
                  ui::TEMPLATE_ID_FILTER_ALL,
                  false,
                  name);
      break;
    }
    default: {
      row.prop(&input_ptr, "value", UI_ITEM_NONE, name, ICON_NONE);
      break;
    }
  }
}

static void draw_effect_inputs(const bContext &C, PointerRNA &effect_ptr, ui::Layout &layout)
{
  SceneCompositorEffect &effect = *effect_ptr.data_as<SceneCompositorEffect>();
  PointerRNA properties_ptr = RNA_pointer_get(&effect_ptr, "properties");

  effect.node_group->ensure_interface_cache();
  Array<nodes::socket_usage_inference::SocketUsage> input_usages;
  input_usages.reinitialize(effect.node_group->interface_inputs().size());
  nodes::socket_usage_inference::infer_group_interface_inputs_usage(
      *effect.node_group, properties_ptr, input_usages);

  for (const bNodeTreeInterfaceItem *item : effect.node_group->tree_interface.root_panel.items()) {
    switch (item->item_type) {
      case NodeTreeInterfaceItemType::Panel: {
        const auto &sub_interface_panel = *reinterpret_cast<const bNodeTreeInterfacePanel *>(item);
        nodes::draw_interface_panel_as_panel(
            C,
            layout,
            &properties_ptr,
            sub_interface_panel,
            [&](const bNodeTreeInterfaceSocket &socket) {
              return input_usages[effect.node_group->interface_input_index(socket)].is_visible;
            },
            [&](const bNodeTreeInterfaceSocket &socket) {
              return input_usages[effect.node_group->interface_input_index(socket)].is_used;
            },
            [&](ui::Layout &layout,
                const bNodeTreeInterfaceSocket &socket,
                PointerRNA *input_ptr,
                const std::optional<StringRef> /*parent_name*/) {
              draw_property_for_socket(
                  C, layout, socket, *input_ptr, *effect.node_group, input_usages);
            });
        break;
      }
      case NodeTreeInterfaceItemType::Socket: {
        const auto &socket = *reinterpret_cast<const bNodeTreeInterfaceSocket *>(item);
        if (socket.flag & NODE_INTERFACE_SOCKET_INPUT) {
          if (&socket == effect.node_group->interface_inputs().first()) {
          }
          else if (!(socket.flag & NODE_INTERFACE_SOCKET_HIDE_IN_MODIFIER)) {
            PointerRNA inputs_ptr = RNA_pointer_get(&properties_ptr, "inputs");
            PointerRNA input_ptr = RNA_pointer_get(&inputs_ptr, socket.identifier);
            draw_property_for_socket(
                C, layout, socket, input_ptr, *effect.node_group, input_usages);
          }
        }
        break;
      }
    }
  }
}

static nodes::eval_log::NodeTreeLog *get_root_tree_log(const Scene &scene,
                                                       const SceneCompositorEffect &effect)
{
  if (!scene.runtime->compositor.nodes_evaluation_log) {
    return nullptr;
  }
  bke::DataBlockComputeContext data_block_context{nullptr, scene.id};
  bke::SceneCompositorEffectComputeContext modifier_context{&data_block_context, effect};
  return &scene.runtime->compositor.nodes_evaluation_log->get_tree_log(modifier_context.hash());
}

static std::string get_node_warning_panel_name(const int num_errors,
                                               const int num_warnings,
                                               const int num_infos)
{
  fmt::memory_buffer buffer;
  fmt::appender buf = fmt::appender(buffer);
  if (num_errors > 0) {
    fmt::format_to(buf, "{} ({})", IFACE_("Errors"), num_errors);
  }
  if (num_warnings > 0) {
    if (num_errors > 0) {
      fmt::format_to(buf, ", ");
    }
    fmt::format_to(buf, "{} ({})", IFACE_("Warnings"), num_warnings);
  }
  if (num_infos > 0) {
    if (num_errors > 0 || num_warnings > 0) {
      fmt::format_to(buf, ", ");
    }
    fmt::format_to(buf, "{} ({})", IFACE_("Info"), num_infos);
  }
  return std::string(buffer.data(), buffer.size());
}

static void draw_warnings(const bContext *C, ui::Layout &layout, PointerRNA &effect_ptr)
{
  using namespace nodes::eval_log;
  Scene &scene = *id_cast<Scene *>(effect_ptr.owner_id);
  SceneCompositorEffect &effect = *effect_ptr.data_as<SceneCompositorEffect>();
  NodeTreeLog *tree_log = get_root_tree_log(scene, effect);
  if (!tree_log) {
    return;
  }

  tree_log->ensure_node_warnings(*CTX_data_main(C));
  if (tree_log->all_warnings.is_empty()) {
    return;
  }

  Map<nodes::NodeWarningType, int> count_by_type;
  for (const nodes::NodeWarning &warning : tree_log->all_warnings) {
    count_by_type.lookup_or_add(warning.type, 0)++;
  }
  const int num_errors = count_by_type.lookup_default(nodes::NodeWarningType::Error, 0);
  const int num_warnings = count_by_type.lookup_default(nodes::NodeWarningType::Warning, 0);
  const int num_infos = count_by_type.lookup_default(nodes::NodeWarningType::Info, 0);
  const std::string panel_name = get_node_warning_panel_name(num_errors, num_warnings, num_infos);
  ui::Layout *panel = layout.panel(C, "Warnings", true, panel_name);
  if (!panel) {
    return;
  }

  Vector<const nodes::NodeWarning *> warnings(tree_log->all_warnings.size());
  for (const int i : warnings.index_range()) {
    warnings[i] = &tree_log->all_warnings[i];
  }
  std::ranges::sort(warnings, [](const nodes::NodeWarning *a, const nodes::NodeWarning *b) {
    const int severity_a = node_warning_type_severity(a->type);
    const int severity_b = node_warning_type_severity(b->type);
    if (severity_a > severity_b) {
      return true;
    }
    if (severity_a < severity_b) {
      return false;
    }
    return BLI_strcasecmp_natural(a->message.c_str(), b->message.c_str()) < 0;
  });

  ui::Layout &col = panel->column(false);
  ui::Block *block = col.block();
  for (const nodes::NodeWarning *warning : warnings) {
    const int icon = node_warning_type_icon(warning->type);
    const StringRef message = RPT_(warning->message);
    ui::Button *but = uiDefIconTextBut(
        block, ui::ButtonType::Label, icon, message, 0, 0, 1, UI_UNIT_Y, nullptr, std::nullopt);
    /* Add tooltip containing the same message. This is helpful if the message is very long so that
     * it doesn't fit in the panel. */
    button_func_tooltip_set(
        but,
        [](bContext * /*C*/, void *argN, StringRef /*tip*/) -> std::string {
          return *static_cast<std::string *>(argN);
        },
        MEM_new<std::string>(__func__, message),
        [](void *arg) { MEM_delete(static_cast<std::string *>(arg)); });
  }
}

static void draw_effect_panel(const bContext *C, Panel *panel)
{
  PointerRNA *effect_ptr = ui::panel_custom_data_get(panel);
  ui::panel_context_pointer_set(panel, "effect", effect_ptr);

  SceneCompositorEffect &effect = *effect_ptr->data_as<SceneCompositorEffect>();

  ui::Layout &layout = *panel->layout;
  layout.use_property_split_set(true);

  if (flag_is_set(effect.flags, SceneCompositorEffectFlags::ShowNodeGroupSelector)) {
    const char *operator_name = (effect.node_group == nullptr) ?
                                    "scene.new_compositor_effect_node_group" :
                                    "scene.duplicate_compositor_effect_node_group";
    template_id(&layout, C, effect_ptr, "node_group", operator_name, nullptr, nullptr);
  }

  if (effect.node_group && !ID_MISSING(effect.node_group)) {
    draw_effect_inputs(*C, *effect_ptr, layout);
  }

  draw_warnings(C, layout, *effect_ptr);
}

static constexpr char SCENE_COMPOSITOR_EFFECT_PANEL_IDNAME[] = "SCENE_COMPOSITOR_EFFECT_PT";

void register_scene_compositor_effects_panel(ARegionType *region_type)
{
  PanelType *panel_type = MEM_new_zeroed<PanelType>(__func__);

  STRNCPY_UTF8(panel_type->idname, SCENE_COMPOSITOR_EFFECT_PANEL_IDNAME);
  STRNCPY_UTF8(panel_type->label, "");
  STRNCPY_UTF8(panel_type->context, "compositor");
  STRNCPY_UTF8(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  STRNCPY_UTF8(panel_type->active_property, "is_active");

  panel_type->draw_header = draw_effect_panel_header;
  panel_type->draw = draw_effect_panel;

  /* Give the panel the special flag that says it was built here and corresponds to a
   * effect rather than a #PanelType. */
  panel_type->flag = PANEL_TYPE_HEADER_EXPAND | PANEL_TYPE_INSTANCED;
  panel_type->reorder = reorder_effect;
  panel_type->get_list_data_expand_flag = get_effect_expand_flag;
  panel_type->set_list_data_expand_flag = set_effect_expand_flag;

  BLI_addtail(&region_type->paneltypes, panel_type);
}

static void effect_panel_id(void * /*effect_link*/, char *r_name)
{
  BLI_strncpy(r_name, SCENE_COMPOSITOR_EFFECT_PANEL_IDNAME, MAX_NAME);
}

void template_scene_compositor_effects(Layout * /*layout*/, bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  if (!scene) {
    return;
  }
  ListBaseT<SceneCompositorEffect> *effects = &scene->compositor_effects;

  ARegion *region = CTX_wm_region(C);
  const bool panels_match = panel_list_matches_data(region, effects, effect_panel_id);

  if (!panels_match) {
    panels_free_instanced(C, region);
    for (SceneCompositorEffect &effect : *effects) {
      /* Create custom data RNA pointer. */
      PointerRNA *effect_ptr = MEM_new<PointerRNA>(__func__);
      *effect_ptr = RNA_pointer_create_discrete(&scene->id, RNA_SceneCompositorEffect, &effect);

      panel_add_instanced(
          C, region, &region->panels, SCENE_COMPOSITOR_EFFECT_PANEL_IDNAME, effect_ptr);
    }
  }
  else {
    /* Assuming there's only one group of instanced panels, update the custom data pointers. */
    Panel *panel = static_cast<Panel *>(region->panels.first);
    for (SceneCompositorEffect &effect : *effects) {
      /* Move to the next instanced panel corresponding to the next effect. */
      while ((panel->type == nullptr) || !(panel->type->flag & PANEL_TYPE_INSTANCED)) {
        panel = panel->next;
        /* There shouldn't be fewer panels than effects with UIs. */
        BLI_assert(panel != nullptr);
      }

      PointerRNA *effect_ptr = MEM_new<PointerRNA>(__func__);
      *effect_ptr = RNA_pointer_create_discrete(&scene->id, RNA_SceneCompositorEffect, &effect);
      panel_custom_data_set(panel, effect_ptr);

      panel = panel->next;
    }
  }
}

}  // namespace blender::ui
