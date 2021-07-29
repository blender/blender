from .vefm_271 import mesh, vertex, edge, face
from math import pi, acos, sin, cos, atan, tan, fabs, sqrt


def check_contains(cl, name, print_value=False):
    dir_class = dir(cl)
    for el in dir_class:
        if el.startswith("_"):
            pass
        else:
            if print_value:
                tmp = getattr(cl, el)
                print(name, " contains ==>", el, " value = ", tmp)
            else:
                print(name, " contains ==>", el)
    print("\ncheck_contains finished\n\n")


class geodesic(mesh):

    def __init__(self):
        mesh.__init__(self)
        self.PKHG_parameters = None

        self.panels = []
        self.vertsdone = []
        self.skeleton = []      # List of verts in the full skeleton edges.
        self.vertskeleton = []  # config needs this member
        self.edgeskeleton = []  # config needs this member
        self.sphericalverts = []
        self.a45 = pi * 0.25
        self.a90 = pi * 0.5
        self.a180 = pi
        self.a270 = pi * 1.5
        self.a360 = pi * 2
        # define members here
        # setparams needs:
        self.frequency = None
        self.eccentricity = None
        self.squish = None
        self.radius = None
        self.square = None
        self.squarez = None
        self.cart = None
        self.shape = None
        self.baselevel = None
        self.faceshape = None
        self.dualflag = None
        self.rotxy = None
        self.rotz = None
        self.klass = None
        self.sform = None
        self.super = None
        self.odd = None
        # config needs
        self.panelpoints = None
        self.paneledges = None
        self.reversepanel = None
        self.edgelength = None
        self.vertsdone = None
        self.panels = []

    def setparameters(self, params):
        parameters = self.PKHG_parameters = params
        self.frequency = parameters[0]     # How many subdivisions - up to 20.
        self.eccentricity = parameters[1]  # Elliptical if >1.0.
        self.squish = parameters[2]        # Flattened if < 1.0.
        self.radius = parameters[3]        # Exactly what it says.
        self.square = parameters[4]        # Controls amount of superellipse in X/Y plane.
        self.squarez = parameters[5]       # Controls amount of superellipse in Z dimension.
        self.cart = parameters[6]          # Cuts out sphericalisation step.
        self.shape = parameters[7]         # Full sphere, dome, flatbase.
        self.baselevel = parameters[8]     # Where the base is cut on a flatbase dome.
        self.faceshape = parameters[9]     # Triangular, hexagonal, tri-hex.
        self.dualflag = parameters[10]
        self.rotxy = parameters[11]
        self.rotz = parameters[12]
        self.klass = parameters[13]
        self.sform = parameters[14]
        self.super = 0                  # Toggles superellipse.
        if self.square != 2.0 or self.squarez != 2.0:
            self.super = 1
        self.odd = 0                    # Is the frequency odd. It matters for dome building.
        if self.frequency % 2 != 0:
            self.odd = 1

    def makegeodesic(self):
        self.vertedgefacedata()         # PKHG only a pass 13okt11
        self.config()                   # Generate all the configuration information.
        if self.klass:
            self.class2()
        if self.faceshape == 1:
            self.hexify()               # Hexagonal faces
        elif self.faceshape == 2:
            self.starify()              # Hex and Triangle faces
        if self.dualflag:
            self.dual()
        if not self.cart:
            self.sphericalize()         # Convert x,y,z positions into spherical u,v.
        self.sphere2cartesian()         # Convert spherical uv back into cartesian x,y,z for final shape.
        for i in range(len(self.verts)):
            self.verts[i].index = i
        for edg in self.edges:
            edg.findvect()

    def vertedgefacedata(self):
        pass

    def config(self):
        for i in range(len(self.vertskeleton)):
            self.vertskeleton[i].index = i
        for edges in self.edgeskeleton:
            s = skeletonrow(self.frequency, edges, 0, self)  # self a geodesic
            self.skeleton.append(s)
        for i in range(len(self.verts)):
            self.verts[i].index = i
        for i in range(len(self.panelpoints)):
            a = self.vertsdone[self.panelpoints[i][0]][1]
            b = self.vertsdone[self.panelpoints[i][1]][1]
            c = self.vertsdone[self.panelpoints[i][2]][1]
            panpoints = [self.verts[a],
                        self.verts[b],
                        self.verts[c]]
            panedges = [self.skeleton[self.paneledges[i][0]],
                        self.skeleton[self.paneledges[i][1]],
                        self.skeleton[self.paneledges[i][2]]]
            reverseflag = 0
            for flag in self.reversepanel:
                if flag == i:
                    reverseflag = 1
            p = panel(panpoints, panedges, reverseflag, self)

    def sphericalize(self):
        if self.shape == 2:
            self.cutbasecomp()
        for vert in(self.verts):

            x = vert.vector.x
            y = vert.vector.y
            z = vert.vector.z

            u = self.usphericalise(x, y, z)
            v = self.vsphericalise(x, y, z)
            self.sphericalverts.append([u, v])

    def sphere2cartesian(self):
        for i in range(len(self.verts)):
            if self.cart:

                x = self.verts[i].vector.x * self.radius * self.eccentricity
                y = self.verts[i].vector.y * self.radius
                z = self.verts[i].vector.z * self.radius * self.squish
            else:
                u = self.sphericalverts[i][0]
                v = self.sphericalverts[i][1]
                if self.squish != 1.0 or self.eccentricity > 1.0:
                    scalez = 1 / self.squish
                    v = self.ellipsecomp(scalez, v)
                    u = self.ellipsecomp(self.eccentricity, u)
                if self.super:
                    r1 = self.superell(self.square, u, self.rotxy)
                    r2 = self.superell(self.squarez, v, self.rotz)
                else:
                    r1 = 1.0
                    r2 = 1.0

                if self.sform[12]:
                    r1 = r1 * self.superform(self.sform[0], self.sform[1],
                                             self.sform[2], self.sform[3],
                                             self.sform[14] + u, self.sform[4],
                                             self.sform[5], self.sform[16] * v)
                if self.sform[13]:
                    r2 = r2 * self.superform(self.sform[6], self.sform[7],
                                             self.sform[8], self.sform[9],
                                             self.sform[15] + v, self.sform[10],
                                             self.sform[11], self.sform[17] * v)
                x, y, z = self.cartesian(u, v, r1, r2)

            self.verts[i] = vertex((x, y, z))

    def usphericalise(self, x, y, z):
        if y == 0.0:
            if x > 0:
                theta = 0.0
            else:
                theta = self.a180
        elif x == 0.0:
            if y > 0:
                theta = self.a90
            else:
                theta = self.a270
        else:
            theta = atan(y / x)

        if x < 0.0 and y < 0.0:
            theta = theta + self.a180
        elif x < 0.0 and y > 0.0:
            theta = theta + self.a180
        u = theta
        return u

    def vsphericalise(self, x, y, z):
        if z == 0.0:
            phi = self.a90
        else:
            rho = sqrt(x ** 2 + y ** 2 + z ** 2)
            phi = acos(z / rho)
        v = phi
        return v

    def ellipsecomp(self, efactor, theta):
        if theta == self.a90:
            result = self.a90
        elif theta == self.a270:
            result = self.a270
        else:
            result = atan(tan(theta) / efactor**0.5)
            if result >= 0.0:
                x = result
                y = self.a180 + result
                if fabs(x - theta) <= fabs(y - theta):
                    result = x
                else:
                    result = y
            else:
                x = self.a180 + result
                y = result

                if fabs(x - theta) <= fabs(y - theta):
                    result = x
                else:
                    result = y
        return result

    def cutbasecomp(self):
        pass

    def cartesian(self, u, v, r1, r2):
        x = r1 * cos(u) * r2 * sin(v) * self.radius * self.eccentricity
        y = r1 * sin(u) * r2 * sin(v) * self.radius
        z = r2 * cos(v) * self.radius * self.squish
        return x, y, z


class edgerow:
    def __init__(self, count, anchor, leftindex, rightindex, stepvector, endflag, parentgeo):
        self.points = []
        self.edges = []
        # Make a row of evenly spaced points.
        for i in range(count + 1):
            if i == 0:
                self.points.append(leftindex)
            elif i == count and not endflag:
                self.points.append(rightindex)
            else:  # PKHG Vectors added!
                newpoint = anchor + (stepvector * i)
                vertcount = len(parentgeo.verts)
                self.points.append(vertcount)
                newpoint.index = vertcount
                parentgeo.verts.append(newpoint)
        for i in range(count):
            a = parentgeo.verts[self.points[i]]
            b = parentgeo.verts[self.points[i + 1]]
            line = edge(a, b)
            self.edges.append(len(parentgeo.edges))
            parentgeo.edges.append(line)


class skeletonrow:
    def __init__(self, count, skeletonedge, shortflag, parentgeo):
        self.points = []
        self.edges = []
        self.vect = skeletonedge.vect
        self.step = skeletonedge.vect / float(count)
        # Make a row of evenly spaced points.
        for i in range(count + 1):
            vert1 = skeletonedge.a
            vert2 = skeletonedge.b
            if i == 0:
                if parentgeo.vertsdone[vert1.index][0]:
                    self.points.append(parentgeo.vertsdone[vert1.index][1])
                else:
                    newpoint = vertex(vert1.vector)
                    vertcount = len(parentgeo.verts)
                    self.points.append(vertcount)
                    newpoint.index = vertcount
                    parentgeo.vertsdone[vert1.index] = [1, vertcount]
                    parentgeo.verts.append(newpoint)

            elif i == count:
                if parentgeo.vertsdone[vert2.index][0]:
                    self.points.append(parentgeo.vertsdone[vert2.index][1])
                else:
                    newpoint = vertex(vert2.vector)
                    vertcount = len(parentgeo.verts)
                    self.points.append(vertcount)
                    newpoint.index = vertcount
                    parentgeo.vertsdone[vert2.index] = [1, vertcount]
                    parentgeo.verts.append(newpoint)
            else:
                newpoint = vertex(vert1.vector + (self.step * i))  # must be a vertex!
                vertcount = len(parentgeo.verts)
                self.points.append(vertcount)
                newpoint.index = vertcount
                parentgeo.verts.append(newpoint)
        for i in range(count):
            a = parentgeo.verts[self.points[i]]
            b = parentgeo.verts[self.points[i + 1]]
            line = edge(a, b)
            self.edges.append(len(parentgeo.edges))
            parentgeo.edges.append(line)


class facefill:
    def __init__(self, upper, lower, reverseflag, parentgeo, finish):
        for i in range(finish):
            a, b, c = upper.points[i], lower.points[i + 1], lower.points[i]
            if reverseflag:
                upface = face([parentgeo.verts[a], parentgeo.verts[c], parentgeo.verts[b]])
            else:
                upface = face([parentgeo.verts[a], parentgeo.verts[b], parentgeo.verts[c]])
            parentgeo.faces.append(upface)
            if i == finish - 1:
                pass
            else:
                d = upper.points[i + 1]
                if reverseflag:
                    downface = face([parentgeo.verts[b], parentgeo.verts[d], parentgeo.verts[a]])
                else:
                    downface = face([parentgeo.verts[b], parentgeo.verts[a], parentgeo.verts[d]])
                line = edge(parentgeo.verts[a], parentgeo.verts[b])
                line2 = edge(parentgeo.verts[d], parentgeo.verts[b])
                parentgeo.faces.append(downface)
                parentgeo.edges.append(line)
                parentgeo.edges.append(line2)


class panel:
    def __init__(self, points, edges, reverseflag, parentgeo):
        self.cardinal = points[0]
        self.leftv = points[1]
        self.rightv = points[2]
        self.leftedge = edges[0]
        self.rightedge = edges[1]
        self.baseedge = edges[2]
        self.rows = []
        self.orient(parentgeo, edges)
        self.createrows(parentgeo)
        self.createfaces(parentgeo, reverseflag)

    def orient(self, parentgeo, edges):
        if self.leftedge.points[0] != self.cardinal.index:
            self.leftedge.points.reverse()
            self.leftedge.vect.negative()

        if self.rightedge.points[0] != self.cardinal.index:
            self.rightedge.points.reverse()
            self.rightedge.vect.negative()

        if self.baseedge.points[0] != self.leftv.index:

            self.baseedge.points.reverse()
            self.baseedge.vect.negative()

    def createrows(self, parentgeo):
        for i in range(len(self.leftedge.points)):
            if i == parentgeo.frequency:
                newrow = self.baseedge
            else:
                newrow = edgerow(i, parentgeo.verts[self.leftedge.points[i]], self.leftedge.points[i],
                                self.rightedge.points[i], self.baseedge.step, 0, parentgeo)
            self.rows.append(newrow)

    def createfaces(self, parentgeo, reverseflag):
        for i in range(len(self.leftedge.points) - 1):
            facefill(self.rows[i], self.rows[i + 1], reverseflag, parentgeo, len(self.rows[i].points))


# for point on top?  YES!
class tetrahedron(geodesic, mesh):
    def __init__(self, parameter):
        geodesic.__init__(mesh)
        geodesic.setparameters(self, parameter)
        self.set_vert_edge_skeleons()

    def set_vert_edge_skeleons(self):
        self.vertskeleton = [vertex((0.0, 0.0, 1.73205080757)),
                            vertex((0.0, -1.63299316185, -0.577350269185)),
                            vertex((1.41421356237, 0.816496580927, -0.57735026919)),
                            vertex((-1.41421356237, 0.816496580927, -0.57735026919))]

        self.edgeskeleton = [edge(self.vertskeleton[0], self.vertskeleton[1]),
                            edge(self.vertskeleton[0], self.vertskeleton[2]),
                            edge(self.vertskeleton[0], self.vertskeleton[3]),
                            edge(self.vertskeleton[1], self.vertskeleton[2]),
                            edge(self.vertskeleton[2], self.vertskeleton[3]),
                            edge(self.vertskeleton[1], self.vertskeleton[3])]

        self.panelpoints = [[0, 1, 2], [0, 2, 3], [0, 1, 3], [1, 2, 3]]
        self.paneledges = [[0, 1, 3], [1, 2, 4], [0, 2, 5], [3, 5, 4]]
        self.reversepanel = [2, 3]
        self.edgelength = []
        self.vertsdone = [[0, 0]] * len(self.vertskeleton)


# for edge on top? YES
class tetraedge(geodesic):
    def __init__(self, parameter):
        geodesic.__init__(mesh)
        geodesic.setparameters(self, parameter)
        self.set_vert_edge_skeleons()

    def set_vert_edge_skeleons(self):
        self.vertskeleton = [vertex((0.0, -1.41421356237, 1.0)),
                            vertex((0.0, 1.41421356237, 1.0)),
                            vertex((1.41421356237, 0.0, -1.0)),
                            vertex((-1.41421356237, 0.0, -1.0))]

        self.edgeskeleton = [edge(self.vertskeleton[0], self.vertskeleton[1]),
                            edge(self.vertskeleton[0], self.vertskeleton[2]),
                            edge(self.vertskeleton[0], self.vertskeleton[3]),
                            edge(self.vertskeleton[1], self.vertskeleton[3]),
                            edge(self.vertskeleton[1], self.vertskeleton[2]),
                            edge(self.vertskeleton[2], self.vertskeleton[3])]

        for i in range(len(self.vertskeleton)):
            self.vertskeleton[i].index = i

        self.panelpoints = [[0, 1, 2], [1, 2, 3], [0, 1, 3], [0, 2, 3]]
        self.paneledges = [[0, 1, 4], [4, 3, 5], [0, 2, 3], [1, 2, 5]]
        self.reversepanel = [0, 3]
        self.edgelength = []
        self.vertsdone = [[0, 0]] * len(self.vertskeleton)


# for face on top? YES
class tetraface(geodesic):
    def __init__(self, parameter):
        geodesic.__init__(mesh)
        geodesic.setparameters(self, parameter)
        self.set_vert_edge_skeleons()

    def set_vert_edge_skeleons(self):
        self.vertskeleton = [vertex((-1.41421356237, -0.816496580927, 0.57735026919)),
                            vertex((1.41421356237, -0.816496580927, 0.57735026919)),
                            vertex((0.0, 1.63299316185, 0.577350269185)),
                            vertex((0.0, 0.0, -1.73205080757))]

        self.edgeskeleton = [edge(self.vertskeleton[0], self.vertskeleton[1]),
                            edge(self.vertskeleton[2], self.vertskeleton[1]),
                            edge(self.vertskeleton[2], self.vertskeleton[0]),
                            edge(self.vertskeleton[0], self.vertskeleton[3]),
                            edge(self.vertskeleton[1], self.vertskeleton[3]),
                            edge(self.vertskeleton[2], self.vertskeleton[3])
                                ]
        self.panelpoints = [[2, 0, 1], [0, 1, 3], [2, 1, 3], [2, 0, 3]]

        self.paneledges = [[2, 1, 0], [0, 3, 4], [1, 5, 4], [2, 5, 3]]
        self.reversepanel = [1, 3]
        self.edgelength = []
        self.vertsdone = [[0, 0]] * len(self.vertskeleton)


class octahedron(geodesic):
    def __init__(self, parameter):
        geodesic.__init__(mesh)
        geodesic.setparameters(self, parameter)
        self.set_vert_edge_skeleons()

    def set_vert_edge_skeleons(self):
        self.vertskeleton = [vertex((0.0, 0.0, 1.0)),
                            vertex((0.0, 1.0, 0.0)),
                            vertex((-1.0, 0.0, 0.0)),
                            vertex((0.0, -1.0, 0.0)),
                            vertex((1.0, 0.0, 0.0)),
                            vertex((0.0, 0.0, -1.0))]

        for i in range(len(self.vertskeleton)):
            self.vertskeleton[i].index = i
        self.edgeskeleton = [edge(self.vertskeleton[0], self.vertskeleton[1]),
                            edge(self.vertskeleton[0], self.vertskeleton[2]),
                            edge(self.vertskeleton[0], self.vertskeleton[3]),
                            edge(self.vertskeleton[0], self.vertskeleton[4]),
                            edge(self.vertskeleton[1], self.vertskeleton[2]),
                            edge(self.vertskeleton[2], self.vertskeleton[3]),
                            edge(self.vertskeleton[3], self.vertskeleton[4]),
                            edge(self.vertskeleton[4], self.vertskeleton[1]),
                            edge(self.vertskeleton[1], self.vertskeleton[5]),
                            edge(self.vertskeleton[2], self.vertskeleton[5]),
                            edge(self.vertskeleton[3], self.vertskeleton[5]),
                            edge(self.vertskeleton[4], self.vertskeleton[5])]

        self.panelpoints = [[0, 1, 2], [0, 2, 3], [0, 3, 4], [0, 4, 1], [1, 2, 5],
                            [2, 3, 5], [3, 4, 5], [4, 1, 5]]
        self.paneledges = [[0, 1, 4], [1, 2, 5], [2, 3, 6], [3, 0, 7], [4, 8, 9],
                            [5, 9, 10], [6, 10, 11], [7, 11, 8]]
        self.reversepanel = [4, 5, 6, 7]
        self.edgelength = []
        self.vertsdone = [[0, 0]] * len(self.vertskeleton)


class octaedge(geodesic):
    def __init__(self, parameter):
        geodesic.__init__(mesh)
        geodesic.setparameters(self, parameter)
        self.set_vert_edge_skeleons()

    def set_vert_edge_skeleons(self):
        self.vertskeleton = [vertex((0.0, -0.707106781187, 0.707106781187)),
                            vertex((0.0, 0.707106781187, 0.707106781187)),
                            vertex((1.0, 0.0, 0.0)),
                            vertex((-1.0, 0.0, 0.0)),
                            vertex((0.0, -0.707106781187, -0.707106781187)),
                            vertex((0.0, 0.707106781187, -0.707106781187))]

        self.edgeskeleton = [edge(self.vertskeleton[0], self.vertskeleton[1]),
                            edge(self.vertskeleton[0], self.vertskeleton[4]),
                            edge(self.vertskeleton[0], self.vertskeleton[2]),
                            edge(self.vertskeleton[1], self.vertskeleton[2]),
                            edge(self.vertskeleton[1], self.vertskeleton[5]),
                            edge(self.vertskeleton[1], self.vertskeleton[3]),
                            edge(self.vertskeleton[0], self.vertskeleton[3]),
                            edge(self.vertskeleton[2], self.vertskeleton[4]),
                            edge(self.vertskeleton[2], self.vertskeleton[5]),
                            edge(self.vertskeleton[3], self.vertskeleton[5]),
                            edge(self.vertskeleton[3], self.vertskeleton[4]),
                            edge(self.vertskeleton[4], self.vertskeleton[5])]

        self.panelpoints = [[0, 1, 2], [0, 1, 3], [0, 2, 4], [1, 2, 5], [1, 3, 5],
                            [0, 3, 4], [2, 4, 5], [3, 4, 5]]
        self.paneledges = [[0, 2, 3], [0, 6, 5], [2, 1, 7], [3, 4, 8], [5, 4, 9],
                           [6, 1, 10], [7, 8, 11], [10, 9, 11]]

        self.reversepanel = [0, 2, 4, 7]
        self.edgelength = []
        self.vertsdone = [[0, 0]] * len(self.vertskeleton)


class octaface(geodesic):
    def __init__(self, parameter):
        geodesic.__init__(mesh)
        geodesic.setparameters(self, parameter)
        self.set_vert_edge_skeleons()

    def set_vert_edge_skeleons(self):
        self.vertskeleton = [vertex((0.408248458663, -0.707106781187, 0.577350150255)),
                            vertex((0.408248458663, 0.707106781187, 0.577350150255)),
                            vertex((-0.816496412728, 0.0, 0.577350507059)),
                            vertex((-0.408248458663, -0.707106781187, -0.577350150255)),
                            vertex((0.816496412728, 0.0, -0.577350507059)),
                            vertex((-0.408248458663, 0.707106781187, -0.577350150255))]

        self.edgeskeleton = [edge(self.vertskeleton[0], self.vertskeleton[1]),
                            edge(self.vertskeleton[2], self.vertskeleton[1]),
                            edge(self.vertskeleton[2], self.vertskeleton[0]),
                            edge(self.vertskeleton[0], self.vertskeleton[3]),
                            edge(self.vertskeleton[0], self.vertskeleton[4]),
                            edge(self.vertskeleton[1], self.vertskeleton[4]),
                            edge(self.vertskeleton[1], self.vertskeleton[5]),
                            edge(self.vertskeleton[2], self.vertskeleton[5]),
                            edge(self.vertskeleton[2], self.vertskeleton[3]),
                            edge(self.vertskeleton[3], self.vertskeleton[4]),
                            edge(self.vertskeleton[4], self.vertskeleton[5]),
                            edge(self.vertskeleton[3], self.vertskeleton[5])]

        self.panelpoints = [[2, 0, 1], [0, 3, 4], [0, 1, 4], [1, 4, 5],
                            [2, 1, 5], [2, 3, 5], [2, 0, 3], [3, 4, 5]]

        self.paneledges = [[2, 1, 0], [3, 4, 9], [0, 4, 5], [5, 6, 10],
                           [1, 7, 6], [8, 7, 11], [2, 8, 3], [9, 11, 10]]

        self.reversepanel = [2, 5, 6, 7]
        self.edgelength = []
        self.vertsdone = [[0, 0]] * len(self.vertskeleton)


class icosahedron(geodesic):
    def __init__(self, parameter):
        geodesic.__init__(mesh)
        geodesic.setparameters(self, parameter)
        self.set_vert_edge_skeleons()

    def set_vert_edge_skeleons(self):
        self.vertskeleton = [vertex((0.0, 0.0, 0.587785252292)),
                            vertex((0.0, -0.525731096637, 0.262865587024)),
                            vertex((0.5, -0.162459832634, 0.262865565628)),
                            vertex((0.309016994375, 0.425325419658, 0.262865531009)),
                            vertex((-0.309016994375, 0.425325419658, 0.262865531009)),
                            vertex((-0.5, -0.162459832634, 0.262865565628)),
                            vertex((0.309016994375, -0.425325419658, -0.262865531009)),
                            vertex((0.5, 0.162459832634, -0.262865565628)),
                            vertex((0.0, 0.525731096637, -0.262865587024)),
                            vertex((-0.5, 0.162459832634, -0.262865565628)),
                            vertex((-0.309016994375, -0.425325419658, -0.262865531009)),
                            vertex((0.0, 0.0, -0.587785252292))]

        self.edgeskeleton = [edge(self.vertskeleton[0], self.vertskeleton[1]),
                            edge(self.vertskeleton[0], self.vertskeleton[2]),
                            edge(self.vertskeleton[0], self.vertskeleton[3]),
                            edge(self.vertskeleton[0], self.vertskeleton[4]),
                            edge(self.vertskeleton[0], self.vertskeleton[5]),
                            edge(self.vertskeleton[1], self.vertskeleton[2]),
                            edge(self.vertskeleton[2], self.vertskeleton[3]),
                            edge(self.vertskeleton[3], self.vertskeleton[4]),
                            edge(self.vertskeleton[4], self.vertskeleton[5]),
                            edge(self.vertskeleton[5], self.vertskeleton[1]),
                            edge(self.vertskeleton[1], self.vertskeleton[6]),
                            edge(self.vertskeleton[2], self.vertskeleton[6]),
                            edge(self.vertskeleton[2], self.vertskeleton[7]),
                            edge(self.vertskeleton[3], self.vertskeleton[7]),
                            edge(self.vertskeleton[3], self.vertskeleton[8]),
                            edge(self.vertskeleton[4], self.vertskeleton[8]),
                            edge(self.vertskeleton[4], self.vertskeleton[9]),
                            edge(self.vertskeleton[5], self.vertskeleton[9]),
                            edge(self.vertskeleton[5], self.vertskeleton[10]),
                            edge(self.vertskeleton[1], self.vertskeleton[10]),
                            edge(self.vertskeleton[6], self.vertskeleton[7]),
                            edge(self.vertskeleton[7], self.vertskeleton[8]),
                            edge(self.vertskeleton[8], self.vertskeleton[9]),
                            edge(self.vertskeleton[9], self.vertskeleton[10]),
                            edge(self.vertskeleton[10], self.vertskeleton[6]),
                            edge(self.vertskeleton[6], self.vertskeleton[11]),
                            edge(self.vertskeleton[7], self.vertskeleton[11]),
                            edge(self.vertskeleton[8], self.vertskeleton[11]),
                            edge(self.vertskeleton[9], self.vertskeleton[11]),
                            edge(self.vertskeleton[10], self.vertskeleton[11])]

        self.panelpoints = [[0, 1, 2], [0, 2, 3], [0, 3, 4], [0, 4, 5], [0, 5, 1], [1, 2, 6],
                            [2, 6, 7], [2, 3, 7], [3, 7, 8], [3, 4, 8], [4, 8, 9], [4, 5, 9],
                            [5, 9, 10], [5, 1, 10], [1, 10, 6], [6, 7, 11], [7, 8, 11],
                            [8, 9, 11], [9, 10, 11], [10, 6, 11]]

        self.paneledges = [[0, 1, 5], [1, 2, 6], [2, 3, 7], [3, 4, 8], [4, 0, 9], [5, 10, 11],
                           [11, 12, 20], [6, 12, 13], [13, 14, 21], [7, 14, 15], [15, 16, 22],
                           [8, 16, 17], [17, 18, 23], [9, 18, 19], [19, 10, 24], [20, 25, 26],
                           [21, 26, 27], [22, 27, 28], [23, 28, 29], [24, 29, 25]]

        self.reversepanel = [5, 7, 9, 11, 13, 15, 16, 17, 18, 19]
        self.edgelength = []
        self.vertsdone = [[0, 0]] * len(self.vertskeleton)


class icoedge(geodesic):
    def __init__(self, parameter):
        geodesic.__init__(mesh)
        geodesic.setparameters(self, parameter)
        self.set_vert_edge_skeleons()

    def set_vert_edge_skeleons(self):
        self.vertskeleton = [vertex((0, 0.309016994375, 0.5)),
                            vertex((0, -0.309016994375, 0.5)),
                            vertex((-0.5, 0, 0.309016994375)),
                            vertex((0.5, 0, 0.309016994375)),
                            vertex((-0.309016994375, -0.5, 0)),
                            vertex((0.309016994375, -0.5, 0)),
                            vertex((0.309016994375, 0.5, 0)),
                            vertex((-0.309016994375, 0.5, 0)),
                            vertex((-0.5, 0, -0.309016994375)),
                            vertex((0.5, 0, -0.309016994375)),
                            vertex((0, 0.309016994375, -0.5)),
                            vertex((0, -0.309016994375, -0.5))]

        self.edgeskeleton = [edge(self.vertskeleton[0], self.vertskeleton[1]),
                            edge(self.vertskeleton[0], self.vertskeleton[7]),
                            edge(self.vertskeleton[0], self.vertskeleton[2]),
                            edge(self.vertskeleton[1], self.vertskeleton[2]),
                            edge(self.vertskeleton[1], self.vertskeleton[4]),
                            edge(self.vertskeleton[1], self.vertskeleton[5]),
                            edge(self.vertskeleton[1], self.vertskeleton[3]),
                            edge(self.vertskeleton[0], self.vertskeleton[3]),
                            edge(self.vertskeleton[0], self.vertskeleton[6]),
                            edge(self.vertskeleton[2], self.vertskeleton[7]),
                            edge(self.vertskeleton[2], self.vertskeleton[8]),
                            edge(self.vertskeleton[2], self.vertskeleton[4]),
                            edge(self.vertskeleton[4], self.vertskeleton[5]),
                            edge(self.vertskeleton[3], self.vertskeleton[5]),
                            edge(self.vertskeleton[3], self.vertskeleton[9]),
                            edge(self.vertskeleton[3], self.vertskeleton[6]),
                            edge(self.vertskeleton[6], self.vertskeleton[7]),
                            edge(self.vertskeleton[7], self.vertskeleton[10]),
                            edge(self.vertskeleton[7], self.vertskeleton[8]),
                            edge(self.vertskeleton[4], self.vertskeleton[8]),
                            edge(self.vertskeleton[4], self.vertskeleton[11]),
                            edge(self.vertskeleton[5], self.vertskeleton[11]),
                            edge(self.vertskeleton[5], self.vertskeleton[9]),
                            edge(self.vertskeleton[6], self.vertskeleton[9]),
                            edge(self.vertskeleton[6], self.vertskeleton[10]),
                            edge(self.vertskeleton[8], self.vertskeleton[10]),
                            edge(self.vertskeleton[8], self.vertskeleton[11]),
                            edge(self.vertskeleton[9], self.vertskeleton[11]),
                            edge(self.vertskeleton[9], self.vertskeleton[10]),
                            edge(self.vertskeleton[10], self.vertskeleton[11])]

        self.panelpoints = [[0, 1, 2], [0, 1, 3], [0, 2, 7], [1, 2, 4], [1, 4, 5],
                            [1, 3, 5], [0, 3, 6], [0, 6, 7], [2, 7, 8], [2, 4, 8],
                            [3, 5, 9], [3, 6, 9], [7, 8, 10], [4, 8, 11], [4, 5, 11],
                            [5, 9, 11], [6, 9, 10], [6, 7, 10], [8, 10, 11], [9, 10, 11]]

        self.paneledges = [[0, 2, 3], [0, 7, 6], [2, 1, 9], [3, 4, 11], [4, 5, 12], [6, 5, 13],
                           [7, 8, 15], [8, 1, 16], [9, 10, 18], [11, 10, 19], [13, 14, 22],
                           [15, 14, 23], [18, 17, 25], [19, 20, 26], [12, 20, 21], [22, 21, 27],
                           [23, 24, 28], [16, 24, 17], [25, 26, 29], [28, 27, 29]]

        self.reversepanel = [0, 2, 5, 9, 11, 12, 14, 15, 17, 19]
        self.edgelength = []
        self.vertsdone = [[0, 0]] * len(self.vertskeleton)


class icoface(geodesic):
    def __init__(self, parameter):
        geodesic.__init__(mesh)
        geodesic.setparameters(self, parameter)
        self.set_vert_edge_skeleons()

    def set_vert_edge_skeleons(self):
        self.vertskeleton = [vertex((-0.17841104489, 0.309016994375, 0.46708617948)),
                            vertex((-0.17841104489, -0.309016994375, 0.46708617948)),
                            vertex((0.35682208977, 0.0, 0.467086179484)),
                            vertex((-0.57735026919, 0.0, 0.110264089705)),
                            vertex((-0.288675134594, -0.5, -0.11026408971)),
                            vertex((0.288675134594, -0.5, 0.11026408971)),
                            vertex((0.57735026919, 0.0, -0.110264089705)),
                            vertex((0.288675134594, 0.5, 0.11026408971)),
                            vertex((-0.288675134594, 0.5, -0.11026408971)),
                            vertex((-0.35682208977, 0.0, -0.467086179484)),
                            vertex((0.17841104489, -0.309016994375, -0.46708617948)),
                            vertex((0.17841104489, 0.309016994375, -0.46708617948))]

        self.edgeskeleton = [edge(self.vertskeleton[0], self.vertskeleton[1]),
                            edge(self.vertskeleton[2], self.vertskeleton[1]),
                            edge(self.vertskeleton[2], self.vertskeleton[0]),
                            edge(self.vertskeleton[0], self.vertskeleton[3]),
                            edge(self.vertskeleton[1], self.vertskeleton[3]),
                            edge(self.vertskeleton[1], self.vertskeleton[4]),
                            edge(self.vertskeleton[1], self.vertskeleton[5]),
                            edge(self.vertskeleton[2], self.vertskeleton[5]),
                            edge(self.vertskeleton[2], self.vertskeleton[6]),
                            edge(self.vertskeleton[2], self.vertskeleton[7]),
                            edge(self.vertskeleton[0], self.vertskeleton[7]),
                            edge(self.vertskeleton[0], self.vertskeleton[8]),
                            edge(self.vertskeleton[3], self.vertskeleton[9]),
                            edge(self.vertskeleton[3], self.vertskeleton[4]),
                            edge(self.vertskeleton[5], self.vertskeleton[4]),
                            edge(self.vertskeleton[5], self.vertskeleton[10]),
                            edge(self.vertskeleton[5], self.vertskeleton[6]),
                            edge(self.vertskeleton[7], self.vertskeleton[6]),
                            edge(self.vertskeleton[7], self.vertskeleton[11]),
                            edge(self.vertskeleton[7], self.vertskeleton[8]),
                            edge(self.vertskeleton[3], self.vertskeleton[8]),
                            edge(self.vertskeleton[4], self.vertskeleton[9]),
                            edge(self.vertskeleton[4], self.vertskeleton[10]),
                            edge(self.vertskeleton[6], self.vertskeleton[10]),
                            edge(self.vertskeleton[6], self.vertskeleton[11]),
                            edge(self.vertskeleton[8], self.vertskeleton[11]),
                            edge(self.vertskeleton[8], self.vertskeleton[9]),
                            edge(self.vertskeleton[9], self.vertskeleton[10]),
                            edge(self.vertskeleton[11], self.vertskeleton[10]),
                            edge(self.vertskeleton[11], self.vertskeleton[9])]

        self.panelpoints = [[2, 0, 1], [0, 1, 3], [2, 1, 5], [2, 0, 7], [1, 3, 4], [1, 5, 4],
                            [2, 5, 6], [2, 7, 6], [0, 7, 8], [0, 3, 8], [3, 4, 9], [5, 4, 10],
                            [5, 6, 10], [7, 6, 11], [7, 8, 11], [3, 8, 9], [4, 9, 10],
                            [6, 11, 10], [8, 11, 9], [11, 9, 10]]

        self.paneledges = [[2, 1, 0], [0, 3, 4], [1, 7, 6], [2, 9, 10], [4, 5, 13], [6, 5, 14],
                           [7, 8, 16], [9, 8, 17], [10, 11, 19], [3, 11, 20], [13, 12, 21],
                           [14, 15, 22], [16, 15, 23], [17, 18, 24], [19, 18, 25], [20, 12, 26],
                           [21, 22, 27], [24, 23, 28], [25, 26, 29], [29, 28, 27]]

        self.reversepanel = [1, 3, 5, 7, 9, 10, 12, 14, 17, 19]
        self.edgelength = []
        self.vertsdone = [[0, 0]] * len(self.vertskeleton)


# PKHG TODO this does not work yet ...
def creategeo(geo, polytype, orientation, parameters):

    if polytype == 'Tetrahedron':
        if orientation == 'PointUp':
            my_tetrahedron = tetrahedron(geodesic)
            my_tetrahedron.set_vert_edge_skeleons()
            my_tetrahedron.config()
            check_contains(my_tetrahedron, "my_tetra", True)
            vefm_add_object(geo)
        elif orientation == 'EdgeUp':
            geo = tetraedge(parameters)
        else:  # orientation == 2:
            geo = tetraface(parameters)
    elif polytype == 'Octahedron':
        if orientation == 'PointUp':
            geo = octahedron(parameters)
        elif orientation == 'EdgeUp':
            geo = octaedge(parameters)
        else:  # if orientation == 2:
            geo = octaface(parameters)
    elif polytype == 'Icosahedron':
        if orientation == 'PointUp':
            geo = icosahedron(parameters)
        elif orientation == 'EdgeUp':
            geo = icoedge(parameters)
        else:  # if orientation == 2:
            geo = icoface(parameters)

    return geo
