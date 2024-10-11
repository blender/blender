/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_node_declaration.hh"
#include "NOD_socket_declarations.hh"
#include "NOD_socket_declarations_geometry.hh"

#include "BLI_stack.hh"
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
    return dynamic_cast<const decl::Geometry *>(socket_decl);
  };

  Vector<int> geometry_inputs;
  for (const int i : declaration_.inputs.index_range()) {
    if (is_data_socket_decl(declaration_.inputs[i])) {
      geometry_inputs.append(i);
    }
  }
  Vector<int> geometry_outputs;
  for (const int i : declaration_.outputs.index_range()) {
    if (is_data_socket_decl(declaration_.outputs[i])) {
      geometry_outputs.append(i);
    }
  }

  for (BaseSocketDeclarationBuilder *socket_builder : input_socket_builders_) {
    if (socket_builder->field_on_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int field_input = socket_builder->decl_base_->index;
      for (const int geometry_input : geometry_inputs) {
        relations.eval_relations.append({field_input, geometry_input});
      }
    }
  }
  for (BaseSocketDeclarationBuilder *socket_builder : output_socket_builders_) {
    if (socket_builder->field_on_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int field_output = socket_builder->decl_base_->index;
      for (const int geometry_output : geometry_outputs) {
        relations.available_relations.append({field_output, geometry_output});
      }
    }
    if (socket_builder->reference_pass_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int field_output = socket_builder->decl_base_->index;
      for (const int input_i : declaration_.inputs.index_range()) {
        SocketDeclaration &input_socket_decl = *declaration_.inputs[input_i];
        if (input_socket_decl.input_field_type != InputSocketFieldType::None) {
          relations.reference_relations.append({input_i, field_output});
        }
      }
    }
    if (socket_builder->propagate_from_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int geometry_output = socket_builder->decl_base_->index;
      for (const int geometry_input : geometry_inputs) {
        relations.propagate_relations.append({geometry_input, geometry_output});
      }
    }
  }
}

void NodeDeclarationBuilder::finalize()
{
  this->build_remaining_anonymous_attribute_relations();
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
  SET_FLAG_FROM_TEST(socket.flag, compact, SOCK_COMPACT);
  SET_FLAG_FROM_TEST(socket.flag, hide_value, SOCK_HIDE_VALUE);
  SET_FLAG_FROM_TEST(socket.flag, hide_label, SOCK_HIDE_LABEL);
  SET_FLAG_FROM_TEST(socket.flag, is_multi_input, SOCK_MULTI_INPUT);
  SET_FLAG_FROM_TEST(socket.flag, no_mute_links, SOCK_NO_INTERNAL_LINK);
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
  if (((socket.flag & SOCK_COMPACT) != 0) != this->compact) {
    return false;
  }
  if (((socket.flag & SOCK_HIDE_VALUE) != 0) != this->hide_value) {
    return false;
  }
  if (((socket.flag & SOCK_HIDE_LABEL) != 0) != this->hide_label) {
    return false;
  }
  if (((socket.flag & SOCK_MULTI_INPUT) != 0) != this->is_multi_input) {
    return false;
  }
  if (((socket.flag & SOCK_NO_INTERNAL_LINK) != 0) != this->no_mute_links) {
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

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::supports_field()
{
  BLI_assert(this->is_input());
  decl_base_->input_field_type = InputSocketFieldType::IsSupported;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::dependent_field(
    Vector<int> input_dependencies)
{
  BLI_assert(this->is_output());
  this->reference_pass(input_dependencies);
  decl_base_->output_field_dependency = OutputFieldDependency::ForPartiallyDependentField(
      std::move(input_dependencies));
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::hide_label(bool value)
{
  decl_base_->hide_label = value;
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

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::translation_context(std::string value)
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

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::field_on_all()
{
  if (this->is_input()) {
    this->supports_field();
  }
  if (this->is_output()) {
    this->field_source();
  }
  field_on_all_ = true;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::field_source()
{
  BLI_assert(this->is_output());
  decl_base_->output_field_dependency = OutputFieldDependency::ForFieldSource();
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::implicit_field(ImplicitInputValueFn fn)
{
  BLI_assert(this->is_input());
  this->hide_value();
  decl_base_->input_field_type = InputSocketFieldType::Implicit;
  decl_base_->implicit_input_fn = std::make_unique<ImplicitInputValueFn>(std::move(fn));
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::implicit_field_on_all(
    ImplicitInputValueFn fn)
{
  this->implicit_field(fn);
  field_on_all_ = true;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::implicit_field_on(
    ImplicitInputValueFn fn, const Span<int> input_indices)
{
  this->field_on(input_indices);
  this->implicit_field(fn);
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::dependent_field()
{
  BLI_assert(this->is_output());
  decl_base_->output_field_dependency = OutputFieldDependency::ForDependentField();
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

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::compositor_realization_options(
    CompositorInputRealizationOptions value)
{
  decl_base_->compositor_realization_options_ = value;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::compositor_domain_priority(
    int priority)
{
  decl_base_->compositor_domain_priority_ = priority;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::compositor_expects_single_value(
    bool value)
{
  decl_base_->compositor_expects_single_value_ = value;
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::make_available(
    std::function<void(bNode &)> fn)
{
  decl_base_->make_available_fn_ = std::move(fn);
  return *this;
}

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::align_with_previous(const bool value)
{
  decl_base_->align_with_previous_socket = value;
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
  return this->socket_name_ptr(RNA_pointer_create(const_cast<ID *>(id),
                                                  const_cast<StructRNA *>(srna),
                                                  const_cast<void *>(data)),
                               property_name);
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

const CompositorInputRealizationOptions &SocketDeclaration::compositor_realization_options() const
{
  return compositor_realization_options_;
}

int SocketDeclaration::compositor_domain_priority() const
{
  return compositor_domain_priority_;
}

bool SocketDeclaration::compositor_expects_single_value() const
{
  return compositor_expects_single_value_;
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

PanelDeclarationBuilder &PanelDeclarationBuilder::default_closed(bool closed)
{
  decl_->default_collapsed = closed;
  return *this;
}

namespace implicit_field_inputs {

void position(const bNode & /*node*/, void *r_value)
{
  new (r_value) bke::SocketValueVariant(bke::AttributeFieldInput::Create<float3>("position"));
}

void normal(const bNode & /*node*/, void *r_value)
{
  new (r_value)
      bke::SocketValueVariant(fn::Field<float3>(std::make_shared<bke::NormalFieldInput>()));
}

void index(const bNode & /*node*/, void *r_value)
{
  new (r_value) bke::SocketValueVariant(fn::Field<int>(std::make_shared<fn::IndexFieldInput>()));
}

void id_or_index(const bNode & /*node*/, void *r_value)
{
  new (r_value)
      bke::SocketValueVariant(fn::Field<int>(std::make_shared<bke::IDAttributeFieldInput>()));
}

void instance_transform(const bNode & /*node*/, void *r_value)
{
  new (r_value)
      bke::SocketValueVariant(bke::AttributeFieldInput::Create<float4x4>("instance_transform"));
}

}  // namespace implicit_field_inputs

}  // namespace blender::nodes
