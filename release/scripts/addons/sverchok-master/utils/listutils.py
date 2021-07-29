# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####


from functools import reduce
#####################################################
#################### lists magic ####################
#####################################################


def create_list(x, y):
    if type(y) in [list, tuple]:
        return reduce(create_list, y, x)
    else:
        return x.append(y) or x





def preobrazovatel(list_a, levels, level2=1):
    list_tmp = []
    level = levels[0]

    if level > level2:
        if type(list_a)in [list, tuple]:
            for l in list_a:
                if type(l) in [list, tuple]:
                    tmp = preobrazovatel(l, levels, level2+1)
                    if type(tmp) in [list, tuple]:
                        list_tmp.extend(tmp)
                    else:
                        list_tmp.append(tmp)
                else:
                    list_tmp.append(l)

    elif level == level2:
        if type(list_a) in [list, tuple]:
            for l in list_a:
                if len(levels) == 1:
                    tmp = preobrazovatel(l, levels, level2+1)
                else:
                    tmp = preobrazovatel(l, levels[1:], level2+1)
                list_tmp.append(tmp if tmp else l)

    else:
        if type(list_a) in [list, tuple]:
            list_tmp = reduce(create_list, list_a, [])

    return list_tmp


def myZip(list_all, level, level2=0):
    if level == level2:
        if type(list_all) in [list, tuple]:
            list_lens = []
            list_res = []
            for l in list_all:
                if type(l) in [list, tuple]:
                    list_lens.append(len(l))
                else:
                    list_lens.append(0)
            if list_lens == []:
                return False
            min_len = min(list_lens)
            for value in range(min_len):
                lt = []
                for l in list_all:
                    lt.append(l[value])
                t = list(lt)
                list_res.append(t)
            return list_res
        else:
            return False
    elif level > level2:
        if type(list_all) in [list, tuple]:
            list_res = []
            list_tr = myZip(list_all, level, level2+1)
            if list_tr is False:
                list_tr = list_all
            t = []
            for tr in list_tr:
                if type(list_tr) in [list, tuple]:
                    list_tl = myZip(tr, level, level2+1)
                    if list_tl is False:
                        list_tl = list_tr
                    t.extend(list_tl)
            list_res.append(list(t))
            return list_res
        else:
            return False


#####################################################
################### update List join magic ##########
#####################################################


def myZip_2(list_all, level, level2=1):
    def create_listDown(list_all, level):
        def subDown(list_a, level):
            list_b = []
            for l2 in list_a:
                if type(l2) in [list, tuple]:
                    list_b.extend(l2)
                else:
                    list_b.append(l2)
            if level > 1:
                list_b = subDown(list_b, level-1)
            return list_b

        list_tmp = []
        if type(list_all) in [list, tuple]:
            for l in list_all:
                list_b = subDown(l, level-1)
                list_tmp.append(list_b)
        else:
            list_tmp = list_all
        return list_tmp

    list_tmp = list_all.copy()
    for x in range(level-1):
        list_tmp = create_listDown(list_tmp, level)

    list_r = []
    l_min = []

    for el in list_tmp:
        if type(el) not in [list, tuple]:
            break

        l_min.append(len(el))

    if l_min == []:
        l_min = [0]
    lm = min(l_min)
    for elm in range(lm):
        for el in list_tmp:
            list_r.append(el[elm])

    list_tmp = list_r

    for lev in range(level-1):
        list_tmp = [list_tmp]

    return list_tmp


def joiner(list_all, level, level2=1):
    list_tmp = []

    if level > level2:
        if type(list_all) in [list, tuple]:
            for list_a in list_all:
                if type(list_a) in [list, tuple]:
                    list_tmp.extend(list_a)
                else:
                    list_tmp.append(list_a)
        else:
            list_tmp = list_all

        list_res = joiner(list_tmp, level, level2=level2+1)
        list_tmp = [list_res]

    if level == level2:
        if type(list_all) in [list, tuple]:
            for list_a in list_all:
                if type(list_a) in [list, tuple]:
                    list_tmp.extend(list_a)
                else:
                    list_tmp.append(list_a)
        else:
            list_tmp.append(list_all)

    if level < level2:
        if type(list_all) in [list, tuple]:
            for l in list_all:
                list_tmp.append(l)
        else:
            list_tmp.append(l)

    return list_tmp


def wrapper_2(l_etalon, list_a, level):
    def subWrap(list_a, level, count):
        list_b = []
        if level == 1:
            if len(list_a) == count:
                for l in list_a:
                    list_b.append([l])
            else:
                dc = len(list_a)//count
                for l in range(count):
                    list_c = []
                    for j in range(dc):
                        list_c.append(list_a[l*dc+j])
                    list_b.append(list_c)
        else:
            for l in list_a:
                list_b = subWrap(l, level-1, count)
        return list_b

    def subWrap_2(l_etalon, len_l, level):
        len_r = len_l
        if type(l_etalon) in [list, tuple]:
            len_r = len(l_etalon) * len_l
            if level > 1:
                len_r = subWrap_2(l_etalon[0], len_r, level-1)

        return len_r

    len_l = len(l_etalon)
    lens_l = subWrap_2(l_etalon, 1, level)
    list_tmp = subWrap(list_a, level, lens_l)

    for l in range(level-1):
        list_tmp = [list_tmp]
    return list_tmp
