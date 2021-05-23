

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Globally used macros and functions
 *
 ******************************************************************************/

#ifndef _GENERAL_H
#define _GENERAL_H

#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace Manta {

// ui data exchange
#ifdef GUI
// defined in qtmain.cpp
extern void updateQtGui(bool full, int frame, float time, const std::string &curPlugin);
#else
// dummy function if GUI is not enabled
inline void updateQtGui(bool full, int frame, float time, const std::string &curPlugin)
{
}
#endif

// activate debug mode if _DEBUG is defined (eg for windows)
#ifndef DEBUG
#  ifdef _DEBUG
#    define DEBUG 1
#  endif  // _DEBUG
#endif    // DEBUG

// Standard exception
class Error : public std::exception {
 public:
  Error(const std::string &s) : mS(s)
  {
#ifdef DEBUG
    // print error
    std::cerr << "Aborting: " << s << " \n";
    // then force immedieate crash in debug mode
    *(volatile int *)(0) = 1;
#endif
  }
  virtual ~Error() throw()
  {
  }
  virtual const char *what() const throw()
  {
    return mS.c_str();
  }

 private:
  std::string mS;
};

// mark unused parameter variables
#define unusedParameter(x) ((void)x)

// Debug output functions and macros
extern int gDebugLevel;

#define MSGSTREAM \
  std::ostringstream msg; \
  msg.precision(7); \
  msg.width(9);
#define debMsg(mStr, level) \
  if (_chklevel(level)) { \
    MSGSTREAM; \
    msg << mStr; \
    std::cout << msg.str() << std::endl; \
  }
inline bool _chklevel(int level = 0)
{
  return gDebugLevel >= level;
}

// error and assertation macros
#ifdef DEBUG
#  define DEBUG_ONLY(a) a
#else
#  define DEBUG_ONLY(a)
#endif
#define throwError(msg) \
  { \
    std::ostringstream __s; \
    __s << msg << std::endl << "Error raised in " << __FILE__ << ":" << __LINE__; \
    throw Manta::Error(__s.str()); \
  }
#define errMsg(msg) throwError(msg);
#define assertMsg(cond, msg) \
  if (!(cond)) \
  throwError(msg)
#define assertDeb(cond, msg) DEBUG_ONLY(assertMsg(cond, msg))

// for compatibility with blender, blender only defines WITH_FLUID, make sure we have "BLENDER"
#ifndef BLENDER
#  ifdef WITH_FLUID
#    define BLENDER 1
#  endif
#endif

// common type for indexing large grids
typedef long long IndexInt;

// template tricks
template<typename T> struct remove_pointers {
  typedef T type;
};

template<typename T> struct remove_pointers<T *> {
  typedef T type;
};

template<typename T> struct remove_pointers<T &> {
  typedef T type;
};

// Commonly used enums and types
//! Timing class for preformance measuring
struct MuTime {
  MuTime()
  {
    get();
  }
  MuTime operator-(const MuTime &a)
  {
    MuTime b;
    b.time = time - a.time;
    return b;
  };
  MuTime operator+(const MuTime &a)
  {
    MuTime b;
    b.time = time + a.time;
    return b;
  };
  MuTime operator/(unsigned long a)
  {
    MuTime b;
    b.time = time / a;
    return b;
  };
  MuTime &operator+=(const MuTime &a)
  {
    time += a.time;
    return *this;
  }
  MuTime &operator-=(const MuTime &a)
  {
    time -= a.time;
    return *this;
  }
  MuTime &operator/=(unsigned long a)
  {
    time /= a;
    return *this;
  }
  std::string toString();

  void clear()
  {
    time = 0;
  }
  void get();
  MuTime update();

  unsigned long time;
};
std::ostream &operator<<(std::ostream &os, const MuTime &t);

//! generate a string with infos about the current mantaflow build
std::string buildInfoString();

// Some commonly used math helpers
template<class T> inline T square(T a)
{
  return a * a;
}
template<class T> inline T cubed(T a)
{
  return a * a * a;
}

template<class T> inline T clamp(const T &val, const T &vmin, const T &vmax)
{
  if (val < vmin)
    return vmin;
  if (val > vmax)
    return vmax;
  return val;
}

template<class T> inline T nmod(const T &a, const T &b);
template<> inline int nmod(const int &a, const int &b)
{
  int c = a % b;
  return (c < 0) ? (c + b) : c;
}
template<> inline float nmod(const float &a, const float &b)
{
  float c = std::fmod(a, b);
  return (c < 0) ? (c + b) : c;
}
template<> inline double nmod(const double &a, const double &b)
{
  double c = std::fmod(a, b);
  return (c < 0) ? (c + b) : c;
}

template<class T> inline T safeDivide(const T &a, const T &b);
template<> inline int safeDivide<int>(const int &a, const int &b)
{
  return (b) ? (a / b) : a;
}
template<> inline float safeDivide<float>(const float &a, const float &b)
{
  return (b) ? (a / b) : a;
}
template<> inline double safeDivide<double>(const double &a, const double &b)
{
  return (b) ? (a / b) : a;
}

inline bool c_isnan(float c)
{
  volatile float d = c;
  return d != d;
}

//! Swap so that a<b
template<class T> inline void sort(T &a, T &b)
{
  if (a > b)
    std::swap(a, b);
}

//! Swap so that a<b<c
template<class T> inline void sort(T &a, T &b, T &c)
{
  if (a > b)
    std::swap(a, b);
  if (a > c)
    std::swap(a, c);
  if (b > c)
    std::swap(b, c);
}

//! Swap so that a<b<c<d
template<class T> inline void sort(T &a, T &b, T &c, T &d)
{
  if (a > b)
    std::swap(a, b);
  if (c > d)
    std::swap(c, d);
  if (a > c)
    std::swap(a, c);
  if (b > d)
    std::swap(b, d);
  if (b > c)
    std::swap(b, c);
}

}  // namespace Manta

#endif
