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

#pragma once
namespace carve {
  namespace heap {
    namespace detail {



      struct ignore_position_t {
        template<typename value_t>
        void operator()(value_t &val, size_t idx) const {}
      };



      template<typename random_access_iter_t,
               typename distance_t,
               typename value_t,
               typename pred_t,
               typename pos_notifier_t>
      void _adjust_heap(random_access_iter_t begin,
                        distance_t pos,
                        distance_t len,
                        value_t val,
                        pred_t pred,
                        pos_notifier_t notify) {
        const distance_t top = pos;

        distance_t child = pos * 2 + 2;
        while (child < len) {
          if (pred(begin[child], begin[child - 1])) child--;

          begin[pos] = begin[child];
          notify(begin[pos], pos);
          pos = child;
          child = pos * 2 + 2;
        }

        if (child == len) {
          child--;
          begin[pos] = begin[child];
          notify(begin[pos], pos);
          pos = child;
        }

        distance_t parent = (pos - 1) / 2;
        while (pos > top && pred(begin[parent], val)) {
          begin[pos] = begin[parent];
          notify(begin[pos], pos);
          pos = parent;
          parent = (pos - 1) / 2;
        }

        begin[pos] = val;
        notify(begin[pos], pos);
      }



      template<typename random_access_iter_t,
               typename distance_t,
               typename value_t,
               typename pred_t,
               typename pos_notifier_t>
      void _push_heap(random_access_iter_t begin,
                      distance_t pos,
                      value_t val,
                      pred_t pred,
                      pos_notifier_t notify) {
        distance_t parent = (pos - 1) / 2;
        while (pos > 0 && pred(begin[parent], val)) {
          begin[pos] = begin[parent];
          notify(begin[pos], pos);
          pos = parent;
          parent = (pos - 1) / 2;
        }
        begin[pos] = val;
        notify(begin[pos], pos);
      }



      template<typename random_access_iter_t,
               typename distance_t,
               typename pred_t,
               typename pos_notifier_t>
      void _remove_heap(random_access_iter_t begin,
                        distance_t pos,
                        distance_t len,
                        pred_t pred,
                        pos_notifier_t notify) {
        --len;
        if (pos != len) {
          typedef typename std::iterator_traits<random_access_iter_t>::value_type value_t;
          value_t removed = begin[pos];
          _adjust_heap(begin, pos, len, begin[len], pred, notify);
          begin[len] = removed;
          notify(begin[len], len);
        }
      }



      template<typename random_access_iter_t,
               typename distance_t,
               typename pred_t,
               typename pos_notifier_t>
      void _make_heap(random_access_iter_t begin,
                      distance_t len,
                      pred_t pred,
                      pos_notifier_t notify) {
        for (distance_t pos = len / 2; pos > 0; ) {
          --pos;
          _adjust_heap(begin, pos, len, begin[pos], pred, ignore_position_t());
        }
        for (distance_t pos = 0; pos < len; ++pos) {
          notify(begin[pos], pos);
        }
      }



      template<typename random_access_iter_t,
               typename distance_t,
               typename pred_t>
      void _make_heap(random_access_iter_t begin,
                      distance_t len,
                      pred_t pred,
                      ignore_position_t) {
        for (distance_t pos = len / 2; pos > 0; ) {
          --pos;
          _adjust_heap(begin, pos, len, begin[pos], pred, ignore_position_t());
        }
      }



      template<typename random_access_iter_t,
               typename distance_t,
               typename pred_t>
      bool _is_heap(random_access_iter_t begin,
                    distance_t len,
                    pred_t pred) {
        distance_t parent = 0;

        for (distance_t child = 1; child < len; ++child) {
          if (pred(begin[parent], begin[child])) {
            return false;
          }
          if (++child == len) break;
          if (pred(begin[parent], begin[child])) {
          return false;
          }
          ++parent;
        }

        return true;
      }



    }



    template<typename random_access_iter_t>
    void adjust_heap(random_access_iter_t begin,
                     random_access_iter_t end,
                     random_access_iter_t pos) {
      typedef typename std::iterator_traits<random_access_iter_t>::value_type value_t;

      detail::_adjust_heap(begin, pos - begin, end - begin, *pos, std::less<value_t>());
    }



    template<typename random_access_iter_t,
             typename pred_t>
    void adjust_heap(random_access_iter_t begin,
                     random_access_iter_t end,
                     random_access_iter_t pos,
                     pred_t pred) {
      detail::_adjust_heap(begin, pos - begin, end - begin, *pos, pred);
    }



    template<typename random_access_iter_t,
             typename pred_t,
             typename pos_notifier_t>
    void adjust_heap(random_access_iter_t begin,
                     random_access_iter_t end,
                     random_access_iter_t pos,
                     pred_t pred,
                     pos_notifier_t notify) {
      detail::_adjust_heap(begin, pos - begin, end - begin, *pos, pred, notify);
    }



    template<typename random_access_iter_t>
    void remove_heap(random_access_iter_t begin,
                     random_access_iter_t end,
                     random_access_iter_t pos) {
      typedef typename std::iterator_traits<random_access_iter_t>::value_type value_t;

      detail::_remove_heap(begin, pos - begin, end - begin, std::less<value_t>(), detail::ignore_position_t());
    }



    template<typename random_access_iter_t,
             typename pred_t>
    void remove_heap(random_access_iter_t begin,
                     random_access_iter_t end,
                     random_access_iter_t pos,
                     pred_t pred) {
      detail::_remove_heap(begin, pos - begin, end - begin, pred, detail::ignore_position_t());
    }



    template<typename random_access_iter_t,
             typename pred_t,
             typename pos_notifier_t>
    void remove_heap(random_access_iter_t begin,
                     random_access_iter_t end,
                     random_access_iter_t pos,
                     pred_t pred,
                     pos_notifier_t notify) {
      detail::_remove_heap(begin, pos - begin, end - begin, pred, notify);
    }



    template<typename random_access_iter_t>
    void pop_heap(random_access_iter_t begin,
                  random_access_iter_t end) {
      typedef typename std::iterator_traits<random_access_iter_t>::value_type value_t;
      typedef typename std::iterator_traits<random_access_iter_t>::difference_type distance_t;

      detail::_remove_heap(begin, distance_t(0), end - begin, std::less<value_t>(), detail::ignore_position_t());
    }



    template<typename random_access_iter_t,
             typename pred_t>
    void pop_heap(random_access_iter_t begin,
                  random_access_iter_t end,
                  pred_t pred) {
      typedef typename std::iterator_traits<random_access_iter_t>::difference_type distance_t;

      detail::_remove_heap(begin, distance_t(0), end - begin, pred, detail::ignore_position_t());
    }



    template<typename random_access_iter_t,
             typename pred_t,
             typename pos_notifier_t>
    void pop_heap(random_access_iter_t begin,
                     random_access_iter_t end,
                     pred_t pred,
                     pos_notifier_t notify) {
      typedef typename std::iterator_traits<random_access_iter_t>::difference_type distance_t;

      detail::_remove_heap(begin, distance_t(0), end - begin, pred, notify);
    }



    template<typename random_access_iter_t>
    void push_heap(random_access_iter_t begin,
                   random_access_iter_t end) {
      typedef typename std::iterator_traits<random_access_iter_t>::value_type value_t;
      typedef typename std::iterator_traits<random_access_iter_t>::difference_type distance_t;

      distance_t pos = end - begin - 1;
      detail::_push_heap(begin, pos, begin[pos], std::less<value_t>(), detail::ignore_position_t());
    }



    template<typename random_access_iter_t,
             typename pred_t>
    void push_heap(random_access_iter_t begin,
                   random_access_iter_t end,
                   pred_t pred) {
      typedef typename std::iterator_traits<random_access_iter_t>::difference_type distance_t;

      distance_t pos = end - begin - 1;
      detail::_push_heap(begin, pos, begin[pos], pred, detail::ignore_position_t());
    }



    template<typename random_access_iter_t,
             typename pred_t,
             typename pos_notifier_t>
    void push_heap(random_access_iter_t begin,
                   random_access_iter_t end,
                   pred_t pred,
                   pos_notifier_t notify) {
      typedef typename std::iterator_traits<random_access_iter_t>::difference_type distance_t;

      distance_t pos = end - begin - 1;
      detail::_push_heap(begin, pos, begin[pos], pred, notify);
    }



    template<typename random_access_iter_t>
    void make_heap(random_access_iter_t begin,
                   random_access_iter_t end) {
      typedef typename std::iterator_traits<random_access_iter_t>::value_type value_t;

      detail::_make_heap(begin, end - begin, std::less<value_t>(), detail::ignore_position_t());
    }



    template<typename random_access_iter_t,
             typename pred_t>
    void make_heap(random_access_iter_t begin,
                   random_access_iter_t end,
                   pred_t pred) {
      detail::_make_heap(begin, end - begin, pred, detail::ignore_position_t());
    }



    template<typename random_access_iter_t,
             typename pred_t,
             typename pos_notifier_t>
    void make_heap(random_access_iter_t begin,
                   random_access_iter_t end,
                   pred_t pred,
                   pos_notifier_t notify) {
      detail::_make_heap(begin, end - begin, pred, notify);
    }



    template<typename random_access_iter_t>
    bool is_heap(random_access_iter_t begin,
                 random_access_iter_t end) {
      typedef typename std::iterator_traits<random_access_iter_t>::value_type value_t;

      return detail::_is_heap(begin, end - begin, std::less<value_t>());
    }



    template<typename random_access_iter_t, typename pred_t>
    bool is_heap(random_access_iter_t begin,
                 random_access_iter_t end,
                 pred_t pred) {
      return detail::_is_heap(begin, end - begin, pred);
    }



    template<typename random_access_iter_t>
    void sort_heap(random_access_iter_t begin,
                   random_access_iter_t end) {
      typedef typename std::iterator_traits<random_access_iter_t>::difference_type distance_t;
      typedef typename std::iterator_traits<random_access_iter_t>::value_type value_t;

      for (distance_t len = end - begin; len > 1; --len) {
        detail::_remove_heap(begin, distance_t(0), len, std::less<value_t>(), detail::ignore_position_t());
      }
    }



    template<typename random_access_iter_t,
             typename pred_t>
    void sort_heap(random_access_iter_t begin,
                   random_access_iter_t end,
                   pred_t pred) {
      typedef typename std::iterator_traits<random_access_iter_t>::difference_type distance_t;

      for (distance_t len = end - begin; len > 1; --len) {
        detail::_remove_heap(begin, distance_t(0), len, pred, detail::ignore_position_t());
      }
    }



    template<typename random_access_iter_t,
             typename pred_t,
             typename pos_notifier_t>
    void sort_heap(random_access_iter_t begin,
                   random_access_iter_t end,
                   pred_t pred,
                   pos_notifier_t notify) {
      typedef typename std::iterator_traits<random_access_iter_t>::difference_type distance_t;

      for (distance_t len = end - begin; len > 1; --len) {
        detail::_remove_heap(begin, distance_t(0), len, pred, detail::ignore_position_t());
        notify(begin[len], len);
      }
      notify(begin[0], 0);
    }



  }
}
