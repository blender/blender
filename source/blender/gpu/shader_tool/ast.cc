/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "ast.hh"
#include "token.hh"

namespace blender::gpu::shader::parser::ast {

std::string NodeData::location(const ParserBase &parser) const
{
  std::string str = parser[front].filename();
  str += std::to_string(parser[front].line_number());
  str += ':';
  str += std::to_string(parser[front].char_number() + 1);
  return str;
}

Node Node::prev() const
{
  return is_valid() ? Node{p, p->ast_nodes[id].prev} : Node{};
}

Node Node::prev(NodeType type) const
{
  Node node = *this;
  do {
    node = node.prev();
  } while (node.is_valid() && node.type() != type);
  return node;
}

Node Node::next() const
{
  return is_valid() ? Node{p, p->ast_nodes[id].next} : Node{};
}

Node Node::next(NodeType type) const
{
  Node node = *this;
  do {
    node = node.next();
  } while (node.is_valid() && node.type() != type);
  return node;
}

Node Node::parent() const
{
  return is_valid() ? Node{p, p->ast_nodes[id].parent} : Node{};
}

Node Node::children() const
{
  return is_valid() ? Node{p, p->ast_nodes[id].child_first} : Node{};
}

Node Node::child_first() const
{
  return is_valid() ? Node{p, p->ast_nodes[id].child_first} : Node{};
}

Node Node::child_last() const
{
  return is_valid() ? Node{p, p->ast_nodes[id].child_last} : Node{};
}

NodeType Node::type() const
{
  return is_valid() ? p->ast_nodes[id].type : NodeType::Invalid;
}

bool Node::is_empty() const
{
  return p->ast_nodes[id].child_first == -1;
}

Token Node::front() const
{
  return (*p)[p->ast_nodes[id].front];
}

Token Node::back() const
{
  return (*p)[p->ast_nodes[id].back];
}

std::string_view Node::str() const
{
  return is_valid() ? p->substr(front(), back()) : "";
}

bool FuncParamList::is_empty() const
{
  return back().index_ - front().index_ == 1;
}

IdQualified ClassDecl::identifier() const
{
  Node node = child_first(NodeType::IdQualified);
  return IdQualified(node.front().prev() == Colon ? Node{} : node);
}

IdQualified ClassDecl::parent_class() const
{
  Node node = body().prev();
  return IdQualified(node.front().prev() == Colon ? node : Node{});
}
static std::string to_string(ast::NodeType type);
static bool display_type(ast::NodeType type);

void Node::print_ast() const
{
  if (!is_valid()) {
    return;
  }

  const ParserBase &parser = *p;
  const Nodes &nodes = parser.ast_nodes;

  /* Recursive lambda to handle tree traversal and indentation. */
  auto print_node =
      [&](auto &self, NodeID id, int depth, const std::vector<bool> &is_last) -> void {
    if (id == -1 || id >= static_cast<int>(nodes.size())) {
      return;
    }

    const auto &node = nodes[id];

    std::string loc = node.location(parser);
    /* Create indentation based on tree depth. */
    int padding_size = std::max(0, 20 - static_cast<int>(loc.size()));
    std::string padding(padding_size, ' ');

    std::cout << loc << padding;

    /* Print the tree branch lines based on depth and ancestor sibling status. */
    for (int i = 0; i < depth - 1; ++i) {
      if (is_last[i]) {
        std::cout << "  "; /* Ancestor was the last sibling, leave space. */
      }
      else {
        std::cout << "│ "; /* Ancestor has more siblings, draw vertical line. */
      }
    }
    /* Print the branch for the current node. */
    if (depth > 0) {
      if (is_last.back()) {
        std::cout << "└─"; /* Last sibling. */
      }
      else {
        std::cout << "├─"; /* Not the last sibling. */
      }
    }

    /* Print the current node. */
    std::cout << "o " << to_string(node.type);
    if (display_type(node.type)) {
      std::cout << " " << parser[node.front].str();
    }
    std::cout << "\n";

    /* Traverse all children of this node. */
    /* We start at child_first, then follow the 'next' sibling chain. */
    NodeID child_id = node.child_first;
    while (child_id != -1) {
      /* Determine if this child is the last one in the sibling chain. */
      bool child_is_last = (nodes[child_id].next == -1);

      /* Pass the updated sibling history to the next depth. */
      std::vector<bool> next_is_last = is_last;
      next_is_last.push_back(child_is_last);

      self(self, child_id, depth + 1, next_is_last);
      child_id = nodes[child_id].next;
    }
  };

  if (nodes[0].parent == -1) {
    print_node(print_node, this->id, 0, {});
  }
}

static std::string to_string(ast::NodeType type)
{
  switch (type) {
    case ast::NodeType::Invalid:
      return "Invalid";
    case ast::NodeType::OpDeref:
      return "OpDeref";
    case ast::NodeType::AccessSpecifier:
      return "AccessSpecifier";
    case ast::NodeType::StaticStmt:
      return "StaticStmt";
    case ast::NodeType::TranslationUnit:
      return "TranslationUnit";
    case ast::NodeType::Preprocessor:
      return "Preprocessor";
    case ast::NodeType::Namespace:
      return "Namespace";
    case ast::NodeType::NamespaceSeparator:
      return "NamespaceSeparator";
    case ast::NodeType::ClassDecl:
      return "ClassDecl";
    case ast::NodeType::IdType:
      return "IdType";
    case ast::NodeType::Const:
      return "Const";
    case ast::NodeType::Reference:
      return "Reference";
    case ast::NodeType::EnumValue:
      return "EnumValue";
    case ast::NodeType::Id:
      return "Id";
    case ast::NodeType::IdQualified:
      return "IdQualified";
    case ast::NodeType::AssignStmt:
      return "AssignStmt";
    case ast::NodeType::VarDecl:
      return "VarDecl";
    case ast::NodeType::FuncDecl:
      return "FuncDecl";
    case ast::NodeType::FuncArgList:
      return "FuncArgList";
    case ast::NodeType::FuncArg:
      return "FuncArg";
    case ast::NodeType::FuncParamList:
      return "FuncParamList";
    case ast::NodeType::LocalScope:
      return "LocalScope";
    case ast::NodeType::Subscript:
      return "Subscript";
    case ast::NodeType::AttrList:
      return "AttrList";
    case ast::NodeType::Attr:
      return "Attr";
    case ast::NodeType::Expr:
      return "Expr";
    case ast::NodeType::TemplateDecl:
      return "TemplateDecl";
    case ast::NodeType::TemplateArgList:
      return "TemplateArgList";
    case ast::NodeType::TemplateParamList:
      return "TemplateParamList";
    case ast::NodeType::TemplateSpec:
      return "TemplateSpec";
    case ast::NodeType::TemplateInst:
      return "TemplateInst";
    case ast::NodeType::TemplateArg:
      return "TemplateArg";
    case ast::NodeType::ReturnStmt:
      return "ReturnStmt";
    case ast::NodeType::UsingStmt:
      return "UsingStmt";
    case ast::NodeType::FuncForwardDecl:
      return "FuncForwardDecl";
    case ast::NodeType::LocalStmt:
      return "LocalStmt";
    case ast::NodeType::FuncCall:
      return "FuncCall";
    case ast::NodeType::LocalVar:
      return "LocalVar";
    case ast::NodeType::Switch:
      return "Switch";
    case ast::NodeType::SwitchCase:
      return "SwitchCase";
    case ast::NodeType::ForLoop:
      return "ForLoop";
    case ast::NodeType::WhileLoop:
      return "WhileLoop";
    case ast::NodeType::DoWhileLoop:
      return "DoWhileLoop";
    case ast::NodeType::Condition:
      return "Condition";
    case ast::NodeType::IfStmt:
      return "IfStmt";
    case ast::NodeType::ElseIfStmt:
      return "ElseIfStmt";
    case ast::NodeType::ElseStmt:
      return "ElseStmt";
    case ast::NodeType::InitializerList:
      return "InitializerList";
    case ast::NodeType::Initializer:
      return "Initializer";
    case ast::NodeType::DesignatedInitializer:
      return "DesignatedInitializer";
    case ast::NodeType::StringConst:
      return "StringConst";
    case ast::NodeType::NumConst:
      return "NumConst";
    case ast::NodeType::Op:
      return "Op";
    case ast::NodeType::PipelineDecl:
      return "PipelineDecl";
    case ast::NodeType::Constructor:
      return "Constructor";
    case ast::NodeType::TemplateExplicit:
      return "TemplateExplicit";
    case ast::NodeType::Declarator:
      return "Declarator";
  }
  return "Error";
}

static bool display_type(ast::NodeType type)
{
  switch (type) {
    case ast::NodeType::Invalid:
      return false;
    case ast::NodeType::OpDeref:
      return true;
    case ast::NodeType::AccessSpecifier:
      return true;
    case ast::NodeType::StaticStmt:
      return false;
    case ast::NodeType::TranslationUnit:
      return false;
    case ast::NodeType::Preprocessor:
      return false;
    case ast::NodeType::Namespace:
      return false;
    case ast::NodeType::NamespaceSeparator:
      return false;
    case ast::NodeType::ClassDecl:
      return true;
    case ast::NodeType::IdType:
      return false;
    case ast::NodeType::Const:
      return true;
    case ast::NodeType::Reference:
      return true;
    case ast::NodeType::EnumValue:
      return false;
    case ast::NodeType::Id:
      return true;
    case ast::NodeType::IdQualified:
      return true;
    case ast::NodeType::AssignStmt:
      return false;
    case ast::NodeType::VarDecl:
      return false;
    case ast::NodeType::FuncDecl:
      return false;
    case ast::NodeType::FuncArgList:
      return false;
    case ast::NodeType::FuncArg:
      return false;
    case ast::NodeType::FuncParamList:
      return false;
    case ast::NodeType::LocalScope:
      return false;
    case ast::NodeType::Subscript:
      return false;
    case ast::NodeType::AttrList:
      return false;
    case ast::NodeType::Attr:
      return true;
    case ast::NodeType::Expr:
      return false;
    case ast::NodeType::TemplateDecl:
      return false;
    case ast::NodeType::TemplateArgList:
      return false;
    case ast::NodeType::TemplateParamList:
      return false;
    case ast::NodeType::TemplateSpec:
      return false;
    case ast::NodeType::TemplateInst:
      return false;
    case ast::NodeType::TemplateArg:
      return false;
    case ast::NodeType::TemplateExplicit:
      return false;
    case ast::NodeType::ReturnStmt:
      return false;
    case ast::NodeType::UsingStmt:
      return false;
    case ast::NodeType::FuncForwardDecl:
      return false;
    case ast::NodeType::LocalStmt:
      return false;
    case ast::NodeType::FuncCall:
      return false;
    case ast::NodeType::LocalVar:
      return false;
    case ast::NodeType::Switch:
      return false;
    case ast::NodeType::SwitchCase:
      return false;
    case ast::NodeType::ForLoop:
      return false;
    case ast::NodeType::WhileLoop:
      return false;
    case ast::NodeType::DoWhileLoop:
      return false;
    case ast::NodeType::Condition:
      return false;
    case ast::NodeType::IfStmt:
      return false;
    case ast::NodeType::ElseIfStmt:
      return false;
    case ast::NodeType::ElseStmt:
      return false;
    case ast::NodeType::InitializerList:
      return false;
    case ast::NodeType::Initializer:
      return false;
    case ast::NodeType::DesignatedInitializer:
      return false;
    case ast::NodeType::StringConst:
      return true;
    case ast::NodeType::NumConst:
      return true;
    case ast::NodeType::Op:
      return true;
    case ast::NodeType::PipelineDecl:
      return false;
    case ast::NodeType::Constructor:
      return false;
    case ast::NodeType::Declarator:
      return false;
  }
  return false;
}

}  // namespace blender::gpu::shader::parser::ast
