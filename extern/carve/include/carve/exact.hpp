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


#pragma once

#include <carve/carve.hpp>

#include <vector>
#include <numeric>
#include <algorithm>


namespace carve {
  namespace exact {

    class exact_t : public std::vector<double> {
      typedef std::vector<double> super;

    public:
      exact_t() : super() {
      }

      exact_t(double v, size_t sz = 1) : super(sz, v) {
      }

      template<typename iter_t>
      exact_t(iter_t a, iter_t b) : super(a, b) {
      }

      exact_t(double a, double b) : super() {
        reserve(2);
        push_back(a);
        push_back(b);
      }

      exact_t(double a, double b, double c) : super() {
        reserve(3);
        push_back(a);
        push_back(b);
        push_back(c);
      }

      exact_t(double a, double b, double c, double d) : super() {
        reserve(4);
        push_back(a);
        push_back(b);
        push_back(c);
        push_back(d);
      }

      exact_t(double a, double b, double c, double d, double e) : super() {
        reserve(5);
        push_back(a);
        push_back(b);
        push_back(c);
        push_back(d);
        push_back(e);
      }

      exact_t(double a, double b, double c, double d, double e, double f) : super() {
        reserve(6);
        push_back(a);
        push_back(b);
        push_back(c);
        push_back(d);
        push_back(e);
        push_back(f);
      }

      exact_t(double a, double b, double c, double d, double e, double f, double g) : super() {
        reserve(7);
        push_back(a);
        push_back(b);
        push_back(c);
        push_back(d);
        push_back(e);
        push_back(f);
        push_back(g);
      }

      exact_t(double a, double b, double c, double d, double e, double f, double g, double h) : super() {
        reserve(8);
        push_back(a);
        push_back(b);
        push_back(c);
        push_back(d);
        push_back(e);
        push_back(f);
        push_back(g);
        push_back(h);
      }

      void compress();

      exact_t compressed() const {
        exact_t result(*this);
        result.compress();
        return result;
      }

      operator double() const {
        return std::accumulate(begin(), end(), 0.0);
      }

      void removeZeroes() {
        erase(std::remove(begin(), end(), 0.0), end());
      }
    };

    inline std::ostream &operator<<(std::ostream &out, const exact_t &p) {
      out << '{';
      out << p[0];
      for (size_t i = 1; i < p.size(); ++i) out << ';' << p[i];
      out << '}';
      return out;
    }



    namespace detail {
      const struct constants_t {
        double splitter;     /* = 2^ceiling(p / 2) + 1.  Used to split floats in half. */
        double epsilon;                /* = 2^(-p).  Used to estimate roundoff errors. */
        /* A set of coefficients used to calculate maximum roundoff errors.          */
        double resulterrbound;
        double ccwerrboundA, ccwerrboundB, ccwerrboundC;
        double o3derrboundA, o3derrboundB, o3derrboundC;
        double iccerrboundA, iccerrboundB, iccerrboundC;
        double isperrboundA, isperrboundB, isperrboundC;

        constants_t() {
          double half;
          double check, lastcheck;
          int every_other;
  
          every_other = 1;
          half = 0.5;
          epsilon = 1.0;
          splitter = 1.0;
          check = 1.0;
          /* Repeatedly divide `epsilon' by two until it is too small to add to    */
          /*   one without causing roundoff.  (Also check if the sum is equal to   */
          /*   the previous sum, for machines that round up instead of using exact */
          /*   rounding.  Not that this library will work on such machines anyway. */
          do {
            lastcheck = check;
            epsilon *= half;
            if (every_other) {
              splitter *= 2.0;
            }
            every_other = !every_other;
            check = 1.0 + epsilon;
          } while ((check != 1.0) && (check != lastcheck));
          splitter += 1.0;
  
          /* Error bounds for orientation and incircle tests. */
          resulterrbound = (3.0 + 8.0 * epsilon) * epsilon;
          ccwerrboundA = (3.0 + 16.0 * epsilon) * epsilon;
          ccwerrboundB = (2.0 + 12.0 * epsilon) * epsilon;
          ccwerrboundC = (9.0 + 64.0 * epsilon) * epsilon * epsilon;
          o3derrboundA = (7.0 + 56.0 * epsilon) * epsilon;
          o3derrboundB = (3.0 + 28.0 * epsilon) * epsilon;
          o3derrboundC = (26.0 + 288.0 * epsilon) * epsilon * epsilon;
          iccerrboundA = (10.0 + 96.0 * epsilon) * epsilon;
          iccerrboundB = (4.0 + 48.0 * epsilon) * epsilon;
          iccerrboundC = (44.0 + 576.0 * epsilon) * epsilon * epsilon;
          isperrboundA = (16.0 + 224.0 * epsilon) * epsilon;
          isperrboundB = (5.0 + 72.0 * epsilon) * epsilon;
          isperrboundC = (71.0 + 1408.0 * epsilon) * epsilon * epsilon;
        }
      } constants;

      template<unsigned U, unsigned V>
      struct op {
        enum {
          Vlo = V / 2,
          Vhi = V - Vlo
        };

        static inline void add(const double *a, const double *b, double *r) {
          double t[U + Vlo];
          op<U, Vlo>::add(a, b, t);
          for (size_t i = 0; i < Vlo; ++i) r[i] = t[i];
          op<U, Vhi>::add(t + Vlo, b + Vlo, r + Vlo);
        }

        static inline void sub(const double *a, const double *b, double *r) {
          double t[U + Vlo];
          op<U, Vlo>::sub(a, b, t);
          for (size_t i = 0; i < Vlo; ++i) r[i] = t[i];
          op<U, Vhi>::sub(t + Vlo, b + Vlo, r + Vlo);
        }
      };

      template<unsigned U>
      struct op<U, 1> {
        enum {
          Ulo = U / 2,
          Uhi = U - Ulo
        };
        static void add(const double *a, const double *b, double *r) {
          double t[Ulo + 1];
          op<Ulo, 1>::add(a, b, t);
          for (size_t i = 0; i < Ulo; ++i) r[i] = t[i];
          op<Uhi, 1>::add(a + Ulo, t + Ulo, r + Ulo);
        }

        static void sub(const double *a, const double *b, double *r) {
          double t[Ulo + 1];
          op<Ulo, 1>::sub(a, b, t);
          for (size_t i = 0; i < Ulo; ++i) r[i] = t[i];
          op<Uhi, 1>::add(a + Ulo, t + Ulo, r + Ulo);
        }
      };

      template<>
      struct op<1, 1> {
        static void add_fast(const double *a, const double *b, double *r) {
          assert(fabs(a[0]) >= fabs(b[0]));
          volatile double sum = a[0] + b[0];
          volatile double bvirt = sum - a[0];
          r[0] = b[0] - bvirt;
          r[1] = sum;
        }

        static void sub_fast(const double *a, const double *b, double *r) {
          assert(fabs(a[0]) >= fabs(b[0]));
          volatile double diff = a[0] - b[0];
          volatile double bvirt = a[0] - diff;
          r[0] = bvirt - b[0];
          r[1] = diff;
        }

        static void add(const double *a, const double *b, double *r) {
          volatile double sum = a[0] + b[0];
          volatile double bvirt = sum - a[0];
          double avirt = sum - bvirt;
          double bround = b[0] - bvirt;
          double around = a[0] - avirt;
          r[0] = around + bround;
          r[1] = sum;
        }

        static void sub(const double *a, const double *b, double *r) {
          volatile double diff = a[0] - b[0];
          volatile double bvirt = a[0] - diff;
          double avirt = diff + bvirt;
          double bround = bvirt - b[0];
          double around = a[0] - avirt;
          r[0] = around + bround;
          r[1] = diff;
        }
      };


      template<unsigned U, unsigned V>
      static exact_t add(const double *a, const double *b) {
        exact_t result;
        result.resize(U + V);
        op<U,V>::add(a, b, &result[0]);
        return result;
      }


      template<unsigned U, unsigned V>
      static exact_t sub(const double *a, const double *b) {
        exact_t result;
        result.resize(U + V);
        op<U,V>::sub(a, b, &result[0]);
        return result;
      }


      template<unsigned U, unsigned V>
      static exact_t add(const exact_t &a, const exact_t &b) {
        assert(a.size() == U);
        assert(b.size() == V);
        exact_t result;
        result.resize(U + V);
        std::fill(result.begin(), result.end(), std::numeric_limits<double>::quiet_NaN());
        op<U,V>::add(&a[0], &b[0], &result[0]);
        return result;
      }


      template<unsigned U, unsigned V>
      static exact_t add(const exact_t &a, const double *b) {
        assert(a.size() == U);
        exact_t result;
        result.resize(U + V);
        std::fill(result.begin(), result.end(), std::numeric_limits<double>::quiet_NaN());
        op<U,V>::add(&a[0], b, &result[0]);
        return result;
      }


      template<unsigned U, unsigned V>
      static exact_t sub(const exact_t &a, const exact_t &b) {
        assert(a.size() == U);
        assert(b.size() == V);
        exact_t result;
        result.resize(U + V);
        std::fill(result.begin(), result.end(), std::numeric_limits<double>::quiet_NaN());
        op<U,V>::sub(&a[0], &b[0], &result[0]);
        return result;
      }


      template<unsigned U, unsigned V>
      static exact_t sub(const exact_t &a, const double *b) {
        assert(a.size() == U);
        exact_t result;
        result.resize(U + V);
        std::fill(result.begin(), result.end(), std::numeric_limits<double>::quiet_NaN());
        op<U,V>::sub(&a[0], &b[0], &result[0]);
        return result;
      }


      static inline void split(const double a, double *r) {
        volatile double c = constants.splitter * a;
        volatile double abig = c - a;
        r[1] = c - abig;
        r[0] = a - r[1];
      }

      static inline void prod_1_1(const double *a, const double *b, double *r) {
        r[1] = a[0] * b[0];
        double a_sp[2]; split(a[0], a_sp);
        double b_sp[2]; split(b[0], b_sp);
        double err1 = r[1] - a_sp[1] * b_sp[1];
        double err2 = err1 - a_sp[0] * b_sp[1];
        double err3 = err2 - a_sp[1] * b_sp[0];
        r[0] = a_sp[0] * b_sp[0] - err3;
      }

      static inline void prod_1_1s(const double *a, const double *b, const double *b_sp, double *r) {
        r[1] = a[0] * b[0];
        double a_sp[2]; split(a[0], a_sp);
        double err1 = r[1] - a_sp[1] * b_sp[1];
        double err2 = err1 - a_sp[0] * b_sp[1];
        double err3 = err2 - a_sp[1] * b_sp[0];
        r[0] = a_sp[0] * b_sp[0] - err3;
      }

      static inline void prod_1s_1s(const double *a, const double *a_sp, const double *b, const double *b_sp, double *r) {
        r[1] = a[0] * b[0];
        double err1 = r[1] - a_sp[1] * b_sp[1];
        double err2 = err1 - a_sp[0] * b_sp[1];
        double err3 = err2 - a_sp[1] * b_sp[0];
        r[0] = a_sp[0] * b_sp[0] - err3;
      }

      static inline void prod_2_1(const double *a, const double *b, double *r) {
        double b_sp[2]; split(b[0], b_sp);
        double t1[2]; prod_1_1s(a+0, b, b_sp, t1);
        r[0] = t1[0];
        double t2[2]; prod_1_1s(a+1, b, b_sp, t2);
        double t3[2]; op<1,1>::add(t1+1, t2, t3);
        r[1] = t3[0];
        double t4[2]; op<1,1>::add_fast(t2+1, t3+1, r + 2);
      }

      static inline void prod_1_2(const double *a, const double *b, double *r) {
        prod_2_1(b, a, r);
      }

      static inline void prod_4_1(const double *a, const double *b, double *r) {
        double b_sp[2]; split(b[0], b_sp);
        double t1[2]; prod_1_1s(a+0, b, b_sp, t1);
        r[0] = t1[0];
        double t2[2]; prod_1_1s(a+1, b, b_sp, t2);
        double t3[2]; op<1,1>::add(t1+1, t2, t3);
        r[1] = t3[0];
        double t4[2]; op<1,1>::add_fast(t2+1, t3+1, t4);
        r[2] = t4[0];
        double t5[2]; prod_1_1s(a+2, b, b_sp, t5);
        double t6[2]; op<1,1>::add(t4+1, t5, t6);
        r[3] = t6[0];
        double t7[2]; op<1,1>::add_fast(t5+1, t6+1, t7);
        r[4] = t7[0];
        double t8[2]; prod_1_1s(a+3, b, b_sp, t8);
        double t9[2]; op<1,1>::add(t7+1, t8, t9);
        r[5] = t9[0];
        op<1,1>::add_fast(t8+1, t9+1, r + 6);
      }

      static inline void prod_1_4(const double *a, const double *b, double *r) {
        prod_4_1(b, a, r);
      }

      static inline void prod_2_2(const double *a, const double *b, double *r) {
        double a1_sp[2]; split(a[1], a1_sp);
        double a0_sp[2]; split(a[0], a0_sp);
        double b1_sp[2]; split(b[1], b1_sp);
        double b0_sp[2]; split(b[0], b0_sp);

        double t1[2]; prod_1s_1s(a+0, a0_sp, b+0, b0_sp, t1);
        r[0] = t1[0];
        double t2[2]; prod_1s_1s(a+1, a1_sp, b+0, b0_sp, t2);

        double t3[2]; op<1,1>::add(t1+1, t2, t3);
        double t4[2]; op<1,1>::add_fast(t2+1, t3+1, t4);

        double t5[2]; prod_1s_1s(a+0, a0_sp, b+1, b1_sp, t5);

        double t6[2]; op<1,1>::add(t3, t5, t6);
        r[1] = t6[0];
        double t7[2]; op<1,1>::add(t4, t6+1, t7);
        double t8[2]; op<1,1>::add(t4+1, t7+1, t8);

        double t9[2]; prod_1s_1s(a+1, a1_sp, b+1, b1_sp, t9);

        double t10[2]; op<1,1>::add(t5+1, t9, t10);
        double t11[2]; op<1,1>::add(t7, t10, t11);
        r[2] = t11[0];
        double t12[2]; op<1,1>::add(t8, t11+1, t12);
        double t13[2]; op<1,1>::add(t8+1, t12+1, t13);
        double t14[2]; op<1,1>::add(t9+1, t10+1, t14);
        double t15[2]; op<1,1>::add(t12, t14, t15);
        r[3] = t15[0];
        double t16[2]; op<1,1>::add(t13, t15+1, t16);
        double t17[2]; op<1,1>::add(t13+1, t16+1, t17);
        double t18[2]; op<1,1>::add(t16, t14+1, t18);
        r[4] = t18[0];
        double t19[2]; op<1,1>::add(t17, t18+1, t19);
        r[5] = t19[0];
        double t20[2]; op<1,1>::add(t17+1, t19+1, t20);
        r[6] = t20[0];
        r[7] = t20[1];
      }



      static inline void square(const double a, double *r) {
        r[1] = a * a;
        double a_sp[2]; split(a, a_sp);
        double err1 = r[1] - (a_sp[1] * a_sp[1]);
        double err3 = err1 - ((a_sp[1] + a_sp[1]) * a_sp[0]);
        r[0] = a_sp[0] * a_sp[0] - err3;
      }

      static inline void square_2(const double *a, double *r) {
        double t1[2]; square(a[0], t1);
        r[0] = t1[0];
        double t2 = a[0] + a[0];
        double t3[2]; prod_1_1(a+1, &t2, t3);
        double t4[3]; op<2,1>::add(t3, t1 + 1, t4);
        r[1] = t4[0];
        double t5[2]; square(a[1], t5);
        double t6[4]; op<2,2>::add(t5, t4 + 1, r + 2);
      }
    }



    void exact_t::compress() {
      double sum[2];

      int j = size() - 1;
      double Q = (*this)[j];
      for (int i = (int)size()-2; i >= 0; --i) {
        detail::op<1,1>::add_fast(&Q, &(*this)[i], sum);
        if (sum[0] != 0) {
          (*this)[j--] = sum[1];
          Q = sum[0];
        } else {
          Q = sum[1];
        }
      }
      int j2 = 0;
      for (int i = j + 1; i < (int)size(); ++i) {
        detail::op<1,1>::add_fast(&(*this)[i], &Q, sum);
        if (sum[0] != 0) {
          (*this)[j2++] = sum[0];
        }
        Q = sum[1];
      }
      (*this)[j2++] = Q;

      erase(begin() + j2, end());
    }

    template<typename iter_t>
    void negate(iter_t begin, iter_t end) {
      while (begin != end) { *begin = -*begin; ++begin; }
    }

    void negate(exact_t &e) {
      negate(&e[0], &e[e.size()]);
    }

    template<typename iter_t>
    void scale_zeroelim(iter_t ebegin,
                        iter_t eend,
                        double b,
                        exact_t &h) {
      double Q;

      h.clear();
      double b_sp[2]; detail::split(b, b_sp);

      double prod[2], sum[2];

      detail::prod_1_1s((double *)ebegin++, &b, b_sp, prod);
      Q = prod[1];
      if (prod[0] != 0.0) {
        h.push_back(prod[0]);
      }
      while (ebegin != eend) {
        double enow = *ebegin++;
        detail::prod_1_1s(&enow, &b, b_sp, prod);
        detail::op<1,1>::add(&Q, prod, sum);
        if (sum[0] != 0) {
          h.push_back(sum[0]);
        }
        detail::op<1,1>::add_fast(prod+1, sum+1, sum);
        Q = sum[1];
        if (sum[0] != 0) {
          h.push_back(sum[0]);
        }
      }
      if ((Q != 0.0) || (h.size() == 0)) {
        h.push_back(Q);
      }
    }

    void scale_zeroelim(const exact_t &e,
                        double b,
                        exact_t &h) {
      scale_zeroelim(&e[0], &e[e.size()], b, h);
    }

    template<typename iter_t>
    void sum_zeroelim(iter_t ebegin,
                      iter_t eend,
                      iter_t fbegin,
                      iter_t fend,
                      exact_t &h) {
      double Q;
      double enow, fnow;

      double sum[2];

      enow = *ebegin;
      fnow = *fbegin;

      h.clear();

      if ((fnow > enow) == (fnow > -enow)) {
        Q = enow;
        enow = *++ebegin;
      } else {
        Q = fnow;
        fnow = *++fbegin;
      }

      if (ebegin != eend && fbegin != fend) {
        if ((fnow > enow) == (fnow > -enow)) {
          detail::op<1,1>::add_fast(&enow, &Q, sum);
          enow = *++ebegin;
        } else {
          detail::op<1,1>::add_fast(&fnow, &Q, sum);
          fnow = *++fbegin;
        }
        Q = sum[1];
        if (sum[0] != 0.0) {
          h.push_back(sum[0]);
        }
        while (ebegin != eend && fbegin != fend) {
          if ((fnow > enow) == (fnow > -enow)) {
            detail::op<1,1>::add(&Q, &enow, sum);
            enow = *++ebegin;
          } else {
            detail::op<1,1>::add(&Q, &fnow, sum);
            fnow = *++fbegin;
          }
          Q = sum[1];
          if (sum[0] != 0.0) {
            h.push_back(sum[0]);
          }
        }
      }

      while (ebegin != eend) {
        detail::op<1,1>::add(&Q, &enow, sum);
        enow = *++ebegin;
        Q = sum[1];
        if (sum[0] != 0.0) {
          h.push_back(sum[0]);
        }
      }
      while (fbegin != fend) {
        detail::op<1,1>::add(&Q, &fnow, sum);
        fnow = *++fbegin;
        Q = sum[1];
        if (sum[0] != 0.0) {
          h.push_back(sum[0]);
        }
      }

      if (Q != 0.0 || !h.size()) {
        h.push_back(Q);
      }
    }

    void sum_zeroelim(const exact_t &e,
                      const exact_t &f,
                      exact_t &h) {
      sum_zeroelim(&e[0], &e[e.size()], &f[0], &f[f.size()], h);
    }

    void sum_zeroelim(const double *ebegin,
                      const double *eend,
                      const exact_t &f,
                      exact_t &h) {
      sum_zeroelim(ebegin, eend, &f[0], &f[f.size()], h);
    }

    void sum_zeroelim(const exact_t &e,
                      const double *fbegin,
                      const double *fend,
                      exact_t &h) {
      sum_zeroelim(&e[0], &e[e.size()], fbegin, fend, h);
    }


    // XXX: not implemented yet
    //exact_t operator+(const exact_t &a, const exact_t &b) {
    //}



    void diffprod(const double a, const double b, const double c, const double d, double *r) {
      // return ab - cd;
      double ab[2], cd[2];
      detail::prod_1_1(&a, &b, ab);
      detail::prod_1_1(&c, &d, cd);
      detail::op<2,2>::sub(ab, cd, r);
    }

    double orient3dexact(const double *pa,
                         const double *pb,
                         const double *pc,
                         const double *pd) {
      using namespace detail;

      double ab[4]; diffprod(pa[0], pb[1], pb[0], pa[1], ab);
      double bc[4]; diffprod(pb[0], pc[1], pc[0], pb[1], bc);
      double cd[4]; diffprod(pc[0], pd[1], pd[0], pc[1], cd);
      double da[4]; diffprod(pd[0], pa[1], pa[0], pd[1], da);
      double ac[4]; diffprod(pa[0], pc[1], pc[0], pa[1], ac);
      double bd[4]; diffprod(pb[0], pd[1], pd[0], pb[1], bd);

      exact_t temp;
      exact_t cda, dab, abc, bcd;
      exact_t adet, bdet, cdet, ddet, abdet, cddet, det;

      sum_zeroelim(cd, cd + 4, da, da + 4, temp);
      sum_zeroelim(temp, ac, ac + 4, cda);

      sum_zeroelim(da, da + 4, ab, ab + 4, temp);
      sum_zeroelim(temp, bd, bd + 4, dab);

      negate(bd, bd + 4);
      negate(ac, bd + 4);

      sum_zeroelim(ab, ab + 4, bc, bc + 4, temp);
      sum_zeroelim(temp, ac, ac + 4, abc);

      sum_zeroelim(bc, bc + 4, cd, cd + 4, temp);
      sum_zeroelim(temp, bd, bd + 4, bcd);

      scale_zeroelim(bcd, +pa[2], adet);
      scale_zeroelim(cda, -pb[2], bdet);
      scale_zeroelim(dab, +pc[2], cdet);
      scale_zeroelim(abc, -pd[2], ddet);

      sum_zeroelim(adet, bdet, abdet);
      sum_zeroelim(cdet, ddet, cddet);

      sum_zeroelim(abdet, cddet, det);
  
      return det[det.size() - 1];
    }

  }
}
