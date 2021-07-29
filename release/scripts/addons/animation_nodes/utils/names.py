import re
import bpy
import random
import keyword
from functools import lru_cache

def toDataPath(name):
    return '["{}"]'.format(name)

def getPossibleObjectName(name = "object"):
    return getPossibleName(bpy.data.objects, name)

def getPossibleCurveName(name = "curve"):
    return getPossibleName(bpy.data.curves, name)

def getPossibleMeshName(name = "mesh"):
    return getPossibleName(bpy.data.meshes, name)

def getPossibleCameraName(name = "camera"):
    return getPossibleName(bpy.data.cameras, name)

def getPossibleLampName(name = "lamp"):
    return getPossibleName(bpy.data.lamps, name)

def getPossibleNodeName(nodeTree, name = "node"):
    return getPossibleName(nodeTree.nodes, name)

def getPossibleSocketName(node, name = "socket"):
    while True:
        name = getPossibleName(node.inputs, name)
        if name not in node.outputs:
            break
    return name

def getPossibleName(field, name = "element"):
    randomString = getRandomString(3)
    counter = 1
    while field.get(name + randomString + str(counter)):
        counter += 1
    return name + randomString + str(counter)

def getRandomString(length):
    random.seed()
    return ''.join(random.choice("abcdefghijklmnopqrstuvwxyz") for _ in range(length))

@lru_cache(maxsize = 2**15)
def replaceVariableName(code, oldName, newName):
    pattern = r"([^\.\"']|^)\b{}\b".format(oldName)
    return re.sub(pattern, r"\1{}".format(newName), code)

def toVariableName(name):
    variable = re.sub("\W+", "", name)
    if keyword.iskeyword(variable): variable += "_"
    if variable == "": variable = "_"
    return variable

def toInterfaceName(sourceName):
    tempName = ""

    for i, char in enumerate(sourceName):
        if i == 0:
            tempName += char.upper()
            continue
        lastChar = tempName[-1]
        if lastChar == " ":
            tempName += char.upper()
        elif lastChar.isalpha() and char.isnumeric():
            tempName += " " + char.upper()
        elif lastChar.isnumeric() and char.isalpha():
            tempName += " " + char.upper()
        elif lastChar.islower() and char.isupper():
            tempName += " " + char.upper()
        elif char == "_":
            tempName += " "
        else:
            tempName += char

    bindingWords = ["and", "of", "to", "from"]
    words = tempName.split()
    newName = ""
    for word in words:
        if word.lower() in bindingWords:
            word = word.lower()
        newName += " " + word

    return newName
