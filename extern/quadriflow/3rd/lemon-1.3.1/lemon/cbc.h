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

#ifndef LEMON_CBC_H
#define LEMON_CBC_H

///\file
///\brief Header of the LEMON-CBC mip solver interface.
///\ingroup lp_group

#include <lemon/lp_base.h>

class CoinModel;
class OsiSolverInterface;
class CbcModel;

namespace lemon {

  /// \brief Interface for the CBC MIP solver
  ///
  /// This class implements an interface for the CBC MIP solver.
  ///\ingroup lp_group
  class CbcMip : public MipSolver {
  protected:

    CoinModel *_prob;
    OsiSolverInterface *_osi_solver;
    CbcModel *_cbc_model;

  public:

    /// \e
    CbcMip();
    /// \e
    CbcMip(const CbcMip&);
    /// \e
    ~CbcMip();
    /// \e
    virtual CbcMip* newSolver() const;
    /// \e
    virtual CbcMip* cloneSolver() const;

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

    virtual ColTypes _getColType(int col) const;
    virtual void _setColType(int col, ColTypes col_type);

    virtual SolveExitStatus _solve();
    virtual ProblemType _getType() const;
    virtual Value _getSol(int i) const;
    virtual Value _getSolValue() const;

    virtual void _clear();

    virtual void _messageLevel(MessageLevel level);
    void _applyMessageLevel();

    int _message_level;



  };

}

#endif
