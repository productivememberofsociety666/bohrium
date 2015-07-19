/*
This file is part of Bohrium and copyright (c) 2012 the Bohrium
team <http://www.bh107.org>.

Bohrium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

Bohrium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the
GNU Lesser General Public License along with Bohrium.

If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#include <Python.h>
#include <structmember.h>
#include <dlfcn.h>
#include <bh_mem_signal.h>

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

//The NumPy API changed in version 1.7
#if(NPY_API_VERSION >= 0x00000007)
    #define BH_PyArrayObject PyArrayObject_fields
#else
    #define BH_PyArrayObject PyArrayObject
    #define NPY_ARRAY_OWNDATA NPY_OWNDATA
#endif

#if PY_MAJOR_VERSION >= 3
    #define NPY_PY3K
#endif

//Forward declaration
static PyObject *BhArray_data_bhc2np(PyObject *self, PyObject *args);
static PyTypeObject BhArrayType;

#define BhArray_CheckExact(op) (((PyObject*)(op))->ob_type == &BhArrayType)
PyObject *ndarray = NULL; //The ndarray Python module
PyObject *ufunc = NULL; //The ufunc Python module
PyObject *bohrium = NULL; //The Bohrium Python module
PyObject *array_create = NULL; //The array_create Python module

typedef struct
{
    BH_PyArrayObject base;
    PyObject *bhc_ary;
    PyObject *bhc_ary_version;
    PyObject *bhc_view;
    PyObject *bhc_view_version;
    int mmap_allocated;
}BhArray;

//Help function that returns number of bytes in 'ary'
//BUT minimum 'itemsize', which mimic the behavior of NumPy
static int64_t ary_nbytes(const BhArray *ary)
{
    int64_t size = PyArray_NBYTES((PyArrayObject*)ary);
    if(size == 0)
        return PyArray_ITEMSIZE((PyArrayObject*)ary);
    else
        return size;
}

//Help function to retrieve the Bohrium-C data pointer
//Return -1 on error
static int get_bhc_data_pointer(PyObject *ary, int force_allocation, int nullify, void **out_data)
{
    if(((BhArray*)ary)->mmap_allocated == 0)
    {
        PyErr_SetString(PyExc_TypeError, "The array data wasn't allocated through mmap(). "
                  "Typically, this is because the base array was created from a template, "
                  "which is not support by Bohrium");
        return -1;
    }
    PyObject *data = PyObject_CallMethod(ndarray, "get_bhc_data_pointer",
                                         "Oii", ary, force_allocation, nullify);
    if(data == NULL)
        return -1;

#if defined(NPY_PY3K)
    if(!PyLong_Check(data))
#else
    if(!PyInt_Check(data))
#endif
    {
        PyErr_SetString(PyExc_TypeError, "get_bhc_data_pointer(ary) should "
                "return a Python integer that represents a memory address");
        Py_DECREF(data);
        return -1;
    }
    void *d = PyLong_AsVoidPtr(data);
    Py_DECREF(data);
    if(force_allocation && d == NULL)
    {
        PyErr_SetString(PyExc_TypeError, "get_bhc_data_pointer(ary, allocate=True) "
                                         "shouldn't return a NULL pointer");
        return -1;
    }
    *out_data = d;
    return 0;
}

//Help function to set the Bohrium-C data from a numpy array
//Return -1 on error
static int set_bhc_data_from_ary(PyObject *self, PyObject *ary)
{
    if(((BhArray*)self)->mmap_allocated == 0)
    {
        PyErr_SetString(PyExc_TypeError, "The array data wasn't allocated through mmap(). "
                  "Typically, this is because the base array was created from a template, "
                  "which is not support by Bohrium");
        return -1;
    }
    PyObject *ret = PyObject_CallMethod(ndarray, "set_bhc_data_from_ary", "OO", self, ary);
    Py_XDECREF(ret);
    if(ret == NULL)
        return -1;
    return 0;
}

//Help function for unprotect memory
//Return -1 on error
static int _munprotect(void *data, npy_intp size)
{
    if(mprotect(data, size, PROT_WRITE) != 0)
    {
        int errsv = errno;//mprotect() sets the errno.
        PyErr_Format(PyExc_RuntimeError,"Error - could not (un-)mprotect a "
                     "data region. Returned error code by mprotect(): %s.\n"
                     ,strerror(errsv));
        return -1;
    }
    return 0;
}

//Help function for memory un-map
//Return -1 on error
static int _munmap(void *addr, npy_intp size)
{
    if(munmap(addr, size) == -1)
    {
        int errsv = errno;//munmmap() sets the errno.
        PyErr_Format(PyExc_RuntimeError, "The Array Data Protection "
                     "could not mummap the data region: %p (size: %ld). "
                     "Returned error code by mmap: %s.", addr, size,
                     strerror(errsv));
        return -1;
    }
    return 0;
}

//Help function for memory re-map
//Return -1 on error
static int _mremap_data(void *dst, void *src, npy_intp size)
{
#if MREMAP_FIXED
    if(mremap(src, size, size, MREMAP_FIXED|MREMAP_MAYMOVE, dst) == MAP_FAILED)
    {
        int errsv = errno;//mremap() sets the errno.
        PyErr_Format(PyExc_RuntimeError,"Error - could not mremap a "
                     "data region. Returned error code by mremap(): %s.\n"
                     ,strerror(errsv));
        return -1;
    }
    return 0;
#else
    //Systems that doesn't support mremap will use memcpy, which introduces a
    //race-condition if another thread access the 'dst' memory before memcpy finishes.
    if(_munprotect(dst, size) != 0)
        return -1;
    memcpy(dst, src, size);
    return _munmap(src, size);
#endif
}

void mem_access_callback(void *id, void *addr)
{
    PyObject *ary = (PyObject *) id;
//    printf("mem_access_callback() - ary: %p, addr: %p\n", ary, addr);

    PyGILState_STATE GIL = PyGILState_Ensure();
    PyErr_WarnEx(NULL,"Encountering an operation not supported by Bohrium. "
                      "It will be handled by the original NumPy.",1);

    if(BhArray_data_bhc2np(ary, NULL) == NULL)
        PyErr_Print();
    PyGILState_Release(GIL);
}

//Help function for protecting the memory of the NumPy part of 'ary'
//Return -1 on error
static int _mprotect_np_part(BhArray *ary)
{
    //Finally we memory protect the NumPy data
    if(mprotect(ary->base.data, ary_nbytes(ary), PROT_NONE) == -1)
    {
        int errsv = errno;//mprotect() sets the errno.
        PyErr_Format(PyExc_RuntimeError,"Error - could not protect a data"
                     "data region. Returned error code by mprotect: %s.\n",
                     strerror(errsv));
        return -1;
    }
    bh_mem_signal_attach(ary, ary->base.data,
                  ary_nbytes(ary), mem_access_callback);
    return 0;
}

//Help function for allocate protected memory for the NumPy part of 'ary'
//This function only allocates if the 'ary' is a new base array and avoids multiple
//allocations by checking and setting the ary->mmap_allocated
//Return -1 on error
static int _protected_malloc(BhArray *ary)
{
    if(ary->mmap_allocated || !PyArray_CHKFLAGS((PyArrayObject*)ary, NPY_ARRAY_OWNDATA))
        return 0;
    ary->mmap_allocated = 1;

    //Allocate page-size aligned memory.
    //The MAP_PRIVATE and MAP_ANONYMOUS flags is not 100% portable. See:
    //<http://stackoverflow.com/questions/4779188/how-to-use-mmap-to-allocate-a-memory-in-heap>
    void *addr = mmap(0, ary_nbytes(ary), PROT_NONE,
                      MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(addr == MAP_FAILED)
    {
        int errsv = errno;//mmap() sets the errno.
        PyErr_Format(PyExc_RuntimeError, "The Array Data Protection "
                     "could not mmap a data region. "
                     "Returned error code by mmap: %s.", strerror(errsv));
        return -1;
    }

    //Lets free the NumPy allocated memory and use the mprotect'ed memory instead
    free(ary->base.data);
    ary->base.data = addr;

    bh_mem_signal_attach(ary, ary->base.data,
                  ary_nbytes(ary), mem_access_callback);
    return 0;
}

static PyObject *
BhArray_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *ret = PyArray_Type.tp_new(type, args, kwds);
    if(_protected_malloc((BhArray *) ret) != 0)
        return NULL;
    return ret;
}

static PyObject *
BhArray_finalize(PyObject *self, PyObject *args)
{
    int e = PyObject_IsInstance(self, (PyObject*) &BhArrayType);
    if(e == -1)
    {
        return NULL;
    }
    else if (e == 0)
    {
        Py_RETURN_NONE;
    }
    ((BhArray*)self)->bhc_ary = Py_None;
    Py_INCREF(Py_None);

    ((BhArray*)self)->bhc_ary_version = PyLong_FromLong(0);

    ((BhArray*)self)->bhc_view = Py_None;
    Py_INCREF(Py_None);

    ((BhArray*)self)->bhc_view_version = Py_None;
    Py_INCREF(Py_None);

    if(_protected_malloc((BhArray *) self) != 0)
        return NULL;

    Py_RETURN_NONE;
}

static PyObject *
BhArray_alloc(PyTypeObject *type, Py_ssize_t nitems)
{
    PyObject *obj;
    obj = (PyObject *)malloc(type->tp_basicsize);
    PyObject_Init(obj, type);
    ((BhArray*)obj)->mmap_allocated = 0;
    return obj;
}

static void
BhArray_dealloc(BhArray* self)
{
    assert(BhArray_CheckExact(self));

    Py_XDECREF(self->bhc_view);
    Py_XDECREF(self->bhc_view_version);
    Py_XDECREF(self->bhc_ary_version);
    Py_XDECREF(self->bhc_ary);

    if(!PyArray_CHKFLAGS((PyArrayObject*)self, NPY_ARRAY_OWNDATA))
    {
        BhArrayType.tp_base->tp_dealloc((PyObject*)self);
        return;//The array doesn't own the array data
    }

    assert(!PyDataType_FLAGCHK(PyArray_DESCR((PyArrayObject*)self), NPY_ITEM_REFCOUNT));

    if(self->mmap_allocated)
    {
        if(_munmap(PyArray_DATA((PyArrayObject*)self), ary_nbytes(self)) == -1)
            PyErr_Print();

        bh_mem_signal_detach(PyArray_DATA((PyArrayObject*)self));
        self->base.data = NULL;
    }
    BhArrayType.tp_base->tp_dealloc((PyObject*)self);
}

static PyObject *
BhArray_data_bhc2np(PyObject *self, PyObject *args)
{
    assert(args == NULL);
    assert(BhArray_CheckExact(self));

    //We move the whole array (i.e. the base array) from Bohrium to NumPy
    PyObject *base = PyObject_CallMethod(ndarray, "get_base", "O", self);

    //Note that we always detach the signal before returning
    bh_mem_signal_detach(PyArray_DATA((PyArrayObject*)base));

    if(base == NULL)
        return NULL;
    assert(BhArray_CheckExact(base));

/* TODO: handle the case where bhc_ary is None by unprotecting the memory.
    //Check if we need to do anything
    if(((BhArray*)base)->bhc_ary == Py_None)
    {
        Py_DECREF(base);
        Py_RETURN_NONE;
    }
*/
    if(!PyArray_CHKFLAGS((PyArrayObject*)base, NPY_ARRAY_OWNDATA))
    {
        PyErr_Format(PyExc_ValueError,"The base array doesn't own its data");
        return NULL;
    }

    //Calling get_bhc_data_pointer(base, allocate=False, nullify=True)
    void *d = NULL;
    if(get_bhc_data_pointer(base, 0, 1, &d) == -1)
        return NULL;
    if(d != NULL)
    {
        if(_mremap_data(PyArray_DATA((PyArrayObject*)base), d,
                        ary_nbytes((BhArray*)base)) != 0)
            return NULL;
            Py_DECREF(base);
    }
    else
    {
        if(_munprotect(PyArray_DATA((PyArrayObject*)base),
                       ary_nbytes((BhArray*)base)) != 0)
            return NULL;
            Py_DECREF(base);
    }

    //Lets delete the current bhc_ary
    if(PyObject_CallMethod(ndarray, "del_bhc", "O", self) == NULL)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
BhArray_data_np2bhc(PyObject *self, PyObject *args)
{
    assert(args == NULL);
    assert(BhArray_CheckExact(self));

    //We move the whole array (i.e. the base array) from Bohrium to NumPy
    PyObject *base = PyObject_CallMethod(ndarray, "get_base", "O", self);
    if(base == NULL)
        return NULL;
    assert(BhArray_CheckExact(base));

    if(!PyArray_CHKFLAGS((PyArrayObject*)base, NPY_ARRAY_OWNDATA))
    {
        PyErr_Format(PyExc_ValueError,"The base array doesn't own its data");
        return NULL;
    }

    //Make sure that bhc_ary exist
    if(((BhArray*)base)->bhc_ary == Py_None)
    {
        PyObject *err = PyObject_CallMethod(ndarray, "new_bhc_base", "O", base);
        if(err == NULL)
            return NULL;
        Py_DECREF(err);
    }
    //Then we unprotect the NumPy memory part
    bh_mem_signal_detach(PyArray_DATA((PyArrayObject*)base));
    if(_munprotect(PyArray_DATA((PyArrayObject*)base),
                   ary_nbytes((BhArray*)base)) != 0)
        return NULL;

    //And sets the bhc data from the NumPy part of 'base'
    if(set_bhc_data_from_ary(base, base) == -1)
        return NULL;

    //Finally, we memory protect the NumPy part of 'base' again
    if(_mprotect_np_part((BhArray*)base) != 0)
        return NULL;
    Py_DECREF(base);
    Py_RETURN_NONE;
}

static PyObject *
BhArray_data_fill(PyObject *self, PyObject *args)
{
    PyObject *np_ary;
    if(!PyArg_ParseTuple(args, "O:ndarray", &np_ary))
        return NULL;

    if(!PyArray_Check(np_ary))
    {
        PyErr_SetString(PyExc_TypeError, "must be a NumPy array");
        return NULL;
    }

    if(!PyArray_ISCARRAY_RO((PyArrayObject*)np_ary))
    {
        PyErr_SetString(PyExc_TypeError, "must be a C-style contiguous array");
        return NULL;
    }

    //Sets the bhc data from the NumPy part of 'base'
    if(set_bhc_data_from_ary(self, np_ary) == -1)
        return NULL;

    Py_RETURN_NONE;
}

static PyObject *
BhArray_copy(PyObject *self, PyObject *args)
{
    assert(args == NULL);
    PyObject *ret = PyObject_CallMethod(array_create, "empty_like", "O", self);
    if(ret == NULL)
        return NULL;
    PyObject *err = PyObject_CallMethod(ufunc, "assign", "OO", self, ret);
    if(err == NULL)
    {
        Py_DECREF(ret);
        return NULL;
    }
    Py_DECREF(err);
    return ret;
}

static PyObject *
BhArray_copy2numpy(PyObject *self, PyObject *args)
{
    assert(args == NULL);

    PyObject *ret = PyArray_NewLikeArray((PyArrayObject*)self, NPY_ANYORDER, NULL, 0);
    if(ret == NULL)
        return NULL;
    PyObject *err = PyObject_CallMethod(ufunc, "assign", "OO", self, ret);
    if(err == NULL)
    {
        Py_DECREF(ret);
        return NULL;
    }
    Py_DECREF(err);
    return ret;
}

static PyObject *
BhArray_resize(PyObject *self, PyObject *args)
{
    PyErr_SetString(PyExc_NotImplementedError, "Bohrium arrays doesn't support resize");
    return NULL;
}


//Help function to make methods calling a Python function
static PyObject *
method2function(char *name, PyObject *self, PyObject *args)
{
    //We parse the 'args' to bohrium.'name' with 'self' as the first argument
    Py_ssize_t i, size = PyTuple_Size(args);
    PyObject *func_args = PyTuple_New(size+1);
    if(func_args == NULL)
        return NULL;
    Py_INCREF(self);
    PyTuple_SET_ITEM(func_args, 0, self);
    for(i=0; i<size; ++i)
    {
        PyObject *t = PyTuple_GET_ITEM(args, i);
        Py_INCREF(t);
        PyTuple_SET_ITEM(func_args, i+1, t);
    }
    PyObject *ret = PyObject_CallMethod(bohrium, name, "O", func_args);
    Py_DECREF(func_args);
    return ret;
}

static PyObject *
BhArray_reshape(PyObject *self, PyObject *args)
{
    return method2function("reshape", self, args);
}

static PyObject *
BhArray_flatten(PyObject *self, PyObject *args)
{
    return method2function("flatten", self, args);
}

static PyObject *
BhArray_sum(PyObject *self, PyObject *args)
{
    return method2function("sum", self, args);
}

static PyObject *
BhArray_prod(PyObject *self, PyObject *args)
{
    return method2function("prod", self, args);
}

static PyMethodDef BhArrayMethods[] = {
    {"__array_finalize__", BhArray_finalize, METH_VARARGS, NULL},
    {"_data_bhc2np", BhArray_data_bhc2np, METH_NOARGS, "Copy the Bohrium-C data to NumPy data"},
    {"_data_np2bhc", BhArray_data_np2bhc, METH_NOARGS, "Copy the NumPy data to Bohrium-C data"},
    {"_data_fill", BhArray_data_fill, METH_VARARGS, "Fill the Bohrium-C data from a numpy NumPy"},
    {"copy", BhArray_copy, METH_NOARGS, "Copy the array in C-style memory layout"},
    {"copy2numpy", BhArray_copy2numpy, METH_NOARGS, "Copy the array in C-style memory "
                                                    "layout to a regular NumPy array"},
    {"resize", BhArray_resize, METH_VARARGS, "Change shape and size of array in-place"},
    {"reshape", BhArray_reshape, METH_VARARGS, "a.reshape(shape)\n\nReturns an array"
                                               "containing the same data with a new shape.\n\n"
                                               "Refer to `bohrium.reshape` for full documentation."},
    {"flatten", BhArray_flatten, METH_VARARGS, "a.flatten()\n\nReturn a copy of the array collapsed into one dimension."},
    {"ravel", BhArray_flatten, METH_VARARGS, "a.ravel()\n\nReturn a copy of the array collapsed into one dimension."},
    {"sum", BhArray_sum, METH_VARARGS, "a.sum(axis=None, dtype=None, out=None)\n\n"
                                       "Return the sum of the array elements over the given axis.\n\n"
                                       "Refer to `bohrium.sum` for full documentation."},
    {"prod", BhArray_prod, METH_VARARGS, " a.prod(axis=None, dtype=None, out=None)\n\n"
                                       "Return the product of the array elements over the given axis\n\n"
                                       "Refer to `numpy.prod` for full documentation."},
    {NULL, NULL, 0, NULL} /* Sentinel */
};

static PyMemberDef BhArrayMembers[] = {
    {"bhc_ary", T_OBJECT_EX, offsetof(BhArray, bhc_ary), 0,
     "The Bohrium backend base-array"},
    {"bhc_ary_version", T_OBJECT_EX, offsetof(BhArray, bhc_ary_version), 0,
     "The version of the Bohrium backend base-array"},
    {"bhc_view", T_OBJECT_EX, offsetof(BhArray, bhc_view), 0,
     "The Bohrium backend view-array"},
    {"bhc_view_version", T_OBJECT_EX, offsetof(BhArray, bhc_view_version), 0,
     "The version of the Bohrium backend view-array"},
    {NULL}  /* Sentinel */
};

static int
BhArray_SetSlice(PyObject *o, Py_ssize_t ilow, Py_ssize_t ihigh, PyObject *v)
{
    if(v == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "cannot delete array elements");
        return -1;
    }
    if(!PyArray_ISWRITEABLE((PyArrayObject *)o))
    {
        PyErr_SetString(PyExc_ValueError, "assignment destination is read-only");
        return -1;
    }
#if defined(NPY_PY3K)
    PyObject *low = PyLong_FromSsize_t(ilow);
    PyObject *high = PyLong_FromSsize_t(ihigh);
#else
    PyObject *low = PyInt_FromSsize_t(ilow);
    PyObject *high = PyInt_FromSsize_t(ihigh);
#endif
    PyObject *slice = PySlice_New(low, high, NULL);
    PyObject *ret = PyObject_CallMethod(ufunc, "setitem", "OOO", o, slice, v);
    Py_XDECREF(low);
    Py_XDECREF(high);
    Py_XDECREF(slice);
    if(ret == NULL)
        return -1;
    Py_XDECREF(ret);
    return 0;
}

static int
BhArray_SetItem(PyObject *o, PyObject *key, PyObject *v)
{
    if(v == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "cannot delete array elements");
        return -1;
    }
    if(!PyArray_ISWRITEABLE((PyArrayObject *)o))
    {
        PyErr_SetString(PyExc_ValueError, "assignment destination is read-only");
        return -1;
    }
    PyObject *ret = PyObject_CallMethod(ufunc, "setitem", "OOO", o, key, v);
    if(ret == NULL)
        return -1;
    Py_XDECREF(ret);
    return 0;
}

static PyObject *
BhArray_GetItem(PyObject *o, PyObject *k)
{
    Py_ssize_t i;
    assert(k != NULL);

    //If the tuple access all dimensions we must check for Python slice objects
    if(PyTuple_Check(k) && (PyTuple_GET_SIZE(k) == PyArray_NDIM((PyArrayObject*)o)))
    {
        for(i=0; i<PyTuple_GET_SIZE(k); ++i)
        {
            PyObject *a = PyTuple_GET_ITEM(k, i);
            if(PySlice_Check(a))
            {
                //A slice never results in a scalar output
                return PyArray_Type.tp_as_mapping->mp_subscript(o, k);
            }
        }
    }
    //If the result is a scalar we let NumPy handle it
#if defined(NPY_PY3K)
    if(PyLong_Check(k) || PyArray_IsScalar(k, Integer) || (PyIndex_Check(k) && !PySequence_Check(k)))
#else
    if(PyArray_IsIntegerScalar(k) || (PyIndex_Check(k) && !PySequence_Check(k)))
#endif
    {
        if(BhArray_data_bhc2np(o, NULL) == NULL)
            return NULL;
    }
    return PyArray_Type.tp_as_mapping->mp_subscript(o, k);
}

static PyObject *
BhArray_GetSeqItem(PyObject *o, Py_ssize_t i)
{
    //If we wrap the index 'i' into a Python Object we can simply use BhArray_GetItem
#if defined(NPY_PY3K)
    PyObject *index = PyLong_FromSsize_t(i);
#else
    PyObject *index = PyInt_FromSsize_t(i);
#endif
    if(index == NULL)
        return NULL;
    PyObject *ret = BhArray_GetItem(o, index);
    Py_DECREF(index);
    return ret;
}

static PyMappingMethods array_as_mapping = {
    (lenfunc)0,                     /*mp_length*/
    (binaryfunc)BhArray_GetItem,    /*mp_subscript*/
    (objobjargproc)BhArray_SetItem, /*mp_ass_subscript*/
};
static PySequenceMethods array_as_sequence = {
    (lenfunc)0,                              /*sq_length*/
    (binaryfunc)NULL,                        /*sq_concat is handled by nb_add*/
    (ssizeargfunc)NULL,                      /*sq_repeat*/
    (ssizeargfunc)BhArray_GetSeqItem,        /*sq_item*/
    (ssizessizeargfunc)0,                    /*sq_slice (Not in the Python doc)*/
    (ssizeobjargproc)BhArray_SetItem,        /*sq_ass_item*/
    (ssizessizeobjargproc)BhArray_SetSlice,  /*sq_ass_slice (Not in the Python doc)*/
    (objobjproc) 0,                          /*sq_contains */
    (binaryfunc) NULL,                       /*sg_inplace_concat */
    (ssizeargfunc)NULL,                      /*sg_inplace_repeat */
};

static PyObject *
BhArray_Repr(PyObject *self)
{
    assert(BhArray_CheckExact(self));
    PyObject *t = BhArray_copy2numpy(self, NULL);
    if(t == NULL)
        return NULL;
    PyObject *str = PyArray_Type.tp_repr(t);
    Py_DECREF(t);
    return str;
}
static PyObject *
BhArray_Str(PyObject *self)
{
    assert(BhArray_CheckExact(self));
    PyObject *t = BhArray_copy2numpy(self, NULL);
    if(t == NULL)
        return NULL;
    PyObject *str = PyArray_Type.tp_str(t);
    Py_DECREF(t);
    return str;
}

//Importing the array_as_number struct
#include "operator_overload.c"

static PyTypeObject BhArrayType = {
#if defined(NPY_PY3K)
    PyVarObject_HEAD_INIT(NULL, 0)
#else
    PyObject_HEAD_INIT(NULL)
    0,                       /* ob_size */
#endif
    "bohrium.ndarray",       /* tp_name */
    sizeof(BhArray),         /* tp_basicsize */
    0,                       /* tp_itemsize */
    (destructor) BhArray_dealloc,/* tp_dealloc */
    0,                       /* tp_print */
    0,                       /* tp_getattr */
    0,                       /* tp_setattr */
#if defined(NPY_PY3K)
    0,                       /* tp_reserved */
#else
    0,                       /* tp_compare */
#endif
    &BhArray_Repr,           /* tp_repr */
    &array_as_number,        /* tp_as_number */
    &array_as_sequence,      /* tp_as_sequence */
    &array_as_mapping,       /* tp_as_mapping */
    0,                       /* tp_hash */
    0,                       /* tp_call */
    &BhArray_Str,            /* tp_str */
    0,                       /* tp_getattro */
    0,                       /* tp_setattro */
    0,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT
#if !defined(NPY_PY3K)
    | Py_TPFLAGS_CHECKTYPES
#endif
    | Py_TPFLAGS_BASETYPE,   /* tp_flags */
    0,                       /* tp_doc */
    0,                       /* tp_traverse */
    0,                       /* tp_clear */
    (richcmpfunc)array_richcompare, /* tp_richcompare */
    0,                       /* tp_weaklistoffset */
    0,                       /* tp_iter */
    0,                       /* tp_iternext */
    BhArrayMethods,          /* tp_methods */
    BhArrayMembers,          /* tp_members */
    0,                       /* tp_getset */
    0,                       /* tp_base */
    0,                       /* tp_dict */
    0,                       /* tp_descr_get */
    0,                       /* tp_descr_set */
    0,                       /* tp_dictoffset */
    0,                       /* tp_init */
    BhArray_alloc,           /* tp_alloc */
    BhArray_new,             /* tp_new */
    0,                                          /* tp_free */
    0,                                          /* tp_is_gc */
    0,                                          /* tp_bases */
    0,                                          /* tp_mro */
    0,                                          /* tp_cache */
    0,                                          /* tp_subclasses */
    0,                                          /* tp_weaklist */
    0,                                          /* tp_del */
    0,                                          /* tp_version_tag */
};

#if defined(NPY_PY3K)
static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "_bh",
        NULL,
        -1,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
};
#endif

#if defined(NPY_PY3K)
#define RETVAL m
PyMODINIT_FUNC PyInit__bh(void)
#else
#define RETVAL
PyMODINIT_FUNC init_bh(void)
#endif
{
    PyObject *m;
    Py_ssize_t i;

#if defined(NPY_PY3K)
    m = PyModule_Create(&moduledef);
#else
    m = Py_InitModule("_bh", NULL);
#endif
    if (m == NULL)
        return RETVAL;

    //Import NumPy
    import_array();

    BhArrayType.tp_base = &PyArray_Type;
    if (PyType_Ready(&BhArrayType) < 0)
        return RETVAL;

    PyObject *_info = PyImport_ImportModule("bohrium._info");
    if(_info == NULL)
        return RETVAL;

    //HACK: In order to force NumPy scalars on the left hand side of an operand to use Bohrium
    //we add all scalar types to the Method Resolution Order tuple.
    PyObject *dtypes = PyObject_GetAttrString(_info, "numpy_types");
    if(dtypes == NULL)
        return RETVAL;
    Py_ssize_t ndtypes = PyList_GET_SIZE(dtypes);
    Py_ssize_t old_size = PyTuple_GET_SIZE(BhArrayType.tp_mro);
    Py_ssize_t new_size = old_size + ndtypes;
    if(_PyTuple_Resize(&BhArrayType.tp_mro, new_size) != 0)
        return RETVAL;
    for(i=0; i<ndtypes; ++i)
    {
        PyObject *t = PyObject_GetAttrString(PyList_GET_ITEM(dtypes, i), "type");
        if(t == NULL)
            return RETVAL;
        PyTuple_SET_ITEM(BhArrayType.tp_mro, i+old_size, t);
    }
    Py_DECREF(_info);

    PyModule_AddObject(m, "ndarray", (PyObject *)&BhArrayType);

    ndarray = PyImport_ImportModule("bohrium.ndarray");
    if(ndarray == NULL)
        return RETVAL;
    ufunc = PyImport_ImportModule("bohrium.ufunc");
    if(ufunc == NULL)
        return RETVAL;
    bohrium = PyImport_ImportModule("bohrium");
    if(bohrium == NULL)
        return RETVAL;
    array_create = PyImport_ImportModule("bohrium.array_create");
    if(array_create == NULL)
        return RETVAL;

    //Initialize the signal handler
    bh_mem_signal_init();
    return RETVAL;
}
