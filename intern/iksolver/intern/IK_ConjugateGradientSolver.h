/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef NAN_INCLUDED_IK_ConjugateGradientSolver_h

#define NAN_INCLUDED_IK_ConjugateGradientSolver_h

/**
 * @author Laurence Bourn
 * @date 28/6/2001
 */

#include "TNT/cmat.h"
#include "MT_Scalar.h"
#include "IK_LineMinimizer.h"

/** 
 * These classes locally minimize n dimensional potenial functions.
 * See Numerical Recipes in C www.nr.com for more details.
 * If a n dimensionable potenial function is 
 * differentiable, then it is diferentiable along
 * any vector x. This can be found by the dot product
 * of the gradient operator with x. 
 * The conjugate gradient solver boils down to 
 * a collection of line minimizations along various lines
 * defined by position,direction pairs. There are 
 * methods in this class to set the lines along which
 * minimizations via the DiffentiablePotenialFunction1d interface
 * are to be performed.
 *	
 * @warning I don't like data inheritance but it is the most efficient
 * wasy to do this here.
 */


class DifferentiablePotenialFunctionNd 
: public DifferentiablePotenialFunction1d 
{
public :

	/** 
	 * Inherited from DiffentiablePotenialFunction1d
	 *
	 * virtual
	 *	MT_Scalar 
	 * Evaluate1d(
	 * 	MT_Scalar x
	 * ) = 0;
     *
	 * virtual
	 * 	MT_Scalar
	 * Derivative1d(
	 * 	MT_Scalar x
	 * ) = 0;
	 */

	/// Methods to set the current line in N dimensions

		void
	SetLineVector(
		const TNT::Vector<MT_Scalar> &pos,
		const TNT::Vector<MT_Scalar> &dir
	){
		m_line_pos = pos;
		m_line_dir = dir;
	};

	virtual
		MT_Scalar
	Evaluate(
		const TNT::Vector<MT_Scalar> &x
	) = 0;

	virtual
		void
	Derivative(
		const TNT::Vector<MT_Scalar> &x,
		TNT::Vector<MT_Scalar> &dy
	) = 0;

	/// @return The dimension of the domain of the potenial function

	virtual
		int
	Dimension(
	) const =0;
	
	virtual
	~DifferentiablePotenialFunctionNd(
	){
	};

protected :

	DifferentiablePotenialFunctionNd(){};

	TNT::Vector<MT_Scalar> m_line_pos;
	TNT::Vector<MT_Scalar> m_line_dir;

};


class IK_ConjugateGradientSolver 
: public MEM_NonCopyable
{
public :

	/** 
	 * This class necessarily needs some (potenially large) 
	 * temporary vectors to aid computation. We therefore
	 * insist creation of these objects on the heap.
	 */

	static
		IK_ConjugateGradientSolver *
	New(
	);

	/** 
	 * Compute the minimum of the potenial function
	 * starting at point p. On return p contains the 
	 * computed minima, iter the number of iterations performed,
	 * fret the potenial value at the minima
	 */

		void
	Solve(
		TNT::Vector<MT_Scalar> &p,
		MT_Scalar ftol,
		int &iter,
		MT_Scalar &fret,
		DifferentiablePotenialFunctionNd &potenial,
		int max_its = 200
	);

	~IK_ConjugateGradientSolver(
	);

private :
		void
	LineMinimize(
		TNT::Vector<MT_Scalar> & p,
		const TNT::Vector<MT_Scalar> & xi,
		MT_Scalar &fret,
		DifferentiablePotenialFunctionNd &potenial
	);

	IK_ConjugateGradientSolver(
	);

		void
	ArmVectors(
		int dimension
	);


	TNT::Vector<MT_Scalar> m_g;
	TNT::Vector<MT_Scalar> m_h;
	TNT::Vector<MT_Scalar> m_xi;

	TNT::Vector<MT_Scalar> m_xi_temp;

};

#endif

