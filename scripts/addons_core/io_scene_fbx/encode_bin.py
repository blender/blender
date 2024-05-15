# SPDX-FileCopyrightText: 2013 Campbell Barton
#
# SPDX-License-Identifier: GPL-2.0-or-later

try:
    from . import data_types
    from .fbx_utils_threading import MultiThreadedTaskConsumer
except:
    import data_types
    from fbx_utils_threading import MultiThreadedTaskConsumer

from struct import pack
from contextlib import contextmanager
import array
import numpy as np
import zlib

_BLOCK_SENTINEL_LENGTH = ...
_BLOCK_SENTINEL_DATA = ...
_ELEM_META_FORMAT = ...
_ELEM_META_SIZE = ...
_IS_BIG_ENDIAN = (__import__("sys").byteorder != 'little')
_HEAD_MAGIC = b'Kaydara FBX Binary\x20\x20\x00\x1a\x00'

# fbx has very strict CRC rules, all based on file timestamp
# until we figure these out, write files at a fixed time. (workaround!)

# Assumes: CreationTime
_TIME_ID = b'1970-01-01 10:00:00:000'
_FILE_ID = b'\x28\xb3\x2a\xeb\xb6\x24\xcc\xc2\xbf\xc8\xb0\x2a\xa9\x2b\xfc\xf1'
_FOOT_ID = b'\xfa\xbc\xab\x09\xd0\xc8\xd4\x66\xb1\x76\xfb\x83\x1c\xf7\x26\x7e'

# Awful exceptions: those "classes" of elements seem to need block sentinel even when having no children and some props.
_ELEMS_ID_ALWAYS_BLOCK_SENTINEL = {b"AnimationStack", b"AnimationLayer"}


class FBXElem:
    __slots__ = (
        "id",
        "props",
        "props_type",
        "elems",

        "_props_length",  # combine length of props
        "_end_offset",  # byte offset from the start of the file.
    )

    def __init__(self, id):
        assert(len(id) < 256)  # length must fit in a uint8
        self.id = id
        self.props = []
        self.props_type = bytearray()
        self.elems = []
        self._end_offset = -1
        self._props_length = -1

    @classmethod
    @contextmanager
    def enable_multithreading_cm(cls):
        """Temporarily enable multithreaded array compression.

        The context manager handles starting up and shutting down the threads.

        Only exits once all the threads are done (either all tasks were completed or an error occurred and the threads
        were stopped prematurely).

        Writing to a file is temporarily disabled as a safeguard."""
        # __enter__()
        orig_func = cls._add_compressed_array_helper
        orig_write = cls._write

        def insert_compressed_array(props, insert_at, data, length):
            # zlib.compress releases the GIL, so can be multithreaded.
            data = zlib.compress(data, 1)
            comp_len = len(data)

            encoding = 1
            data = pack('<3I', length, encoding, comp_len) + data
            props[insert_at] = data

        with MultiThreadedTaskConsumer.new_cpu_bound_cm(insert_compressed_array) as wrapped_func:
            try:
                def _add_compressed_array_helper_multi(self, data, length):
                    # Append a dummy value that will be replaced with the compressed array data later.
                    self.props.append(...)
                    # The index to insert the compressed array into.
                    insert_at = len(self.props) - 1
                    # Schedule the array to be compressed on a separate thread and then inserted into the hierarchy at
                    # `insert_at`.
                    wrapped_func(self.props, insert_at, data, length)

                # As an extra safeguard, temporarily replace the `_write` function to raise an error if called.
                def temp_write(*_args, **_kwargs):
                    raise RuntimeError("Writing is not allowed until multithreaded array compression has been disabled")

                cls._add_compressed_array_helper = _add_compressed_array_helper_multi
                cls._write = temp_write

                # Return control back to the caller of __enter__().
                yield
            finally:
                # __exit__()
                # Restore the original functions.
                cls._add_compressed_array_helper = orig_func
                cls._write = orig_write
            # Exiting the MultiThreadedTaskConsumer context manager will wait for all scheduled tasks to complete.

    def add_bool(self, data):
        assert(isinstance(data, bool))
        data = pack('?', data)

        self.props_type.append(data_types.BOOL)
        self.props.append(data)

    def add_char(self, data):
        assert(isinstance(data, bytes))
        assert(len(data) == 1)
        data = pack('<c', data)

        self.props_type.append(data_types.CHAR)
        self.props.append(data)

    def add_int8(self, data):
        assert(isinstance(data, int))
        data = pack('<b', data)

        self.props_type.append(data_types.INT8)
        self.props.append(data)

    def add_int16(self, data):
        assert(isinstance(data, int))
        data = pack('<h', data)

        self.props_type.append(data_types.INT16)
        self.props.append(data)

    def add_int32(self, data):
        assert(isinstance(data, int))
        data = pack('<i', data)

        self.props_type.append(data_types.INT32)
        self.props.append(data)

    def add_int64(self, data):
        assert(isinstance(data, int))
        data = pack('<q', data)

        self.props_type.append(data_types.INT64)
        self.props.append(data)

    def add_float32(self, data):
        assert(isinstance(data, float))
        data = pack('<f', data)

        self.props_type.append(data_types.FLOAT32)
        self.props.append(data)

    def add_float64(self, data):
        assert(isinstance(data, float))
        data = pack('<d', data)

        self.props_type.append(data_types.FLOAT64)
        self.props.append(data)

    def add_bytes(self, data):
        assert(isinstance(data, bytes))
        data = pack('<I', len(data)) + data

        self.props_type.append(data_types.BYTES)
        self.props.append(data)

    def add_string(self, data):
        assert(isinstance(data, bytes))
        data = pack('<I', len(data)) + data

        self.props_type.append(data_types.STRING)
        self.props.append(data)

    def add_string_unicode(self, data):
        assert(isinstance(data, str))
        data = data.encode('utf8')
        data = pack('<I', len(data)) + data

        self.props_type.append(data_types.STRING)
        self.props.append(data)

    def _add_compressed_array_helper(self, data, length):
        """Note: This function may be swapped out by enable_multithreading_cm with an equivalent that supports
        multithreading."""
        data = zlib.compress(data, 1)
        comp_len = len(data)

        encoding = 1
        data = pack('<3I', length, encoding, comp_len) + data
        self.props.append(data)

    def _add_array_helper(self, data, prop_type, length):
        self.props_type.append(prop_type)
        # mimic behavior of fbxconverter (also common sense)
        # we could make this configurable.
        encoding = 0 if len(data) <= 128 else 1
        if encoding == 0:
            data = pack('<3I', length, encoding, len(data)) + data
            self.props.append(data)
        elif encoding == 1:
            self._add_compressed_array_helper(data, length)

    def _add_parray_helper(self, data, array_type, prop_type):
        assert (isinstance(data, array.array))
        assert (data.typecode == array_type)

        length = len(data)

        if _IS_BIG_ENDIAN:
            data = data[:]
            data.byteswap()
        data = data.tobytes()

        self._add_array_helper(data, prop_type, length)

    def _add_ndarray_helper(self, data, dtype, prop_type):
        assert (isinstance(data, np.ndarray))
        assert (data.dtype == dtype)

        length = data.size

        if _IS_BIG_ENDIAN and data.dtype.isnative:
            data = data.byteswap()
        data = data.tobytes()

        self._add_array_helper(data, prop_type, length)

    def add_int32_array(self, data):
        if isinstance(data, np.ndarray):
            self._add_ndarray_helper(data, np.int32, data_types.INT32_ARRAY)
        else:
            if not isinstance(data, array.array):
                data = array.array(data_types.ARRAY_INT32, data)
            self._add_parray_helper(data, data_types.ARRAY_INT32, data_types.INT32_ARRAY)

    def add_int64_array(self, data):
        if isinstance(data, np.ndarray):
            self._add_ndarray_helper(data, np.int64, data_types.INT64_ARRAY)
        else:
            if not isinstance(data, array.array):
                data = array.array(data_types.ARRAY_INT64, data)
            self._add_parray_helper(data, data_types.ARRAY_INT64, data_types.INT64_ARRAY)

    def add_float32_array(self, data):
        if isinstance(data, np.ndarray):
            self._add_ndarray_helper(data, np.float32, data_types.FLOAT32_ARRAY)
        else:
            if not isinstance(data, array.array):
                data = array.array(data_types.ARRAY_FLOAT32, data)
            self._add_parray_helper(data, data_types.ARRAY_FLOAT32, data_types.FLOAT32_ARRAY)

    def add_float64_array(self, data):
        if isinstance(data, np.ndarray):
            self._add_ndarray_helper(data, np.float64, data_types.FLOAT64_ARRAY)
        else:
            if not isinstance(data, array.array):
                data = array.array(data_types.ARRAY_FLOAT64, data)
            self._add_parray_helper(data, data_types.ARRAY_FLOAT64, data_types.FLOAT64_ARRAY)

    def add_bool_array(self, data):
        if isinstance(data, np.ndarray):
            self._add_ndarray_helper(data, bool, data_types.BOOL_ARRAY)
        else:
            if not isinstance(data, array.array):
                data = array.array(data_types.ARRAY_BOOL, data)
            self._add_parray_helper(data, data_types.ARRAY_BOOL, data_types.BOOL_ARRAY)

    def add_byte_array(self, data):
        if isinstance(data, np.ndarray):
            self._add_ndarray_helper(data, np.byte, data_types.BYTE_ARRAY)
        else:
            if not isinstance(data, array.array):
                data = array.array(data_types.ARRAY_BYTE, data)
            self._add_parray_helper(data, data_types.ARRAY_BYTE, data_types.BYTE_ARRAY)

    # -------------------------
    # internal helper functions

    def _calc_offsets(self, offset, is_last):
        """
        Call before writing, calculates fixed offsets.
        """
        assert(self._end_offset == -1)
        assert(self._props_length == -1)

        offset += _ELEM_META_SIZE  # 3 uints (or 3 ulonglongs for FBX 7500 and later)
        offset += 1 + len(self.id)  # len + idname

        props_length = 0
        for data in self.props:
            # 1 byte for the prop type
            props_length += 1 + len(data)
        self._props_length = props_length
        offset += props_length

        offset = self._calc_offsets_children(offset, is_last)

        self._end_offset = offset
        return offset

    def _calc_offsets_children(self, offset, is_last):
        if self.elems:
            elem_last = self.elems[-1]
            for elem in self.elems:
                offset = elem._calc_offsets(offset, (elem is elem_last))
            offset += _BLOCK_SENTINEL_LENGTH
        elif (not self.props and not is_last) or self.id in _ELEMS_ID_ALWAYS_BLOCK_SENTINEL:
            offset += _BLOCK_SENTINEL_LENGTH

        return offset

    def _write(self, write, tell, is_last):
        assert(self._end_offset != -1)
        assert(self._props_length != -1)

        write(pack(_ELEM_META_FORMAT, self._end_offset, len(self.props), self._props_length))

        write(bytes((len(self.id),)))
        write(self.id)

        for i, data in enumerate(self.props):
            write(bytes((self.props_type[i],)))
            write(data)

        self._write_children(write, tell, is_last)

        if tell() != self._end_offset:
            raise IOError("scope length not reached, "
                          "something is wrong (%d)" % (self._end_offset - tell()))

    def _write_children(self, write, tell, is_last):
        if self.elems:
            elem_last = self.elems[-1]
            for elem in self.elems:
                assert(elem.id != b'')
                elem._write(write, tell, (elem is elem_last))
            write(_BLOCK_SENTINEL_DATA)
        elif (not self.props and not is_last) or self.id in _ELEMS_ID_ALWAYS_BLOCK_SENTINEL:
            write(_BLOCK_SENTINEL_DATA)


def _write_timedate_hack(elem_root):
    # perform 2 changes
    # - set the FileID
    # - set the CreationTime

    ok = 0
    for elem in elem_root.elems:
        if elem.id == b'FileId':
            assert(elem.props_type[0] == b'R'[0])
            assert(len(elem.props_type) == 1)
            elem.props.clear()
            elem.props_type.clear()

            elem.add_bytes(_FILE_ID)
            ok += 1
        elif elem.id == b'CreationTime':
            assert(elem.props_type[0] == b'S'[0])
            assert(len(elem.props_type) == 1)
            elem.props.clear()
            elem.props_type.clear()

            elem.add_string(_TIME_ID)
            ok += 1

        if ok == 2:
            break

    if ok != 2:
        print("Missing fields!")


# FBX 7500 (aka FBX2016) introduces incompatible changes at binary level:
#   * The NULL block marking end of nested stuff switches from 13 bytes long to 25 bytes long.
#   * The FBX element metadata (end_offset, prop_count and prop_length) switch from uint32 to uint64.
def init_version(fbx_version):
    global _BLOCK_SENTINEL_LENGTH, _BLOCK_SENTINEL_DATA, _ELEM_META_FORMAT, _ELEM_META_SIZE

    _BLOCK_SENTINEL_LENGTH = ...
    _BLOCK_SENTINEL_DATA = ...
    _ELEM_META_FORMAT = ...
    _ELEM_META_SIZE = ...

    if fbx_version < 7500:
        _ELEM_META_FORMAT = '<3I'
        _ELEM_META_SIZE = 12
    else:
        _ELEM_META_FORMAT = '<3Q'
        _ELEM_META_SIZE = 24
    _BLOCK_SENTINEL_LENGTH = _ELEM_META_SIZE + 1
    _BLOCK_SENTINEL_DATA = (b'\0' * _BLOCK_SENTINEL_LENGTH)


def write(fn, elem_root, version):
    assert(elem_root.id == b'')

    with open(fn, 'wb') as f:
        write = f.write
        tell = f.tell

        init_version(version)

        write(_HEAD_MAGIC)
        write(pack('<I', version))

        # hack since we don't decode time.
        # ideally we would _not_ modify this data.
        _write_timedate_hack(elem_root)

        elem_root._calc_offsets_children(tell(), False)
        elem_root._write_children(write, tell, False)

        write(_FOOT_ID)
        write(b'\x00' * 4)

        # padding for alignment (values between 1 & 16 observed)
        # if already aligned to 16, add a full 16 bytes padding.
        ofs = tell()
        pad = ((ofs + 15) & ~15) - ofs
        if pad == 0:
            pad = 16

        write(b'\0' * pad)

        write(pack('<I', version))

        # unknown magic (always the same)
        write(b'\0' * 120)
        write(b'\xf8\x5a\x8c\x6a\xde\xf5\xd9\x7e\xec\xe9\x0c\xe3\x75\x8f\x29\x0b')
