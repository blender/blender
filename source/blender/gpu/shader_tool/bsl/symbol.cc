/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "processor.hh"
#include "symbol_table.hh"
#include "type_checker.hh"

namespace bsl {

using namespace blender::gpu::shader::parser;
using namespace blender::gpu::shader::parser::ast;
using namespace std;
using Token = blender::gpu::shader::parser::Token;

static const char *ns_sep = blender::gpu::shader::SourceProcessor::namespace_separator;

struct SymbolParser : NodeErrorHandler {
  SymbolTable &table;

  SymbolParser(ErrorHandler &handler, SymbolTable &table) : NodeErrorHandler(handler), table(table)
  {
  }

  void parse_scope(SymbolScope &scope,
                   Node node,
                   const std::string &prefix,
                   TemplateInst template_inst = {})
  {
    Node parent = node.parent();

    if (parent.parent().type() == NodeType::TemplateDecl) {
      parse_template_arguments(scope, parent.parent(), template_inst);
    }

    if (parent.type() == NodeType::FuncDecl) {
      parse_function_return_type(scope, parent);
      parse_function_arguments(scope, parent);
    }
    if (parent.type() == NodeType::ForLoop) {
      parse_loop_arguments(scope, parent);
    }

    int offset = 0;

    /* For enums, the value of the last declaration. */
    ConstexprValue enum_last_val = -1;

    if (node.type() == NodeType::FuncDecl || node.type() == NodeType::ClassDecl ||
        node.type() == NodeType::LocalScope)
    {
      for (Node child = node.child_first(); child.is_valid(); child = child.next()) {
        switch (child.type()) {
          case NodeType::Preprocessor:
            parse_preprocessor(scope, child);
            break;
          case NodeType::Namespace:
            parse_namespace(scope, child, prefix);
            break;
          case NodeType::UsingStmt:
            parse_using_stmt(scope, child);
            break;
          case NodeType::LocalScope:
            parse_local_scope(scope, child, prefix);
            break;
          case NodeType::ClassDecl:
            parse_class_decl(scope, child, parent, offset, prefix);
            break;
          case NodeType::EnumValue:
            parse_enum_value(scope, child, parent, enum_last_val, prefix);
            break;
          case NodeType::FuncForwardDecl:
            parse_func_forward_decl(scope, child);
            break;
          case NodeType::FuncDecl:
            parse_func_decl(scope, child, prefix);
            break;
          case NodeType::VarDecl:
            parse_var_decl(scope, child, offset, prefix);
            break;
          case NodeType::StructuredBinding:
            parse_structured_binding(scope, child);
            break;
          case NodeType::TemplateDecl:
            parse_template_decl(scope, child);
            break;
          case NodeType::TemplateInst:
            parse_template_inst(scope, child, prefix);
            break;
          case NodeType::TemplateSpec:
            parse_template_spec(scope, child, prefix);
            break;
          case NodeType::SwitchStmt:
            /* Special case for switch statements which are 3 level deep. */
            for (SwitchCase stmt : child.children_of_type<SwitchCase>()) {
              for (LocalScope local : stmt.children_of_type<LocalScope>()) {
                parse_local_scope(scope, local, prefix);
              }
            }
            break;
          case NodeType::ForLoop:
          case NodeType::DoWhileLoop:
          case NodeType::WhileLoop:
          case NodeType::IfStmt:
          case NodeType::ElseIfStmt:
          case NodeType::ElseStmt:
          case NodeType::ReturnStmt:
            /* Convert local scopes that are nested one level deeper (eg.  if, etc...). */
            for (LocalScope local : child.children_of_type<LocalScope>()) {
              parse_local_scope(scope, local, prefix);
            }
            break;
          case NodeType::PipelineDecl:
            parse_pipeline_decl(scope, child, prefix);
            break;
          default:
            break;
        }
      }
    }
  }

  void parse_local_scope(SymbolScope &scope, LocalScope local_node, const std::string &prefix)
  {
    SymbolScope *sym = table.scp_arena.alloc(&scope, local_node);
    scope.scopes.emplace(sym->identifier, sym);

    parse_scope(*sym, local_node, prefix + sym->identifier + ns_sep);
  }

  /* Create declaration for each template argument so name lookup will work with these. */
  void parse_template_arguments(SymbolScope &scope, TemplateDecl decl, TemplateInst inst)
  {
    TemplateParamList params = inst.parameters();
    Expr param = params.child_first();

    assert(scope.parent != nullptr);
    /* Search for types in the parent namespace, where the instantiation resides. */
    SymbolScope &inst_scope = *scope.parent;

    for (TemplateArg arg : decl.arguments().children_range()) {
      assert(arg.is_valid());
      if (arg.front() == Typename) {
        IdQualified arg_id = arg.type();
        string id(arg_id.str());
        if (param.child_count() != 1 || param.child_first().type() != NodeType::LocalVar) {
          error(param, Diag::TemplateInvalidArg, id);
          return;
        }
        IdQualified type_id(LocalVar(param.child_first()).identifier());
        SymbolClass *resolved = inst_scope.lookup_class(table, type_id).unwrap(this);
        /* IMPORTANT: Instantiate at argument declaration. Allow correct lookup. */
        scope.classes.emplace(id, resolved, arg.identifier().front());
      }
      else {
        IdQualified type = arg.type();
        string id(arg.identifier().str());
        SymbolClass *cls = inst_scope.lookup_class(table, type).unwrap(this);
        SymbolVariable *var = table.var_arena.alloc(&scope, cls, arg.identifier().front(), id);
        auto val = table.expr_type_analysis(scope, param.child_first()).unwrap(this);
        if (!val.is_constexpr()) {
          error(param, Diag::TemplateParameterNotConstexpr);
          return;
        }
        var->is_constexpr = true;
        var->value = val.value;
        if (scope.variable_emplace(id, var)) {
          error(var->loc.tok, Diag::Redefinition, id);
        }
      }
      param = param.next();
    }
  }

  void parse_function_return_type(SymbolScope &scope, FuncDecl decl)
  {
    SymbolFunction *fn_sym = static_cast<SymbolFunction *>(&scope);
    fn_sym->return_type =
        fn_sym->lookup_class(table, decl.return_type().identifier()).unwrap(this);

    if (fn_sym->is_entry_point() && fn_sym->return_type != table.void_cls) {
      error(decl.return_type(), Diag::EntryPointVoidReturn);
    }
  }

  /* Create declaration for each function argument so name lookup will work with these.
   * Also create declaration for `this_`. */
  void parse_function_arguments(SymbolScope &scope, FuncDecl decl)
  {
    SymbolFunction *fn_sym = static_cast<SymbolFunction *>(&scope);
    const bool is_entry_point = fn_sym->is_entry_point();
    const auto entry_point_type = fn_sym->entry_point_type;

    for (FuncArg arg : decl.arguments().children_of_type<FuncArg>()) {
      auto attr = resource_type_from_attributes(arg.attributes()).unwrap(this);

      SymbolClass *type = scope.lookup_class(table, arg.type().identifier()).unwrap(this);
      if (type->is_error) {
        error(
            arg.type().identifier(), Diag::UnknownTypeNameWithArg, arg.type().identifier().str());
      }
      SymbolVariable *var = table.var_arena.alloc(&scope, type, arg.declarator(), table);
      if (scope.variable_emplace(var)) {
        error(var->loc.tok, Diag::Redefinition, var->identifier);
      }
      /* Register argument type for argument resolution. */
      fn_sym->add_argument(type, arg.declarator().initial_value().expr());

      switch (attr.res_type) {
        case ResourceType::BASE_INSTANCE:
          var->identifier = "gpu_BaseInstance";
          break;
        case ResourceType::FRONT_FACING:
          var->identifier = "gl_FrontFacing";
          break;
        case ResourceType::INSTANCE_ID:
          var->identifier = "gl_InstanceID";
          break;
        case ResourceType::POINT_SIZE:
          var->identifier = "gl_PointSize";
          break;
        case ResourceType::CLIP_DISTANCES:
          var->identifier = "gl_ClipDistance";
          break;
        case ResourceType::LAYER:
          var->identifier = "gl_Layer";
          break;
        case ResourceType::INSTANCE_INDEX:
          var->identifier = "gpu_InstanceIndex";
          break;
        case ResourceType::VIEWPORT_INDEX:
          var->identifier = "gpu_ViewportIndex";
          break;
        case ResourceType::VERTEX_ID:
          var->identifier = "gl_VertexID";
          break;
        case ResourceType::POSITION:
          var->identifier = "gl_Position";
          break;
        case ResourceType::FRAG_DEPTH:
          var->identifier = "gl_FragDepth";
          break;
        case ResourceType::FRAG_COORD:
          var->identifier = "gl_FragCoord";
          break;
        case ResourceType::FRAG_STENCIL_REF:
          var->identifier = "gl_FragStencilRefARB";
          break;
        case ResourceType::POINT_COORD:
          var->identifier = "gl_PointCoord";
          break;
        case ResourceType::LOCAL_INVOCATION_ID:
          var->identifier = "gl_LocalInvocationID";
          break;
        case ResourceType::LOCAL_INVOCATION_INDEX:
          var->identifier = "gl_LocalInvocationIndex";
          break;
        case ResourceType::GLOBAL_INVOCATION_ID:
          var->identifier = "gl_GlobalInvocationID";
          break;
        case ResourceType::WORK_GROUP_ID:
          var->identifier = "gl_WorkGroupID";
          break;
        case ResourceType::NUM_WORK_GROUP:
          var->identifier = "gl_NumWorkGroups";
          break;
        case ResourceType::RESOURCE_TABLE:
          if (type->srt_type != ResourceTableType::RESOURCE_TABLE) {
            error(arg.type(),
                  Diag::ExpectedResourceOfType,
                  to_str(ResourceTableType::RESOURCE_TABLE),
                  to_str(type->srt_type));
          }
          break;
        case ResourceType::OUT:
          switch (entry_point_type) {
            case SymbolFunction::EntryPointType::COMP:
              error(arg.type(), Diag::AttributeNotAllowedInComp, "out");
              break;
            case SymbolFunction::EntryPointType::FRAG:
              if (type->srt_type != ResourceTableType::FRAGMENT_OUT) {
                error(arg.type().identifier(),
                      Diag::ExpectedResourceOfType,
                      to_str(ResourceTableType::FRAGMENT_OUT),
                      to_str(type->srt_type));
              }
              break;
            case SymbolFunction::EntryPointType::VERT:
              if (type->srt_type != ResourceTableType::VERTEX_OUT) {
                error(arg.type().identifier(),
                      Diag::ExpectedResourceOfType,
                      to_str(ResourceTableType::VERTEX_IN),
                      to_str(type->srt_type));
              }
              break;
            case SymbolFunction::EntryPointType::NONE:
              error(arg.type().identifier(), Diag::AttributeOnlyEntryPointsCanUseOut);
              break;
          }
          break;
        case ResourceType::IN:
          switch (entry_point_type) {
            case SymbolFunction::EntryPointType::COMP:
              error(arg.type().identifier(), Diag::AttributeNotAllowedInComp, "in");
              break;
            case SymbolFunction::EntryPointType::FRAG:
              if (type->srt_type != ResourceTableType::FRAGMENT_IN &&
                  type->srt_type != ResourceTableType::VERTEX_OUT)
              {
                error(arg.type().identifier(),
                      Diag::ExpectedResourceOfEitherType,
                      to_str(ResourceTableType::FRAGMENT_IN),
                      to_str(ResourceTableType::VERTEX_OUT),
                      to_str(type->srt_type));
              }
              break;
            case SymbolFunction::EntryPointType::VERT:
              if (type->srt_type != ResourceTableType::VERTEX_IN) {
                error(arg.type().identifier(),
                      Diag::ExpectedResourceOfType,
                      to_str(ResourceTableType::VERTEX_IN),
                      to_str(type->srt_type));
              }
              break;
            case SymbolFunction::EntryPointType::NONE:
              error(arg.type().identifier(), Diag::AttributeOnlyEntryPointsCanUseIn);
              break;
          }
          break;
        default:
          break;
      }

      if (is_entry_point && attr.srt_type != ResourceTableType::ENTRY_POINT &&
          attr.res_type != ResourceType::RESOURCE_TABLE)
      {
        error(arg, Diag::ExpectedEntryPointResourceAttribute, to_str(attr.srt_type));
      }
      else if (!is_entry_point && attr.srt_type != ResourceTableType::NONE &&
               attr.res_type != ResourceType::RESOURCE_TABLE)
      {
        error(arg, Diag::ResourceAttributesOnlyOnEntryPointArgs);
      }
    }

    if (scope.parent != nullptr) {
      /* Add `this_` to the var declarations. */
      auto &fns = scope.parent->functions;
      if (auto it = fns.find(scope.identifier); it != fns.end()) {
        SymbolFunction *fn = it->second.second;
        if (fn->fn_type == SymbolFunction::MEMBER) {
          SymbolVariable *var = table.var_arena.alloc(
              &scope, fn->parent_class(), decl.front(), "this_");
          scope.variable_emplace(var);
        }
      }
    }
  }

  /* Parse declarators inside for loop conditions. */
  void parse_loop_arguments(SymbolScope &scope, ForLoop loop)
  {
    for (VarDecl var : loop.condition().descendants_of_type<VarDecl>()) {
      SymbolClass *type = scope.lookup_class(table, var.type().identifier()).unwrap(this);
      for (Declarator decl : var.children_of_type<Declarator>()) {
        SymbolVariable *sym = table.var_arena.alloc(&scope, type, decl, table);
        sym->type = type;
        if (scope.variable_emplace(sym)) {
          NOTE(scope.variables.find(sym->identifier)->second.first.tok,
               Diag::NotePreviousDefinition,
               sym->identifier);
          error(sym->loc.tok, Diag::Redefinition, sym->identifier);
        }
      }
    }
  }

  void parse_preprocessor(SymbolScope & /*scope*/, Preprocessor directive)
  {
    /* Keep track of all defines with value to make sure to not hit an undefined symbol error. */
    Token type = directive.front().next();
    if (type.str() == "define") {
      Token name = type.next();
      if (name == directive.back()) {
        /* Doesn't define anything. */
        return;
      }
      Token value = name.next();
      bool is_func = false;
      if (value == '(' && name.str_with_whitespace().back() != ' ') {
        /* This is a functional macro. */
        is_func = true;
        value = value.next(')');
        if (value == directive.back()) {
          /* Doesn't define anything. */
          return;
        }
        value = value.next();
      }

      SymbolScope *root = table.root;
      /* Macro is not empty and defines something. Assume it is a int constant. */
      if (is_func) {
        SymbolFunction *fn = table.fun_arena.alloc(
            root, name, table.int_cls, string(name.str()), SymbolFunction::GLOBAL);

        root->functions.emplace(fn);
      }
      else {
        SymbolClass *type = table.int_cls;
        if (directive.back() == value) {
          if (value == Number ||
              (value == Word && (value.str() == "true" || value.str() == "false")))
          {
            /* Value is a single token try to guess its type. */
            type = table.get_literal_type(value.str());
          }
          else if (value == Word) {
            /* Try to match type. Workaround for legacy type alias. */
            SymbolClass *cls = nullptr;
            if (value.str() == "int") {
              cls = table.int_cls;
            }
            else if (value.str() == "uint") {
              cls = table.uint_cls;
            }
            else if (value.str() == "float") {
              cls = table.float_cls;
            }
            else if (value.str() == "bool") {
              cls = table.bool_cls;
            }
            if (cls) {
              /* TODO: Make a warning/error if this just defines a type alias. */
              root->classes.emplace(string(name.str()), cls);
            }
          }
        }
        SymbolVariable *var = table.var_arena.alloc(root, type, name, string(name.str()));
        root->variable_emplace(var);
      }
      /* TODO: Make a warning/error if this just defines a constant in global space without #if
       * directives. */
    }
  }

  void parse_namespace(SymbolScope &scope, ast::Namespace decl, string ns_prefix)
  {
    IdQualified ns = decl.identifier();
    /* Walk nested identifier */
    SymbolScope *ns_scope = &scope;
    for (Id id : ns.children_of_type<Id>()) {
      /* In case the namespace doesn't exist, create it. */
      SymbolScope *sym = table.scp_arena.alloc(ns_scope, id, SymbolScope::NAMESPACE);
      ns_scope = ns_scope->scopes.try_emplace(sym->identifier, sym).first->second;
      ns_prefix = ns_prefix + sym->identifier + ns_sep;
    }
    parse_scope(*ns_scope, decl.body(), ns_prefix);
  }

  void parse_using_stmt(SymbolScope &scope, UsingStmt stmt)
  {
    if (stmt.is_namespace()) {
      error(stmt, Diag::UsingNamespaceNotSupported);
    }
    else {
      IdQualified aliased(stmt.aliased());
      IdQualified name(stmt.identifier());
      string identifier(name.name().str());
      SymbolFunction *fun = scope.lookup_function(table, aliased).unwrap(this);
      if (!fun->is_error) {
        scope.functions.emplace(identifier, fun, stmt.identifier().front());
        return;
      }
      SymbolClass *type = scope.lookup_class(table, aliased).unwrap(this);
      if (type->is_error && fun->is_error) {
        error(aliased, Diag::UnknownTypeName);
        return;
      }
      if (type) {
        scope.classes.emplace(identifier, type, stmt.identifier().front());
        return;
      }
    }
  }

  void parse_enum_value(SymbolScope &scope,
                        EnumValue val,
                        ClassDecl cls,
                        ConstexprValue &enum_last_val,
                        const string &prefix)
  {
    SymbolClass *type = scope.lookup_class(table, cls.parent_class()).unwrap(this);

    SymbolVariable *sym = table.var_arena.alloc(&scope, type, val);
    sym->is_static = true;
    sym->is_constexpr = true;
    /* Resolve its value. */
    if (AssignStmt assign = val.value(); assign.is_valid()) {
      if (auto result = initialize_assign(scope, type, assign); result.is_constexpr()) {
        sym->value = result.value;
      }
      else {
        error(assign, Diag::ExprNotIntegralConstant);
      }
    }
    else {
      sym->value = std::visit([](auto &&v) -> ConstexprValue { return v + 1; }, enum_last_val);
    }

    if (scope.variable_emplace(sym)) {
      NOTE(scope.variables.find(sym->identifier)->second.first.tok,
           Diag::NotePreviousDefinition,
           sym->identifier);
      error(sym->loc.tok, Diag::Redefinition, sym->identifier);
    }
    /* Save value for the next member. */
    enum_last_val = sym->value;
    /* Alias to the parent namespace for non-anonymous, non-class enum. */
    if (!cls.is_enum_class()) {
      if (scope.parent->variable_emplace(sym)) {
        NOTE(scope.variables.find(sym->identifier)->second.first.tok,
             Diag::NotePreviousDefinition,
             sym->identifier);
        error(sym->loc.tok, Diag::Redefinition, sym->identifier);
      }
    }
    /* Set resolved identifier. */
    sym->identifier = prefix + sym->identifier;
  }

  SymbolClass *parse_class_decl(SymbolScope &scope,
                                ClassDecl decl,
                                ClassDecl parent_class,
                                int &offset,
                                const std::string &prefix,
                                const std::string &suffix = "",
                                optional<SourceLocation> poi = nullopt,
                                TemplateInst temp = {},
                                SymbolClassTemplate *cls_template = nullptr)
  {
    SymbolClass *cls = table.cls_arena.alloc(&scope, decl, suffix);
    if (!scope.classes.emplace(cls).second) {
      error(decl, Diag::Redefinition, cls->identifier);
      return table.err_cls;
    }
    if (poi) {
      cls->poi = poi;
    }
    else if (scope.poi) {
      /* Classes inherit the point of instantiation of their parent scope. */
      cls->poi = scope.poi;
    }

    scope.scopes.emplace(cls->identifier, cls);

    if (cls->is_anonymous && anonymous_scope_prefix(cls).non_anonymous_parent == nullptr) {
      error(decl, Diag::AnonymousUnionNotSupportedAtNamespaceScope);
      return table.err_cls;
    }

    if (cls_template) {
      /* Add the template definition early so it can be queried during instantiation. */
      cls_template->instances.emplace(suffix, cls);
      /* Create an alias to the unspecified class name (no template arg): e.g. `A` -> `A<T>`. */
      string alias_name = cls->identifier.substr(0, cls->identifier.size() - suffix.size());
      cls->classes.emplace(alias_name, cls);
    }

    parse_scope(*cls, decl.body(), prefix + cls->identifier + ns_sep, temp);

    /* Set identifier to resolved symbol. */
    cls->identifier = prefix + cls->identifier;

    if (cls->is_enum) {
      SymbolClass *underlying_type = scope.lookup_class(table, decl.parent_class()).unwrap(this);
      cls->size = underlying_type->size;
      cls->align = underlying_type->align;
      cls->builtin_class = underlying_type->builtin_class;
      cls->is_std140_compatible = cls->size == 4;
      cls->is_std430_compatible = cls->size == 4;

      /* Create scalar constructor. */
      SymbolFunction *ctor = table.fun_arena.alloc(
          &scope, decl.front(), cls, cls->original, SymbolFunction::Type::GLOBAL);
      ctor->add_argument(cls);
      ctor->is_builtin = true;
      scope.function_emplace(ctor, true);
      ctor->identifier = prefix + ctor->identifier;
    }
    else {
      cls->ensure_size_and_align();
    }

    if (cls->is_anonymous) {
      /* Instantiate into parent class as member. */
      SymbolVariable *var = table.var_arena.alloc(
          &scope, cls, decl.front(), cls->original + ns_sep);
      if (scope.variable_emplace(cls->original, var)) {
        error(var->loc.tok, Diag::Redefinition, cls->original);
      }

      var->type = cls;
      var->set_offset(parent_class.is_union(), offset);
    }
    return cls;
  }

  void parse_func_forward_decl(SymbolScope &scope, FuncForwardDecl decl)
  {
    /* Record a function prototype. */
    scope.function_prototypes.emplace_back(decl);
  }

  SymbolFunction *parse_func_decl(SymbolScope &scope,
                                  FuncDecl func,
                                  const std::string &prefix,
                                  const std::string &suffix = "",
                                  optional<SourceLocation> poi = nullopt,
                                  TemplateInst temp = {})
  {
    /* Set return type to error type since we need to parse it after template argument
     * instantiation. */
    SymbolFunction *fn = table.fun_arena.alloc(&scope, table.err_cls, func, suffix);
    if (FuncForwardDecl fdecl = scope.lookup_function_forward_decl(func); fdecl.is_valid()) {
      /* Modify symbol location if it is forward declared. */
      fn->loc = fdecl.front();
    }
    if (poi) {
      fn->poi = poi;
    }
    else if (scope.poi) {
      /* Functions inherit the point of instantiation of their class. */
      fn->poi = scope.poi;
    }
    bool is_overload = scope.function_emplace(fn);
    if (is_overload && fn->is_entry_point()) {
      error(func.identifier(), Diag::RedefinitionOfEntryPointFunction, func.identifier().str());
    }

    parse_scope(*fn, func.body(), prefix + fn->identifier + ns_sep, temp);

    /* Set resolved identifier. */
    switch (fn->fn_type) {
      case SymbolFunction::GLOBAL:
      case SymbolFunction::STATIC:
        fn->identifier = prefix + fn->identifier;
        break;
      case SymbolFunction::MEMBER:
        /* Member function use overload resolution. */
        fn->identifier = "_" + fn->identifier;
        break;
    }

    if (fn->is_inline && fn->return_type != table.void_cls) {
      /* Create return variable instance. To be used during inlining. */
      SymbolVariable *var = table.var_arena.alloc(
          fn, fn->return_type, fn->decl.front(), SymbolFunction::inline_fn_ret_id);
      if (fn->variable_emplace(SymbolFunction::inline_fn_ret_id, var)) {
        error(var->loc.tok, Diag::Redefinition, SymbolFunction::inline_fn_ret_id);
      }
    }

    return fn;
  }

  void parse_template_decl(SymbolScope &scope, TemplateDecl decl)
  {
    /* Do not parse. Only keep the symbol definition. The instantiation will do the parsing. */
    if (decl.is_function()) {
      FuncDecl fn_decl(decl.decl());
      /* Set return type to error type since we need to parse it after template argument
       * instantiation. */
      SymbolFunction *fn = table.fun_arena.alloc(&scope, table.err_cls, fn_decl);
      fn->is_complete = false;
      fn = scope.functions.emplace(fn).first;
      if (fn->template_data) {
        error(fn_decl.identifier(), Diag::RedefinitionTemplate, fn_decl.identifier().str());
      }
      else {
        fn->template_data = table.tmp_fun_arena.alloc(decl);
      }
    }
    else {
      SymbolClass *cls = table.cls_arena.alloc(&scope, decl.decl());
      cls->template_data = table.tmp_cls_arena.alloc(decl);
      if (!scope.classes.emplace(cls).second) {
        ClassDecl cls_decl(decl.decl());
        error(cls_decl.identifier(), Diag::Redefinition, cls_decl.identifier().str());
      }
    }
  }

  template<typename DeclAst, typename SymbolT>
  TemplateDecl check_template_instance(TemplateParamList temp_params,
                                       DeclAst &decl,
                                       const SymbolT &sym)
  {
    if (sym.template_data == nullptr) {
      error(decl.identifier(), Diag::TemplateMissingDefinition);
      return {};
    }
    TemplateDecl temp_decl = sym.template_data->decl;
    if (temp_decl.arguments().child_count() != temp_params.child_count()) {
      error(temp_params, Diag::TemplateParameterCountMismatch);
      return {};
    }
    return temp_decl;
  }

  void parse_template_inst(SymbolScope &scope, TemplateInst temp, const std::string &prefix)
  {
    TemplateParamList tmp_params = temp.parameters();

    if (temp.is_function()) {
      const FuncForwardDecl decl = temp.decl();
      const IdQualified id = decl.identifier();
      const SourceLocation poi = id.front();
      SymbolFunction *fn = scope.lookup_function_base(table, id);

      if (TemplateDecl temp_decl = check_template_instance(tmp_params, decl, *fn);
          temp_decl.is_valid())
      {
        auto mangled =
            table.mangle_identifier(temp_decl.arguments(), tmp_params, scope).unwrap(this);

        SymbolScope *parent = &scope;
        if (fn->fn_type == SymbolFunction::MEMBER) {
          parent = nullptr;
          Id start = id.namespace_start();
          Id last = id.child_last(NodeType::Id).prev(NodeType::Id);
          if (auto *parent_cls = scope.lookup_class(table, start, last).unwrap(this); parent_cls) {
            if (parent_cls->template_data && !last.template_params().is_valid()) {
              error(last, Diag::TemplateMissingParameters);
            }
            parent = parent_cls;
          }

          if (parent == nullptr) {
            error(id, Diag::TemplateMissingParentClassInstantiation, id.str());
            return;
          }
        }
        else {
          parent = fn->parent;
        }

        string full_id = string(id.name().str()) + "<" + mangled.str_debug.substr(2) + ">";
        NOTE(id, Diag::NoteInstantiationFuncTemplateRequested, full_id);
        SymbolFunction *fn_inst = parse_func_decl(
            *parent, temp_decl.decl(), prefix, mangled.str, poi, temp);
        fn_inst->is_specialization = true;
        fn_inst->template_data = fn->template_data;
        /* For compatibility with previous version of BSL, do not mangle function names with
         * arguments if ADL is possible. */
        if (fn->template_data->is_adl_possible()) {
          fn_inst->identifier = fn_inst->identifier.substr(
              0, fn_inst->identifier.size() - mangled.str.size());
        }
        fn->template_data->instances.emplace(mangled.str, fn_inst);
        fn->add_overload(fn_inst);
      }
    }
    else {
      const ClassDecl decl = temp.decl();
      const IdQualified id = decl.identifier();
      const SourceLocation poi = id.front();
      auto *cls = scope.lookup_class_base(table, id);

      if (TemplateDecl temp_decl = check_template_instance(tmp_params, decl, *cls);
          temp_decl.is_valid())
      {
        auto mangled =
            table.mangle_identifier(temp_decl.arguments(), tmp_params, scope).unwrap(this);
        string full_id = string(id.name().str()) + "<" + mangled.str_debug.substr(2) + ">";
        NOTE(id, Diag::NoteInstantiationClassTemplateRequested, full_id);
        int unused_offset = 0;
        parse_class_decl(scope,
                         temp_decl.decl(),
                         Node{},
                         unused_offset,
                         prefix,
                         mangled.str,
                         poi,
                         temp,
                         cls->template_data);
      }
    }
  }

  void parse_template_spec(SymbolScope &scope, TemplateSpec temp, const std::string &prefix)
  {
    TemplateParamList temp_params = temp.parameters();
    /* Instantiate the whole symbol into the current namespace. */
    if (temp.is_function()) {
      const FuncDecl decl = temp.decl();
      const IdQualified id = decl.identifier();
      const SourceLocation poi = id.front();
      auto *fn = scope.lookup_function_base(table, decl.identifier());

      if (TemplateDecl temp_decl = check_template_instance(temp_params, decl, *fn);
          temp_decl.is_valid())
      {
        auto mangled =
            table.mangle_identifier(temp_decl.arguments(), temp_params, scope).unwrap(this);
        string full_id = string(decl.identifier().str()) + "<" + mangled.str_debug.substr(2) + ">";
        NOTE(decl.identifier(), Diag::NoteInstantiationFuncTemplateRequested, full_id);
        SymbolFunction *spec = parse_func_decl(scope, decl, prefix, mangled.str, poi);
        spec->is_specialization = true;
        spec->template_data = fn->template_data;
        /* For compatibility with previous version of BSL, do not mangle function names with
         * arguments if ADL is possible. */
        if (fn->template_data->is_adl_possible()) {
          spec->identifier = spec->identifier.substr(0,
                                                     spec->identifier.size() - mangled.str.size());
        }
        fn->template_data->instances.emplace(mangled.str, spec);
        fn->add_overload(spec);
      }
    }
    else {
      const ClassDecl decl = temp.decl();
      const IdQualified id = decl.identifier();
      const SourceLocation poi = id.front();
      auto *cls = scope.lookup_class_base(table, decl.identifier());

      if (TemplateDecl temp_decl = check_template_instance(temp_params, decl, *cls);
          temp_decl.is_valid())
      {
        auto mangled =
            table.mangle_identifier(temp_decl.arguments(), temp_params, scope).unwrap(this);
        string full_id = string(decl.identifier().str()) + "<" + mangled.str_debug.substr(2) + ">";
        NOTE(decl.identifier(), Diag::NoteInstantiationClassTemplateRequested, full_id);
        int unused_offset = 0;
        SymbolClass *spec = parse_class_decl(
            scope, decl, Node{}, unused_offset, prefix, mangled.str, poi);
        cls->template_data->instances.emplace(mangled.str, spec);
      }
    }
  }

  void parse_pipeline_decl(SymbolScope &scope, PipelineDecl decl, const std::string &prefix)
  {
    SymbolVariable *sym = table.var_arena.alloc(
        &scope, table.void_cls, decl.front(), string(decl.identifier().str()));
    sym->is_static = true;
    if (scope.variable_emplace(sym)) {
      NOTE(scope.variables.find(sym->identifier)->second.first.tok,
           Diag::NotePreviousDefinition,
           sym->identifier);
      error(decl, Diag::Redefinition, sym->identifier);
    }
    /* Set resolved identifier. */
    sym->identifier = prefix + sym->identifier;
  }

  void parse_structured_binding(SymbolScope &scope, StructuredBinding decl)
  {
    AssignStmt assign = decl.assign();
    Expr expr = assign.expr();
    if (!expr.is_valid()) {
      error(decl, Diag::AutoTypeCannotBeDeduced);
      return;
    }

    SymbolClass *cls = table.expr_type_analysis(scope, expr.child_first()).unwrap(this).type;
    if (cls->is_error) {
      error(decl, Diag::AutoTypeCannotBeDeduced);
      return;
    }

    Id id = decl.child_first();

    /* Create temp variable to write to. */
    SymbolVariable *tmp = table.var_arena.alloc(&scope, cls, decl.front(), decl.tmp_id());
    if (scope.variable_emplace(tmp)) {
      NOTE(scope.variables.find(tmp->identifier)->second.first.tok,
           Diag::NotePreviousDefinition,
           tmp->identifier);
      error(decl, Diag::Redefinition, tmp->identifier);
    }

    for (SymbolVariable *var : cls->non_static_variables_in_declaration_order()) {
      if (!id.is_valid()) {
        error(decl, Diag::StructuredBindingNotEnoughNames);
        return;
      }
      /* Make copy of the variable in local scope. */
      SymbolVariable *sym = table.var_arena.alloc(*var);
      sym->identifier = id.str();
      sym->parent = &scope;
      if (scope.variable_emplace(sym)) {
        NOTE(scope.variables.find(sym->identifier)->second.first.tok,
             Diag::NotePreviousDefinition,
             sym->identifier);
        error(decl, Diag::Redefinition, sym->identifier);
      }
      /* Resolve to the tmp variable member. */
      sym->identifier = tmp->identifier + '.' + var->identifier;

      id = id.next();
    }
    if (id.is_valid()) {
      error(decl, Diag::StructuredBindingTooManyNames);
      return;
    }
  }

  void parse_var_decl(SymbolScope &scope, VarDecl var, int &offset, const std::string &prefix)
  {
    IdQualified type_id = var.type().identifier();
    string_view type_id_str = type_id.str();

    /* Assumes C++ compilation already checked that all declarator uses the same type.
     * Deduce type using only the first declarator. */
    Declarator first_decl = var.child_first(NodeType::Declarator);

    SymbolClass *type = ((type_id_str == "auto") ? table.resolve_auto_type(scope, first_decl) :
                                                   scope.lookup_class(table, type_id))
                            .unwrap(this);

    if (type->is_error) {
      error(var, Diag::UnknownTypeNameWithArg, type_id_str);
      return;
    }

    SymbolClass *cls = scope.as_class();

    auto attr = resource_type_from_attributes(var.attributes()).unwrap(this);

    if (attr.res_type == ResourceType::UNIFORM_BUF && !type->is_std140_compatible) {
      NOTE(type_id, Diag::NoteInUniformBufferDeclaration, type->original);
      type->ensure_size_and_align(this);
    }
    else if (attr.res_type == ResourceType::STORAGE_BUF && !type->is_std430_compatible) {
      NOTE(type_id, Diag::NoteInStorageBufferDeclaration, type->original);
      type->ensure_size_and_align(this, true);
    }

    if (cls) {
      if (cls->srt_type == ResourceTableType::NONE && attr.srt_type != ResourceTableType::NONE) {
        if (cls->is_anonymous) {
          error(var, Diag::ResourcesNotAllowedInAnonymousStruct);
        }
        if (attr.srt_type != ResourceTableType::VERTEX_IN &&
            attr.srt_type != ResourceTableType::VERTEX_OUT &&
            attr.srt_type != ResourceTableType::FRAGMENT_IN &&
            attr.srt_type != ResourceTableType::FRAGMENT_OUT &&
            attr.srt_type != ResourceTableType::RESOURCE_TABLE)
        {
          error(var, Diag::InvalidResourceTypeForResourceTableClass, to_str(cls->srt_type));
        }
        cls->srt_type = attr.srt_type;
      }
      else if (cls->srt_type != attr.srt_type) {
        error(var,
              Diag::ExpectedResourceDefinitionOfType,
              to_str(cls->srt_type),
              to_str(attr.srt_type));
      }
    }
    else if (attr.res_type == ResourceType::RESOURCE_TABLE) {
      /* Currently supported as noop for legacy resource table reference getter. */
    }
    else if (attr.res_type != ResourceType::NONE) {
      error(var, Diag::ResourceOutOfClassDeclaration);
    }

    const bool is_srt_local_ref = type->is_srt() && cls == nullptr;

    string anon_prefix;
    SymbolScope *named_parent = cls;
    if (cls && cls->is_anonymous) {
      auto prefix = anonymous_scope_prefix(cls);
      anon_prefix = prefix.prefix;
      named_parent = prefix.non_anonymous_parent;
    }

    const bool is_ref = var.is_reference();
    for (Declarator decl : var.children_of_type<Declarator>()) {
      if (is_ref != decl.is_reference()) {
        error(decl, Diag::ReferenceMixedVarDecl);
        continue;
      }

      SymbolVariable *sym = table.var_arena.alloc(&scope, type, decl, table);
      sym->res_type = attr.res_type;
      sym->is_static |= attr.res_type != ResourceType::NONE;
      if (scope.variable_emplace(sym)) {
        NOTE(scope.variables.find(sym->identifier)->second.first.tok,
             Diag::NotePreviousDefinition,
             sym->identifier);
        error(sym->loc.tok, Diag::Redefinition, sym->identifier);
      }

      if (is_srt_local_ref && !is_ref) {
        error(decl, Diag::ResourceTableMustBeReference);
      }

      /* Local References. */
      if (is_ref && cls == nullptr) {
        AssignStmt stmt = decl.initial_value();
        if (stmt.is_valid() && stmt.expr().is_valid()) {
          /* Run analysis to check if we bind a temporary. */
          /* Run const analysis to check if we bind a const expr. */
          ExpressionResult result =
              table.expr_type_analysis(scope, stmt.expr().child_first(), true).unwrap(this);

          if (result.is_temporary && !result.type->is_srt()) {
            error(decl, Diag::ReferenceCannotBindTemporary);
          }
          else if (type != result.type) {
            error(decl, Diag::ReferenceTypeMismatch, type->identifier, result.type->identifier);
          }

          sym->reference_value = stmt.expr();
          sym->array_dimensions = result.array_dim;
        }
        else {
          error(decl, Diag::ReferenceVariableRequiresInitializer, decl.identifier().str());
        }
      }

      if (cls) {
        sym->set_offset(cls->is_union, offset);

        /* Alias to the first, non-anonymous parent. */
        if (cls->is_anonymous) {
          if (named_parent->variable_emplace(sym)) {
            error(sym->loc.tok, Diag::Redefinition, sym->identifier);
          }
        }

        if (cls->is_anonymous || cls->is_union) {
          /* Append accessor function. */
          string suffix = cls->is_union ? "()" : "";
          string prefix = cls->is_union ? "_" : "";
          /* Rename to the resolved symbol. */
          sym->identifier = anon_prefix + prefix + sym->identifier + suffix;

          if (sym->is_static) {
            error(var,
                  Diag::StaticDataMemberNotAllowed,
                  decl.identifier().str(),
                  (cls->is_anonymous ? "anonymous" : "local"),
                  (cls->is_union ? "union" : "struct"));
          }
        }
      }

      if (sym->is_static) {
        if (attr.res_type == ResourceType::VERT_ATTR_OUT ||
            attr.res_type == ResourceType::FRAG_OUT)
        {
          sym->identifier = prefix + sym->identifier;
        }
      }

      if (sym->is_constexpr) {
        ExpressionResult result = initialize_constexpr(scope, sym->type, decl);
        /* For now demote constexpr if we couldn't deduce its value. */
        if (result.is_constexpr()) {
          sym->value = result.value;
        }

        if (!sym->is_static) {
          if (sym->parent_class()) {
            error(var.type(), Diag::ConstexprMemberNonStatic);
          }
          else if (sym->parent->parent_function()) {
            /* Only case where its fine. */
          }
          else {
            /* Could be allowed if compiler can make them also static.
             * This is required for MSL where we have a wrapper class. */
            error(var.type(), Diag::ConstexprGlobalNonStatic);
          }
        }
      }
    }
  }

  ExpressionResult eval_scalar_initializer_list(const SymbolScope &scope,
                                                SymbolClass *cls,
                                                InitializerList list)
  {
    if (list.is_empty()) {
      return cls;
    }
    if (list.child_count() != 1) {
      error(list, Diag::ExpectedScalarInitializer);
      return cls;
    }
    if (Initializer init = list.child_first(); init.is_valid()) {
      Node node = init.child_first();
      if (node.type() == NodeType::InitializerList) {
        return eval_scalar_initializer_list(scope, cls, node);
      }
      return table.expr_type_analysis(scope, node.child_first()).unwrap(this);
    }
    return cls;
  }

  ExpressionResult initialize_assign(const SymbolScope &scope, SymbolClass *cls, AssignStmt assign)
  {
    ExpressionResult result = cls;
    if (InitializerList list = assign.initializer_list(); list.is_valid()) {
      result = eval_scalar_initializer_list(scope, cls, list);
    }
    else {
      result = table.expr_type_analysis(scope, assign.expr().child_first()).unwrap(this);
    }
    return result;
  }

  ExpressionResult initialize_constexpr(const SymbolScope &scope,
                                        SymbolClass *cls,
                                        Declarator decl)
  {
    if (cls != table.int_cls && cls != table.uint_cls && cls != table.bool_cls &&
        cls != table.float_cls)
    {
      error(decl, Diag::ConstexprVarMustBeIntOrUint);
      return {table.err_cls};
    }

    ExpressionResult result = cls;
    if (AssignStmt assign = decl.initial_value(); assign.is_valid()) {
      result = initialize_assign(scope, cls, assign);
    }
    else if (InitializerList list = decl.initializer_list(); list.is_valid()) {
      result = eval_scalar_initializer_list(scope, cls, list);
    }

    if (!result.is_constexpr()) {
      error(decl, Diag::ConstexprVarMustBeInitializedByConstantExpr, decl.identifier().str());
    }
    return result;
  }

  struct AnonScopePrefix {
    string prefix;
    SymbolScope *non_anonymous_parent;
  };

  static AnonScopePrefix anonymous_scope_prefix(SymbolClass *cls)
  {
    string prefix;
    SymbolClass *parent = nullptr;
    /* Support multiple nesting level. */
    do {
      /* Union members need to go through the getter functions. */
      parent = cls->parent->as_class();
      bool access = parent && parent->is_union && parent->is_anonymous;
      prefix = (access ? "_" : "") + cls->original + "_" + (access ? "()" : "") + "." + prefix;
    } while ((cls = parent, cls && cls->is_anonymous));

    return {prefix, cls};
  }
};  // namespace blender::gpu::shader::parser

void SymbolTable::parse(LocalScope node, ErrorHandler &err_handler)
{
  SymbolParser symbol_parser(err_handler, *this);
  symbol_parser.parse_scope(*root, node, "");

  if (err_handler.err.has_value()) {
    throw ParserException();
  }
}

}  // namespace bsl
