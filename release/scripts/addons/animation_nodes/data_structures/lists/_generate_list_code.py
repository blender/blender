import re
from textwrap import indent

paths = {}

def setup(utils):
    global paths
    paths = {
        "implementation" : utils.changeFileName(__file__, "__list_implementation.src"),
        "declaration" : utils.changeFileName(__file__, "__list_declaration.src"),
        "numericLists" : utils.changeFileName(__file__, "numeric_list_types.json"),
        "specialLists" : utils.changeFileName(__file__, "special_list_types.json"),
        "numericListFunctions" : utils.changeFileName(__file__, "__numeric_list_functions.src")
    }

def getPyPreprocessTasks(PyPreprocessTask, utils):
    dependencies = [
        __file__,
        paths["implementation"],
        paths["declaration"],
        paths["numericLists"],
        paths["specialLists"]
    ]
    dependencies.extend(iterAdditionalMethodsSources(utils))

    pyxTask = PyPreprocessTask(
        target = utils.changeFileName(__file__, "base_lists.pyx"),
        dependencies = dependencies,
        function = generate_pyx
    )
    pxdTask = PyPreprocessTask(
        target = utils.changeFileName(__file__, "base_lists.pxd"),
        dependencies = dependencies,
        function = generate_pxd
    )
    return [pyxTask, pxdTask]

def generate_pyx(target, utils):
    implementation = utils.readTextFile(paths["implementation"])

    parts = []
    parts.append("cdef struct NotExistentType:\n    char tmp")

    for info in getListInfo(utils):
        listCode = implementation.replace("MORE_METHODS", indent(info["MORE_METHODS"], " "*4))
        listCode = re.sub("EQUALS\((.*), (.*)\)", "({})".format(info["EQUALS"]), listCode)
        listCode = utils.multiReplace(listCode,
            LISTNAME = info["LISTNAME"],
            TYPE = info["TYPE"],
            MEMVIEW = info["MEMVIEW"],
            TRY_CONVERSION_CODE = indent(info["TRY_CONVERSION_CODE"], " "*8),
            TO_PYOBJECT_CODE = indent(info["TO_PYOBJECT_CODE"], " "*8)
        )
        parts.append(listCode)

    utils.writeTextFile(target, "\n\n".join(parts))

def generate_pxd(target, utils):
    declaration = utils.readTextFile(paths["declaration"])
    numericLists = utils.readJsonFile(paths["numericLists"])

    parts = []
    parts.append("ctypedef fused list_or_tuple:\n    list\n    tuple")

    fusedTypeCode = "ctypedef fused NumericList:\n"
    for listName, _ in numericLists:
        fusedTypeCode += "    " + listName + "\n"
    parts.append(fusedTypeCode)

    for info in getListInfo(utils):
        parts.extend(info["DECLARATIONS"])
        parts.append(utils.multiReplace(declaration,
            LISTNAME = info["LISTNAME"],
            TYPE = info["TYPE"],
            MEMVIEW = info["MEMVIEW"]
        ))

    utils.writeTextFile(target, "\n\n".join(parts))

def iterAdditionalMethodsSources(utils):
    yield paths["numericListFunctions"]
    specialLists = utils.readJsonFile(paths["specialLists"])
    for info in specialLists.values():
        if info["Additional Methods"] != "":
            yield utils.changeFileName(__file__, info["Additional Methods"])

def getListInfo(utils):
    lists = []
    lists.extend(iterNumericListInfo(utils))
    lists.extend(iterSpecialListInfo(utils))
    return lists

def iterNumericListInfo(utils):
    numericLists = utils.readJsonFile(paths["numericLists"])
    numericListFunctions = utils.readTextFile(paths["numericListFunctions"])

    for listName, dataType in numericLists:
        yield dict(
            LISTNAME = listName,
            TYPE = dataType,
            MEMVIEW = dataType,
            EQUALS = r"\1 == \2",
            TRY_CONVERSION_CODE = "target[0] = value",
            TO_PYOBJECT_CODE = "return value[0]",
            MORE_METHODS = numericListFunctions,
            DECLARATIONS = []
        )

def iterSpecialListInfo(utils):
    specialLists = utils.readJsonFile(paths["specialLists"])

    for name, info in specialLists.items():
        methodsSource = info["Additional Methods"]
        if methodsSource == "": methods = ""
        else:
            path = utils.changeFileName(__file__, methodsSource)
            methods = utils.readTextFile(path)

        yield dict(
            LISTNAME = name,
            TYPE = info["Type"],
            MEMVIEW = info["Buffer Type"],
            EQUALS = info["Equals"],
            TRY_CONVERSION_CODE = info["Try Conversion"],
            TO_PYOBJECT_CODE = "return " + info["To PyObject"],
            MORE_METHODS = methods,
            DECLARATIONS = info["Declarations"]
        )
