def sv_main(name=[], ind=[]):

    in_sockets = [
        ['v', ' "name" of object ', name],  # formula "string"
        ['s', ' index of vertex ',   ind]]  # Can also take list of polygons from ObjectIN node
    
    if name and ind:
        obj= bpy.data.objects[name[0][0][0]]
        objv= obj.data.vertices
        
        bpy.context.scene.objects.active= obj
        obj.data.update()
        bpy.ops.object.mode_set(mode='EDIT')
        
        bpy.ops.mesh.select_all(action='DESELECT')

        bpy.ops.object.mode_set(mode='OBJECT')
        for i in ind[0]:
            for i2 in i:
                objv[i2].select= 1
        bpy.ops.object.mode_set(mode='EDIT')
        
    out_sockets = [
        ['v', '', [] ]
    ]
    
    return in_sockets, out_sockets
