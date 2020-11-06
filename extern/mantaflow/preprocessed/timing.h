

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
 * Plugin timing
 *
 ******************************************************************************/

#ifndef _TIMING_H
#define _TIMING_H

#include "manta.h"
#include <map>
namespace Manta {

class TimingData {
 private:
  TimingData();

 public:
  static TimingData &instance()
  {
    static TimingData a;
    return a;
  }

  void print();
  void saveMean(const std::string &filename);
  void start(FluidSolver *parent, const std::string &name);
  void stop(FluidSolver *parent, const std::string &name);

 protected:
  void step();
  struct TimingSet {
    TimingSet() : num(0), updated(false)
    {
      cur.clear();
      total.clear();
    }
    MuTime cur, total;
    int num;
    bool updated;
    std::string solver;
  };
  bool updated;

  int num;
  MuTime mPluginTimer;
  std::string mLastPlugin;
  std::map<std::string, std::vector<TimingSet>> mData;
};

// Python interface
class Timings : public PbClass {
 public:
  Timings() : PbClass(0)
  {
  }
  static int _W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    PbClass *obj = Pb::objFromPy(_self);
    if (obj)
      delete obj;
    try {
      PbArgs _args(_linargs, _kwds);
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(0, "Timings::Timings", !noTiming);
      {
        ArgLocker _lock;
        obj = new Timings();
        obj->registerObject(_self, &_args);
        _args.check();
      }
      pbFinalizePlugin(obj->getParent(), "Timings::Timings", !noTiming);
      return 0;
    }
    catch (std::exception &e) {
      pbSetError("Timings::Timings", e.what());
      return -1;
    }
  }

  void display()
  {
    TimingData::instance().print();
  }
  static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Timings *pbo = dynamic_cast<Timings *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Timings::display", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->display();
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Timings::display", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Timings::display", e.what());
      return 0;
    }
  }
  void saveMean(std::string file)
  {
    TimingData::instance().saveMean(file);
  }
  static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
  {
    try {
      PbArgs _args(_linargs, _kwds);
      Timings *pbo = dynamic_cast<Timings *>(Pb::objFromPy(_self));
      bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
      pbPreparePlugin(pbo->getParent(), "Timings::saveMean", !noTiming);
      PyObject *_retval = nullptr;
      {
        ArgLocker _lock;
        std::string file = _args.get<std::string>("file", 0, &_lock);
        pbo->_args.copy(_args);
        _retval = getPyNone();
        pbo->saveMean(file);
        pbo->_args.check();
      }
      pbFinalizePlugin(pbo->getParent(), "Timings::saveMean", !noTiming);
      return _retval;
    }
    catch (std::exception &e) {
      pbSetError("Timings::saveMean", e.what());
      return 0;
    }
  }

 public:
  PbArgs _args;
}
#define _C_Timings
;

}  // namespace Manta

#endif
