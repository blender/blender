#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"

#include "BLI_math.h"
#include "BLI_math_vec_types.hh"
#include "BLI_vector.hh"

#include <cstdio>
#include <utility>

/*
 * Arc length parameterized spline library.
 */
namespace blender {
/*
 Abstract curve interface.

template<typename Float> class Curve {
  using Vector = vec_base<Float, 2>;

 public:
  Float length;

  Vector evaluate(Float s);
  Vector derivative(Float s);
  Vector derivative2(Float s);
  Float curvature(float s);

  void update();
};
*/

/*
comment: Reduce algebra script;

on factor;
off period;

procedure bez(a, b);
  a + (b - a) * t;

lin := bez(k1, k2);
quad := bez(lin, sub(k2=k3, k1=k2, lin));

cubic := bez(quad, sub(k3=k4, k2=k3, k1=k2, quad));
dcubic := df(cubic, t);
icubic := int(cubic, t);

x1 := 0;
y1 := 0;

dx := sub(k1=x1, k2=x2, k3=x3, k4=x4, dcubic);
dy := sub(k1=y1, k2=y2, k3=y3, k4=y4, dcubic);
darc := sqrt(dx**2 + dy**2);

arcstep := darc*dt + 0.5*df(darc, t)*dt*dt;

gentran
begin
declare <<
x1,x2,x3,x4 : float;
y1,y2,y3,y4 : float;
dt,t : float;
>>;
return eval(arcstep)
end;

on fort;
cubic;
dcubic;
icubic;
arcstep;
off fort;

*/
template<typename Float, int axes = 2, int table_size = 512> class CubicBezier {
  using Vector = vec_base<Float, axes>;

 public:
  Vector ps[4];

  CubicBezier(Vector a, Vector b, Vector c, Vector d)
  {
    ps[0] = a;
    ps[1] = b;
    ps[2] = c;
    ps[3] = d;

    deleted = false;
    _arc_to_t = new Float[table_size];
  }

  ~CubicBezier()
  {
    deleted = true;

    if (_arc_to_t) {
      delete[] _arc_to_t;
      _arc_to_t = nullptr;
    }
  }

  CubicBezier()
  {
    deleted = false;
    _arc_to_t = new Float[table_size];
  }

  CubicBezier(const CubicBezier &b)
  {
    _arc_to_t = new Float[table_size];
    *this = b;
    deleted = false;
  }

  CubicBezier &operator=(const CubicBezier &b)
  {
    ps[0] = b.ps[0];
    ps[1] = b.ps[1];
    ps[2] = b.ps[2];
    ps[3] = b.ps[3];

    length = b.length;

    if (!_arc_to_t) {
      _arc_to_t = new Float[table_size];
    }

    if (b._arc_to_t) {
      for (int i = 0; i < table_size; i++) {
        _arc_to_t[i] = b._arc_to_t[i];
      }
    }

    return *this;
  }

#if 1
  CubicBezier(CubicBezier &&b)
  {
    *this = b;
  }

  CubicBezier &operator=(CubicBezier &&b)
  {
    ps[0] = b.ps[0];
    ps[1] = b.ps[1];
    ps[2] = b.ps[2];
    ps[3] = b.ps[3];

    length = b.length;

    if (b._arc_to_t) {
      _arc_to_t = std::move(b._arc_to_t);
      b._arc_to_t = nullptr;
    }
    else {
      _arc_to_t = new Float[table_size];
    }

    return *this;
  }
#endif

  Float length;

  void update()
  {
    Float t = 0.0, dt = 1.0 / (Float)table_size;
    Float s = 0.0;

    if (!_arc_to_t) {
      _arc_to_t = new Float[table_size];
    }

    auto table = _arc_to_t;

    for (int i = 0; i < table_size; i++) {
      table[i] = -1.0;
    }

    length = 0.0;

    for (int i = 0; i < table_size; i++, t += dt) {
      Float dlen = 0.0;
      for (int j = 0; j < axes; j++) {
        float dv = dcubic(ps[0][j], ps[1][j], ps[2][j], ps[3][j], t);

        dlen += dv * dv;
      }

      dlen = sqrt(dlen) * dt;

      length += dlen;
    }

    const int samples = table_size;
    dt = 1.0 / (Float)samples;

    t = 0.0;
    s = 0.0;

    for (int i = 0; i < samples; i++, t += dt) {
      Float dlen = 0.0;
      for (int j = 0; j < axes; j++) {
        float dv = dcubic(ps[0][j], ps[1][j], ps[2][j], ps[3][j], t);

        dlen += dv * dv;
      }

      dlen = sqrt(dlen) * dt;

      int j = (int)((s / length) * (Float)table_size * 0.999999);
      j = min_ii(j, table_size - 1);

      table[j] = t;

      s += dlen;
    }

    table[0] = 0.0;
    table[table_size - 1] = 1.0;

#if 1
    /* Interpolate gaps in table. */
    for (int i = 0; i < table_size - 1; i++) {
      if (table[i] == -1.0 || table[i + 1] != -1.0) {
        continue;
      }

      int i1 = i;
      int i2 = i + 1;

      while (table[i2] == -1.0) {
        i2++;
      }

      int start = table[i1];
      int end = table[i2];
      Float dt2 = 1.0 / (i2 - i1);

      for (int j = i1 + 1; j < i2; j++) {
        Float factor = (Float)(j - i1) * dt2;
        table[j] = start + (end - start) * factor;
      }

      i = i2 - 1;
    }

#  if 0
    for (int i = 0; i < table_size; i++) {
      printf("%.3f ", table[i]);
    }
    printf("\n\n");
#  endif
#endif
  }

  Vector evaluate(Float s)
  {
    Float t = arc_to_t(s);
    Vector r;

    for (int i = 0; i < axes; i++) {
      r[i] = cubic(ps[0][i], ps[1][i], ps[2][i], ps[3][i], t);
    }

    return r;
  }

  Vector derivative(Float s)
  {
    Float t = arc_to_t(s);
    Vector r;

    for (int i = 0; i < axes; i++) {
      r[i] = dcubic(ps[0][i], ps[1][i], ps[2][i], ps[3][i], t) * length;
    }

    return r;
  }

  Vector derivative2(Float s)
  {
    Float t = arc_to_t(s);
    Vector r;

    for (int i = 0; i < axes; i++) {
      r[i] = d2cubic(ps[0][i], ps[1][i], ps[2][i], ps[3][i], t) * length;
    }

    return r;
  }

  Float curvature(Float s)
  {
    /* Get signed curvature if in 2d. */
    if constexpr (axes == 2) {
      Vector dv1 = derivative(s);
      Vector dv2 = derivative2(s);

      return (dv1[0] * dv2[1] - dv1[1] * dv2[0]) /
             powf(dv1[0] * dv1[0] + dv1[1] * dv1[1], 3.0 / 2.0);
    }
    else { /* Otherwise use magnitude of second derivative (this works because we are arc-length
              parameterized). */
      Vector dv2 = derivative2(s);
      Float len = 0.0;

      for (int i = 0; i < axes; i++) {
        len += dv2[i] * dv2[i];
      }

      return sqrt(len);
    }
  }

 private:
  Float *_arc_to_t;
  bool deleted = false;

  Float cubic(Float k1, Float k2, Float k3, Float k4, Float t)
  {
    return -(((3.0 * (t - 1.0) * k3 - k4 * t) * t - 3.0 * (t - 1.0) * (t - 1.0) * k2) * t +
             (t - 1) * (t - 1) * (t - 1) * k1);
  }

  Float dcubic(Float k1, Float k2, Float k3, Float k4, Float t)
  {
    return -3.0 * ((t - 1.0) * (t - 1.0) * k1 - k4 * t * t + (3.0 * t - 2.0) * k3 * t -
                   (3.0 * t - 1.0) * (t - 1.0) * k2);
  }

  Float d2cubic(Float k1, Float k2, Float k3, Float k4, Float t)
  {
    return -6.0 * (k1 * t - k1 - 3.0 * k2 * t + 2.0 * k2 + 3.0 * k3 * t - k3 - k4 * t);
  }

  Float clamp_s(Float s)
  {
    s = s < 0.0 ? 0.0 : s;
    s = s >= length ? length * 0.999999 : s;

    return s;
  }

  Float arc_to_t(Float s)
  {
    if (length == 0.0) {
      return 0.0;
    }

    s = clamp_s(s);

    Float t = s * (Float)(table_size - 1) / length;

    int i1 = floorf(t);
    int i2 = min_ii(i1 + 1, table_size - 1);

    t -= (Float)i1;

    Float s1 = _arc_to_t[i1];
    Float s2 = _arc_to_t[i2];

    return s1 + (s2 - s1) * t;
  }
};

template<typename Float, int axes = 2> class BezierSpline {
  using Vector = vec_base<Float, axes>;
  struct Segment {
    CubicBezier<Float, axes> bezier;
    Float start = 0.0;

    Segment(const CubicBezier<Float> &bez)
    {
      bezier = bez;
    }

    Segment(const Segment &b)
    {
      *this = b;
    }

    Segment &operator=(const Segment &b)
    {
      bezier = b.bezier;
      start = b.start;

      return *this;
    }

    Segment()
    {
    }
  };

 public:
  Float length = 0.0;
  bool deleted = false;
  blender::Vector<Segment> segments;

  void clear()
  {
    segments.clear();
  }

  BezierSpline()
  {
  }

  ~BezierSpline()
  {
    deleted = true;
  }

  void add(CubicBezier<Float, axes> &bez)
  {
    need_update = true;

    Segment seg;

    seg.bezier = bez;
    segments.append(seg);

    update();
  }

  void update()
  {
    need_update = false;

    length = 0.0;
    for (Segment &seg : segments) {
      seg.start = length;
      length += seg.bezier.length;
    }
  }

  Vector evaluate(Float s)
  {
    if (segments.size() == 0) {
      return Vector();
    }

    if (s == 0.0) {
      return segments[0].bezier.ps[0];
    }

    if (s >= length) {
      return segments[segments.size() - 1].bezier.ps[3];
    }

    Segment *seg = get_segment(s);

    return seg->bezier.evaluate(s - seg->start);
  }

  Vector derivative(Float s)
  {
    if (segments.size() == 0) {
      return Vector();
    }

    s = clamp_s(s);
    Segment *seg = get_segment(s);

    return seg->bezier.derivative(s - seg->start);
  }

  Vector derivative2(Float s)
  {
    if (segments.size() == 0) {
      return Vector();
    }

    s = clamp_s(s);
    Segment *seg = get_segment(s);

    return seg->bezier.derivative2(s - seg->start);
  }

  Float curvature(Float s)
  {
    if (segments.size() == 0) {
      return 0.0;
    }

    s = clamp_s(s);
    Segment *seg = get_segment(s);

    return seg->bezier.curvature(s - seg->start);
  }

  Vector closest_point(const Vector p, Float &r_s, Vector &r_tan, Float &r_dis)
  {
    const int steps = 5;
    Float s = 0.0, ds = length / steps;
    Float mindis = FLT_MAX;
    Vector minp;
    Float mins = 0.0;
    bool found = false;

    Vector lastdv, lastp;
    Vector b, dvb;

    for (int i = 0; i < steps + 1; i++, s += ds, lastp = b, lastdv = dvb) {
      b = evaluate(s);
      dvb = derivative(s);

      if (i == 0) {
        continue;
      }

      Vector dva = lastdv;
      Vector a = lastp;

      // Vector dva = derivative(s);
      // Vector dvb = derivative(s + ds);
      // Vector a = evaluate(s);
      // Vector b = evaluate(s + ds);

      Vector vec1 = a - p;
      Vector vec2 = b - p;

      Float sign1 = _dot(vec1, dva);
      Float sign2 = _dot(vec2, dvb);

      if ((sign1 < 0.0) == (sign2 < 0.0)) {
        found = true;

        Float len = _dot(vec1, vec1);

        if (len < mindis) {
          mindis = len;
          mins = s;
          minp = evaluate(s);
        }
        continue;
      }

      found = true;

      Float start = s - ds;
      Float end = s;
      Float mid = (start + end) * 0.5;
      const int binary_steps = 4;

      for (int j = 0; j < binary_steps; j++) {
        Vector dvmid = derivative(mid);
        Vector vecmid = evaluate(mid) - p;
        Float sign_mid = _dot(vecmid, dvmid);

        if ((sign_mid < 0.0) == (sign1 < 0.0)) {
          start = mid;
        }
        else {
          end = mid;
        }
        mid = (start + end) * 0.5;
      }

      Vector p2 = evaluate(mid);
      Vector vec_mid = p2 - p;
      Float len = _dot(vec_mid, vec_mid);

      if (len < mindis) {
        mindis = len;
        minp = p2;
        mins = mid;
      }
    }

    if (!found) {
      mins = 0.0;
      minp = evaluate(mins);
      Vector vec = minp - p;
      mindis = _dot(vec, vec);
    }

    r_tan = derivative(mins);
    r_s = mins;
    r_dis = sqrtf(mindis);

    return minp;
  }

  void pop_front(int n = 1)
  {
    for (int i = 0; i < segments.size() - n; i++) {
      segments[i] = segments[i + n];
    }

    segments.resize(segments.size() - n);
    update();
  }

 private:
  bool need_update;

  Float _dot(Vector a, Vector b)
  {
    Float sum = 0.0;

    for (int i = 0; i < axes; i++) {
      sum += a[i] * b[i];
    }

    return sum;
  }

  Float clamp_s(Float s)
  {
    s = s < 0.0 ? 0.0 : s;
    s = s >= length ? length * 0.999999 : s;

    return s;
  }

  Segment *get_segment(Float s)
  {
    // printf("\n");

    for (Segment &seg : segments) {
      // printf("s: %f %f\n", seg.start, seg.start + seg.bezier.length);

      if (s >= seg.start && s < seg.start + seg.bezier.length) {
        return &seg;
      }
    }

    // printf("\n");

    return nullptr;
  }
};

using BezierSpline2f = BezierSpline<float, 2>;
using BezierSpline3f = BezierSpline<float, 3>;
}  // namespace blender
