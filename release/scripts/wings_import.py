#!BPY

"""
Name: 'Wings3D...'
Blender: 232
Group: 'Import'
Tooltip: 'Import Wings3D File Format (*.wings)'
"""

# $Id$
#
# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | Feb 19, 2002                                            |
# | Released under the Blender Artistic Licence (BAL)       |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Read and write Wings3D File Format (*.wings)            |
# +---------------------------------------------------------+

import Blender, mod_meshtools
import struct, time, sys, os, zlib, cStringIO

# ==============================================
# === Read The 'Header' Common To All Chunks ===
# ==============================================
def read_chunkheader(data):
	data.read(2) #version, tag = struct.unpack(">BB", data.read(2))
	misc, namelen = struct.unpack(">BH", data.read(3))
	name = data.read(namelen)
	return name

# ==============================
# === Read The Material Mode ===
# ==============================
def read_mode(data):
	data.read(5)			# BL
	read_chunkheader(data)	# "mode"
	misc, namelen = struct.unpack(">BH", data.read(3))
	data.read(namelen)
	data.read(1)			# 6A

# =======================
# === Read Hard Edges ===
# =======================
def read_hardedges(data):
	tag = data.read(1)
	if tag == '\x6A':
		return # There are no hard edges
	elif tag == '\x6B':
		numhardedges, = struct.unpack(">H", data.read(2))
		print "numhardedges:", numhardedges
		for i in range(numhardedges):
			data.read(1)
	elif tag == '\x6C':
		numhardedges, = struct.unpack(">L", data.read(4))
		print "numhardedges:", numhardedges
		for i in range(numhardedges):
			misc = data.read(1)
			if misc == '\x61':    # next value is stored as a byte
				data.read(1)
			elif misc == '\x62':  # next value is stored as a long
				data.read(4)
		data.read(1) # 6A
	else:
		print tag

# ==================
# === Read Edges ===
# ==================
def read_edges(data):
	misc, numedges = struct.unpack(">BL", data.read(5))
	edge_table = {}  # the winged-edge table
	for i in range(numedges):
		if not i%100 and mod_meshtools.show_progress: Blender.Window.DrawProgressBar(float(i)/numedges, "Reading Edges")
		misc, etype = struct.unpack(">BL", data.read(5))
		if etype == 2:				# Vertex Colors
			data.read(10)			# or read_chunkheader(data)  # "color"
			data.read(5)			# BL
			r1,g1,b1,r2,g2,b2 = struct.unpack(">dddddd", data.read(48))
			#print "%3d %3d %3d | %3d %3d %3d" % (r1*255,g1*255,b1*255,r2*255,g2*255,b2*255),
			#print "%f %f %f | %f %f %f" % (r1, g1, b1, r2, g2, b2)
		data.read(9) # or read_chunkheader(data)  # "edge"
		edge = []			# the eight entries for this edge
		for e in range(8):	# Sv Ev | Lf Rf | Lp Ls | Rp Rs
			misc = data.read(1)
			if misc == '\x61':    # next value is stored as a byte
				entry, = struct.unpack(">B", data.read(1))
				edge.append(entry)
			elif misc == '\x62':  # next value is stored as a long
				entry, = struct.unpack(">L", data.read(4))
				edge.append(entry)
		edge_table[i] = edge
		data.read(1) # 6A
	data.read(1) # 6A
	return edge_table

# ==================
# === Read Faces ===
# ==================
def read_faces(data):
	misc, numfaces = struct.unpack(">BL", data.read(5))
	for i in range(numfaces):
		if not i%100 and mod_meshtools.show_progress: Blender.Window.DrawProgressBar(float(i)/numfaces, "Reading Faces")
		if data.read(1) == '\x6C':  # a material follows
			data.read(4)
			read_chunkheader(data)
			misc, namelen = struct.unpack(">BH", data.read(3))
			materialname = data.read(namelen)
			data.read(1)
	data.read(1) # 6A
	return numfaces

# ==================
# === Read Verts ===
# ==================
def read_verts(data):
	misc, numverts = struct.unpack(">BL", data.read(5))
	verts = []	# a list of verts
	for i in range(numverts):
		if not i%100 and mod_meshtools.show_progress: Blender.Window.DrawProgressBar(float(i)/numverts, "Reading Verts")
		data.read(10)
		x, y, z = struct.unpack(">ddd", data.read(24))  # double precision
		verts.append((x, -z, y))
		data.read(1) # 6A
	data.read(1) # 6A
	return verts

# =======================
# === Make Face Table ===
# =======================
def make_face_table(edge_table): # For Wings
	face_table = {}
	for i in range(len(edge_table)):
		Lf = edge_table[i][2]
		Rf = edge_table[i][3]
		face_table[Lf] = i
		face_table[Rf] = i
	return face_table

# =======================
# === Make Vert Table ===
# =======================
def make_vert_table(edge_table): # For Wings
	vert_table = {}
	for i in range(len(edge_table)):
		Sv = edge_table[i][0]
		Ev = edge_table[i][1]
		vert_table[Sv] = i
		vert_table[Ev] = i
	return vert_table

# ==================
# === Make Faces ===
# ==================
def make_faces(edge_table): # For Wings
	face_table = make_face_table(edge_table)
	faces=[]
	for i in range(len(face_table)):
		face_verts = []
		current_edge = face_table[i]
		while(1):
			if i == edge_table[current_edge][3]:
				next_edge = edge_table[current_edge][7] # Right successor edge
				next_vert = edge_table[current_edge][0]
			else:
				next_edge = edge_table[current_edge][5] # Left successor edge
				next_vert = edge_table[current_edge][1]
			face_verts.append(next_vert)
			current_edge = next_edge
			if current_edge == face_table[i]: break
		face_verts.reverse()
		faces.append(face_verts)
	return faces

# =======================
# === Dump Wings File ===
# =======================
def dump_wings(filename):
	import pprint
	start = time.clock()
	file = open(filename, "rb")
	header = file.read(15)
	fsize, = struct.unpack(">L",  file.read(4))   # file_size - 19
	misc,  = struct.unpack(">H",  file.read(2))
	dsize, = struct.unpack(">L",  file.read(4))   # uncompressed data size
	data   = file.read(fsize-6)
	file.close()
	data = zlib.decompress(data)
	if dsize != len(data): print "ERROR: uncompressed size does not match."
	data = cStringIO.StringIO(data)
	print "header:", header
	print read_chunkheader(data)  # === wings chunk ===
	data.read(4) # misc bytes
	misc, numobjs, = struct.unpack(">BL", data.read(5))
	print "filename:", filename
	print "numobjs :", numobjs
	for obj in range(numobjs):
		print read_chunkheader(data) # === object chunk ===
		misc, namelen = struct.unpack(">BH", data.read(3))
		objname = data.read(namelen)
		print read_chunkheader(data) # === winged chunk ===
		edge_table = read_edges(data)
		numfaces = read_faces(data)
		verts = read_verts(data)
		read_hardedges(data)

		face_table = {}  # contains an incident edge
		vert_table = {}  # contains an incident edge
		for i in range(len(edge_table)):
			face_table[edge_table[i][2]] = i  # generate face_table
			face_table[edge_table[i][3]] = i
			vert_table[edge_table[i][0]] = i  # generate vert_table
			vert_table[edge_table[i][1]] = i

		print "objname :", objname
		print "numedges:", len(edge_table)
		print "numfaces:", numfaces
		print "numverts:", len(verts)
		print
		print "Ä"*79
		print "edge_table:"
		pprint.pprint(edge_table)
		#for i in range(len(edge_table)): print "%2d" % (i), edge_table[i]
		print
		print "face_table:"
		pprint.pprint(face_table)
		#for i in range(len(face_table)): print "%2d %2d" % (i, face_table[i])
		print
		print "vert_table:"
		pprint.pprint(vert_table)
		#for i in range(len(vert_table)): print "%2d %2d" % (i, vert_table[i])
	file.close()
	end = time.clock()
	print '\a\r',
	sys.stderr.write("\nDone in %.2f %s" % (end-start, "seconds"))

# =========================
# === Read Wings Format ===
# =========================
def read(filename):
	start = time.clock()
	file = open(filename, "rb")
	header = file.read(15)
	fsize, = struct.unpack(">L",  file.read(4))   # file_size - 19
	misc,  = struct.unpack(">H",  file.read(2))
	dsize, = struct.unpack(">L",  file.read(4))   # uncompressed data size
	data   = file.read(fsize-6)
	#print file.tell(), "bytes"
	file.close()
	Blender.Window.DrawProgressBar(1.0, "Decompressing Data")
	data = zlib.decompress(data)
	data = cStringIO.StringIO(data)
	read_chunkheader(data) # wings chunk
	data.read(4)		   # misc bytes
	misc, numobjs = struct.unpack(">BL", data.read(5))
	message = "Successfully imported " + os.path.basename(filename) + '\n\n'
	message += "%s %8s %8s %8s\n" % ("Object".ljust(15), "faces", "edges", "verts")
	message += "%s %8s %8s %8s\n" % ("ÄÄÄÄÄÄ".ljust(15), "ÄÄÄÄÄ", "ÄÄÄÄÄ", "ÄÄÄÄÄ")

	for obj in range(numobjs):
		read_chunkheader(data) # object chunk
		misc, namelen = struct.unpack(">BH", data.read(3))
		objname = data.read(namelen)
		read_chunkheader(data) # winged chunk
		edge_table = read_edges(data)
		numfaces = read_faces(data)
		verts = read_verts(data)
		read_hardedges(data)
		read_mode(data)
		faces = make_faces(edge_table)
		message += "%s %8s %8s %8s\n" % (objname.ljust(15), len(faces), len(edge_table), len(verts))
		mod_meshtools.create_mesh(verts, faces, objname)

	material = data.read()
	#for i in material[0:6]: print "%02X" % ord(i),
	#print
	Blender.Window.DrawProgressBar(1.0, "Done")    # clear progressbar
	data.close()
	end = time.clock()
	seconds = "\nDone in %.2f %s" % (end-start, "seconds")
	message += seconds
	mod_meshtools.print_boxed(message)

def fs_callback(filename):
	read(filename)

Blender.Window.FileSelector(fs_callback, "Wings3D Import")
