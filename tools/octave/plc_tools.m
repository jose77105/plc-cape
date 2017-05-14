%
%  Copyright (C) 2016-2017 Jose Maria Ortega\n
%  Distributed under the GNU GPLv3. For full terms see the file LICENSE
%
% PURPOSE
%  Common tools to be used within the 'plc-cape' project
%
% REMARKS
% Interesting info on plotting control here:
%   https://www.gnu.org/software/octave/doc/v4.0.1/Plot-Annotations.html
%   https://www.gnu.org/software/octave/doc/v4.0.0/Axes-Properties.html
%

function seq = plc_generate_predef_sequence_async(f_carrier_hz)
  global const plc_adc_sampling_freq;
  data=[0, 1, 0, 1, 1, 1, 0, 1];
  period = plc_adc_sampling_freq/f_carrier_hz;
  seq_len = 1024;
  assert(mod(seq_len, length(data)) == 0);
  seq_symbol_len = seq_len/length(data);
  seq = [];
  for i = 1:length(data)
    if (data(i) == 0)
      seq = [seq zeros(1,seq_symbol_len)];
    else
      seq = [seq data(i)*sinewave(seq_symbol_len,period)];
    endif
  endfor
  % Interference simulation
  seq += sinewave(seq_len,plc_adc_sampling_freq/30000);
endfunction

function seq = plc_generate_predef_sequence_sync(f_carrier_hz)
  global const plc_adc_sampling_freq;
  data=[1, 1, 1, 1, 1, 1, 1, 1];
  period = plc_adc_sampling_freq/f_carrier_hz;
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
  % Interference simulation
  % seq += sinewave(seq_len,plc_adc_sampling_freq/30000);
endfunction

function plc_plot_seq(title_arg, xlabel_arg, ylabel_arg, seq)
  global const plc_stem_plot;
  if (plc_stem_plot)
    stem(seq)
  else
    plot(seq);
  endif
  axis([1, length(seq)]);
  title(title_arg); xlabel(xlabel_arg); ylabel(ylabel_arg);
endfunction

function plot_seq_and_fft(title, figure_num, seq)
  figure(figure_num);
  subplot(2,1,1)
  plc_plot_seq(title, 'Index', 'Value', seq)
  subplot(2,1,2)
  plc_plot_seq('FFT', 'Digitial Frequency', 'Abs(FFT)', abs(fft(seq)))
endfunction

function f_max = plc_plot_adc_capture(title, figure_num, seq)
  global const plc_adc_sampling_freq;
  seq_len = length(seq);
  figure(figure_num);
  subplot(2,1,1)
  plc_plot_seq(title, 'Sample', 'Value', seq)
  subplot(2,1,2)
  dc_mean = mean(seq);
  seq_mean = seq - mean(seq);
  ac_mean = mean(abs(seq_mean));
  ac_range = max(seq_mean)-min(seq_mean);
  seq_mean_fft = abs(fft(seq_mean));
  plc_plot_seq('FFT', 'Digitial Frequency', 'Abs(FFT)', seq_mean_fft);
  [seq_mean_fft_max seq_mean_fft_max_index] = max(seq_mean_fft);
  f_max = (seq_mean_fft_max_index-1)/seq_len;
  printf("DC mean = %.2f, AC mean = %.2f, AC Range = %.2f, FFT max at %.2f Hz\n", 
    dc_mean, ac_mean, ac_range, f_max*plc_adc_sampling_freq);
endfunction

function plc_plot_adc_capture_and_demod(title, seq, f_passband_hz, f_demod_hz)
  global const plc_adc_sampling_freq;
  band_width_hz = 500;
  if (f_passband_hz < band_width_hz)
    band_width_hz = f_passband_hz;
  endif
  band_width = band_width_hz/plc_adc_sampling_freq;
  f_max = plc_plot_adc_capture(title, 2, seq);
  seq_mean = seq - mean(seq);
  pkg load signal
  % Band-pass filter (freqs between 0 and 1; 1 means plc_adc_sampling_freq/2)
  % [b,a] = butter(n, [Wl, Wh]) band pass filter with edges pi*Wl and pi*Wh radians
  if (f_passband_hz != 0)
    f_passband = f_passband_hz/plc_adc_sampling_freq;
  printf("Pass band = %f, %f\n", 2*(f_passband - band_width), 2*(f_passband + band_width));
    [b_band, a_band] = butter(3, [2*(f_passband - band_width) 2*(f_passband + band_width)]);
    % Warning: Depending on butter order and filter selectivity the filtered sequence may overflow
    seq_mean_filt = filter(b_band, a_band, seq_mean);
    plot_seq_and_fft('ADC - Mean + Band-pass', 3, seq_mean_filt);
  else
    seq_mean_filt = seq_mean;
    plot_seq_and_fft('ADC - Mean', 3, seq_mean_filt);
  endif
  if (f_demod_hz == 0)
    demod = abs(seq_mean_filt);
    % Alternate way with less harmonic content
    % demod = seq_mean_filt.^2;
    plot_seq_and_fft('Demod Abs()', 4, demod);
  else
  w_demod = 2*pi*f_demod_hz/plc_adc_sampling_freq;
  % SEGUIR POR AQUÍ: Mirar cómo calcular retraso de fase para demod coherente
  %  Igual la mejor solución sería calcular paso por cero
  % fft_demod_index = round(f_demod_hz/plc_adc_sampling_freq*length(seq_mean_filt));
  % seq_mean_filt_fft = fft(seq_mean_filt);
  % figure(7);
  % plot(arg(seq_mean_filt_fft));
  % seq_mean_filt_fft_f_demod = seq_mean_filt_fft(fft_demod_index+1);
  % printf("Abs(FFT) = %f\n", abs(seq_mean_filt_fft_f_demod));
  % printf("Phase(FFT) = %f\n", arg(seq_mean_filt_fft_f_demod));
    % phase_error = arg(seq_mean_filt_fft_f_demod);
  seq_index = (0:length(seq_mean_filt)-1);
  % If there is no delay between input and output (or it is known in advance) simple 'cos' or
  % 'sin' (depending on modulated wave) can be used:
  %  demod = seq_mean_filt.*(sin(seq_index*w_demod));
  % This generates replicas of original wave at zero and at double frequencies
  % Note that a 'phase_error = pi/2' "eliminates" the zero frequency
  phase_error = 0;
    demod = seq_mean_filt.*(sin(seq_index*w_demod-phase_error));
    printf("Max = %f\n", max(demod));
    plot_seq_and_fft('Demod Sin()', 4, demod);
  endif
  % Low-pass filter
  % [b,a] = butter(n, [Wl, Wh]) band pass filter with edges pi*Wl and pi*Wh radians
  [b_low, a_low] = butter(3, band_width);
  demod_filt = filter(b_low,a_low,demod);
  plot_seq_and_fft('Demod + Filter', 5, demod_filt);
endfunction
