from .base_exporter import BasePrimitiveDXFExporter


class TextDXFExporter(BasePrimitiveDXFExporter):
    pass

#-----------------------------------------------------
def exportText(ob, mx, mw, **common):
    """converts Text-Object to desired projection and representation(DXF-Entity type)
    """
    text3d = ob.getData()
    textstr = text3d.getText()
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

        # no thickness/width for TEXTs converted into ScreenCS
        if text3d.getExtrudeDepth():
            Thickness = text3d.getExtrudeDepth() * sizeZ

    #Horizontal text justification type, code 72, (optional, default = 0)
    # integer codes (not bit-coded)
    #0=left, 1=center, 2=right
    #3=aligned, 4=middle, 5=fit
    Alignment = None
    alignment = text3d.getAlignment().value
    if alignment in {1, 2}: Alignment = alignment

    textHeight = text3d.getSize() / 1.7
    textFlag = 0
    if sizeX < 0.0: textFlag |= 2 # set flag for horizontal mirrored
    if sizeZ < 0.0: textFlag |= 4 # vertical mirrored

    entities = []
    c = text_as_list[GUI_A['text_as'].val]

    if c=="TEXT": # export text as TEXT
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
                textHeight *= coef
                #print 'deb: coef=', coef #--------------

        #print 'deb: point=', point #--------------
        [point] = toGlobalOrigin([point])
        point2 = point

        #if DEBUG: text_drawBlender(textstr,points,OCS_origin) #deb: draw to scene
        common['extrusion']= Extrusion
        #common['elevation']= Elevation
        common['thickness']= Thickness
        #print 'deb: common=', common #------------------
        if 0: #DEBUG
            #linepoints = [[0,0,0], [AXaxis[0],AXaxis[1],AXaxis[2]]]
            linepoints = [[0,0,0], point]
            dxfLINE = DXF.Line(linepoints,**common)
            entities.append(dxfLINE)

        dxfTEXT = DXF.Text(text=textstr,point=point,alignment=point2,rotation=ZRotation,\
            flag=textFlag,height=textHeight,justifyhor=Alignment,**common)
        entities.append(dxfTEXT)
        if Thickness:
            common['thickness']= -Thickness
            dxfTEXT = DXF.Text(text=textstr,point=point,alignment=point2,rotation=ZRotation,\
                flag=textFlag,height=textHeight,justifyhor=Alignment,**common)
            entities.append(dxfTEXT)
    return entities


