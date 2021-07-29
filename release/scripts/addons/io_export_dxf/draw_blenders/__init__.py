""" NOTE:
This stuff was in original code but it seems it will be no longer needed.
NOT USED now.
"""

#-----------------------------------------------------
def mesh_drawBlender(vertList, edgeList, faceList, name="dxfMesh", flatten=False, AT_CUR=True, link=True):
    #print 'deb:mesh_drawBlender started XXXXXXXXXXXXXXXXXX' #---------
    ob = Object.New("Mesh",name)
    me = Mesh.New(name)
    #print 'deb: vertList=\n', vertList #---------
    #print 'deb: edgeList=\n', edgeList #---------
    #print 'deb: faceList=\n', faceList #---------
    me.verts.extend(vertList)
    if edgeList: me.edges.extend(edgeList)
    if faceList: me.faces.extend(faceList)
    if flatten:
        for v in me.verts: v.co.z = 0.0
    ob.link(me)
    if link:
        sce = Scene.getCurrent()
        sce.objects.link(ob)
        #me.triangleToQuad()
        if AT_CUR:
            cur_loc = Window.GetCursorPos()
            ob.setLocation(cur_loc)
        Blender.Redraw()
    #return ob

#-----------------------------------------------------
def curve_drawBlender(vertList, org_point=[0.0,0.0,0.0], closed=0, name="dxfCurve", flatten=False, AT_CUR=True, link=True):
    #print 'deb:curve_drawBlender started XXXXXXXXXXXXXXXXXX' #---------
    ob = Object.New("Curve",name)
    cu = Curve.New(name)
    #print 'deb: vertList=\n', vertList #---------
    curve = cu.appendNurb(BezTriple.New(vertList[0][0]))
    for p in vertList[1:]:
        curve.append(BezTriple.New(p[0]))
    for point in curve:
        #point.handleTypes = [VECT, VECT]
        point.handleTypes = [FREE, FREE]
        point.radius = 1.0
    curve.flagU = closed # 0 sets the curve not cyclic=open
    cu.setResolu(6)
    cu.update() #important for handles calculation
    if flatten:
        for v in cu.verts: v.co.z = 0.0
    ob.link(cu)
    if link:
        sce = Scene.getCurrent()
        sce.objects.link(ob)
        #me.triangleToQuad()
        if AT_CUR:
            cur_loc = Window.GetCursorPos()
            ob.setLocation(cur_loc)
        elif org_point:
            cur_loc=org_point
            ob.setLocation(cur_loc)
        Blender.Redraw()
    #return ob

#-----------------------------------------------------
def drawClipBox(clip_box):
    """debug tool: draws Clipping-Box of a Camera View
    """
    min_X1, max_X1, min_Y1, max_Y1,\
    min_X2, max_X2, min_Y2, max_Y2,\
        min_Z, max_Z = clip_box
    verts = []
    verts.append([min_X1, min_Y1, min_Z])
    verts.append([max_X1, min_Y1, min_Z])
    verts.append([max_X1, max_Y1, min_Z])
    verts.append([min_X1, max_Y1, min_Z])
    verts.append([min_X2, min_Y2, max_Z])
    verts.append([max_X2, min_Y2, max_Z])
    verts.append([max_X2, max_Y2, max_Z])
    verts.append([min_X2, max_Y2, max_Z])
    faces = [[0,1,2,3],[4,5,6,7]]
    newmesh = Mesh.New()
    newmesh.verts.extend(verts)
    newmesh.faces.extend(faces)

    plan = Object.New('Mesh','clip_box')
    plan.link(newmesh)
    sce = Scene.GetCurrent()
    sce.objects.link(plan)
    plan.setMatrix(sce.objects.camera.matrix)
