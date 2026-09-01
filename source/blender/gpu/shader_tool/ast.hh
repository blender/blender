/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#pragma once

#include "lexit/lexit.hh"

namespace blender::gpu::shader::parser {

using namespace lexit;

struct ParserBase;
struct Token;

namespace ast {

using Token = parser::Token;

enum class NodeType : char {
  Invalid = 0,
  TranslationUnit,
  Preprocessor,
  Namespace,
  NamespaceSeparator,
  ClassDecl,
  AccessSpecifier,
  EnumValue,
  Id,
  IdQualified,
  IdType,
  Const,
  Reference,
  AssignStmt,
  PipelineDecl,
  VarDecl,
  Declarator,
  StaticStmt,
  FuncDecl,
  FuncForwardDecl,
  FuncArgList,
  FuncArg,
  FuncParamList,
  FuncCall,
  TemplateDecl,
  TemplateArgList,
  TemplateArg,
  TemplateParamList,
  TemplateSpec,
  TemplateInst,
  TemplateExplicit,
  LocalScope,
  ArrayDecl,
  Subscript,
  AttrList,
  Attr,
  Expr,
  ContinueStmt,
  BreakStmt,
  ReturnStmt,
  UsingStmt,
  LocalStmt,
  LocalVar,
  SwitchStmt,
  SwitchCase,
  ForLoop,
  WhileLoop,
  DoWhileLoop,
  Condition,
  IfStmt,
  ElseIfStmt,
  ElseStmt,
  InitializerList,
  DesignatedInitializer,
  Initializer,
  StringConst,
  ExprSub,
  NumConst,
  Op,
  OpDeref,
  Constructor,
  StructuredBinding,
};

using TokenID = int;
using NodeID = int;

struct NodeData;
using Nodes = std::vector<NodeData>;

/**
 * Immutable AST node data.
 * Contains indices of its neighbor and the token range it covers.
 *
 * A node cannot overlap with its neighbors.
 * All nodes are stored in source order.
 */
struct NodeData {
  NodeType type = NodeType::Invalid;

  /* Boundary tokens. */
  TokenID front = -1;
  TokenID back = -1;

  NodeID parent = -1;
  /* Neighbors. */
  NodeID prev = -1, next = -1;
  /* Children. */
  NodeID child_first = -1, child_last = -1;

  NodeData() = default;

  NodeData(Nodes &nodes, NodeID id, NodeID parent_id, lexit::Token tok, NodeType type)
      : type(type), front(tok.index_), back(tok.index_)
  {
    if (parent_id != -1) {
      NodeData &parent = nodes[parent_id];
      if (parent.child_last != -1) {
        nodes[parent.child_last].next = id;
        this->prev = parent.child_last;
      }
      if (parent.child_first == -1) {
        parent.child_first = id;
      }
      parent.child_last = id;
    }
    this->parent = parent_id;
  }

  std::string location(const ParserBase &parser) const;
};

/**
 * Type-less, read-only interface to a AST node data.
 * Can represent an invalid node.
 *
 * Serves as a basis for typed node interfaces.
 */
struct Node {
  const ParserBase *p = nullptr;
  NodeID id = -1;

#ifdef LEXIT_DEBUG
  NodeType debug_type;
  const NodeData *debug_data;
  std::string_view debug_str;
#endif

  Node() = default;
  Node(const ParserBase *p, NodeID id) : p(p), id(id)
  {
#ifdef LEXIT_DEBUG
    debug_str = str();
    debug_type = type();
    debug_data = data();
#endif
  }

  const NodeData *data() const;
  NodeType type() const;

  /* Boundary tokens. */
  Token front() const;
  Token back() const;

  /* Return the neighbor node or an invalid node if it doesn't exists. */
  Node prev() const;
  Node next() const;
  Node parent() const;
  Node children() const;
  Node child_first() const;
  Node child_last() const;

  /* Return the first neighbor node of type or an invalid node if it doesn't exists. */
  Node prev(NodeType type) const;
  Node next(NodeType type) const;
  Node parent(NodeType type) const;
  Node child_first(NodeType type) const;
  Node child_last(NodeType type) const;

  int child_count() const;
  bool is_empty() const;
  bool has_single_child() const;
  bool is_valid() const
  {
    return id != -1;
  }

  std::string_view str() const;
  /* Same as str buf with trailing white-spaces. */
  std::string_view str_full() const;

  /* Print AST dump to the standard output. */
  void print_ast() const;

  bool operator==(NodeType type) const
  {
    return this->type() == type;
  }
  bool operator!=(NodeType type) const
  {
    return this->type() != type;
  }

  std::ostream &operator<<(std::ostream &os)
  {
    os << str();
    return os;
  }

  /* Traversal. */

  struct ChildIterator;
  struct ChildRange;
  ChildRange children_range() const;

  template<typename NodeT> struct TypedChildIterator;
  template<typename NodeT> struct TypedChildRange;
  template<typename NodeT> TypedChildRange<NodeT> children_of_type() const;

  template<typename NodeT> struct RecursiveTypedIterator;
  template<typename NodeT> struct RecursiveTypedRange;
  template<typename NodeT> RecursiveTypedRange<NodeT> descendants_of_type() const;

 private:
  static Node first_node_of_type(Node node, NodeType type, Node end)
  {
    while (node.id != end.id && node.type() != type) {
      node = Node(node.p, node.id + 1);
    }
    return node;
  }

  static Node next_node_of_type(Node node, NodeType type, Node end)
  {
    do {
      node = Node(node.p, node.id + 1);
    } while (node.id != end.id && node.type() != type);
    return node;
  }

  Node find_recursion_end() const
  {
    {
      /* Try using next first. */
      Node node = *this;
      if (node.next().is_valid()) {
        return node.next();
      }
    }
    {
      /* Try using parent next. */
      Node node = *this;
      while (node.is_valid()) {
        node = node.parent();
        if (node.next().is_valid()) {
          return node.next();
        }
      }
    }
    {
      /* Using children in last resort. */
      Node node = *this;
      while (node.is_valid()) {
        node = node.child_last();
        if (!node.child_last().is_valid()) {
          /* No children. Deepest latest node. */
          return Node(p, node.id + 1);
        }
      }
    }
    /* Nothing to iterate on. */
    return *this;
  }
};

inline Node Node::child_first(NodeType type) const
{
  Node node = child_first();
  return (node.type() == type) ? node : node.next(type);
}

inline Node Node::child_last(NodeType type) const
{
  Node node = child_last();
  return (node.type() == type) ? node : node.prev(type);
}

struct Node::ChildIterator {
  Node current;

  ChildIterator &operator++()
  {
    current = current.next();
    return *this;
  }
  ChildIterator operator++(int)
  {
    ChildIterator tmp = *this;
    ++(*this);
    return tmp;
  }
  Node operator*() const
  {
    return current;
  }
  bool operator!=(const ChildIterator &other) const
  {
    return current.id != other.current.id;
  }
  bool operator==(const ChildIterator &other) const
  {
    return current.id == other.current.id;
  }
};

struct Node::ChildRange {
  Node parent;
  ChildIterator begin() const
  {
    return {parent.child_first()};
  }
  ChildIterator end() const
  {
    return {Node()};
  }
};

inline Node::ChildRange Node::children_range() const
{
  return {*this};
}

template<typename NodeT> struct Node::TypedChildIterator {
  Node current;

  TypedChildIterator &operator++()
  {
    current = current.next(NodeT::NodeEnumVal);
    return *this;
  }
  TypedChildIterator operator++(int)
  {
    TypedChildIterator tmp = *this;
    ++(*this);
    return tmp;
  }
  NodeT operator*() const
  {
    return NodeT(current);
  }
  bool operator!=(const TypedChildIterator &other) const
  {
    return current.id != other.current.id;
  }
  bool operator==(const TypedChildIterator &other) const
  {
    return current.id == other.current.id;
  }
};

template<typename NodeT> struct Node::TypedChildRange {
  Node parent;
  TypedChildIterator<NodeT> begin() const
  {
    return {parent.child_first(NodeT::NodeEnumVal)};
  }
  TypedChildIterator<NodeT> end() const
  {
    return {Node()};
  }
};

template<typename NodeT> inline Node::TypedChildRange<NodeT> Node::children_of_type() const
{
  return {*this};
}

template<typename NodeT> struct Node::RecursiveTypedIterator {
  Node current;
  Node end_node;

  RecursiveTypedIterator &operator++()
  {
    current = Node::next_node_of_type(current, NodeT::NodeEnumVal, end_node);
    return *this;
  }
  RecursiveTypedIterator operator++(int)
  {
    RecursiveTypedIterator tmp = *this;
    ++(*this);
    return tmp;
  }
  NodeT operator*() const
  {
    return NodeT(current);
  }
  bool operator!=(const RecursiveTypedIterator &other) const
  {
    return current.id != other.current.id;
  }
  bool operator==(const RecursiveTypedIterator &other) const
  {
    return current.id == other.current.id;
  }
};

template<typename NodeT> struct Node::RecursiveTypedRange {
  Node start;
  Node end_node;
  RecursiveTypedIterator<NodeT> begin() const
  {
    return {Node::first_node_of_type(start.child_first(), NodeT::NodeEnumVal, end_node), end_node};
  }
  RecursiveTypedIterator<NodeT> end() const
  {
    return {end_node, end_node};
  }
};

template<typename NodeT> inline Node::RecursiveTypedRange<NodeT> Node::descendants_of_type() const
{
  return {*this, find_recursion_end()};
}

/* All typed nodes are implicitly convertible to each other an to the type-less #Node class.
 * Converting a #Node into a typed node of the wrong type will create an invalid node. */
#define NODE_COMMON(Type) \
  static constexpr NodeType NodeEnumVal = NodeType::Type; \
  Type() = default; \
  Type(const Node &node) : Node(node.type() == NodeType::Type ? node : Node{}) {}

using TokenRange = Token; /* TODO */

struct FuncParamList;
struct LocalScope;
struct ClassDecl;
struct InitializerList;
struct TemplateParamList;

struct Preprocessor : Node {
  NODE_COMMON(Preprocessor);
};

struct NumConst : Node {
  NODE_COMMON(NumConst);
};

struct Id : Node {
  NODE_COMMON(Id);

  /* Return the next Id in an IdQualified.  */
  Id next_id() const
  {
    Node next_node = next();
    next_node = (next_node.type() == NodeType::TemplateParamList) ? next_node.next() : next_node;
    return next_node.type() == NodeType::NamespaceSeparator ? next_node.next() : Node{};
  }

  bool is_namespace() const
  {
    return next_id().is_valid();
  }

  TemplateParamList template_params() const;
};

struct IdQualified : Node {
  NODE_COMMON(IdQualified);

  bool has_namespace() const
  {
    return child_first(NodeType::NamespaceSeparator).is_valid();
  }

  bool is_global() const
  {
    return child_first().type() == NodeType::NamespaceSeparator;
  }

  Id name() const
  {
    return child_last(NodeType::Id);
  }

  Id namespace_start() const
  {
    return child_first(NodeType::Id);
  }

  TemplateParamList template_params() const;
};

struct TemplateParamList : Node {
  NODE_COMMON(TemplateParamList);
};

inline TemplateParamList IdQualified::template_params() const
{
  return child_last(NodeType::TemplateParamList);
}

inline TemplateParamList Id::template_params() const
{
  return next();
}

struct OpDeref : Node {
  NODE_COMMON(OpDeref);
};

struct StaticStmt : Node {
  NODE_COMMON(StaticStmt);
};

struct AccessSpecifier : Node {
  NODE_COMMON(AccessSpecifier);
};

struct TemplateExplicit : Node {
  NODE_COMMON(TemplateExplicit);
};

struct TemplateSpec : Node {
  NODE_COMMON(TemplateSpec);

  TemplateParamList parameters() const;

  bool is_function() const
  {
    return child_last().type() == NodeType::FuncDecl;
  }

  bool is_class() const
  {
    return child_last().type() == NodeType::ClassDecl;
  }

  Node decl() const
  {
    return child_last();
  }
};

struct TemplateInst : Node {
  NODE_COMMON(TemplateInst);

  TemplateParamList parameters() const;

  bool is_function() const
  {
    return child_last().type() == NodeType::FuncForwardDecl;
  }

  bool is_class() const
  {
    return child_last().type() == NodeType::ClassDecl;
  }

  Node decl() const
  {
    return child_last();
  }
};

struct TemplateArg : Node {
  NODE_COMMON(TemplateArg);

  IdQualified type() const
  {
    return child_first();
  }

  IdQualified identifier() const
  {
    return child_last();
  }
};

struct TemplateArgList : Node {
  NODE_COMMON(TemplateArgList);
};

struct TemplateDecl : Node {
  NODE_COMMON(TemplateDecl);

  TemplateArgList arguments() const
  {
    return child_first();
  }

  bool is_function() const
  {
    return child_last().type() == NodeType::FuncDecl;
  }

  bool is_class() const
  {
    return child_last().type() == NodeType::ClassDecl;
  }

  Node decl() const
  {
    return child_last();
  }
};

struct Const : Node {
  NODE_COMMON(Const);
};

struct Reference : Node {
  NODE_COMMON(Reference);
};

struct Expr : Node {
  NODE_COMMON(Expr);
};

struct ExprSub : Node {
  NODE_COMMON(ExprSub);

  Expr expr()
  {
    return child_first();
  }
};

struct UsingStmt : Node {
  NODE_COMMON(UsingStmt);

  bool is_namespace() const;

  IdQualified identifier() const
  {
    return child_first();
  }

  IdQualified aliased() const
  {
    return child_last();
  }
};

struct LocalVar : Node {
  NODE_COMMON(LocalVar);

  IdQualified identifier() const
  {
    return child_first();
  }
};

struct ReturnStmt : Node {
  NODE_COMMON(ReturnStmt);

  Expr expression() const
  {
    return child_last();
  }
};

struct IdType : Node {
  NODE_COMMON(IdType);

  bool is_const() const
  {
    return constant().is_valid();
  }

  bool is_constexpr() const;

  bool is_static() const
  {
    return child_first(NodeType::StaticStmt).is_valid();
  }

  Const constant() const
  {
    return child_first(NodeType::Const);
  }

  IdQualified identifier() const
  {
    return child_first(NodeType::IdQualified);
  }

  Reference reference() const
  {
    return child_first(NodeType::Reference);
  }
};

struct Subscript : Node {
  NODE_COMMON(Subscript);

  Expr expr() const
  {
    return child_first();
  }
};

struct ArrayDecl : Node {
  NODE_COMMON(ArrayDecl);

  /* Return 0 if invalid. */
  int dimensions() const
  {
    return is_valid() ? child_count() : 0;
  }

  Subscript sub() const
  {
    return child_first();
  }
};

struct Initializer : Node {
  NODE_COMMON(Initializer);
};

struct InitializerList : Node {
  NODE_COMMON(InitializerList);
};

struct Constructor : Node {
  NODE_COMMON(Constructor);

  IdQualified identifier() const
  {
    return child_first();
  }

  InitializerList initializer_list() const
  {
    return child_last();
  }
};

struct Attr : Node {
  NODE_COMMON(Attr);

  Id identifier() const
  {
    return child_first();
  }

  FuncParamList parameters() const;
};

struct AttrList : Node {
  NODE_COMMON(AttrList);

  bool contains_attr(std::string_view attr_name) const
  {
    if (!is_valid()) {
      return false;
    }
    bool found = false;
    for (Attr attr : children_of_type<Attr>()) {
      if (attr.identifier().str() == attr_name) {
        found = true;
      }
    }
    return found;
  }
};

struct LocalStmt : Node {
  NODE_COMMON(LocalStmt);

  AttrList attributes() const
  {
    return child_first();
  }

  Expr expr() const
  {
    return child_last();
  }
};

struct AssignStmt : Node {
  NODE_COMMON(AssignStmt);

  InitializerList initializer_list() const
  {
    return child_first();
  }

  Expr expr() const
  {
    return child_first();
  }
};

struct DesignatedInitializer : Node {
  NODE_COMMON(DesignatedInitializer);

  Id identifier() const
  {
    return child_first();
  }

  AssignStmt assign() const
  {
    return child_last();
  }
};

struct StructuredBinding : Node {
  NODE_COMMON(StructuredBinding);

  /* Unique identified used to name the temporary variable. */
  std::string tmp_id() const;

  AssignStmt assign() const
  {
    return child_last();
  }
};

struct Declarator : Node {
  NODE_COMMON(Declarator);

  bool is_reference() const
  {
    return reference().is_valid();
  }

  bool is_array() const
  {
    return array().is_valid();
  }

  IdType type() const
  {
    return prev(NodeType::IdType);
  }

  Reference reference() const
  {
    return child_first();
  }

  IdQualified identifier() const
  {
    return child_first(NodeType::IdQualified);
  }

  ArrayDecl array() const
  {
    return identifier().next();
  }

  InitializerList initializer_list() const
  {
    return child_last();
  }

  AssignStmt initial_value() const
  {
    return child_last();
  }
};

struct VarDecl : Node {
  NODE_COMMON(VarDecl);

  bool is_const() const
  {
    return type().is_const();
  }

  AttrList attributes() const
  {
    return child_first();
  }

  IdType type() const
  {
    return child_first(NodeType::IdType);
  }

  /* Return true if the first declarator is a reference. */
  bool is_reference() const
  {
    return Declarator(child_first(NodeType::Declarator)).is_reference();
  }
};

struct FuncArg : Node {
  NODE_COMMON(FuncArg);

  AttrList attributes() const
  {
    return child_first();
  }

  IdType type() const
  {
    return child_first(NodeType::IdType);
  }

  Declarator declarator() const
  {
    return child_first(NodeType::Declarator);
  }

  bool is_reference() const
  {
    return declarator().is_reference();
  }

  bool is_const() const
  {
    return type().is_const();
  }

  IdQualified identifier() const
  {
    return declarator().identifier();
  }

  Subscript array() const
  {
    return declarator().array();
  }

  InitializerList initializer_list() const
  {
    return declarator().initializer_list();
  }

  AssignStmt initial_value() const
  {
    return declarator().initial_value();
  }
};

struct FuncArgList : Node {
  NODE_COMMON(FuncArgList);
};

struct FuncParamList : Node {
  NODE_COMMON(FuncParamList);

  bool is_empty() const;

  struct Splat1 {
    Expr arg1;
  };
  struct Splat2 {
    Expr arg1, arg2;
  };
  struct Splat3 {
    Expr arg1, arg2, arg3;
  };

  Splat1 splat_1() const
  {
    return {child_first()};
  }

  Splat2 splat_2() const
  {
    Node node = child_first();
    return {node, node.next()};
  }

  Splat3 splat_3() const
  {
    Node node = child_first();
    return {node, node.next(), node.next().next()};
  }
};

inline FuncParamList Attr::parameters() const
{
  return FuncParamList(child_first().next());
}

struct FuncForwardDecl : Node {
  NODE_COMMON(FuncForwardDecl);

  bool is_method() const
  {
    return parent(NodeType::ClassDecl).is_valid();
  }

  bool is_static() const
  {
    return child_first(NodeType::StaticStmt).is_valid();
  }

  bool is_const() const
  {
    return child_last(NodeType::Const).is_valid();
  }

  AttrList attributes() const
  {
    return child_first();
  }

  IdType return_type() const
  {
    return child_first(NodeType::IdType);
  }

  IdQualified identifier() const
  {
    return return_type().next();
  }

  FuncArgList arguments() const
  {
    return child_first(NodeType::FuncArgList);
  }

  /* Associated class if this function is a class method. */
  ClassDecl parent_class() const;
};

struct FuncDecl : Node {
  NODE_COMMON(FuncDecl);

  bool is_method() const
  {
    return parent(NodeType::ClassDecl).is_valid();
  }

  bool is_static() const
  {
    return child_first(NodeType::StaticStmt).is_valid();
  }

  bool is_const() const
  {
    return child_last(NodeType::Const).is_valid();
  }

  bool is_template() const
  {
    return TemplateDecl(parent()).is_valid();
  }

  AttrList attributes() const
  {
    return child_first();
  }

  IdType return_type() const
  {
    return child_first(NodeType::IdType);
  }

  IdQualified identifier() const
  {
    return return_type().next();
  }

  FuncArgList arguments() const
  {
    return child_first(NodeType::FuncArgList);
  }

  /* Associated class if this function is a class method. */
  ClassDecl parent_class() const;

  LocalScope body() const;
};

struct FuncCall : Node {
  NODE_COMMON(FuncCall);

  IdQualified identifier() const
  {
    return child_first();
  }

  FuncParamList parameters() const
  {
    return child_last();
  }
};

struct PipelineDecl : Node {
  NODE_COMMON(PipelineDecl);

  IdQualified type() const
  {
    return child_first();
  }

  IdQualified identifier() const
  {
    return type().next();
  }

  FuncParamList parameters() const
  {
    return child_last();
  }
};

struct EnumValue : Node {
  NODE_COMMON(EnumValue);

  IdQualified identifier() const
  {
    return child_first();
  }

  AssignStmt value() const
  {
    return child_last();
  }
};

struct LocalScope : Node {
  NODE_COMMON(LocalScope);
};

struct Condition : Node {
  NODE_COMMON(Condition);

  AttrList attributes() const
  {
    return next();
  }
};

struct IfStmt : Node {
  NODE_COMMON(IfStmt);

  Condition condition() const
  {
    return child_first();
  }

  AttrList attributes() const
  {
    return child_last(NodeType::AttrList);
  }

  LocalScope body() const
  {
    return child_last();
  }
};

struct ElseIfStmt : Node {
  NODE_COMMON(ElseIfStmt);

  Condition condition() const
  {
    return child_first();
  }

  AttrList attributes() const
  {
    return child_last(NodeType::AttrList);
  }

  LocalScope body() const
  {
    return child_last();
  }
};

struct ElseStmt : Node {
  NODE_COMMON(ElseStmt);

  LocalScope body() const
  {
    return child_last();
  }
};

struct ForLoop : Node {
  NODE_COMMON(ForLoop);

  Condition condition() const
  {
    return child_first();
  }

  LocalScope body() const
  {
    return child_last();
  }
};

struct WhileLoop : Node {
  NODE_COMMON(WhileLoop);

  Condition condition() const
  {
    return child_first();
  }

  LocalScope body() const
  {
    return child_last();
  }
};

struct DoWhileLoop : Node {
  NODE_COMMON(DoWhileLoop);

  Condition condition() const
  {
    return child_last();
  }

  LocalScope body() const
  {
    return child_first();
  }
};

struct SwitchStmt : Node {
  NODE_COMMON(SwitchStmt);

  Condition condition() const
  {
    return child_first();
  }
};

struct SwitchCase : Node {
  NODE_COMMON(SwitchCase);

  bool is_default_case() const
  {
    return child_first().type() == NodeType::LocalScope;
  }

  /* Either NumConst or IdQualified. */
  Node value() const
  {
    return child_first();
  }

  LocalScope body() const
  {
    return child_last();
  }
};

/* Enum, Struct, Class, Union. */
struct ClassDecl : Node {
  NODE_COMMON(ClassDecl);

  bool is_enum() const;
  bool is_enum_class() const;
  bool is_union() const;

  bool is_anonymous() const
  {
    return !identifier().is_valid();
  }

  IdQualified identifier() const;

  AttrList attributes() const
  {
    return child_first();
  }

  IdQualified parent_class() const;

  LocalScope body() const
  {
    return child_last();
  }
};

inline ClassDecl FuncDecl::parent_class() const
{
  return is_template() ? parent().parent().parent() : parent().parent();
}

inline LocalScope FuncDecl::body() const
{
  return child_last(NodeType::LocalScope);
}

inline TemplateParamList TemplateSpec::parameters() const
{
  Node node = child_last();
  if (node.type() == NodeType::ClassDecl) {
    ClassDecl decl(node);
    return decl.identifier().template_params();
  }
  if (node.type() == NodeType::FuncDecl) {
    FuncDecl decl(node);
    return decl.identifier().template_params();
  }
  assert(0);
  return {};
}

inline TemplateParamList TemplateInst::parameters() const
{
  Node node = child_last();
  if (node.type() == NodeType::ClassDecl) {
    ClassDecl decl(node);
    return decl.identifier().template_params();
  }
  if (node.type() == NodeType::FuncForwardDecl) {
    FuncForwardDecl decl(node);
    return decl.identifier().template_params();
  }
  assert(0);
  return {};
}

struct Namespace : Node {
  NODE_COMMON(Namespace);

  LocalScope body() const
  {
    return child_last();
  }

  IdQualified identifier() const
  {
    return child_first();
  }
};

#undef NODE_COMMON

}  // namespace ast
}  // namespace blender::gpu::shader::parser
