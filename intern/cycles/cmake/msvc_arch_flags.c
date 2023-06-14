/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <isa_availability.h>
#include <stdio.h>

/* The MS CRT defines this */
extern int __isa_available;

const char *get_arch_flags()
{
  if (__isa_available >= __ISA_AVAILABLE_AVX2) {
    return "/arch:AVX2";
  }
  if (__isa_available >= __ISA_AVAILABLE_AVX) {
    return "/arch:AVX";
  }
  return "";
}

int main()
{
  printf("%s\n", get_arch_flags());
  return 0;
}
