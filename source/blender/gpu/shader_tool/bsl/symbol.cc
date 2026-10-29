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

/* Prefix of symbols inside a namespace or class scope, e.g. `a_b_` for `namespace a::b`. */
static std::string scope_prefix(const SymbolScope &scope)
{
  std::string prefix;
  const SymbolScope *parent = &scope;
  /* Namespace identifiers are not prefixed by their parent namespaces. */
  for (; parent && parent->type == SymbolScope::NAMESPACE; parent = parent->parent) {
    prefix = parent->identifier + ns_sep + prefix;
  }
  /* Class identifiers are already fully prefixed. */
  if (parent && parent->type == SymbolScope::CLASS) {
    prefix = parent->identifier + ns_sep + prefix;
  }
  return prefix;
}

struct SymbolParser : NodeErrorHandler {
  SymbolTable &table;

  SymbolParser(ErrorHandler &handler, SymbolTable &table) : NodeErrorHandler(handler), table(table)
  {
  }

  void parse_scope(SymbolScope &scope,
                   Node node,
                   const std::string &prefix,
                   TemplateParamList template_params = {})
  {
    Node parent = node.parent();

    if (parent.parent().type() == NodeType::TemplateDecl) {
      parse_template_arguments(scope, parent.parent(), template_params);
    }

    if (parent.type() == NodeType::FuncDecl) {
      parse_function_return_type(scope, ast::FuncDecl(parent).return_type());
      parse_function_arguments(scope, ast::FuncDecl(parent).arguments());
    }
    if (parent.type() == NodeType::ForLoop) {
      parse_loop_arguments(scope, parent);
    }

    int offset = 0;
    int bitfield_offset = 0;
    SymbolVariable *bitfield_base = nullptr;

    /* For enums, the value of the last declaration. */
    ConstexprValue enum_last_val(-1);

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
            parse_var_decl(scope, child, offset, bitfield_offset, bitfield_base, prefix);
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
  void parse_template_arguments(SymbolScope &scope, TemplateDecl decl, TemplateParamList params)
  {
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
        scope.scopes.emplace(id, resolved);
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

  void parse_function_return_type(SymbolScope &scope, ast::IdType return_type)
  {
    SymbolFunction *fn_sym = static_cast<SymbolFunction *>(&scope);
    fn_sym->return_type = fn_sym->lookup_class(table, return_type.identifier()).unwrap(this);

    if (fn_sym->is_entry_point() && fn_sym->return_type != table.void_cls) {
      error(return_type, Diag::EntryPointVoidReturn);
    }
  }

  /* Create declaration for each function argument so name lookup will work with these.
   * Also create declaration for `this_`. */
  void parse_function_arguments(SymbolScope &scope, FuncArgList arg_list)
  {
    SymbolFunction *fn_sym = static_cast<SymbolFunction *>(&scope);
    const bool is_entry_point = fn_sym->is_entry_point();
    const auto entry_point_type = fn_sym->entry_point_type;

    for (FuncArg arg : arg_list.children_of_type<FuncArg>()) {
      auto attr = resource_type_from_attributes(arg.attributes()).unwrap(this);

      SymbolClass *type = scope.lookup_class(table, arg.type().identifier()).unwrap(this);
      if (type->is_error) {
        error(
            arg.type().identifier(), Diag::UnknownTypeNameWithArg, arg.type().identifier().str());
      }
      Declarator decl = arg.declarator();
      SymbolVariable *var = table.var_arena.alloc(&scope, type, decl, table);
      if (scope.variable_emplace(var)) {
        error(var->loc.tok, Diag::Redefinition, var->identifier);
      }
      /* Register argument type for argument resolution. */
      fn_sym->add_argument(arg.type().is_const(), type, decl.initial_value().expr());

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
        case ResourceType::BARY_COORD:
          var->identifier = "gpu_BaryCoord";
          break;
        case ResourceType::STENCIL_REF:
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
      else if (is_entry_point && attr.srt_type == ResourceTableType::ENTRY_POINT &&
               !fn_sym->is_compatible(attr.res_type))
      {
        error(arg,
              Diag::ResourceAttributesIncompatibleEntryPoint,
              var->original,
              to_str(attr.res_type),
              to_str(fn_sym->entry_point_type));
      }

      if (!is_entry_point && attr.condition.is_valid()) {
        error(arg, Diag::ConditionAttributeNotOnResource);
      }

      if (attr.condition.is_valid()) {
        var->condition = attr.condition;
      }
    }

    if (scope.parent != nullptr) {
      /* Add `this_` to the var declarations. */
      auto &fns = scope.parent->functions;
      if (auto it = fns.find(scope.identifier); it != fns.end()) {
        SymbolFunction *fn = it->second.second;
        if (fn->fn_type == SymbolFunction::MEMBER) {
          SymbolVariable *var = table.var_arena.alloc(
              &scope, fn->parent_class(), arg_list.front(), "this");
          var->is_ptr = true;
          var->is_this = true;
          scope.variable_emplace(var);
          /* Mangle the emitted token to avoid conflict with MSL. */
          var->identifier = "this_";
          /* Some syntax might have already been lowered and need access to "this_". Add alias. */
          /* TODO(fclem): Do not preprocess this. */
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
        var->is_macro = true;
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
      /* Note: Add unsigned to not change the type of the ConstexprValue. */
      sym->value = enum_last_val + ConstexprValue(1u);
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
    /* TODO(fclem): Not enabling this for non-class-enum to stay compatible with the older
     * compiler. */
    if (cls.is_enum_class()) {
      /* Set resolved identifier. */
      sym->identifier = prefix + sym->identifier;
    }
  }

  SymbolClass *parse_class_decl(SymbolScope &scope,
                                ClassDecl decl,
                                ClassDecl parent_class,
                                int &offset,
                                const std::string &prefix,
                                const std::string &suffix = "",
                                optional<SourceLocation> poi = nullopt,
                                TemplateParamList temp_params = {},
                                SymbolClassTemplate *cls_template = nullptr)
  {
    SymbolClass *cls = table.cls_arena.alloc(&scope, decl, suffix);
    if (auto [cls_ptr, inserted] = scope.classes.emplace(cls); !inserted) {
      if (cls_ptr->parent == &scope) {
        error(decl, Diag::Redefinition, cls->identifier);
        return table.err_cls;
      }
      /* Inherited class subclass hiding. */
      scope.classes.insert_override(cls);
    }
    if (poi) {
      cls->poi = poi;
    }
    else if (scope.poi) {
      /* Classes inherit the point of instantiation of their parent scope. */
      cls->poi = scope.poi;
    }

    scope.scopes.emplace(cls->identifier, cls);

    if (!cls->is_enum && cls->is_anonymous &&
        anonymous_scope_prefix(cls).non_anonymous_parent == nullptr)
    {
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

    /* Set identifier to resolved symbol. */
    cls->identifier = prefix + cls->identifier;

    if (IdQualified parent_id = decl.parent_class(); parent_id.is_valid()) {
      parse_inheritance(scope, cls, parent_id);
    }

    parse_scope(*cls, decl.body(), cls->identifier + ns_sep, temp_params);

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
      ctor->add_argument(true, cls);
      ctor->allow_vector_promotion = true;
      scope.function_emplace(ctor, true);
      ctor->identifier = prefix + ctor->identifier;
    }
    else {
      cls->ensure_size_and_align();

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
    }
    return cls;
  }

  void parse_inheritance(SymbolScope &scope, SymbolClass *cls, IdQualified parent_id)
  {
    while (parent_id.is_valid()) {
      SymbolClass *parent_cls = nullptr;
      SymbolClass *base_cls = scope.lookup_class_base(table, parent_id);
      TemplateParamList tmp_params = parent_id.template_params();
      if (base_cls->template_data) {
        /* Check if derived class is part of template arguments. */
        bool is_crtp = false;
        for (IdQualified param_id : tmp_params.descendants_of_type<IdQualified>()) {
          if (scope.lookup_class_base(table, param_id) == cls) {
            is_crtp = true;
            break;
          }
        }

        if (is_crtp) {
          /* Implicit instantiation. We cannot declare the derived class without first declaring
           * the base class which depends on the derived class (CRTP). */
          if (TemplateDecl temp_decl = check_template_instance(tmp_params, parent_id, *base_cls);
              temp_decl.is_valid())
          {
            IdQualified id = parent_id;
            const SourceLocation poi = id.front();
            auto mangled =
                table.mangle_identifier(temp_decl.arguments(), tmp_params, scope).unwrap(this);
            string full_id = string(id.name().str()) + "<" + mangled.str_debug.substr(2) + ">";
            NOTE(id, Diag::NoteInstantiationClassTemplateRequested, full_id);
            int unused_offset = 0;
            /* Shouldn't be required since its the same as the template definition.  */
            const char *prefix = "";
            parent_cls = parse_class_decl(scope,
                                          temp_decl.decl(),
                                          Node{},
                                          unused_offset,
                                          prefix,
                                          mangled.str,
                                          poi,
                                          tmp_params,
                                          base_cls->template_data);
            parent_cls->is_crtp = true;
            parent_cls->template_data = base_cls->template_data;
          }
        }
        else {
          /* Full template lookup. */
          parent_cls = scope.lookup_class(table, parent_id).unwrap(this);
        }
      }
      else {
        /* Not a template. */
        parent_cls = base_cls;
      }

      if (parent_cls->is_error) {
        error(parent_id, Diag::UnknownIdentifier);
      }
      else {
        cls->base_classes.emplace_back(parent_id, parent_cls);
        inherit_members(*cls, *parent_cls);
      }
      parent_id = parent_id.next();
    }
  }

  void inherit_members(SymbolClass &cls, const SymbolClass &parent)
  {
    for (auto it : parent.variables) {
      SymbolVariable *var = table.var_arena.alloc(*it.second.second);
      cls.variable_override(var->original, var);
      var->identifier = "_" + parent.identifier + "." + var->identifier;
    }
    for (auto it : parent.functions) {
      cls.functions.insert_override(it.first, it.second.second, it.second.first);
    }
    for (auto it : parent.classes) {
      cls.classes.insert_override(it.first, it.second.second, it.second.first);
    }
  }

  void parse_func_forward_decl(SymbolScope &scope, FuncForwardDecl decl)
  {
    /* Treat the forward declaration as a standard function declaration to parse its signature. */
    SymbolFunction *fn = table.fun_arena.alloc(&scope, table.err_cls, decl);
    /* Mark as forward declared so it doesn't conflict with or replace the actual definition. */
    fn->is_defined = false;
    fn->fwd = decl;

    if (scope.poi) {
      /* Functions inherit the point of instantiation of their class. */
      fn->poi = scope.poi;
    }
    /* Emplace the function into the scope so it becomes available during function lookups. */
    bool is_overload = scope.function_emplace(fn);
    if (is_overload && fn->is_entry_point()) {
      error(decl.identifier(), Diag::RedefinitionOfEntryPointFunction, decl.identifier().str());
    }
    /* Parse the return type and arguments to correctly populate matching criteria. */
    parse_function_return_type(*fn, decl.return_type());
    parse_function_arguments(*fn, decl.arguments());
    /* Record as function prototype. */
    scope.function_prototypes.emplace_back(decl, fn);
  }

  SymbolFunction *parse_func_decl(SymbolScope &def_scope,
                                  FuncDecl func,
                                  const std::string &prefix,
                                  const std::string &suffix = "",
                                  optional<SourceLocation> poi = nullopt,
                                  TemplateParamList temp_params = {})
  {
    SymbolFunction *fn = nullptr;

    IdQualified id = func.identifier();
    SymbolScope *ns = nullptr;
    if (id.has_namespace()) {
      /* Definition of a member function or function from a different namespace.
       * Try to query the function to find its namespace. */
      SymbolFunction *fn_prototype = def_scope.lookup_function(table, id).unwrap(this);
      if (!fn_prototype->is_error) {
        ns = fn_prototype->parent;
      }
      else {
        error(id, Diag::UnknownFunction, id.str());
      }
    }

    SymbolScope &scope = ns ? *ns : def_scope;
    if (auto [fdecl, fn_ptr] = scope.lookup_function_forward_decl(func); fdecl.is_valid()) {
      fn = fn_ptr;
      /* Modify symbol location if it is forward declared. */
      fn->loc = fdecl.front();
      fn->is_defined = true;
      fn->decl = func;
      /* Use attributes of the definition. */
      fn->is_inline = func.attributes().contains_attr("force_inline");
      /* Note: We discard the argument parsed by the forward declaration.
       * They are parsed again by `parse_scope`. */
      fn->variables.clear();
      fn->arg_const.clear();
      fn->arg_types.clear();
      fn->arg_defaults.clear();
    }
    else if (ns) {
      error(id.name(), Diag::FunctionOutOfClassDefinition, id.name().str(), ns->original);
      /* Still return a valid function to avoid crash. */
      fn = table.fun_arena.alloc(&scope, table.err_cls, func, suffix);
      return fn;
    }
    else {
      /* Set return type to error type since we need to parse it after template argument
       * instantiation. */
      fn = table.fun_arena.alloc(&scope, table.err_cls, func, suffix);

      bool is_overload = scope.function_emplace(fn);
      if (is_overload && fn->is_entry_point()) {
        error(func.identifier(), Diag::RedefinitionOfEntryPointFunction, func.identifier().str());
      }
    }

    if (poi) {
      fn->poi = poi;
    }
    else if (scope.poi) {
      /* Functions inherit the point of instantiation of their class. */
      fn->poi = scope.poi;
    }

    /* Out-of-class definitions use the prefix of their class, e.g. `A_s` for `int A::s()`. */
    std::string fn_prefix = ns ? scope_prefix(*ns) : prefix;

    parse_scope(*fn, func.body(), fn_prefix + fn->identifier + ns_sep, temp_params);

    /* Set resolved identifier. */
    switch (fn->fn_type) {
      case SymbolFunction::GLOBAL:
      case SymbolFunction::STATIC:
        fn->identifier = fn_prefix + fn->identifier;
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

  template<typename SymbolT>
  TemplateDecl check_template_instance(TemplateParamList temp_params,
                                       IdQualified id,
                                       const SymbolT &sym)
  {
    if (sym.template_data == nullptr) {
      error(id, Diag::TemplateMissingDefinition);
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

      if (TemplateDecl temp_decl = check_template_instance(tmp_params, decl.identifier(), *fn);
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
            *parent, temp_decl.decl(), prefix, mangled.str, poi, temp.parameters());
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

      if (TemplateDecl temp_decl = check_template_instance(tmp_params, decl.identifier(), *cls);
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
                         temp.parameters(),
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

      if (TemplateDecl temp_decl = check_template_instance(temp_params, decl.identifier(), *fn);
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

      if (TemplateDecl temp_decl = check_template_instance(temp_params, decl.identifier(), *cls);
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

  void parse_var_decl(SymbolScope &scope,
                      VarDecl var,
                      int &offset,
                      int &bitfield_offset,
                      SymbolVariable *&bitfield_base,
                      const std::string &prefix)
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

    if (first_decl.is_pointer() && first_decl.pointer().str().size() != 1) {
      error(first_decl.pointer(), Diag::PointerMultiIndirection);
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

    if (attr.condition.is_valid()) {
      if (attr.res_type == ResourceType::NONE) {
        error(var, Diag::ConditionAttributeNotOnResource);
      }
      else if (attr.res_type == ResourceType::SHARED) {
        error(var, Diag::ConditionAttributeUnsupported, to_str(attr.res_type));
      }
    }

    const bool is_srt_local_ref = type->is_srt() && cls == nullptr;

    string anon_prefix;
    SymbolScope *named_parent = cls;
    if (cls && cls->is_anonymous) {
      auto prefix = anonymous_scope_prefix(cls);
      anon_prefix = prefix.prefix;
      named_parent = prefix.non_anonymous_parent;
    }

    const bool is_ptr = var.is_pointer();
    const bool is_ref = var.is_reference();
    for (Declarator decl : var.children_of_type<Declarator>()) {
      /* Since we bake the pointer in the type we require that all declarators must be pointers or
       * none of them. */
      if (is_ptr != decl.is_pointer()) {
        error(decl, Diag::PointerMixedVarDecl);
        continue;
      }
      if (is_ref != decl.is_reference()) {
        error(decl, Diag::ReferenceMixedVarDecl);
        continue;
      }
      ast::BitField bitfield = decl.bitfield();

      SymbolVariable *sym = table.var_arena.alloc(&scope, type, decl, table);
      sym->res_type = attr.res_type;
      sym->is_static |= attr.res_type != ResourceType::NONE;
      if (scope.variable_emplace(sym)) {
        /* Check if redefinition is simply hiding a base class member. */
        SymbolVariable *other = scope.variables.find(sym->identifier)->second.second;
        if (scope.as_class() && scope.as_class() != other->parent_class()) {
          /* Hide base class member. */
          scope.variable_override(sym);
        }
        else {
          NOTE(scope.variables.find(sym->identifier)->second.first.tok,
               Diag::NotePreviousDefinition,
               sym->identifier);
          error(sym->loc.tok, Diag::Redefinition, sym->identifier);
        }
      }

      if (sym->is_ptr) {
        /* We only support single indirection pointers. */
        if (decl.pointer().str().size() != 1) {
          error(decl.pointer(), Diag::PointerMultiIndirection);
        }
        /* When declared inside a class, pointers are only allowed to be declared in resource
         * declarations. */
        if (cls && (attr.res_type != ResourceType::PUSH_CONST &&
                    attr.res_type != ResourceType::UNIFORM_BUF &&
                    attr.res_type != ResourceType::STORAGE_BUF))
        {
          error(decl.pointer(), Diag::PointerNotResource);
        }
      }

      if (is_srt_local_ref && !is_ref) {
        error(decl, Diag::ResourceTableMustBeReference);
      }

      /* Capacity attribute. */
      if (attr.capacity.is_valid()) {
        if (sym->array_dimensions != 1) {
          error(decl, Diag::CapacityArrayDimensionMismatch);
        }
        /* TODO(fclem): Check that this references a single reachable compilation constant. */
        sym->capacity_value = attr.capacity;
      }

      if (attr.condition.is_valid()) {
        sym->condition = attr.condition;
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
        if (sym->is_bitfield) {
          if ((sym->type != table.int_cls && sym->type != table.uint_cls) ||
              sym->array_dimensions != 0)
          {
            error(decl, Diag::BitFieldNotIntegral, sym->type->original);
          }
          const Node width = bitfield.expr().child_first();
          if (!width.is_valid()) {
            error(decl, Diag::ExprNotIntegralConstant);
            continue;
          }
          ExpressionResult result = table.expr_type_analysis(scope, width).unwrap(this);
          if (!result.is_constexpr()) {
            error(decl, Diag::ExprNotIntegralConstant);
          }
          int len = result.value.comp_as<int>(0);
          if (len > 32) {
            error(decl, Diag::BitFieldSizeTooLarge, to_string(len));
          }
          else if (len == 0) {
            error(decl, Diag::BitFieldSizeNull);
          }
          else if (len < 0) {
            error(decl, Diag::BitFieldSizeNegative, to_string(len));
          }
          else {
            if (bitfield_base && bitfield_offset + len <= 32 && !cls->is_union) {
              /* Fits in existing bitfield. */
              sym->bit_length = len;
              sym->bit_offset = bitfield_offset;
              bitfield_offset += len;
            }
            else {
              /* Doesn't fit in existing bitfield or first in bitfield. Start a new bitfield. */
              sym->bit_length = len;
              sym->bit_offset = 0;
              bitfield_offset = len;
              bitfield_base = sym;
            }
          }
        }
        else {
          /* Terminate any in progress bitfield. */
          bitfield_base = nullptr;
        }

        if (sym->bit_offset != 0 && bitfield_base) {
          /* Bitfield members copy their offset from the first bitfield member. */
          sym->offset = bitfield_base->offset;
        }
        else if (!sym->is_static) {
          /* Static members are not part of the class layout. */
          sym->set_offset(cls->is_union, offset);
        }

        /* Alias to the first, non-anonymous parent. */
        if (cls->is_anonymous) {
          if (named_parent->variable_emplace(sym)) {
            error(sym->loc.tok, Diag::Redefinition, sym->identifier);
          }
        }

        if (bitfield_base) {
          /* Alias to the first member of the bitfield, so that it gets the same accessor
           * decoration below. */
          sym->identifier = bitfield_base->original;
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

        if (bitfield_base) {
          /* Note: we mutate assignments into bitfieldInsert in a second pass. */
          sym->identifier += ".bitfieldExtract(" + to_string(sym->bit_offset) + ", " +
                             to_string(sym->bit_length) + ")";
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
        if (sym->array_dimensions > 0) {
          error(decl.array(), Diag::ConstexprVarMustNotBeArray);
        }

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
    if (cls != table.int_cls && cls != table.int2_cls && cls != table.int3_cls &&
        cls != table.int4_cls && cls != table.uint_cls && cls != table.uint2_cls &&
        cls != table.uint3_cls && cls != table.uint4_cls && cls != table.bool_cls &&
        cls != table.bool2_cls && cls != table.bool3_cls && cls != table.bool4_cls &&
        cls != table.float_cls && cls != table.float2_cls && cls != table.float3_cls &&
        cls != table.float4_cls)
    {
      error(decl, Diag::ConstexprVarMustBeValidType);
      return {table.err_cls};
    }

    ExpressionResult result = cls;
    if (AssignStmt assign = decl.initial_value(); assign.is_valid()) {
      result = initialize_assign(scope, cls, assign);
    }
    else if (InitializerList list = decl.initializer_list(); list.is_valid()) {
      result = eval_scalar_initializer_list(scope, cls, list);
    }

    if (result.type != cls) {
      error(decl,
            Diag::ConstexprVarMustBeInitializedByCorrectType,
            decl.identifier().str(),
            cls->original,
            result.type->original);
    }
    else if (!result.is_constexpr()) {
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
