/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * In geometry nodes, many functions accept fields as inputs. For the implementation that means
 * that the inputs are virtual arrays. Usually those are backed by actual arrays or single values
 * but sometimes virtual arrays are used to compute values on demand or convert between data
 * formats.
 *
 * Using virtual arrays has the downside that individual elements are accessed through a virtual
 * method call, which has some overhead compared to normal array access. Whether this overhead is
 * negligible depends on the context. For very small functions (e.g. a single addition), the
 * overhead can make the function many times slower. Furthermore, it prevents the compiler from
 * doing some optimizations (e.g. loop unrolling and inserting SIMD instructions).
 *
 * The solution is to "devirtualize" the virtual arrays in cases when the overhead cannot be
 * ignored. That means that the function is instantiated multiple times at compile time for the
 * different cases. For example, there can be an optimized function that adds a span and a single
 * value, and another function that adds a span and another span. At run-time there is a dynamic
 * dispatch that executes the best function given the specific virtual arrays.
 *
 * The problem with this devirtualization is that it can result in exponentially increasing compile
 * times and binary sizes, depending on the number of parameters that are devirtualized separately.
 * So there is always a trade-off between run-time performance and compile-time/binary-size.
 *
 * This file provides a utility to devirtualize function parameters using a high level API. This
 * makes it easy to experiment with different extremes of the mentioned trade-off and allows
 * finding a good compromise for each function.
 */

namespace blender {

/**
 * Calls the given function with devirtualized parameters if possible. Note that using many
 * non-trivial devirtualizers results in exponential code growth.
 *
 * \return True if the function has been called.
 *
 * Every devirtualizer is expected to have a `devirtualize(auto fn) -> bool` method.
 * This method is expected to do one of two things:
 * - Call `fn` with the devirtualized argument and return what `fn` returns.
 * - Don't call `fn` (because the devirtualization failed) and return false.
 *
 * Examples for devirtualizers: #BasicDevirtualizer, #IndexMaskDevirtualizer, #VArrayDevirtualizer.
 */
template<typename Fn, typename... Devirtualizers>
inline bool call_with_devirtualized_parameters(const std::tuple<Devirtualizers...> &devis,
                                               const Fn &fn)
{
  /* In theory the code below could be generalized to avoid code duplication. However, the maximum
   * number of parameters is expected to be relatively low. Explicitly implementing the different
   * cases makes it more obvious to see what is going on and also makes inlining everything easier
   * for the compiler. */
  constexpr size_t DeviNum = sizeof...(Devirtualizers);
  if constexpr (DeviNum == 0) {
    fn();
    return true;
  }
  if constexpr (DeviNum == 1) {
    return std::get<0>(devis).devirtualize([&](auto param0) {
      fn(param0);
      return true;
    });
  }
  if constexpr (DeviNum == 2) {
    return std::get<0>(devis).devirtualize([&](auto &&param0) {
      return std::get<1>(devis).devirtualize([&](auto &&param1) {
        fn(param0, param1);
        return true;
      });
    });
  }
  if constexpr (DeviNum == 3) {
    return std::get<0>(devis).devirtualize([&](auto &&param0) {
      return std::get<1>(devis).devirtualize([&](auto &&param1) {
        return std::get<2>(devis).devirtualize([&](auto &&param2) {
          fn(param0, param1, param2);
          return true;
        });
      });
    });
  }
  if constexpr (DeviNum == 4) {
    return std::get<0>(devis).devirtualize([&](auto &&param0) {
      return std::get<1>(devis).devirtualize([&](auto &&param1) {
        return std::get<2>(devis).devirtualize([&](auto &&param2) {
          return std::get<3>(devis).devirtualize([&](auto &&param3) {
            fn(param0, param1, param2, param3);
            return true;
          });
        });
      });
    });
  }
  if constexpr (DeviNum == 5) {
    return std::get<0>(devis).devirtualize([&](auto &&param0) {
      return std::get<1>(devis).devirtualize([&](auto &&param1) {
        return std::get<2>(devis).devirtualize([&](auto &&param2) {
          return std::get<3>(devis).devirtualize([&](auto &&param3) {
            return std::get<4>(devis).devirtualize([&](auto &&param4) {
              fn(param0, param1, param2, param3, param4);
              return true;
            });
          });
        });
      });
    });
  }
  if constexpr (DeviNum == 6) {
    return std::get<0>(devis).devirtualize([&](auto &&param0) {
      return std::get<1>(devis).devirtualize([&](auto &&param1) {
        return std::get<2>(devis).devirtualize([&](auto &&param2) {
          return std::get<3>(devis).devirtualize([&](auto &&param3) {
            return std::get<4>(devis).devirtualize([&](auto &&param4) {
              return std::get<5>(devis).devirtualize([&](auto &&param5) {
                fn(param0, param1, param2, param3, param4, param5);
                return true;
              });
            });
          });
        });
      });
    });
  }
  if constexpr (DeviNum == 7) {
    return std::get<0>(devis).devirtualize([&](auto &&param0) {
      return std::get<1>(devis).devirtualize([&](auto &&param1) {
        return std::get<2>(devis).devirtualize([&](auto &&param2) {
          return std::get<3>(devis).devirtualize([&](auto &&param3) {
            return std::get<4>(devis).devirtualize([&](auto &&param4) {
              return std::get<5>(devis).devirtualize([&](auto &&param5) {
                return std::get<6>(devis).devirtualize([&](auto &&param6) {
                  fn(param0, param1, param2, param3, param4, param5, param6);
                  return true;
                });
              });
            });
          });
        });
      });
    });
  }
  return false;
}

/**
 * A devirtualizer to be used with #call_with_devirtualized_parameters.
 *
 * This one is very simple, it does not perform any actual devirtualization. It can be used to pass
 * parameters to the function that shouldn't be devirtualized.
 */
template<typename T> struct BasicDevirtualizer {
  const T value;

  template<typename Fn> bool devirtualize(const Fn &fn) const
  {
    return fn(this->value);
  }
};

}  // namespace blender
