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

#ifndef LEMON_SOPLEX_H
#define LEMON_SOPLEX_H

///\file
///\brief Header of the LEMON-SOPLEX lp solver interface.

#include <vector>
#include <string>

#include <lemon/lp_base.h>

// Forward declaration
namespace soplex {
  class SoPlex;
}

namespace lemon {

  /// \ingroup lp_group
  ///
  /// \brief Interface for the SOPLEX solver
  ///
  /// This class implements an interface for the SoPlex LP solver.
  /// The SoPlex library is an object oriented lp solver library
  /// developed at the Konrad-Zuse-Zentrum für Informationstechnik
  /// Berlin (ZIB). You can find detailed information about it at the
  /// <tt>http://soplex.zib.de</tt> address.
  class SoplexLp : public LpSolver {
  private:

    soplex::SoPlex* soplex;

    std::vector<std::string> _col_names;
    std::map<std::string, int> _col_names_ref;

    std::vector<std::string> _row_names;
    std::map<std::string, int> _row_names_ref;

  private:

    // these values cannot be retrieved element by element
    mutable std::vector<Value> _primal_values;
    mutable std::vector<Value> _dual_values;

    mutable std::vector<Value> _primal_ray;
    mutable std::vector<Value> _dual_ray;

    void _clear_temporals();

  public:

    /// \e
    SoplexLp();
    /// \e
    SoplexLp(const SoplexLp&);
    /// \e
    ~SoplexLp();
    /// \e
    virtual SoplexLp* newSolver() const;
    /// \e
    virtual SoplexLp* cloneSolver() const;

  protected:

    virtual const char* _solverName() const;

    virtual int _addCol();
    virtual int _addRow();
    virtual int _addRow(Value l, ExprIterator b, ExprIterator e, Value u);

    virtual void _eraseCol(int i);
    virtual void _eraseRow(int i);

    virtual void _eraseColId(int i);
    virtual void _eraseRowId(int i);

    virtual void _getColName(int col, std::string& name) const;
    virtual void _setColName(int col, const std::string& name);
    virtual int _colByName(const std::string& name) const;

    virtual void _getRowName(int row, std::string& name) const;
    virtual void _setRowName(int row, const std::string& name);
    virtual int _rowByName(const std::string& name) const;

    virtual void _setRowCoeffs(int i, ExprIterator b, ExprIterator e);
    virtual void _getRowCoeffs(int i, InsertIterator b) const;

    virtual void _setColCoeffs(int i, ExprIterator b, ExprIterator e);
    virtual void _getColCoeffs(int i, InsertIterator b) const;

    virtual void _setCoeff(int row, int col, Value value);
    virtual Value _getCoeff(int row, int col) const;

    virtual void _setColLowerBound(int i, Value value);
    virtual Value _getColLowerBound(int i) const;
    virtual void _setColUpperBound(int i, Value value);
    virtual Value _getColUpperBound(int i) const;

    virtual void _setRowLowerBound(int i, Value value);
    virtual Value _getRowLowerBound(int i) const;
    virtual void _setRowUpperBound(int i, Value value);
    virtual Value _getRowUpperBound(int i) const;

    virtual void _setObjCoeffs(ExprIterator b, ExprIterator e);
    virtual void _getObjCoeffs(InsertIterator b) const;

    virtual void _setObjCoeff(int i, Value obj_coef);
    virtual Value _getObjCoeff(int i) const;

    virtual void _setSense(Sense sense);
    virtual Sense _getSense() const;

    virtual SolveExitStatus _solve();
    virtual Value _getPrimal(int i) const;
    virtual Value _getDual(int i) const;

    virtual Value _getPrimalValue() const;

    virtual Value _getPrimalRay(int i) const;
    virtual Value _getDualRay(int i) const;

    virtual VarStatus _getColStatus(int i) const;
    virtual VarStatus _getRowStatus(int i) const;

    virtual ProblemType _getPrimalType() const;
    virtual ProblemType _getDualType() const;

    virtual void _clear();

    void _messageLevel(MessageLevel m);
    void _applyMessageLevel();

    int _message_level;

  };

} //END OF NAMESPACE LEMON

#endif //LEMON_SOPLEX_H

