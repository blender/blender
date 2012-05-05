#!/usr/bin/env python3.2

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
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENCE BLOCK *****

######################################################
# Importing modules
######################################################

import os
import struct
import gzip
import tempfile

import logging
log = logging.getLogger("BlendFileReader")

######################################################
# module global routines
######################################################

def ReadString(handle, length):
    '''
    ReadString reads a String of given length or a zero terminating String
    from a file handle
    '''
    if length != 0:
        return handle.read(length).decode()
    else:
        # length == 0 means we want a zero terminating string
        result = ""
        s = ReadString(handle, 1)
        while s!="\0":
            result += s
            s = ReadString(handle, 1)
        return result


def Read(type, handle, fileheader):
    '''
    Reads the chosen type from a file handle
    '''
    def unpacked_bytes(type_char, size):
        return struct.unpack(fileheader.StructPre + type_char, handle.read(size))[0]
    
    if type == 'ushort':
        return unpacked_bytes("H", 2)   # unsigned short
    elif type == 'short':
        return unpacked_bytes("h", 2)   # short
    elif type == 'uint':
        return unpacked_bytes("I", 4)   # unsigned int
    elif type == 'int':
        return unpacked_bytes("i", 4)   # int
    elif type == 'float':
        return unpacked_bytes("f", 4)   # float
    elif type == 'ulong':
        return unpacked_bytes("Q", 8)   # unsigned long
    elif type == 'pointer':
        # The pointersize is given by the header (BlendFileHeader).
        if fileheader.PointerSize == 4:
            return Read('uint', handle, fileheader)
        if fileheader.PointerSize == 8:
            return Read('ulong', handle, fileheader)


def openBlendFile(filename):
    '''
    Open a filename, determine if the file is compressed and returns a handle
    '''
    handle = open(filename, 'rb')
    magic = ReadString(handle, 7)
    if magic in ("BLENDER", "BULLETf"):
        log.debug("normal blendfile detected")
        handle.seek(0, os.SEEK_SET)
        return handle
    else:
        log.debug("gzip blendfile detected?")
        handle.close()
        log.debug("decompressing started")
        fs = gzip.open(filename, "rb")
        handle = tempfile.TemporaryFile()
        data = fs.read(1024*1024) 
        while data: 
            handle.write(data) 
            data = fs.read(1024*1024) 
        log.debug("decompressing finished")
        fs.close()
        log.debug("resetting decompressed file")
        handle.seek(0, os.SEEK_SET)
        return handle


def Align(handle):
    '''
    Aligns the filehandle on 4 bytes
    '''
    offset = handle.tell()
    trim = offset % 4
    if trim != 0:
        handle.seek(4-trim, os.SEEK_CUR)


######################################################
# module classes
######################################################

class BlendFile:
    '''
    Reads a blendfile and store the header, all the fileblocks, and catalogue 
    structs foound in the DNA fileblock
    
    - BlendFile.Header  (BlendFileHeader instance)
    - BlendFile.Blocks  (list of BlendFileBlock instances)
    - BlendFile.Catalog (DNACatalog instance)
    '''
    
    def __init__(self, handle):
        log.debug("initializing reading blend-file")
        self.Header = BlendFileHeader(handle)
        self.Blocks = []
        fileblock = BlendFileBlock(handle, self)
        found_dna_block = False
        while not found_dna_block:
            if fileblock.Header.Code in ("DNA1", "SDNA"):
                self.Catalog = DNACatalog(self.Header, handle)
                found_dna_block = True
            else:
                fileblock.Header.skip(handle)
            
            self.Blocks.append(fileblock)
            fileblock = BlendFileBlock(handle, self)
        
        # appending last fileblock, "ENDB"
        self.Blocks.append(fileblock)
    
    # seems unused?
    """
    def FindBlendFileBlocksWithCode(self, code):
        #result = []
        #for block in self.Blocks:
            #if block.Header.Code.startswith(code) or block.Header.Code.endswith(code):
                #result.append(block)
        #return result
    """


class BlendFileHeader:
    '''
    BlendFileHeader allocates the first 12 bytes of a blend file.
    It contains information about the hardware architecture.
    Header example: BLENDER_v254
    
    BlendFileHeader.Magic             (str)
    BlendFileHeader.PointerSize       (int)
    BlendFileHeader.LittleEndianness  (bool)
    BlendFileHeader.StructPre         (str)   see http://docs.python.org/py3k/library/struct.html#byte-order-size-and-alignment
    BlendFileHeader.Version           (int)
    '''
    
    def __init__(self, handle):
        log.debug("reading blend-file-header")
        
        self.Magic = ReadString(handle, 7)
        log.debug(self.Magic)
        
        pointersize = ReadString(handle, 1)
        log.debug(pointersize)
        if pointersize == "-":
            self.PointerSize = 8
        if pointersize == "_":
            self.PointerSize = 4
                    
        endianness = ReadString(handle, 1)
        log.debug(endianness)
        if endianness == "v":
            self.LittleEndianness = True
            self.StructPre = "<"
        if endianness == "V":
            self.LittleEndianness = False
            self.StructPre = ">"
        
        version = ReadString(handle, 3)
        log.debug(version)
        self.Version = int(version)
        
        log.debug("{0} {1} {2} {3}".format(self.Magic, self.PointerSize, self.LittleEndianness, version))


class BlendFileBlock:
    '''
    BlendFileBlock.File     (BlendFile)
    BlendFileBlock.Header   (FileBlockHeader)
    '''
    
    def __init__(self, handle, blendfile):
        self.File = blendfile
        self.Header = FileBlockHeader(handle, blendfile.Header)
        
    def Get(self, handle, path):
        log.debug("find dna structure")
        dnaIndex = self.Header.SDNAIndex
        dnaStruct = self.File.Catalog.Structs[dnaIndex]
        log.debug("found " + dnaStruct.Type.Name)
        handle.seek(self.Header.FileOffset, os.SEEK_SET)
        return dnaStruct.GetField(self.File.Header, handle, path)


class FileBlockHeader:
    '''
    FileBlockHeader contains the information in a file-block-header.
    The class is needed for searching to the correct file-block (containing Code: DNA1)

    Code        (str)
    Size        (int)
    OldAddress  (pointer)
    SDNAIndex   (int)
    Count       (int)
    FileOffset  (= file pointer of datablock)
    '''
    
    def __init__(self, handle, fileheader):
        self.Code = ReadString(handle, 4).strip()
        if self.Code != "ENDB":
            self.Size = Read('uint', handle, fileheader)
            self.OldAddress = Read('pointer', handle, fileheader)
            self.SDNAIndex = Read('uint', handle, fileheader)
            self.Count = Read('uint', handle, fileheader)
            self.FileOffset = handle.tell()
        else:
            self.Size = Read('uint', handle, fileheader)
            self.OldAddress = 0
            self.SDNAIndex = 0
            self.Count = 0
            self.FileOffset = handle.tell()
        #self.Code += ' ' * (4 - len(self.Code))
        log.debug("found blend-file-block-fileheader {0} {1}".format(self.Code, self.FileOffset))

    def skip(self, handle):
        handle.read(self.Size)


class DNACatalog:
    '''
    DNACatalog is a catalog of all information in the DNA1 file-block
    
    Header = None
    Names = None
    Types = None
    Structs = None
    '''
    
    def __init__(self, fileheader, handle):
        log.debug("building DNA catalog")
        self.Names=[]
        self.Types=[]
        self.Structs=[]
        self.Header = fileheader
        
        SDNA = ReadString(handle, 4)
        
        # names
        NAME = ReadString(handle, 4)
        numberOfNames = Read('uint', handle, fileheader)
        log.debug("building #{0} names".format(numberOfNames))
        for i in range(numberOfNames):
            name = ReadString(handle,0)
            self.Names.append(DNAName(name))
        Align(handle)

        # types
        TYPE = ReadString(handle, 4)
        numberOfTypes = Read('uint', handle, fileheader)
        log.debug("building #{0} types".format(numberOfTypes))
        for i in range(numberOfTypes):
            type = ReadString(handle,0)
            self.Types.append(DNAType(type))
        Align(handle)

        # type lengths
        TLEN = ReadString(handle, 4)
        log.debug("building #{0} type-lengths".format(numberOfTypes))
        for i in range(numberOfTypes):
            length = Read('ushort', handle, fileheader)
            self.Types[i].Size = length
        Align(handle)

        # structs
        STRC = ReadString(handle, 4)
        numberOfStructures = Read('uint', handle, fileheader)
        log.debug("building #{0} structures".format(numberOfStructures))
        for structureIndex in range(numberOfStructures):
            type = Read('ushort', handle, fileheader)
            Type = self.Types[type]
            structure = DNAStructure(Type)
            self.Structs.append(structure)

            numberOfFields = Read('ushort', handle, fileheader)
            for fieldIndex in range(numberOfFields):
                fTypeIndex = Read('ushort', handle, fileheader)
                fNameIndex = Read('ushort', handle, fileheader)
                fType = self.Types[fTypeIndex]
                fName = self.Names[fNameIndex]
                structure.Fields.append(DNAField(fType, fName))


class DNAName:
    '''
    DNAName is a C-type name stored in the DNA.
    
    Name = str
    '''
    
    def __init__(self, name):
        self.Name = name
        
    def AsReference(self, parent):
        if parent is None:
            result = ""
        else:
            result = parent+"."
            
        result = result + self.ShortName()
        return result

    def ShortName(self):
        result = self.Name;
        result = result.replace("*", "")
        result = result.replace("(", "")
        result = result.replace(")", "")
        Index = result.find("[")
        if Index != -1:
            result = result[0:Index]
        return result
        
    def IsPointer(self):
        return self.Name.find("*")>-1

    def IsMethodPointer(self):
        return self.Name.find("(*")>-1

    def ArraySize(self):
        result = 1
        Temp = self.Name
        Index = Temp.find("[")

        while Index != -1:
            Index2 = Temp.find("]")
            result*=int(Temp[Index+1:Index2])
            Temp = Temp[Index2+1:]
            Index = Temp.find("[")
        
        return result


class DNAType:
    '''
    DNAType is a C-type stored in the DNA

    Name = str
    Size = int
    Structure = DNAStructure
    '''
    
    def __init__(self, aName):
        self.Name = aName
        self.Structure=None


class DNAStructure:
    '''
    DNAType is a C-type structure stored in the DNA
    
    Type = DNAType
    Fields = [DNAField]
    '''
    
    def __init__(self, aType):
        self.Type = aType
        self.Type.Structure = self
        self.Fields=[]
        
    def GetField(self, header, handle, path):
        splitted = path.partition(".")
        name = splitted[0]
        rest = splitted[2]
        offset = 0;
        for field in self.Fields:
            if field.Name.ShortName() == name:
                log.debug("found "+name+"@"+str(offset))
                handle.seek(offset, os.SEEK_CUR)
                return field.DecodeField(header, handle, rest)
            else:
                offset += field.Size(header)

        log.debug("error did not find "+path)
        return None


class DNAField:
    '''
    DNAField is a coupled DNAType and DNAName.
    
    Type = DNAType
    Name = DNAName
    '''

    def __init__(self, aType, aName):
        self.Type = aType
        self.Name = aName
        
    def Size(self, header):
        if self.Name.IsPointer() or self.Name.IsMethodPointer():
            return header.PointerSize*self.Name.ArraySize()
        else:
            return self.Type.Size*self.Name.ArraySize()

    def DecodeField(self, header, handle, path):
        if path == "":
            if self.Name.IsPointer():
                return Read('pointer', handle, header)
            if self.Type.Name=="int":
                return Read('int', handle, header)
            if self.Type.Name=="short":
                return Read('short', handle, header)
            if self.Type.Name=="float":
                return Read('float', handle, header)
            if self.Type.Name=="char":
                return ReadString(handle, self.Name.ArraySize())
        else:
            return self.Type.Structure.GetField(header, handle, path)

