#!BPY

""" Registration info for Blender menus:
Name: 'AC3D (.ac)...'
Blender: 243
Group: 'Export'
Tip: 'Export selected meshes to AC3D (.ac) format'
"""

__author__ = "Willian P. Germano"
__url__ = ("blender", "blenderartists.org", "AC3D's homepage, http://www.ac3d.org",
	"PLib 3d gaming lib, http://plib.sf.net")
__version__ = "2.44 2007-05-05"

__bpydoc__ = """\
This script exports selected Blender meshes to AC3D's .ac file format.

AC3D is a simple commercial 3d modeller also built with OpenGL.
The .ac file format is an easy to parse text format well supported,
for example, by the PLib 3d gaming library (AC3D 3.x).

Supported:<br>
    UV-textured meshes with hierarchy (grouping) information.

Missing:<br>
    The 'url' tag, specific to AC3D.  It is easy to add by hand to the exported
file, if needed.

Known issues:<br>
    The ambient and emit data we can retrieve from Blender are single values,
that this script copies to R, G, B, giving shades of gray.<br>
    Loose edges (lines) receive the first material found in the mesh, if any, or a default white material.<br>
    In AC3D 4 "compatibility mode":<br>
    - shininess of materials is taken from the shader specularity value in Blender, mapped from [0.0, 2.0] to [0, 128];<br>
    - crease angle is exported, but in Blender it is limited to [1, 80], since there are other more powerful ways to control surface smoothing.  In AC3D 4.0 crease's range is [0.0, 180.0];

Config Options:<br>
    toggle:<br>
    - AC3D 4 mode: unset it to export without the 'crease' tag that was
introduced with AC3D 4.0 and with the old material handling;<br>
    - global coords: transform all vertices of all meshes to global coordinates;<br>
    - skip data: set it if you don't want mesh names (ME:, not OB: field)
to be exported as strings for AC's "data" tags (19 chars max);<br>
    - rgb mirror color can be exported as ambient and/or emissive if needed,
since Blender handles these differently;<br>
    - default mat: a default (white) material is added if some mesh was
left without mats -- it's better to always add your own materials;<br>
    - no split: don't split meshes (see above);<br>
    - set texture dir: override the actual textures path with a given default
path (or simply export the texture names, without dir info, if the path is
empty);<br>
    - per face 1 or 2 sided: override the "Double Sided" button that defines this behavior per whole mesh in favor of the UV Face Select mode "twosided" per face atribute;<br>
    - only selected: only consider selected objects when looking for meshes
to export (read notes below about tokens, too);<br>
    strings:<br>
    - export dir: default dir to export to;<br>
    - texture dir: override textures path with this path if 'set texture dir'
toggle is "on".

Notes:<br>
	This version updates:<br>
    - modified meshes are correctly exported, no need to apply the modifiers in Blender;<br>
    - correctly export each used material, be it assigned to the object or to its mesh data;<br>
    - exporting lines (edges) is again supported; color comes from first material found in the mesh, if any, or a default white one.<br>
    - there's a new option to choose between exporting meshes with transformed (global) coordinates or local ones;<br>
    Multiple textures per mesh are supported (mesh gets split);<br>
	Parents are exported as a group containing both the parent and its children;<br>
    Start mesh object names (OB: field) with "!" or "#" if you don't want them to be exported;<br>
    Start mesh object names (OB: field) with "=" or "$" to prevent them from being split (meshes with multiple textures or both textured and non textured faces are split unless this trick is used or the "no split" option is set.
"""

# $Id$
#
# --------------------------------------------------------------------------
# AC3DExport version 2.44
# Program versions: Blender 2.42+ and AC3Db files (means version 0xb)
# new: updated for new Blender version and Mesh module; supports lines (edges) again;
# option to export vertices transformed to global coordinates or not; now the modified
# (by existing mesh modifiers) mesh is exported; materials are properly exported, no
# matter if each of them is linked to the mesh or to the object. New (2.43.1): loose
# edges use color of first material found in the mesh, if any.
# --------------------------------------------------------------------------
# Thanks: Steve Baker for discussions and inspiration; for testing, bug
# reports, suggestions, patches: David Megginson, Filippo di Natale,
# Franz Melchior, Campbell Barton, Josh Babcock, Ralf Gerlich, Stewart Andreason.
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2004-2007: Willian P. Germano, wgermano _at_ ig.com.br
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# --------------------------------------------------------------------------

import Blender
from Blender import Object, Mesh, Material, Image, Mathutils, Registry
from Blender import sys as bsys

# Globals
REPORT_DATA = {
	'main': [],
	'errors': [],
	'warns': [],
	'nosplit': [],
	'noexport': []
}
TOKENS_DONT_EXPORT = ['!', '#']
TOKENS_DONT_SPLIT  = ['=', '$']

MATIDX_ERROR = 0

# flags:
LOOSE = Mesh.EdgeFlags['LOOSE']
FACE_TWOSIDED = Mesh.FaceModes['TWOSIDE']
MESH_TWOSIDED = Mesh.Modes['TWOSIDED']

REG_KEY = 'ac3d_export'

# config options:
GLOBAL_COORDS = True
SKIP_DATA = False
MIRCOL_AS_AMB = False
MIRCOL_AS_EMIS = False
ADD_DEFAULT_MAT = True
SET_TEX_DIR = True
TEX_DIR = ''
AC3D_4 = True # export crease value, compatible with AC3D 4 loaders
NO_SPLIT = False
ONLY_SELECTED = True
EXPORT_DIR = ''
PER_FACE_1_OR_2_SIDED = True

tooltips = {
	'GLOBAL_COORDS': "transform all vertices of all meshes to global coordinates",
	'SKIP_DATA': "don't export mesh names as data fields",
	'MIRCOL_AS_AMB': "export mirror color as ambient color",
	'MIRCOL_AS_EMIS': "export mirror color as emissive color",
	'ADD_DEFAULT_MAT': "always add a default white material",
	'SET_TEX_DIR': "don't export default texture paths (edit also \"tex dir\")",
	'EXPORT_DIR': "default / last folder used to export .ac files to",
	'TEX_DIR': "(see \"set tex dir\") dir to prepend to all exported texture names (leave empty for no dir)",
	'AC3D_4': "compatibility mode, adds 'crease' tag and slightly better material support",
	'NO_SPLIT': "don't split meshes with multiple textures (or both textured and non textured polygons)",
	'ONLY_SELECTED': "export only selected objects",
	'PER_FACE_1_OR_2_SIDED': "override \"Double Sided\" button in favor of per face \"twosided\" attribute (UV Face Select mode)"
}

def update_RegistryInfo():
	d = {}
	d['SKIP_DATA'] = SKIP_DATA
	d['MIRCOL_AS_AMB'] = MIRCOL_AS_AMB
	d['MIRCOL_AS_EMIS'] = MIRCOL_AS_EMIS
	d['ADD_DEFAULT_MAT'] = ADD_DEFAULT_MAT
	d['SET_TEX_DIR'] = SET_TEX_DIR
	d['TEX_DIR'] = TEX_DIR
	d['AC3D_4'] = AC3D_4
	d['NO_SPLIT'] = NO_SPLIT
	d['EXPORT_DIR'] = EXPORT_DIR
	d['ONLY_SELECTED'] = ONLY_SELECTED
	d['PER_FACE_1_OR_2_SIDED'] = PER_FACE_1_OR_2_SIDED
	d['tooltips'] = tooltips
	d['GLOBAL_COORDS'] = GLOBAL_COORDS
	Registry.SetKey(REG_KEY, d, True)

# Looking for a saved key in Blender.Registry dict:
rd = Registry.GetKey(REG_KEY, True)

if rd:
	try:
		AC3D_4 = rd['AC3D_4']
		SKIP_DATA = rd['SKIP_DATA']
		MIRCOL_AS_AMB = rd['MIRCOL_AS_AMB']
		MIRCOL_AS_EMIS = rd['MIRCOL_AS_EMIS']
		ADD_DEFAULT_MAT = rd['ADD_DEFAULT_MAT']
		SET_TEX_DIR = rd['SET_TEX_DIR']
		TEX_DIR = rd['TEX_DIR']
		EXPORT_DIR = rd['EXPORT_DIR']
		ONLY_SELECTED = rd['ONLY_SELECTED']
		NO_SPLIT = rd['NO_SPLIT']
		PER_FACE_1_OR_2_SIDED = rd['PER_FACE_1_OR_2_SIDED']
		GLOBAL_COORDS = rd['GLOBAL_COORDS']
	except KeyError: update_RegistryInfo()

else:
	update_RegistryInfo()

VERBOSE = True
CONFIRM_OVERWRITE = True

# check General scripts config key for default behaviors
rd = Registry.GetKey('General', True)
if rd:
	try:
		VERBOSE = rd['verbose']
		CONFIRM_OVERWRITE = rd['confirm_overwrite']
	except: pass


# The default material to be used when necessary (see ADD_DEFAULT_MAT)
DEFAULT_MAT = \
'MATERIAL "DefaultWhite" rgb 1 1 1  amb 1 1 1  emis 0 0 0  \
spec 0.5 0.5 0.5  shi 64  trans 0'

# This transformation aligns Blender and AC3D coordinate systems:
BLEND_TO_AC3D_MATRIX = Mathutils.Matrix([1,0,0,0], [0,0,-1,0], [0,1,0,0], [0,0,0,1])

def Round_s(f):
	"Round to default precision and turn value to a string"
	r = round(f,6) # precision set to 10e-06
	if r == int(r):
		return str(int(r))
	else:
		return str(r)
 
def transform_verts(verts, m):
	vecs = []
	for v in verts:
		x, y, z = v.co
		vec = Mathutils.Vector([x, y, z, 1])
		vecs.append(vec*m)
	return vecs

def get_loose_edges(mesh):
	loose = LOOSE
	return [e for e in mesh.edges if e.flag & loose]

# ---

# meshes with more than one texture assigned
# are split and saved as these foomeshes
class FooMesh:

	class FooVert:
		def __init__(self, v):
			self.v = v
			self.index = 0

	class FooFace:
		def __init__(self, foomesh, f):
			self.f = f
			foov = foomesh.FooVert
			self.v = [foov(f.v[0]), foov(f.v[1])]
			len_fv = len(f.v)
			if len_fv > 2 and f.v[2]:
				self.v.append(foov(f.v[2]))
				if len_fv > 3 and f.v[3]: self.v.append(foov(f.v[3]))

		def __getattr__(self, attr):
			if attr == 'v': return self.v
			return getattr(self.f, attr)

		def __len__(self):
			return len(self.f)

	def __init__(self, tex, faces, mesh):
		self.name = mesh.name
		self.mesh = mesh
		self.looseEdges = []
		self.faceUV = mesh.faceUV
		self.degr = mesh.degr
		vidxs = [0]*len(mesh.verts)
		foofaces = []
		for f in faces:
			foofaces.append(self.FooFace(self, f))
			for v in f.v:
				if v: vidxs[v.index] = 1
		i = 0
		fooverts = []
		for v in mesh.verts:
			if vidxs[v.index]:
				fooverts.append(v)
				vidxs[v.index] = i
				i += 1
		for f in foofaces:
			for v in f.v:
				if v: v.index = vidxs[v.v.index]
		self.faces = foofaces
		self.verts = fooverts


class AC3DExport: # the ac3d exporter part

	def __init__(self, scene_objects, file):

		global ARG, SKIP_DATA, ADD_DEFAULT_MAT, DEFAULT_MAT

		header = 'AC3Db'
		self.file = file
		self.buf = ''
		self.mbuf = []
		self.mlist = []
		world_kids = 0
		parents_list = self.parents_list = []
		kids_dict = self.kids_dict = {}
		objs = []
		exp_objs = self.exp_objs = []
		tree = {}

		file.write(header+'\n')

		objs = \
			[o for o in scene_objects if o.type in ['Mesh', 'Empty']]

		# create a tree from parents to children objects

		for obj in objs[:]:
			parent = obj.parent
			lineage = [obj]

			while parent:
				parents_list.append(parent.name)
				obj = parent
				parent = parent.getParent()
				lineage.insert(0, obj)

			d = tree
			for i in xrange(len(lineage)):
				lname = lineage[i].getType()[:2] + lineage[i].name
				if lname not in d.keys():
					d[lname] = {}
				d = d[lname]

		# traverse the tree to get an ordered list of names of objects to export
		self.traverse_dict(tree)

		world_kids = len(tree.keys())

		# get list of objects to export, start writing the .ac file

		objlist = [Object.Get(name) for name in exp_objs]

		meshlist = [o for o in objlist if o.type == 'Mesh']

		# create a temporary mesh to hold actual (modified) mesh data
		TMP_mesh = Mesh.New('tmp_for_ac_export')

		# write materials

		self.MATERIALS(meshlist, TMP_mesh)
		mbuf = self.mbuf
		if not mbuf or ADD_DEFAULT_MAT:
			mbuf.insert(0, "%s\n" % DEFAULT_MAT)
		mbuf = "".join(mbuf)
		file.write(mbuf)

		file.write('OBJECT world\nkids %s\n' % world_kids)

		# write the objects

		for obj in objlist:
			self.obj = obj

			objtype = obj.type
			objname = obj.name
			kidsnum = kids_dict[objname]

			# A parent plus its children are exported as a group.
			# If the parent is a mesh, its rot and loc are exported as the
			# group rot and loc and the mesh (w/o rot and loc) is added to the group.
			if kidsnum:
				self.OBJECT('group')
				self.name(objname)
				if objtype == 'Mesh':
					kidsnum += 1
				if not GLOBAL_COORDS:
					localmatrix = obj.getMatrix('localspace')
					if not obj.getParent():
						localmatrix *= BLEND_TO_AC3D_MATRIX
					self.rot(localmatrix.rotationPart()) 
					self.loc(localmatrix.translationPart())
				self.kids(kidsnum)

			if objtype == 'Mesh':
				mesh = TMP_mesh # temporary mesh to hold actual (modified) mesh data
				mesh.getFromObject(objname)
				self.mesh = mesh
				if mesh.faceUV:
					meshes = self.split_mesh(mesh)
				else:
					meshes = [mesh]
				if len(meshes) > 1:
					if NO_SPLIT or self.dont_split(objname):
						self.export_mesh(mesh, ob)
						REPORT_DATA['nosplit'].append(objname)
					else:
						self.OBJECT('group')
						self.name(objname)
						self.kids(len(meshes))
						counter = 0
						for me in meshes:
							self.export_mesh(me, obj,
								name = '%s_%s' % (obj.name, counter), foomesh = True)
							self.kids()
							counter += 1
				else:
					self.export_mesh(mesh, obj)
					self.kids()


	def traverse_dict(self, d):
		kids_dict = self.kids_dict
		exp_objs = self.exp_objs
		keys = d.keys()
		keys.sort() # sort for predictable output
		keys.reverse()
		for k in keys:
			objname = k[2:]
			klen = len(d[k])
			kids_dict[objname] = klen
			if self.dont_export(objname):
				d.pop(k)
				parent = Object.Get(objname).getParent()
				if parent: kids_dict[parent.name] -= 1
				REPORT_DATA['noexport'].append(objname)
				continue
			if klen:
				self.traverse_dict(d[k])
				exp_objs.insert(0, objname)
			else:
				if k.find('Em', 0) == 0: # Empty w/o children
					d.pop(k)
					parent = Object.Get(objname).getParent()
					if parent: kids_dict[parent.name] -= 1
				else:
					exp_objs.insert(0, objname)

	def dont_export(self, name): # if name starts with '!' or '#'
		length = len(name)
		if length >= 1:
			if name[0] in TOKENS_DONT_EXPORT: # '!' or '#' doubled (escaped): export
				if length > 1 and name[1] == name[0]:
					return 0
				return 1

	def dont_split(self, name): # if name starts with '=' or '$'
		length = len(name)
		if length >= 1:
			if name[0] in TOKENS_DONT_SPLIT: # '=' or '$' doubled (escaped): split
				if length > 1 and name[1] == name[0]:
					return 0
				return 1

	def split_mesh(self, mesh):
		tex_dict = {0:[]}
		for f in mesh.faces:
			if f.image:
				if not f.image.name in tex_dict: tex_dict[f.image.name] = []
				tex_dict[f.image.name].append(f)
			else: tex_dict[0].append(f)
		keys = tex_dict.keys()
		len_keys = len(keys)
		if not tex_dict[0]:
			len_keys -= 1
			tex_dict.pop(0)
			keys.remove(0)
		elif len_keys > 1:
			lines = []
			anyimgkey = [k for k in keys if k != 0][0]
			for f in tex_dict[0]:
				if len(f.v) < 3:
					lines.append(f)
			if len(tex_dict[0]) == len(lines):
				for l in lines:
					tex_dict[anyimgkey].append(l)
				len_keys -= 1
				tex_dict.pop(0)
		if len_keys > 1:
			foo_meshes = []
			for k in keys:
				faces = tex_dict[k]
				foo_meshes.append(FooMesh(k, faces, mesh))
			foo_meshes[0].edges = get_loose_edges(mesh)
			return foo_meshes
		return [mesh]

	def export_mesh(self, mesh, obj, name = None, foomesh = False):
		file = self.file
		self.OBJECT('poly')
		if not name: name = obj.name
		self.name(name)
		if not SKIP_DATA:
			meshname = obj.getData(name_only = True)
			self.data(len(meshname), meshname)
		if mesh.faceUV:
			texline = self.texture(mesh.faces)
			if texline: file.write(texline)
		if AC3D_4:
			self.crease(mesh.degr)

		# If exporting using local coordinates, children object coordinates should not be
		# transformed to ac3d's coordinate system, since that will be accounted for in
		# their topmost parents (the parents w/o parents) transformations.
		if not GLOBAL_COORDS:
			# We hold parents in a list, so they also don't get transformed,
			# because for each parent we create an ac3d group to hold both the
			# parent and its children.
			if obj.name not in self.parents_list:
				localmatrix = obj.getMatrix('localspace')
				if not obj.getParent():
					localmatrix *= BLEND_TO_AC3D_MATRIX
				self.rot(localmatrix.rotationPart())
				self.loc(localmatrix.translationPart())
			matrix = None
		else:
			matrix = obj.getMatrix() * BLEND_TO_AC3D_MATRIX

		self.numvert(mesh.verts, matrix)
		self.numsurf(mesh, foomesh)

	def MATERIALS(self, meshlist, me):
		for meobj in meshlist:
			me.getFromObject(meobj)
			mats = me.materials
			mbuf = []
			mlist = self.mlist
			for m in mats:
				if not m: continue
				name = m.name
				if name not in mlist:
					mlist.append(name)
					M = Material.Get(name)
					material = 'MATERIAL "%s"' % name
					mirCol = "%s %s %s" % (Round_s(M.mirCol[0]), Round_s(M.mirCol[1]),
						Round_s(M.mirCol[2]))
					rgb = "rgb %s %s %s" % (Round_s(M.R), Round_s(M.G), Round_s(M.B))
					ambval = Round_s(M.amb)
					amb = "amb %s %s %s" % (ambval, ambval, ambval)
					spec = "spec %s %s %s" % (Round_s(M.specCol[0]),
						 Round_s(M.specCol[1]), Round_s(M.specCol[2]))
					if AC3D_4:
						emit = Round_s(M.emit)
						emis = "emis %s %s %s" % (emit, emit, emit)
						shival = int(M.spec * 64)
					else:
						emis = "emis 0 0 0"
						shival = 72
					shi = "shi %s" % shival
					trans = "trans %s" % (Round_s(1 - M.alpha))
					if MIRCOL_AS_AMB:
						amb = "amb %s" % mirCol 
					if MIRCOL_AS_EMIS:
						emis = "emis %s" % mirCol
					mbuf.append("%s %s %s %s %s %s %s\n" \
						% (material, rgb, amb, emis, spec, shi, trans))
			self.mlist = mlist
			self.mbuf.append("".join(mbuf))

	def OBJECT(self, type):
		self.file.write('OBJECT %s\n' % type)

	def name(self, name):
		if name[0] in TOKENS_DONT_EXPORT or name[0] in TOKENS_DONT_SPLIT:
			if len(name) > 1: name = name[1:]
		self.file.write('name "%s"\n' % name)

	def kids(self, num = 0):
		self.file.write('kids %s\n' % num)

	def data(self, num, str):
		self.file.write('data %s\n%s\n' % (num, str))

	def texture(self, faces):
		tex = ""
		for f in faces:
			if f.image:
				tex = f.image.name
				break
		if tex:
			image = Image.Get(tex)
			texfname = image.filename
			if SET_TEX_DIR:
				texfname = bsys.basename(texfname)
				if TEX_DIR:
					texfname = bsys.join(TEX_DIR, texfname)
			buf = 'texture "%s"\n' % texfname
			xrep = image.xrep
			yrep = image.yrep
			buf += 'texrep %s %s\n' % (xrep, yrep)
			self.file.write(buf)

	def rot(self, matrix):
		rot = ''
		not_I = 0 # not identity
		matstr = []
		for i in [0, 1, 2]:
			r = map(Round_s, matrix[i])
			not_I += (r[0] != '0')+(r[1] != '0')+(r[2] != '0')
			not_I -= (r[i] == '1')
			for j in [0, 1, 2]:
				matstr.append(' %s' % r[j])
		if not_I: # no need to write identity
			self.file.write('rot%s\n' % "".join(matstr))
				
	def loc(self, loc):
		loc = map(Round_s, loc)
		if loc != ['0', '0', '0']: # no need to write default
			self.file.write('loc %s %s %s\n' % (loc[0], loc[1], loc[2]))

	def crease(self, crease):
		self.file.write('crease %f\n' % crease)

	def numvert(self, verts, matrix):
		file = self.file
		nvstr = []
		nvstr.append("numvert %s\n" % len(verts))

		if matrix:
			verts = transform_verts(verts, matrix)
			for v in verts:
				v = map (Round_s, v)
				nvstr.append("%s %s %s\n" % (v[0], v[1], v[2]))
		else:
			for v in verts:
				v = map(Round_s, v.co)
				nvstr.append("%s %s %s\n" % (v[0], v[1], v[2]))

		file.write("".join(nvstr))

	def numsurf(self, mesh, foomesh = False):

		global MATIDX_ERROR

		# local vars are faster and so better in tight loops
		lc_ADD_DEFAULT_MAT = ADD_DEFAULT_MAT
		lc_MATIDX_ERROR = MATIDX_ERROR
		lc_PER_FACE_1_OR_2_SIDED = PER_FACE_1_OR_2_SIDED
		lc_FACE_TWOSIDED = FACE_TWOSIDED
		lc_MESH_TWOSIDED = MESH_TWOSIDED

		faces = mesh.faces
		hasFaceUV = mesh.faceUV
		if foomesh:
			looseEdges = mesh.looseEdges
		else:
			looseEdges = get_loose_edges(mesh)

		file = self.file
 
		file.write("numsurf %s\n" % (len(faces) + len(looseEdges)))

		if not foomesh: verts = list(self.mesh.verts)

		materials = self.mesh.materials
		mlist = self.mlist
		matidx_error_reported = False
		objmats = []
		for omat in materials:
			if omat: objmats.append(omat.name)
			else: objmats.append(None)
		for f in faces:
			if not objmats:
				m_idx = 0
			elif objmats[f.mat] in mlist:
				m_idx = mlist.index(objmats[f.mat])
			else:
				if not lc_MATIDX_ERROR:
					rdat = REPORT_DATA['warns']
					rdat.append("Object %s" % self.obj.name)
					rdat.append("has at least one material *index* assigned but not")
					rdat.append("defined (not linked to an existing material).")
					rdat.append("Result: some faces may be exported with a wrong color.")
					rdat.append("You can assign materials in the Edit Buttons window (F9).")
				elif not matidx_error_reported:
					midxmsg = "- Same for object %s." % self.obj.name
					REPORT_DATA['warns'].append(midxmsg)
				lc_MATIDX_ERROR += 1
				matidx_error_reported = True
				m_idx = 0
				if lc_ADD_DEFAULT_MAT: m_idx -= 1
			refs = len(f)
			flaglow = 0 # polygon
			if lc_PER_FACE_1_OR_2_SIDED and hasFaceUV: # per face attribute
				two_side = f.mode & lc_FACE_TWOSIDED
			else: # global, for the whole mesh
				two_side = self.mesh.mode & lc_MESH_TWOSIDED
			two_side = (two_side > 0) << 1
			flaghigh = f.smooth | two_side
			surfstr = "SURF 0x%d%d\n" % (flaghigh, flaglow)
			if lc_ADD_DEFAULT_MAT and objmats: m_idx += 1
			matstr = "mat %s\n" % m_idx
			refstr = "refs %s\n" % refs
			u, v, vi = 0, 0, 0
			fvstr = []
			if foomesh:
				for vert in f.v:
					fvstr.append(str(vert.index))
					if hasFaceUV:
						u = f.uv[vi][0]
						v = f.uv[vi][1]
						vi += 1
					fvstr.append(" %s %s\n" % (u, v))
			else:
				for vert in f.v:
					fvstr.append(str(verts.index(vert)))
					if hasFaceUV:
						u = f.uv[vi][0]
						v = f.uv[vi][1]
						vi += 1
					fvstr.append(" %s %s\n" % (u, v))

			fvstr = "".join(fvstr)

			file.write("%s%s%s%s" % (surfstr, matstr, refstr, fvstr))

		# material for loose edges
		edges_mat = 0 # default to first material
		for omat in objmats: # but look for a material from this mesh
			if omat in mlist:
				edges_mat = mlist.index(omat)
				if lc_ADD_DEFAULT_MAT: edges_mat += 1
				break

		for e in looseEdges:
			fvstr = []
			#flaglow = 2 # 1 = closed line, 2 = line
			#flaghigh = 0
			#surfstr = "SURF 0x%d%d\n" % (flaghigh, flaglow)
			surfstr = "SURF 0x02\n"

			fvstr.append("%d 0 0\n" % verts.index(e.v1))
			fvstr.append("%d 0 0\n" % verts.index(e.v2))
			fvstr = "".join(fvstr)

			matstr = "mat %d\n" % edges_mat # for now, use first material 
			refstr = "refs 2\n" # 2 verts

			file.write("%s%s%s%s" % (surfstr, matstr, refstr, fvstr))

		MATIDX_ERROR = lc_MATIDX_ERROR

# End of Class AC3DExport

from Blender.Window import FileSelector

def report_data():
	global VERBOSE

	if not VERBOSE: return

	d = REPORT_DATA
	msgs = {
		'0main': '%s\nExporting meshes to AC3D format' % str(19*'-'),
		'1warns': 'Warnings',
		'2errors': 'Errors',
		'3nosplit': 'Not split (because name starts with "=" or "$")',
		'4noexport': 'Not exported (because name starts with "!" or "#")'
	}
	if NO_SPLIT:
		l = msgs['3nosplit']
		l = "%s (because OPTION NO_SPLIT is set)" % l.split('(')[0] 
		msgs['3nosplit'] = l
	keys = msgs.keys()
	keys.sort()
	for k in keys:
		msgk = msgs[k]
		msg = '\n'.join(d[k[1:]])
		if msg:
			print '\n-%s:' % msgk
			print msg

# File Selector callback:
def fs_callback(filename):
	global EXPORT_DIR, OBJS, CONFIRM_OVERWRITE, VERBOSE

	if not filename.endswith('.ac'): filename = '%s.ac' % filename

	if bsys.exists(filename) and CONFIRM_OVERWRITE:
		if Blender.Draw.PupMenu('OVERWRITE?%t|File exists') != 1:
			return

	Blender.Window.WaitCursor(1)
	starttime = bsys.time()

	export_dir = bsys.dirname(filename)
	if export_dir != EXPORT_DIR:
		EXPORT_DIR = export_dir
		update_RegistryInfo()

	try:
		file = open(filename, 'w')
	except IOError, (errno, strerror):
		error = "IOError #%s: %s" % (errno, strerror)
		REPORT_DATA['errors'].append("Saving failed - %s." % error)
		error_msg = "Couldn't save file!%%t|%s" % error
		Blender.Draw.PupMenu(error_msg)
		return

	try:
		test = AC3DExport(OBJS, file)
	except:
		file.close()
		raise
	else:
		file.close()
		endtime = bsys.time() - starttime
		REPORT_DATA['main'].append("Done. Saved to: %s" % filename)
		REPORT_DATA['main'].append("Data exported in %.3f seconds." % endtime)

	if VERBOSE: report_data()
	Blender.Window.WaitCursor(0)


# -- End of definitions

scn = Blender.Scene.GetCurrent()

if ONLY_SELECTED:
	OBJS = list(scn.objects.context)
else:
	OBJS = list(scn.objects)

if not OBJS:
	Blender.Draw.PupMenu('ERROR: no objects selected')
else:
	fname = bsys.makename(ext=".ac")
	if EXPORT_DIR:
		fname = bsys.join(EXPORT_DIR, bsys.basename(fname))
	FileSelector(fs_callback, "Export AC3D", fname)
