%
%  Copyright (C) 2016-2017 Jose Maria Ortega\n
%  Distributed under the GNU GPLv3. For full terms see the file LICENSE
%
% PURPOSE
%  Shorcuts to the most typical operations within the 'plc-cape' project
%

%
% plc_shortcuts_plot(test_id)
% test_id:
%   "adc": adc.csv + FFT
%   "adcN": adc<N>.csv
%   "adc_filter": adc_filter.csv
%   "adc+adc_filter": adc.csv + adc_filter.csv
%   "adc+adc_filter+adc_data": adc.csv + adc_filter.csv + adc_data.csv
%   "adc_fft": adc_fft.csv
%   "adc+demod": adc.csv + Octave filtering.
%     Needs two optional parameters:
%     * band-pass center filter frequency
%     * demodulation frequency
%   "predef_seq_sync+demod": hard-coded data + Octave demodulation.
%     It applies an OOK pattern to a continuos carrier
%   "predef_seq_async+demod": hard-coded data + Octave demodulation.
%     Each OOK symbol starts with a zero-phase carrier
%
function plc_shortcuts_plot(test_id, varargin)
  global plc_base_dir;
  global plc_adc_sampling_freq;
  switch (test_id)
    case "adc"
      in = transpose(load([plc_base_dir "/adc.csv"]));
      plc_plot_adc_capture('adc.csv', 2, in);
    case "adcN"
      adc_captures = 4;
      figure(2);
      for i = 0:adc_captures-1
        adc_filename = sprintf("%s/adc%d.csv", plc_base_dir, i);
        in = transpose(load(adc_filename));
        in_len = length(in);
        subplot(adc_captures,1,i+1)
        plot(in)
        axis([1, length(in)]);
      endfor
    case "adc_filter"
      in = transpose(load([plc_base_dir "/adc_filter.csv"]));
    plc_plot_adc_capture('adc_filter.csv', 3, in);
    case "adc+adc_filter"
      in = transpose(load([plc_base_dir "/adc.csv"]));
    plc_plot_adc_capture('adc.csv', 2, in);
      in = transpose(load([plc_base_dir "/adc_filter.csv"]));
    plc_plot_adc_capture('adc_filter.csv', 3, in);
    case "adc+adc_filter+adc_data"
      figure(4);
      in = transpose(load([plc_base_dir "/adc.csv"]));
      subplot(3,1,1)
      plot(in)
      in = transpose(load([plc_base_dir "/adc_filter.csv"]));
      subplot(3,1,2)
      plot(in)
      axis([1, length(in)]);
      in = transpose(load([plc_base_dir "/adc_data.csv"]));
      % bar, barh, stairs are other interesting graphs
      subplot(3,1,3)
      stem(in) 
      axis([1, length(in)]);
    case "adc_fft"
      in = transpose(load([plc_base_dir "/adc_fft.csv"]));
      plc_plot_seq('adc_fft.csv', 'Index', 'Value', in);
    case "adc+demod"
      if (length (varargin) < 2)
        printf(["Please specify required parameters:\n" ...
       " 1.Band-pass filter center frequency (Hz): 0 skips it\n" ...
       " 2.Demodulation frequency (Hz): 0 means demodulation by 'abs()'\n"]);
        return;
      endif
      in = transpose(load([plc_base_dir "/adc.csv"]));
      plc_plot_adc_capture_and_demod('adc.csv', in, varargin{1}, varargin{2});
    case "predef_seq_sync+demod"
      f_carrier_hz = 90000;
      seq = plc_generate_predef_sequence_sync(f_carrier_hz);
      % To use 'demod_by_abs' specify f_demod = 0
      % plc_plot_adc_capture_and_demod(seq, 0);
      plc_plot_adc_capture_and_demod('Predef sync sequence', seq, f_carrier_hz, f_carrier_hz);
    case "predef_seq_async+demod"
      f_carrier_hz = 90000;
      seq = plc_generate_predef_sequence_async(f_carrier_hz);
      plc_plot_adc_capture_and_demod('Predef async sequence', seq, f_carrier_hz, 0);
    otherwise
      printf("Unexpected 'test_id' type '%s'\n", test_id);
  endswitch
endfunction
