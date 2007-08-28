/*
 * SOLID - Software Library for Interference Detection
 * 
 * Copyright (C) 2001-2003  Dtecta.  All rights reserved.
 *
 * This library may be distributed under the terms of the Q Public License
 * (QPL) as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This library may be distributed and/or modified under the terms of the
 * GNU General Public License (GPL) version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This library is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Commercial use or any other use of this library not covered by either 
 * the QPL or the GPL requires an additional license from Dtecta. 
 * Please contact info@dtecta.com for enquiries about the terms of commercial
 * use of this library.
 */

#ifndef GEN_RANDOM_H
#define GEN_RANDOM_H

#ifdef MT19937

#include <limits.h>
#include <mt19937.h>

#define GEN_RAND_MAX UINT_MAX

inline void         GEN_srand(unsigned int seed) { init_genrand(seed); }
inline unsigned int GEN_rand()                   { return genrand_int32(); }

#else

#include <stdlib.h>

#define GEN_RAND_MAX RAND_MAX

inline void         GEN_srand(unsigned int seed) { srand(seed); } 
inline unsigned int GEN_rand()                   { return rand(); }

#endif

#endif

