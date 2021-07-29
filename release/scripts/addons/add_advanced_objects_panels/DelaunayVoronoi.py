# -*- coding: utf-8 -*-

# Voronoi diagram calculator/ Delaunay triangulator
#
# - Voronoi Diagram Sweepline algorithm and C code by Steven Fortune,
#   1987, http://ect.bell-labs.com/who/sjf/
# - Python translation to file voronoi.py by Bill Simons, 2005, http://www.oxfish.com/
# - Additional changes for QGIS by Carson Farmer added November 2010
# - 2012 Ported to Python 3 and additional clip functions by domlysz at gmail.com
#
# Calculate Delaunay triangulation or the Voronoi polygons for a set of
# 2D input points.
#
# Derived from code bearing the following notice:
#
#  The author of this software is Steven Fortune.  Copyright (c) 1994 by AT&T
#  Bell Laboratories.
#  Permission to use, copy, modify, and distribute this software for any
#  purpose without fee is hereby granted, provided that this entire notice
#  is included in all copies of any software which is or includes a copy
#  or modification of this software and in all copies of the supporting
#  documentation for such software.
#  THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
#  WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR AT&T MAKE ANY
#  REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
#  OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
#
# Comments were incorporated from Shane O'Sullivan's translation of the
# original code into C++ (http://mapviewer.skynet.ie/voronoi.html)
#
# Steve Fortune's homepage: http://netlib.bell-labs.com/cm/cs/who/sjf/index.html
#
# For programmatic use, two functions are available:
#
#   computeVoronoiDiagram(points, xBuff, yBuff, polygonsOutput=False, formatOutput=False):
#   Takes :
#       - a list of point objects (which must have x and y fields).
#       - x and y buffer values which are the expansion percentages of the
#         bounding box rectangle including all input points.
#       Returns :
#       - With default options :
#         A list of 2-tuples, representing the two points of each Voronoi diagram edge.
#         Each point contains 2-tuples which are the x,y coordinates of point.
#         if formatOutput is True, returns :
#               - a list of 2-tuples, which are the x,y coordinates of the Voronoi diagram vertices.
#               - and a list of 2-tuples (v1, v2) representing edges of the Voronoi diagram.
#                 v1 and v2 are the indices of the vertices at the end of the edge.
#       - If polygonsOutput option is True, returns :
#         A dictionary of polygons, keys are the indices of the input points,
#         values contains n-tuples representing the n points of each Voronoi diagram polygon.
#         Each point contains 2-tuples which are the x,y coordinates of point.
#         if formatOutput is True, returns :
#               - A list of 2-tuples, which are the x,y coordinates of the Voronoi diagram vertices.
#               - and a dictionary of input points indices. Values contains n-tuples representing
#                 the n points of each Voronoi diagram polygon.
#                 Each tuple contains the vertex indices of the polygon vertices.
#
#   computeDelaunayTriangulation(points):
#       Takes a list of point objects (which must have x and y fields).
#       Returns a list of 3-tuples: the indices of the points that form a Delaunay triangle.

import bpy
import math

# Globals
TOLERANCE = 1e-9
BIG_FLOAT = 1e38


class Context(object):

    def __init__(self):
        self.doPrint = 0
        self.debug = 0

        # tuple (xmin, xmax, ymin, ymax)
        self.extent = ()
        self.triangulate = False
        # list of vertex 2-tuples: (x,y)
        self.vertices = []
        # equation of line 3-tuple (a b c), for the equation of the line a*x+b*y = c
        self.lines = []

        # edge 3-tuple: (line index, vertex 1 index, vertex 2 index)
        # if either vertex index is -1, the edge extends to infinity
        self.edges = []
        # 3-tuple of vertex indices
        self.triangles = []
        # a dict of site:[edges] pairs
        self.polygons = {}


# Clip functions #
    def getClipEdges(self):
        xmin, xmax, ymin, ymax = self.extent
        clipEdges = []
        for edge in self.edges:
            equation = self.lines[edge[0]]       # line equation
            if edge[1] != -1 and edge[2] != -1:  # finite line
                x1, y1 = self.vertices[edge[1]][0], self.vertices[edge[1]][1]
                x2, y2 = self.vertices[edge[2]][0], self.vertices[edge[2]][1]
                pt1, pt2 = (x1, y1), (x2, y2)
                inExtentP1, inExtentP2 = self.inExtent(x1, y1), self.inExtent(x2, y2)
                if inExtentP1 and inExtentP2:
                    clipEdges.append((pt1, pt2))
                elif inExtentP1 and not inExtentP2:
                    pt2 = self.clipLine(x1, y1, equation, leftDir=False)
                    clipEdges.append((pt1, pt2))
                elif not inExtentP1 and inExtentP2:
                    pt1 = self.clipLine(x2, y2, equation, leftDir=True)
                    clipEdges.append((pt1, pt2))
            else:  # infinite line
                if edge[1] != -1:
                    x1, y1 = self.vertices[edge[1]][0], self.vertices[edge[1]][1]
                    leftDir = False
                else:
                    x1, y1 = self.vertices[edge[2]][0], self.vertices[edge[2]][1]
                    leftDir = True
                if self.inExtent(x1, y1):
                    pt1 = (x1, y1)
                    pt2 = self.clipLine(x1, y1, equation, leftDir)
                    clipEdges.append((pt1, pt2))
        return clipEdges

    def getClipPolygons(self, closePoly):
        xmin, xmax, ymin, ymax = self.extent
        poly = {}
        for inPtsIdx, edges in self.polygons.items():
            clipEdges = []
            for edge in edges:
                equation = self.lines[edge[0]]       # line equation
                if edge[1] != -1 and edge[2] != -1:  # finite line
                    x1, y1 = self.vertices[edge[1]][0], self.vertices[edge[1]][1]
                    x2, y2 = self.vertices[edge[2]][0], self.vertices[edge[2]][1]
                    pt1, pt2 = (x1, y1), (x2, y2)
                    inExtentP1, inExtentP2 = self.inExtent(x1, y1), self.inExtent(x2, y2)
                    if inExtentP1 and inExtentP2:
                        clipEdges.append((pt1, pt2))
                    elif inExtentP1 and not inExtentP2:
                        pt2 = self.clipLine(x1, y1, equation, leftDir=False)
                        clipEdges.append((pt1, pt2))
                    elif not inExtentP1 and inExtentP2:
                        pt1 = self.clipLine(x2, y2, equation, leftDir=True)
                        clipEdges.append((pt1, pt2))
                else:  # infinite line
                    if edge[1] != -1:
                        x1, y1 = self.vertices[edge[1]][0], self.vertices[edge[1]][1]
                        leftDir = False
                    else:
                        x1, y1 = self.vertices[edge[2]][0], self.vertices[edge[2]][1]
                        leftDir = True
                    if self.inExtent(x1, y1):
                        pt1 = (x1, y1)
                        pt2 = self.clipLine(x1, y1, equation, leftDir)
                        clipEdges.append((pt1, pt2))
            # create polygon definition from edges and check if polygon is completely closed
            polyPts, complete = self.orderPts(clipEdges)
            if not complete:
                startPt = polyPts[0]
                endPt = polyPts[-1]
                # if start & end points are collinear then they are along an extent border
                if startPt[0] == endPt[0] or startPt[1] == endPt[1]:
                    polyPts.append(polyPts[0])  # simple close
                else:  # close at extent corner
                    # upper left
                    if (startPt[0] == xmin and endPt[1] == ymax) or (endPt[0] == xmin and startPt[1] == ymax):
                        polyPts.append((xmin, ymax))  # corner point
                        polyPts.append(polyPts[0])    # close polygon
                    # upper right
                    if (startPt[0] == xmax and endPt[1] == ymax) or (endPt[0] == xmax and startPt[1] == ymax):
                        polyPts.append((xmax, ymax))
                        polyPts.append(polyPts[0])
                    # bottom right
                    if (startPt[0] == xmax and endPt[1] == ymin) or (endPt[0] == xmax and startPt[1] == ymin):
                        polyPts.append((xmax, ymin))
                        polyPts.append(polyPts[0])
                    # bottom left
                    if (startPt[0] == xmin and endPt[1] == ymin) or (endPt[0] == xmin and startPt[1] == ymin):
                        polyPts.append((xmin, ymin))
                        polyPts.append(polyPts[0])
            if not closePoly:  # unclose polygon
                polyPts = polyPts[:-1]
            poly[inPtsIdx] = polyPts
        return poly

    def clipLine(self, x1, y1, equation, leftDir):
        xmin, xmax, ymin, ymax = self.extent
        a, b, c = equation
        if b == 0:       # vertical line
            if leftDir:  # left is bottom of vertical line
                return (x1, ymax)
            else:
                return (x1, ymin)
        elif a == 0:     # horizontal line
            if leftDir:
                return (xmin, y1)
            else:
                return (xmax, y1)
        else:
            y2_at_xmin = (c - a * xmin) / b
            y2_at_xmax = (c - a * xmax) / b
            x2_at_ymin = (c - b * ymin) / a
            x2_at_ymax = (c - b * ymax) / a
            intersectPts = []
            if ymin <= y2_at_xmin <= ymax:  # valid intersect point
                intersectPts.append((xmin, y2_at_xmin))
            if ymin <= y2_at_xmax <= ymax:
                intersectPts.append((xmax, y2_at_xmax))
            if xmin <= x2_at_ymin <= xmax:
                intersectPts.append((x2_at_ymin, ymin))
            if xmin <= x2_at_ymax <= xmax:
                intersectPts.append((x2_at_ymax, ymax))
            # delete duplicate (happens if intersect point is at extent corner)
            intersectPts = set(intersectPts)
            # choose target intersect point
            if leftDir:
                pt = min(intersectPts)  # smaller x value
            else:
                pt = max(intersectPts)
            return pt

    def inExtent(self, x, y):
        xmin, xmax, ymin, ymax = self.extent
        return x >= xmin and x <= xmax and y >= ymin and y <= ymax

    def orderPts(self, edges):
        poly = []  # returned polygon points list [pt1, pt2, pt3, pt4 ....]
        pts = []
        # get points list
        for edge in edges:
            pts.extend([pt for pt in edge])
        # try to get start & end point
        try:
            startPt, endPt = [pt for pt in pts if pts.count(pt) < 2]  # start and end point aren't duplicate
        except:  # all points are duplicate --> polygon is complete --> append some or other edge points
            complete = True
            firstIdx = 0
            poly.append(edges[0][0])
            poly.append(edges[0][1])
        else:  # incomplete --> append the first edge points
            complete = False
            # search first edge
            for i, edge in enumerate(edges):
                if startPt in edge:  # find
                    firstIdx = i
                    break
            poly.append(edges[firstIdx][0])
            poly.append(edges[firstIdx][1])
            if poly[0] != startPt:
                poly.reverse()
        # append next points in list
        del edges[firstIdx]
        while edges:  # all points will be treated when edges list will be empty
            currentPt = poly[-1]  # last item
            for i, edge in enumerate(edges):
                if currentPt == edge[0]:
                    poly.append(edge[1])
                    break
                elif currentPt == edge[1]:
                    poly.append(edge[0])
                    break
            del edges[i]
        return poly, complete

    def setClipBuffer(self, xpourcent, ypourcent):
        xmin, xmax, ymin, ymax = self.extent
        witdh = xmax - xmin
        height = ymax - ymin
        xmin = xmin - witdh * xpourcent / 100
        xmax = xmax + witdh * xpourcent / 100
        ymin = ymin - height * ypourcent / 100
        ymax = ymax + height * ypourcent / 100
        self.extent = xmin, xmax, ymin, ymax

    # End clip functions #

    def outSite(self, s):
        if(self.debug):
            print("site (%d) at %f %f" % (s.sitenum, s.x, s.y))
        elif(self.triangulate):
            pass
        elif(self.doPrint):
            print("s %f %f" % (s.x, s.y))

    def outVertex(self, s):
        self.vertices.append((s.x, s.y))
        if(self.debug):
            print("vertex(%d) at %f %f" % (s.sitenum, s.x, s.y))
        elif(self.triangulate):
            pass
        elif(self.doPrint):
            print("v %f %f" % (s.x, s.y))

    def outTriple(self, s1, s2, s3):
        self.triangles.append((s1.sitenum, s2.sitenum, s3.sitenum))
        if (self.debug):
            print("circle through left=%d right=%d bottom=%d" % (s1.sitenum, s2.sitenum, s3.sitenum))
        elif (self.triangulate and self.doPrint):
            print("%d %d %d" % (s1.sitenum, s2.sitenum, s3.sitenum))

    def outBisector(self, edge):
        self.lines.append((edge.a, edge.b, edge.c))
        if (self.debug):
            print("line(%d) %gx+%gy=%g, bisecting %d %d" % (edge.edgenum, edge.a, edge.b,
                                                            edge.c, edge.reg[0].sitenum,
                                                            edge.reg[1].sitenum)
                )
        elif(self.doPrint):
            print("l %f %f %f" % (edge.a, edge.b, edge.c))

    def outEdge(self, edge):
        sitenumL = -1
        if edge.ep[Edge.LE] is not None:
            sitenumL = edge.ep[Edge.LE].sitenum
        sitenumR = -1
        if edge.ep[Edge.RE] is not None:
            sitenumR = edge.ep[Edge.RE].sitenum

        # polygons dict add by CF
        if edge.reg[0].sitenum not in self.polygons:
            self.polygons[edge.reg[0].sitenum] = []
        if edge.reg[1].sitenum not in self.polygons:
            self.polygons[edge.reg[1].sitenum] = []
        self.polygons[edge.reg[0].sitenum].append((edge.edgenum, sitenumL, sitenumR))
        self.polygons[edge.reg[1].sitenum].append((edge.edgenum, sitenumL, sitenumR))

        self.edges.append((edge.edgenum, sitenumL, sitenumR))

        if (not self.triangulate):
            if (self.doPrint):
                print("e %d" % edge.edgenum)
                print(" %d " % sitenumL)
                print("%d" % sitenumR)


def voronoi(siteList, context):
    context.extent = siteList.extent
    edgeList = EdgeList(siteList.xmin, siteList.xmax, len(siteList))
    priorityQ = PriorityQueue(siteList.ymin, siteList.ymax, len(siteList))
    siteIter = siteList.iterator()

    bottomsite = siteIter.next()
    context.outSite(bottomsite)
    newsite = siteIter.next()
    minpt = Site(-BIG_FLOAT, -BIG_FLOAT)
    while True:
        if not priorityQ.isEmpty():
            minpt = priorityQ.getMinPt()

        if (newsite and (priorityQ.isEmpty() or newsite < minpt)):
            # newsite is smallest -  this is a site event
            context.outSite(newsite)

            # get first Halfedge to the LEFT and RIGHT of the new site
            lbnd = edgeList.leftbnd(newsite)
            rbnd = lbnd.right

            # if this halfedge has no edge, bot = bottom site (whatever that is)
            # create a new edge that bisects
            bot = lbnd.rightreg(bottomsite)
            edge = Edge.bisect(bot, newsite)
            context.outBisector(edge)

            # create a new Halfedge, setting its pm field to 0 and insert
            # this new bisector edge between the left and right vectors in
            # a linked list
            bisector = Halfedge(edge, Edge.LE)
            edgeList.insert(lbnd, bisector)

            # if the new bisector intersects with the left edge, remove
            # the left edge's vertex, and put in the new one
            p = lbnd.intersect(bisector)
            if p is not None:
                priorityQ.delete(lbnd)
                priorityQ.insert(lbnd, p, newsite.distance(p))

            # create a new Halfedge, setting its pm field to 1
            # insert the new Halfedge to the right of the original bisector
            lbnd = bisector
            bisector = Halfedge(edge, Edge.RE)
            edgeList.insert(lbnd, bisector)

            # if this new bisector intersects with the right Halfedge
            p = bisector.intersect(rbnd)
            if p is not None:
                # push the Halfedge into the ordered linked list of vertices
                priorityQ.insert(bisector, p, newsite.distance(p))

            newsite = siteIter.next()

        elif not priorityQ.isEmpty():
            # intersection is smallest - this is a vector (circle) event
            # pop the Halfedge with the lowest vector off the ordered list of
            # vectors.  Get the Halfedge to the left and right of the above HE
            # and also the Halfedge to the right of the right HE
            lbnd = priorityQ.popMinHalfedge()
            llbnd = lbnd.left
            rbnd = lbnd.right
            rrbnd = rbnd.right

            # get the Site to the left of the left HE and to the right of
            # the right HE which it bisects
            bot = lbnd.leftreg(bottomsite)
            top = rbnd.rightreg(bottomsite)

            # output the triple of sites, stating that a circle goes through them
            mid = lbnd.rightreg(bottomsite)
            context.outTriple(bot, top, mid)

            # get the vertex that caused this event and set the vertex number
            # couldn't do this earlier since we didn't know when it would be processed
            v = lbnd.vertex
            siteList.setSiteNumber(v)
            context.outVertex(v)

            # set the endpoint of the left and right Halfedge to be this vector
            if lbnd.edge.setEndpoint(lbnd.pm, v):
                context.outEdge(lbnd.edge)

            if rbnd.edge.setEndpoint(rbnd.pm, v):
                context.outEdge(rbnd.edge)

            # delete the lowest HE, remove all vertex events to do with the
            # right HE and delete the right HE
            edgeList.delete(lbnd)
            priorityQ.delete(rbnd)
            edgeList.delete(rbnd)

            # if the site to the left of the event is higher than the Site
            # to the right of it, then swap them and set 'pm' to RIGHT
            pm = Edge.LE
            if bot.y > top.y:
                bot, top = top, bot
                pm = Edge.RE

            # Create an Edge (or line) that is between the two Sites.  This
            # creates the formula of the line, and assigns a line number to it
            edge = Edge.bisect(bot, top)
            context.outBisector(edge)

            # create a HE from the edge
            bisector = Halfedge(edge, pm)

            # insert the new bisector to the right of the left HE
            # set one endpoint to the new edge to be the vector point 'v'
            # If the site to the left of this bisector is higher than the right
            # Site, then this endpoint is put in position 0; otherwise in pos 1
            edgeList.insert(llbnd, bisector)
            if edge.setEndpoint(Edge.RE - pm, v):
                context.outEdge(edge)

            # if left HE and the new bisector don't intersect, then delete
            # the left HE, and reinsert it
            p = llbnd.intersect(bisector)
            if p is not None:
                priorityQ.delete(llbnd)
                priorityQ.insert(llbnd, p, bot.distance(p))

            # if right HE and the new bisector don't intersect, then reinsert it
            p = bisector.intersect(rrbnd)
            if p is not None:
                priorityQ.insert(bisector, p, bot.distance(p))
        else:
            break

    he = edgeList.leftend.right
    while he is not edgeList.rightend:
        context.outEdge(he.edge)
        he = he.right
    Edge.EDGE_NUM = 0  # CF


def isEqual(a, b, relativeError=TOLERANCE):
    # is nearly equal to within the allowed relative error
    norm = max(abs(a), abs(b))
    return (norm < relativeError) or (abs(a - b) < (relativeError * norm))


class Site(object):

    def __init__(self, x=0.0, y=0.0, sitenum=0):
        self.x = x
        self.y = y
        self.sitenum = sitenum

    def dump(self):
        print("Site #%d (%g, %g)" % (self.sitenum, self.x, self.y))

    def __lt__(self, other):
        if self.y < other.y:
            return True
        elif self.y > other.y:
            return False
        elif self.x < other.x:
            return True
        elif self.x > other.x:
            return False
        else:
            return False

    def __eq__(self, other):
        if self.y == other.y and self.x == other.x:
            return True

    def distance(self, other):
        dx = self.x - other.x
        dy = self.y - other.y
        return math.sqrt(dx * dx + dy * dy)


class Edge(object):
    LE = 0  # left end indice --> edge.ep[Edge.LE]
    RE = 1  # right end indice
    EDGE_NUM = 0
    DELETED = {}  # marker value

    def __init__(self):
        self.a = 0.0  # equation of the line a*x+b*y = c
        self.b = 0.0
        self.c = 0.0
        self.ep = [None, None]  # end point (2 tuples of site)
        self.reg = [None, None]
        self.edgenum = 0

    def dump(self):
        print("(#%d a=%g, b=%g, c=%g)" % (self.edgenum, self.a, self.b, self.c))
        print("ep", self.ep)
        print("reg", self.reg)

    def setEndpoint(self, lrFlag, site):
        self.ep[lrFlag] = site
        if self.ep[Edge.RE - lrFlag] is None:
            return False
        return True

    @staticmethod
    def bisect(s1, s2):
        newedge = Edge()
        newedge.reg[0] = s1  # store the sites that this edge is bisecting
        newedge.reg[1] = s2

        # to begin with, there are no endpoints on the bisector - it goes to infinity
        # ep[0] and ep[1] are None

        # get the difference in x dist between the sites
        dx = float(s2.x - s1.x)
        dy = float(s2.y - s1.y)
        adx = abs(dx)  # make sure that the difference in positive
        ady = abs(dy)

        # get the slope of the line
        newedge.c = float(s1.x * dx + s1.y * dy + (dx * dx + dy * dy) * 0.5)
        if adx > ady:
            # set formula of line, with x fixed to 1
            newedge.a = 1.0
            newedge.b = dy / dx
            newedge.c /= dx
        else:
            # set formula of line, with y fixed to 1
            newedge.b = 1.0
            newedge.a = dx / dy
            newedge.c /= dy

        newedge.edgenum = Edge.EDGE_NUM
        Edge.EDGE_NUM += 1
        return newedge


class Halfedge(object):

    def __init__(self, edge=None, pm=Edge.LE):
        self.left = None    # left Halfedge in the edge list
        self.right = None   # right Halfedge in the edge list
        self.qnext = None   # priority queue linked list pointer
        self.edge = edge    # edge list Edge
        self.pm = pm
        self.vertex = None  # Site()
        self.ystar = BIG_FLOAT

    def dump(self):
        print("Halfedge--------------------------")
        print("left: ", self.left)
        print("right: ", self.right)
        print("edge: ", self.edge)
        print("pm: ", self.pm)
        print("vertex: "),
        if self.vertex:
            self.vertex.dump()
        else:
            print("None")
        print("ystar: ", self.ystar)

    def __lt__(self, other):
        if self.ystar < other.ystar:
            return True
        elif self.ystar > other.ystar:
            return False
        elif self.vertex.x < other.vertex.x:
            return True
        elif self.vertex.x > other.vertex.x:
            return False
        else:
            return False

    def __eq__(self, other):
        if self.ystar == other.ystar and self.vertex.x == other.vertex.x:
            return True

    def leftreg(self, default):
        if not self.edge:
            return default
        elif self.pm == Edge.LE:
            return self.edge.reg[Edge.LE]
        else:
            return self.edge.reg[Edge.RE]

    def rightreg(self, default):
        if not self.edge:
            return default
        elif self.pm == Edge.LE:
            return self.edge.reg[Edge.RE]
        else:
            return self.edge.reg[Edge.LE]

    # returns True if p is to right of halfedge self
    def isPointRightOf(self, pt):
        e = self.edge
        topsite = e.reg[1]
        right_of_site = pt.x > topsite.x

        if(right_of_site and self.pm == Edge.LE):
            return True

        if(not right_of_site and self.pm == Edge.RE):
            return False

        if(e.a == 1.0):
            dyp = pt.y - topsite.y
            dxp = pt.x - topsite.x
            fast = 0
            if ((not right_of_site and e.b < 0.0) or (right_of_site and e.b >= 0.0)):
                above = dyp >= e.b * dxp
                fast = above
            else:
                above = pt.x + pt.y * e.b > e.c
                if(e.b < 0.0):
                    above = not above
                if (not above):
                    fast = 1
            if (not fast):
                dxs = topsite.x - (e.reg[0]).x
                above = e.b * (dxp * dxp - dyp * dyp) < dxs * dyp * (1.0 + 2.0 * dxp / dxs + e.b * e.b)
                if(e.b < 0.0):
                    above = not above
        else:  # e.b == 1.0
            yl = e.c - e.a * pt.x
            t1 = pt.y - yl
            t2 = pt.x - topsite.x
            t3 = yl - topsite.y
            above = t1 * t1 > t2 * t2 + t3 * t3

        if(self.pm == Edge.LE):
            return above
        else:
            return not above

    # create a new site where the Halfedges el1 and el2 intersect
    def intersect(self, other):
        e1 = self.edge
        e2 = other.edge
        if (e1 is None) or (e2 is None):
            return None

        # if the two edges bisect the same parent return None
        if e1.reg[1] is e2.reg[1]:
            return None

        d = e1.a * e2.b - e1.b * e2.a
        if isEqual(d, 0.0):
            return None

        xint = (e1.c * e2.b - e2.c * e1.b) / d
        yint = (e2.c * e1.a - e1.c * e2.a) / d
        if e1.reg[1] < e2.reg[1]:
            he = self
            e = e1
        else:
            he = other
            e = e2

        rightOfSite = xint >= e.reg[1].x
        if((rightOfSite and he.pm == Edge.LE) or
                (not rightOfSite and he.pm == Edge.RE)):
            return None

        # create a new site at the point of intersection - this is a new
        # vector event waiting to happen
        return Site(xint, yint)


class EdgeList(object):

    def __init__(self, xmin, xmax, nsites):
        if xmin > xmax:
            xmin, xmax = xmax, xmin
        self.hashsize = int(2 * math.sqrt(nsites + 4))

        self.xmin = xmin
        self.deltax = float(xmax - xmin)
        self.hash = [None] * self.hashsize

        self.leftend = Halfedge()
        self.rightend = Halfedge()
        self.leftend.right = self.rightend
        self.rightend.left = self.leftend
        self.hash[0] = self.leftend
        self.hash[-1] = self.rightend

    def insert(self, left, he):
        he.left = left
        he.right = left.right
        left.right.left = he
        left.right = he

    def delete(self, he):
        he.left.right = he.right
        he.right.left = he.left
        he.edge = Edge.DELETED

    # Get entry from hash table, pruning any deleted nodes
    def gethash(self, b):
        if(b < 0 or b >= self.hashsize):
            return None
        he = self.hash[b]
        if he is None or he.edge is not Edge.DELETED:
            return he

        #  Hash table points to deleted half edge.  Patch as necessary.
        self.hash[b] = None
        return None

    def leftbnd(self, pt):
        # Use hash table to get close to desired halfedge
        bucket = int(((pt.x - self.xmin) / self.deltax * self.hashsize))

        if(bucket < 0):
            bucket = 0

        if(bucket >= self.hashsize):
            bucket = self.hashsize - 1

        he = self.gethash(bucket)
        if(he is None):
            i = 1
            while True:
                he = self.gethash(bucket - i)
                if (he is not None):
                    break
                he = self.gethash(bucket + i)
                if (he is not None):
                    break
                i += 1

        # Now search linear list of halfedges for the corect one
        if (he is self.leftend) or (he is not self.rightend and he.isPointRightOf(pt)):
            he = he.right
            while he is not self.rightend and he.isPointRightOf(pt):
                he = he.right
            he = he.left
        else:
            he = he.left
            while (he is not self.leftend and not he.isPointRightOf(pt)):
                he = he.left

        # Update hash table and reference counts
        if(bucket > 0 and bucket < self.hashsize - 1):
            self.hash[bucket] = he
        return he


class PriorityQueue(object):

    def __init__(self, ymin, ymax, nsites):
        self.ymin = ymin
        self.deltay = ymax - ymin
        self.hashsize = int(4 * math.sqrt(nsites))
        self.count = 0
        self.minidx = 0
        self.hash = []
        for i in range(self.hashsize):
            self.hash.append(Halfedge())

    def __len__(self):
        return self.count

    def isEmpty(self):
        return self.count == 0

    def insert(self, he, site, offset):
        he.vertex = site
        he.ystar = site.y + offset
        last = self.hash[self.getBucket(he)]
        next = last.qnext
        while((next is not None) and he > next):
            last = next
            next = last.qnext
        he.qnext = last.qnext
        last.qnext = he
        self.count += 1

    def delete(self, he):
        if (he.vertex is not None):
            last = self.hash[self.getBucket(he)]
            while last.qnext is not he:
                last = last.qnext
            last.qnext = he.qnext
            self.count -= 1
            he.vertex = None

    def getBucket(self, he):
        bucket = int(((he.ystar - self.ymin) / self.deltay) * self.hashsize)
        if bucket < 0:
            bucket = 0
        if bucket >= self.hashsize:
            bucket = self.hashsize - 1
        if bucket < self.minidx:
            self.minidx = bucket
        return bucket

    def getMinPt(self):
        while(self.hash[self.minidx].qnext is None):
            self.minidx += 1
        he = self.hash[self.minidx].qnext
        x = he.vertex.x
        y = he.ystar
        return Site(x, y)

    def popMinHalfedge(self):
        curr = self.hash[self.minidx].qnext
        self.hash[self.minidx].qnext = curr.qnext
        self.count -= 1
        return curr


class SiteList(object):

    def __init__(self, pointList):
        self.__sites = []
        self.__sitenum = 0

        self.__xmin = min([pt.x for pt in pointList])
        self.__ymin = min([pt.y for pt in pointList])
        self.__xmax = max([pt.x for pt in pointList])
        self.__ymax = max([pt.y for pt in pointList])
        self.__extent = (self.__xmin, self.__xmax, self.__ymin, self.__ymax)

        for i, pt in enumerate(pointList):
            self.__sites.append(Site(pt.x, pt.y, i))
        self.__sites.sort()

    def setSiteNumber(self, site):
        site.sitenum = self.__sitenum
        self.__sitenum += 1

    class Iterator(object):

        def __init__(this, lst):
            this.generator = (s for s in lst)

        def __iter__(this):
            return this

        def next(this):
            try:
                # Note: Blender is Python 3.x so no need for 2.x checks
                return this.generator.__next__()
            except StopIteration:
                return None

    def iterator(self):
        return SiteList.Iterator(self.__sites)

    def __iter__(self):
        return SiteList.Iterator(self.__sites)

    def __len__(self):
        return len(self.__sites)

    def _getxmin(self):
        return self.__xmin

    def _getymin(self):
        return self.__ymin

    def _getxmax(self):
        return self.__xmax

    def _getymax(self):
        return self.__ymax

    def _getextent(self):
        return self.__extent

    xmin = property(_getxmin)
    ymin = property(_getymin)
    xmax = property(_getxmax)
    ymax = property(_getymax)
    extent = property(_getextent)


def computeVoronoiDiagram(points, xBuff=0, yBuff=0, polygonsOutput=False,
                          formatOutput=False, closePoly=True):
    """
    Takes :
    - a list of point objects (which must have x and y fields).
    - x and y buffer values which are the expansion percentages of the bounding box
        rectangle including all input points.
    Returns :
    - With default options :
      A list of 2-tuples, representing the two points of each Voronoi diagram edge.
      Each point contains 2-tuples which are the x,y coordinates of point.
      if formatOutput is True, returns :
                    - a list of 2-tuples, which are the x,y coordinates of the Voronoi diagram vertices.
                    - and a list of 2-tuples (v1, v2) representing edges of the Voronoi diagram.
                      v1 and v2 are the indices of the vertices at the end of the edge.
    - If polygonsOutput option is True, returns :
      A dictionary of polygons, keys are the indices of the input points,
      values contains n-tuples representing the n points of each Voronoi diagram polygon.
      Each point contains 2-tuples which are the x,y coordinates of point.
      if formatOutput is True, returns :
                    - A list of 2-tuples, which are the x,y coordinates of the Voronoi diagram vertices.
                    - and a dictionary of input points indices. Values contains n-tuples representing
                      the n points of each Voronoi diagram polygon.
                      Each tuple contains the vertex indices of the polygon vertices.
    - if closePoly is True then, in the list of points of a polygon, last point will be the same of first point
    """
    siteList = SiteList(points)
    context = Context()
    voronoi(siteList, context)
    context.setClipBuffer(xBuff, yBuff)
    if not polygonsOutput:
        clipEdges = context.getClipEdges()
        if formatOutput:
            vertices, edgesIdx = formatEdgesOutput(clipEdges)
            return vertices, edgesIdx
        else:
            return clipEdges
    else:
        clipPolygons = context.getClipPolygons(closePoly)
        if formatOutput:
            vertices, polyIdx = formatPolygonsOutput(clipPolygons)
            return vertices, polyIdx
        else:
            return clipPolygons


def formatEdgesOutput(edges):
    # get list of points
    pts = []
    for edge in edges:
        pts.extend(edge)
    # get unique values
    pts = set(pts)  # unique values (tuples are hashable)
    # get dict {values:index}
    valuesIdxDict = dict(zip(pts, range(len(pts))))
    # get edges index reference
    edgesIdx = []
    for edge in edges:
        edgesIdx.append([valuesIdxDict[pt] for pt in edge])
    return list(pts), edgesIdx


def formatPolygonsOutput(polygons):
    # get list of points
    pts = []
    for poly in polygons.values():
        pts.extend(poly)
    # get unique values
    pts = set(pts)  # unique values (tuples are hashable)
    # get dict {values:index}
    valuesIdxDict = dict(zip(pts, range(len(pts))))
    # get polygons index reference
    polygonsIdx = {}
    for inPtsIdx, poly in polygons.items():
        polygonsIdx[inPtsIdx] = [valuesIdxDict[pt] for pt in poly]
    return list(pts), polygonsIdx


def computeDelaunayTriangulation(points):
    """ Takes a list of point objects (which must have x and y fields).
            Returns a list of 3-tuples: the indices of the points that form a
            Delaunay triangle.
    """
    siteList = SiteList(points)
    context = Context()
    context.triangulate = True
    voronoi(siteList, context)
    return context.triangles
