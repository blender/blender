#!BPY

""" Registration info for Blender menus:
Name: 'AC3D (.ac)...'
Blender: 232
Group: 'Export'
Submenu: 'All meshes...' all
Submenu: 'Only selected...' sel
Submenu: 'Configure +' config
Tip: 'Export to AC3D (.ac) format.'
"""

# $Id$
#
# --------------------------------------------------------------------------
# AC3DExport version 2.32-1 Jan 21, 2004
# Program versions: Blender 2.32+ and AC3Db files (means version 0xb)
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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

import Blender

ARG = __script__['arg'] # user selected argument

HELPME = 0 # help window

SKIP_DATA = 1
MIRCOL_AS_AMB = 0
MIRCOL_AS_EMIS = 0
ADD_DEFAULT_MAT = 1

# Looking for a saved key in Blender.Registry dict:
rd = Blender.Registry.GetKey('AC3DExport')
if rd:
  SKIP_DATA = rd['SKIP_DATA']
  MIRCOL_AS_AMB = rd['MIRCOL_AS_AMB']
  MIRCOL_AS_EMIS = rd['MIRCOL_AS_EMIS']
  ADD_DEFAULT_MAT = rd['ADD_DEFAULT_MAT']

def update_RegistryInfo():
  d = {}
  d['SKIP_DATA'] = SKIP_DATA
  d['MIRCOL_AS_AMB'] = MIRCOL_AS_AMB
  d['MIRCOL_AS_EMIS'] = MIRCOL_AS_EMIS
  d['ADD_DEFAULT_MAT'] = ADD_DEFAULT_MAT
  Blender.Registry.SetKey('AC3DExport', d)

# The default material to be used when necessary (see ADD_DEFAULT_MAT)
DEFAULT_MAT = \
'MATERIAL "DefaultWhite" rgb 1 1 1  amb 1 1 1  emis 0 0 0  spec 0.5 0.5 0.5 shi 64  trans 0'

# This transformation aligns Blender and AC3D coordinate systems:
acmatrix = [[1,0,0,0],[0,0,-1,0],[0,1,0,0],[0,0,0,1]]

def Round(f):
    r = round(f,6) # precision set to 10e-06
    if r == int(r):
        return str(int(r))
    else:
        return str(r)
    
def transform_verts(verts, m):
    r = []
    for v in verts:
        t = [0,0,0]
        t[0] = m[0][0]*v[0] + m[1][0]*v[1] + m[2][0]*v[2] + m[3][0]
        t[1] = m[0][1]*v[0] + m[1][1]*v[1] + m[2][1]*v[2] + m[3][1]
        t[2] = m[0][2]*v[0] + m[1][2]*v[1] + m[2][2]*v[2] + m[3][2]
        r.append(t)
    return r

def matrix_mul(m, n = acmatrix):
    indices = [0,1,2,3]
    t = [[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0]]
    for i in indices:
        for j in indices:
            for k in indices:
                t[i][j] += m[i][k]*n[k][j]
    return t

# ---

errmsg = ''

class AC3DExport:

    def __init__(self, scene, filename):

        global ARG, SKIP_DATA, ADD_DEFAULT_MAT, DEFAULT_MAT, errmsg

        print 'Trying AC3DExport...'

        header = 'AC3Db'
        self.buf = ''
        self.mbuf = ''
        world_kids = 0
        self.mlist = []
        kids_dict = {}
        objlist = []
        bl_objlist2 = []
 
        if ARG == 'all': bl_objlist = scene.getChildren()
        elif ARG == 'sel': bl_objlist = Blender.Object.GetSelected()

        for obj in bl_objlist:
            if obj.getType() != 'Mesh' and obj.getType() != 'Empty':
                continue
            else: kids_dict[obj.name] = 0
            if obj.getParent() == None:
                objlist.append(obj.name)
            else:
                bl_objlist2.append(obj)

        bl_objlist = bl_objlist2[:]
        world_kids = len(objlist)

        while bl_objlist2:
            for obj in bl_objlist:
                obj2 = obj
                dad = obj.getParent()
                kids_dict[dad.name] += 1
                while dad.name not in objlist:
                    obj2 = dad
                    dad = dad.getParent()
                    kids_dict[dad.name] += 1
                objlist.insert(objlist.index(dad.name)+1, obj2.name)
                bl_objlist2.remove(obj2)

        for object in objlist:
            obj = Blender.Object.Get(object)
            self.obj = obj

            if obj.getType() == 'Empty':
                self.OBJECT("group")
                self.name(obj.name)
                #self.rot(obj.rot)
                #self.loc(obj.loc)
            else:
                mesh = self.mesh = obj.getData()
                self.MATERIAL(mesh.materials)
                self.OBJECT("poly")
                self.name(obj.name)
                if not SKIP_DATA: self.data(mesh.name)
                self.texture(mesh.faces)
                self.numvert(mesh.verts, obj.getMatrix())
                self.numsurf(mesh.faces, mesh.hasFaceUV())

            self.kids(kids_dict[object])

        if not self.mbuf or ADD_DEFAULT_MAT:
            self.mbuf = DEFAULT_MAT + '\n' + self.mbuf
            print "\nNo materials: a default (white) has been assigned.\n"
        self.mbuf = self.mbuf + "%s\n%s %s\n" \
                    % ('OBJECT world', 'kids', world_kids)
        buf = "%s\n%s%s" % (header, self.mbuf, self.buf)

        if filename.find('.ac', -3) <= 0: filename += '.ac'

        try:
            file = open(filename, 'w')
        except IOError, (errno, strerror):
            errmsg = "IOError #%s: %s" % (errno, strerror)
            return None
        file.write(buf)
        file.close()

        print "Done. Saved to %s\n" % filename

    def MATERIAL(self, mat):
        if mat == [None]:
            print "Notice -- object %s has no material linked to it:" % self.name
            print "\tThe first entry in the .ac file will be used."
            return
        mbuf = ''
        mlist = self.mlist
        for m in xrange(len(mat)):
            name = mat[m].name
            try:
                mlist.index(name)
            except ValueError:
                mlist.append(name)
                M = Blender.Material.Get(name)
                material = 'MATERIAL "%s"' % name
                mirCol = "%s %s %s" % (Round(M.mirCol[0]),
                                       Round(M.mirCol[1]), Round(M.mirCol[2]))
                rgb = "rgb %s %s %s" % (Round(M.R), Round(M.G), Round(M.B))
                amb = "amb %s %s %s" % (Round(M.amb), Round(M.amb), Round(M.amb))
                if MIRCOL_AS_AMB:
                    amb = "amb %s" % mirCol 
                emis = "emis 0 0 0"
                if MIRCOL_AS_EMIS:
                    emis = "emis %s" % mirCol
                spec = "spec %s %s %s" % (Round(M.specCol[0]),
                                          Round(M.specCol[1]), Round(M.specCol[2]))
                shi = "shi 72"
                trans = "trans %s" % (Round(1 - M.alpha))
                mbuf = mbuf + "%s %s %s %s %s %s %s\n" \
                       % (material, rgb, amb, emis, spec, shi, trans)
        self.mlist = mlist
        self.mbuf = self.mbuf + mbuf

    def OBJECT(self, type):
        self.buf = self.buf + "OBJECT %s\n" % type

    def name(self, name):
        self.buf = self.buf + 'name "%s"\n' % name

    def data(self, name):
        self.buf = self.buf + 'data %s\n%s\n' % (len(name), name)

    def texture(self, faces):
        tex = []
        for f in faces:
            if f.image and f.image.name not in tex:
                tex.append(f.image.name)
        if tex:
            if len(tex) > 1:
                print "\nAC3Db format supports only one texture per object."
                print "Object %s -- using only the first one: %s\n" % (self.obj.name, tex[0])
            image = Blender.Image.Get(tex[0])
            buf = 'texture "%s"\n' % image.filename
            xrep = image.xrep
            yrep = image.yrep
            buf += 'texrep %s %s\n' % (xrep, yrep)
            self.buf = self.buf + buf

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
            rot = rot.strip()
            buf = 'rot %s\n' % rot
            self.buf = self.buf + buf
        
    def loc(self, loc):
        loc = map(Round, loc)
        if loc[0] or loc[1] or loc[2]:
            buf = 'loc %s %s %s\n' % (loc[0], loc[1], loc[2])
            self.buf = self.buf + buf

    def numvert(self, verts, matrix):
        buf = "numvert %s\n" % len(verts)
        m = matrix_mul(matrix)
        verts = transform_verts(verts, m)
        for v in verts:
            v = map(Round, v)
            buf = buf + "%s %s %s\n" % (v[0], v[1], v[2])
        self.buf = self.buf + buf

    def numsurf(self, faces, hasFaceUV):

        global ADD_DEFAULT_MAT
        
        buf = "numsurf %s\n" % len(faces)
        
        mlist = self.mlist
        indexerror = 0
        omlist = {}
        objmats = self.mesh.materials
        for i in range(len(objmats)):
            objmats[i] = objmats[i].name
        for f in faces:
            m_idx = f.materialIndex
            try:
                m_idx = mlist.index(objmats[m_idx])
            except IndexError:
                if not indexerror:
                    print "\nNotice: object " + self.obj.name + \
                          " has at least one material *index* assigned"
                    print "\tbut not defined (not linked to an existing material)."
                    print "\tThis can cause some of its faces to be exported with a wrong color."
                    print "\tYou can fix the problem in the Blender Edit Buttons Window (F9).\n"
                    indexerror = 1
                m_idx = 0
            refs = len(f)
            flaglow = (refs == 2) << 1
            two_side = f.mode & Blender.NMesh.FaceModes['TWOSIDE']
            two_side = (two_side > 0) << 1
            flaghigh = f.smooth | two_side
            buf = buf + "SURF 0x%d%d\n" % (flaghigh, flaglow)
            if ADD_DEFAULT_MAT and objmats: m_idx += 1
            buf = buf + "mat %s\n" % m_idx
            buf = buf + "refs %s\n" % refs
            u, v, vi = 0, 0, 0
            for vert in f.v:
                vindex = self.mesh.verts.index(vert)
                if hasFaceUV:
                    u = f.uv[vi][0]
                    v = f.uv[vi][1]
                    vi += 1
                buf = buf + "%s %s %s\n" % (vindex, u, v)
        self.buf = self.buf + buf

    def kids(self, kids = 0):
        self.buf = self.buf + "kids %s\n" % kids

# End of Class AC3DExport

from Blender import Draw, BGL

def gui():
  global SKIP_DATA, MIRCOL_AS_AMB, MIRCOL_AS_EMIS, ADD_DEFAULT_MAT, HELPME
  global HELPME

  if HELPME:
    BGL.glClearColor(0.6,0.6,0.9,1)
    BGL.glClear(BGL.GL_COLOR_BUFFER_BIT)
    BGL.glColor3f(1,1,1)
    BGL.glRasterPos2i(18, 150)
    Draw.Text("AC3D is a simple, affordable commercial 3d modeller that can be found at www.ac3d.org .")
    BGL.glRasterPos2i(18, 130)
    Draw.Text("It uses a nice text file format (extension .ac) which supports uv-textured meshes")
    BGL.glRasterPos2i(18, 110)
    Draw.Text("with parenting (grouping) information.")
    BGL.glRasterPos2i(18, 90)
    Draw.Text("Notes: AC3D has a 'data' token that assigns a string to each mesh, useful for games,")
    BGL.glRasterPos2i(55, 70)
    Draw.Text("for example. You can use Blender's mesh datablock name for that.")
    BGL.glRasterPos2i(55, 50)
    Draw.Text("The .ac format is well supported by the PLib 3d gaming library.")
    Draw.Button("Ok", 21, 285, 10, 45, 20, "Click to return to previous screen.")
  else:
    BGL.glClearColor(0,0,1,1)
    BGL.glClear(BGL.GL_COLOR_BUFFER_BIT)
    BGL.glColor3f(1,1,1)
    BGL.glRasterPos2i(20, 150)
    Draw.Text("AC3D Exporter")
    Draw.Toggle("Default mat", 1, 15, 100, 90, 20, ADD_DEFAULT_MAT, "Objects without materials assigned get a default (white) one automatically.")
    Draw.Toggle("Skip data", 2, 15, 80, 90, 20, SKIP_DATA, "Don't export mesh names as 'data' info.")
    Draw.Toggle("Mir2Amb", 3, 15, 50, 90, 20, MIRCOL_AS_AMB, "Get AC3D's ambient RGB color for each object from its mirror color in Blender.")
    Draw.Toggle("Mir2Emis", 4, 15, 30, 90, 20, MIRCOL_AS_EMIS, "Get AC3D's emissive RGB color for each object from its mirror color in Blender.")
    Draw.Button("Export All...", 10, 140, 80, 110, 30, "Export all meshes to an AC3D file.")
    Draw.Button("Export Selected...", 11, 140, 40, 110, 30, "Export selected meshes to an AC3D file.")
    Draw.Button("HELP", 20, 285, 80, 100, 40, "Click for additional info.")
    Draw.Button("EXIT", 22, 285, 30, 100, 40, "Click to leave.")

def event(evt, val):
  global HELPME

  if not val: return

  if HELPME:
    if evt == Draw.ESCKEY:
      HELPME = 0
      Draw.Register(gui, event, b_event)
      return
    else: return

  if evt == Draw.ESCKEY:
    update_RegistryInfo()
    Draw.Exit()
    return
  else: return

  Draw.Register(gui, event, b_event)

def b_event(evt):
  global ARG, SKIP_DATA, MIRCOL_AS_AMB, MIRCOL_AS_EMIS, ADD_DEFAULT_MAT
  global HELPME

  if evt == 1:
    ADD_DEFAULT_MAT = 1 - ADD_DEFAULT_MAT
    Draw.Redraw(1)
  elif evt == 2:
    SKIP_DATA = 1 - SKIP_DATA
    Draw.Redraw(1)
  elif evt == 3:
    MIRCOL_AS_AMB = 1 - MIRCOL_AS_AMB
    Draw.Redraw(1)
  elif evt == 4:
    MIRCOL_AS_EMIS = 1 - MIRCOL_AS_EMIS
    Draw.Redraw(1)
  elif evt == 10:
    ARG = 'all'
    Blender.Window.FileSelector(fs_callback, "AC3D Export")
  elif evt == 11:
    ARG = 'sel'
    Blender.Window.FileSelector(fs_callback, "AC3D Export")
  elif evt == 20:
    HELPME = 1 - HELPME
    Draw.Redraw(1)
  elif evt == 21: # leave Help screen
    HELPME = 0
    Draw.Register(gui, event, b_event)
  elif evt == 22:
    update_RegistryInfo()
    Draw.Exit()
  else:
    Draw.Register(gui, event, b_event)

def fs_callback(filename):
  scene = Blender.Scene.GetCurrent()
  test = AC3DExport(scene, filename)

if __script__['arg'] == 'config':
  Draw.Register(gui, event, b_event)
else:
  Blender.Window.FileSelector(fs_callback, "Export AC3D")
