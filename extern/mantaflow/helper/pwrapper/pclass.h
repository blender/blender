/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011-2014 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Base class for all Python-exposed classes
 *
 ******************************************************************************/

// -----------------------------------------------------------------
// NOTE:
// Do not include this file in user code, include "manta.h" instead
// -----------------------------------------------------------------

#ifdef _MANTA_H
#  ifndef _PTYPE_H
#    define _PTYPE_H

#    include <string>
#    include <vector>
#    include <map>

class QMutex;

namespace Manta {
struct PbClassData;
class FluidSolver;
class PbArgs;

struct PbType {
  std::string S;
  std::string str() const;
};
struct PbTypeVec {
  std::vector<PbType> T;
  std::string str() const;
};

//! Base class for all classes exposed to Python
class PbClass {
 public:
  PbClass(FluidSolver *parent, const std::string &name = "", PyObject *obj = nullptr);
  PbClass(const PbClass &a);
  virtual ~PbClass();

  // basic property setter/getters
  void setName(const std::string &name)
  {
    mName = name;
  }
  std::string getName() const
  {
    return mName;
  }
  PyObject *getPyObject() const
  {
    return mPyObject;
  }
  void registerObject(PyObject *obj, PbArgs *args);
  FluidSolver *getParent() const
  {
    return mParent;
  }
  void setParent(FluidSolver *v)
  {
    mParent = v;
  }
  void checkParent();

  // hidden flag for GUI, debug output
  inline bool isHidden()
  {
    return mHidden;
  }
  inline void setHidden(bool v)
  {
    mHidden = v;
  }

  void lock();
  void unlock();
  bool tryLock();

  // PbClass instance registry
  static int getNumInstances();
  static PbClass *getInstance(int index);
  static void renameObjects();

  // converters
  static bool isNullRef(PyObject *o);
  static bool isNoneRef(PyObject *o);
  static PbClass *createPyObject(const std::string &classname,
                                 const std::string &name,
                                 PbArgs &args,
                                 PbClass *parent);
  inline bool canConvertTo(const std::string &classname)
  {
    return Pb::canConvert(mPyObject, classname);
  }

 protected:
  QMutex *mMutex;
  FluidSolver *mParent;
  PyObject *mPyObject;
  std::string mName;
  bool mHidden;

  static std::vector<PbClass *> mInstances;
};

//!\cond Register

void pbFinalizePlugin(FluidSolver *parent, const std::string &name, bool doTime = true);
void pbPreparePlugin(FluidSolver *parent, const std::string &name, bool doTime = true);
void pbSetError(const std::string &fn, const std::string &ex);

//!\endcond

}  // namespace Manta

#  endif
#endif
