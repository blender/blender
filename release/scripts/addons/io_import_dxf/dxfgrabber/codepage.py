# Purpose: codepage handling
# Created: 21.07.2012, taken from my ezdxf project
# Copyright (C) 2012, Manfred Moitzi
# License: MIT License
from __future__ import unicode_literals
__author__ = "mozman <mozman@gmx.at>"

codepages = {
    '874': 'cp874', # Thai,
    '932': 'cp932', # Japanese
    '936': 'gbk', # UnifiedChinese
    '949': 'cp949', # Korean
    '950': 'cp950', # TradChinese
    '1250': 'cp1250', # CentralEurope
    '1251': 'cp1251', # Cyrillic
    '1252': 'cp1252', # WesternEurope
    '1253': 'cp1253', # Greek
    '1254': 'cp1254', # Turkish
    '1255': 'cp1255', # Hebrew
    '1256': 'cp1256', # Arabic
    '1257': 'cp1257', # Baltic
    '1258': 'cp1258', # Vietnam
}


def toencoding(dxfcodepage):
    for codepage, encoding in codepages.items():
        if dxfcodepage.endswith(codepage):
            return encoding
    return 'cp1252'


def tocodepage(encoding):
    for codepage, enc in codepages.items():
        if enc == encoding:
            return 'ANSI_'+codepage
    return 'ANSI_1252'
