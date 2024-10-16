/*
 * fsyscalltest.c
 *
 * Tests file-related system calls open, close, read and write.
 *
 * Should run on emufs. This test allows testing the file-related system calls
 * early on, before much of the functionality implemented. This test does not
 * rely on full process functionality (e.g., fork/exec).
 *
 * Much of the code is borrowed from filetest.c
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <limits.h>


/* This test takes no arguments, so we can run it before argument passing
 * is fully implemented. 
 */
int
main()
{	
	char buf[256];
	__getcwd(buf,256);
	printf("bla");
	printf(buf);
	return 0;
}
