# SPDX-FileCopyrightText: 2018-2024 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import numpy as np


def fast_structured_np_unique(arr, *args, **kwargs):
    """
    np.unique optimized for structured arrays when a sorted result is not required.

    np.unique works through sorting, but sorting a structured array requires as many sorts as there are fields in the
    structured dtype.

    By viewing the array as a single non-structured dtype that sorts according to its bytes, unique elements can be
    found with a single sort. Since the values are viewed as a different type to their original, this means that the
    returned array of unique values may not be sorted according to their original type.

    Float field caveats:
    All elements of -0.0 in the input array will be replaced with 0.0 to ensure that both values are collapsed into one.
    NaN values can have lots of different byte representations (e.g. signaling/quiet and custom payloads). Only the
    duplicates of each unique byte representation will be collapsed into one.

    Nested structured dtypes are not supported.
    The behavior of structured dtypes with overlapping fields is undefined.
    """
    structured_dtype = arr.dtype
    fields = structured_dtype.fields
    if fields is None:
        raise RuntimeError('%s is not a structured dtype' % structured_dtype)

    for field_name, (field_dtype, *_offset_and_optional_title) in fields.items():
        if field_dtype.subdtype is not None:
            raise RuntimeError('Nested structured types are not supported in %s' % structured_dtype)
        if field_dtype.kind == 'f':
            # Replace all -0.0 in the array with 0.0 because -0.0 and 0.0 have different byte representations.
            arr[field_name][arr[field_name] == -0.0] = 0.0
        elif field_dtype.kind not in "iuUSV":
            # Signed integer, unsigned integer, unicode string, byte string (bytes) and raw bytes (void) can be left
            # as they are. Everything else is unsupported.
            raise RuntimeError('Unsupported structured field type %s for field %s' % (field_dtype, field_name))

    structured_itemsize = structured_dtype.itemsize

    # Integer types sort the fastest, but are only available for specific itemsizes.
    uint_dtypes_by_itemsize = {1: np.uint8, 2: np.uint16, 4: np.uint32, 8: np.uint64}
    # Signed/unsigned makes no noticeable speed difference, but using unsigned will result in ordering according to
    # individual bytes like the other, non-integer types.
    if structured_itemsize in uint_dtypes_by_itemsize:
        entire_structure_dtype = uint_dtypes_by_itemsize[structured_itemsize]
    else:
        # Construct a flexible size dtype with matching itemsize to the entire structured dtype.
        # Should always be 4 because each character in a unicode string is UCS4.
        str_itemsize = np.dtype((np.str_, 1)).itemsize
        if structured_itemsize % str_itemsize == 0:
            # Unicode strings seem to be slightly faster to sort than bytes.
            entire_structure_dtype = np.dtype((np.str_, structured_itemsize // str_itemsize))
        else:
            # Bytes seem to be slightly faster to sort than raw bytes (np.void).
            entire_structure_dtype = np.dtype((np.bytes_, structured_itemsize))

    result = np.unique(arr.view(entire_structure_dtype), *args, **kwargs)

    unique = result[0] if isinstance(result, tuple) else result
    # View in the original dtype.
    unique = unique.view(arr.dtype)
    if isinstance(result, tuple):
        return (unique,) + result[1:]
    else:
        return unique
