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

namespace blender::nodes {

static void reset_declaration(NodeDeclaration &declaration)
{
  std::destroy_at(&declaration);
  new (&declaration) NodeDeclaration();
}

void build_node_declaration(const bNodeType &typeinfo,
                            NodeDeclaration &r_declaration,
                            const bNodeTree *ntree,
                            const bNode *node)
{
  reset_declaration(r_declaration);
  NodeDeclarationBuilder node_decl_builder{r_declaration, ntree, node};
  typeinfo.declare(node_decl_builder);
  node_decl_builder.finalize();
}

void NodeDeclarationBuilder::finalize()
{
  if (is_function_node_) {
    for (BaseSocketDeclarationBuilder *socket_builder : input_socket_builders_) {
      if (socket_builder->decl_base_->input_field_type != InputSocketFieldType::Implicit) {
        socket_builder->decl_base_->input_field_type = InputSocketFieldType::IsSupported;
      }
    }
    for (BaseSocketDeclarationBuilder *socket_builder : output_socket_builders_) {
      socket_builder->decl_base_->output_field_dependency =
          OutputFieldDependency::ForDependentField();
      socket_builder->reference_pass_all_ = true;
    }
  }

  Vector<int> geometry_inputs;
  for (const int i : declaration_.inputs.index_range()) {
    if (dynamic_cast<decl::Geometry *>(declaration_.inputs[i])) {
      geometry_inputs.append(i);
    }
  }
  Vector<int> geometry_outputs;
  for (const int i : declaration_.outputs.index_range()) {
    if (dynamic_cast<decl::Geometry *>(declaration_.outputs[i])) {
      geometry_outputs.append(i);
    }
  }

  for (BaseSocketDeclarationBuilder *socket_builder : input_socket_builders_) {
    if (socket_builder->field_on_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int field_input = socket_builder->index_;
      for (const int geometry_input : geometry_inputs) {
        relations.eval_relations.append({field_input, geometry_input});
      }
    }
  }
  for (BaseSocketDeclarationBuilder *socket_builder : output_socket_builders_) {
    if (socket_builder->field_on_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int field_output = socket_builder->index_;
      for (const int geometry_output : geometry_outputs) {
        relations.available_relations.append({field_output, geometry_output});
      }
    }
    if (socket_builder->reference_pass_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int field_output = socket_builder->index_;
      for (const int input_i : declaration_.inputs.index_range()) {
        SocketDeclaration &input_socket_decl = *declaration_.inputs[input_i];
        if (input_socket_decl.input_field_type != InputSocketFieldType::None) {
          relations.reference_relations.append({input_i, field_output});
        }
      }
    }
    if (socket_builder->propagate_from_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int geometry_output = socket_builder->index_;
      for (const int geometry_input : geometry_inputs) {
        relations.propagate_relations.append({geometry_input, geometry_output});
      }
    }
  }

  BLI_assert(declaration_.is_valid());
}

NodeDeclarationBuilder::NodeDeclarationBuilder(NodeDeclaration &declaration,
                                               const bNodeTree *ntree,
                                               const bNode *node)
    : declaration_(declaration), ntree_(ntree), node_(node)
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

void NodeDeclarationBuilder::set_active_panel_builder(const PanelDeclarationBuilder *panel_builder)
{
  if (panel_builders_.is_empty()) {
    BLI_assert(panel_builder == nullptr);
    return;
  }

  BLI_assert(!panel_builder || !panel_builder->is_complete_);
  PanelDeclarationBuilder *last_panel_builder = panel_builders_.last().get();
  if (last_panel_builder != panel_builder) {
    last_panel_builder->is_complete_ = true;
  }
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

bool NodeDeclaration::is_valid() const
{
  if (!this->use_custom_socket_order) {
    /* Skip validation for conventional socket layouts. */
    return true;
  }

  /* Validation state for the interface root items as well as any panel content. */
  struct ValidationState {
    /* Remaining number of items expected in a panel */
    int remaining_items = 0;
    /* Sockets first, followed by panels. */
    NodeTreeInterfaceItemType item_type = NODE_INTERFACE_SOCKET;
    /* Output sockets first, followed by input sockets. */
    eNodeSocketInOut socket_in_out = SOCK_OUT;
  };

  Stack<ValidationState> panel_states;
  panel_states.push({});

  for (const ItemDeclarationPtr &item_decl : items) {
    BLI_assert(panel_states.size() >= 1);
    ValidationState &state = panel_states.peek();

    if (const SocketDeclaration *socket_decl = dynamic_cast<const SocketDeclaration *>(
            item_decl.get()))
    {
      if (state.item_type != NODE_INTERFACE_SOCKET && !this->allow_any_socket_order) {
        std::cout << "Socket added after panel" << std::endl;
        return false;
      }

      /* Check for consistent outputs.., inputs.. blocks. */
      if (state.socket_in_out == SOCK_OUT && socket_decl->in_out == SOCK_IN) {
        /* Start of input sockets. */
        state.socket_in_out = SOCK_IN;
      }
      if (socket_decl->in_out != state.socket_in_out && !this->allow_any_socket_order) {
        std::cout << "Output socket added after input socket" << std::endl;
        return false;
      }

      /* Item counting for the panels, but ignore for root items. */
      if (panel_states.size() > 1) {
        if (state.remaining_items <= 0) {
          std::cout << "More sockets than expected in panel" << std::endl;
          return false;
        }
        --state.remaining_items;
        /* Panel closed after last item is added. */
        if (state.remaining_items == 0) {
          panel_states.pop();
        }
      }
    }
    else if (const PanelDeclaration *panel_decl = dynamic_cast<const PanelDeclaration *>(
                 item_decl.get()))
    {
      if (state.item_type == NODE_INTERFACE_SOCKET) {
        /* Start of panels section */
        state.item_type = NODE_INTERFACE_PANEL;
      }
      BLI_assert(state.item_type == NODE_INTERFACE_PANEL);

      if (panel_decl->num_child_decls > 0) {
        /* New panel started. */
        panel_states.push({panel_decl->num_child_decls});
      }
    }
    else {
      BLI_assert_unreachable();
      return false;
    }
  }

  /* All panels complete? */
  if (panel_states.size() != 1) {
    std::cout << "Incomplete last panel" << std::endl;
    return false;
  }
  return true;
}

bool NodeDeclaration::matches(const bNode &node) const
{
  const bNodeSocket *current_input = static_cast<bNodeSocket *>(node.inputs.first);
  const bNodeSocket *current_output = static_cast<bNodeSocket *>(node.outputs.first);
  const bNodePanelState *current_panel = node.panel_states_array;
  for (const ItemDeclarationPtr &item_decl : items) {
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
    else {
      /* Unknown item type. */
      BLI_assert_unreachable();
    }
  }
  /* If items are left over, some were removed from the declaration. */
  if (current_input == nullptr || current_output == nullptr ||
      !node.panel_states().contains_ptr(current_panel))
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
  SET_FLAG_FROM_TEST(socket.flag, is_unavailable, SOCK_UNAVAIL);
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
  if (((socket.flag & SOCK_UNAVAIL) != 0) != this->is_unavailable) {
    return false;
  }
  return true;
}

PanelDeclarationBuilder &NodeDeclarationBuilder::add_panel(StringRef name, int identifier)
{
  std::unique_ptr<PanelDeclaration> panel_decl = std::make_unique<PanelDeclaration>();
  std::unique_ptr<PanelDeclarationBuilder> panel_decl_builder =
      std::make_unique<PanelDeclarationBuilder>();
  panel_decl_builder->decl_ = &*panel_decl;

  panel_decl_builder->node_decl_builder_ = this;
  if (identifier >= 0) {
    panel_decl->identifier = identifier;
  }
  else {
    /* Use index as identifier. */
    panel_decl->identifier = declaration_.items.size();
  }
  panel_decl->name = name;
  declaration_.items.append(std::move(panel_decl));

  PanelDeclarationBuilder &builder_ref = *panel_decl_builder;
  panel_builders_.append(std::move(panel_decl_builder));
  set_active_panel_builder(&builder_ref);

  return builder_ref;
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

std::unique_ptr<SocketDeclaration> make_declaration_for_socket_type(
    const eNodeSocketDatatype socket_type)
{
  switch (socket_type) {
    case SOCK_FLOAT:
      return std::make_unique<decl::Float>();
    case SOCK_VECTOR:
      return std::make_unique<decl::Vector>();
    case SOCK_RGBA:
      return std::make_unique<decl::Color>();
    case SOCK_BOOLEAN:
      return std::make_unique<decl::Bool>();
    case SOCK_ROTATION:
      return std::make_unique<decl::Rotation>();
    case SOCK_MATRIX:
      return std::make_unique<decl::Matrix>();
    case SOCK_INT:
      return std::make_unique<decl::Int>();
    case SOCK_STRING:
      return std::make_unique<decl::String>();
    case SOCK_GEOMETRY:
      return std::make_unique<decl::Geometry>();
    case SOCK_OBJECT:
      return std::make_unique<decl::Object>();
    case SOCK_IMAGE:
      return std::make_unique<decl::Image>();
    case SOCK_COLLECTION:
      return std::make_unique<decl::Collection>();
    case SOCK_MATERIAL:
      return std::make_unique<decl::Material>();
    case SOCK_MENU:
      return std::make_unique<decl::Menu>();
    default:
      return {};
  }
}

BaseSocketDeclarationBuilder &NodeDeclarationBuilder::add_input(
    const eNodeSocketDatatype socket_type, const StringRef name, const StringRef identifier)
{
  switch (socket_type) {
    case SOCK_FLOAT:
      return this->add_input<decl::Float>(name, identifier);
    case SOCK_VECTOR:
      return this->add_input<decl::Vector>(name, identifier);
    case SOCK_RGBA:
      return this->add_input<decl::Color>(name, identifier);
    case SOCK_BOOLEAN:
      return this->add_input<decl::Bool>(name, identifier);
    case SOCK_ROTATION:
      return this->add_input<decl::Rotation>(name, identifier);
    case SOCK_MATRIX:
      return this->add_input<decl::Matrix>(name, identifier);
    case SOCK_INT:
      return this->add_input<decl::Int>(name, identifier);
    case SOCK_STRING:
      return this->add_input<decl::String>(name, identifier);
    case SOCK_GEOMETRY:
      return this->add_input<decl::Geometry>(name, identifier);
    case SOCK_OBJECT:
      return this->add_input<decl::Object>(name, identifier);
    case SOCK_IMAGE:
      return this->add_input<decl::Image>(name, identifier);
    case SOCK_COLLECTION:
      return this->add_input<decl::Collection>(name, identifier);
    case SOCK_MATERIAL:
      return this->add_input<decl::Material>(name, identifier);
    case SOCK_MENU:
      return this->add_input<decl::Menu>(name, identifier);
    default:
      BLI_assert_unreachable();
      return this->add_input<decl::Float>("", "");
  }
}

BaseSocketDeclarationBuilder &NodeDeclarationBuilder::add_input(const eCustomDataType data_type,
                                                                const StringRef name,
                                                                const StringRef identifier)
{
  return this->add_input(*bke::custom_data_type_to_socket_type(data_type), name, identifier);
}

BaseSocketDeclarationBuilder &NodeDeclarationBuilder::add_output(
    const eNodeSocketDatatype socket_type, const StringRef name, const StringRef identifier)
{
  switch (socket_type) {
    case SOCK_FLOAT:
      return this->add_output<decl::Float>(name, identifier);
    case SOCK_VECTOR:
      return this->add_output<decl::Vector>(name, identifier);
    case SOCK_RGBA:
      return this->add_output<decl::Color>(name, identifier);
    case SOCK_BOOLEAN:
      return this->add_output<decl::Bool>(name, identifier);
    case SOCK_ROTATION:
      return this->add_output<decl::Rotation>(name, identifier);
    case SOCK_MATRIX:
      return this->add_output<decl::Matrix>(name, identifier);
    case SOCK_INT:
      return this->add_output<decl::Int>(name, identifier);
    case SOCK_STRING:
      return this->add_output<decl::String>(name, identifier);
    case SOCK_GEOMETRY:
      return this->add_output<decl::Geometry>(name, identifier);
    case SOCK_OBJECT:
      return this->add_output<decl::Object>(name, identifier);
    case SOCK_IMAGE:
      return this->add_output<decl::Image>(name, identifier);
    case SOCK_COLLECTION:
      return this->add_output<decl::Collection>(name, identifier);
    case SOCK_MATERIAL:
      return this->add_output<decl::Material>(name, identifier);
    case SOCK_MENU:
      return this->add_output<decl::Menu>(name, identifier);
    default:
      BLI_assert_unreachable();
      return this->add_output<decl::Float>("", "");
  }
}

BaseSocketDeclarationBuilder &NodeDeclarationBuilder::add_output(const eCustomDataType data_type,
                                                                 const StringRef name,
                                                                 const StringRef identifier)
{
  return this->add_output(*bke::custom_data_type_to_socket_type(data_type), name, identifier);
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

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::reference_pass(
    const Span<int> input_indices)
{
  BLI_assert(this->is_output());
  aal::RelationsInNode &relations = node_decl_builder_->get_anonymous_attribute_relations();
  for (const int from_input : input_indices) {
    aal::ReferenceRelation relation;
    relation.from_field_input = from_input;
    relation.to_field_output = index_;
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
      relation.field_input = index_;
      relation.geometry_input = input_index;
      relations.eval_relations.append(relation);
    }
  }
  else {
    this->field_source();
    for (const int output_index : indices) {
      aal::AvailableRelation relation;
      relation.field_output = index_;
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

BaseSocketDeclarationBuilder &BaseSocketDeclarationBuilder::unavailable(bool value)
{
  decl_base_->is_unavailable = value;
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

PanelDeclarationBuilder &PanelDeclarationBuilder::draw_buttons(PanelDrawButtonsFunction func)
{
  decl_->draw_buttons = func;
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

}  // namespace implicit_field_inputs

}  // namespace blender::nodes
