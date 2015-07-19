import bohrium as np
from numpytest import numpytest, TYPES
import six.moves

sign_str="""
def sign(a):
    if np.ndarray.check(a):
        return np.sign(a)
    else:
        s = np.absolute(a)
        s += s == 0
        return a/s
"""

class test_sign(numpytest):

    def init(self):
        self.config['maxerror'] = 0.00001

        for dtype in TYPES.ALL_SIGNED:
            a = {}
            cmd = "a[0] = self.arange(-10, 0, 1, dtype=%s);" % dtype
            d = locals()
            exec(cmd, globals(), d)
            a = d["a"]
            yield (a, cmd)

    def test_sign(self,a):
        cmd = "res = np.sign(a[0])"
        d = locals()
        exec(cmd, globals(), d)
        res = d["res"]
        return (res, cmd)

class test_csign_neg(numpytest):

    def init(self):
        self.config['maxerror'] = 0.00001

        for dtype in TYPES.COMPLEX:
            a = {}
            cmd = "a[0] = self.arange(-10, 0, 1, dtype=%s);" % dtype
            d = locals()
            exec(cmd, globals(), d)
            a = d["a"]
            yield (a, cmd)

    def test_sign(self,a):
        cmd = "res = np.sign(a[0])"
        d = locals()
        exec(cmd, globals(), d)
        res = d["res"]
        return (res, cmd)

class test_csign_pos(numpytest):

    def init(self):
        self.config['maxerror'] = 0.00001

        for dtype in TYPES.COMPLEX:
            a = {}
            cmd = "a[0] = self.arange(1, 10, 1, dtype=%s);" % dtype
            d = locals()
            exec(cmd, globals(), d)
            a = d["a"]
            yield (a, cmd)

    def test_sign(self,a):
        cmd = "res = np.sign(a[0])"
        d = locals()
        exec(cmd, globals(), d)
        res = d["res"]
        return (res, cmd)

class test_csign_zero(numpytest):

    def init(self):
        self.config['maxerror'] = 0.00001

        for dtype in TYPES.COMPLEX:
            a = {}
            cmd = "a[0] = self.zeros((10), dtype=%s);" % dtype
            d = locals()
            exec(cmd, globals(), d)
            a = d["a"]
            yield (a, cmd)

    def test_sign(self,a):
        cmd = "res = np.sign(a[0])"
        d = locals()
        exec(cmd, globals(), d)
        res = d["res"]
        return (res, cmd)

class test_csign_mixed(numpytest):

    def init(self):
        self.config['maxerror'] = 0.00001
        signs = []
        for x in six.moves.range(-1,2):
            for y in six.moves.range(-1,2):
                d = locals()
                exec("z = %d+%dj"% (x,y), globals(), d)
                z = d["z"]
                signs.append(z)

        for dtype in TYPES.COMPLEX:
            a = {}
            cmd = "a[0] = self.asarray(%s, dtype=%s);" % (signs, dtype)
            d = locals()
            exec(cmd, globals(), d)
            a = d["a"]
            yield (a, cmd)

    def test_sign(self,a):
        cmd = sign_str+"res = sign(a[0])"
        d = locals()
        exec(cmd, globals(), d)
        res = d["res"]
        return (res, cmd)
