
from .base_exporter import BasePrimitiveDXFExporter


class InsertDXFExporter(BasePrimitiveDXFExporter):
    pass

#-----------------------------------------------------
def exportInsert(ob, mx, insert_name, **common):
    """converts Object to DXF-INSERT in given orientation
    """
    WCS_loc = ob.loc # WCS_loc is object location in WorldCoordSystem
    sizeX = ob.SizeX
    sizeY = ob.SizeY
    sizeZ = ob.SizeZ
    rotX  = ob.RotX
    rotY  = ob.RotY
    rotZ  = ob.RotZ
    #print 'deb: sizeX=%s, sizeY=%s' %(sizeX, sizeY) #---------

    Thickness,Extrusion,ZRotation,Elevation = None,None,None,None

    AXaxis = mx[0].copy().resize3D() # = ArbitraryXvector
    if not PROJECTION:
        #Extrusion, ZRotation, Elevation = getExtrusion(mx)
        Extrusion, AXaxis = getExtrusion(mx)

    entities = []

    if 1:
        if not PROJECTION:
            ZRotation,Zrotmatrix,OCS_origin,ECS_origin = getTargetOrientation(mx,Extrusion,\
                AXaxis,WCS_loc,sizeX,sizeY,sizeZ,rotX,rotY,rotZ)
            ZRotation *= r2d
            point = ECS_origin
        else:    #TODO: fails correct location
            point1 = mathutils.Vector(ob.loc)
            [point] = projected_co([point1], mx)
            if PERSPECTIVE:
                clipStart = 10.0
                coef = -clipStart / (point1*mx)[2]
                #print 'deb: coef=', coef #--------------
                #TODO: ? sizeX *= coef
                #sizeY *= coef
                #sizeZ *= coef

        #print 'deb: point=', point #--------------
        [point] = toGlobalOrigin([point])

        #if DEBUG: text_drawBlender(textstr,points,OCS_origin) #deb: draw to scene
        common['extrusion']= Extrusion
        #common['elevation']= Elevation
        #print 'deb: common=', common #------------------
        if 0: #DEBUG
            #linepoints = [[0,0,0], [AXaxis[0],AXaxis[1],AXaxis[2]]]
            linepoints = [[0,0,0], point]
            dxfLINE = DXF.Line(linepoints,**common)
            entities.append(dxfLINE)

        xscale=sizeX
        yscale=sizeY
        zscale=sizeZ
        cols=None
        colspacing=None
        rows=None
        rowspacing=None

        dxfINSERT = DXF.Insert(insert_name,point=point,rotation=ZRotation,\
            xscale=xscale,yscale=yscale,zscale=zscale,\
            cols=cols,colspacing=colspacing,rows=rows,rowspacing=rowspacing,\
            **common)
        entities.append(dxfINSERT)

    return entities

