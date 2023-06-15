# SPDX-FileCopyrightText: 2020-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

import os
import pprint


class TestHelper:

    @staticmethod
    def id_to_uid(id_data):
        return (type(id_data).__name__,
                id_data.name_full,
                id_data.users)

    @classmethod
    def blender_data_to_tuple(cls, bdata, pprint_name=None):
        ret = sorted(tuple((cls.id_to_uid(k), sorted(tuple(cls.id_to_uid(vv) for vv in v)))
                           for k, v in bdata.user_map().items()))
        if pprint_name is not None:
            print("\n%s:" % pprint_name)
            pprint.pprint(ret)
        return ret

    @staticmethod
    def ensure_path(path):
        if not os.path.exists(path):
            os.makedirs(path)

    def run_all_tests(self):
        for inst_attr_id in dir(self):
            if not inst_attr_id.startswith("test_"):
                continue
            inst_attr = getattr(self, inst_attr_id)
            if callable(inst_attr):
                inst_attr()
