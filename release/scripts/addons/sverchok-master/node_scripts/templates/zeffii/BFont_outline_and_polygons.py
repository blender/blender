def sv_main(loc=[[]], mode=0):

    ''' takes vectors in, if using list input, must list split
    place outline of A A1 A2 A3 at those location
    '''

    in_sockets = [
        ['v', 'loc', loc],
        ['s', 'mode', mode]
    ]
        
    out_sockets = [
        ['v', 'verts', []],
        ['s', 'edges', []],
        ['s', 'faces', []]
    ]
    
    # alias for convenience and speed
    objects = bpy.data.objects
    curves = bpy.data.curves
    scene = bpy.context.scene
    meshes = bpy.data.meshes

    base_body = "A"
    curve_name = "SN_" + base_body
    
    # reuse the same curve.
    if curve_name not in curves:
        f = curves.new(curve_name, 'FONT')
    else:
        f = curves[curve_name]
    
    verts_out, edges_out, faces_out = [], [], []
    
    items = len(loc[0])
    for c in range(items):

        body = base_body + str(c) if c > 0 else base_body
        f.body = body
    
        if mode == 0:
            f.fill_mode = 'NONE'
        else:
            f.fill_mode = 'FRONT'

        # reuse the same donor object, hide it.
        temp_object_name = curve_name
        if temp_object_name in objects:
            obj = objects[temp_object_name]
            obj.data = f
        else:
            obj = objects.new(temp_object_name, f)
            scene.objects.link(obj)
            
        obj.hide = True
        
        mesh_settings = (bpy.context.scene, False, 'PREVIEW')
        data = obj.to_mesh(*mesh_settings)

        if data:

            # collect verts
            verts = data.vertices
            v = [0 for i in range(len(verts)*3)]
            verts.foreach_get('co', v)
            v_packed = [(v[i], v[i+1], v[i+2]) for i in range(0, len(v),3)]
            
            # collect edges
            e_packed = data.edge_keys
            
            verts_out.append(v_packed)
            edges_out.append(e_packed)
            
            if mode == 1:
                f_packed = [fx.vertices[:] for fx in data.polygons]
                # print(f_packed)
                faces_out.append(f_packed)
                
        meshes.remove(data)

    out_sockets[0][2] = [verts_out]
    out_sockets[1][2] = [edges_out]
    if mode == 1:
        out_sockets[2][2] = [faces_out]
    
    return in_sockets, out_sockets
