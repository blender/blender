# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy

EPS = 0.001
EPS_LINE_LINE = 0.02
EPS_COLLAPSE = 0.05
EPS_HUB = 0.002

def get_hub(co, _hubs):
    
    if 1:
        for hub in _hubs.values():
            if (hub.co - co).length < EPS_HUB:
                return hub
        
        key = co.toTuple(3)
        hub = _hubs[key] = Hub(co, key, len(_hubs))
        return hub
    else:
        pass
        
        '''
        key = co.toTuple(3)
        try:
            return _hubs[key]
        except:
            hub = _hubs[key] = Hub(co, key, len(_hubs))
            return hub
        '''
        

class Hub:
    def __init__(self, co, key, index):
        self.co = co.copy()
        self.key = key
        self.index = index
        self.links = []

    def get_weight(self):
        f = 0.0
        
        for hub_other in self.links:
            f += (self.co - hub_other.co).length
    
    def replace(self, other):
        for hub in self.links:
            try:
                hub.links.remove(self)
            except:
                pass
            if other not in hub.links:
                hub.links.append(other)
            
    
    def dist(self, other):
        return (self.co - other.co).length

    def calc_faces(self, hub_ls):
        faces = []
        # first tris
        for l_a in self.links:
            for l_b in l_a.links:
                if l_b is not self and l_b in self.links:
                    # will give duplicates
                    faces.append((self.index, l_a.index, l_b.index))
        
        # now quads, check which links share 2 different verts
        # directly
        def validate_quad(face):
            if len(set(face)) != len(face):
                return False
            if hub_ls[face[0]] in hub_ls[face[2]].links:
                return False
            if hub_ls[face[2]] in hub_ls[face[0]].links:
                return False
                
            if hub_ls[face[1]] in hub_ls[face[3]].links:
                return False
            if hub_ls[face[3]] in hub_ls[face[1]].links:
                return False
            
            return True

        for i, l_a in enumerate(self.links):
            links_a = set([l.index for l in l_a.links])
            for j in range(i):
                l_b = self.links[j]
                
                links_b = set([l.index for l in l_b.links])
                
                isect = links_a.intersection(links_b)
                if len(isect) == 2:
                    isect = list(isect)
                    
                    # check there are no diagonal lines
                    face = (isect[0], l_a.index, isect[1], l_b.index)
                    if validate_quad(face):
                    
                        faces.append(face)
        
        return faces
    


class Spline:
    def __init__(self, points):
        self.points = points
        self.hubs = []

    def link(self):
        if len(self.hubs) < 2:
            return
        
        edges = list(set([i for i, hub in self.hubs]))
        edges.sort()

        edges_order = {}
        for i in edges:
            edges_order[i] = []
        
        
        # self.hubs.sort()
        for i, hub in self.hubs:
            edges_order[i].append(hub)
        
        hubs_order = []
        for i in edges:
            ls = edges_order[i]
            edge_start = self.points[i]
            ls.sort(key=lambda hub: (hub.co - edge_start).length)
            hubs_order.extend(ls)
        
        # Now we have the order, connect the hubs
        hub_prev = hubs_order[0]
        
        for hub in hubs_order[1:]:
            hub.links.append(hub_prev)
            hub_prev.links.append(hub)
            hub_prev = hub

def get_points(stroke):
    return [point.co.copy() for point in stroke.points]

def get_splines(gp):
    for l in gp.layers:
        if l.active: # XXX - should be layers.active
            break
    
    frame = l.active_frame
    
    return [Spline(get_points(stroke)) for stroke in frame.strokes]

def xsect_spline(sp_a, sp_b, _hubs):
    from Mathutils import LineIntersect
    from Mathutils import MidpointVecs
    from Geometry import ClosestPointOnLine
    pt_a_prev = pt_b_prev = None
    
    pt_a_prev = sp_a.points[0]
    for a, pt_a in enumerate(sp_a.points[1:]):
        pt_b_prev = sp_b.points[0]
        for b, pt_b in enumerate(sp_b.points[1:]):

            # Now we have 2 edges
            # print(pt_a, pt_a_prev, pt_b, pt_b_prev)
            xsect = LineIntersect(pt_a, pt_a_prev, pt_b, pt_b_prev)
            if xsect is not None:
                if (xsect[0]-xsect[1]).length <= EPS_LINE_LINE:
                    f = ClosestPointOnLine(xsect[1], pt_a, pt_a_prev)[1]
                    if f >= 0.0 and f <= 1.0:
                        f = ClosestPointOnLine(xsect[0], pt_b, pt_b_prev)[1]
                        if f >= 0.0 and f <= 1.0:
                            # This wont happen often
                            co = MidpointVecs(xsect[0], xsect[1])
                            hub = get_hub(co, _hubs)

                            sp_a.hubs.append((a, hub))
                            sp_b.hubs.append((b, hub))

            pt_b_prev = pt_b
            
        pt_a_prev = pt_a


def calculate(gp):
    splines = get_splines(gp)
    _hubs = {}
    
    for i, sp in enumerate(splines):
        for j, sp_other in enumerate(splines):
            if j<=i:
                continue

            xsect_spline(sp, sp_other, _hubs)
            
    for sp in splines:
        sp.link()
    
    # remove these
    hubs_ls = [hub for hub in _hubs.values() if hub.index != -1]
        
    _hubs.clear()
    _hubs = None
    
    for i, hub in enumerate(hubs_ls):
        hub.index = i
    
    # Now we have connected hubs, write all edges!
    def order(i1, i2):
        if i1 > i2:
            return i2, i1
        return i1, i2
    
    edges = {}
    
    for hub in hubs_ls:
        i1 = hub.index
        for hub_other in hub.links: 
            i2 = hub_other.index
            edges[order(i1, i2)] = None
    
    verts = []
    edges = edges.keys()
    faces = []
    
    for hub in hubs_ls:
        verts.append(hub.co)
        faces.extend(hub.calc_faces(hubs_ls))
    
    # remove double faces
    faces = dict([(tuple(sorted(f)), f) for f in faces]).values()
        
    mesh = bpy.data.add_mesh("Retopo")
    mesh.from_pydata(verts, [], faces)
    
    scene = bpy.context.scene
    mesh.update()
    obj_new = bpy.data.add_object('MESH', "Torus")
    obj_new.data = mesh
    scene.objects.link(obj_new)
    
    return obj_new
    
    
def main():
    scene = bpy.context.scene
    obj = bpy.context.object
    
    gp = None
    
    if obj:
        gp = obj.grease_pencil
    
    if not gp:
        gp = scene.grease_pencil

    if not gp:
        raise Exception("no active grease pencil")
    
    obj_new = calculate(gp)
    
    scene.objects.active = obj_new
    obj_new.selected = True
    
    # nasty, recalc normals
    bpy.ops.object.mode_set(mode='EDIT', toggle=False)
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT', toggle=False)
