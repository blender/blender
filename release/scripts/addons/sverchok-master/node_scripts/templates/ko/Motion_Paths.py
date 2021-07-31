def sv_main(ObjectID=[]):

    in_sockets = [
        ['v', 'ObjectID', ObjectID]
        
        ]
    
    
    
    out=[]
    
    if ObjectID:
        for i in ObjectID[0][0]:
            for i2 in i.motion_path.points:
                out.append(i2.co[:])
        
    
    
    
    out_sockets = [
        ['v', 'Points', [out] ]
        
    ]
    
    return in_sockets, out_sockets