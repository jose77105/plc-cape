/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include "+common/api/+base.h"
#include "spi.h"
#include "tx_sched.h"

ATTR_INTERN void dummy()
{
}

ATTR_INTERN void set_dummy_functions(void *struct_ptr, uint32_t struct_size)
{
	// Set all the functions of the interface to a dummy function to avoid a segmentation fault if
	//	not implemented
	int functions_count = struct_size / sizeof(void (*)());
	void **fn_ptr = (void**) struct_ptr;
	for (; functions_count > 0; functions_count--, fn_ptr++)
		*fn_ptr = (void *) dummy;
}
