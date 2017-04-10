ui-ncurses {#plugin-ui-ncurses}
==========

@brief Basic UI plugin based on the _ncurses_ package

## SUMMARY

<table>
<tr>
	<td><b>Target</b><td><i>ui-ncurses.so</i>
<tr>
	<td><b>Purpose</b><td>
	Implement a menu-oriented user interface based on the _ncurses_ package
<tr>
	<td><b>Details</b><td>
	Features implemented
	<ul>
		<li><b>Popup menus</b>: the main application provides the hierarchy of the menus and
			operations related
		<li><b>Popup dialogs</b>: allows edition of multi-fields in a generic and comfortable way
		<li><b>Popup list boxes</b>: asks the user for a choice among a list of predefined options
	</ul>
<tr>
	<td><b>Source code</b>
	<td>@link ./plugins/ui/ui-ncurses @endlink
</table>

## SCREENSHOTS

## MORE INFO

### NCURSES
* http://www.tldp.org/HOWTO/NCURSES-Programming-HOWTO/
* https://www.gnu.org/software/guile-ncurses/manual/html_node/index.html#SEC_Contents

### NCURSES SOURCE CODE
* https://github.com/D-Programming-Deimos/ncurses/blob/master/C/form.h
* http://pubs.opengroup.org/onlinepubs/7908799/xcurses/curses.h.html
* http://invisible-island.net/ncurses/man/menu.3x.html

### Info on pads, panels and forms
* https://www.gnu.org/software/guile-ncurses/manual/html_node/Create-and-display-pads.html
* http://www.tldp.org/HOWTO/NCURSES-Programming-HOWTO/panels.html
* http://graysoftinc.com/terminal-tricks/curses-windows-pads-and-panels

@dir plugins/ui/ui-ncurses
@see @ref plugin-ui-ncurses
