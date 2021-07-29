from mathutils import Vector
import math
from math import cos, sin, pi,radians

class LineScript(SvScriptSimpleGenerator):
    
    @staticmethod
    def make_line(integer, step, phi, theta, start_point ):
        vertices = [start_point] 
        r = step
        r_t = radians(theta)
        r_p = radians(phi)
        sin_theta = sin(r_t)
        cos_theta = cos(r_t)
        sin_phi = sin(r_p)
        cos_phi = cos(r_p)
        for i in range(integer-1):
            v = Vector(vertices[i]) + r * Vector((sin_theta*cos_phi,sin_theta*sin_phi,cos_theta))
            vertices.append(v[:])
        return vertices
   
    @staticmethod
    def make_edges(*param):
        integer = param[0]
        edges = []
        for i in range(integer-1):
            edges.append((i, i+1))
        return  edges
    
    inputs = [('s', "N Verts",2),
              ('s', "Step", 1.0),
              ('s', "phi", 180),
              ('s', "theta", 0),
              ('v', "start")]
    
    outputs = [("v", "Verts", "make_line"),
                ("s", "Edges", "make_edges")]
