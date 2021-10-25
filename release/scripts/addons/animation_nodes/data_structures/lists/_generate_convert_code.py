paths = {}

def setup(utils):
    global paths
    paths = {
        "source" : utils.changeFileName(__file__, "__convert.src"),
        "numericLists" : utils.changeFileName(__file__, "numeric_list_types.json")
    }

def getPyPreprocessTasks(PyPreprocessTask, utils):
    dependencies = [
        __file__,
        paths["source"],
        paths["numericLists"]
    ]
    pyxTask = PyPreprocessTask(
        target = utils.changeFileName(__file__, "convert.pyx"),
        dependencies = dependencies,
        function = generate
    )
    return [pyxTask]

def generate(target, utils):
    source = utils.readTextFile(paths["source"])
    numericLists = utils.readJsonFile(paths["numericLists"])
    listNames = ", ".join(name for name, _ in numericLists)

    parts = []
    parts.append("from . base_lists cimport NumericList")
    parts.append("from . base_lists cimport " + listNames)
    parts.append("")

    for listName, listType in numericLists:
        code = utils.multiReplace(source,
            TARGETLIST = listName,
            TYPE = listType)
        parts.append(code)

    utils.writeTextFile(target, "\n".join(parts))
