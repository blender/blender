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

#ifndef LEMON_LP_SKELETON_H
#define LEMON_LP_SKELETON_H

#include <lemon/lp_base.h>

///\file
///\brief Skeleton file to implement LP/MIP solver interfaces
///
///The classes in this file do nothing, but they can serve as skeletons when
///implementing an interface to new solvers.
namespace lemon {

  ///A skeleton class to implement LP/MIP solver base interface

  ///This class does nothing, but it can serve as a skeleton when
  ///implementing an interface to new solvers.
  class SkeletonSolverBase : public virtual LpBase {
    int col_num,row_num;

  protected:

    SkeletonSolverBase()
      : col_num(-1), row_num(-1) {}

    /// \e
    virtual int _addCol();
    /// \e
    virtual int _addRow();
    /// \e
    virtual int _addRow(Value l, ExprIterator b, ExprIterator e, Value u);
    /// \e
    virtual void _eraseCol(int i);
    /// \e
    virtual void _eraseRow(int i);

    /// \e
    virtual void _getColName(int col, std::string& name) const;
    /// \e
    virtual void _setColName(int col, const std::string& name);
    /// \e
    virtual int _colByName(const std::string& name) const;

    /// \e
    virtual void _getRowName(int row, std::string& name) const;
    /// \e
    virtual void _setRowName(int row, const std::string& name);
    /// \e
    virtual int _rowByName(const std::string& name) const;

    /// \e
    virtual void _setRowCoeffs(int i, ExprIterator b, ExprIterator e);
    /// \e
    virtual void _getRowCoeffs(int i, InsertIterator b) const;
    /// \e
    virtual void _setColCoeffs(int i, ExprIterator b, ExprIterator e);
    /// \e
    virtual void _getColCoeffs(int i, InsertIterator b) const;

    /// Set one element of the coefficient matrix
    virtual void _setCoeff(int row, int col, Value value);

    /// Get one element of the coefficient matrix
    virtual Value _getCoeff(int row, int col) const;

    /// The lower bound of a variable (column) have to be given by an
    /// extended number of type Value, i.e. a finite number of type
    /// Value or -\ref INF.
    virtual void _setColLowerBound(int i, Value value);
    /// \e

    /// The lower bound of a variable (column) is an
    /// extended number of type Value, i.e. a finite number of type
    /// Value or -\ref INF.
    virtual Value _getColLowerBound(int i) const;

    /// The upper bound of a variable (column) have to be given by an
    /// extended number of type Value, i.e. a finite number of type
    /// Value or \ref INF.
    virtual void _setColUpperBound(int i, Value value);
    /// \e

    /// The upper bound of a variable (column) is an
    /// extended number of type Value, i.e. a finite number of type
    /// Value or \ref INF.
    virtual Value _getColUpperBound(int i) const;

    /// The lower bound of a constraint (row) have to be given by an
    /// extended number of type Value, i.e. a finite number of type
    /// Value or -\ref INF.
    virtual void _setRowLowerBound(int i, Value value);
    /// \e

    /// The lower bound of a constraint (row) is an
    /// extended number of type Value, i.e. a finite number of type
    /// Value or -\ref INF.
    virtual Value _getRowLowerBound(int i) const;

    /// The upper bound of a constraint (row) have to be given by an
    /// extended number of type Value, i.e. a finite number of type
    /// Value or \ref INF.
    virtual void _setRowUpperBound(int i, Value value);
    /// \e

    /// The upper bound of a constraint (row) is an
    /// extended number of type Value, i.e. a finite number of type
    /// Value or \ref INF.
    virtual Value _getRowUpperBound(int i) const;

    /// \e
    virtual void _setObjCoeffs(ExprIterator b, ExprIterator e);
    /// \e
    virtual void _getObjCoeffs(InsertIterator b) const;

    /// \e
    virtual void _setObjCoeff(int i, Value obj_coef);
    /// \e
    virtual Value _getObjCoeff(int i) const;

    ///\e
    virtual void _setSense(Sense);
    ///\e
    virtual Sense _getSense() const;

    ///\e
    virtual void _clear();

    ///\e
    virtual void _messageLevel(MessageLevel);

    ///\e
    virtual void _write(std::string file, std::string format) const;

  };

  /// \brief Skeleton class for an LP solver interface
  ///
  ///This class does nothing, but it can serve as a skeleton when
  ///implementing an interface to new solvers.

  ///\ingroup lp_group
  class LpSkeleton : public LpSolver, public SkeletonSolverBase {
  public:
    ///\e
    LpSkeleton() : LpSolver(), SkeletonSolverBase() {}
    ///\e
    virtual LpSkeleton* newSolver() const;
    ///\e
    virtual LpSkeleton* cloneSolver() const;
  protected:

    ///\e
    virtual SolveExitStatus _solve();

    ///\e
    virtual Value _getPrimal(int i) const;
    ///\e
    virtual Value _getDual(int i) const;

    ///\e
    virtual Value _getPrimalValue() const;

    ///\e
    virtual Value _getPrimalRay(int i) const;
    ///\e
    virtual Value _getDualRay(int i) const;

    ///\e
    virtual ProblemType _getPrimalType() const;
    ///\e
    virtual ProblemType _getDualType() const;

    ///\e
    virtual VarStatus _getColStatus(int i) const;
    ///\e
    virtual VarStatus _getRowStatus(int i) const;

    ///\e
    virtual const char* _solverName() const;

  };

  /// \brief Skeleton class for a MIP solver interface
  ///
  ///This class does nothing, but it can serve as a skeleton when
  ///implementing an interface to new solvers.
  ///\ingroup lp_group
  class MipSkeleton : public MipSolver, public SkeletonSolverBase {
  public:
    ///\e
    MipSkeleton() : MipSolver(), SkeletonSolverBase() {}
    ///\e
    virtual MipSkeleton* newSolver() const;
    ///\e
    virtual MipSkeleton* cloneSolver() const;

  protected:
    ///\e
    virtual SolveExitStatus _solve();

    ///\e
    virtual Value _getSol(int i) const;

    ///\e
    virtual Value _getSolValue() const;

    ///\e
    virtual ProblemType _getType() const;

    ///\e
    virtual const char* _solverName() const;

  };

} //namespace lemon

#endif
