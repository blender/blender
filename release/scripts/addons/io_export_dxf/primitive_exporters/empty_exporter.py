from .base_exporter import BasePrimitiveDXFExporter


class EmptyDXFExporter(BasePrimitiveDXFExporter):
    pass

#-----------------------------------------------------
def exportEmpty(ob, mx, mw, **common):
    """converts Empty-Object to desired projection and representation(DXF-Entity type)
    """
    p =  mathutils.Vector(ob.loc)
    [p] = projected_co([p], mx)
    [p] = toGlobalOrigin([p])

    entities = []
    c = empty_as_list[GUI_A['empty_as'].val]
    if c=="POINT": # export Empty as POINT
        dxfPOINT = DXF.Point(points=[p],**common)
        entities.append(dxfPOINT)
    return entities
