# GPL # "author": revolt_randy, Jambay

# Create "Beam" primitives. Based on original script by revolt_randy


import bpy
from bpy.types import Operator
from bpy.props import (
        BoolProperty,
        EnumProperty,
        FloatProperty,
        IntProperty,
        )


# #####################
# Create vertices for end of mesh
#
# y_off - verts y-axis origin
#
# returns:
#  endVs - x,y,z list

def beamEndVs(sRef, y_off):
    thick = sRef.beamW * 2

    if sRef.Type == '2':  # swap width and height for C shape
        bEndX2 = sRef.beamZ / 2
        bEndXInr = ((sRef.beamZ - thick) / 2)
        bEndZ2 = sRef.beamX / 2
        bEndZInr = ((sRef.beamX - thick) / 2)
    else:
        bEndX2 = sRef.beamX / 2
        bEndXInr = ((sRef.beamX - thick) / 2)
        bEndZ2 = sRef.beamZ / 2
        bEndZInr = ((sRef.beamZ - thick) / 2)

    endVs = []

    # outer ...
    endVs.append((bEndX2, y_off, bEndZ2))
    endVs.append((-bEndX2, y_off, bEndZ2))
    endVs.append((-bEndX2, y_off, -bEndZ2))
    endVs.append((bEndX2, y_off, -bEndZ2))
    # innner ...
    endVs.append((bEndXInr, y_off, bEndZInr))
    endVs.append((-bEndXInr, y_off, bEndZInr))
    endVs.append((-bEndXInr, y_off, -bEndZInr))
    endVs.append((bEndXInr, y_off, -bEndZInr))

    return endVs


# #####################
# Create End Faces
#
# verts_list - list of vertices
#
# returns:
#  beamFs, a list of tuples defining the end faces.

def beamEndFaces(verts_list):

    beamFs = []

    num_of_verts = int(len(verts_list) / 2)

    # Create list of faces
    for index in range(num_of_verts):
        faces_temp = []

        if index == (num_of_verts - 1):
            faces_temp.append(verts_list[index])
            faces_temp.append(verts_list[index - index])
            faces_temp.append(verts_list[index + 1])
            faces_temp.append(verts_list[index * 2 + 1])
        else:
            faces_temp.append(verts_list[index])
            faces_temp.append(verts_list[index + 1])
            faces_temp.append(verts_list[index + num_of_verts + 1])
            faces_temp.append(verts_list[index + num_of_verts])

        beamFs.append(tuple(faces_temp))

    return beamFs


# #####################
# Bridge vertices to create side faces.
#
# front_verts - front face vertices
# back_verts - back face vertices
#  front & back must be ordered in same direction
#  with respect to y-axis
#
# returns:
#  sideFaces, a list of the bridged faces

def beamSides(front_verts, back_verts):
    sideFaces = []

    num_of_faces = (len(front_verts))

    # add first value to end of lists for looping
    front_verts.append(front_verts[0])
    back_verts.append(back_verts[0])

    # Build the faces
    for index in range(num_of_faces):
        facestemp = (front_verts[index], front_verts[index + 1], back_verts[index + 1], back_verts[index])
        sideFaces.append(facestemp)

    return sideFaces


# #####################
# Creates a box beam
#
# returns:
#  beamVs - x, y, z, location of each vertice
#  beamFs - vertices that make up each face

def create_beam(sRef):

    frontVs = []
    frontFs = []
    backVs = []

    y_off = sRef.beamY / 2  # offset from center for vertices

    frontVs = beamEndVs(sRef, y_off)
    backVs = beamEndVs(sRef, -y_off)

    # Combine vertices
    beamVs = frontVs + backVs

    # Create front face
    numofverts = len(frontVs)
    verts_front_list = []
    for index in range(numofverts):
        verts_front_list.append(index)

    frontFs = beamEndFaces(verts_front_list)

    # Create back face
    faces_back_temp = []
    verts_back_list = []

    numofverts = len(backVs)
    for index in range(numofverts):
        verts_back_list.append(index + numofverts)

    faces_back_temp = beamEndFaces(verts_back_list)

    # Create side faces
    faces_side_temp = []

    # Object has thickness, create list of outside vertices
    numofverts = len(verts_front_list)
    halfVerts = int(numofverts / 2)
    frontVs = verts_front_list[0:halfVerts]
    backVs = verts_back_list[0:halfVerts]

    faces_side_temp = beamSides(frontVs, backVs)

    # Create list of inside vertices
    frontVs = verts_front_list[halfVerts:numofverts]
    backVs = verts_back_list[halfVerts:numofverts]

    faces_side_temp += beamSides(frontVs, backVs)

    # Combine all faces
    beamFs = frontFs + faces_back_temp + faces_side_temp

    return beamVs, beamFs


# #####################
# Taper/angle faces of beam.
#  inner vert toward outer vert
#  based on percentage of taper.
#
# returns:
#  adVert - the calculated vertex

def beamSlant(sRef, outV, inV):
    bTaper = 100 - sRef.edgeA

    # calcuate variance & adjust vertex
    deltaV = ((inV - outV) / 100)
    adVert = outV + (deltaV * bTaper)

    return adVert


# #####################
# Modify location to shape beam.
#
# verts - tuples for one end of beam
#
# returns:
#  verts - modified tuples for beam shape.

def beamSquareEnds(sRef, verts):

    # match 5th & 6th z locations to 1st & 2nd

    vert_orig = verts[0]
    vert_temp = verts[4]
    vert_x = beamSlant(sRef, vert_orig[0], vert_temp[0])
    verts[4] = (vert_x, vert_temp[1], vert_orig[2])

    vert_orig = verts[1]
    vert_temp = verts[5]
    vert_x = beamSlant(sRef, vert_orig[0], vert_temp[0])
    verts[5] = (vert_x, vert_temp[1], vert_orig[2])

    return verts


# #####################
#
# Create U shaped beam
#  Shared with C shape - see beamEndVs
#  for sizing and rotate in addBeamObj.
#
# returns:
#  beamVs - vertice x, y, z, locations
#  beamFs - face vertices

def create_u_beam(sRef):

    # offset vertices from center
    y_off = sRef.beamY / 2

    frontVtemp = []
    frontFtemp = []
    frontVlist = []

    backVtemp = []
    backFtemp = []
    backVlist = []

    sideFs = []

    frontVtemp = beamEndVs(sRef, y_off)  # Box beam
    frontVtemp = beamSquareEnds(sRef, frontVtemp)  # U shape

    backVtemp = beamEndVs(sRef, -y_off)
    backVtemp = beamSquareEnds(sRef, backVtemp)

    beamVs = frontVtemp + backVtemp

    # Create front face
    for index in range(len(frontVtemp)):  # Build vert list
        frontVlist.append(index)

    frontFtemp = beamEndFaces(frontVlist)
    frontFtemp = frontFtemp[1:4]  # Remove 1st face

    # Create back face
    numofverts = len(backVtemp)
    for index in range(numofverts):  # Build vertex list
        backVlist.append(index + numofverts)

    backFtemp = beamEndFaces(backVlist)
    backFtemp = backFtemp[1:4]  # Remove face

    # Create list vertices for outside faces
    numofverts = int(len(frontVlist))
    halfVerts = int(numofverts / 2)
    frontVtemp = frontVlist[0:halfVerts]
    backVtemp = backVlist[0:halfVerts]

    sideFs = beamSides(frontVtemp, backVtemp)
    sideFs = sideFs[1:]  # Remove face

    # Create inside verts
    frontVtemp = frontVlist[halfVerts:numofverts]
    backVtemp = backVlist[halfVerts:numofverts]

    sideFs += beamSides(frontVtemp, backVtemp)
    sideFs = sideFs[0:3] + sideFs[4:]  # Remove face

    # fill in faces
    sideFs.append((0, 4, 12, 8))
    sideFs.append((5, 1, 9, 13))

    beamFs = frontFtemp + backFtemp + sideFs  # Combine faces

    return beamVs, beamFs


# #####################
# returns:
#  verts_final - x, y, z, location of each vertice
#  faces_final - vertices that make up each face

def create_L_beam(sRef):

    thick = sRef.beamW

    # offset vertices from center
    x_off = sRef.beamX / 2
    y_off = sRef.beamY / 2
    z_off = sRef.beamZ / 2

    # Create temporarylists to hold vertices locations
    verts_front_temp = []
    verts_back_temp = []

    # Create front vertices by calculation
    verts_front_temp = [
            (-x_off, -y_off, z_off),
            (-(x_off - thick), -y_off, z_off),
            (-(x_off - thick), -y_off, -(z_off - thick)),
            (x_off, -y_off, -(z_off - thick)),
            (x_off, -y_off, -z_off),
            (-x_off, -y_off, -z_off)
            ]

    # Adjust taper
    vert_outside = verts_front_temp[0]
    vert_inside = verts_front_temp[1]
    vert_taper = beamSlant(sRef, vert_outside[0], vert_inside[0])
    verts_front_temp[1] = [vert_taper, vert_inside[1], vert_inside[2]]

    vert_outside = verts_front_temp[4]
    vert_inside = verts_front_temp[3]
    vert_taper = beamSlant(sRef, vert_outside[2], vert_inside[2])
    verts_front_temp[3] = [vert_inside[0], vert_inside[1], vert_taper]

    # Create back vertices by calculation
    verts_back_temp = [
            (-x_off, y_off, z_off),
            (-(x_off - thick), y_off, z_off),
            (-(x_off - thick), y_off, -(z_off - thick)),
            (x_off, y_off, -(z_off - thick)),
            (x_off, y_off, -z_off),
            (-x_off, y_off, -z_off)
            ]

    # Adjust taper
    vert_outside = verts_back_temp[0]
    vert_inside = verts_back_temp[1]
    vert_taper = beamSlant(sRef, vert_outside[0], vert_inside[0])
    verts_back_temp[1] = [vert_taper, vert_inside[1], vert_inside[2]]

    vert_outside = verts_back_temp[4]
    vert_inside = verts_back_temp[3]
    vert_taper = beamSlant(sRef, vert_outside[2], vert_inside[2])
    verts_back_temp[3] = [vert_inside[0], vert_inside[1], vert_taper]

    verts_final = verts_front_temp + verts_back_temp

    # define end faces, only 4 so just coded
    faces_front_temp = []
    faces_back_temp = []
    faces_side_temp = []

    faces_front_temp = [(0, 1, 2, 5), (2, 3, 4, 5)]
    faces_back_temp = [(6, 7, 8, 11), (8, 9, 10, 11)]

    verts_front_list = []
    verts_back_list = []
    num_of_verts = len(verts_front_temp)

    # build lists of back and front verts for beamSides function
    for index in range(num_of_verts):
        verts_front_list.append(index)
    for index in range(num_of_verts):
        verts_back_list.append(index + 6)

    faces_side_temp = beamSides(verts_front_list, verts_back_list)

    faces_final = faces_front_temp + faces_back_temp + faces_side_temp

    return verts_final, faces_final


# #####################
# returns:
#  verts_final - a list of tuples of the x, y, z, location of each vertice
#  faces_final - a list of tuples of the vertices that make up each face

def create_T_beam(sRef):

    thick = sRef.beamW

    # Get offset of vertices from center
    x_off = sRef.beamX / 2
    y_off = sRef.beamY / 2
    z_off = sRef.beamZ / 2
    thick_off = thick / 2

    # Create temporarylists to hold vertices locations
    verts_front_temp = []
    verts_back_temp = []

    # Create front vertices
    verts_front_temp = [
            (-x_off, -y_off, z_off),
            (-thick_off, -y_off, z_off),
            (thick_off, -y_off, z_off),
            (x_off, -y_off, z_off),
            (x_off, -y_off, z_off - thick),
            (thick_off, -y_off, z_off - thick),
            (thick_off, -y_off, -z_off),
            (-thick_off, -y_off, -z_off),
            (-thick_off, -y_off, z_off - thick),
            (-x_off, -y_off, z_off - thick)
            ]

    # Adjust taper
    vert_outside = verts_front_temp[0]
    vert_inside = verts_front_temp[9]
    vert_taper = (beamSlant(sRef, vert_outside[2], vert_inside[2]))
    verts_front_temp[9] = [vert_inside[0], vert_inside[1], vert_taper]

    vert_outside = verts_front_temp[3]
    vert_inside = verts_front_temp[4]
    verts_front_temp[4] = [vert_inside[0], vert_inside[1], vert_taper]

    # Adjust taper of bottom of beam, so 0 the center
    # now becomes vert_outside, and vert_inside is calculated
    # 1/2 way towards center
    vert_outside = (0, -y_off, -z_off)
    vert_inside = verts_front_temp[6]
    vert_taper = (beamSlant(sRef, vert_outside[0], vert_inside[0]))
    verts_front_temp[6] = [vert_taper, vert_inside[1], vert_inside[2]]

    vert_outside = (0, -y_off, -z_off)
    vert_inside = verts_front_temp[7]
    vert_taper = beamSlant(sRef, vert_outside[0], vert_inside[0])
    verts_front_temp[7] = [vert_taper, vert_inside[1], vert_inside[2]]

    # Create fack vertices by calculation
    verts_back_temp = [
            (-x_off, y_off, z_off),
            (-thick_off, y_off, z_off),
            (thick_off, y_off, z_off),
            (x_off, y_off, z_off),
            (x_off, y_off, z_off - thick),
            (thick_off, y_off, z_off - thick),
            (thick_off, y_off, -z_off),
            (-thick_off, y_off, -z_off),
            (-thick_off, y_off, z_off - thick),
            (-x_off, y_off, z_off - thick)
            ]

    # Adjust taper
    vert_outside = verts_back_temp[0]
    vert_inside = verts_back_temp[9]
    vert_taper = (beamSlant(sRef, vert_outside[2], vert_inside[2]))
    verts_back_temp[9] = [vert_inside[0], vert_inside[1], vert_taper]

    vert_outside = verts_back_temp[3]
    vert_inside = verts_back_temp[4]
    vert_taper = (beamSlant(sRef, vert_outside[2], vert_inside[2]))
    verts_back_temp[4] = [vert_inside[0], vert_inside[1], vert_taper]

    # Adjust taper of bottom of beam, so 0 the center
    # now becomes vert_outside, and vert_inside is calculated
    # 1/2 way towards center
    vert_outside = (0, -y_off, -z_off)
    vert_inside = verts_back_temp[6]
    vert_taper = (beamSlant(sRef, vert_outside[0], vert_inside[0]))
    verts_back_temp[6] = [vert_taper, vert_inside[1], vert_inside[2]]

    vert_outside = (0, -y_off, -z_off)
    vert_inside = verts_back_temp[7]
    vert_taper = (beamSlant(sRef, vert_outside[0], vert_inside[0]))
    verts_back_temp[7] = [vert_taper, vert_inside[1], vert_inside[2]]

    verts_final = verts_front_temp + verts_back_temp

    # define end faces, only 8 so just coded
    faces_front_temp = []
    faces_back_temp = []
    faces_side_temp = []

    faces_front_temp = [(0, 1, 8, 9), (1, 2, 5, 8),
                        (2, 3, 4, 5), (5, 6, 7, 8)]

    faces_back_temp = [(10, 11, 18, 19), (11, 12, 15, 18),
                       (12, 13, 14, 15), (15, 16, 17, 18)]

    verts_front_list = []
    verts_back_list = []
    num_of_verts = len(verts_front_temp)

    # build lists of back and front verts for beamSides function
    for index in range(num_of_verts):
        verts_front_list.append(index)
    for index in range(num_of_verts):
        verts_back_list.append(index + 10)

    faces_side_temp = beamSides(verts_front_list, verts_back_list)

    faces_final = faces_front_temp + faces_back_temp + faces_side_temp

    return verts_final, faces_final


# #####################
# returns:
#  verts_final - a list of tuples of the x, y, z, location of each vertice
#  faces_final - a list of tuples of the vertices that make up each face

def create_I_beam(sRef):

    thick = sRef.beamW

    # Get offset of vertices from center
    x_off = sRef.beamX / 2
    y_off = sRef.beamY / 2
    z_off = sRef.beamZ / 2
    thick_off = thick / 2

    # Create temporarylists to hold vertices locations
    verts_front_temp = []
    verts_back_temp = []

    # Create front vertices by calculation
    verts_front_temp = [
            (-x_off, -y_off, z_off),
            (-thick_off, -y_off, z_off),
            (thick_off, -y_off, z_off),
            (x_off, -y_off, z_off),
            (x_off, -y_off, z_off - thick),
            (thick_off, -y_off, z_off - thick),
            (thick_off, -y_off, -z_off + thick),
            (x_off, -y_off, -z_off + thick),
            (x_off, -y_off, -z_off),
            (thick_off, -y_off, -z_off),
            (-thick_off, -y_off, -z_off),
            (-x_off, -y_off, -z_off),
            (-x_off, -y_off, -z_off + thick),
            (-thick_off, -y_off, -z_off + thick),
            (-thick_off, -y_off, z_off - thick),
            (-x_off, -y_off, z_off - thick)
            ]

    # Adjust taper
    vert_outside = verts_front_temp[0]
    vert_inside = verts_front_temp[15]
    vert_taper = (beamSlant(sRef, vert_outside[2], vert_inside[2]))
    verts_front_temp[15] = [vert_inside[0], vert_inside[1], vert_taper]

    vert_outside = verts_front_temp[3]
    vert_inside = verts_front_temp[4]
    vert_taper = (beamSlant(sRef, vert_outside[2], vert_inside[2]))
    verts_front_temp[4] = [vert_inside[0], vert_inside[1], vert_taper]

    vert_outside = verts_front_temp[8]
    vert_inside = verts_front_temp[7]
    vert_taper = (beamSlant(sRef, vert_outside[2], vert_inside[2]))
    verts_front_temp[7] = [vert_inside[0], vert_inside[1], vert_taper]

    vert_outside = verts_front_temp[11]
    vert_inside = verts_front_temp[12]
    vert_taper = (beamSlant(sRef, vert_outside[2], vert_inside[2]))
    verts_front_temp[12] = [vert_inside[0], vert_inside[1], vert_taper]

    # Create back vertices by calculation
    verts_back_temp = [
            (-x_off, y_off, z_off),
            (-thick_off, y_off, z_off),
            (thick_off, y_off, z_off),
            (x_off, y_off, z_off),
            (x_off, y_off, z_off - thick),
            (thick_off, y_off, z_off - thick),
            (thick_off, y_off, -z_off + thick),
            (x_off, y_off, -z_off + thick),
            (x_off, y_off, -z_off),
            (thick_off, y_off, -z_off),
            (-thick_off, y_off, -z_off),
            (-x_off, y_off, -z_off),
            (-x_off, y_off, -z_off + thick),
            (-thick_off, y_off, -z_off + thick),
            (-thick_off, y_off, z_off - thick),
            (-x_off, y_off, z_off - thick)
            ]

    # Adjust taper
    vert_outside = verts_back_temp[0]
    vert_inside = verts_back_temp[15]
    vert_taper = (beamSlant(sRef, vert_outside[2], vert_inside[2]))
    verts_back_temp[15] = [vert_inside[0], vert_inside[1], vert_taper]

    vert_outside = verts_back_temp[3]
    vert_inside = verts_back_temp[4]
    vert_taper = (beamSlant(sRef, vert_outside[2], vert_inside[2]))
    verts_back_temp[4] = [vert_inside[0], vert_inside[1], vert_taper]

    vert_outside = verts_back_temp[8]
    vert_inside = verts_back_temp[7]
    vert_taper = (beamSlant(sRef, vert_outside[2], vert_inside[2]))
    verts_back_temp[7] = [vert_inside[0], vert_inside[1], vert_taper]

    vert_outside = verts_back_temp[11]
    vert_inside = verts_back_temp[12]
    vert_taper = (beamSlant(sRef, vert_outside[2], vert_inside[2]))
    verts_back_temp[12] = [vert_inside[0], vert_inside[1], vert_taper]

    verts_final = verts_front_temp + verts_back_temp

# define end faces, only 7 per end, so just coded
    faces_front_temp = []
    faces_back_temp = []
    faces_side_temp = []

    faces_front_temp = [(0, 1, 14, 15), (1, 2, 5, 14),
                        (2, 3, 4, 5), (6, 7, 8, 9),
                        (6, 9, 10, 13), (12, 13, 10, 11),
                        (5, 6, 13, 14)]

    faces_back_temp = [(16, 17, 30, 31), (17, 18, 21, 30),
                       (18, 19, 20, 21), (22, 23, 24, 25),
                       (22, 25, 26, 29), (28, 29, 26, 27),
                       (21, 22, 29, 30)]

    verts_front_list = []
    verts_back_list = []
    num_of_verts = len(verts_front_temp)

    # build lists of back and front verts for beamSides function
    for index in range(num_of_verts):
        verts_front_list.append(index)
    for index in range(num_of_verts):
        verts_back_list.append(index + 16)

    faces_side_temp = beamSides(verts_front_list, verts_back_list)

    faces_final = faces_front_temp + faces_back_temp + faces_side_temp

    return verts_final, faces_final


# ######################
#
# Generate beam object.

def addBeamObj(sRef, context):
    verts = []
    faces = []

    # type of beam to add
    if sRef.Type == '0':
        verts, faces = create_beam(sRef)
    elif sRef.Type == '1':
        verts, faces = create_u_beam(sRef)
    elif sRef.Type == '2':
        verts, faces = create_u_beam(sRef)
    elif sRef.Type == '3':
        verts, faces = create_L_beam(sRef)
    elif sRef.Type == '4':
        verts, faces = create_I_beam(sRef)
    elif sRef.Type == '5':
        verts, faces = create_T_beam(sRef)
    else:  # unknown type, use default.
        verts, faces = create_beam(sRef)

    beamMesh = bpy.data.meshes.new("Beam")
    beamObj = bpy.data.objects.new("Beam", beamMesh)
    context.scene.objects.link(beamObj)
    context.scene.objects.active = beamObj
    beamObj.select = True

    beamMesh.from_pydata(verts, [], faces)
    beamMesh.update(calc_edges=True)

    if sRef.Type == '2':  # Rotate C shape
        bpy.ops.transform.rotate(value=1.570796, constraint_axis=[False, True, False])
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)

    if sRef.Cursor:
        if beamObj.select is True:
            # we also have to check if we're considered to be in 3D View (view3d)
            if bpy.ops.view3d.snap_selected_to_cursor.poll():
                bpy.ops.view3d.snap_selected_to_cursor()
            else:
                sRef.Cursor = False


# ######################
# Create a beam primitive.
#
#  UI functions and object creation.

class addBeam(Operator):
    bl_idname = "mesh.add_beam"
    bl_label = "Beam Builder"
    bl_description = "Create beam meshes of various profiles"
    bl_options = {'REGISTER', 'UNDO'}

    Type = EnumProperty(
            items=(
            ('0', "Box Profile", "Square Beam"),
            ("1", "U Profile", "U Profile Beam"),
            ("2", "C Profile", "C Profile Beam"),
            ("3", "L Profile", "L Profile Beam"),
            ("4", "I Profile", "I Profile Beam"),
            ("5", "T Profile", "T Profile Beam")
            ),
            description="Beam form"
            )
    beamZ = FloatProperty(
            name="Height",
            min=0.01, max=100,
            default=1
            )
    beamX = FloatProperty(
            name="Width",
            min=0.01, max=100,
            default=.5
            )
    beamY = FloatProperty(
            name="Depth",
            min=0.01,
            max=100,
            default=2
            )
    beamW = FloatProperty(
            name="Thickness",
            min=0.01, max=1,
            default=0.1
            )
    edgeA = IntProperty(
            name="Taper",
            min=0, max=100,
            default=0,
            description="Angle beam edges"
            )
    Cursor = BoolProperty(
            name="Use 3D Cursor",
            default=False,
            description="Draw the beam where the 3D Cursor is"
            )

    def draw(self, context):
        layout = self.layout

        box = layout.box()
        split = box.split(percentage=0.85, align=True)
        split.prop(self, "Type", text="")
        split.prop(self, "Cursor", text="", icon="CURSOR")

        box.prop(self, "beamZ")
        box.prop(self, "beamX")
        box.prop(self, "beamY")
        box.prop(self, "beamW")

        if self.Type != '0':
            box.prop(self, "edgeA")

    def execute(self, context):
        if bpy.context.mode == "OBJECT":
            addBeamObj(self, context)
            return {'FINISHED'}

        self.report({'WARNING'}, "Option only valid in Object mode")
        return {'CANCELLED'}
