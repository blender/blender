/*
 * Copyright 2011-2016 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define __KERNEL_AVX2__
#define __KERNEL_CPU__

#define TEST_CATEGORY_NAME util_avx2

#if (defined(i386) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64)) && \
    defined(__AVX2__)
#  include "util_avxf_test.h"
#endif
