%
%  Copyright (C) 2017 Jose Maria Ortega\n
%  Distributed under the GNU GPLv3. For full terms see the file LICENSE
%
% PURPOSE
%  Tools related to the 'plc-cape-freq-response' application
%

function plc_freq_response_plot_samples_captured()
  global plc_base_dir;
  in = transpose(load([plc_base_dir "/adc.csv"]));
  plot_seq_and_fft('adc.csv', 1, in);
endfunction

%
% plc_freq_response_plot_all(freq_range, highlight_H_level)
% This function plots graphs from:
% * 'freq_response.csv': list of output vs input gains for a frequency range
% * 'adc.csv': samples captured for the reference frequency
%
function plc_freq_response_plot_all(freq_range, highlight_H_level = 0.0)
  global plc_base_dir;
  % Plot format: "r-" means red and solid lines, "k" is black...
  highlight = "r-";
  in = transpose(load([plc_base_dir "/adc.csv"]));
  % Plot 2000 samples to simplify conversion from FFT index to corresponding
  % frequency (FFT index 2000 corresponds to 200 kHz)
  plot_seq_and_fft('adc.csv', 1, in(1:2000));
  in = transpose(load([plc_base_dir "/freq_response.csv"]));
  % To get the number of samples of the 'in' matrix here use 'size(in, 2)'
  % instead of 'length(in)'
  H_max = max(in(2,:));
  H_min = min(in(2,:));
  H_max_plot = H_max*1.1;
  H_min_plot = min(H_min*0.9, 0.01);
  freq_min = in(1, 1);
  freq_max = in(1, size(in, 2));
  if (exist("freq_range", "var") == 0)
    if ((freq_min <= 10000) && (freq_max >= 200000))
      freq_range = "10k-200k";
    else
      freq_range = "auto";
    endif
  endif
  % H function with linear axis
  figure(2);
  subplot(1,1,1)
  plot(in(1,:), in(2,:), '-o');
  title('Frequency response'); xlabel('Frequency [Hz]'); ylabel('Gain');
  box off;
  if (strcmp(freq_range, "10k-200k")
    || strcmp(freq_range, "10k-200k-cenelec-b"))
    axis([0, freq_max, 0, H_max_plot]);
    set(gca, 'xtick',
      [50 25000 50000 75000 100000 125000 150000 175000 200000]);
    set(gca, 'xticklabel',
      {'50', '25k', '50k', '75k', '100k', '125k ', '150k', '175k', '200k'});
    if (strcmp(freq_range, "10k-200k-cenelec-b"))
      hold on;
      plot([95000 95000], [0 H_max_plot], highlight, "linewidth", 1);
      plot([125000 125000], [0 H_max_plot], highlight, "linewidth", 1);
      hold off;
    endif
  else
    axis([freq_min, freq_max, 0, H_max_plot]);
  endif
  if (highlight_H_level != 0.0)
    hold on;
    plot([freq_min freq_max],
      [highlight_H_level highlight_H_level], highlight, "linewidth", 1);
    hold off;
  endif
  grid on;
  % H function with logarithmic axis
  figure(3);
  subplot(1,1,1)
  loglog(in(1,:), in(2,:), '-+');
  % NOTE: Remember to not use <=0 in the 'log' axis. Otherwise error
  title('Frequency response'); xlabel('Frequency [Hz]'); ylabel('Gain');
  box off;
  switch (freq_range)
  case "10-1k"
    axis([10, 1000, H_min_plot, H_max_plot]);
    set(gca, 'xtick', [10, 25, 50, 100, 1000]);
    set(gca, 'xticklabel', {'10', '25', '50', '100', '1k'});
  case "10-10k"
    axis([10, 10000, H_min_plot, H_max_plot]);
    set(gca, 'xtick', [10, 50, 100, 1000, 10000]);
    set(gca, 'xticklabel', {'10', '50', '100', '1k', '10k'});
  case "10k-200k"
    axis([10000, 200000, H_min_plot, H_max_plot]);
    set(gca, 'xtick', [10000 50000 70000 100000 125000 150000 200000]);
    set(gca, 'xticklabel',
      {'10k', '50k', '70k', '100k', '125k', '150k', '200k'});
  case "10k-200k-cenelec-b"
    axis([10000, 200000, H_min_plot, H_max_plot]);
    set(gca, 'xtick', [10000 50000 70000 95000 125000 150000 200000]);
    set(gca, 'xticklabel',
      {'10k', '50k', '70k', '95k', '125k', '150k', '200k'});
    % Highlight the band of interest
    hold on;
    plot([95000 95000], [H_min_plot H_max_plot], highlight, "linewidth", 1);
    plot([125000 125000], [H_min_plot H_max_plot], highlight, "linewidth", 1);
    hold off;
  case "10-200k"
    axis([10, 200000, H_min_plot, H_max_plot]);
    set(gca, 'xtick', [10, 50, 100, 1000, 10000 50000 100000 200000]);
    set(gca, 'xticklabel',
      {'10', '50', '100', '1k', '10k', '50k', '100k', '200k'});
  case "auto"
    axis([freq_min, freq_max, H_min_plot, H_max_plot]);
  otherwise
    printf("Unknown 'freq_range' '%s'\n", freq_range);
    return;
  endswitch
  if (highlight_H_level != 0.0)
    hold on;
    plot([freq_min freq_max],
      [highlight_H_level highlight_H_level], highlight, "linewidth", 1);
    hold off;
  endif
  set(gca, 'tickdir', "out");
  % For some interesting extra ticks on the y-axis
  set(gca, 'ytick', 
    [get(gca, 'ytick') [0.02 0.05 0.07 0.2 0.5 0.7 2.0 5.0 7.0]]);
  % Change default exponential notation (1e+0, 1e-1...) to floats with 2 digits
  set(gca, 'yticklabel', num2str(get(gca, 'ytick'), '%.2f|'));
  grid on;
  printf("H max = %.2f, H min = %.2f\n", H_max, H_min); 
endfunction

function plc_freq_response_save_plots()
  global plc_base_dir;
  % Save graphs notifying about the progress (it can be a slow process)
  % 'fflush' required to display the message immediately
  printf("Storing adc.pdf...\n"); fflush(stdout);
  % NOTE: For some reason (a bug on 'ghostscrip') trying to save 'adc.csv'
  %   graphs in PNG format (or BMP) freezes the PC. It works with PDF
  saveas(1, [plc_base_dir "/adc.pdf"]);
  printf("Storing freq_response.png...\n"); fflush(stdout);
  saveas(2, [plc_base_dir "/freq_response.png"]);
  printf("Storing freq_response_log.png...\n"); fflush(stdout);
  saveas(3, [plc_base_dir "/freq_response_log.png"]);
endfunction
