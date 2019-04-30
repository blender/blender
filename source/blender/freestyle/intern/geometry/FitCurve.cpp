/*
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
 */

/** \file
 * \ingroup freestyle
 * \brief An Algorithm for Automatically Fitting Digitized Curves by Philip J. Schneider,
 * \brief from "Graphics Gems", Academic Press, 1990
 */

#include <cstdlib>  // for malloc and free
#include <stdio.h>
#include <math.h>

#include "FitCurve.h"

using namespace std;

namespace Freestyle {

typedef Vector2 *BezierCurve;

/* Forward declarations */
static double *Reparameterize(Vector2 *d, int first, int last, double *u, BezierCurve bezCurve);
static double NewtonRaphsonRootFind(BezierCurve Q, Vector2 P, double u);
static Vector2 BezierII(int degree, Vector2 *V, double t);
static double B0(double u);
static double B1(double u);
static double B2(double u);
static double B3(double u);
static Vector2 ComputeLeftTangent(Vector2 *d, int end);
static double ComputeMaxError(
    Vector2 *d, int first, int last, BezierCurve bezCurve, double *u, int *splitPoint);
static double *ChordLengthParameterize(Vector2 *d, int first, int last);
static BezierCurve GenerateBezier(
    Vector2 *d, int first, int last, double *uPrime, Vector2 tHat1, Vector2 tHat2);
static Vector2 V2AddII(Vector2 a, Vector2 b);
static Vector2 V2ScaleIII(Vector2 v, double s);
static Vector2 V2SubII(Vector2 a, Vector2 b);

/* returns squared length of input vector */
static double V2SquaredLength(Vector2 *a)
{
  return (((*a)[0] * (*a)[0]) + ((*a)[1] * (*a)[1]));
}

/* returns length of input vector */
static double V2Length(Vector2 *a)
{
  return (sqrt(V2SquaredLength(a)));
}

static Vector2 *V2Scale(Vector2 *v, double newlen)
{
  double len = V2Length(v);
  if (len != 0.0) {
    (*v)[0] *= newlen / len;
    (*v)[1] *= newlen / len;
  }
  return v;
}

/* return the dot product of vectors a and b */
static double V2Dot(Vector2 *a, Vector2 *b)
{
  return (((*a)[0] * (*b)[0]) + ((*a)[1] * (*b)[1]));
}

/* return the distance between two points */
static double V2DistanceBetween2Points(Vector2 *a, Vector2 *b)
{
  double dx = (*a)[0] - (*b)[0];
  double dy = (*a)[1] - (*b)[1];
  return (sqrt((dx * dx) + (dy * dy)));
}

/* return vector sum c = a+b */
static Vector2 *V2Add(Vector2 *a, Vector2 *b, Vector2 *c)
{
  (*c)[0] = (*a)[0] + (*b)[0];
  (*c)[1] = (*a)[1] + (*b)[1];
  return c;
}

/* normalizes the input vector and returns it */
static Vector2 *V2Normalize(Vector2 *v)
{
  double len = V2Length(v);
  if (len != 0.0) {
    (*v)[0] /= len;
    (*v)[1] /= len;
  }
  return v;
}

/* negates the input vector and returns it */
static Vector2 *V2Negate(Vector2 *v)
{
  (*v)[0] = -(*v)[0];
  (*v)[1] = -(*v)[1];
  return v;
}

/*  GenerateBezier:
 *  Use least-squares method to find Bezier control points for region.
 *    Vector2 *d;           Array of digitized points
 *    int     first, last;  Indices defining region
 *    double  *uPrime;      Parameter values for region
 *    Vector2 tHat1, tHat2; Unit tangents at endpoints
 */
static BezierCurve GenerateBezier(
    Vector2 *d, int first, int last, double *uPrime, Vector2 tHat1, Vector2 tHat2)
{
  int i;
  Vector2 A[2];     /* rhs for eqn */
  int nPts;         /* Number of pts in sub-curve */
  double C[2][2];   /* Matrix C */
  double X[2];      /* Matrix X */
  double det_C0_C1; /* Determinants of matrices */
  double det_C0_X;
  double det_X_C1;
  double alpha_l; /* Alpha values, left and right */
  double alpha_r;
  Vector2 tmp;          /* Utility variable */
  BezierCurve bezCurve; /* RETURN bezier curve ctl pts */

  bezCurve = (Vector2 *)malloc(4 * sizeof(Vector2));
  nPts = last - first + 1;

  /* Create the C and X matrices */
  C[0][0] = 0.0;
  C[0][1] = 0.0;
  C[1][0] = 0.0;
  C[1][1] = 0.0;
  X[0] = 0.0;
  X[1] = 0.0;
  for (i = 0; i < nPts; i++) {
    /* Compute the A's */
    A[0] = tHat1;
    A[1] = tHat2;
    V2Scale(&A[0], B1(uPrime[i]));
    V2Scale(&A[1], B2(uPrime[i]));

    C[0][0] += V2Dot(&A[0], &A[0]);
    C[0][1] += V2Dot(&A[0], &A[1]);
    //      C[1][0] += V2Dot(&A[0], &A[1]);
    C[1][0] = C[0][1];
    C[1][1] += V2Dot(&A[1], &A[1]);

    tmp = V2SubII(d[first + i],
                  V2AddII(V2ScaleIII(d[first], B0(uPrime[i])),
                          V2AddII(V2ScaleIII(d[first], B1(uPrime[i])),
                                  V2AddII(V2ScaleIII(d[last], B2(uPrime[i])),
                                          V2ScaleIII(d[last], B3(uPrime[i]))))));

    X[0] += V2Dot(&A[0], &tmp);
    X[1] += V2Dot(&A[1], &tmp);
  }

  /* Compute the determinants of C and X */
  det_C0_C1 = C[0][0] * C[1][1] - C[1][0] * C[0][1];
  det_C0_X = C[0][0] * X[1] - C[0][1] * X[0];
  det_X_C1 = X[0] * C[1][1] - X[1] * C[0][1];

  /* Finally, derive alpha values */
  if (det_C0_C1 == 0.0) {
    det_C0_C1 = (C[0][0] * C[1][1]) * 10.0e-12;
  }
  alpha_l = det_X_C1 / det_C0_C1;
  alpha_r = det_C0_X / det_C0_C1;

  /* If alpha negative, use the Wu/Barsky heuristic (see text) (if alpha is 0, you get coincident
   * control points that lead to divide by zero in any subsequent NewtonRaphsonRootFind() call).
   */
  if (alpha_l < 1.0e-6 || alpha_r < 1.0e-6) {
    double dist = V2DistanceBetween2Points(&d[last], &d[first]) / 3.0;

    bezCurve[0] = d[first];
    bezCurve[3] = d[last];
    V2Add(&(bezCurve[0]), V2Scale(&(tHat1), dist), &(bezCurve[1]));
    V2Add(&(bezCurve[3]), V2Scale(&(tHat2), dist), &(bezCurve[2]));
    return bezCurve;
  }

  /* First and last control points of the Bezier curve are positioned exactly at the first and last
   * data points Control points 1 and 2 are positioned an alpha distance out on the tangent
   * vectors, left and right, respectively
   */
  bezCurve[0] = d[first];
  bezCurve[3] = d[last];
  V2Add(&bezCurve[0], V2Scale(&tHat1, alpha_l), &bezCurve[1]);
  V2Add(&bezCurve[3], V2Scale(&tHat2, alpha_r), &bezCurve[2]);
  return (bezCurve);
}

/*
 *  Reparameterize:
 *  Given set of points and their parameterization, try to find a better parameterization.
 *    Vector2     *d;           Array of digitized points
 *    int         first, last;  Indices defining region
 *    double      *u;           Current parameter values
 *    BezierCurve bezCurve;     Current fitted curve
 */
static double *Reparameterize(Vector2 *d, int first, int last, double *u, BezierCurve bezCurve)
{
  int nPts = last - first + 1;
  int i;
  double *uPrime; /* New parameter values */

  uPrime = (double *)malloc(nPts * sizeof(double));
  for (i = first; i <= last; i++) {
    uPrime[i - first] = NewtonRaphsonRootFind(bezCurve, d[i], u[i - first]);
  }
  return (uPrime);
}

/*
 *  NewtonRaphsonRootFind:
 *  Use Newton-Raphson iteration to find better root.
 *    BezierCurve Q;  Current fitted curve
 *    Vector2     P;  Digitized point
 *    double      u;  Parameter value for "P"
 */
static double NewtonRaphsonRootFind(BezierCurve Q, Vector2 P, double u)
{
  double numerator, denominator;
  Vector2 Q1[3], Q2[2];    /* Q' and Q'' */
  Vector2 Q_u, Q1_u, Q2_u; /* u evaluated at Q, Q', & Q'' */
  double uPrime;           /* Improved u */
  int i;

  /* Compute Q(u) */
  Q_u = BezierII(3, Q, u);

  /* Generate control vertices for Q' */
  for (i = 0; i <= 2; i++) {
    Q1[i][0] = (Q[i + 1][0] - Q[i][0]) * 3.0;
    Q1[i][1] = (Q[i + 1][1] - Q[i][1]) * 3.0;
  }

  /* Generate control vertices for Q'' */
  for (i = 0; i <= 1; i++) {
    Q2[i][0] = (Q1[i + 1][0] - Q1[i][0]) * 2.0;
    Q2[i][1] = (Q1[i + 1][1] - Q1[i][1]) * 2.0;
  }

  /* Compute Q'(u) and Q''(u) */
  Q1_u = BezierII(2, Q1, u);
  Q2_u = BezierII(1, Q2, u);

  /* Compute f(u)/f'(u) */
  numerator = (Q_u[0] - P[0]) * (Q1_u[0]) + (Q_u[1] - P[1]) * (Q1_u[1]);
  denominator = (Q1_u[0]) * (Q1_u[0]) + (Q1_u[1]) * (Q1_u[1]) + (Q_u[0] - P[0]) * (Q2_u[0]) +
                (Q_u[1] - P[1]) * (Q2_u[1]);

  /* u = u - f(u)/f'(u) */
  if (denominator == 0)  // FIXME
    return u;
  uPrime = u - (numerator / denominator);
  return uPrime;
}

/*
 *  Bezier:
 *  Evaluate a Bezier curve at a particular parameter value
 *    int     degree;  The degree of the bezier curve
 *    Vector2 *V;      Array of control points
 *    double  t;       Parametric value to find point for
 */
static Vector2 BezierII(int degree, Vector2 *V, double t)
{
  int i, j;
  Vector2 Q;      /* Point on curve at parameter t */
  Vector2 *Vtemp; /* Local copy of control points */

  /* Copy array */
  Vtemp = (Vector2 *)malloc((unsigned)((degree + 1) * sizeof(Vector2)));
  for (i = 0; i <= degree; i++) {
    Vtemp[i] = V[i];
  }

  /* Triangle computation */
  for (i = 1; i <= degree; i++) {
    for (j = 0; j <= degree - i; j++) {
      Vtemp[j][0] = (1.0 - t) * Vtemp[j][0] + t * Vtemp[j + 1][0];
      Vtemp[j][1] = (1.0 - t) * Vtemp[j][1] + t * Vtemp[j + 1][1];
    }
  }

  Q = Vtemp[0];
  free((void *)Vtemp);
  return Q;
}

/*
 *  B0, B1, B2, B3:
 *  Bezier multipliers
 */
static double B0(double u)
{
  double tmp = 1.0 - u;
  return (tmp * tmp * tmp);
}

static double B1(double u)
{
  double tmp = 1.0 - u;
  return (3 * u * (tmp * tmp));
}

static double B2(double u)
{
  double tmp = 1.0 - u;
  return (3 * u * u * tmp);
}

static double B3(double u)
{
  return (u * u * u);
}

/*
 * ComputeLeftTangent, ComputeRightTangent, ComputeCenterTangent:
 * Approximate unit tangents at endpoints and "center" of digitized curve
 */
/*    Vector2 *d;   Digitized points
 *    int     end;  Index to "left" end of region
 */
static Vector2 ComputeLeftTangent(Vector2 *d, int end)
{
  Vector2 tHat1;
  tHat1 = V2SubII(d[end + 1], d[end]);
  tHat1 = *V2Normalize(&tHat1);
  return tHat1;
}

/*    Vector2 *d;   Digitized points
 *    int     end;  Index to "right" end of region
 */
static Vector2 ComputeRightTangent(Vector2 *d, int end)
{
  Vector2 tHat2;
  tHat2 = V2SubII(d[end - 1], d[end]);
  tHat2 = *V2Normalize(&tHat2);
  return tHat2;
}

/*    Vector2 *d;   Digitized points
 *    int     end;  Index to point inside region
 */
static Vector2 ComputeCenterTangent(Vector2 *d, int center)
{
  Vector2 V1, V2, tHatCenter;

  V1 = V2SubII(d[center - 1], d[center]);
  V2 = V2SubII(d[center], d[center + 1]);
  tHatCenter[0] = (V1[0] + V2[0]) / 2.0;
  tHatCenter[1] = (V1[1] + V2[1]) / 2.0;
  tHatCenter = *V2Normalize(&tHatCenter);

  /* avoid numerical singularity in the special case when V1 == -V2 */
  if (V2Length(&tHatCenter) < M_EPSILON) {
    tHatCenter = *V2Normalize(&V1);
  }

  return tHatCenter;
}

/*
 *  ChordLengthParameterize:
 *  Assign parameter values to digitized points using relative distances between points.
 *    Vector2 *d;           Array of digitized points
 *    int     first, last;  Indices defining region
 */
static double *ChordLengthParameterize(Vector2 *d, int first, int last)
{
  int i;
  double *u; /* Parameterization */

  u = (double *)malloc((unsigned)(last - first + 1) * sizeof(double));

  u[0] = 0.0;
  for (i = first + 1; i <= last; i++) {
    u[i - first] = u[i - first - 1] + V2DistanceBetween2Points(&d[i], &d[i - 1]);
  }

  for (i = first + 1; i <= last; i++) {
    u[i - first] = u[i - first] / u[last - first];
  }

  return u;
}

/*
 *  ComputeMaxError :
 *  Find the maximum squared distance of digitized points to fitted curve.
 *    Vector2     *d;           Array of digitized points
 *    int         first, last;  Indices defining region
 *    BezierCurve bezCurve;     Fitted Bezier curve
 *    double      *u;           Parameterization of points
 *    int         *splitPoint;  Point of maximum error
 */
static double ComputeMaxError(
    Vector2 *d, int first, int last, BezierCurve bezCurve, double *u, int *splitPoint)
{
  int i;
  double maxDist; /* Maximum error */
  double dist;    /* Current error */
  Vector2 P;      /* Point on curve */
  Vector2 v;      /* Vector from point to curve */

  *splitPoint = (last - first + 1) / 2;
  maxDist = 0.0;
  for (i = first + 1; i < last; i++) {
    P = BezierII(3, bezCurve, u[i - first]);
    v = V2SubII(P, d[i]);
    dist = V2SquaredLength(&v);
    if (dist >= maxDist) {
      maxDist = dist;
      *splitPoint = i;
    }
  }
  return maxDist;
}

static Vector2 V2AddII(Vector2 a, Vector2 b)
{
  Vector2 c;
  c[0] = a[0] + b[0];
  c[1] = a[1] + b[1];
  return c;
}

static Vector2 V2ScaleIII(Vector2 v, double s)
{
  Vector2 result;
  result[0] = v[0] * s;
  result[1] = v[1] * s;
  return result;
}

static Vector2 V2SubII(Vector2 a, Vector2 b)
{
  Vector2 c;
  c[0] = a[0] - b[0];
  c[1] = a[1] - b[1];
  return c;
}

//------------------------- WRAPPER -----------------------------//

FitCurveWrapper::FitCurveWrapper()
{
}

FitCurveWrapper::~FitCurveWrapper()
{
  _vertices.clear();
}

void FitCurveWrapper::DrawBezierCurve(int n, Vector2 *curve)
{
  for (int i = 0; i <= n; ++i)
    _vertices.push_back(curve[i]);
}

void FitCurveWrapper::FitCurve(vector<Vec2d> &data, vector<Vec2d> &oCurve, double error)
{
  int size = data.size();
  Vector2 *d = new Vector2[size];
  for (int i = 0; i < size; ++i) {
    d[i][0] = data[i][0];
    d[i][1] = data[i][1];
  }

  FitCurve(d, size, error);

  delete[] d;

  // copy results
  for (vector<Vector2>::iterator v = _vertices.begin(), vend = _vertices.end(); v != vend; ++v) {
    oCurve.push_back(Vec2d(v->x(), v->y()));
  }
}

void FitCurveWrapper::FitCurve(Vector2 *d, int nPts, double error)
{
  Vector2 tHat1, tHat2; /* Unit tangent vectors at endpoints */

  tHat1 = ComputeLeftTangent(d, 0);
  tHat2 = ComputeRightTangent(d, nPts - 1);
  FitCubic(d, 0, nPts - 1, tHat1, tHat2, error);
}

void FitCurveWrapper::FitCubic(
    Vector2 *d, int first, int last, Vector2 tHat1, Vector2 tHat2, double error)
{
  BezierCurve bezCurve;  /* Control points of fitted Bezier curve */
  double *u;             /* Parameter values for point */
  double *uPrime;        /* Improved parameter values */
  double maxError;       /* Maximum fitting error */
  int splitPoint;        /* Point to split point set at */
  int nPts;              /* Number of points in subset */
  double iterationError; /* Error below which you try iterating */
  int maxIterations = 4; /* Max times to try iterating */
  Vector2 tHatCenter;    /* Unit tangent vector at splitPoint */
  int i;

  iterationError = error * error;
  nPts = last - first + 1;

  /* Use heuristic if region only has two points in it */
  if (nPts == 2) {
    double dist = V2DistanceBetween2Points(&d[last], &d[first]) / 3.0;

    bezCurve = (Vector2 *)malloc(4 * sizeof(Vector2));
    bezCurve[0] = d[first];
    bezCurve[3] = d[last];
    V2Add(&bezCurve[0], V2Scale(&tHat1, dist), &bezCurve[1]);
    V2Add(&bezCurve[3], V2Scale(&tHat2, dist), &bezCurve[2]);
    DrawBezierCurve(3, bezCurve);
    free((void *)bezCurve);
    return;
  }

  /* Parameterize points, and attempt to fit curve */
  u = ChordLengthParameterize(d, first, last);
  bezCurve = GenerateBezier(d, first, last, u, tHat1, tHat2);

  /* Find max deviation of points to fitted curve */
  maxError = ComputeMaxError(d, first, last, bezCurve, u, &splitPoint);
  if (maxError < error) {
    DrawBezierCurve(3, bezCurve);
    free((void *)u);
    free((void *)bezCurve);
    return;
  }

  /* If error not too large, try some reparameterization and iteration */
  if (maxError < iterationError) {
    for (i = 0; i < maxIterations; i++) {
      uPrime = Reparameterize(d, first, last, u, bezCurve);

      free((void *)u);
      free((void *)bezCurve);
      u = uPrime;

      bezCurve = GenerateBezier(d, first, last, u, tHat1, tHat2);
      maxError = ComputeMaxError(d, first, last, bezCurve, u, &splitPoint);

      if (maxError < error) {
        DrawBezierCurve(3, bezCurve);
        free((void *)u);
        free((void *)bezCurve);
        return;
      }
    }
  }

  /* Fitting failed -- split at max error point and fit recursively */
  free((void *)u);
  free((void *)bezCurve);
  tHatCenter = ComputeCenterTangent(d, splitPoint);
  FitCubic(d, first, splitPoint, tHat1, tHatCenter, error);
  V2Negate(&tHatCenter);
  FitCubic(d, splitPoint, last, tHatCenter, tHat2, error);
}

} /* namespace Freestyle */
