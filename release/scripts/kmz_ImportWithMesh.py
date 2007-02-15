#!BPY

""" Registration info for Blender menus
Name: 'Google Earth 3 (.kml / .kmz)...'
Blender: 243
Group: 'Import'
Tip: 'Import geometry of a .kml or .kmz 3D model'
"""

__author__ = "Jean-Michel Soler (jms)"
__version__ = "0.1.9h, february, 13th, 2007"
__bpydoc__ = """\
       To read 3d geometry .kmz and .kml file 

       Caution  : the geometry data of the Google Earth's files on the web 
       are licensed  and you can not load or use it in a personnal work .
       Be aware that the content of the file you try to read must be free 
       or legaly yours .

		Attention, this script uses the Blender's intern fill() function to create 
		certain complex faces but to work correctly this function  needs a few 
		conditions :
			
		1/ At least one 3D window must be open in the the work space
		
		2/ Work space must be set in "Global"  (the layer panel can be saw in the blender task bar, it 's ok)  and not in "Local"
		
		3/ You have to make the import in Object modebut the vertex mode must be active. 

       
"""
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2006-2007: jm soler, jmsoler_at_free.fr
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# --------------------------------------------------------------------------

# This script has been modified on Feb 15, 2007 by the Blender Foundation
# changes include text in the user interface and text output.

import Blender
from Blender  import Window
import sys

"Read and write ZIP files."

import struct, os, time
import binascii

try:
	import zlib # We may need its compression method
except ImportError:
	zlib = None

__all__ = ["BadZipfile", "error", "ZIP_STORED", "ZIP_DEFLATED", "is_zipfile",
		"ZipInfo", "ZipFile"]

class BadZipfile(Exception):
	pass
error = BadZipfile      # The exception raised by this module

# constants for Zip file compression methods
ZIP_STORED = 0
ZIP_DEFLATED = 8
# Other ZIP compression methods not supported

# Here are some struct module formats for reading headers
structEndArchive = "<4s4H2lH"     # 9 items, end of archive, 22 bytes
stringEndArchive = "PK\005\006"   # magic number for end of archive record
structCentralDir = "<4s4B4HlLL5HLl"# 19 items, central directory, 46 bytes
stringCentralDir = "PK\001\002"   # magic number for central directory
structFileHeader = "<4s2B4HlLL2H"  # 12 items, file header record, 30 bytes
stringFileHeader = "PK\003\004"   # magic number for file header

# indexes of entries in the central directory structure
_CD_SIGNATURE = 0
_CD_CREATE_VERSION = 1
_CD_CREATE_SYSTEM = 2
_CD_EXTRACT_VERSION = 3
_CD_EXTRACT_SYSTEM = 4                  # is this meaningful?
_CD_FLAG_BITS = 5
_CD_COMPRESS_TYPE = 6
_CD_TIME = 7
_CD_DATE = 8
_CD_CRC = 9
_CD_COMPRESSED_SIZE = 10
_CD_UNCOMPRESSED_SIZE = 11
_CD_FILENAME_LENGTH = 12
_CD_EXTRA_FIELD_LENGTH = 13
_CD_COMMENT_LENGTH = 14
_CD_DISK_NUMBER_START = 15
_CD_INTERNAL_FILE_ATTRIBUTES = 16
_CD_EXTERNAL_FILE_ATTRIBUTES = 17
_CD_LOCAL_HEADER_OFFSET = 18

# indexes of entries in the local file header structure
_FH_SIGNATURE = 0
_FH_EXTRACT_VERSION = 1
_FH_EXTRACT_SYSTEM = 2                  # is this meaningful?
_FH_GENERAL_PURPOSE_FLAG_BITS = 3
_FH_COMPRESSION_METHOD = 4
_FH_LAST_MOD_TIME = 5
_FH_LAST_MOD_DATE = 6
_FH_CRC = 7
_FH_COMPRESSED_SIZE = 8
_FH_UNCOMPRESSED_SIZE = 9
_FH_FILENAME_LENGTH = 10
_FH_EXTRA_FIELD_LENGTH = 11

def is_zipfile(filename):
	"""Quickly see if file is a ZIP file by checking the magic number."""
	try:
		fpin = open(filename, "rb")
		endrec = _EndRecData(fpin)
		fpin.close()
		if endrec:
			return True                 # file has correct magic number
	except IOError:
		pass
	return False

def _EndRecData(fpin):
	"""Return data from the "End of Central Directory" record, or None.

	The data is a list of the nine items in the ZIP "End of central dir"
	record followed by a tenth item, the file seek offset of this record."""
	fpin.seek(-22, 2)               # Assume no archive comment.
	filesize = fpin.tell() + 22     # Get file size
	data = fpin.read()
	if data[0:4] == stringEndArchive and data[-2:] == "\000\000":
		endrec = struct.unpack(structEndArchive, data)
		endrec = list(endrec)
		endrec.append("")               # Append the archive comment
		endrec.append(filesize - 22)    # Append the record start offset
		return endrec
	# Search the last END_BLOCK bytes of the file for the record signature.
	# The comment is appended to the ZIP file and has a 16 bit length.
	# So the comment may be up to 64K long.  We limit the search for the
	# signature to a few Kbytes at the end of the file for efficiency.
	# also, the signature must not appear in the comment.
	END_BLOCK = min(filesize, 1024 * 4)
	fpin.seek(filesize - END_BLOCK, 0)
	data = fpin.read()
	start = data.rfind(stringEndArchive)
	if start >= 0:     # Correct signature string was found
		endrec = struct.unpack(structEndArchive, data[start:start+22])
		endrec = list(endrec)
		comment = data[start+22:]
		if endrec[7] == len(comment):     # Comment length checks out
			# Append the archive comment and start offset
			endrec.append(comment)
			endrec.append(filesize - END_BLOCK + start)
			return endrec
	return      # Error, return None


class ZipInfo:
	"""Class with attributes describing each file in the ZIP archive."""

	def __init__(self, filename="NoName", date_time=(1980,1,1,0,0,0)):
		self.orig_filename = filename   # Original file name in archive
# Terminate the file name at the first null byte.  Null bytes in file
# names are used as tricks by viruses in archives.
		null_byte = filename.find(chr(0))
		if null_byte >= 0:
			filename = filename[0:null_byte]
# This is used to ensure paths in generated ZIP files always use
# forward slashes as the directory separator, as required by the
# ZIP format specification.
		if os.sep != "/":
			filename = filename.replace(os.sep, "/")
		self.filename = filename        # Normalized file name
		self.date_time = date_time      # year, month, day, hour, min, sec
		# Standard values:
		self.compress_type = ZIP_STORED # Type of compression for the file
		self.comment = ""               # Comment for each file
		self.extra = ""                 # ZIP extra data
		self.create_system = 0          # System which created ZIP archive
		self.create_version = 20        # Version which created ZIP archive
		self.extract_version = 20       # Version needed to extract archive
		self.reserved = 0               # Must be zero
		self.flag_bits = 0              # ZIP flag bits
		self.volume = 0                 # Volume number of file header
		self.internal_attr = 0          # Internal attributes
		self.external_attr = 0          # External file attributes
		# Other attributes are set by class ZipFile:
		# header_offset         Byte offset to the file header
		# file_offset           Byte offset to the start of the file data
		# CRC                   CRC-32 of the uncompressed file
		# compress_size         Size of the compressed file
		# file_size             Size of the uncompressed file

	def FileHeader(self):
		"""Return the per-file header as a string."""
		dt = self.date_time
		dosdate = (dt[0] - 1980) << 9 | dt[1] << 5 | dt[2]
		dostime = dt[3] << 11 | dt[4] << 5 | (dt[5] // 2)
		if self.flag_bits & 0x08:
			# Set these to zero because we write them after the file data
			CRC = compress_size = file_size = 0
		else:
			CRC = self.CRC
			compress_size = self.compress_size
			file_size = self.file_size
		header = struct.pack(structFileHeader, stringFileHeader,
				 self.extract_version, self.reserved, self.flag_bits,
				 self.compress_type, dostime, dosdate, CRC,
				 compress_size, file_size,
				 len(self.filename), len(self.extra))
		return header + self.filename + self.extra


class ZipFile:
	""" Class with methods to open, read, write, close, list zip files.

	z = ZipFile(file, mode="r", compression=ZIP_STORED)

	file: Either the path to the file, or a file-like object.
		  If it is a path, the file will be opened and closed by ZipFile.
	mode: The mode can be either read "r", write "w" or append "a".
	compression: ZIP_STORED (no compression) or ZIP_DEFLATED (requires zlib).
	"""

	fp = None                   # Set here since __del__ checks it

	def __init__(self, file, mode="r", compression=ZIP_STORED):
		"""Open the ZIP file with mode read "r", write "w" or append "a"."""
		if compression == ZIP_STORED:
			pass
		elif compression == ZIP_DEFLATED:
			if not zlib:
				raise RuntimeError,\
					  "Compression requires the (missing) zlib module"
		else:
			raise RuntimeError, "That compression method is not supported"
		self.debug = 0  # Level of printing: 0 through 3
		self.NameToInfo = {}    # Find file info given name
		self.filelist = []      # List of ZipInfo instances for archive
		self.compression = compression  # Method of compression
		self.mode = key = mode.replace('b', '')[0]

		# Check if we were passed a file-like object
		if isinstance(file, basestring):
			self._filePassed = 0
			self.filename = file
			modeDict = {'r' : 'rb', 'w': 'wb', 'a' : 'r+b'}
			self.fp = open(file, modeDict[mode])
		else:
			self._filePassed = 1
			self.fp = file
			self.filename = getattr(file, 'name', None)

		if key == 'r':
			self._GetContents()
		elif key == 'w':
			pass
		elif key == 'a':
			try:                        # See if file is a zip file
				self._RealGetContents()
				# seek to start of directory and overwrite
				self.fp.seek(self.start_dir, 0)
			except BadZipfile:          # file is not a zip file, just append
				self.fp.seek(0, 2)
		else:
			if not self._filePassed:
				self.fp.close()
				self.fp = None
			raise RuntimeError, 'Mode must be "r", "w" or "a"'

	def _GetContents(self):
		"""Read the directory, making sure we close the file if the format
		is bad."""
		try:
			self._RealGetContents()
		except BadZipfile:
			if not self._filePassed:
				self.fp.close()
				self.fp = None
			raise

	def _RealGetContents(self):
		"""Read in the table of contents for the ZIP file."""
		fp = self.fp
		endrec = _EndRecData(fp)
		if not endrec:
			raise BadZipfile, "File is not a zip file"
		if self.debug > 1:
			print endrec
		size_cd = endrec[5]             # bytes in central directory
		offset_cd = endrec[6]   # offset of central directory
		self.comment = endrec[8]        # archive comment
		# endrec[9] is the offset of the "End of Central Dir" record
		x = endrec[9] - size_cd
		# "concat" is zero, unless zip was concatenated to another file
		concat = x - offset_cd
		if self.debug > 2:
			print "given, inferred, offset", offset_cd, x, concat
		# self.start_dir:  Position of start of central directory
		self.start_dir = offset_cd + concat
		fp.seek(self.start_dir, 0)
		total = 0
		while total < size_cd:
			centdir = fp.read(46)
			total = total + 46
			if centdir[0:4] != stringCentralDir:
				raise BadZipfile, "Bad magic number for central directory"
			centdir = struct.unpack(structCentralDir, centdir)
			if self.debug > 2:
				print centdir
			filename = fp.read(centdir[_CD_FILENAME_LENGTH])
			# Create ZipInfo instance to store file information
			x = ZipInfo(filename)
			x.extra = fp.read(centdir[_CD_EXTRA_FIELD_LENGTH])
			x.comment = fp.read(centdir[_CD_COMMENT_LENGTH])
			total = (total + centdir[_CD_FILENAME_LENGTH]
					 + centdir[_CD_EXTRA_FIELD_LENGTH]
					 + centdir[_CD_COMMENT_LENGTH])
			x.header_offset = centdir[_CD_LOCAL_HEADER_OFFSET] + concat
			# file_offset must be computed below...
			(x.create_version, x.create_system, x.extract_version, x.reserved,
				x.flag_bits, x.compress_type, t, d,
				x.CRC, x.compress_size, x.file_size) = centdir[1:12]
			x.volume, x.internal_attr, x.external_attr = centdir[15:18]
			# Convert date/time code to (year, month, day, hour, min, sec)
			x.date_time = ( (d>>9)+1980, (d>>5)&0xF, d&0x1F,
									 t>>11, (t>>5)&0x3F, (t&0x1F) * 2 )
			self.filelist.append(x)
			self.NameToInfo[x.filename] = x
			if self.debug > 2:
				print "total", total
		for data in self.filelist:
			fp.seek(data.header_offset, 0)
			fheader = fp.read(30)
			if fheader[0:4] != stringFileHeader:
				raise BadZipfile, "Bad magic number for file header"
			fheader = struct.unpack(structFileHeader, fheader)
			# file_offset is computed here, since the extra field for
			# the central directory and for the local file header
			# refer to different fields, and they can have different
			# lengths
			data.file_offset = (data.header_offset + 30
								+ fheader[_FH_FILENAME_LENGTH]
								+ fheader[_FH_EXTRA_FIELD_LENGTH])
			fname = fp.read(fheader[_FH_FILENAME_LENGTH])
			if fname != data.orig_filename:
				raise RuntimeError, \
					  'File name in directory "%s" and header "%s" differ.' % (
						  data.orig_filename, fname)

	def namelist(self):
		"""Return a list of file names in the archive."""
		l = []
		for data in self.filelist:
			l.append(data.filename)
		return l

	def infolist(self):
		"""Return a list of class ZipInfo instances for files in the
		archive."""
		return self.filelist

	def printdir(self):
		"""Print a table of contents for the zip file."""
		print "%-46s %19s %12s" % ("File Name", "Modified    ", "Size")
		for zinfo in self.filelist:
			date = "%d-%02d-%02d %02d:%02d:%02d" % zinfo.date_time
			print "%-46s %s %12d" % (zinfo.filename, date, zinfo.file_size)

	def testzip(self):
		"""Read all the files and check the CRC."""
		for zinfo in self.filelist:
			try:
				self.read(zinfo.filename)       # Check CRC-32
			except BadZipfile:
				return zinfo.filename

	def getinfo(self, name):
		"""Return the instance of ZipInfo given 'name'."""
		return self.NameToInfo[name]

	def read(self, name):
		"""Return file bytes (as a string) for name."""
		if self.mode not in ("r", "a"):
			raise RuntimeError, 'read() requires mode "r" or "a"'
		if not self.fp:
			raise RuntimeError, \
				  "Attempt to read ZIP archive that was already closed"
		zinfo = self.getinfo(name)
		filepos = self.fp.tell()
		self.fp.seek(zinfo.file_offset, 0)
		bytes = self.fp.read(zinfo.compress_size)
		self.fp.seek(filepos, 0)
		if zinfo.compress_type == ZIP_STORED:
			pass
		elif zinfo.compress_type == ZIP_DEFLATED:
			if not zlib:
				raise RuntimeError, \
					  "De-compression requires the (missing) zlib module"
			# zlib compress/decompress code by Jeremy Hylton of CNRI
			dc = zlib.decompressobj(-15)
			bytes = dc.decompress(bytes)
			# need to feed in unused pad byte so that zlib won't choke
			ex = dc.decompress('Z') + dc.flush()
			if ex:
				bytes = bytes + ex
		else:
			raise BadZipfile, \
				  "Unsupported compression method %d for file %s" % \
			(zinfo.compress_type, name)
		crc = binascii.crc32(bytes)
		if crc != zinfo.CRC:
			raise BadZipfile, "Bad CRC-32 for file %s" % name
		return bytes

	def _writecheck(self, zinfo):
		"""Check for errors before writing a file to the archive."""
		if zinfo.filename in self.NameToInfo:
			if self.debug:      # Warning for duplicate names
				print "Duplicate name:", zinfo.filename
		if self.mode not in ("w", "a"):
			raise RuntimeError, 'write() requires mode "w" or "a"'
		if not self.fp:
			raise RuntimeError, \
				  "Attempt to write ZIP archive that was already closed"
		if zinfo.compress_type == ZIP_DEFLATED and not zlib:
			raise RuntimeError, \
				  "Compression requires the (missing) zlib module"
		if zinfo.compress_type not in (ZIP_STORED, ZIP_DEFLATED):
			raise RuntimeError, \
				  "That compression method is not supported"

	def write(self, filename, arcname=None, compress_type=None):
		"""Put the bytes from filename into the archive under the name
		arcname."""
		st = os.stat(filename)
		mtime = time.localtime(st.st_mtime)
		date_time = mtime[0:6]
		# Create ZipInfo instance to store file information
		if arcname is None:
			zinfo = ZipInfo(filename, date_time)
		else:
			zinfo = ZipInfo(arcname, date_time)
		zinfo.external_attr = (st[0] & 0xFFFF) << 16L      # Unix attributes
		if compress_type is None:
			zinfo.compress_type = self.compression
		else:
			zinfo.compress_type = compress_type
		self._writecheck(zinfo)
		fp = open(filename, "rb")
		zinfo.flag_bits = 0x00
		zinfo.header_offset = self.fp.tell()    # Start of header bytes
		# Must overwrite CRC and sizes with correct data later
		zinfo.CRC = CRC = 0
		zinfo.compress_size = compress_size = 0
		zinfo.file_size = file_size = 0
		self.fp.write(zinfo.FileHeader())
		zinfo.file_offset = self.fp.tell()      # Start of file bytes
		if zinfo.compress_type == ZIP_DEFLATED:
			cmpr = zlib.compressobj(zlib.Z_DEFAULT_COMPRESSION,
				 zlib.DEFLATED, -15)
		else:
			cmpr = None
		while 1:
			buf = fp.read(1024 * 8)
			if not buf:
				break
			file_size = file_size + len(buf)
			CRC = binascii.crc32(buf, CRC)
			if cmpr:
				buf = cmpr.compress(buf)
				compress_size = compress_size + len(buf)
			self.fp.write(buf)
		fp.close()
		if cmpr:
			buf = cmpr.flush()
			compress_size = compress_size + len(buf)
			self.fp.write(buf)
			zinfo.compress_size = compress_size
		else:
			zinfo.compress_size = file_size
		zinfo.CRC = CRC
		zinfo.file_size = file_size
		# Seek backwards and write CRC and file sizes
		position = self.fp.tell()       # Preserve current position in file
		self.fp.seek(zinfo.header_offset + 14, 0)
		self.fp.write(struct.pack("<lLL", zinfo.CRC, zinfo.compress_size,
			  zinfo.file_size))
		self.fp.seek(position, 0)
		self.filelist.append(zinfo)
		self.NameToInfo[zinfo.filename] = zinfo

	def writestr(self, zinfo_or_arcname, bytes):
		"""Write a file into the archive.  The contents is the string
		'bytes'.  'zinfo_or_arcname' is either a ZipInfo instance or
		the name of the file in the archive."""
		if not isinstance(zinfo_or_arcname, ZipInfo):
			zinfo = ZipInfo(filename=zinfo_or_arcname,
							date_time=time.localtime(time.time()))
			zinfo.compress_type = self.compression
		else:
			zinfo = zinfo_or_arcname
		self._writecheck(zinfo)
		zinfo.file_size = len(bytes)            # Uncompressed size
		zinfo.CRC = binascii.crc32(bytes)       # CRC-32 checksum
		if zinfo.compress_type == ZIP_DEFLATED:
			co = zlib.compressobj(zlib.Z_DEFAULT_COMPRESSION,
				 zlib.DEFLATED, -15)
			bytes = co.compress(bytes) + co.flush()
			zinfo.compress_size = len(bytes)    # Compressed size
		else:
			zinfo.compress_size = zinfo.file_size
		zinfo.header_offset = self.fp.tell()    # Start of header bytes
		self.fp.write(zinfo.FileHeader())
		zinfo.file_offset = self.fp.tell()      # Start of file bytes
		self.fp.write(bytes)
		if zinfo.flag_bits & 0x08:
			# Write CRC and file sizes after the file data
			self.fp.write(struct.pack("<lLL", zinfo.CRC, zinfo.compress_size,
				  zinfo.file_size))
		self.filelist.append(zinfo)
		self.NameToInfo[zinfo.filename] = zinfo

	def __del__(self):
		"""Call the "close()" method in case the user forgot."""
		self.close()

	def close(self):
		"""Close the file, and for mode "w" and "a" write the ending
		records."""
		if self.fp is None:
			return
		if self.mode in ("w", "a"):             # write ending records
			count = 0
			pos1 = self.fp.tell()
			for zinfo in self.filelist:         # write central directory
				count = count + 1
				dt = zinfo.date_time
				dosdate = (dt[0] - 1980) << 9 | dt[1] << 5 | dt[2]
				dostime = dt[3] << 11 | dt[4] << 5 | (dt[5] // 2)
				centdir = struct.pack(structCentralDir,
				  stringCentralDir, zinfo.create_version,
				  zinfo.create_system, zinfo.extract_version, zinfo.reserved,
				  zinfo.flag_bits, zinfo.compress_type, dostime, dosdate,
				  zinfo.CRC, zinfo.compress_size, zinfo.file_size,
				  len(zinfo.filename), len(zinfo.extra), len(zinfo.comment),
				  0, zinfo.internal_attr, zinfo.external_attr,
				  zinfo.header_offset)
				self.fp.write(centdir)
				self.fp.write(zinfo.filename)
				self.fp.write(zinfo.extra)
				self.fp.write(zinfo.comment)
			pos2 = self.fp.tell()
			# Write end-of-zip-archive record
			endrec = struct.pack(structEndArchive, stringEndArchive,
					 0, 0, count, count, pos2 - pos1, pos1, 0)
			self.fp.write(endrec)
			self.fp.flush()
		if not self._filePassed:
			self.fp.close()
		self.fp = None

from Blender import Mathutils
BLversion=Blender.Get('version')
from math import cos, sin, acos, radians

try:
	import nt
	os=nt
	os.sep='\\'

except: 
	import posix
	os=posix
	os.sep='/'

def isdir(path):
	try:
		st = os.stat(path)
		return 1 
	except:
		return 0

def split(pathname):
	if pathname.find(os.sep)!=-1:
		k0=pathname.split(os.sep)
	else:
		if os.sep=='/':
			k0=pathname.split('\\')
		else:
			k0=pathname.split('/') 

	directory=pathname.replace(k0[len(k0)-1],'')
	Name=k0[len(k0)-1]
	return directory, Name

def join(l0,l1): 
	return  l0+os.sep+l1

os.isdir=isdir
os.split=split
os.join=join

def fonctionSELECT(nom):
	scan_FILE(nom)

def filtreFICHIER(nom):
	"""
	Function  filtreFICHIER
	in  : string  nom , filename
	out : string of data   , if correct filecontaint 
	"""
	if nom.upper().find('.KMZ')!=-1:
		#from zipfile import ZipFile, ZIP_DEFLATED
		tz=ZipFile(nom,'r',ZIP_DEFLATED)
		if 'models/' in tz.namelist():
			return ''
		else:
			t = tz.read(tz.namelist()[0])
			tz.close()
			t=t.replace('<br>','') 
			return t		
		
	elif nom.upper().find('.KML')!=-1:
		tz=open(nom,'r')
		t = tz.read()
		tz.close()
		t=t.replace('<br>','')
		return t
	else : 
		return None
# ....
# needed to global call
# ....
OB = None 
SC = None 
ME=  None

POLYGON_NUMBER=0
DOCUMENTORIGINE=[]
UPDATE_V=[]
UPDATE_F=[]
POS=0
NUMBER=0
PLACEMARK=0
TAG3=100
TAG4=100
TAG4=1

eps=0.0000001
npoly=0
nedge=0
gt1=Blender.sys.time()

def create_LINE(BROKEN_LINE,tv):
	global TAG4,nedge 
	sc = Blender.Scene.GetCurrent()
	me = Blender.Mesh.New('myMesh')
	ob = sc.objects.new(me)
	ob.setDrawMode(32)
	ob.setDrawType(2)
	v=me.verts
	e=me.edges
	for bl in BROKEN_LINE:
		nedge+=1
		v.extend(bl)
		e.extend(v[-2],v[-1])
		#v[-2].sel=1
		#v[-1].sel=1	
		if TAG4 and nedge %TAG4 == 1 : 
			print 'Pedg: ', nedge
	me.sel = True
	if tv :
		me.remDoubles(0.0001)

def cree_POLYGON(ME,TESSEL):
	global  OB, npoly, UPDATE_V, UPDATE_F, POS,TAG3,TAG4, TAG5
	npoly+=1 
	for T in TESSEL: del T[-1]
	if TAG3 and npoly %TAG3 == 1 : 
		print 'Pgon: ', npoly, 'verts:',[len(T) for T in TESSEL]

	if TAG5 and npoly %TAG5 == 1 : 
		Blender.Window.Redraw(Blender.Window.Types.VIEW3D) # Blender.Window.RedrawAll()
		g2= Blender.sys.time()-gt1
		print int(g2/60),':',int(g2%60)      

	if len(TESSEL)==1 and len(TESSEL[0]) in [3,4] :
		if not UPDATE_F:
					POS=len(ME.verts)

		for VE in TESSEL[0]:
						UPDATE_V.append(VE)

		if len(TESSEL[0])==3:
					UPDATE_F.append([POS,POS+1,POS+2])
					POS+=3
		else :
					UPDATE_F.append([POS,POS+1,POS+2,POS+3])
					POS+=4
	else :
		if UPDATE_V : ME.verts.extend(UPDATE_V)
		if UPDATE_F:
			ME.faces.extend(UPDATE_F)
		UPDATE_F=[]
		UPDATE_V=[]
		EDGES=[]
		for T in TESSEL: 
			ME.verts.extend(T)
			ME_verts=ME.verts
			for t in range(len(T),1,-1):
						ME_verts[-t].sel=1
						EDGES.append([ME_verts[-t],ME_verts[-t+1]])
			ME_verts[-1].sel=1
			EDGES.append([ME_verts[-1],ME_verts[-len(T)]])
		ME.edges.extend(EDGES)
		ME.fill()
		if npoly %500 == 1 :
			ME.sel = True
			ME.remDoubles(0.0)
		ME.sel = False
	TESSEL=[] 
	return ME,TESSEL

X_COEF=85331.2  # old value
Y_COEF=110976.0 # old value

def XY_COEFF(DOCUMENTORIGINE):
	"""
	Constants too convert latitude and longitude degres in meters
	"""
	global X_COEF, Y_COEF  
 	lat = radians(DOCUMENTORIGINE[1])	
	X_COEF = 111412.84*cos(lat)-93.5*cos(3*lat)+0.118*cos(5*lat)
	Y_COEF = 111132.92-559.82*cos(2*lat)+1.175*cos(4*lat)-0.0023*cos(6*lat)

def cree_FORME(v,TESSEL):
	global X_COEF, Y_COEF
	VE=[(v[0]-DOCUMENTORIGINE[0])* X_COEF,
								(v[1]-DOCUMENTORIGINE[1])* Y_COEF,
								(v[2]-DOCUMENTORIGINE[2])  ]
	TESSEL.append(VE)

def active_FORME():
	global ME, UPDATE_V, UPDATE_F, POS, OB
	if len(UPDATE_V)>2 :
		ME.verts.extend(UPDATE_V)
	if UPDATE_F: ME.faces.extend(UPDATE_F)
	UPDATE_V=[]
	UPDATE_F=[]
	POS=0
	if len(ME.verts)>0:
		ME.sel=1
		ME.remDoubles(0.0)
				
def wash_DATA(ndata):
	if ndata:
		ndata=ndata.replace('\n',',')
		ndata=ndata.replace('\r','') 
		while ndata[-1]=='  ': 
			ndata=ndata.replace('  ',' ')
		while ndata[0]==' ':  
			ndata=ndata[1:]
		while ndata[-1]==' ': 
			ndata=ndata[:-1]	
		if ndata[0]==',':ndata=ndata[1:]
		if ndata[-1]==',':ndata=ndata[:-1]
		ndata=ndata.replace(',,',',')    
		ndata=ndata.replace(' ',',')
		ndata=ndata.split(',')	
		for n in ndata :
			if n=='' : ndata.remove(n)
	return  ndata 

def collecte_ATTRIBUTS(data):
		data=data.replace('  ',' ')
		ELEM={'TYPE':data[1:data.find(' ')]}
		t1=len(data)
		t2=0
		ct=data.count('="')
		while ct>0:
			t0=data.find('="',t2)
			t2=data.find(' ',t2)+1
			id=data[t2:t0]
			t2=data.find('"',t0+2)
			if id!='d':
				exec "ELEM[id]=\"\"\"%s\"\"\""%(data[t0+2:t2].replace('\\','/'))
			else:
				exec "ELEM[id]=[%s,%s]"%(t0+2,t2) 
			ct=data.count('="',t2)
		return ELEM

def contruit_HIERARCHIE(t,tv0=0,tv=0):
	global DOCUMENTORIGINE, OB , ME, SC 
	global NUMBER, PLACEMARK, POLYGON_NUMBER
	
	vv=[]
	TESSEL=[]
	BROKEN_LINE=[]
	
	ME= Blender.Mesh.New()
	OB = SC.objects.new(ME)

	t=t.replace('\t',' ')
	while t.find('  ')!=-1:
							t=t.replace('  ',' ')
	n0=0 
	t0=t1=0
	baliste=[]
	balisetype=['?','?','/','/','!','!']
	BALISES=['D',  #DECL_TEXTE',
						'D',  #DECL_TEXTE',
						'F',  #FERMANTE',
						'E',  #ELEM_VIDE',
						'd',  #DOC',
						'R',  #REMARQUES',
						'C',  #CONTENU',
						'O'   #OUVRANTE'
						]

	TAGS = ['kml','Document','description','DocumentSource',
			'DocumentOrigin','visibility','LookAt',
		'Folder','name','heading','tilt','latitude',
		'longitude','range','Style','LineStyle','color',
		'Placemark','styleUrl','GeometryCollection',
		'Polygon','LinearRing','outerBoundaryIs',
		'altitudeMode','coordinates','LineString',
			'fill','PolyStyle','outline','innerBoundaryIs',
			'IconStyle', 'Icon', 'x','y' ,'w','href','h'
			]

	STACK=[]
	latitude = float(t[t.find('<latitude>')+len('<latitude>'):t.find('</latitude>')])
	longitude = float(t[t.find('<longitude>')+len('<longitude>'):t.find('</longitude>')])
	DOCUMENTORIGINE=[longitude,latitude,0 ]
	
	XY_COEFF(DOCUMENTORIGINE)
	
	GETMAT=0
	MATERIALS=[M.getName() for M in Blender.Material.Get()]
	while t1<len(t) and t0>-1 :
		t0=t.find('<',t0)
		t1=t.find('>',t0)
		ouvrante=0
		if t0>-1 and t1>-1:
			if t[t0+1] in balisetype:
				b=balisetype.index(t[t0+1])
				if t[t0+2]=='-': 
						b=balisetype.index(t[t0+1])+1
				balise=BALISES[b]
				if b==2 and t[t0:t1].find(STACK[-1])>-1:
								parent=STACK.pop(-1)
			elif t[t1-1] in balisetype:
				balise=BALISES[balisetype.index(t[t1-1])+1]
			else:
				t2=t.find(' ',t0) 
				if t2>t1: t2=t1
				ouvrante=1
				NOM=t[t0+1:t2]
				if t.find('</'+NOM)>-1:
							balise=BALISES[-1]
				else:
							balise=BALISES[-2]
			if balise=='E' or balise=='O':
				if NOM not in TAGS :
					if NOM not in ['a','b','table','tr','td','div','hr']:
						TAGS.append(NOM)
					else :
						t0=t.find('</'+NOM,t0)
						t1=t.find('>',t0)
						if t0==-1 and t1==-1:
											break
				if  balise=='O' and NOM in TAGS: 
					
					STACK.append(NOM)
					
					if not PLACEMARK :
						if NOM.find('Style')==0:
							proprietes=collecte_ATTRIBUTS(t[t0:t1+ouvrante])
						if NOM.find('PolyStyle')==0:	 
								GETMAT=1
								
						if NOM.find('color')==0 and GETMAT:
								COLOR=t[t2+1:t.find('</color',t2)]
								COLOR=[eval('0x'+COLOR[0:2]), eval('0x'+COLOR[2:4]), eval('0x'+COLOR[4:6]), eval('0x'+COLOR[6:])]
								if 'id' in proprietes.keys() and proprietes['id'] not in MATERIALS:
									MAT=Blender.Material.New(proprietes['id'])
									MAT.rgbCol = [COLOR[3]/255.0,COLOR[2]/255.0,COLOR[1]/255.0]  
									MAT.setAlpha(COLOR[0]/255.0)
									MATERIALS.append(MAT.getName())
								GETMAT=0
								
					if NOM.find('Polygon')>-1:
						VAL=t[t2+2:t.find('</Polygon',t2)]
						n=VAL.count('<outerBoundaryIs>')+VAL.count('<innerBoundaryIs>')
						
					if NOM.find('LineString')>-1:
						VAL=t[t2+2:t.find('</LineString',t2)]
						
					if NUMBER and NOM.find('Placemark')>-1 :
						PLACEMARK=1
						if t[t2:t.find('</Placemark',t2)].find('Polygon')>-1 and len(ME.verts)>0:
							active_FORME()
							ME = Blender.Mesh.New()
							OB = SC.objects.new(ME)

					if NOM.find('styleUrl')>-1:
							material= t[t2+2:t.find('</styleUrl',t2)]
							if material in MATERIALS :
								ME.materials=[Blender.Material.Get(material)]
							else :
								SMat=t.find('<Style id="'+material)
								if SMat>-1 :
									SMatF=t.find('</Style',SMat)
									SPolSt=t[SMat:SMatF].find('<PolyStyle>')
									if t[SMat:SMatF].find('<color>',SPolSt)>-1:
										COLOR=t[SMat:SMatF][t[SMat:SMatF].find('<color>',SPolSt)+7:t[SMat:SMatF].find('</color>',SPolSt)]
										if len(COLOR)>0 : COLOR=[eval('0x'+COLOR[0:2]), eval('0x'+COLOR[2:4]), eval('0x'+COLOR[4:6]), eval('0x'+COLOR[6:])]
									else :
										COLOR=[255,255,255,255]
									MAT=Blender.Material.New(material)
									MAT.rgbCol = [COLOR[3]/255.0,COLOR[2]/255.0,COLOR[1]/255.0]  
									MAT.setAlpha(COLOR[0]/255.0)
									MATERIALS.append(MAT.name)
									ME.materials=[MAT]
									
					if NOM.find('coordinates')>-1:
						VAL=t[t2+2:t.find('</coordinates',t2)]
						if STACK[-2]=='DocumentOrigin' :
							DOCUMENTORIGINE=[float(V) for V in  VAL.replace(' ','').replace('\n','').split(',')]
						if STACK[-2]=='LinearRing'  :
							n-=1
							TESSEL.append([])
							VAL=wash_DATA(VAL) 
							vv=[[float(VAL[a+ii]) for ii in xrange(3)] for a in xrange(0,len(VAL),3)]
							if vv  : [cree_FORME(v,TESSEL[-1]) for v in vv]
							del VAL
							if n==0: ME,TESSEL= cree_POLYGON(ME,TESSEL)
						if tv0 and STACK[-2]=='LineString'   :
							BROKEN_LINE.append([])
							VAL=wash_DATA(VAL) 
							vv=[[float(VAL[a+ii]) for ii in xrange(3)] for a in xrange(0,len(VAL),3)]
							if vv  : [cree_FORME(v,BROKEN_LINE[-1]) for v in vv]
							del VAL
				D=[] 
		else:
					break
		t1+=1
		t0=t1 
	if tv0 and BROKEN_LINE :
		create_LINE(BROKEN_LINE,tv)
def WARNING_nodata():
			name = "WARNING %t| Sorry, these data are perhaps in Google Earth 4.0 format and are not managed for the moement."  # if no %xN int is set, indices start from 1
			result = Blender.Draw.PupMenu(name)
			print '#----------------------------------------------'
			print '# Sorry the script can\'t find any geometry in this' 
			print '# file .'
			print '#  '				
			print '# If you have exported this data from Sketchup '
			print '# select the simple Google Earth format instead of' 
			print '# the Google Earth 4 .'
			print '#----------------------------------------------'
			return
					
def scan_FILE(nom):
		global NUMBER, PLACEMARK, SC, OB, ME, POLYGON_NUMBER, TAG3, TAG4, TAG5, gt1
 		
		dir,name=split(nom)
		name=name.split('.')
		result=0
		t=filtreFICHIER(nom)
		# print len(t)
		if t:
			PLACEMARK_NUMBER=t.count('<Placemark>')
			print 'Number of Placemark   :  ', PLACEMARK_NUMBER
			POLYGON_NUMBER=t.count('<Polygon')
			print 'Number of Polygons :  ', POLYGON_NUMBER
			EDGES_NUMBER=t.count('<LineString')
			print 'Number of Edges :  ', EDGES_NUMBER
								
			tag1 = Blender.Draw.Create(1)
			tag2 = Blender.Draw.Create(1)
			tag3 = Blender.Draw.Create(0)
			tag4 = Blender.Draw.Create(0)
			tag5 = Blender.Draw.Create(0)		
			
			block = []
			block.append("Import Edges only")
			block.append("-> Placemarker : %s"%PLACEMARK_NUMBER)			
			block.append("-> Polygons : %s"%POLYGON_NUMBER)
			block.append("-> Edges : %s"%EDGES_NUMBER)				
			block.append(("Polys count: ", tag3, 0, 1000,"a progression indicator can be displayed, 0 for none"))					
			block.append(("Force Edges", tag1, "force edge import if no polygon found"))
			block.append(("Edges count: ", tag4, 0, 1000,"a progression indicator can be displayed, 0 for none"))					
			block.append(("Remove double ", tag2, " "))
			block.append("Display Update;")
			block.append("Updates the 3d view and")
			block.append("prints progress to the")
			block.append("console while importing")
			block.append(("Display Update", tag5, "Update progress and time in the console"))
					
			if POLYGON_NUMBER==0 :
				retval = Blender.Draw.PupBlock("KML/KMZ import", block)
				if tag1.val==0: tag4.val==0  
					
				if not tag1.val or EDGES_NUMBER==0 and PLACEMARK_NUMBER==0:
					name = "WARNING %t| Sorry, the script can\'t find any geometry in this file ."  # if no %xN int is set, indices start from 1
					result = Blender.Draw.PupMenu(name)
					print '#----------------------------------------------'
					print '#  Sorry the script can\'t find any geometry in this' 
					print '#  file .'
					print '#----------------------------------------------'
					Blender.Window.RedrawAll()
					return
				elif not tag1.val or EDGES_NUMBER==0 and PLACEMARK_NUMBER==1:
					WARNING_nodata()
				elif EDGES_NUMBER:
					SC = Blender.Scene.GetCurrent()
					print 'Number of Placemark   :  ', PLACEMARK_NUMBER
					if PLACEMARK_NUMBER!=POLYGON_NUMBER :
						NUMBER=1
						PLACEMARK=0
						TAG3=tag3.val 
						TAG4=tag4.val
						TAG5=tag5.val					
					if t!='false':
						gt1=Blender.sys.time()
						contruit_HIERARCHIE(t,tag1.val,tag2.val)
			else:
				retval = Blender.Draw.PupBlock("KML/KMZ import", block)
				if retval:
					SC = Blender.Scene.GetCurrent()
					if PLACEMARK_NUMBER!=POLYGON_NUMBER :
						NUMBER=1
						PLACEMARK=0
					if t!='false':
						TAG3=tag3.val 
						TAG4=tag4.val
						TAG5=tag5.val
						gt1=Blender.sys.time()
						contruit_HIERARCHIE(t,tag1.val,tag2.val)
						active_FORME()
			
			gt2=Blender.sys.time()-gt1
			print "KML Imported, duration", int(gt2/60),':',int(gt2%60) 
		else:
			WARNING_nodata()
	
Blender.Window.FileSelector (fonctionSELECT, 'SELECT a .KMZ FILE')
