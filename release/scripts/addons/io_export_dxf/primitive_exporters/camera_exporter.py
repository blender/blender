from .base_exporter import BasePrimitiveDXFExporter


class CameraDXFExporter(BasePrimitiveDXFExporter):
    pass


#-----------------------------------------------------
def exportCamera(ob, mx, mw, **common):
    """converts Camera-Object to desired projection and representation(DXF-Entity type)
    """
    location =  mathutils.Vector(ob.loc)
    [location] = projected_co([location], mx)
    [location] = toGlobalOrigin([location])
    view_name=validDXFr12name(('CAM_'+ ob.name))

    camera = Camera.Get(ob.getData(name_only=True))
    #print 'deb: camera=', dir(camera) #------------------
    if camera.type=='persp':
        mode = 1+2+4+16
        # mode flags: 1=persp, 2=frontclip, 4=backclip,16=FrontZ
    elif camera.type=='ortho':
        mode = 0+2+4+16

    leftBottom=(0.0,0.0) # default
    rightTop=(1.0,1.0) # default
    center=(0.0,0.0) # default

    direction = mathutils.Vector(0.0,0.0,1.0) * mx.to_euler().to_matrix() # in W-C-S
    direction.normalize()
    target=mathutils.Vector(ob.loc) - direction # in W-C-S
    #ratio=1.0
    width=height= camera.scale # for ortho-camera
    lens = camera.lens # for persp-camera
    frontClipping = -(camera.clipStart - 1.0)
    backClipping = -(camera.clipEnd - 1.0)

    entities, vport, view = [], None, None
    c = camera_as_list[GUI_A['camera_as'].val]
    if c=="POINT": # export as POINT
        dxfPOINT = DXF.Point(points=[location],**common)
        entities.append(dxfPOINT)
    elif c=="VIEW": # export as VIEW
        view = DXF.View(name=view_name,
            center=center, width=width, height=height,
            frontClipping=frontClipping,backClipping=backClipping,
            direction=direction,target=target,lens=lens,mode=mode
            )
    elif c=="VPORT": # export as VPORT
        vport = DXF.VPort(name=view_name,
            center=center, ratio=1.0, height=height,
            frontClipping=frontClipping,backClipping=backClipping,
            direction=direction,target=target,lens=lens,mode=mode
            )
    return entities, vport, view
