from ... data_structures import BooleanList

class IDKeyDataType:

    @classmethod
    def create(cls, object, name):
        raise NotImplementedError()

    @classmethod
    def remove(cls, object, name):
        raise NotImplementedError()

    @classmethod
    def exists(cls, object, name):
        raise NotImplementedError()

    @classmethod
    def existsList(cls, objects, name):
        return BooleanList.fromValues(cls.exists(object, name) for object in objects)


    @classmethod
    def set(cls, object, name, data):
        raise NotImplementedError()

    @classmethod
    def get(cls, object, name):
        raise NotImplementedError()

    @classmethod
    def getList(cls, objects, name):
        return [cls.get(object, name) for object in objects]


    @classmethod
    def drawProperty(cls, layout, object, name):
        pass

    @classmethod
    def drawExtras(cls, layout, object, name):
        pass

    @classmethod
    def drawCopyMenu(cls, layout, object, name):
        pass


class CompoundIDKeyDataType(IDKeyDataType):
    pass


class SingleIDKeyDataType(IDKeyDataType):
    identifier = None
    default = None

    @classmethod
    def create(cls, object, name):
        cls.set(object, name, cls.default)

    @classmethod
    def remove(cls, object, name):
        try: del object[cls.getKey(name)]
        except: pass

    @classmethod
    def exists(cls, object, name):
        return hasattr(object, cls.getPath(name))

    @classmethod
    def set(cls, object, name, data):
        object[cls.getKey(name)] = data

    @classmethod
    def get(cls, object, name):
        return getattr(object, cls.getPath(name), cls.default)

    @classmethod
    def getList(cls, objects, name):
        default = cls.default
        path = cls.getPath(name)
        return [getattr(object, path, default) for object in objects]

    @classmethod
    def drawProperty(cls, layout, object, name):
        layout.prop(object, cls.getPath(name), text = "")

    @classmethod
    def getKey(cls, name):
        return "AN*%s*%s" % (cls.identifier, name)

    @classmethod
    def getPath(cls, name):
        return '["AN*%s*%s"]' % (cls.identifier, name)
