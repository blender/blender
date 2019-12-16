/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2017 Steffen Wiewel, Moritz Baecher, Rachel Chu
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Convert mantaflow grids to/from numpy arrays
 *
 ******************************************************************************/

#ifdef _PCONVERT_H
#  ifndef _NUMPYCONVERT_H
#    define _NUMPYCONVERT_H

enum NumpyTypes {
  N_BOOL = 0,
  N_BYTE,
  N_UBYTE,
  N_SHORT,
  N_USHORT,
  N_INT,
  N_UINT,
  N_LONG,
  N_ULONG,
  N_LONGLONG,
  N_ULONGLONG,
  N_FLOAT,
  N_DOUBLE,
  N_LONGDOUBLE,
  N_CFLOAT,
  N_CDOUBLE,
  N_CLONGDOUBLE,
  N_OBJECT = 17,
  N_STRING,
  N_UNICODE,
  N_VOID,
  /*
   * New 1.6 types appended, may be integrated
   * into the above in 2.0.
   */
  N_DATETIME,
  N_TIMEDELTA,
  N_HALF,

  N_NTYPES,
  N_NOTYPE,
  N_CHAR,          /* special flag */
  N_USERDEF = 256, /* leave room for characters */

  /* The number of types not including the new 1.6 types */
  N_NTYPES_ABI_COMPATIBLE = 21
};

namespace Manta {
class PyArrayContainer {
 public:
  /// Constructors
  PyArrayContainer(void *_pParentPyArray);
  PyArrayContainer(const PyArrayContainer &_Other);
  ~PyArrayContainer();
  /// Operators
  PyArrayContainer &operator=(const PyArrayContainer &_Other);

 private:
  void ExtractData(void *_pParentPyArray);

 public:
  void *pData;
  NumpyTypes DataType;
  unsigned int TotalSize;
  std::vector<long> Dims;

 private:
  void *pParentPyArray;
};

// template<> PyArrayContainer* fromPyPtr<PyArrayContainer>(PyObject* obj, std::vector<void*>*
// tmp);
template<> PyArrayContainer fromPy<PyArrayContainer>(PyObject *obj);
}  // namespace Manta

#  endif
#endif
