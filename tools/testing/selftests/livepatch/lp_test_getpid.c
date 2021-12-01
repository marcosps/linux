// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 SUSE LLC
 * Author: Libor Pechacek <lpechacek@suse.com>
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>

int run = 1;
int sig_int;

void hup_handler(int signum)
{
	run = 0;
}

void int_handler(int signum)
{
	run = 0;
	sig_int = 1;
}

int main(int argc, char *argv[])
{
	pid_t orig_pid, pid;
	long count = 0;

	signal(SIGHUP, &hup_handler);
	signal(SIGINT, &int_handler);

	orig_pid = syscall(SYS_getpid);

	while(run) {
		pid = syscall(SYS_getpid);
		if (pid != orig_pid)
			return 1;
		count++;
	}

	if (sig_int)
		printf("%d iterations done\n", count);

	return 0;
}
