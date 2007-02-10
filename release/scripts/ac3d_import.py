#!BPY

""" Registration info for Blender menus:
Name: 'AC3D (.ac)...'
Blender: 242
Group: 'Import'
Tip: 'Import an AC3D (.ac) file.'
"""

__author__ = "Willian P. Germano"
__url__ = ("blender", "elysiun", "AC3D's homepage, http://www.ac3d.org",
	"PLib 3d gaming lib, http://plib.sf.net")
__version__ = "2.43 2007-01-28"

__bpydoc__ = """\
This script imports AC3D models into Blender.

AC3D is a simple and affordable commercial 3d modeller also built with OpenGL.
The .ac file format is an easy to parse text format well supported,
for example, by the PLib 3d gaming library.

Supported:<br>
    UV-textured meshes with hierarchy (grouping) information.

Missing:<br>
    The url tag is irrelevant for Blender.

Known issues:<br>
    - Some objects may be imported with wrong normals due to wrong information in the model itself. This can be noticed by strange shading, like darker than expected parts in the model. To fix this, select the mesh with wrong normals, enter edit mode and tell Blender to recalculate the normals, either to make them point outside (the usual case) or inside.<br>
 
Config Options:<br>
    - textures dir (string): if non blank, when imported texture paths are
wrong in the .ac file, Blender will also look for them at this dir.

Notes:<br>
   - When looking for assigned textures, Blender tries in order: the actual
paths from the .ac file, the .ac file's dir and the default textures dir path
users can configure (see config options above).
"""

# $Id$
#
# --------------------------------------------------------------------------
# AC3DImport version 2.43 Jan 04, 2007
# Program versions: Blender 2.43 and AC3Db files (means version 0xb)
# changed: updated for new Blender version, Mesh module
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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

# Note:
# Blender doesn't handle n-gons (polygons with more than 4 vertices):
#  The script triangulates them, but concave polygons come out wrong and need
#  fixing. Avoiding or triangulating concave n-gons in AC3D is a simple way to
#  avoid problems.

from math import radians

import Blender
from Blender import Scene, Object, Mesh, Lamp, Registry, sys as bsys, Window, Image, Material
from Blender.sys import dirsep
from Blender.Mathutils import Vector, Matrix, Euler

# Default folder for AC3D textures, to override wrong paths, change to your
# liking or leave as "":
TEXTURES_DIR = ""

tooltips = {
	'TEXTURES_DIR': 'additional dir to look for missing textures'
}

def update_registry():
	global TEXTURES_DIR
	rd = dict([('TEXTURES_DIR', TEXTURES_DIR)])
	Registry.SetKey('ac3d_import', rd, True)

rd = Registry.GetKey('ac3d_import', True)

if rd:
	if 'GROUP' in rd:
		update_registry()
	TEXTURES_DIR = rd['TEXTURES_DIR']
else: update_registry()

if TEXTURES_DIR:
	oldtexdir = TEXTURES_DIR
	if dirsep == '/': TEXTURES_DIR = TEXTURES_DIR.replace('\\', '/')
	if TEXTURES_DIR[-1] != dirsep: TEXTURES_DIR = "%s%s" % (TEXTURES_DIR, dirsep)
	if oldtexdir != TEXTURES_DIR: update_registry()


VERBOSE = True
rd = Registry.GetKey('General', True)
if rd:
	if rd.has_key('verbose'):
		VERBOSE = rd['verbose']

	
errmsg = ""

# Matrix to align ac3d's coordinate system with Blender's one,
# it's a -90 degrees rotation around the x axis:
AC_TO_BLEND_MATRIX = Matrix([1, 0, 0], [0, 0, 1], [0, -1, 0])

AC_WORLD = 0
AC_GROUP = 1
AC_POLY = 2
AC_LIGHT = 3
AC_OB_TYPES = {
	'world': AC_WORLD,
	'group': AC_GROUP,
	'poly':  AC_POLY,
	'light':  AC_LIGHT
	}

def inform(msg):
	global VERBOSE
	if VERBOSE: print msg

def euler_in_radians(eul):
	"Used while there's a bug in the BPY API"
	eul.x = radians(eul.x)
	eul.y = radians(eul.y)
	eul.z = radians(eul.z)
	return eul

class Obj:
	
	def __init__(self, type):
		self.type = type
		self.dad = None
		self.name = ''
		self.data = ''
		self.tex = ''
		self.texrep = [1,1]
		self.texoff = None
		self.loc = []
		self.rot = []
		self.size = []
		self.crease = 30
		self.vlist = []
		self.flist_cfg = []
		self.flist_v = []
		self.flist_uv = []
		self.elist = []
		self.matlist = []
		self.kids = 0

		self.bl_obj = None # the actual Blender object created from this data

class AC3DImport:

	def __init__(self, filename):

		global errmsg

		self.scene = Scene.GetCurrent()

		self.i = 0
		errmsg = ''
		self.importdir = bsys.dirname(filename)
		try:
			file = open(filename, 'r')
		except IOError, (errno, strerror):
			errmsg = "IOError #%s: %s" % (errno, strerror)
			Blender.Draw.PupMenu('ERROR: %s' % errmsg)
			inform(errmsg)
			return None
		header = file.read(5)
		header, version = header[:4], header[-1]
		if header != 'AC3D':
			file.close()
			errmsg = 'AC3D header not found (invalid file)'
			Blender.Draw.PupMenu('ERROR: %s' % errmsg)
			inform(errmsg)
			return None
		elif version != 'b':
			inform('AC3D file version 0x%s.' % version)
			inform('This importer is for version 0xb, so it may fail.')

		self.token = {'OBJECT':		self.parse_obj,
					  'numvert':	self.parse_vert,
					  'numsurf':	self.parse_surf,
					  'name':		self.parse_name,
					  'data':		self.parse_data,
					  'kids':		self.parse_kids,
					  'loc':		self.parse_loc,
					  'rot':		self.parse_rot,
					  'MATERIAL':	self.parse_mat,
					  'texture':	self.parse_tex,
					  'texrep':		self.parse_texrep,
					  'texoff':		self.parse_texoff,
					  'crease':		self.parse_crease}

		self.objlist = []
		self.mlist = []
		self.kidsnumlist = []
		self.dad = None

		self.lines = file.readlines()
		self.lines.append('')
		self.parse_file()
		file.close()
		
		self.testAC3DImport()
				
	def parse_obj(self, value):
		if self.kidsnumlist:
			while not self.kidsnumlist[-1]:
				self.kidsnumlist.pop()
				self.dad = self.dad.dad
			self.kidsnumlist[-1] -= 1
		new = Obj(AC_OB_TYPES[value])
		new.dad = self.dad
		new.name = value
		self.objlist.append(new)

	def parse_kids(self, value):
		kids = int(value)
		if kids:
			self.kidsnumlist.append(kids)
			self.dad = self.objlist[-1]
		self.objlist[-1].kids = kids

	def parse_name(self, value):
		name = value.split('"')[1]
		self.objlist[-1].name = name

	def parse_data(self, value):
		data = self.lines[self.i].strip()
		self.objlist[-1].data = data

	def parse_tex(self, value):
		texture = value.split('"')[1]
		self.objlist[-1].tex = texture

	def parse_texrep(self, trash):
		trep = self.lines[self.i - 1]
		trep = trep.split()
		trep = [float(trep[1]), float(trep[2])]
		self.objlist[-1].texrep = trep
		self.objlist[-1].texoff = [0, 0]

	def parse_texoff(self, trash):
		toff = self.lines[self.i - 1]
		toff = toff.split()
		toff = [float(toff[1]), float(toff[2])]
		self.objlist[-1].texoff = toff
		
	def parse_mat(self, value):
		i = self.i - 1
		lines = self.lines
		line = lines[i].split()
		mat_name = ''
		mat_col = mat_amb = mat_emit = mat_spec_col = [0,0,0]
		mat_alpha = 1
		mat_spec = 1.0

		while line[0] == 'MATERIAL':
			mat_name = line[1].split('"')[1]
			mat_col = map(float,[line[3],line[4],line[5]])
			v = map(float,[line[7],line[8],line[9]])
			mat_amb = (v[0]+v[1]+v[2]) / 3.0
			v = map(float,[line[11],line[12],line[13]])
			mat_emit = (v[0]+v[1]+v[2]) / 3.0
			mat_spec_col = map(float,[line[15],line[16],line[17]])
			mat_spec = float(line[19]) / 64.0
			mat_alpha = float(line[-1])
			mat_alpha = 1 - mat_alpha
			self.mlist.append([mat_name, mat_col, mat_amb, mat_emit, mat_spec_col, mat_spec, mat_alpha])
			i += 1
			line = lines[i].split()

		self.i = i

	def parse_rot(self, trash):
		i = self.i - 1
		ob = self.objlist[-1]
		rot = self.lines[i].split(' ', 1)[1]
		rot = map(float, rot.split())
		matrix = Matrix(rot[:3], rot[3:6], rot[6:])
		ob.rot = matrix
		size = matrix.scalePart() # vector
		ob.size = size

	def parse_loc(self, trash):
		i = self.i - 1
		loc = self.lines[i].split(' ', 1)[1]
		loc = map(float, loc.split())
		self.objlist[-1].loc = Vector(loc)

	def parse_crease(self, value):
		# AC3D: range is [0.0, 180.0]; Blender: [1, 80]
		value = float(value)
		self.objlist[-1].crease = int(value)

	def parse_vert(self, value):
		i = self.i
		lines = self.lines
		obj = self.objlist[-1]
		vlist = obj.vlist
		n = int(value)

		while n:
			line = lines[i].split()
			line = map(float, line)
			vlist.append(line)
			n -= 1
			i += 1

		self.i = i

	def parse_surf(self, value):
		i = self.i
		is_smooth = 0
		double_sided = 0
		lines = self.lines
		obj = self.objlist[-1]
		matlist = obj.matlist
		numsurf = int(value)
		NUMSURF = numsurf

		badface_notpoly = badface_multirefs = 0

		while numsurf:
			flags = lines[i].split()[1][2:]
			if len(flags) > 1:
				flaghigh = int(flags[0])
				flaglow = int(flags[1])
			else:
				flaghigh = 0
				flaglow = int(flags[0])

			is_smooth = flaghigh & 1
			twoside = flaghigh & 2
			mat = lines[i+1].split()
			mat = int(mat[1])
			if not mat in matlist: matlist.append(mat)
			refs = lines[i+2].split()
			refs = int(refs[1])
			i += 3
			face = []
			faces = []
			edges = []
			fuv = []
			fuvs = []
			rfs = refs

			while rfs:
				line = lines[i].split()
				v = int(line[0])
				uv = [float(line[1]), float(line[2])]
				face.append(v)
				fuv.append(Vector(uv))
				rfs -= 1
				i += 1

			if flaglow: # it's a line or closed line, not a polygon
				while len(face) >= 2:
					cut = face[:2]
					edges.append(cut)
					face = face[1:]

				if flaglow == 1 and edges: # closed line
					face = [edges[-1][-1], edges[0][0]]
					edges.append(face)

			else: # polygon

				# check for bad face, that references same vertex more than once
				lenface = len(face)
				if lenface < 3:
					# less than 3 vertices, not a face
					badface_notpoly += 1
				elif sum(map(face.count, face)) != lenface:
					# multiple references to the same vertex
					badface_multirefs += 1
				else: # ok, seems fine
					while len(face) > 4:
						cut = face[:4]
						cutuv = fuv[:4]
						face = face[3:]
						fuv = fuv[3:]
						face.insert(0, cut[0])
						fuv.insert(0, cutuv[0])
						faces.append(cut)
						fuvs.append(cutuv)

					faces.append(face)
					fuvs.append(fuv)

			obj.flist_cfg.extend([[mat, is_smooth, twoside]] * len(faces))
			obj.flist_v.extend(faces)
			obj.flist_uv.extend(fuvs)
			obj.elist.extend(edges) # loose edges

			numsurf -= 1	  

		if badface_notpoly or badface_multirefs:
			inform('Object "%s" - ignoring bad faces:' % obj.name)
			if badface_notpoly:
				inform('\t%d face(s) with less than 3 vertices.' % badface_notpoly)
			if badface_multirefs:
				inform('\t%d face(s) with multiple references to a same vertex.' % badface_multirefs)

		self.i = i

	def parse_file(self):
		i = 1
		lines = self.lines
		line = lines[i].split()

		while line:
			kw = ''
			for k in self.token.keys():
				if line[0] == k:
					kw = k
					break
			i += 1
			if kw:
				self.i = i
				self.token[kw](line[1])
				i = self.i
			line = lines[i].split()

	# for each group of meshes we try to find one that can be used as
	# parent of the group in Blender.
	# If not found, we can use an Empty as parent.
	def found_parent(self, groupname, olist):
		l = [o for o in olist if o.type == AC_POLY \
				and not o.kids and not o.rot and not o.loc]
		if l:
			for o in l:
				if o.name == groupname:
					return o
				#return l[0]
		return None

	def build_hierarchy(self):
		blmatrix = AC_TO_BLEND_MATRIX

		olist = self.objlist[1:]
		olist.reverse()

		scene = self.scene

		newlist = []

		for o in olist:
			kids = o.kids
			if kids:
				children = newlist[-kids:]
				newlist = newlist[:-kids]
				if o.type == AC_GROUP:
					parent = self.found_parent(o.name, children)
					if parent:
						children.remove(parent)
						o.bl_obj = parent.bl_obj
					else: # not found, use an empty
						empty = scene.objects.new('Empty', o.name)
						o.bl_obj = empty

				bl_children = [c.bl_obj for c in children if c.bl_obj != None]
				
				o.bl_obj.makeParent(bl_children, 0, 1)
				for child in children:
					blob = child.bl_obj
					if not blob: continue
					if child.rot:
						eul = euler_in_radians(child.rot.toEuler())
						blob.setEuler(eul)
					if child.size:
						blob.size = child.size
					if not child.loc:
						child.loc = Vector(0.0, 0.0, 0.0)
					blob.setLocation(child.loc)

			newlist.append(o)

		for o in newlist: # newlist now only has objs w/o parents
			blob = o.bl_obj
			if not blob:
				continue
			if o.size:
				o.bl_obj.size = o.size
			if not o.rot:
				blob.setEuler([1.5707963267948966, 0, 0])
			else:
				matrix = o.rot * blmatrix
				eul = euler_in_radians(matrix.toEuler())
				blob.setEuler(eul)
			if o.loc:
				o.loc *= blmatrix
			else:
				o.loc = Vector(0.0, 0.0, 0.0)
			blob.setLocation(o.loc) # forces DAG update, so we do it even for 0, 0, 0

		# XXX important: until we fix the BPy API so it doesn't increase user count
		# when wrapping a Blender object, this piece of code is needed for proper
		# object (+ obdata) deletion in Blender:
		for o in self.objlist:
			if o.bl_obj:
				o.bl_obj = None

	def testAC3DImport(self):

		FACE_TWOSIDE = Mesh.FaceModes['TWOSIDE']
		FACE_TEX = Mesh.FaceModes['TEX']
		MESH_AUTOSMOOTH = Mesh.Modes['AUTOSMOOTH']

		MAT_MODE_ZTRANSP = Material.Modes['ZTRANSP']
		MAT_MODE_TRANSPSHADOW = Material.Modes['TRANSPSHADOW']

		scene = self.scene

		bl_images = {} # loaded texture images

		objlist = self.objlist[1:] # skip 'world'

		bmat = []
		has_transp_mats = False
		for mat in self.mlist:
			name = mat[0]
			m = Material.New(name)
			m.rgbCol = (mat[1][0], mat[1][1], mat[1][2])
			m.amb = mat[2]
			m.emit = mat[3]
			m.specCol = (mat[4][0], mat[4][1], mat[4][2])
			m.spec = mat[5]
			m.alpha = mat[6]
			if m.alpha < 1.0:
				m.mode |= MAT_MODE_ZTRANSP
				has_transp_mats = True
			bmat.append(m)

		if has_transp_mats:
			for mat in bmat:
				mat.mode |= MAT_MODE_TRANSPSHADOW

		obj_idx = 0 # index of current obj in loop
		for obj in objlist:
			if obj.type == AC_GROUP:
				continue
			elif obj.type == AC_LIGHT:
				light = Lamp.New('Lamp')
				object = scene.objects.new(light, obj.name)
				#object.select(True)
				obj.bl_obj = object
				if obj.data:
					light.name = obj.data
				continue

			# type AC_POLY:

			# old .ac files used empty meshes as groups, convert to a real ac group
			if not obj.vlist and obj.kids:
				obj.type = AC_GROUP
				continue

			mesh = Mesh.New()
			object = scene.objects.new(mesh, obj.name)
			#object.select(True)
			obj.bl_obj = object
			if obj.data: mesh.name = obj.data
			mesh.degr = obj.crease # will auto clamp to [1, 80]

			if not obj.vlist: # no vertices? nothing more to do
				continue

			mesh.verts.extend(obj.vlist)

			objmat_indices = []
			for mat in bmat:
				if bmat.index(mat) in obj.matlist:
					objmat_indices.append(bmat.index(mat))
					mesh.materials += [mat]

			for e in obj.elist:
				mesh.edges.extend(e)

			if obj.flist_v:
				mesh.faces.extend(obj.flist_v)

				facesnum = len(mesh.faces)

				if facesnum == 0: # shouldn't happen, of course
					continue

				mesh.faceUV = True

				# checking if the .ac file had duplicate faces (Blender ignores them)
				if facesnum != len(obj.flist_v):
					# it has, ugh. Let's clean the uv list:
					lenfl = len(obj.flist_v)
					flist = obj.flist_v
					uvlist = obj.flist_uv
					cfglist = obj.flist_cfg
					for f in flist:
						f.sort()
					fi = lenfl
					while fi > 0: # remove data related to duplicates
						fi -= 1
						if flist[fi] in flist[:fi]:
							uvlist.pop(fi)
							cfglist.pop(fi)

				img = None
				if obj.tex != '':
					baseimgname = bsys.basename(obj.tex)
					if obj.tex in bl_images.keys():
						img = bl_images[obj.tex]
					else:
						try:
							img = Image.Load(obj.tex)
							# Commented because it's unnecessary:
							#img.xrep = int(obj.texrep[0])
							#img.yrep = int(obj.texrep[1])
						except:
							try:
								obj.tex = self.importdir + '/' + baseimgname
								img = Image.Load(obj.tex)
							except:
								try:
									obj.tex = TEXTURES_DIR + baseimgname
									img = Image.Load(obj.tex)
								except:
									inform("Couldn't load texture: %s" % baseimgname)
						if img:
							bl_images[obj.tex] = img

				for i in range(facesnum):
					f = obj.flist_cfg[i]
					fmat = f[0]
					is_smooth = f[1]
					twoside = f[2]
					bface = mesh.faces[i]
					bface.smooth = is_smooth
					if twoside: bface.mode |= FACE_TWOSIDE
					if img:
						bface.mode |= FACE_TEX
						bface.image = img
					bface.mat = objmat_indices.index(fmat)
					fuv = obj.flist_uv[i]
					if obj.texoff:
						uoff = obj.texoff[0]
						voff = obj.texoff[1]
						urep = obj.texrep[0]
						vrep = obj.texrep[1]
						for uv in fuv:
							uv[0] *= urep
							uv[1] *= vrep
							uv[0] += uoff
							uv[1] += voff

					mesh.faces[i].uv = fuv

				mesh.calcNormals()

				mesh.mode = MESH_AUTOSMOOTH

			obj_idx += 1

		self.build_hierarchy()
		scene.update()

# End of class AC3DImport

def filesel_callback(filename):

	inform("\nTrying to import AC3D model(s) from:\n%s ..." % filename)
	Window.WaitCursor(1)
	starttime = bsys.time()
	test = AC3DImport(filename)
	Window.WaitCursor(0)
	endtime = bsys.time() - starttime
	inform('Done! Data imported in %.3f seconds.\n' % endtime)

Window.EditMode(0)

Window.FileSelector(filesel_callback, "Import AC3D", "*.ac")
