"""
in   in_data    v  d=[]  n=1
in   in_colors  v  d=[]  n=1
out  float_out  s
"""

import bgl
import bpy


from sverchok.data_structure import node_id
from sverchok.ui import bgl_callback_3dview as v3dBGL

self.n_id = node_id(self)
v3dBGL.callback_disable(self.n_id)


def screen_v3dBGL(context, args):
    region = context.region
    region3d = context.space_data.region_3d
    
    points = args[0]
    colors = args[1]
    size= 5.0
    
    bgl.glEnable(bgl.GL_POINT_SMOOTH) # for round vertex
    bgl.glPointSize(size)
    bgl.glBlendFunc(bgl.GL_SRC_ALPHA, bgl.GL_ONE_MINUS_SRC_ALPHA)
    
    if colors:
        bgl.glBegin(bgl.GL_POINTS)
        for coord, color in zip(points, colors):
            bgl.glColor4f(*color)    
            bgl.glVertex3f(*coord)
        bgl.glEnd()

    else:
        gl_col = (0.9, 0.9, 0.8, 1.0)
        bgl.glColor4f(*gl_col)    
        bgl.glBegin(bgl.GL_POINTS)
        for coord in points:
            bgl.glVertex3f(*coord)
        bgl.glEnd()        
    
    bgl.glDisable(bgl.GL_POINT_SMOOTH)
    bgl.glDisable(bgl.GL_POINTS)
        
    

if self.inputs['in_data'].links:

    draw_data = {
        'tree_name': self.id_data.name[:],
        'custom_function': screen_v3dBGL,
        'args': (in_data, in_colors)
    }

    v3dBGL.callback_enable(self.n_id, draw_data, overlay='POST_VIEW')
