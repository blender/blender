# $Id$
#
# --------------------------------------------------------------------------
# helper functions to be used by other scripts
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

import Blender
from Blender.Mathutils import *

# ------ Mersenne Twister - start

# Copyright (C) 1997 Makoto Matsumoto and Takuji Nishimura.
# Any feedback is very welcome. For any question, comments,
# see http://www.math.keio.ac.jp/matumoto/emt.html or email
# matumoto@math.keio.ac.jp

# The link above is dead, this is the new one:
# http://www.math.sci.hiroshima-u.ac.jp/m-mat/MT/emt.html
# And here the license info, from Mr. Matsumoto's site:
# Until 2001/4/6, MT had been distributed under GNU Public License,
# but after 2001/4/6, we decided to let MT be used for any purpose, including
# commercial use. 2002-versions mt19937ar.c, mt19937ar-cok.c are considered
# to be usable freely.
#
# So from the year above (1997), this code is under GPL.

# Period parameters
N = 624
M = 397
MATRIX_A = 0x9908b0dfL   # constant vector a
UPPER_MASK = 0x80000000L # most significant w-r bits
LOWER_MASK = 0x7fffffffL # least significant r bits

# Tempering parameters
TEMPERING_MASK_B = 0x9d2c5680L
TEMPERING_MASK_C = 0xefc60000L

def TEMPERING_SHIFT_U(y):
    return (y >> 11)

def TEMPERING_SHIFT_S(y):
    return (y << 7)

def TEMPERING_SHIFT_T(y):
    return (y << 15)

def TEMPERING_SHIFT_L(y):
    return (y >> 18)

mt = []   # the array for the state vector
mti = N+1 # mti==N+1 means mt[N] is not initialized

# initializing the array with a NONZERO seed
def sgenrand(seed):
  # setting initial seeds to mt[N] using
  # the generator Line 25 of Table 1 in
  # [KNUTH 1981, The Art of Computer Programming
  #    Vol. 2 (2nd Ed.), pp102]

  global mt, mti

  mt = []
  
  mt.append(seed & 0xffffffffL)
  for i in xrange(1, N + 1):
    mt.append((69069 * mt[i-1]) & 0xffffffffL)

  mti = i
# end sgenrand


def genrand():
  global mt, mti
  
  mag01 = [0x0L, MATRIX_A]
  # mag01[x] = x * MATRIX_A  for x=0,1
  y = 0
  
  if mti >= N: # generate N words at one time
    if mti == N+1:   # if sgenrand() has not been called,
      sgenrand(4357) # a default initial seed is used

    for kk in xrange((N-M) + 1):
      y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK)
      mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1]

    for kk in xrange(kk, N):
      y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK)
      mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1]

    y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK)
    mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1]

    mti = 0

  y = mt[mti]
  mti += 1
  y ^= TEMPERING_SHIFT_U(y)
  y ^= TEMPERING_SHIFT_S(y) & TEMPERING_MASK_B
  y ^= TEMPERING_SHIFT_T(y) & TEMPERING_MASK_C
  y ^= TEMPERING_SHIFT_L(y)

  return ( float(y) / 0xffffffffL ) # reals

#------ Mersenne Twister -- end 
