#include"libsmu.hpp"
#include <Python.h>
#include <iostream>

#include <memory>
#include <vector>
#include <cstdint>
#include <string>

using std::cout;
using std::cerr;
using std::endl;
using std::unique_ptr;
using std::vector;
using std::string;

Session* session;

extern "C" {
    static PyObject* initSession(PyObject* self, PyObject* args){
        //Session* x = new Session();
        //cpp_initSession();
        session = new Session();
        int good = session->update_available_devices();//cpp_checkAvailable();
        PyObject* ret;
        if (good == 0){
            ret = PyString_FromString("Success");
        }
        else{
            ret = PyString_FromString("Error");
        }
        return ret;
    }

    static PyObject* cleanupSession(PyObject* self, PyObject* args){
        session->end();
        return PyInt_FromLong(0l);
    }

    static PyObject* getDevInfo(PyObject* self, PyObject* args){
        PyObject* data = PyList_New(0);
        for (auto i: session->m_available_devices){
            auto dev = session->add_device(&*i);
            auto dev_info = dev->info();
            PyObject* dev_data = PyDict_New();
            for (unsigned chan=0; chan < dev_info->channel_count; chan++) {
                auto chan_info =dev->channel_info(chan);
                dev->set_mode(chan, 1);
                PyObject* sigs= PyList_New(0);
                for (unsigned sig=0; sig < chan_info->signal_count; sig++) {
                    auto info = dev->signal(chan, sig)->info()->label;
                    PyList_Append(sigs, PyString_FromString(string(info).c_str()));
                }
                PyDict_SetItem(
                    dev_data, 
                    PyString_FromString(string(chan_info->label).c_str()),
                    sigs);
            }
            PyList_Append(data, dev_data);
        }
        return data;
    }


    static PyMethodDef HelloMethods[] = {
        { "setup", initSession, METH_VARARGS, "start session"  },
        { "get_dev_info", getDevInfo, METH_VARARGS, "get device information"  },
        { "cleanup", cleanupSession, METH_VARARGS, "end session"  },
        { NULL, NULL, 0, NULL  }
    };

    DL_EXPORT(void) initlibsmu(void)
    {
        Py_InitModule("libsmu", HelloMethods);
    }
};

