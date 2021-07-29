def live_curve(out):
    curves = bpy.data.curves
    objects = bpy.data.objects
    scene = bpy.context.scene
 
    curve_name = 'tie_binding'
        
    # if exists, pick up
    cu = curves.get(curve_name, curves.new(name=curve_name, type='CURVE'))
    cu.dimensions = '3D'
    cu.fill_mode = 'FULL'
    cu.bevel_depth = 0.01
    tie = cu.splines.new('POLY')
    obj = objects.get(curve_name, objects.new(curve_name, cu))
     
    # flattens the list for foreach_set
    out2 = []
    [out2.extend(list(i)+[0.0]) for i in out] 
    
    # break down and rebuild
    if cu.splines:
        num_splines = len(cu.splines)
        for i in range(num_splines):
            cu.splines.remove(cu.splines[-1])

    tie = cu.splines.new('POLY')
    tie.points.add(len(out)-1)
    tie.points.foreach_set('co', out2)
    cu.extrude = 0.4
 
    if not curve_name in scene.objects:
        scene.objects.link(obj)    

def sv_main(verts=[[]]):
    masked_items_out = []

    in_sockets = [
        ['v', 'smoothed', verts]
    ]

    out_sockets = [
        ['s', 'mask', masked_items_out]
    ]
    
    if verts and verts[0]:
        live_curve(verts[0])

    return in_sockets, out_sockets

