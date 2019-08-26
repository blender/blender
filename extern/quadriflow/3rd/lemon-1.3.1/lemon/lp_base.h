/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2013
 * Egervary Jeno Kombinatorikus Optimalizalasi Kutatocsoport
 * (Egervary Research Group on Combinatorial Optimization, EGRES).
 *
 * Permission to use, modify and distribute this software is granted
 * provided that this copyright notice appears in all copies. For
 * precise terms see the accompanying LICENSE file.
 *
 * This software is provided "AS IS" with no warranty of any kind,
 * express or implied, and with no claim as to its suitability for any
 * purpose.
 *
 */

#ifndef LEMON_LP_BASE_H
#define LEMON_LP_BASE_H

#include<iostream>
#include<vector>
#include<map>
#include<limits>
#include<lemon/math.h>

#include<lemon/error.h>
#include<lemon/assert.h>

#include<lemon/core.h>
#include<lemon/bits/solver_bits.h>

///\file
///\brief The interface of the LP solver interface.
///\ingroup lp_group
namespace lemon {

  ///Common base class for LP and MIP solvers

  ///Usually this class is not used directly, please use one of the concrete
  ///implementations of the solver interface.
  ///\ingroup lp_group
  class LpBase {

  protected:

    _solver_bits::VarIndex rows;
    _solver_bits::VarIndex cols;

  public:

    ///Possible outcomes of an LP solving procedure
    enum SolveExitStatus {
      /// = 0. It means that the problem has been successfully solved: either
      ///an optimal solution has been found or infeasibility/unboundedness
      ///has been proved.
      SOLVED = 0,
      /// = 1. Any other case (including the case when some user specified
      ///limit has been exceeded).
      UNSOLVED = 1
    };

    ///Direction of the optimization
    enum Sense {
      /// Minimization
      MIN,
      /// Maximization
      MAX
    };

    ///Enum for \c messageLevel() parameter
    enum MessageLevel {
      /// No output (default value).
      MESSAGE_NOTHING,
      /// Error messages only.
      MESSAGE_ERROR,
      /// Warnings.
      MESSAGE_WARNING,
      /// Normal output.
      MESSAGE_NORMAL,
      /// Verbose output.
      MESSAGE_VERBOSE
    };


    ///The floating point type used by the solver
    typedef double Value;
    ///The infinity constant
    static const Value INF;
    ///The not a number constant
    static const Value NaN;

    friend class Col;
    friend class ColIt;
    friend class Row;
    friend class RowIt;

    ///Refer to a column of the LP.

    ///This type is used to refer to a column of the LP.
    ///
    ///Its value remains valid and correct even after the addition or erase of
    ///other columns.
    ///
    ///\note This class is similar to other Item types in LEMON, like
    ///Node and Arc types in digraph.
    class Col {
      friend class LpBase;
    protected:
      int _id;
      explicit Col(int id) : _id(id) {}
    public:
      typedef Value ExprValue;
      typedef True LpCol;
      /// Default constructor

      /// \warning The default constructor sets the Col to an
      /// undefined value.
      Col() {}
      /// Invalid constructor \& conversion.

      /// This constructor initializes the Col to be invalid.
      /// \sa Invalid for more details.
      Col(const Invalid&) : _id(-1) {}
      /// Equality operator

      /// Two \ref Col "Col"s are equal if and only if they point to
      /// the same LP column or both are invalid.
      bool operator==(Col c) const  {return _id == c._id;}
      /// Inequality operator

      /// \sa operator==(Col c)
      ///
      bool operator!=(Col c) const  {return _id != c._id;}
      /// Artificial ordering operator.

      /// To allow the use of this object in std::map or similar
      /// associative container we require this.
      ///
      /// \note This operator only have to define some strict ordering of
      /// the items; this order has nothing to do with the iteration
      /// ordering of the items.
      bool operator<(Col c) const  {return _id < c._id;}
    };

    ///Iterator for iterate over the columns of an LP problem

    /// Its usage is quite simple, for example, you can count the number
    /// of columns in an LP \c lp:
    ///\code
    /// int count=0;
    /// for (LpBase::ColIt c(lp); c!=INVALID; ++c) ++count;
    ///\endcode
    class ColIt : public Col {
      const LpBase *_solver;
    public:
      /// Default constructor

      /// \warning The default constructor sets the iterator
      /// to an undefined value.
      ColIt() {}
      /// Sets the iterator to the first Col

      /// Sets the iterator to the first Col.
      ///
      ColIt(const LpBase &solver) : _solver(&solver)
      {
        _solver->cols.firstItem(_id);
      }
      /// Invalid constructor \& conversion

      /// Initialize the iterator to be invalid.
      /// \sa Invalid for more details.
      ColIt(const Invalid&) : Col(INVALID) {}
      /// Next column

      /// Assign the iterator to the next column.
      ///
      ColIt &operator++()
      {
        _solver->cols.nextItem(_id);
        return *this;
      }
    };

    /// \brief Returns the ID of the column.
    static int id(const Col& col) { return col._id; }
    /// \brief Returns the column with the given ID.
    ///
    /// \pre The argument should be a valid column ID in the LP problem.
    static Col colFromId(int id) { return Col(id); }

    ///Refer to a row of the LP.

    ///This type is used to refer to a row of the LP.
    ///
    ///Its value remains valid and correct even after the addition or erase of
    ///other rows.
    ///
    ///\note This class is similar to other Item types in LEMON, like
    ///Node and Arc types in digraph.
    class Row {
      friend class LpBase;
    protected:
      int _id;
      explicit Row(int id) : _id(id) {}
    public:
      typedef Value ExprValue;
      typedef True LpRow;
      /// Default constructor

      /// \warning The default constructor sets the Row to an
      /// undefined value.
      Row() {}
      /// Invalid constructor \& conversion.

      /// This constructor initializes the Row to be invalid.
      /// \sa Invalid for more details.
      Row(const Invalid&) : _id(-1) {}
      /// Equality operator

      /// Two \ref Row "Row"s are equal if and only if they point to
      /// the same LP row or both are invalid.
      bool operator==(Row r) const  {return _id == r._id;}
      /// Inequality operator

      /// \sa operator==(Row r)
      ///
      bool operator!=(Row r) const  {return _id != r._id;}
      /// Artificial ordering operator.

      /// To allow the use of this object in std::map or similar
      /// associative container we require this.
      ///
      /// \note This operator only have to define some strict ordering of
      /// the items; this order has nothing to do with the iteration
      /// ordering of the items.
      bool operator<(Row r) const  {return _id < r._id;}
    };

    ///Iterator for iterate over the rows of an LP problem

    /// Its usage is quite simple, for example, you can count the number
    /// of rows in an LP \c lp:
    ///\code
    /// int count=0;
    /// for (LpBase::RowIt c(lp); c!=INVALID; ++c) ++count;
    ///\endcode
    class RowIt : public Row {
      const LpBase *_solver;
    public:
      /// Default constructor

      /// \warning The default constructor sets the iterator
      /// to an undefined value.
      RowIt() {}
      /// Sets the iterator to the first Row

      /// Sets the iterator to the first Row.
      ///
      RowIt(const LpBase &solver) : _solver(&solver)
      {
        _solver->rows.firstItem(_id);
      }
      /// Invalid constructor \& conversion

      /// Initialize the iterator to be invalid.
      /// \sa Invalid for more details.
      RowIt(const Invalid&) : Row(INVALID) {}
      /// Next row

      /// Assign the iterator to the next row.
      ///
      RowIt &operator++()
      {
        _solver->rows.nextItem(_id);
        return *this;
      }
    };

    /// \brief Returns the ID of the row.
    static int id(const Row& row) { return row._id; }
    /// \brief Returns the row with the given ID.
    ///
    /// \pre The argument should be a valid row ID in the LP problem.
    static Row rowFromId(int id) { return Row(id); }

  public:

    ///Linear expression of variables and a constant component

    ///This data structure stores a linear expression of the variables
    ///(\ref Col "Col"s) and also has a constant component.
    ///
    ///There are several ways to access and modify the contents of this
    ///container.
    ///\code
    ///e[v]=5;
    ///e[v]+=12;
    ///e.erase(v);
    ///\endcode
    ///or you can also iterate through its elements.
    ///\code
    ///double s=0;
    ///for(LpBase::Expr::ConstCoeffIt i(e);i!=INVALID;++i)
    ///  s+=*i * primal(i);
    ///\endcode
    ///(This code computes the primal value of the expression).
    ///- Numbers (<tt>double</tt>'s)
    ///and variables (\ref Col "Col"s) directly convert to an
    ///\ref Expr and the usual linear operations are defined, so
    ///\code
    ///v+w
    ///2*v-3.12*(v-w/2)+2
    ///v*2.1+(3*v+(v*12+w+6)*3)/2
    ///\endcode
    ///are valid expressions.
    ///The usual assignment operations are also defined.
    ///\code
    ///e=v+w;
    ///e+=2*v-3.12*(v-w/2)+2;
    ///e*=3.4;
    ///e/=5;
    ///\endcode
    ///- The constant member can be set and read by dereference
    ///  operator (unary *)
    ///
    ///\code
    ///*e=12;
    ///double c=*e;
    ///\endcode
    ///
    ///\sa Constr
    class Expr {
      friend class LpBase;
    public:
      /// The key type of the expression
      typedef LpBase::Col Key;
      /// The value type of the expression
      typedef LpBase::Value Value;

    protected:
      Value const_comp;
      std::map<int, Value> comps;

    public:
      typedef True SolverExpr;
      /// Default constructor

      /// Construct an empty expression, the coefficients and
      /// the constant component are initialized to zero.
      Expr() : const_comp(0) {}
      /// Construct an expression from a column

      /// Construct an expression, which has a term with \c c variable
      /// and 1.0 coefficient.
      Expr(const Col &c) : const_comp(0) {
        typedef std::map<int, Value>::value_type pair_type;
        comps.insert(pair_type(id(c), 1));
      }
      /// Construct an expression from a constant

      /// Construct an expression, which's constant component is \c v.
      ///
      Expr(const Value &v) : const_comp(v) {}
      /// Returns the coefficient of the column
      Value operator[](const Col& c) const {
        std::map<int, Value>::const_iterator it=comps.find(id(c));
        if (it != comps.end()) {
          return it->second;
        } else {
          return 0;
        }
      }
      /// Returns the coefficient of the column
      Value& operator[](const Col& c) {
        return comps[id(c)];
      }
      /// Sets the coefficient of the column
      void set(const Col &c, const Value &v) {
        if (v != 0.0) {
          typedef std::map<int, Value>::value_type pair_type;
          comps.insert(pair_type(id(c), v));
        } else {
          comps.erase(id(c));
        }
      }
      /// Returns the constant component of the expression
      Value& operator*() { return const_comp; }
      /// Returns the constant component of the expression
      const Value& operator*() const { return const_comp; }
      /// \brief Removes the coefficients which's absolute value does
      /// not exceed \c epsilon. It also sets to zero the constant
      /// component, if it does not exceed epsilon in absolute value.
      void simplify(Value epsilon = 0.0) {
        std::map<int, Value>::iterator it=comps.begin();
        while (it != comps.end()) {
          std::map<int, Value>::iterator jt=it;
          ++jt;
          if (std::fabs((*it).second) <= epsilon) comps.erase(it);
          it=jt;
        }
        if (std::fabs(const_comp) <= epsilon) const_comp = 0;
      }

      void simplify(Value epsilon = 0.0) const {
        const_cast<Expr*>(this)->simplify(epsilon);
      }

      ///Sets all coefficients and the constant component to 0.
      void clear() {
        comps.clear();
        const_comp=0;
      }

      ///Compound assignment
      Expr &operator+=(const Expr &e) {
        for (std::map<int, Value>::const_iterator it=e.comps.begin();
             it!=e.comps.end(); ++it)
          comps[it->first]+=it->second;
        const_comp+=e.const_comp;
        return *this;
      }
      ///Compound assignment
      Expr &operator-=(const Expr &e) {
        for (std::map<int, Value>::const_iterator it=e.comps.begin();
             it!=e.comps.end(); ++it)
          comps[it->first]-=it->second;
        const_comp-=e.const_comp;
        return *this;
      }
      ///Multiply with a constant
      Expr &operator*=(const Value &v) {
        for (std::map<int, Value>::iterator it=comps.begin();
             it!=comps.end(); ++it)
          it->second*=v;
        const_comp*=v;
        return *this;
      }
      ///Division with a constant
      Expr &operator/=(const Value &c) {
        for (std::map<int, Value>::iterator it=comps.begin();
             it!=comps.end(); ++it)
          it->second/=c;
        const_comp/=c;
        return *this;
      }

      ///Iterator over the expression

      ///The iterator iterates over the terms of the expression.
      ///
      ///\code
      ///double s=0;
      ///for(LpBase::Expr::CoeffIt i(e);i!=INVALID;++i)
      ///  s+= *i * primal(i);
      ///\endcode
      class CoeffIt {
      private:

        std::map<int, Value>::iterator _it, _end;

      public:

        /// Sets the iterator to the first term

        /// Sets the iterator to the first term of the expression.
        ///
        CoeffIt(Expr& e)
          : _it(e.comps.begin()), _end(e.comps.end()){}

        /// Convert the iterator to the column of the term
        operator Col() const {
          return colFromId(_it->first);
        }

        /// Returns the coefficient of the term
        Value& operator*() { return _it->second; }

        /// Returns the coefficient of the term
        const Value& operator*() const { return _it->second; }
        /// Next term

        /// Assign the iterator to the next term.
        ///
        CoeffIt& operator++() { ++_it; return *this; }

        /// Equality operator
        bool operator==(Invalid) const { return _it == _end; }
        /// Inequality operator
        bool operator!=(Invalid) const { return _it != _end; }
      };

      /// Const iterator over the expression

      ///The iterator iterates over the terms of the expression.
      ///
      ///\code
      ///double s=0;
      ///for(LpBase::Expr::ConstCoeffIt i(e);i!=INVALID;++i)
      ///  s+=*i * primal(i);
      ///\endcode
      class ConstCoeffIt {
      private:

        std::map<int, Value>::const_iterator _it, _end;

      public:

        /// Sets the iterator to the first term

        /// Sets the iterator to the first term of the expression.
        ///
        ConstCoeffIt(const Expr& e)
          : _it(e.comps.begin()), _end(e.comps.end()){}

        /// Convert the iterator to the column of the term
        operator Col() const {
          return colFromId(_it->first);
        }

        /// Returns the coefficient of the term
        const Value& operator*() const { return _it->second; }

        /// Next term

        /// Assign the iterator to the next term.
        ///
        ConstCoeffIt& operator++() { ++_it; return *this; }

        /// Equality operator
        bool operator==(Invalid) const { return _it == _end; }
        /// Inequality operator
        bool operator!=(Invalid) const { return _it != _end; }
      };

    };

    ///Linear constraint

    ///This data stucture represents a linear constraint in the LP.
    ///Basically it is a linear expression with a lower or an upper bound
    ///(or both). These parts of the constraint can be obtained by the member
    ///functions \ref expr(), \ref lowerBound() and \ref upperBound(),
    ///respectively.
    ///There are two ways to construct a constraint.
    ///- You can set the linear expression and the bounds directly
    ///  by the functions above.
    ///- The operators <tt>\<=</tt>, <tt>==</tt> and  <tt>\>=</tt>
    ///  are defined between expressions, or even between constraints whenever
    ///  it makes sense. Therefore if \c e and \c f are linear expressions and
    ///  \c s and \c t are numbers, then the followings are valid expressions
    ///  and thus they can be used directly e.g. in \ref addRow() whenever
    ///  it makes sense.
    ///\code
    ///  e<=s
    ///  e<=f
    ///  e==f
    ///  s<=e<=t
    ///  e>=t
    ///\endcode
    ///\warning The validity of a constraint is checked only at run
    ///time, so e.g. \ref addRow(<tt>x[1]\<=x[2]<=5</tt>) will
    ///compile, but will fail an assertion.
    class Constr
    {
    public:
      typedef LpBase::Expr Expr;
      typedef Expr::Key Key;
      typedef Expr::Value Value;

    protected:
      Expr _expr;
      Value _lb,_ub;
    public:
      ///\e
      Constr() : _expr(), _lb(NaN), _ub(NaN) {}
      ///\e
      Constr(Value lb, const Expr &e, Value ub) :
        _expr(e), _lb(lb), _ub(ub) {}
      Constr(const Expr &e) :
        _expr(e), _lb(NaN), _ub(NaN) {}
      ///\e
      void clear()
      {
        _expr.clear();
        _lb=_ub=NaN;
      }

      ///Reference to the linear expression
      Expr &expr() { return _expr; }
      ///Cont reference to the linear expression
      const Expr &expr() const { return _expr; }
      ///Reference to the lower bound.

      ///\return
      ///- \ref INF "INF": the constraint is lower unbounded.
      ///- \ref NaN "NaN": lower bound has not been set.
      ///- finite number: the lower bound
      Value &lowerBound() { return _lb; }
      ///The const version of \ref lowerBound()
      const Value &lowerBound() const { return _lb; }
      ///Reference to the upper bound.

      ///\return
      ///- \ref INF "INF": the constraint is upper unbounded.
      ///- \ref NaN "NaN": upper bound has not been set.
      ///- finite number: the upper bound
      Value &upperBound() { return _ub; }
      ///The const version of \ref upperBound()
      const Value &upperBound() const { return _ub; }
      ///Is the constraint lower bounded?
      bool lowerBounded() const {
        return _lb != -INF && !isNaN(_lb);
      }
      ///Is the constraint upper bounded?
      bool upperBounded() const {
        return _ub != INF && !isNaN(_ub);
      }

    };

    ///Linear expression of rows

    ///This data structure represents a column of the matrix,
    ///thas is it strores a linear expression of the dual variables
    ///(\ref Row "Row"s).
    ///
    ///There are several ways to access and modify the contents of this
    ///container.
    ///\code
    ///e[v]=5;
    ///e[v]+=12;
    ///e.erase(v);
    ///\endcode
    ///or you can also iterate through its elements.
    ///\code
    ///double s=0;
    ///for(LpBase::DualExpr::ConstCoeffIt i(e);i!=INVALID;++i)
    ///  s+=*i;
    ///\endcode
    ///(This code computes the sum of all coefficients).
    ///- Numbers (<tt>double</tt>'s)
    ///and variables (\ref Row "Row"s) directly convert to an
    ///\ref DualExpr and the usual linear operations are defined, so
    ///\code
    ///v+w
    ///2*v-3.12*(v-w/2)
    ///v*2.1+(3*v+(v*12+w)*3)/2
    ///\endcode
    ///are valid \ref DualExpr dual expressions.
    ///The usual assignment operations are also defined.
    ///\code
    ///e=v+w;
    ///e+=2*v-3.12*(v-w/2);
    ///e*=3.4;
    ///e/=5;
    ///\endcode
    ///
    ///\sa Expr
    class DualExpr {
      friend class LpBase;
    public:
      /// The key type of the expression
      typedef LpBase::Row Key;
      /// The value type of the expression
      typedef LpBase::Value Value;

    protected:
      std::map<int, Value> comps;

    public:
      typedef True SolverExpr;
      /// Default constructor

      /// Construct an empty expression, the coefficients are
      /// initialized to zero.
      DualExpr() {}
      /// Construct an expression from a row

      /// Construct an expression, which has a term with \c r dual
      /// variable and 1.0 coefficient.
      DualExpr(const Row &r) {
        typedef std::map<int, Value>::value_type pair_type;
        comps.insert(pair_type(id(r), 1));
      }
      /// Returns the coefficient of the row
      Value operator[](const Row& r) const {
        std::map<int, Value>::const_iterator it = comps.find(id(r));
        if (it != comps.end()) {
          return it->second;
        } else {
          return 0;
        }
      }
      /// Returns the coefficient of the row
      Value& operator[](const Row& r) {
        return comps[id(r)];
      }
      /// Sets the coefficient of the row
      void set(const Row &r, const Value &v) {
        if (v != 0.0) {
          typedef std::map<int, Value>::value_type pair_type;
          comps.insert(pair_type(id(r), v));
        } else {
          comps.erase(id(r));
        }
      }
      /// \brief Removes the coefficients which's absolute value does
      /// not exceed \c epsilon.
      void simplify(Value epsilon = 0.0) {
        std::map<int, Value>::iterator it=comps.begin();
        while (it != comps.end()) {
          std::map<int, Value>::iterator jt=it;
          ++jt;
          if (std::fabs((*it).second) <= epsilon) comps.erase(it);
          it=jt;
        }
      }

      void simplify(Value epsilon = 0.0) const {
        const_cast<DualExpr*>(this)->simplify(epsilon);
      }

      ///Sets all coefficients to 0.
      void clear() {
        comps.clear();
      }
      ///Compound assignment
      DualExpr &operator+=(const DualExpr &e) {
        for (std::map<int, Value>::const_iterator it=e.comps.begin();
             it!=e.comps.end(); ++it)
          comps[it->first]+=it->second;
        return *this;
      }
      ///Compound assignment
      DualExpr &operator-=(const DualExpr &e) {
        for (std::map<int, Value>::const_iterator it=e.comps.begin();
             it!=e.comps.end(); ++it)
          comps[it->first]-=it->second;
        return *this;
      }
      ///Multiply with a constant
      DualExpr &operator*=(const Value &v) {
        for (std::map<int, Value>::iterator it=comps.begin();
             it!=comps.end(); ++it)
          it->second*=v;
        return *this;
      }
      ///Division with a constant
      DualExpr &operator/=(const Value &v) {
        for (std::map<int, Value>::iterator it=comps.begin();
             it!=comps.end(); ++it)
          it->second/=v;
        return *this;
      }

      ///Iterator over the expression

      ///The iterator iterates over the terms of the expression.
      ///
      ///\code
      ///double s=0;
      ///for(LpBase::DualExpr::CoeffIt i(e);i!=INVALID;++i)
      ///  s+= *i * dual(i);
      ///\endcode
      class CoeffIt {
      private:

        std::map<int, Value>::iterator _it, _end;

      public:

        /// Sets the iterator to the first term

        /// Sets the iterator to the first term of the expression.
        ///
        CoeffIt(DualExpr& e)
          : _it(e.comps.begin()), _end(e.comps.end()){}

        /// Convert the iterator to the row of the term
        operator Row() const {
          return rowFromId(_it->first);
        }

        /// Returns the coefficient of the term
        Value& operator*() { return _it->second; }

        /// Returns the coefficient of the term
        const Value& operator*() const { return _it->second; }

        /// Next term

        /// Assign the iterator to the next term.
        ///
        CoeffIt& operator++() { ++_it; return *this; }

        /// Equality operator
        bool operator==(Invalid) const { return _it == _end; }
        /// Inequality operator
        bool operator!=(Invalid) const { return _it != _end; }
      };

      ///Iterator over the expression

      ///The iterator iterates over the terms of the expression.
      ///
      ///\code
      ///double s=0;
      ///for(LpBase::DualExpr::ConstCoeffIt i(e);i!=INVALID;++i)
      ///  s+= *i * dual(i);
      ///\endcode
      class ConstCoeffIt {
      private:

        std::map<int, Value>::const_iterator _it, _end;

      public:

        /// Sets the iterator to the first term

        /// Sets the iterator to the first term of the expression.
        ///
        ConstCoeffIt(const DualExpr& e)
          : _it(e.comps.begin()), _end(e.comps.end()){}

        /// Convert the iterator to the row of the term
        operator Row() const {
          return rowFromId(_it->first);
        }

        /// Returns the coefficient of the term
        const Value& operator*() const { return _it->second; }

        /// Next term

        /// Assign the iterator to the next term.
        ///
        ConstCoeffIt& operator++() { ++_it; return *this; }

        /// Equality operator
        bool operator==(Invalid) const { return _it == _end; }
        /// Inequality operator
        bool operator!=(Invalid) const { return _it != _end; }
      };
    };


  protected:

    class InsertIterator {
    private:

      std::map<int, Value>& _host;
      const _solver_bits::VarIndex& _index;

    public:

      typedef std::output_iterator_tag iterator_category;
      typedef void difference_type;
      typedef void value_type;
      typedef void reference;
      typedef void pointer;

      InsertIterator(std::map<int, Value>& host,
                   const _solver_bits::VarIndex& index)
        : _host(host), _index(index) {}

      InsertIterator& operator=(const std::pair<int, Value>& value) {
        typedef std::map<int, Value>::value_type pair_type;
        _host.insert(pair_type(_index[value.first], value.second));
        return *this;
      }

      InsertIterator& operator*() { return *this; }
      InsertIterator& operator++() { return *this; }
      InsertIterator operator++(int) { return *this; }

    };

    class ExprIterator {
    private:
      std::map<int, Value>::const_iterator _host_it;
      const _solver_bits::VarIndex& _index;
    public:

      typedef std::bidirectional_iterator_tag iterator_category;
      typedef std::ptrdiff_t difference_type;
      typedef const std::pair<int, Value> value_type;
      typedef value_type reference;

      class pointer {
      public:
        pointer(value_type& _value) : value(_value) {}
        value_type* operator->() { return &value; }
      private:
        value_type value;
      };

      ExprIterator(const std::map<int, Value>::const_iterator& host_it,
                   const _solver_bits::VarIndex& index)
        : _host_it(host_it), _index(index) {}

      reference operator*() {
        return std::make_pair(_index(_host_it->first), _host_it->second);
      }

      pointer operator->() {
        return pointer(operator*());
      }

      ExprIterator& operator++() { ++_host_it; return *this; }
      ExprIterator operator++(int) {
        ExprIterator tmp(*this); ++_host_it; return tmp;
      }

      ExprIterator& operator--() { --_host_it; return *this; }
      ExprIterator operator--(int) {
        ExprIterator tmp(*this); --_host_it; return tmp;
      }

      bool operator==(const ExprIterator& it) const {
        return _host_it == it._host_it;
      }

      bool operator!=(const ExprIterator& it) const {
        return _host_it != it._host_it;
      }

    };

  protected:

    //Abstract virtual functions

    virtual int _addColId(int col) { return cols.addIndex(col); }
    virtual int _addRowId(int row) { return rows.addIndex(row); }

    virtual void _eraseColId(int col) { cols.eraseIndex(col); }
    virtual void _eraseRowId(int row) { rows.eraseIndex(row); }

    virtual int _addCol() = 0;
    virtual int _addRow() = 0;

    virtual int _addRow(Value l, ExprIterator b, ExprIterator e, Value u) {
      int row = _addRow();
      _setRowCoeffs(row, b, e);
      _setRowLowerBound(row, l);
      _setRowUpperBound(row, u);
      return row;
    }

    virtual void _eraseCol(int col) = 0;
    virtual void _eraseRow(int row) = 0;

    virtual void _getColName(int col, std::string& name) const = 0;
    virtual void _setColName(int col, const std::string& name) = 0;
    virtual int _colByName(const std::string& name) const = 0;

    virtual void _getRowName(int row, std::string& name) const = 0;
    virtual void _setRowName(int row, const std::string& name) = 0;
    virtual int _rowByName(const std::string& name) const = 0;

    virtual void _setRowCoeffs(int i, ExprIterator b, ExprIterator e) = 0;
    virtual void _getRowCoeffs(int i, InsertIterator b) const = 0;

    virtual void _setColCoeffs(int i, ExprIterator b, ExprIterator e) = 0;
    virtual void _getColCoeffs(int i, InsertIterator b) const = 0;

    virtual void _setCoeff(int row, int col, Value value) = 0;
    virtual Value _getCoeff(int row, int col) const = 0;

    virtual void _setColLowerBound(int i, Value value) = 0;
    virtual Value _getColLowerBound(int i) const = 0;

    virtual void _setColUpperBound(int i, Value value) = 0;
    virtual Value _getColUpperBound(int i) const = 0;

    virtual void _setRowLowerBound(int i, Value value) = 0;
    virtual Value _getRowLowerBound(int i) const = 0;

    virtual void _setRowUpperBound(int i, Value value) = 0;
    virtual Value _getRowUpperBound(int i) const = 0;

    virtual void _setObjCoeffs(ExprIterator b, ExprIterator e) = 0;
    virtual void _getObjCoeffs(InsertIterator b) const = 0;

    virtual void _setObjCoeff(int i, Value obj_coef) = 0;
    virtual Value _getObjCoeff(int i) const = 0;

    virtual void _setSense(Sense) = 0;
    virtual Sense _getSense() const = 0;

    virtual void _clear() = 0;

    virtual const char* _solverName() const = 0;

    virtual void _messageLevel(MessageLevel level) = 0;

    //Own protected stuff

    //Constant component of the objective function
    Value obj_const_comp;

    LpBase() : rows(), cols(), obj_const_comp(0) {}

  public:

    ///Unsupported file format exception
    class UnsupportedFormatError : public Exception
    {
      std::string _format;
      mutable std::string _what;
    public:
      explicit UnsupportedFormatError(std::string format) throw()
        : _format(format) { }
      virtual ~UnsupportedFormatError() throw() {}
      virtual const char* what() const throw() {
        try {
          _what.clear();
          std::ostringstream oss;
          oss << "lemon::UnsupportedFormatError: " << _format;
          _what = oss.str();
        }
        catch (...) {}
        if (!_what.empty()) return _what.c_str();
        else return "lemon::UnsupportedFormatError";
      }
    };

  protected:
    virtual void _write(std::string, std::string format) const
    {
      throw UnsupportedFormatError(format);
    }

  public:

    /// Virtual destructor
    virtual ~LpBase() {}

    ///Gives back the name of the solver.
    const char* solverName() const {return _solverName();}

    ///\name Build Up and Modify the LP

    ///@{

    ///Add a new empty column (i.e a new variable) to the LP
    Col addCol() { Col c; c._id = _addColId(_addCol()); return c;}

    ///\brief Adds several new columns (i.e variables) at once
    ///
    ///This magic function takes a container as its argument and fills
    ///its elements with new columns (i.e. variables)
    ///\param t can be
    ///- a standard STL compatible iterable container with
    ///\ref Col as its \c values_type like
    ///\code
    ///std::vector<LpBase::Col>
    ///std::list<LpBase::Col>
    ///\endcode
    ///- a standard STL compatible iterable container with
    ///\ref Col as its \c mapped_type like
    ///\code
    ///std::map<AnyType,LpBase::Col>
    ///\endcode
    ///- an iterable lemon \ref concepts::WriteMap "write map" like
    ///\code
    ///ListGraph::NodeMap<LpBase::Col>
    ///ListGraph::ArcMap<LpBase::Col>
    ///\endcode
    ///\return The number of the created column.
#ifdef DOXYGEN
    template<class T>
    int addColSet(T &t) { return 0;}
#else
    template<class T>
    typename enable_if<typename T::value_type::LpCol,int>::type
    addColSet(T &t,dummy<0> = 0) {
      int s=0;
      for(typename T::iterator i=t.begin();i!=t.end();++i) {*i=addCol();s++;}
      return s;
    }
    template<class T>
    typename enable_if<typename T::value_type::second_type::LpCol,
                       int>::type
    addColSet(T &t,dummy<1> = 1) {
      int s=0;
      for(typename T::iterator i=t.begin();i!=t.end();++i) {
        i->second=addCol();
        s++;
      }
      return s;
    }
    template<class T>
    typename enable_if<typename T::MapIt::Value::LpCol,
                       int>::type
    addColSet(T &t,dummy<2> = 2) {
      int s=0;
      for(typename T::MapIt i(t); i!=INVALID; ++i)
        {
          i.set(addCol());
          s++;
        }
      return s;
    }
#endif

    ///Set a column (i.e a dual constraint) of the LP

    ///\param c is the column to be modified
    ///\param e is a dual linear expression (see \ref DualExpr)
    ///a better one.
    void col(Col c, const DualExpr &e) {
      e.simplify();
      _setColCoeffs(cols(id(c)), ExprIterator(e.comps.begin(), rows),
                    ExprIterator(e.comps.end(), rows));
    }

    ///Get a column (i.e a dual constraint) of the LP

    ///\param c is the column to get
    ///\return the dual expression associated to the column
    DualExpr col(Col c) const {
      DualExpr e;
      _getColCoeffs(cols(id(c)), InsertIterator(e.comps, rows));
      return e;
    }

    ///Add a new column to the LP

    ///\param e is a dual linear expression (see \ref DualExpr)
    ///\param o is the corresponding component of the objective
    ///function. It is 0 by default.
    ///\return The created column.
    Col addCol(const DualExpr &e, Value o = 0) {
      Col c=addCol();
      col(c,e);
      objCoeff(c,o);
      return c;
    }

    ///Add a new empty row (i.e a new constraint) to the LP

    ///This function adds a new empty row (i.e a new constraint) to the LP.
    ///\return The created row
    Row addRow() { Row r; r._id = _addRowId(_addRow()); return r;}

    ///\brief Add several new rows (i.e constraints) at once
    ///
    ///This magic function takes a container as its argument and fills
    ///its elements with new row (i.e. variables)
    ///\param t can be
    ///- a standard STL compatible iterable container with
    ///\ref Row as its \c values_type like
    ///\code
    ///std::vector<LpBase::Row>
    ///std::list<LpBase::Row>
    ///\endcode
    ///- a standard STL compatible iterable container with
    ///\ref Row as its \c mapped_type like
    ///\code
    ///std::map<AnyType,LpBase::Row>
    ///\endcode
    ///- an iterable lemon \ref concepts::WriteMap "write map" like
    ///\code
    ///ListGraph::NodeMap<LpBase::Row>
    ///ListGraph::ArcMap<LpBase::Row>
    ///\endcode
    ///\return The number of rows created.
#ifdef DOXYGEN
    template<class T>
    int addRowSet(T &t) { return 0;}
#else
    template<class T>
    typename enable_if<typename T::value_type::LpRow,int>::type
    addRowSet(T &t, dummy<0> = 0) {
      int s=0;
      for(typename T::iterator i=t.begin();i!=t.end();++i) {*i=addRow();s++;}
      return s;
    }
    template<class T>
    typename enable_if<typename T::value_type::second_type::LpRow, int>::type
    addRowSet(T &t, dummy<1> = 1) {
      int s=0;
      for(typename T::iterator i=t.begin();i!=t.end();++i) {
        i->second=addRow();
        s++;
      }
      return s;
    }
    template<class T>
    typename enable_if<typename T::MapIt::Value::LpRow, int>::type
    addRowSet(T &t, dummy<2> = 2) {
      int s=0;
      for(typename T::MapIt i(t); i!=INVALID; ++i)
        {
          i.set(addRow());
          s++;
        }
      return s;
    }
#endif

    ///Set a row (i.e a constraint) of the LP

    ///\param r is the row to be modified
    ///\param l is lower bound (-\ref INF means no bound)
    ///\param e is a linear expression (see \ref Expr)
    ///\param u is the upper bound (\ref INF means no bound)
    void row(Row r, Value l, const Expr &e, Value u) {
      e.simplify();
      _setRowCoeffs(rows(id(r)), ExprIterator(e.comps.begin(), cols),
                    ExprIterator(e.comps.end(), cols));
      _setRowLowerBound(rows(id(r)),l - *e);
      _setRowUpperBound(rows(id(r)),u - *e);
    }

    ///Set a row (i.e a constraint) of the LP

    ///\param r is the row to be modified
    ///\param c is a linear expression (see \ref Constr)
    void row(Row r, const Constr &c) {
      row(r, c.lowerBounded()?c.lowerBound():-INF,
          c.expr(), c.upperBounded()?c.upperBound():INF);
    }


    ///Get a row (i.e a constraint) of the LP

    ///\param r is the row to get
    ///\return the expression associated to the row
    Expr row(Row r) const {
      Expr e;
      _getRowCoeffs(rows(id(r)), InsertIterator(e.comps, cols));
      return e;
    }

    ///Add a new row (i.e a new constraint) to the LP

    ///\param l is the lower bound (-\ref INF means no bound)
    ///\param e is a linear expression (see \ref Expr)
    ///\param u is the upper bound (\ref INF means no bound)
    ///\return The created row.
    Row addRow(Value l,const Expr &e, Value u) {
      Row r;
      e.simplify();
      r._id = _addRowId(_addRow(l - *e, ExprIterator(e.comps.begin(), cols),
                                ExprIterator(e.comps.end(), cols), u - *e));
      return r;
    }

    ///Add a new row (i.e a new constraint) to the LP

    ///\param c is a linear expression (see \ref Constr)
    ///\return The created row.
    Row addRow(const Constr &c) {
      Row r;
      c.expr().simplify();
      r._id = _addRowId(_addRow(c.lowerBounded()?c.lowerBound()-*c.expr():-INF,
                                ExprIterator(c.expr().comps.begin(), cols),
                                ExprIterator(c.expr().comps.end(), cols),
                                c.upperBounded()?c.upperBound()-*c.expr():INF));
      return r;
    }
    ///Erase a column (i.e a variable) from the LP

    ///\param c is the column to be deleted
    void erase(Col c) {
      _eraseCol(cols(id(c)));
      _eraseColId(cols(id(c)));
    }
    ///Erase a row (i.e a constraint) from the LP

    ///\param r is the row to be deleted
    void erase(Row r) {
      _eraseRow(rows(id(r)));
      _eraseRowId(rows(id(r)));
    }

    /// Get the name of a column

    ///\param c is the coresponding column
    ///\return The name of the colunm
    std::string colName(Col c) const {
      std::string name;
      _getColName(cols(id(c)), name);
      return name;
    }

    /// Set the name of a column

    ///\param c is the coresponding column
    ///\param name The name to be given
    void colName(Col c, const std::string& name) {
      _setColName(cols(id(c)), name);
    }

    /// Get the column by its name

    ///\param name The name of the column
    ///\return the proper column or \c INVALID
    Col colByName(const std::string& name) const {
      int k = _colByName(name);
      return k != -1 ? Col(cols[k]) : Col(INVALID);
    }

    /// Get the name of a row

    ///\param r is the coresponding row
    ///\return The name of the row
    std::string rowName(Row r) const {
      std::string name;
      _getRowName(rows(id(r)), name);
      return name;
    }

    /// Set the name of a row

    ///\param r is the coresponding row
    ///\param name The name to be given
    void rowName(Row r, const std::string& name) {
      _setRowName(rows(id(r)), name);
    }

    /// Get the row by its name

    ///\param name The name of the row
    ///\return the proper row or \c INVALID
    Row rowByName(const std::string& name) const {
      int k = _rowByName(name);
      return k != -1 ? Row(rows[k]) : Row(INVALID);
    }

    /// Set an element of the coefficient matrix of the LP

    ///\param r is the row of the element to be modified
    ///\param c is the column of the element to be modified
    ///\param val is the new value of the coefficient
    void coeff(Row r, Col c, Value val) {
      _setCoeff(rows(id(r)),cols(id(c)), val);
    }

    /// Get an element of the coefficient matrix of the LP

    ///\param r is the row of the element
    ///\param c is the column of the element
    ///\return the corresponding coefficient
    Value coeff(Row r, Col c) const {
      return _getCoeff(rows(id(r)),cols(id(c)));
    }

    /// Set the lower bound of a column (i.e a variable)

    /// The lower bound of a variable (column) has to be given by an
    /// extended number of type Value, i.e. a finite number of type
    /// Value or -\ref INF.
    void colLowerBound(Col c, Value value) {
      _setColLowerBound(cols(id(c)),value);
    }

    /// Get the lower bound of a column (i.e a variable)

    /// This function returns the lower bound for column (variable) \c c
    /// (this might be -\ref INF as well).
    ///\return The lower bound for column \c c
    Value colLowerBound(Col c) const {
      return _getColLowerBound(cols(id(c)));
    }

    ///\brief Set the lower bound of  several columns
    ///(i.e variables) at once
    ///
    ///This magic function takes a container as its argument
    ///and applies the function on all of its elements.
    ///The lower bound of a variable (column) has to be given by an
    ///extended number of type Value, i.e. a finite number of type
    ///Value or -\ref INF.
#ifdef DOXYGEN
    template<class T>
    void colLowerBound(T &t, Value value) { return 0;}
#else
    template<class T>
    typename enable_if<typename T::value_type::LpCol,void>::type
    colLowerBound(T &t, Value value,dummy<0> = 0) {
      for(typename T::iterator i=t.begin();i!=t.end();++i) {
        colLowerBound(*i, value);
      }
    }
    template<class T>
    typename enable_if<typename T::value_type::second_type::LpCol,
                       void>::type
    colLowerBound(T &t, Value value,dummy<1> = 1) {
      for(typename T::iterator i=t.begin();i!=t.end();++i) {
        colLowerBound(i->second, value);
      }
    }
    template<class T>
    typename enable_if<typename T::MapIt::Value::LpCol,
                       void>::type
    colLowerBound(T &t, Value value,dummy<2> = 2) {
      for(typename T::MapIt i(t); i!=INVALID; ++i){
        colLowerBound(*i, value);
      }
    }
#endif

    /// Set the upper bound of a column (i.e a variable)

    /// The upper bound of a variable (column) has to be given by an
    /// extended number of type Value, i.e. a finite number of type
    /// Value or \ref INF.
    void colUpperBound(Col c, Value value) {
      _setColUpperBound(cols(id(c)),value);
    };

    /// Get the upper bound of a column (i.e a variable)

    /// This function returns the upper bound for column (variable) \c c
    /// (this might be \ref INF as well).
    /// \return The upper bound for column \c c
    Value colUpperBound(Col c) const {
      return _getColUpperBound(cols(id(c)));
    }

    ///\brief Set the upper bound of  several columns
    ///(i.e variables) at once
    ///
    ///This magic function takes a container as its argument
    ///and applies the function on all of its elements.
    ///The upper bound of a variable (column) has to be given by an
    ///extended number of type Value, i.e. a finite number of type
    ///Value or \ref INF.
#ifdef DOXYGEN
    template<class T>
    void colUpperBound(T &t, Value value) { return 0;}
#else
    template<class T1>
    typename enable_if<typename T1::value_type::LpCol,void>::type
    colUpperBound(T1 &t, Value value,dummy<0> = 0) {
      for(typename T1::iterator i=t.begin();i!=t.end();++i) {
        colUpperBound(*i, value);
      }
    }
    template<class T1>
    typename enable_if<typename T1::value_type::second_type::LpCol,
                       void>::type
    colUpperBound(T1 &t, Value value,dummy<1> = 1) {
      for(typename T1::iterator i=t.begin();i!=t.end();++i) {
        colUpperBound(i->second, value);
      }
    }
    template<class T1>
    typename enable_if<typename T1::MapIt::Value::LpCol,
                       void>::type
    colUpperBound(T1 &t, Value value,dummy<2> = 2) {
      for(typename T1::MapIt i(t); i!=INVALID; ++i){
        colUpperBound(*i, value);
      }
    }
#endif

    /// Set the lower and the upper bounds of a column (i.e a variable)

    /// The lower and the upper bounds of
    /// a variable (column) have to be given by an
    /// extended number of type Value, i.e. a finite number of type
    /// Value, -\ref INF or \ref INF.
    void colBounds(Col c, Value lower, Value upper) {
      _setColLowerBound(cols(id(c)),lower);
      _setColUpperBound(cols(id(c)),upper);
    }

    ///\brief Set the lower and the upper bound of several columns
    ///(i.e variables) at once
    ///
    ///This magic function takes a container as its argument
    ///and applies the function on all of its elements.
    /// The lower and the upper bounds of
    /// a variable (column) have to be given by an
    /// extended number of type Value, i.e. a finite number of type
    /// Value, -\ref INF or \ref INF.
#ifdef DOXYGEN
    template<class T>
    void colBounds(T &t, Value lower, Value upper) { return 0;}
#else
    template<class T2>
    typename enable_if<typename T2::value_type::LpCol,void>::type
    colBounds(T2 &t, Value lower, Value upper,dummy<0> = 0) {
      for(typename T2::iterator i=t.begin();i!=t.end();++i) {
        colBounds(*i, lower, upper);
      }
    }
    template<class T2>
    typename enable_if<typename T2::value_type::second_type::LpCol, void>::type
    colBounds(T2 &t, Value lower, Value upper,dummy<1> = 1) {
      for(typename T2::iterator i=t.begin();i!=t.end();++i) {
        colBounds(i->second, lower, upper);
      }
    }
    template<class T2>
    typename enable_if<typename T2::MapIt::Value::LpCol, void>::type
    colBounds(T2 &t, Value lower, Value upper,dummy<2> = 2) {
      for(typename T2::MapIt i(t); i!=INVALID; ++i){
        colBounds(*i, lower, upper);
      }
    }
#endif

    /// Set the lower bound of a row (i.e a constraint)

    /// The lower bound of a constraint (row) has to be given by an
    /// extended number of type Value, i.e. a finite number of type
    /// Value or -\ref INF.
    void rowLowerBound(Row r, Value value) {
      _setRowLowerBound(rows(id(r)),value);
    }

    /// Get the lower bound of a row (i.e a constraint)

    /// This function returns the lower bound for row (constraint) \c c
    /// (this might be -\ref INF as well).
    ///\return The lower bound for row \c r
    Value rowLowerBound(Row r) const {
      return _getRowLowerBound(rows(id(r)));
    }

    /// Set the upper bound of a row (i.e a constraint)

    /// The upper bound of a constraint (row) has to be given by an
    /// extended number of type Value, i.e. a finite number of type
    /// Value or -\ref INF.
    void rowUpperBound(Row r, Value value) {
      _setRowUpperBound(rows(id(r)),value);
    }

    /// Get the upper bound of a row (i.e a constraint)

    /// This function returns the upper bound for row (constraint) \c c
    /// (this might be -\ref INF as well).
    ///\return The upper bound for row \c r
    Value rowUpperBound(Row r) const {
      return _getRowUpperBound(rows(id(r)));
    }

    ///Set an element of the objective function
    void objCoeff(Col c, Value v) {_setObjCoeff(cols(id(c)),v); };

    ///Get an element of the objective function
    Value objCoeff(Col c) const { return _getObjCoeff(cols(id(c))); };

    ///Set the objective function

    ///\param e is a linear expression of type \ref Expr.
    ///
    void obj(const Expr& e) {
      _setObjCoeffs(ExprIterator(e.comps.begin(), cols),
                    ExprIterator(e.comps.end(), cols));
      obj_const_comp = *e;
    }

    ///Get the objective function

    ///\return the objective function as a linear expression of type
    ///Expr.
    Expr obj() const {
      Expr e;
      _getObjCoeffs(InsertIterator(e.comps, cols));
      *e = obj_const_comp;
      return e;
    }


    ///Set the direction of optimization
    void sense(Sense sense) { _setSense(sense); }

    ///Query the direction of the optimization
    Sense sense() const {return _getSense(); }

    ///Set the sense to maximization
    void max() { _setSense(MAX); }

    ///Set the sense to maximization
    void min() { _setSense(MIN); }

    ///Clear the problem
    void clear() { _clear(); rows.clear(); cols.clear(); }

    /// Set the message level of the solver
    void messageLevel(MessageLevel level) { _messageLevel(level); }

    /// Write the problem to a file in the given format

    /// This function writes the problem to a file in the given format.
    /// Different solver backends may support different formats.
    /// Trying to write in an unsupported format will trigger
    /// \ref UnsupportedFormatError. For the supported formats,
    /// visit the documentation of the base class of the related backends
    /// (\ref CplexBase, \ref GlpkBase etc.)
    /// \param file The file path
    /// \param format The output file format.
    void write(std::string file, std::string format = "MPS") const
    {
      _write(file.c_str(),format.c_str());
    }

    ///@}

  };

  /// Addition

  ///\relates LpBase::Expr
  ///
  inline LpBase::Expr operator+(const LpBase::Expr &a, const LpBase::Expr &b) {
    LpBase::Expr tmp(a);
    tmp+=b;
    return tmp;
  }
  ///Substraction

  ///\relates LpBase::Expr
  ///
  inline LpBase::Expr operator-(const LpBase::Expr &a, const LpBase::Expr &b) {
    LpBase::Expr tmp(a);
    tmp-=b;
    return tmp;
  }
  ///Multiply with constant

  ///\relates LpBase::Expr
  ///
  inline LpBase::Expr operator*(const LpBase::Expr &a, const LpBase::Value &b) {
    LpBase::Expr tmp(a);
    tmp*=b;
    return tmp;
  }

  ///Multiply with constant

  ///\relates LpBase::Expr
  ///
  inline LpBase::Expr operator*(const LpBase::Value &a, const LpBase::Expr &b) {
    LpBase::Expr tmp(b);
    tmp*=a;
    return tmp;
  }
  ///Divide with constant

  ///\relates LpBase::Expr
  ///
  inline LpBase::Expr operator/(const LpBase::Expr &a, const LpBase::Value &b) {
    LpBase::Expr tmp(a);
    tmp/=b;
    return tmp;
  }

  ///Create constraint

  ///\relates LpBase::Constr
  ///
  inline LpBase::Constr operator<=(const LpBase::Expr &e,
                                   const LpBase::Expr &f) {
    return LpBase::Constr(0, f - e, LpBase::NaN);
  }

  ///Create constraint

  ///\relates LpBase::Constr
  ///
  inline LpBase::Constr operator<=(const LpBase::Value &e,
                                   const LpBase::Expr &f) {
    return LpBase::Constr(e, f, LpBase::NaN);
  }

  ///Create constraint

  ///\relates LpBase::Constr
  ///
  inline LpBase::Constr operator<=(const LpBase::Expr &e,
                                   const LpBase::Value &f) {
    return LpBase::Constr(LpBase::NaN, e, f);
  }

  ///Create constraint

  ///\relates LpBase::Constr
  ///
  inline LpBase::Constr operator>=(const LpBase::Expr &e,
                                   const LpBase::Expr &f) {
    return LpBase::Constr(0, e - f, LpBase::NaN);
  }


  ///Create constraint

  ///\relates LpBase::Constr
  ///
  inline LpBase::Constr operator>=(const LpBase::Value &e,
                                   const LpBase::Expr &f) {
    return LpBase::Constr(LpBase::NaN, f, e);
  }


  ///Create constraint

  ///\relates LpBase::Constr
  ///
  inline LpBase::Constr operator>=(const LpBase::Expr &e,
                                   const LpBase::Value &f) {
    return LpBase::Constr(f, e, LpBase::NaN);
  }

  ///Create constraint

  ///\relates LpBase::Constr
  ///
  inline LpBase::Constr operator==(const LpBase::Expr &e,
                                   const LpBase::Value &f) {
    return LpBase::Constr(f, e, f);
  }

  ///Create constraint

  ///\relates LpBase::Constr
  ///
  inline LpBase::Constr operator==(const LpBase::Expr &e,
                                   const LpBase::Expr &f) {
    return LpBase::Constr(0, f - e, 0);
  }

  ///Create constraint

  ///\relates LpBase::Constr
  ///
  inline LpBase::Constr operator<=(const LpBase::Value &n,
                                   const LpBase::Constr &c) {
    LpBase::Constr tmp(c);
    LEMON_ASSERT(isNaN(tmp.lowerBound()), "Wrong LP constraint");
    tmp.lowerBound()=n;
    return tmp;
  }
  ///Create constraint

  ///\relates LpBase::Constr
  ///
  inline LpBase::Constr operator<=(const LpBase::Constr &c,
                                   const LpBase::Value &n)
  {
    LpBase::Constr tmp(c);
    LEMON_ASSERT(isNaN(tmp.upperBound()), "Wrong LP constraint");
    tmp.upperBound()=n;
    return tmp;
  }

  ///Create constraint

  ///\relates LpBase::Constr
  ///
  inline LpBase::Constr operator>=(const LpBase::Value &n,
                                   const LpBase::Constr &c) {
    LpBase::Constr tmp(c);
    LEMON_ASSERT(isNaN(tmp.upperBound()), "Wrong LP constraint");
    tmp.upperBound()=n;
    return tmp;
  }
  ///Create constraint

  ///\relates LpBase::Constr
  ///
  inline LpBase::Constr operator>=(const LpBase::Constr &c,
                                   const LpBase::Value &n)
  {
    LpBase::Constr tmp(c);
    LEMON_ASSERT(isNaN(tmp.lowerBound()), "Wrong LP constraint");
    tmp.lowerBound()=n;
    return tmp;
  }

  ///Addition

  ///\relates LpBase::DualExpr
  ///
  inline LpBase::DualExpr operator+(const LpBase::DualExpr &a,
                                    const LpBase::DualExpr &b) {
    LpBase::DualExpr tmp(a);
    tmp+=b;
    return tmp;
  }
  ///Substraction

  ///\relates LpBase::DualExpr
  ///
  inline LpBase::DualExpr operator-(const LpBase::DualExpr &a,
                                    const LpBase::DualExpr &b) {
    LpBase::DualExpr tmp(a);
    tmp-=b;
    return tmp;
  }
  ///Multiply with constant

  ///\relates LpBase::DualExpr
  ///
  inline LpBase::DualExpr operator*(const LpBase::DualExpr &a,
                                    const LpBase::Value &b) {
    LpBase::DualExpr tmp(a);
    tmp*=b;
    return tmp;
  }

  ///Multiply with constant

  ///\relates LpBase::DualExpr
  ///
  inline LpBase::DualExpr operator*(const LpBase::Value &a,
                                    const LpBase::DualExpr &b) {
    LpBase::DualExpr tmp(b);
    tmp*=a;
    return tmp;
  }
  ///Divide with constant

  ///\relates LpBase::DualExpr
  ///
  inline LpBase::DualExpr operator/(const LpBase::DualExpr &a,
                                    const LpBase::Value &b) {
    LpBase::DualExpr tmp(a);
    tmp/=b;
    return tmp;
  }

  /// \ingroup lp_group
  ///
  /// \brief Common base class for LP solvers
  ///
  /// This class is an abstract base class for LP solvers. This class
  /// provides a full interface for set and modify an LP problem,
  /// solve it and retrieve the solution. You can use one of the
  /// descendants as a concrete implementation, or the \c Lp
  /// default LP solver. However, if you would like to handle LP
  /// solvers as reference or pointer in a generic way, you can use
  /// this class directly.
  class LpSolver : virtual public LpBase {
  public:

    /// The problem types for primal and dual problems
    enum ProblemType {
      /// = 0. Feasible solution hasn't been found (but may exist).
      UNDEFINED = 0,
      /// = 1. The problem has no feasible solution.
      INFEASIBLE = 1,
      /// = 2. Feasible solution found.
      FEASIBLE = 2,
      /// = 3. Optimal solution exists and found.
      OPTIMAL = 3,
      /// = 4. The cost function is unbounded.
      UNBOUNDED = 4
    };

    ///The basis status of variables
    enum VarStatus {
      /// The variable is in the basis
      BASIC,
      /// The variable is free, but not basic
      FREE,
      /// The variable has active lower bound
      LOWER,
      /// The variable has active upper bound
      UPPER,
      /// The variable is non-basic and fixed
      FIXED
    };

  protected:

    virtual SolveExitStatus _solve() = 0;

    virtual Value _getPrimal(int i) const = 0;
    virtual Value _getDual(int i) const = 0;

    virtual Value _getPrimalRay(int i) const = 0;
    virtual Value _getDualRay(int i) const = 0;

    virtual Value _getPrimalValue() const = 0;

    virtual VarStatus _getColStatus(int i) const = 0;
    virtual VarStatus _getRowStatus(int i) const = 0;

    virtual ProblemType _getPrimalType() const = 0;
    virtual ProblemType _getDualType() const = 0;

  public:

    ///Allocate a new LP problem instance
    virtual LpSolver* newSolver() const = 0;
    ///Make a copy of the LP problem
    virtual LpSolver* cloneSolver() const = 0;

    ///\name Solve the LP

    ///@{

    ///\e Solve the LP problem at hand
    ///
    ///\return The result of the optimization procedure. Possible
    ///values and their meanings can be found in the documentation of
    ///\ref SolveExitStatus.
    SolveExitStatus solve() { return _solve(); }

    ///@}

    ///\name Obtain the Solution

    ///@{

    /// The type of the primal problem
    ProblemType primalType() const {
      return _getPrimalType();
    }

    /// The type of the dual problem
    ProblemType dualType() const {
      return _getDualType();
    }

    /// Return the primal value of the column

    /// Return the primal value of the column.
    /// \pre The problem is solved.
    Value primal(Col c) const { return _getPrimal(cols(id(c))); }

    /// Return the primal value of the expression

    /// Return the primal value of the expression, i.e. the dot
    /// product of the primal solution and the expression.
    /// \pre The problem is solved.
    Value primal(const Expr& e) const {
      double res = *e;
      for (Expr::ConstCoeffIt c(e); c != INVALID; ++c) {
        res += *c * primal(c);
      }
      return res;
    }
    /// Returns a component of the primal ray

    /// The primal ray is solution of the modified primal problem,
    /// where we change each finite bound to 0, and we looking for a
    /// negative objective value in case of minimization, and positive
    /// objective value for maximization. If there is such solution,
    /// that proofs the unsolvability of the dual problem, and if a
    /// feasible primal solution exists, then the unboundness of
    /// primal problem.
    ///
    /// \pre The problem is solved and the dual problem is infeasible.
    /// \note Some solvers does not provide primal ray calculation
    /// functions.
    Value primalRay(Col c) const { return _getPrimalRay(cols(id(c))); }

    /// Return the dual value of the row

    /// Return the dual value of the row.
    /// \pre The problem is solved.
    Value dual(Row r) const { return _getDual(rows(id(r))); }

    /// Return the dual value of the dual expression

    /// Return the dual value of the dual expression, i.e. the dot
    /// product of the dual solution and the dual expression.
    /// \pre The problem is solved.
    Value dual(const DualExpr& e) const {
      double res = 0.0;
      for (DualExpr::ConstCoeffIt r(e); r != INVALID; ++r) {
        res += *r * dual(r);
      }
      return res;
    }

    /// Returns a component of the dual ray

    /// The dual ray is solution of the modified primal problem, where
    /// we change each finite bound to 0 (i.e. the objective function
    /// coefficients in the primal problem), and we looking for a
    /// ositive objective value. If there is such solution, that
    /// proofs the unsolvability of the primal problem, and if a
    /// feasible dual solution exists, then the unboundness of
    /// dual problem.
    ///
    /// \pre The problem is solved and the primal problem is infeasible.
    /// \note Some solvers does not provide dual ray calculation
    /// functions.
    Value dualRay(Row r) const { return _getDualRay(rows(id(r))); }

    /// Return the basis status of the column

    /// \see VarStatus
    VarStatus colStatus(Col c) const { return _getColStatus(cols(id(c))); }

    /// Return the basis status of the row

    /// \see VarStatus
    VarStatus rowStatus(Row r) const { return _getRowStatus(rows(id(r))); }

    ///The value of the objective function

    ///\return
    ///- \ref INF or -\ref INF means either infeasibility or unboundedness
    /// of the primal problem, depending on whether we minimize or maximize.
    ///- \ref NaN if no primal solution is found.
    ///- The (finite) objective value if an optimal solution is found.
    Value primal() const { return _getPrimalValue()+obj_const_comp;}
    ///@}

  protected:

  };


  /// \ingroup lp_group
  ///
  /// \brief Common base class for MIP solvers
  ///
  /// This class is an abstract base class for MIP solvers. This class
  /// provides a full interface for set and modify an MIP problem,
  /// solve it and retrieve the solution. You can use one of the
  /// descendants as a concrete implementation, or the \c Lp
  /// default MIP solver. However, if you would like to handle MIP
  /// solvers as reference or pointer in a generic way, you can use
  /// this class directly.
  class MipSolver : virtual public LpBase {
  public:

    /// The problem types for MIP problems
    enum ProblemType {
      /// = 0. Feasible solution hasn't been found (but may exist).
      UNDEFINED = 0,
      /// = 1. The problem has no feasible solution.
      INFEASIBLE = 1,
      /// = 2. Feasible solution found.
      FEASIBLE = 2,
      /// = 3. Optimal solution exists and found.
      OPTIMAL = 3,
      /// = 4. The cost function is unbounded.
      ///The Mip or at least the relaxed problem is unbounded.
      UNBOUNDED = 4
    };

    ///Allocate a new MIP problem instance
    virtual MipSolver* newSolver() const = 0;
    ///Make a copy of the MIP problem
    virtual MipSolver* cloneSolver() const = 0;

    ///\name Solve the MIP

    ///@{

    /// Solve the MIP problem at hand
    ///
    ///\return The result of the optimization procedure. Possible
    ///values and their meanings can be found in the documentation of
    ///\ref SolveExitStatus.
    SolveExitStatus solve() { return _solve(); }

    ///@}

    ///\name Set Column Type
    ///@{

    ///Possible variable (column) types (e.g. real, integer, binary etc.)
    enum ColTypes {
      /// = 0. Continuous variable (default).
      REAL = 0,
      /// = 1. Integer variable.
      INTEGER = 1
    };

    ///Sets the type of the given column to the given type

    ///Sets the type of the given column to the given type.
    ///
    void colType(Col c, ColTypes col_type) {
      _setColType(cols(id(c)),col_type);
    }

    ///Gives back the type of the column.

    ///Gives back the type of the column.
    ///
    ColTypes colType(Col c) const {
      return _getColType(cols(id(c)));
    }
    ///@}

    ///\name Obtain the Solution

    ///@{

    /// The type of the MIP problem
    ProblemType type() const {
      return _getType();
    }

    /// Return the value of the row in the solution

    ///  Return the value of the row in the solution.
    /// \pre The problem is solved.
    Value sol(Col c) const { return _getSol(cols(id(c))); }

    /// Return the value of the expression in the solution

    /// Return the value of the expression in the solution, i.e. the
    /// dot product of the solution and the expression.
    /// \pre The problem is solved.
    Value sol(const Expr& e) const {
      double res = *e;
      for (Expr::ConstCoeffIt c(e); c != INVALID; ++c) {
        res += *c * sol(c);
      }
      return res;
    }
    ///The value of the objective function

    ///\return
    ///- \ref INF or -\ref INF means either infeasibility or unboundedness
    /// of the problem, depending on whether we minimize or maximize.
    ///- \ref NaN if no primal solution is found.
    ///- The (finite) objective value if an optimal solution is found.
    Value solValue() const { return _getSolValue()+obj_const_comp;}
    ///@}

  protected:

    virtual SolveExitStatus _solve() = 0;
    virtual ColTypes _getColType(int col) const = 0;
    virtual void _setColType(int col, ColTypes col_type) = 0;
    virtual ProblemType _getType() const = 0;
    virtual Value _getSol(int i) const = 0;
    virtual Value _getSolValue() const = 0;

  };



} //namespace lemon

#endif //LEMON_LP_BASE_H
