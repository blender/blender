

def sv_main(factor=[]):

    in_sockets = [
        ['s', 'factor', factor]]

    if not factor:
        factor = [1]

    objs = bpy.data.objects
    for obj, f in zip(objs,factor):
        if 'Alpha' in obj.name and obj.data.shape_keys:
            obj.shape_key_add(name=obj.name, from_mix=True)
            obj.shape_key_add(name=obj.name+'k', from_mix=True)
        else:
            obj.data.shape_keys[obj.name].key_blocks['Key 1'].value = f

    return in_sockets, []

