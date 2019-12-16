

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

#include "timing.h"
#include <fstream>

using namespace std;
namespace Manta {

TimingData::TimingData() : updated(false), num(0)
{
}

void TimingData::start(FluidSolver *parent, const string &name)
{
  mLastPlugin = name;
  mPluginTimer.get();
}

void TimingData::stop(FluidSolver *parent, const string &name)
{
  if (mLastPlugin == name && name != "FluidSolver::step") {
    updated = true;
    const string parentName = parent ? parent->getName() : "";
    MuTime diff = mPluginTimer.update();
    vector<TimingSet> &cur = mData[name];
    for (vector<TimingSet>::iterator it = cur.begin(); it != cur.end(); it++) {
      if (it->solver == parentName) {
        it->cur += diff;
        it->updated = true;
        return;
      }
    }
    TimingSet s;
    s.solver = parentName;
    s.cur = diff;
    s.updated = true;
    cur.push_back(s);
  }
}

void TimingData::step()
{
  if (updated)
    num++;
  std::map<std::string, std::vector<TimingSet>>::iterator it;
  for (it = mData.begin(); it != mData.end(); it++) {
    for (vector<TimingSet>::iterator it2 = it->second.begin(); it2 != it->second.end(); it2++) {
      if (it2->updated) {
        it2->total += it2->cur;
        it2->num++;
      }
      it2->cur.clear();
      it2->updated = false;
    }
  }
  updated = false;
}

void TimingData::print()
{
  MuTime total;
  total.clear();
  std::map<std::string, std::vector<TimingSet>>::iterator it;
  for (it = mData.begin(); it != mData.end(); it++)
    for (vector<TimingSet>::iterator it2 = it->second.begin(); it2 != it->second.end(); it2++)
      total += it2->cur;

  printf("\n-- STEP %3d ----------------------------\n", num);
  for (it = mData.begin(); it != mData.end(); it++) {
    for (vector<TimingSet>::iterator it2 = it->second.begin(); it2 != it->second.end(); it2++) {
      if (!it2->updated)
        continue;
      string name = it->first;
      if (it->second.size() > 1 && !it2->solver.empty())
        name += "[" + it2->solver + "]";
      printf("[%4.1f%%] %s (%s)\n",
             100.0 * ((Real)it2->cur.time / (Real)total.time),
             name.c_str(),
             it2->cur.toString().c_str());
    }
  }
  step();

  printf("----------------------------------------\n");
  printf("Total : %s\n\n", total.toString().c_str());
}

void TimingData::saveMean(const string &filename)
{
  ofstream ofs(filename.c_str());
  step();
  if (!ofs.good())
    errMsg("can't open " + filename + " as timing log");
  ofs << "Mean timings of " << num << " steps :" << endl << endl;
  MuTime total;
  total.clear();
  std::map<std::string, std::vector<TimingSet>>::iterator it;
  for (it = mData.begin(); it != mData.end(); it++)
    for (vector<TimingSet>::iterator it2 = it->second.begin(); it2 != it->second.end(); it2++) {
      total += it2->cur;
      string name = it->first;
      if (it->second.size() > 1)
        name += "[" + it2->solver + "]";

      ofs << name << " " << (it2->total / it2->num) << endl;
    }

  ofs << endl << "Total : " << total << " (mean " << total / num << ")" << endl;
  ofs.close();
}

}  // namespace Manta
