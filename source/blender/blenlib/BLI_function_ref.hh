/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <optional>
#include <type_traits>
#include <utility>

#include "BLI_utildefines.h"

/** \file
 * \ingroup bli
 *
 * A `FunctionRef<Signature>` is a non-owning reference to some callable object with a specific
 * signature. It can be used to pass some callback to another function.
 *
 * A `FunctionRef` is small and cheap to copy. Therefore it should generally be passed by value.
 *
 * Example signatures:
 *   `FunctionRef<void()>`        - A function without parameters and void return type.
 *   `FunctionRef<int(float)>`    - A function with a float parameter and an int return value.
 *   `FunctionRef<int(int, int)>` - A function with two int parameters and an int return value.
 *
 * There are multiple ways to achieve that, so here is a comparison of the different approaches:
 * 1. Pass function pointer and user data (as void *) separately:
 *    - The only method that is compatible with C interfaces.
 *    - Is cumbersome to work with in many cases, because one has to keep track of two parameters.
 *    - Not type safe at all, because of the void pointer.
 *    - It requires workarounds when one wants to pass a lambda into a function.
 * 2. Using `std::function`:
 *    - It works well with most callables and is easy to use.
 *    - Owns the callable, so it can be returned from a function more safely than other methods.
 *    - Requires that the callable is copyable.
 *    - Requires an allocation when the callable is too large (typically > 16 bytes).
 * 3. Using a template for the callable type:
 *    - Most efficient solution at runtime, because compiler knows the exact callable at the place
 *      where it is called.
 *    - Works well with all callables.
 *    - Requires the function to be in a header file.
 *    - It's difficult to constrain the signature of the function.
 * 4. Using `FunctionRef`:
 *    - Second most efficient solution at runtime.
 *    - It's easy to constrain the signature of the callable.
 *    - Does not require the function to be in a header file.
 *    - Works well with all callables.
 *    - It's a non-owning reference, so it *cannot* be stored safely in general.
 *
 * The fact that this is a non-owning reference makes `FunctionRef` very well suited for some use
 * cases, but one has to be a bit more careful when using it to make sure that the referenced
 * callable is not destructed.
 *
 * In particular, one must not construct a `FunctionRef` variable from a lambda directly as shown
 * below. This is because the lambda object goes out of scope after the line finished executing and
 * will be destructed. Calling the reference afterwards invokes undefined behavior.
 *
 * Don't:
 *   FunctionRef<int()> ref = []() { return 0; };
 * Do:
 *   auto f = []() { return 0; };
 *   FuntionRef<int()> ref = f;
 *
 * It is fine to pass a lambda directly to a function:
 *
 *   void some_function(FunctionRef<int()> f);
 *   some_function([]() { return 0; });
 *
 */

namespace blender {

template<typename Function> class FunctionRef;

template<typename Ret, typename... Params> class FunctionRef<Ret(Params...)> {
 private:
  /**
   * A function pointer that knows how to call the referenced callable with the given parameters.
   */
  Ret (*callback_)(intptr_t callable, Params... params) = nullptr;

  /**
   * A pointer to the referenced callable object. This can be a C function, a lambda object or any
   * other callable.
   *
   * The value does not need to be initialized because it is not used unless `callback_` is set as
   * well, in which case it will be initialized as well.
   *
   * Use `intptr_t` to avoid warnings when casting to function pointers.
   */
  intptr_t callable_;

  template<typename Callable> static Ret callback_fn(intptr_t callable, Params... params)
  {
    return (*reinterpret_cast<Callable *>(callable))(std::forward<Params>(params)...);
  }

 public:
  FunctionRef() = default;

  FunctionRef(std::nullptr_t)
  {
  }

  /**
   * A `FunctionRef` itself is a callable as well. However, we don't want that this
   * constructor is called when `Callable` is a `FunctionRef`. If we would allow this, it
   * would be easy to accidentally create a `FunctionRef` that internally calls another
   * `FunctionRef`. Usually, when assigning a `FunctionRef` to another, we want that both
   * contain a reference to the same underlying callable afterwards.
   *
   * It is still possible to reference another `FunctionRef` by first wrapping it in
   * another lambda.
   */
  template<typename Callable,
           std::enable_if_t<!std::is_same_v<std::remove_cv_t<std::remove_reference_t<Callable>>,
                                            FunctionRef>> * = nullptr>
  FunctionRef(Callable &&callable)
      : callback_(callback_fn<typename std::remove_reference_t<Callable>>),
        callable_(reinterpret_cast<intptr_t>(&callable))
  {
  }

  /**
   * Call the referenced function and forward all parameters to it.
   *
   * This invokes undefined behavior if the `FunctionRef` does not reference a function currently.
   */
  Ret operator()(Params... params) const
  {
    BLI_assert(callback_ != nullptr);
    return callback_(callable_, std::forward<Params>(params)...);
  }

  using OptionalReturnValue = std::conditional_t<std::is_void_v<Ret>, void, std::optional<Ret>>;

  /**
   * Calls the referenced function if it is available.
   * The return value is of type `std::optional<Ret>` if `Ret` is not `void`.
   * Otherwise the return type is `void`.
   */
  OptionalReturnValue call_safe(Params... params) const
  {
    if constexpr (std::is_void_v<Ret>) {
      if (callback_ == nullptr) {
        return;
      }
      callback_(callable_, std::forward<Params>(params)...);
    }
    else {
      if (callback_ == nullptr) {
        return {};
      }
      return callback_(callable_, std::forward<Params>(params)...);
    }
  }

  /**
   * Returns true, when the `FunctionRef` references a function currently.
   * If this returns false, the `FunctionRef` must not be called.
   */
  operator bool() const
  {
    /* Just checking `callback_` is enough to determine if the `FunctionRef` is in a state that it
     * can be called in. */
    return callback_ != nullptr;
  }
};

}  // namespace blender
