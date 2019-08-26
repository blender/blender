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

#ifndef LEMON_BITS_SOLVER_BITS_H
#define LEMON_BITS_SOLVER_BITS_H

#include <vector>

namespace lemon {

  namespace _solver_bits {

    class VarIndex {
    private:
      struct ItemT {
        int prev, next;
        int index;
      };
      std::vector<ItemT> items;
      int first_item, last_item, first_free_item;

      std::vector<int> cross;

    public:

      VarIndex()
        : first_item(-1), last_item(-1), first_free_item(-1) {
      }

      void clear() {
        first_item = -1;
        last_item = -1;
        first_free_item = -1;
        items.clear();
        cross.clear();
      }

      int addIndex(int idx) {
        int n;
        if (first_free_item == -1) {
          n = items.size();
          items.push_back(ItemT());
        } else {
          n = first_free_item;
          first_free_item = items[n].next;
          if (first_free_item != -1) {
            items[first_free_item].prev = -1;
          }
        }
        items[n].index = idx;
        if (static_cast<int>(cross.size()) <= idx) {
          cross.resize(idx + 1, -1);
        }
        cross[idx] = n;

        items[n].prev = last_item;
        items[n].next = -1;
        if (last_item != -1) {
          items[last_item].next = n;
        } else {
          first_item = n;
        }
        last_item = n;

        return n;
      }

      int addIndex(int idx, int n) {
        while (n >= static_cast<int>(items.size())) {
          items.push_back(ItemT());
          items.back().prev = -1;
          items.back().next = first_free_item;
          if (first_free_item != -1) {
            items[first_free_item].prev = items.size() - 1;
          }
          first_free_item = items.size() - 1;
        }
        if (items[n].next != -1) {
          items[items[n].next].prev = items[n].prev;
        }
        if (items[n].prev != -1) {
          items[items[n].prev].next = items[n].next;
        } else {
          first_free_item = items[n].next;
        }

        items[n].index = idx;
        if (static_cast<int>(cross.size()) <= idx) {
          cross.resize(idx + 1, -1);
        }
        cross[idx] = n;

        items[n].prev = last_item;
        items[n].next = -1;
        if (last_item != -1) {
          items[last_item].next = n;
        } else {
          first_item = n;
        }
        last_item = n;

        return n;
      }

      void eraseIndex(int idx) {
        int n = cross[idx];

        if (items[n].prev != -1) {
          items[items[n].prev].next = items[n].next;
        } else {
          first_item = items[n].next;
        }
        if (items[n].next != -1) {
          items[items[n].next].prev = items[n].prev;
        } else {
          last_item = items[n].prev;
        }

        if (first_free_item != -1) {
          items[first_free_item].prev = n;
        }
        items[n].next = first_free_item;
        items[n].prev = -1;
        first_free_item = n;

        while (!cross.empty() && cross.back() == -1) {
          cross.pop_back();
        }
      }

      int maxIndex() const {
        return cross.size() - 1;
      }

      void shiftIndices(int idx) {
        for (int i = idx + 1; i < static_cast<int>(cross.size()); ++i) {
          cross[i - 1] = cross[i];
          if (cross[i] != -1) {
            --items[cross[i]].index;
          }
        }
        cross.back() = -1;
        cross.pop_back();
        while (!cross.empty() && cross.back() == -1) {
          cross.pop_back();
        }
      }

      void relocateIndex(int idx, int jdx) {
        cross[idx] = cross[jdx];
        items[cross[jdx]].index = idx;
        cross[jdx] = -1;

        while (!cross.empty() && cross.back() == -1) {
          cross.pop_back();
        }
      }

      int operator[](int idx) const {
        return cross[idx];
      }

      int operator()(int fdx) const {
        return items[fdx].index;
      }

      void firstItem(int& fdx) const {
        fdx = first_item;
      }

      void nextItem(int& fdx) const {
        fdx = items[fdx].next;
      }

    };
  }
}

#endif
