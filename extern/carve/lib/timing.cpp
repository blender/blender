// Begin License:
// Copyright (C) 2006-2011 Tobias Sargeant (tobias.sargeant@gmail.com).
// All rights reserved.
//
// This file is part of the Carve CSG Library (http://carve-csg.com/)
//
// This file may be used under the terms of the GNU General Public
// License version 2.0 as published by the Free Software Foundation
// and appearing in the file LICENSE.GPL2 included in the packaging of
// this file.
//
// This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
// INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE.
// End:


#if CARVE_USE_TIMINGS

#if defined(HAVE_CONFIG_H)
#  include <carve_config.h>
#endif

#include <carve/timing.hpp>

#include <cstring>
#include <list>
#include <stack>
#include <vector>
#include <map>
#include <iostream>
#include <string>
#include <algorithm>

#ifdef WIN32
#include <windows.h>
#else 
#include <time.h>
#include <sys/time.h>
#endif

#ifndef CARVE_USE_GLOBAL_NEW_DELETE
#define CARVE_USE_GLOBAL_NEW_DELETE 0
#endif

namespace carve {
  static uint64_t memoryCurr = 0;
  static uint64_t memoryTotal = 0;
  unsigned blkCntCurr[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
  unsigned blkCntTotal[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

  void addBlk(unsigned size) {
    unsigned i = 0;
    while (i < 31 && (1U<<i) < size) ++i;
    blkCntCurr[i]++;
    blkCntTotal[i]++;
  }
  void remBlk(unsigned size) {
    unsigned i = 0;
    while (i < 31 && (1<<i) < size) ++i;
    blkCntCurr[i]--;
  }
}

// lets provide a global new and delete as well

#if CARVE_USE_GLOBAL_NEW_DELETE

#if defined(__APPLE__)

#include <stdlib.h>
#include <malloc/malloc.h>

void* carve_alloc(size_t size) {
  void *p = malloc(size); 
  if (p == 0) throw std::bad_alloc(); // ANSI/ISO compliant behavior

  unsigned sz = malloc_size(p);
  carve::memoryCurr += sz;
  carve::memoryTotal += sz;
  carve::addBlk(sz);
  return p;
}

void carve_free(void *p) {
  unsigned sz = malloc_size(p);
  carve::memoryCurr -= sz;
  carve::remBlk(sz);
  free(p); 
}

#else

void* carve_alloc(size_t size) {
  void *p = malloc(size + 4); 
  if (p == 0) throw std::bad_alloc(); // ANSI/ISO compliant behavior

  int *sizePtr = (int*)p;
  *sizePtr = size;
  ++sizePtr;
  carve::memoryCurr += size;
  carve::memoryTotal += size;
  carve::addBlk(size);
  return sizePtr;
}

void carve_free(void *p) {
  // our memory block is actually a size of an int behind this pointer.
  int *sizePtr = (int*)p;

  --sizePtr;

  carve::memoryCurr -= *sizePtr;
  int size = *sizePtr;
  carve::remBlk(size);
  free(sizePtr); 
}

#endif


void* operator new (size_t size) {
  return carve_alloc(size);
}

void* operator new[](size_t size) {
  return carve_alloc(size);
}


void operator delete (void *p) {
  carve_free(p);
}

void operator delete[](void *p) {
  carve_free(p);
}

#endif

namespace carve {



#ifdef WIN32

  typedef __int64 precise_time_t;
  
  precise_time_t g_frequency;
  
  void initTime() {
    ::QueryPerformanceFrequency((LARGE_INTEGER*)&g_frequency);  
  }
  
  void getTime(precise_time_t &t) {
    ::QueryPerformanceCounter((LARGE_INTEGER*)&t);
  }
  
  double diffTime(precise_time_t from, precise_time_t to) {
    return (double)(to - from) / (double)g_frequency;
  }
  
#else 

  typedef double precise_time_t;

  void initTime() {
  }

  void getTime(precise_time_t &t) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    t = tv.tv_sec + tv.tv_usec / 1000000.0;
  }

  double diffTime(precise_time_t from, precise_time_t to) {
    return to - from;
  }

#endif

  struct Entry {
    Entry(int _id) {
      id = _id;
      time = 0;
      parent = NULL;
    }
    int id;
    double time;
    int64_t memoryDiff;
    int64_t allocTotal;
    int delta_blk_cnt_curr[32];
    int delta_blk_cnt_total[32];
    Entry *parent;
    std::vector<Entry *> children;
  };

  struct Timer {
    struct cmp {
      bool operator()(const std::pair<int, double> &a, const std::pair<int, double> &b) const {
        return b.second < a.second;
      }
      bool operator()(const Entry * const &a, const Entry * const &b) const {
        return b->time < a->time;
      }
    };

    Timer() {
      initTime();
    }

    struct Snapshot {
      precise_time_t time;
      uint64_t memory_curr;
      uint64_t memory_total;
      unsigned blk_cnt_curr[32];
      unsigned blk_cnt_total[32];
    };
    
    static void getSnapshot(Snapshot &snapshot) {
      getTime(snapshot.time);
      snapshot.memory_curr = carve::memoryCurr;
      snapshot.memory_total = carve::memoryTotal;
      std::memcpy(snapshot.blk_cnt_curr, carve::blkCntCurr, sizeof(carve::blkCntCurr));
      std::memcpy(snapshot.blk_cnt_total, carve::blkCntTotal, sizeof(carve::blkCntTotal));
    }

    static void compareSnapshot(const Snapshot &from, const Snapshot &to, Entry *entry) {
      entry->time = diffTime(from.time, to.time);
      entry->memoryDiff = to.memory_curr - from.memory_curr;
      entry->allocTotal = to.memory_total - from.memory_total;
      for (int i = 0; i < 32; i++) {
        entry->delta_blk_cnt_curr[i] = to.blk_cnt_curr[i] - from.blk_cnt_curr[i];
        entry->delta_blk_cnt_total[i] = to.blk_cnt_total[i] - from.blk_cnt_total[i];
      }
    }
    
    std::stack<std::pair<Entry*, Snapshot> > currentTimers;
    
    void startTiming(int id) {
      entries.push_back(Entry(id)); 
      currentTimers.push(std::make_pair(&entries.back(), Snapshot()));  
      getSnapshot(currentTimers.top().second);
    }
    
    double endTiming() {
      Snapshot end;
      getSnapshot(end);

      Entry *entry = currentTimers.top().first;
      compareSnapshot(currentTimers.top().second, end, entry);
      
      currentTimers.pop();
      if (!currentTimers.empty()) {
        entry->parent = currentTimers.top().first;
        entry->parent->children.push_back(entry);
      } else {
        root_entries.push_back(entry);
      }
      //std::sort(entry->children.begin(), entry->children.end(), cmp());
      return entry->time;
    }

    typedef std::list<Entry> EntryList;
    EntryList entries;
    std::vector<Entry *> root_entries;

    std::map<int, std::string> names;
    
    static std::string formatMemory(int64_t value) {
      
      std::ostringstream result;

      result << (value >= 0 ? "+" : "-");
      if (value < 0) {
        value = -value;
      }
      
      int power = 1;
      while (value > pow(10.0, power)) {
        power++;
      }
      
      for (power--; power >= 0; power--) {
        int64_t base = pow(10.0, power);
        int64_t amount = value / base;
        result <<
#if defined(_MSC_VER) && _MSC_VER < 1300
          (long)
#endif
          amount;
        if (power > 0 && (power % 3) == 0) {
          result << ",";
        }
        value -= amount * base;
      }
      
      result << " bytes";

      return result.str();
    }

    void printEntries(std::ostream &o, const std::vector<Entry *> &entries, const std::string &indent, double parent_time) {
      if (parent_time <= 0.0) {
        parent_time = 0.0;
        for (size_t i = 0; i < entries.size(); ++i) {
          parent_time += entries[i]->time;
        }
      }
      double t_tot = 0.0;
      for (size_t i = 0; i < entries.size(); ++i) {
        const Entry *entry = entries[i];

        std::ostringstream r;
        r << indent;
        std::string str = names[entry->id];
        if (str.empty()) {
          r << "(" << entry->id << ")";
        } else {
          r << str;
        }
        r << " ";
        std::string pad(r.str().size(), ' ');
        r << " - exectime: " << entry->time << "s (" << (entry->time * 100.0 / parent_time) << "%)" << std::endl;
        if (entry->allocTotal || entry->memoryDiff) {
          r << pad << " - alloc: " << formatMemory(entry->allocTotal) << " delta: " << formatMemory(entry->memoryDiff) << std::endl;
          r << pad << " - alloc blks:";
          for (int i = 0; i < 32; i++) { if (entry->delta_blk_cnt_total[i]) r << ' ' << ((1 << (i - 1)) + 1) << '-' << (1 << i) << ':' << entry->delta_blk_cnt_total[i]; }
          r << std::endl;
          r << pad << " - delta blks:";
          for (int i = 0; i < 32; i++) { if (entry->delta_blk_cnt_curr[i]) r << ' ' << ((1 << (i - 1)) + 1) << '-' << (1 << i) << ':' << entry->delta_blk_cnt_curr[i]; }
          r << std::endl;
        }
        o << r.str();
        t_tot += entry->time;
        if (entry->children.size()) printEntries(o, entry->children, indent + "   ", entry->time);
      }
      if (t_tot < parent_time) {
        o << indent << "*** unaccounted: " << (parent_time - t_tot) << "s   (" << (100.0 - t_tot * 100.0 / parent_time) << "%)" << std::endl;
      }
    }

    void print() {
      std::map<int, double> totals;
      std::cerr << "Timings: " << std::endl;
      // print out all the entries.

      //std::sort(root_entries.begin(), root_entries.end(), cmp());

      printEntries(std::cerr, root_entries, "   ", -1.0);

      for (EntryList::const_iterator it = entries.begin(); it != entries.end(); ++it) {
        totals[(*it).id] += (*it).time;
      }

      std::cerr << std::endl;
      std::cerr << "Totals: " << std::endl;

      std::vector<std::pair<int, double> > sorted_totals;
      sorted_totals.reserve(totals.size());
      for (std::map<int,double>::iterator it = totals.begin(); it != totals.end(); ++it) {
        sorted_totals.push_back(*it);
      }

      std::sort(sorted_totals.begin(), sorted_totals.end(), cmp());

      for (std::vector<std::pair<int,double> >::iterator it = sorted_totals.begin(); it != sorted_totals.end(); ++it) {
        std::cerr << "  ";
        std::string str = names[it->first];
        if (str.empty()) {
          std::cerr << "(" << it->first << ")";
        } else {
          std::cerr << str;
        }
        std::cerr << " - " << it->second << "s " << std::endl;
      }
    }
    void registerID(int id, const char *name) {
      names[id] = name;
    }
    int registerID(const char *name) {
      int id = names.size() + 1;
      names[id] = name;
      return id;
    }

  };

  Timer timer;


  TimingBlock::TimingBlock(int id) {
#if CARVE_USE_TIMINGS
    timer.startTiming(id);
#endif
  }

  TimingBlock::TimingBlock(const TimingName &name) {
#if CARVE_USE_TIMINGS
    timer.startTiming(name.id);
#endif
  }

  
  TimingBlock::~TimingBlock() {
#if CARVE_USE_TIMINGS
    timer.endTiming();
#endif
  }
  void Timing::start(int id) {
#if CARVE_USE_TIMINGS
    timer.startTiming(id);
#endif
  }
  
  double Timing::stop() {
#if CARVE_USE_TIMINGS
    return timer.endTiming();
#endif
  }
  
  void Timing::printTimings() {
    timer.print();
  }
  
  void Timing::registerID(int id, const char *name) {
    timer.registerID(id, name);
  }
 
  TimingName::TimingName(const char *name) {
    id = timer.registerID(name);
  }

}

#endif
