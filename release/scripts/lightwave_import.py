#!BPY

"""
Name: 'LightWave...'
Blender: 232
Group: 'Import'
Tooltip: 'Import LightWave Object File Format (*.lwo)'
"""

# $Id$
#
# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | April 21, 2002                                          |
# | Released under the Blender Artistic Licence (BAL)       |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Read and write LightWave Object File Format (*.lwo)     |
# +---------------------------------------------------------+

import Blender, mod_meshtools
import struct, chunk, os, cStringIO, time, operator

# =============================
# === Read LightWave Format ===
# =============================
def read(filename):
	start = time.clock()
	file = open(filename, "rb")

	# === LWO header ===
	form_id, form_size, form_type = struct.unpack(">4s1L4s",  file.read(12))
	if (form_type != "LWOB") and (form_type != "LWO2"):
		print "Can't read a file with the form_type:", form_type
		return

	objname = os.path.splitext(os.path.basename(filename))[0]

	while 1:
		try:
			lwochunk = chunk.Chunk(file)
		except EOFError:
			break
		if lwochunk.chunkname == "LAYR":
			objname = read_layr(lwochunk)
		elif lwochunk.chunkname == "PNTS":                         # Verts
			verts = read_verts(lwochunk)
		elif lwochunk.chunkname == "POLS" and form_type == "LWO2": # Faces v6.0
			faces = read_faces_6(lwochunk)
			mod_meshtools.create_mesh(verts, faces, objname)
		elif lwochunk.chunkname == "POLS" and form_type == "LWOB": # Faces v5.5
			faces = read_faces_5(lwochunk)
			mod_meshtools.create_mesh(verts, faces, objname)
		else:													   # Misc Chunks
			lwochunk.skip()

	Blender.Window.DrawProgressBar(1.0, "")    # clear progressbar
	file.close()
	end = time.clock()
	seconds = " in %.2f %s" % (end-start, "seconds")
	if form_type == "LWO2": fmt = " (v6.0 Format)"
	if form_type == "LWOB": fmt = " (v5.5 Format)"
	message = "Successfully imported " + os.path.basename(filename) + fmt + seconds
	mod_meshtools.print_boxed(message)

# ==================
# === Read Verts ===
# ==================
def read_verts(lwochunk):
	data = cStringIO.StringIO(lwochunk.read())
	numverts = lwochunk.chunksize/12
	#$verts = []
	verts = [None] * numverts
	for i in range(numverts):
		if not i%100 and mod_meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/numverts, "Reading Verts")
		x, y, z = struct.unpack(">fff", data.read(12))
		#$verts.append((x, z, y))
		verts[i] = (x, z, y)
	return verts

# =================
# === Read Name ===
# =================
def read_name(file):
	name = ""
	while 1:
		char = file.read(1)
		if char == "\0": break
		else: name += char
	return name

# ==================
# === Read Layer ===
# ==================
def read_layr(lwochunk):
	data = cStringIO.StringIO(lwochunk.read())
	idx, flags = struct.unpack(">hh", data.read(4))
	pivot = struct.unpack(">fff", data.read(12))
	layer_name = read_name(data)
	if not layer_name: layer_name = "No Name"
	return layer_name

# ======================
# === Read Faces 5.5 ===
# ======================
def read_faces_5(lwochunk):
	data = cStringIO.StringIO(lwochunk.read())
	faces = []
	i = 0
	while i < lwochunk.chunksize:
		if not i%100 and mod_meshtools.show_progress:
		   Blender.Window.DrawProgressBar(float(i)/lwochunk.chunksize, "Reading Faces")
		facev = []
		numfaceverts, = struct.unpack(">H", data.read(2))
		for j in range(numfaceverts):
			index, = struct.unpack(">H", data.read(2))
			facev.append(index)
		facev.reverse()
		faces.append(facev)
		surfaceindex, = struct.unpack(">H", data.read(2))
		if surfaceindex < 0:
			print "detail polygons follow, error."
			return
		i += (4+numfaceverts*2)
	return faces

# ==================================
# === Read Variable-Length Index ===
# ==================================
def read_vx(data):
	byte1, = struct.unpack(">B", data.read(1))
	if byte1 != 0xFF:	# 2-byte index
		byte2, = struct.unpack(">B", data.read(1))
		index = byte1*256 + byte2
		index_size = 2
	else:				# 4-byte index
		byte2, byte3, byte4 = struct.unpack(">3B", data.read(3))
		index = byte2*65536 + byte3*256 + byte4
		index_size = 4
	return index, index_size

# ======================
# === Read Faces 6.0 ===
# ======================
def read_faces_6(lwochunk):
	data = cStringIO.StringIO(lwochunk.read())
	faces = []
	polygon_type = data.read(4)
	if polygon_type != "FACE":
		print "No Faces Were Found. Polygon Type:", polygon_type
		return ""
	i = 0
	while(i < lwochunk.chunksize-4):
		if not i%100 and mod_meshtools.show_progress:
		   Blender.Window.DrawProgressBar(float(i)/lwochunk.chunksize, "Reading Faces")
		facev = []
		numfaceverts, = struct.unpack(">H", data.read(2))
		i += 2

		for j in range(numfaceverts):
			index, index_size = read_vx(data)
			i += index_size
			facev.append(index)
		facev.reverse()
		faces.append(facev)
	return faces

def fs_callback(filename):
	read(filename)

Blender.Window.FileSelector(fs_callback, "LWO Import")
