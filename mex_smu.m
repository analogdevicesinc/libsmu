clear all;
close all;

% Generate the MEX file
mex libsmu_mex.cpp device_m1000.cpp device_cee.cpp session.cpp signal.cpp libusb-1.0.lib

% Initialize the the libsmu session 
libsmu_mex('initSession');
libsmu_mex('getDevInfo');

libsmu_mex('setOutputConstant',  ...
 			0, ... 		% dev_num
 			0, ... 		% chan_num
 			1, ...      % mode
 			5.0);		% buf

% Read and plot the input data
[v, i] = libsmu_mex('getInputs', ...
					0, ...  % dev_num 
					0, ...	% chan_num	
					100); 	% nsamples
figure; hold on; plot(v, '-r'); plot(i, '-b'); grid; hold off; legend('Voltage', 'Current');

% Close the libsmu session
libsmu_mex('cleanupSession');
