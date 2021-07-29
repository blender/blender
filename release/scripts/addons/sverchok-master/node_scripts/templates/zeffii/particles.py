def sv_main(obj_id=0, particle_sys=0):

    in_sockets = [
        ['s', 'obj_id', obj_id],
        ['s', 'particle_sys', particle_sys]]

    out_sockets = [
        ['v', 'locations', []]
    ]

    objects = bpy.data.objects
    if not obj_id < len(objects):
        return in_sockets, out_sockets       

    obj = objects[obj_id]
    if not particle_sys < len(obj.particle_systems):
        out_sockets[0][2] = []
        return in_sockets, out_sockets

    ps = obj.particle_systems[particle_sys]
    particles = ps.particles

    # on large objects list comprehension seems to be slower than index
    # and . dotted access is also a slowing factor
    locs = []
    add_loc = locs.append

    for i in range(len(particles)):
        # alive_state
        # angular_velocity
        # birth_time
        # die_time
        # hair_keys
        # is_exist
        # is_visible
        # lifetime
        pt = particles[i]
        if pt.is_exist and pt.alive_state == 'ALIVE':
            add_loc(pt.location[:])
        # prev_angular_velocity
        # prev_location
        # prev_rotation
        # prev_velocity
        # rotation
        # size
        # velocity
        #

    out_sockets[0][2] = [locs]

    return in_sockets, out_sockets