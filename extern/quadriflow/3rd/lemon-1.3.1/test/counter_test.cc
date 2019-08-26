/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2009
 * Egervary Jeno Kombinatorikus Optimalizalasi Kutatocsoport
 * (Egervary Research Group on Combinatorial Optimization, EGRES).
 *
 * Permission to use, modify and distribute this software is granted
 * provided that this copyright notice appears in all copies. For
 * precise terms see the accompanying LICENSE file.
 *
 * This software is provided "AS IS" with no warranty of any kind,
 * express or implied, and with no claim as to its suitability for any
 * purpose.
 *
 */

#include <lemon/counter.h>
#include <vector>
#include <sstream>

#include "test/test_tools.h"

using namespace lemon;

template <typename T>
void bubbleSort(std::vector<T>& v) {
  std::stringstream s1, s2, s3;
  {
    Counter op("Bubble Sort - Operations: ", s1);
    Counter::SubCounter as(op, "Assignments: ", s2);
    Counter::SubCounter co(op, "Comparisons: ", s3);
    for (int i = v.size()-1; i > 0; --i) {
      for (int j = 0; j < i; ++j) {
        if (v[j] > v[j+1]) {
          T tmp = v[j];
          v[j] = v[j+1];
          v[j+1] = tmp;
          as += 3;
        }
        ++co;
      }
    }
  }
  check(s1.str() == "Bubble Sort - Operations: 102\n", "Wrong counter");
  check(s2.str() == "Assignments: 57\n", "Wrong subcounter");
  check(s3.str() == "Comparisons: 45\n", "Wrong subcounter");
}

template <typename T>
void insertionSort(std::vector<T>& v) {
  std::stringstream s1, s2, s3;
  {
    Counter op("Insertion Sort - Operations: ", s1);
    Counter::SubCounter as(op, "Assignments: ", s2);
    Counter::SubCounter co(op, "Comparisons: ", s3);
    for (int i = 1; i < int(v.size()); ++i) {
      T value = v[i];
      ++as;
      int j = i;
      while (j > 0 && v[j-1] > value) {
        v[j] = v[j-1];
        --j;
        ++co; ++as;
      }
      v[j] = value;
      ++as;
    }
  }
  check(s1.str() == "Insertion Sort - Operations: 56\n", "Wrong counter");
  check(s2.str() == "Assignments: 37\n", "Wrong subcounter");
  check(s3.str() == "Comparisons: 19\n", "Wrong subcounter");
}

template <typename MyCounter>
void counterTest(bool output) {
  std::stringstream s1, s2, s3;
  {
    MyCounter c("Main Counter: ", s1);
    c++;
    typename MyCounter::SubCounter d(c, "SubCounter: ", s2);
    d++;
    typename MyCounter::SubCounter::NoSubCounter e(d, "SubSubCounter: ", s3);
    e++;
    d+=3;
    c-=4;
    e-=2;
    c.reset(2);
    c.reset();
  }
  if (output) {
    check(s1.str() == "Main Counter: 3\n", "Wrong Counter");
    check(s2.str() == "SubCounter: 3\n", "Wrong SubCounter");
    check(s3.str() == "", "Wrong NoSubCounter");
  } else {
    check(s1.str() == "", "Wrong NoCounter");
    check(s2.str() == "", "Wrong SubCounter");
    check(s3.str() == "", "Wrong NoSubCounter");
  }
}

void init(std::vector<int>& v) {
  v[0] = 10; v[1] = 60; v[2] = 20; v[3] = 90; v[4] = 100;
  v[5] = 80; v[6] = 40; v[7] = 30; v[8] = 50; v[9] = 70;
}

int main()
{
  counterTest<Counter>(true);
  counterTest<NoCounter>(false);

  std::vector<int> x(10);
  init(x); bubbleSort(x);
  init(x); insertionSort(x);

  return 0;
}
