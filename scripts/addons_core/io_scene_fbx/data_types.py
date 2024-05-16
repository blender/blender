# SPDX-FileCopyrightText: 2006-2012 assimp team
# SPDX-FileCopyrightText: 2013 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

BOOL = b'B'[0]
CHAR = b'C'[0]
INT8 = b'Z'[0]
INT16 = b'Y'[0]
INT32 = b'I'[0]
INT64 = b'L'[0]
FLOAT32 = b'F'[0]
FLOAT64 = b'D'[0]
BYTES = b'R'[0]
STRING = b'S'[0]
INT32_ARRAY = b'i'[0]
INT64_ARRAY = b'l'[0]
FLOAT32_ARRAY = b'f'[0]
FLOAT64_ARRAY = b'd'[0]
BOOL_ARRAY = b'b'[0]
BYTE_ARRAY = b'c'[0]

# Some other misc defines
# Known combinations so far - supposed meaning: A = animatable, A+ = animated, U = UserProp
# VALID_NUMBER_FLAGS = {b'A', b'A+', b'AU', b'A+U'}  # Not used...

# array types - actual length may vary (depending on underlying C implementation)!
import array

# For now, bytes and bool are assumed always 1byte.
ARRAY_BOOL = 'b'
ARRAY_BYTE = 'B'

ARRAY_INT32 = None
ARRAY_INT64 = None
for _t in 'ilq':
    size = array.array(_t).itemsize
    if size == 4:
        ARRAY_INT32 = _t
    elif size == 8:
        ARRAY_INT64 = _t
    if ARRAY_INT32 and ARRAY_INT64:
        break
if not ARRAY_INT32:
    raise Exception("Impossible to get a 4-bytes integer type for array!")
if not ARRAY_INT64:
    raise Exception("Impossible to get an 8-bytes integer type for array!")

ARRAY_FLOAT32 = None
ARRAY_FLOAT64 = None
for _t in 'fd':
    size = array.array(_t).itemsize
    if size == 4:
        ARRAY_FLOAT32 = _t
    elif size == 8:
        ARRAY_FLOAT64 = _t
    if ARRAY_FLOAT32 and ARRAY_FLOAT64:
        break
if not ARRAY_FLOAT32:
    raise Exception("Impossible to get a 4-bytes float type for array!")
if not ARRAY_FLOAT64:
    raise Exception("Impossible to get an 8-bytes float type for array!")
