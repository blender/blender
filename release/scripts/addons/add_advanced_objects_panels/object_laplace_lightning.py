# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# NOTE: moved the winmgr properties to __init__ and scene
# search for context.scene.advanced_objects1

bl_info = {
    "name": "Laplacian Lightning",
    "author": "teldredge",
    "blender": (2, 78, 0),
    "location": "3D View > Toolshelf > Create > Laplacian Lightning",
    "description": "Lightning mesh generator using laplacian growth algorithm",
    "warning": "",
    "category": "Object"}

# BLENDER LAPLACIAN LIGHTNING
# teldredge
# www.funkboxing.com
# https://developer.blender.org/T27189

# using algorithm from
# FAST SIMULATION OF LAPLACIAN GROWTH (FSLG)
# http://gamma.cs.unc.edu/FRAC/

# and a few ideas ideas from
# FAST ANIMATION OF LIGHTNING USING AN ADAPTIVE MESH (FALUAM)
# http://gamma.cs.unc.edu/FAST_LIGHTNING/


"""
----- RELEASE LOG/NOTES/PONTIFICATIONS -----
v0.1.0 - 04.11.11
    basic generate functions and UI
    object creation report (Custom Properties: FSLG_REPORT)
v0.2.0 - 04.15.11
    started spelling laplacian right.
    add curve function (not in UI) ...twisting problem
    classify stroke by MAIN path, h-ORDER paths, TIP paths
    jitter cells for mesh creation
    add materials if present
v0.2.1 - 04.16.11
    mesh classification speedup
v0.2.2 - 04.21.11
    fxns to write/read array to file
    restrict growth to insulator cells (object bounding box)
    origin/ground defineable by object
    gridunit more like 'resolution'
v0.2.3 - 04.24.11
    cloud attractor object (termintates loop if hit)
    secondary path orders (hOrder) disabled in UI (set to 1)
v0.2.4 - 04.26.11
    fixed object selection in UI
    will not run if required object not selected
    moved to view 3d > toolbox
v0.2.5 - 05.08.11
    testing for 2.57b
    single mesh output (for build modifier)
    speedups (dist fxn)
v0.2.6 - 06.20.11
    scale/pos on 'write to cubes' works now
    if origin obj is mesh, uses all verts as initial charges
    semi-helpful tooltips
    speedups, faster dedupe fxn, faster classification
    use any shape mesh obj as insulator mesh
        must have rot=0, scale=1, origin set to geometry
        often fails to block bolt with curved/complex shapes
    separate single and multi mesh creation
v0.2.7 - 01.05.13
    fixed the issue that prevented enabling the add-on
    fixed makeMeshCube fxn
    disabled visualization for voxels

v0.x -
    -prevent create_setup_objects from generating duplicates
    -fix vis fxn to only buildCPGraph once for VM or VS
    -improve list fxns (rid of ((x,y,z),w) and use (x,y,z,w)), use 'sets'
    -create python cmodule for a few of most costly fxns
        i have pretty much no idea how to do this yet
    -cloud and insulator can be groups of MESH objs
    -text output, possibly to save on interrupt, allow continue from text
    -?hook modifiers from tips->sides->main, weight w/ vert groups
    -user defined 'attractor' path
    -fix add curve function
    -animated arcs via. ionization path
    -environment map boundary conditions - requires Eqn. 15 from FSLG.
    -assign wattage at each segment for HDRI
    -?default settings for -lightning, -teslacoil, -spark/arc
    -fix hOrder functionality
    -multiple 'MAIN' brances for non-lightning discharges
    -n-symmetry option, create mirror images, snowflakes, etc...
"""

import bpy
import time
import random
from bpy.types import (
        Operator,
        Panel,
        )
# from math import sqrt
from mathutils import Vector
import struct
import bisect
import os.path

# -- Globals --
notZero = 0.0000000001
# set to True to enable debug prints
DEBUG = False


# Utility Functions

# func - function name, text - message, var - variable to print
# it can have one variable to observe
def debug_prints(func="", text="Message", var=None):
    global DEBUG
    if DEBUG:
        print("\n[{}]\nmessage: {}".format(func, text))
        if var:
            print("variable: ", var)


# pass variables just like for the regular prints
def debug_print_vars(*args, **kwargs):
    global DEBUG
    if DEBUG:
        print(*args, **kwargs)


def within(x, y, d):
    # CHECK IF x - d <= y <= x + d
    if x - d <= y and x + d >= y:
        return True
    else:
        return False


def dist(ax, ay, az, bx, by, bz):
    dv = Vector((ax, ay, az)) - Vector((bx, by, bz))
    d = dv.length
    return d


def splitList(aList, idx):
    ll = []
    for x in aList:
        ll.append(x[idx])
    return ll


def splitListCo(aList):
    ll = []
    for p in aList:
        ll.append((p[0], p[1], p[2]))
    return ll


def getLowHigh(aList):
    tLow = aList[0]
    tHigh = aList[0]
    for a in aList:
        if a < tLow:
            tLow = a
        if a > tHigh:
            tHigh = a
    return tLow, tHigh


def weightedRandomChoice(aList):
    tL = []
    tweight = 0
    for a in range(len(aList)):
        idex = a
        weight = aList[a]
        if weight > 0.0:
            tweight += weight
            tL.append((tweight, idex))
    i = bisect.bisect(tL, (random.uniform(0, tweight), None))
    r = tL[i][1]
    return r


def getStencil3D_26(x, y, z):
    nL = []
    for xT in range(x - 1, x + 2):
        for yT in range(y - 1, y + 2):
            for zT in range(z - 1, z + 2):
                nL.append((xT, yT, zT))
    nL.remove((x, y, z))
    return nL


def jitterCells(aList, jit):
    j = jit / 2
    bList = []
    for a in aList:
        ax = a[0] + random.uniform(-j, j)
        ay = a[1] + random.uniform(-j, j)
        az = a[2] + random.uniform(-j, j)
        bList.append((ax, ay, az))
    return bList


def deDupe(seq, idfun=None):
    # Thanks to this guy - http://www.peterbe.com/plog/uniqifiers-benchmark
    if idfun is None:
        def idfun(x):
            return x
    seen = {}
    result = []
    for item in seq:
        marker = idfun(item)
        if marker in seen:
            continue
        seen[marker] = 1
        result.append(item)
    return result


# Visulization functions

def writeArrayToVoxel(arr, filename):
    gridS = 64
    half = int(gridS / 2)
    bitOn = 255
    aGrid = [[[0 for z in range(gridS)] for y in range(gridS)] for x in range(gridS)]
    for a in arr:
        try:
            aGrid[a[0] + half][a[1] + half][a[2] + half] = bitOn
        except:
            debug_prints(func="writeArrayToVoxel", text="Particle beyond voxel domain")

    file = open(filename, "wb")
    for z in range(gridS):
        for y in range(gridS):
            for x in range(gridS):
                file.write(struct.pack('B', aGrid[x][y][z]))
    file.flush()
    file.close()


def writeArrayToFile(arr, filename):
    file = open(filename, "w")
    for a in arr:
        tstr = str(a[0]) + ',' + str(a[1]) + ',' + str(a[2]) + '\n'
        file.write(tstr)
    file.close


def readArrayFromFile(filename):
    file = open(filename, "r")
    arr = []
    for f in file:
        pt = f[0:-1].split(',')
        arr.append((int(pt[0]), int(pt[1]), int(pt[2])))
    return arr


def makeMeshCube_OLD(msize):
    msize = msize / 2
    mmesh = bpy.data.meshes.new('q')
    mmesh.vertices.add(8)
    mmesh.vertices[0].co = [-msize, -msize, -msize]
    mmesh.vertices[1].co = [-msize, msize, -msize]
    mmesh.vertices[2].co = [msize, msize, -msize]
    mmesh.vertices[3].co = [msize, -msize, -msize]
    mmesh.vertices[4].co = [-msize, -msize, msize]
    mmesh.vertices[5].co = [-msize, msize, msize]
    mmesh.vertices[6].co = [msize, msize, msize]
    mmesh.vertices[7].co = [msize, -msize, msize]
    mmesh.faces.add(6)
    mmesh.faces[0].vertices_raw = [0, 1, 2, 3]
    mmesh.faces[1].vertices_raw = [0, 4, 5, 1]
    mmesh.faces[2].vertices_raw = [2, 1, 5, 6]
    mmesh.faces[3].vertices_raw = [3, 2, 6, 7]
    mmesh.faces[4].vertices_raw = [0, 3, 7, 4]
    mmesh.faces[5].vertices_raw = [5, 4, 7, 6]
    mmesh.update(calc_edges=True)

    return(mmesh)


def makeMeshCube(msize):
    m2 = msize / 2
    # verts = [(0,0,0),(0,5,0),(5,5,0),(5,0,0),(0,0,5),(0,5,5),(5,5,5),(5,0,5)]
    verts = [(-m2, -m2, -m2), (-m2, m2, -m2), (m2, m2, -m2), (m2, -m2, -m2),
             (-m2, -m2, m2), (-m2, m2, m2), (m2, m2, m2), (m2, -m2, m2)]
    faces = [
        (0, 1, 2, 3), (4, 5, 6, 7), (0, 4, 5, 1),
        (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)
        ]
    # Define mesh and object
    mmesh = bpy.data.meshes.new("Cube")

    # Create mesh
    mmesh.from_pydata(verts, [], faces)
    mmesh.update(calc_edges=True)
    return(mmesh)


def writeArrayToCubes(arr, gridBU, orig, cBOOL=False, jBOOL=True):
    for a in arr:
        x = a[0]
        y = a[1]
        z = a[2]
        me = makeMeshCube(gridBU)
        ob = bpy.data.objects.new('xCUBE', me)
        ob.location.x = (x * gridBU) + orig[0]
        ob.location.y = (y * gridBU) + orig[1]
        ob.location.z = (z * gridBU) + orig[2]

        if cBOOL:  # mostly unused
            # pos + blue, neg - red, zero: black
            col = (1.0, 1.0, 1.0, 1.0)
            if a[3] == 0:
                col = (0.0, 0.0, 0.0, 1.0)
            if a[3] < 0:
                col = (-a[3], 0.0, 0.0, 1.0)
            if a[3] > 0:
                col = (0.0, 0.0, a[3], 1.0)
            ob.color = col
        bpy.context.scene.objects.link(ob)
        bpy.context.scene.update()

    if jBOOL:
        # Selects all cubes w/ ?bpy.ops.object.join() b/c
        # Can't join all cubes to a single mesh right... argh...
        for q in bpy.context.scene.objects:
            q.select = False
            if q.name[0:5] == 'xCUBE':
                q.select = True
                bpy.context.scene.objects.active = q


def addVert(ob, pt, conni=-1):
    mmesh = ob.data
    mmesh.vertices.add(1)
    vcounti = len(mmesh.vertices) - 1
    mmesh.vertices[vcounti].co = [pt[0], pt[1], pt[2]]
    if conni > -1:
        mmesh.edges.add(1)
        ecounti = len(mmesh.edges) - 1
        mmesh.edges[ecounti].vertices = [conni, vcounti]
        mmesh.update()


def addEdge(ob, va, vb):
    mmesh = ob.data
    mmesh.edges.add(1)
    ecounti = len(mmesh.edges) - 1
    mmesh.edges[ecounti].vertices = [va, vb]
    mmesh.update()


def newMesh(mname):
    mmesh = bpy.data.meshes.new(mname)
    omesh = bpy.data.objects.new(mname, mmesh)
    bpy.context.scene.objects.link(omesh)
    return omesh


def writeArrayToMesh(mname, arr, gridBU, rpt=None):
    mob = newMesh(mname)
    mob.scale = (gridBU, gridBU, gridBU)
    if rpt:
        addReportProp(mob, rpt)
    addVert(mob, arr[0], -1)
    for ai in range(1, len(arr)):
        a = arr[ai]
        addVert(mob, a, ai - 1)
    return mob


# out of order - some problem with it adding (0,0,0)
def writeArrayToCurves(cname, arr, gridBU, bd=.05, rpt=None):
    cur = bpy.data.curves.new('fslg_curve', 'CURVE')
    cur.use_fill_front = False
    cur.use_fill_back = False
    cur.bevel_depth = bd
    cur.bevel_resolution = 2
    cob = bpy.data.objects.new(cname, cur)
    cob.scale = (gridBU, gridBU, gridBU)

    if rpt:
        addReportProp(cob, rpt)
    bpy.context.scene.objects.link(cob)
    cur.splines.new('BEZIER')
    cspline = cur.splines[0]
    div = 1  # spacing for handles (2 - 1/2 way, 1 - next bezier)

    for a in range(len(arr)):
        cspline.bezier_points.add(1)
        bp = cspline.bezier_points[len(cspline.bezier_points) - 1]
        if a - 1 < 0:
            hL = arr[a]
        else:
            hx = arr[a][0] - ((arr[a][0] - arr[a - 1][0]) / div)
            hy = arr[a][1] - ((arr[a][1] - arr[a - 1][1]) / div)
            hz = arr[a][2] - ((arr[a][2] - arr[a - 1][2]) / div)
            hL = (hx, hy, hz)

        if a + 1 > len(arr) - 1:
            hR = arr[a]
        else:
            hx = arr[a][0] + ((arr[a + 1][0] - arr[a][0]) / div)
            hy = arr[a][1] + ((arr[a + 1][1] - arr[a][1]) / div)
            hz = arr[a][2] + ((arr[a + 1][2] - arr[a][2]) / div)
            hR = (hx, hy, hz)
        bp.co = arr[a]
        bp.handle_left = hL
        bp.handle_right = hR


def addArrayToMesh(mob, arr):
    addVert(mob, arr[0], -1)
    mmesh = mob.data
    vcounti = len(mmesh.vertices) - 1
    for ai in range(1, len(arr)):
        a = arr[ai]
        addVert(mob, a, len(mmesh.vertices) - 1)


def addMaterial(ob, matname):
    mat = bpy.data.materials[matname]
    ob.active_material = mat


def writeStokeToMesh(arr, jarr, MAINi, HORDERi, TIPSi, orig, gs, rpt=None):
    # main branch
    debug_prints(func="writeStokeToMesh", text='Writing main branch')
    llmain = []

    for x in MAINi:
        llmain.append(jarr[x])
    mob = writeArrayToMesh('la0MAIN', llmain, gs)
    mob.location = orig

    # horder branches
    for hOi in range(len(HORDERi)):
        debug_prints(func="writeStokeToMesh", text="Writing order", var=hOi)
        hO = HORDERi[hOi]
        hob = newMesh('la1H' + str(hOi))

        for y in hO:
            llHO = []
            for x in y:
                llHO.append(jarr[x])
            addArrayToMesh(hob, llHO)
        hob.scale = (gs, gs, gs)
        hob.location = orig

    # tips
    debug_prints(func="writeStokeToMesh", text="Writing tip paths")
    tob = newMesh('la2TIPS')
    for y in TIPSi:
        llt = []
        for x in y:
            llt.append(jarr[x])
        addArrayToMesh(tob, llt)
    tob.scale = (gs, gs, gs)
    tob.location = orig

    # add materials to objects (if they exist)
    try:
        addMaterial(mob, 'edgeMAT-h0')
        addMaterial(hob, 'edgeMAT-h1')
        addMaterial(tob, 'edgeMAT-h2')
        debug_prints(func="writeStokeToMesh", text="Added materials")

    except:
        debug_prints(func="writeStokeToMesh", text="Materials not found")

    # add generation report to all meshes
    if rpt:
        addReportProp(mob, rpt)
        addReportProp(hob, rpt)
        addReportProp(tob, rpt)


def writeStokeToSingleMesh(arr, jarr, orig, gs, mct, rpt=None):
    sgarr = buildCPGraph(arr, mct)
    llALL = []

    Aob = newMesh('laALL')
    for pt in jarr:
        addVert(Aob, pt)
    for cpi in range(len(sgarr)):
        ci = sgarr[cpi][0]
        pi = sgarr[cpi][1]
        addEdge(Aob, pi, ci)
    Aob.location = orig
    Aob.scale = ((gs, gs, gs))

    if rpt:
        addReportProp(Aob, rpt)


def visualizeArray(cg, oob, gs, vm, vs, vc, vv, rst):
    winmgr = bpy.context.scene.advanced_objects1
    # IN: (cellgrid, origin, gridscale,
    # mulimesh, single mesh, cubes, voxels, report sting)
    origin = oob.location

    # deal with vert multi-origins
    oct = 2
    if oob.type == 'MESH':
        oct = len(oob.data.vertices)

    # jitter cells
    if vm or vs:
        cjarr = jitterCells(cg, 1)

    if vm:  # write array to multi mesh

        aMi, aHi, aTi = classifyStroke(cg, oct, winmgr.HORDER)
        debug_prints(func="visualizeArray", text="Writing to multi-mesh")
        writeStokeToMesh(cg, cjarr, aMi, aHi, aTi, origin, gs, rst)
        debug_prints(func="visualizeArray", text="Multi-mesh written")

    if vs:  # write to single mesh
        debug_prints(func="visualizeArray", text="Writing to single mesh")
        writeStokeToSingleMesh(cg, cjarr, origin, gs, oct, rst)
        debug_prints(func="visualizeArray", text="Single mesh written")

    if vc:  # write array to cube objects
        debug_prints(func="visualizeArray", text="Writing to cubes")
        writeArrayToCubes(cg, gs, origin)
        debug_prints(func="visualizeArray", text="Cubes written")

    if vv:  # write array to voxel data file
        debug_prints(func="visualizeArray", text="Writing to voxels")
        fname = "FSLGvoxels.raw"
        path = os.path.dirname(bpy.data.filepath)
        writeArrayToVoxel(cg, path + "\\" + fname)

        debug_prints(func="visualizeArray",
                     text="Voxel data written to:", var=path + "\\" + fname)

    # read/write array to file (might not be necessary)
    # tfile = 'c:\\testarr.txt'
    # writeArrayToFile(cg, tfile)
    # cg = readArrayFromFile(tfile)

    # read/write array to curves (out of order)
    # writeArrayToCurves('laMAIN', llmain, .10, .25)


# Algorithm functions
# from faluam paper
# plus some stuff i made up

def buildCPGraph(arr, sti=2):
    # in -xyz array as built by generator
    # out -[(childindex, parentindex)]
    # sti - start index, 2 for empty, len(me.vertices) for mesh
    sgarr = []
    sgarr.append((1, 0))

    for ai in range(sti, len(arr)):
        cs = arr[ai]
        cpts = arr[0:ai]
        cslap = getStencil3D_26(cs[0], cs[1], cs[2])

        for nc in cslap:
            ct = cpts.count(nc)
            if ct > 0:
                cti = cpts.index(nc)
        sgarr.append((ai, cti))

    return sgarr


def buildCPGraph_WORKINPROGRESS(arr, sti=2):
    # in -xyz array as built by generator
    # out -[(childindex, parentindex)]
    # sti - start index, 2 for empty, len(me.vertices) for mesh
    sgarr = []
    sgarr.append((1, 0))
    ctix = 0
    for ai in range(sti, len(arr)):
        cs = arr[ai]
        # cpts = arr[0:ai]
        cpts = arr[ctix:ai]
        cslap = getStencil3D_26(cs[0], cs[1], cs[2])
        for nc in cslap:
            ct = cpts.count(nc)
            if ct > 0:
                # cti = cpts.index(nc)
                cti = ctix + cpts.index(nc)
                ctix = cpts.index(nc)

        sgarr.append((ai, cti))

    return sgarr


def findChargePath(oc, fc, ngraph, restrict=[], partial=True):
    # oc -origin charge index, fc -final charge index
    # ngraph -node graph, restrict- index of sites cannot traverse
    # partial -return partial path if restriction encounterd
    cList = splitList(ngraph, 0)
    pList = splitList(ngraph, 1)
    aRi = []
    cNODE = fc
    for x in range(len(ngraph)):
        pNODE = pList[cList.index(cNODE)]
        aRi.append(cNODE)
        cNODE = pNODE
        npNODECOUNT = cList.count(pNODE)
        if cNODE == oc:             # stop if origin found
            aRi.append(cNODE)       # return path
            return aRi
        if npNODECOUNT == 0:        # stop if no parents
            return []               # return []
        if pNODE in restrict:       # stop if parent is in restriction
            if partial:             # return partial or []
                aRi.append(cNODE)
                return aRi
            else:
                return []


def findTips(arr):
    lt = []
    for ai in arr[0: len(arr) - 1]:
        a = ai[0]
        cCOUNT = 0
        for bi in arr:
            b = bi[1]
            if a == b:
                cCOUNT += 1
        if cCOUNT == 0:
            lt.append(a)

    return lt


def findChannelRoots(path, ngraph, restrict=[]):
    roots = []
    for ai in range(len(ngraph)):
        chi = ngraph[ai][0]
        par = ngraph[ai][1]
        if par in path and chi not in path and chi not in restrict:
            roots.append(par)
    droots = deDupe(roots)

    return droots


def findChannels(roots, tips, ngraph, restrict):
    cPATHS = []
    for ri in range(len(roots)):
        r = roots[ri]
        sL = 1
        sPATHi = []
        for ti in range(len(tips)):
            t = tips[ti]
            if t < r:
                continue
            tPATHi = findChargePath(r, t, ngraph, restrict, False)
            tL = len(tPATHi)
            if tL > sL:
                if countChildrenOnPath(tPATHi, ngraph) > 1:
                    sL = tL
                    sPATHi = tPATHi
                    tTEMP = t
                    tiTEMP = ti
        if len(sPATHi) > 0:
            debug_print_vars(
                    "\n[findChannels]\n",
                    "found path/idex from", ri, 'of',
                    len(roots), "possible | tips:", tTEMP, tiTEMP
                    )
            cPATHS.append(sPATHi)
            tips.remove(tTEMP)

    return cPATHS


def findChannels_WORKINPROGRESS(roots, ttips, ngraph, restrict):
    cPATHS = []
    tips = list(ttips)
    for ri in range(len(roots)):
        r = roots[ri]
        sL = 1
        sPATHi = []
        tipREMOVE = []  # checked tip indexes, to be removed for next loop
        for ti in range(len(tips)):
            t = tips[ti]
            if ti < ri:
                continue

            tPATHi = findChargePath(r, t, ngraph, restrict, False)
            tL = len(tPATHi)
            if tL > sL:
                if countChildrenOnPath(tPATHi, ngraph) > 1:
                    sL = tL
                    sPATHi = tPATHi
                    tTEMP = t
                    tiTEMP = ti
            if tL > 0:
                tipREMOVE.append(t)
        if len(sPATHi) > 0:
            debug_print_vars(
                    "\n[findChannels_WORKINPROGRESS]\n",
                    "found path from root idex", ri, 'of',
                    len(roots), "possible roots | of tips= ", len(tips)
                    )
            cPATHS.append(sPATHi)

        for q in tipREMOVE:
            tips.remove(q)

    return cPATHS


def countChildrenOnPath(aPath, ngraph, quick=True):
    # return how many branches
    # count when node is a parent >1 times
    # quick -stop and return after first
    cCOUNT = 0
    pList = splitList(ngraph, 1)

    for ai in range(len(aPath) - 1):
        ap = aPath[ai]
        pc = pList.count(ap)

        if quick and pc > 1:
            return pc

    return cCOUNT


# classify channels into 'main', 'hORDER/secondary' and 'side'
def classifyStroke(sarr, mct, hORDER=1):
    debug_prints(func="classifyStroke", text="Classifying stroke")
    # build child/parent graph (indexes of sarr)
    sgarr = buildCPGraph(sarr, mct)

    # find main channel
    debug_prints(func="classifyStroke", text="Finding MAIN")
    oCharge = sgarr[0][1]
    fCharge = sgarr[len(sgarr) - 1][0]
    aMAINi = findChargePath(oCharge, fCharge, sgarr)

    # find tips
    debug_prints(func="classifyStroke", text="Finding TIPS")
    aTIPSi = findTips(sgarr)

    # find horder channel roots
    # hcount = orders bewteen main and side/tips
    # !!!still buggy!!!
    hRESTRICT = list(aMAINi)    # add to this after each time
    allHPATHSi = []             # all ho paths: [[h0], [h1]...]
    curPATHSi = [aMAINi]        # list of paths find roots on

    for h in range(hORDER):
        allHPATHSi.append([])
        for pi in range(len(curPATHSi)):     # loop through all paths in this order
            p = curPATHSi[pi]
            # get roots for this path
            aHROOTSi = findChannelRoots(p, sgarr, hRESTRICT)
            debug_print_vars(
                    "\n[classifyStroke]\n",
                    "found", len(aHROOTSi), "roots in ORDER", h, ":paths:", len(curPATHSi)
                    )
            # get channels for these roots
            if len(aHROOTSi) == 0:
                debug_prints(func="classifyStroke", text="No roots for found for channel")
                aHPATHSi = []
                continue
            else:
                aHPATHSiD = findChannels(aHROOTSi, aTIPSi, sgarr, hRESTRICT)
                aHPATHSi = aHPATHSiD
                allHPATHSi[h] += aHPATHSi
                # set these channels as restrictions for next iterations
                for hri in aHPATHSi:
                    hRESTRICT += hri
        curPATHSi = aHPATHSi

    # side branches, final order of heirarchy
    # from tips that are not in an existing path
    # back to any other point that is already on a path
    aDRAWNi = []
    aDRAWNi += aMAINi
    for oH in allHPATHSi:
        for o in oH:
            aDRAWNi += o
    aTPATHSi = []
    for a in aTIPSi:
        if a not in aDRAWNi:
            aPATHi = findChargePath(oCharge, a, sgarr, aDRAWNi)
            aDRAWNi += aPATHi
            aTPATHSi.append(aPATHi)

    return aMAINi, allHPATHSi, aTPATHSi


def voxelByVertex(ob, gs):
    # 'voxelizes' verts in a mesh to list [(x,y,z),(x,y,z)]
    # w/ respect gscale and ob origin (b/c should be origin obj)
    # orig = ob.location
    ll = []
    for v in ob.data.vertices:
        x = int(v.co.x / gs)
        y = int(v.co.y / gs)
        z = int(v.co.z / gs)
        ll.append((x, y, z))

    return ll


def voxelByRays(ob, orig, gs):
    # mesh into a 3dgrid w/ respect gscale and bolt origin
    # - does not take object rotation/scale into account
    # - this is a horrible, inefficient function
    # maybe the raycast/grid thing are a bad idea. but i
    # have to 'voxelize the object w/ resct to gscale/origin
    bbox = ob.bound_box
    bbxL = bbox[0][0]
    bbxR = bbox[4][0]
    bbyL = bbox[0][1]
    bbyR = bbox[2][1]
    bbzL = bbox[0][2]
    bbzR = bbox[1][2]
    xct = int((bbxR - bbxL) / gs)
    yct = int((bbyR - bbyL) / gs)
    zct = int((bbzR - bbzL) / gs)
    xs = int(xct / 2)
    ys = int(yct / 2)
    zs = int(zct / 2)

    debug_print_vars(
            "\n[voxelByRays]\n",
            "Casting", xct, '/', yct, '/', zct, 'cells, total:',
            xct * yct * zct, 'in obj-', ob.name
            )
    ll = []
    rc = 100    # distance to cast from
    # raycast top/bottom
    debug_prints(func="voxelByRays", text="Raycasting top/bottom")

    for x in range(xct):
        for y in range(yct):
            xco = bbxL + (x * gs)
            yco = bbyL + (y * gs)
            v1 = ((xco, yco, rc))
            v2 = ((xco, yco, -rc))
            vz1 = ob.ray_cast(v1, v2)
            vz2 = ob.ray_cast(v2, v1)

            debug_print_vars(
                        "\n[voxelByRays]\n", "vz1 is: ", vz1, "\nvz2 is: ", vz2
                        )
            # Note: the API raycast return has changed now it is
            # (result, location, normal, index) - result is a boolean
            if vz1[0] is True:
                ll.append((x - xs, y - ys, int(vz1[1][2] * (1 / gs))))
            if vz2[0] is True:
                ll.append((x - xs, y - ys, int(vz2[1][2] * (1 / gs))))

    # raycast front/back
    debug_prints(func="voxelByRays", text="Raycasting front/back")

    for x in range(xct):
        for z in range(zct):
            xco = bbxL + (x * gs)
            zco = bbzL + (z * gs)
            v1 = ((xco, rc, zco))
            v2 = ((xco, -rc, zco))
            vy1 = ob.ray_cast(v1, v2)
            vy2 = ob.ray_cast(v2, v1)
            if vy1[0] is True:
                ll.append((x - xs, int(vy1[1][1] * (1 / gs)), z - zs))
            if vy2[0] is True:
                ll.append((x - xs, int(vy2[1][1] * (1 / gs)), z - zs))

    # raycast left/right
    debug_prints(func="voxelByRays", text="Raycasting left/right")

    for y in range(yct):
        for z in range(zct):
            yco = bbyL + (y * gs)
            zco = bbzL + (z * gs)
            v1 = ((rc, yco, zco))
            v2 = ((-rc, yco, zco))
            vx1 = ob.ray_cast(v1, v2)
            vx2 = ob.ray_cast(v2, v1)
            if vx1[0] is True:
                ll.append((int(vx1[1][0] * (1 / gs)), y - ys, z - zs))
            if vx2[0] is True:
                ll.append((int(vx2[1][0] * (1 / gs)), y - ys, z - zs))

    # add in neighbors so bolt wont go through
    nlist = []
    for l in ll:
        nl = getStencil3D_26(l[0], l[1], l[2])
        nlist += nl

    # dedupe
    debug_prints(func="voxelByRays", text="Added neighbors, deduping...")
    rlist = deDupe(ll + nlist)
    qlist = []

    # relocate grid w/ respect gscale and bolt origin
    # !!!need to add in obj rot/scale here somehow...
    od = Vector(
            ((ob.location[0] - orig[0]) / gs,
             (ob.location[1] - orig[1]) / gs,
             (ob.location[2] - orig[2]) / gs)
            )
    for r in rlist:
        qlist.append((r[0] + int(od[0]), r[1] + int(od[1]), r[2] + int(od[2])))

    return qlist


def fakeGroundChargePlane(z, charge):
    eCL = []
    xy = abs(z) / 2
    eCL += [(0, 0, z, charge)]
    eCL += [(xy, 0, z, charge)]
    eCL += [(0, xy, z, charge)]
    eCL += [(-xy, 0, z, charge)]
    eCL += [(0, -xy, z, charge)]

    return eCL


def addCharges(ll, charge):
    # in: ll - [(x,y,z), (x,y,z)], charge - w
    # out clist - [(x,y,z,w), (x,y,z,w)]
    clist = []
    for l in ll:
        clist.append((l[0], l[1], l[2], charge))
    return clist


# algorithm functions #
# from fslg #

def getGrowthProbability_KEEPFORREFERENCE(uN, aList):
    # in: un -user term, clist -candidate sites, olist -candidate site charges
    # out: list of [(xyz), pot, prob]
    cList = splitList(aList, 0)
    oList = splitList(aList, 1)
    Omin, Omax = getLowHigh(oList)
    if Omin == Omax:
        Omax += notZero
        Omin -= notZero
    PdL = []
    E = 0
    E = notZero   # divisor for (fslg - eqn. 12)

    for o in oList:
        Uj = (o - Omin) / (Omax - Omin)  # (fslg - eqn. 13)
        E += pow(Uj, uN)

    for oi in range(len(oList)):
        o = oList[oi]
        Ui = (o - Omin) / (Omax - Omin)
        Pd = (pow(Ui, uN)) / E  # (fslg - eqn. 12)
        PdINT = Pd * 100
        PdL.append(Pd)

    return PdL


# work in progress, trying to speed these up
def fslg_e13(x, min, max, u):
    return pow((x - min) / (max - min), u)


def addit(x, y):
    return x + y


def fslg_e12(x, min, max, u, e):
    return (fslg_e13(x, min, max, u) / e) * 100


def getGrowthProbability(uN, aList):
    # In: uN - user_term, cList - candidate sites, oList - candidate site charges
    # Out: list of prob
    cList = splitList(aList, 0)
    oList = splitList(aList, 1)
    Omin, Omax = getLowHigh(oList)

    if Omin == Omax:
        Omax += notZero
        Omin -= notZero

    PdL = []
    E = notZero
    minL = [Omin for q in range(len(oList))]
    maxL = [Omax for q in range(len(oList))]
    uNL = [uN for q in range(len(oList))]
    E = sum(map(fslg_e13, oList, minL, maxL, uNL))
    EL = [E for q in range(len(oList))]
    mp = map(fslg_e12, oList, minL, maxL, uNL, EL)

    for m in mp:
        PdL.append(m)

    return PdL


def updatePointCharges(p, cList, eList=[]):
    # In: pNew - new growth cell
    # cList - old candidate sites, eList -SAME
    # Out: list of new charge at candidate sites
    r1 = 1 / 2        # (FSLG - Eqn. 10)
    nOiL = []

    for oi in range(len(cList)):
        o = cList[oi][1]
        c = cList[oi][0]
        iOe = 0
        rit = dist(c[0], c[1], c[2], p[0], p[1], p[2])
        iOe += (1 - (r1 / rit))
        Oit = o + iOe
        nOiL.append((c, Oit))

    return nOiL


def initialPointCharges(pList, cList, eList=[]):
    # In: p -CHARGED CELL (XYZ), cList -candidate sites (XYZ, POT, PROB)
    # Out: cList -with potential calculated
    r1 = 1 / 2        # (FSLG - Eqn. 10)
    npList = []

    for p in pList:
        npList.append(((p[0], p[1], p[2]), 1.0))

    for e in eList:
        npList.append(((e[0], e[1], e[2]), e[3]))

    OiL = []
    for i in cList:
        Oi = 0
        for j in npList:
            if i != j[0]:
                rij = dist(i[0], i[1], i[2], j[0][0], j[0][1], j[0][2])
                Oi += (1 - (r1 / rij)) * j[1]  # charge influence
        OiL.append(((i[0], i[1], i[2]), Oi))

    return OiL


def getCandidateSites(aList, iList=[]):
    # In: aList -(X,Y,Z) of charged cell sites, iList - insulator sites
    # Out: candidate list of growth sites [(X,Y,Z)]
    cList = []
    for c in aList:
        tempList = getStencil3D_26(c[0], c[1], c[2])
        for t in tempList:
            if t not in aList and t not in iList:
                cList.append(t)
    ncList = deDupe(cList)

    return ncList


# Setup functions

def setupObjects():
    winmgr = bpy.context.scene.advanced_objects1
    oOB = bpy.data.objects.new('ELorigin', None)
    oOB.location = ((0, 0, 10))
    bpy.context.scene.objects.link(oOB)

    gOB = bpy.data.objects.new('ELground', None)
    gOB.empty_draw_type = 'ARROWS'
    bpy.context.scene.objects.link(gOB)

    cME = makeMeshCube(1)
    cOB = bpy.data.objects.new('ELcloud', cME)
    cOB.location = ((-2, 8, 12))
    cOB.hide_render = True
    bpy.context.scene.objects.link(cOB)

    iME = makeMeshCube(1)
    for v in iME.vertices:
        xyl = 6.5
        zl = .5
        v.co[0] = v.co[0] * xyl
        v.co[1] = v.co[1] * xyl
        v.co[2] = v.co[2] * zl
    iOB = bpy.data.objects.new('ELinsulator', iME)
    iOB.location = ((0, 0, 5))
    iOB.hide_render = True
    bpy.context.scene.objects.link(iOB)

    try:
        winmgr.OOB = 'ELorigin'
        winmgr.GOB = 'ELground'
        winmgr.COB = 'ELcloud'
        winmgr.IOB = 'ELinsulator'
    except:
        pass


def checkSettings():
    check = True
    winmgr = bpy.context.scene.advanced_objects1
    message = ""
    if winmgr.OOB == "":
        message = "Error: no origin object selected"
        check = False

    if winmgr.GROUNDBOOL and winmgr.GOB == "":
        message = "Error: no ground object selected"
        check = False

    if winmgr.CLOUDBOOL and winmgr.COB == "":
        message = "Error: no cloud object selected"
        check = False

    if winmgr.IBOOL and winmgr.IOB == "":
        message = "Error: no insulator object selected"
        check = False

    if check is False:
        debug_prints(func="checkSettings", text=message)

    # return state and the message for the operator report
    return check, message


# Main

def FSLG():
    winmgr = bpy.context.scene.advanced_objects1
    # fast simulation of laplacian growth
    debug_prints(func="FSLG",
                 text="Go go gadget: fast simulation of laplacian growth")
    tc1 = time.clock()
    TSTEPS = winmgr.TSTEPS

    obORIGIN = bpy.context.scene.objects[winmgr.OOB]
    obGROUND = bpy.context.scene.objects[winmgr.GOB]
    winmgr.ORIGIN = obORIGIN.location
    winmgr.GROUNDZ = int((obGROUND.location[2] - winmgr.ORIGIN[2]) / winmgr.GSCALE)

    # 1) insert intial charge(s) point (uses verts if mesh)
    cgrid = [(0, 0, 0)]

    if obORIGIN.type == 'MESH':
        debug_prints(
                func="FSLG",
                text="Origin object is mesh, 'voxelizing' intial charges from verts"
                )
        cgrid = voxelByVertex(obORIGIN, winmgr.GSCALE)

        if winmgr.VMMESH:
            debug_prints(
                func="FSLG",
                text="Cannot classify stroke from vert origins yet, no multi-mesh output"
                )
            winmgr.VMMESH = False
            winmgr.VSMESH = True

    # ground charge cell / insulator lists (echargelist/iclist)
    eChargeList = []
    icList = []
    if winmgr.GROUNDBOOL:
        eChargeList = fakeGroundChargePlane(winmgr.GROUNDZ, winmgr.GROUNDC)

    if winmgr.CLOUDBOOL:
        debug_prints(
                func="FSLG",
                text="'Voxelizing' cloud object (could take some time)"
                )
        obCLOUD = bpy.context.scene.objects[winmgr.COB]
        eChargeListQ = voxelByRays(obCLOUD, winmgr.ORIGIN, winmgr.GSCALE)
        eChargeList = addCharges(eChargeListQ, winmgr.CLOUDC)
        debug_prints(
                func="FSLG",
                text="cloud object cell count", var=len(eChargeList)
                )

    if winmgr.IBOOL:
        debug_prints(
                func="FSLG",
                text="'Voxelizing' insulator object (could take some time)"
                )
        obINSULATOR = bpy.context.scene.objects[winmgr.IOB]
        icList = voxelByRays(obINSULATOR, winmgr.ORIGIN, winmgr.GSCALE)

        debug_prints(
                func="FSLG",
                text="Insulator object cell count", var=len(icList)
                )

    # 2) locate candidate sites around charge
    cSites = getCandidateSites(cgrid, icList)

    # 3) calc potential at each site (eqn. 10)
    cSites = initialPointCharges(cgrid, cSites, eChargeList)

    ts = 1
    while ts <= TSTEPS:
        # 1) select new growth site (eqn. 12)
        # get probabilities at candidate sites
        gProbs = getGrowthProbability(winmgr.BIGVAR, cSites)
        # choose new growth site based on probabilities
        gSitei = weightedRandomChoice(gProbs)
        gsite = cSites[gSitei][0]

        # 2) add new point charge at growth site
        # add new growth cell to grid
        cgrid.append(gsite)
        # remove new growth cell from candidate sites
        cSites.remove(cSites[gSitei])

        # 3) update potential at candidate sites (eqn. 11)
        cSites = updatePointCharges(gsite, cSites, eChargeList)

        # 4) add new candidates surrounding growth site
        # get candidate 'stencil'
        ncSitesT = getCandidateSites([gsite], icList)
        # remove candidates already in candidate list or charge grid
        ncSites = []
        cSplit = splitList(cSites, 0)
        for cn in ncSitesT:
            if cn not in cSplit and cn not in cgrid:
                ncSites.append((cn, 0))

        # 5) calc potential at new candidate sites (eqn. 10)
        ncSplit = splitList(ncSites, 0)
        ncSites = initialPointCharges(cgrid, ncSplit, eChargeList)

        # add new candidate sites to candidate list
        for ncs in ncSites:
            cSites.append(ncs)

        # iteration complete
        istr1 = ':::T-STEP: ' + str(ts) + '/' + str(TSTEPS)
        istr12 = ' | GROUNDZ: ' + str(winmgr.GROUNDZ) + ' | '
        istr2 = 'CANDS: ' + str(len(cSites)) + ' | '
        istr3 = 'GSITE: ' + str(gsite)
        debug_prints(
                func="FSLG",
                text="Iteration complete",
                var=istr1 + istr12 + istr2 + istr3
                )
        ts += 1

        # early termination for ground/cloud strike
        if winmgr.GROUNDBOOL:
            if gsite[2] == winmgr.GROUNDZ:
                ts = TSTEPS + 1
                debug_prints(
                        func="FSLG",
                        text="Early termination due to groundstrike"
                        )
                continue

        if winmgr.CLOUDBOOL:
            if gsite in splitListCo(eChargeList):
                ts = TSTEPS + 1
                debug_prints(
                        func="FSLG",
                        text="Early termination due to cloudstrike"
                        )
                continue

    tc2 = time.clock()
    tcRUN = tc2 - tc1
    debug_prints(
            func="FSLG",
            text="Laplacian growth loop completed",
            var=str(len(cgrid)) + " / " + str(tcRUN)[0:5] + " Seconds"
            )
    debug_prints(func="FSLG", text="Visualizing data")

    reportSTRING = getReportString(tcRUN)

    # Visualize array
    visualizeArray(
            cgrid, obORIGIN, winmgr.GSCALE,
            winmgr.VMMESH, winmgr.VSMESH,
            winmgr.VCUBE, winmgr.VVOX, reportSTRING
            )

    debug_prints(func="FSLG", text="COMPLETE")


# GUI #

class runFSLGLoopOperator(Operator):
    bl_idname = "object.runfslg_operator"
    bl_label = "run FSLG Loop Operator"
    bl_description = "By The Mighty Hammer Of Thor!!!"

    def execute(self, context):
        # tuple - state, report text
        is_conditions, message = checkSettings()

        if is_conditions:
            FSLG()
        else:
            self.report({'WARNING'}, message + " Operation Cancelled")

            return {'CANCELLED'}

        return {'FINISHED'}


class setupObjectsOperator(Operator):
    bl_idname = "object.setup_objects_operator"
    bl_label = "Setup Objects Operator"
    bl_description = "Create origin/ground/cloud/insulator objects"

    def execute(self, context):
        setupObjects()

        return {'FINISHED'}


class OBJECT_PT_fslg(Panel):
    bl_label = "Laplacian Lightning"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_context = "objectmode"
    bl_category = "Create"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        winmgr = context.scene.advanced_objects1

        col = layout.column(align=True)
        col.prop(winmgr, "TSTEPS")
        col.prop(winmgr, "GSCALE")
        col.prop(winmgr, "BIGVAR")

        col = layout.column()
        col.operator("object.setup_objects_operator", text="Create Setup objects")
        col.label("Origin object")
        col.prop_search(winmgr, "OOB", context.scene, "objects")

        box = layout.box()
        col = box.column()
        col.prop(winmgr, "GROUNDBOOL")
        if winmgr.GROUNDBOOL:
            col.prop_search(winmgr, "GOB", context.scene, "objects")
            col.prop(winmgr, "GROUNDC")

        box = layout.box()
        col = box.column()
        col.prop(winmgr, "CLOUDBOOL")
        if winmgr.CLOUDBOOL:
            col.prop_search(winmgr, "COB", context.scene, "objects")
            col.prop(winmgr, "CLOUDC")

        box = layout.box()
        col = box.column()
        col.prop(winmgr, "IBOOL")
        if winmgr.IBOOL:
            col.prop_search(winmgr, "IOB", context.scene, "objects")

        col = layout.column()
        col.operator("object.runfslg_operator",
                     text="Generate Lightning", icon="RNDCURVE")

        row = layout.row(align=True)
        row.prop(winmgr, "VMMESH", toggle=True)
        row.prop(winmgr, "VSMESH", toggle=True)
        row.prop(winmgr, "VCUBE", toggle=True)


def getReportString(rtime):
    winmgr = bpy.context.scene.advanced_objects1
    rSTRING1 = 't:' + str(winmgr.TSTEPS) + ',sc:' + str(winmgr.GSCALE)[0:4] + ',uv:' + str(winmgr.BIGVAR)[0:4] + ','
    rSTRING2 = 'ori:' + str(winmgr. ORIGIN[0]) + '/' + str(winmgr. ORIGIN[1]) + '/' + str(winmgr. ORIGIN[2]) + ','
    rSTRING3 = 'gz:' + str(winmgr.GROUNDZ) + ',gc:' + str(winmgr.GROUNDC) + ',rtime:' + str(int(rtime))
    return rSTRING1 + rSTRING2 + rSTRING3


def addReportProp(ob, str):
    bpy.types.Object.FSLG_REPORT = bpy.props.StringProperty(
        name='fslg_report', default='')
    ob.FSLG_REPORT = str


def register():
    bpy.utils.register_class(runFSLGLoopOperator)
    bpy.utils.register_class(setupObjectsOperator)
    bpy.utils.register_class(OBJECT_PT_fslg)


def unregister():
    bpy.utils.unregister_class(runFSLGLoopOperator)
    bpy.utils.unregister_class(setupObjectsOperator)
    bpy.utils.unregister_class(OBJECT_PT_fslg)


if __name__ == "__main__":
    register()
    pass


# Benchmarks Function

def BENCH():
    debug_prints(func="BENCH", text="BEGIN BENCHMARK")
    bt0 = time.clock()
    # make a big list
    tsize = 25
    tlist = []
    for x in range(tsize):
        for y in range(tsize):
            for z in range(tsize):
                tlist.append((x, y, z))
                tlist.append((x, y, z))

    # function to test
    bt1 = time.clock()
    bt2 = time.clock()
    btRUNb = bt2 - bt1
    btRUNa = bt1 - bt0

    debug_prints(func="BENCH", text="SETUP TIME", var=btRUNa)
    debug_prints(func="BENCH", text="BENCHMARK TIME", var=btRUNb)
    debug_print_vars(
            "\n[BENCH]\n",
            "GRIDSIZE: ", tsize, ' - ', tsize * tsize * tsize
            )
