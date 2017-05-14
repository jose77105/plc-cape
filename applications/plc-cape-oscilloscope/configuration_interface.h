/**
 * @file
 * @brief	Generic configuration interface
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef CONFIGURATION_INTERFACE_H
#define CONFIGURATION_INTERFACE_H

class Configuration_interface
{
public:
	// Empty virtual destructor recommended for interfaces:
	//	http://stackoverflow.com/questions/318064/how-do-you-declare-an-interface-in-c
	//	http://stackoverflow.com/questions/14323595/best-way-to-declare-an-interface-in-c11
	virtual ~Configuration_interface() {}
	virtual int begin_configuration(void) = 0;
	virtual int set_configuration_setting(const char *identifier, const char *data) = 0;
	virtual int end_configuration(void) = 0;
};

#endif /* CONFIGURATION_INTERFACE_H */
