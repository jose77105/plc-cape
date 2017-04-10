% PURPOSE
%  Display results of different tests within the 'plc-cape' project
%
% REMARKS
% Interesting info on plotting control here:
%   https://www.gnu.org/software/octave/doc/v4.0.1/Plot-Annotations.html
%   https://www.gnu.org/software/octave/doc/v4.0.0/Axes-Properties.html
%
% Main function: execute_test(test_id, base_dir)
% test_id:
%   "adc": adc.csv + FFT
%   "adc+demod": adc.csv + Octave filtering
%   "predef_seq+demod": hard-coded data + Octave filtering
%   "adcN": adc<N>.csv
%   "adc_filter": adc_filter.csv
%   "adc+adc_filter+adc_data": adc.csv + adc_filter.csv + adc_data.csv
% base_dir:
%	Linux example: "/media/y/applications/plc-cape-lab"
%	Windows example: "Y:/applications/plc-cape-lab"

global adc_sampling_freq = 200000;

function seq = generate_predef_sequence(period)
  data=[0, 1, 0, 1, 1, 1, 0, 1];
  seq_len = 1024;
  assert(mod(seq_len, length(data)) == 0);
  seq_symbol_len = seq_len/length(data);
  seq = sinewave(seq_len,period);
  data_index = 0;
  for i = 1:seq_len
    if (mod(i, seq_symbol_len) == 1)
      data_index++;
    endif
    seq(i) = seq(i)*data(data_index);
  endfor
endfunction

function plot_seq(seq, title_arg, xlabel_arg, ylabel_arg)
  plot(seq);
  axis([1, length(seq)]);
  title(title_arg); xlabel(xlabel_arg); ylabel(ylabel_arg);
endfunction

function f_max = plot_seq_and_display_max(seq)
  figure(1);
  subplot(1,1,1)
  plot_seq(seq)
  [seq_max_value seq_max_index] = max(seq);
  printf ("Sequence max = %f at index %u (1-based)\n", seq_max_value, seq_max_index);
endfunction

function f_max = plot_seq_and_fft(seq, figure_num)
  global const adc_sampling_freq;
  seq_len = length(seq);
  figure(figure_num);
  subplot(2,1,1)
  plot_seq(seq, 'ADC capture', 'Sample', 'Value')
  subplot(2,1,2)
  dc_mean = mean(seq);
  seq_mean = seq - mean(seq);
  ac_mean = mean(abs(seq_mean));
  ac_range = max(seq_mean)-min(seq_mean);
  seq_mean_fft = abs(fft(seq_mean));
  plot_seq(seq_mean_fft, 'FFT', 'Digitial Frequency', 'Abs(FFT)');
  axis([1, length(seq_mean_fft)]);
  [seq_mean_fft_max seq_mean_fft_max_index] = max(seq_mean_fft);
  f_max = (seq_mean_fft_max_index-1)/seq_len;
  printf("DC mean = %.2f, AC mean = %.2f, AC Range = %.2f, FFT max at %.2f Hz\n", 
    dc_mean, ac_mean, ac_range, f_max*adc_sampling_freq);
endfunction

% 'f_demod' is the frequency to demodulate. Can be '0' for autocalculation
function plot_seq_and_demod(seq, f_demod)
  f_max = plot_seq_and_fft(seq, 2);
  printf("Freq=%d\n", f_max);
  % Note: 'f_demod' should be equal to 'seq_mean_fft_max_index/seq_len'
  % If 'f_demod' not accurately set -> autodetect (for accuration enough only use on low freqs)
  % Modulation
  % demod = seq_mean.*sin((0:length(seq_mean)-1)*f_demod*2*pi);
  % demod = seq_mean.^2;
  seq_mean = seq - mean(seq);
  demod = abs(seq_mean);
  figure(3)
  subplot(2,1,1)
  plot(demod)
  subplot(2,1,2)
  plot(abs(fft(demod)))
  % Display filtered sequence
  pkg load signal
  [b, a] = butter(10, 0.1);	% Cuttoff = 0.1*pi radians
  demod_filt = filter(b,a,demod);
  figure(4)
  subplot(2,1,1)
  plot(demod_filt)
  subplot(2,1,2)
  plot(abs(fft(demod_filt)))
endfunction

function execute_test(test_id, base_dir)
  switch (test_id)
    case "adc"
      in = transpose(load([base_dir "/adc.csv"]));
      plot_seq_and_fft(in, 2);
    case "adc+demod"
      in = transpose(load([base_dir "/adc.csv"]));
      plot_seq_and_demod(in, 0);
    case "predef_seq+demod"
      period = 20;
      seq = generate_predef_sequence(period);
      plot_seq_and_demod(seq, 1/period);
    case "adcN"
      adc_captures = 4;
      figure(2);
      for i = 0:adc_captures-1
        adc_filename = sprintf("%s/adc%d.csv", base_dir, i);
        in = transpose(load(adc_filename));
        in_len = length(in);
        subplot(adc_captures,1,i+1)
        plot(in)
        axis([1, length(in)]);
      endfor
      return;
    case "adc_filter"
      in = transpose(load([base_dir "/adc_filter.csv"]));
      figure(4);
      subplot(1,1,1)
      plot(in)
      axis([1, length(in)]);
    case "adc+adc_filter+adc_data"
      figure(4);
      in = transpose(load([base_dir "/adc.csv"]));
      subplot(3,1,1)
      plot(in)
      in = transpose(load([base_dir "/adc_filter.csv"]));
      subplot(3,1,2)
      plot(in)
      axis([1, length(in)]);
      in = transpose(load([base_dir "/adc_data.csv"]));
      % bar, barh, stairs are other interesting graphs
      subplot(3,1,3)
      stem(in) 
      axis([1, length(in)]);
      return;
    otherwise
      printf("Unexpected 'test_id' type #%d\n", test_id);
  endswitch
endfunction

% execute_test("adc")
