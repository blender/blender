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

#ifndef LEMON_CPLEX_H
#define LEMON_CPLEX_H

///\file
///\brief Header of the LEMON-CPLEX lp solver interface.

#include <lemon/lp_base.h>

struct cpxenv;
struct cpxlp;

namespace lemon {

  /// \brief Reference counted wrapper around cpxenv pointer
  ///
  /// The cplex uses environment object which is responsible for
  /// checking the proper license usage. This class provides a simple
  /// interface for share the environment object between different
  /// problems.
  class CplexEnv {
    friend class CplexBase;
  private:
    cpxenv* _env;
    mutable int* _cnt;

  public:

    /// \brief This exception is thrown when the license check is not
    /// sufficient
    class LicenseError : public Exception {
      friend class CplexEnv;
    private:

      LicenseError(int status);
      char _message[510];

    public:

      /// The short error message
      virtual const char* what() const throw() {
        return _message;
      }
    };

    /// Constructor
    CplexEnv();
    /// Shallow copy constructor
    CplexEnv(const CplexEnv&);
    /// Shallow assignement
    CplexEnv& operator=(const CplexEnv&);
    /// Destructor
    virtual ~CplexEnv();

  protected:

    cpxenv* cplexEnv() { return _env; }
    const cpxenv* cplexEnv() const { return _env; }
  };

  /// \brief Base interface for the CPLEX LP and MIP solver
  ///
  /// This class implements the common interface of the CPLEX LP and
  /// MIP solvers.
  /// \ingroup lp_group
  class CplexBase : virtual public LpBase {
  protected:

    CplexEnv _env;
    cpxlp* _prob;

    CplexBase();
    CplexBase(const CplexEnv&);
    CplexBase(const CplexBase &);
    virtual ~CplexBase();

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

  private:
    void _set_row_bounds(int i, Value lb, Value ub);
  protected:

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

    virtual void _clear();

    virtual void _messageLevel(MessageLevel level);
    void _applyMessageLevel();

    bool _message_enabled;

    void _write(std::string file, std::string format) const;

  public:

    /// Returns the used \c CplexEnv instance
    const CplexEnv& env() const { return _env; }

    /// \brief Returns the const cpxenv pointer
    ///
    /// \note The cpxenv might be destructed with the solver.
    const cpxenv* cplexEnv() const { return _env.cplexEnv(); }

    /// \brief Returns the const cpxenv pointer
    ///
    /// \note The cpxenv might be destructed with the solver.
    cpxenv* cplexEnv() { return _env.cplexEnv(); }

    /// Returns the cplex problem object
    cpxlp* cplexLp() { return _prob; }
    /// Returns the cplex problem object
    const cpxlp* cplexLp() const { return _prob; }

#ifdef DOXYGEN
    /// Write the problem or the solution to a file in the given format

    /// This function writes the problem or the solution
    /// to a file in the given format.
    /// Trying to write in an unsupported format will trigger
    /// \ref lemon::LpBase::UnsupportedFormatError "UnsupportedFormatError".
    /// \param file The file path
    /// \param format The output file format.
    /// Supportted formats are "MPS", "LP" and "SOL".
    void write(std::string file, std::string format = "MPS") const {}
#endif

  };

  /// \brief Interface for the CPLEX LP solver
  ///
  /// This class implements an interface for the CPLEX LP solver.
  ///\ingroup lp_group
  class CplexLp : public LpSolver, public CplexBase {
  public:
    /// \e
    CplexLp();
    /// \e
    CplexLp(const CplexEnv&);
    /// \e
    CplexLp(const CplexLp&);
    /// \e
    virtual ~CplexLp();

    /// \e
    virtual CplexLp* cloneSolver() const;
    /// \e
    virtual CplexLp* newSolver() const;

  private:

    // these values cannot retrieved element by element
    mutable std::vector<int> _col_status;
    mutable std::vector<int> _row_status;

    mutable std::vector<Value> _primal_ray;
    mutable std::vector<Value> _dual_ray;

    void _clear_temporals();

    SolveExitStatus convertStatus(int status);

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

    /// Solve with primal simplex method
    SolveExitStatus solvePrimal();

    /// Solve with dual simplex method
    SolveExitStatus solveDual();

    /// Solve with barrier method
    SolveExitStatus solveBarrier();

  };

  /// \brief Interface for the CPLEX MIP solver
  ///
  /// This class implements an interface for the CPLEX MIP solver.
  ///\ingroup lp_group
  class CplexMip : public MipSolver, public CplexBase {
  public:
    /// \e
    CplexMip();
    /// \e
    CplexMip(const CplexEnv&);
    /// \e
    CplexMip(const CplexMip&);
    /// \e
    virtual ~CplexMip();

    /// \e
    virtual CplexMip* cloneSolver() const;
    /// \e
    virtual CplexMip* newSolver() const;

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

#endif //LEMON_CPLEX_H

