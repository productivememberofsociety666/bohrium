import bohrium as np
from numpytest import numpytest,gen_views,TYPES


class test_flatten(numpytest):

    def init(self):
        for v in gen_views(3,64,6):
            a = {}
            d = locals()
            exec(v, globals(), d)
            a = d["a"]
            yield (a,v)

    def test_flatten(self,a):
        cmd = "res = np.flatten(a[0])"
        d = locals()
        exec(cmd, globals(), d)
        res = d["res"]
        return (res,cmd)

    def test_flatten_self(self,a):
        cmd = "res = a[0].flatten()"
        exec(cmd)
        return (res,cmd)

    def test_ravel_self(self,a):
        cmd = "res = a[0].ravel()"
        exec(cmd)
        return (res,cmd)

class test_diagonal(numpytest):

    def init(self):
        for v in gen_views(2,64,12,min_ndim=2):
            a = {}
            d = locals()
            exec(v, globals(), d)
            a = d["a"]
            yield (a,v)

    def test_diagonal(self,a):
        cmd = "res = np.diagonal(a[0])"
        d = locals()
        exec(cmd, globals(), d)
        res = d["res"]
        return (res,cmd)


class test_transpose(numpytest):

    def init(self):
        for v in gen_views(4,16,6,min_ndim=2):
            a = {}
            d = locals()
            exec(v, globals(), d)
            a = d["a"]
            yield (a,v)

    def test_transpose(self,a):
        cmd = "res = np.transpose(a[0])"
        d = locals()
        exec(cmd, globals(), d)
        res = d["res"]
        return (res,cmd)
