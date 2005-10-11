#!BPY
"""
Name: 'LightWave + Materials (.lwo)...'
Blender: 237
Group: 'Import'
Tooltip: 'Import LightWave Object File Format (.lwo)'
"""

__author__ = "Alessandro Pirovano, Anthony D'Agostino (Scorpius)"
__url__ = ("blender", "elysiun",
"Author's homepage, http://www.redrival.com/scorpius", "Author's homepage, http://uaraus.altervista.org")

importername = "lwo_import 0.1.16"
# +---------------------------------------------------------+
# | Save your work before and after use.                    |
# | Please report any useful comment to:                    |
# | uaraus-dem@yahoo.it                                     |
# | Thanks                                                  |
# +---------------------------------------------------------+
# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | April 21, 2002                                          |
# | Released under the Blender Artistic Licence (BAL)       |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Read and write LightWave Object File Format (*.lwo)     |
# +---------------------------------------------------------+
# +---------------------------------------------------------+
# | Alessandro Pirovano tweaked starting on March 2005      |
# | http://uaraus.altervista.org                            |
# +---------------------------------------------------------+
# +---------------------------------------------------------+
# | Release log:                                            |
# | 0.1.16: fixed (try 2) texture offset calculations       |
# |         added hint on axis mapping                      |
# |         added hint on texture blending mode             |
# |         added hint on texture transparency setting      |
# |         search images in original directory first       |
# |         fixed texture order application                 |
# | 0.1.15: added release log                               |
# |         fixed texture offset calculations (non-UV)      |
# |         fixed reverting vertex order in face generation |
# |         associate texture on game-engine settings       |
# |         vector math definitely based on mathutils       |
# |         search images in "Images" and "../Images" dir   |
# |         revised logging facility                        |
# |         fixed subsurf texture and material mappings     |
# | 0.1.14: patched missing mod_vector (not definitive)     |
# | 0.1.13: first public release                            |
# +---------------------------------------------------------+

#blender related import
import Blender

#iosuite related import
try: #new naming
    import meshtools as my_meshtools
except ImportError: #fallback to the old one
    print "using old mod_meshtools"
    import mod_meshtools as my_meshtools

#python specific modules import
import struct, chunk, os, cStringIO, time, operator, copy

# ===========================================================
# === Utility Preamble ======================================
# ===========================================================

textname = "lwo_log"
#uncomment the following line to disable logging facility
#textname = None                      1

# ===========================================================

class dotext:

    _NO = 0    #use internal to class only
    LOG = 1    #write only to LOG
    CON = 2    #write to both LOG and CONSOLE

    def __init__(self, tname, where=LOG):
        self.dwhere = where #defaults on console only
        if (tname==None):
            print "*** not using text object to log script"
            self.txtobj = None
            return
        tlist = Blender.Text.get()
        for i in range(len(tlist)):
            if (tlist[i].getName()==tname):
                tlist[i].clear()
                #print tname, " text object found and cleared!"
                self.txtobj = tlist[i]
                return
        #print tname, " text object not found and created!"
        self.txtobj = Blender.Text.New(tname)
    # end def __init__

    def write(self, wstring, maxlen=100):
        if (self.txtobj==None): return
        while (1):
            ll = len(wstring)
            if (ll>maxlen):
                self.txtobj.write((wstring[:maxlen]))
                self.txtobj.write("\n")
                wstring = (wstring[maxlen:])
            else:
                self.txtobj.write(wstring)
                break
    # end def write

    def pstring(self, ppstring, where = _NO):
        if where == dotext._NO: where = self.dwhere
        if where == dotext.CON:
            print ppstring
        self.write(ppstring)
        self.write("\n")
    # end def pstring

    def plist(self, pplist, where = _NO):
        self.pprint ("list:[")
        for pp in range(len(pplist)):
            self.pprint ("[%d] -> %s" % (pp, pplist[pp]), where)
        self.pprint ("]")
    # end def plist

    def pdict(self, pdict, where = _NO):
        self.pprint ("dict:{", where)
        for pp in pdict.keys():
            self.pprint ("[%s] -> %s" % (pp, pdict[pp]), where)
        self.pprint ("}")
    # end def pdict

    def pprint(self, parg, where = _NO):
        if parg == None:
            self.pstring("_None_", where)
        elif type(parg) == type ([]):
            self.plist(parg, where)
        elif type(parg) == type ({}):
            self.pdict(parg, where)
        else:
            self.pstring(parg, where)
    # end def pprint

    def logcon(self, parg):
        self.pprint(parg, dotext.CON)
    # end def logcon
# endclass dotext

tobj=dotext(textname)
#uncomment the following line to log all messages on both console and logfile
#tobj=dotext(textname,dotext.CON)


# ===========================================================
# === Main read functions ===================================
# ===========================================================

# =============================
# === Read LightWave Format ===
# =============================
def read(filename):
    global tobj

    tobj.logcon ("#####################################################################")
    tobj.logcon ("This is: %s" % importername)
    tobj.logcon ("Importing file:")
    tobj.logcon (filename)
    tobj.pprint ("#####################################################################")

    start = time.clock()
    file = open(filename, "rb")

    # === LWO header ===
    form_id, form_size, form_type = struct.unpack(">4s1L4s",  file.read(12))
    if (form_type == "LWOB"):
        read_lwob(file, filename)
    elif (form_type == "LWO2"):
        read_lwo2(file, filename)
    else:
        tobj.logcon ("Can't read a file with the form_type: %s" %form_type)
        return

    Blender.Window.DrawProgressBar(1.0, "")    # clear progressbar
    file.close()
    end = time.clock()
    seconds = " in %.2f %s" % (end-start, "seconds")
    if form_type == "LWO2": fmt = " (v6.0 Format)"
    if form_type == "LWOB": fmt = " (v5.5 Format)"
    message = "Successfully imported " + os.path.basename(filename) + fmt + seconds
    #my_meshtools.print_boxed(message)
    tobj.pprint ("#####################################################################")
    tobj.logcon (message)
    tobj.logcon ("#####################################################################")

# enddef read


# =================================
# === Read LightWave 5.5 format ===
# =================================
def read_lwob(file, filename):
    global tobj

    tobj.logcon("LightWave 5.5 format")
    objname = os.path.splitext(os.path.basename(filename))[0]

    while 1:
        try:
            lwochunk = chunk.Chunk(file)
        except EOFError:
            break
        if lwochunk.chunkname == "LAYR":
            objname = read_layr(lwochunk)
        elif lwochunk.chunkname == "PNTS":                         # Verts
            verts = read_verts(lwochunk)
        elif lwochunk.chunkname == "POLS": # Faces v5.5
            faces = read_faces_5(lwochunk)
            my_meshtools.create_mesh(verts, faces, objname)
        else:                                                       # Misc Chunks
            lwochunk.skip()
    return
# enddef read_lwob


# =============================
# === Read LightWave Format ===
# =============================
def read_lwo2(file, filename, typ="LWO2"):
    global tobj

    tobj.logcon("LightWave 6 (and above) format")

    dir_part = Blender.sys.dirname(filename)
    fname_part = Blender.sys.basename(filename)

    #first initialization of data structures
    defaultname = os.path.splitext(fname_part)[0]
    tag_list = []              #tag list: global for the whole file?
    surf_list = []             #surf list: global for the whole file?
    clip_list = []             #clip list: global for the whole file?
    object_index = 0
    object_list = None
    # init value is: object_list = [[None, {}, [], [], {}, {}, 0, {}, {}]]
    #0 - objname                    #original name
    #1 - obj_dict = {TAG}           #objects created
    #2 - verts = []                 #object vertexes
    #3 - faces = []                 #object faces (associations poly -> vertexes)
    #4 - obj_dim_dict = {TAG}       #tuples size and pos in local object coords - used for NON-UV mappings
    #5 - polytag_dict = {TAG}       #tag to polygon mapping
    #6 - patch_flag                 #0 = surf; 1 = patch (subdivision surface) - it was the image list
    #7 - uvcoords_dict = {name}     #uvmap coordinates (mixed mode per face/per vertex)
    #8 - facesuv_dict = {name}      #uvmap coordinates associations poly -> uv tuples

    while 1:
        try:
            lwochunk = chunk.Chunk(file)
        except EOFError:
            break
        tobj.pprint(" ")
        if lwochunk.chunkname == "LAYR":
            tobj.pprint("---- LAYR")
            objname = read_layr(lwochunk)
            tobj.pprint(objname)
            if object_list == None:
                object_list = [[objname, {}, [], [], {}, {}, 0, {}, {}]]
            else:
                object_list.append([objname, {}, [], [], {}, {}, 0, {}, {}])
                object_index += 1
        elif lwochunk.chunkname == "PNTS":                         # Verts
            tobj.pprint("---- PNTS")
            verts = read_verts(lwochunk)
            object_list[object_index][2] = verts
        elif lwochunk.chunkname == "VMAP":                         # MAPS (UV)
            tobj.pprint("---- VMAP")
            object_list[object_index][7], object_list[object_index][8] = read_vmap(object_list[object_index][7], object_list[object_index][8], object_list[object_index][3], len(object_list[object_index][2]), lwochunk)
        elif lwochunk.chunkname == "VMAD":                         # MAPS (UV) per-face
            tobj.pprint("---- VMAD")
            object_list[object_index][7], object_list[object_index][8] = read_vmad(object_list[object_index][7], object_list[object_index][8], object_list[object_index][3], len(object_list[object_index][2]), lwochunk)
        elif lwochunk.chunkname == "POLS": # Faces v6.0
            tobj.pprint("-------- POLS(6)")
            faces, flag = read_faces_6(lwochunk)
            #flag is 0 for regular polygon, 1 for patches (= subsurf), 2 for anything else to be ignored
            if flag<2:
                if object_list[object_index][3] != []:
                    object_list.append([object_list[object_index][0],                  #update name
                                        {},                                            #init
                                        copy.deepcopy(object_list[object_index][2]),   #same vertexes
                                        [],                                            #no faces
                                        {},                                            #no need to copy - filled at runtime
                                        {},                                            #polygon tagging will follow
                                        flag,                                          #patch flag
                                        copy.deepcopy(object_list[object_index][7]),   #same uvcoords
                                        {}])                                           #no uv mapping
                    object_index += 1
                #end if already has a face list
                #update uv coords mapping if VMAP already encountered
                for uvname in object_list[object_index][7]:
                    tobj.pprint("updating uv to face mapping for %s" % uvname)
                    object_list[object_index][8][uvname] = copy.deepcopy(faces)
                object_list[object_index][3] = faces
                objname = object_list[object_index][0]
                if objname == None:
                    objname = defaultname
            #end if processing a valid poly type
        elif lwochunk.chunkname == "TAGS":                         # Tags
            tobj.pprint("---- TAGS")
            tag_list.extend(read_tags(lwochunk))
        elif lwochunk.chunkname == "PTAG":                         # PTags
            tobj.pprint("---- PTAG")
            polytag_dict = read_ptags(lwochunk, tag_list)
            for kk in polytag_dict.keys(): object_list[object_index][5][kk] = polytag_dict[kk]
        elif lwochunk.chunkname == "SURF":                         # surfaces
            tobj.pprint("---- SURF")
            surf_list.append(read_surfs(lwochunk, surf_list, tag_list))
        elif lwochunk.chunkname == "CLIP":                         # texture images
            tobj.pprint("---- CLIP")
            clip_list.append(read_clip(lwochunk))
            tobj.pprint("read total %s clips" % len(clip_list))
        else:                                                       # Misc Chunks
            tobj.pprint("---- %s: skipping" % lwochunk.chunkname)
            lwochunk.skip()
        #uncomment here to log data structure as it is built
        #tobj.pprint(object_list)

    tobj.pprint ("\n#####################################################################")
    tobj.pprint("Found %d objects:" % len(object_list))
    tobj.pprint ("#####################################################################")
    for objspec_list in object_list:
        tobj.pprint ("\n#===================================================================#")
        tobj.pprint("Processing Object: %s" % objspec_list[0])
        tobj.pprint ("#===================================================================#")
        objspec_list[3], objspec_list[5], objspec_list[8] = recalc_faces(objspec_list[2], objspec_list[3], objspec_list[5], objspec_list[8]) #recalculate faces, polytag_dict and uv_mapping get rid of faces fanning

        create_objects(objspec_list)

        if surf_list != []:
            create_material(clip_list, surf_list, objspec_list, dir_part) #give it all the object
    return
# enddef read_lwo2






# ===========================================================
# === File reading routines =================================
# ===========================================================
# ==================
# === Read Verts ===
# ==================
def read_verts(lwochunk):
    global tobj

    data = cStringIO.StringIO(lwochunk.read())
    numverts = lwochunk.chunksize/12
    #$verts = []
    verts = [None] * numverts
    for i in range(numverts):
        if not i%100 and my_meshtools.show_progress:
            Blender.Window.DrawProgressBar(float(i)/numverts, "Reading Verts")
        x, y, z = struct.unpack(">fff", data.read(12))
        verts[i] = (x, z, y)
    tobj.pprint("read %d vertexes" % (i+1))
    return verts
# enddef read_verts


# =================
# === Read Name ===
# =================
# modified to deal with odd lenght strings
def read_name(file):
    name = ""
    while 1:
        char = file.read(1)
        if char == "\0": break
        else: name += char
    len_name = len(name) + 1 #count the trailing zero
    if len_name%2==1:
        char = file.read(1) #remove zero padding to even lenght
        len_name += 1
    return name, len_name


# ==================
# === Read Layer ===
# ==================
def read_layr(lwochunk):
    data = cStringIO.StringIO(lwochunk.read())
    idx, flags = struct.unpack(">hh", data.read(4))
    pivot = struct.unpack(">fff", data.read(12))
    layer_name, discard = read_name(data)
    if not layer_name: layer_name = "NoName"
    return layer_name
# enddef read_layr


# ======================
# === Read Faces 5.5 ===
# ======================
def read_faces_5(lwochunk):
    data = cStringIO.StringIO(lwochunk.read())
    faces = []
    i = 0
    while i < lwochunk.chunksize:
        if not i%100 and my_meshtools.show_progress:
           Blender.Window.DrawProgressBar(float(i)/lwochunk.chunksize, "Reading Faces")
        facev = []
        numfaceverts, = struct.unpack(">H", data.read(2))
        for j in range(numfaceverts):
            index, = struct.unpack(">H", data.read(2))
            facev.append(index)
        facev.reverse()
        faces.append(facev)
        surfaceindex, = struct.unpack(">H", data.read(2))
        if surfaceindex < 0:
            tobj.logcon ("***Error. Referencing uncorrect surface index")
            return
        i += (4+numfaceverts*2)
    return faces


# ==================================
# === Read Variable-Length Index ===
# ==================================
def read_vx(data):
    byte1, = struct.unpack(">B", data.read(1))
    if byte1 != 0xFF:    # 2-byte index
        byte2, = struct.unpack(">B", data.read(1))
        index = byte1*256 + byte2
        index_size = 2
    else:                # 4-byte index
        byte2, byte3, byte4 = struct.unpack(">3B", data.read(3))
        index = byte2*65536 + byte3*256 + byte4
        index_size = 4
    return index, index_size


# ======================
# === Read uvmapping ===
# ======================
def read_vmap(uvcoords_dict, facesuv_dict, faces, maxvertnum, lwochunk):
    if maxvertnum == 0:
        tobj.pprint ("Found VMAP but no vertexes to map!")
        return uvcoords_dict, facesuv_dict
    data = cStringIO.StringIO(lwochunk.read())
    map_type = data.read(4)
    if map_type != "TXUV":
        tobj.pprint ("Reading VMAP: No Texture UV map Were Found. Map Type: %s" % map_type)
        return uvcoords_dict, facesuv_dict
    dimension, = struct.unpack(">H", data.read(2))
    name, i = read_name(data) #i initialized with string lenght + zeros
    tobj.pprint ("TXUV %d %s" % (dimension, name))
    #my_uv_list = [None] * maxvertnum
    my_uv_list = [(0.0, 0.0)] * maxvertnum         #more safe to have some default coordinates to associate in any case?
    while (i < lwochunk.chunksize - 6):            #4+2 header bytes already read
        vertnum, vnum_size = read_vx(data)
        u, v = struct.unpack(">ff", data.read(8))
        if vertnum >= maxvertnum:
            tobj.pprint ("Hem: more uvmap than vertexes? ignoring uv data for vertex %d" % vertnum)
        else:
            my_uv_list[vertnum] = (u, v)
        i += 8 + vnum_size
    #end loop on uv pairs
    uvcoords_dict[name] = my_uv_list
    #this is a per-vertex mapping AND the uv tuple is vertex-ordered, so faces_uv is the same as faces
    if faces == []:
        tobj.pprint ("no faces read yet! delaying uv to face assignments")
        facesuv_dict[name] = []
    else:
        #deepcopy so we could modify it without actually modify faces
        tobj.pprint ("faces already present: proceeding with assignments")
        facesuv_dict[name] = copy.deepcopy(faces)
    return uvcoords_dict, facesuv_dict


# ========================
# === Read uvmapping 2 ===
# ========================
def read_vmad(uvcoords_dict, facesuv_dict, faces, maxvertnum, lwochunk):
    maxfacenum = len(faces)
    if maxvertnum == 0 or maxfacenum == 0:
        tobj.pprint ("Found VMAD but no vertexes to map!")
        return uvcoords_dict, facesuv_dict
    data = cStringIO.StringIO(lwochunk.read())
    map_type = data.read(4)
    if map_type != "TXUV":
        tobj.pprint ("Reading VMAD: No Texture UV map Were Found. Map Type: %s" % map_type)
        return uvcoords_dict, facesuv_dict
    dimension, = struct.unpack(">H", data.read(2))
    name, i = read_name(data) #i initialized with string lenght + zeros
    tobj.pprint ("TXUV %d %s" % (dimension, name))
    if uvcoords_dict.has_key(name):
        my_uv_list = uvcoords_dict[name]          #update existing
        my_facesuv_list = facesuv_dict[name]
    else:
        my_uv_list = [(0.0, 0.0)] * maxvertnum    #start a brand new: this could be made more smart
        my_facesuv_list = copy.deepcopy(faces)
    #end variable initialization
    lastindex = len(my_uv_list) - 1
    while (i < lwochunk.chunksize - 6):  #4+2 header bytes already read
        vertnum, vnum_size = read_vx(data)
        i += vnum_size
        polynum, vnum_size = read_vx(data)
        i += vnum_size
        u, v = struct.unpack(">ff", data.read(8))
        if polynum >= maxfacenum or vertnum >= maxvertnum:
            tobj.pprint ("Hem: more uvmap than vertexes? ignorig uv data for vertex %d" % vertnum)
        else:
            my_uv_list.append( (u,v) )
            newindex = len(my_uv_list) - 1
            for vi in range(len(my_facesuv_list[polynum])): #polynum starting from 1 or from 0?
                if my_facesuv_list[polynum][vi] == vertnum:
                    my_facesuv_list[polynum][vi] = newindex
            #end loop on current face vertexes
        i += 8
    #end loop on uv pairs
    uvcoords_dict[name] = my_uv_list
    facesuv_dict[name] = my_facesuv_list
    tobj.pprint ("updated %d vertexes data" % (newindex-lastindex))
    return uvcoords_dict, facesuv_dict


# =================
# === Read tags ===
# =================
def read_tags(lwochunk):
    data = cStringIO.StringIO(lwochunk.read())
    tag_list = []
    current_tag = ""
    i = 0
    while i < lwochunk.chunksize:
        char = data.read(1)
        if char == "\0":
            tag_list.append(current_tag)
            if (len(current_tag) % 2 == 0): char = data.read(1)
            current_tag = ""
        else:
            current_tag += char
        i += 1
    tobj.pprint("read %d tags, list follows:" % len(tag_list))
    tobj.pprint( tag_list)
    return tag_list


# ==================
# === Read Ptags ===
# ==================
def read_ptags(lwochunk, tag_list):
    data = cStringIO.StringIO(lwochunk.read())
    polygon_type = data.read(4)
    if polygon_type != "SURF":
        tobj.pprint ("No Surf Were Found. Polygon Type: %s" % polygon_type)
        return {}
    ptag_dict = {}
    i = 0
    while(i < lwochunk.chunksize-4): #4 bytes polygon type already read
        if not i%100 and my_meshtools.show_progress:
           Blender.Window.DrawProgressBar(float(i)/lwochunk.chunksize, "Reading PTAGS")
        poln, poln_size = read_vx(data)
        i += poln_size
        tag_index, = struct.unpack(">H", data.read(2))
        if tag_index > (len(tag_list)):
            tobj.pprint ("Reading PTAG: Surf belonging to undefined TAG: %d. Skipping" % tag_index)
            return {}
        i += 2
        tag_key = tag_list[tag_index]
        if not(ptag_dict.has_key(tag_key)):
            ptag_dict[tag_list[tag_index]] = [poln]
        else:
            ptag_dict[tag_list[tag_index]].append(poln)
    for i in ptag_dict.keys():
        tobj.pprint ("read %d polygons belonging to TAG %s" % (len(ptag_dict[i]), i))
    return ptag_dict



# ==================
# === Read Clips ===
# ==================
def read_clip(lwochunk):
    clip_dict = {}
    data = cStringIO.StringIO(lwochunk.read())
    image_index, = struct.unpack(">L", data.read(4))
    clip_dict['ID'] = image_index
    i = 4
    while(i < lwochunk.chunksize):
        subchunkname, = struct.unpack("4s", data.read(4))
        subchunklen, = struct.unpack(">H", data.read(2))
        if subchunkname == "STIL":
            tobj.pprint("-------- STIL")
            clip_name, k = read_name(data)
            #now split text independently from platform
            #depend on the system where image was saved. NOT the one where the script is run
            no_sep = "\\"
            if Blender.sys.sep == no_sep: no_sep ="/"
            if (no_sep in clip_name):
                clip_name = clip_name.replace(no_sep, Blender.sys.sep)
            short_name = Blender.sys.basename(clip_name)
            if (clip_name == "") or (short_name == ""):
                tobj.pprint ("Reading CLIP: Empty clip name not allowed. Skipping")
                discard = data.read(subchunklen-k)
            clip_dict['NAME'] = clip_name
            clip_dict['BASENAME'] = short_name
        elif subchunkname == "XREF":                           #cross reference another image
            tobj.pprint("-------- XREF")
            image_index, = struct.unpack(">L", data.read(4))
            clip_name, k = read_name(data)
            clip_dict['NAME'] = clip_name
            clip_dict['XREF'] = image_index
        elif subchunkname == "NEGA":                           #negate texture effect
            tobj.pprint("-------- NEGA")
            n, = struct.unpack(">H", data.read(2))
            clip_dict['NEGA'] = n
        else:                                                       # Misc Chunks
            tobj.pprint("-------- SURF:%s: skipping" % subchunkname)
            discard = data.read(subchunklen)
        i = i + 6 + subchunklen
    #end loop on surf chunks
    tobj.pprint("read image:%s" % clip_dict)
    return clip_dict


# ===========================
# === Read Surfaces Block ===
# ===========================
def read_surfblok(subchunkdata):
    lenght = len(subchunkdata)
    my_dict = {}
    my_uvname = ""
    data = cStringIO.StringIO(subchunkdata)
    ##############################################################
    # blok header sub-chunk
    ##############################################################
    subchunkname, = struct.unpack("4s", data.read(4))
    subchunklen, = struct.unpack(">h", data.read(2))
    accumulate_i = subchunklen + 6
    if subchunkname != 'IMAP':
        tobj.pprint("---------- SURF: BLOK: %s: block aborting" % subchunkname)
        return {}, ""
    tobj.pprint ("---------- IMAP")
    ordinal, i = read_name(data)
    my_dict['ORD'] = ordinal
    my_dict['g_ORD'] = -1
    my_dict['ENAB'] = True
    while(i < subchunklen): # ---------left 6------------------------- loop on header parameters
        sub2chunkname, = struct.unpack("4s", data.read(4))
        sub2chunklen, = struct.unpack(">h", data.read(2))
        i = i + 6 + sub2chunklen
        if sub2chunkname == "CHAN":
            tobj.pprint("------------ CHAN")
            sub2chunkname, = struct.unpack("4s", data.read(4))
            my_dict['CHAN'] = sub2chunkname
            sub2chunklen -= 4
        elif sub2chunkname == "ENAB":                             #only present if is to be disabled
            tobj.pprint("------------ ENAB")
            ena, = struct.unpack(">h", data.read(2))
            my_dict['ENAB'] = ena
            sub2chunklen -= 2
        elif sub2chunkname == "NEGA":                             #only present if is to be enabled
            tobj.pprint("------------ NEGA")
            ena, = struct.unpack(">h", data.read(2))
            if ena == 1:
                my_dict['NEGA'] = ena
            sub2chunklen -= 2
        elif sub2chunkname == "OPAC":                             #only present if is to be disabled
            tobj.pprint("------------ OPAC")
            opa, = struct.unpack(">h", data.read(2))
            s, = struct.unpack(">f", data.read(4))
            envelope, env_size = read_vx(data)
            my_dict['OPAC'] = opa
            my_dict['OPACVAL'] = s
            sub2chunklen -= 6
        elif sub2chunkname == "AXIS":
            tobj.pprint("------------ AXIS")
            ena, = struct.unpack(">h", data.read(2))
            my_dict['DISPLAXIS'] = ena
            sub2chunklen -= 2
        else:                                                       # Misc Chunks
            tobj.pprint("------------ SURF: BLOK: IMAP: %s: skipping" % sub2chunkname)
            discard = data.read(sub2chunklen)
    #end loop on blok header subchunks
    ##############################################################
    # blok attributes sub-chunk
    ##############################################################
    subchunkname, = struct.unpack("4s", data.read(4))
    subchunklen, = struct.unpack(">h", data.read(2))
    accumulate_i += subchunklen + 6
    if subchunkname != 'TMAP':
        tobj.pprint("---------- SURF: BLOK: %s: block aborting" % subchunkname)
        return {}, ""
    tobj.pprint ("---------- TMAP")
    i = 0
    while(i < subchunklen): # -----------left 6----------------------- loop on header parameters
        sub2chunkname, = struct.unpack("4s", data.read(4))
        sub2chunklen, = struct.unpack(">h", data.read(2))
        i = i + 6 + sub2chunklen
        if sub2chunkname == "CNTR":
            tobj.pprint("------------ CNTR")
            x, y, z = struct.unpack(">fff", data.read(12))
            envelope, env_size = read_vx(data)
            my_dict['CNTR'] = [x, y, z]
            sub2chunklen -= (12+env_size)
        elif sub2chunkname == "SIZE":
            tobj.pprint("------------ SIZE")
            x, y, z = struct.unpack(">fff", data.read(12))
            envelope, env_size = read_vx(data)
            my_dict['SIZE'] = [x, y, z]
            sub2chunklen -= (12+env_size)
        elif sub2chunkname == "ROTA":
            tobj.pprint("------------ ROTA")
            x, y, z = struct.unpack(">fff", data.read(12))
            envelope, env_size = read_vx(data)
            my_dict['ROTA'] = [x, y, z]
            sub2chunklen -= (12+env_size)
        elif sub2chunkname == "CSYS":
            tobj.pprint("------------ CSYS")
            ena, = struct.unpack(">h", data.read(2))
            my_dict['CSYS'] = ena
            sub2chunklen -= 2
        else:                                                       # Misc Chunks
            tobj.pprint("------------ SURF: BLOK: TMAP: %s: skipping" % sub2chunkname)
        if  sub2chunklen > 0:
            discard = data.read(sub2chunklen)
    #end loop on blok attributes subchunks
    ##############################################################
    # ok, now other attributes without sub_chunks
    ##############################################################
    while(accumulate_i < lenght): # ---------------------------------- loop on header parameters: lenght has already stripped the 6 bypes header
        subchunkname, = struct.unpack("4s", data.read(4))
        subchunklen, = struct.unpack(">H", data.read(2))
        accumulate_i = accumulate_i + 6 + subchunklen
        if subchunkname == "PROJ":
            tobj.pprint("---------- PROJ")
            p, = struct.unpack(">h", data.read(2))
            my_dict['PROJ'] = p
            subchunklen -= 2
        elif subchunkname == "AXIS":
            tobj.pprint("---------- AXIS")
            a, = struct.unpack(">h", data.read(2))
            my_dict['MAJAXIS'] = a
            subchunklen -= 2
        elif subchunkname == "IMAG":
            tobj.pprint("---------- IMAG")
            i, i_size = read_vx(data)
            my_dict['IMAG'] = i
            subchunklen -= i_size
        elif subchunkname == "WRAP":
            tobj.pprint("---------- WRAP")
            ww, wh = struct.unpack(">hh", data.read(4))
            #reduce width and height to just 1 parameter for both
            my_dict['WRAP'] = max([ww,wh])
            #my_dict['WRAPWIDTH'] = ww
            #my_dict['WRAPHEIGHT'] = wh
            subchunklen -= 4
        elif subchunkname == "WRPW":
            tobj.pprint("---------- WRPW")
            w, = struct.unpack(">f", data.read(4))
            my_dict['WRPW'] = w
            envelope, env_size = read_vx(data)
            subchunklen -= (env_size+4)
        elif subchunkname == "WRPH":
            tobj.pprint("---------- WRPH")
            w, = struct.unpack(">f", data.read(4))
            my_dict['WRPH'] = w
            envelope, env_size = read_vx(data)
            subchunklen -= (env_size+4)
        elif subchunkname == "VMAP":
            tobj.pprint("---------- VMAP")
            vmp, i = read_name(data)
            my_dict['VMAP'] = vmp
            my_uvname = vmp
            subchunklen -= i
        else:                                                    # Misc Chunks
            tobj.pprint("---------- SURF: BLOK: %s: skipping" % subchunkname)
        if  subchunklen > 0:
            discard = data.read(subchunklen)
    #end loop on blok subchunks
    return my_dict, my_uvname


# =====================
# === Read Surfaces ===
# =====================
def read_surfs(lwochunk, surf_list, tag_list):
    my_dict = {}
    data = cStringIO.StringIO(lwochunk.read())
    surf_name, i = read_name(data)
    parent_name, j = read_name(data)
    i += j
    if (surf_name == "") or not(surf_name in tag_list):
        tobj.pprint ("Reading SURF: Actually empty surf name not allowed. Skipping")
        return {}
    if (parent_name != ""):
        parent_index = [x['NAME'] for x in surf_list].count(parent_name)
        if parent_index >0:
            my_dict = surf_list[parent_index-1]
    my_dict['NAME'] = surf_name
    tobj.pprint ("Surface data for TAG %s" % surf_name)
    while(i < lwochunk.chunksize):
        subchunkname, = struct.unpack("4s", data.read(4))
        subchunklen, = struct.unpack(">H", data.read(2))
        i = i + 6 + subchunklen #6 bytes subchunk header
        if subchunkname == "COLR":                             #color: mapped on color
            tobj.pprint("-------- COLR")
            r, g, b = struct.unpack(">fff", data.read(12))
            envelope, env_size = read_vx(data)
            my_dict['COLR'] = [r, g, b]
            subchunklen -= (12+env_size)
        elif subchunkname == "DIFF":                           #diffusion: mapped on reflection (diffuse shader)
            tobj.pprint("-------- DIFF")
            s, = struct.unpack(">f", data.read(4))
            envelope, env_size = read_vx(data)
            my_dict['DIFF'] = s
            subchunklen -= (4+env_size)
        elif subchunkname == "SPEC":                           #specularity: mapped to specularity (spec shader)
            tobj.pprint("-------- SPEC")
            s, = struct.unpack(">f", data.read(4))
            envelope, env_size = read_vx(data)
            my_dict['SPEC'] = s
            subchunklen -= (4+env_size)
        elif subchunkname == "REFL":                           #reflection: mapped on raymirror
            tobj.pprint("-------- REFL")
            s, = struct.unpack(">f", data.read(4))
            envelope, env_size = read_vx(data)
            my_dict['REFL'] = s
            subchunklen -= (4+env_size)
        elif subchunkname == "TRNL":                           #translucency: mapped on same param
            tobj.pprint("-------- TRNL")
            s, = struct.unpack(">f", data.read(4))
            envelope, env_size = read_vx(data)
            my_dict['TRNL'] = s
            subchunklen -= (4+env_size)
        elif subchunkname == "GLOS":                           #glossiness: mapped on specularity hardness (spec shader)
            tobj.pprint("-------- GLOS")
            s, = struct.unpack(">f", data.read(4))
            envelope, env_size = read_vx(data)
            my_dict['GLOS'] = s
            subchunklen -= (4+env_size)
        elif subchunkname == "TRAN":                           #transparency: inverted and mapped on alpha channel
            tobj.pprint("-------- TRAN")
            s, = struct.unpack(">f", data.read(4))
            envelope, env_size = read_vx(data)
            my_dict['TRAN'] = s
            subchunklen -= (4+env_size)
        elif subchunkname == "LUMI":                           #luminosity: mapped on emit channel
            tobj.pprint("-------- LUMI")
            s, = struct.unpack(">f", data.read(4))
            envelope, env_size = read_vx(data)
            my_dict['LUMI'] = s
            subchunklen -= (4+env_size)
        elif subchunkname == "GVAL":                           #glow: mapped on add channel
            tobj.pprint("-------- GVAL")
            s, = struct.unpack(">f", data.read(4))
            envelope, env_size = read_vx(data)
            my_dict['GVAL'] = s
            subchunklen -= (4+env_size)
        elif subchunkname == "SMAN":                           #smoothing angle
            tobj.pprint("-------- SMAN")
            s, = struct.unpack(">f", data.read(4))
            my_dict['SMAN'] = s
            subchunklen -= 4
        elif subchunkname == "SIDE":                           #double sided?
            tobj.pprint("-------- SIDE")                             #if 1 side do not define key
            s, = struct.unpack(">H", data.read(2))
            if s == 3:
                my_dict['SIDE'] = s
            subchunklen -= 2
        elif subchunkname == "RIND":                           #Refraction: mapped on IOR
            tobj.pprint("-------- RIND")
            s, = struct.unpack(">f", data.read(4))
            envelope, env_size = read_vx(data)
            my_dict['RIND'] = s
            subchunklen -= (4+env_size)
        elif subchunkname == "BLOK":                           #blocks
            tobj.pprint("-------- BLOK")
            rr, uvname = read_surfblok(data.read(subchunklen))
            #paranoia setting: preventing adding an empty dict
            if rr != {}:
                if not(my_dict.has_key('BLOK')):
                    my_dict['BLOK'] = [rr]
                else:
                    my_dict['BLOK'].append(rr)
            if uvname != "":
                my_dict['UVNAME'] = uvname                            #theoretically there could be a number of them: only one used per surf
            subchunklen = 0 #force ending
        else:                                                       # Misc Chunks
            tobj.pprint("-------- SURF:%s: skipping" % subchunkname)
        if  subchunklen > 0:
            discard = data.read(subchunklen)
    #end loop on surf chunks
    if my_dict.has_key('BLOK'):
       my_dict['BLOK'].reverse()
    return my_dict






# ===========================================================
# === Generation Routines ===================================
# ===========================================================
# ==================================================
# === Compute vector distance between two points ===
# ==================================================
def dist_vector (head, tail): #vector from head to tail
    return Blender.Mathutils.Vector([head[0] - tail[0], head[1] - tail[1], head[2] - tail[2]])


# ================
# === Find Ear ===
# ================
def find_ear(normal, list_dict, verts, face):
    nv = len(list_dict['MF'])
    #looping through vertexes trying to find an ear
    #most likely in case of panic
    mlc = 0
    mla = 1
    mlb = 2

    for c in range(nv):
        a = (c+1) % nv; b = (a+1) % nv

        if list_dict['P'][a] > 0.0: #we have to start from a convex vertex
        #if (list_dict['P'][a] > 0.0) and (list_dict['P'][b] <= 0.0): #we have to start from a convex vertex
            mlc = c
            mla = a
            mlb = b
            #tobj.pprint ("## mmindex: %s, %s, %s  'P': %s, %s, %s" % (c, a, b, list_dict['P'][c],list_dict['P'][a],list_dict['P'][b]))
            #tobj.pprint ("   ok, this one passed")
            concave = 0
            concave_inside = 0
            for xx in range(nv): #looking for concave vertex
                if (list_dict['P'][xx] <= 0.0) and (xx != b) and (xx != c): #cannot be a: it's convex
                    #ok, found concave vertex
                    concave = 1
                    #a, b, c, xx are all meta-meta vertex indexes
                    mva = list_dict['MF'][a] #meta-vertex-index
                    mvb = list_dict['MF'][b]
                    mvc = list_dict['MF'][c]
                    mvxx = list_dict['MF'][xx]
                    va = face[mva] #vertex
                    vb = face[mvb]
                    vc = face[mvc]
                    vxx = face[mvxx]

                    #Distances
                    d_ac_v = list_dict['D'][c]
                    d_ba_v = list_dict['D'][a]
                    d_cb_v = dist_vector(verts[vc], verts[vb])

                    #distance from triangle points
                    d_xxa_v = dist_vector(verts[vxx], verts[va])
                    d_xxb_v = dist_vector(verts[vxx], verts[vb])
                    d_xxc_v = dist_vector(verts[vxx], verts[vc])

                    #normals
                    n_xxa_v = Blender.Mathutils.CrossVecs(d_ba_v, d_xxa_v)
                    n_xxb_v = Blender.Mathutils.CrossVecs(d_cb_v, d_xxb_v)
                    n_xxc_v = Blender.Mathutils.CrossVecs(d_ac_v, d_xxc_v)

                    #how are oriented the normals?
                    p_xxa_v = Blender.Mathutils.DotVecs(normal, n_xxa_v)
                    p_xxb_v = Blender.Mathutils.DotVecs(normal, n_xxb_v)
                    p_xxc_v = Blender.Mathutils.DotVecs(normal, n_xxc_v)

                    #if normals are oriented all to same directions - so it is insida
                    if ((p_xxa_v > 0.0) and (p_xxb_v > 0.0) and (p_xxc_v > 0.0)) or ((p_xxa_v <= 0.0) and (p_xxb_v <= 0.0) and (p_xxc_v <= 0.0)):
                        #print "vertex %d: concave inside" % xx
                        concave_inside = 1
                        break
                #endif found a concave vertex
            #end loop looking for concave vertexes
            if (concave == 0) or (concave_inside == 0):
                #no concave vertexes in polygon (should not be): return immediately
                #looped all concave vertexes and no one inside found
                return [c, a, b]
        #no convex vertex, try another one
    #end loop to find a suitable base vertex for ear
    #looped all candidate ears and find no-one suitable
    tobj.pprint ("Reducing face: no valid ear found to reduce!")
    return [mlc, mla, mlb] #uses most likely




# ====================
# === Reduce Faces ===
# ====================
# http://www-cgrl.cs.mcgill.ca/~godfried/teaching/cg-projects/97/Ian/cutting_ears.html per l'import
def reduce_face(verts, face):
    nv = len (face)
    if nv == 3: return [[0,1,2]] #trivial decomposition list
    list_dict = {}
    #meta-vertex indexes
    list_dict['MF'] = range(nv) # these are meta-vertex-indexes
    list_dict['D'] = [None] * nv
    list_dict['X'] = [None] * nv
    list_dict['P'] = [None] * nv
    #list of distances
    for mvi in list_dict['MF']:
        #vector between two vertexes
        mvi_hiend = (mvi+1) % nv      #last-to-first
        vi_hiend = face[mvi_hiend] #vertex
        vi = face[mvi]
        list_dict['D'][mvi] = dist_vector(verts[vi_hiend], verts[vi])
    #list of cross products - normals evaluated into vertexes
    for vi in range(nv):
        list_dict['X'][vi] = Blender.Mathutils.CrossVecs(list_dict['D'][vi], list_dict['D'][vi-1])
    my_face_normal = Blender.Mathutils.Vector([list_dict['X'][0][0], list_dict['X'][0][1], list_dict['X'][0][2]])
    #list of dot products
    list_dict['P'][0] = 1.0
    for vi in range(1, nv):
        list_dict['P'][vi] = Blender.Mathutils.DotVecs(my_face_normal, list_dict['X'][vi])
    #is there at least one concave vertex?
    #one_concave = reduce(lambda x, y: (x) or (y<=0.0), list_dict['P'], 0)
    one_concave = reduce(lambda x, y: (x) + (y<0.0), list_dict['P'], 0)
    decomposition_list = []

    while 1:
        if nv == 3: break
        if one_concave:
            #look for triangle
            ct = find_ear(my_face_normal, list_dict, verts, face)
            mv0 = list_dict['MF'][ct[0]] #meta-vertex-index
            mv1 = list_dict['MF'][ct[1]]
            mv2 = list_dict['MF'][ct[2]]
            #add the triangle to output list
            decomposition_list.append([mv0, mv1, mv2])
            #update data structures removing remove middle vertex from list
            #distances
            v0 = face[mv0] #vertex
            v1 = face[mv1]
            v2 = face[mv2]
            list_dict['D'][ct[0]] = dist_vector(verts[v2], verts[v0])
            #cross products
            list_dict['X'][ct[0]] = Blender.Mathutils.CrossVecs(list_dict['D'][ct[0]], list_dict['D'][ct[0]-1])
            list_dict['X'][ct[2]] = Blender.Mathutils.CrossVecs(list_dict['D'][ct[2]], list_dict['D'][ct[0]])
            #list of dot products
            list_dict['P'][ct[0]] = Blender.Mathutils.DotVecs(my_face_normal, list_dict['X'][ct[0]])
            list_dict['P'][ct[2]] = Blender.Mathutils.DotVecs(my_face_normal, list_dict['X'][ct[2]])
            #physical removal
            list_dict['MF'].pop(ct[1])
            list_dict['D'].pop(ct[1])
            list_dict['X'].pop(ct[1])
            list_dict['P'].pop(ct[1])
            one_concave = reduce(lambda x, y: (x) or (y<0.0), list_dict['P'], 0)
            nv -=1
        else: #here if no more concave vertexes
            if nv == 4: break  #quads only if no concave vertexes
            decomposition_list.append([list_dict['MF'][0], list_dict['MF'][1], list_dict['MF'][2]])
            #physical removal
            list_dict['MF'].pop(1)
            nv -=1
    #end while there are more my_face to triangulate
    decomposition_list.append(list_dict['MF'])
    return decomposition_list


# =========================
# === Recalculate Faces ===
# =========================
# --------- this do the standard face + ptag_dict + uv-map recalc
def recalc_faces(verts, faces, polytag_dict, facesuv_dict):
    # init local face list
    my_faces = []
    # init local uvface dict
    my_facesuv = {}
    for uvname in facesuv_dict:
        my_facesuv[uvname] = []
    replaced_faces_dict = {}
    j = 0
    if len(faces)==0:
        return faces, polytag_dict, facesuv_dict
    for i in range(len(faces)):
        # i = index that spans on original faces
        # j = index that spans on new faces
        if not i%100 and my_meshtools.show_progress: Blender.Window.DrawProgressBar(float(i)/len(faces), "Recalculating faces")
        numfaceverts=len(faces[i])
        if numfaceverts < 4:                #This face is a triangle or quad: more strict - it has to be a triangle
            my_faces.append(faces[i])       #ok, leave it alone ....
            for uvname in facesuv_dict:
                my_facesuv[uvname].append(facesuv_dict[uvname][i])
            replaced_faces_dict[i] = [j]     #.... but change the nuber order of the face
            j += 1
        else:                                                # Reduce n-sided convex polygon.
            meta_faces = reduce_face(verts, faces[i])   # Indices of triangles.
            this_faces = []                                  # list of triangles poly replacing original face
            this_faces_index = []
            for mf in meta_faces:
                ll = len(mf)
                if ll == 3: #triangle
                    this_faces.append([faces[i][mf[0]], faces[i][mf[1]], faces[i][mf[2]]])
                else:        #quads
                    this_faces.append([faces[i][mf[0]], faces[i][mf[1]], faces[i][mf[2]], faces[i][mf[3]]])
                for uvname in facesuv_dict:
                    if ll == 3:  #triangle
                        my_facesuv[uvname].append([facesuv_dict[uvname][i][mf[0]], facesuv_dict[uvname][i][mf[1]], facesuv_dict[uvname][i][mf[2]]])
                    else:        #quads
                        my_facesuv[uvname].append([facesuv_dict[uvname][i][mf[0]], facesuv_dict[uvname][i][mf[1]], facesuv_dict[uvname][i][mf[2]], facesuv_dict[uvname][i][mf[3]]])
                this_faces_index.append(j)
                j +=1
            my_faces.extend(this_faces)
            replaced_faces_dict[i] = this_faces_index   #face i substituted by this list of faces
        #endif on face vertex number
    #end loop on every face
    #now we have the new faces list and a dictionary replacement.
    #going for polygon tagging
    my_ptag_dict = {}
    for tag in polytag_dict:                                      #for every tag group
        my_ptag_dict[tag] = []                                    #rebuild a new entry
        for poly in polytag_dict[tag]:                            #take every element of old face list
            my_ptag_dict[tag].extend(replaced_faces_dict[poly])   #substitutes the element of new face list
    return my_faces, my_ptag_dict, my_facesuv


# ========================================
# === Revert list keeping first vertex ===
# ========================================
def revert (llist):
    #different flavors: the reverse one is the one that works better
    #rhead = [llist[0]]
    #rtail = llist[1:]
    #rhead.extend(rtail)
    #return rhead
    #--------------
    rhead=copy.deepcopy(llist)
    rhead.reverse()
    return rhead
    #--------------
    #return llist

# ====================================
# === Modified Create Blender Mesh ===
# ====================================
def my_create_mesh(complete_vertlist, complete_facelist, current_facelist, objname, not_used_faces):
    #take the needed faces and update the not-used face list
    vertex_map = [-1] * len(complete_vertlist)
    cur_ptag_faces = []
    for ff in current_facelist:
        cur_face = complete_facelist[ff]
        cur_ptag_faces.append(cur_face)
        if not_used_faces != []: not_used_faces[ff] = -1
        for vv in cur_face:
            vertex_map[vv] = 1
        #end loop on vertex on this face
    #end loop on faces

    mesh = Blender.NMesh.GetRaw()

    #append vertexes
    jj = 0
    for i in range(len(complete_vertlist)):
        if vertex_map[i] == 1:
            if not i%100 and my_meshtools.show_progress: Blender.Window.DrawProgressBar(float(i)/len(complete_vertlist), "Generating Verts")
            x, y, z = complete_vertlist[i]
            mesh.verts.append(Blender.NMesh.Vert(x, y, z))
            vertex_map[i] = jj
            jj += 1
    #end sweep over vertexes

    #append faces
    for i in range(len(cur_ptag_faces)):
        if not i%100 and my_meshtools.show_progress: Blender.Window.DrawProgressBar(float(i)/len(cur_ptag_faces), "Generating Faces")
        face = Blender.NMesh.Face()
        rev_face = revert(cur_ptag_faces[i])
        for vi in rev_face:
        #for vi in cur_ptag_faces[i]:
            index = vertex_map[vi]
            face.v.append(mesh.verts[index])
        #end sweep over vertexes
        mesh.faces.append(face)
    #end sweep over faces

    if not my_meshtools.overwrite_mesh_name:
        objname = my_meshtools.versioned_name(objname)
    Blender.NMesh.PutRaw(mesh, objname)    # Name the Mesh
    obj = Blender.Object.GetSelected()[0]
    obj.name=objname        # Name the Object
    Blender.Redraw()
    return obj, not_used_faces              #return the created object


# ============================================
# === Set Subsurf attributes on given mesh ===
# ============================================
def set_subsurf(obj):
    msh = obj.getData()
    msh.setSubDivLevels([2, 2])
    msh.mode |= Blender.NMesh.Modes.SUBSURF
    msh.update(1)
    obj.makeDisplayList()
    return


# =================================
# === object size and dimension ===
# =================================
def obj_size_pos(obj):
    bbox = obj.getBoundBox()
    bbox_min = map(lambda *row: min(row), *bbox) #transpose & get min
    bbox_max = map(lambda *row: max(row), *bbox) #transpose & get max
    obj_size = (bbox_max[0]-bbox_min[0], bbox_max[1]-bbox_min[1], bbox_max[2]-bbox_min[2])
    obj_pos = ( (bbox_max[0]+bbox_min[0]) / 2, (bbox_max[1]+bbox_min[1]) / 2, (bbox_max[2]+bbox_min[2]) / 2)
    return (obj_size, obj_pos)


# =========================
# === Create the object ===
# =========================
def create_objects(objspec_list):
    nf = len(objspec_list[3])
    not_used_faces = range(nf)
    ptag_dict = objspec_list[5]
    obj_dict = {}  #links tag names to object, used for material assignments
    obj_dim_dict = {}
    obj_list = []  #have it handy for parent association
    middlechar = "+"
    endchar = ""
    if (objspec_list[6] == 1):
        middlechar = endchar = "#"
    for cur_tag in ptag_dict.keys():
        if ptag_dict[cur_tag] != []:
            cur_obj, not_used_faces= my_create_mesh(objspec_list[2], objspec_list[3], ptag_dict[cur_tag], objspec_list[0][:9]+middlechar+cur_tag[:9], not_used_faces)
            if objspec_list[6] == 1:
                set_subsurf(cur_obj)
            obj_dict[cur_tag] = cur_obj
            obj_dim_dict[cur_tag] = obj_size_pos(cur_obj)
            obj_list.append(cur_obj)
    #end loop on current group
    #and what if some faces not used in any named PTAG? get rid of unused faces
    for ff in range(nf):
        tt = nf-1-ff #reverse order
        if not_used_faces[tt] == -1:
            not_used_faces.pop(tt)
    #end sweep on unused face list
    if not_used_faces != []:
        cur_obj, not_used_faces = my_create_mesh(objspec_list[2], objspec_list[3], not_used_faces, objspec_list[0][:9]+middlechar+"lone", [])
        #my_meshtools.create_mesh(objspec_list[2], not_used_faces, "_unk") #vert, faces, name
        #cur_obj = Blender.Object.GetSelected()[0]
        if objspec_list[6] == 1:
            set_subsurf(cur_obj)
        obj_dict["lone"] = cur_obj
        obj_dim_dict["lone"] = obj_size_pos(cur_obj)
        obj_list.append(cur_obj)
    objspec_list[1] = obj_dict
    objspec_list[4] = obj_dim_dict
    scene = Blender.Scene.getCurrent ()                   # get the current scene
    ob = Blender.Object.New ('Empty', objspec_list[0]+endchar)    # make empty object
    scene.link (ob)                                       # link the object into the scene
    ob.makeParent(obj_list, 1, 0)                         # set the root for created objects (no inverse, update scene hyerarchy (slow))
    Blender.Redraw()
    return


# =====================
# === Load an image ===
# =====================
#extensively search for image name
def load_image(dir_part, name):
    img = None
    nname = Blender.sys.splitext(name)
    lname = [c.lower() for c in nname]
    ext_list = []
    if lname[1] != nname[1]:
        ext_list.append(lname[1])
    ext_list.extend(['.tga', '.png', '.jpg', '.gif', '.bmp'])  #order from best to worst (personal judgement) bmp last cause of nasty bug
    #first round: original "case"
    current = Blender.sys.join(dir_part, name)
    name_list = [current]
    name_list.extend([Blender.sys.makename(current, ext) for ext in ext_list])
    #second round: lower "case"
    if lname[0] != nname[0]:
        current = Blender.sys.join(dir_part, lname[0])
        name_list.extend([Blender.sys.makename(current, ext) for ext in ext_list])
    for nn in name_list:
        if Blender.sys.exists(nn) == 1:
            break
    try:
        img = Blender.Image.Load(nn)
        return img
    except IOError:
        return None


# ===========================================
# === Lookup for image index in clip_list ===
# ===========================================
def lookup_imag(clip_list,ima_id):
    for ii in clip_list:
        if ii['ID'] == ima_id:
            if ii.has_key('XREF'):
                #cross reference - recursively look for images
                return lookup_imag(clip_list, ii['XREF'])
            else:
                return ii
    return None


# ===================================================
# === Create and assign image mapping to material ===
# ===================================================
def create_blok(surf, mat, clip_list, dir_part, obj_size, obj_pos):

    def output_size_ofs(size, pos, blok):
        #just automate repetitive task
        c_map = [0,1,2]
        c_map_txt = ["    X--", "    -Y-", "    --Z"]
        if blok['MAJAXIS'] == 0:
            c_map = [1,2,0]
        if blok['MAJAXIS'] == 2:
            c_map = [0,2,1]
        tobj.pprint ("!!!axis mapping:")
        for mp in c_map: tobj.pprint (c_map_txt[mp])

        s = ["1.0 (Forced)"] * 3
        o = ["0.0 (Forced)"] * 3
        if blok['SIZE'][0] > 0.0:          #paranoia controls
            s[0] = "%.5f" % (size[0]/blok['SIZE'][0])
            o[0] = "%.5f" % ((blok['CNTR'][0]-pos[0])/blok['SIZE'][0])
        if blok['SIZE'][1] > 0.0:
            s[2] = "%.5f" % (size[2]/blok['SIZE'][1])
            o[2] = "%.5f" % ((blok['CNTR'][1]-pos[2])/blok['SIZE'][1])
        if blok['SIZE'][2] > 0.0:
            s[1] = "%.5f" % (size[1]/blok['SIZE'][2])
            o[1] = "%.5f" % ((blok['CNTR'][2]-pos[1])/blok['SIZE'][2])
        tobj.pprint ("!!!texture size and offsets:")
        tobj.pprint ("    sizeX = %s; sizeY = %s; sizeZ = %s" % (s[c_map[0]], s[c_map[1]], s[c_map[2]]))
        tobj.pprint ("    ofsX = %s; ofsY = %s; ofsZ = %s" % (o[c_map[0]], o[c_map[1]], o[c_map[2]]))
        return

    ti = 0
    for blok in surf['BLOK']:
        tobj.pprint ("#...................................................................#")
        tobj.pprint ("# Processing texture block no.%s for surf %s" % (ti,surf['NAME']))
        tobj.pprint ("#...................................................................#")
        tobj.pdict (blok)
        if ti > 9: break                                    #only 8 channels 0..7 allowed for texture mapping
        if not blok['ENAB']:
            tobj.pprint (  "***Image is not ENABled! Quitting this block")
            break
        if not(blok.has_key('IMAG')):
            tobj.pprint (  "***No IMAGe for this block? Quitting")
            break                 #extract out the image index within the clip_list
        tobj.pprint ("looking for image number %d" % blok['IMAG'])
        ima = lookup_imag(clip_list, blok['IMAG'])
        if ima == None:
            tobj.pprint (  "***Block index image not within CLIP list? Quitting Block")
            break                              #safety check (paranoia setting)
        #look for images
        img = load_image("",ima['NAME'])
        if img == None:
            tobj.pprint (  "***No image %s found: trying LWO file subdir" % ima['NAME'])
            img = load_image(dir_part,ima['BASENAME'])
        if img == None:
            tobj.pprint (  "***No image %s found in directory %s: trying Images subdir" % (ima['BASENAME'], dir_part))
            img = load_image(dir_part+Blender.sys.sep+"Images",ima['BASENAME'])
        if img == None:
            tobj.pprint (  "***No image %s found: trying alternate Images subdir" % ima['BASENAME'])
            img = load_image(dir_part+Blender.sys.sep+".."+Blender.sys.sep+"Images",ima['BASENAME'])
        if img == None:
            tobj.pprint (  "***No image %s found: giving up" % ima['BASENAME'])
            break
        #lucky we are: we have an image
        tname = str(ima['ID'])
        if blok.has_key('CHAN'):
            tname = tname + "+" + blok['CHAN']
        newtex = Blender.Texture.New(tname)
        newtex.setType('Image')                 # make it an image texture
        newtex.image = img
        #how does it extends beyond borders
        if blok.has_key('WRAP'):
            if (blok['WRAP'] == 3) or (blok['WRAP'] == 2):
                newtex.setExtend('Extend')
            elif (blok['WRAP'] == 1):
                newtex.setExtend('Repeat')
            elif (blok['WRAP'] == 0):
                newtex.setExtend('Clip')
        tobj.pprint ("generated texture %s" % tname)

        blendmode_list = ['Mix',
                         'Subtractive',
                         'Difference',
                         'Multiply',
                         'Divide',
                         'Mix (CalcAlpha already set; try setting Stencil!',
                         'Texture Displacement',
                         'Additive']
        set_blendmode = 7 #default additive
        if blok.has_key('OPAC'):
            set_blendmode = blok['OPAC']
        if set_blendmode == 5: #transparency
            newtex.imageFlags |= Blender.Texture.ImageFlags.CALCALPHA
        tobj.pprint ("!!!Set Texture -> MapTo -> Blending Mode = %s" % blendmode_list[set_blendmode])

        set_dvar = 1.0
        if blok.has_key('OPACVAL'):
            set_dvar = blok['OPACVAL']

        #MapTo is determined by CHAN parameter
        mapflag = Blender.Texture.MapTo.COL  #default to color
        if blok.has_key('CHAN'):
            if blok['CHAN'] == 'COLR':
                tobj.pprint ("!!!Set Texture -> MapTo -> Col = %.3f" % set_dvar)
                if set_blendmode == 0:
                    surf['g_IM'] = img                 #do not set anything, just save image object for later assignment
            if blok['CHAN'] == 'BUMP':
                mapflag = Blender.Texture.MapTo.NOR
                tobj.pprint ("!!!Set Texture -> MapTo -> Nor = %.3f" % set_dvar)
            if blok['CHAN'] == 'LUMI':
                mapflag = Blender.Texture.MapTo.EMIT
                tobj.pprint ("!!!Set Texture -> MapTo -> DVar = %.3f" % set_dvar)
            if blok['CHAN'] == 'DIFF':
                mapflag = Blender.Texture.MapTo.REF
                tobj.pprint ("!!!Set Texture -> MapTo -> DVar = %.3f" % set_dvar)
            if blok['CHAN'] == 'SPEC':
                mapflag = Blender.Texture.MapTo.SPEC
                tobj.pprint ("!!!Set Texture -> MapTo -> DVar = %.3f" % set_dvar)
            if blok['CHAN'] == 'TRAN':
                mapflag = Blender.Texture.MapTo.ALPHA
                tobj.pprint ("!!!Set Texture -> MapTo -> DVar = %.3f" % set_dvar)
        if blok.has_key('NEGA'):
            tobj.pprint ("!!!Watch-out: effect of this texture channel must be INVERTED!")

        #the TexCo flag is determined by PROJ parameter
        if blok.has_key('PROJ'):
            if blok['PROJ'] == 0: #0 - Planar
               tobj.pprint ("!!!Flat projection")
               coordflag = Blender.Texture.TexCo.ORCO
               output_size_ofs(obj_size, obj_pos, blok)
            elif blok['PROJ'] == 1: #1 - Cylindrical
               tobj.pprint ("!!!Cylindrical projection")
               coordflag = Blender.Texture.TexCo.ORCO
               output_size_ofs(obj_size, obj_pos, blok)
            elif blok['PROJ'] == 2: #2 - Spherical
               tobj.pprint ("!!!Spherical projection")
               coordflag = Blender.Texture.TexCo.ORCO
               output_size_ofs(obj_size, obj_pos, blok)
            elif blok['PROJ'] == 3: #3 - Cubic
               tobj.pprint ("!!!Cubic projection")
               coordflag = Blender.Texture.TexCo.ORCO
               output_size_ofs(obj_size, obj_pos, blok)
            elif blok['PROJ'] == 4: #4 - Front Projection
               tobj.pprint ("!!!Front projection")
               coordflag = Blender.Texture.TexCo.ORCO
               output_size_ofs(obj_size, obj_pos, blok)
            elif blok['PROJ'] == 5: #5 - UV
               tobj.pprint ("UVMapped")
               coordflag = Blender.Texture.TexCo.UV
        mat.setTexture(ti, newtex, coordflag, mapflag)
        ti += 1
    #end loop over bloks
    return




# ========================================
# === Create and assign a new material ===
# ========================================
#def create_material(surf_list, ptag_dict, obj, clip_list, uv_dict, dir_part):
def create_material(clip_list, surf_list, objspec, dir_part):
    if (surf_list == []) or (objspec[5] == {}) or (objspec[1] == {}):
        tobj.pprint( surf_list)
        tobj.pprint( objspec[5])
        tobj.pprint( objspec[1])
        tobj.pprint( "something getting wrong in create_material ...")
        return
    obj_dict = objspec[1]
    obj_dim_dict = objspec[4]
    ptag_dict = objspec[5]
    uvcoords_dict = objspec[7]
    facesuv_dict = objspec[8]
    for surf in surf_list:
        if (surf['NAME'] in ptag_dict.keys()):
            tobj.pprint ("#-------------------------------------------------------------------#")
            tobj.pprint ("Processing surface (material): %s" % surf['NAME'])
            tobj.pprint ("#-------------------------------------------------------------------#")
            #material set up
            facelist = ptag_dict[surf['NAME']]
            #bounding box and position
            cur_obj = obj_dict[surf['NAME']]
            obj_size = obj_dim_dict[surf['NAME']][0]
            obj_pos = obj_dim_dict[surf['NAME']][1]
            tobj.pprint(surf)
            mat = Blender.Material.New(surf['NAME'])
            if surf.has_key('COLR'):
                mat.rgbCol = surf['COLR']
            if surf.has_key('LUMI'):
                mat.setEmit(surf['LUMI'])
            if surf.has_key('GVAL'):
                mat.setAdd(surf['GVAL'])
            if surf.has_key('SPEC'):
                mat.setSpec(surf['SPEC'])                        #it should be * 2 but seems to be a bit higher lwo [0.0, 1.0] - blender [0.0, 2.0]
            if surf.has_key('DIFF'):
                mat.setRef(surf['DIFF'])                         #lwo [0.0, 1.0] - blender [0.0, 1.0]
            if surf.has_key('REFL'):
                mat.setRayMirr(surf['REFL'])                     #lwo [0.0, 1.0] - blender [0.0, 1.0]
                #mat.setMode('RAYMIRROR')
                mat.mode |= Blender.Material.Modes.RAYMIRROR
            #WARNING translucency not implemented yet check 2.36 API
            #if surf.has_key('TRNL'):
            #
            if surf.has_key('GLOS'):                             #lwo [0.0, 1.0] - blender [0, 255]
                glo = int(371.67 * surf['GLOS'] - 42.334)        #linear mapping - seems to work better than exp mapping
                if glo <32:  glo = 32                            #clamped to 32-255
                if glo >255: glo = 255
                mat.setHardness(glo)
            if surf.has_key('TRAN'):
                mat.setAlpha(1.0-surf['TRAN'])                                        #lwo [0.0, 1.0] - blender [1.0, 0.0]
                mat.mode |= Blender.Material.Modes.RAYTRANSP
            if surf.has_key('RIND'):
                s = surf['RIND']
                if s < 1.0: s = 1.0
                if s > 3.0: s = 3.0
                mat.setIOR(s)                                                         #clipped to blender [1.0, 3.0]
                mat.mode |= Blender.Material.Modes.RAYTRANSP
            if surf.has_key('BLOK') and surf['BLOK'] != []:
                #update the material according to texture.
                create_blok(surf, mat, clip_list, dir_part, obj_size, obj_pos)
            #finished setting up the material
            #associate material to mesh
            msh = cur_obj.getData()
            mat_index = len(msh.getMaterials(1))
            msh.addMaterial(mat)
            msh.mode |= Blender.NMesh.Modes.AUTOSMOOTH                                #smooth it anyway
            msh.update(1)
            for f in range(len(msh.faces)):
                msh.faces[f].materialIndex = mat_index
                msh.faces[f].smooth = 1 #smooth it anyway
                msh.faces[f].mode |= Blender.NMesh.FaceModes.TWOSIDE                  #set it anyway
                msh.faces[f].transp = Blender.NMesh.FaceTranspModes['SOLID']
                msh.faces[f].flag = Blender.NMesh.FaceTranspModes['SOLID']
                if surf.has_key('SMAN'):
                    #not allowed mixed mode mesh (all the mesh is smoothed and all with the same angle)
                    #only one smoothing angle will be active! => take the max one
                    s = int(surf['SMAN']/3.1415926535897932384626433832795*180.0)     #lwo in radians - blender in degrees
                    if msh.getMaxSmoothAngle() < s: msh.setMaxSmoothAngle(s)
                #if surf.has_key('SIDE'):
                #    msh.faces[f].mode |= Blender.NMesh.FaceModes.TWOSIDE             #set it anyway
                if surf.has_key('TRAN') and mat.getAlpha()<1.0:
                    msh.faces[f].transp = Blender.NMesh.FaceTranspModes['ALPHA']
                if surf.has_key('UVNAME') and facesuv_dict.has_key(surf['UVNAME']):
                    #assign uv-data
                    msh.hasFaceUV(1)
                    #WARNING every block could have its own uvmap set of coordinate. take only the first one
                    facesuv_list = facesuv_dict[surf['UVNAME']]
                    #print "facesuv_list: ",f , facelist[f]
                    rev_face = revert(facesuv_list[facelist[f]])
                    for vi in rev_face:
                        msh.faces[f].uv.append(uvcoords_dict[surf['UVNAME']][vi])
                    if surf.has_key('g_IM'):
                        msh.faces[f].mode |= Blender.NMesh.FaceModes['TEX']
                        msh.faces[f].image = surf['g_IM']
            #end loop over faces
            msh.update(1)
            mat_index += 1
        #end if exist faces ib this object belonging to surf
    #end loop on surfaces
    return


# ======================
# === Read Faces 6.0 ===
# ======================
def read_faces_6(lwochunk):
    data = cStringIO.StringIO(lwochunk.read())
    faces = []
    polygon_type = data.read(4)
    subsurf = 0
    if polygon_type != "FACE" and polygon_type != "PTCH":
        tobj.pprint("No FACE/PATCH Were Found. Polygon Type: %s" % polygon_type)
        return "", 2
    if polygon_type == 'PTCH': subsurf = 1
    i = 0
    while(i < lwochunk.chunksize-4):
        if not i%100 and my_meshtools.show_progress:
            Blender.Window.DrawProgressBar(float(i)/lwochunk.chunksize, "Reading Faces")
        facev = []
        numfaceverts, = struct.unpack(">H", data.read(2))
        i += 2

        for j in range(numfaceverts):
            index, index_size = read_vx(data)
            i += index_size
            facev.append(index)
        faces.append(facev)
    tobj.pprint("read %s faces; type of block %d (0=FACE; 1=PATCH)" % (len(faces), subsurf))
    return faces, subsurf



# ===========================================================
# === Start the show and main callback ======================
# ===========================================================

def fs_callback(filename):
    read(filename)

Blender.Window.FileSelector(fs_callback, "Import LWO")
