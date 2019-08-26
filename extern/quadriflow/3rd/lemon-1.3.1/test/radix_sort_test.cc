/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2013
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

#include <lemon/time_measure.h>
#include <lemon/smart_graph.h>
#include <lemon/maps.h>
#include <lemon/radix_sort.h>
#include <lemon/math.h>

#include "test_tools.h"

#include <vector>
#include <list>
#include <algorithm>

using namespace lemon;

static const int n = 10000;

struct Negate {
  typedef int argument_type;
  typedef int result_type;
  int operator()(int a) { return - a; }
};

int negate(int a) { return - a; }

template<class T>
bool isTheSame(T &a, T&b)
{
  typename T::iterator ai=a.begin();
  typename T::iterator bi=b.begin();
  for(;ai!=a.end()||bi!=b.end();++ai,++bi)
    if(*ai!=*bi) return false;
  return ai==a.end()&&bi==b.end();
}

template<class T>
T listsort(typename T::iterator b, typename T::iterator e)
{
  if(b==e) return T();
  typename T::iterator bn=b;
  if(++bn==e) {
    T l;
    l.push_back(*b);
    return l;
  }
  typename T::iterator m=b;
  bool x=false;
  for(typename T::iterator i=b;i!=e;++i,x=!x)
    if(x) ++m;
  T l1(listsort<T>(b,m));
  T l2(listsort<T>(m,e));
  T l;
  while((!l1.empty())&&(!l2.empty()))
    if(l1.front()<=l2.front())
      {
        l.push_back(l1.front());
        l1.pop_front();
      }
    else {
      l.push_back(l2.front());
      l2.pop_front();
    }
  while(!l1.empty())
    {
      l.push_back(l1.front());
      l1.pop_front();
    }
  while(!l2.empty())
    {
      l.push_back(l2.front());
      l2.pop_front();
    }
  return l;
}

template<class T>
void generateIntSequence(int n, T & data) {
  int prime = 9973;
  int root = 136, value = 1;
  for (int i = 0; i < n; ++i) {
    data.push_back(value - prime / 2);
    value = (value * root) % prime;
  }
}

template<class T>
void generateCharSequence(int n, T & data) {
  int prime = 251;
  int root = 3, value = root;
  for (int i = 0; i < n; ++i) {
    data.push_back(static_cast<unsigned char>(value));
    value = (value * root) % prime;
  }
}

void checkRadixSort() {
  {
    std::vector<int> data1;
    generateIntSequence(n, data1);

    std::vector<int> data2(data1);
    std::sort(data1.begin(), data1.end());

    radixSort(data2.begin(), data2.end());
    for (int i = 0; i < n; ++i) {
      check(data1[i] == data2[i], "Test failed");
    }

    // radixSort(data2.begin(), data2.end(), Negate());
    // for (int i = 0; i < n; ++i) {
    //   check(data1[i] == data2[n - 1 - i], "Test failed");
    // }

    // radixSort(data2.begin(), data2.end(), negate);
    // for (int i = 0; i < n; ++i) {
    //   check(data1[i] == data2[n - 1 - i], "Test failed");
    // }

  }

  {
    std::vector<unsigned char> data1(n);
    generateCharSequence(n, data1);

    std::vector<unsigned char> data2(data1);
    std::sort(data1.begin(), data1.end());

    radixSort(data2.begin(), data2.end());
    for (int i = 0; i < n; ++i) {
      check(data1[i] == data2[i], "Test failed");
    }

  }
  {
    std::list<int> data1;
    generateIntSequence(n, data1);

    std::list<int> data2(listsort<std::list<int> >(data1.begin(), data1.end()));

    radixSort(data1.begin(), data1.end());

    check(isTheSame(data1,data2), "Test failed");


    // radixSort(data2.begin(), data2.end(), Negate());
    // check(isTheSame(data1,data2), "Test failed");
    // for (int i = 0; i < n; ++i) {
    //   check(data1[i] == data2[n - 1 - i], "Test failed");
    // }

    // radixSort(data2.begin(), data2.end(), negate);
    // for (int i = 0; i < n; ++i) {
    //   check(data1[i] == data2[n - 1 - i], "Test failed");
    // }

  }

  {
    std::list<unsigned char> data1(n);
    generateCharSequence(n, data1);

    std::list<unsigned char> data2(listsort<std::list<unsigned char> >
                                   (data1.begin(),
                                    data1.end()));

    radixSort(data1.begin(), data1.end());
    check(isTheSame(data1,data2), "Test failed");

  }
}


void checkStableRadixSort() {
  {
    std::vector<int> data1;
    generateIntSequence(n, data1);

    std::vector<int> data2(data1);
    std::sort(data1.begin(), data1.end());

    stableRadixSort(data2.begin(), data2.end());
    for (int i = 0; i < n; ++i) {
      check(data1[i] == data2[i], "Test failed");
    }

    stableRadixSort(data2.begin(), data2.end(), Negate());
    for (int i = 0; i < n; ++i) {
      check(data1[i] == data2[n - 1 - i], "Test failed");
    }

    stableRadixSort(data2.begin(), data2.end(), negate);
    for (int i = 0; i < n; ++i) {
      check(data1[i] == data2[n - 1 - i], "Test failed");
    }
  }

  {
    std::vector<unsigned char> data1(n);
    generateCharSequence(n, data1);

    std::vector<unsigned char> data2(data1);
    std::sort(data1.begin(), data1.end());

    radixSort(data2.begin(), data2.end());
    for (int i = 0; i < n; ++i) {
      check(data1[i] == data2[i], "Test failed");
    }

  }
  {
    std::list<int> data1;
    generateIntSequence(n, data1);

    std::list<int> data2(listsort<std::list<int> >(data1.begin(),
                                                   data1.end()));
    stableRadixSort(data1.begin(), data1.end());
    check(isTheSame(data1,data2), "Test failed");

    // stableRadixSort(data2.begin(), data2.end(), Negate());
    // for (int i = 0; i < n; ++i) {
    //   check(data1[i] == data2[n - 1 - i], "Test failed");
    // }

    // stableRadixSort(data2.begin(), data2.end(), negate);
    // for (int i = 0; i < n; ++i) {
    //   check(data1[i] == data2[n - 1 - i], "Test failed");
    // }
  }

  {
    std::list<unsigned char> data1(n);
    generateCharSequence(n, data1);

    std::list<unsigned char> data2(listsort<std::list<unsigned char> >
                                   (data1.begin(),
                                    data1.end()));
    radixSort(data1.begin(), data1.end());
    check(isTheSame(data1,data2), "Test failed");

  }
}

int main() {

  checkRadixSort();
  checkStableRadixSort();

  return 0;
}
