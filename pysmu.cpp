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

    struct point{
        int x;
        int y;
    };

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
            session->add_device(&*i);
        }
        for(auto i: session->m_devices){
            auto dev = i;
            auto dev_info = dev->info();
            PyObject* dev_data = PyDict_New();
            for (unsigned chan=0; chan < dev_info->channel_count; chan++) {
                auto chan_info = dev->channel_info(chan);
                dev->set_mode(chan, 1);
                PyObject* sigs= PyList_New(0);

                for (unsigned sig=0; sig < chan_info->signal_count; sig++) {
                    auto info = dev->signal(chan, sig)->info()->label;
                    PyList_Append(sigs, PyString_FromString(string(info).c_str()));
                }
                PyDict_SetItem(dev_data, PyString_FromString(string(chan_info->label).c_str()), sigs);
            }
            PyList_Append(data, dev_data);
        }
        return data;
    }

    static PyObject* setMode(PyObject* self, PyObject* args){
        int dev_num;
        int chan_num;
        int mode_num;
        if (!PyArg_ParseTuple(args, "iii", &dev_num, &chan_num, &mode_num)) 
            {return PyString_FromString("Error");}
        int idx = 0;
        for (auto i: session->m_available_devices){
            if (idx == dev_num){
                i->set_mode((unsigned) chan_num, (unsigned) mode_num);
                break;
            }
            idx++;
        }
        return PyString_FromString("Success");
    }
    static PyObject* getInputs(PyObject* self, PyObject* args){
        int dev_num;
        int chan_num;
        int nsamples;
        if (!PyArg_ParseTuple(args, "iii", &dev_num, &chan_num, &nsamples)) 
            {return PyString_FromString("Error");}
        int idx = 0;
        for (auto i: session->m_available_devices){
            if (idx == dev_num){
                auto sgnl_v = i->signal(chan_num, 0);
                auto sgnl_i = i->signal(chan_num, 1);
                vector<float> buf_v;
                vector<float> buf_i;
                buf_v.resize(nsamples);
                buf_i.resize(nsamples);
                sgnl_v->measure_buffer(buf_v.data(), nsamples);
                sgnl_i->measure_buffer(buf_i.data(), nsamples);
                // sample rate fixed at 100k 
                session->configure(100000);
                session->run(nsamples);
                PyObject* samples = PyList_New(0);
                for(int i=0; i<nsamples; i++)
                    PyList_Append(samples, Py_BuildValue("(f,f)", buf_v[i], buf_i[i]));
                return samples;
            }
            idx++;
        }
        return PyString_FromString("Success");
    }

    static PyObject* setOutputWave(PyObject* self, PyObject* args){
        int dev_num;
        int chan_num;
        int mode;
        int wave;
        double duty;
        double period;
        double phase;
        double val1;
        double val2;

        if (!PyArg_ParseTuple(args, "iiiifffff", &dev_num, &chan_num, &mode, &wave, &val1, &val2, &period, &phase, &duty)) 
            {return PyString_FromString("Error");}
        int idx = 0;
        for (auto i: session->m_available_devices){
            if (idx == dev_num){
                auto sgnl =  i->signal(chan_num, 0);
                if (mode == SIMV)
                    sgnl = i->signal(chan_num, 1);
                if (wave == SRC_SQUARE)
                    sgnl->source_square(val1, val2, period, duty, phase);
                if (wave == SRC_SAWTOOTH)
                    sgnl->source_sawtooth(val1, val2, period, phase);
                if (wave == SRC_TRIANGLE)
                    sgnl->source_triangle(val1, val2, period, phase);
                if (wave == SRC_SINE)
                    sgnl->source_sine(val1, val2, period, phase);
            }
            idx++;
        }
        return PyString_FromString("Success");
    }

    static PyObject* setOutputConstant(PyObject* self, PyObject* args){
        int dev_num;
        int chan_num;
        int mode;
        float val;
        if (!PyArg_ParseTuple(args, "iiif", &dev_num, &chan_num, &mode, &val)) 
            {return PyString_FromString("Error");}
        int idx = 0;
        for (auto i: session->m_available_devices){
            if (idx == dev_num){
                if (mode == SVMI) {
                    auto sgnl_v = i->signal(chan_num, 0);
                    sgnl_v->source_constant(val);
                }
                if (mode == SIMV) {
                    auto sgnl_i = i->signal(chan_num, 1);
                    sgnl_i->source_constant(val);
                }
            }
            idx++;
        }
        return PyString_FromString("Success");
    }

    static PyMethodDef pysmu_methods [] = {
        { "setup", initSession, METH_VARARGS, "start session"  },
        { "get_dev_info", getDevInfo, METH_VARARGS, "get device information"  },
        { "cleanup", cleanupSession, METH_VARARGS, "end session"  },
        { "set_mode", setMode, METH_VARARGS, "set channel mode"  },
        { "get_inputs", getInputs, METH_VARARGS, "get measured voltage and current from a channel"  },
        { "set_output_constant", setOutputConstant, METH_VARARGS, "set channel output - constant"  },
        { "set_output_wave", setOutputWave, METH_VARARGS, "set channel output - wave"  },
        { NULL, NULL, 0, NULL  }
    };

    DL_EXPORT(void) initpysmu(void)
    {
        Py_InitModule("pysmu", pysmu_methods);
    }
}
