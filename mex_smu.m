clear all;
close all;

% Generate the MEX file
mex libsmu_mex.cpp device_m1000.cpp device_cee.cpp session.cpp signal.cpp libusb-1.0.lib

% Initialize the the libsmu session 
libsmu_mex('initSession');

% Get the available channels
channel_names = libsmu_mex('getDevInfo');
if(isempty(channel_names))
	disp('Could not find any valid channels!');
	return;
else
	disp('M1K channels: '); disp(channel_names);
end

% Set the output waveform
libsmu_mex('setOutputWave',  ...
			0, ... 		% dev_num
			0, ... 		% chan_num
			262154, ... % mode
			1, ...		% wave
			50, ...		% duty
			1, ...		% period
			0, ...		% phase
			0, ...		% val1	
			100);		% val2 

% Read and plot the input data
[v, i] = libsmu_mex('getInputs', ...
					0, ...  % dev_num 
					0, ...	% chan_num	
					100); 	% nsamples
figure; hold on; plot(v, '-r'); plot(i, '-b'); grid; hold off; legend('Voltage', 'Current');

% Close the libsmu session
libsmu_mex('cleanupSession');
