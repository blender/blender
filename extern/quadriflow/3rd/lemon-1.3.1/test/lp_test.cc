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

#include <sstream>
#include <lemon/lp_skeleton.h>
#include "test_tools.h"
#include <lemon/tolerance.h>

#include <lemon/config.h>

#ifdef LEMON_HAVE_GLPK
#include <lemon/glpk.h>
#endif

#ifdef LEMON_HAVE_CPLEX
#include <lemon/cplex.h>
#endif

#ifdef LEMON_HAVE_SOPLEX
#include <lemon/soplex.h>
#endif

#ifdef LEMON_HAVE_CLP
#include <lemon/clp.h>
#endif

#ifdef LEMON_HAVE_LP
#include <lemon/lp.h>
#endif
using namespace lemon;

int countCols(LpBase & lp) {
  int count=0;
  for (LpBase::ColIt c(lp); c!=INVALID; ++c) ++count;
  return count;
}

int countRows(LpBase & lp) {
  int count=0;
  for (LpBase::RowIt r(lp); r!=INVALID; ++r) ++count;
  return count;
}


void lpTest(LpSolver& lp)
{

  typedef LpSolver LP;

  // Test LpBase::clear()
  check(countRows(lp)==0, "Wrong number of rows");
  check(countCols(lp)==0, "Wrong number of cols");
  lp.addCol(); lp.addRow(); lp.addRow();
  check(countRows(lp)==2, "Wrong number of rows");
  check(countCols(lp)==1, "Wrong number of cols");
  lp.clear();
  check(countRows(lp)==0, "Wrong number of rows");
  check(countCols(lp)==0, "Wrong number of cols");
  lp.addCol(); lp.addCol(); lp.addCol(); lp.addRow();
  check(countRows(lp)==1, "Wrong number of rows");
  check(countCols(lp)==3, "Wrong number of cols");
  lp.clear();

  std::vector<LP::Col> x(10);
  //  for(int i=0;i<10;i++) x.push_back(lp.addCol());
  lp.addColSet(x);
  lp.colLowerBound(x,1);
  lp.colUpperBound(x,1);
  lp.colBounds(x,1,2);

  std::vector<LP::Col> y(10);
  lp.addColSet(y);

  lp.colLowerBound(y,1);
  lp.colUpperBound(y,1);
  lp.colBounds(y,1,2);

  std::map<int,LP::Col> z;

  z.insert(std::make_pair(12,INVALID));
  z.insert(std::make_pair(2,INVALID));
  z.insert(std::make_pair(7,INVALID));
  z.insert(std::make_pair(5,INVALID));

  lp.addColSet(z);

  lp.colLowerBound(z,1);
  lp.colUpperBound(z,1);
  lp.colBounds(z,1,2);

  {
    LP::Expr e,f,g;
    LP::Col p1,p2,p3,p4,p5;
    LP::Constr c;

    p1=lp.addCol();
    p2=lp.addCol();
    p3=lp.addCol();
    p4=lp.addCol();
    p5=lp.addCol();

    e[p1]=2;
    *e=12;
    e[p1]+=2;
    *e+=12;
    e[p1]-=2;
    *e-=12;

    e=2;
    e=2.2;
    e=p1;
    e=f;

    e+=2;
    e+=2.2;
    e+=p1;
    e+=f;

    e-=2;
    e-=2.2;
    e-=p1;
    e-=f;

    e*=2;
    e*=2.2;
    e/=2;
    e/=2.2;

    e=((p1+p2)+(p1-p2)+(p1+12)+(12+p1)+(p1-12)+(12-p1)+
       (f+12)+(12+f)+(p1+f)+(f+p1)+(f+g)+
       (f-12)+(12-f)+(p1-f)+(f-p1)+(f-g)+
       2.2*f+f*2.2+f/2.2+
       2*f+f*2+f/2+
       2.2*p1+p1*2.2+p1/2.2+
       2*p1+p1*2+p1/2
       );


    c = (e  <= f  );
    c = (e  <= 2.2);
    c = (e  <= 2  );
    c = (e  <= p1 );
    c = (2.2<= f  );
    c = (2  <= f  );
    c = (p1 <= f  );
    c = (p1 <= p2 );
    c = (p1 <= 2.2);
    c = (p1 <= 2  );
    c = (2.2<= p2 );
    c = (2  <= p2 );

    c = (e  >= f  );
    c = (e  >= 2.2);
    c = (e  >= 2  );
    c = (e  >= p1 );
    c = (2.2>= f  );
    c = (2  >= f  );
    c = (p1 >= f  );
    c = (p1 >= p2 );
    c = (p1 >= 2.2);
    c = (p1 >= 2  );
    c = (2.2>= p2 );
    c = (2  >= p2 );

    c = (e  == f  );
    c = (e  == 2.2);
    c = (e  == 2  );
    c = (e  == p1 );
    c = (2.2== f  );
    c = (2  == f  );
    c = (p1 == f  );
    //c = (p1 == p2 );
    c = (p1 == 2.2);
    c = (p1 == 2  );
    c = (2.2== p2 );
    c = (2  == p2 );

    c = ((2 <= e) <= 3);
    c = ((2 <= p1) <= 3);

    c = ((2 >= e) >= 3);
    c = ((2 >= p1) >= 3);

    { //Tests for #430
      LP::Col v=lp.addCol();
      LP::Constr c = v >= -3;
      c = c <= 4;
      LP::Constr c2;
#if ( __GNUC__ == 4 ) && ( __GNUC_MINOR__ == 3 )
      c2 = ( -3 <= v ) <= 4;
#else
      c2 = -3 <= v <= 4;
#endif

    }

    e[x[3]]=2;
    e[x[3]]=4;
    e[x[3]]=1;
    *e=12;

    lp.addRow(-LP::INF,e,23);
    lp.addRow(-LP::INF,3.0*(x[1]+x[2]/2)-x[3],23);
    lp.addRow(-LP::INF,3.0*(x[1]+x[2]*2-5*x[3]+12-x[4]/3)+2*x[4]-4,23);

    lp.addRow(x[1]+x[3]<=x[5]-3);
    lp.addRow((-7<=x[1]+x[3]-12)<=3);
    lp.addRow(x[1]<=x[5]);

    std::ostringstream buf;


    e=((p1+p2)+(p1-0.99*p2));
    //e.prettyPrint(std::cout);
    //(e<=2).prettyPrint(std::cout);
    double tolerance=0.001;
    e.simplify(tolerance);
    buf << "Coeff. of p2 should be 0.01";
    check(e[p2]>0, buf.str());

    tolerance=0.02;
    e.simplify(tolerance);
    buf << "Coeff. of p2 should be 0";
    check(const_cast<const LpSolver::Expr&>(e)[p2]==0, buf.str());

    //Test for clone/new
    LP* lpnew = lp.newSolver();
    LP* lpclone = lp.cloneSolver();
    delete lpnew;
    delete lpclone;

  }

  {
    LP::DualExpr e,f,g;
    LP::Row p1 = INVALID, p2 = INVALID;

    e[p1]=2;
    e[p1]+=2;
    e[p1]-=2;

    e=p1;
    e=f;

    e+=p1;
    e+=f;

    e-=p1;
    e-=f;

    e*=2;
    e*=2.2;
    e/=2;
    e/=2.2;

    e=((p1+p2)+(p1-p2)+
       (p1+f)+(f+p1)+(f+g)+
       (p1-f)+(f-p1)+(f-g)+
       2.2*f+f*2.2+f/2.2+
       2*f+f*2+f/2+
       2.2*p1+p1*2.2+p1/2.2+
       2*p1+p1*2+p1/2
       );
  }

}

void solveAndCheck(LpSolver& lp, LpSolver::ProblemType stat,
                   double exp_opt) {
  using std::string;
  lp.solve();

  std::ostringstream buf;
  buf << "PrimalType should be: " << int(stat) << int(lp.primalType());

  check(lp.primalType()==stat, buf.str());

  if (stat ==  LpSolver::OPTIMAL) {
    std::ostringstream sbuf;
    sbuf << "Wrong optimal value (" << lp.primal() <<") with "
         << lp.solverName() <<"\n     the right optimum is " << exp_opt;
    check(std::abs(lp.primal()-exp_opt) < 1e-3, sbuf.str());
  }
}

void aTest(LpSolver & lp)
{
  typedef LpSolver LP;

 //The following example is very simple

  typedef LpSolver::Row Row;
  typedef LpSolver::Col Col;


  Col x1 = lp.addCol();
  Col x2 = lp.addCol();


  //Constraints
  Row upright=lp.addRow(x1+2*x2 <=1);
  lp.addRow(x1+x2 >=-1);
  lp.addRow(x1-x2 <=1);
  lp.addRow(x1-x2 >=-1);
  //Nonnegativity of the variables
  lp.colLowerBound(x1, 0);
  lp.colLowerBound(x2, 0);
  //Objective function
  lp.obj(x1+x2);

  lp.sense(lp.MAX);

  //Testing the problem retrieving routines
  check(lp.objCoeff(x1)==1,"First term should be 1 in the obj function!");
  check(lp.sense() == lp.MAX,"This is a maximization!");
  check(lp.coeff(upright,x1)==1,"The coefficient in question is 1!");
  check(lp.colLowerBound(x1)==0,
        "The lower bound for variable x1 should be 0.");
  check(lp.colUpperBound(x1)==LpSolver::INF,
        "The upper bound for variable x1 should be infty.");
  check(lp.rowLowerBound(upright) == -LpSolver::INF,
        "The lower bound for the first row should be -infty.");
  check(lp.rowUpperBound(upright)==1,
        "The upper bound for the first row should be 1.");
  LpSolver::Expr e = lp.row(upright);
  check(e[x1] == 1, "The first coefficient should 1.");
  check(e[x2] == 2, "The second coefficient should 1.");

  lp.row(upright, x1+x2 <=1);
  e = lp.row(upright);
  check(e[x1] == 1, "The first coefficient should 1.");
  check(e[x2] == 1, "The second coefficient should 1.");

  LpSolver::DualExpr de = lp.col(x1);
  check(  de[upright] == 1, "The first coefficient should 1.");

  LpSolver* clp = lp.cloneSolver();

  //Testing the problem retrieving routines
  check(clp->objCoeff(x1)==1,"First term should be 1 in the obj function!");
  check(clp->sense() == clp->MAX,"This is a maximization!");
  check(clp->coeff(upright,x1)==1,"The coefficient in question is 1!");
  //  std::cout<<lp.colLowerBound(x1)<<std::endl;
  check(clp->colLowerBound(x1)==0,
        "The lower bound for variable x1 should be 0.");
  check(clp->colUpperBound(x1)==LpSolver::INF,
        "The upper bound for variable x1 should be infty.");

  check(lp.rowLowerBound(upright)==-LpSolver::INF,
        "The lower bound for the first row should be -infty.");
  check(lp.rowUpperBound(upright)==1,
        "The upper bound for the first row should be 1.");
  e = clp->row(upright);
  check(e[x1] == 1, "The first coefficient should 1.");
  check(e[x2] == 1, "The second coefficient should 1.");

  de = clp->col(x1);
  check(de[upright] == 1, "The first coefficient should 1.");

  delete clp;

  //Maximization of x1+x2
  //over the triangle with vertices (0,0) (0,1) (1,0)
  double expected_opt=1;
  solveAndCheck(lp, LpSolver::OPTIMAL, expected_opt);

  //Minimization
  lp.sense(lp.MIN);
  expected_opt=0;
  solveAndCheck(lp, LpSolver::OPTIMAL, expected_opt);

  //Vertex (-1,0) instead of (0,0)
  lp.colLowerBound(x1, -LpSolver::INF);
  expected_opt=-1;
  solveAndCheck(lp, LpSolver::OPTIMAL, expected_opt);

  //Erase one constraint and return to maximization
  lp.erase(upright);
  lp.sense(lp.MAX);
  expected_opt=LpSolver::INF;
  solveAndCheck(lp, LpSolver::UNBOUNDED, expected_opt);

  //Infeasibilty
  lp.addRow(x1+x2 <=-2);
  solveAndCheck(lp, LpSolver::INFEASIBLE, expected_opt);

}

template<class LP>
void cloneTest()
{
  //Test for clone/new

  LP* lp = new LP();
  LP* lpnew = lp->newSolver();
  LP* lpclone = lp->cloneSolver();
  delete lp;
  delete lpnew;
  delete lpclone;
}

int main()
{
  LpSkeleton lp_skel;
  lpTest(lp_skel);

#ifdef LEMON_HAVE_LP
  {
    Lp lp,lp2;
    lpTest(lp);
    aTest(lp2);
    cloneTest<Lp>();
  }
#endif

#ifdef LEMON_HAVE_GLPK
  {
    GlpkLp lp_glpk1,lp_glpk2;
    lpTest(lp_glpk1);
    aTest(lp_glpk2);
    cloneTest<GlpkLp>();
  }
#endif

#ifdef LEMON_HAVE_CPLEX
  try {
    CplexLp lp_cplex1,lp_cplex2;
    lpTest(lp_cplex1);
    aTest(lp_cplex2);
    cloneTest<CplexLp>();
  } catch (CplexEnv::LicenseError& error) {
    check(false, error.what());
  }
#endif

#ifdef LEMON_HAVE_SOPLEX
  {
    SoplexLp lp_soplex1,lp_soplex2;
    lpTest(lp_soplex1);
    aTest(lp_soplex2);
    cloneTest<SoplexLp>();
  }
#endif

#ifdef LEMON_HAVE_CLP
  {
    ClpLp lp_clp1,lp_clp2;
    lpTest(lp_clp1);
    aTest(lp_clp2);
    cloneTest<ClpLp>();
  }
#endif

  return 0;
}
