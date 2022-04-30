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
 * This file provides a utility to devirtualize array parameters to a function using a high level
 * API. This makes it easy to experiment with different extremes of the mentioned trade-off and
 * allows finding a good compromise for each function.
 */

#include "BLI_parameter_pack_utils.hh"
#include "BLI_virtual_array.hh"

namespace blender::devirtualize_parameters {

/**
 * Bit flag that specifies how an individual parameter is or can be devirtualized.
 */
enum class DeviMode {
  /* This is used as zero-value to compare to, to avoid casting to int. */
  None = 0,
  /* Don't use devirtualization for that parameter, just pass it along. */
  Keep = (1 << 0),
  /* Devirtualize #Varray as #Span. */
  Span = (1 << 1),
  /* Devirtualize #VArray as #SingleAsSpan.  */
  Single = (1 << 2),
  /* Devirtualize #IndexMask as #IndexRange. */
  Range = (1 << 3),
};
ENUM_OPERATORS(DeviMode, DeviMode::Range);

/** Utility to encode multiple #DeviMode in a type. */
template<DeviMode... Mode> using DeviModeSequence = ValueSequence<DeviMode, Mode...>;

/**
 * Main class that performs the devirtualization.
 */
template<typename Fn, typename... SourceTypes> class Devirtualizer {
 private:
  /** Utility to get the tag of the I-th source type. */
  template<size_t I>
  using type_at_index = typename TypeSequence<SourceTypes...>::template at_index<I>;
  static constexpr size_t SourceTypesNum = sizeof...(SourceTypes);

  /** Function to devirtualize. */
  Fn fn_;

  /**
   * Source values that will be devirtualized. Note that these are stored as pointers to avoid
   * unnecessary copies. The caller is responsible for keeping the memory alive.
   */
  std::tuple<const SourceTypes *...> sources_;

  /** Keeps track of whether #fn_ has been called already to avoid calling it twice. */
  bool executed_ = false;

 public:
  Devirtualizer(Fn fn, const SourceTypes *...sources) : fn_(std::move(fn)), sources_{sources...}
  {
  }

  /**
   * Return true when the function passed to the constructor has been called already.
   */
  bool executed() const
  {
    return executed_;
  }

  /**
   * At compile time, generates multiple variants of the function, each optimized for a different
   * combination of devirtualized parameters. For every parameter, a bit flag is passed that
   * determines how it will be devirtualized. At run-time, if possible, one of the generated
   * functions is picked and executed.
   *
   * To check whether the function was called successfully, call #executed() afterwards.
   *
   * \note This generates an exponential amount of code in the final binary, depending on how many
   * to-be-virtualized parameters there are.
   */
  template<DeviMode... AllowedModes>
  void try_execute_devirtualized(DeviModeSequence<AllowedModes...> /* allowed_modes */)
  {
    BLI_assert(!executed_);
    static_assert(sizeof...(AllowedModes) == SourceTypesNum);
    this->try_execute_devirtualized_impl(DeviModeSequence<>(),
                                         DeviModeSequence<AllowedModes...>());
  }

  /**
   * Execute the function and pass in the original parameters without doing any devirtualization.
   */
  void execute_without_devirtualization()
  {
    BLI_assert(!executed_);
    this->try_execute_devirtualized_impl_call(
        make_value_sequence<DeviMode, DeviMode::Keep, SourceTypesNum>(),
        std::make_index_sequence<SourceTypesNum>());
  }

 private:
  /**
   * A recursive method that generates all the combinations of devirtualized parameters that the
   * caller requested. A recursive function is necessary to achieve generating an exponential
   * number of function calls (which has to be used with care, but is expected here).
   *
   * At every recursive step, the #DeviMode of one parameter is determined. This is achieved by
   * extending #DeviModeSequence<Mode...> by one element in each step. The recursion ends once all
   * parameters are handled.
   *
   * \return True when the function has been executed.
   */
  template<DeviMode... Mode, DeviMode... AllowedModes>
  bool try_execute_devirtualized_impl(
      /* Initially empty, but then extended by one element in each recursive step.  */
      DeviModeSequence<Mode...> /* modes */,
      /* Bit flag for every parameter. */
      DeviModeSequence<AllowedModes...> /* allowed_modes */)
  {
    static_assert(SourceTypesNum == sizeof...(AllowedModes));
    if constexpr (SourceTypesNum == sizeof...(Mode)) {
      /* End of recursion, now call the function with the determined #DeviModes. */
      this->try_execute_devirtualized_impl_call(DeviModeSequence<Mode...>(),
                                                std::make_index_sequence<SourceTypesNum>());
      return true;
    }
    else {
      /* Index of the parameter that is checked in the current recursive step. */
      constexpr size_t I = sizeof...(Mode);
      /* Non-devirtualized parameter type. */
      using SourceType = type_at_index<I>;
      /* A bit flag indicating what devirtualizations are allowed in this step. */
      [[maybe_unused]] constexpr DeviMode allowed_modes =
          DeviModeSequence<AllowedModes...>::template at_index<I>();

      /* Handle #VArray types. */
      if constexpr (is_VArray_v<SourceType>) {
        /* The actual virtual array, used for dynamic dispatch at run-time. */
        const SourceType &varray = *std::get<I>(sources_);
        /* Check if the virtual array is a single value. */
        if constexpr ((allowed_modes & DeviMode::Single) != DeviMode::None) {
          if (varray.is_single()) {
            if (this->try_execute_devirtualized_impl(DeviModeSequence<Mode..., DeviMode::Single>(),
                                                     DeviModeSequence<AllowedModes...>())) {
              return true;
            }
          }
        }
        /* Check if the virtual array is a span. */
        if constexpr ((allowed_modes & DeviMode::Span) != DeviMode::None) {
          if (varray.is_span()) {
            if (this->try_execute_devirtualized_impl(DeviModeSequence<Mode..., DeviMode::Span>(),
                                                     DeviModeSequence<AllowedModes...>())) {
              return true;
            }
          }
        }
        /* Check if it is ok if the virtual array is not devirtualized. */
        if constexpr ((allowed_modes & DeviMode::Keep) != DeviMode::None) {
          if (this->try_execute_devirtualized_impl(DeviModeSequence<Mode..., DeviMode::Keep>(),
                                                   DeviModeSequence<AllowedModes...>())) {
            return true;
          }
        }
      }

      /* Handle #IndexMask. */
      else if constexpr (std::is_same_v<IndexMask, SourceType>) {
        /* Check if the mask is actually a contiguous range. */
        if constexpr ((allowed_modes & DeviMode::Range) != DeviMode::None) {
          /* The actual mask used for dynamic dispatch at run-time. */
          const IndexMask &mask = *std::get<I>(sources_);
          if (mask.is_range()) {
            if (this->try_execute_devirtualized_impl(DeviModeSequence<Mode..., DeviMode::Range>(),
                                                     DeviModeSequence<AllowedModes...>())) {
              return true;
            }
          }
        }
        /* Check if mask is also allowed to stay a span. */
        if constexpr ((allowed_modes & DeviMode::Span) != DeviMode::None) {
          if (this->try_execute_devirtualized_impl(DeviModeSequence<Mode..., DeviMode::Span>(),
                                                   DeviModeSequence<AllowedModes...>())) {
            return true;
          }
        }
      }

      /* Handle unknown types. */
      else {
        if (this->try_execute_devirtualized_impl(DeviModeSequence<Mode..., DeviMode::Keep>(),
                                                 DeviModeSequence<AllowedModes...>())) {
          return true;
        }
      }
    }
    return false;
  }

  /**
   * Actually call the function with devirtualized parameters.
   */
  template<DeviMode... Mode, size_t... I>
  void try_execute_devirtualized_impl_call(DeviModeSequence<Mode...> /* modes */,
                                           std::index_sequence<I...> /* indices */)
  {

    BLI_assert(!executed_);
    fn_(this->get_devirtualized_parameter<I, Mode>()...);
    executed_ = true;
  }

  /**
   * Return the I-th parameter devirtualized using the passed in #DeviMode. This has different
   * return types based on the template parameters.
   *
   * \note It is expected that the caller already knows that the parameter can be devirtualized
   * with the given mode.
   */
  template<size_t I, DeviMode Mode> decltype(auto) get_devirtualized_parameter()
  {
    using SourceType = type_at_index<I>;
    static_assert(Mode != DeviMode::None);
    if constexpr (Mode == DeviMode::Keep) {
      /* Don't change the original parameter at all. */
      return *std::get<I>(sources_);
    }
    if constexpr (is_VArray_v<SourceType>) {
      const SourceType &varray = *std::get<I>(sources_);
      if constexpr (Mode == DeviMode::Single) {
        /* Devirtualize virtual array as single value. */
        return SingleAsSpan(varray);
      }
      else if constexpr (Mode == DeviMode::Span) {
        /* Devirtualize virtual array as span. */
        return varray.get_internal_span();
      }
    }
    else if constexpr (std::is_same_v<IndexMask, SourceType>) {
      const IndexMask &mask = *std::get<I>(sources_);
      if constexpr (ELEM(Mode, DeviMode::Span)) {
        /* Don't devirtualize mask, it's still a span. */
        return mask;
      }
      else if constexpr (Mode == DeviMode::Range) {
        /* Devirtualize the mask as range. */
        return mask.as_range();
      }
    }
  }
};

}  // namespace blender::devirtualize_parameters

namespace blender {

/**
 * Generate multiple versions of the given function optimized for different virtual arrays.
 * One has to be careful with nesting multiple devirtualizations, because that results in an
 * exponential number of function instantiations (increasing compile time and binary size).
 *
 * Generally, this function should only be used when the virtual method call overhead to get an
 * element from a virtual array is significant.
 */
template<typename T, typename Func>
inline void devirtualize_varray(const VArray<T> &varray, const Func &func, bool enable = true)
{
  using namespace devirtualize_parameters;
  if (enable) {
    Devirtualizer<decltype(func), VArray<T>> devirtualizer(func, &varray);
    constexpr DeviMode devi_mode = DeviMode::Single | DeviMode::Span;
    devirtualizer.try_execute_devirtualized(DeviModeSequence<devi_mode>());
    if (devirtualizer.executed()) {
      return;
    }
  }
  func(varray);
}

/**
 * Same as `devirtualize_varray`, but devirtualizes two virtual arrays at the same time.
 * This is better than nesting two calls to `devirtualize_varray`, because it instantiates fewer
 * cases.
 */
template<typename T1, typename T2, typename Func>
inline void devirtualize_varray2(const VArray<T1> &varray1,
                                 const VArray<T2> &varray2,
                                 const Func &func,
                                 bool enable = true)
{
  using namespace devirtualize_parameters;
  if (enable) {
    Devirtualizer<decltype(func), VArray<T1>, VArray<T2>> devirtualizer(func, &varray1, &varray2);
    constexpr DeviMode devi_mode = DeviMode::Single | DeviMode::Span;
    devirtualizer.try_execute_devirtualized(DeviModeSequence<devi_mode, devi_mode>());
    if (devirtualizer.executed()) {
      return;
    }
  }
  func(varray1, varray2);
}

}  // namespace blender
