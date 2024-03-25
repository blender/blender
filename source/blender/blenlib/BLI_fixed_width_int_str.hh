/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** Conversions to and from strings use GMP internally currently. */
#ifdef WITH_GMP

#  include <gmpxx.h>

#  include "BLI_array.hh"
#  include "BLI_fixed_width_int.hh"

namespace blender::fixed_width_int {

template<typename T, int S>
inline void UIntF<T, S>::set_from_str(const StringRefNull str, const int base)
{
  mpz_t x;
  mpz_init(x);
  mpz_set_str(x, str.c_str(), base);
  for (int i = 0; i < S; i++) {
    static_assert(sizeof(T) <= sizeof(decltype(mpz_get_ui(x))));
    this->v[i] = T(mpz_get_ui(x));
    mpz_div_2exp(x, x, 8 * sizeof(T));
  }
  mpz_clear(x);
}

template<typename T, int S>
inline void IntF<T, S>::set_from_str(const StringRefNull str, const int base)
{
  if (str[0] == '-') {
    const UIntF<T, S> unsigned_value(str.c_str() + 1, base);
    this->v = unsigned_value.v;
    *this = -*this;
  }
  else {
    const UIntF<T, S> unsigned_value(str.c_str(), base);
    this->v = unsigned_value.v;
  }
}

template<typename T, int S> inline std::string UIntF<T, S>::to_string(const int base) const
{
  mpz_t x;
  mpz_init(x);
  for (int i = S - 1; i >= 0; i--) {
    static_assert(sizeof(T) <= sizeof(decltype(mpz_get_ui(x))));
    mpz_mul_2exp(x, x, 8 * sizeof(T));
    mpz_add_ui(x, x, this->v[i]);
  }
  /* Add 2 because of possible +/- sign and null terminator. */
  /* Also see https://gmplib.org/manual/Converting-Integers. */
  const int str_size = mpz_sizeinbase(x, base) + 2;
  Array<char, 1024> str(str_size);
  mpz_get_str(str.data(), base, x);
  mpz_clear(x);
  return std::string(str.data());
}

template<typename T, int S> inline std::string IntF<T, S>::to_string(const int base) const
{
  if (is_negative(*this)) {
    std::string str = UIntF<T, S>(-*this);
    str.insert(str.begin(), '-');
    return str;
  }
  return UIntF<T, S>(*this).to_string();
}

}  // namespace blender::fixed_width_int

#endif /* WITH_GMP */
