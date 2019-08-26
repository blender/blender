/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2009
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

#include "test_tools.h"

#include <lemon/config.h>

#ifdef LEMON_HAVE_CPLEX
#include <lemon/cplex.h>
#endif

#ifdef LEMON_HAVE_GLPK
#include <lemon/glpk.h>
#endif

#ifdef LEMON_HAVE_CBC
#include <lemon/cbc.h>
#endif

#ifdef LEMON_HAVE_MIP
#include <lemon/lp.h>
#endif


using namespace lemon;

void solveAndCheck(MipSolver& mip, MipSolver::ProblemType stat,
                   double exp_opt) {
  using std::string;

  mip.solve();
  //int decimal,sign;
  std::ostringstream buf;
  buf << "Type should be: " << int(stat)<<" and it is "<<int(mip.type());


  //  itoa(stat,buf1, 10);
  check(mip.type()==stat, buf.str());

  if (stat ==  MipSolver::OPTIMAL) {
    std::ostringstream sbuf;
    sbuf << "Wrong optimal value ("<< mip.solValue()
         <<" instead of " << exp_opt << ")";
    check(std::abs(mip.solValue()-exp_opt) < 1e-3, sbuf.str());
    //+ecvt(exp_opt,2)
  }
}

void aTest(MipSolver& mip)
{
  //The following example is very simple


  typedef MipSolver::Row Row;
  typedef MipSolver::Col Col;


  Col x1 = mip.addCol();
  Col x2 = mip.addCol();


  //Objective function
  mip.obj(x1);

  mip.max();

  //Unconstrained optimization
  mip.solve();
  //Check it out!

  //Constraints
  mip.addRow(2 * x1 + x2 <= 2);
  Row y2 = mip.addRow(x1 - 2 * x2 <= 0);

  //Nonnegativity of the variable x1
  mip.colLowerBound(x1, 0);


  //Maximization of x1
  //over the triangle with vertices (0,0),(4/5,2/5),(0,2)
  double expected_opt=4.0/5.0;
  solveAndCheck(mip, MipSolver::OPTIMAL, expected_opt);


  //Restrict x2 to integer
  mip.colType(x2,MipSolver::INTEGER);
  expected_opt=1.0/2.0;
  solveAndCheck(mip, MipSolver::OPTIMAL, expected_opt);


  //Restrict both to integer
  mip.colType(x1,MipSolver::INTEGER);
  expected_opt=0;
  solveAndCheck(mip, MipSolver::OPTIMAL, expected_opt);

  //Erase a variable
  mip.erase(x2);
  mip.rowUpperBound(y2, 8);
  expected_opt=1;
  solveAndCheck(mip, MipSolver::OPTIMAL, expected_opt);

}


template<class MIP>
void cloneTest()
{

  MIP* mip = new MIP();
  MIP* mipnew = mip->newSolver();
  MIP* mipclone = mip->cloneSolver();
  delete mip;
  delete mipnew;
  delete mipclone;
}

int main()
{

#ifdef LEMON_HAVE_MIP
  {
    Mip mip1;
    aTest(mip1);
    cloneTest<Mip>();
  }
#endif

#ifdef LEMON_HAVE_GLPK
  {
    GlpkMip mip1;
    aTest(mip1);
    cloneTest<GlpkMip>();
  }
#endif

#ifdef LEMON_HAVE_CPLEX
  try {
    CplexMip mip2;
    aTest(mip2);
    cloneTest<CplexMip>();
  } catch (CplexEnv::LicenseError& error) {
    check(false, error.what());
  }
#endif

#ifdef LEMON_HAVE_CBC
  {
    CbcMip mip1;
    aTest(mip1);
    cloneTest<CbcMip>();
  }
#endif

  return 0;

}
