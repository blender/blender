#!BPY

"""
Name: 'LightWave (.lwo)...'
Blender: 232
Group: 'Export'
Tooltip: 'Export selected meshes to LightWave File Format (.lwo)'
"""

__author__ = "Anthony D'Agostino (Scorpius)"
__url__ = ("blender", "elysiun",
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
# | Released under the Blender Artistic Licence (BAL)       |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Read and write LightWave Object File Format (*.lwo)     |
# +---------------------------------------------------------+

import Blender, meshtools
import struct, chunk, os, cStringIO, time, operator

# ==============================
# === Write LightWave Format ===
# ==============================
def write(filename):
	start = time.clock()
	file = open(filename, "wb")

	objects = Blender.Object.GetSelected()
	objects.sort(lambda a,b: cmp(a.name, b.name))
	if not objects:
		meshtools.print_boxed("No mesh objects are selected.")
		return

	if len(objects) > 20 and meshtools.show_progress:
		meshtools.show_progress = 0

	text = generate_text()
	desc = generate_desc()
	icon = "" #generate_icon()

	material_names = get_used_material_names(objects)
	tags = generate_tags(material_names)
	surfs = generate_surfs(material_names)
	chunks = [text, desc, icon, tags]

	meshdata = cStringIO.StringIO()
	layer_index = 0
	for object in objects:
		objname = object.name
		meshname = object.data.name
		mesh = Blender.NMesh.GetRaw(meshname)
		#mesh = Blender.NMesh.GetRawFromObject(meshname)	# for SubSurf
		obj = Blender.Object.Get(objname)
		if not mesh: continue

		layr = generate_layr(objname, layer_index)
		pnts = generate_pnts(mesh, obj.matrix)
		bbox = generate_bbox(mesh)
		pols = generate_pols(mesh)
		ptag = generate_ptag(mesh, material_names)
		clip = generate_clip(mesh, material_names)

		if mesh.hasFaceUV():
			vmad_uv = generate_vmad_uv(mesh)  # per face

		if meshtools.has_vertex_colors(mesh):
			if meshtools.average_vcols:
				vmap_vc = generate_vmap_vc(mesh)  # per vert
			else:
				vmad_vc = generate_vmad_vc(mesh)  # per face

		write_chunk(meshdata, "LAYR", layr); chunks.append(layr)
		write_chunk(meshdata, "PNTS", pnts); chunks.append(pnts)
		write_chunk(meshdata, "BBOX", bbox); chunks.append(bbox)
		write_chunk(meshdata, "POLS", pols); chunks.append(pols)
		write_chunk(meshdata, "PTAG", ptag); chunks.append(ptag)

		if meshtools.has_vertex_colors(mesh):
			if meshtools.average_vcols:
				write_chunk(meshdata, "VMAP", vmap_vc)
				chunks.append(vmap_vc)
			else:
				write_chunk(meshdata, "VMAD", vmad_vc)
				chunks.append(vmad_vc)

		if mesh.hasFaceUV():
			write_chunk(meshdata, "VMAD", vmad_uv)
			chunks.append(vmad_uv)
			write_chunk(meshdata, "CLIP", clip)
			chunks.append(clip)
		
		layer_index += 1

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
	end = time.clock()
	seconds = " in %.2f %s" % (end-start, "seconds")
	message = "Successfully exported " + os.path.basename(filename) + seconds
	meshtools.print_boxed(message)

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
def get_used_material_names(objects):
	matnames = {}
	for object in objects:
		objname = object.name
		meshname = object.data.name
		mesh = Blender.NMesh.GetRaw(meshname)
		if not mesh: continue
		if (not mesh.materials) and (meshtools.has_vertex_colors(mesh)):
			# vcols only
			if meshtools.average_vcols:
				matnames["\251 Per-Vert Vertex Colors"] = None
			else:
				matnames["\251 Per-Face Vertex Colors"] = None
		elif (mesh.materials) and (not meshtools.has_vertex_colors(mesh)):
			# materials only
			for material in mesh.materials:
				matnames[material.name] = None
		elif (not mesh.materials) and (not meshtools.has_vertex_colors(mesh)):
			# neither
			matnames["\251 Blender Default"] = None
		else:
			# both
			for material in mesh.materials:
				matnames[material.name] = None
	return matnames

# =========================================
# === Generate Tag Strings (TAGS Chunk) ===
# =========================================
def generate_tags(material_names):
	material_names = map(generate_nstring, material_names.keys())
	tags_data = reduce(operator.add, material_names)
	return tags_data

# ========================
# === Generate Surface ===
# ========================
def generate_surface(name, mesh):
	if name.find("\251 Per-") == 0:
		return generate_vcol_surf(mesh)
	elif name == "\251 Blender Default":
		return generate_default_surf()
	else:
		return generate_surf(name)

# ======================
# === Generate Surfs ===
# ======================
def generate_surfs(material_names):
	keys = material_names.keys()
	values = material_names.values()
	surfaces = map(generate_surface, keys, values)
	return surfaces

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
def generate_pnts(mesh, matrix):
	data = cStringIO.StringIO()
	for i in range(len(mesh.verts)):
		if not i%100 and meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Writing Verts")
		x, y, z = meshtools.apply_transform(mesh.verts[i].co, matrix)
		data.write(struct.pack(">fff", x, z, y))
	return data.getvalue()

# ==========================================
# === Generate Bounding Box (BBOX Chunk) ===
# ==========================================
def generate_bbox(mesh):
	data = cStringIO.StringIO()
	# need to transform verts here
	nv = map(getattr, mesh.verts, ["co"]*len(mesh.verts))
	xx = map(operator.getitem, nv, [0]*len(nv))
	yy = map(operator.getitem, nv, [1]*len(nv))
	zz = map(operator.getitem, nv, [2]*len(nv))
	data.write(struct.pack(">6f", min(xx), min(zz), min(yy), max(xx), max(zz), max(yy)))
	return data.getvalue()

# ========================================
# === Average All Vertex Colors (Fast) ===
# ========================================
def average_vertexcolors(mesh):
	vertexcolors = {}
	vcolor_add = lambda u, v: [u[0]+v[0], u[1]+v[1], u[2]+v[2], u[3]+v[3]]
	vcolor_div = lambda u, s: [u[0]/s, u[1]/s, u[2]/s, u[3]/s]
	for i in range(len(mesh.faces)):	# get all vcolors that share this vertex
		if not i%100 and meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Finding Shared VColors")
		for j in range(len(mesh.faces[i].v)):
			index = mesh.faces[i].v[j].index
			color = mesh.faces[i].col[j]
			r,g,b,a = color.r, color.g, color.b, color.a
			vertexcolors.setdefault(index, []).append([r,g,b,a])
	for i in range(len(vertexcolors)):	# average them
		if not i%100 and meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Averaging Vertex Colors")
		vcolor = [0,0,0,0]	# rgba
		for j in range(len(vertexcolors[i])):
			vcolor = vcolor_add(vcolor, vertexcolors[i][j])
		shared = len(vertexcolors[i])
		vertexcolors[i] = vcolor_div(vcolor, shared)
	return vertexcolors

# ====================================================
# === Generate Per-Vert Vertex Colors (VMAP Chunk) ===
# ====================================================
def generate_vmap_vc(mesh):
	data = cStringIO.StringIO()
	data.write("RGB ")                                      # type
	data.write(struct.pack(">H", 3))                        # dimension
	data.write(generate_nstring("Blender's Vertex Colors")) # name
	vertexcolors = average_vertexcolors(mesh)
	for i in range(len(vertexcolors)):
		r, g, b, a = vertexcolors[i]
		data.write(struct.pack(">H", i)) # vertex index
		data.write(struct.pack(">fff", r/255.0, g/255.0, b/255.0))
	return data.getvalue()

# ====================================================
# === Generate Per-Face Vertex Colors (VMAD Chunk) ===
# ====================================================
def generate_vmad_vc(mesh):
	data = cStringIO.StringIO()
	data.write("RGB ")                                      # type
	data.write(struct.pack(">H", 3))                        # dimension
	data.write(generate_nstring("Blender's Vertex Colors")) # name
	for i in range(len(mesh.faces)):
		if not i%100 and meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.faces), "Writing Vertex Colors")
		numfaceverts = len(mesh.faces[i].v)
		for j in range(numfaceverts-1, -1, -1): 			# Reverse order
			r = mesh.faces[i].col[j].r
			g = mesh.faces[i].col[j].g
			b = mesh.faces[i].col[j].b
			v = mesh.faces[i].v[j].index
			data.write(struct.pack(">H", v)) # vertex index
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
	for i in range(len(mesh.faces)):
		if not i%100 and meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.faces), "Writing UV Coordinates")
		numfaceverts = len(mesh.faces[i].v)
		for j in range(numfaceverts-1, -1, -1): 			# Reverse order
			U,V = mesh.faces[i].uv[j]
			v = mesh.faces[i].v[j].index
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
	for i in range(len(mesh.faces)):
		if not i%100 and meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.faces), "Writing Faces")
		data.write(struct.pack(">H", len(mesh.faces[i].v))) # numfaceverts
		numfaceverts = len(mesh.faces[i].v)
		for j in range(numfaceverts-1, -1, -1): 			# Reverse order
			index = mesh.faces[i].v[j].index
			data.write(generate_vx(index))
	return data.getvalue()

# =================================================
# === Generate Polygon Tag Mapping (PTAG Chunk) ===
# =================================================
def generate_ptag(mesh, material_names):
	data = cStringIO.StringIO()
	data.write("SURF")  # polygon tag type
	for i in range(len(mesh.faces)): # numfaces
		if not i%100 and meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.faces), "Writing Surface Indices")
		data.write(generate_vx(i))
		if (not mesh.materials) and (meshtools.has_vertex_colors(mesh)):		# vcols only
			if meshtools.average_vcols:
				name = "\251 Per-Vert Vertex Colors"
			else:
				name = "\251 Per-Face Vertex Colors"
		elif (mesh.materials) and (not meshtools.has_vertex_colors(mesh)):		# materials only
			idx = mesh.faces[i].mat	#erialIndex
			name = mesh.materials[idx].name
		elif (not mesh.materials) and (not meshtools.has_vertex_colors(mesh)):	# neither
			name = "\251 Blender Default"
		else:																		# both
			idx = mesh.faces[i].mat
			name = mesh.materials[idx].name
		names = material_names.keys()
		surfidx = names.index(name)
		data.write(struct.pack(">H", surfidx)) # surface index
	return data.getvalue()

# ===================================================
# === Generate VC Surface Definition (SURF Chunk) ===
# ===================================================
def generate_vcol_surf(mesh):
	data = cStringIO.StringIO()
	if meshtools.average_vcols and meshtools.has_vertex_colors(mesh):
		surface_name = generate_nstring("\251 Per-Vert Vertex Colors")
	else:
		surface_name = generate_nstring("\251 Per-Face Vertex Colors")
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
	comment = "Vertex Colors: Exported from Blender\256 " + meshtools.blender_version_str
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

	material = Blender.Material.Get(material_name)
	R,G,B = material.R, material.G, material.B
	data.write("COLR")
	data.write(struct.pack(">H", 14))
	data.write(struct.pack(">fffH", R, G, B, 0))

	data.write("DIFF")
	data.write(struct.pack(">H", 6))
	data.write(struct.pack(">fH", material.ref, 0))

	data.write("LUMI")
	data.write(struct.pack(">H", 6))
	data.write(struct.pack(">fH", material.emit, 0))

	data.write("SPEC")
	data.write(struct.pack(">H", 6))
	data.write(struct.pack(">fH", material.spec, 0))

	data.write("GLOS")
	data.write(struct.pack(">H", 6))
	gloss = material.hard / (255/2.0)
	gloss = round(gloss, 1)
	data.write(struct.pack(">fH", gloss, 0))

	data.write("CMNT")  # material comment
	comment = material_name + ": Exported from Blender\256 " + meshtools.blender_version_str
	comment = generate_nstring(comment)
	data.write(struct.pack(">H", len(comment)))
	data.write(comment)

	# Check if the material contains any image maps
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
	material_name = "\251 Blender Default"
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
	comment = material_name + ": Exported from Blender\256 " + meshtools.blender_version_str

	# vals = map(chr, range(164,255,1))
	# keys = range(164,255,1)
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
	comment  = "Lightwave Export Script for Blender "
	comment +=	meshtools.blender_version_str + "\n"
	comment += "by Anthony D'Agostino\n"
	comment += "scorpius@netzero.com\n"
	comment += "http://ourworld.compuserve.com/homepages/scorpius\n"
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
	for i in range(len(mesh.materials)):									# Run through list of materials used by mesh
		material = Blender.Material.Get(mesh.materials[i].name)
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
	if filename.find('.lwo', -4) <= 0: filename += '.lwo'
	write(filename)

Blender.Window.FileSelector(fs_callback, "Export LWO")
