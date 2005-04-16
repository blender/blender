#!BPY

""" Registration info for Blender menus:
Name: 'AC3D (.ac)...'
Blender: 236
Group: 'Export'
Tip: 'Export selected meshes to AC3D (.ac) format'
"""

__author__ = "Willian P. Germano"
__url__ = ("blender", "elysiun", "AC3D's homepage, http://www.ac3d.org",
	"PLib 3d gaming lib, http://plib.sf.net")
__version__ = "2.36 2005-04-14"

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
    In AC3D 4 "compatibility mode":<br>
    - shininess of materials is taken from the shader specularity value in Blender, mapped from [0.0, 2.0] to [0, 128];<br>
    - crease angle is exported, but in Blender it is limited to [1, 80], since there are other more powerful ways to control surface smoothing.  In AC3D 4.0 crease's range is [0.0, 180.0];

Config Options:<br>
    toggle:
    - AC3D 4 mode: unset it to export without the 'crease' tag that was
introduced with AC3D 4.0 and with the old material handling;<br>
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
    strings:
    - export dir: default dir to export to;<br>
    - texture dir: override textures path with this path if 'set texture dir'
toggle is "on".

Notes:<br>
    This version is considerably faster than previous ones for large meshes;<br>
    Multiple textures per mesh are supported (mesh gets split);<br>
    Parenting with meshes or empties as parents is converted to AC3D group
information;<br>
    Start mesh object names (OB: field) with "!" or "#" if you don't want them to be exported;<br>
    Start mesh object names (OB: field) with "=" or "$" to prevent them from being split (meshes with multiple textures or both textured and non textured faces are split unless this trick is used or the "no split" option is set.
"""

# $Id$
#
# --------------------------------------------------------------------------
# AC3DExport version 2.36+
# Program versions: Blender 2.36+ and AC3Db files (means version 0xb)
# new: faster, supports multiple textures per object and parenting is
# properly exported as group info, adapted to work with the Config Editor
# --------------------------------------------------------------------------
# Thanks: Steve Baker for discussions and inspiration; for testing, bug
# reports, suggestions: David Megginson, Filippo di Natale, Franz Melchior
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2004: Willian P. Germano, wgermano _at_ ig.com.br
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
from Blender import sys as bsys
from Blender import Mathutils

# Globals
ERROR_MSG = '' # popup error msg
REPORT_DATA = {
	'main': [],
	'errors': [],
	'warns': [],
	'nosplit': [],
	'noexport': []
}
TOKENS_DONT_EXPORT = ['!', '#']
TOKENS_DONT_SPLIT  = ['=', '$']
MATIDX_ERROR = False

REG_KEY = 'ac3d_export'

# config options:
SKIP_DATA = False
MIRCOL_AS_AMB = False
MIRCOL_AS_EMIS = False
ADD_DEFAULT_MAT = True
SET_TEX_DIR = True
TEX_DIR = ''
AC3D_4 = True # export crease value, compatible with AC3D 4 loaders
NO_SPLIT = False
EXPORT_DIR = ''

tooltips = {
	'SKIP_DATA': "don't export mesh names as data fields",
	'MIRCOL_AS_AMB': "export mirror color as ambient color",
	'MIRCOL_AS_EMIS': "export mirror color as emissive color",
	'ADD_DEFAULT_MAT': "always add a default white material",
	'SET_TEX_DIR': "don't export default texture paths (edit also \"tex dir\")",
	'EXPORT_DIR': "default / last folder used to export .ac files to",
	'TEX_DIR': "(see \"set tex dir\") dir to prepend to all exported texture names (leave empty for no dir)",
	'AC3D_4': "compatibility mode, adds 'crease' tag and slightly better material support",
	'NO_SPLIT': "don't split meshes with multiple textures (or both textured and non textured polygons)",
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
	d['tooltips'] = tooltips
	Blender.Registry.SetKey(REG_KEY, d, True)

# Looking for a saved key in Blender.Registry dict:
rd = Blender.Registry.GetKey(REG_KEY, True)

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
		NO_SPLIT = rd['NO_SPLIT']
	except KeyError: update_RegistryInfo()

else:
	update_RegistryInfo()

VERBOSE = True
CONFIRM_OVERWRITE = True

# check General scripts config key for default behaviors
rd = Blender.Registry.GetKey('General', True)
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
acmatrix = Mathutils.Matrix([1,0,0,0], [0,0,-1,0], [0,1,0,0], [0,0,0,1])

def Round(f):
	r = round(f,6) # precision set to 10e-06
	if r == int(r):
		return str(int(r))
	else:
		return str(r)
 
def transform_verts(verts, m):
	vecs = []
	for v in verts:
		vec = Mathutils.Vector([v[0],v[1],v[2], 1])
		vecs.append(Mathutils.VecMultMat(vec, m))
	return vecs

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
		self.faces = []
		self.verts = verts = []
		vidxs = [0]*len(mesh.verts)
		faces2 = [0]*len(faces)
		for f in faces:
			self.faces.append(self.FooFace(self, f))
			for v in f.v:
				if v: vidxs[v.index] = 1
		i = 0
		for v in mesh.verts:
			if vidxs[v.index]:
				verts.append(v)
				vidxs[v.index] = i
				i += 1
		for f in self.faces:
			for v in f.v:
				if v: v.index = vidxs[v.v.index]

	def hasFaceUV(self):
		return self.mesh.hasFaceUV()

	def getMaxSmoothAngle(self):
		return self.mesh.getMaxSmoothAngle()


class AC3DExport: # the ac3d exporter part

	def __init__(self, scene_objects, filename):

		global ARG, SKIP_DATA, ADD_DEFAULT_MAT, DEFAULT_MAT
		global ERROR_MSG, MATIDX_ERROR

		MATIDX_ERROR = 0

		header = 'AC3Db'
		self.buf = ''
		self.mbuf = ''
		self.mlist = []
		world_kids = 0
		kids_dict = self.kids_dict = {}
		objs = []
		exp_objs = self.exp_objs = []
		tree = {}

		try:
			file = self.file = open(filename, 'w')
		except IOError, (errno, strerror):
			error = "IOError #%s: %s" % (errno, strerror)
			REPORT_DATA['errors'].append("Saving failed - %s." % error)
			ERROR_MSG = "Couldn't save file!%%t|%s" % error
			return None

		file.write(header+'\n')

		objs = \
			[o for o in scene_objects if o.getType() in ['Mesh', 'Empty']]

		for obj in objs[:]:
			parent = obj.getParent()
			list = [obj]

			while parent:
				obj = parent
				parent = parent.getParent()
				list.insert(0, obj)

			dict = tree
			for i in range(len(list)):
				lname = list[i].getType()[:2] + list[i].name
				if lname not in dict.keys():
					dict[lname] = {}
				dict = dict[lname]

		self.traverse_dict(tree)

		world_kids = len(tree.keys())

		objlist = [Blender.Object.Get(name) for name in exp_objs]

		meshlist = [o for o in objlist if o.getType() == 'Mesh']

		self.MATERIALS(meshlist)
		if not self.mbuf or ADD_DEFAULT_MAT:
			self.mbuf = DEFAULT_MAT + '\n' + self.mbuf
		file.write(self.mbuf)

		file.write('OBJECT world\nkids %s\n' % world_kids)

		for obj in objlist:
			self.obj = obj

			objtype = obj.getType()
			objname = obj.name
			kidsnum = kids_dict[objname]

			if kidsnum:
				self.OBJECT('group')
				parent_is_mesh = 0
				if objtype == 'Mesh':
					kidsnum += 1
					parent_is_mesh = 1
				self.name(objname)
				self.kids(kidsnum)

			if objtype == 'Mesh':
				mesh = self.mesh = obj.getData()
				meshes = self.split_mesh(mesh)
				if len(meshes) > 1:
					if NO_SPLIT or self.dont_split(objname):
						self.export_mesh(mesh, obj)
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

		file.close()
		REPORT_DATA['main'].append("Done. Saved to: %s" % filename)

	def traverse_dict(self, dict):
		kids_dict = self.kids_dict
		exp_objs = self.exp_objs
		keys = dict.keys()
		for k in keys:
			objname = k[2:]
			klen = len(dict[k])
			kids_dict[objname] = klen
			if self.dont_export(objname):
				dict.pop(k)
				parent = Blender.Object.Get(objname).getParent()
				if parent: kids_dict[parent.name] -= 1
				REPORT_DATA['noexport'].append(objname)
				continue
			if klen:
				self.traverse_dict(dict[k])
				exp_objs.insert(0, objname)
			else:
				if k.find('Em', 0) == 0: # Empty w/o children
					dict.pop(k)
					parent = Blender.Object.Get(objname).getParent()
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
			return foo_meshes
		return [mesh]

	def export_mesh(self, mesh, obj, name = None, foomesh = False):
		file = self.file
		self.OBJECT('poly')
		if not name: name = obj.name
		self.name(name)
		if not SKIP_DATA:
			self.data(len(mesh.name), mesh.name)
		texline = self.texture(mesh.faces)
		if texline: file.write(texline)
		if AC3D_4:
			self.crease(mesh.getMaxSmoothAngle())
		self.numvert(mesh.verts, obj.getMatrix())
		self.numsurf(mesh.faces, mesh.hasFaceUV(), foomesh)

	def MATERIALS(self, meshlist):
		for meobj in meshlist:
			me = meobj.getData()
			mat = me.materials
			mbuf = []
			mlist = self.mlist
			for m in xrange(len(mat)):
				name = mat[m].name
				try:
					mlist.index(name)
				except ValueError:
					mlist.append(name)
					M = Blender.Material.Get(name)
					material = 'MATERIAL "%s"' % name
					mirCol = "%s %s %s" % (Round(M.mirCol[0]), Round(M.mirCol[1]),
						Round(M.mirCol[2]))
					rgb = "rgb %s %s %s" % (Round(M.R), Round(M.G), Round(M.B))
					amb = "amb %s %s %s" % (Round(M.amb), Round(M.amb), Round(M.amb))
					spec = "spec %s %s %s" % (Round(M.specCol[0]),
						 Round(M.specCol[1]), Round(M.specCol[2]))
					if AC3D_4:
						emit = Round(M.emit)
						emis = "emis %s %s %s" % (emit, emit, emit)
						shival = int(M.spec * 64)
					else:
						emis = "emis 0 0 0"
						shival = 72
					shi = "shi %s" % shival
					trans = "trans %s" % (Round(1 - M.alpha))
					if MIRCOL_AS_AMB:
						amb = "amb %s" % mirCol 
					if MIRCOL_AS_EMIS:
						emis = "emis %s" % mirCol
					mbuf.append("%s %s %s %s %s %s %s\n" \
						% (material, rgb, amb, emis, spec, shi, trans))
			self.mlist = mlist
			self.mbuf += "".join(mbuf)

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
			image = Blender.Image.Get(tex)
			texfname = image.filename
			if SET_TEX_DIR:
				texfname = Blender.sys.basename(texfname)
				if TEX_DIR:
					texfname = Blender.sys.join(TEX_DIR, texfname)
			buf = 'texture "%s"\n' % texfname
			xrep = image.xrep
			yrep = image.yrep
			buf += 'texrep %s %s\n' % (xrep, yrep)
			self.file.write(buf)

	def rot(self, matrix):
		rot = ''
		not_I = 0
		for i in [0, 1, 2]:
			r = map(Round, matrix[i])
			not_I += (r[0] != '0.0')+(r[1] != '0.0')+(r[2] != '0.0')
			not_I -= (r[i] == '1.0')
			for j in [0, 1, 2]:
				rot = "%s %s" % (rot, r[j])
		if not_I:
			self.file.write('rot %s\n' % rot.strip())
				
	def loc(self, loc):
		loc = map(Round, loc)
		if loc[0] or loc[1] or loc[2]:
			self.file.write('loc %s %s %s\n' % (loc[0], loc[1], loc[2]))

	def crease(self, crease):
		self.file.write('crease %s\n' % crease)

	def numvert(self, verts, matrix):
		file = self.file
		file.write("numvert %s\n" % len(verts))
		m = matrix * acmatrix
		verts = transform_verts(verts, m)
		for v in verts:
			v0, v1, v2 = Round(v[0]), Round(v[1]), Round(v[2])
			file.write("%s %s %s\n" % (v0, v1, v2))

	def numsurf(self, faces, hasFaceUV, foomesh = False):

		global ADD_DEFAULT_MAT, MATIDX_ERROR
		file = self.file
 
		file.write("numsurf %s\n" % len(faces))

		if not foomesh: verts = self.mesh.verts

		mlist = self.mlist
		omlist = {}
		objmats = self.mesh.materials[:]
		matidx_error_told = 0
		for i in range(len(objmats)):
			objmats[i] = objmats[i].name
		for f in faces:
			m_idx = f.materialIndex
			try:
				m_idx = mlist.index(objmats[m_idx])
			except IndexError:
				if not MATIDX_ERROR:
					rdat = REPORT_DATA['warns']
					rdat.append("Object %s" % self.obj.name)
					rdat.append("has at least one material *index* assigned but not")
					rdat.append("defined (not linked to an existing material).")
					rdat.append("Result: some faces may be exported with a wrong color.")
					rdat.append("You can link materials in the Edit Buttons window (F9).")
				elif not matidx_error_told:
					midxmsg = "- Same for object %s." % self.obj.name
					REPORT_DATA['warns'].append(midxmsg)
				MATIDX_ERROR += 1
				matidx_error_told = 1
				m_idx = 0
			refs = len(f)
			flaglow = (refs == 2) << 1
			two_side = f.mode & Blender.NMesh.FaceModes['TWOSIDE']
			two_side = (two_side > 0) << 1
			flaghigh = f.smooth | two_side
			surfstr = "SURF 0x%d%d\n" % (flaghigh, flaglow)
			if ADD_DEFAULT_MAT and objmats: m_idx += 1
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
	global ERROR_MSG, EXPORT_DIR, OBJS, CONFIRM_OVERWRITE, VERBOSE

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

	test = AC3DExport(OBJS, filename)
	if ERROR_MSG:
		Blender.Draw.PupMenu(ERROR_MSG)
		ERROR_MSG = ''
	else:
		endtime = bsys.time() - starttime
		REPORT_DATA['main'].append("Data exported in %.3f seconds." % endtime)

	if VERBOSE: report_data()
	Blender.Window.WaitCursor(0)
	return


# -- End of definitions

OBJS = Blender.Object.GetSelected()

if not OBJS:
	Blender.Draw.PupMenu('ERROR: No objects selected')
else:
	fname = bsys.makename(ext=".ac")
	if EXPORT_DIR:
		fname = bsys.join(EXPORT_DIR, bsys.basename(fname))
	FileSelector(fs_callback, "Export AC3D", fname)
