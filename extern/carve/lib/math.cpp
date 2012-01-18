// Begin License:
// Copyright (C) 2006-2011 Tobias Sargeant (tobias.sargeant@gmail.com).
// All rights reserved.
//
// This file is part of the Carve CSG Library (http://carve-csg.com/)
//
// This file may be used under the terms of the GNU General Public
// License version 2.0 as published by the Free Software Foundation
// and appearing in the file LICENSE.GPL2 included in the packaging of
// this file.
//
// This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
// INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE.
// End:


#if defined(HAVE_CONFIG_H)
#  include <carve_config.h>
#endif

#include <carve/math.hpp>
#include <carve/matrix.hpp>

#include <iostream>
#include <limits>

#include <stdio.h>

#define M_2PI_3 2.0943951023931953
#define M_SQRT_3_4 0.8660254037844386
#define EPS std::numeric_limits<double>::epsilon()

namespace carve {
  namespace math {

    struct Root {
      double root;
      int multiplicity;

      Root(double r) : root(r), multiplicity(1) {}
      Root(double r, int m) : root(r), multiplicity(m) {}
    };

    void cplx_sqrt(double re, double im,
                   double &re_1, double &im_1,
                   double &re_2, double &im_2) {
      if (re == 0.0 && im == 0.0) {
        re_1 = re_2 = re;
        im_1 = im_2 = im;
      } else {
        double d = sqrt(re * re + im * im);
        re_1 = sqrt((d + re) / 2.0);
        re_2 = re_1;
        im_1 = fabs(sqrt((d - re) / 2.0));
        im_2 = -im_1;
      }
    }

    void cplx_cbrt(double re, double im,
                   double &re_1, double &im_1,
                   double &re_2, double &im_2,
                   double &re_3, double &im_3) {
      if (re == 0.0 && im == 0.0) {
        re_1 = re_2 = re_3 = re;
        im_1 = im_2 = im_3 = im;
      } else {
        double r = cbrt(sqrt(re * re + im * im));
        double t = atan2(im, re) / 3.0;
        re_1 = r * cos(t);
        im_1 = r * sin(t);
        re_2 = r * cos(t + M_TWOPI / 3.0);
        im_2 = r * sin(t + M_TWOPI / 3.0);
        re_3 = r * cos(t + M_TWOPI * 2.0 / 3.0);
        im_3 = r * sin(t + M_TWOPI * 2.0 / 3.0);
      }
    }

    void add_root(std::vector<Root> &roots, double root) {
      for (size_t i = 0; i < roots.size(); ++i) {
        if (roots[i].root == root) {
          roots[i].multiplicity++;
          return;
        }
      }
      roots.push_back(Root(root));
    }

    void linear_roots(double c1, double c0, std::vector<Root> &roots) {
      roots.push_back(Root(c0 / c1));
    }

    void quadratic_roots(double c2, double c1, double c0, std::vector<Root> &roots) {
      if (fabs(c2) < EPS) {
        linear_roots(c1, c0, roots);
        return;
      }

      double p = 0.5 * c1 / c2;
      double dis = p * p - c0 / c2;

      if (dis > 0.0) {
        dis = sqrt(dis);
        if (-p - dis != -p + dis) {
          roots.push_back(Root(-p - dis));
          roots.push_back(Root(-p + dis));
        } else {
          roots.push_back(Root(-p, 2));
        }
      }
    }

    void cubic_roots(double c3, double c2, double c1, double c0, std::vector<Root> &roots) {
      int n_sol = 0;
      double _r[3];

      if (fabs(c3) < EPS) {
        quadratic_roots(c2, c1, c0, roots);
        return;
      }

      if (fabs(c0) < EPS) {
        quadratic_roots(c3, c2, c1, roots);
        add_root(roots, 0.0);
        return;
      }

      double xN = -c2 / (3.0 * c3);
      double yN = c0 + xN * (c1 + xN * (c2 + c3 * xN));

      double delta_sq = (c2 * c2 - 3.0 * c3 * c1) / (9.0 * c3 * c3);
      double h_sq = 4.0 / 9.0 * (c2 * c2 - 3.0 * c3 * c1) * (delta_sq * delta_sq);
      double dis = yN * yN - h_sq;

      if (dis > EPS) {
        // One real root, two complex roots.

        double dis_sqrt = sqrt(dis);
        double r_p = yN - dis_sqrt;
        double r_q = yN + dis_sqrt;
        double p = cbrt(fabs(r_p)/(2.0 * c3));
        double q = cbrt(fabs(r_q)/(2.0 * c3));

        if (r_p > 0.0) p = -p;
        if (r_q > 0.0) q = -q;

        _r[0] = xN + p + q;
        n_sol = 1;

        double re = xN - p * .5 - q * .5;
        double im = p * M_SQRT_3_4 - q * M_SQRT_3_4;

        // root 2: xN + p * exp(M_2PI_3.i) + q * exp(-M_2PI_3.i);
        // root 3: complex conjugate of root 2

        if (im < EPS) {
          _r[1] = _r[2] = re;
          n_sol += 2;
        }
      } else if (dis < -EPS) {
        // Three distinct real roots.
        double theta = acos(-yN / sqrt(h_sq)) / 3.0;
        double delta = sqrt(c2 * c2 - 3.0 * c3 * c1) / (3.0 * c3);

        _r[0] = xN + (2.0 * delta) * cos(theta);
        _r[1] = xN + (2.0 * delta) * cos(M_2PI_3 - theta);
        _r[2] = xN + (2.0 * delta) * cos(M_2PI_3 + theta);
        n_sol = 3;
      } else {
        // Three real roots (two or three equal).
        double r = yN / (2.0 * c3);
        double delta = cbrt(r);

        _r[0] = xN + delta;
        _r[1] = xN + delta;
        _r[2] = xN - 2.0 * delta;
        n_sol = 3;
      }

      for (int i=0; i < n_sol; i++) {
        add_root(roots, _r[i]);
      }
    }

    static void U(const Matrix3 &m,
                  double l,
                  double u[6],
                  double &u_max,
                  int &u_argmax) {
      u[0] = (m._22 - l) * (m._33 - l) - m._23 * m._23;
      u[1] = m._13 * m._23 - m._12 * (m._33 - l);
      u[2] = m._12 * m._23 - m._13 * (m._22 - l);
      u[3] = (m._11 - l) * (m._33 - l) - m._13 * m._13;
      u[4] = m._12 * m._13 - m._23 * (m._11 - l);
      u[5] = (m._11 - l) * (m._22 - l) - m._12 * m._12;

      u_max = -1.0;
      u_argmax = -1;

      for (int i = 0; i < 6; ++i) {
        if (u_max < fabs(u[i])) { u_max = fabs(u[i]); u_argmax = i; }
      }
    }

    static void eig1(const Matrix3 &m, double l, carve::geom::vector<3> &e) {
      double u[6];
      double u_max;
      int u_argmax;

      U(m, l, u, u_max, u_argmax);

      switch(u_argmax) {
      case 0:
        e.x = u[0]; e.y = u[1]; e.z = u[2]; break;
      case 1: case 3:
        e.x = u[1]; e.y = u[3]; e.z = u[4]; break;
      case  2: case 4: case 5:
        e.x = u[2]; e.y = u[4]; e.z = u[5]; break;
      }
      e.normalize();
    }

    static void eig2(const Matrix3 &m, double l, carve::geom::vector<3> &e1, carve::geom::vector<3> &e2) {
      double u[6];
      double u_max;
      int u_argmax;

      U(m, l, u, u_max, u_argmax);

      switch(u_argmax) {
      case 0: case 1:
        e1.x = -m._12; e1.y = m._11; e1.z = 0.0;
        e2.x = -m._13 * m._11; e2.y = -m._13 * m._12; e2.z = m._11 * m._11 + m._12 * m._12;
        break;
      case 2:
        e1.x = m._12; e1.y = 0.0; e1.z = -m._11;
        e2.x = -m._12 * m._11; e2.y = m._11 * m._11 + m._13 * m._13; e2.z = -m._12 * m._13;
        break;
      case 3: case 4:
        e1.x = 0.0; e1.y = -m._23; e1.z = -m._22;
        e2.x = m._22 * m._22 + m._23 * m._23; e2.y = -m._12 * m._22; e2.z = -m._12 * m._23;
        break;
      case 5:
        e1.x = 0.0; e1.y = -m._33; e1.z = m._23;
        e2.x = m._23 * m._23 + m._33 * m._33; e2.y = -m._13 * m._23; e2.z = -m._13 * m._33;
      }
      e1.normalize();
      e2.normalize();
    }

    static void eig3(const Matrix3 &m,
                     double l,
                     carve::geom::vector<3> &e1,
                     carve::geom::vector<3> &e2,
                     carve::geom::vector<3> &e3) {
      e1.x = 1.0; e1.y = 0.0; e1.z = 0.0;
      e2.x = 0.0; e2.y = 1.0; e2.z = 0.0;
      e3.x = 0.0; e3.y = 0.0; e3.z = 1.0;
    }

    void eigSolveSymmetric(const Matrix3 &m,
                           double &l1, carve::geom::vector<3> &e1,
                           double &l2, carve::geom::vector<3> &e2,
                           double &l3, carve::geom::vector<3> &e3) {
      double c0 =
        m._11 * m._22 * m._33 +
        2.0 * m._12 * m._13 * m._23 -
        m._11 * m._23 * m._23 -
        m._22 * m._13 * m._13 -
        m._33 * m._12 * m._12;
      double c1 =
        m._11 * m._22 -
        m._12 * m._12 +
        m._11 * m._33 -
        m._13 * m._13 +
        m._22 * m._33 -
        m._23 * m._23;
      double c2 =
        m._11 +
        m._22 +
        m._33;

      double a = (3.0 * c1 - c2 * c2) / 3.0;
      double b = (-2.0 * c2 * c2 * c2 + 9.0 * c1 * c2 - 27.0 * c0) / 27.0;

      double Q = b * b / 4.0 + a * a * a / 27.0;

      if (fabs(Q) < 1e-16) {
        l1 = m._11;   e1.x = 1.0; e1.y = 0.0; e1.z = 0.0;
        l2 = m._22;   e2.x = 0.0; e2.y = 1.0; e2.z = 0.0;
        l3 = m._33;   e3.x = 0.0; e3.y = 0.0; e3.z = 1.0;
      } else if (Q > 0) {
        l1 = l2 = c2 / 3.0 + cbrt(b / 2.0);
        l3 = c2 / 3.0 - 2.0 * cbrt(b / 2.0);

        eig2(m, l1, e1, e2);
        eig1(m, l3, e3);
      } else if (Q < 0) {
        double t = atan2(sqrt(-Q), -b / 2.0);
        double cos_t3 = cos(t / 3.0);
        double sin_t3 = sin(t / 3.0);
        double r = cbrt(sqrt(b * b / 4.0 - Q));

        l1 = c2 / 3.0 + 2 * r * cos_t3;
        l2 = c2 / 3.0 - r * (cos_t3 + M_SQRT_3 * sin_t3);
        l3 = c2 / 3.0 - r * (cos_t3 - M_SQRT_3 * sin_t3);

        eig1(m, l1, e1);
        eig1(m, l2, e2);
        eig1(m, l3, e3);
      }
    }

    void eigSolve(const Matrix3 &m, double &l1, double &l2, double &l3) {
      double c3, c2, c1, c0;
      std::vector<Root> roots;

      c3 = -1.0;
      c2 = m._11 + m._22 + m._33;
      c1 = 
        -(m._22 * m._33 + m._11 * m._22 + m._11 * m._33)
        +(m._23 * m._32 + m._13 * m._31 + m._12 * m._21);
      c0 =
        +(m._11 * m._22 - m._12 * m._21) * m._33
        -(m._11 * m._23 - m._13 * m._21) * m._32
        +(m._12 * m._23 - m._13 * m._22) * m._31;

      cubic_roots(c3, c2, c1, c0, roots);

      for (size_t i = 0; i < roots.size(); i++) {
        Matrix3 M(m);
        M._11 -= roots[i].root;
        M._22 -= roots[i].root;
        M._33 -= roots[i].root;
        // solve M.v = 0
      }

      std::cerr << "n_roots=" << roots.size() << std::endl;
      for (size_t i = 0; i < roots.size(); i++) {
        fprintf(stderr, "  %.24f(%d)", roots[i].root, roots[i].multiplicity);
      }
      std::cerr << std::endl;
    }

  }
}

