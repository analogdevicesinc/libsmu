#include <memory>
#include "libsmu.hpp"
#include <iostream>
#include <math.h>
#include "mex.h"

using namespace std;

static Session* session = NULL;

//*****************************************************************************
// M1K MEX layer functions 
//*****************************************************************************
static int initSession(void)
{
	session = new Session();
	
	return session->update_available_devices();
}

static int cleanupSession()
{
	session->end();	
	delete session;
	session = NULL;
	
	return 0;
}

static int getDevInfo(vector<string>& names)
{
	unsigned dev_i = 0;		
	for (auto i: session->m_available_devices) 
	{
		auto dev = session->add_device(&*i);
		auto dev_info = dev->info();

		for (unsigned ch_i = 0; ch_i < dev_info->channel_count; ch_i++) 
		{
			auto ch_info = dev->channel_info(ch_i);
			dev->set_mode(ch_i, 1);

			for (unsigned sig_i = 0; sig_i < ch_info->signal_count; sig_i++) 
			{
				auto sig = dev->signal(ch_i, sig_i);
				auto sig_info = sig->info();

				names.push_back(std::to_string(dev_i) + "." + string(ch_info->label) + "." + string(sig_info->label));			
			}
		}
		dev_i++;
	}

	return 0;
}

static int setMode(int dev_num, int chan_num, int mode_num)
{
	int idx = 0;	
	for (auto i: session->m_available_devices)
	{
		if (idx == dev_num)
		{
			i->set_mode((unsigned) chan_num, (unsigned) mode_num);
			break;
		}
		idx++;
	}
	return 0;
}

static int getInputs(int dev_num, int chan_num,	int nsamples, vector<float> &buf_v, vector<float> &buf_i)
{
	int idx = 0;	
	for (auto i: session->m_available_devices)
	{
		if (idx == dev_num)
		{
			auto sgnl_v = i->signal(chan_num, 0);
			auto sgnl_i = i->signal(chan_num, 1);
			
			buf_v.resize(nsamples);
			buf_i.resize(nsamples);
			sgnl_v->measure_buffer(buf_v.data(), nsamples);
			sgnl_i->measure_buffer(buf_i.data(), nsamples);
			
			// sample rate fixed at 100k 
			session->configure(100000);
			session->run(nsamples);
		}
		idx++;
	}
	return 0;
}

static int setOutputWave(int dev_num, int chan_num, int mode, int wave, 
						 double duty, double period, double phase, double val1, double val2)
{
    int idx = 0;	
	for (auto i: session->m_available_devices)
	{
		if (idx == dev_num)
		{
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
	return 0;
}

static int setOutputConstant(int dev_num, int chan_num, int mode, float val)
{
	int idx = 0;	
	for (auto i: session->m_available_devices)
	{
		if (idx == dev_num)
		{
			if (mode == SVMI) 
			{
				auto sgnl_v = i->signal(chan_num, 0);
				sgnl_v->source_constant(val);
			}
			if (mode == SIMV) 
			{
				auto sgnl_i = i->signal(chan_num, 1);
				sgnl_i->source_constant(val);
			}
		}
		idx++;
	}
	return 0;
}

//*****************************************************************************
// MEX functions selection 
//*****************************************************************************
static void libsmu_mex(int nlhs, mxArray *plhs[],
					   int nrhs, const mxArray *prhs[])
{
	char fname[256];
	mxGetString(prhs[0], fname, 256);
	
	if(session == NULL && strcmp(fname, "initSession"))
	{
		cout << "\nSession is NULL, cannot run " << fname << "\n";
	}
	else if(!strcmp(fname, "initSession"))
	{
		initSession();
	}
	else if(!strcmp(fname, "cleanupSession"))
	{
		cleanupSession();
	}
	else if(!strcmp(fname, "getDevInfo"))
	{
		vector<string> names;
		getDevInfo(names);
		
		string full_name;
		for (auto name: names) 
		{
			full_name += name + " ";
		}
		plhs[0] = mxCreateString(full_name.c_str());
	}
	else if(!strcmp(fname, "setMode"))
	{
		if (nrhs < 3) 
		{
			mexErrMsgIdAndTxt("MATLAB:libsmu_mex:nargin", 
				"setMode requires 3 input arguments.");
			return;
		}
		
		setMode(*mxGetPr(prhs[1]), *mxGetPr(prhs[2]), *mxGetPr(prhs[3]));
	}
	else if(!strcmp(fname, "getInputs"))
	{
		vector<float> buf_v; 
		vector<float> buf_i;
		double *pv, *pi;
		int index;
		
		buf_v.resize(*mxGetPr(prhs[3]));
		buf_i.resize(*mxGetPr(prhs[3]));
		
		if (nrhs < 3) 
		{
			mexErrMsgIdAndTxt("MATLAB:libsmu_mex:nargin", 
				"getInputs requires 3 input arguments.");
			return;
		}
		
		getInputs(*mxGetPr(prhs[1]), *mxGetPr(prhs[2]), *mxGetPr(prhs[3]),
				  buf_v, buf_i);				  
		
		/*for ( index = 0; index < *mxGetPr(prhs[3]); index++ ) 
		{
			buf_v[index] = index;
			buf_i[index] = index;
		}*/
		
		plhs[0] = mxCreateDoubleMatrix(1, buf_v.size(), mxREAL);
		plhs[1] = mxCreateDoubleMatrix(1, buf_i.size(), mxREAL);
		pv = mxGetPr(plhs[0]);
		pi = mxGetPr(plhs[1]);

		for ( index = 0; index < buf_v.size(); index++ ) 
		{
			pv[index] = buf_v[index];
			pi[index] = buf_i[index];
		}
	}
	else if(!strcmp(fname, "setOutputWave"))
	{
		if (nrhs < 9) 
		{
			mexErrMsgIdAndTxt("MATLAB:libsmu_mex:nargin", 
				"setOutputWave requires 8 input arguments.");
			return;
		} 
		
		setOutputWave(*mxGetPr(prhs[1]), *mxGetPr(prhs[2]), *mxGetPr(prhs[3]),
					  *mxGetPr(prhs[4]), *mxGetPr(prhs[5]), *mxGetPr(prhs[6]),
					  *mxGetPr(prhs[7]), *mxGetPr(prhs[8]), *mxGetPr(prhs[8]));
	}
	else if(!strcmp(fname, "setOutputConstant"))
	{
		setOutputConstant(*mxGetPr(prhs[1]), *mxGetPr(prhs[2]), 
						  *mxGetPr(prhs[3]), *mxGetPr(prhs[4]));
	}

	return;
}

//*****************************************************************************
// MEX function main entry point 
//*****************************************************************************
void mexFunction(int nlhs, mxArray *plhs[],
				 int nrhs, const mxArray *prhs[])
{
	// Check for proper number of arguments
	if (nrhs < 1) 
	{
		mexErrMsgIdAndTxt("MATLAB:libsmu_mex:nargin", 
			"libsmu_mex requires at least one input argument.");
		return;
	} 
	
	// Call the corresponding libsmu function
	libsmu_mex(nlhs, plhs, nrhs, prhs);

	return;
}
