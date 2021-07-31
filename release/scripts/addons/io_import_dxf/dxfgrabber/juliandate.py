# Purpose: julian date
# Created: 21.03.2011
# Copyright (C) 2011, Manfred Moitzi
# License: MIT License
from __future__ import unicode_literals
__author__ = "mozman <mozman@gmx.at>"

from math import floor
from datetime import datetime


def frac(number):
    return number - floor(number)


class JulianDate:
    def __init__(self, date):
        self.date = date
        self.result = self.julian_date() + self.fractional_day()

    def fractional_day(self):
        seconds = self.date.hour * 3600. + self.date.minute * 60. + self.date.second
        return seconds / 86400.

    def julian_date(self):
        y = self.date.year + (float(self.date.month) - 2.85) / 12.
        A = floor(367. * y) - 1.75 * floor(y) + self.date.day
        B = floor(A) - 0.75 * floor(y / 100.)
        return floor(B) + 1721115.


class CalendarDate:
    def __init__(self, juliandate):
        self.jdate = juliandate
        year, month, day = self.get_date()
        hour, minute, second = frac2time(self.jdate)
        self.result = datetime(year, month, day, hour, minute, second)

    def get_date(self):
        Z = floor(self.jdate)

        if Z < 2299161:
            A = Z  # julian calender
        else:
            g = floor((Z - 1867216.25) / 36524.25)  # gregorian calendar
            A = Z + 1. + g - floor(g / 4.)

        B = A + 1524.
        C = floor((B - 122.1) / 365.25)
        D = floor(365.25 * C)
        E = floor((B - D) / 30.6001)

        day = B - D - floor(30.6001 * E)
        month = E - 1 if E < 14 else E - 13
        year = C - 4716 if month > 2 else C - 4715
        return int(year), int(month), int(day)


def frac2time(jdate):
    seconds = int(frac(jdate) * 86400.)
    hour = int(seconds / 3600)
    seconds = seconds % 3600
    minute = int(seconds / 60)
    second = seconds % 60
    return hour, minute, second


def julian_date(date):
    return JulianDate(date).result


def calendar_date(juliandate):
    return CalendarDate(juliandate).result