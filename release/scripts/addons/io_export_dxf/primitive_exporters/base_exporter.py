import mathutils

class BasePrimitiveDXFExporter(object):

    INSTANCES = False
    PROJECTION = False
    HIDDEN_LINES = False

    def __init__(self, settings):
        self._settings = settings

    def projected_co(self, verts, matrix):
        """ converts coordinates of points from OCS to WCS->ScreenCS
        needs matrix: a projection matrix
        needs verts: a list of vectors[x,y,z]
        returns a list of [x,y,z]
        """
        #print 'deb:projected_co()  verts=', verts #---------
        temp_verts = [matrix*mathutils.Vector(v) for v in verts]
        #print 'deb:projected_co()  temp_verts=', temp_verts #---------

    #    if GUI_A['Z_force_on'].val: locZ = GUI_A['Z_elev'].val
    #    else:locZ = 0.0
        locZ = 0.0

        if self.PROJECTION:
            if self.PERSPECTIVE:
                clipStart = 10.0
                for v in temp_verts:
                    coef = - clipStart / v[2]
                    v[0] *= coef
                    v[1] *= coef
                    v[2] = locZ
            for v in temp_verts:
                v[2] = locZ
        temp_verts = [v[:3] for v in temp_verts]
        #print 'deb:projected_co()  out_verts=', temp_verts #---------
        return temp_verts

    def isLeftHand(self, matrix):
        #Is the matrix a left-hand-system, or not?
        ma = matrix.to_euler().to_matrix()
        crossXY = self.M_CrossVecs(ma[0], ma[1])
        check = self.M_DotVecs(ma[2], crossXY)
        if check < 0.00001: return 1
        return 0

    #-----------------------------------------------------
    def hidden_status(self, faces, mx, mx_n):
        # sort out back-faces = with normals pointed away from camera
        #print 'HIDDEN_LINES: caution! not full implemented yet'
        front_faces = []
        front_edges = []
        for f in faces:
            #print 'deb: face=', f #---------
            #print 'deb: dir(face)=', dir(f) #---------
            # get its normal-vector in localCS
            vec_normal = f.no.copy()
            #print 'deb: vec_normal=', vec_normal #------------------
            # must be transfered to camera/view-CS
            vec_normal *= mx_n
            #vec_normal *= mb.rotationPart()
            #print 'deb:2vec_normal=', vec_normal #------------------
            #vec_normal *= mw0.rotationPart()
            #print 'deb:3vec_normal=', vec_normal, '\n' #------------------


            frontFace = False
            if not self.PERSPECTIVE: #for ortho mode ----------
                # normal must point the Z direction-hemisphere
                if vec_normal[2] > 0.00001:
                    frontFace = True
            else:
                v = f.verts[0]
                vert = mathutils.Vector(v.co) * mx
                if mathutils.DotVecs(vert, vec_normal) < 0.00001:
                    frontFace = True

            if frontFace:
                front_faces.append(f.index)
                for key in f.edge_keys:
                    #this test can be done faster with set()
                    if key not in front_edges:
                        front_edges.append(key)

        #print 'deb: amount of visible faces=', len(front_faces) #---------
        #print 'deb: visible faces=', front_faces #---------
        #print 'deb: amount of visible edges=', len(front_edges) #---------
        #print 'deb: visible edges=', front_edges #---------
        return front_faces, front_edges

    #-----------------------------------------------------
    def toGlobalOrigin(self, points):
        """relocates points to the new location
        needs a list of points [x,y,z]
        """
    #    if GUI_A['g_origin_on'].val:
    #        for p in points:
    #            p[0] += G_ORIGIN[0]
    #            p[1] += G_ORIGIN[1]
    #            p[2] += G_ORIGIN[2]
        return points

    #---- migration to 2.49-------------------------------------------------

    #Draw.PupMenu('DXF exporter: Abort%t|This script version works for Blender up 2.49 only!')
    def M_CrossVecs(self, v1,v2):
        if 'cross' in dir(mathutils.Vector()):
            return v1.cross(v2) #for up2.49
        else:
            return mathutils.CrossVecs(v1,v2) #for pre2.49

    def M_DotVecs(self, v1,v2):
        if 'cross' in dir(mathutils.Vector()):
            return v1.dot(v2) #for up2.49
        else:
            return mathutils.DotVecs(v1,v2) #for pre2.49

#-----------------------------------------------------
    def getExtrusion(self, matrix):
        """ calculates DXF-Extrusion = Arbitrary Xaxis and Zaxis vectors """
        AZaxis = matrix[2].copy().resize3D().normalize() # = ArbitraryZvector
        Extrusion = [AZaxis[0],AZaxis[1],AZaxis[2]]
        if AZaxis[2]==1.0:
            Extrusion = None
            AXaxis = matrix[0].copy().resize3D() # = ArbitraryXvector
        else:
            threshold = 1.0 / 64.0
            if abs(AZaxis[0]) < threshold and abs(AZaxis[1]) < threshold:
                # AXaxis is the intersection WorldPlane and ExtrusionPlane
                AXaxis = self.M_CrossVecs(WORLDY,AZaxis)
            else:
                AXaxis = self.M_CrossVecs(WORLDZ,AZaxis)
        #print 'deb:\n' #-------------
        #print 'deb:getExtrusion()  Extrusion=', Extrusion #---------
        return Extrusion, AXaxis.normalize()

    #-----------------------------------------------------
#    def getZRotation(AXaxis, rot_matrix_invert):
#        """calculates ZRotation = angle between ArbitraryXvector and obj.matrix.Xaxis
#
#        """
#        # this works: Xaxis is the obj.matrix-Xaxis vector
#        # but not correct for all orientations
#        #Xaxis = matrix[0].copy().resize3D() # = ArbitraryXvector
#        ##Xaxis.normalize() # = ArbitraryXvector
#        #ZRotation = - mathutils.AngleBetweenVecs(Xaxis,AXaxis) #output in radians
#
#        # this works for all orientations, maybe a bit faster
#        # transform AXaxis into OCS:Object-Coord-System
#        #rot_matrix = normalizeMat(matrix.rotationPart())
#        #rot_matrix_invert = rot_matrix.invert()
#        vec = AXaxis * rot_matrix_invert
#        ##vec = AXaxis * matrix.copy().invert()
#        ##vec.normalize() # not needed for atan2()
#        #print '\ndeb:getExtrusion()  vec=', vec #---------
#        ZRotation = - atan2(vec[1],vec[0]) #output in radians
#
#        #print 'deb:ZRotation()  ZRotation=', ZRotation*r2d #---------
#        return ZRotation
#
#
#    #-----------------------------------------------------
#    def getTargetOrientation(mx,Extrusion,AXaxis,WCS_loc,sizeX,sizeY,sizeZ,rotX,rotY,rotZ):
#        """given
#        """
#        if 1:
#            rot_matrix = normalizeMat(mx.to_euler().to_matrix())
#            #TODO: workaround for blender negative-matrix.invert()
#            # partially done: works only for rotX,rotY==0.0
#            if sizeX<0.0: rot_matrix[0] *= -1
#            if sizeY<0.0: rot_matrix[1] *= -1
#            #if sizeZ<0.0: rot_matrix[2] *= -1
#            rot_matrix_invert = rot_matrix.invert()
#        else: #TODO: to check, why below rot_matrix_invert is not equal above one
#            rot_euler_matrix = euler2matrix(rotX,rotY,rotZ)
#            rot_matrix_invert = euler2matrix(-rotX,-rotY,-rotZ)
#
#        # OCS_origin is Global_Origin in ObjectCoordSystem
#        OCS_origin = mathutils.Vector(WCS_loc) * rot_matrix_invert
#        #print 'deb: OCS_origin=', OCS_origin #---------
#
#        ZRotation = rotZ
#        if Extrusion is not None:
#            ZRotation = getZRotation(AXaxis,rot_matrix_invert)
#        #Zrotmatrix = mathutils.RotationMatrix(-ZRotation, 3, "Z")
#        rs, rc = sin(ZRotation), cos(ZRotation)
#        Zrotmatrix = mathutils.Matrix([rc, rs,0.0],[-rs,rc,0.0],[0.0,0.0,1.0])
#        #print 'deb: Zrotmatrix=\n', Zrotmatrix #--------------
#
#        # ECS_origin is Global_Origin in EntityCoordSystem
#        ECS_origin = OCS_origin * Zrotmatrix
#        #print 'deb: ECS_origin=', ECS_origin #---------
#        #TODO: it doesnt work yet for negative scaled curve-objects!
#        return ZRotation,Zrotmatrix,OCS_origin,ECS_origin
