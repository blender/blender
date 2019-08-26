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

#include <lemon/clp.h>
#include <coin/ClpSimplex.hpp>

namespace lemon {

  ClpLp::ClpLp() {
    _prob = new ClpSimplex();
    _init_temporals();
    messageLevel(MESSAGE_NOTHING);
  }

  ClpLp::ClpLp(const ClpLp& other) {
    _prob = new ClpSimplex(*other._prob);
    rows = other.rows;
    cols = other.cols;
    _init_temporals();
    messageLevel(MESSAGE_NOTHING);
  }

  ClpLp::~ClpLp() {
    delete _prob;
    _clear_temporals();
  }

  void ClpLp::_init_temporals() {
    _primal_ray = 0;
    _dual_ray = 0;
  }

  void ClpLp::_clear_temporals() {
    if (_primal_ray) {
      delete[] _primal_ray;
      _primal_ray = 0;
    }
    if (_dual_ray) {
      delete[] _dual_ray;
      _dual_ray = 0;
    }
  }

  ClpLp* ClpLp::newSolver() const {
    ClpLp* newlp = new ClpLp;
    return newlp;
  }

  ClpLp* ClpLp::cloneSolver() const {
    ClpLp* copylp = new ClpLp(*this);
    return copylp;
  }

  const char* ClpLp::_solverName() const { return "ClpLp"; }

  int ClpLp::_addCol() {
    _prob->addColumn(0, 0, 0, -COIN_DBL_MAX, COIN_DBL_MAX, 0.0);
    return _prob->numberColumns() - 1;
  }

  int ClpLp::_addRow() {
    _prob->addRow(0, 0, 0, -COIN_DBL_MAX, COIN_DBL_MAX);
    return _prob->numberRows() - 1;
  }

  int ClpLp::_addRow(Value l, ExprIterator b, ExprIterator e, Value u) {
    std::vector<int> indexes;
    std::vector<Value> values;

    for(ExprIterator it = b; it != e; ++it) {
      indexes.push_back(it->first);
      values.push_back(it->second);
    }

    _prob->addRow(values.size(), &indexes.front(), &values.front(), l, u);
    return _prob->numberRows() - 1;
  }


  void ClpLp::_eraseCol(int c) {
    _col_names_ref.erase(_prob->getColumnName(c));
    _prob->deleteColumns(1, &c);
  }

  void ClpLp::_eraseRow(int r) {
    _row_names_ref.erase(_prob->getRowName(r));
    _prob->deleteRows(1, &r);
  }

  void ClpLp::_eraseColId(int i) {
    cols.eraseIndex(i);
    cols.shiftIndices(i);
  }

  void ClpLp::_eraseRowId(int i) {
    rows.eraseIndex(i);
    rows.shiftIndices(i);
  }

  void ClpLp::_getColName(int c, std::string& name) const {
    name = _prob->getColumnName(c);
  }

  void ClpLp::_setColName(int c, const std::string& name) {
    _prob->setColumnName(c, const_cast<std::string&>(name));
    _col_names_ref[name] = c;
  }

  int ClpLp::_colByName(const std::string& name) const {
    std::map<std::string, int>::const_iterator it = _col_names_ref.find(name);
    return it != _col_names_ref.end() ? it->second : -1;
  }

  void ClpLp::_getRowName(int r, std::string& name) const {
    name = _prob->getRowName(r);
  }

  void ClpLp::_setRowName(int r, const std::string& name) {
    _prob->setRowName(r, const_cast<std::string&>(name));
    _row_names_ref[name] = r;
  }

  int ClpLp::_rowByName(const std::string& name) const {
    std::map<std::string, int>::const_iterator it = _row_names_ref.find(name);
    return it != _row_names_ref.end() ? it->second : -1;
  }


  void ClpLp::_setRowCoeffs(int ix, ExprIterator b, ExprIterator e) {
    std::map<int, Value> coeffs;

    int n = _prob->clpMatrix()->getNumCols();

    const int* indices = _prob->clpMatrix()->getIndices();
    const double* elements = _prob->clpMatrix()->getElements();

    for (int i = 0; i < n; ++i) {
      CoinBigIndex begin = _prob->clpMatrix()->getVectorStarts()[i];
      CoinBigIndex end = begin + _prob->clpMatrix()->getVectorLengths()[i];

      const int* it = std::lower_bound(indices + begin, indices + end, ix);
      if (it != indices + end && *it == ix && elements[it - indices] != 0.0) {
        coeffs[i] = 0.0;
      }
    }

    for (ExprIterator it = b; it != e; ++it) {
      coeffs[it->first] = it->second;
    }

    for (std::map<int, Value>::iterator it = coeffs.begin();
         it != coeffs.end(); ++it) {
      _prob->modifyCoefficient(ix, it->first, it->second);
    }
  }

  void ClpLp::_getRowCoeffs(int ix, InsertIterator b) const {
    int n = _prob->clpMatrix()->getNumCols();

    const int* indices = _prob->clpMatrix()->getIndices();
    const double* elements = _prob->clpMatrix()->getElements();

    for (int i = 0; i < n; ++i) {
      CoinBigIndex begin = _prob->clpMatrix()->getVectorStarts()[i];
      CoinBigIndex end = begin + _prob->clpMatrix()->getVectorLengths()[i];

      const int* it = std::lower_bound(indices + begin, indices + end, ix);
      if (it != indices + end && *it == ix) {
        *b = std::make_pair(i, elements[it - indices]);
      }
    }
  }

  void ClpLp::_setColCoeffs(int ix, ExprIterator b, ExprIterator e) {
    std::map<int, Value> coeffs;

    CoinBigIndex begin = _prob->clpMatrix()->getVectorStarts()[ix];
    CoinBigIndex end = begin + _prob->clpMatrix()->getVectorLengths()[ix];

    const int* indices = _prob->clpMatrix()->getIndices();
    const double* elements = _prob->clpMatrix()->getElements();

    for (CoinBigIndex i = begin; i != end; ++i) {
      if (elements[i] != 0.0) {
        coeffs[indices[i]] = 0.0;
      }
    }
    for (ExprIterator it = b; it != e; ++it) {
      coeffs[it->first] = it->second;
    }
    for (std::map<int, Value>::iterator it = coeffs.begin();
         it != coeffs.end(); ++it) {
      _prob->modifyCoefficient(it->first, ix, it->second);
    }
  }

  void ClpLp::_getColCoeffs(int ix, InsertIterator b) const {
    CoinBigIndex begin = _prob->clpMatrix()->getVectorStarts()[ix];
    CoinBigIndex end = begin + _prob->clpMatrix()->getVectorLengths()[ix];

    const int* indices = _prob->clpMatrix()->getIndices();
    const double* elements = _prob->clpMatrix()->getElements();

    for (CoinBigIndex i = begin; i != end; ++i) {
      *b = std::make_pair(indices[i], elements[i]);
      ++b;
    }
  }

  void ClpLp::_setCoeff(int ix, int jx, Value value) {
    _prob->modifyCoefficient(ix, jx, value);
  }

  ClpLp::Value ClpLp::_getCoeff(int ix, int jx) const {
    CoinBigIndex begin = _prob->clpMatrix()->getVectorStarts()[ix];
    CoinBigIndex end = begin + _prob->clpMatrix()->getVectorLengths()[ix];

    const int* indices = _prob->clpMatrix()->getIndices();
    const double* elements = _prob->clpMatrix()->getElements();

    const int* it = std::lower_bound(indices + begin, indices + end, jx);
    if (it != indices + end && *it == jx) {
      return elements[it - indices];
    } else {
      return 0.0;
    }
  }

  void ClpLp::_setColLowerBound(int i, Value lo) {
    _prob->setColumnLower(i, lo == - INF ? - COIN_DBL_MAX : lo);
  }

  ClpLp::Value ClpLp::_getColLowerBound(int i) const {
    double val = _prob->getColLower()[i];
    return val == - COIN_DBL_MAX ? - INF : val;
  }

  void ClpLp::_setColUpperBound(int i, Value up) {
    _prob->setColumnUpper(i, up == INF ? COIN_DBL_MAX : up);
  }

  ClpLp::Value ClpLp::_getColUpperBound(int i) const {
    double val = _prob->getColUpper()[i];
    return val == COIN_DBL_MAX ? INF : val;
  }

  void ClpLp::_setRowLowerBound(int i, Value lo) {
    _prob->setRowLower(i, lo == - INF ? - COIN_DBL_MAX : lo);
  }

  ClpLp::Value ClpLp::_getRowLowerBound(int i) const {
    double val = _prob->getRowLower()[i];
    return val == - COIN_DBL_MAX ? - INF : val;
  }

  void ClpLp::_setRowUpperBound(int i, Value up) {
    _prob->setRowUpper(i, up == INF ? COIN_DBL_MAX : up);
  }

  ClpLp::Value ClpLp::_getRowUpperBound(int i) const {
    double val = _prob->getRowUpper()[i];
    return val == COIN_DBL_MAX ? INF : val;
  }

  void ClpLp::_setObjCoeffs(ExprIterator b, ExprIterator e) {
    int num = _prob->clpMatrix()->getNumCols();
    for (int i = 0; i < num; ++i) {
      _prob->setObjectiveCoefficient(i, 0.0);
    }
    for (ExprIterator it = b; it != e; ++it) {
      _prob->setObjectiveCoefficient(it->first, it->second);
    }
  }

  void ClpLp::_getObjCoeffs(InsertIterator b) const {
    int num = _prob->clpMatrix()->getNumCols();
    for (int i = 0; i < num; ++i) {
      Value coef = _prob->getObjCoefficients()[i];
      if (coef != 0.0) {
        *b = std::make_pair(i, coef);
        ++b;
      }
    }
  }

  void ClpLp::_setObjCoeff(int i, Value obj_coef) {
    _prob->setObjectiveCoefficient(i, obj_coef);
  }

  ClpLp::Value ClpLp::_getObjCoeff(int i) const {
    return _prob->getObjCoefficients()[i];
  }

  ClpLp::SolveExitStatus ClpLp::_solve() {
    return _prob->primal() >= 0 ? SOLVED : UNSOLVED;
  }

  ClpLp::SolveExitStatus ClpLp::solvePrimal() {
    return _prob->primal() >= 0 ? SOLVED : UNSOLVED;
  }

  ClpLp::SolveExitStatus ClpLp::solveDual() {
    return _prob->dual() >= 0 ? SOLVED : UNSOLVED;
  }

  ClpLp::SolveExitStatus ClpLp::solveBarrier() {
    return _prob->barrier() >= 0 ? SOLVED : UNSOLVED;
  }

  ClpLp::Value ClpLp::_getPrimal(int i) const {
    return _prob->primalColumnSolution()[i];
  }
  ClpLp::Value ClpLp::_getPrimalValue() const {
    return _prob->objectiveValue();
  }

  ClpLp::Value ClpLp::_getDual(int i) const {
    return _prob->dualRowSolution()[i];
  }

  ClpLp::Value ClpLp::_getPrimalRay(int i) const {
    if (!_primal_ray) {
      _primal_ray = _prob->unboundedRay();
      LEMON_ASSERT(_primal_ray != 0, "Primal ray is not provided");
    }
    return _primal_ray[i];
  }

  ClpLp::Value ClpLp::_getDualRay(int i) const {
    if (!_dual_ray) {
      _dual_ray = _prob->infeasibilityRay();
      LEMON_ASSERT(_dual_ray != 0, "Dual ray is not provided");
    }
    return _dual_ray[i];
  }

  ClpLp::VarStatus ClpLp::_getColStatus(int i) const {
    switch (_prob->getColumnStatus(i)) {
    case ClpSimplex::basic:
      return BASIC;
    case ClpSimplex::isFree:
      return FREE;
    case ClpSimplex::atUpperBound:
      return UPPER;
    case ClpSimplex::atLowerBound:
      return LOWER;
    case ClpSimplex::isFixed:
      return FIXED;
    case ClpSimplex::superBasic:
      return FREE;
    default:
      LEMON_ASSERT(false, "Wrong column status");
      return VarStatus();
    }
  }

  ClpLp::VarStatus ClpLp::_getRowStatus(int i) const {
    switch (_prob->getColumnStatus(i)) {
    case ClpSimplex::basic:
      return BASIC;
    case ClpSimplex::isFree:
      return FREE;
    case ClpSimplex::atUpperBound:
      return UPPER;
    case ClpSimplex::atLowerBound:
      return LOWER;
    case ClpSimplex::isFixed:
      return FIXED;
    case ClpSimplex::superBasic:
      return FREE;
    default:
      LEMON_ASSERT(false, "Wrong row status");
      return VarStatus();
    }
  }


  ClpLp::ProblemType ClpLp::_getPrimalType() const {
    if (_prob->isProvenOptimal()) {
      return OPTIMAL;
    } else if (_prob->isProvenPrimalInfeasible()) {
      return INFEASIBLE;
    } else if (_prob->isProvenDualInfeasible()) {
      return UNBOUNDED;
    } else {
      return UNDEFINED;
    }
  }

  ClpLp::ProblemType ClpLp::_getDualType() const {
    if (_prob->isProvenOptimal()) {
      return OPTIMAL;
    } else if (_prob->isProvenDualInfeasible()) {
      return INFEASIBLE;
    } else if (_prob->isProvenPrimalInfeasible()) {
      return INFEASIBLE;
    } else {
      return UNDEFINED;
    }
  }

  void ClpLp::_setSense(ClpLp::Sense sense) {
    switch (sense) {
    case MIN:
      _prob->setOptimizationDirection(1);
      break;
    case MAX:
      _prob->setOptimizationDirection(-1);
      break;
    }
  }

  ClpLp::Sense ClpLp::_getSense() const {
    double dir = _prob->optimizationDirection();
    if (dir > 0.0) {
      return MIN;
    } else {
      return MAX;
    }
  }

  void ClpLp::_clear() {
    delete _prob;
    _prob = new ClpSimplex();
    _col_names_ref.clear();
    _clear_temporals();
  }

  void ClpLp::_messageLevel(MessageLevel level) {
    switch (level) {
    case MESSAGE_NOTHING:
      _prob->setLogLevel(0);
      break;
    case MESSAGE_ERROR:
      _prob->setLogLevel(1);
      break;
    case MESSAGE_WARNING:
      _prob->setLogLevel(2);
      break;
    case MESSAGE_NORMAL:
      _prob->setLogLevel(3);
      break;
    case MESSAGE_VERBOSE:
      _prob->setLogLevel(4);
      break;
    }
  }

} //END OF NAMESPACE LEMON
