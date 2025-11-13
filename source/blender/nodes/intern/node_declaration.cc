/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_node_declaration.hh"
#include "NOD_socket_declarations.hh"
#include "NOD_socket_declarations_geometry.hh"
#include "NOD_socket_usage_inference.hh"

#include "BLI_assert.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_geometry_fields.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_socket_value.hh"

#include "RNA_access.hh"

namespace blender::nodes {

static void reset_declaration(NodeDeclaration &declaration)
{
  std::destroy_at(&declaration);
  new (&declaration) NodeDeclaration();
}

void build_node_declaration(const bke::bNodeType &typeinfo,
                            NodeDeclaration &r_declaration,
                            const bNodeTree *ntree,
                            const bNode *node)
{
  reset_declaration(r_declaration);
  NodeDeclarationBuilder node_decl_builder{typeinfo, r_declaration, ntree, node};
  typeinfo.declare(node_decl_builder);
  node_decl_builder.finalize();
}

void NodeDeclarationBuilder::build_remaining_anonymous_attribute_relations()
{
  auto is_data_socket_decl = [](const SocketDeclaration *socket_decl) {
    return ELEM(socket_decl->socket_type, SOCK_GEOMETRY, SOCK_BUNDLE, SOCK_CLOSURE);
  };

  Vector<int> data_inputs;
  for (const int i : declaration_.inputs.index_range()) {
    if (is_data_socket_decl(declaration_.inputs[i])) {
      data_inputs.append(i);
    }
  }
  Vector<int> data_outputs;
  for (const int i : declaration_.outputs.index_range()) {
    if (is_data_socket_decl(declaration_.outputs[i])) {
      data_outputs.append(i);
    }
  }

  for (BaseSocketDeclarationBuilder *socket_builder : input_socket_builders_) {
    if (socket_builder->field_on_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int field_input = socket_builder->decl_base_->index;
      for (const int data_input : data_inputs) {
        relations.eval_relations.append({field_input, data_input});
      }
    }
  }
  for (BaseSocketDeclarationBuilder *socket_builder : output_socket_builders_) {
    if (socket_builder->field_on_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int field_output = socket_builder->decl_base_->index;
      for (const int data_output : data_outputs) {
        relations.available_relations.append({field_output, data_output});
      }
    }
    if (socket_builder->reference_pass_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int field_output = socket_builder->decl_base_->index;
      for (const int input_i : declaration_.inputs.index_range()) {
        SocketDeclaration &input_socket_decl = *declaration_.inputs[input_i];
        if (input_socket_decl.input_field_type != InputSocketFieldType::None ||
            ELEM(input_socket_decl.socket_type, SOCK_BUNDLE, SOCK_CLOSURE))
        {
          relations.reference_relations.append({input_i, field_output});
        }
      }
    }
    if (socket_builder->propagate_from_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int data_output = socket_builder->decl_base_->index;
      for (const int data_input : data_inputs) {
        relations.propagate_relations.append({data_input, data_output});
      }
    }
  }
}

void NodeDeclarationBuilder::finalize()
{
  this->build_remaining_anonymous_attribute_relations();
  if (is_function_node_) {
    for (SocketDeclaration *socket_decl : declaration_.inputs) {
      socket_decl->structure_type = StructureType::Dynamic;
    }
    for (SocketDeclaration *socket_decl : declaration_.outputs) {
      socket_decl->structure_type = StructureType::Dynamic;
    }
  }
#ifndef NDEBUG
  declaration_.assert_valid();
#endif
}

NodeDeclarationBuilder::NodeDeclarationBuilder(const bke::bNodeType &typeinfo,
                                               NodeDeclaration &declaration,
                                               const bNodeTree *ntree,
                                               const bNode *node)
    : DeclarationListBuilder(*this, declaration.root_items),
      typeinfo_(typeinfo),
      declaration_(declaration),
      ntree_(ntree),
      node_(node)
{
  /* Unused in release builds, but used for BLI_assert() in debug builds. */
  UNUSED_VARS(typeinfo_);
}

void NodeDeclarationBuilder::use_custom_socket_order(bool enable)
{
  declaration_.use_custom_socket_order = enable;
}

void NodeDeclarationBuilder::allow_any_socket_order(bool enable)
{
  BLI_assert(declaration_.use_custom_socket_order);
  declaration_.allow_any_socket_order = enable;
}

Span<SocketDeclaration *> NodeDeclaration::sockets(eNodeSocketInOut in_out) const
{
  if (in_out == SOCK_IN) {
    return inputs;
  }
  return outputs;
}

namespace anonymous_attribute_lifetime {

std::ostream &operator<<(std::ostream &stream, const RelationsInNode &relations)
{
  stream << "Propagate Relations: " << relations.propagate_relations.size() << "\n";
  for (const PropagateRelation &relation : relations.propagate_relations) {
    stream << "  " << relation.from_geometry_input << " -> " << relation.to_geometry_output
           << "\n";
  }
  stream << "Reference Relations: " << relations.reference_relations.size() << "\n";
  for (const ReferenceRelation &relation : relations.reference_relations) {
    stream << "  " << relation.from_field_input << " -> " << relation.to_field_output << "\n";
  }
  stream << "Eval Relations: " << relations.eval_relations.size() << "\n";
  for (const EvalRelation &relation : relations.eval_relations) {
    stream << "  eval " << relation.field_input << " on " << relation.geometry_input << "\n";
  }
  stream << "Available Relations: " << relations.available_relations.size() << "\n";
  for (const AvailableRelation &relation : relations.available_relations) {
    stream << "  " << relation.field_output << " available on " << relation.geometry_output
           << "\n";
  }
  stream << "Available on None: " << relations.available_on_none.size() << "\n";
  for (const int i : relations.available_on_none) {
    stream << "  output " << i << " available on none\n";
  }
  return stream;
}

}  // namespace anonymous_attribute_lifetime

static void assert_valid_panels_recursive(const NodeDeclaration &node_decl,
                                          const Span<const ItemDeclaration *> items,
                                          Vector<const SocketDeclaration *> &r_flat_inputs,
                                          Vector<const SocketDeclaration *> &r_flat_outputs)
{
  /* Expected item order unless any order is allowed: outputs, inputs, panels. */
  bool found_input = false;
  bool found_panel = false;

  for (const ItemDeclaration *item_decl : items) {
    if (const auto *socket_decl = dynamic_cast<const SocketDeclaration *>(item_decl)) {
      if (socket_decl->in_out == SOCK_IN) {
        BLI_assert(node_decl.allow_any_socket_order || !found_panel);
        found_input = true;
        r_flat_inputs.append(socket_decl);
      }
      else {
        BLI_assert(node_decl.allow_any_socket_order || (!found_input && !found_panel));
        r_flat_outputs.append(socket_decl);
      }
    }
    else if (const auto *panel_decl = dynamic_cast<const PanelDeclaration *>(item_decl)) {
      found_panel = true;
      assert_valid_panels_recursive(node_decl, panel_decl->items, r_flat_inputs, r_flat_outputs);
    }
  }
  UNUSED_VARS(found_input, found_panel);
}

void NodeDeclaration::assert_valid() const
{
  if (!this->use_custom_socket_order) {
    /* Skip validation for conventional socket layouts. Those are reordered in drawing code. */
    return;
  }

  Vector<const SocketDeclaration *> flat_inputs;
  Vector<const SocketDeclaration *> flat_outputs;
  assert_valid_panels_recursive(*this, this->root_items, flat_inputs, flat_outputs);

  BLI_assert(this->inputs.as_span() == flat_inputs);
  BLI_assert(this->outputs.as_span() == flat_outputs);
}

bool NodeDeclaration::matches(const bNode &node) const
{
  const bNodeSocket *current_input = static_cast<bNodeSocket *>(node.inputs.first);
  const bNodeSocket *current_output = static_cast<bNodeSocket *>(node.outputs.first);
  const bNodePanelState *current_panel = node.panel_states_array;
  for (const ItemDeclarationPtr &item_decl : this->all_items) {
    if (const SocketDeclaration *socket_decl = dynamic_cast<const SocketDeclaration *>(
            item_decl.get()))
    {
      switch (socket_decl->in_out) {
        case SOCK_IN:
          if (current_input == nullptr || !socket_decl->matches(*current_input)) {
            return false;
          }
          current_input = current_input->next;
          break;
        case SOCK_OUT:
          if (current_output == nullptr || !socket_decl->matches(*current_output)) {
            return false;
          }
          current_output = current_output->next;
          break;
      }
    }
    else if (const PanelDeclaration *panel_decl = dynamic_cast<const PanelDeclaration *>(
                 item_decl.get()))
    {
      if (!node.panel_states().contains_ptr(current_panel) || !panel_decl->matches(*current_panel))
      {
        return false;
      }
      ++current_panel;
    }
    else if (dynamic_cast<const SeparatorDeclaration *>(item_decl.get()) ||
             dynamic_cast<const LayoutDeclaration *>(item_decl.get()))
    {
      /* Ignored because they don't have corresponding data in DNA. */
    }
    else {
      /* Unknown item type. */
      BLI_assert_unreachable();
    }
  }
  /* If items are left over, some were removed from the declaration. */
  if (current_input != nullptr || current_output != nullptr ||
      node.panel_states().contains_ptr(current_panel))
  {
    return false;
  }
  return true;
}

bNodeSocket &SocketDeclaration::update_or_build(bNodeTree &ntree,
                                                bNode &node,
                                                bNodeSocket &socket) const
{
  /* By default just rebuild. */
  BLI_assert(socket.in_out == this->in_out);
  UNUSED_VARS_NDEBUG(socket);
  return this->build(ntree, node);
}

void SocketDeclaration::set_common_flags(bNodeSocket &socket) const
{
  SET_FLAG_FROM_TEST(socket.flag, hide_value, SOCK_HIDE_VALUE);
  SET_FLAG_FROM_TEST(socket.flag, is_multi_input, SOCK_MULTI_INPUT);
  SET_FLAG_FROM_TEST(socket.flag, !is_available, SOCK_UNAVAIL);
}

bool SocketDeclaration::matches_common_data(const bNodeSocket &socket) const
{
  if (socket.name != this->name) {
    return false;
  }
  if (socket.identifier != this->identifier) {
    return false;
  }
  if (((socket.flag & SOCK_HIDE_VALUE) != 0) != this->hide_value) {
    return false;
  }
  if (((socket.flag & SOCK_MULTI_INPUT) != 0) != this->is_multi_input) {
    return false;
  }
  if (((socket.flag & SOCK_UNAVAIL) != 0) != !this->is_available) {
    return false;
  }
  return true;
}

template<typename Fn>
static bool socket_type_to_static_decl_type(const eNodeSocketDatatype socket_type, Fn &&fn)
{
  switch (socket_type) {
    case SOCK_FLOAT:
      fn(TypeTag<decl::Float>());
      return true;
    case SOCK_VECTOR:
      fn(TypeTag<decl::Vector>());
      return true;
    case SOCK_RGBA:
      fn(TypeTag<decl::Color>());
      return true;
    case SOCK_SHADER:
      fn(TypeTag<decl::Shader>());
      return true;
    case SOCK_BOOLEAN:
      fn(TypeTag<decl::Bool>());
      return true;
    case SOCK_ROTATION:
      fn(TypeTag<decl::Rotation>());
      return true;
    case SOCK_MATRIX:
      fn(TypeTag<decl::Matrix>());
      return true;
    case SOCK_INT:
      fn(TypeTag<decl::Int>());
      return true;
    case SOCK_STRING:
      fn(TypeTag<decl::String>());
      return true;
    case SOCK_GEOMETRY:
      fn(TypeTag<decl::Geometry>());
      return true;
    case SOCK_OBJECT:
      fn(TypeTag<decl::Object>());
      return true;
    case SOCK_IMAGE:
      fn(TypeTag<decl::Image>());
      return true;
    case SOCK_COLLECTION:
      fn(TypeTag<decl::Collection>());
      return true;
    case SOCK_MATERIAL:
      fn(TypeTag<decl::Material>());
      return true;
    case SOCK_MENU:
      fn(TypeTag<decl::Menu>());
      return true;
    case SOCK_BUNDLE:
      fn(TypeTag<decl::Bundle>());
      return true;
    case SOCK_CLOSURE:
      fn(TypeTag<decl::Closure>());
      return true;
    default:
      return false;
  }
}

std::unique_ptr<SocketDeclaration> make_declaration_for_socket_type(
    const eNodeSocketDatatype socket_type)
{
  std::unique_ptr<SocketDeclaration> decl;
  socket_type_to_static_decl_type(socket_type, [&](auto type_tag) {
    using DeclT = typename decltype(type_tag)::type;
    decl = std::make_unique<DeclT>();
  });
  return decl;
}

BaseSocketDeclarationBuilder &DeclarationListBuilder::add_input(
    const eNodeSocketDatatype socket_type, const StringRef name, const StringRef identifier)
{
  BaseSocketDeclarationBuilder *decl = nullptr;
  socket_type_to_static_decl_type(socket_type, [&](auto type_tag) {
    using DeclT = typename decltype(type_tag)::type;
    decl = &this->add_input<DeclT>(name, identifier);
  });
  if (!decl) {
    BLI_assert_unreachable();
    decl = &this->add_input<decl::Float>("", "");
  }
  return *decl;
}

BaseSocketDeclarationBuilder &DeclarationListBuilder::add_input(const eCustomDataType data_type,
                                                                const StringRef name,
                                                                const StringRef identifier)
{
  return this->add_input(*bke::custom_data_type_to_socket_type(data_type), name, identifier);
}

BaseSocketDeclarationBuilder &DeclarationListBuilder::add_output(
    const eNodeSocketDatatype socket_type, const StringRef name, const StringRef identifier)
{
  BaseSocketDeclarationBuilder *decl = nullptr;
  socket_type_to_static_decl_type(socket_type, [&](auto type_tag) {
    using DeclT = typename decltype(type_tag)::type;
    decl = &this->add_output<DeclT>(name, identifier);
  });
  if (!decl) {
    BLI_assert_unreachable();
    decl = &this->add_output<decl::Float>("", "");
  }
  return *decl;
}

BaseSocketDeclarationBuilder &DeclarationListBuilder::add_output(const eCustomDataType data_type,
                                                                 const StringRef name,
                                                                 const StringRef identifier)
{
  return this->add_output(*bke::custom_data_type_to_socket_type(data_type), name, identifier);
}

void DeclarationListBuilder::add_separator()
{
  auto decl_ptr = std::make_unique<SeparatorDeclaration>();
  SeparatorDeclaration &decl = *decl_ptr;
  this->node_decl_builder.declaration_.all_items.append(std::move(decl_ptr));
  this->items.append(&decl);
}

void DeclarationListBuilder::add_default_layout()
{
  BLI_assert(this->node_decl_builder.typeinfo_.draw_buttons);
  this->add_layout([](uiLayout *layout, bContext *C, PointerRNA *ptr) {
    const bNode &node = *static_cast<bNode *>(ptr->data);
    node.typeinfo->draw_buttons(layout, C, ptr);
  });
  static_cast<LayoutDeclaration &>(*this->items.last()).is_default = true;
}

void DeclarationListBuilder::add_layout(
    std::function<void(uiLayout *, bContext *, PointerRNA *)> draw)
{
  auto decl_ptr = std::make_unique<LayoutDeclaration>();
  LayoutDeclaration &decl = *decl_ptr;
  decl.draw = std::move(draw);
  this->node_decl_builder.declaration_.all_items.append(std::move(decl_ptr));
  this->items.append(&decl);
}

PanelDeclarationBuilder &DeclarationListBuilder::add_panel(const StringRef name, int identifier)
{
  auto panel_decl_ptr = std::make_unique<PanelDeclaration>();
  PanelDeclaration &panel_decl = *panel_decl_ptr;
  auto panel_decl_builder_ptr = std::make_unique<PanelDeclarationBuilder>(this->node_decl_builder,
                                                                          panel_decl);
  PanelDeclarationBuilder &panel_decl_builder = *panel_decl_builder_ptr;

  if (identifier >= 0) {
    panel_decl.identifier = identifier;
  }
  else {
    /* Use index as identifier. */
    panel_decl.identifier = this->node_decl_builder.declaration_.all_items.size();
  }
  panel_decl.name = name;
  panel_decl.parent_panel = this->parent_panel_decl;
  panel_decl.index = this->node_decl_builder.declaration_.panels.append_and_get_index(&panel_decl);
  this->node_decl_builder.declaration_.all_items.append(std::move(panel_decl_ptr));
  this->node_decl_builder.panel_builders_.append_and_get_index(std::move(panel_decl_builder_ptr));
  this->items.append(&panel_decl);
  return panel_decl_builder;
}

void PanelDeclaration::build(bNodePanelState &panel) const
{
  panel = {0};
  panel.identifier = this->identifier;
  SET_FLAG_FROM_TEST(panel.flag, this->default_collapsed, NODE_PANEL_COLLAPSED);
}

bool PanelDeclaration::matches(const bNodePanelState &panel) const
{
  return panel.identifier == this->identifier;
}

void PanelDeclaration::update_or_build(const bNodePanelState &old_panel,
                                       bNodePanelState &new_panel) const
{
  build(new_panel);
  /* Copy existing state to the new panel */
  SET_FLAG_FROM_TEST(new_panel.flag, old_panel.is_collapsed(), NODE_PANEL_COLLAPSED);
}

int PanelDeclaration::depth() const
{
  int count = 0;
  for (const PanelDeclaration *parent = this->parent_panel; parent; parent = parent->parent_panel)
  {
    count++;
  }
  return count;
}

const nodes::SocketDeclaration *PanelDeclaration::panel_input_decl() const
{
  if (this->items.is_empty()) {
    return nullptr;
  }
  const nodes::ItemDeclaration *item_decl = this->items.first();
  if (const auto *socket_decl = dynamic_cast<const nodes::SocketDeclaration *>(item_decl)) {
    if (socket_decl->is_panel_toggle && (socket_decl->in_out & SOCK_IN) &&
        (socket_decl->socket_type & SOCK_BOOLEAN))
    {
      return socket_decl;
    }
  }
  return nullptr;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::supports_field()
{
  BLI_assert(this->is_input());
  decl_base_->input_field_type = InputSocketFieldType::IsSupported;
  this->structure_type(StructureType::Field);
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::dependent_field(
    Vector<int> input_dependencies)
{
  BLI_assert(this->is_output());
  this->reference_pass(input_dependencies);
  decl_base_->output_field_dependency = OutputFieldDependency::ForPartiallyDependentField(
      std::move(input_dependencies));
  this->structure_type(StructureType::Dynamic);
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::optional_label(bool value)
{
  decl_base_->optional_label = value;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::hide_value(bool value)
{
  decl_base_->hide_value = value;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::multi_input(bool value)
{
  BLI_assert(this->is_input());
  decl_base_->is_multi_input = value;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::compact(bool value)
{
  decl_base_->compact = value;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::reference_pass(
    const Span<int> input_indices)
{
  BLI_assert(this->is_output());
  aal::RelationsInNode &relations = node_decl_builder_->get_anonymous_attribute_relations();
  for (const int from_input : input_indices) {
    aal::ReferenceRelation relation;
    relation.from_field_input = from_input;
    relation.to_field_output = decl_base_->index;
    relations.reference_relations.append(relation);
  }
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::field_on(const Span<int> indices)
{
  aal::RelationsInNode &relations = node_decl_builder_->get_anonymous_attribute_relations();
  if (this->is_input()) {
    this->supports_field();
    for (const int input_index : indices) {
      aal::EvalRelation relation;
      relation.field_input = decl_base_->index;
      relation.geometry_input = input_index;
      relations.eval_relations.append(relation);
    }
  }
  else {
    this->field_source();
    for (const int output_index : indices) {
      aal::AvailableRelation relation;
      relation.field_output = decl_base_->index;
      relation.geometry_output = output_index;
      relations.available_relations.append(relation);
    }
  }
  this->structure_type(StructureType::Field);
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::label_fn(CustomSocketLabelFn fn)
{
  decl_base_->label_fn = std::make_unique<CustomSocketLabelFn>(std::move(fn));
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::short_label(std::string value)
{
  decl_base_->short_label = std::move(value);
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::description(std::string value)
{
  decl_base_->description = std::move(value);
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::translation_context(
    std::optional<std::string> value)
{
  decl_base_->translation_context = std::move(value);
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::no_muted_links(bool value)
{
  decl_base_->no_mute_links = value;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::available(bool value)
{
  decl_base_->is_available = value;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::is_attribute_name(bool value)
{
  decl_base_->is_attribute_name = value;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::is_default_link_socket(bool value)
{
  decl_base_->is_default_link_socket = value;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::default_input_type(
    const NodeDefaultInputType value)
{
  decl_base_->default_input_type = value;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::field_on_all()
{
  if (this->is_input()) {
    this->supports_field();
  }
  if (this->is_output()) {
    this->field_source();
  }
  field_on_all_ = true;
  this->structure_type(StructureType::Field);
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::field_source()
{
  BLI_assert(this->is_output());
  decl_base_->output_field_dependency = OutputFieldDependency::ForFieldSource();
  this->structure_type(StructureType::Field);
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::implicit_field(
    const NodeDefaultInputType default_input_type)
{
  BLI_assert(this->is_input());
  this->hide_value();
  this->structure_type(StructureType::Dynamic);
  decl_base_->input_field_type = InputSocketFieldType::Implicit;
  decl_base_->default_input_type = default_input_type;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::implicit_field_on_all(
    const NodeDefaultInputType default_input_type)
{
  this->implicit_field(default_input_type);
  field_on_all_ = true;
  this->structure_type(StructureType::Field);
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::implicit_field_on(
    const NodeDefaultInputType default_input_type, const Span<int> input_indices)
{
  this->field_on(input_indices);
  this->implicit_field(default_input_type);
  this->structure_type(StructureType::Field);
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::dependent_field()
{
  BLI_assert(this->is_output());
  decl_base_->output_field_dependency = OutputFieldDependency::ForDependentField();
  this->structure_type(StructureType::Dynamic);
  this->reference_pass_all();
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::field_source_reference_all()
{
  this->field_source();
  this->reference_pass_all();
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::reference_pass_all()
{
  reference_pass_all_ = true;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::propagate_all()
{
  propagate_from_all_ = true;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::propagate_all_instance_attributes()
{
  /* We can't distinguish between actually propagating everything or just instance attributes
   * currently. It's still nice to be more explicit at the node declaration level. */
  this->propagate_all();
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::compositor_realization_mode(
    CompositorInputRealizationMode value)
{
  decl_base_->compositor_realization_mode_ = value;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::compositor_domain_priority(
    int priority)
{
  BLI_assert(priority >= 0);
  decl_base_->compositor_domain_priority_ = priority;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::make_available(
    std::function<void(bNode &)> fn)
{
  decl_base_->make_available_fn_ = std::move(fn);
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::custom_draw(CustomSocketDrawFn fn)
{
  decl_base_->custom_draw_fn = std::make_unique<CustomSocketDrawFn>(std::move(fn));
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::usage_inference(
    SocketUsageInferenceFn fn)
{
  decl_base_->usage_inference_fn = std::make_unique<SocketUsageInferenceFn>(std::move(fn));
  return *this;
}

static const bNodeSocket &find_single_menu_input(const bNode &node)
{
#ifndef NDEBUG
  int menu_input_count = 0;
  /* Topology cache may not be available here and this function may be called while doing tree
   * modifications. */
  LISTBASE_FOREACH (bNodeSocket *, socket, &node.inputs) {
    if (socket->type == SOCK_MENU) {
      menu_input_count++;
    }
  }
  BLI_assert(menu_input_count == 1);
#endif

  LISTBASE_FOREACH (bNodeSocket *, socket, &node.inputs) {
    if (!socket->is_available()) {
      continue;
    }
    if (socket->type != SOCK_MENU) {
      continue;
    }
    return *socket;
  }
  BLI_assert_unreachable();
  return node.input_socket(0);
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::usage_by_single_menu(
    const int menu_value)
{
  this->make_available([menu_value](bNode &node) {
    bNodeSocket &socket = const_cast<bNodeSocket &>(find_single_menu_input(node));
    bNodeSocketValueMenu *value = socket.default_value_typed<bNodeSocketValueMenu>();
    value->value = menu_value;
  });
  this->usage_inference([menu_value](const socket_usage_inference::SocketUsageParams &params)
                            -> std::optional<bool> {
    const bNodeSocket &socket = find_single_menu_input(params.node);
    if (params.socket.is_input()) {
      if (const std::optional<bool> any_output_used = params.any_output_is_used()) {
        if (!*any_output_used) {
          /* If no output is used, none of the inputs is used either. */
          return false;
        }
      }
      else {
        /* It's not known if any output is used yet. This function will be called again once new
         * information about output usages is available. */
        return std::nullopt;
      }
    }
    return params.menu_input_may_be(socket.identifier, menu_value);
  });
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::usage_by_menu(
    const StringRef menu_input_identifier, const int menu_value)
{
  Array<int> menu_values = {menu_value};
  this->usage_by_menu(menu_input_identifier, menu_values);
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::usage_by_menu(
    const StringRef menu_input_identifier, const Array<int> menu_values)
{
  this->make_available([menu_input_identifier, menu_values](bNode &node) {
    bNodeSocket &menu_socket = *blender::bke::node_find_socket(
        node, SOCK_IN, menu_input_identifier);
    const SocketDeclaration &socket_declaration = *menu_socket.runtime->declaration;
    socket_declaration.make_available(node);
    bNodeSocketValueMenu *value = menu_socket.default_value_typed<bNodeSocketValueMenu>();
    value->value = menu_values[0];
  });
  this->usage_inference(
      [menu_input_identifier, menu_values](
          const socket_usage_inference::SocketUsageParams &params) -> std::optional<bool> {
        if (params.socket.is_input()) {
          if (const std::optional<bool> any_output_used = params.any_output_is_used()) {
            if (!*any_output_used) {
              /* If no output is used, none of the inputs is used either. */
              return false;
            }
          }
          else {
            /* It's not known if any output is used yet. This function will be called again once
             * new information about output usages is available. */
            return std::nullopt;
          }
        }

        /* Check if the menu might be any of the given values. */
        bool menu_might_be_any_value = false;
        for (const int menu_value : menu_values) {
          menu_might_be_any_value = params.menu_input_may_be(menu_input_identifier, menu_value);
          if (menu_might_be_any_value) {
            break;
          }
        }

        const bNodeSocket &menu_socket = *blender::bke::node_find_socket(
            params.node, SOCK_IN, menu_input_identifier);
        const SocketDeclaration &menu_socket_declaration = *menu_socket.runtime->declaration;
        if (!menu_socket_declaration.usage_inference_fn) {
          return menu_might_be_any_value;
        }

        /* If the menu socket has a usage inference function, check if it might be used. */
        const std::optional<bool> menu_might_be_used =
            (*menu_socket_declaration.usage_inference_fn)(params);
        if (!menu_might_be_used.has_value()) {
          return menu_might_be_any_value;
        }

        /* The input is only used if the menu might be any of the values and the menu itself is
         * used. */
        return *menu_might_be_used && menu_might_be_any_value;
      });
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::align_with_previous(const bool value)
{
  decl_base_->align_with_previous_socket = value;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::structure_type(
    const StructureType structure_type)
{
  BLI_assert(NodeSocketInterfaceStructureType(structure_type) !=
             NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO);
  decl_base_->structure_type = structure_type;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder ::socket_name_ptr(
    const PointerRNA ptr, const StringRef property_name)
{
  decl_base_->socket_name_rna = std::make_unique<SocketNameRNA>();
  decl_base_->socket_name_rna->owner = ptr;
  decl_base_->socket_name_rna->property_name = property_name;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::socket_name_ptr(
    const ID *id, const StructRNA *srna, const void *data, StringRef property_name)
{
  /* Doing const-casts here because this data is generally only available as const when creating
   * the declaration, but it's still valid to modify later. */
  return this->socket_name_ptr(RNA_pointer_create_discrete(const_cast<ID *>(id),
                                                           const_cast<StructRNA *>(srna),
                                                           const_cast<void *>(data)),
                               property_name);
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::panel_toggle(const bool value)
{
  decl_base_->is_panel_toggle = value;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::is_layer_name(const bool value)
{
  decl_base_->is_layer_name = value;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::is_volume_grid_name(const bool value)
{
  decl_base_->is_volume_grid_name = value;
  return *this;
}

OutputFieldDependency OutputFieldDependency::ForFieldSource()
{
  OutputFieldDependency field_dependency;
  field_dependency.type_ = OutputSocketFieldType::FieldSource;
  return field_dependency;
}

OutputFieldDependency OutputFieldDependency::ForDataSource()
{
  OutputFieldDependency field_dependency;
  field_dependency.type_ = OutputSocketFieldType::None;
  return field_dependency;
}

OutputFieldDependency OutputFieldDependency::ForDependentField()
{
  OutputFieldDependency field_dependency;
  field_dependency.type_ = OutputSocketFieldType::DependentField;
  return field_dependency;
}

OutputFieldDependency OutputFieldDependency::ForPartiallyDependentField(Vector<int> indices)
{
  OutputFieldDependency field_dependency;
  if (indices.is_empty()) {
    field_dependency.type_ = OutputSocketFieldType::None;
  }
  else {
    field_dependency.type_ = OutputSocketFieldType::PartiallyDependent;
    field_dependency.linked_input_indices_ = std::move(indices);
  }
  return field_dependency;
}

OutputSocketFieldType OutputFieldDependency::field_type() const
{
  return type_;
}

Span<int> OutputFieldDependency::linked_input_indices() const
{
  return linked_input_indices_;
}

const CompositorInputRealizationMode &SocketDeclaration::compositor_realization_mode() const
{
  return compositor_realization_mode_;
}

int SocketDeclaration::compositor_domain_priority() const
{
  return compositor_domain_priority_;
}

void SocketDeclaration::make_available(bNode &node) const
{
  if (make_available_fn_) {
    make_available_fn_(node);
  }
}

PanelDeclarationBuilder &PanelDeclarationBuilder::description(std::string value)
{
  decl_->description = std::move(value);
  return *this;
}

PanelDeclarationBuilder &PanelDeclarationBuilder::translation_context(
    std::optional<std::string> value)
{
  decl_->translation_context = value;
  return *this;
}

PanelDeclarationBuilder &PanelDeclarationBuilder::default_closed(bool closed)
{
  decl_->default_collapsed = closed;
  return *this;
}

namespace implicit_field_inputs {

static void position(const bNode & /*node*/, void *r_value)
{
  bke::SocketValueVariant::ConstructIn(r_value,
                                       bke::AttributeFieldInput::from<float3>("position"));
}

static void normal(const bNode & /*node*/, void *r_value)
{
  bke::SocketValueVariant::ConstructIn(
      r_value, fn::Field<float3>(std::make_shared<bke::NormalFieldInput>()));
}

static void index(const bNode & /*node*/, void *r_value)
{
  bke::SocketValueVariant::ConstructIn(r_value,
                                       fn::Field<int>(std::make_shared<fn::IndexFieldInput>()));
}

static void id_or_index(const bNode & /*node*/, void *r_value)
{
  bke::SocketValueVariant::ConstructIn(
      r_value, fn::Field<int>(std::make_shared<bke::IDAttributeFieldInput>()));
}

static void instance_transform(const bNode & /*node*/, void *r_value)
{
  bke::SocketValueVariant::ConstructIn(
      r_value, bke::AttributeFieldInput::from<float4x4>("instance_transform"));
}

static void handle_left(const bNode & /*node*/, void *r_value)
{
  bke::SocketValueVariant::ConstructIn(r_value,
                                       bke::AttributeFieldInput::from<float3>("handle_left"));
}

static void handle_right(const bNode & /*node*/, void *r_value)
{
  bke::SocketValueVariant::ConstructIn(r_value,
                                       bke::AttributeFieldInput::from<float3>("handle_right"));
}

}  // namespace implicit_field_inputs

std::optional<ImplicitInputValueFn> get_implicit_input_value_fn(const NodeDefaultInputType type)
{
  switch (type) {
    case NODE_DEFAULT_INPUT_VALUE:
      return std::nullopt;
    case NODE_DEFAULT_INPUT_INDEX_FIELD:
      return std::make_optional(implicit_field_inputs::index);
    case NODE_DEFAULT_INPUT_ID_INDEX_FIELD:
      return std::make_optional(implicit_field_inputs::id_or_index);
    case NODE_DEFAULT_INPUT_NORMAL_FIELD:
      return std::make_optional(implicit_field_inputs::normal);
    case NODE_DEFAULT_INPUT_POSITION_FIELD:
      return std::make_optional(implicit_field_inputs::position);
    case NODE_DEFAULT_INPUT_INSTANCE_TRANSFORM_FIELD:
      return std::make_optional(implicit_field_inputs::instance_transform);
    case NODE_DEFAULT_INPUT_HANDLE_LEFT_FIELD:
      return std::make_optional(implicit_field_inputs::handle_left);
    case NODE_DEFAULT_INPUT_HANDLE_RIGHT_FIELD:
      return std::make_optional(implicit_field_inputs::handle_right);
  }
  return std::nullopt;
}

bool socket_type_supports_default_input_type(const bke::bNodeSocketType &socket_type,
                                             const NodeDefaultInputType input_type)
{
  const eNodeSocketDatatype stype = socket_type.type;
  switch (input_type) {
    case NODE_DEFAULT_INPUT_VALUE:
      return true;
    case NODE_DEFAULT_INPUT_ID_INDEX_FIELD:
    case NODE_DEFAULT_INPUT_INDEX_FIELD:
      return stype == SOCK_INT;
    case NODE_DEFAULT_INPUT_NORMAL_FIELD:
    case NODE_DEFAULT_INPUT_POSITION_FIELD:
    case NODE_DEFAULT_INPUT_HANDLE_LEFT_FIELD:
    case NODE_DEFAULT_INPUT_HANDLE_RIGHT_FIELD:
      return stype == SOCK_VECTOR;
    case NODE_DEFAULT_INPUT_INSTANCE_TRANSFORM_FIELD:
      return stype == SOCK_MATRIX;
  }
  return false;
}

void CustomSocketDrawParams::draw_standard(uiLayout &layout,
                                           const std::optional<StringRefNull> label_override)
{
  this->socket.typeinfo->draw(const_cast<bContext *>(&this->C),
                              &layout,
                              &this->socket_ptr,
                              &this->node_ptr,
                              label_override.has_value() ? *label_override : this->label);
}

}  // namespace blender::nodes
