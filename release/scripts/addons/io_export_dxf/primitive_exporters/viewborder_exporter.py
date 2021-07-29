from base_exporter import BasePrimitiveDXFExporter


class ViewBorderDXFExporter(BasePrimitiveDXFExporter):

    def export(self, ob, mx, mw, **common):
        """converts Lamp-Object to desired projection and representation(DXF-Entity type)
        """
        identity_matrix = mathutils.Matrix().identity()
        points = projected_co(border, identity_matrix)
        closed = 1
        points = toGlobalOrigin(points)
        c = settings['curve_as']
        if c=="LINEs": # export Curve as multiple LINEs
            for i in range(len(points)-1):
                linepoints = [points[i], points[i+1]]
                dxfLINE = DXF.Line(linepoints,paperspace=espace,color=LAYERCOLOR_DEF)
                entities.append(dxfLINE)
        else:
            fag70, flag75 = closed, 0
            dxfPOLYFACE = DXF.PolyLine([allpoints, faces], flag70=flag70, flag75=flag70, width=0.0, paperspace=espace, color=LAYERCOLOR_DEF)
            #dxfPLINE = DXF.PolyLine(points,points[0],[closed,0,0], paperspace=espace, color=LAYERCOLOR_DEF)
            d.append(dxfPLINE)

