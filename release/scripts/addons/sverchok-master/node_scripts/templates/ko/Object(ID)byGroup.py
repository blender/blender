def sv_main(Gname=[]):
 
    
    in_sockets = [
        ['s', 'GroupName',  Gname]
    ]
    
    
    import mathutils 
    from mathutils import Vector
    
    out=[]
    
    
    if Gname:
        for i in bpy.data.groups.get(Gname[0][0][0]).objects:
            out.append(i)
            
    
    out_sockets = [
        ['v', 'Object(ID)', [out] ]
    ]
 
    return in_sockets, out_sockets