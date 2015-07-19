import numpy as np
import bohrium as bh
from numpytest import numpytest,gen_views,TYPES


class test_empty(numpytest):

    def init(self):
        a = {}
        cmd = "a[0] = bh.array([]);a[1] = bh.array([])"
        d = locals()
        exec(cmd, globals(), d)
        a = d["a"]
        yield (a, cmd)

    def test_add(self,a):
        cmd = "res = a[0] + a[1]"
        d = locals()
        exec(cmd, globals(), d)
        res = d["res"]
        return (res,cmd)
