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

#ifndef LEMON_GLPK_H
#define LEMON_GLPK_H

///\file
///\brief Header of the LEMON-GLPK lp solver interface.
///\ingroup lp_group

#include <lemon/lp_base.h>

namespace lemon {

  namespace _solver_bits {
    class VoidPtr {
    private:
      void *_ptr;
    public:
      VoidPtr() : _ptr(0) {}

      template <typename T>
      VoidPtr(T* ptr) : _ptr(reinterpret_cast<void*>(ptr)) {}

      template <typename T>
      VoidPtr& operator=(T* ptr) {
        _ptr = reinterpret_cast<void*>(ptr);
        return *this;
      }

      template <typename T>
      operator T*() const { return reinterpret_cast<T*>(_ptr); }
    };
  }

  /// \brief Base interface for the GLPK LP and MIP solver
  ///
  /// This class implements the common interface of the GLPK LP and MIP solver.
  /// \ingroup lp_group
  class GlpkBase : virtual public LpBase {
  protected:

    _solver_bits::VoidPtr lp;

    GlpkBase();
    GlpkBase(const GlpkBase&);
    virtual ~GlpkBase();

  protected:

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

    virtual void _setSense(Sense);
    virtual Sense _getSense() const;

    virtual void _clear();

    virtual void _messageLevel(MessageLevel level);

    virtual void _write(std::string file, std::string format) const;

  private:

    static void freeEnv();

    struct FreeEnvHelper {
      ~FreeEnvHelper() {
        freeEnv();
      }
    };

    static FreeEnvHelper freeEnvHelper;

  protected:

    int _message_level;

  public:

    ///Pointer to the underlying GLPK data structure.
    _solver_bits::VoidPtr lpx() {return lp;}
    ///Const pointer to the underlying GLPK data structure.
    _solver_bits::VoidPtr lpx() const {return lp;}

    ///Returns the constraint identifier understood by GLPK.
    int lpxRow(Row r) const { return rows(id(r)); }

    ///Returns the variable identifier understood by GLPK.
    int lpxCol(Col c) const { return cols(id(c)); }

#ifdef DOXYGEN
    /// Write the problem or the solution to a file in the given format

    /// This function writes the problem or the solution
    /// to a file in the given format.
    /// Trying to write in an unsupported format will trigger
    /// \ref LpBase::UnsupportedFormatError.
    /// \param file The file path
    /// \param format The output file format.
    /// Supportted formats are "MPS" and "LP".
    void write(std::string file, std::string format = "MPS") const {}
#endif

  };

  /// \brief Interface for the GLPK LP solver
  ///
  /// This class implements an interface for the GLPK LP solver.
  ///\ingroup lp_group
  class GlpkLp : public LpSolver, public GlpkBase {
  public:

    ///\e
    GlpkLp();
    ///\e
    GlpkLp(const GlpkLp&);

    ///\e
    virtual GlpkLp* cloneSolver() const;
    ///\e
    virtual GlpkLp* newSolver() const;

  private:

    mutable std::vector<double> _primal_ray;
    mutable std::vector<double> _dual_ray;

    void _clear_temporals();

  protected:

    virtual const char* _solverName() const;

    virtual SolveExitStatus _solve();
    virtual Value _getPrimal(int i) const;
    virtual Value _getDual(int i) const;

    virtual Value _getPrimalValue() const;

    virtual VarStatus _getColStatus(int i) const;
    virtual VarStatus _getRowStatus(int i) const;

    virtual Value _getPrimalRay(int i) const;
    virtual Value _getDualRay(int i) const;

    virtual ProblemType _getPrimalType() const;
    virtual ProblemType _getDualType() const;

  public:

    ///Solve with primal simplex
    SolveExitStatus solvePrimal();

    ///Solve with dual simplex
    SolveExitStatus solveDual();

  private:

    bool _presolve;

  public:

    ///Turns on or off the presolver

    ///Turns on (\c b is \c true) or off (\c b is \c false) the presolver
    ///
    ///The presolver is off by default.
    void presolver(bool presolve);

  };

  /// \brief Interface for the GLPK MIP solver
  ///
  /// This class implements an interface for the GLPK MIP solver.
  ///\ingroup lp_group
  class GlpkMip : public MipSolver, public GlpkBase {
  public:

    ///\e
    GlpkMip();
    ///\e
    GlpkMip(const GlpkMip&);

    virtual GlpkMip* cloneSolver() const;
    virtual GlpkMip* newSolver() const;

  protected:

    virtual const char* _solverName() const;

    virtual ColTypes _getColType(int col) const;
    virtual void _setColType(int col, ColTypes col_type);

    virtual SolveExitStatus _solve();
    virtual ProblemType _getType() const;
    virtual Value _getSol(int i) const;
    virtual Value _getSolValue() const;

  };


} //END OF NAMESPACE LEMON

#endif //LEMON_GLPK_H

