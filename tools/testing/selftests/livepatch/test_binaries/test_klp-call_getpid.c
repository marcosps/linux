// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017 SUSE
 * Author: Libor Pechacek <lpechacek@suse.cz>
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>

static int stop = 0;
static int sig_int;

void hup_handler(int signum)
{
	stop = 1;
}

void int_handler(int signum)
{
	stop = 1;
	sig_int = 1;
}

int main(int argc, char *argv[])
{
	pid_t orig_pid, pid;
	long count = 0;

	signal(SIGHUP, &hup_handler);
	signal(SIGINT, &int_handler);

	orig_pid = syscall(SYS_getpid);

	while (!stop) {
		pid = syscall(SYS_getpid);
		if (pid != orig_pid)
			return 1;
		count++;
	}

	if (sig_int)
		printf("%ld iterations done\n", count);

	return 0;
}
