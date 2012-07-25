/*
 * lxc: linux Container library
 *
 * (C) Copyright IBM Corp. 2007, 2008
 *
 * Authors:
 * Daniel Lezcano <dlezcano at fr.ibm.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <grp.h>

#include "log.h"
#include "start.h"

lxc_log_define(lxc_execute, lxc_start);

struct execute_args {
	char *const *argv;
	int quiet;
};

static int execute_start(struct lxc_handler *handler, void* data)
{
	int j, i = 0;
	struct execute_args *my_args = data;
	char **argv;
	int argc = 0;

	while (my_args->argv[argc++]);

	argv = malloc((argc + my_args->quiet ? 5 : 4) * sizeof(*argv));
	if (!argv)
		return 1;

	argv[i++] = LXCINITDIR "/lxc-init";
	if (my_args->quiet)
		argv[i++] = "--quiet";
	argv[i++] = "--";
	for (j = 0; j < argc; j++)
		argv[i++] = my_args->argv[j];
	argv[i++] = NULL;

	NOTICE("exec'ing '%s'", my_args->argv[0]);

	execvp(argv[0], argv);
	SYSERROR("failed to exec %s", argv[0]);
	return 1;
}

static int execute_start_noinit(struct lxc_handler *handler, void* data)
{
	struct execute_args *my_args = data;

	/* FIXME: the following values should come from config/cmdline args */
	const char *log = "/instance/log";
	const gid_t gid = 1001;
	const uid_t uid = 1001;

	if (setgroups(1, &gid) < 0) {
		SYSERROR("failed to change groups to '%d'", gid);
		return 1;
	}

	if (setresgid(gid, gid, gid)) {
		SYSERROR("failed to change real, effective, saved gid to '%d'",
			 gid);
		return 1;
	}

	if (setresuid(uid, uid, uid)) {
		SYSERROR("failed to change real, effective, saved uid to '%d'",
			 uid);
		return 1;
	}

	int fd = open(log, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		SYSERROR("failed to open log '%s'", log);
		return 1;
	}

	fd = dup2(fd, 1);
	if (fd < 0) {
		SYSERROR("failed to redirect stdout");
		return 1;
	}

	fd = dup2(1, 2);
	if (fd < 0) {
		SYSERROR("failed to redirect stderr");
		return -1;
	}

	NOTICE("exec'ing '%s'", my_args->argv[0]);
	execvp(my_args->argv[0], my_args->argv);
	SYSERROR("failed to exec %s", my_args->argv[0]);

	return 1;
}

static int execute_post_start(struct lxc_handler *handler, void* data)
{
	struct execute_args *my_args = data;
	NOTICE("'%s' started with pid '%d'", my_args->argv[0], handler->pid);
	return 0;
}

static struct lxc_operations execute_start_ops = {
	.start = execute_start_noinit,
	.post_start = execute_post_start
};

int lxc_execute(const char *name, char *const argv[], int quiet,
		struct lxc_conf *conf)
{
	struct execute_args args = {
		.argv = argv,
		.quiet = quiet
	};

	if (lxc_check_inherited(conf, -1))
		return -1;

	return __lxc_start(name, conf, &execute_start_ops, &args);
}
