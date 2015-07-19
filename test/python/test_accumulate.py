import numpy as np
import bohrium as bh
from numpytest import numpytest,gen_views,TYPES

class test_accumulate(numpytest):
    def __init__(self):
        numpytest.__init__(self)
        self.config['maxerror'] = 0.00001

    def init(self):
        for v in gen_views(4,8,6,min_ndim=1):
            a = {}
            self.axis = 0
            d = locals()
            exec(v, globals(), d)
            a = d["a"]
            yield (a,v)
            for axis in range(1,a[0].ndim):
                exec(v)
                self.axis = axis
                yield (a,v)

    def test_cumsum(self,a):
        if bh.check(a[0]):
            cmd = "res = bh.add.accumulate(a[0],axis=%d)"%self.axis
        else:
            cmd = "res = np.add.accumulate(a[0],axis=%d)"%self.axis
        d = locals()
        exec(cmd, globals(), d)
        res = d["res"]
        return (res,cmd)

    def test_cumprod(self,a):
        if bh.check(a[0]):
            cmd = "res = bh.multiply.accumulate(a[0],axis=%d)"%self.axis
        else:
            cmd = "res = np.multiply.accumulate(a[0],axis=%d)"%self.axis
        d = locals()
        exec(cmd, globals(), d)
        res = d["res"]
        return (res,cmd)
