/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is:
 *     GTS - Library for the manipulation of triangulated surfaces
 *     Copyright (C) 1999 Stephane Popinet
 * and:
 *     OGF/Graphite: Geometry and Graphics Programming Library + Utilities
 *     Copyright (C) 2000-2003 Bruno Levy
 *     Contact: Bruno Levy levy@loria.fr
 *         ISA Project
 *         LORIA, INRIA Lorraine,
 *         Campus Scientifique, BP 239
 *         54506 VANDOEUVRE LES NANCY CEDEX
 *         FRANCE
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/freestyle/intern/winged_edge/Curvature.cpp
 *  \ingroup freestyle
 *  \brief GTS - Library for the manipulation of triangulated surfaces
 *  \author Stephane Popinet
 *  \date 1999
 *  \brief OGF/Graphite: Geometry and Graphics Programming Library + Utilities
 *  \author Bruno Levy
 *  \date 2000-2003
 */

#include <assert.h>
#include <cstdlib> // for malloc and free
#include <set>
#include <stack>

#include "Curvature.h"
#include "WEdge.h"

#include "../geometry/normal_cycle.h"

#include "BLI_math.h"

namespace Freestyle {

static bool angle_obtuse(WVertex *v, WFace *f)
{
	WOEdge *e;
	f->getOppositeEdge(v, e);

	Vec3r vec1(e->GetaVertex()->GetVertex() - v->GetVertex());
	Vec3r vec2(e->GetbVertex()->GetVertex() - v->GetVertex());
	return ((vec1 * vec2) < 0);
}

// FIXME
// WVvertex is useless but kept for history reasons
static bool triangle_obtuse(WVertex *, WFace *f)
{
	bool b = false;
	for (int i = 0; i < 3; i++)
		b = b || ((f->getEdgeList()[i]->GetVec() * f->getEdgeList()[(i + 1) % 3]->GetVec()) < 0);
	return b;
}

static real cotan(WVertex *vo, WVertex *v1, WVertex *v2)
{
	/* cf. Appendix B of [Meyer et al 2002] */
	real udotv, denom;

	Vec3r u(v1->GetVertex() - vo->GetVertex());
	Vec3r v(v2->GetVertex() - vo->GetVertex());

	udotv = u * v;
	denom = sqrt(u.squareNorm() * v.squareNorm() - udotv * udotv);

	/* denom can be zero if u==v.  Returning 0 is acceptable, based on the callers of this function below. */
	if (denom == 0.0)
		return 0.0;
	return (udotv / denom);
}

static real angle_from_cotan(WVertex *vo, WVertex *v1, WVertex *v2)
{
	/* cf. Appendix B and the caption of Table 1 from [Meyer et al 2002] */
	real udotv, denom;

	Vec3r u (v1->GetVertex() - vo->GetVertex());
	Vec3r v(v2->GetVertex() - vo->GetVertex());

	udotv = u * v;
	denom = sqrt(u.squareNorm() * v.squareNorm() - udotv * udotv);

	/* Note: I assume this is what they mean by using atan2(). -Ray Jones */

	/* tan = denom/udotv = y/x (see man page for atan2) */
	return (fabs(atan2(denom, udotv)));
}

/*! gts_vertex_mean_curvature_normal:
 *  @v: a #WVertex.
 *  @s: a #GtsSurface.
 *  @Kh: the Mean Curvature Normal at @v.
 *
 *  Computes the Discrete Mean Curvature Normal approximation at @v.
 *  The mean curvature at @v is half the magnitude of the vector @Kh.
 *
 *  Note: the normal computed is not unit length, and may point either into or out of the surface, depending on
 *  the curvature at @v. It is the responsibility of the caller of the function to use the mean curvature normal
 *  appropriately.
 *
 *  This approximation is from the paper:
 *      Discrete Differential-Geometry Operators for Triangulated 2-Manifolds
 *      Mark Meyer, Mathieu Desbrun, Peter Schroder, Alan H. Barr
 *      VisMath '02, Berlin (Germany)
 *      http://www-grail.usc.edu/pubs.html
 *
 *  Returns: %true if the operator could be evaluated, %false if the evaluation failed for some reason (@v is
 *  boundary or is the endpoint of a non-manifold edge.)
 */
bool gts_vertex_mean_curvature_normal(WVertex *v, Vec3r &Kh)
{
	real area = 0.0;

	if (!v)
		return false;

	/* this operator is not defined for boundary edges */
	if (v->isBoundary())
		return false;

	WVertex::incoming_edge_iterator itE;

	for (itE = v->incoming_edges_begin(); itE != v->incoming_edges_end(); itE++)
		area += (*itE)->GetaFace()->getArea();

	Kh = Vec3r(0.0, 0.0, 0.0);

	for (itE = v->incoming_edges_begin(); itE != v->incoming_edges_end(); itE++) {
		WOEdge *e = (*itE)->getPrevOnFace();
#if 0
		if ((e->GetaVertex() == v) || (e->GetbVertex() == v))
			cerr<< "BUG ";
#endif
		WVertex *v1 = e->GetaVertex();
		WVertex *v2 = e->GetbVertex();
		real temp;

		temp = cotan(v1, v, v2);
		Kh = Vec3r(Kh + temp * (v2->GetVertex() - v->GetVertex()));

		temp = cotan(v2, v, v1);
		Kh = Vec3r(Kh + temp * (v1->GetVertex() - v->GetVertex()));
	}
	if (area > 0.0) {
		Kh[0] /= 2 * area;
		Kh[1] /= 2 * area;
		Kh[2] /= 2 * area;
	}
	else {
		return false;
	}

	return true;
}

/*! gts_vertex_gaussian_curvature:
 *  @v: a #WVertex.
 *  @s: a #GtsSurface.
 *  @Kg: the Discrete Gaussian Curvature approximation at @v.
 *
 *  Computes the Discrete Gaussian Curvature approximation at @v.
 *
 *  This approximation is from the paper:
 *      Discrete Differential-Geometry Operators for Triangulated 2-Manifolds
 *      Mark Meyer, Mathieu Desbrun, Peter Schroder, Alan H. Barr
 *      VisMath '02, Berlin (Germany)
 *      http://www-grail.usc.edu/pubs.html
 *
 *  Returns: %true if the operator could be evaluated, %false if the evaluation failed for some reason (@v is
 * boundary or is the endpoint of a non-manifold edge.)
 */
bool gts_vertex_gaussian_curvature(WVertex *v, real *Kg)
{
	real area = 0.0;
	real angle_sum = 0.0;

	if (!v)
		return false;
	if (!Kg)
		return false;

	/* this operator is not defined for boundary edges */
	if (v->isBoundary()) {
		*Kg = 0.0;
		return false;
	}

	WVertex::incoming_edge_iterator itE;
	for (itE = v->incoming_edges_begin(); itE != v->incoming_edges_end(); itE++)
		 area += (*itE)->GetaFace()->getArea();

	for (itE = v->incoming_edges_begin(); itE != v->incoming_edges_end(); itE++) {
		WOEdge *e = (*itE)->getPrevOnFace();
		WVertex *v1 = e->GetaVertex();
		WVertex *v2 = e->GetbVertex();
		angle_sum += angle_from_cotan(v, v1, v2);
	}

	*Kg = (2.0 * M_PI - angle_sum) / area;

	return true;
}

/*! gts_vertex_principal_curvatures:
 *  @Kh: mean curvature.
 *  @Kg: Gaussian curvature.
 *  @K1: first principal curvature.
 *  @K2: second principal curvature.
 *
 *  Computes the principal curvatures at a point given the mean and Gaussian curvatures at that point.
 *
 *  The mean curvature can be computed as one-half the magnitude of the vector computed by
 *  gts_vertex_mean_curvature_normal().
 *
 *  The Gaussian curvature can be computed with gts_vertex_gaussian_curvature().
 */
void gts_vertex_principal_curvatures (real Kh, real Kg, real *K1, real *K2)
{
	real temp = Kh * Kh - Kg;

	if (!K1 || !K2)
		return;

	if (temp < 0.0)
		temp = 0.0;
	temp = sqrt (temp);
	*K1 = Kh + temp;
	*K2 = Kh - temp;
}

/* from Maple */
static void linsolve(real m11, real m12, real b1, real m21, real m22, real b2, real *x1, real *x2)
{
	real temp;

	temp = 1.0 / (m21 * m12 - m11 * m22);
	*x1 = (m12 * b2 - m22 * b1) * temp;
	*x2 = (m11 * b2 - m21 * b1) * temp;
}

/* from Maple - largest eigenvector of [a b; b c] */
static void eigenvector(real a, real b, real c, Vec3r e)
{
	if (b == 0.0) {
		e[0] = 0.0;
	}
	else {
		e[0] = -(c - a - sqrt(c * c - 2 * a * c + a * a + 4 * b * b)) / (2 * b);
	}
	e[1] = 1.0;
	e[2] = 0.0;
}

/*! gts_vertex_principal_directions:
 *  @v: a #WVertex.
 *  @s: a #GtsSurface.
 *  @Kh: mean curvature normal (a #Vec3r).
 *  @Kg: Gaussian curvature (a real).
 *  @e1: first principal curvature direction (direction of largest curvature).
 *  @e2: second principal curvature direction.
 *
 *  Computes the principal curvature directions at a point given @Kh and @Kg, the mean curvature normal and
 *  Gaussian curvatures at that point, computed with gts_vertex_mean_curvature_normal() and
 *  gts_vertex_gaussian_curvature(), respectively.
 *
 *  Note that this computation is very approximate and tends to be unstable. Smoothing of the surface or the principal
 *  directions may be necessary to achieve reasonable results.
 */
void gts_vertex_principal_directions(WVertex *v, Vec3r Kh, real Kg, Vec3r &e1, Vec3r &e2)
{
	Vec3r N;
	real normKh;

	Vec3r basis1, basis2, d, eig;
	real ve2, vdotN;
	real aterm_da, bterm_da, cterm_da, const_da;
	real aterm_db, bterm_db, cterm_db, const_db;
	real a, b, c;
	real K1, K2;
	real *weights, *kappas, *d1s, *d2s;
	int edge_count;
	real err_e1, err_e2;
	int e;
	WVertex::incoming_edge_iterator itE;

	/* compute unit normal */
	normKh = Kh.norm();

	if (normKh > 0.0) {
		Kh.normalize();
	}
	else {
		/* This vertex is a point of zero mean curvature (flat or saddle point). Compute a normal by averaging
		 * the adjacent triangles
		 */
		N[0] = N[1] = N[2] = 0.0;

		for (itE = v->incoming_edges_begin(); itE != v->incoming_edges_end(); itE++)
			N = Vec3r(N + (*itE)->GetaFace()->GetNormal());
		real normN = N.norm();
		if (normN <= 0.0)
			return;
		N.normalize();
	}

	/* construct a basis from N: */
	/* set basis1 to any component not the largest of N */
	basis1[0] =  basis1[1] =  basis1[2] = 0.0;
	if (fabs (N[0]) > fabs (N[1]))
		basis1[1] = 1.0;
	else
		basis1[0] = 1.0;

	/* make basis2 orthogonal to N */
	basis2 = (N ^ basis1);
	basis2.normalize();

	/* make basis1 orthogonal to N and basis2 */
	basis1 = (N ^ basis2);
	basis1.normalize();

	aterm_da = bterm_da = cterm_da = const_da = 0.0;
	aterm_db = bterm_db = cterm_db = const_db = 0.0;
	int nb_edges = v->GetEdges().size();

	weights = (real *)malloc(sizeof(real) * nb_edges);
	kappas = (real *)malloc(sizeof(real) * nb_edges);
	d1s = (real *)malloc(sizeof(real) * nb_edges);
	d2s = (real *)malloc(sizeof(real) * nb_edges);
	edge_count = 0;

	for (itE = v->incoming_edges_begin(); itE != v->incoming_edges_end(); itE++) {
		WOEdge *e;
		WFace *f1, *f2;
		real weight, kappa, d1, d2;
		Vec3r vec_edge;
		if (!*itE)
			continue;
		e = *itE;

		/* since this vertex passed the tests in gts_vertex_mean_curvature_normal(), this should be true. */
		//g_assert(gts_edge_face_number (e, s) == 2);

		/* identify the two triangles bordering e in s */
		f1 = e->GetaFace();
		f2 = e->GetbFace();

		/* We are solving for the values of the curvature tensor
		*     B = [ a b ; b c ].
		*  The computations here are from section 5 of [Meyer et al 2002].
		*
		*  The first step is to calculate the linear equations governing the values of (a,b,c). These can be computed
		*  by setting the derivatives of the error E to zero (section 5.3).
		*
		*  Since a + c = norm(Kh), we only compute the linear equations for dE/da and dE/db. (NB: [Meyer et al 2002]
		*  has the equation a + b = norm(Kh), but I'm almost positive this is incorrect).
		*
		*  Note that the w_ij (defined in section 5.2) are all scaled by (1/8*A_mixed). We drop this uniform scale
		*  factor because the solution of the linear equations doesn't rely on it.
		*
		*  The terms of the linear equations are xterm_dy with x in {a,b,c} and y in {a,b}. There are also const_dy
		*  terms that are the constant factors in the equations.
		*/

		/* find the vector from v along edge e */
		vec_edge = Vec3r(-1 * e->GetVec());

		ve2 = vec_edge.squareNorm();
		vdotN = vec_edge * N;

		/* section 5.2 - There is a typo in the computation of kappa. The edges should be x_j-x_i. */
		kappa = 2.0 * vdotN / ve2;

		/* section 5.2 */

		/* I don't like performing a minimization where some of the weights can be negative (as can be the case
		*  if f1 or f2 are obtuse). To ensure all-positive weights, we check for obtuseness. */
		weight = 0.0;
		if (!triangle_obtuse(v, f1)) {
			weight += ve2 * cotan(f1->GetNextOEdge(e->twin())->GetbVertex(), e->GetaVertex(), e->GetbVertex()) / 8.0;
		}
		else {
			if (angle_obtuse(v, f1)) {
				weight += ve2 * f1->getArea() / 4.0;
			}
			else {
				weight += ve2 * f1->getArea() / 8.0;
			}
		}

		if (!triangle_obtuse(v, f2)) {
			weight += ve2 * cotan (f2->GetNextOEdge(e)->GetbVertex(), e->GetaVertex(), e->GetbVertex()) / 8.0;
		}
		else {
			if (angle_obtuse(v, f2)) {
				weight += ve2 * f1->getArea() / 4.0;
			}
			else {
				weight += ve2 * f1->getArea() / 8.0;
			}
		}

		/* projection of edge perpendicular to N (section 5.3) */
		d[0] = vec_edge[0] - vdotN * N[0];
		d[1] = vec_edge[1] - vdotN * N[1];
		d[2] = vec_edge[2] - vdotN * N[2];
		d.normalize();

		/* not explicit in the paper, but necessary. Move d to 2D basis. */
		d1 = d * basis1;
		d2 = d * basis2;

		/* store off the curvature, direction of edge, and weights for later use */
		weights[edge_count] = weight;
		kappas[edge_count] = kappa;
		d1s[edge_count] = d1;
		d2s[edge_count] = d2;
		edge_count++;

		/* Finally, update the linear equations */
		aterm_da += weight * d1 * d1 * d1 * d1;
		bterm_da += weight * d1 * d1 * 2 * d1 * d2;
		cterm_da += weight * d1 * d1 * d2 * d2;
		const_da += weight * d1 * d1 * (-kappa);

		aterm_db += weight * d1 * d2 * d1 * d1;
		bterm_db += weight * d1 * d2 * 2 * d1 * d2;
		cterm_db += weight * d1 * d2 * d2 * d2;
		const_db += weight * d1 * d2 * (-kappa);
	}

	/* now use the identity (Section 5.3) a + c = |Kh| = 2 * kappa_h */
	aterm_da -= cterm_da;
	const_da += cterm_da * normKh;

	aterm_db -= cterm_db;
	const_db += cterm_db * normKh;

	/* check for solvability of the linear system */
	if (((aterm_da * bterm_db - aterm_db * bterm_da) != 0.0) && ((const_da != 0.0) || (const_db != 0.0))) {
		linsolve(aterm_da, bterm_da, -const_da, aterm_db, bterm_db, -const_db, &a, &b);

		c = normKh - a;

		eigenvector(a, b, c, eig);
	}
	else {
		/* region of v is planar */
		eig[0] = 1.0;
		eig[1] = 0.0;
	}

	/* Although the eigenvectors of B are good estimates of the principal directions, it seems that which one is
	 * attached to which curvature direction is a bit arbitrary. This may be a bug in my implementation, or just
	 * a side-effect of the inaccuracy of B due to the discrete nature of the sampling.
	 *
	 * To overcome this behavior, we'll evaluate which assignment best matches the given eigenvectors by comparing
	 * the curvature estimates computed above and the curvatures calculated from the discrete differential operators.
	 */

	gts_vertex_principal_curvatures(0.5 * normKh, Kg, &K1, &K2);

	err_e1 = err_e2 = 0.0;
	/* loop through the values previously saved */
	for (e = 0; e < edge_count; e++) {
		real weight, kappa, d1, d2;
		real temp1, temp2;
		real delta;

		weight = weights[e];
		kappa = kappas[e];
		d1 = d1s[e];
		d2 = d2s[e];

		temp1 = fabs (eig[0] * d1 + eig[1] * d2);
		temp1 = temp1 * temp1;
		temp2 = fabs (eig[1] * d1 - eig[0] * d2);
		temp2 = temp2 * temp2;

		/* err_e1 is for K1 associated with e1 */
		delta = K1 * temp1 + K2 * temp2 - kappa;
		err_e1 += weight * delta * delta;

		/* err_e2 is for K1 associated with e2 */
		delta = K2 * temp1 + K1 * temp2 - kappa;
		err_e2 += weight * delta * delta;
	}
	free (weights);
	free (kappas);
	free (d1s);
	free (d2s);

	/* rotate eig by a right angle if that would decrease the error */
	if (err_e2 < err_e1) {
		real temp = eig[0];

		eig[0] = eig[1];
		eig[1] = -temp;
	}

	e1[0] = eig[0] * basis1[0] + eig[1] * basis2[0];
	e1[1] = eig[0] * basis1[1] + eig[1] * basis2[1];
	e1[2] = eig[0] * basis1[2] + eig[1] * basis2[2];
	e1.normalize();

	/* make N,e1,e2 a right handed coordinate sytem */
	e2 =  N ^ e1;
	e2.normalize();
}

namespace OGF {

#if 0
inline static real angle(WOEdge *h)
{
	const Vec3r& n1 = h->GetbFace()->GetNormal();
	const Vec3r& n2 = h->GetaFace()->GetNormal();
	const Vec3r v = h->GetVec();
	real sine = (n1 ^ n2) * v / v.norm();
	if (sine >= 1.0) {
		return M_PI / 2.0;
	}
	if (sine <= -1.0) {
		return -M_PI / 2.0;
	}
	return ::asin(sine);
}
#endif

// precondition1: P is inside the sphere
// precondition2: P,V points to the outside of the sphere (i.e. OP.V > 0)
static bool sphere_clip_vector(const Vec3r& O, real r, const Vec3r& P, Vec3r& V)
{
	Vec3r W = P - O;
	real a = V.squareNorm();
	real b = 2.0 * V * W;
	real c = W.squareNorm() - r * r;
	real delta = b * b - 4 * a * c;
	if (delta < 0) {
		// Should not happen, but happens sometimes (numerical precision)
		return true;
	}
	real t = - b + ::sqrt(delta) / (2.0 * a);
	if (t < 0.0) {
		// Should not happen, but happens sometimes (numerical precision)
		return true;
	}
	if (t >= 1.0) {
		// Inside the sphere
		return false;
	}

	V[0] = (t * V.x());
	V[1] = (t * V.y());
	V[2] = (t * V.z());

	return true;
}

// TODO: check optimizations:
// use marking ? (measure *timings* ...)
void compute_curvature_tensor(WVertex *start, real radius, NormalCycle& nc)
{
	// in case we have a non-manifold vertex, skip it...
	if (start->isBoundary())
		return;

	std::set<WVertex*> vertices;
	const Vec3r& O = start->GetVertex();
	std::stack<WVertex*> S;
	S.push(start);
	vertices.insert(start);
	while (!S.empty()) {
		WVertex *v = S.top();
		S.pop();
		if (v->isBoundary())
			continue;
		const Vec3r& P = v->GetVertex();
		WVertex::incoming_edge_iterator woeit = v->incoming_edges_begin();
		WVertex::incoming_edge_iterator woeitend = v->incoming_edges_end();
		for (; woeit != woeitend; ++woeit) {
			WOEdge *h = *woeit;
			if ((v == start) || h->GetVec() * (O - P) > 0.0) {
				Vec3r V(-1 * h->GetVec());
				bool isect = sphere_clip_vector(O, radius, P, V);
				assert (h->GetOwner()->GetNumberOfOEdges() == 2); // Because otherwise v->isBoundary() would be true
				nc.accumulate_dihedral_angle(V, h->GetAngle());

				if (!isect) {
					WVertex *w = h->GetaVertex();
					if (vertices.find(w) == vertices.end()) {
						vertices.insert(w);
						S.push(w);
					}
				}
			}
		}
	}
}

void compute_curvature_tensor_one_ring(WVertex *start, NormalCycle& nc)
{
	// in case we have a non-manifold vertex, skip it...
	if (start->isBoundary())
		return;

	WVertex::incoming_edge_iterator woeit = start->incoming_edges_begin();
	WVertex::incoming_edge_iterator woeitend = start->incoming_edges_end();
	for (; woeit != woeitend; ++woeit) {
		WOEdge *h = (*woeit)->twin();
		nc.accumulate_dihedral_angle(h->GetVec(), h->GetAngle());
		WOEdge *hprev = h->getPrevOnFace();
		nc.accumulate_dihedral_angle(hprev->GetVec(), hprev->GetAngle());
	}
}

}  // OGF namespace

} /* namespace Freestyle */
