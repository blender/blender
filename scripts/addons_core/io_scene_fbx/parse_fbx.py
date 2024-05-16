# SPDX-FileCopyrightText: 2006-2012 assimp team
# SPDX-FileCopyrightText: 2013 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "parse",
    "data_types",
    "parse_version",
    "FBXElem",
)

from struct import unpack
import array
import zlib
from io import BytesIO

from . import data_types
from .fbx_utils_threading import MultiThreadedTaskConsumer

# at the end of each nested block, there is a NUL record to indicate
# that the sub-scope exists (i.e. to distinguish between P: and P : {})
_BLOCK_SENTINEL_LENGTH = ...
_BLOCK_SENTINEL_DATA = ...
read_fbx_elem_start = ...
_IS_BIG_ENDIAN = (__import__("sys").byteorder != 'little')
_HEAD_MAGIC = b'Kaydara FBX Binary\x20\x20\x00\x1a\x00'
from collections import namedtuple
FBXElem = namedtuple("FBXElem", ("id", "props", "props_type", "elems"))
del namedtuple


def read_uint(read):
    return unpack(b'<I', read(4))[0]


def read_ubyte(read):
    return unpack(b'B', read(1))[0]


def read_string_ubyte(read):
    size = read_ubyte(read)
    data = read(size)
    return data


def read_array_params(read):
    return unpack(b'<III', read(12))


def read_elem_start32(read):
    end_offset, prop_count, _prop_length, elem_id_size = unpack(b'<IIIB', read(13))
    elem_id = read(elem_id_size) if elem_id_size else b""
    return end_offset, prop_count, elem_id


def read_elem_start64(read):
    end_offset, prop_count, _prop_length, elem_id_size = unpack(b'<QQQB', read(25))
    elem_id = read(elem_id_size) if elem_id_size else b""
    return end_offset, prop_count, elem_id


def _create_array(data, length, array_type, array_stride, array_byteswap):
    """Create an array from FBX data."""
    # If size of the data does not match the expected size of the array, then something is wrong with the code or the
    # FBX file.
    assert(length * array_stride == len(data))

    data_array = array.array(array_type, data)
    if array_byteswap and _IS_BIG_ENDIAN:
        data_array.byteswap()
    return data_array


def _decompress_and_insert_array(elem_props_data, index_to_set, compressed_array_args):
    """Decompress array data and insert the created array into the FBX tree being parsed.

    This is usually called from a separate thread to the main thread."""
    compressed_data, length, array_type, array_stride, array_byteswap = compressed_array_args

    # zlib.decompress releases the Global Interpreter Lock, so another thread can run code while waiting for the
    # decompression to complete.
    data = zlib.decompress(compressed_data, bufsize=length * array_stride)

    # Create and insert the array into the parsed FBX hierarchy.
    elem_props_data[index_to_set] = _create_array(data, length, array_type, array_stride, array_byteswap)


def unpack_array(read, array_type, array_stride, array_byteswap):
    """Unpack an array from an FBX file being parsed.

    If the array data is compressed, the compressed data is combined with the other arguments into a tuple to prepare
    for decompressing on a separate thread if possible.

    If the array data is not compressed, the array is created.

    Returns (tuple, True) or (array, False)."""
    length, encoding, comp_len = read_array_params(read)

    data = read(comp_len)

    if encoding == 1:
        # Array data requires decompression, which is done in a separate thread if possible.
        return (data, length, array_type, array_stride, array_byteswap), True
    else:
        return _create_array(data, length, array_type, array_stride, array_byteswap), False


read_array_dict = {
    b'b'[0]: lambda read: unpack_array(read, data_types.ARRAY_BOOL, 1, False),     # bool
    b'c'[0]: lambda read: unpack_array(read, data_types.ARRAY_BYTE, 1, False),     # ubyte
    b'i'[0]: lambda read: unpack_array(read, data_types.ARRAY_INT32, 4, True),     # int
    b'l'[0]: lambda read: unpack_array(read, data_types.ARRAY_INT64, 8, True),     # long
    b'f'[0]: lambda read: unpack_array(read, data_types.ARRAY_FLOAT32, 4, False),  # float
    b'd'[0]: lambda read: unpack_array(read, data_types.ARRAY_FLOAT64, 8, False),  # double
}

read_data_dict = {
    b'Z'[0]: lambda read: unpack(b'<b', read(1))[0],  # byte
    b'Y'[0]: lambda read: unpack(b'<h', read(2))[0],  # 16 bit int
    b'B'[0]: lambda read: unpack(b'?', read(1))[0],   # 1 bit bool (yes/no)
    b'C'[0]: lambda read: unpack(b'<c', read(1))[0],  # char
    b'I'[0]: lambda read: unpack(b'<i', read(4))[0],  # 32 bit int
    b'F'[0]: lambda read: unpack(b'<f', read(4))[0],  # 32 bit float
    b'D'[0]: lambda read: unpack(b'<d', read(8))[0],  # 64 bit float
    b'L'[0]: lambda read: unpack(b'<q', read(8))[0],  # 64 bit int
    b'R'[0]: lambda read: read(read_uint(read)),      # binary data
    b'S'[0]: lambda read: read(read_uint(read)),      # string data
}


# FBX 7500 (aka FBX2016) introduces incompatible changes at binary level:
#   * The NULL block marking end of nested stuff switches from 13 bytes long to 25 bytes long.
#   * The FBX element metadata (end_offset, prop_count and prop_length) switch from uint32 to uint64.
def init_version(fbx_version):
    global _BLOCK_SENTINEL_LENGTH, _BLOCK_SENTINEL_DATA, read_fbx_elem_start

    _BLOCK_SENTINEL_LENGTH = ...
    _BLOCK_SENTINEL_DATA = ...

    if fbx_version < 7500:
        _BLOCK_SENTINEL_LENGTH = 13
        read_fbx_elem_start = read_elem_start32
    else:
        _BLOCK_SENTINEL_LENGTH = 25
        read_fbx_elem_start = read_elem_start64
    _BLOCK_SENTINEL_DATA = (b'\0' * _BLOCK_SENTINEL_LENGTH)


def read_elem(read, tell, use_namedtuple, decompress_array_func, tell_file_offset=0):
    # [0] the offset at which this block ends
    # [1] the number of properties in the scope
    # [2] the length of the property list
    # [3] elem name length
    # [4] elem name of the scope/key
    # read_fbx_elem_start does not return [2] because we don't use it and does not return [3] because it is only used to
    # get [4].
    end_offset, prop_count, elem_id = read_fbx_elem_start(read)
    if end_offset == 0:
        return None

    elem_props_type = bytearray(prop_count)  # elem property types
    elem_props_data = [None] * prop_count    # elem properties (if any)
    elem_subtree = []                        # elem children (if any)

    for i in range(prop_count):
        data_type = read(1)[0]
        if data_type in read_array_dict:
            val, needs_decompression = read_array_dict[data_type](read)
            if needs_decompression:
                # Array decompression releases the GIL, so can be multithreaded (if possible on the current system) for
                # performance.
                # After decompressing, the array is inserted into elem_props_data[i].
                decompress_array_func(elem_props_data, i, val)
            else:
                elem_props_data[i] = val
        else:
            elem_props_data[i] = read_data_dict[data_type](read)
        elem_props_type[i] = data_type

    pos = tell()
    local_end_offset = end_offset - tell_file_offset

    if pos < local_end_offset:
        # The default BufferedReader used when `open()`-ing files in 'rb' mode has to get the raw stream position from
        # the OS every time its tell() function is called. This is about 10 times slower than the tell() function of
        # BytesIO objects, so reading chunks of bytes from the file into memory at once and exposing them through
        # BytesIO can give better performance. We know the total size of each element's subtree so can read entire
        # subtrees into memory at a time.
        # The "Objects" element's subtree, however, usually makes up most of the file, so we specifically avoid reading
        # all its sub-elements into memory at once to reduce memory requirements at the cost of slightly worse
        # performance when memory is not a concern.
        # If we're currently reading directly from the opened file, then tell_file_offset will be zero.
        if tell_file_offset == 0 and elem_id != b"Objects":
            block_bytes_remaining = local_end_offset - pos

            # Read the entire subtree
            sub_elem_bytes = read(block_bytes_remaining)
            num_bytes_read = len(sub_elem_bytes)
            if num_bytes_read != block_bytes_remaining:
                raise IOError("failed to read complete nested block, expected %i bytes, but only got %i"
                              % (block_bytes_remaining, num_bytes_read))

            # BytesIO provides IO API for reading bytes in memory, so we can use the same code as reading bytes directly
            # from a file.
            f = BytesIO(sub_elem_bytes)
            tell = f.tell
            read = f.read
            # The new `tell` function starts at zero and is offset by `pos` bytes from the start of the file.
            start_sub_pos = 0
            tell_file_offset = pos
            sub_tree_end = block_bytes_remaining - _BLOCK_SENTINEL_LENGTH
        else:
            # The `tell` function is unchanged, so starts at the value returned by `tell()`, which is still `pos`
            # because no reads have been made since then.
            start_sub_pos = pos
            sub_tree_end = local_end_offset - _BLOCK_SENTINEL_LENGTH

        sub_pos = start_sub_pos
        while sub_pos < sub_tree_end:
            elem_subtree.append(read_elem(read, tell, use_namedtuple, decompress_array_func, tell_file_offset))
            sub_pos = tell()

        # At the end of each subtree there should be a sentinel (an empty element with all bytes set to zero).
        if read(_BLOCK_SENTINEL_LENGTH) != _BLOCK_SENTINEL_DATA:
            raise IOError("failed to read nested block sentinel, "
                          "expected all bytes to be 0")

        # Update `pos` for the number of bytes that have been read.
        pos += (sub_pos - start_sub_pos) + _BLOCK_SENTINEL_LENGTH

    if pos != local_end_offset:
        raise IOError("scope length not reached, something is wrong")

    args = (elem_id, elem_props_data, elem_props_type, elem_subtree)
    return FBXElem(*args) if use_namedtuple else args


def parse_version(fn):
    """
    Return the FBX version,
    if the file isn't a binary FBX return zero.
    """
    with open(fn, 'rb') as f:
        read = f.read

        if read(len(_HEAD_MAGIC)) != _HEAD_MAGIC:
            return 0

        return read_uint(read)


def parse(fn, use_namedtuple=True):
    root_elems = []

    multithread_decompress_array_cm = MultiThreadedTaskConsumer.new_cpu_bound_cm(_decompress_and_insert_array)
    with open(fn, 'rb') as f, multithread_decompress_array_cm as decompress_array_func:
        read = f.read
        tell = f.tell

        if read(len(_HEAD_MAGIC)) != _HEAD_MAGIC:
            raise IOError("Invalid header")

        fbx_version = read_uint(read)
        init_version(fbx_version)

        while True:
            elem = read_elem(read, tell, use_namedtuple, decompress_array_func)
            if elem is None:
                break
            root_elems.append(elem)

    args = (b'', [], bytearray(0), root_elems)
    return FBXElem(*args) if use_namedtuple else args, fbx_version
