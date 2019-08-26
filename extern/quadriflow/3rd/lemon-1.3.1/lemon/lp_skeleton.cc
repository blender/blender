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

#include <lemon/lp_skeleton.h>

///\file
///\brief A skeleton file to implement LP solver interfaces
namespace lemon {

  int SkeletonSolverBase::_addCol()
  {
    return ++col_num;
  }

  int SkeletonSolverBase::_addRow()
  {
    return ++row_num;
  }

  int SkeletonSolverBase::_addRow(Value, ExprIterator, ExprIterator, Value)
  {
    return ++row_num;
  }

  void SkeletonSolverBase::_eraseCol(int) {}
  void SkeletonSolverBase::_eraseRow(int) {}

  void SkeletonSolverBase::_getColName(int, std::string &) const {}
  void SkeletonSolverBase::_setColName(int, const std::string &) {}
  int SkeletonSolverBase::_colByName(const std::string&) const { return -1; }

  void SkeletonSolverBase::_getRowName(int, std::string &) const {}
  void SkeletonSolverBase::_setRowName(int, const std::string &) {}
  int SkeletonSolverBase::_rowByName(const std::string&) const { return -1; }

  void SkeletonSolverBase::_setRowCoeffs(int, ExprIterator, ExprIterator) {}
  void SkeletonSolverBase::_getRowCoeffs(int, InsertIterator) const {}

  void SkeletonSolverBase::_setColCoeffs(int, ExprIterator, ExprIterator) {}
  void SkeletonSolverBase::_getColCoeffs(int, InsertIterator) const {}

  void SkeletonSolverBase::_setCoeff(int, int, Value) {}
  SkeletonSolverBase::Value SkeletonSolverBase::_getCoeff(int, int) const
  { return 0; }

  void SkeletonSolverBase::_setColLowerBound(int, Value) {}
  SkeletonSolverBase::Value SkeletonSolverBase::_getColLowerBound(int) const
  {  return 0; }

  void SkeletonSolverBase::_setColUpperBound(int, Value) {}
  SkeletonSolverBase::Value SkeletonSolverBase::_getColUpperBound(int) const
  {  return 0; }

  void SkeletonSolverBase::_setRowLowerBound(int, Value) {}
  SkeletonSolverBase::Value SkeletonSolverBase::_getRowLowerBound(int) const
  {  return 0; }

  void SkeletonSolverBase::_setRowUpperBound(int, Value) {}
  SkeletonSolverBase::Value SkeletonSolverBase::_getRowUpperBound(int) const
  {  return 0; }

  void SkeletonSolverBase::_setObjCoeffs(ExprIterator, ExprIterator) {}
  void SkeletonSolverBase::_getObjCoeffs(InsertIterator) const {};

  void SkeletonSolverBase::_setObjCoeff(int, Value) {}
  SkeletonSolverBase::Value SkeletonSolverBase::_getObjCoeff(int) const
  {  return 0; }

  void SkeletonSolverBase::_setSense(Sense) {}
  SkeletonSolverBase::Sense SkeletonSolverBase::_getSense() const
  { return MIN; }

  void SkeletonSolverBase::_clear() {
    row_num = col_num = 0;
  }

  void SkeletonSolverBase::_messageLevel(MessageLevel) {}

  void SkeletonSolverBase::_write(std::string, std::string) const {}

  LpSkeleton::SolveExitStatus LpSkeleton::_solve() { return SOLVED; }

  LpSkeleton::Value LpSkeleton::_getPrimal(int) const { return 0; }
  LpSkeleton::Value LpSkeleton::_getDual(int) const { return 0; }
  LpSkeleton::Value LpSkeleton::_getPrimalValue() const { return 0; }

  LpSkeleton::Value LpSkeleton::_getPrimalRay(int) const { return 0; }
  LpSkeleton::Value LpSkeleton::_getDualRay(int) const { return 0; }

  LpSkeleton::ProblemType LpSkeleton::_getPrimalType() const
  { return UNDEFINED; }

  LpSkeleton::ProblemType LpSkeleton::_getDualType() const
  { return UNDEFINED; }

  LpSkeleton::VarStatus LpSkeleton::_getColStatus(int) const
  { return BASIC; }

  LpSkeleton::VarStatus LpSkeleton::_getRowStatus(int) const
  { return BASIC; }

  LpSkeleton* LpSkeleton::newSolver() const
  { return static_cast<LpSkeleton*>(0); }

  LpSkeleton* LpSkeleton::cloneSolver() const
  { return static_cast<LpSkeleton*>(0); }

  const char* LpSkeleton::_solverName() const { return "LpSkeleton"; }

  MipSkeleton::SolveExitStatus MipSkeleton::_solve()
  { return SOLVED; }

  MipSkeleton::Value MipSkeleton::_getSol(int) const { return 0; }
  MipSkeleton::Value MipSkeleton::_getSolValue() const { return 0; }

  MipSkeleton::ProblemType MipSkeleton::_getType() const
  { return UNDEFINED; }

  MipSkeleton* MipSkeleton::newSolver() const
  { return static_cast<MipSkeleton*>(0); }

  MipSkeleton* MipSkeleton::cloneSolver() const
  { return static_cast<MipSkeleton*>(0); }

  const char* MipSkeleton::_solverName() const { return "MipSkeleton"; }

} //namespace lemon

