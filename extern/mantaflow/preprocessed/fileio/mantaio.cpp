

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2020 Sebastian Barschkis, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * General functions that make use of functions from other io files.
 *
 ******************************************************************************/

#include "mantaio.h"

using namespace std;

namespace Manta {

int load(const string &name, std::vector<PbClass *> &objects, float worldSize = 1.0)
{
  if (name.find_last_of('.') == string::npos)
    errMsg("file '" + name + "' does not have an extension");
  string ext = name.substr(name.find_last_of('.'));

  if (ext == ".raw")
    return readGridsRaw(name, &objects);
  else if (ext == ".uni")
    return readGridsUni(name, &objects);
  else if (ext == ".vol")
    return readGridsVol(name, &objects);
  if (ext == ".vdb")
    return readObjectsVDB(name, &objects, worldSize);
  else if (ext == ".npz")
    return readGridsNumpy(name, &objects);
  else if (ext == ".txt")
    return readGridsTxt(name, &objects);
  else
    errMsg("file '" + name + "' filetype not supported");
  return 0;
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "load", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const string &name = _args.get<string>("name", 0, &_lock);
      std::vector<PbClass *> &objects = *_args.getPtr<std::vector<PbClass *>>(
          "objects", 1, &_lock);
      float worldSize = _args.getOpt<float>("worldSize", 2, 1.0, &_lock);
      _retval = toPy(load(name, objects, worldSize));
      _args.check();
    }
    pbFinalizePlugin(parent, "load", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("load", e.what());
    return 0;
  }
}
static const Pb::Register _RP_load("", "load", _W_0);
extern "C" {
void PbRegister_load()
{
  KEEP_UNUSED(_RP_load);
}
}

int save(const string &name,
         std::vector<PbClass *> &objects,
         float worldSize = 1.0,
         bool skipDeletedParts = false,
         int compression = COMPRESSION_ZIP,
         bool precisionHalf = true)
{
  if (name.find_last_of('.') == string::npos)
    errMsg("file '" + name + "' does not have an extension");
  string ext = name.substr(name.find_last_of('.'));

  if (ext == ".raw")
    return writeGridsRaw(name, &objects);
  else if (ext == ".uni")
    return writeGridsUni(name, &objects);
  else if (ext == ".vol")
    return writeGridsVol(name, &objects);
  if (ext == ".vdb")
    return writeObjectsVDB(
        name, &objects, worldSize, skipDeletedParts, compression, precisionHalf);
  else if (ext == ".npz")
    return writeGridsNumpy(name, &objects);
  else if (ext == ".txt")
    return writeGridsTxt(name, &objects);
  else
    errMsg("file '" + name + "' filetype not supported");
  return 0;
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "save", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const string &name = _args.get<string>("name", 0, &_lock);
      std::vector<PbClass *> &objects = *_args.getPtr<std::vector<PbClass *>>(
          "objects", 1, &_lock);
      float worldSize = _args.getOpt<float>("worldSize", 2, 1.0, &_lock);
      bool skipDeletedParts = _args.getOpt<bool>("skipDeletedParts", 3, false, &_lock);
      int compression = _args.getOpt<int>("compression", 4, COMPRESSION_ZIP, &_lock);
      bool precisionHalf = _args.getOpt<bool>("precisionHalf", 5, true, &_lock);
      _retval = toPy(save(name, objects, worldSize, skipDeletedParts, compression, precisionHalf));
      _args.check();
    }
    pbFinalizePlugin(parent, "save", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("save", e.what());
    return 0;
  }
}
static const Pb::Register _RP_save("", "save", _W_1);
extern "C" {
void PbRegister_save()
{
  KEEP_UNUSED(_RP_save);
}
}

}  // namespace Manta
