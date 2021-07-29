import numpy as np

def sv_main(n_items=1500):

    in_sockets = [
        ['s', 'n_items', n_items]]

    np.random.seed(2)
    points=np.random.uniform(0.0,1.0,size = (n_items,2))
    points *= (2, 10)

    a = points[:,0]
    b = points[:,1]
    c = np.array([0.0 for i in b])
    d = np.column_stack((a,b,c))

    # consumables
    Verts = d.tolist()

    def func1():
        if 'Cube_Object' not in bpy.data.objects:
            mesh_data = bpy.data.meshes.new("cube_mesh_data")
            mesh_data.from_pydata(Verts, [], [])
            mesh_data.update() # (calc_edges=True) not needed here

            cube_object = bpy.data.objects.new("Cube_Object", mesh_data)

            scene = bpy.context.scene  
            scene.objects.link(cube_object)  
            cube_object.select = True      

    out_sockets = [
        ['v', 'Verts', [Verts]]
    ]

    ui_operators = [
        ['button1', func1]
    ]    

    return in_sockets, out_sockets, ui_operators