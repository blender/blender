from mathutils import noise


def get_offset(seed):
    if seed == 0:
        offset = [0.0, 0.0, 0.0]
    else:
        noise.seed_set(seed)
        offset = noise.random_unit_vector() * 10.0
    return offset


def seed_adjusted(vert_list, seed):
    if seed == 0.0:
        return vert_list

    ox = get_offset(seed)
    return [[v[0] + ox[0], v[1] + ox[1], v[2] + ox[2]] for v in vert_list]
