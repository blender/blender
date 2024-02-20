# SPDX-FileCopyrightText: 2009 At Mind B.V. - Jeroen Bakker.
# SPDX-FileCopyrightText: 2014 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# -----------------------------------------------------------------------------
# NOTICE: this module is expanded upon in Blender Asset Tracer.
#
# See https://projects.blender.org/blender/blender-asset-tracer
# and https://pypi.org/project/blender-asset-tracer/
# -----------------------------------------------------------------------------

import gzip
import logging
import os
import struct
import tempfile

log = logging.getLogger("blendfile")

FILE_BUFFER_SIZE = 1024 * 1024


class BlendFileError(Exception):
    """Raised when there was an error reading/parsing a blend file."""


def pad_up_4(offset):
    return (offset + 3) & ~3


# -----------------------------------------------------------------------------
# module classes


class BlendFile:
    """
    Blend file.
    """
    __slots__ = (
        # file (result of open())
        "handle",
        # str (original name of the file path)
        "filepath_orig",
        # BlendFileHeader
        "header",
        # struct.Struct
        "block_header_struct",
        # BlendFileBlock
        "blocks",
        # [DNAStruct, ...]
        "structs",
        # dict {b'StructName': sdna_index}
        # (where the index is an index into 'structs')
        "sdna_index_from_id",
        # dict {addr_old: block}
        "block_from_offset",
        # int
        "code_index",
        # bool (did we make a change)
        "is_modified",
        # bool (is file gzipped)
        "is_compressed",
    )

    def __init__(self, handle):
        log.debug("initializing reading blend-file")
        self.handle = handle
        self.header = BlendFileHeader(handle)
        self.block_header_struct = self.header.create_block_header_struct()
        self.blocks = []
        self.code_index = {}
        self.structs = []
        self.sdna_index_from_id = {}

        block = BlendFileBlock(handle, self)
        while block.code != b'ENDB':
            if block.code == b'DNA1':
                (self.structs,
                 self.sdna_index_from_id,
                 ) = BlendFile.decode_structs(self.header, block, handle)
            else:
                handle.seek(block.size, os.SEEK_CUR)

            self.blocks.append(block)
            self.code_index.setdefault(block.code, []).append(block)

            block = BlendFileBlock(handle, self)
        self.is_modified = False
        self.blocks.append(block)

        if not self.structs:
            raise BlendFileError("No DNA1 block in file, this is not a valid .blend file!")

        # Cache (could lazy init, in case we never use?).
        self.block_from_offset = {block.addr_old: block for block in self.blocks if block.code != b'ENDB'}

    def __repr__(self):
        return '<%s %r>' % (self.__class__.__qualname__, self.handle)

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        self.close()

    def find_blocks_from_code(self, code):
        assert type(code) == bytes
        if code not in self.code_index:
            return []
        return self.code_index[code]

    def find_block_from_offset(self, offset):
        # same as looking looping over all blocks,
        # then checking `block.addr_old == offset`.
        assert type(offset) is int
        return self.block_from_offset.get(offset)

    def close(self):
        """
        Close the blend file
        writes the blend file to disk if changes has happened
        """
        handle = self.handle

        if self.is_modified:
            if self.is_compressed:
                log.debug("close compressed blend file")
                handle.seek(os.SEEK_SET, 0)
                log.debug("compressing started")
                fs = gzip.open(self.filepath_orig, "wb")
                data = handle.read(FILE_BUFFER_SIZE)
                while data:
                    fs.write(data)
                    data = handle.read(FILE_BUFFER_SIZE)
                fs.close()
                log.debug("compressing finished")

        handle.close()

    def ensure_subtype_smaller(self, sdna_index_curr, sdna_index_next):
        # never refine to a smaller type
        if (self.structs[sdna_index_curr].size >
                self.structs[sdna_index_next].size):

            raise RuntimeError("cant refine to smaller type (%s -> %s)" %
                               (self.structs[sdna_index_curr].dna_type_id.decode('ascii'),
                                self.structs[sdna_index_next].dna_type_id.decode('ascii')))

    @staticmethod
    def decode_structs(header, block, handle):
        """
        DNACatalog is a catalog of all information in the DNA1 file-block
        """
        log.debug("building DNA catalog")
        shortstruct = DNA_IO.USHORT[header.endian_index]
        shortstruct2 = struct.Struct(header.endian_str + b'HH')
        intstruct = DNA_IO.UINT[header.endian_index]

        data = handle.read(block.size)
        types = []
        names = []

        structs = []
        sdna_index_from_id = {}

        offset = 8
        names_len = intstruct.unpack_from(data, offset)[0]
        offset += 4

        log.debug("building #%d names" % names_len)
        for i in range(names_len):
            tName = DNA_IO.read_data0_offset(data, offset)
            offset = offset + len(tName) + 1
            names.append(DNAName(tName))
        del names_len

        offset = pad_up_4(offset)
        offset += 4
        types_len = intstruct.unpack_from(data, offset)[0]
        offset += 4
        log.debug("building #%d types" % types_len)
        for i in range(types_len):
            dna_type_id = DNA_IO.read_data0_offset(data, offset)
            # None will be replaced by the DNAStruct, below
            types.append(DNAStruct(dna_type_id))
            offset += len(dna_type_id) + 1

        offset = pad_up_4(offset)
        offset += 4
        log.debug("building #%d type-lengths" % types_len)
        for i in range(types_len):
            tLen = shortstruct.unpack_from(data, offset)[0]
            offset = offset + 2
            types[i].size = tLen
        del types_len

        offset = pad_up_4(offset)
        offset += 4

        structs_len = intstruct.unpack_from(data, offset)[0]
        offset += 4
        log.debug("building #%d structures" % structs_len)
        for sdna_index in range(structs_len):
            d = shortstruct2.unpack_from(data, offset)
            struct_type_index = d[0]
            offset += 4
            dna_struct = types[struct_type_index]
            sdna_index_from_id[dna_struct.dna_type_id] = sdna_index
            structs.append(dna_struct)

            fields_len = d[1]
            dna_offset = 0

            for field_index in range(fields_len):
                d2 = shortstruct2.unpack_from(data, offset)
                field_type_index = d2[0]
                field_name_index = d2[1]
                offset += 4
                dna_type = types[field_type_index]
                dna_name = names[field_name_index]
                if dna_name.is_pointer or dna_name.is_method_pointer:
                    dna_size = header.pointer_size * dna_name.array_size
                else:
                    dna_size = dna_type.size * dna_name.array_size

                field = DNAField(dna_type, dna_name, dna_size, dna_offset)
                dna_struct.fields.append(field)
                dna_struct.field_from_name[dna_name.name_only] = field
                dna_offset += dna_size

        return structs, sdna_index_from_id


class BlendFileBlock:
    """
    Instance of a struct.
    """
    __slots__ = (
        # BlendFile
        "file",
        "code",
        "size",
        "addr_old",
        "sdna_index",
        "count",
        "file_offset",
        "user_data",
    )

    def __str__(self):
        return ("<%s.%s (%s), size=%d at %s>" %
                # fields=[%s]
                (self.__class__.__name__,
                 self.dna_type_name,
                 self.code.decode(),
                 self.size,
                 # b", ".join(f.dna_name.name_only for f in self.dna_type.fields).decode('ascii'),
                 hex(self.addr_old),
                 ))

    def __init__(self, handle, bfile):
        OLDBLOCK = struct.Struct(b'4sI')

        self.file = bfile
        self.user_data = None

        data = handle.read(bfile.block_header_struct.size)

        if len(data) != bfile.block_header_struct.size:
            print("WARNING! Blend file seems to be badly truncated!")
            self.code = b'ENDB'
            self.size = 0
            self.addr_old = 0
            self.sdna_index = 0
            self.count = 0
            self.file_offset = 0
            return
        # header size can be 8, 20, or 24 bytes long
        # 8: old blend files ENDB block (exception)
        # 20: normal headers 32 bit platform
        # 24: normal headers 64 bit platform
        if len(data) > 15:
            blockheader = bfile.block_header_struct.unpack(data)
            self.code = blockheader[0].partition(b'\0')[0]
            if self.code != b'ENDB':
                self.size = blockheader[1]
                self.addr_old = blockheader[2]
                self.sdna_index = blockheader[3]
                self.count = blockheader[4]
                self.file_offset = handle.tell()
            else:
                self.size = 0
                self.addr_old = 0
                self.sdna_index = 0
                self.count = 0
                self.file_offset = 0
        else:
            blockheader = OLDBLOCK.unpack(data)
            self.code = blockheader[0].partition(b'\0')[0]
            self.code = DNA_IO.read_data0(blockheader[0])
            self.size = 0
            self.addr_old = 0
            self.sdna_index = 0
            self.count = 0
            self.file_offset = 0

    @property
    def dna_type(self):
        return self.file.structs[self.sdna_index]

    @property
    def dna_type_name(self):
        return self.dna_type.dna_type_id.decode('ascii')

    def refine_type_from_index(self, sdna_index_next):
        assert type(sdna_index_next) is int
        sdna_index_curr = self.sdna_index
        self.file.ensure_subtype_smaller(sdna_index_curr, sdna_index_next)
        self.sdna_index = sdna_index_next

    def refine_type(self, dna_type_id):
        assert type(dna_type_id) is bytes
        self.refine_type_from_index(self.file.sdna_index_from_id[dna_type_id])

    def get_file_offset(
            self, path,
            default=...,
            sdna_index_refine=None,
            base_index=0,
    ):
        """
        Return (offset, length)
        """
        assert type(path) is bytes

        ofs = self.file_offset
        if base_index != 0:
            assert base_index < self.count
            ofs += (self.size // self.count) * base_index
        self.file.handle.seek(ofs, os.SEEK_SET)

        if sdna_index_refine is None:
            sdna_index_refine = self.sdna_index
        else:
            self.file.ensure_subtype_smaller(self.sdna_index, sdna_index_refine)

        dna_struct = self.file.structs[sdna_index_refine]
        field = dna_struct.field_from_path(
            self.file.header, self.file.handle, path)

        return (self.file.handle.tell(), field.dna_name.array_size)

    def get(
            self, path,
            default=...,
            sdna_index_refine=None,
            use_nil=True, use_str=True,
            base_index=0,
    ):

        ofs = self.file_offset
        if base_index != 0:
            assert base_index < self.count
            ofs += (self.size // self.count) * base_index
        self.file.handle.seek(ofs, os.SEEK_SET)

        if sdna_index_refine is None:
            sdna_index_refine = self.sdna_index
        else:
            self.file.ensure_subtype_smaller(self.sdna_index, sdna_index_refine)

        dna_struct = self.file.structs[sdna_index_refine]
        return dna_struct.field_get(
            self.file.header, self.file.handle, path,
            default=default,
            use_nil=use_nil, use_str=use_str,
        )

    def get_raw_data(self, dna_type_id, base_index=0):
        dna_types_to_size = {
            b'char': 1, b'uchar': 1,
            b'short': 2, b'ushort': 2,
            b'int': 4, b'uint': 4,
            b'int64_t': 8, b'uint64_t': 8,
            b'float': 4,
            b'double': 8,
        }
        if dna_type_id not in dna_types_to_size:
            raise NotImplementedError("Cannot read raw data of type %r" % dna_type_id)

        is_pointer = False
        dna_size = dna_types_to_size[dna_type_id]
        array_size = self.size // dna_size

        ofs = self.file_offset
        if base_index != 0:
            assert base_index < array_size
            ofs += dna_size * base_index
        self.file.handle.seek(ofs, os.SEEK_SET)

        print(dna_type_id, array_size, dna_size)
        return DNA_IO.read_data(self.file.handle, self.file.header,
                                is_pointer,
                                dna_type_id,
                                dna_size,
                                array_size)

    def get_recursive_iter(
            self, path, path_root=b"",
            default=...,
            sdna_index_refine=None,
            use_nil=True, use_str=True,
            base_index=0,
    ):
        if path_root:
            path_full = (
                (path_root if type(path_root) is tuple else (path_root, )) +
                (path if type(path) is tuple else (path, )))
        else:
            path_full = path

        try:
            yield (path_full, self.get(path_full, default, sdna_index_refine, use_nil, use_str, base_index))
        except NotImplementedError as ex:
            msg, dna_name, dna_type = ex.args
            struct_index = self.file.sdna_index_from_id.get(dna_type.dna_type_id, None)
            if struct_index is None:
                yield (path_full, "<%s>" % dna_type.dna_type_id.decode('ascii'))
            else:
                struct = self.file.structs[struct_index]
                if dna_name.array_size > 1:
                    for index in range(dna_name.array_size):
                        for f in struct.fields:
                            yield from self.get_recursive_iter(
                                (index, f.dna_name.name_only), path_full,
                                default, None, use_nil, use_str, 0)
                else:
                    for f in struct.fields:
                        yield from self.get_recursive_iter(
                            f.dna_name.name_only, path_full,
                            default, None, use_nil, use_str, 0)

    def items_recursive_iter(self, use_nil=True):
        for k in self.keys():
            yield from self.get_recursive_iter(k, use_nil=use_nil, use_str=False)

    def get_data_hash(self, seed=1):
        """
        Generates a 'hash' that can be used instead of addr_old as block id, and that should be 'stable' across .blend
        file load & save (i.e. it does not changes due to pointer addresses variations).
        """
        # TODO This implementation is most likely far from optimal... and CRC32 is not renown as the best hashing
        #      algorithm either. But for now does the job!
        import zlib

        def _is_pointer(self, k):
            return self.file.structs[self.sdna_index].field_from_path(
                self.file.header, self.file.handle, k).dna_name.is_pointer

        hsh = seed
        for k, v in self.items_recursive_iter():
            if not _is_pointer(self, k):
                hsh = zlib.adler32(str(v).encode(), hsh)
        return hsh

    def set(
            self, path, value,
            sdna_index_refine=None,
    ):

        if sdna_index_refine is None:
            sdna_index_refine = self.sdna_index
        else:
            self.file.ensure_subtype_smaller(self.sdna_index, sdna_index_refine)

        dna_struct = self.file.structs[sdna_index_refine]
        self.file.handle.seek(self.file_offset, os.SEEK_SET)
        self.file.is_modified = True
        return dna_struct.field_set(
            self.file.header, self.file.handle, path, value)

    # ---------------
    # Utility get/set
    #
    #   avoid inline pointer casting
    def get_pointer(
            self, path,
            default=...,
            sdna_index_refine=None,
            base_index=0,
    ):
        if sdna_index_refine is None:
            sdna_index_refine = self.sdna_index
        result = self.get(path, default, sdna_index_refine=sdna_index_refine, base_index=base_index)

        # default
        if type(result) is not int:
            return result

        assert self.file.structs[sdna_index_refine].field_from_path(
            self.file.header, self.file.handle, path).dna_name.is_pointer
        if result != 0:
            # possible (but unlikely)
            # that this fails and returns None
            # maybe we want to raise some exception in this case
            return self.file.find_block_from_offset(result)
        else:
            return None

    # ----------------------
    # Python convenience API

    # dict like access
    def __getitem__(self, item):
        return self.get(item, use_str=False)

    def __setitem__(self, item, value):
        self.set(item, value)

    def keys(self):
        return (f.dna_name.name_only for f in self.dna_type.fields)

    def values(self):
        for k in self.keys():
            try:
                yield self[k]
            except NotImplementedError as ex:
                msg, dna_name, dna_type = ex.args
                yield "<%s>" % dna_type.dna_type_id.decode('ascii')

    def items(self):
        for k in self.keys():
            try:
                yield (k, self[k])
            except NotImplementedError as ex:
                msg, dna_name, dna_type = ex.args
                yield (k, "<%s>" % dna_type.dna_type_id.decode('ascii'))


########################################################################################################################
# Way more basic access to blend-file data, without any DNA handling.

class BlendFileRaw:
    """
    Blend file, at a very low-level (only a collection of blocks).
    Can survive opening e.g. blend-files without DNA info.
    """
    __slots__ = (
        # file (result of open())
        "handle",
        # str (original name of the file path)
        "filepath_orig",
        # BlendFileHeader
        "header",
        # struct.Struct
        "block_header_struct",
        # BlendFileBlock
        "blocks",
        # dict {addr_old: block}
        "block_from_offset",
        # int
        "code_index",
        # bool (did we make a change)
        "is_modified",
        # bool (is file gzipped)
        "is_compressed",
    )

    def __init__(self, handle):
        log.debug("initializing reading blend-file")
        self.handle = handle
        self.header = BlendFileHeader(handle)
        self.block_header_struct = self.header.create_block_header_struct()
        self.blocks = []
        self.code_index = {}

        block = BlendFileBlockRaw(handle, self)
        while block.code != b'ENDB':
            handle.seek(block.size, os.SEEK_CUR)
            self.blocks.append(block)
            self.code_index.setdefault(block.code, []).append(block)

            block = BlendFileBlockRaw(handle, self)
        self.is_modified = False
        self.blocks.append(block)

        # Cache (could lazy init, in case we never use?).
        self.block_from_offset = {block.addr_old: block for block in self.blocks if block.code != b'ENDB'}

    def __repr__(self):
        return '<%s %r>' % (self.__class__.__qualname__, self.handle)

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        self.close()

    def find_blocks_from_code(self, code):
        assert type(code) == bytes
        if code not in self.code_index:
            return []
        return self.code_index[code]

    def find_block_from_offset(self, offset):
        # same as looking looping over all blocks,
        # then checking `block.addr_old == offset`.
        assert type(offset) is int
        return self.block_from_offset.get(offset)

    def close(self):
        """
        Close the blend file
        writes the blend file to disk if changes has happened
        """
        handle = self.handle

        if self.is_modified:
            if self.is_compressed:
                log.debug("close compressed blend file")
                handle.seek(os.SEEK_SET, 0)
                log.debug("compressing started")
                fs = gzip.open(self.filepath_orig, "wb")
                data = handle.read(FILE_BUFFER_SIZE)
                while data:
                    fs.write(data)
                    data = handle.read(FILE_BUFFER_SIZE)
                fs.close()
                log.debug("compressing finished")

        handle.close()

    def ensure_subtype_smaller(self, sdna_index_curr, sdna_index_next):
        # never refine to a smaller type
        if (self.structs[sdna_index_curr].size >
                self.structs[sdna_index_next].size):

            raise RuntimeError("cant refine to smaller type (%s -> %s)" %
                               (self.structs[sdna_index_curr].dna_type_id.decode('ascii'),
                                self.structs[sdna_index_next].dna_type_id.decode('ascii')))


class BlendFileBlockRaw:
    """
    Instance of a raw blend-file block (only contains its header currently).
    """
    __slots__ = (
        # BlendFile
        "file",
        "code",
        "size",
        "addr_old",
        "sdna_index",
        "count",
        "file_offset",
        "user_data",
    )

    def __str__(self):
        return ("<%s.%s (%s), size=%d at %s>" %
                # fields=[%s]
                (self.__class__.__name__,
                 self.dna_type_name,
                 self.code.decode(),
                 self.size,
                 # b", ".join(f.dna_name.name_only for f in self.dna_type.fields).decode('ascii'),
                 hex(self.addr_old),
                 ))

    def __init__(self, handle, bfile):
        OLDBLOCK = struct.Struct(b'4sI')

        self.file = bfile
        self.user_data = None

        data = handle.read(bfile.block_header_struct.size)

        if len(data) != bfile.block_header_struct.size:
            print("WARNING! Blend file seems to be badly truncated!")
            self.code = b'ENDB'
            self.size = 0
            self.addr_old = 0
            self.sdna_index = 0
            self.count = 0
            self.file_offset = 0
            return
        # header size can be 8, 20, or 24 bytes long
        # 8: old blend files ENDB block (exception)
        # 20: normal headers 32 bit platform
        # 24: normal headers 64 bit platform
        if len(data) > 15:
            blockheader = bfile.block_header_struct.unpack(data)
            self.code = blockheader[0].partition(b'\0')[0]
            if self.code != b'ENDB':
                self.size = blockheader[1]
                self.addr_old = blockheader[2]
                self.sdna_index = blockheader[3]
                self.count = blockheader[4]
                self.file_offset = handle.tell()
            else:
                self.size = 0
                self.addr_old = 0
                self.sdna_index = 0
                self.count = 0
                self.file_offset = 0
        else:
            blockheader = OLDBLOCK.unpack(data)
            self.code = blockheader[0].partition(b'\0')[0]
            self.code = DNA_IO.read_data0(blockheader[0])
            self.size = 0
            self.addr_old = 0
            self.sdna_index = 0
            self.count = 0
            self.file_offset = 0

    def get_data_hash(self, seed=1):
        """
        Generates a 'hash' that can be used instead of addr_old as block id, and that should be 'stable' across .blend
        file load & save (i.e. it does not changes due to pointer addresses variations).
        """
        # TODO This implementation is most likely far from optimal... and CRC32 is not renown as the best hashing
        #      algorithm either. But for now does the job!
        import zlib

        hsh = seed
        hsh = zlib.adler32(str(self.code).encode(), hsh)
        hsh = zlib.adler32(str(self.size).encode(), hsh)
        hsh = zlib.adler32(str(self.sdna_index).encode(), hsh)
        hsh = zlib.adler32(str(self.count).encode(), hsh)
        return hsh


# -----------------------------------------------------------------------------
# Read Magic
#
# magic = str
# pointer_size = int
# is_little_endian = bool
# version = int


class BlendFileHeader:
    """
    BlendFileHeader allocates the first 12 bytes of a blend file
    it contains information about the hardware architecture
    """
    __slots__ = (
        # str
        "magic",
        # int 4/8
        "pointer_size",
        # bool
        "is_little_endian",
        # int
        "version",
        # str, used to pass to 'struct'
        "endian_str",
        # int, used to index common types
        "endian_index",
    )

    def __init__(self, handle):
        FILEHEADER = struct.Struct(b'7s1s1s3s')

        log.debug("reading blend-file-header")
        values = FILEHEADER.unpack(handle.read(FILEHEADER.size))
        self.magic = values[0]
        pointer_size_id = values[1]
        if pointer_size_id == b'-':
            self.pointer_size = 8
        elif pointer_size_id == b'_':
            self.pointer_size = 4
        else:
            assert 0
        endian_id = values[2]
        if endian_id == b'v':
            self.is_little_endian = True
            self.endian_str = b'<'
            self.endian_index = 0
        elif endian_id == b'V':
            self.is_little_endian = False
            self.endian_index = 1
            self.endian_str = b'>'
        else:
            assert 0

        version_id = values[3]
        self.version = int(version_id)

    def create_block_header_struct(self):
        return struct.Struct(b''.join((
            self.endian_str,
            b'4sI',
            b'I' if self.pointer_size == 4 else b'Q',
            b'II',
        )))


class DNAName:
    """
    DNAName is a C-type name stored in the DNA
    """
    __slots__ = (
        "name_full",
        "name_only",
        "is_pointer",
        "is_method_pointer",
        "array_size",
    )

    def __init__(self, name_full):
        self.name_full = name_full
        self.name_only = self.calc_name_only()
        self.is_pointer = self.calc_is_pointer()
        self.is_method_pointer = self.calc_is_method_pointer()
        self.array_size = self.calc_array_size()

    def __repr__(self):
        return '%s(%r)' % (type(self).__qualname__, self.name_full)

    def as_reference(self, parent):
        if parent is None:
            result = b''
        else:
            result = parent + b'.'

        result = result + self.name_only
        return result

    def calc_name_only(self):
        result = self.name_full.strip(b'*()')
        index = result.find(b'[')
        if index != -1:
            result = result[:index]
        return result

    def calc_is_pointer(self):
        return (b'*' in self.name_full)

    def calc_is_method_pointer(self):
        return (b'(*' in self.name_full)

    def calc_array_size(self):
        result = 1
        temp = self.name_full
        index = temp.find(b'[')

        while index != -1:
            index_2 = temp.find(b']')
            result *= int(temp[index + 1:index_2])
            temp = temp[index_2 + 1:]
            index = temp.find(b'[')

        return result


class DNAField:
    """
    DNAField is a coupled DNAStruct and DNAName
    and cache offset for reuse
    """
    __slots__ = (
        # DNAName
        "dna_name",
        # tuple of 3 items
        # [bytes (struct name), int (struct size), DNAStruct]
        "dna_type",
        # size on-disk
        "dna_size",
        # cached info (avoid looping over fields each time)
        "dna_offset",
    )

    def __init__(self, dna_type, dna_name, dna_size, dna_offset):
        self.dna_type = dna_type
        self.dna_name = dna_name
        self.dna_size = dna_size
        self.dna_offset = dna_offset


class DNAStruct:
    """
    DNAStruct is a C-type structure stored in the DNA
    """
    __slots__ = (
        "dna_type_id",
        "size",
        "fields",
        "field_from_name",
        "user_data",
    )

    def __init__(self, dna_type_id):
        self.dna_type_id = dna_type_id
        self.fields = []
        self.field_from_name = {}
        self.user_data = None

    def __repr__(self):
        return '%s(%r)' % (type(self).__qualname__, self.dna_type_id)

    def field_from_path(self, header, handle, path):
        """
        Support lookups as bytes or a tuple of bytes and optional index.

        C style 'id.name'   -->  (b'id', b'name')
        C style 'array[4]'  -->  ('array', 4)
        """
        if type(path) is tuple:
            name = path[0]
            if len(path) >= 2 and type(path[1]) is not bytes:
                name_tail = path[2:]
                index = path[1]
                assert type(index) is int
            else:
                name_tail = path[1:]
                index = 0
        else:
            name = path
            name_tail = None
            index = 0

        assert type(name) is bytes

        field = self.field_from_name.get(name)

        if field is not None:
            handle.seek(field.dna_offset, os.SEEK_CUR)
            if index != 0:
                if field.dna_name.is_pointer:
                    index_offset = header.pointer_size * index
                else:
                    index_offset = field.dna_type.size * index
                assert index_offset < field.dna_size
                handle.seek(index_offset, os.SEEK_CUR)
            if not name_tail:  # None or ()
                return field
            else:
                return field.dna_type.field_from_path(header, handle, name_tail)

    def field_get(
            self, header, handle, path,
            default=...,
            use_nil=True, use_str=True,
    ):
        field = self.field_from_path(header, handle, path)
        if field is None:
            if default is not ...:
                return default
            else:
                raise KeyError("%r not found in %r (%r)" %
                               (path, [f.dna_name.name_only for f in self.fields], self.dna_type_id))

        dna_type = field.dna_type
        dna_name = field.dna_name
        dna_size = field.dna_size

        try:
            return DNA_IO.read_data(handle, header,
                                    dna_name.is_pointer,
                                    dna_type.dna_type_id,
                                    dna_size,
                                    dna_name.array_size,
                                    use_str=use_str,
                                    use_str_nil=use_nil,
                                    )
        except NotImplementedError as e:
            raise NotImplementedError("%r exists, but can't resolve field %r" %
                                      (path, dna_name.name_only), dna_name, dna_type)

    def field_set(self, header, handle, path, value):
        assert type(path) == bytes

        field = self.field_from_path(header, handle, path)
        if field is None:
            raise KeyError("%r not found in %r" %
                           (path, [f.dna_name.name_only for f in self.fields]))

        dna_type = field.dna_type
        dna_name = field.dna_name

        if dna_type.dna_type_id == b'char':
            if type(value) is str:
                return DNA_IO.write_string(handle, value, dna_name.array_size)
            else:
                return DNA_IO.write_bytes(handle, value, dna_name.array_size)
        else:
            raise NotImplementedError("Setting %r is not yet supported for %r" %
                                      (dna_type, dna_name), dna_name, dna_type)


class DNA_IO:
    """
    Module like class, for read-write utility functions.

    Only stores static methods & constants.
    """

    __slots__ = ()

    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    @classmethod
    def read_data(
            cls,
            handle, header,
            is_pointer, dna_type_id, dna_size, array_size,
            use_str=True, use_str_nil=True,
    ):
        if is_pointer:
            return cls.read_pointer(handle, header)
        elif dna_type_id == b'int':
            if array_size > 1:
                return [cls.read_int(handle, header) for i in range(array_size)]
            return cls.read_int(handle, header)
        elif dna_type_id == b'short':
            if array_size > 1:
                return [cls.read_short(handle, header) for i in range(array_size)]
            return cls.read_short(handle, header)
        elif dna_type_id == b'ushort':
            if array_size > 1:
                return [cls.read_ushort(handle, header) for i in range(array_size)]
            return cls.read_ushort(handle, header)
        elif dna_type_id == b'uint64_t':
            if array_size > 1:
                return [cls.read_ulong(handle, header) for i in range(array_size)]
            return cls.read_ulong(handle, header)
        elif dna_type_id == b'float':
            if array_size > 1:
                return [cls.read_float(handle, header) for i in range(array_size)]
            return cls.read_float(handle, header)
        elif dna_type_id == b'char':
            if dna_size == 1 and array_size <= 1:
                # Single char, assume it's bit-flag or int value, and not a string/bytes data.
                return cls.read_char(handle, header)
            if use_str:
                if use_str_nil:
                    return cls.read_string0(handle, array_size)
                else:
                    return cls.read_string(handle, array_size)
            else:
                if use_str_nil:
                    return cls.read_bytes0(handle, array_size)
                else:
                    return cls.read_bytes(handle, array_size)
        elif dna_type_id == b'uchar':
            if array_size > 1:
                return [cls.read_uchar(handle, header) for i in range(array_size)]
            return cls.read_uchar(handle, header)
        else:
            raise NotImplementedError("Reading %r type is not implemented" % dna_type_id)

    @staticmethod
    def write_string(handle, astring, fieldlen):
        assert isinstance(astring, str)
        if len(astring) >= fieldlen:
            stringw = astring[0:fieldlen]
        else:
            stringw = astring + '\0'
        handle.write(stringw.encode('utf-8'))

    @staticmethod
    def write_bytes(handle, astring, fieldlen):
        assert isinstance(astring, (bytes, bytearray))
        if len(astring) >= fieldlen:
            stringw = astring[0:fieldlen]
        else:
            stringw = astring + b'\0'

        handle.write(stringw)

    @staticmethod
    def read_bytes(handle, length):
        data = handle.read(length)
        return data

    @staticmethod
    def read_bytes0(handle, length):
        data = handle.read(length)
        return DNA_IO.read_data0(data)

    @staticmethod
    def read_string(handle, length):
        return DNA_IO.read_bytes(handle, length).decode('utf-8')

    @staticmethod
    def read_string0(handle, length):
        return DNA_IO.read_bytes0(handle, length).decode('utf-8')

    @staticmethod
    def read_data0_offset(data, offset):
        add = data.find(b'\0', offset) - offset
        return data[offset:offset + add]

    @staticmethod
    def read_data0(data):
        add = data.find(b'\0')
        return data[:add]

    UCHAR = struct.Struct(b'<B'), struct.Struct(b'>B')

    @staticmethod
    def read_uchar(handle, fileheader):
        st = DNA_IO.UCHAR[fileheader.endian_index]
        return st.unpack(handle.read(st.size))[0]

    SCHAR = struct.Struct(b'<b'), struct.Struct(b'>b')

    @staticmethod
    def read_char(handle, fileheader):
        st = DNA_IO.SCHAR[fileheader.endian_index]
        return st.unpack(handle.read(st.size))[0]

    USHORT = struct.Struct(b'<H'), struct.Struct(b'>H')

    @staticmethod
    def read_ushort(handle, fileheader):
        st = DNA_IO.USHORT[fileheader.endian_index]
        return st.unpack(handle.read(st.size))[0]

    SSHORT = struct.Struct(b'<h'), struct.Struct(b'>h')

    @staticmethod
    def read_short(handle, fileheader):
        st = DNA_IO.SSHORT[fileheader.endian_index]
        return st.unpack(handle.read(st.size))[0]

    UINT = struct.Struct(b'<I'), struct.Struct(b'>I')

    @staticmethod
    def read_uint(handle, fileheader):
        st = DNA_IO.UINT[fileheader.endian_index]
        return st.unpack(handle.read(st.size))[0]

    SINT = struct.Struct(b'<i'), struct.Struct(b'>i')

    @staticmethod
    def read_int(handle, fileheader):
        st = DNA_IO.SINT[fileheader.endian_index]
        return st.unpack(handle.read(st.size))[0]

    FLOAT = struct.Struct(b'<f'), struct.Struct(b'>f')

    @staticmethod
    def read_float(handle, fileheader):
        st = DNA_IO.FLOAT[fileheader.endian_index]
        return st.unpack(handle.read(st.size))[0]

    ULONG = struct.Struct(b'<Q'), struct.Struct(b'>Q')

    @staticmethod
    def read_ulong(handle, fileheader):
        st = DNA_IO.ULONG[fileheader.endian_index]
        return st.unpack(handle.read(st.size))[0]

    @staticmethod
    def read_pointer(handle, header):
        """
        reads an pointer from a file handle
        the pointer size is given by the header (BlendFileHeader)
        """
        if header.pointer_size == 4:
            st = DNA_IO.UINT[header.endian_index]
            return st.unpack(handle.read(st.size))[0]
        if header.pointer_size == 8:
            st = DNA_IO.ULONG[header.endian_index]
            return st.unpack(handle.read(st.size))[0]


# -----------------------------------------------------------------------------
# module global routines
#
# read routines
# open a filename
# determine if the file is compressed
# and returns a handle
def open_blend(filename, access="rb", wrapper_type=BlendFile):
    """Opens a blend file for reading or writing pending on the access
    supports 2 kind of blend files. Uncompressed and compressed.
    Known issue: does not support packaged blend files
    """
    handle = open(filename, access)
    magic_test = b"BLENDER"
    magic = handle.read(len(magic_test))
    if magic == magic_test:
        log.debug("normal blendfile detected")
        handle.seek(0, os.SEEK_SET)
        bfile = wrapper_type(handle)
        bfile.is_compressed = False
        bfile.filepath_orig = filename
        return bfile
    elif magic[:2] == b'\x1f\x8b':
        log.debug("gzip blendfile detected")
        handle.close()
        log.debug("decompressing started")
        fs = gzip.open(filename, "rb")
        data = fs.read(FILE_BUFFER_SIZE)
        magic = data[:len(magic_test)]
        if magic == magic_test:
            handle = tempfile.TemporaryFile()
            while data:
                handle.write(data)
                data = fs.read(FILE_BUFFER_SIZE)
            log.debug("decompressing finished")
            fs.close()
            log.debug("resetting decompressed file")
            handle.seek(os.SEEK_SET, 0)
            bfile = wrapper_type(handle)
            bfile.is_compressed = True
            bfile.filepath_orig = filename
            return bfile
        else:
            raise BlendFileError("filetype inside gzip not a blend")
    else:
        raise BlendFileError("filetype not a blend or a gzip blend")
