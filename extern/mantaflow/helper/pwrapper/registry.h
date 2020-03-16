/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011-2014 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Auto python registry
 *
 ******************************************************************************/

#ifndef _REGISTRY_H
#define _REGISTRY_H

#include <string>
#include <vector>

// forward declaration to minimize Python.h includes
#ifndef PyObject_HEAD
#  ifndef PyObject_Fake
struct _object;
typedef _object PyObject;
#    define PyObject_Fake
#  endif
#endif

namespace Manta {
class PbClass;
class PbArgs;
}  // namespace Manta

// **************************************************
//                      NOTE
// Everything in this file is intend only for internal
// use by the generated wrappers or pclass/pconvert.
// For user code, use the functionality exposed in
// pclass.h / pconvert.h instead.
// **************************************************

// Used to turn names into strings
namespace Manta {
template<class T> struct Namify {
  static const char *S;
};
}  // namespace Manta
namespace Pb {

// internal registry access
void setup(const std::string &filename, const std::vector<std::string> &args);
void finalize();
bool canConvert(PyObject *obj, const std::string &to);
Manta::PbClass *objFromPy(PyObject *obj);
Manta::PbClass *createPy(const std::string &classname,
                         const std::string &name,
                         Manta::PbArgs &args,
                         Manta::PbClass *parent);
void setReference(Manta::PbClass *cls, PyObject *obj);
PyObject *copyObject(Manta::PbClass *cls, const std::string &classname);
void MantaEnsureRegistration();

#ifdef BLENDER
#  ifdef PyMODINIT_FUNC
PyMODINIT_FUNC PyInit_manta_main(void);
#  endif
#endif

// callback type
typedef void (*InitFunc)(PyObject *);
typedef PyObject *(*GenericFunction)(PyObject *self, PyObject *args, PyObject *kwds);
typedef PyObject *(*OperatorFunction)(PyObject *self, PyObject *o);
typedef int (*Constructor)(PyObject *self, PyObject *args, PyObject *kwds);
typedef PyObject *(*Getter)(PyObject *self, void *closure);
typedef int (*Setter)(PyObject *self, PyObject *value, void *closure);

//! Auto registry of python methods and classes
struct Register {
  //! register method
  Register(const std::string &className, const std::string &funcName, GenericFunction func);
  //! register operator
  Register(const std::string &className, const std::string &funcName, OperatorFunction func);
  //! register constructor
  Register(const std::string &className, const std::string &funcName, Constructor func);
  //! register getter/setter
  Register(const std::string &className,
           const std::string &property,
           Getter getter,
           Setter setter);
  //! register class
  Register(const std::string &className, const std::string &pyName, const std::string &baseClass);
  //! register enum entry
  Register(const std::string &name, const int value);
  //! register python code
  Register(const std::string &file, const std::string &pythonCode);
  //! register external code
  Register(InitFunc func);
};

#define KEEP_UNUSED(var) \
  do { \
    (void)var; \
  } while (false);

}  // namespace Pb
#endif
