%
%  Copyright (C) 2017 Jose Maria Ortega\n
%  Distributed under the GNU GPLv3. For full terms see the file LICENSE
%
% PURPOSE
%  Tools related to the 'plc-cape-oscilloscope' application
%

function plc_oscilloscope_plot(period)
  global plc_base_dir;
  plot(transpose(load([plc_base_dir "/oscilloscope.csv"])))
endfunction
