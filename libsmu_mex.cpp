/***************************************************************************//**
 *   @file   libsmu_mex.cpp
 *   @brief  Implementation of libsmu MEX layer
********************************************************************************
 * Copyright 2013(c) MathWorks, Inc.
 *
 *
********************************************************************************
 *  This file implements all the functions needed to work with the M1K devices
 *	from Matlab. 
 *
 *	Connected devices are initialized by calling the initSession function
 * 	which starts a new work session, followed by a call to getDevInfo. 
 *  To release the device the cleanupSession function must be called 
 *  at the end of a work session. 
 *	
 *	All the return data buffers are allocated inside the data read functions. 
 *	The caller of the read function is responsible for clearing the return 
 *	buffer. 
 *
 *	For additional info about libsmu see: https://github.com/signalspec/libsmu
*******************************************************************************/

#include <memory>
#include "libsmu.hpp"
#include <iostream>
#include <math.h>
#include "mex.h"

using namespace std;

static Session* session = NULL; /*!< Global session variable */
static const uint64_t SAMPLE_RATE = 100000; /*!< M1K sampling rate */
static vector<float> buf_v_async; /*!< Buffer to store voltage input data for async operations */
static vector<float> buf_i_async; /*!< Buffer to store current input data for async operations */
string matlab_progress_callback; /*!< Matlab progress callback function name for async operations */
string matlab_completion_callback; /*!< Matlab completion callback function name for async operations */

/**
 * lockMex
 *
 * Locks the mex to prevent from clearing the mex inadvertently
 *
 * @return None
 */
void lockMex(void)
{
	if(!mexIsLocked())
		mexLock();
}

/**
 * unlockMex
 *
 * Unlocks the mex to allow clearing the mex
 *
 * @return None
 */
void unlockMex(void)
{
	if(mexIsLocked())
		mexUnlock();
}

/**
 * initSession
 *
 * Initializes a libsmu session
 *
 * Function example: 
 *		ret = initSession()
 *		
 * @return Returns 0 for success or negative error code
 */
static int initSession(void)
{
	if(session == NULL)
	{
		session = new Session();
	}	
	
	return session->update_available_devices();
}

/**
 * cleanupSession
 *
 * Closes the active libsmu session
 *
 * Function example: 
 *		ret = cleanupSession()
 *
 * @return Returns 0 for success or negative error code
 */
static int cleanupSession()
{
	if(session)
	{
		session->end();	
		delete session;
		session = NULL;
	}
	
	return 0;
}

/**
 * getDevInfo
 *
 * Reads the device, channels and signals information. 
 *
 * Function example: 
 *		devInfo = getDevInfo()
 *
 * @param  dev_info Vector of sl_device_info structures
 * @param  ch_info Vector of sl_channel_info structures
 * @param  sig_info Vector of sl_signal_info structures
 *
 * @return Returns 0 for success or negative error code
 */
static int getDevInfo(vector<sl_device_info>& dev_info, 
					  vector<sl_channel_info>& ch_info, 
					  vector<sl_signal_info>& sig_info)
{	
	for (auto i: session->m_available_devices) 
	{
		auto dev = session->add_device(&*i);
		auto info_dev = dev->info();
		dev_info.push_back(*info_dev);

		for (unsigned ch_i = 0; ch_i < info_dev->channel_count; ch_i++) 
		{
			auto info_ch = dev->channel_info(ch_i);
			ch_info.push_back(*info_ch);

			for (unsigned sig_i = 0; sig_i < info_ch->signal_count; sig_i++) 
			{
				auto sig = dev->signal(ch_i, sig_i);
				auto info_sig = sig->info();
				sig_info.push_back(*info_sig);			
			}
		}
	}

	return 0;
}

/**
 * setMode
 *
 * Sets the operating mode for a specific device channel
 *
 * Function example: 
 *		ret = setMode(0,...	 % dev_num 
 *					  0,...  % chan_num
 *					  SVMI)  % mode
 *
 * @param  dev_num Device ID
 * @param  chan_num Channel ID
 * @param  mode Channel mode - see the Modes enumeration from libsmu.hpp
 * 
 * @return Returns 0 for success or negative error code
 */
static int setMode(int dev_num, int chan_num, int mode)
{
	int idx = 0;	
	for (auto i: session->m_available_devices)
	{
		if (idx == dev_num)
		{
			i->set_mode((unsigned) chan_num, (unsigned) mode);
			
			return 0;
		}
		idx++;
	}
	return -1;
}

/**
 * getInputs
 *
 * Reads an array of voltage and current samples from a specific device channel
 * The function returns only after all the samples are read.
 *
 * Function example:
 * 		[V, I] = getInputs(0,...   % dev_num
  						   0,...   % chan_num	
 						   1024)   % nsamples 
 *
 * @param  dev_num Device ID
 * @param  chan_num Channel ID
 * @param  nsamples Number of samples to read
 * @param  buf_v Vector to store the voltage readings
 * @param  buf_i Vector to store the current readings
 *
 * @return Returns 0 for success or negative error code
 */
static int getInputs(int dev_num, int chan_num,	int nsamples, 
					 vector<float> &buf_v, vector<float> &buf_i)
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
			
			session->configure(SAMPLE_RATE);
			session->run(nsamples);
			
			return 0;
		}
		idx++;
	}
	return -1;
}

/**
 * getInputsAsync
 *
 * Reads an array of voltage and current samples from a specific device channel.
 * The function returns immediately and does not wait for the data transfer to finish.
 * The data read progress is signalled by calling the provided progress callback function. 
 * The data read completion is signalled by calling the provided completion callback function.
 *
 * Function example:
 * 		[V, I] = getInputsAsync(0,...      		  	 % dev_num
 * 						     	0,...      		  	 % chan_num	
 *						     	1024,...   		  	 % nsamples
 *							 	progressCallback,... % progress_callback
 *							 	completionCallback)  % completion_callback
 *
 * @param  dev_num Device ID
 * @param  chan_num Channel ID
 * @param  nsamples Number of samples to read
 * @param  buf_v Vector to store the voltage readings
 * @param  buf_i Vector to store the current readings
 * @param  progress_callback Progress callback function. This function is called every time 
 *							 a new chunk of data is read from the device. The supplied 
 *							 parameter contains the number of samples read so far.
 * @param completion_callback Completion callback function. This function is called when the 
 *							  data transfer is complete.	
 *
 * @return Returns 0 for success or negative error code
 */
static int getInputsAsync(int dev_num, int chan_num, int nsamples, 
						  vector<float> &buf_v, vector<float> &buf_i,
						  void(*progress_callback)(sample_t),
						  void(*completion_callback)())
{
	int idx = 0;	
	for (auto i: session->m_available_devices)
	{
		if (idx == dev_num)
		{
			if(progress_callback)
				session->m_progress_callback = progress_callback;
			if(completion_callback)
				session->m_completion_callback = completion_callback;

			auto sgnl_v = i->signal(chan_num, 0);
			auto sgnl_i = i->signal(chan_num, 1);
			
			buf_v.resize(nsamples);
			buf_i.resize(nsamples);
			
			sgnl_v->measure_buffer(buf_v.data(), nsamples);
			sgnl_i->measure_buffer(buf_i.data(), nsamples);
			
			session->configure(SAMPLE_RATE);
			session->start(nsamples);
			
			return 0;
		}
		idx++;
	}
	return -1;
}

/**
 * setOutputWave
 *
 * Sets the output waveform for a specific device channel
 *
 * Function example:
 * 		ret = setOutputWave(0,...			% dev_num 
 *							0,... 			% chan_num 
 *							SIMV,... 		% mode
 *							SRC_SQUARE,...  % wave 
 *							0.5,..			% duty 
 *							10,..			% period 
 *							0,...			% phase 
 *							-100,...		% min 
 *							100)			% max
 *
 * @param  dev_num Device ID
 * @param  chan_num Channel ID
 * @param  mode Channel mode - see the Modes enumeration from libsmu.hpp 
 * @param  wave Output waveform type - see the Src enumeration from libsmu.hpp
 * @param  duty Duty cycle of a square output waveform, range [0, 1] 
 * @param  period Waveform period in ms, range [0, 1]
 * @param  phase Waveform phase in rad, range [0, 2*PI]
 * @param  min Waveform minimum value, range [XX, XX]
 * @param  max Waveform maximum value, range [XX, XX]
 *
 * @return Returns 0 for success or negative error code
 */
static int setOutputWave(int dev_num, int chan_num, int mode, 
						 int wave, double duty, 
						 double period, double phase, 
						 double min, double max)
{
    int idx = 0;	
	for (auto i: session->m_available_devices)
	{
		if (idx == dev_num)
		{
			setMode(dev_num, chan_num, mode);
			
			auto sgnl =  i->signal(chan_num, 0);
			if (mode == SIMV)
				sgnl = i->signal(chan_num, 1);
			if (wave == SRC_SQUARE)
				sgnl->source_square(min, max, period, duty, phase);
			if (wave == SRC_SAWTOOTH)
				sgnl->source_sawtooth(min, max, period, phase);
			if (wave == SRC_TRIANGLE)
				sgnl->source_triangle(min, max, period, phase);
			if (wave == SRC_SINE)
				sgnl->source_sine(min, max, period, phase);
		
			return 0;
		}
		idx++;
	}
	return -1;
}

/**
 * setOutputConstant
 *
 * Function example:
 * 		ret = setOutputConstant(0,... 	 % dev_num 
 *						   		0,... 	 % chan_num 
 *						   		SIMV,... % mode
 *						   		100); 	 % val  
 *
 * Sets an output constant value for a specific device channel
 *
 * @param  dev_num Device ID
 * @param  chan_num Channel ID
 * @param  mode Channel mode - see the Modes enumeration from libsmu.hpp 
 * @param  val Value to be set at the output, range [XX, XX]
 *
 * @return Returns 0 for success or negative error code
 */
static int setOutputConstant(int dev_num, int chan_num, int mode, float val)
{
	int idx = 0;	
	for (auto i: session->m_available_devices)
	{
		if (idx == dev_num)
		{
			setMode(dev_num, chan_num, mode);
			
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
			
			return 0;
		}
		idx++;
	}
	
	return -1;
}

/**
 * setOutputArbitrary
 *
 * Sets an arbitrary output for a specific device channel 
 *
 * Example:
 *		data_buf = ones(1, 1024); 
 *		ret = setOutputArbitrary(0,... 		% dev_num 
 *								 0,... 		% chan_num 
 *								 SIMV,... 	% mode
 *								 1,... 		% repeat
 *								 1024,...	% buf_len
 *								 data_buf);	% buf
 * 
 * @param  dev_num Device ID
 * @param  chan_num Channel ID
 * @param  mode Channel mode - see the Modes enumeration from libsmu.hpp 
 * @param  repeat Set to 1 to continuously repeat the data sequence, 
 *				  set to 0 otherwise
 * @param  buf_len Length of arbitrary output data array
 * @param  buf Buffer containing the arbitrary output data
 *
 * @return Returns 0 for success or negative error code
 */
static int setOutputArbitrary(int dev_num, int chan_num, int mode, 
							  int repeat, size_t buf_len, double* buf)
{
	int idx = 0;
	
	for (auto i: session->m_available_devices)
	{
		if (idx == dev_num)
		{
			// Copy the input buffer to a float buffer
			float* dev_buf = (float*)(malloc(sizeof(float) * buf_len));
			for(int i = 0; i < buf_len; i++)
			{
				dev_buf[i] = (float)buf[i];
			}
			
			// Set the arbitrary data
			setMode(dev_num, chan_num, mode);
			auto sgnl = i->signal(chan_num, mode == SVMI ? 0 : 1);
			sgnl->source_buffer(dev_buf, buf_len, (repeat > 0));
			
			free(dev_buf);
			return 0;
		}
		idx++;
	}
	return -1;
}

/**
 * getInputsHelper
 *
 * Reads a batch of samples from a specific device channel and stores the data
 * in a Matlab compatible array
 *
 * @param  plhs[0] Buffer to store the read voltage samples
 * @param  plhs[1] Buffer to store the read current samples
 * @param  prhs[1] Device number
 * @param  prhs[2] Channel number
 * @param  prhs[3] Number of samples to read
 *
 * @return Returns 0 for success or negative error code 
 */
static int getInputsHelper(mxArray *plhs[], const mxArray *prhs[])
{
	vector<float> buf_v; 
	vector<float> buf_i;
	double *pv, *pi;
	int index;
	
	buf_v.resize(*mxGetPr(prhs[3]));
	buf_i.resize(*mxGetPr(prhs[3]));
	
	int ret = getInputs(*mxGetPr(prhs[1]), 
						*mxGetPr(prhs[2]), 
						*mxGetPr(prhs[3]),
						buf_v, buf_i);				  
	
	if(ret < 0)
		return ret;
	
	plhs[0] = mxCreateDoubleMatrix(1, buf_v.size(), mxREAL);
	plhs[1] = mxCreateDoubleMatrix(1, buf_i.size(), mxREAL);
	pv = mxGetPr(plhs[0]);
	pi = mxGetPr(plhs[1]);

	for(index = 0; index < buf_v.size(); index++) 
	{
		pv[index] = buf_v[index];
		pi[index] = buf_i[index];
	}
	
	return ret;
}

/**
 * progress_callback
 *
 * Progress callback for async operations
 *
 * @param sample Total number of samples already received from the device 
 *
 * @return None 
 */
static void progress_callback(sample_t sample)
{
	mexPrintf("progress_callback\n");			
	
	int index;
	double *pv, *pi;
	mxArray* prhs[3];
	
	prhs[0] = mxCreateDoubleMatrix(1, buf_v_async.size(), mxREAL);
	prhs[1] = mxCreateDoubleMatrix(1, buf_i_async.size(), mxREAL);
	prhs[2] = mxCreateDoubleMatrix(1, 1, mxREAL);
	pv = mxGetPr(prhs[0]);
	pi = mxGetPr(prhs[1]);
	*mxGetPr(prhs[2]) = sample;

	for(index = 0; index < sample; index++) 
	{
		pv[index] = buf_v_async[index];
		pi[index] = buf_i_async[index];
	}
	
	mexCallMATLAB(0, NULL, 3, prhs, matlab_progress_callback.c_str());
}

/**
 * completion_callback
 *
 * Completion callback for async operations 
 *
 * @return None 
 */
static void completion_callback()
{
	mexPrintf("completion_callback\n");
	
	int index;
	double *pv, *pi;
	mxArray* prhs[2];
	
	prhs[0] = mxCreateDoubleMatrix(1, buf_v_async.size(), mxREAL);
	prhs[1] = mxCreateDoubleMatrix(1, buf_i_async.size(), mxREAL);
	pv = mxGetPr(prhs[0]);
	pi = mxGetPr(prhs[1]);

	for(index = 0; index < buf_v_async.size(); index++) 
	{
		pv[index] = buf_v_async[index];
		pi[index] = buf_i_async[index];
	}
	
	mexCallMATLAB(0, NULL, 2, prhs, matlab_completion_callback.c_str());
}

/**
 * getInputsAsyncHelper
 *
 * Reads a batch of samples from a specific device channel and stores the data
 * in a Matlab compatible array
 *
 * @param  plhs[0] Buffer to store the read voltage samples
 * @param  plhs[1] Buffer to store the read current samples
 * @param  prhs[1] Device number
 * @param  prhs[2] Channel number
 * @param  prhs[3] Number of samples to read
 *
 * @return Returns 0 for success or negative error code 
 */
static int getInputsAsyncHelper(mxArray *plhs[], const mxArray *prhs[])
{
	buf_v_async.resize(*mxGetPr(prhs[3]));
	buf_i_async.resize(*mxGetPr(prhs[3]));
	
	matlab_progress_callback.assign(mxArrayToString(prhs[4]));
	matlab_completion_callback.assign(mxArrayToString(prhs[5]));
	
	int ret = getInputsAsync(*mxGetPr(prhs[1]), 
							 *mxGetPr(prhs[2]), 
							 *mxGetPr(prhs[3]),
							 buf_v_async, buf_i_async,
							 matlab_progress_callback.empty() ? 0 : progress_callback,
							 matlab_completion_callback.empty() ? 0 : completion_callback);				  
	
	if(ret < 0)
		return ret;
	
	return ret;
}

/**
 * getDevInfoHelper
 *
 * Reads the device number of channels and channels types and stores the info
 * in a Matlab compatible array
 *
 * @param  plhs[0] Array to store the device info
 * @param  plhs[1] Array to store the channel info
 * @param  plhs[2] Array to store the signal info
 *
 * @return Returns 0 for success or negative error code
 */
static int getDevInfoHelper(mxArray *plhs[])
{
	int ret;
    mwSize dims[] = {1,1};
	mxArray* fout[16];
	mxArray* chi_st;
	mxArray* sigi_st;
	int ch_idx = 0, sig_idx = 0;
	vector<sl_device_info> devi;
	vector<sl_channel_info> chi; 
	vector<sl_signal_info> sigi;
	const char* fname_dev_info[]     = {"Type", "Label", "SerialNum", 
										"ChannelCount", "ChannelInfo"};
	const char* fname_channel_info[] = {"Type", "Label", "ModeCount", 
										"SignalCount", "SignalInfo"};
	const char* fname_signal_info[]  = {"Type", "Label", 
										"InputModes", "OutputModes", 
									    "Unit", "Min", "Max", "Resolution"};
	
	ret = getDevInfo(devi, chi, sigi);
	
	if(ret < 0)
		return ret;
	
	plhs[0] = mxCreateStructMatrix(1, devi.size(), sizeof(fname_dev_info) / sizeof(char*), fname_dev_info);
	
	for(int i = 0; i < devi.size(); i++)
	{
		fout[0] = mxCreateString(devi[i].type == DEVICE_M1000 ? "ADALM1000" : "UNKNOWN");
		fout[1] = mxCreateString(devi[i].label);
		fout[2] = mxCreateString("12345678");
		fout[3] = mxCreateNumericArray(2, dims, mxDOUBLE_CLASS, mxREAL);
		fout[4] = mxCreateStructMatrix(1, devi[i].channel_count, sizeof(fname_channel_info) / sizeof(char*), fname_channel_info);
		chi_st = fout[4];
		
		*((double*)mxGetData(fout[3])) = devi[i].channel_count;
        
        for(int j = 0; j < sizeof(fname_dev_info) / sizeof(char*); j++)
		{
			mxSetFieldByNumber(plhs[0], i, j, fout[j]);
		}
	
		for(int j = 0; j < devi[i].channel_count; j++)
		{
			fout[0] = mxCreateString(chi[j + ch_idx].type == CHANNEL_SMU ? "CHANNEL_SMU" : "UNKNOWN");
			fout[1] = mxCreateString(chi[j + ch_idx].label);
			fout[2] = mxCreateNumericArray(2, dims, mxDOUBLE_CLASS, mxREAL);
			fout[3] = mxCreateNumericArray(2, dims, mxDOUBLE_CLASS, mxREAL);
			fout[4] = mxCreateStructMatrix(1, chi[j + ch_idx].signal_count, sizeof(fname_signal_info) / sizeof(char*), fname_signal_info);
			sigi_st = fout[4];
			
			*((double*)mxGetData(fout[2])) = chi[j + ch_idx].mode_count;
			*((double*)mxGetData(fout[3])) = chi[j + ch_idx].signal_count;
			
			for(int l = 0; l < sizeof(fname_channel_info) / sizeof(char*); l++)
			{
				mxSetFieldByNumber(chi_st, j, l, fout[l]);
			}
    
			for(int k = 0; k < chi[j + ch_idx].signal_count; k++)
			{
				fout[0] = mxCreateString(sigi[k + sig_idx].type == SIGNAL ? "SIGNAL" : "UNKNOWN");
				fout[1] = mxCreateString(sigi[k + sig_idx].label);
				fout[2] = mxCreateNumericArray(2, dims, mxDOUBLE_CLASS, mxREAL);
				fout[3] = mxCreateNumericArray(2, dims, mxDOUBLE_CLASS, mxREAL);
				fout[4] = mxCreateString(!memcmp(&sigi[k + sig_idx].unit, &unit_V, sizeof(unit_V)) ? "V" : "A");
				fout[5] = mxCreateNumericArray(2, dims, mxDOUBLE_CLASS, mxREAL);
				fout[6] = mxCreateNumericArray(2, dims, mxDOUBLE_CLASS, mxREAL);
				fout[7] = mxCreateNumericArray(2, dims, mxDOUBLE_CLASS, mxREAL);
				
				*((double*)mxGetData(fout[2])) = sigi[k + sig_idx].inputModes;
				*((double*)mxGetData(fout[3])) = sigi[k + sig_idx].outputModes;		
				*((double*)mxGetData(fout[5])) = sigi[k + sig_idx].min;
				*((double*)mxGetData(fout[6])) = sigi[k + sig_idx].max;
				*((double*)mxGetData(fout[7])) = sigi[k + sig_idx].resolution;
				
				for(int l = 0; l < sizeof(fname_signal_info) / sizeof(char*); l++)
				{
					mxSetFieldByNumber(sigi_st, k, l, fout[l]);
				}
			}			
			sig_idx += chi[j + ch_idx].signal_count;
		}
		ch_idx += devi[i].channel_count;
	}
	
	return ret;
}

/**
 * libsmu_mex
 *
 * Stores a return value into a Matlab supplied variable
 *
 * @param  nlhs Number of output arguments
 * @param  plhs Array of output arguments
 * @param  ret	Return value
 *
 * @return None
 */
void setRetunValue(int nlhs, mxArray *plhs[], int ret)
{
	if(nlhs != 1)
		return;
	
	plhs[0] = mxCreateDoubleMatrix(1, 1, mxREAL);	
	*mxGetPr(plhs[0]) = (double)ret;
}

/**
 * libsmu_mex
 *
 * Selects the mex function to be executed.
 *
 * @param  nlhs Number of output arguments
 * @param  plhs Array of output arguments
 * @param  nrhs Number of input arguments
 * @param  prhs Array of input arguments
 *
 * @return None
 */
static void libsmu_mex(int nlhs, mxArray *plhs[],
					   int nrhs, const mxArray *prhs[])
{
	int ret;
	char fname[256];
	mxGetString(prhs[0], fname, 256);
	
	if(session == NULL && strcmp(fname, "initSession"))
	{
		cout << "\nSession is NULL, cannot run " << fname << "\n";
	}
	else if(!strcmp(fname, "lockMex"))
	{
		lockMex();
	}
	else if(!strcmp(fname, "unlockMex"))
	{
		unlockMex();
	}
	else if(!strcmp(fname, "initSession"))
	{
		ret = initSession();
		setRetunValue(nlhs, plhs, ret);
	}
	else if(!strcmp(fname, "cleanupSession"))
	{
		ret = cleanupSession();
		setRetunValue(nlhs, plhs, ret);
	}
	else if(!strcmp(fname, "getDevInfo"))
	{
		getDevInfoHelper(plhs);
	}
	else if(!strcmp(fname, "setMode"))
	{
		if (nrhs < 3) 
		{
			mexErrMsgIdAndTxt("MATLAB:libsmu_mex:nargin", 
				"setMode requires 3 input arguments.");
			return;
		}		
		ret = setMode(*mxGetPr(prhs[1]), *mxGetPr(prhs[2]), *mxGetPr(prhs[3]));
		setRetunValue(nlhs, plhs, ret);
	}
	else if(!strcmp(fname, "getInputs"))
	{
		if (nrhs < 3) 
		{
			mexErrMsgIdAndTxt("MATLAB:libsmu_mex:nargin", 
				"getInputs requires 3 input arguments.");
			return;
		}
		getInputsHelper(plhs, prhs);
	}
	else if(!strcmp(fname, "getInputsAsync"))
	{
		if (nrhs < 5) 
		{
			mexErrMsgIdAndTxt("MATLAB:libsmu_mex:nargin", 
				"getInputsAsync requires 5 input arguments.");
			return;
		}
		getInputsAsyncHelper(plhs, prhs);
	}
	else if(!strcmp(fname, "setOutputWave"))
	{
		if (nrhs < 9) 
		{
			mexErrMsgIdAndTxt("MATLAB:libsmu_mex:nargin", 
				"setOutputWave requires 8 input arguments.");
			return;
		} 		
		ret = setOutputWave(*mxGetPr(prhs[1]), *mxGetPr(prhs[2]), *mxGetPr(prhs[3]),
							*mxGetPr(prhs[4]), *mxGetPr(prhs[5]), *mxGetPr(prhs[6]),
							*mxGetPr(prhs[7]), *mxGetPr(prhs[8]), *mxGetPr(prhs[8]));
		setRetunValue(nlhs, plhs, ret);
	}
	else if(!strcmp(fname, "setOutputConstant"))
	{
		if (nrhs < 5) 
		{
			mexErrMsgIdAndTxt("MATLAB:libsmu_mex:nargin", 
				"setOutputConstant requires 4 input arguments.");
			return;
		}
		ret = setOutputConstant(*mxGetPr(prhs[1]), *mxGetPr(prhs[2]), 
								*mxGetPr(prhs[3]), *mxGetPr(prhs[4]));
		setRetunValue(nlhs, plhs, ret);
	}
	else if(!strcmp(fname, "setOutputArbitrary"))
	{
		if (nrhs < 6) 
		{
			mexErrMsgIdAndTxt("MATLAB:libsmu_mex:nargin", 
				"setOutputArbitrary requires 5 input arguments.");
			return;
		}
		ret = setOutputArbitrary(*mxGetPr(prhs[1]), *mxGetPr(prhs[2]), 
								 *mxGetPr(prhs[3]), *mxGetPr(prhs[4]),
								 mxGetNumberOfElements(prhs[5]), mxGetPr(prhs[5]));
		setRetunValue(nlhs, plhs, ret);
	}

	return;
}

/**
 * mexFunction
 *
 * MEX layer main entry point
 *
 * @param  nlhs Number of input arguments
 * @param  plhs Array of input arguments
 * @param  nrhs Number of output arguments
 * @param  prhs Array of output arguments
 *
 * @return None
 */
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
