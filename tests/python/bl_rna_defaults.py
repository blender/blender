# Apache License, Version 2.0

# ./blender.bin --background -noaudio --factory-startup --python tests/python/bl_rna_defaults.py

import bpy

DUMMY_NAME = "Untitled"
DUMMY_PATH = __file__
GLOBALS = {
    "error_num": 0,
}


def as_float_32(f):
    from struct import pack, unpack
    return unpack("f", pack("f", f))[0]


def repr_float_precision(f, round_fn):
    """
    Get's the value which was most likely entered by a human in C.

    Needed since Python will show trailing precision from a 32bit float.
    """
    f_round = round_fn(f)
    f_str = repr(f)
    f_str_frac = f_str.partition(".")[2]
    if not f_str_frac:
        return f_str
    for i in range(1, len(f_str_frac)):
        f_test = round(f, i)
        f_test_round = round_fn(f_test)
        if f_test_round == f_round:
            return "%.*f" % (i, f_test)
    return f_str


def repr_float_32(f):
    return repr_float_precision(f, as_float_32)


def validate_defaults(test_id, o):

    def warning(prop_id, val_real, val_default, *, repr_fn=repr):
        print("Error %s: '%s.%s' is:%s, expected:%s" %
              (test_id, o.__class__.__name__, prop_id,
               repr_fn(val_real), repr_fn(val_default)))
        GLOBALS["error_num"] += 1

    properties = type(o).bl_rna.properties.items()
    for prop_id, prop in properties:
        if prop_id == "rna_type":
            continue
        prop_type = prop.type
        if prop_type in {'STRING', 'COLLECTION'}:
            continue

        if prop_type == 'POINTER':
            # traverse down pointers if they're set
            val_real = getattr(o, prop_id)
            if (val_real is not None) and (not isinstance(val_real, bpy.types.ID)):
                validate_defaults("%s.%s" % (test_id, prop_id), val_real)
        elif prop_type in {'INT', 'BOOL'}:
            # array_length = prop.array_length
            if not prop.is_array:
                val_real = getattr(o, prop_id)
                val_default = prop.default
                if val_real != val_default:
                    warning(prop_id, val_real, val_default)
            else:
                pass  # TODO, array defaults
        elif prop_type == 'FLOAT':
            # array_length = prop.array_length
            if not prop.is_array:
                val_real = getattr(o, prop_id)
                val_default = prop.default
                if val_real != val_default:
                    warning(prop_id, val_real, val_default, repr_fn=repr_float_32)
            else:
                pass  # TODO, array defaults
        elif prop_type == 'ENUM':
            val_real = getattr(o, prop_id)
            if prop.is_enum_flag:
                val_default = prop.default_flag
            else:
                val_default = prop.default
            if val_real != val_default:
                warning(prop_id, val_real, val_default)

        # print(prop_id, prop_type)


def _test_id_gen(data_attr, args_create=(DUMMY_NAME,), create_method="new"):
    def test_gen(test_id):
        id_collection = getattr(bpy.data, data_attr)
        create_fn = getattr(id_collection, create_method)
        o = create_fn(*args_create)
        o.user_clear()
        validate_defaults(test_id, o)
        id_collection.remove(o)
    return test_gen


test_Action = _test_id_gen("actions")
test_Armature = _test_id_gen("armatures")
test_Camera = _test_id_gen("cameras")
test_Group = _test_id_gen("groups")
test_Lattice = _test_id_gen("lattices")
test_LineStyle = _test_id_gen("linestyles")
test_Mask = _test_id_gen("masks")
test_Material = _test_id_gen("materials")
test_Mesh = _test_id_gen("meshes")
test_MetaBall = _test_id_gen("metaballs")
test_MovieClip = _test_id_gen("movieclips", args_create=(DUMMY_PATH,), create_method="load")
test_Object = _test_id_gen("objects", args_create=(DUMMY_NAME, None))
test_Palette = _test_id_gen("palettes")
test_Particle = _test_id_gen("particles")
test_Scene = _test_id_gen("scenes")
test_Sound = _test_id_gen("sounds", args_create=(DUMMY_PATH,), create_method="load")
test_Speaker = _test_id_gen("speakers")
test_Text = _test_id_gen("texts")
test_VectorFont = _test_id_gen("fonts", args_create=("<builtin>",), create_method="load")
test_World = _test_id_gen("worlds")

ns = globals()
for t in bpy.data.curves.bl_rna.functions["new"].parameters["type"].enum_items.keys():
    ns["test_Curve_%s" % t] = _test_id_gen("curves", args_create=(DUMMY_NAME, t))
for t in bpy.data.lights.bl_rna.functions["new"].parameters["type"].enum_items.keys():
    ns["test_Light_%s" % t] = _test_id_gen("lights", args_create=(DUMMY_NAME, t))
# types are a dynamic enum, have to hard-code.
for t in "ShaderNodeTree", "CompositorNodeTree", "TextureNodeTree":
    ns["test_NodeGroup_%s" % t] = _test_id_gen("node_groups", args_create=(DUMMY_NAME, t))
for t in bpy.data.textures.bl_rna.functions["new"].parameters["type"].enum_items.keys():
    ns["test_Texture_%s" % t] = _test_id_gen("textures", args_create=(DUMMY_NAME, t))
del ns


def main():
    for fn_id, fn_val in sorted(globals().items()):
        if fn_id.startswith("test_") and callable(fn_val):
            fn_val(fn_id)

    print("Error (total): %d" % GLOBALS["error_num"])


if __name__ == "__main__":
    main()
