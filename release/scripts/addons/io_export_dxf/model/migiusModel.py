"""
Created on Sep 2, 2011

@author: vencax
"""

from .model import DxfDrawing

try:
    from . import dxfLibrary as DXF
    #reload(DXF)
    #reload(dxfLibrary)
    #from dxfLibrary import *
except Exception:
    raise Exception("No dxfLibrary.py module in Blender script folder found!")

#------------------------------------------------------
#def col2RGB(color):
#    return [int(floor(255*color[0])),
#            int(floor(255*color[1])),
#            int(floor(255*color[2]))]
#
#global dxfColors
#dxfColors=None
#
##------------------------------------------------------
#def col2DXF(rgbcolor):
#    global dxfColors
#    if dxfColors is None:
#        from dxfColorMap import color_map
#        dxfColors = [(tuple(color),idx) for idx, color in color_map.iteritems()]
#        dxfColors.sort()
#    entry = (tuple(rgbcolor), -1)
#    dxfColors.append(entry)
#    dxfColors.sort()
#    i = dxfColors.index(entry)
#    dxfColors.pop(i)
#    return dxfColors[i-1][1]

"""
v = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,\
28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,46,47,58,59,60,61,62,63,64,91,92,93,94,96,\
123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,\
151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,\
171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,\
191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,\
211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,\
231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254]
invalid = ''.join([chr(i) for i in v])
del v, i
"""
#TODO: validDXFr14 = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.'
validDXFr12 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_'
NAMELENGTH_MAX = 80   #max_obnamelength in DXF, (limited to 256? )
#------------------------------------------------------
def cleanName(name, valid):
    validname = ''
    for ch in name:
        if ch not in valid:    ch = '_'
        validname += ch
    return validname

#------------------------------------------------------
def validDXFr12name(str_name):
    dxfname = str(str_name)
    dxfname = dxfname[:NAMELENGTH_MAX].upper()
    dxfname = cleanName(dxfname, validDXFr12)
    return dxfname

class MigiusDXFLibDrawing(DxfDrawing):
    """ Drawing that can convert itself into MIGIUS DXFLib stuff objects """

    def convert(self, file, **kwargs):
        drawing = self._write()
        for type, ents in self._entities.items():
            self._processEntityArray(drawing, type, ents)
        for v in self._views:
            drawing.views.append(DXF.View(**v))
        for v in self._vports:
            drawing.vports.append(DXF.VPort(**v))
#        for l in self._layers:
        drawing.saveas(file)

    def _write(self):
        # init Drawing ---------------------
        d=DXF.Drawing()
        # add Tables -----------------
        # initialized automatic: d.blocks.append(b)                 #section BLOCKS
        # initialized automatic: d.styles.append(DXF.Style())            #table STYLE

        #table LTYPE ---------------
        #d.linetypes.append(DXF.LineType(name='CONTINUOUS',description='--------',elements=[0.0]))
        d.linetypes.append(DXF.LineType(name='DOT',description='. . . . . . .',elements=[0.25, 0.0, -0.25]))
        d.linetypes.append(DXF.LineType(name='DASHED',description='__ __ __ __ __',elements=[0.8, 0.5, -0.3]))
        d.linetypes.append(DXF.LineType(name='DASHDOT',description='__ . __ . __ .',elements=[1.0, 0.5, -0.25, 0.0, -0.25]))
        d.linetypes.append(DXF.LineType(name='DIVIDE',description='____ . . ____ . . ',elements=[1.25, 0.5, -0.25, 0.0, -0.25, 0.0, -0.25]))
        d.linetypes.append(DXF.LineType(name='BORDER',description='__ __ . __ __ . ',elements=[1.75, 0.5, -0.25, 0.5, -0.25, 0.0, -0.25]))
        d.linetypes.append(DXF.LineType(name='HIDDEN',description='__ __ __ __ __',elements=[0.4, 0.25, -0.25]))
        d.linetypes.append(DXF.LineType(name='CENTER',description='____ _ ____ _ __',elements=[2.0, 1.25, -0.25, 0.25, -0.25]))

        #d.vports.append(DXF.VPort('*ACTIVE'))
        d.vports.append(DXF.VPort('*ACTIVE',center=(-5.0,1.0),height=10.0))
        #d.vports.append(DXF.VPort('*ACTIVE',leftBottom=(-100.0,-60.0),rightTop=(100.0,60.0)))
        #d.views.append(DXF.View('Normal'))      #table view
        d.views.append(DXF.ViewByWindow('BF_TOPVIEW',leftBottom=(-100,-60),rightTop=(100,60)))  #idem

        return d

    def _processEntityArray(self, drawing, type, ents):
        if type=='Point':
            for e in ents:
                drawing.append(DXF.Point(**e))
        elif type=='Line':
            for e in ents:
                drawing.append(DXF.Line(**e))
        elif type=='PolyLine':
            for e in ents:
                drawing.append(DXF.PolyLine(**e))
        elif type=='Face':
            for e in ents:
                drawing.append(DXF.Face(**e))

