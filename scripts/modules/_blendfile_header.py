# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

'''
This module contains utility classes for reading headers in .blend files.

This is a pure Python implementation of the corresponding C++ code in Blender
in BLO_core_blend_header.hh and BLO_core_bhead.hh.
'''

import os
import struct
import typing

from dataclasses import dataclass


class BlendHeaderError(Exception):
    pass


@dataclass
class BHead4:
    code: bytes
    len: int
    old: int
    SDNAnr: int
    nr: int


@dataclass
class SmallBHead8:
    code: bytes
    len: int
    old: int
    SDNAnr: int
    nr: int


@dataclass
class LargeBHead8:
    code: bytes
    SDNAnr: int
    old: int
    len: int
    nr: int


@dataclass
class BlockHeaderStruct:
    # Binary format of the encoded header.
    struct: struct.Struct
    # Corresponding Python type for retrieving block header values.
    type: typing.Type[typing.Union[BHead4, SmallBHead8, LargeBHead8]]

    @property
    def size(self) -> int:
        return self.struct.size

    def parse(self, data: bytes) -> typing.Union[BHead4, SmallBHead8, LargeBHead8]:
        return self.type(*self.struct.unpack(data))


class BlendFileHeader:
    """
    BlendFileHeader represents the first 12-17 bytes of a blend file.

    It contains information about the hardware architecture, which is relevant
    to the structure of the rest of the file.
    """

    # Always 'BLENDER'.
    magic: bytes
    # Currently always 0 or 1.
    file_format_version: int
    # Either 4 or 8.
    pointer_size: int
    # Endianness of values stored in the file.
    is_little_endian: bool
    # Blender version the file has been written with.
    # The last two digits are the minor version. So 280 is 2.80.
    version: int

    def __init__(self, file: typing.IO[bytes]) -> None:
        file.seek(0, os.SEEK_SET)

        bytes_0_6 = file.read(7)
        if bytes_0_6 != b'BLENDER':
            raise BlendHeaderError("invalid first bytes %r" % bytes_0_6)
        self.magic = bytes_0_6

        byte_7 = file.read(1)
        is_legacy_header = byte_7 in (b'_', b'-')
        if is_legacy_header:
            self.file_format_version = 0
            if byte_7 == b'_':
                self.pointer_size = 4
            elif byte_7 == b'-':
                self.pointer_size = 8
            else:
                raise BlendHeaderError("invalid pointer size %r" % byte_7)
            byte_8 = file.read(1)
            if byte_8 == b'v':
                self.is_little_endian = True
            elif byte_8 == b'V':
                self.is_little_endian = False
            else:
                raise BlendHeaderError("invalid endian indicator %r" % byte_8)
            bytes_9_11 = file.read(3)
            self.version = int(bytes_9_11)
        else:
            byte_8 = file.read(1)
            header_size = int(byte_7 + byte_8)
            if header_size != 17:
                raise BlendHeaderError("unknown file header size %d" % header_size)
            byte_9 = file.read(1)
            if byte_9 != b'-':
                raise BlendHeaderError("invalid file header")
            self.pointer_size = 8
            byte_10_11 = file.read(2)
            self.file_format_version = int(byte_10_11)
            if self.file_format_version != 1:
                raise BlendHeaderError("unsupported file format version %r" % self.file_format_version)
            byte_12 = file.read(1)
            if byte_12 != b'v':
                raise BlendHeaderError("invalid file header")
            self.is_little_endian = True
            byte_13_16 = file.read(4)
            self.version = int(byte_13_16)

    def create_block_header_struct(self) -> BlockHeaderStruct:
        assert self.file_format_version in (0, 1)
        endian_str = b'<' if self.is_little_endian else b'>'
        if self.file_format_version == 1:
            header_struct = struct.Struct(b''.join((
                endian_str,
                # LargeBHead8.code
                b'4s',
                # LargeBHead8.SDNAnr
                b'i',
                # LargeBHead8.old
                b'Q',
                # LargeBHead8.len
                b'q',
                # LargeBHead8.nr
                b'q',
            )))
            return BlockHeaderStruct(header_struct, LargeBHead8)

        if self.pointer_size == 4:
            header_struct = struct.Struct(b''.join((
                endian_str,
                # BHead4.code
                b'4s',
                # BHead4.len
                b'i',
                # BHead4.old
                b'I',
                # BHead4.SDNAnr
                b'i',
                # BHead4.nr
                b'i',
            )))
            return BlockHeaderStruct(header_struct, BHead4)

        assert self.pointer_size == 8
        header_struct = struct.Struct(b''.join((
            endian_str,
            # SmallBHead8.code
            b'4s',
            # SmallBHead8.len
            b'i',
            # SmallBHead8.old
            b'Q',
            # SmallBHead8.SDNAnr
            b'i',
            # SmallBHead8.nr
            b'i',
        )))
        return BlockHeaderStruct(header_struct, SmallBHead8)


class BlockHeader:
    """
    A .blend file consists of a sequence of blocks whereby each block has a header.
    This class can parse a header block in a specific .blend file.

    Note the binary representation of this header is different for different files.
    This class provides a unified interface for these underlying representations.
    """

    __slots__ = (
        "code",
        "size",
        "addr_old",
        "sdna_index",
        "count",
    )

    # Indicates the type of the block. See BLO_CODE_* in BLO_core_bhead.hh.
    code: bytes
    # Number of bytes in the block.
    size: int
    # Old pointer/identifier of the block.
    addr_old: int
    # DNA struct index of the data in the block.
    sdna_index: int
    # Number of DNA structures in the block.
    count: int

    def __init__(self, file: typing.IO[bytes], block_header_struct: BlockHeaderStruct) -> None:
        data = file.read(block_header_struct.size)

        if len(data) != block_header_struct.size:
            if len(data) != 8:
                raise BlendHeaderError("invalid block header size")
            legacy_endb = struct.Struct(b'4sI')
            endb_header = legacy_endb.unpack(data)
            if endb_header[0] != b'ENDB':
                raise BlendHeaderError("invalid block header")
            self.code = b'ENDB'
            self.size = 0
            self.addr_old = 0
            self.sdna_index = 0
            self.count = 0
            return

        header = block_header_struct.parse(data)
        self.code = header.code.partition(b'\0')[0]
        self.size = header.len
        self.addr_old = header.old
        self.sdna_index = header.SDNAnr
        self.count = header.nr
