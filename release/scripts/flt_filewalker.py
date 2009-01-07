#!BPY

# flt_filewalker.py is an utility module for OpenFlight IO scripts for blender.
# Copyright (C) 2005 Greg MacDonald
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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

__bpydoc__ ="""\
File read/write module used by OpenFlight I/O and tool scripts. OpenFlight is a
registered trademark of MultiGen-Paradigm, Inc.
"""

import Blender
from struct import *
import re

class FltIn:
    def __init__(self, filename):
        self.file = open(filename, 'rb')
        self.position = 0
        self.next_position = 100000
        self.opcode = 0
        self.length = 0
        self.level = 0
        self.repeat = False # Repeat the last record.

    def begin_record(self):
        if self.repeat == True:
            self.repeat = False
        else:
            self.position += self.length
        try:
            self.file.seek(self.position)
            input = self.file.read(4)
        except:
            print 'Parse Error!'
            return False
            
        if not input:
            self.close_file()
            return False
            
        self.opcode = unpack('>h', input[:2])[0]
        self.length = unpack('>H', input[-2:])[0]
        
        self.next_position = self.position + self.length
        
        return True

    def repeat_record(self):
        self.repeat = True

    def get_opcode(self):
        return self.opcode

    def get_level(self):
        return self.level

    def up_level(self):
        self.level += 1

    def down_level(self):
        self.level -= 1

    def read_string(self, length):
        s = ''
        if self.file.tell() + length <= self.next_position:
            start = self.file.tell()
            for i in xrange(length):
                char = self.file.read(1)
                if char == '\x00':
                    break
                s = s + char
            
            self.file.seek(start+length)
#        else:
#            print 'Warning: string truncated'

        return s
    
    def read_int(self):
        if self.file.tell() + 4 <= self.next_position:
            return unpack('>i', self.file.read(4))[0]
        else:
            #print 'Warning: int truncated'
            return 0

    def read_uint(self):
        if self.file.tell() + 4 <= self.next_position:
            return unpack('>I', self.file.read(4))[0]
        else:
            #print 'Warning: uint truncated'
            return 0

    def read_double(self):
        if self.file.tell() + 8 <= self.next_position:
            return unpack('>d', self.file.read(8))[0]
        else:
            #print 'Warning: double truncated'
            return 0.0

    def read_float(self):
        if self.file.tell() + 4 <= self.next_position:
            return unpack('>f', self.file.read(4))[0]
        else:
            #print 'Warning: float truncated'
            return 0.0

    def read_ushort(self):
        if self.file.tell() + 2 <= self.next_position:
            return unpack('>H', self.file.read(2))[0]
        else:
            #print 'Warning: ushort truncated'
            return 0

    def read_short(self):
        if self.file.tell() + 2 <= self.next_position:
            return unpack('>h', self.file.read(2))[0]
        else:
            #print 'Warning: short trunated'
            return 0

    def read_uchar(self):
        if self.file.tell() + 1 <= self.next_position:
            return unpack('>B', self.file.read(1))[0]
        else:
            #print 'Warning: uchar truncated'
            return 0

    def read_char(self):
        if self.file.tell() + 1 <= self.next_position:
            return unpack('>b', self.file.read(1))[0]
        else:
            #print 'Warning: char truncated'
            return 0

    def read_ahead(self, i):
        if self.file.tell() + i <= self.next_position:
            self.file.seek(i, 1)
#        else:
#            print 'Warning: attempt to seek past record'      

    def get_length(self):
        return self.length

    def close_file(self):
        self.file.close()
        
class FltOut:
    # Length includes terminating null
    def write_string(self, string, length):
        if len(string) > length - 1:
            str_len = length - 1
        else:
            str_len = len(string)

        pad_len = length - str_len

        self.file.write(string[:str_len])

        self.pad(pad_len)

    def write_int(self, a):
        self.file.write( pack('>i', a) )

    def write_uint(self, a):
        self.file.write( pack('>I', a) )

    def write_double(self, a):
        self.file.write( pack('>d', a) )

    def write_float(self, a):
        self.file.write( pack('>f', a) )

    def write_ushort(self, a):
        self.file.write( pack('>H', a) )

    def write_short(self, a):
        self.file.write( pack('>h', a) )

    def write_uchar(self, a):
        self.file.write( pack('>B', a) )

    def write_char(self, a):
        self.file.write( pack('>b', a) )

    def pad(self, reps):
        for i in xrange(reps):
            self.file.write('\x00')

    def close_file(self):
        self.file.close()

    def __init__(self, filename):
		self.file = open(filename, 'wb')
		self.filename = filename
		

class FileFinder:
    def add_file_to_search_path(self, filename):
        dir = Blender.sys.dirname(filename)
        if dir != None and dir != '':
            self.search_dirs.append(dir)

    def strip_path(self, full_path):
        # One of my flt files had a windows path with unix seperation. Basename
        # returned the whole path + filename, which isn't expected. So my
        # attempt to fix it is to replace all / or \ with the platform specific
        # dir seperator.
        #
        # note: \\\\ is actually just one \ indirected twice, once for python
        #       then again for re.sub
        if Blender.sys.sep == '\\':
            full_path = re.sub('/', '\\\\', full_path)
        elif Blender.sys.sep == '/':
            full_path = re.sub('\\\\', '/', full_path)
            
        filename = Blender.sys.basename(full_path)
        return filename

    def find(self, full_path):
        if full_path == '':
            return None

        # Seperate out the path.
        dirname = Blender.sys.dirname(full_path)

        # Try it first.
        if Blender.sys.exists(full_path):
            if not dirname in self.search_dirs:
                self.search_dirs.append(dirname)
            return full_path

        # Maybe it's relative.
        for path in self.search_dirs:
            rel_full_path = Blender.sys.join(path, full_path)
            if Blender.sys.exists(rel_full_path):
                return rel_full_path

        # Search previous directories that have worked.
        filename = self.strip_path(full_path)
        for path in self.search_dirs:
            t = Blender.sys.join(path, filename)
            if Blender.sys.exists(t):
                return t

        # Ask user where it is.
        self.user_input = Blender.Draw.PupStrInput(filename + "? ", '', 100)
        #self.user_input = None
        if self.user_input != None:
            t = Blender.sys.join(self.user_input, filename)
            if Blender.sys.exists(t):
                user_dirname = Blender.sys.dirname(t)
                if not user_dirname in self.search_dirs:
                    self.search_dirs.append(user_dirname)
                return t

        # Couldn't find it.
        return None

    def __init__(self):
        self.user_input = ''
        self.current_file = ''
        self.search_dirs = []
        
        dir = Blender.Get('texturesdir')
        if dir != None and dir != '':
            self.search_dirs.append(dir)
        
        dir = Blender.sys.dirname(Blender.Get('filename'))
        if dir != None and dir != '':
            print dir
            self.search_dirs.append(dir)
  