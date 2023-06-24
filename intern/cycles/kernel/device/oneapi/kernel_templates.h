/* SPDX-FileCopyrightText: 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Some macro magic to generate templates for kernel arguments.
 * The resulting oneapi_call() template allows to call a SYCL/C++ kernel
 * with typed arguments by only giving it a void `**args` as given by Cycles.
 * The template will automatically cast from void* to the expected type. */

/* When expanded by the preprocessor, the generated templates will look like this example: */
#if 0
template<typename T0, typename T1, typename T2>
void oneapi_call(
    KernelGlobalsGPU *kg,
    sycl::handler &cgh,
    size_t global_size,
    size_t local_size,
    void **args,
    void (*func)(const KernelGlobalsGPU *, size_t, size_t, sycl::handler &, T0, T1, T2))
{
  func(kg, global_size, local_size, cgh, *(T0 *)(args[0]), *(T1 *)(args[1]), *(T2 *)(args[2]));
}
#endif

/* clang-format off */
#define ONEAPI_TYP(x) typename T##x
#define ONEAPI_CAST(x) *(T##x *)(args[x])
#define ONEAPI_T(x) T##x

#define ONEAPI_GET_NTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, N,  ...) N
#define ONEAPI_0(_call, ...)
#define ONEAPI_1(_call, x) _call(x)
#define ONEAPI_2(_call, x, ...) _call(x), ONEAPI_1(_call, __VA_ARGS__)
#define ONEAPI_3(_call, x, ...) _call(x), ONEAPI_2(_call, __VA_ARGS__)
#define ONEAPI_4(_call, x, ...) _call(x), ONEAPI_3(_call, __VA_ARGS__)
#define ONEAPI_5(_call, x, ...) _call(x), ONEAPI_4(_call, __VA_ARGS__)
#define ONEAPI_6(_call, x, ...) _call(x), ONEAPI_5(_call, __VA_ARGS__)
#define ONEAPI_7(_call, x, ...) _call(x), ONEAPI_6(_call, __VA_ARGS__)
#define ONEAPI_8(_call, x, ...) _call(x), ONEAPI_7(_call, __VA_ARGS__)
#define ONEAPI_9(_call, x, ...) _call(x), ONEAPI_8(_call, __VA_ARGS__)
#define ONEAPI_10(_call, x, ...) _call(x), ONEAPI_9(_call, __VA_ARGS__)
#define ONEAPI_11(_call, x, ...) _call(x), ONEAPI_10(_call, __VA_ARGS__)
#define ONEAPI_12(_call, x, ...) _call(x), ONEAPI_11(_call, __VA_ARGS__)
#define ONEAPI_13(_call, x, ...) _call(x), ONEAPI_12(_call, __VA_ARGS__)
#define ONEAPI_14(_call, x, ...) _call(x), ONEAPI_13(_call, __VA_ARGS__)
#define ONEAPI_15(_call, x, ...) _call(x), ONEAPI_14(_call, __VA_ARGS__)
#define ONEAPI_16(_call, x, ...) _call(x), ONEAPI_15(_call, __VA_ARGS__)
#define ONEAPI_17(_call, x, ...) _call(x), ONEAPI_16(_call, __VA_ARGS__)
#define ONEAPI_18(_call, x, ...) _call(x), ONEAPI_17(_call, __VA_ARGS__)
#define ONEAPI_19(_call, x, ...) _call(x), ONEAPI_18(_call, __VA_ARGS__)
#define ONEAPI_20(_call, x, ...) _call(x), ONEAPI_19(_call, __VA_ARGS__)
#define ONEAPI_21(_call, x, ...) _call(x), ONEAPI_20(_call, __VA_ARGS__)

#define ONEAPI_CALL_FOR(x, ...) \
  ONEAPI_GET_NTH_ARG("ignored", \
                     ##__VA_ARGS__, \
                     ONEAPI_21, \
                     ONEAPI_20, \
                     ONEAPI_19, \
                     ONEAPI_18, \
                     ONEAPI_17, \
                     ONEAPI_16, \
                     ONEAPI_15, \
                     ONEAPI_14, \
                     ONEAPI_13, \
                     ONEAPI_12, \
                     ONEAPI_11, \
                     ONEAPI_10, \
                     ONEAPI_9, \
                     ONEAPI_8, \
                     ONEAPI_7, \
                     ONEAPI_6, \
                     ONEAPI_5, \
                     ONEAPI_4, \
                     ONEAPI_3, \
                     ONEAPI_2, \
                     ONEAPI_1, \
                     ONEAPI_0) \
  (x, ##__VA_ARGS__)

/* This template automatically casts entries in the void **args array to the types requested by the kernel func.
 * Since kernel parameters are passed as void ** to the device, this is the closest that we have to type safety. */
#define oneapi_template(...) \
  template<ONEAPI_CALL_FOR(ONEAPI_TYP, __VA_ARGS__)> \
  void oneapi_call( \
      KernelGlobalsGPU *kg, \
      sycl::handler &cgh, \
      size_t global_size, \
      size_t local_size, \
      void **args, \
      void (*func)(KernelGlobalsGPU*, size_t, size_t, sycl::handler &, ONEAPI_CALL_FOR(ONEAPI_T, __VA_ARGS__))) \
  { \
        func(kg, \
             global_size, \
             local_size, \
             cgh, \
             ONEAPI_CALL_FOR(ONEAPI_CAST, __VA_ARGS__)); \
  }

oneapi_template(0)
oneapi_template(0, 1)
oneapi_template(0, 1, 2)
oneapi_template(0, 1, 2, 3)
oneapi_template(0, 1, 2, 3, 4)
oneapi_template(0, 1, 2, 3, 4, 5)
oneapi_template(0, 1, 2, 3, 4, 5, 6)
oneapi_template(0, 1, 2, 3, 4, 5, 6, 7)
oneapi_template(0, 1, 2, 3, 4, 5, 6, 7, 8)
oneapi_template(0, 1, 2, 3, 4, 5, 6, 7, 8, 9)
oneapi_template(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10)
oneapi_template(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11)
oneapi_template(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)
oneapi_template(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13)
oneapi_template(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14)
oneapi_template(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)
oneapi_template(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)
oneapi_template(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17)
oneapi_template(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18)
oneapi_template(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19)
oneapi_template(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20)

    /* clang-format on */
