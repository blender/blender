/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "symbol_class.hh"

namespace bsl {

using namespace blender::gpu::shader::parser::ast;

SymbolClass::SymbolClass(SymbolScope *parent, ClassDecl decl, const string &suffix)
    : SymbolScope(parent,
                  decl.front(),
                  decl.identifier().str().empty() ?
                      string("a") + std::to_string(parent->classes.size()) :
                      string(decl.identifier().name().str()) + suffix,
                  CLASS)

{
  this->is_anonymous = decl.identifier().str().empty();
  this->is_union = decl.front() == Union;
  this->is_enum = decl.front() == Enum;
}

Result<vector<SymbolClass::MemberNode>, TokenException> SymbolClass::get_all_members_flat() const
{
  vector<MemberNode> flat_buffer;
  auto err = get_flat_members_recursive(this, "", 0, flat_buffer);
  return {flat_buffer, err};
}

/* Recursive helper to accumulate path and offsets */
optional<TokenException> SymbolClass::get_flat_members_recursive(const SymbolClass *cls,
                                                                 const string &current_path,
                                                                 int current_offset,
                                                                 vector<MemberNode> &out_buffer)
{
  /* Iterate through variables in their declaration order */
  for (SymbolVariable *var : cls->non_static_variables_in_declaration_order()) {
    /* Construct the full access path. */
    /* If the identifier is empty (e.g., an anonymous union/struct), we skip adding the dot. */
    std::string next_path = current_path;
    if (!var->identifier.empty()) {
      if (!next_path.empty()) {
        next_path += ".";
      }
      next_path += var->original;
    }
    /* Accumulate the total offset from the root struct. */
    int offset = current_offset + var->offset;

    if (var->array_dimensions > 1) {
      /* TODO */
      return TokenException(var->loc.tok, Diag::ArrayMultidimensionalNotSupported);
    }
    int elements = (var->array_dimensions == 0) ? 1 : var->array_elements;
    if (elements == -1) {
      return TokenException(var->loc.tok, Diag::ArraySizeNotConstantExpression);
    }

    for (int i = 0; i < elements; ++i) {
      string array_accessor = var->array_dimensions > 0 ? "[" + to_string(i) + "]" : "";
      if (var->type->is_builtin()) {
        /* Base case: It's a primitive/builtin type. Add to the flat buffer. */
        out_buffer.emplace_back(var->type, next_path + array_accessor, offset);
        offset += var->type->size;
      }
      else {
        /* Recursive step: It's a nested user-defined structure. */
        auto err = get_flat_members_recursive(
            var->type, next_path + array_accessor, offset, out_buffer);
        offset += var->type->size;
        if (err) {
          return err;
        }
      }
    }
  }
  return {};
}

void SymbolClass::ensure_size_and_align(NodeErrorHandler *err_handler, bool check_st430)
{
  if (size != -1 && (err_handler == nullptr || is_builtin())) {
    return;
  }
  size = 0;
  align = 1;
  is_std140_compatible = true;
  is_std430_compatible = true;
  for (SymbolVariable *var : non_static_variables_in_declaration_order()) {
    SymbolClass &cls = *var->type;

    if (err_handler && !cls.is_enum) {
      auto scope = err_handler->note(
          var->loc.tok, Diag::NoteInDeclarationOfVariable, var->identifier, cls.identifier);
      cls.ensure_size_and_align(err_handler, check_st430);
    }
    else {
      cls.ensure_size_and_align();
    }

    int member_size = cls.size;
    if (var->array_elements == 0) {
      /* Not an array. */
    }
    else if (var->array_elements == -1) {
      /* Array size need to be known at compile time for shared structs. */
      is_std140_compatible = false;
      is_std430_compatible = false;
      if (err_handler) {
        err_handler->error(var->loc.tok, Diag::ArraySizeNotConstantExpression);
      }
    }
    else {
      if (cls.size == 12) {
        /* Arrays of float3 padded to 16 bytes for each element for std430. */
        is_std140_compatible = false;
        if (err_handler) {
          err_handler->error(var->loc.tok, Diag::ImplicitPadding12BytesArrayElement);
        }
      }
      else if (cls.size % 16 != 0) {
        /* Arrays are padded to 16 bytes for each element for std140. */
        is_std140_compatible = false;
        if (err_handler && !check_st430) {
          err_handler->error(
              var->loc.tok, Diag::ImplicitPaddingArrayElementStd140, 16 - (cls.size % 16));
        }
      }
      member_size *= var->array_elements;
    }

    /* Inherit base compatibility from the member. */
    is_std140_compatible &= cls.is_std140_compatible;
    is_std430_compatible &= cls.is_std430_compatible;

    if (err_handler) {
      string hint;
      if (cls.is_enum) {
        hint = " because its underlying type is not 'uint' or 'int'";
      }
      else if (cls.identifier == "float3") {
        hint = "; use 'packed_float3' instead";
      }
      else if (cls.identifier == "int3") {
        hint = "; use 'packed_int3' instead";
      }
      else if (cls.identifier == "uint3") {
        hint = "; use 'packed_uint3' instead";
      }
      else if (cls.identifier == "bool") {
        hint = "; use 'bool32_t' instead";
      }
      else if (cls.identifier == "float3x3") {
        hint = "; use 'float3x4' instead";
      }

      if (!is_std140_compatible && !check_st430) {
        err_handler->error(var->loc.tok,
                           Diag::TypeNotAllowedInUniformAndStorageBuffer,
                           var->type->original,
                           hint);
      }
      else if (!is_std430_compatible) {
        err_handler->error(
            var->loc.tok, Diag::TypeNotAllowedInStorageBuffer, var->type->original, hint);
      }
    }

    if (this->is_union) {
      size = max(size, member_size);
    }
    else {
      /* Padding to next member alignment.
       * Force struct and arrays to all have alignment of float4. */
      int align = (cls.is_builtin() || cls.is_enum) && var->array_dimensions == 0 ? cls.align : 16;
      int padded = pad(size, align);
      /* Consider class compatible if it has no implicit padding. */
      is_std140_compatible &= padded == size;
      is_std430_compatible &= padded == size;
      if (err_handler && padded != size) {
        err_handler->error(
            var->loc.tok, Diag::ImplicitPaddingBeforeMember, padded - size, var->original);
      }
      size = padded;
      /* Add member size. */
      size += member_size;
    }
    /* Update the overall alignment of the current struct. */
    align = max(align, cls.align);

    if (!cls.is_builtin() && !cls.is_enum && cls.size % 16 != 0) {
      is_std140_compatible = false;
      if (err_handler && !check_st430) {
        err_handler->error(
            var->loc.tok, Diag::TypeNotAllowedInUniformBufferImplicitPadding, var->type->original);
      }
    }
  }

  /* The total size of the struct must be a multiple of its alignment. */
  if (align > 0) {

    int padded = pad(size, align);
    if (padded != size) {
      is_std140_compatible = false;
      is_std430_compatible = false;
      if (err_handler) {
        err_handler->error(
            loc.tok, Diag::ImplicitPaddingAtEndOfStruct, padded - size, this->original);
      }
    }
    size = padded;
  }

#if 0 /* For debugging. */
    std::cout << "size:" << size << " align:" << align << " id:" << identifier << std::endl;
#endif

  if (size == 0) {
    is_std140_compatible = false;
    is_std430_compatible = false;

    if (err_handler) {
      err_handler->error(loc.tok, Diag::EmptyClassNotSupportedInBuffer);
    }
  }
  /* The overall size and alignment of a struct in std140 must be rounded up to the base alignment
   * of a vec4 (16 bytes). */
  if (size % 16 != 0) {
    is_std140_compatible = false;

    if (err_handler && !check_st430) {
      err_handler->error(
          loc.tok, Diag::ImplicitPaddingAtEndOfStruct, 16 - (size % 16), this->original);
    }
  }
}

}  // namespace bsl
