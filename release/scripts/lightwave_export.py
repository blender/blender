#!BPY

"""
Name: 'LightWave (.lwo)...'
Blender: 243
Group: 'Export'
Tooltip: 'Export selected meshes to LightWave File Format (.lwo)'
"""

__author__ = "Anthony D'Agostino (Scorpius)"
__url__ = ("blender", "blenderartists.org",
"Author's homepage, http://www.redrival.com/scorpius")
__version__ = "Part of IOSuite 0.5"

__bpydoc__ = """\
This script exports meshes to LightWave file format.

LightWave is a full-featured commercial modeling and rendering
application. The lwo file format is composed of 'chunks,' is well
defined, and easy to read and write. It is similar in structure to the
trueSpace cob format.

Usage:<br>
	Select meshes to be exported and run this script from "File->Export" menu.

Supported:<br>
	UV Coordinates, Meshes, Materials, Material Indices, Specular
Highlights, and Vertex Colors. For added functionality, each object is
placed on its own layer. Someone added the CLIP chunk and imagename support.

Missing:<br>
	Not too much, I hope! :).

Known issues:<br>
	Empty objects crash has been fixed.

Notes:<br>
	For compatibility reasons, it also reads lwo files in the old LW
v5.5 format.
"""

# $Id$
#
# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | April 21, 2002                                          |
# | Read and write LightWave Object File Format (*.lwo)     |
# +---------------------------------------------------------+

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

import Blender
import struct, cStringIO, operator
import BPyMesh

VCOL_NAME = "\251 Per-Face Vertex Colors"
DEFAULT_NAME = "\251 Blender Default"
# ==============================
# === Write LightWave Format ===
# ==============================
def write(filename):
	start = Blender.sys.time()
	file = open(filename, "wb")
	
	scn = Blender.Scene.GetCurrent()
	objects = list(scn.objects.context)
	
	if not objects:
		Blender.Draw.PupMenu('Error%t|No Objects selected')
		return
	
	try:	objects.sort( key = lambda a: a.name )
	except:	objects.sort(lambda a,b: cmp(a.name, b.name))

	text = generate_text()
	desc = generate_desc()
	icon = "" #generate_icon()

	meshes = []
	for obj in objects:
		mesh = BPyMesh.getMeshFromObject(obj, None, True, False, scn)
		if mesh:
			mesh.transform(obj.matrixWorld)
			meshes.append(mesh)

	material_names = get_used_material_names(meshes)
	tags = generate_tags(material_names)
	surfs = generate_surfs(material_names)
	chunks = [text, desc, icon, tags]

	meshdata = cStringIO.StringIO()
	
	layer_index = 0
	
	for mesh in meshes:
		layr = generate_layr(obj.name, layer_index)
		pnts = generate_pnts(mesh)
		bbox = generate_bbox(mesh)
		pols = generate_pols(mesh)
		ptag = generate_ptag(mesh, material_names)
		clip = generate_clip(mesh, material_names)

		if mesh.faceUV:
			vmad_uv = generate_vmad_uv(mesh)  # per face

		if mesh.vertexColors:
			#if meshtools.average_vcols:
			#	vmap_vc = generate_vmap_vc(mesh)  # per vert
			#else:
			vmad_vc = generate_vmad_vc(mesh)  # per face

		write_chunk(meshdata, "LAYR", layr); chunks.append(layr)
		write_chunk(meshdata, "PNTS", pnts); chunks.append(pnts)
		write_chunk(meshdata, "BBOX", bbox); chunks.append(bbox)
		write_chunk(meshdata, "POLS", pols); chunks.append(pols)
		write_chunk(meshdata, "PTAG", ptag); chunks.append(ptag)

		if mesh.vertexColors:
			#if meshtools.average_vcols:
			#	write_chunk(meshdata, "VMAP", vmap_vc)
			#	chunks.append(vmap_vc)
			#else:
			write_chunk(meshdata, "VMAD", vmad_vc)
			chunks.append(vmad_vc)

		if mesh.faceUV:
			write_chunk(meshdata, "VMAD", vmad_uv)
			chunks.append(vmad_uv)
			write_chunk(meshdata, "CLIP", clip)
			chunks.append(clip)
		
		layer_index += 1
		mesh.verts = None # save some ram

	for surf in surfs:
		chunks.append(surf)

	write_header(file, chunks)
	write_chunk(file, "ICON", icon)
	write_chunk(file, "TEXT", text)
	write_chunk(file, "DESC", desc)
	write_chunk(file, "TAGS", tags)
	file.write(meshdata.getvalue()); meshdata.close()
	for surf in surfs:
		write_chunk(file, "SURF", surf)
	write_chunk(file, "DATE", "August 19, 2005")

	Blender.Window.DrawProgressBar(1.0, "")    # clear progressbar
	file.close()
	print '\a\r',
	print "Successfully exported %s in %.3f seconds" % (filename.split('\\')[-1].split('/')[-1], Blender.sys.time() - start)
	

# =======================================
# === Generate Null-Terminated String ===
# =======================================
def generate_nstring(string):
	if len(string)%2 == 0:	# even
		string += "\0\0"
	else:					# odd
		string += "\0"
	return string

# ===============================
# === Get Used Material Names ===
# ===============================
def get_used_material_names(meshes):
	matnames = {}
	for mesh in meshes:
		if (not mesh.materials) and mesh.vertexColors:
			# vcols only
			matnames[VCOL_NAME] = None
			
		elif mesh.materials and (not mesh.vertexColors):
			# materials only
			for material in mesh.materials:
				if material:
					matnames[material.name] = None
		elif (not mesh.materials) and (not mesh.vertexColors):
			# neither
			matnames[DEFAULT_NAME] = None
		else:
			# both
			for material in mesh.materials:
				if material:
					matnames[material.name] = None
	return matnames.keys()

# =========================================
# === Generate Tag Strings (TAGS Chunk) ===
# =========================================
def generate_tags(material_names):
	if material_names:
		material_names = map(generate_nstring, material_names)
		tags_data = reduce(operator.add, material_names)
	else:
		tags_data = generate_nstring('');
	return tags_data

# ========================
# === Generate Surface ===
# ========================
def generate_surface(name):
	#if name.find("\251 Per-") == 0:
	#	return generate_vcol_surf(mesh)
	if name == DEFAULT_NAME:
		return generate_default_surf()
	else:
		return generate_surf(name)

# ======================
# === Generate Surfs ===
# ======================
def generate_surfs(material_names):
	return map(generate_surface, material_names)

# ===================================
# === Generate Layer (LAYR Chunk) ===
# ===================================
def generate_layr(name, idx):
	data = cStringIO.StringIO()
	data.write(struct.pack(">h", idx))          # layer number
	data.write(struct.pack(">h", 0))            # flags
	data.write(struct.pack(">fff", 0, 0, 0))    # pivot
	data.write(generate_nstring(name))			# name
	return data.getvalue()

# ===================================
# === Generate Verts (PNTS Chunk) ===
# ===================================
def generate_pnts(mesh):
	data = cStringIO.StringIO()
	for i, v in enumerate(mesh.verts):
		if not i%100:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Writing Verts")
		x, y, z = v.co
		data.write(struct.pack(">fff", x, z, y))
	return data.getvalue()

# ==========================================
# === Generate Bounding Box (BBOX Chunk) ===
# ==========================================
def generate_bbox(mesh):
	data = cStringIO.StringIO()
	# need to transform verts here
	if mesh.verts:
		nv = [v.co for v in mesh.verts]
		xx = [ co[0] for co in nv ]
		yy = [ co[1] for co in nv ]
		zz = [ co[2] for co in nv ]
	else:
		xx = yy = zz = [0.0,]
	
	data.write(struct.pack(">6f", min(xx), min(zz), min(yy), max(xx), max(zz), max(yy)))
	return data.getvalue()

# ========================================
# === Average All Vertex Colors (Fast) ===
# ========================================
'''
def average_vertexcolors(mesh):
	vertexcolors = {}
	vcolor_add = lambda u, v: [u[0]+v[0], u[1]+v[1], u[2]+v[2], u[3]+v[3]]
	vcolor_div = lambda u, s: [u[0]/s, u[1]/s, u[2]/s, u[3]/s]
	for i, f in enumerate(mesh.faces):	# get all vcolors that share this vertex
		if not i%100:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Finding Shared VColors")
		col = f.col
		for j in xrange(len(f)):
			index = f[j].index
			color = col[j]
			r,g,b = color.r, color.g, color.b
			vertexcolors.setdefault(index, []).append([r,g,b,255])
	i = 0
	for index, value in vertexcolors.iteritems():	# average them
		if not i%100:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Averaging Vertex Colors")
		vcolor = [0,0,0,0]	# rgba
		for v in value:
			vcolor = vcolor_add(vcolor, v)
		shared = len(value)
		value[:] = vcolor_div(vcolor, shared)
		i+=1
	return vertexcolors
'''

# ====================================================
# === Generate Per-Vert Vertex Colors (VMAP Chunk) ===
# ====================================================
# Blender now has all vcols per face
"""
def generate_vmap_vc(mesh):
	data = cStringIO.StringIO()
	data.write("RGB ")                                      # type
	data.write(struct.pack(">H", 3))                        # dimension
	data.write(generate_nstring("Blender's Vertex Colors")) # name
	vertexcolors = average_vertexcolors(mesh)
	for i in xrange(len(vertexcolors)):
		try:	r, g, b, a = vertexcolors[i] # has a face user
		except:	r, g, b, a = 255,255,255,255
		data.write(struct.pack(">H", i)) # vertex index
		data.write(struct.pack(">fff", r/255.0, g/255.0, b/255.0))
	return data.getvalue()
"""

# ====================================================
# === Generate Per-Face Vertex Colors (VMAD Chunk) ===
# ====================================================
def generate_vmad_vc(mesh):
	data = cStringIO.StringIO()
	data.write("RGB ")                                      # type
	data.write(struct.pack(">H", 3))                        # dimension
	data.write(generate_nstring("Blender's Vertex Colors")) # name
	for i, f in enumerate(mesh.faces):
		if not i%100:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.faces), "Writing Vertex Colors")
		col = f.col
		f_v = f.v
		for j in xrange(len(f)-1, -1, -1): 			# Reverse order
			r,g,b, dummy = tuple(col[j])
			data.write(struct.pack(">H", f_v[j].index)) # vertex index
			data.write(struct.pack(">H", i)) # face index
			data.write(struct.pack(">fff", r/255.0, g/255.0, b/255.0))
	return data.getvalue()

# ================================================
# === Generate Per-Face UV Coords (VMAD Chunk) ===
# ================================================
def generate_vmad_uv(mesh):
	data = cStringIO.StringIO()
	data.write("TXUV")                                       # type
	data.write(struct.pack(">H", 2))                         # dimension
	data.write(generate_nstring("Blender's UV Coordinates")) # name
	
	for i, f in enumerate(mesh.faces):
		if not i%100:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.faces), "Writing UV Coordinates")
		
		uv = f.uv
		f_v = f.v
		for j in xrange(len(f)-1, -1, -1): 			# Reverse order
			U,V = uv[j]
			v = f_v[j].index
			data.write(struct.pack(">H", v)) # vertex index
			data.write(struct.pack(">H", i)) # face index
			data.write(struct.pack(">ff", U, V))
	return data.getvalue()

# ======================================
# === Generate Variable-Length Index ===
# ======================================
def generate_vx(index):
	if index < 0xFF00:
		value = struct.pack(">H", index)                 # 2-byte index
	else:
		value = struct.pack(">L", index | 0xFF000000)    # 4-byte index
	return value

# ===================================
# === Generate Faces (POLS Chunk) ===
# ===================================
def generate_pols(mesh):
	data = cStringIO.StringIO()
	data.write("FACE")  # polygon type
	for i,f in enumerate(mesh.faces):
		if not i%100:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.faces), "Writing Faces")
		data.write(struct.pack(">H", len(f))) # numfaceverts
		numfaceverts = len(f)
		f_v = f.v
		for j in xrange(numfaceverts-1, -1, -1): 			# Reverse order
			data.write(generate_vx(f_v[j].index))
	return data.getvalue()

# =================================================
# === Generate Polygon Tag Mapping (PTAG Chunk) ===
# =================================================
def generate_ptag(mesh, material_names):
	
	def surf_indicies(mat):
		try:
			if mat:
				return material_names.index(mat.name)
		except:
			pass
		
		return 0
		
	
	data = cStringIO.StringIO()
	data.write("SURF")  # polygon tag type
	mesh_materials = mesh.materials
	mesh_surfindicies = [surf_indicies(mat) for mat in mesh_materials]
	
	try:	VCOL_NAME_SURF_INDEX = material_names.index(VCOL_NAME)
	except:	VCOL_NAME_SURF_INDEX = 0
	
	try:	DEFAULT_NAME_SURF_INDEX = material_names.index(DEFAULT_NAME)
	except:	DEFAULT_NAME_SURF_INDEX = 0
	len_mat = len(mesh_materials)
	for i, f in enumerate(mesh.faces): # numfaces
		f_mat = f.mat
		if f_mat >= len_mat: f_mat = 0 # Rare annoying eror
			
		
		if not i%100:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.faces), "Writing Surface Indices")
		
		data.write(generate_vx(i))
		if (not mesh_materials) and mesh.vertexColors:		# vcols only
			surfidx = VCOL_NAME_SURF_INDEX
		elif mesh_materials and not mesh.vertexColors:		# materials only
			surfidx = mesh_surfindicies[f_mat]
		elif (not mesh_materials) and (not mesh.vertexColors):	# neither
			surfidx = DEFAULT_NAME_SURF_INDEX
		else:												# both
			surfidx = mesh_surfindicies[f_mat]
		
		data.write(struct.pack(">H", surfidx)) # surface index
	return data.getvalue()

# ===================================================
# === Generate VC Surface Definition (SURF Chunk) ===
# ===================================================
def generate_vcol_surf(mesh):
	data = cStringIO.StringIO()
	if mesh.vertexColors:
		surface_name = generate_nstring(VCOL_NAME)
	data.write(surface_name)
	data.write("\0\0")

	data.write("COLR")
	data.write(struct.pack(">H", 14))
	data.write(struct.pack(">fffH", 1, 1, 1, 0))

	data.write("DIFF")
	data.write(struct.pack(">H", 6))
	data.write(struct.pack(">fH", 0.0, 0))

	data.write("LUMI")
	data.write(struct.pack(">H", 6))
	data.write(struct.pack(">fH", 1.0, 0))

	data.write("VCOL")
	data.write(struct.pack(">H", 34))
	data.write(struct.pack(">fH4s", 1.0, 0, "RGB "))  # intensity, envelope, type
	data.write(generate_nstring("Blender's Vertex Colors")) # name

	data.write("CMNT")  # material comment
	comment = "Vertex Colors: Exported from Blender\256 243"
	comment = generate_nstring(comment)
	data.write(struct.pack(">H", len(comment)))
	data.write(comment)
	return data.getvalue()

# ================================================
# === Generate Surface Definition (SURF Chunk) ===
# ================================================
def generate_surf(material_name):
	data = cStringIO.StringIO()
	data.write(generate_nstring(material_name))
	data.write("\0\0")
	
	try:
		material = Blender.Material.Get(material_name)
		R,G,B = material.R, material.G, material.B
		ref = material.ref
		emit = material.emit
		spec = material.spec
		hard = material.hard
		
	except:
		material = None
		
		R=G=B = 1.0
		ref = 1.0
		emit = 0.0
		spec = 0.2
		hard = 0.0
	
		
	data.write("COLR")
	data.write(struct.pack(">H", 14))
	data.write(struct.pack(">fffH", R, G, B, 0))

	data.write("DIFF")
	data.write(struct.pack(">H", 6))
	data.write(struct.pack(">fH", ref, 0))

	data.write("LUMI")
	data.write(struct.pack(">H", 6))
	data.write(struct.pack(">fH", emit, 0))

	data.write("SPEC")
	data.write(struct.pack(">H", 6))
	data.write(struct.pack(">fH", spec, 0))

	data.write("GLOS")
	data.write(struct.pack(">H", 6))
	gloss = hard / (255/2.0)
	gloss = round(gloss, 1)
	data.write(struct.pack(">fH", gloss, 0))

	data.write("CMNT")  # material comment
	comment = material_name + ": Exported from Blender\256 243"
	comment = generate_nstring(comment)
	data.write(struct.pack(">H", len(comment)))
	data.write(comment)
	
	# Check if the material contains any image maps
	if material:
		mtextures = material.getTextures()									# Get a list of textures linked to the material
		for mtex in mtextures:
			if (mtex) and (mtex.tex.type == Blender.Texture.Types.IMAGE):	# Check if the texture is of type "IMAGE"
				data.write("BLOK")                  # Surface BLOK header
				data.write(struct.pack(">H", 104))  # Hardcoded and ugly! Will only handle 1 image per material

				# IMAP subchunk (image map sub header)
				data.write("IMAP")                  
				data_tmp = cStringIO.StringIO()
				data_tmp.write(struct.pack(">H", 0))  # Hardcoded - not sure what it represents
				data_tmp.write("CHAN")
				data_tmp.write(struct.pack(">H", 4))
				data_tmp.write("COLR")
				data_tmp.write("OPAC")                # Hardcoded texture layer opacity
				data_tmp.write(struct.pack(">H", 8))
				data_tmp.write(struct.pack(">H", 0))
				data_tmp.write(struct.pack(">f", 1.0))
				data_tmp.write(struct.pack(">H", 0))
				data_tmp.write("ENAB")
				data_tmp.write(struct.pack(">HH", 2, 1))  # 1 = texture layer enabled
				data_tmp.write("NEGA")
				data_tmp.write(struct.pack(">HH", 2, 0))  # Disable negative image (1 = invert RGB values)
				data_tmp.write("AXIS")
				data_tmp.write(struct.pack(">HH", 2, 1))
				data.write(struct.pack(">H", len(data_tmp.getvalue())))
				data.write(data_tmp.getvalue())

				# IMAG subchunk
				data.write("IMAG")
				data.write(struct.pack(">HH", 2, 1))
				data.write("PROJ")
				data.write(struct.pack(">HH", 2, 5)) # UV projection

				data.write("VMAP")
				uvname = generate_nstring("Blender's UV Coordinates")
				data.write(struct.pack(">H", len(uvname)))
				data.write(uvname)

	return data.getvalue()

# =============================================
# === Generate Default Surface (SURF Chunk) ===
# =============================================
def generate_default_surf():
	data = cStringIO.StringIO()
	material_name = DEFAULT_NAME
	data.write(generate_nstring(material_name))
	data.write("\0\0")

	data.write("COLR")
	data.write(struct.pack(">H", 14))
	data.write(struct.pack(">fffH", 1, 1, 1, 0))

	data.write("DIFF")
	data.write(struct.pack(">H", 6))
	data.write(struct.pack(">fH", 0.8, 0))

	data.write("LUMI")
	data.write(struct.pack(">H", 6))
	data.write(struct.pack(">fH", 0, 0))

	data.write("SPEC")
	data.write(struct.pack(">H", 6))
	data.write(struct.pack(">fH", 0.5, 0))

	data.write("GLOS")
	data.write(struct.pack(">H", 6))
	gloss = 50 / (255/2.0)
	gloss = round(gloss, 1)
	data.write(struct.pack(">fH", gloss, 0))

	data.write("CMNT")  # material comment
	comment = material_name + ": Exported from Blender\256 243"

	# vals = map(chr, xrange(164,255,1))
	# keys = xrange(164,255,1)
	# keys = map(lambda x: `x`, keys)
	# comment = map(None, keys, vals)
	# comment = reduce(operator.add, comment)
	# comment = reduce(operator.add, comment)

	comment = generate_nstring(comment)
	data.write(struct.pack(">H", len(comment)))
	data.write(comment)
	return data.getvalue()

# ============================================
# === Generate Object Comment (TEXT Chunk) ===
# ============================================
def generate_text():
	comment  = "Lightwave Export Script for Blender by Anthony D'Agostino"
	return generate_nstring(comment)

# ==============================================
# === Generate Description Line (DESC Chunk) ===
# ==============================================
def generate_desc():
	comment = "Copyright 2002 Scorpius Entertainment"
	return generate_nstring(comment)

# ==================================================
# === Generate Thumbnail Icon Image (ICON Chunk) ===
# ==================================================
def generate_icon():
	data = cStringIO.StringIO()
	file = open("f:/obj/radiosity/lwo2_icon.tga", "rb") # 60x60 uncompressed TGA
	file.read(18)
	icon_data = file.read(3600) # ?
	file.close()
	data.write(struct.pack(">HH", 0, 60))
	data.write(icon_data)
	#print len(icon_data)
	return data.getvalue()

# ===============================================
# === Generate CLIP chunk with STIL subchunks ===
# ===============================================
def generate_clip(mesh, material_names):
	data = cStringIO.StringIO()
	clipid = 1
	for i, material in enumerate(mesh.materials):									# Run through list of materials used by mesh
		if material:
			mtextures = material.getTextures()									# Get a list of textures linked to the material
			for mtex in mtextures:
				if (mtex) and (mtex.tex.type == Blender.Texture.Types.IMAGE):	# Check if the texture is of type "IMAGE"
					pathname = mtex.tex.image.filename							# If full path is needed use filename in place of name
					pathname = pathname[0:2] + pathname.replace("\\", "/")[3:]  # Convert to Modo standard path
					imagename = generate_nstring(pathname)
					data.write(struct.pack(">L", clipid))                       # CLIP sequence/id
					data.write("STIL")                                          # STIL image
					data.write(struct.pack(">H", len(imagename)))               # Size of image name
					data.write(imagename)
					clipid += 1
	return data.getvalue()

# ===================
# === Write Chunk ===
# ===================
def write_chunk(file, name, data):
	file.write(name)
	file.write(struct.pack(">L", len(data)))
	file.write(data)

# =============================
# === Write LWO File Header ===
# =============================
def write_header(file, chunks):
	chunk_sizes = map(len, chunks)
	chunk_sizes = reduce(operator.add, chunk_sizes)
	form_size = chunk_sizes + len(chunks)*8 + len("FORM")
	file.write("FORM")
	file.write(struct.pack(">L", form_size))
	file.write("LWO2")

def fs_callback(filename):
	if not filename.lower().endswith('.lwo'): filename += '.lwo'
	write(filename)

Blender.Window.FileSelector(fs_callback, "Export LWO", Blender.sys.makename(ext='.lwo'))
