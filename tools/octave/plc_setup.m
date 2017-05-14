%
%  Copyright (C) 2017 Jose Maria Ortega\n
%  Distributed under the GNU GPLv3. For full terms see the file LICENSE
%
% PURPOSE
%  Setups the 'plc-cape project' environment
%
% TYPICAL USAGE EXAMPLES
%  run "X:/tools/octave/plc_setup";
%  plc_setup_configure("Y:/applications/plc-cape-freq-response", 200000);
%
% NOTE on 'run' vs 'source'
%  * 'source' needs the suffix '.m' to be explicitly given
%  * 'run' makes all the '.m' within the same directory easily available in the
%    workspace without requiring speciying the absolute path. This way it
%    simplifies multi-script management in the dependencies. If we would use
%    'source', the dependencies would require absolute paths
%  Use 'plc_' + TAB to see all the functions and scripts available in the
%  workspace
%
% More info on 'script and function files':
%  https://www.gnu.org/software/octave/doc/v4.0.1/Function-Files.html
%
% BUG ON 'FAT' FILE SYSTEMS:
%  There is a bug on the Octave dependency system management in FAT. It is based
%  on FOLDER stamps, which are not udpated on some FAT filesystems when a file
%  within changes (e.g. in Windows XP).
%  This is reported in http://savannah.gnu.org/bugs/?31080
%  So, external changes into a '.m' file are not made effective due to the cache
%  PATCH: Call 'plc_setup_load_dependencies' before each function call
%

global plc_adc_sampling_freq = 20000;
global plc_stem_plot = 0;
global plc_base_dir = "";

function plc_setup_load_dependencies()
  %  'source' could also be used here instead of 'run'
  run plc_freq_response
  run plc_oscilloscope
  run plc_shortcuts_plot
  run plc_tools
endfunction

%
% plc_setup_configure(base_dir, rx_sampling_rate)
% base_dir:
%  Linux example: "/media/y/applications/plc-cape-lab"
%  Linux example: "/media/y/applications/plc-cape-freq-response"
%  Windows example: "Y:/applications/plc-cape-lab"
% rx_sampling_rate:
%  The sampling rate for the ADC captures (e.g. 200000 for BBB-based captures)
%
function plc_setup_configure(base_dir, rx_sampling_rate_sps)
  global plc_base_dir;
  global plc_adc_sampling_freq;
  plc_base_dir = base_dir;
  plc_adc_sampling_freq = rx_sampling_rate_sps;
endfunction

%
% MAIN SCRIPT
%  Loads the dependent scrits into the Octave workspace
% 
printf("plc_setup.m: Loading dependencies...\n");
plc_setup_load_dependencies
printf("plc_setup.m: Workspace ready\n");