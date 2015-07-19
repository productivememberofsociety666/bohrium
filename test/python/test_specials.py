import numpy as np
import bohrium as bh
from numpytest import numpytest,gen_views,TYPES


class test_doubletranspose(numpytest):

    def init(self):
        for v in gen_views(3,64,2):
            a = {}
            d = locals()
            exec(v, globals(), d)
            a = d["a"]
            yield (a,v)

    def test_doubletranspose(self,a):
        cmd = "res = a[0].T + a[0].T"
        d = locals()
        exec(cmd, globals(), d)
        res = d["res"]
        return (res,cmd)

class test_largedim(numpytest):

    def init(self):
        for v in gen_views(7,8,4):
            a = {}
            d = locals()
            exec(v, globals(), d)
            a = d["a"]
            yield (a,v)

    def test_largedim(self,a):
        cmd = "res = a[0] + (a[0] * 4)"
        d = locals()
        exec(cmd, globals(), d)
        res = d["res"]
        return (res,cmd)

class test_overlapping(numpytest):
    def init(self):
        for v in gen_views(7,8,4):
            a = {}
            d = locals()
            exec(v, globals(), d)
            a = d["a"]
            yield (a,v)

    def test_identity(self,a):
        cmd = "a[0][1:] = a[0][:-1]"
        d = locals()
        exec(cmd, globals(), d)
        a = d["a"]
        return (a[0],cmd)

    def test_add(self,a):
        if bh.check(a[0]):
            cmd = "bh.add(a[0][:-1], 42, a[0][1:])"
        else:
            cmd = "t = np.add(a[0][:-1], 42); a[0][1:] = t"
        d = locals()
        exec(cmd, globals(), d)
        a = d["a"]
        return (a[0],cmd)
