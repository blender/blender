import bpy
from mathutils import Vector, Euler
from . base import CompoundIDKeyDataType
from ... utils.operators import makeOperator
from ... data_structures import Vector3DList, EulerList

class TransformDataType(CompoundIDKeyDataType):
    identifier = "Transforms"

    @classmethod
    def create(cls, object, name):
        cls.set(object, name, ((0.0, 0.0, 0.0), Euler((0.0, 0.0, 0.0)), (1.0, 1.0, 1.0)))

    @classmethod
    def remove(cls, object, name):
        for key in cls.iterSubpropertyKeys(name):
            if key in object: del object[key]
        try: del object[cls.getRotationOrderKey(name)]
        except: pass


    @classmethod
    def exists(cls, object, name):
        return (all(hasattr(object, key) for key in cls.iterSubpropertyPaths(name))
                and hasattr(object, cls.getRotationOrderPath(name)))

    @classmethod
    def set(cls, object, name, data):
        for value, key in zip(data, cls.iterSubpropertyKeys(name)):
            object[key] = value
        object[cls.getRotationOrderKey(name)] = data[1].order

    @classmethod
    def get(cls, object, name):
        paths = tuple(cls.iterSubpropertyPaths(name))
        return (Vector(getattr(object, paths[0], (0.0, 0.0, 0.0))),
                Euler(getattr(object, paths[1], (0.0, 0.0, 0.0)), cls.getRotationOrder(object, name)),
                Vector(getattr(object, paths[2], (1.0, 1.0, 1.0))))

    @classmethod
    def getLocation(cls, object, name):
        path = '["AN*Transforms*Location*%s"]' % name
        return Vector(getattr(object, paths, (0.0, 0.0, 0.0)))

    @classmethod
    def getRotation(cls, object, name):
        path = '["AN*Transforms*Rotation*%s"]' % name
        return Euler(getattr(object, paths, (0.0, 0.0, 0.0)), cls.getRotationOrder(object, name))

    @classmethod
    def getScale(cls, object, name):
        path = '["AN*Transforms*Scale*%s"]' % name
        return Vector(getattr(object, paths, (1.0, 1.0, 1.0)))

    @classmethod
    def getLocations(cls, objects, name):
        default = (0.0, 0.0, 0.0)
        path = '["AN*Transforms*Location*%s"]' % name
        return Vector3DList.fromValues(getattr(object, path, default) for object in objects)

    @classmethod
    def getRotations(cls, objects, name):
        default = (0.0, 0.0, 0.0)
        path = '["AN*Transforms*Rotation*%s"]' % name
        rotations = EulerList(capacity = len(objects))
        for object in objects:
            rot = Euler(getattr(object, path, default), cls.getRotationOrder(object, name))
            rotations.append(rot)
        return rotations

    @classmethod
    def getRotationOrder(cls, object, name):
        return getattr(object, cls.getRotationOrderPath(name), "XYZ")

    @classmethod
    def getScales(cls, objects, name):
        default = (1.0, 1.0, 1.0)
        path = '["AN*Transforms*Scale*%s"]' % name
        return Vector3DList.fromValues(getattr(object, path, default) for object in objects)


    @classmethod
    def drawProperty(cls, layout, object, name):
        row = layout.row()
        for path, label in zip(cls.iterSubpropertyPaths(name), ["Location", "Rotation", "Scale"]):
            col = row.column(align = True)
            if label == "Rotation":
                label += " ({})".format(cls.getRotationOrder(object, name))
            col.label(label)
            col.prop(object, path, text = "")

    @classmethod
    def drawExtras(cls, layout, object, name):
        props = layout.operator("an.id_key_from_current_transforms", icon = "ROTACTIVE")
        props.name = name

    @classmethod
    def drawCopyMenu(cls, layout, object, name):
        props = layout.operator("an.id_key_to_current_transforms", text = "to Transforms")
        props.name = name

    @classmethod
    def iterSubpropertyKeys(cls, name):
        yield "AN*Transforms*Location*" + name
        yield "AN*Transforms*Rotation*" + name
        yield "AN*Transforms*Scale*" + name

    @classmethod
    def iterSubpropertyPaths(cls, name):
        yield '["AN*Transforms*Location*%s"]' % name
        yield '["AN*Transforms*Rotation*%s"]' % name
        yield '["AN*Transforms*Scale*%s"]' % name

    @classmethod
    def getRotationOrderKey(cls, name):
        return "AN*Transforms*Rotation Order*" + name

    @classmethod
    def getRotationOrderPath(cls, name):
        return '["AN*Transforms*Rotation Order*%s"]' % name


@makeOperator("an.id_key_from_current_transforms", "From Current Transforms",
              arguments = ["String"],
              description = "Assign transform ID Key based on current loc/rot/scale.")
def idKeyFromCurrentTransforms(name):
    for object in bpy.context.selected_objects:
        object.id_keys.set("Transforms", name, (object.location, object.rotation_euler, object.scale))

@makeOperator("an.id_key_to_current_transforms", "To Current Transforms",
              arguments = ["String"],
              description = "Set transformation on object.")
def idKeyToCurrentTransforms(name):
    for object in bpy.context.selected_objects:
        loc, rot, scale = object.id_keys.get("Transforms", name)
        object.location = loc
        object.rotation_euler = rot
        object.scale = scale
