/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Functions for property setting/getting via python
 *
 ******************************************************************************/

#include "pythonInclude.h"
#include "structmember.h"
#include "manta.h"
#include "general.h"
#include "timing.h"

#ifdef GUI
#  include <QMutex>
#else
class QMutex {
 public:
  void lock(){};
  void unlock(){};
  bool tryLock()
  {
    return true;
  };
};
#endif

using namespace std;
namespace Manta {

//******************************************************************************
// Free functions

void pbPreparePlugin(FluidSolver *parent, const string &name, bool doTime)
{
  if (doTime)
    TimingData::instance().start(parent, name);
}

void pbFinalizePlugin(FluidSolver *parent, const string &name, bool doTime)
{
  if (doTime)
    TimingData::instance().stop(parent, name);

  // GUI update, also print name of parent if there's more than one
  std::ostringstream msg;
  if (name != "FluidSolver::step") {
    if (parent && (parent->getNumInstances() > 0))
      msg << parent->getName() << string(".");
    msg << name;
  }
  updateQtGui(false, 0, 0., msg.str());

  debMsg(name << " done", 3);
  // name unnamed PbClass Objects from var name
  PbClass::renameObjects();
}

void pbSetError(const string &fn, const string &ex)
{
  debMsg("Error in " << fn, 1);
  if (!ex.empty())
    PyErr_SetString(PyExc_RuntimeError, ex.c_str());
}

//******************************************************************************
// Helpers

string PbTypeVec::str() const
{
  if (T.empty())
    return "";
  string s = "<";
  for (int i = 0; i < (int)T.size(); i++) {
    s += T[i].str();
    s += (i != (int)T.size() - 1) ? ',' : '>';
  }
  return s;
}
string PbType::str() const
{
  if (S == "float")
    return "Real";
  if (S == "manta.vec3")
    return "Vec3";
  return S;
}

//******************************************************************************
// PbClass

vector<PbClass *> PbClass::mInstances;

PbClass::PbClass(FluidSolver *parent, const string &name, PyObject *obj)
    : mMutex(nullptr), mParent(parent), mPyObject(obj), mName(name), mHidden(false)
{
  mMutex = new QMutex();
}

PbClass::PbClass(const PbClass &a)
    : mMutex(nullptr), mParent(a.mParent), mPyObject(0), mName("_unnamed"), mHidden(false)
{
  mMutex = new QMutex();
}

PbClass::~PbClass()
{
  for (vector<PbClass *>::iterator it = mInstances.begin(); it != mInstances.end(); ++it) {
    if (*it == this) {
      mInstances.erase(it);
      break;
    }
  }
  delete mMutex;
}

void PbClass::lock()
{
  mMutex->lock();
}
void PbClass::unlock()
{
  mMutex->unlock();
}
bool PbClass::tryLock()
{
  return mMutex->tryLock();
}

PbClass *PbClass::getInstance(int idx)
{
  if (idx < 0 || idx > (int)mInstances.size())
    errMsg("PbClass::getInstance(): invalid index");
  return mInstances[idx];
}

int PbClass::getNumInstances()
{
  return mInstances.size();
}

bool PbClass::isNullRef(PyObject *obj)
{
  return PyLong_Check(obj) && PyLong_AsDouble(obj) == 0;
}

bool PbClass::isNoneRef(PyObject *obj)
{
  return (obj == Py_None);
}

void PbClass::registerObject(PyObject *obj, PbArgs *args)
{
  // cross link
  Pb::setReference(this, obj);
  mPyObject = obj;

  mInstances.push_back(this);

  if (args) {
    string _name = args->getOpt<std::string>("name", -1, "");
    if (!_name.empty())
      setName(_name);
  }
}

PbClass *PbClass::createPyObject(const string &classname,
                                 const string &name,
                                 PbArgs &args,
                                 PbClass *parent)
{
  return Pb::createPy(classname, name, args, parent);
}

void PbClass::checkParent()
{
  if (getParent() == nullptr) {
    errMsg("New class " + mName + ": no parent given -- specify using parent=xxx !");
  }
}
//! Assign unnamed PbClass objects their Python variable name
void PbClass::renameObjects()
{
  PyObject *sys_mod_dict = PyImport_GetModuleDict();
  PyObject *loc_mod = PyMapping_GetItemString(sys_mod_dict, (char *)"__main__");
  if (!loc_mod)
    return;
  PyObject *locdict = PyObject_GetAttrString(loc_mod, "__dict__");
  if (!locdict)
    return;

  // iterate all PbClass instances
  for (size_t i = 0; i < mInstances.size(); i++) {
    PbClass *obj = mInstances[i];
    if (obj->getName().empty()) {
      // empty, try to find instance in module local dictionary

      PyObject *lkey, *lvalue;
      Py_ssize_t lpos = 0;
      while (PyDict_Next(locdict, &lpos, &lkey, &lvalue)) {
        if (lvalue == obj->mPyObject) {
          string varName = fromPy<string>(PyObject_Str(lkey));
          obj->setName(varName);
          // cout << "assigning variable name '" << varName << "' to unnamed instance" << endl;
          break;
        }
      }
    }
  }
  Py_DECREF(locdict);
  Py_DECREF(loc_mod);
}

}  // namespace Manta
