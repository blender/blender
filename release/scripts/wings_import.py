#!BPY

"""
Name: 'Wings3D (.wings)...'
Blender: 232
Group: 'Import'
Tooltip: 'Import Wings3D File Format (.wings)'
"""

__author__ = "Anthony D'Agostino (Scorpius)"
__url__ = ("blender", "elysiun",
"Author's homepage, http://www.redrival.com/scorpius",
"Wings 3D, http://www.wings3d.com")
__version__ = "Update on version from IOSuite 0.5"

__bpydoc__ = """\
This script imports Wings3D files to Blender.

Wings3D is an open source polygon modeler written in Erlang, a
language similar to Lisp. The .wings file format is a binary
representation of erlang terms (lists, tuples, atoms, etc.) and is
compressed with zlib.

Usage:<br>
  Execute this script from the "File->Import" menu and choose a Wings file
to open.

Supported:<br>
	Meshes only. Not guaranteed to work in all situations.

Missing:<br>
  Materials, UV Coordinates, and Vertex Color info will be ignored.

Known issues:<br>
	Triangulation of convex polygons works fine, and uses a very simple
fanning algorithm. Convex polygons (i.e., shaped like the letter "U")
require a different algorithm, and will be triagulated incorrectly.

Notes:<br>
	Last tested with Wings 3D 0.98.25 & Blender 2.35a.<br>
	This version has improvements made by Adam Saltsman (AdamAtomic) and Toastie.
"""

# $Id$
#
# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | Feb 19, 2002                                            |

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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****


import Blender, meshtools
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
	hardedge_table = {} # hard edges table
	tag = data.read(1)
	if tag == '\x6A':
		return hardedge_table # There are no hard edges
	elif tag == '\x6B':
		numhardedges, = struct.unpack(">H", data.read(2))
		#print "numhardedges:", numhardedges
		for i in range(numhardedges):
			hardedge_table[i] = struct.unpack(">B", data.read(1))[0]
	elif tag == '\x6C':
		numhardedges, = struct.unpack(">L", data.read(4))
		#print "numhardedges:", numhardedges
		for i in range(numhardedges):
			misc = data.read(1)
			if misc == '\x61':    # next value is stored as a byte
				hardedge_table[i] = struct.unpack(">B", data.read(1))[0]
			elif misc == '\x62':  # next value is stored as a long
				hardedge_table[i] = struct.unpack(">L", data.read(4))[0]
		data.read(1) # 6A
	else:
		print tag
	return hardedge_table

# ==================
# === Read Edges ===
# ==================
def read_edges(data):
	misc, numedges = struct.unpack(">BL", data.read(5))
	edge_table = {}  # the winged-edge table
	for i in range(numedges):
		if not i%100 and meshtools.show_progress: Blender.Window.DrawProgressBar(float(i)/numedges, "Reading Edges")
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
	mat_table = {} #list of faces and material names
	misc, numfaces = struct.unpack(">BL", data.read(5))
	for i in range(numfaces):
		if not i%100 and meshtools.show_progress: Blender.Window.DrawProgressBar(float(i)/numfaces, "Reading Faces")
		if data.read(1) == '\x6C':  # a material follows
			data.read(4)
			read_chunkheader(data)
			misc, namelen = struct.unpack(">BH", data.read(3))
			mat_table[i] = data.read(namelen)
			data.read(1) # 6A?
	data.read(1) # 6A
	return mat_table

# ==================
# === Read Verts ===
# ==================
def read_verts(data):
	misc, numverts = struct.unpack(">BL", data.read(5))
	verts = []	# a list of verts
	for i in range(numverts):
		if not i%100 and meshtools.show_progress: Blender.Window.DrawProgressBar(float(i)/numverts, "Reading Verts")
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
		if Lf >= 0:
			face_table[Lf] = i
		if Rf >= 0:
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
			elif i == edge_table[current_edge][2]:
				next_edge = edge_table[current_edge][5] # Left successor edge
				next_vert = edge_table[current_edge][1]
			else:
				break
			face_verts.append(next_vert)
			current_edge = next_edge
			if current_edge == face_table[i]: break
		if len(face_verts) > 0:
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
		mat_table = read_faces(data)
		numfaces = len(mat_table)
		verts = read_verts(data)
		hardedge_table = read_hardedges(data)

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
		#pprint.pprint(edge_table)
		#for i in range(len(edge_table)): print "%2d" % (i), edge_table[i]
		print
		print "face_table:"
		#pprint.pprint(face_table)
		#for i in range(len(face_table)): print "%2d %2d" % (i, face_table[i])
		print
		print "vert_table:"
		#pprint.pprint(vert_table)
		#for i in range(len(vert_table)): print "%2d %2d" % (i, vert_table[i])
	file.close()
	end = time.clock()
	print '\a\r',
	sys.stderr.write("\nDone in %.2f %s\a\r" % (end-start, "seconds"))

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
		mat_table = read_faces(data)
		numfaces = len(mat_table)
		verts = read_verts(data)
		hardedge_table = read_hardedges(data)

		# Manually split hard edges
		# TODO: Handle the case where there are 2+ edges on a face
		duped = {}
		processed = []
		cleanup = []
		oldedgecount = len(edge_table)
		for i in range(len(verts)):
			duped[i] = -1
		for j in range(len(hardedge_table)):
			hardedge = hardedge_table[j]
			oldedge = edge_table[hardedge]
			newedge = [] # Copy old edge into a new list
			for k in range(len(oldedge)):
				newedge.append(oldedge[k])
			
			# Duplicate start vert if not duped already
			sv = newedge[0]
			if duped[sv] == -1:
				verts.append(verts[sv])
				duped[sv] = len(verts)-1
			newedge[0] = duped[sv]
			
			# Duplicate end vert if not duped already
			ev = newedge[1]
			if duped[ev] == -1:
				verts.append(verts[ev])
				duped[ev] = len(verts)-1
			newedge[1] = duped[ev]
			
			# Decide which way to cut the edge
			flip = 0
			for v in range(len(processed)):
				if processed[v][0] == oldedge[0]:
					flip = 1
				elif processed[v][1] == oldedge[1]:
					flip = 1
			if flip == 0:
				of = 3
				oe1 = 6
				oe2 = 7
				nf = 2
				ne1 = 4
				ne2 = 5
			else:
				of = 2
				oe1 = 4
				oe2 = 5
				nf = 3
				ne1 = 6
				ne2 = 7
			
			# Fix up side-specific edge fields
			oldedge[of]  = -1
			oldedge[oe1] = -1
			oldedge[oe2] = -1
			newedge[nf]  = -1
			newedge[ne1] = -1
			newedge[ne2] = -1
			
			# Store new edge's neighbors for cleanup later
			cleanup.append(edge_table[newedge[oe1]])
			cleanup.append(edge_table[newedge[oe2]])
			
			#DEBUG
			# Sv Ev | Lf Rf | Lp Ls | Rp Rs
			#print "Old Edge:",hardedge,oldedge
			#print "New Edge:",len(edge_table),newedge
			
			# Add this new edge to the edge table
			edge_table[len(edge_table)] = newedge
			if flip == 0:
				processed.append(oldedge) # mark it off as processed
			
		# Cycle through cleanup list and fix it up
		for c in range(len(cleanup)):
			cleanupedge = cleanup[c]
			
			# Fix up their verts in case they were duped
			sv = cleanupedge[0]
			if sv < len(duped):
				if duped[sv] >= 0:
					cleanupedge[0] = duped[sv]
			ev = cleanupedge[1]
			if ev < len(duped):
				if duped[ev] >= 0:
					cleanupedge[1] = duped[ev]
			
			# Fix up edge info (in case a hard edge was replaced with a new one)
			edgecount = c/2
			hardedge = hardedge_table[edgecount] # look up what edge we were replacing
			newedgenum = oldedgecount+edgecount  # calculate new edge's index
			if cleanupedge[4] == hardedge:
				cleanupedge[4] = newedgenum
			if cleanupedge[5] == hardedge:
				cleanupedge[5] = newedgenum
			if cleanupedge[6] == hardedge:
				cleanupedge[6] = newedgenum
			if cleanupedge[7] == hardedge:
				cleanupedge[7] = newedgenum
				
		#for i in range(len(edge_table)): print "%2d" % (i), edge_table[i]
		
		read_mode(data)
		faces = make_faces(edge_table)
		message += "%s %8s %8s %8s\n" % (objname.ljust(15), len(faces), len(edge_table), len(verts))
		meshtools.create_mesh(verts, faces, objname)

	material = data.read()
	#for i in material[0:6]: print "%02X" % ord(i),
	#print
	Blender.Window.DrawProgressBar(1.0, "Done")    # clear progressbar
	data.close()
	end = time.clock()
	seconds = "\nDone in %.2f %s" % (end-start, "seconds")
	message += seconds
	meshtools.print_boxed(message)

def fs_callback(filename):
	read(filename)

Blender.Window.FileSelector(fs_callback, "Import Wings3D")
