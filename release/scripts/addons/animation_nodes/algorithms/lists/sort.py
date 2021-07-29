import re

def naturalSortKey(text):
    return [_convert(c) for c in re.split('([0-9]+)', text)]

def _convert(text):
    return text.zfill(10) if text.isdigit() else text.lower()
