from .. algorithms.hashing import strToEnumItemID

def enumItemsFromList(itemData):
    items = []
    for element in itemData:
        items.append((element, element, "", "NONE", strToEnumItemID(element)))
    if len(items) == 0:
        items = [("NONE", "NONE", "")]
    return items
