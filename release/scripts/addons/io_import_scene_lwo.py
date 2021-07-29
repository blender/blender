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

bl_info= {
    "name": "Import LightWave Objects",
    "author": "Ken Nign (Ken9)",
    "version": (1, 2),
    "blender": (2, 57, 0),
    "location": "File > Import > LightWave Object (.lwo)",
    "description": "Imports a LWO file including any UV, Morph and Color maps. "
                   "Can convert Skelegons to an Armature.",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Import-Export/LightWave_Object",
    "category": "Import-Export",
}

# Copyright (c) Ken Nign 2010
# ken@virginpi.com
#
# Version 1.3 - Aug 11, 2011
#
# Loads a LightWave .lwo object file, including the vertex maps such as
# UV, Morph, Color and Weight maps.
#
# Will optionally create an Armature from an embedded Skelegon rig.
#
# Point orders are maintained so that .mdds can exchanged with other
# 3D programs.
#
#
# Notes:
# NGons, polygons with more than 4 points are supported, but are
# added (as triangles) after the vertex maps have been applied. Thus they
# won't contain all the vertex data that the original ngon had.
#
# Blender is limited to only 8 UV Texture and 8 Vertex Color maps,
# thus only the first 8 of each can be imported.
#
# History:
#
# 1.3 Fixed CC Edge Weight loading.
#
# 1.2 Added Absolute Morph and CC Edge Weight support.
#     Made edge creation safer.
# 1.0 First Release


import os
import struct
import chunk

import bpy
import mathutils
from mathutils.geometry import tessellate_polygon


class _obj_layer(object):
    __slots__ = (
        "name",
        "index",
        "parent_index",
        "pivot",
        "pols",
        "bones",
        "bone_names",
        "bone_rolls",
        "pnts",
        "wmaps",
        "colmaps",
        "uvmaps",
        "morphs",
        "edge_weights",
        "surf_tags",
        "has_subds",
        )
    def __init__(self):
        self.name= ""
        self.index= -1
        self.parent_index= -1
        self.pivot= [0, 0, 0]
        self.pols= []
        self.bones= []
        self.bone_names= {}
        self.bone_rolls= {}
        self.pnts= []
        self.wmaps= {}
        self.colmaps= {}
        self.uvmaps= {}
        self.morphs= {}
        self.edge_weights= {}
        self.surf_tags= {}
        self.has_subds= False


class _obj_surf(object):
    __slots__ = (
        "bl_mat",
        "name",
        "source_name",
        "colr",
        "diff",
        "lumi",
        "spec",
        "refl",
        "rblr",
        "tran",
        "rind",
        "tblr",
        "trnl",
        "glos",
        "shrp",
        "smooth",
        )

    def __init__(self):
        self.bl_mat= None
        self.name= "Default"
        self.source_name= ""
        self.colr= [1.0, 1.0, 1.0]
        self.diff= 1.0   # Diffuse
        self.lumi= 0.0   # Luminosity
        self.spec= 0.0   # Specular
        self.refl= 0.0   # Reflectivity
        self.rblr= 0.0   # Reflection Bluring
        self.tran= 0.0   # Transparency (the opposite of Blender's Alpha value)
        self.rind= 1.0   # RT Transparency IOR
        self.tblr= 0.0   # Refraction Bluring
        self.trnl= 0.0   # Translucency
        self.glos= 0.4   # Glossiness
        self.shrp= 0.0   # Diffuse Sharpness
        self.smooth= False  # Surface Smoothing


def load_lwo(filename,
             context,
             ADD_SUBD_MOD=True,
             LOAD_HIDDEN=False,
             SKEL_TO_ARM=True,
             USE_EXISTING_MATERIALS=False):
    """Read the LWO file, hand off to version specific function."""
    name, ext= os.path.splitext(os.path.basename(filename))
    file= open(filename, 'rb')

    try:
        header, chunk_size, chunk_name = struct.unpack(">4s1L4s", file.read(12))
    except:
        print("Error parsing file header!")
        file.close()
        return

    layers= []
    surfs= {}
    tags= []
    # Gather the object data using the version specific handler.
    if chunk_name == b'LWO2':
        read_lwo2(file, filename, layers, surfs, tags, ADD_SUBD_MOD, LOAD_HIDDEN, SKEL_TO_ARM)
    elif chunk_name == b'LWOB' or chunk_name == b'LWLO':
        # LWOB and LWLO are the old format, LWLO is a layered object.
        read_lwob(file, filename, layers, surfs, tags, ADD_SUBD_MOD)
    else:
        print("Not a supported file type!")
        file.close()
        return

    file.close()

    # With the data gathered, build the object(s).
    build_objects(layers, surfs, tags, name, ADD_SUBD_MOD, SKEL_TO_ARM, USE_EXISTING_MATERIALS)

    layers= None
    surfs.clear()
    tags= None


def read_lwo2(file, filename, layers, surfs, tags, add_subd_mod, load_hidden, skel_to_arm):
    """Read version 2 file, LW 6+."""
    handle_layer= True
    last_pols_count= 0
    just_read_bones= False
    print("Importing LWO: " + filename + "\nLWO v2 Format")

    while True:
        try:
            rootchunk = chunk.Chunk(file)
        except EOFError:
            break

        if rootchunk.chunkname == b'TAGS':
            read_tags(rootchunk.read(), tags)
        elif rootchunk.chunkname == b'LAYR':
            handle_layer= read_layr(rootchunk.read(), layers, load_hidden)
        elif rootchunk.chunkname == b'PNTS' and handle_layer:
            read_pnts(rootchunk.read(), layers)
        elif rootchunk.chunkname == b'VMAP' and handle_layer:
            vmap_type = rootchunk.read(4)

            if vmap_type == b'WGHT':
                read_weightmap(rootchunk.read(), layers)
            elif vmap_type == b'MORF':
                read_morph(rootchunk.read(), layers, False)
            elif vmap_type == b'SPOT':
                read_morph(rootchunk.read(), layers, True)
            elif vmap_type == b'TXUV':
                read_uvmap(rootchunk.read(), layers)
            elif vmap_type == b'RGB ' or vmap_type == b'RGBA':
                read_colmap(rootchunk.read(), layers)
            else:
                rootchunk.skip()

        elif rootchunk.chunkname == b'VMAD' and handle_layer:
            vmad_type= rootchunk.read(4)

            if vmad_type == b'TXUV':
                read_uv_vmad(rootchunk.read(), layers, last_pols_count)
            elif vmad_type == b'RGB ' or vmad_type == b'RGBA':
                read_color_vmad(rootchunk.read(), layers, last_pols_count)
            elif vmad_type == b'WGHT':
                # We only read the Edge Weight map if it's there.
                read_weight_vmad(rootchunk.read(), layers)
            else:
                rootchunk.skip()

        elif rootchunk.chunkname == b'POLS' and handle_layer:
            face_type = rootchunk.read(4)
            just_read_bones= False
            # PTCH is LW's Subpatches, SUBD is CatmullClark.
            if (face_type == b'FACE' or face_type == b'PTCH' or
                face_type == b'SUBD') and handle_layer:
                last_pols_count= read_pols(rootchunk.read(), layers)
                if face_type != b'FACE':
                    layers[-1].has_subds= True
            elif face_type == b'BONE' and handle_layer:
                read_bones(rootchunk.read(), layers)
                just_read_bones= True
            else:
                rootchunk.skip()

        elif rootchunk.chunkname == b'PTAG' and handle_layer:
            tag_type,= struct.unpack("4s", rootchunk.read(4))
            if tag_type == b'SURF' and not just_read_bones:
                # Ignore the surface data if we just read a bones chunk.
                read_surf_tags(rootchunk.read(), layers, last_pols_count)

            elif skel_to_arm:
                if tag_type == b'BNUP':
                    read_bone_tags(rootchunk.read(), layers, tags, 'BNUP')
                elif tag_type == b'BONE':
                    read_bone_tags(rootchunk.read(), layers, tags, 'BONE')
                else:
                    rootchunk.skip()
            else:
                rootchunk.skip()
        elif rootchunk.chunkname == b'SURF':
            read_surf(rootchunk.read(), surfs)
        else:
            #if handle_layer:
                #print("Skipping Chunk:", rootchunk.chunkname)
            rootchunk.skip()


def read_lwob(file, filename, layers, surfs, tags, add_subd_mod):
    """Read version 1 file, LW < 6."""
    last_pols_count= 0
    print("Importing LWO: " + filename + "\nLWO v1 Format")

    while True:
        try:
            rootchunk = chunk.Chunk(file)
        except EOFError:
            break

        if rootchunk.chunkname == b'SRFS':
            read_tags(rootchunk.read(), tags)
        elif rootchunk.chunkname == b'LAYR':
            read_layr_5(rootchunk.read(), layers)
        elif rootchunk.chunkname == b'PNTS':
            if len(layers) == 0:
                # LWOB files have no LAYR chunk to set this up.
                nlayer= _obj_layer()
                nlayer.name= "Layer 1"
                layers.append(nlayer)
            read_pnts(rootchunk.read(), layers)
        elif rootchunk.chunkname == b'POLS':
            last_pols_count= read_pols_5(rootchunk.read(), layers)
        elif rootchunk.chunkname == b'PCHS':
            last_pols_count= read_pols_5(rootchunk.read(), layers)
            layers[-1].has_subds= True
        elif rootchunk.chunkname == b'PTAG':
            tag_type,= struct.unpack("4s", rootchunk.read(4))
            if tag_type == b'SURF':
                read_surf_tags_5(rootchunk.read(), layers, last_pols_count)
            else:
                rootchunk.skip()
        elif rootchunk.chunkname == b'SURF':
            read_surf_5(rootchunk.read(), surfs)
        else:
            # For Debugging \/.
            #if handle_layer:
                #print("Skipping Chunk: ", rootchunk.chunkname)
            rootchunk.skip()


def read_lwostring(raw_name):
    """Parse a zero-padded string."""

    i = raw_name.find(b'\0')
    name_len = i + 1
    if name_len % 2 == 1:   # Test for oddness.
        name_len += 1

    if i > 0:
        # Some plugins put non-text strings in the tags chunk.
        name = raw_name[0:i].decode("utf-8", "ignore")
    else:
        name = ""

    return name, name_len


def read_vx(pointdata):
    """Read a variable-length index."""
    if pointdata[0] != 255:
        index= pointdata[0]*256 + pointdata[1]
        size= 2
    else:
        index= pointdata[1]*65536 + pointdata[2]*256 + pointdata[3]
        size= 4

    return index, size


def read_tags(tag_bytes, object_tags):
    """Read the object's Tags chunk."""
    offset= 0
    chunk_len= len(tag_bytes)

    while offset < chunk_len:
        tag, tag_len= read_lwostring(tag_bytes[offset:])
        offset+= tag_len
        object_tags.append(tag)


def read_layr(layr_bytes, object_layers, load_hidden):
    """Read the object's layer data."""
    new_layr= _obj_layer()
    new_layr.index, flags= struct.unpack(">HH", layr_bytes[0:4])

    if flags > 0 and not load_hidden:
        return False

    print("Reading Object Layer")
    offset= 4
    pivot= struct.unpack(">fff", layr_bytes[offset:offset+12])
    # Swap Y and Z to match Blender's pitch.
    new_layr.pivot= [pivot[0], pivot[2], pivot[1]]
    offset+= 12
    layr_name, name_len = read_lwostring(layr_bytes[offset:])
    offset+= name_len

    if layr_name:
        new_layr.name= layr_name
    else:
        new_layr.name= "Layer %d" % (new_layr.index + 1)

    if len(layr_bytes) == offset+2:
        new_layr.parent_index,= struct.unpack(">h", layr_bytes[offset:offset+2])

    object_layers.append(new_layr)
    return True


def read_layr_5(layr_bytes, object_layers):
    """Read the object's layer data."""
    # XXX: Need to check what these two exactly mean for a LWOB/LWLO file.
    new_layr= _obj_layer()
    new_layr.index, flags= struct.unpack(">HH", layr_bytes[0:4])

    print("Reading Object Layer")
    offset= 4
    layr_name, name_len = read_lwostring(layr_bytes[offset:])
    offset+= name_len

    if name_len > 2 and layr_name != 'noname':
        new_layr.name= layr_name
    else:
        new_layr.name= "Layer %d" % new_layr.index

    object_layers.append(new_layr)


def read_pnts(pnt_bytes, object_layers):
    """Read the layer's points."""
    print("\tReading Layer ("+object_layers[-1].name+") Points")
    offset= 0
    chunk_len= len(pnt_bytes)

    while offset < chunk_len:
        pnts= struct.unpack(">fff", pnt_bytes[offset:offset+12])
        offset+= 12
        # Re-order the points so that the mesh has the right pitch,
        # the pivot already has the correct order.
        pnts= [pnts[0] - object_layers[-1].pivot[0],\
               pnts[2] - object_layers[-1].pivot[1],\
               pnts[1] - object_layers[-1].pivot[2]]
        object_layers[-1].pnts.append(pnts)


def read_weightmap(weight_bytes, object_layers):
    """Read a weight map's values."""
    chunk_len= len(weight_bytes)
    offset= 2
    name, name_len= read_lwostring(weight_bytes[offset:])
    offset+= name_len
    weights= []

    while offset < chunk_len:
        pnt_id, pnt_id_len= read_vx(weight_bytes[offset:offset+4])
        offset+= pnt_id_len
        value,= struct.unpack(">f", weight_bytes[offset:offset+4])
        offset+= 4
        weights.append([pnt_id, value])

    object_layers[-1].wmaps[name]= weights


def read_morph(morph_bytes, object_layers, is_abs):
    """Read an endomorph's relative or absolute displacement values."""
    chunk_len= len(morph_bytes)
    offset= 2
    name, name_len= read_lwostring(morph_bytes[offset:])
    offset+= name_len
    deltas= []

    while offset < chunk_len:
        pnt_id, pnt_id_len= read_vx(morph_bytes[offset:offset+4])
        offset+= pnt_id_len
        pos= struct.unpack(">fff", morph_bytes[offset:offset+12])
        offset+= 12
        pnt= object_layers[-1].pnts[pnt_id]

        if is_abs:
            deltas.append([pnt_id, pos[0], pos[2], pos[1]])
        else:
            # Swap the Y and Z to match Blender's pitch.
            deltas.append([pnt_id, pnt[0]+pos[0], pnt[1]+pos[2], pnt[2]+pos[1]])

        object_layers[-1].morphs[name]= deltas


def read_colmap(col_bytes, object_layers):
    """Read the RGB or RGBA color map."""
    chunk_len= len(col_bytes)
    dia,= struct.unpack(">H", col_bytes[0:2])
    offset= 2
    name, name_len= read_lwostring(col_bytes[offset:])
    offset+= name_len
    colors= {}

    if dia == 3:
        while offset < chunk_len:
            pnt_id, pnt_id_len= read_vx(col_bytes[offset:offset+4])
            offset+= pnt_id_len
            col= struct.unpack(">fff", col_bytes[offset:offset+12])
            offset+= 12
            colors[pnt_id]= (col[0], col[1], col[2])
    elif dia == 4:
        while offset < chunk_len:
            pnt_id, pnt_id_len= read_vx(col_bytes[offset:offset+4])
            offset+= pnt_id_len
            col= struct.unpack(">ffff", col_bytes[offset:offset+16])
            offset+= 16
            colors[pnt_id]= (col[0], col[1], col[2])

    if name in object_layers[-1].colmaps:
        if "PointMap" in object_layers[-1].colmaps[name]:
            object_layers[-1].colmaps[name]["PointMap"].update(colors)
        else:
            object_layers[-1].colmaps[name]["PointMap"]= colors
    else:
        object_layers[-1].colmaps[name]= dict(PointMap=colors)


def read_color_vmad(col_bytes, object_layers, last_pols_count):
    """Read the Discontinous (per-polygon) RGB values."""
    chunk_len= len(col_bytes)
    dia,= struct.unpack(">H", col_bytes[0:2])
    offset= 2
    name, name_len= read_lwostring(col_bytes[offset:])
    offset+= name_len
    colors= {}
    abs_pid= len(object_layers[-1].pols) - last_pols_count

    if dia == 3:
        while offset < chunk_len:
            pnt_id, pnt_id_len= read_vx(col_bytes[offset:offset+4])
            offset+= pnt_id_len
            pol_id, pol_id_len= read_vx(col_bytes[offset:offset+4])
            offset+= pol_id_len

            # The PolyID in a VMAD can be relative, this offsets it.
            pol_id+= abs_pid
            col= struct.unpack(">fff", col_bytes[offset:offset+12])
            offset+= 12
            if pol_id in colors:
                colors[pol_id][pnt_id]= (col[0], col[1], col[2])
            else:
                colors[pol_id]= dict({pnt_id: (col[0], col[1], col[2])})
    elif dia == 4:
        while offset < chunk_len:
            pnt_id, pnt_id_len= read_vx(col_bytes[offset:offset+4])
            offset+= pnt_id_len
            pol_id, pol_id_len= read_vx(col_bytes[offset:offset+4])
            offset+= pol_id_len

            pol_id+= abs_pid
            col= struct.unpack(">ffff", col_bytes[offset:offset+16])
            offset+= 16
            if pol_id in colors:
                colors[pol_id][pnt_id]= (col[0], col[1], col[2])
            else:
                colors[pol_id]= dict({pnt_id: (col[0], col[1], col[2])})

    if name in object_layers[-1].colmaps:
        if "FaceMap" in object_layers[-1].colmaps[name]:
            object_layers[-1].colmaps[name]["FaceMap"].update(colors)
        else:
            object_layers[-1].colmaps[name]["FaceMap"]= colors
    else:
        object_layers[-1].colmaps[name]= dict(FaceMap=colors)


def read_uvmap(uv_bytes, object_layers):
    """Read the simple UV coord values."""
    chunk_len= len(uv_bytes)
    offset= 2
    name, name_len= read_lwostring(uv_bytes[offset:])
    offset+= name_len
    uv_coords= {}

    while offset < chunk_len:
        pnt_id, pnt_id_len= read_vx(uv_bytes[offset:offset+4])
        offset+= pnt_id_len
        pos= struct.unpack(">ff", uv_bytes[offset:offset+8])
        offset+= 8
        uv_coords[pnt_id]= (pos[0], pos[1])

    if name in object_layers[-1].uvmaps:
        if "PointMap" in object_layers[-1].uvmaps[name]:
            object_layers[-1].uvmaps[name]["PointMap"].update(uv_coords)
        else:
            object_layers[-1].uvmaps[name]["PointMap"]= uv_coords
    else:
        object_layers[-1].uvmaps[name]= dict(PointMap=uv_coords)


def read_uv_vmad(uv_bytes, object_layers, last_pols_count):
    """Read the Discontinous (per-polygon) uv values."""
    chunk_len= len(uv_bytes)
    offset= 2
    name, name_len= read_lwostring(uv_bytes[offset:])
    offset+= name_len
    uv_coords= {}
    abs_pid= len(object_layers[-1].pols) - last_pols_count

    while offset < chunk_len:
        pnt_id, pnt_id_len= read_vx(uv_bytes[offset:offset+4])
        offset+= pnt_id_len
        pol_id, pol_id_len= read_vx(uv_bytes[offset:offset+4])
        offset+= pol_id_len

        pol_id+= abs_pid
        pos= struct.unpack(">ff", uv_bytes[offset:offset+8])
        offset+= 8
        if pol_id in uv_coords:
            uv_coords[pol_id][pnt_id]= (pos[0], pos[1])
        else:
            uv_coords[pol_id]= dict({pnt_id: (pos[0], pos[1])})

    if name in object_layers[-1].uvmaps:
        if "FaceMap" in object_layers[-1].uvmaps[name]:
            object_layers[-1].uvmaps[name]["FaceMap"].update(uv_coords)
        else:
            object_layers[-1].uvmaps[name]["FaceMap"]= uv_coords
    else:
        object_layers[-1].uvmaps[name]= dict(FaceMap=uv_coords)


def read_weight_vmad(ew_bytes, object_layers):
    """Read the VMAD Weight values."""
    chunk_len= len(ew_bytes)
    offset= 2
    name, name_len= read_lwostring(ew_bytes[offset:])
    if name != "Edge Weight":
        return  # We just want the Catmull-Clark edge weights

    offset+= name_len
    # Some info: LW stores a face's points in a clock-wize order (with the
    # normal pointing at you). This gives edges a 'direction' which is used
    # when it comes to storing CC edge weight values. The weight is given
    # to the point preceding the edge that the weight belongs to.
    while offset < chunk_len:
        pnt_id, pnt_id_len = read_vx(ew_bytes[offset:offset+4])
        offset+= pnt_id_len
        pol_id, pol_id_len= read_vx(ew_bytes[offset:offset+4])
        offset+= pol_id_len
        weight,= struct.unpack(">f", ew_bytes[offset:offset+4])
        offset+= 4

        face_pnts= object_layers[-1].pols[pol_id]
        try:
            # Find the point's location in the polygon's point list
            first_idx= face_pnts.index(pnt_id)
        except:
            continue

        # Then get the next point in the list, or wrap around to the first
        if first_idx == len(face_pnts) - 1:
            second_pnt= face_pnts[0]
        else:
            second_pnt= face_pnts[first_idx + 1]

        object_layers[-1].edge_weights["{0} {1}".format(second_pnt, pnt_id)]= weight


def read_pols(pol_bytes, object_layers):
    """Read the layer's polygons, each one is just a list of point indexes."""
    print("\tReading Layer ("+object_layers[-1].name+") Polygons")
    offset= 0
    pols_count = len(pol_bytes)
    old_pols_count= len(object_layers[-1].pols)

    while offset < pols_count:
        pnts_count,= struct.unpack(">H", pol_bytes[offset:offset+2])
        offset+= 2
        all_face_pnts= []
        for j in range(pnts_count):
            face_pnt, data_size= read_vx(pol_bytes[offset:offset+4])
            offset+= data_size
            all_face_pnts.append(face_pnt)

        object_layers[-1].pols.append(all_face_pnts)

    return len(object_layers[-1].pols) - old_pols_count


def read_pols_5(pol_bytes, object_layers):
    """
    Read the polygons, each one is just a list of point indexes.
    But it also includes the surface index.
    """
    print("\tReading Layer ("+object_layers[-1].name+") Polygons")
    offset= 0
    chunk_len= len(pol_bytes)
    old_pols_count= len(object_layers[-1].pols)
    poly= 0

    while offset < chunk_len:
        pnts_count,= struct.unpack(">H", pol_bytes[offset:offset+2])
        offset+= 2
        all_face_pnts= []
        for j in range(pnts_count):
            face_pnt,= struct.unpack(">H", pol_bytes[offset:offset+2])
            offset+= 2
            all_face_pnts.append(face_pnt)

        object_layers[-1].pols.append(all_face_pnts)
        sid,= struct.unpack(">h", pol_bytes[offset:offset+2])
        offset+= 2
        sid= abs(sid) - 1
        if sid not in object_layers[-1].surf_tags:
            object_layers[-1].surf_tags[sid]= []
        object_layers[-1].surf_tags[sid].append(poly)
        poly+= 1

    return len(object_layers[-1].pols) - old_pols_count


def read_bones(bone_bytes, object_layers):
    """Read the layer's skelegons."""
    print("\tReading Layer ("+object_layers[-1].name+") Bones")
    offset= 0
    bones_count = len(bone_bytes)

    while offset < bones_count:
        pnts_count,= struct.unpack(">H", bone_bytes[offset:offset+2])
        offset+= 2
        all_bone_pnts= []
        for j in range(pnts_count):
            bone_pnt, data_size= read_vx(bone_bytes[offset:offset+4])
            offset+= data_size
            all_bone_pnts.append(bone_pnt)

        object_layers[-1].bones.append(all_bone_pnts)


def read_bone_tags(tag_bytes, object_layers, object_tags, type):
    """Read the bone name or roll tags."""
    offset= 0
    chunk_len= len(tag_bytes)

    if type == 'BONE':
        bone_dict= object_layers[-1].bone_names
    elif type == 'BNUP':
        bone_dict= object_layers[-1].bone_rolls
    else:
        return

    while offset < chunk_len:
        pid, pid_len= read_vx(tag_bytes[offset:offset+4])
        offset+= pid_len
        tid,= struct.unpack(">H", tag_bytes[offset:offset+2])
        offset+= 2
        bone_dict[pid]= object_tags[tid]


def read_surf_tags(tag_bytes, object_layers, last_pols_count):
    """Read the list of PolyIDs and tag indexes."""
    print("\tReading Layer ("+object_layers[-1].name+") Surface Assignments")
    offset= 0
    chunk_len= len(tag_bytes)

    # Read in the PolyID/Surface Index pairs.
    abs_pid= len(object_layers[-1].pols) - last_pols_count
    while offset < chunk_len:
        pid, pid_len= read_vx(tag_bytes[offset:offset+4])
        offset+= pid_len
        sid,= struct.unpack(">H", tag_bytes[offset:offset+2])
        offset+=2
        if sid not in object_layers[-1].surf_tags:
            object_layers[-1].surf_tags[sid]= []
        object_layers[-1].surf_tags[sid].append(pid + abs_pid)


def read_surf(surf_bytes, object_surfs):
    """Read the object's surface data."""
    if len(object_surfs) == 0:
        print("Reading Object Surfaces")

    surf= _obj_surf()
    name, name_len= read_lwostring(surf_bytes)
    if len(name) != 0:
        surf.name = name

    # We have to read this, but we won't use it...yet.
    s_name, s_name_len= read_lwostring(surf_bytes[name_len:])
    offset= name_len+s_name_len
    block_size= len(surf_bytes)
    while offset < block_size:
        subchunk_name,= struct.unpack("4s", surf_bytes[offset:offset+4])
        offset+= 4
        subchunk_len,= struct.unpack(">H", surf_bytes[offset:offset+2])
        offset+= 2

        # Now test which subchunk it is.
        if subchunk_name == b'COLR':
            surf.colr= struct.unpack(">fff", surf_bytes[offset:offset+12])
            # Don't bother with any envelopes for now.

        elif subchunk_name == b'DIFF':
            surf.diff,= struct.unpack(">f", surf_bytes[offset:offset+4])

        elif subchunk_name == b'LUMI':
            surf.lumi,= struct.unpack(">f", surf_bytes[offset:offset+4])

        elif subchunk_name == b'SPEC':
            surf.spec,= struct.unpack(">f", surf_bytes[offset:offset+4])

        elif subchunk_name == b'REFL':
            surf.refl,= struct.unpack(">f", surf_bytes[offset:offset+4])

        elif subchunk_name == b'RBLR':
            surf.rblr,= struct.unpack(">f", surf_bytes[offset:offset+4])

        elif subchunk_name == b'TRAN':
            surf.tran,= struct.unpack(">f", surf_bytes[offset:offset+4])

        elif subchunk_name == b'RIND':
            surf.rind,= struct.unpack(">f", surf_bytes[offset:offset+4])

        elif subchunk_name == b'TBLR':
            surf.tblr,= struct.unpack(">f", surf_bytes[offset:offset+4])

        elif subchunk_name == b'TRNL':
            surf.trnl,= struct.unpack(">f", surf_bytes[offset:offset+4])

        elif subchunk_name == b'GLOS':
            surf.glos,= struct.unpack(">f", surf_bytes[offset:offset+4])

        elif subchunk_name == b'SHRP':
            surf.shrp,= struct.unpack(">f", surf_bytes[offset:offset+4])

        elif subchunk_name == b'SMAN':
            s_angle,= struct.unpack(">f", surf_bytes[offset:offset+4])
            if s_angle > 0.0:
                surf.smooth = True

        offset+= subchunk_len

    object_surfs[surf.name]= surf


def read_surf_5(surf_bytes, object_surfs):
    """Read the object's surface data."""
    if len(object_surfs) == 0:
        print("Reading Object Surfaces")

    surf= _obj_surf()
    name, name_len= read_lwostring(surf_bytes)
    if len(name) != 0:
        surf.name = name

    offset= name_len
    chunk_len= len(surf_bytes)
    while offset < chunk_len:
        subchunk_name,= struct.unpack("4s", surf_bytes[offset:offset+4])
        offset+= 4
        subchunk_len,= struct.unpack(">H", surf_bytes[offset:offset+2])
        offset+= 2

        # Now test which subchunk it is.
        if subchunk_name == b'COLR':
            color= struct.unpack(">BBBB", surf_bytes[offset:offset+4])
            surf.colr= [color[0] / 255.0, color[1] / 255.0, color[2] / 255.0]

        elif subchunk_name == b'DIFF':
            surf.diff,= struct.unpack(">h", surf_bytes[offset:offset+2])
            surf.diff/= 256.0    # Yes, 256 not 255.

        elif subchunk_name == b'LUMI':
            surf.lumi,= struct.unpack(">h", surf_bytes[offset:offset+2])
            surf.lumi/= 256.0

        elif subchunk_name == b'SPEC':
            surf.spec,= struct.unpack(">h", surf_bytes[offset:offset+2])
            surf.spec/= 256.0

        elif subchunk_name == b'REFL':
            surf.refl,= struct.unpack(">h", surf_bytes[offset:offset+2])
            surf.refl/= 256.0

        elif subchunk_name == b'TRAN':
            surf.tran,= struct.unpack(">h", surf_bytes[offset:offset+2])
            surf.tran/= 256.0

        elif subchunk_name == b'RIND':
            surf.rind,= struct.unpack(">f", surf_bytes[offset:offset+4])

        elif subchunk_name == b'GLOS':
            surf.glos,= struct.unpack(">h", surf_bytes[offset:offset+2])

        elif subchunk_name == b'SMAN':
            s_angle,= struct.unpack(">f", surf_bytes[offset:offset+4])
            if s_angle > 0.0:
                surf.smooth = True

        offset+= subchunk_len

    object_surfs[surf.name]= surf


def create_mappack(data, map_name, map_type):
    """Match the map data to faces."""
    pack= {}

    def color_pointmap(map):
        for fi in range(len(data.pols)):
            if fi not in pack:
                pack[fi]= []
            for pnt in data.pols[fi]:
                if pnt in map:
                    pack[fi].append(map[pnt])
                else:
                    pack[fi].append((1.0, 1.0, 1.0))

    def color_facemap(map):
        for fi in range(len(data.pols)):
            if fi not in pack:
                pack[fi]= []
                for p in data.pols[fi]:
                    pack[fi].append((1.0, 1.0, 1.0))
            if fi in map:
                for po in range(len(data.pols[fi])):
                    if data.pols[fi][po] in map[fi]:
                        pack[fi].insert(po, map[fi][data.pols[fi][po]])
                        del pack[fi][po+1]

    def uv_pointmap(map):
        for fi in range(len(data.pols)):
            if fi not in pack:
                pack[fi]= []
                for p in data.pols[fi]:
                    pack[fi].append((-0.1,-0.1))
            for po in range(len(data.pols[fi])):
                pnt_id= data.pols[fi][po]
                if pnt_id in map:
                    pack[fi].insert(po, map[pnt_id])
                    del pack[fi][po+1]

    def uv_facemap(map):
        for fi in range(len(data.pols)):
            if fi not in pack:
                pack[fi]= []
                for p in data.pols[fi]:
                    pack[fi].append((-0.1,-0.1))
            if fi in map:
                for po in range(len(data.pols[fi])):
                    pnt_id= data.pols[fi][po]
                    if pnt_id in map[fi]:
                        pack[fi].insert(po, map[fi][pnt_id])
                        del pack[fi][po+1]

    if map_type == "COLOR":
        # Look at the first map, is it a point or face map
        if "PointMap" in data.colmaps[map_name]:
            color_pointmap(data.colmaps[map_name]["PointMap"])

        if "FaceMap" in data.colmaps[map_name]:
            color_facemap(data.colmaps[map_name]["FaceMap"])
    elif map_type == "UV":
        if "PointMap" in data.uvmaps[map_name]:
            uv_pointmap(data.uvmaps[map_name]["PointMap"])

        if "FaceMap" in data.uvmaps[map_name]:
            uv_facemap(data.uvmaps[map_name]["FaceMap"])

    return pack


def build_armature(layer_data, bones):
    """Build an armature from the skelegon data in the mesh."""
    print("Building Armature")

    # New Armatures include a default bone, remove it.
    bones.remove(bones[0])

    # Now start adding the bones at the point locations.
    prev_bone= None
    for skb_idx in range(len(layer_data.bones)):
        if skb_idx in layer_data.bone_names:
            nb= bones.new(layer_data.bone_names[skb_idx])
        else:
            nb= bones.new("Bone")

        nb.head= layer_data.pnts[layer_data.bones[skb_idx][0]]
        nb.tail= layer_data.pnts[layer_data.bones[skb_idx][1]]

        if skb_idx in layer_data.bone_rolls:
            xyz= layer_data.bone_rolls[skb_idx].split(' ')
            vec= mathutils.Vector((float(xyz[0]), float(xyz[1]), float(xyz[2])))
            quat= vec.to_track_quat('Y', 'Z')
            nb.roll= max(quat.to_euler('YZX'))
            if nb.roll == 0.0:
                nb.roll= min(quat.to_euler('YZX')) * -1
            # YZX order seems to produce the correct roll value.
        else:
            nb.roll= 0.0

        if prev_bone is not None:
            if nb.head == prev_bone.tail:
                nb.parent= prev_bone

        nb.use_connect= True
        prev_bone= nb


def build_objects(object_layers, object_surfs, object_tags, object_name, add_subd_mod, skel_to_arm, use_existing_materials):
    """Using the gathered data, create the objects."""
    ob_dict= {}  # Used for the parenting setup.
    print("Adding %d Materials" % len(object_surfs))

    for surf_key in object_surfs:
        surf_data= object_surfs[surf_key]
        surf_data.bl_mat = bpy.data.materials.get(surf_data.name) if use_existing_materials else None
        if surf_data.bl_mat is None:
            surf_data.bl_mat= bpy.data.materials.new(surf_data.name)
            surf_data.bl_mat.diffuse_color= (surf_data.colr[:])
            surf_data.bl_mat.diffuse_intensity= surf_data.diff
            surf_data.bl_mat.emit= surf_data.lumi
            surf_data.bl_mat.specular_intensity= surf_data.spec
            if surf_data.refl != 0.0:
                surf_data.bl_mat.raytrace_mirror.use= True
            surf_data.bl_mat.raytrace_mirror.reflect_factor= surf_data.refl
            surf_data.bl_mat.raytrace_mirror.gloss_factor= 1.0-surf_data.rblr
            if surf_data.tran != 0.0:
                surf_data.bl_mat.use_transparency= True
                surf_data.bl_mat.transparency_method= 'RAYTRACE'
            surf_data.bl_mat.alpha= 1.0 - surf_data.tran
            surf_data.bl_mat.raytrace_transparency.ior= surf_data.rind
            surf_data.bl_mat.raytrace_transparency.gloss_factor= 1.0 - surf_data.tblr
            surf_data.bl_mat.translucency= surf_data.trnl
            surf_data.bl_mat.specular_hardness= int(4*((10*surf_data.glos)*(10*surf_data.glos)))+4
        # The Gloss is as close as possible given the differences.

    # Single layer objects use the object file's name instead.
    if len(object_layers) and object_layers[-1].name == 'Layer 1':
        object_layers[-1].name= object_name
        print("Building '%s' Object" % object_name)
    else:
        print("Building %d Objects" % len(object_layers))

    # Before adding any meshes or armatures go into Object mode.
    if bpy.ops.object.mode_set.poll():
        bpy.ops.object.mode_set(mode='OBJECT')

    for layer_data in object_layers:
        me= bpy.data.meshes.new(layer_data.name)
        me.vertices.add(len(layer_data.pnts))
        me.tessfaces.add(len(layer_data.pols))
        # for vi in range(len(layer_data.pnts)):
        #     me.vertices[vi].co= layer_data.pnts[vi]

        # faster, would be faster again to use an array
        me.vertices.foreach_set("co", [axis for co in layer_data.pnts for axis in co])

        ngons= {}   # To keep the FaceIdx consistent, handle NGons later.
        edges= []   # Holds the FaceIdx of the 2-point polys.
        for fi, fpol in enumerate(layer_data.pols):
            fpol.reverse()   # Reversing gives correct normal directions
            # PointID 0 in the last element causes Blender to think it's un-used.
            if fpol[-1] == 0:
                fpol.insert(0, fpol[-1])
                del fpol[-1]

            vlen= len(fpol)
            if vlen == 3 or vlen == 4:
                for i in range(vlen):
                    me.tessfaces[fi].vertices_raw[i]= fpol[i]
            elif vlen == 2:
                edges.append(fi)
            elif vlen != 1:
                ngons[fi]= fpol  # Deal with them later

        ob= bpy.data.objects.new(layer_data.name, me)
        bpy.context.scene.objects.link(ob)
        ob_dict[layer_data.index]= [ob, layer_data.parent_index]

        # Move the object so the pivot is in the right place.
        ob.location= layer_data.pivot

        # Create the Material Slots and assign the MatIndex to the correct faces.
        mat_slot= 0
        for surf_key in layer_data.surf_tags:
            if object_tags[surf_key] in object_surfs:
                me.materials.append(object_surfs[object_tags[surf_key]].bl_mat)

                for fi in layer_data.surf_tags[surf_key]:
                    me.tessfaces[fi].material_index= mat_slot
                    me.tessfaces[fi].use_smooth= object_surfs[object_tags[surf_key]].smooth

                mat_slot+=1

        # Create the Vertex Groups (LW's Weight Maps).
        if len(layer_data.wmaps) > 0:
            print("Adding %d Vertex Groups" % len(layer_data.wmaps))
            for wmap_key in layer_data.wmaps:
                vgroup= ob.vertex_groups.new()
                vgroup.name= wmap_key
                wlist= layer_data.wmaps[wmap_key]
                for pvp in wlist:
                    vgroup.add((pvp[0], ), pvp[1], 'REPLACE')

        # Create the Shape Keys (LW's Endomorphs).
        if len(layer_data.morphs) > 0:
            print("Adding %d Shapes Keys" % len(layer_data.morphs))
            ob.shape_key_add('Basis')   # Got to have a Base Shape.
            for morph_key in layer_data.morphs:
                skey= ob.shape_key_add(morph_key)
                dlist= layer_data.morphs[morph_key]
                for pdp in dlist:
                    me.shape_keys.key_blocks[skey.name].data[pdp[0]].co= [pdp[1], pdp[2], pdp[3]]

        # Create the Vertex Color maps.
        if len(layer_data.colmaps) > 0:
            print("Adding %d Vertex Color Maps" % len(layer_data.colmaps))
            for cmap_key in layer_data.colmaps:
                map_pack= create_mappack(layer_data, cmap_key, "COLOR")
                me.vertex_colors.new(cmap_key)
                vcol= me.tessface_vertex_colors[-1]
                if not vcol or not vcol.data:
                    break
                for fi in map_pack:
                    if fi > len(vcol.data):
                        continue
                    face= map_pack[fi]
                    colf= vcol.data[fi]

                    if len(face) > 2:
                        colf.color1= face[0]
                        colf.color2= face[1]
                        colf.color3= face[2]
                    if len(face) == 4:
                        colf.color4= face[3]

        # Create the UV Maps.
        if len(layer_data.uvmaps) > 0:
            print("Adding %d UV Textures" % len(layer_data.uvmaps))
            for uvmap_key in layer_data.uvmaps:
                map_pack= create_mappack(layer_data, uvmap_key, "UV")
                me.uv_textures.new(name=uvmap_key)
                uvm= me.tessface_uv_textures[-1]
                if not uvm or not uvm.data:
                    break
                for fi in map_pack:
                    if fi > len(uvm.data):
                        continue
                    face= map_pack[fi]
                    uvf= uvm.data[fi]

                    if len(face) > 2:
                        uvf.uv1= face[0]
                        uvf.uv2= face[1]
                        uvf.uv3= face[2]
                    if len(face) == 4:
                        uvf.uv4= face[3]

        # Now add the NGons.
        if len(ngons) > 0:
            for ng_key in ngons:
                face_offset= len(me.tessfaces)
                ng= ngons[ng_key]
                v_locs= []
                for vi in range(len(ng)):
                    v_locs.append(mathutils.Vector(layer_data.pnts[ngons[ng_key][vi]]))
                tris= tessellate_polygon([v_locs])
                me.tessfaces.add(len(tris))
                for tri in tris:
                    face= me.tessfaces[face_offset]
                    face.vertices_raw[0]= ng[tri[0]]
                    face.vertices_raw[1]= ng[tri[1]]
                    face.vertices_raw[2]= ng[tri[2]]
                    face.material_index= me.tessfaces[ng_key].material_index
                    face.use_smooth= me.tessfaces[ng_key].use_smooth
                    face_offset+= 1

        # FaceIDs are no longer a concern, so now update the mesh.
        has_edges= len(edges) > 0 or len(layer_data.edge_weights) > 0
        me.update(calc_edges=has_edges)

        # Add the edges.
        edge_offset= len(me.edges)
        me.edges.add(len(edges))
        for edge_fi in edges:
            me.edges[edge_offset].vertices[0]= layer_data.pols[edge_fi][0]
            me.edges[edge_offset].vertices[1]= layer_data.pols[edge_fi][1]
            edge_offset+= 1

        # Apply the Edge Weighting.
        if len(layer_data.edge_weights) > 0:
            for edge in me.edges:
                edge_sa= "{0} {1}".format(edge.vertices[0], edge.vertices[1])
                edge_sb= "{0} {1}".format(edge.vertices[1], edge.vertices[0])
                if edge_sa in layer_data.edge_weights:
                    edge.crease= layer_data.edge_weights[edge_sa]
                elif edge_sb in layer_data.edge_weights:
                    edge.crease= layer_data.edge_weights[edge_sb]

        # Unfortunately we can't exlude certain faces from the subdivision.
        if layer_data.has_subds and add_subd_mod:
            ob.modifiers.new(name="Subsurf", type='SUBSURF')

        # Should we build an armature from the embedded rig?
        if len(layer_data.bones) > 0 and skel_to_arm:
            bpy.ops.object.armature_add()
            arm_object= bpy.context.active_object
            arm_object.name= "ARM_" + layer_data.name
            arm_object.data.name= arm_object.name
            arm_object.location= layer_data.pivot
            bpy.ops.object.mode_set(mode='EDIT')
            build_armature(layer_data, arm_object.data.edit_bones)
            bpy.ops.object.mode_set(mode='OBJECT')

        # Clear out the dictionaries for this layer.
        layer_data.bone_names.clear()
        layer_data.bone_rolls.clear()
        layer_data.wmaps.clear()
        layer_data.colmaps.clear()
        layer_data.uvmaps.clear()
        layer_data.morphs.clear()
        layer_data.surf_tags.clear()

        # We may have some invalid mesh data, See: [#27916]
        # keep this last!
        print("validating mesh: %r..." % me.name)
        me.validate(verbose=1)
        print("done!")

    # With the objects made, setup the parents and re-adjust the locations.
    for ob_key in ob_dict:
        if ob_dict[ob_key][1] != -1 and ob_dict[ob_key][1] in ob_dict:
            parent_ob = ob_dict[ob_dict[ob_key][1]]
            ob_dict[ob_key][0].parent= parent_ob[0]
            ob_dict[ob_key][0].location-= parent_ob[0].location

    bpy.context.scene.update()

    print("Done Importing LWO File")


from bpy.props import StringProperty, BoolProperty


class IMPORT_OT_lwo(bpy.types.Operator):
    """Import LWO Operator"""
    bl_idname= "import_scene.lwo"
    bl_label= "Import LWO"
    bl_description= "Import a LightWave Object file"
    bl_options= {'REGISTER', 'UNDO'}

    filepath= StringProperty(name="File Path", description="Filepath used for importing the LWO file", maxlen=1024, default="")

    ADD_SUBD_MOD= BoolProperty(name="Apply SubD Modifier", description="Apply the Subdivision Surface modifier to layers with Subpatches", default=True)
    LOAD_HIDDEN= BoolProperty(name="Load Hidden Layers", description="Load object layers that have been marked as hidden", default=False)
    SKEL_TO_ARM= BoolProperty(name="Create Armature", description="Create an armature from an embedded Skelegon rig", default=True)
    USE_EXISTING_MATERIALS= BoolProperty(name="Use Existing Materials", description="Use existing materials if a material by that name already exists", default=False)

    def execute(self, context):
        load_lwo(self.filepath,
                 context,
                 self.ADD_SUBD_MOD,
                 self.LOAD_HIDDEN,
                 self.SKEL_TO_ARM,
                 self.USE_EXISTING_MATERIALS)
        return {'FINISHED'}

    def invoke(self, context, event):
        wm= context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


def menu_func(self, context):
    self.layout.operator(IMPORT_OT_lwo.bl_idname, text="LightWave Object (.lwo)")


def register():
    bpy.utils.register_module(__name__)

    bpy.types.INFO_MT_file_import.append(menu_func)


def unregister():
    bpy.utils.unregister_module(__name__)

    bpy.types.INFO_MT_file_import.remove(menu_func)

if __name__ == "__main__":
    register()
