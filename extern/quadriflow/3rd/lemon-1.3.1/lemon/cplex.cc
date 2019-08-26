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

#include <iostream>
#include <vector>
#include <cstring>

#include <lemon/cplex.h>

extern "C" {
#include <ilcplex/cplex.h>
}


///\file
///\brief Implementation of the LEMON-CPLEX lp solver interface.
namespace lemon {

  CplexEnv::LicenseError::LicenseError(int status) {
    if (!CPXgeterrorstring(0, status, _message)) {
      std::strcpy(_message, "Cplex unknown error");
    }
  }

  CplexEnv::CplexEnv() {
    int status;
    _cnt = new int;
    (*_cnt) = 1;
    _env = CPXopenCPLEX(&status);
    if (_env == 0) {
      delete _cnt;
      _cnt = 0;
      throw LicenseError(status);
    }
  }

  CplexEnv::CplexEnv(const CplexEnv& other) {
    _env = other._env;
    _cnt = other._cnt;
    ++(*_cnt);
  }

  CplexEnv& CplexEnv::operator=(const CplexEnv& other) {
    _env = other._env;
    _cnt = other._cnt;
    ++(*_cnt);
    return *this;
  }

  CplexEnv::~CplexEnv() {
    --(*_cnt);
    if (*_cnt == 0) {
      delete _cnt;
      CPXcloseCPLEX(&_env);
    }
  }

  CplexBase::CplexBase() : LpBase() {
    int status;
    _prob = CPXcreateprob(cplexEnv(), &status, "Cplex problem");
    messageLevel(MESSAGE_NOTHING);
  }

  CplexBase::CplexBase(const CplexEnv& env)
    : LpBase(), _env(env) {
    int status;
    _prob = CPXcreateprob(cplexEnv(), &status, "Cplex problem");
    messageLevel(MESSAGE_NOTHING);
  }

  CplexBase::CplexBase(const CplexBase& cplex)
    : LpBase() {
    int status;
    _prob = CPXcloneprob(cplexEnv(), cplex._prob, &status);
    rows = cplex.rows;
    cols = cplex.cols;
    messageLevel(MESSAGE_NOTHING);
  }

  CplexBase::~CplexBase() {
    CPXfreeprob(cplexEnv(),&_prob);
  }

  int CplexBase::_addCol() {
    int i = CPXgetnumcols(cplexEnv(), _prob);
    double lb = -INF, ub = INF;
    CPXnewcols(cplexEnv(), _prob, 1, 0, &lb, &ub, 0, 0);
    return i;
  }


  int CplexBase::_addRow() {
    int i = CPXgetnumrows(cplexEnv(), _prob);
    const double ub = INF;
    const char s = 'L';
    CPXnewrows(cplexEnv(), _prob, 1, &ub, &s, 0, 0);
    return i;
  }

  int CplexBase::_addRow(Value lb, ExprIterator b,
                         ExprIterator e, Value ub) {
    int i = CPXgetnumrows(cplexEnv(), _prob);
    if (lb == -INF) {
      const char s = 'L';
      CPXnewrows(cplexEnv(), _prob, 1, &ub, &s, 0, 0);
    } else if (ub == INF) {
      const char s = 'G';
      CPXnewrows(cplexEnv(), _prob, 1, &lb, &s, 0, 0);
    } else if (lb == ub){
      const char s = 'E';
      CPXnewrows(cplexEnv(), _prob, 1, &lb, &s, 0, 0);
    } else {
      const char s = 'R';
      double len = ub - lb;
      CPXnewrows(cplexEnv(), _prob, 1, &lb, &s, &len, 0);
    }

    std::vector<int> indices;
    std::vector<int> rowlist;
    std::vector<Value> values;

    for(ExprIterator it=b; it!=e; ++it) {
      indices.push_back(it->first);
      values.push_back(it->second);
      rowlist.push_back(i);
    }

    CPXchgcoeflist(cplexEnv(), _prob, values.size(),
                   &rowlist.front(), &indices.front(), &values.front());

    return i;
  }

  void CplexBase::_eraseCol(int i) {
    CPXdelcols(cplexEnv(), _prob, i, i);
  }

  void CplexBase::_eraseRow(int i) {
    CPXdelrows(cplexEnv(), _prob, i, i);
  }

  void CplexBase::_eraseColId(int i) {
    cols.eraseIndex(i);
    cols.shiftIndices(i);
  }
  void CplexBase::_eraseRowId(int i) {
    rows.eraseIndex(i);
    rows.shiftIndices(i);
  }

  void CplexBase::_getColName(int col, std::string &name) const {
    int size;
    CPXgetcolname(cplexEnv(), _prob, 0, 0, 0, &size, col, col);
    if (size == 0) {
      name.clear();
      return;
    }

    size *= -1;
    std::vector<char> buf(size);
    char *cname;
    int tmp;
    CPXgetcolname(cplexEnv(), _prob, &cname, &buf.front(), size,
                  &tmp, col, col);
    name = cname;
  }

  void CplexBase::_setColName(int col, const std::string &name) {
    char *cname;
    cname = const_cast<char*>(name.c_str());
    CPXchgcolname(cplexEnv(), _prob, 1, &col, &cname);
  }

  int CplexBase::_colByName(const std::string& name) const {
    int index;
    if (CPXgetcolindex(cplexEnv(), _prob,
                       const_cast<char*>(name.c_str()), &index) == 0) {
      return index;
    }
    return -1;
  }

  void CplexBase::_getRowName(int row, std::string &name) const {
    int size;
    CPXgetrowname(cplexEnv(), _prob, 0, 0, 0, &size, row, row);
    if (size == 0) {
      name.clear();
      return;
    }

    size *= -1;
    std::vector<char> buf(size);
    char *cname;
    int tmp;
    CPXgetrowname(cplexEnv(), _prob, &cname, &buf.front(), size,
                  &tmp, row, row);
    name = cname;
  }

  void CplexBase::_setRowName(int row, const std::string &name) {
    char *cname;
    cname = const_cast<char*>(name.c_str());
    CPXchgrowname(cplexEnv(), _prob, 1, &row, &cname);
  }

  int CplexBase::_rowByName(const std::string& name) const {
    int index;
    if (CPXgetrowindex(cplexEnv(), _prob,
                       const_cast<char*>(name.c_str()), &index) == 0) {
      return index;
    }
    return -1;
  }

  void CplexBase::_setRowCoeffs(int i, ExprIterator b,
                                      ExprIterator e)
  {
    std::vector<int> indices;
    std::vector<int> rowlist;
    std::vector<Value> values;

    for(ExprIterator it=b; it!=e; ++it) {
      indices.push_back(it->first);
      values.push_back(it->second);
      rowlist.push_back(i);
    }

    CPXchgcoeflist(cplexEnv(), _prob, values.size(),
                   &rowlist.front(), &indices.front(), &values.front());
  }

  void CplexBase::_getRowCoeffs(int i, InsertIterator b) const {
    int tmp1, tmp2, tmp3, length;
    CPXgetrows(cplexEnv(), _prob, &tmp1, &tmp2, 0, 0, 0, &length, i, i);

    length = -length;
    std::vector<int> indices(length);
    std::vector<double> values(length);

    CPXgetrows(cplexEnv(), _prob, &tmp1, &tmp2,
               &indices.front(), &values.front(),
               length, &tmp3, i, i);

    for (int i = 0; i < length; ++i) {
      *b = std::make_pair(indices[i], values[i]);
      ++b;
    }
  }

  void CplexBase::_setColCoeffs(int i, ExprIterator b, ExprIterator e) {
    std::vector<int> indices;
    std::vector<int> collist;
    std::vector<Value> values;

    for(ExprIterator it=b; it!=e; ++it) {
      indices.push_back(it->first);
      values.push_back(it->second);
      collist.push_back(i);
    }

    CPXchgcoeflist(cplexEnv(), _prob, values.size(),
                   &indices.front(), &collist.front(), &values.front());
  }

  void CplexBase::_getColCoeffs(int i, InsertIterator b) const {

    int tmp1, tmp2, tmp3, length;
    CPXgetcols(cplexEnv(), _prob, &tmp1, &tmp2, 0, 0, 0, &length, i, i);

    length = -length;
    std::vector<int> indices(length);
    std::vector<double> values(length);

    CPXgetcols(cplexEnv(), _prob, &tmp1, &tmp2,
               &indices.front(), &values.front(),
               length, &tmp3, i, i);

    for (int i = 0; i < length; ++i) {
      *b = std::make_pair(indices[i], values[i]);
      ++b;
    }

  }

  void CplexBase::_setCoeff(int row, int col, Value value) {
    CPXchgcoef(cplexEnv(), _prob, row, col, value);
  }

  CplexBase::Value CplexBase::_getCoeff(int row, int col) const {
    CplexBase::Value value;
    CPXgetcoef(cplexEnv(), _prob, row, col, &value);
    return value;
  }

  void CplexBase::_setColLowerBound(int i, Value value) {
    const char s = 'L';
    CPXchgbds(cplexEnv(), _prob, 1, &i, &s, &value);
  }

  CplexBase::Value CplexBase::_getColLowerBound(int i) const {
    CplexBase::Value res;
    CPXgetlb(cplexEnv(), _prob, &res, i, i);
    return res <= -CPX_INFBOUND ? -INF : res;
  }

  void CplexBase::_setColUpperBound(int i, Value value)
  {
    const char s = 'U';
    CPXchgbds(cplexEnv(), _prob, 1, &i, &s, &value);
  }

  CplexBase::Value CplexBase::_getColUpperBound(int i) const {
    CplexBase::Value res;
    CPXgetub(cplexEnv(), _prob, &res, i, i);
    return res >= CPX_INFBOUND ? INF : res;
  }

  CplexBase::Value CplexBase::_getRowLowerBound(int i) const {
    char s;
    CPXgetsense(cplexEnv(), _prob, &s, i, i);
    CplexBase::Value res;

    switch (s) {
    case 'G':
    case 'R':
    case 'E':
      CPXgetrhs(cplexEnv(), _prob, &res, i, i);
      return res <= -CPX_INFBOUND ? -INF : res;
    default:
      return -INF;
    }
  }

  CplexBase::Value CplexBase::_getRowUpperBound(int i) const {
    char s;
    CPXgetsense(cplexEnv(), _prob, &s, i, i);
    CplexBase::Value res;

    switch (s) {
    case 'L':
    case 'E':
      CPXgetrhs(cplexEnv(), _prob, &res, i, i);
      return res >= CPX_INFBOUND ? INF : res;
    case 'R':
      CPXgetrhs(cplexEnv(), _prob, &res, i, i);
      {
        double rng;
        CPXgetrngval(cplexEnv(), _prob, &rng, i, i);
        res += rng;
      }
      return res >= CPX_INFBOUND ? INF : res;
    default:
      return INF;
    }
  }

  //This is easier to implement
  void CplexBase::_set_row_bounds(int i, Value lb, Value ub) {
    if (lb == -INF) {
      const char s = 'L';
      CPXchgsense(cplexEnv(), _prob, 1, &i, &s);
      CPXchgrhs(cplexEnv(), _prob, 1, &i, &ub);
    } else if (ub == INF) {
      const char s = 'G';
      CPXchgsense(cplexEnv(), _prob, 1, &i, &s);
      CPXchgrhs(cplexEnv(), _prob, 1, &i, &lb);
    } else if (lb == ub){
      const char s = 'E';
      CPXchgsense(cplexEnv(), _prob, 1, &i, &s);
      CPXchgrhs(cplexEnv(), _prob, 1, &i, &lb);
    } else {
      const char s = 'R';
      CPXchgsense(cplexEnv(), _prob, 1, &i, &s);
      CPXchgrhs(cplexEnv(), _prob, 1, &i, &lb);
      double len = ub - lb;
      CPXchgrngval(cplexEnv(), _prob, 1, &i, &len);
    }
  }

  void CplexBase::_setRowLowerBound(int i, Value lb)
  {
    LEMON_ASSERT(lb != INF, "Invalid bound");
    _set_row_bounds(i, lb, CplexBase::_getRowUpperBound(i));
  }

  void CplexBase::_setRowUpperBound(int i, Value ub)
  {

    LEMON_ASSERT(ub != -INF, "Invalid bound");
    _set_row_bounds(i, CplexBase::_getRowLowerBound(i), ub);
  }

  void CplexBase::_setObjCoeffs(ExprIterator b, ExprIterator e)
  {
    std::vector<int> indices;
    std::vector<Value> values;
    for(ExprIterator it=b; it!=e; ++it) {
      indices.push_back(it->first);
      values.push_back(it->second);
    }
    CPXchgobj(cplexEnv(), _prob, values.size(),
              &indices.front(), &values.front());

  }

  void CplexBase::_getObjCoeffs(InsertIterator b) const
  {
    int num = CPXgetnumcols(cplexEnv(), _prob);
    std::vector<Value> x(num);

    CPXgetobj(cplexEnv(), _prob, &x.front(), 0, num - 1);
    for (int i = 0; i < num; ++i) {
      if (x[i] != 0.0) {
        *b = std::make_pair(i, x[i]);
        ++b;
      }
    }
  }

  void CplexBase::_setObjCoeff(int i, Value obj_coef)
  {
    CPXchgobj(cplexEnv(), _prob, 1, &i, &obj_coef);
  }

  CplexBase::Value CplexBase::_getObjCoeff(int i) const
  {
    Value x;
    CPXgetobj(cplexEnv(), _prob, &x, i, i);
    return x;
  }

  void CplexBase::_setSense(CplexBase::Sense sense) {
    switch (sense) {
    case MIN:
      CPXchgobjsen(cplexEnv(), _prob, CPX_MIN);
      break;
    case MAX:
      CPXchgobjsen(cplexEnv(), _prob, CPX_MAX);
      break;
    }
  }

  CplexBase::Sense CplexBase::_getSense() const {
    switch (CPXgetobjsen(cplexEnv(), _prob)) {
    case CPX_MIN:
      return MIN;
    case CPX_MAX:
      return MAX;
    default:
      LEMON_ASSERT(false, "Invalid sense");
      return CplexBase::Sense();
    }
  }

  void CplexBase::_clear() {
    CPXfreeprob(cplexEnv(),&_prob);
    int status;
    _prob = CPXcreateprob(cplexEnv(), &status, "Cplex problem");
  }

  void CplexBase::_messageLevel(MessageLevel level) {
    switch (level) {
    case MESSAGE_NOTHING:
      _message_enabled = false;
      break;
    case MESSAGE_ERROR:
    case MESSAGE_WARNING:
    case MESSAGE_NORMAL:
    case MESSAGE_VERBOSE:
      _message_enabled = true;
      break;
    }
  }

  void CplexBase::_applyMessageLevel() {
    CPXsetintparam(cplexEnv(), CPX_PARAM_SCRIND,
                   _message_enabled ? CPX_ON : CPX_OFF);
  }

  void CplexBase::_write(std::string file, std::string format) const
  {
    if(format == "MPS" || format == "LP")
      CPXwriteprob(cplexEnv(), cplexLp(), file.c_str(), format.c_str());
    else if(format == "SOL")
      CPXsolwrite(cplexEnv(), cplexLp(), file.c_str());
    else throw UnsupportedFormatError(format);
  }



  // CplexLp members

  CplexLp::CplexLp()
    : LpBase(), LpSolver(), CplexBase() {}

  CplexLp::CplexLp(const CplexEnv& env)
    : LpBase(), LpSolver(), CplexBase(env) {}

  CplexLp::CplexLp(const CplexLp& other)
    : LpBase(), LpSolver(), CplexBase(other) {}

  CplexLp::~CplexLp() {}

  CplexLp* CplexLp::newSolver() const { return new CplexLp; }
  CplexLp* CplexLp::cloneSolver() const {return new CplexLp(*this); }

  const char* CplexLp::_solverName() const { return "CplexLp"; }

  void CplexLp::_clear_temporals() {
    _col_status.clear();
    _row_status.clear();
    _primal_ray.clear();
    _dual_ray.clear();
  }

  // The routine returns zero unless an error occurred during the
  // optimization. Examples of errors include exhausting available
  // memory (CPXERR_NO_MEMORY) or encountering invalid data in the
  // CPLEX problem object (CPXERR_NO_PROBLEM). Exceeding a
  // user-specified CPLEX limit, or proving the model infeasible or
  // unbounded, are not considered errors. Note that a zero return
  // value does not necessarily mean that a solution exists. Use query
  // routines CPXsolninfo, CPXgetstat, and CPXsolution to obtain
  // further information about the status of the optimization.
  CplexLp::SolveExitStatus CplexLp::convertStatus(int status) {
#if CPX_VERSION >= 800
    if (status == 0) {
      switch (CPXgetstat(cplexEnv(), _prob)) {
      case CPX_STAT_OPTIMAL:
      case CPX_STAT_INFEASIBLE:
      case CPX_STAT_UNBOUNDED:
        return SOLVED;
      default:
        return UNSOLVED;
      }
    } else {
      return UNSOLVED;
    }
#else
    if (status == 0) {
      //We want to exclude some cases
      switch (CPXgetstat(cplexEnv(), _prob)) {
      case CPX_OBJ_LIM:
      case CPX_IT_LIM_FEAS:
      case CPX_IT_LIM_INFEAS:
      case CPX_TIME_LIM_FEAS:
      case CPX_TIME_LIM_INFEAS:
        return UNSOLVED;
      default:
        return SOLVED;
      }
    } else {
      return UNSOLVED;
    }
#endif
  }

  CplexLp::SolveExitStatus CplexLp::_solve() {
    _clear_temporals();
    _applyMessageLevel();
    return convertStatus(CPXlpopt(cplexEnv(), _prob));
  }

  CplexLp::SolveExitStatus CplexLp::solvePrimal() {
    _clear_temporals();
    _applyMessageLevel();
    return convertStatus(CPXprimopt(cplexEnv(), _prob));
  }

  CplexLp::SolveExitStatus CplexLp::solveDual() {
    _clear_temporals();
    _applyMessageLevel();
    return convertStatus(CPXdualopt(cplexEnv(), _prob));
  }

  CplexLp::SolveExitStatus CplexLp::solveBarrier() {
    _clear_temporals();
    _applyMessageLevel();
    return convertStatus(CPXbaropt(cplexEnv(), _prob));
  }

  CplexLp::Value CplexLp::_getPrimal(int i) const {
    Value x;
    CPXgetx(cplexEnv(), _prob, &x, i, i);
    return x;
  }

  CplexLp::Value CplexLp::_getDual(int i) const {
    Value y;
    CPXgetpi(cplexEnv(), _prob, &y, i, i);
    return y;
  }

  CplexLp::Value CplexLp::_getPrimalValue() const {
    Value objval;
    CPXgetobjval(cplexEnv(), _prob, &objval);
    return objval;
  }

  CplexLp::VarStatus CplexLp::_getColStatus(int i) const {
    if (_col_status.empty()) {
      _col_status.resize(CPXgetnumcols(cplexEnv(), _prob));
      CPXgetbase(cplexEnv(), _prob, &_col_status.front(), 0);
    }
    switch (_col_status[i]) {
    case CPX_BASIC:
      return BASIC;
    case CPX_FREE_SUPER:
      return FREE;
    case CPX_AT_LOWER:
      return LOWER;
    case CPX_AT_UPPER:
      return UPPER;
    default:
      LEMON_ASSERT(false, "Wrong column status");
      return CplexLp::VarStatus();
    }
  }

  CplexLp::VarStatus CplexLp::_getRowStatus(int i) const {
    if (_row_status.empty()) {
      _row_status.resize(CPXgetnumrows(cplexEnv(), _prob));
      CPXgetbase(cplexEnv(), _prob, 0, &_row_status.front());
    }
    switch (_row_status[i]) {
    case CPX_BASIC:
      return BASIC;
    case CPX_AT_LOWER:
      {
        char s;
        CPXgetsense(cplexEnv(), _prob, &s, i, i);
        return s != 'L' ? LOWER : UPPER;
      }
    case CPX_AT_UPPER:
      return UPPER;
    default:
      LEMON_ASSERT(false, "Wrong row status");
      return CplexLp::VarStatus();
    }
  }

  CplexLp::Value CplexLp::_getPrimalRay(int i) const {
    if (_primal_ray.empty()) {
      _primal_ray.resize(CPXgetnumcols(cplexEnv(), _prob));
      CPXgetray(cplexEnv(), _prob, &_primal_ray.front());
    }
    return _primal_ray[i];
  }

  CplexLp::Value CplexLp::_getDualRay(int i) const {
    if (_dual_ray.empty()) {

    }
    return _dual_ray[i];
  }

  // Cplex 7.0 status values
  // This table lists the statuses, returned by the CPXgetstat()
  // routine, for solutions to LP problems or mixed integer problems. If
  // no solution exists, the return value is zero.

  // For Simplex, Barrier
  // 1          CPX_OPTIMAL
  //          Optimal solution found
  // 2          CPX_INFEASIBLE
  //          Problem infeasible
  // 3    CPX_UNBOUNDED
  //          Problem unbounded
  // 4          CPX_OBJ_LIM
  //          Objective limit exceeded in Phase II
  // 5          CPX_IT_LIM_FEAS
  //          Iteration limit exceeded in Phase II
  // 6          CPX_IT_LIM_INFEAS
  //          Iteration limit exceeded in Phase I
  // 7          CPX_TIME_LIM_FEAS
  //          Time limit exceeded in Phase II
  // 8          CPX_TIME_LIM_INFEAS
  //          Time limit exceeded in Phase I
  // 9          CPX_NUM_BEST_FEAS
  //          Problem non-optimal, singularities in Phase II
  // 10         CPX_NUM_BEST_INFEAS
  //          Problem non-optimal, singularities in Phase I
  // 11         CPX_OPTIMAL_INFEAS
  //          Optimal solution found, unscaled infeasibilities
  // 12         CPX_ABORT_FEAS
  //          Aborted in Phase II
  // 13         CPX_ABORT_INFEAS
  //          Aborted in Phase I
  // 14          CPX_ABORT_DUAL_INFEAS
  //          Aborted in barrier, dual infeasible
  // 15          CPX_ABORT_PRIM_INFEAS
  //          Aborted in barrier, primal infeasible
  // 16          CPX_ABORT_PRIM_DUAL_INFEAS
  //          Aborted in barrier, primal and dual infeasible
  // 17          CPX_ABORT_PRIM_DUAL_FEAS
  //          Aborted in barrier, primal and dual feasible
  // 18          CPX_ABORT_CROSSOVER
  //          Aborted in crossover
  // 19          CPX_INForUNBD
  //          Infeasible or unbounded
  // 20   CPX_PIVOT
  //       User pivot used
  //
  // Pending return values
  // ??case CPX_ABORT_DUAL_INFEAS
  // ??case CPX_ABORT_CROSSOVER
  // ??case CPX_INForUNBD
  // ??case CPX_PIVOT

  //Some more interesting stuff:

  // CPX_PARAM_PROBMETHOD  1062  int  LPMETHOD
  // 0 Automatic
  // 1 Primal Simplex
  // 2 Dual Simplex
  // 3 Network Simplex
  // 4 Standard Barrier
  // Default: 0
  // Description: Method for linear optimization.
  // Determines which algorithm is used when CPXlpopt() (or "optimize"
  // in the Interactive Optimizer) is called. Currently the behavior of
  // the "Automatic" setting is that CPLEX simply invokes the dual
  // simplex method, but this capability may be expanded in the future
  // so that CPLEX chooses the method based on problem characteristics
#if CPX_VERSION < 900
  void statusSwitch(CPXENVptr cplexEnv(),int& stat){
    int lpmethod;
    CPXgetintparam (cplexEnv(),CPX_PARAM_PROBMETHOD,&lpmethod);
    if (lpmethod==2){
      if (stat==CPX_UNBOUNDED){
        stat=CPX_INFEASIBLE;
      }
      else{
        if (stat==CPX_INFEASIBLE)
          stat=CPX_UNBOUNDED;
      }
    }
  }
#else
  void statusSwitch(CPXENVptr,int&){}
#endif

  CplexLp::ProblemType CplexLp::_getPrimalType() const {
    // Unboundedness not treated well: the following is from cplex 9.0 doc
    // About Unboundedness

    // The treatment of models that are unbounded involves a few
    // subtleties. Specifically, a declaration of unboundedness means that
    // ILOG CPLEX has determined that the model has an unbounded
    // ray. Given any feasible solution x with objective z, a multiple of
    // the unbounded ray can be added to x to give a feasible solution
    // with objective z-1 (or z+1 for maximization models). Thus, if a
    // feasible solution exists, then the optimal objective is
    // unbounded. Note that ILOG CPLEX has not necessarily concluded that
    // a feasible solution exists. Users can call the routine CPXsolninfo
    // to determine whether ILOG CPLEX has also concluded that the model
    // has a feasible solution.

    int stat = CPXgetstat(cplexEnv(), _prob);
#if CPX_VERSION >= 800
    switch (stat)
      {
      case CPX_STAT_OPTIMAL:
        return OPTIMAL;
      case CPX_STAT_UNBOUNDED:
        return UNBOUNDED;
      case CPX_STAT_INFEASIBLE:
        return INFEASIBLE;
      default:
        return UNDEFINED;
      }
#else
    statusSwitch(cplexEnv(),stat);
    //CPXgetstat(cplexEnv(), _prob);
    switch (stat) {
    case 0:
      return UNDEFINED; //Undefined
    case CPX_OPTIMAL://Optimal
      return OPTIMAL;
    case CPX_UNBOUNDED://Unbounded
      return INFEASIBLE;//In case of dual simplex
      //return UNBOUNDED;
    case CPX_INFEASIBLE://Infeasible
      //    case CPX_IT_LIM_INFEAS:
      //     case CPX_TIME_LIM_INFEAS:
      //     case CPX_NUM_BEST_INFEAS:
      //     case CPX_OPTIMAL_INFEAS:
      //     case CPX_ABORT_INFEAS:
      //     case CPX_ABORT_PRIM_INFEAS:
      //     case CPX_ABORT_PRIM_DUAL_INFEAS:
      return UNBOUNDED;//In case of dual simplex
      //return INFEASIBLE;
      //     case CPX_OBJ_LIM:
      //     case CPX_IT_LIM_FEAS:
      //     case CPX_TIME_LIM_FEAS:
      //     case CPX_NUM_BEST_FEAS:
      //     case CPX_ABORT_FEAS:
      //     case CPX_ABORT_PRIM_DUAL_FEAS:
      //       return FEASIBLE;
    default:
      return UNDEFINED; //Everything else comes here
      //FIXME error
    }
#endif
  }

  // Cplex 9.0 status values
  // CPX_STAT_ABORT_DUAL_OBJ_LIM
  // CPX_STAT_ABORT_IT_LIM
  // CPX_STAT_ABORT_OBJ_LIM
  // CPX_STAT_ABORT_PRIM_OBJ_LIM
  // CPX_STAT_ABORT_TIME_LIM
  // CPX_STAT_ABORT_USER
  // CPX_STAT_FEASIBLE_RELAXED
  // CPX_STAT_INFEASIBLE
  // CPX_STAT_INForUNBD
  // CPX_STAT_NUM_BEST
  // CPX_STAT_OPTIMAL
  // CPX_STAT_OPTIMAL_FACE_UNBOUNDED
  // CPX_STAT_OPTIMAL_INFEAS
  // CPX_STAT_OPTIMAL_RELAXED
  // CPX_STAT_UNBOUNDED

  CplexLp::ProblemType CplexLp::_getDualType() const {
    int stat = CPXgetstat(cplexEnv(), _prob);
#if CPX_VERSION >= 800
    switch (stat) {
    case CPX_STAT_OPTIMAL:
      return OPTIMAL;
    case CPX_STAT_UNBOUNDED:
      return INFEASIBLE;
    default:
      return UNDEFINED;
    }
#else
    statusSwitch(cplexEnv(),stat);
    switch (stat) {
    case 0:
      return UNDEFINED; //Undefined
    case CPX_OPTIMAL://Optimal
      return OPTIMAL;
    case CPX_UNBOUNDED:
      return INFEASIBLE;
    default:
      return UNDEFINED; //Everything else comes here
      //FIXME error
    }
#endif
  }

  // CplexMip members

  CplexMip::CplexMip()
    : LpBase(), MipSolver(), CplexBase() {

#if CPX_VERSION < 800
    CPXchgprobtype(cplexEnv(),  _prob, CPXPROB_MIP);
#else
    CPXchgprobtype(cplexEnv(),  _prob, CPXPROB_MILP);
#endif
  }

  CplexMip::CplexMip(const CplexEnv& env)
    : LpBase(), MipSolver(), CplexBase(env) {

#if CPX_VERSION < 800
    CPXchgprobtype(cplexEnv(),  _prob, CPXPROB_MIP);
#else
    CPXchgprobtype(cplexEnv(),  _prob, CPXPROB_MILP);
#endif

  }

  CplexMip::CplexMip(const CplexMip& other)
    : LpBase(), MipSolver(), CplexBase(other) {}

  CplexMip::~CplexMip() {}

  CplexMip* CplexMip::newSolver() const { return new CplexMip; }
  CplexMip* CplexMip::cloneSolver() const {return new CplexMip(*this); }

  const char* CplexMip::_solverName() const { return "CplexMip"; }

  void CplexMip::_setColType(int i, CplexMip::ColTypes col_type) {

    // Note If a variable is to be changed to binary, a call to CPXchgbds
    // should also be made to change the bounds to 0 and 1.

    switch (col_type){
    case INTEGER: {
      const char t = 'I';
      CPXchgctype (cplexEnv(), _prob, 1, &i, &t);
    } break;
    case REAL: {
      const char t = 'C';
      CPXchgctype (cplexEnv(), _prob, 1, &i, &t);
    } break;
    default:
      break;
    }
  }

  CplexMip::ColTypes CplexMip::_getColType(int i) const {
    char t;
    CPXgetctype (cplexEnv(), _prob, &t, i, i);
    switch (t) {
    case 'I':
      return INTEGER;
    case 'C':
      return REAL;
    default:
      LEMON_ASSERT(false, "Invalid column type");
      return ColTypes();
    }

  }

  CplexMip::SolveExitStatus CplexMip::_solve() {
    int status;
    _applyMessageLevel();
    status = CPXmipopt (cplexEnv(), _prob);
    if (status==0)
      return SOLVED;
    else
      return UNSOLVED;

  }


  CplexMip::ProblemType CplexMip::_getType() const {

    int stat = CPXgetstat(cplexEnv(), _prob);

    //Fortunately, MIP statuses did not change for cplex 8.0
    switch (stat) {
    case CPXMIP_OPTIMAL:
      // Optimal integer solution has been found.
    case CPXMIP_OPTIMAL_TOL:
      // Optimal soluton with the tolerance defined by epgap or epagap has
      // been found.
      return OPTIMAL;
      //This also exists in later issues
      //    case CPXMIP_UNBOUNDED:
      //return UNBOUNDED;
      case CPXMIP_INFEASIBLE:
        return INFEASIBLE;
    default:
      return UNDEFINED;
    }
    //Unboundedness not treated well: the following is from cplex 9.0 doc
    // About Unboundedness

    // The treatment of models that are unbounded involves a few
    // subtleties. Specifically, a declaration of unboundedness means that
    // ILOG CPLEX has determined that the model has an unbounded
    // ray. Given any feasible solution x with objective z, a multiple of
    // the unbounded ray can be added to x to give a feasible solution
    // with objective z-1 (or z+1 for maximization models). Thus, if a
    // feasible solution exists, then the optimal objective is
    // unbounded. Note that ILOG CPLEX has not necessarily concluded that
    // a feasible solution exists. Users can call the routine CPXsolninfo
    // to determine whether ILOG CPLEX has also concluded that the model
    // has a feasible solution.
  }

  CplexMip::Value CplexMip::_getSol(int i) const {
    Value x;
    CPXgetmipx(cplexEnv(), _prob, &x, i, i);
    return x;
  }

  CplexMip::Value CplexMip::_getSolValue() const {
    Value objval;
    CPXgetmipobjval(cplexEnv(), _prob, &objval);
    return objval;
  }

} //namespace lemon

