#!BPY

"""
Name: 'Nendo...'
Blender: 232
Group: 'Export'
Tooltip: 'Export selected mesh to Nendo File Format (*.ndo)'
"""

# +---------------------------------------------------------+
# | Copyright (c) 2001 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | September 25, 2001                                      |
# | Released under the Blender Artistic Licence (BAL)       |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Read and write Nendo File Format (*.nendo)              |
# +---------------------------------------------------------+

import Blender, mod_meshtools
import struct, time, sys, os

# ==============================
# === Write Nendo 1.1 Format ===
# ==============================
def write(filename):
	start = time.clock()

	objects = Blender.Object.GetSelected()
	objname = objects[0].name
	meshname = objects[0].data.name
	mesh = Blender.NMesh.GetRaw(meshname)
	obj = Blender.Object.Get(objname)

	numedges = len(mesh.verts)+len(mesh.faces)-2
	maxedges = (2**16)-1	# Blender & Wings can read more edges
	#maxedges = 32767		# Nendo can't
	if numedges > maxedges:
		message = objname + " can't be exported to Nendo format (too many edges)."
		Blender.Draw.PupMenu("Nendo Export Error%t|"+message)
		return

	edge_table = mod_meshtools.generate_edgetable(mesh)

	try:
		edge_table = mod_meshtools.generate_edgetable(mesh)
		assert len(edge_table) <= maxedges
	except:
		edge_table = {}
		message = "Unable to generate Edge Table for the object named " + meshname
		mod_meshtools.print_boxed(message)
		Blender.Draw.PupMenu("Edge Table Error%t|"+message)
		Blender.Window.DrawProgressBar(1.0, "")    # clear progressbar
		return

	file = open(filename, "wb")
	write_header(file)
	write_object_flags(file, objname)
	write_edge_table(file, edge_table)
	write_face_table(file, edge_table)
	write_vert_table(file, edge_table, mesh)
	write_texture(file)
	file.close()

	Blender.Window.DrawProgressBar(1.0, "")    # clear progressbar
	print '\a\r',
	end = time.clock()
	seconds = " in %.2f %s" % (end-start, "seconds")
	message = "Successfully exported " + os.path.basename(filename) + seconds
	mod_meshtools.print_boxed(message)

# ====================
# === Write Header ===
# ====================
def write_header(file):
	file.write("nendo 1.1")
	file.write("\0\0")
	file.write("\1") # numobjects

# ==========================
# === Write Object Flags ===
# ==========================
def write_object_flags(file, objname):
	file.write("\1") # good flag
	file.write(struct.pack(">H", len(objname)))
	file.write(objname)
	file.write("\1"*4)
	data = struct.pack(">18f",0,0,0,1,1,1,1,1,1,1,1,1,0.2,0.2,0.2,1,100,1)
	data = "<<<< Nendo Export Script for Blender -- (c) 2004 Anthony D'Agostino >>>>"
	file.write(data)

# ========================
# === Write Edge Table ===
# ========================
def write_edge_table(file, edge_table):
	"+--------------------------------------+"
	"| Wings: Sv Ev | Lf Rf | Lp Ls | Rp Rs |"
	"| Nendo: Ev Sv | Lf Rf | Ls Rs | Rp Lp |"
	"+--------------------------------------+"
	#$print "edge_table"; pprint.pprint(edge_table)
	file.write(struct.pack(">H", len(edge_table)))
	keys = edge_table.keys()
	keys.sort()
	for key in keys:
		file.write(struct.pack(">2H", key[0], key[1]))                          # Ev Sv
		file.write(struct.pack(">2H", edge_table[key][0], edge_table[key][1]))  # Lf Rf
		file.write(struct.pack(">2H", edge_table[key][3], edge_table[key][5]))  # Ls Rs
		file.write(struct.pack(">2H", edge_table[key][4], edge_table[key][2]))  # Rp Lp
		file.write(struct.pack(">1B", 0))                                       # Hard flag
		try:
			r1,g1,b1 = map(lambda x:x*255, edge_table[key][8])
			r2,g2,b2 = map(lambda x:x*255, edge_table[key][7])
		except:
			r1,g1,b1 = map(lambda x:x*255, [0.9,0.8,0.7])
			r2,g2,b2 = r1,g1,b1
		file.write(struct.pack(">8B", r1,g1,b1,0,r2,g2,b2,0))

# ========================
# === Write Face Table ===
# ========================
def write_face_table(file, edge_table):
	face_table = build_face_table(edge_table)
	#$print "face_table"; pprint.pprint(face_table)
	file.write(struct.pack(">H", len(face_table)))
	keys = face_table.keys()
	keys.sort()
	for key in keys:
		file.write(struct.pack(">1H", face_table[key]))

# ========================
# === Write Vert Table ===
# ========================
def write_vert_table(file, edge_table, mesh):
	vert_table = build_vert_table(edge_table)
	#$print "vert_table"; pprint.pprint(vert_table)
	file.write(struct.pack(">H", len(vert_table)))
	keys = vert_table.keys()
	keys.sort()
	for key in keys:
		vertex = mesh.verts[key].co
		x,y,z = map(lambda x:x*10, vertex) # scale
		idx = vert_table[key]
		#$print "%i % f % f % f" % (idx, x, y, z)
		file.write(struct.pack(">1H3f", idx, x, z, -y))

# =====================
# === Write Texture ===
# =====================
def write_texture(file):
	file.write("\0"*5)

# ========================
# === Build Vert Table ===
# ========================
def build_vert_table(edge_table): # For Nendo
	vert_table = {}
	for key in edge_table.keys():
		i = edge_table[key][6]
		Sv = key[0]
		Ev = key[1]
		vert_table[Sv] = i
		vert_table[Ev] = i
	return vert_table

# ========================
# === Build Face Table ===
# ========================
def build_face_table(edge_table): # For Nendo
	face_table = {}
	for key in edge_table.keys():
		i = edge_table[key][6]
		Lf = edge_table[key][0]
		Rf = edge_table[key][1]
		face_table[Lf] = i
		face_table[Rf] = i
	return face_table

def fs_callback(filename):
	if filename.find('.ndo', -4) <= 0: filename += '.ndo'
	write(filename)

Blender.Window.FileSelector(fs_callback, "Nendo Export")
