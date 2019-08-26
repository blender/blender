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

///\file
///\brief Implementation of the CBC MIP solver interface.

#include "cbc.h"

#include <coin/CoinModel.hpp>
#include <coin/CbcModel.hpp>
#include <coin/OsiSolverInterface.hpp>

#include "coin/OsiClpSolverInterface.hpp"

#include "coin/CbcCutGenerator.hpp"
#include "coin/CbcHeuristicLocal.hpp"
#include "coin/CbcHeuristicGreedy.hpp"
#include "coin/CbcHeuristicFPump.hpp"
#include "coin/CbcHeuristicRINS.hpp"

#include "coin/CglGomory.hpp"
#include "coin/CglProbing.hpp"
#include "coin/CglKnapsackCover.hpp"
#include "coin/CglOddHole.hpp"
#include "coin/CglClique.hpp"
#include "coin/CglFlowCover.hpp"
#include "coin/CglMixedIntegerRounding.hpp"

#include "coin/CbcHeuristic.hpp"

namespace lemon {

  CbcMip::CbcMip() {
    _prob = new CoinModel();
    _prob->setProblemName("LEMON");
    _osi_solver = 0;
    _cbc_model = 0;
    messageLevel(MESSAGE_NOTHING);
  }

  CbcMip::CbcMip(const CbcMip& other) {
    _prob = new CoinModel(*other._prob);
    _prob->setProblemName("LEMON");
    _osi_solver = 0;
    _cbc_model = 0;
    messageLevel(MESSAGE_NOTHING);
  }

  CbcMip::~CbcMip() {
    delete _prob;
    if (_osi_solver) delete _osi_solver;
    if (_cbc_model) delete _cbc_model;
  }

  const char* CbcMip::_solverName() const { return "CbcMip"; }

  int CbcMip::_addCol() {
    _prob->addColumn(0, 0, 0, -COIN_DBL_MAX, COIN_DBL_MAX, 0.0, 0, false);
    return _prob->numberColumns() - 1;
  }

  CbcMip* CbcMip::newSolver() const {
    CbcMip* newlp = new CbcMip;
    return newlp;
  }

  CbcMip* CbcMip::cloneSolver() const {
    CbcMip* copylp = new CbcMip(*this);
    return copylp;
  }

  int CbcMip::_addRow() {
    _prob->addRow(0, 0, 0, -COIN_DBL_MAX, COIN_DBL_MAX);
    return _prob->numberRows() - 1;
  }

  int CbcMip::_addRow(Value l, ExprIterator b, ExprIterator e, Value u) {
    std::vector<int> indexes;
    std::vector<Value> values;

    for(ExprIterator it = b; it != e; ++it) {
      indexes.push_back(it->first);
      values.push_back(it->second);
    }

    _prob->addRow(values.size(), &indexes.front(), &values.front(), l, u);
    return _prob->numberRows() - 1;
  }

  void CbcMip::_eraseCol(int i) {
    _prob->deleteColumn(i);
  }

  void CbcMip::_eraseRow(int i) {
    _prob->deleteRow(i);
  }

  void CbcMip::_eraseColId(int i) {
    cols.eraseIndex(i);
  }

  void CbcMip::_eraseRowId(int i) {
    rows.eraseIndex(i);
  }

  void CbcMip::_getColName(int c, std::string& name) const {
    name = _prob->getColumnName(c);
  }

  void CbcMip::_setColName(int c, const std::string& name) {
    _prob->setColumnName(c, name.c_str());
  }

  int CbcMip::_colByName(const std::string& name) const {
    return _prob->column(name.c_str());
  }

  void CbcMip::_getRowName(int r, std::string& name) const {
    name = _prob->getRowName(r);
  }

  void CbcMip::_setRowName(int r, const std::string& name) {
    _prob->setRowName(r, name.c_str());
  }

  int CbcMip::_rowByName(const std::string& name) const {
    return _prob->row(name.c_str());
  }

  void CbcMip::_setRowCoeffs(int i, ExprIterator b, ExprIterator e) {
    for (ExprIterator it = b; it != e; ++it) {
      _prob->setElement(i, it->first, it->second);
    }
  }

  void CbcMip::_getRowCoeffs(int ix, InsertIterator b) const {
    int length = _prob->numberRows();

    std::vector<int> indices(length);
    std::vector<Value> values(length);

    length = _prob->getRow(ix, &indices[0], &values[0]);

    for (int i = 0; i < length; ++i) {
      *b = std::make_pair(indices[i], values[i]);
      ++b;
    }
  }

  void CbcMip::_setColCoeffs(int ix, ExprIterator b, ExprIterator e) {
    for (ExprIterator it = b; it != e; ++it) {
      _prob->setElement(it->first, ix, it->second);
    }
  }

  void CbcMip::_getColCoeffs(int ix, InsertIterator b) const {
    int length = _prob->numberColumns();

    std::vector<int> indices(length);
    std::vector<Value> values(length);

    length = _prob->getColumn(ix, &indices[0], &values[0]);

    for (int i = 0; i < length; ++i) {
      *b = std::make_pair(indices[i], values[i]);
      ++b;
    }
  }

  void CbcMip::_setCoeff(int ix, int jx, Value value) {
    _prob->setElement(ix, jx, value);
  }

  CbcMip::Value CbcMip::_getCoeff(int ix, int jx) const {
    return _prob->getElement(ix, jx);
  }


  void CbcMip::_setColLowerBound(int i, Value lo) {
    LEMON_ASSERT(lo != INF, "Invalid bound");
    _prob->setColumnLower(i, lo == - INF ? - COIN_DBL_MAX : lo);
  }

  CbcMip::Value CbcMip::_getColLowerBound(int i) const {
    double val = _prob->getColumnLower(i);
    return val == - COIN_DBL_MAX ? - INF : val;
  }

  void CbcMip::_setColUpperBound(int i, Value up) {
    LEMON_ASSERT(up != -INF, "Invalid bound");
    _prob->setColumnUpper(i, up == INF ? COIN_DBL_MAX : up);
  }

  CbcMip::Value CbcMip::_getColUpperBound(int i) const {
    double val = _prob->getColumnUpper(i);
    return val == COIN_DBL_MAX ? INF : val;
  }

  void CbcMip::_setRowLowerBound(int i, Value lo) {
    LEMON_ASSERT(lo != INF, "Invalid bound");
    _prob->setRowLower(i, lo == - INF ? - COIN_DBL_MAX : lo);
  }

  CbcMip::Value CbcMip::_getRowLowerBound(int i) const {
    double val = _prob->getRowLower(i);
    return val == - COIN_DBL_MAX ? - INF : val;
  }

  void CbcMip::_setRowUpperBound(int i, Value up) {
    LEMON_ASSERT(up != -INF, "Invalid bound");
    _prob->setRowUpper(i, up == INF ? COIN_DBL_MAX : up);
  }

  CbcMip::Value CbcMip::_getRowUpperBound(int i) const {
    double val = _prob->getRowUpper(i);
    return val == COIN_DBL_MAX ? INF : val;
  }

  void CbcMip::_setObjCoeffs(ExprIterator b, ExprIterator e) {
    int num = _prob->numberColumns();
    for (int i = 0; i < num; ++i) {
      _prob->setColumnObjective(i, 0.0);
    }
    for (ExprIterator it = b; it != e; ++it) {
      _prob->setColumnObjective(it->first, it->second);
    }
  }

  void CbcMip::_getObjCoeffs(InsertIterator b) const {
    int num = _prob->numberColumns();
    for (int i = 0; i < num; ++i) {
      Value coef = _prob->getColumnObjective(i);
      if (coef != 0.0) {
        *b = std::make_pair(i, coef);
        ++b;
      }
    }
  }

  void CbcMip::_setObjCoeff(int i, Value obj_coef) {
    _prob->setColumnObjective(i, obj_coef);
  }

  CbcMip::Value CbcMip::_getObjCoeff(int i) const {
    return _prob->getColumnObjective(i);
  }

  CbcMip::SolveExitStatus CbcMip::_solve() {

    if (_osi_solver) {
      delete _osi_solver;
    }
    _osi_solver = new OsiClpSolverInterface();

    _osi_solver->loadFromCoinModel(*_prob);

    if (_cbc_model) {
      delete _cbc_model;
    }
    _cbc_model= new CbcModel(*_osi_solver);

    _osi_solver->messageHandler()->setLogLevel(_message_level);
    _cbc_model->setLogLevel(_message_level);

    _cbc_model->initialSolve();
    _cbc_model->solver()->setHintParam(OsiDoReducePrint, true, OsiHintTry);

    if (!_cbc_model->isInitialSolveAbandoned() &&
        _cbc_model->isInitialSolveProvenOptimal() &&
        !_cbc_model->isInitialSolveProvenPrimalInfeasible() &&
        !_cbc_model->isInitialSolveProvenDualInfeasible()) {

      CglProbing generator1;
      generator1.setUsingObjective(true);
      generator1.setMaxPass(3);
      generator1.setMaxProbe(100);
      generator1.setMaxLook(50);
      generator1.setRowCuts(3);
      _cbc_model->addCutGenerator(&generator1, -1, "Probing");

      CglGomory generator2;
      generator2.setLimit(300);
      _cbc_model->addCutGenerator(&generator2, -1, "Gomory");

      CglKnapsackCover generator3;
      _cbc_model->addCutGenerator(&generator3, -1, "Knapsack");

      CglOddHole generator4;
      generator4.setMinimumViolation(0.005);
      generator4.setMinimumViolationPer(0.00002);
      generator4.setMaximumEntries(200);
      _cbc_model->addCutGenerator(&generator4, -1, "OddHole");

      CglClique generator5;
      generator5.setStarCliqueReport(false);
      generator5.setRowCliqueReport(false);
      _cbc_model->addCutGenerator(&generator5, -1, "Clique");

      CglMixedIntegerRounding mixedGen;
      _cbc_model->addCutGenerator(&mixedGen, -1, "MixedIntegerRounding");

      CglFlowCover flowGen;
      _cbc_model->addCutGenerator(&flowGen, -1, "FlowCover");

      OsiClpSolverInterface* osiclp =
        dynamic_cast<OsiClpSolverInterface*>(_cbc_model->solver());
      if (osiclp->getNumRows() < 300 && osiclp->getNumCols() < 500) {
        osiclp->setupForRepeatedUse(2, 0);
      }

      CbcRounding heuristic1(*_cbc_model);
      heuristic1.setWhen(3);
      _cbc_model->addHeuristic(&heuristic1);

      CbcHeuristicLocal heuristic2(*_cbc_model);
      heuristic2.setWhen(3);
      _cbc_model->addHeuristic(&heuristic2);

      CbcHeuristicGreedyCover heuristic3(*_cbc_model);
      heuristic3.setAlgorithm(11);
      heuristic3.setWhen(3);
      _cbc_model->addHeuristic(&heuristic3);

      CbcHeuristicFPump heuristic4(*_cbc_model);
      heuristic4.setWhen(3);
      _cbc_model->addHeuristic(&heuristic4);

      CbcHeuristicRINS heuristic5(*_cbc_model);
      heuristic5.setWhen(3);
      _cbc_model->addHeuristic(&heuristic5);

      if (_cbc_model->getNumCols() < 500) {
        _cbc_model->setMaximumCutPassesAtRoot(-100);
      } else if (_cbc_model->getNumCols() < 5000) {
        _cbc_model->setMaximumCutPassesAtRoot(100);
      } else {
        _cbc_model->setMaximumCutPassesAtRoot(20);
      }

      if (_cbc_model->getNumCols() < 5000) {
        _cbc_model->setNumberStrong(10);
      }

      _cbc_model->solver()->setIntParam(OsiMaxNumIterationHotStart, 100);
      _cbc_model->branchAndBound();
    }

    if (_cbc_model->isAbandoned()) {
      return UNSOLVED;
    } else {
      return SOLVED;
    }
  }

  CbcMip::Value CbcMip::_getSol(int i) const {
    return _cbc_model->getColSolution()[i];
  }

  CbcMip::Value CbcMip::_getSolValue() const {
    return _cbc_model->getObjValue();
  }

  CbcMip::ProblemType CbcMip::_getType() const {
    if (_cbc_model->isProvenOptimal()) {
      return OPTIMAL;
    } else if (_cbc_model->isContinuousUnbounded()) {
      return UNBOUNDED;
    }
    return FEASIBLE;
  }

  void CbcMip::_setSense(Sense sense) {
    switch (sense) {
    case MIN:
      _prob->setOptimizationDirection(1.0);
      break;
    case MAX:
      _prob->setOptimizationDirection(- 1.0);
      break;
    }
  }

  CbcMip::Sense CbcMip::_getSense() const {
    if (_prob->optimizationDirection() > 0.0) {
      return MIN;
    } else if (_prob->optimizationDirection() < 0.0) {
      return MAX;
    } else {
      LEMON_ASSERT(false, "Wrong sense");
      return CbcMip::Sense();
    }
  }

  void CbcMip::_setColType(int i, CbcMip::ColTypes col_type) {
    switch (col_type){
    case INTEGER:
      _prob->setInteger(i);
      break;
    case REAL:
      _prob->setContinuous(i);
      break;
    default:;
      LEMON_ASSERT(false, "Wrong sense");
    }
  }

  CbcMip::ColTypes CbcMip::_getColType(int i) const {
    return _prob->getColumnIsInteger(i) ? INTEGER : REAL;
  }

  void CbcMip::_clear() {
    delete _prob;
    if (_osi_solver) {
      delete _osi_solver;
      _osi_solver = 0;
    }
    if (_cbc_model) {
      delete _cbc_model;
      _cbc_model = 0;
    }

    _prob = new CoinModel();
  }

  void CbcMip::_messageLevel(MessageLevel level) {
    switch (level) {
    case MESSAGE_NOTHING:
      _message_level = 0;
      break;
    case MESSAGE_ERROR:
      _message_level = 1;
      break;
    case MESSAGE_WARNING:
      _message_level = 1;
      break;
    case MESSAGE_NORMAL:
      _message_level = 2;
      break;
    case MESSAGE_VERBOSE:
      _message_level = 3;
      break;
    }
  }

} //END OF NAMESPACE LEMON
