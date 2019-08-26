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

#ifndef LEMON_LP_H
#define LEMON_LP_H

#include<lemon/config.h>


#ifdef LEMON_HAVE_GLPK
#include <lemon/glpk.h>
#elif LEMON_HAVE_CPLEX
#include <lemon/cplex.h>
#elif LEMON_HAVE_SOPLEX
#include <lemon/soplex.h>
#elif LEMON_HAVE_CLP
#include <lemon/clp.h>
#elif LEMON_HAVE_CBC
#include <lemon/cbc.h>
#endif

///\file
///\brief Defines a default LP solver
///\ingroup lp_group
namespace lemon {

#ifdef DOXYGEN
  ///The default LP solver identifier

  ///The default LP solver identifier.
  ///\ingroup lp_group
  ///
  ///Currently, the possible values are \c _LEMON_GLPK, \c LEMON__CPLEX,
  ///\c _LEMON_SOPLEX or \c LEMON__CLP
#define LEMON_DEFAULT_LP SOLVER
  ///The default LP solver

  ///The default LP solver.
  ///\ingroup lp_group
  ///
  ///Currently, it is either \c GlpkLp, \c CplexLp, \c SoplexLp or \c ClpLp
  typedef GlpkLp Lp;

  ///The default MIP solver identifier

  ///The default MIP solver identifier.
  ///\ingroup lp_group
  ///
  ///Currently, the possible values are \c _LEMON_GLPK, \c LEMON__CPLEX
  ///or \c _LEMON_CBC
#define LEMON_DEFAULT_MIP SOLVER
  ///The default MIP solver.

  ///The default MIP solver.
  ///\ingroup lp_group
  ///
  ///Currently, it is either \c GlpkMip, \c CplexMip , \c CbcMip
  typedef GlpkMip Mip;
#else
#if LEMON_DEFAULT_LP == _LEMON_GLPK
  typedef GlpkLp Lp;
#elif LEMON_DEFAULT_LP == _LEMON_CPLEX
  typedef CplexLp Lp;
#elif LEMON_DEFAULT_LP == _LEMON_SOPLEX
  typedef SoplexLp Lp;
#elif LEMON_DEFAULT_LP == _LEMON_CLP
  typedef ClpLp Lp;
#endif
#if LEMON_DEFAULT_MIP == _LEMON_GLPK
  typedef GlpkMip Mip;
#elif LEMON_DEFAULT_MIP == _LEMON_CPLEX
  typedef CplexMip Mip;
#elif LEMON_DEFAULT_MIP == _LEMON_CBC
  typedef CbcMip Mip;
#endif
#endif

} //namespace lemon

#endif //LEMON_LP_H
