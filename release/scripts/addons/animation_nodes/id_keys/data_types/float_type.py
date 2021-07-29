from . base import SingleIDKeyDataType
from ... data_structures import DoubleList

class FloatDataType(SingleIDKeyDataType):
    identifier = "Float"
    default = 0.0

    @classmethod
    def getList(cls, objects, name):
        default = cls.default
        path = cls.getPath(name)
        return DoubleList.fromValues(getattr(object, path, default) for object in objects)
