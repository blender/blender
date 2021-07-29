from .base_exporter import BasePrimitiveDXFExporter


class CurveDXFExporter(BasePrimitiveDXFExporter):
    pass

#-----------------------------------------------------
def exportCurve(ob, mx, mw, **common):
    """converts Curve-Object to desired projection and representation(DXF-Entity type)
    """
    entities = []
    block = None
    curve = ob.getData()
    #print 'deb: curve=', dir(curve) #---------
    # TODO: should be: if curve.users>1 and not (PERSPECTIVE or (PROJECTION and HIDDEN_MODE):
    if INSTANCES and curve.users>1 and not PROJECTION:
        if curve.name in BLOCKREGISTRY.keys():
            insert_name = BLOCKREGISTRY[curve.name]
            # write INSERT to entities
            entities = exportInsert(ob, mx,insert_name, **common)
        else:
            # generate geom_output in ObjectCS
            imx = mathutils.Matrix().identity()
            WCS_loc = [0,0,0] # WCS_loc is object location in WorldCoordSystem
            #print 'deb: WCS_loc=', WCS_loc #---------
            sizeX = sizeY = sizeZ = 1.0
            rotX  = rotY  = rotZ = 0.0
            Thickness,Extrusion,ZRotation,Elevation = None,None,None,None
            ZRotation,Zrotmatrix,OCS_origin,ECS_origin = None,None,None,None
            AXaxis = imx[0].copy().resize3D() # = ArbitraryXvector
            OCS_origin = [0,0,0]
            if not PROJECTION:
                #Extrusion, ZRotation, Elevation = getExtrusion(mx)
                Extrusion, AXaxis = getExtrusion(imx)

                # no thickness/width for POLYLINEs converted into Screen-C-S
                #print 'deb: curve.ext1=', curve.ext1 #---------
                if curve.ext1: Thickness = curve.ext1 * sizeZ
                if curve.ext2 and sizeX==sizeY:
                    Width = curve.ext2 * sizeX
                if "POLYLINE"==curve_as_list[GUI_A['curve_as'].val]: # export as POLYLINE
                    ZRotation,Zrotmatrix,OCS_origin,ECS_origin = getTargetOrientation(imx,Extrusion,\
                        AXaxis,WCS_loc,sizeX,sizeY,sizeZ,rotX,rotY,rotZ)

            entities = writeCurveEntities(curve, imx,
                Thickness,Extrusion,ZRotation,Elevation,AXaxis,Zrotmatrix,
                WCS_loc,OCS_origin,ECS_origin,sizeX,sizeY,sizeZ,
                **common)

            if entities: # if not empty block
                # write BLOCK definition and INSERT entity
                # BLOCKREGISTRY = dictionary 'blender_name':'dxf_name'.append(me.name)
                BLOCKREGISTRY[curve.name]=validDXFr12name(('CU_'+ curve.name))
                insert_name = BLOCKREGISTRY[curve.name]
                block = DXF.Block(insert_name,flag=0,base=(0,0,0),entities=entities)
                # write INSERT as entity
                entities = exportInsert(ob, mx, insert_name, **common)

    else: # no other instances, so go the standard way
        WCS_loc = ob.loc # WCS_loc is object location in WorldCoordSystem
        #print 'deb: WCS_loc=', WCS_loc #---------
        sizeX = ob.SizeX
        sizeY = ob.SizeY
        sizeZ = ob.SizeZ
        rotX  = ob.RotX
        rotY  = ob.RotY
        rotZ  = ob.RotZ
        #print 'deb: sizeX=%s, sizeY=%s' %(sizeX, sizeY) #---------

        Thickness,Extrusion,ZRotation,Elevation = None,None,None,None
        ZRotation,Zrotmatrix,OCS_origin,ECS_origin = None,None,None,None
        AXaxis = mx[0].copy().resize3D() # = ArbitraryXvector
        OCS_origin = [0,0,0]
        if not PROJECTION:
            #Extrusion, ZRotation, Elevation = getExtrusion(mx)
            Extrusion, AXaxis = getExtrusion(mx)

            # no thickness/width for POLYLINEs converted into Screen-C-S
            #print 'deb: curve.ext1=', curve.ext1 #---------
            if curve.ext1: Thickness = curve.ext1 * sizeZ
            if curve.ext2 and sizeX==sizeY:
                Width = curve.ext2 * sizeX
            if "POLYLINE"==curve_as_list[GUI_A['curve_as'].val]: # export as POLYLINE
                ZRotation,Zrotmatrix,OCS_origin,ECS_origin = getTargetOrientation(mx,Extrusion,\
                    AXaxis,WCS_loc,sizeX,sizeY,sizeZ,rotX,rotY,rotZ)
        entities = writeCurveEntities(curve, mx,
                Thickness,Extrusion,ZRotation,Elevation,AXaxis,Zrotmatrix,
                WCS_loc,OCS_origin,ECS_origin,sizeX,sizeY,sizeZ,
                **common)

    return entities, block


#-------------------------------------------------
def writeCurveEntities(curve, mx,
        Thickness,Extrusion,ZRotation,Elevation,AXaxis,Zrotmatrix,
        WCS_loc,OCS_origin,ECS_origin,sizeX,sizeY,sizeZ,
        **common):
    """help routine for exportCurve()
    """
    entities = []
    width1,width2 = None, None
    if 1:
        for cur in curve:
            #print 'deb: START cur=', cur #--------------
            #print 'deb:  dir(curve):',dir(cur)  #---------
            #print 'deb:  curve.type:',cur.type  #---------
            points = []
            flags = []
            pflag70, pflag75 = 0,0

            if cur.type==4: # is NURBS
                #if cur.isNurb():
                #print 'deb:isNurb --------------'  #---------
                pflag70 = 4
                orderU = cur.orderU
                # curve type:
                # 0: curvNoFitted
                # 5: curvQuadraticBspline
                # 6: curvCubicBspline
                # 8: curvBezier
                if orderU<=4: pflag75 = 5
                elif orderU>=5: pflag75 = 6

                vflag70 = 16
                i = -2
                for point in cur:
                    #print 'deb:isNurb point=', point #---------
                    i+=1
                    if i==orderU-1: i = 0
                    if i:
                        flags.append([16, [width1,width2]])
                    else:
                        flags.append([8, [width1,width2]])
                    vec = point[0:3]
                    #print 'deb: vec=', vec #---------
                    pkt = mathutils.Vector(vec)
                    #print 'deb: pkt=', pkt #---------
                    points.append(pkt)
                if not cur.isCyclic():
                    points = points[1:-1]
                    flags = flags[1:-1]
            elif cur.type==1: # is Bezier
                #print 'deb:isBezier --------------'  #---------
                pflag75 = 8
                vflag70 = 1
                for point in cur:
                    #print 'deb:isBezier point=', point #---------
                    #print 'deb:isBezier point=', point.getTriple() #---------
                    ptan1,pfit,ptan2 = point.getTriple()
                    #print 'deb: point=', pt #---------
                    ptan1 = mathutils.Vector(ptan1)
                    pfit = mathutils.Vector(pfit)
                    ptan2 = mathutils.Vector(ptan2)
                    #print 'deb: pkt=', pkt #---------
                    points.append(ptan1)
                    flags.append([2, [width1,width2]])
                    points.append(pfit)
                    flags.append([1, [width1,width2]])
                    points.append(ptan2)
                    flags.append([2, [width1,width2]])
                if not cur.isCyclic():
                    points = points[1:-1]
                    flags = flags[1:-1]
            elif cur.type==0: # is Polygon
                #print 'deb:isPolygon --------------'  #---------
                #pflag70 = 4
                pflag75 = 0
                for point in cur:
                    #print 'deb:isPoly point=', point #---------
                    vec = point[0:3]
                    #print 'deb: vec=', vec #---------
                    pkt = mathutils.Vector(vec)
                    #print 'deb: pkt=', pkt #---------
                    points.append(pkt)
                    flags.append([None, [width1,width2]])

            #print 'deb: points', points #--------------
            if len(points)>1:
                c = curve_as_list[GUI_A['curve_as'].val]

                if c=="POLYLINE": # export Curve as POLYLINE
                    if not PROJECTION:
                        # recalculate points(2d=X,Y) into Entity-Coords-System
                        for p in points: # list of vectors
                            p[0] *= sizeX
                            p[1] *= sizeY
                            p2 = p * Zrotmatrix
                            p2[0] += ECS_origin[0]
                            p2[1] += ECS_origin[1]
                            p[0],p[1] = p2[0],p2[1]
                    else:
                        points = projected_co(points, mx)
                    #print 'deb: points', points #--------------

                    if cur.isCyclic(): closed = 1
                    else: closed = 0
                    points = toGlobalOrigin(points)
                    points_temp = []
                    for p,f in zip(points,flags):
                        points_temp.append([p,f[0],f[1]])
                    points = points_temp
                    #print 'deb: points', points #--------------

                    if DEBUG: curve_drawBlender(points,OCS_origin,closed) #deb: draw to scene

                    common['extrusion']= Extrusion
                    ##common['rotation']= ZRotation
                    ##common['elevation']= Elevation
                    common['thickness']= Thickness
                    #print 'deb: common=', common #------------------

                    flag70, flag75 = pflag70+closed, pflag75
                    if 0: #DEBUG
                        p=AXaxis[:3]
                        entities.append(DXF.Line([[0,0,0], p],**common))
                        p=ECS_origin[:3]
                        entities.append(DXF.Line([[0,0,0], p],**common))
                        common['color']= 5
                        p=OCS_origin[:3]
                        entities.append(DXF.Line([[0,0,0], p],**common))
                        #OCS_origin=[0,0,0] #only debug----------------
                        dxfPLINE = DXF.PolyLine(points,OCS_origin, flag70=flag70, flag75=flag70, width=0.0,**common)
                        entities.append(dxfPLINE)

                    dxfPLINE = DXF.PolyLine(points,OCS_origin, flag70=flag70, flag75=flag70, width=0.0,**common)
                    entities.append(dxfPLINE)
                    if Thickness:
                        common['thickness']= -Thickness
                        dxfPLINE = DXF.PolyLine(points,OCS_origin, flag70=flag70, flag75=flag70, width=0.0,**common)
                        entities.append(dxfPLINE)

                elif c=="LINEs": # export Curve as multiple LINEs
                    points = projected_co(points, mx)
                    if cur.isCyclic(): points.append(points[0])
                    #print 'deb: points', points #--------------
                    points = toGlobalOrigin(points)

                    if DEBUG: curve_drawBlender(points,WCS_loc,closed) #deb: draw to scene
                    common['extrusion']= Extrusion
                    common['elevation']= Elevation
                    common['thickness']= Thickness
                    #print 'deb: common=', common #------------------
                    for i in range(len(points)-1):
                        linepoints = [points[i], points[i+1]]
                        dxfLINE = DXF.Line(linepoints,**common)
                        entities.append(dxfLINE)
                    if Thickness:
                        common['thickness']= -Thickness
                        for i in range(len(points)-1):
                            linepoints = [points[i], points[i+1]]
                            dxfLINE = DXF.Line(linepoints,**common)
                            entities.append(dxfLINE)

                elif c=="POINTs": # export Curve as multiple POINTs
                    points = projected_co(points, mx)
                    for p in points:
                        dxfPOINT = DXF.Point(points=[p],**common)
                        entities.append(dxfPOINT)
    return entities
