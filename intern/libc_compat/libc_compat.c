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
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/* On Linux, precompiled libraries may be made with an glibc version that is
 * incompatible with the system libraries that Blender is built on. To solve
 * this we add a few -ffast-math symbols that can be missing. */

#ifdef __linux__
#  include <features.h>
#  include <math.h>

#  if defined(__GLIBC_PREREQ) && __GLIBC_PREREQ(2, 31)

double __exp_finite(double x);
double __exp2_finite(double x);
double __acos_finite(double x);
double __asin_finite(double x);
double __log2_finite(double x);
double __log10_finite(double x);
double __log_finite(double x);
double __pow_finite(double x, double y);
float __expf_finite(float x);
float __exp2f_finite(float x);
float __acosf_finite(float x);
float __asinf_finite(float x);
float __log2f_finite(float x);
float __log10f_finite(float x);
float __logf_finite(float x);
float __powf_finite(float x, float y);

double __exp_finite(double x)
{
  return exp(x);
}

double __exp2_finite(double x)
{
  return exp2(x);
}

double __acos_finite(double x)
{
  return acos(x);
}

double __asin_finite(double x)
{
  return asin(x);
}

double __log2_finite(double x)
{
  return log2(x);
}

double __log10_finite(double x)
{
  return log10(x);
}

double __log_finite(double x)
{
  return log(x);
}

double __pow_finite(double x, double y)
{
  return pow(x, y);
}

float __expf_finite(float x)
{
  return expf(x);
}

float __exp2f_finite(float x)
{
  return exp2f(x);
}

float __acosf_finite(float x)
{
  return acosf(x);
}

float __asinf_finite(float x)
{
  return asinf(x);
}

float __log2f_finite(float x)
{
  return log2f(x);
}

float __log10f_finite(float x)
{
  return log10f(x);
}

float __logf_finite(float x)
{
  return logf(x);
}

float __powf_finite(float x, float y)
{
  return powf(x, y);
}

#  endif /* __GLIBC_PREREQ */
#endif   /* __linux__ */
