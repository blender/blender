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

#include <string.h>
#include "pythonInclude.h"
#include "structmember.h"
#include "manta.h"

using namespace std;

const string gDefaultModuleName = "manta";

namespace Pb {

//******************************************************************************
// Custom object definition

struct Method {
  Method(const string &n, const string &d, GenericFunction f) : name(n), doc(d), func(f)
  {
  }
  string name, doc;
  GenericFunction func;

  PyMethodDef def()
  {
    PyMethodDef def = {&name[0], (PyCFunction)func, METH_VARARGS | METH_KEYWORDS, &doc[0]};
    return def;
  }
};
struct GetSet {
  GetSet() : getter(0), setter(0)
  {
  }
  GetSet(const string &n, const string &d, Getter g, Setter s)
      : name(n), doc(d), getter(g), setter(s)
  {
  }
  string name, doc;
  Getter getter;
  Setter setter;

  PyGetSetDef def()
  {
    PyGetSetDef def = {&name[0], getter, setter, &doc[0], NULL};
    return def;
  }
};

struct ClassData {
  string cName, pyName;
  string cPureName, cTemplate;
  InitFunc init;
  PyTypeObject typeInfo;
  PyNumberMethods numInfo;
  // PySequenceMethods seqInfo;
  vector<Method> methods;
  map<string, GetSet> getset;
  map<string, OperatorFunction> ops;
  ClassData *baseclass;
  string baseclassName;
  Constructor constructor;

  vector<PyMethodDef> genMethods;
  vector<PyGetSetDef> genGetSet;
};

struct PbObject {
  PyObject_HEAD Manta::PbClass *instance;
  ClassData *classdef;
};

//******************************************************
// Internal wrapper class

//! Registers all classes and methods exposed to Python.
/*! This class is only used internally by Pb:: framwork.
 *  Please use the functionality of PbClass to lookup and translate pointers. */
class WrapperRegistry {
 public:
  static WrapperRegistry &instance();
  void addClass(const std::string &name,
                const std::string &internalName,
                const std::string &baseclass);
  void addEnumEntry(const std::string &name, int value);
  void addExternalInitializer(InitFunc func);
  void addMethod(const std::string &classname,
                 const std::string &methodname,
                 GenericFunction method);
  void addOperator(const std::string &classname,
                   const std::string &methodname,
                   OperatorFunction method);
  void addConstructor(const std::string &classname, Constructor method);
  void addGetSet(const std::string &classname,
                 const std::string &property,
                 Getter getfunc,
                 Setter setfunc);
  void addPythonPath(const std::string &path);
  void addPythonCode(const std::string &file, const std::string &code);
  PyObject *createPyObject(const std::string &classname,
                           const std::string &name,
                           Manta::PbArgs &args,
                           Manta::PbClass *parent);
  void construct(const std::string &scriptname, const vector<string> &args);
  void cleanup();
  void renameObjects();
  void runPreInit();
  PyObject *initModule();
  ClassData *lookup(const std::string &name);
  bool canConvert(ClassData *from, ClassData *to);

 private:
  ClassData *getOrConstructClass(const string &name);
  void registerBaseclasses();
  void registerDummyTypes();
  void registerMeta();
  void addConstants(PyObject *module);
  void registerOperators(ClassData *cls);
  void addParentMethods(ClassData *cls, ClassData *base);
  WrapperRegistry();
  ~WrapperRegistry();
  std::map<std::string, ClassData *> mClasses;
  std::vector<ClassData *> mClassList;
  std::vector<InitFunc> mExtInitializers;
  std::vector<std::string> mPaths;
  std::string mCode, mScriptName;
  std::vector<std::string> args;
  std::map<std::string, int> mEnumValues;
};

//******************************************************************************
// Callback functions

PyObject *cbGetClass(PbObject *self, void *cl)
{
  return Manta::toPy(self->classdef->cPureName);
}

PyObject *cbGetTemplate(PbObject *self, void *cl)
{
  return Manta::toPy(self->classdef->cTemplate);
}

PyObject *cbGetCName(PbObject *self, void *cl)
{
  return Manta::toPy(self->classdef->cName);
}

void cbDealloc(PbObject *self)
{
  // cout << "dealloc " << self->instance->getName() << " " << self->classdef->cName << endl;
  if (self->instance) {
#ifndef BLENDER
    // don't delete top-level objects
    if (self->instance->getParent() != self->instance)
      delete self->instance;
#else
    // in Blender we *have* to delete all objects
    delete self->instance;
#endif
  }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

PyObject *cbNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  PbObject *self = (PbObject *)type->tp_alloc(type, 0);
  if (self != NULL) {
    // lookup and link classdef
    self->classdef = WrapperRegistry::instance().lookup(type->tp_name);
    self->instance = NULL;
    // cout << "creating " << self->classdef->cName << endl;
  }
  else
    errMsg("can't allocate new python class object");
  return (PyObject *)self;
}

int cbDisableConstructor(PyObject *self, PyObject *args, PyObject *kwds)
{
  errMsg("Can't instantiate a class template without template arguments");
  return -1;
}

PyMODINIT_FUNC PyInit_manta_main(void)
{
  MantaEnsureRegistration();
#if PY_MAJOR_VERSION >= 3
  return WrapperRegistry::instance().initModule();
#else
  WrapperRegistry::instance().initModule();
#endif
}

//******************************************************
// WrapperRegistry

WrapperRegistry::WrapperRegistry()
{
  addClass("__modclass__", "__modclass__", "");
  addClass("PbClass", "PbClass", "");
}

WrapperRegistry::~WrapperRegistry()
{
  // Some static constructions may have called WrapperRegistry.instance() and added
  // own classes, functions, etc. Ensure everything is cleaned up properly.
  cleanup();
}

ClassData *WrapperRegistry::getOrConstructClass(const string &classname)
{
  map<string, ClassData *>::iterator it = mClasses.find(classname);

  if (it != mClasses.end())
    return it->second;
  ClassData *data = new ClassData;
  data->cName = classname;
  data->cPureName = classname;
  data->cTemplate = "";
  size_t tplIdx = classname.find('<');
  if (tplIdx != string::npos) {
    data->cPureName = classname.substr(0, tplIdx);
    data->cTemplate = classname.substr(tplIdx + 1, classname.find('>') - tplIdx - 1);
  }
  data->baseclass = NULL;
  data->constructor = cbDisableConstructor;
  mClasses[classname] = data;
  mClassList.push_back(data);
  return data;
}

void replaceAll(string &source, string const &find, string const &replace)
{
  for (string::size_type i = 0; (i = source.find(find, i)) != std::string::npos;) {
    source.replace(i, find.length(), replace);
    i += replace.length() - find.length() + 1;
  }
}

void WrapperRegistry::addClass(const string &pyName,
                               const string &internalName,
                               const string &baseclass)
{
  ClassData *data = getOrConstructClass(internalName);

  // regularize python name
  string pythonName = pyName;
  replaceAll(pythonName, "<", "_");
  replaceAll(pythonName, ">", "");
  replaceAll(pythonName, ",", "_");

  if (data->pyName.empty())
    data->pyName = pythonName;
  mClasses[pythonName] = data;
  if (!baseclass.empty())
    data->baseclassName = baseclass;
}

void WrapperRegistry::addEnumEntry(const string &name, int value)
{
  /// Gather static definitions to add them as static python objects afterwards
  if (mEnumValues.insert(std::make_pair(name, value)).second == false) {
    errMsg("Enum entry '" + name + "' already existing...");
  }
}

void WrapperRegistry::addExternalInitializer(InitFunc func)
{
  mExtInitializers.push_back(func);
}

void WrapperRegistry::addPythonPath(const string &path)
{
  mPaths.push_back(path);
}

void WrapperRegistry::addPythonCode(const string &file, const string &code)
{
  mCode += code + "\n";
}

void WrapperRegistry::addGetSet(const string &classname,
                                const string &property,
                                Getter getfunc,
                                Setter setfunc)
{
  ClassData *classdef = getOrConstructClass(classname);
  GetSet &def = classdef->getset[property];
  if (def.name.empty()) {
    def.name = property;
    def.doc = property;
  }
  if (getfunc)
    def.getter = getfunc;
  if (setfunc)
    def.setter = setfunc;
}

void WrapperRegistry::addMethod(const string &classname,
                                const string &methodname,
                                GenericFunction func)
{
  string aclass = classname;
  if (aclass.empty())
    aclass = "__modclass__";

  ClassData *classdef = getOrConstructClass(aclass);
  for (int i = 0; i < (int)classdef->methods.size(); i++)
    if (classdef->methods[i].name == methodname)
      return;  // avoid duplicates
  classdef->methods.push_back(Method(methodname, methodname, func));
}

void WrapperRegistry::addOperator(const string &classname,
                                  const string &methodname,
                                  OperatorFunction func)
{
  if (classname.empty())
    errMsg("PYTHON operators have to be defined within classes.");

  string op = methodname.substr(8);
  ClassData *classdef = getOrConstructClass(classname);
  classdef->ops[op] = func;
}

void WrapperRegistry::addConstructor(const string &classname, Constructor func)
{
  ClassData *classdef = getOrConstructClass(classname);
  classdef->constructor = func;
}

void WrapperRegistry::addParentMethods(ClassData *cur, ClassData *base)
{
  if (base == 0)
    return;

  for (vector<Method>::iterator it = base->methods.begin(); it != base->methods.end(); ++it)
    addMethod(cur->cName, it->name, it->func);

  for (map<string, GetSet>::iterator it = base->getset.begin(); it != base->getset.end(); ++it)
    addGetSet(cur->cName, it->first, it->second.getter, it->second.setter);

  for (map<string, OperatorFunction>::iterator it = base->ops.begin(); it != base->ops.end(); ++it)
    cur->ops[it->first] = it->second;

  addParentMethods(cur, base->baseclass);
}

void WrapperRegistry::registerBaseclasses()
{
  for (int i = 0; i < (int)mClassList.size(); i++) {
    string bname = mClassList[i]->baseclassName;
    if (!bname.empty()) {
      mClassList[i]->baseclass = lookup(bname);
      if (!mClassList[i]->baseclass)
        errMsg("Registering class '" + mClassList[i]->cName + "' : Base class '" + bname +
               "' not found");
    }
  }

  for (int i = 0; i < (int)mClassList.size(); i++) {
    addParentMethods(mClassList[i], mClassList[i]->baseclass);
  }
}

void WrapperRegistry::registerMeta()
{
  for (int i = 0; i < (int)mClassList.size(); i++) {
    mClassList[i]->getset["_class"] = GetSet("_class", "C class name", (Getter)cbGetClass, 0);
    mClassList[i]->getset["_cname"] = GetSet("_cname", "Full C name", (Getter)cbGetCName, 0);
    mClassList[i]->getset["_T"] = GetSet("_T", "C template argument", (Getter)cbGetTemplate, 0);
  }
}

void WrapperRegistry::registerOperators(ClassData *cls)
{
  PyNumberMethods &num = cls->numInfo;
  for (map<string, OperatorFunction>::iterator it = cls->ops.begin(); it != cls->ops.end(); it++) {
    const string &op = it->first;
    OperatorFunction func = it->second;
    if (op == "+=")
      num.nb_inplace_add = func;
    else if (op == "-=")
      num.nb_inplace_subtract = func;
    else if (op == "*=")
      num.nb_inplace_multiply = func;
    else if (op == "+")
      num.nb_add = func;
    else if (op == "-")
      num.nb_subtract = func;
    else if (op == "*")
      num.nb_multiply = func;
#if PY_MAJOR_VERSION < 3
    else if (op == "/=")
      num.nb_inplace_divide = func;
    else if (op == "/")
      num.nb_divide = func;
#else
    else if (op == "/=")
      num.nb_inplace_true_divide = func;
    else if (op == "/")
      num.nb_true_divide = func;
#endif
    else
      errMsg("PYTHON operator " + op + " not supported");
  }
}

void WrapperRegistry::registerDummyTypes()
{
  vector<string> add;
  for (vector<ClassData *>::iterator it = mClassList.begin(); it != mClassList.end(); ++it) {
    string cName = (*it)->cName;
    if (cName.find('<') != string::npos)
      add.push_back(cName.substr(0, cName.find('<')));
  }
  for (int i = 0; i < (int)add.size(); i++)
    addClass(add[i], add[i], "");
}

ClassData *WrapperRegistry::lookup(const string &name)
{
  for (map<string, ClassData *>::iterator it = mClasses.begin(); it != mClasses.end(); ++it) {
    if (it->first == name || it->second->cName == name)
      return it->second;
  }
  return NULL;
}

void WrapperRegistry::cleanup()
{
  for (vector<ClassData *>::iterator it = mClassList.begin(); it != mClassList.end(); ++it) {
    delete *it;
  }
  mClasses.clear();
  mClassList.clear();
}

WrapperRegistry &WrapperRegistry::instance()
{
  static WrapperRegistry inst;
  return inst;
}

bool WrapperRegistry::canConvert(ClassData *from, ClassData *to)
{
  if (from == to)
    return true;
  if (from->baseclass)
    return canConvert(from->baseclass, to);
  return false;
}

void WrapperRegistry::addConstants(PyObject *module)
{
  // expose arguments
  PyObject *list = PyList_New(args.size());
  for (int i = 0; i < (int)args.size(); i++)
    PyList_SET_ITEM(list, i, Manta::toPy(args[i]));
  PyModule_AddObject(module, "args", list);
  PyModule_AddObject(module, "SCENEFILE", Manta::toPy(mScriptName));

  // expose compile flags
#ifdef DEBUG
  PyModule_AddObject(module, "DEBUG", Manta::toPy<bool>(true));
#else
  PyModule_AddObject(module, "DEBUG", Manta::toPy<bool>(false));
#endif
#ifdef MANTA_MT
  PyModule_AddObject(module, "MT", Manta::toPy<bool>(true));
#else
  PyModule_AddObject(module, "MT", Manta::toPy<bool>(false));
#endif
#ifdef GUI
  PyModule_AddObject(module, "GUI", Manta::toPy<bool>(true));
#else
  PyModule_AddObject(module, "GUI", Manta::toPy<bool>(false));
#endif
#if FLOATINGPOINT_PRECISION == 2
  PyModule_AddObject(module, "DOUBLEPRECISION", Manta::toPy<bool>(true));
#else
  PyModule_AddObject(module, "DOUBLEPRECISION", Manta::toPy<bool>(false));
#endif
  // cuda off for now
  PyModule_AddObject(module, "CUDA", Manta::toPy<bool>(false));

  // expose enum entries
  std::map<std::string, int>::iterator it;
  for (it = mEnumValues.begin(); it != mEnumValues.end(); it++) {
    PyModule_AddObject(module, it->first.c_str(), Manta::toPy(it->second));
    // Alternative would be:
    // e.g. PyModule_AddIntConstant(module, "FlagFluid", 1);
  }
}

void WrapperRegistry::runPreInit()
{
  // add python directories to path
  PyObject *sys_path = PySys_GetObject((char *)"path");
  for (size_t i = 0; i < mPaths.size(); i++) {
    PyObject *path = Manta::toPy(mPaths[i]);
    if (sys_path == NULL || path == NULL || PyList_Append(sys_path, path) < 0) {
      errMsg("unable to set python path");
    }
    Py_DECREF(path);
  }
  if (!mCode.empty()) {
    mCode = "from manta import *\n" + mCode;
    PyRun_SimpleString(mCode.c_str());
  }
}

PyObject *WrapperRegistry::createPyObject(const string &classname,
                                          const string &name,
                                          Manta::PbArgs &args,
                                          Manta::PbClass *parent)
{
  ClassData *classdef = lookup(classname);
  if (!classdef)
    errMsg("Class " + classname + " doesn't exist.");

  // create object
  PyObject *obj = cbNew(&classdef->typeInfo, NULL, NULL);
  PbObject *self = (PbObject *)obj;
  PyObject *nkw = 0;

  if (args.kwds())
    nkw = PyDict_Copy(args.kwds());
  else
    nkw = PyDict_New();

  PyObject *nocheck = Py_BuildValue("s", "yes");
  PyDict_SetItemString(nkw, "nocheck", nocheck);
  if (parent)
    PyDict_SetItemString(nkw, "parent", parent->getPyObject());

  // create instance
  if (self->classdef->constructor(obj, args.linArgs(), nkw) < 0)
    errMsg("error raised in constructor");  // assume condition is already set

  Py_DECREF(nkw);
  Py_DECREF(nocheck);
  self->instance->setName(name);

  return obj;
}

// prepare typeinfo and register python module
void WrapperRegistry::construct(const string &scriptname, const vector<string> &args)
{
  mScriptName = scriptname;
  this->args = args;

  registerBaseclasses();
  registerMeta();
  registerDummyTypes();

  // work around for certain gcc versions, cast to char*
  PyImport_AppendInittab((char *)gDefaultModuleName.c_str(), PyInit_manta_main);
}

inline PyObject *castPy(PyTypeObject *p)
{
  return reinterpret_cast<PyObject *>(static_cast<void *>(p));
}

PyObject *WrapperRegistry::initModule()
{
  // generate and terminate all method lists
  PyMethodDef sentinelFunc = {NULL, NULL, 0, NULL};
  PyGetSetDef sentinelGetSet = {NULL, NULL, NULL, NULL, NULL};
  for (int i = 0; i < (int)mClassList.size(); i++) {
    ClassData *cls = mClassList[i];
    cls->genMethods.clear();
    cls->genGetSet.clear();
    for (vector<Method>::iterator i2 = cls->methods.begin(); i2 != cls->methods.end(); ++i2)
      cls->genMethods.push_back(i2->def());
    for (map<string, GetSet>::iterator i2 = cls->getset.begin(); i2 != cls->getset.end(); ++i2)
      cls->genGetSet.push_back(i2->second.def());

    cls->genMethods.push_back(sentinelFunc);
    cls->genGetSet.push_back(sentinelGetSet);
  }

  // prepare module info
#if PY_MAJOR_VERSION >= 3
  static PyModuleDef MainModule = {PyModuleDef_HEAD_INIT,
                                   gDefaultModuleName.c_str(),
                                   "Bridge module to the C++ solver",
                                   -1,
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL};
  // get generic methods (plugin functions)
  MainModule.m_methods = &mClasses["__modclass__"]->genMethods[0];

  // create module
  PyObject *module = PyModule_Create(&MainModule);
#else
  PyObject *module = Py_InitModule(gDefaultModuleName.c_str(),
                                   &mClasses["__modclass__"]->genMethods[0]);
#endif
  if (module == NULL)
    return NULL;

  // load classes
  for (vector<ClassData *>::iterator it = mClassList.begin(); it != mClassList.end(); ++it) {
    ClassData &data = **it;
    char *nameptr = (char *)data.pyName.c_str();

    // define numeric substruct
    PyNumberMethods *num = 0;
    if (!data.ops.empty()) {
      num = &data.numInfo;
      memset(num, 0, sizeof(PyNumberMethods));
      registerOperators(&data);
    }

    // define python classinfo
    PyTypeObject t = {
        PyVarObject_HEAD_INIT(NULL, 0)(char *) data.pyName.c_str(),  // tp_name
        sizeof(PbObject),                                            // tp_basicsize
        0,                                                           // tp_itemsize
        (destructor)cbDealloc,                                       // tp_dealloc
        0,                                                           // tp_print
        0,                                                           // tp_getattr
        0,                                                           // tp_setattr
        0,                                                           // tp_reserved
        0,                                                           // tp_repr
        num,                                                         // tp_as_number
        0,                                                           // tp_as_sequence
        0,                                                           // tp_as_mapping
        0,                                                           // tp_hash
        0,                                                           // tp_call
        0,                                                           // tp_str
        0,                                                           // tp_getattro
        0,                                                           // tp_setattro
        0,                                                           // tp_as_buffer
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,                    // tp_flags
        nameptr,                                                     // tp_doc
        0,                                                           // tp_traverse
        0,                                                           // tp_clear
        0,                                                           // tp_richcompare
        0,                                                           // tp_weaklistoffset
        0,                                                           // tp_iter
        0,                                                           // tp_iternext
        &data.genMethods[0],                                         // tp_methods
        0,                                                           // tp_members
        &data.genGetSet[0],                                          // tp_getset
        0,                                                           // tp_base
        0,                                                           // tp_dict
        0,                                                           // tp_descr_get
        0,                                                           // tp_descr_set
        0,                                                           // tp_dictoffset
        (initproc)(data.constructor),                                // tp_init
        0,                                                           // tp_alloc
        cbNew                                                        // tp_new
    };
    data.typeInfo = t;

    if (PyType_Ready(&data.typeInfo) < 0)
      continue;

    for (map<string, ClassData *>::iterator i2 = mClasses.begin(); i2 != mClasses.end(); ++i2) {
      if (*it != i2->second)
        continue;
      // register all aliases
      Py_INCREF(castPy(&data.typeInfo));
      PyModule_AddObject(module, (char *)i2->first.c_str(), (PyObject *)&data.typeInfo);
    }
  }

  // externals
  for (vector<InitFunc>::iterator it = mExtInitializers.begin(); it != mExtInitializers.end();
       ++it) {
    (*it)(module);
  }

  addConstants(module);

  return module;
}

//******************************************************
// Register members and exposed functions

void setup(const std::string &filename, const std::vector<std::string> &args)
{
  WrapperRegistry::instance().construct(filename, args);
  Py_Initialize();
  WrapperRegistry::instance().runPreInit();
}

void finalize()
{
  Py_Finalize();
  WrapperRegistry::instance().cleanup();
}

bool canConvert(PyObject *obj, const string &classname)
{
  ClassData *from = ((PbObject *)obj)->classdef;
  ClassData *dest = WrapperRegistry::instance().lookup(classname);
  if (!dest)
    errMsg("Classname '" + classname + "' is not registered.");
  return WrapperRegistry::instance().canConvert(from, dest);
}

Manta::PbClass *objFromPy(PyObject *obj)
{
  if (Py_TYPE(obj)->tp_dealloc != (destructor)cbDealloc)  // not a manta object
    return NULL;

  return ((PbObject *)obj)->instance;
}

PyObject *copyObject(Manta::PbClass *cls, const string &classname)
{
  ClassData *classdef = WrapperRegistry::instance().lookup(classname);
  assertMsg(classdef, "python class " + classname + " does not exist.");

  // allocate new object
  PbObject *obj = (PbObject *)classdef->typeInfo.tp_alloc(&(classdef->typeInfo), 0);
  assertMsg(obj, "cannot allocate new python object");

  obj->classdef = classdef;
  cls->registerObject((PyObject *)obj, 0);

  return cls->getPyObject();
}

Manta::PbClass *createPy(const std::string &classname,
                         const std::string &name,
                         Manta::PbArgs &args,
                         Manta::PbClass *parent)
{
  PyObject *obj = WrapperRegistry::instance().createPyObject(classname, name, args, parent);
  return ((PbObject *)obj)->instance;
}

void setReference(Manta::PbClass *cls, PyObject *obj)
{
  ((PbObject *)obj)->instance = cls;
}

Register::Register(const string &className, const string &funcName, GenericFunction func)
{
  WrapperRegistry::instance().addMethod(className, funcName, func);
}
Register::Register(const string &className, const string &funcName, OperatorFunction func)
{
  WrapperRegistry::instance().addOperator(className, funcName, func);
}
Register::Register(const string &className, const string &funcName, Constructor func)
{
  WrapperRegistry::instance().addConstructor(className, func);
}
Register::Register(const string &className, const string &property, Getter getter, Setter setter)
{
  WrapperRegistry::instance().addGetSet(className, property, getter, setter);
}
Register::Register(const string &className, const string &pyName, const string &baseClass)
{
  WrapperRegistry::instance().addClass(pyName, className, baseClass);
}
Register::Register(const string &name, const int value)
{
  WrapperRegistry::instance().addEnumEntry(name, value);
}
Register::Register(const string &file, const string &pythonCode)
{
  WrapperRegistry::instance().addPythonCode(file, pythonCode);
}
Register::Register(InitFunc func)
{
  WrapperRegistry::instance().addExternalInitializer(func);
}

}  // namespace Pb
