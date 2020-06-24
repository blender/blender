/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Python argument wrappers and conversion tools
 *
 ******************************************************************************/

// -----------------------------------------------------------------
// NOTE:
// Do not include this file in user code, include "manta.h" instead
// -----------------------------------------------------------------

#ifdef _MANTA_H
#  ifndef _PCONVERT_H
#    define _PCONVERT_H

#    include <string>
#    include <map>
#    include <vector>

namespace Manta {
template<class T> class Grid;

//! Locks the given PbClass Arguments until ArgLocker goes out of scope
struct ArgLocker {
  void add(PbClass *p);
  ~ArgLocker();
  std::vector<PbClass *> locks;
};

PyObject *getPyNone();

// for PbClass-derived classes
template<class T> T *fromPyPtr(PyObject *obj, std::vector<void *> *tmp)
{
  if (PbClass::isNullRef(obj) || PbClass::isNoneRef(obj))
    return 0;
  PbClass *pbo = Pb::objFromPy(obj);
  const std::string &type = Namify<T>::S;
  if (!pbo || !(pbo->canConvertTo(type)))
    throw Error("can't convert argument to " + type + "*");
  return (T *)(pbo);
}

template<> float *fromPyPtr<float>(PyObject *obj, std::vector<void *> *tmp);
template<> double *fromPyPtr<double>(PyObject *obj, std::vector<void *> *tmp);
template<> int *fromPyPtr<int>(PyObject *obj, std::vector<void *> *tmp);
template<> std::string *fromPyPtr<std::string>(PyObject *obj, std::vector<void *> *tmp);
template<> bool *fromPyPtr<bool>(PyObject *obj, std::vector<void *> *tmp);
template<> Vec3 *fromPyPtr<Vec3>(PyObject *obj, std::vector<void *> *tmp);
template<> Vec3i *fromPyPtr<Vec3i>(PyObject *obj, std::vector<void *> *tmp);
template<> Vec4 *fromPyPtr<Vec4>(PyObject *obj, std::vector<void *> *tmp);
template<> Vec4i *fromPyPtr<Vec4i>(PyObject *obj, std::vector<void *> *tmp);
template<>
std::vector<PbClass *> *fromPyPtr<std::vector<PbClass *>>(PyObject *obj, std::vector<void *> *tmp);
template<>
std::vector<float> *fromPyPtr<std::vector<float>>(PyObject *obj, std::vector<void *> *tmp);

PyObject *incref(PyObject *obj);
template<class T> PyObject *toPy(const T &v)
{
  PyObject *obj = v.getPyObject();
  if (obj) {
    return incref(obj);
  }
  T *co = new T(v);
  const std::string &type = Namify<typename remove_pointers<T>::type>::S;
  return Pb::copyObject(co, type);
}
template<class T> bool isPy(PyObject *obj)
{
  if (PbClass::isNullRef(obj) || PbClass::isNoneRef(obj))
    return false;
  PbClass *pbo = Pb::objFromPy(obj);
  const std::string &type = Namify<typename remove_pointers<T>::type>::S;
  return pbo && pbo->canConvertTo(type);
}

template<class T> T fromPy(PyObject *obj)
{
  throw Error(
      "Unknown type conversion. Did you pass a PbClass by value? Instead always pass "
      "grids/particlesystems/etc. by reference or using a pointer.");
}

// builtin types
template<> float fromPy<float>(PyObject *obj);
template<> double fromPy<double>(PyObject *obj);
template<> int fromPy<int>(PyObject *obj);
template<> PyObject *fromPy<PyObject *>(PyObject *obj);
template<> std::string fromPy<std::string>(PyObject *obj);
template<> const char *fromPy<const char *>(PyObject *obj);
template<> bool fromPy<bool>(PyObject *obj);
template<> Vec3 fromPy<Vec3>(PyObject *obj);
template<> Vec3i fromPy<Vec3i>(PyObject *obj);
template<> Vec4 fromPy<Vec4>(PyObject *obj);
template<> Vec4i fromPy<Vec4i>(PyObject *obj);
template<> PbType fromPy<PbType>(PyObject *obj);
template<> PbTypeVec fromPy<PbTypeVec>(PyObject *obj);
template<> PbClass *fromPy<PbClass *>(PyObject *obj);
template<> std::vector<PbClass *> fromPy<std::vector<PbClass *>>(PyObject *obj);
template<> std::vector<float> fromPy<std::vector<float>>(PyObject *obj);

template<> PyObject *toPy<int>(const int &v);
template<> PyObject *toPy<std::string>(const std::string &val);
template<> PyObject *toPy<float>(const float &v);
template<> PyObject *toPy<double>(const double &v);
template<> PyObject *toPy<bool>(const bool &v);
template<> PyObject *toPy<Vec3i>(const Vec3i &v);
template<> PyObject *toPy<Vec3>(const Vec3 &v);
template<> PyObject *toPy<Vec4i>(const Vec4i &v);
template<> PyObject *toPy<Vec4>(const Vec4 &v);
typedef PbClass *PbClass_Ptr;
template<> PyObject *toPy<PbClass *>(const PbClass_Ptr &obj);
template<> PyObject *toPy<std::vector<PbClass *>>(const std::vector<PbClass *> &vec);
template<> PyObject *toPy<std::vector<float>>(const std::vector<float> &vec);

template<> bool isPy<float>(PyObject *obj);
template<> bool isPy<double>(PyObject *obj);
template<> bool isPy<int>(PyObject *obj);
template<> bool isPy<PyObject *>(PyObject *obj);
template<> bool isPy<std::string>(PyObject *obj);
template<> bool isPy<const char *>(PyObject *obj);
template<> bool isPy<bool>(PyObject *obj);
template<> bool isPy<Vec3>(PyObject *obj);
template<> bool isPy<Vec3i>(PyObject *obj);
template<> bool isPy<Vec4>(PyObject *obj);
template<> bool isPy<Vec4i>(PyObject *obj);
template<> bool isPy<PbType>(PyObject *obj);
template<> bool isPy<std::vector<PbClass *>>(PyObject *obj);
template<> bool isPy<std::vector<float>>(PyObject *obj);

//! Encapsulation of python arguments
class PbArgs {
 public:
  PbArgs(PyObject *linargs = NULL, PyObject *dict = NULL);
  ~PbArgs();
  void setup(PyObject *linargs = NULL, PyObject *dict = NULL);

  void check();
  FluidSolver *obtainParent();

  inline int numLinArgs()
  {
    return mLinData.size();
  }

  inline bool has(const std::string &key)
  {
    return getItem(key, false) != NULL;
  }
  inline void deleteItem(const std::string &key)
  {
    if (mData.find(key) != mData.end())
      mData.erase(mData.find(key));
  }

  inline PyObject *linArgs()
  {
    return mLinArgs;
  }
  inline PyObject *kwds()
  {
    return mKwds;
  }

  void addLinArg(PyObject *obj);

  template<class T> inline void add(const std::string &key, T arg)
  {
    DataElement el = {toPy(arg), false};
    mData[key] = el;
  }
  template<class T> inline T get(const std::string &key, int number = -1, ArgLocker *lk = NULL)
  {
    visit(number, key);
    PyObject *o = getItem(key, false, lk);
    if (o)
      return fromPy<T>(o);
    o = getItem(number, false, lk);
    if (o)
      return fromPy<T>(o);
    errMsg("Argument '" + key + "' is not defined.");
  }
  template<class T>
  inline T getOpt(const std::string &key, int number, T defarg, ArgLocker *lk = NULL)
  {
    visit(number, key);
    PyObject *o = getItem(key, false, lk);
    if (o)
      return fromPy<T>(o);
    if (number >= 0)
      o = getItem(number, false, lk);
    return (o) ? fromPy<T>(o) : defarg;
  }
  template<class T>
  inline T *getPtrOpt(const std::string &key, int number, T *defarg, ArgLocker *lk = NULL)
  {
    visit(number, key);
    PyObject *o = getItem(key, false, lk);
    if (o)
      return fromPyPtr<T>(o, &mTmpStorage);
    if (number >= 0)
      o = getItem(number, false, lk);
    return o ? fromPyPtr<T>(o, &mTmpStorage) : defarg;
  }
  template<class T> inline T *getPtr(const std::string &key, int number = -1, ArgLocker *lk = NULL)
  {
    visit(number, key);
    PyObject *o = getItem(key, false, lk);
    if (o)
      return fromPyPtr<T>(o, &mTmpStorage);
    o = getItem(number, false, lk);
    if (o)
      return fromPyPtr<T>(o, &mTmpStorage);
    errMsg("Argument '" + key + "' is not defined.");
  }

  // automatic template type deduction
  template<class T> bool typeCheck(int num, const std::string &name)
  {
    PyObject *o = getItem(name, false, 0);
    if (!o)
      o = getItem(num, false, 0);
    return o ? isPy<typename remove_pointers<T>::type>(o) : false;
  }

  PbArgs &operator=(const PbArgs &a);  // dummy
  void copy(PbArgs &a);
  void clear();
  void visit(int num, const std::string &key);

  static PbArgs EMPTY;

 protected:
  PyObject *getItem(const std::string &key, bool strict, ArgLocker *lk = NULL);
  PyObject *getItem(size_t number, bool strict, ArgLocker *lk = NULL);

  struct DataElement {
    PyObject *obj;
    bool visited;
  };
  std::map<std::string, DataElement> mData;
  std::vector<DataElement> mLinData;
  PyObject *mLinArgs, *mKwds;
  std::vector<void *> mTmpStorage;
};

}  // namespace Manta

#    if NUMPY == 1
#      include "numpyWrap.h"
#    endif

#  endif
#endif
