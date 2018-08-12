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

# TODO <pep8 compliant>

from mathutils import (
    Matrix,
    Vector,
    geometry,
)
import bpy
from bpy.types import Operator

DEG_TO_RAD = 0.017453292519943295  # pi/180.0
# see bugs:
# - T31598 (when too small).
# - T48086 (when too big).
SMALL_NUM = 1e-12


global USER_FILL_HOLES
global USER_FILL_HOLES_QUALITY
USER_FILL_HOLES = None
USER_FILL_HOLES_QUALITY = None


def pointInTri2D(v, v1, v2, v3):
    key = v1.x, v1.y, v2.x, v2.y, v3.x, v3.y

    # Commented because its slower to do the bounds check, we should really cache the bounds info for each face.
    '''
    # BOUNDS CHECK
    xmin= 1000000
    ymin= 1000000

    xmax= -1000000
    ymax= -1000000

    for i in (0,2,4):
        x= key[i]
        y= key[i+1]

        if xmax<x:	xmax= x
        if ymax<y:	ymax= y
        if xmin>x:	xmin= x
        if ymin>y:	ymin= y

    x= v.x
    y= v.y

    if x<xmin or x>xmax or y < ymin or y > ymax:
        return False
    # Done with bounds check
    '''
    try:
        mtx = dict_matrix[key]
        if not mtx:
            return False
    except:
        side1 = v2 - v1
        side2 = v3 - v1

        nor = side1.cross(side2)

        mtx = Matrix((side1, side2, nor))

        # Zero area 2d tri, even tho we throw away zero area faces
        # the projection UV can result in a zero area UV.
        if not mtx.determinant():
            dict_matrix[key] = None
            return False

        mtx.invert()

        dict_matrix[key] = mtx

    uvw = (v - v1) @ mtx
    return 0 <= uvw[0] and 0 <= uvw[1] and uvw[0] + uvw[1] <= 1


def boundsIsland(faces):
    minx = maxx = faces[0].uv[0][0]  # Set initial bounds.
    miny = maxy = faces[0].uv[0][1]
    # print len(faces), minx, maxx, miny , maxy
    for f in faces:
        for uv in f.uv:
            x = uv.x
            y = uv.y
            if x < minx:
                minx = x
            if y < miny:
                miny = y
            if x > maxx:
                maxx = x
            if y > maxy:
                maxy = y

    return minx, miny, maxx, maxy


"""
def boundsEdgeLoop(edges):
    minx = maxx = edges[0][0] # Set initial bounds.
    miny = maxy = edges[0][1]
    # print len(faces), minx, maxx, miny , maxy
    for ed in edges:
        for pt in ed:
            x= pt[0]
            y= pt[1]
            if x<minx: x= minx
            if y<miny: y= miny
            if x>maxx: x= maxx
            if y>maxy: y= maxy

    return minx, miny, maxx, maxy
"""

# Turns the islands into a list of unpordered edges (Non internal)
# Only for UV's
# only returns outline edges for intersection tests. and unique points.


def island2Edge(island):

    # Vert index edges
    edges = {}

    unique_points = {}

    for f in island:
        f_uvkey = map(tuple, f.uv)

        for vIdx, edkey in enumerate(f.edge_keys):
            unique_points[f_uvkey[vIdx]] = f.uv[vIdx]

            if f.v[vIdx].index > f.v[vIdx - 1].index:
                i1 = vIdx - 1
                i2 = vIdx
            else:
                i1 = vIdx
                i2 = vIdx - 1

            try:
                edges[f_uvkey[i1], f_uvkey[i2]] *= 0  # sets any edge with more than 1 user to 0 are not returned.
            except:
                edges[f_uvkey[i1], f_uvkey[i2]] = (f.uv[i1] - f.uv[i2]).length,

    # If 2 are the same then they will be together, but full [a,b] order is not correct.

    # Sort by length

    length_sorted_edges = [(Vector(key[0]), Vector(key[1]), value) for key, value in edges.items() if value != 0]

    try:
        length_sorted_edges.sort(key=lambda A: -A[2])  # largest first
    except:
        length_sorted_edges.sort(lambda A, B: cmp(B[2], A[2]))

    # Its okay to leave the length in there.
    # for e in length_sorted_edges:
    #	e.pop(2)

    # return edges and unique points
    return length_sorted_edges, [v.to_3d() for v in unique_points.values()]


# ========================= NOT WORKING????
# Find if a points inside an edge loop, unordered.
# pt is and x/y
# edges are a non ordered loop of edges.
# offsets are the edge x and y offset.
"""
def pointInEdges(pt, edges):
    #
    x1 = pt[0]
    y1 = pt[1]

    # Point to the left of this line.
    x2 = -100000
    y2 = -10000
    intersectCount = 0
    for ed in edges:
        xi, yi = lineIntersection2D(x1,y1, x2,y2, ed[0][0], ed[0][1], ed[1][0], ed[1][1])
        if xi is not None: # Is there an intersection.
            intersectCount+=1

    return intersectCount % 2
"""


def pointInIsland(pt, island):
    vec1, vec2, vec3 = Vector(), Vector(), Vector()
    for f in island:
        vec1.x, vec1.y = f.uv[0]
        vec2.x, vec2.y = f.uv[1]
        vec3.x, vec3.y = f.uv[2]

        if pointInTri2D(pt, vec1, vec2, vec3):
            return True

        if len(f.v) == 4:
            vec1.x, vec1.y = f.uv[0]
            vec2.x, vec2.y = f.uv[2]
            vec3.x, vec3.y = f.uv[3]
            if pointInTri2D(pt, vec1, vec2, vec3):
                return True
    return False


# box is (left,bottom, right, top)
def islandIntersectUvIsland(source, target, SourceOffset):
    # Is 1 point in the box, inside the vertLoops
    edgeLoopsSource = source[6]  # Pretend this is offset
    edgeLoopsTarget = target[6]

    # Edge intersect test
    for ed in edgeLoopsSource:
        for seg in edgeLoopsTarget:
            i = geometry.intersect_line_line_2d(seg[0],
                                                seg[1],
                                                SourceOffset + ed[0],
                                                SourceOffset + ed[1],
                                                )
            if i:
                return 1  # LINE INTERSECTION

    # 1 test for source being totally inside target
    SourceOffset.resize_3d()
    for pv in source[7]:
        if pointInIsland(pv + SourceOffset, target[0]):
            return 2  # SOURCE INSIDE TARGET

    # 2 test for a part of the target being totally inside the source.
    for pv in target[7]:
        if pointInIsland(pv - SourceOffset, source[0]):
            return 3  # PART OF TARGET INSIDE SOURCE.

    return 0  # NO INTERSECTION


def rotate_uvs(uv_points, angle):

    if angle != 0.0:
        mat = Matrix.Rotation(angle, 2)
        for uv in uv_points:
            uv[:] = mat @ uv


def optiRotateUvIsland(faces):
    uv_points = [uv for f in faces for uv in f.uv]
    angle = geometry.box_fit_2d(uv_points)

    if angle != 0.0:
        rotate_uvs(uv_points, angle)

    # orient them vertically (could be an option)
    minx, miny, maxx, maxy = boundsIsland(faces)
    w, h = maxx - minx, maxy - miny
    # use epsilon so we dont randomly rotate (almost) perfect squares.
    if h + 0.00001 < w:
        from math import pi
        angle = pi / 2.0
        rotate_uvs(uv_points, angle)


# Takes an island list and tries to find concave, hollow areas to pack smaller islands into.
def mergeUvIslands(islandList):
    global USER_FILL_HOLES
    global USER_FILL_HOLES_QUALITY

    # Pack islands to bottom LHS
    # Sync with island

    # islandTotFaceArea = [] # A list of floats, each island area
    # islandArea = [] # a list of tuples ( area, w,h)

    decoratedIslandList = []

    islandIdx = len(islandList)
    while islandIdx:
        islandIdx -= 1
        minx, miny, maxx, maxy = boundsIsland(islandList[islandIdx])
        w, h = maxx - minx, maxy - miny

        totFaceArea = 0
        offset = Vector((minx, miny))
        for f in islandList[islandIdx]:
            for uv in f.uv:
                uv -= offset

            totFaceArea += f.area

        islandBoundsArea = w * h
        efficiency = abs(islandBoundsArea - totFaceArea)

        # UV Edge list used for intersections as well as unique points.
        edges, uniqueEdgePoints = island2Edge(islandList[islandIdx])

        decoratedIslandList.append([islandList[islandIdx], totFaceArea, efficiency, islandBoundsArea, w, h, edges, uniqueEdgePoints])

    # Sort by island bounding box area, smallest face area first.
    # no.. chance that to most simple edge loop first.
    decoratedIslandListAreaSort = decoratedIslandList[:]

    decoratedIslandListAreaSort.sort(key=lambda A: A[3])

    # sort by efficiency, Least Efficient first.
    decoratedIslandListEfficSort = decoratedIslandList[:]
    # decoratedIslandListEfficSort.sort(lambda A, B: cmp(B[2], A[2]))

    decoratedIslandListEfficSort.sort(key=lambda A: -A[2])

    # ================================================== THESE CAN BE TWEAKED.
    # This is a quality value for the number of tests.
    # from 1 to 4, generic quality value is from 1 to 100
    USER_STEP_QUALITY = ((USER_FILL_HOLES_QUALITY - 1) / 25.0) + 1

    # If 100 will test as long as there is enough free space.
    # this is rarely enough, and testing takes a while, so lower quality speeds this up.

    # 1 means they have the same quality
    USER_FREE_SPACE_TO_TEST_QUALITY = 1 + (((100 - USER_FILL_HOLES_QUALITY) / 100.0) * 5)

    # print 'USER_STEP_QUALITY', USER_STEP_QUALITY
    # print 'USER_FREE_SPACE_TO_TEST_QUALITY', USER_FREE_SPACE_TO_TEST_QUALITY

    removedCount = 0

    areaIslandIdx = 0
    ctrl = Window.Qual.CTRL
    BREAK = False
    while areaIslandIdx < len(decoratedIslandListAreaSort) and not BREAK:
        sourceIsland = decoratedIslandListAreaSort[areaIslandIdx]
        # Already packed?
        if not sourceIsland[0]:
            areaIslandIdx += 1
        else:
            efficIslandIdx = 0
            while efficIslandIdx < len(decoratedIslandListEfficSort) and not BREAK:

                if Window.GetKeyQualifiers() & ctrl:
                    BREAK = True
                    break

                # Now we have 2 islands, if the efficiency of the islands lowers theres an
                # increasing likely hood that we can fit merge into the bigger UV island.
                # this ensures a tight fit.

                # Just use figures we have about user/unused area to see if they might fit.

                targetIsland = decoratedIslandListEfficSort[efficIslandIdx]

                if sourceIsland[0] == targetIsland[0] or\
                        not targetIsland[0] or\
                        not sourceIsland[0]:
                    pass
                else:

                    #~ ([island, totFaceArea, efficiency, islandArea, w,h])
                    # Wasted space on target is greater then UV bounding island area.

                    #~ if targetIsland[3] > (sourceIsland[2]) and\ #
                    # ~ print USER_FREE_SPACE_TO_TEST_QUALITY
                    if targetIsland[2] > (sourceIsland[1] * USER_FREE_SPACE_TO_TEST_QUALITY) and\
                            targetIsland[4] > sourceIsland[4] and\
                            targetIsland[5] > sourceIsland[5]:

                        # DEBUG # print '%.10f  %.10f' % (targetIsland[3], sourceIsland[1])

                        # These enough spare space lets move the box until it fits

                        # How many times does the source fit into the target x/y
                        blockTestXUnit = targetIsland[4] / sourceIsland[4]
                        blockTestYUnit = targetIsland[5] / sourceIsland[5]

                        boxLeft = 0

                        # Distance we can move between whilst staying inside the targets bounds.
                        testWidth = targetIsland[4] - sourceIsland[4]
                        testHeight = targetIsland[5] - sourceIsland[5]

                        # Increment we move each test. x/y
                        xIncrement = (testWidth / (blockTestXUnit * ((USER_STEP_QUALITY / 50) + 0.1)))
                        yIncrement = (testHeight / (blockTestYUnit * ((USER_STEP_QUALITY / 50) + 0.1)))

                        # Make sure were not moving less then a 3rg of our width/height
                        if xIncrement < sourceIsland[4] / 3:
                            xIncrement = sourceIsland[4]
                        if yIncrement < sourceIsland[5] / 3:
                            yIncrement = sourceIsland[5]

                        boxLeft = 0  # Start 1 back so we can jump into the loop.
                        boxBottom = 0  # -yIncrement

                        # ~ testcount= 0

                        while boxBottom <= testHeight:
                            # Should we use this? - not needed for now.
                            # ~ if Window.GetKeyQualifiers() & ctrl:
                            # ~     BREAK= True
                            # ~     break

                            # testcount+=1
                            # print 'Testing intersect'
                            Intersect = islandIntersectUvIsland(sourceIsland, targetIsland, Vector((boxLeft, boxBottom)))
                            # print 'Done', Intersect
                            if Intersect == 1:  # Line intersect, don't bother with this any more
                                pass

                            if Intersect == 2:  # Source inside target
                                """
                                We have an intersection, if we are inside the target
                                then move us 1 whole width across,
                                Its possible this is a bad idea since 2 skinny Angular faces
                                could join without 1 whole move, but its a lot more optimal to speed this up
                                since we have already tested for it.

                                It gives about 10% speedup with minimal errors.
                                """
                                # Move the test along its width + SMALL_NUM
                                #boxLeft += sourceIsland[4] + SMALL_NUM
                                boxLeft += sourceIsland[4]
                            elif Intersect == 0:  # No intersection?? Place it.
                                # Progress
                                removedCount += 1
# XXX								Window.DrawProgressBar(0.0, 'Merged: %i islands, Ctrl to finish early.' % removedCount)

                                # Move faces into new island and offset
                                targetIsland[0].extend(sourceIsland[0])
                                offset = Vector((boxLeft, boxBottom))

                                for f in sourceIsland[0]:
                                    for uv in f.uv:
                                        uv += offset

                                del sourceIsland[0][:]  # Empty

                                # Move edge loop into new and offset.
                                # targetIsland[6].extend(sourceIsland[6])
                                # while sourceIsland[6]:
                                targetIsland[6].extend([(
                                    (e[0] + offset, e[1] + offset, e[2])
                                ) for e in sourceIsland[6]])

                                del sourceIsland[6][:]  # Empty

                                # Sort by edge length, reverse so biggest are first.

                                try:
                                    targetIsland[6].sort(key=lambda A: A[2])
                                except:
                                    targetIsland[6].sort(lambda B, A: cmp(A[2], B[2]))

                                targetIsland[7].extend(sourceIsland[7])
                                offset = Vector((boxLeft, boxBottom, 0.0))
                                for p in sourceIsland[7]:
                                    p += offset

                                del sourceIsland[7][:]

                                # Decrement the efficiency
                                targetIsland[1] += sourceIsland[1]  # Increment totFaceArea
                                targetIsland[2] -= sourceIsland[1]  # Decrement efficiency
                                # IF we ever used these again, should set to 0, eg
                                sourceIsland[2] = 0  # No area if anyone wants to know

                                break

                            # INCREMENT NEXT LOCATION
                            if boxLeft > testWidth:
                                boxBottom += yIncrement
                                boxLeft = 0.0
                            else:
                                boxLeft += xIncrement
                        # print testcount

                efficIslandIdx += 1
        areaIslandIdx += 1

    # Remove empty islands
    i = len(islandList)
    while i:
        i -= 1
        if not islandList[i]:
            del islandList[i]  # Can increment islands removed here.

# Takes groups of faces. assumes face groups are UV groups.


def getUvIslands(faceGroups, me):

    # Get seams so we don't cross over seams
    edge_seams = {}  # should be a set
    for ed in me.edges:
        if ed.use_seam:
            edge_seams[ed.key] = None  # dummy var- use sets!
    # Done finding seams

    islandList = []

# XXX	Window.DrawProgressBar(0.0, 'Splitting %d projection groups into UV islands:' % len(faceGroups))
    # print '\tSplitting %d projection groups into UV islands:' % len(faceGroups),
    # Find grouped faces

    faceGroupIdx = len(faceGroups)

    while faceGroupIdx:
        faceGroupIdx -= 1
        faces = faceGroups[faceGroupIdx]

        if not faces:
            continue

        # Build edge dict
        edge_users = {}

        for i, f in enumerate(faces):
            for ed_key in f.edge_keys:
                if ed_key in edge_seams:  # DELIMIT SEAMS! ;)
                    edge_users[ed_key] = []  # so as not to raise an error
                else:
                    try:
                        edge_users[ed_key].append(i)
                    except:
                        edge_users[ed_key] = [i]

        # Modes
        # 0 - face not yet touched.
        # 1 - added to island list, and need to search
        # 2 - touched and searched - don't touch again.
        face_modes = [0] * len(faces)  # initialize zero - untested.

        face_modes[0] = 1  # start the search with face 1

        newIsland = []

        newIsland.append(faces[0])

        ok = True
        while ok:

            ok = True
            while ok:
                ok = False
                for i in range(len(faces)):
                    if face_modes[i] == 1:  # search
                        for ed_key in faces[i].edge_keys:
                            for ii in edge_users[ed_key]:
                                if i != ii and face_modes[ii] == 0:
                                    face_modes[ii] = ok = 1  # mark as searched
                                    newIsland.append(faces[ii])

                        # mark as searched, don't look again.
                        face_modes[i] = 2

            islandList.append(newIsland)

            ok = False
            for i in range(len(faces)):
                if face_modes[i] == 0:
                    newIsland = []
                    newIsland.append(faces[i])

                    face_modes[i] = ok = 1
                    break
            # if not ok will stop looping

# XXX	Window.DrawProgressBar(0.1, 'Optimizing Rotation for %i UV Islands' % len(islandList))

    for island in islandList:
        optiRotateUvIsland(island)

    return islandList


def packIslands(islandList):
    if USER_FILL_HOLES:
        # XXX		Window.DrawProgressBar(0.1, 'Merging Islands (Ctrl: skip merge)...')
        mergeUvIslands(islandList)  # Modify in place

    # Now we have UV islands, we need to pack them.

    # Make a synchronized list with the islands
    # so we can box pack the islands.
    packBoxes = []

    # Keep a list of X/Y offset so we can save time by writing the
    # uv's and packed data in one pass.
    islandOffsetList = []

    islandIdx = 0

    while islandIdx < len(islandList):
        minx, miny, maxx, maxy = boundsIsland(islandList[islandIdx])

        w, h = maxx - minx, maxy - miny

        if USER_ISLAND_MARGIN:
            minx -= USER_ISLAND_MARGIN  # *w
            miny -= USER_ISLAND_MARGIN  # *h
            maxx += USER_ISLAND_MARGIN  # *w
            maxy += USER_ISLAND_MARGIN  # *h

            # recalc width and height
            w, h = maxx - minx, maxy - miny

        if w < SMALL_NUM:
            w = SMALL_NUM
        if h < SMALL_NUM:
            h = SMALL_NUM

        """Save the offset to be applied later,
        we could apply to the UVs now and allign them to the bottom left hand area
        of the UV coords like the box packer imagines they are
        but, its quicker just to remember their offset and
        apply the packing and offset in 1 pass """
        islandOffsetList.append((minx, miny))

        # Add to boxList. use the island idx for the BOX id.
        packBoxes.append([0, 0, w, h])
        islandIdx += 1

    # Now we have a list of boxes to pack that syncs
    # with the islands.

    # print '\tPacking UV Islands...'
# XXX	Window.DrawProgressBar(0.7, "Packing %i UV Islands..." % len(packBoxes) )

    # time1 = time.time()
    packWidth, packHeight = geometry.box_pack_2d(packBoxes)

    # print 'Box Packing Time:', time.time() - time1

    # if len(pa	ckedLs) != len(islandList):
    #    raise ValueError("Packed boxes differs from original length")

    # print '\tWriting Packed Data to faces'
# XXX	Window.DrawProgressBar(0.8, "Writing Packed Data to faces")

    # Sort by ID, so there in sync again
    islandIdx = len(islandList)
    # Having these here avoids divide by 0
    if islandIdx:

        if USER_STRETCH_ASPECT:
            # Maximize to uv area?? Will write a normalize function.
            xfactor = 1.0 / packWidth
            yfactor = 1.0 / packHeight
        else:
            # Keep proportions.
            xfactor = yfactor = 1.0 / max(packWidth, packHeight)

    while islandIdx:
        islandIdx -= 1
        # Write the packed values to the UV's

        xoffset = packBoxes[islandIdx][0] - islandOffsetList[islandIdx][0]
        yoffset = packBoxes[islandIdx][1] - islandOffsetList[islandIdx][1]

        for f in islandList[islandIdx]:  # Offsetting the UV's so they fit in there packed box
            for uv in f.uv:
                uv.x = (uv.x + xoffset) * xfactor
                uv.y = (uv.y + yoffset) * yfactor


def VectoQuat(vec):
    vec = vec.normalized()
    return vec.to_track_quat('Z', 'X' if abs(vec.x) > 0.5 else 'Y').inverted()


class thickface:
    __slost__ = "v", "uv", "no", "area", "edge_keys"

    def __init__(self, face, uv_layer, mesh_verts):
        self.v = [mesh_verts[i] for i in face.vertices]
        self.uv = [uv_layer[i].uv for i in face.loop_indices]

        self.no = face.normal.copy()
        self.area = face.area
        self.edge_keys = face.edge_keys


def main_consts():
    from math import radians

    global ROTMAT_2D_POS_90D
    global ROTMAT_2D_POS_45D
    global RotMatStepRotation

    ROTMAT_2D_POS_90D = Matrix.Rotation(radians(90.0), 2)
    ROTMAT_2D_POS_45D = Matrix.Rotation(radians(45.0), 2)

    RotMatStepRotation = []
    rot_angle = 22.5  # 45.0/2
    while rot_angle > 0.1:
        RotMatStepRotation.append([
            Matrix.Rotation(radians(+rot_angle), 2),
            Matrix.Rotation(radians(-rot_angle), 2),
        ])

        rot_angle = rot_angle / 2.0


global ob
ob = None


def main(context,
         island_margin,
         projection_limit,
         user_area_weight,
         use_aspect,
         stretch_to_bounds,
         ):
    global USER_FILL_HOLES
    global USER_FILL_HOLES_QUALITY
    global USER_STRETCH_ASPECT
    global USER_ISLAND_MARGIN

    from math import cos
    import time

    global dict_matrix
    dict_matrix = {}

    # Constants:
    # Takes a list of faces that make up a UV island and rotate
    # until they optimally fit inside a square.
    global ROTMAT_2D_POS_90D
    global ROTMAT_2D_POS_45D
    global RotMatStepRotation
    main_consts()

    # Create the variables.
    USER_PROJECTION_LIMIT = projection_limit
    USER_ONLY_SELECTED_FACES = True
    USER_SHARE_SPACE = 1  # Only for hole filling.
    USER_STRETCH_ASPECT = stretch_to_bounds
    USER_ISLAND_MARGIN = island_margin  # Only for hole filling.
    USER_FILL_HOLES = 0
    USER_FILL_HOLES_QUALITY = 50  # Only for hole filling.
    USER_VIEW_INIT = 0  # Only for hole filling.

    is_editmode = (context.active_object.mode == 'EDIT')
    if is_editmode:
        obList = [ob for ob in [context.active_object] if ob and ob.type == 'MESH']
    else:
        obList = [ob for ob in context.selected_editable_objects if ob and ob.type == 'MESH']
        USER_ONLY_SELECTED_FACES = False

    if not obList:
        raise Exception("error, no selected mesh objects")

    # Reuse variable
    if len(obList) == 1:
        ob = "Unwrap %i Selected Mesh"
    else:
        ob = "Unwrap %i Selected Meshes"

    # HACK, loop until mouse is lifted.
    '''
    while Window.GetMouseButtons() != 0:
        time.sleep(10)
    '''

# ~ XXX	if not Draw.PupBlock(ob % len(obList), pup_block):
# ~ XXX		return
# ~ XXX	del ob

    # Convert from being button types

    USER_PROJECTION_LIMIT_CONVERTED = cos(USER_PROJECTION_LIMIT * DEG_TO_RAD)
    USER_PROJECTION_LIMIT_HALF_CONVERTED = cos((USER_PROJECTION_LIMIT / 2) * DEG_TO_RAD)

    # Toggle Edit mode
    is_editmode = (context.active_object.mode == 'EDIT')
    if is_editmode:
        bpy.ops.object.mode_set(mode='OBJECT')
    # Assume face select mode! an annoying hack to toggle face select mode because Mesh doesn't like faceSelectMode.

    if USER_SHARE_SPACE:
        # Sort by data name so we get consistent results
        obList.sort(key=lambda ob: ob.data.name)
        collected_islandList = []

# XXX	Window.WaitCursor(1)

    time1 = time.time()

    # Tag as False so we don't operate on the same mesh twice.
# XXX	bpy.data.meshes.tag = False
    for me in bpy.data.meshes:
        me.tag = False

    for ob in obList:
        me = ob.data

        if me.tag or me.library:
            continue

        # Tag as used
        me.tag = True

        if not me.uv_layers:  # Mesh has no UV Coords, don't bother.
            me.uv_layers.new()

        uv_layer = me.uv_layers.active.data
        me_verts = list(me.vertices)

        if USER_ONLY_SELECTED_FACES:
            meshFaces = [thickface(f, uv_layer, me_verts) for i, f in enumerate(me.polygons) if f.select]
        else:
            meshFaces = [thickface(f, uv_layer, me_verts) for i, f in enumerate(me.polygons)]

# XXX		Window.DrawProgressBar(0.1, 'SmartProj UV Unwrapper, mapping "%s", %i faces.' % (me.name, len(meshFaces)))

        # =======
        # Generate a projection list from face normals, this is meant to be smart :)

        # make a list of face props that are in sync with meshFaces
        # Make a Face List that is sorted by area.
        # meshFaces = []

        # meshFaces.sort( lambda a, b: cmp(b.area , a.area) ) # Biggest first.
        meshFaces.sort(key=lambda a: -a.area)

        # remove all zero area faces
        while meshFaces and meshFaces[-1].area <= SMALL_NUM:
            # Set their UV's to 0,0
            for uv in meshFaces[-1].uv:
                uv.zero()
            meshFaces.pop()

        if not meshFaces:
            continue

        # Smallest first is slightly more efficient, but if the user cancels early then its better we work on the larger data.

        # Generate Projection Vecs
        # 0d is   1.0
        # 180 IS -0.59846

        # Initialize projectVecs
        if USER_VIEW_INIT:
            # Generate Projection
            projectVecs = [Vector(Window.GetViewVector()) @ ob.matrix_world.inverted().to_3x3()]  # We add to this along the way
        else:
            projectVecs = []

        newProjectVec = meshFaces[0].no
        newProjectMeshFaces = []  # Popping stuffs it up.

        # Pretend that the most unique angle is ages away to start the loop off
        mostUniqueAngle = -1.0

        # This is popped
        tempMeshFaces = meshFaces[:]

        # This while only gathers projection vecs, faces are assigned later on.
        while 1:
            # If theres none there then start with the largest face

            # add all the faces that are close.
            for fIdx in range(len(tempMeshFaces) - 1, -1, -1):
                # Use half the angle limit so we don't overweight faces towards this
                # normal and hog all the faces.
                if newProjectVec.dot(tempMeshFaces[fIdx].no) > USER_PROJECTION_LIMIT_HALF_CONVERTED:
                    newProjectMeshFaces.append(tempMeshFaces.pop(fIdx))

            # Add the average of all these faces normals as a projectionVec
            averageVec = Vector((0.0, 0.0, 0.0))
            if user_area_weight == 0.0:
                for fprop in newProjectMeshFaces:
                    averageVec += fprop.no
            elif user_area_weight == 1.0:
                for fprop in newProjectMeshFaces:
                    averageVec += fprop.no * fprop.area
            else:
                for fprop in newProjectMeshFaces:
                    averageVec += fprop.no * ((fprop.area * user_area_weight) + (1.0 - user_area_weight))

            if averageVec.x != 0 or averageVec.y != 0 or averageVec.z != 0:  # Avoid NAN
                projectVecs.append(averageVec.normalized())

            # Get the next vec!
            # Pick the face thats most different to all existing angles :)
            mostUniqueAngle = 1.0  # 1.0 is 0d. no difference.
            mostUniqueIndex = 0  # dummy

            for fIdx in range(len(tempMeshFaces) - 1, -1, -1):
                angleDifference = -1.0  # 180d difference.

                # Get the closest vec angle we are to.
                for p in projectVecs:
                    temp_angle_diff = p.dot(tempMeshFaces[fIdx].no)

                    if angleDifference < temp_angle_diff:
                        angleDifference = temp_angle_diff

                if angleDifference < mostUniqueAngle:
                    # We have a new most different angle
                    mostUniqueIndex = fIdx
                    mostUniqueAngle = angleDifference

            if mostUniqueAngle < USER_PROJECTION_LIMIT_CONVERTED:
                # print 'adding', mostUniqueAngle, USER_PROJECTION_LIMIT, len(newProjectMeshFaces)
                # Now weight the vector to all its faces, will give a more direct projection
                # if the face its self was not representative of the normal from surrounding faces.

                newProjectVec = tempMeshFaces[mostUniqueIndex].no
                newProjectMeshFaces = [tempMeshFaces.pop(mostUniqueIndex)]

            else:
                if len(projectVecs) >= 1:  # Must have at least 2 projections
                    break

        # If there are only zero area faces then its possible
        # there are no projectionVecs
        if not len(projectVecs):
            Draw.PupMenu('error, no projection vecs where generated, 0 area faces can cause this.')
            return

        faceProjectionGroupList = [[] for i in range(len(projectVecs))]

        # MAP and Arrange # We know there are 3 or 4 faces here

        for fIdx in range(len(meshFaces) - 1, -1, -1):
            fvec = meshFaces[fIdx].no
            i = len(projectVecs)

            # Initialize first
            bestAng = fvec.dot(projectVecs[0])
            bestAngIdx = 0

            # Cycle through the remaining, first already done
            while i - 1:
                i -= 1

                newAng = fvec.dot(projectVecs[i])
                if newAng > bestAng:  # Reverse logic for dotvecs
                    bestAng = newAng
                    bestAngIdx = i

            # Store the area for later use.
            faceProjectionGroupList[bestAngIdx].append(meshFaces[fIdx])

        # Cull faceProjectionGroupList,

        # Now faceProjectionGroupList is full of faces that face match the project Vecs list
        for i in range(len(projectVecs)):
            # Account for projectVecs having no faces.
            if not faceProjectionGroupList[i]:
                continue

            # Make a projection matrix from a unit length vector.
            MatQuat = VectoQuat(projectVecs[i])

            # Get the faces UV's from the projected vertex.
            for f in faceProjectionGroupList[i]:
                f_uv = f.uv
                for j, v in enumerate(f.v):
                    # XXX - note, between mathutils in 2.4 and 2.5 the order changed.
                    f_uv[j][:] = (MatQuat @ v.co).xy

        if USER_SHARE_SPACE:
            # Should we collect and pack later?
            islandList = getUvIslands(faceProjectionGroupList, me)
            collected_islandList.extend(islandList)

        else:
            # Should we pack the islands for this 1 object?
            islandList = getUvIslands(faceProjectionGroupList, me)
            packIslands(islandList)

        # update the mesh here if we need to.

    # We want to pack all in 1 go, so pack now
    if USER_SHARE_SPACE:
        # XXX        Window.DrawProgressBar(0.9, "Box Packing for all objects...")
        packIslands(collected_islandList)

    print("Smart Projection time: %.2f" % (time.time() - time1))
    # Window.DrawProgressBar(0.9, "Smart Projections done, time: %.2f sec" % (time.time() - time1))

    # aspect correction is only done in edit mode - and only smart unwrap supports currently
    if is_editmode:
        bpy.ops.object.mode_set(mode='EDIT')

        if use_aspect:
            import bmesh
            aspect = context.scene.uvedit_aspect(context.active_object)
            if aspect[0] > aspect[1]:
                aspect[0] = aspect[1] / aspect[0]
                aspect[1] = 1.0
            else:
                aspect[1] = aspect[0] / aspect[1]
                aspect[0] = 1.0

            bm = bmesh.from_edit_mesh(me)

            uv_act = bm.loops.layers.uv.active

            faces = [f for f in bm.faces if f.select]

            for f in faces:
                for l in f.loops:
                    l[uv_act].uv[0] *= aspect[0]
                    l[uv_act].uv[1] *= aspect[1]

    dict_matrix.clear()

# XXX	Window.DrawProgressBar(1.0, "")
# XXX	Window.WaitCursor(0)
# XXX	Window.RedrawAll()


"""
    pup_block = [\
    'Projection',\
    ('Selected Faces Only', USER_ONLY_SELECTED_FACES, 'Use only selected faces from all selected meshes.'),\
    ('Init from view', USER_VIEW_INIT, 'The first projection will be from the view vector.'),\
    '',\
    'UV Layout',\
    ('Share Tex Space', USER_SHARE_SPACE, 'Objects Share texture space, map all objects into 1 uvmap.'),\
    ('Island Margin:', USER_ISLAND_MARGIN, 0.0, 0.5, ''),\
    'Fill in empty areas',\
    ('Fill Holes', USER_FILL_HOLES, 'Fill in empty areas reduced texture waistage (slow).'),\
    ('Fill Quality:', USER_FILL_HOLES_QUALITY, 1, 100, 'Depends on fill holes, how tightly to fill UV holes, (higher is slower)'),\
    ]
"""

from bpy.props import FloatProperty, BoolProperty


class SmartProject(Operator):
    """This script projection unwraps the selected faces of a mesh """ \
        """(it operates on all selected mesh objects, and can be used """ \
        """to unwrap selected faces, or all faces)"""
    bl_idname = "uv.smart_project"
    bl_label = "Smart UV Project"
    bl_options = {'REGISTER', 'UNDO'}

    angle_limit: FloatProperty(
        name="Angle Limit",
        description="Lower for more projection groups, higher for less distortion",
        min=1.0, max=89.0,
        default=66.0,
    )
    island_margin: FloatProperty(
        name="Island Margin",
        description="Margin to reduce bleed from adjacent islands",
        unit='LENGTH', subtype='DISTANCE',
        min=0.0, max=1.0,
        default=0.0,
    )
    user_area_weight: FloatProperty(
        name="Area Weight",
        description="Weight projections vector by faces with larger areas",
        min=0.0, max=1.0,
        default=0.0,
    )
    use_aspect: BoolProperty(
        name="Correct Aspect",
        description="Map UVs taking image aspect ratio into account",
        default=True
    )
    stretch_to_bounds: BoolProperty(
        name="Stretch to UV Bounds",
        description="Stretch the final output to texture bounds",
        default=True,
    )

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        main(context,
             self.island_margin,
             self.angle_limit,
             self.user_area_weight,
             self.use_aspect,
             self.stretch_to_bounds
             )
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)


classes = (
    SmartProject,
)
