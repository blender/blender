from math import sin, cos, sqrt
from .vefm_271 import *


class form(mesh):
    def __init__(self, uresolution, vresolution, uscale, vscale, upart,
                 vpart, uphase, vphase, utwist, vtwist, xscale, yscale, sform):
        mesh.__init__(self)

        self.PKHG_parameters = [uresolution, vresolution, uscale, vscale, upart,
                                vpart, uphase, vphase, utwist, vtwist, xscale, yscale, sform]
        self.ures = uresolution
        self.vres = vresolution

        self.uscale = uscale
        self.vscale = vscale
        self.upart = upart
        self.vpart = vpart
        self.uphase = uphase * self.a360
        self.vphase = vphase * self.a360
        self.utwist = utwist
        self.vtwist = vtwist

        self.xscale = xscale
        self.yscale = yscale
        self.sform = sform

        if self.upart != 1.0:    # there is a gap in the major radius
            self.uflag = 1
        else:
            self.uflag = 0
        if self.vpart != 1.0:    # there is a gap in the minor radius
            self.vflag = 1
        else:
            self.vflag = 0
        if self.uflag:
            self.ufinish = self.ures + 1
        else:
            self.ufinish = self.ures
        if self.vflag:
            self.vfinish = self.vres + 1
        else:
            self.vfinish = self.vres
        self.ustep = (self.a360 / self.ures) * self.upart
        self.vstep = (self.a360 / self.vres) * self.vpart
        if self.xscale != 1.0:
            self.xscaleflag = 1
        else:
            self.xscaleflag = 0
        if self.yscale != 1.0:
            self.yscaleflag = 1
        else:
            self.yscaleflag = 0
        self.rowlist = []

    def generatepoints(self):
        for i in range(self.ufinish):
            row = []
            for j in range(self.vfinish):
                u = self.ustep * i + self.uphase
                v = self.vstep * j + self.vphase

                if self.sform[12]:
                    r1 = self.superform(self.sform[0], self.sform[1], self.sform[2],
                                        self.sform[3], self.sform[14] + u, self.sform[4],
                                        self.sform[5], self.sform[16] * v)
                else:
                    r1 = 1.0
                if self.sform[13]:
                    r2 = self.superform(self.sform[6], self.sform[7], self.sform[8],
                                        self.sform[9], self.sform[15] + v, self.sform[10],
                                        self.sform[11], self.sform[17] * v)
                else:
                    r2 = 1.0
                x, y, z = self.formula(u, v, r1, r2)
                point = vertex((x, y, z))
                row.append(point)
                self.verts.append(point)
            self.rowlist.append(row)

        if self.vflag:
            pass
        else:
            for i in range(len(self.rowlist)):
                self.rowlist[i].append(self.rowlist[i][0])
        if self.uflag:
            pass
        else:
            self.rowlist.append(self.rowlist[0])

    def generatefaces(self):
        ufin = len(self.rowlist) - 1
        vfin = len(self.rowlist[0]) - 1
        for i in range(ufin):
            for j in range(vfin):
                top = i
                bottom = i + 1
                left = j
                right = j + 1
                a = self.rowlist[top][left]
                b = self.rowlist[top][right]
                c = self.rowlist[bottom][right]
                d = self.rowlist[bottom][left]
                face1 = face([a, b, c, d])
                self.faces.append(face1)
                edge1 = edge(a, b)
                edge2 = edge(a, d)
                self.edges.append(edge1)
                self.edges.append(edge2)
                if i + 1 == ufin:
                    edge3 = edge(d, c)
                    self.edges.append(edge3)
                if j + 1 == vfin:
                    edge4 = edge(b, c)
                    self.edges.append(edge4)


class grid(form):
    def __init__(self, uresolution, vresolution, uscale, vscale, upart, vpart,
                 uphase, vphase, utwist, vtwist, xscale, yscale, sform):
        form.__init__(self, uresolution, vresolution, uscale, vscale, upart, vpart,
                      uphase, vphase, utwist, vtwist, xscale, yscale, sform)
        unit = 1.0 / self.a360

        if self.ures == 1:
            print("\n***ERRORin forms_271.grid L126***, ures is  1, changed into 2\n\n")
            self.ures = 2
        if self.vres == 1:
            print("\n***ERROR in grid forms_271.grid L129***, vres is 1, changed into 2\n\n")
            self.vres = 2
        self.ustep = self.a360 / (self.ures - 1)
        self.vstep = self.a360 / (self.vres - 1)

        self.uflag = 1
        self.vflag = 1

        self.xscaleflag = 0
        self.yscaleflag = 0
        self.uexpand = unit * self.uscale
        self.vexpand = unit * self.vscale
        self.ushift = self.uscale * 0.5
        self.vshift = self.vscale * 0.5

        self.generatepoints()
        self.generatefaces()
        for i in range(len(self.verts)):
            self.verts[i].index = i
        self.connectivity()

    def formula(self, u, v, r1, r2):
        x = u * self.uexpand - self.ushift
        y = v * self.vexpand - self.vshift
        z = r1 * r2 - 1.0
        return x, y, z


class cylinder(form):
    def __init__(self, uresolution, vresolution, uscale, vscale, upart, vpart,
                 uphase, vphase, utwist, vtwist, xscale, yscale, sform):
        form.__init__(self, uresolution, vresolution, uscale, vscale, upart, vpart,
                      uphase, vphase, utwist, vtwist, xscale, yscale, sform)
        unit = 1.0 / self.a360
        self.vshift = self.vscale * 0.5
        self.vexpand = unit * self.vscale
        self.vflag = 1
        self.generatepoints()
        self.generatefaces()
        for i in range(len(self.verts)):
            self.verts[i].index = i
        self.connectivity()

    def formula(self, u, v, r1, r2):
        x = sin(u) * self.uscale * r1 * r2 * self.xscale
        y = cos(u) * self.uscale * r1 * r2
        z = v * self.vexpand - self.vshift
        return x, y, z


class parabola(form):
    def __init__(self, uresolution, vresolution, uscale, vscale, upart, vpart,
                 uphase, vphase, utwist, vtwist, xscale, yscale, sform):
        form.__init__(self, uresolution, vresolution, uscale, vscale, upart, vpart,
                      uphase, vphase, utwist, vtwist, xscale, yscale, sform)
        unit = 1.0 / self.a360
        self.vshift = self.vscale * 0.5
        self.vexpand = unit * self.vscale
        self.vflag = 1
        self.generatepoints()
        self.generatefaces()
        for i in range(len(self.verts)):
            self.verts[i].index = i
        self.connectivity()

    def formula(self, u, v, r1, r2):
        factor = sqrt(v) + 0.001
        x = sin(u) * factor * self.uscale * r1 * r2 * self.xscale
        y = cos(u) * factor * self.uscale * r1 * r2
        z = - v * self.vexpand + self.vshift
        return x, y, z


class torus(form):
    def __init__(self, uresolution, vresolution, uscale, vscale, upart, vpart,
                 uphase, vphase, utwist, vtwist, xscale, yscale, sform):
        form.__init__(self, uresolution, vresolution, uscale, vscale, upart, vpart,
                      uphase, vphase, utwist, vtwist, xscale, yscale, sform)
        self.generatepoints()
        self.generatefaces()
        for i in range(len(self.verts)):
            self.verts[i].index = i
        self.connectivity()

    def formula(self, u, v, r1, r2):
        z = sin(v) * self.uscale * r2 * self.yscale
        y = (self.vscale + self.uscale * cos(v)) * cos(u) * r1 * r2
        x = (self.vscale + self.uscale * cos(v)) * sin(u) * r1 * r2 * self.xscale
        return x, y, z


class sphere(form):
    def __init__(self, uresolution, vresolution, uscale, vscale, upart, vpart,
                 uphase, vphase, utwist, vtwist, xscale, yscale, sform):
        form.__init__(self, uresolution, vresolution, uscale, vscale, upart, vpart,
                      uphase, vphase, utwist, vtwist, xscale, yscale, sform)
        self.vstep = (self.a360 / (self.vres - 1)) * self.vpart
        self.vflag = 1
        self.generatepoints()
        self.generatefaces()
        for i in range(len(self.verts)):
            self.verts[i].index = i
        self.connectivity()

    def formula(self, u, v, r1, r2):
        v = (v * 0.5) - (self.a360 * 0.25)
        x = r1 * cos(u) * r2 * cos(v) * self.uscale * self.xscale
        y = r1 * sin(u) * r2 * cos(v) * self.uscale
        z = r2 * sin(v) * self.uscale * self.yscale
        return x, y, z
