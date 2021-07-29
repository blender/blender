import os
import pkgutil
import importlib

def importAllSubmodules(path, packageName):
    modules = []
    for name in sorted(iterSubModuleNames(path)):
        module = importlib.import_module("." + name, packageName)
        modules.append(module)
    return modules

def iterSubModuleNames(path, root = ""):
    for importer, moduleName, isPackage in pkgutil.iter_modules([path]):
        if isPackage:
            subPath = os.path.join(path, moduleName)
            subRoot = root + moduleName + "."
            yield from iterSubModuleNames(subPath, subRoot)
        else:
            yield root + moduleName
