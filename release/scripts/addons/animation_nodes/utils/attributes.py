import functools

def setattrRecursive(owner, propName, value):
    getAttributeSetter(propName)(owner, value)

def getattrRecursive(owner, propName):
    return getAttributeGetter(propName)(owner)

@functools.lru_cache(maxsize = 1024)
def getAttributeSetter(propName):
    variables = {}
    exec("def attrSetter(owner, value): owner.{} = value".format(propName), variables)
    return variables["attrSetter"]

@functools.lru_cache(maxsize = 1024)
def getAttributeGetter(propName):
    return eval("lambda owner: owner.{}".format(propName))
