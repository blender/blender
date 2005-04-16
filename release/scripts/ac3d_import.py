#!BPY

""" Registration info for Blender menus:
Name: 'AC3D (.ac)...'
Blender: 236
Group: 'Import'
Tip: 'Import an AC3D (.ac) file.'
"""

__author__ = "Willian P. Germano"
__url__ = ("blender", "elysiun", "AC3D's homepage, http://www.ac3d.org",
	"PLib 3d gaming lib, http://plib.sf.net")
__version__ = "2.36 2005-04-14"

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
    None.

Config Options:<br>
    - group (toggle): if "on", grouped objects in the .ac file are parented to
Empties.
    - textures dir (string): if non blank, when imported texture paths are
wrong in the .ac file, Blender will also look for them at this dir.

Notes:<br>
    - when looking for assigned textures, Blender tries in order: the actual
paths from the .ac file, the .ac file's dir and the default textures dir path
users can configure (see config options above).
"""

# $Id$
#
# --------------------------------------------------------------------------
# AC3DImport version 2.36 Apr 14, 2005
# Program versions: Blender 2.36+ and AC3Db files (means version 0xb)
# changed: updated to use the Scripts Config Editor facilities
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2005: Willian P. Germano, wgermano _at_ ig.com.br
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

import Blender
from Blender import Registry
from Blender.sys import dirsep

# Default folder for AC3D textures, to override wrong paths, change to your
# liking or leave as "":
TEXTURES_DIR = ""

# Set 'GROUP' to True to make Blender group imported objects using Empties,
# to reproduce the object hierarchy in the .ac file
GROUP = False

tooltips = {
	'TEXTURES_DIR': 'additional dir to look for missing textures',
	'GROUP': 'mimick grouping information by parenting grouped meshes to empties'
}

def update_registry():
	global GROUP, TEXTURES_DIR
	rd = dict([('GROUP', GROUP), ('TEXTURES_DIR', TEXTURES_DIR)])
	Registry.SetKey('ac3d_import', rd, True)

rd = Registry.GetKey('ac3d_import', True)

if rd:
	TEXTURES_DIR = rd['TEXTURES_DIR']
	GROUP = rd['GROUP']
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


def inform(msg):
	global VERBOSE
	if VERBOSE: print msg


class Obj:
	
	def __init__(self, type):
		self.type = type
		self.dad = None
		self.name = ''
		self.data = ''
		self.tex = ''
		self.texrep = [1,1]
		self.texoff = None
		self.loc = [0, 0, 0]
		self.rot = []
		self.crease = 30
		self.vlist = []
		self.flist = []
		self.matlist = []
		self.kids = 0

class AC3DImport:

	def __init__(self, filename):

		global errmsg

		self.i = 0
		errmsg = ''
		self.importdir = Blender.sys.dirname(filename)
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
		self.dads = []
		self.kids = []
		self.dad = None

		self.lines = file.readlines()
		self.lines.append('')
		self.parse_file()
		file.close()
		
		self.testAC3DImport()
				
	def parse_obj(self, value):
		if self.kids:
			while not self.kids[-1]:
				self.kids.pop()
				self.dad = self.dad.dad
			self.kids[-1] -= 1
		new = Obj(value)
		new.dad = self.dad
		new.name = value
		self.objlist.append(new)

	def parse_kids(self, value):
		kids = int(value)
		if kids:
			self.kids.append(kids)
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
		rot = self.lines[i].split(' ', 1)[1]
		rot = map(float, rot.split())
		self.objlist[-1].rot = rot

	def parse_loc(self, trash):
		i = self.i - 1
		loc = self.lines[i].split(' ', 1)[1]
		loc = map(float, loc.split())
		self.objlist[-1].loc = loc

	def parse_crease(self, value):
		# AC3D: range is [0.0, 180.0]; Blender: [1, 80]
		try:
			value = int(value)
		except ValueError:
			value = int(float(value)) # duh
		self.objlist[-1].crease = value

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

		rot = obj.rot
		if rot:
			nv = len(vlist)
			for j in range(nv):
				v = vlist[j]
				t = [0,0,0]
				t[0] = rot[0]*v[0] + rot[3]*v[1] + rot[6]*v[2]
				t[1] = rot[1]*v[0] + rot[4]*v[1] + rot[7]*v[2]
				t[2] = rot[2]*v[0] + rot[5]*v[1] + rot[8]*v[2]
				vlist[j] = t

		loc = obj.loc
		dad = obj.dad
		while dad:
			for j in [0, 1, 2]:
				loc[j] += dad.loc[j]
			dad = dad.dad

		for v in vlist:
			for j in [0, 1, 2]:
				v[j] += loc[j]

	def parse_surf(self, value):
		i = self.i
		is_smooth = 0
		double_sided = 0
		lines = self.lines
		obj = self.objlist[-1]
		matlist = obj.matlist
		numsurf = int(value)

		while numsurf:
			flags = lines[i].split()
			flaglow = 0
			if len(flags[1]) > 3: flaglow = int(flags[1][3])
			flaghigh = int(flags[1][2])
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
			fuv = []
			rfs = refs

			while rfs:
				line = lines[i].split()
				v = int(line[0])
				uv = [float(line[1]), float(line[2])]
				face.append([v, uv])
				rfs -= 1
				i += 1
				
			if flaglow:
				while len(face) >= 2:
					cut = face[:2]
					faces.append(cut)
					face = face[1:]

				if flaglow == 1:
					face = [faces[-1][-1], faces[0][0]]
					faces.append(face)

			else:
				while len(face) > 4:
					cut = face[:4]
					face = face[3:]
					face.insert(0, cut[0])
					faces.append(cut)	 

				faces.append(face)

			for f in faces:
				f.append(mat)
				f.append(is_smooth)
				f.append(twoside)
				self.objlist[-1].flist.append(f)

			numsurf -= 1	  

							
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

	def testAC3DImport(self):
		global GROUP
		scene = Blender.Scene.GetCurrent()

		bmat = []
		for mat in self.mlist:
			name = mat[0]
			m = Blender.Material.New(name)
			m.rgbCol = (mat[1][0], mat[1][1], mat[1][2])
			m.amb = mat[2]
			m.emit = mat[3]
			m.specCol = (mat[4][0], mat[4][1], mat[4][2])
			m.spec = mat[5]
			m.alpha = mat[6]
			bmat.append(m)

		for obj in self.objlist:
			if obj.type == 'world':
				continue
			elif obj.type == 'group':
				if not GROUP: continue
				empty = Blender.Object.New('Empty')
				empty.name = obj.name
				scene.link(empty)
				if self.dads:
					dadobj = Blender.Object.get(self.dads.pop())
					dadobj.makeParent([empty])
				while obj.kids:
					self.dads.append(empty.name)
					obj.kids -= 1
				continue
			mesh = Blender.NMesh.New()
			if obj.data: mesh.name = obj.data
			mesh.setMaxSmoothAngle(obj.crease) # will clamp to [1, 80]
			mesh.hasFaceUV(1)

			tex = None
			if obj.tex != '':
				try:
					tex = Blender.Image.Load(obj.tex)
					# Commented because it's unnecessary:
					#tex.xrep = int(obj.texrep[0])
					#tex.yrep = int(obj.texrep[1])
				except:
					basetexname = Blender.sys.basename(obj.tex)
					try:
						obj.tex = self.importdir + '/' + basetexname
						tex = Blender.Image.Load(obj.tex)
					except:
						try:
							obj.tex = TEXTURES_DIR + basetexname
							tex = Blender.Image.Load(obj.tex)
						except:
							inform("Couldn't load texture: %s" % basetexname)

			for v in obj.vlist:
				bvert = Blender.NMesh.Vert(v[0],v[1],v[2])
				mesh.verts.append(bvert)

			objmat_indices = []
			for mat in bmat:
				if bmat.index(mat) in obj.matlist:
					objmat_indices.append(bmat.index(mat))
					mesh.materials.append(mat)
			for f in obj.flist:
				twoside = f[-1]
				is_smooth = f[-2]
				fmat = f[-3]
				f=f[:-3]
				bface = Blender.NMesh.Face()
				bface.smooth = is_smooth
				if twoside: bface.mode |= Blender.NMesh.FaceModes['TWOSIDE']
				if tex:
					bface.mode |= Blender.NMesh.FaceModes['TEX']
					bface.image = tex
				bface.materialIndex = objmat_indices.index(fmat)
				if obj.texoff:
					uoff = obj.texoff[0]
					voff = obj.texoff[1]
					urep = obj.texrep[0]
					vrep = obj.texrep[1]
					for vi in range(len(f)):
						f[vi][1][0] *= urep
						f[vi][1][1] *= vrep
						f[vi][1][0] += uoff
						f[vi][1][1] += voff

				for vi in range(len(f)):
					bface.v.append(mesh.verts[f[vi][0]])
					bface.uv.append((f[vi][1][0], f[vi][1][1]))
				mesh.faces.append(bface)

			mesh.mode = 0
			object = Blender.NMesh.PutRaw(mesh)
			object.setName(obj.name)
			object.setEuler([1.5707963,0,0]) # align ac3d w/ Blender
			if self.dads:
				dadobj = Blender.Object.get(self.dads.pop())
				dadobj.makeParent([object])

# End of class AC3DImport

def filesel_callback(filename):

	inform("Trying to import AC3D model(s) from %s ..." % filename)
	Blender.Window.WaitCursor(1)
	starttime = Blender.sys.time()
	test = AC3DImport(filename)
	Blender.Window.WaitCursor(0)
	endtime = Blender.sys.time() - starttime
	inform('... done!  Data imported in %.3f seconds.\n' % endtime)

Blender.Window.FileSelector(filesel_callback, "Import AC3D", "*.ac")
