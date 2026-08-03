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

std::string to_str(TokenType type);

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
  Subscript,
  AttrList,
  Attr,
  Expr,
  ReturnStmt,
  UsingStmt,
  LocalStmt,
  LocalVar,
  Switch,
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
  NumConst,
  Op,
  OpDeref,
  Constructor,
};

using TokenID = int;
using NodeID = int;

struct NodeData;
using Nodes = std::vector<NodeData>;

struct NodeData {
  TokenID front = -1;
  TokenID back = -1;
  NodeID parent = -1;
  NodeID prev = -1;
  NodeID next = -1;
  NodeID child_first = -1;
  NodeID child_last = -1;
  NodeType type = NodeType::Invalid;

  NodeData() = default;

  NodeData(Nodes &nodes, NodeID id, NodeID parent_id, lexit::Token tok, NodeType type)
      : front(tok.index_), back(tok.index_), type(type)
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

struct Node {
  const ParserBase *p = nullptr;
  NodeID id = -1;
#ifdef LEXIT_DEBUG
  std::string_view debug_str;
#endif

  Node() = default;
  Node(const ParserBase *p, NodeID id) : p(p), id(id)
  {
#ifdef LEXIT_DEBUG
    debug_str = str();
#endif
  }

  NodeType type() const;

  Token front() const;
  Token back() const;

  Node prev() const;
  Node next() const;
  Node prev(NodeType type) const;
  Node next(NodeType type) const;
  Node parent() const;
  Node children() const;
  Node child_first() const;
  Node child_last() const;
  Node child_first(NodeType type) const
  {
    Node node = child_first();
    return (node.type() == type) ? node : node.next(type);
  }
  Node child_last(NodeType type) const
  {
    Node node = child_last();
    return (node.type() == type) ? node : node.prev(type);
  }

  bool is_empty() const;
  bool is_valid() const
  {
    return id != -1;
  }

  void print_ast() const;
  std::string_view str() const;

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

  template<typename NodeT, typename CallbackT> void foreach_recursive(CallbackT cb) const
  {
    for (Node end = Node(p, child_last().id + 1),
              node = first_node_of_type(children(), NodeT::NodeEnumVal, end);
         node.id != end.id;
         node = next_node_of_type(node, NodeT::NodeEnumVal, end))
    {
      cb(NodeT(node));
    }
  }

  template<typename NodeT, typename CallbackT> void foreach(CallbackT cb) const
  {
    for (Node node = children(); node.is_valid(); node = node.next(NodeT::NodeEnumVal)) {
      cb(NodeT(node));
    }
  }

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
};

#define NODE_COMMON(Type) \
  static constexpr NodeType NodeEnumVal = NodeType::Type; \
  Type() = default; \
  Type(const Node &node) : Node(node.type() == NodeType::Type ? node : Node{}) {}

using TokenRange = Token; /* TODO */

struct FuncParamList;
struct LocalScope;
struct InitializerList;

struct Preprocessor : Node {
  NODE_COMMON(Preprocessor);
};

struct Id : Node {
  NODE_COMMON(Id);
};

struct IdQualified : Node {
  NODE_COMMON(IdQualified);

  bool has_namespace() const
  {
    return child_first(NodeType::NamespaceSeparator).is_valid();
  }

  TokenRange full_namespace() const;
  IdQualified parent_namespace() const;

  Id name() const
  {
    Node node = children().child_last();
    return Id(node);
  }
};

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

struct Const : Node {
  NODE_COMMON(Const);
};

struct Reference : Node {
  NODE_COMMON(Reference);
};

struct Expr : Node {
  NODE_COMMON(Expr);
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

  Const constant() const
  {
    return child_first();
  }

  IdQualified id() const
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

struct DesignatedInitializer : Node {
  NODE_COMMON(DesignatedInitializer);
};

struct Initializer : Node {
  NODE_COMMON(Initializer);
};

struct InitializerList : Node {
  NODE_COMMON(InitializerList);
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
    bool found = false;
    foreach<Attr>([&](Attr attr) {
      if (attr.identifier().str() == attr_name) {
        found = true;
      }
    });
    return found;
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

struct Declarator : Node {
  NODE_COMMON(Declarator);

  bool is_reference() const
  {
    return reference().is_valid();
  }

  Reference reference() const
  {
    return child_first();
  }

  Id identifier() const
  {
    return child_first(NodeType::Id);
  }

  Subscript array() const
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

  Id identifier() const
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
};

inline FuncParamList Attr::parameters() const
{
  return FuncParamList(children().next());
}

struct FuncDecl : Node {
  NODE_COMMON(FuncDecl);

  bool is_static() const
  {
    return child_first(NodeType::StaticStmt).is_valid();
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

struct EnumValue : Node {
  NODE_COMMON(EnumValue);

  Id identifier() const
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

/* Enum, Struct, Class. */
struct ClassDecl : Node {
  NODE_COMMON(ClassDecl);

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

inline LocalScope FuncDecl::body() const
{
  return child_last(NodeType::LocalScope);
}

struct Namespace : Node {
  NODE_COMMON(Namespace);

  IdType name() const;
};

#undef NODE_COMMON

}  // namespace ast
}  // namespace blender::gpu::shader::parser
