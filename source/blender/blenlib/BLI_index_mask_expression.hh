/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_index_mask.hh"
#include "BLI_resource_scope.hh"

namespace blender::index_mask {

struct AtomicExpr;
struct UnionExpr;
struct IntersectionExpr;
struct DifferenceExpr;

struct Expr {
  enum class Type {
    Atomic,
    Union,
    Intersection,
    Difference,
  };

  Type type;
  int index;
  Vector<const Expr *> terms;

  int expression_array_size() const;

  const AtomicExpr &as_atomic() const;
  const UnionExpr &as_union() const;
  const IntersectionExpr &as_intersection() const;
  const DifferenceExpr &as_difference() const;
};

struct AtomicExpr : public Expr {
  const IndexMask *mask;
};

struct UnionExpr : public Expr {};

struct IntersectionExpr : public Expr {};

struct DifferenceExpr : public Expr {};

class ExprBuilder {
 private:
  ResourceScope scope_;
  int expr_count_ = 0;

 public:
  using Term = std::variant<const Expr *, const IndexMask *, IndexRange>;

  const UnionExpr &merge(const Span<Term> terms);
  const DifferenceExpr &subtract(const Term &main_term, const Span<Term> subtract_terms);
  const IntersectionExpr &intersect(const Span<Term> terms);

 private:
  const Expr &term_to_expr(const Term &term);
};

IndexMask evaluate_expression(const Expr &expression, IndexMaskMemory &memory);

inline int Expr::expression_array_size() const
{
  return this->index + 1;
}

inline const AtomicExpr &Expr::as_atomic() const
{
  BLI_assert(this->type == Type::Atomic);
  return static_cast<const AtomicExpr &>(*this);
}

inline const UnionExpr &Expr::as_union() const
{
  BLI_assert(this->type == Type::Union);
  return static_cast<const UnionExpr &>(*this);
}

inline const IntersectionExpr &Expr::as_intersection() const
{
  BLI_assert(this->type == Type::Intersection);
  return static_cast<const IntersectionExpr &>(*this);
}

inline const DifferenceExpr &Expr::as_difference() const
{
  BLI_assert(this->type == Type::Difference);
  return static_cast<const DifferenceExpr &>(*this);
}

}  // namespace blender::index_mask
