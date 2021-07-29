
import mathutils
from .base_exporter import BasePrimitiveDXFExporter
import copy

class MeshDXFExporter(BasePrimitiveDXFExporter):

    def export(self, ctx, drawing, ob, mx, mx_n, **kwargs):
        """
        Converts Mesh-Object to desired projection and representation(DXF-Entity type)
        """
        me = self._getMeshData(ctx, ob, self._settings)
        # idea: me.transform(mx); get verts data; me.transform(mx_inv)= back to the origin state
        # the .transform-method is fast, but bad, cause invasive:
        # it manipulates original geometry and by retransformation lefts back rounding-errors
        # we dont want to manipulate original data!
        #temp_verts = me.verts[:] #doesn't work on ubuntu(Yorik), bug?
        if me.vertices:
            # check if there are more instances of this mesh (if used by other objects), then write to BLOCK/INSERT
            if self.INSTANCES and me.users>1 and not self.PROJECTION and not (ob.modifiers and self._settings['apply_modifiers']):
                if drawing.containsBlock(me.name):
                    entities = self._writeInsert(drawing, ob, mx, me.name)
                else:
                    # generate geom_output in ObjectCS
                    allpoints = [v.co for v in me.vertices]
                    identity_matrix = mathutils.Matrix().identity()
                    allpoints = self.projected_co(allpoints, identity_matrix)
                    #allpoints = toGlobalOrigin(allpoints)
                    faces=[]
                    edges=[]
                    for e in me.edges: edges.append(e.key)
                    faces = [[v.index for v in f.verts] for f in me.faces]
                    entities = self._writeMeshEntities(allpoints, edges, faces, **kwargs)
                    if entities: # if not empty block
                        # write BLOCK definition and INSERT entity
                        # BLOCKREGISTRY = dictionary 'blender_name':'dxf_name'.append(me.name)
#                        BLOCKREGISTRY[me.name]=self.validDXFr12name(('ME_'+ me.name))
#                        insert_name = BLOCKREGISTRY[me.name]
                        drawing.addBlock(me.name, flag=0,base=(0,0,0),entities=entities)
#                        block = DXF.Block(insert_name,flag=0,base=(0,0,0),entities=entities)
                        # write INSERT as entity
                        entities = self._writeInsert(ob, mx, me.name, **(kwargs))

            else: # no other instances, so go the standard way
                return self._standard_way(drawing, me, mx, mx_n)

    def _writeInsert(self, drawing, ob, mx, insert_name, **kwargs):
        from insert_exporter import InsertDXFExporter
        ex = InsertDXFExporter(self._settings)
        ex.export(drawing, ob, mx, insert_name, **(kwargs))

    def _getMeshData(self, ctx, obj, settings):
        if obj.modifiers and settings['apply_modifiers']:
            #this gets mesh with applied modifiers
            data = obj.to_mesh(ctx.scene, True, 'PREVIEW')
        else:
    #        me = ob.getData(mesh=1) # is a Mesh if mesh>0 (otherwise it is a NMesh)
            data = obj.data
        return data

    def _standard_way(self, drawing, me, mx, mx_n, **kwargs):
        allpoints = [v.co for v in me.vertices]
        allpoints = self.projected_co(allpoints, mx)
        allpoints = self.toGlobalOrigin(allpoints)
        faces=[]
        edges=[]
        me.update(calc_tessface=True)
        me_faces = me.tessfaces
        #print('deb: allpoints=\n', allpoints) #---------
        #print('deb: me_faces=\n', me_faces) #---------
        if me_faces and self.PROJECTION and self.HIDDEN_LINES:
            #if DEBUG: print 'deb:exportMesh HIDDEN_LINES mode' #---------
            faces, edges = self.hidden_status(me_faces, mx, mx_n)
            faces = [[v.index for v in me_faces[f_nr].vertices] for f_nr in faces]
        else:
            #if DEBUG: print 'deb:exportMesh STANDARD mode' #---------
            for e in me.edges: edges.append(e.key)
            #faces = [f.index for f in me.faces]
            ##faces = [[v.index for v in f.vertices] for f in me.faces]
            faces = me_faces
            #faces = [[allpoints[v.index] for v in f.vertices] for f in me.faces]
        #print('deb: allpoints=\n', allpoints) #---------
        #print('deb: edges=\n', edges) #---------
        #print('deb: faces=\n', faces) #---------
        if self.isLeftHand(mx): # then change vertex-order in every face
            for f in faces:
                f.reverse()
                #f = [f[-1]] + f[:-1] #TODO: might be needed
            #print('deb: faces=\n', faces) #---------
        entities = self._writeMeshEntities(allpoints, edges, faces, **kwargs)
        # TODO: rewrite
        for type, args in entities:
            drawing.addEntity(type, **(args))
        return True

    def _writeMeshEntities(self, allpoints, edges, faces, **kwargs):
        """help routine for exportMesh()
        """
        entities = []
        c = self._settings['mesh_as']
        if c=='POINTs': # export Mesh as multiple POINTs
            for p in allpoints:
                args = copy.copy(kwargs)
                args['points'] = [p]
                entities.append(('Point', args))
        elif c=='LINEs' or (not faces):
            if edges and allpoints:
#                if exportsettings['verbose']:
#                    mesh_drawBlender(allpoints, edges, None) #deb: draw to blender scene
                for e in edges:
                    points = [allpoints[e[0]], allpoints[e[1]]]
                    args = copy.copy(kwargs)
                    args['points'] = points
                    entities.append(('Line', args))
        elif c in {'POLYFACE', 'POLYLINE'}:
            if faces and allpoints:
                #TODO: purge allpoints: left only vertices used by faces
#                    if exportsettings['verbose']:
#                        mesh_drawBlender(allpoints, None, faces) #deb: draw to scene
                if not (self.PROJECTION and self.HIDDEN_LINES):
                    faces = [[v+1 for v in f.vertices] for f in faces]
                else:
                    # for back-Faces-mode remove face-free vertices
                    map=verts_state= [0]*len(allpoints)
                    for f in faces:
                        for v in f:
                            verts_state[v]=1
                    if 0 in verts_state: # if dirty state
                        i,newverts=0,[]
                        for used_i,used in enumerate(verts_state):
                            if used:
                                newverts.append(allpoints[used_i])
                                map[used_i]=i
                                i+=1
                        allpoints = newverts
                        faces = [[map[v]+1 for v in f] for f in faces]
                args = copy.copy(kwargs)
                args['flag70'] = 64
                args['flag75'] = 0
                args['width'] = 0.0
                args['points'] = [allpoints, faces]
                entities.append(('PolyLine', args))
        elif c=='3DFACEs':
            if faces and allpoints:
#                if exportsettings['verbose']:
#                    mesh_drawBlender(allpoints, None, faces) #deb: draw to scene
                for f in faces:
                    points = [allpoints[v_id] for v_id in f.vertices]
                    args = copy.copy(kwargs)
                    args['points'] = points
#                    print(args)
                    entities.append(('Face', args))

        return entities
