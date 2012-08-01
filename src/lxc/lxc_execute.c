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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#include "caps.h"
#include "lxc.h"
#include "log.h"
#include "conf.h"
#include "confile.h"
#include "arguments.h"
#include "config.h"
#include "start.h"

lxc_log_define(lxc_execute_ui, lxc_execute);

static struct lxc_list defines;

static int invalid_num(const char *s)
{
	char *endptr = NULL;
	long int num = strtol(s, &endptr, 10);
	return num == 0 && (endptr == NULL || *endptr != '\0');
}

static int my_checker(const struct lxc_arguments* args)
{
	if (!args->argc) {
		lxc_error(args, "missing command to execute !");
		return -1;
	}

	if (!args->uid) {
		lxc_error(args, "missing user id, use -u option");
		return -1;
	} else if (invalid_num(args->uid)) {
		lxc_error(args, "invalid user id, expected integer identifier");
		return -1;
	}

	if (!args->gid) {
		lxc_error(args, "missing group id, use -g option");
		return -1;
	} else if (invalid_num(args->gid)) {
		lxc_error(args, "invalid group id, expected integer identifier");
		return -1;
	}

	if (!args->ofile) {
		lxc_error(args, "missing output file, use -O option");
		return -1;
	}

	return 0;
}

static int my_parser(struct lxc_arguments* args, int c, char* arg)
{
	switch (c) {
	case 'f': args->rcfile = arg; break;
	case 's': return lxc_config_define_add(&defines, arg); break;
	case 'd': args->daemonize = 1; args->close_all_fds = 1; break;
	case 'u': args->uid = arg; break;
	case 'g': args->gid = arg; break;
	case 'O': args->ofile = arg; break;
	}
	return 0;
}

static const struct option my_longopts[] = {
	{"rcfile", required_argument, 0, 'f'},
	{"define", required_argument, 0, 's'},
	{"daemon", no_argument, 0, 'd'},
	{"group", required_argument, 0, 'g'},
	{"output", required_argument, 0, 'O'},
	{"user", required_argument, 0, 'u'},
	LXC_COMMON_OPTIONS
};

static struct lxc_arguments my_args = {
	.progname = "lxc-execute",
	.help     = "\
--name=NAME -- COMMAND\n\
\n\
lxc-execute creates a container with the identifier NAME\n\
and execs COMMAND into this container.\n\
\n\
Options :\n\
  -n, --name=NAME      NAME for name of the container\n\
  -d, --daemon         daemonize the container\n\
  -O, --output=FILE    Redirect stdout/stderr to FILE\n\
  -u, --user=uid       Use this user id within the container\n\
  -g, --group=gid      Use this group id within the container\n\
  -f, --rcfile=FILE    Load configuration file FILE\n\
  -s, --define KEY=VAL Assign VAL to configuration variable KEY\n",
	.options  = my_longopts,
	.parser   = my_parser,
	.checker  = my_checker,
};

int main(int argc, char *argv[])
{
	char *rcfile;
	struct lxc_conf *conf;

	lxc_list_init(&defines);

	if (lxc_caps_init())
		return -1;

	if (lxc_arguments_parse(&my_args, argc, argv))
		return -1;

	if (lxc_log_init(my_args.log_file, my_args.log_priority,
			 my_args.progname, my_args.quiet))
		return -1;

	/* rcfile is specified in the cli option */
	if (my_args.rcfile)
		rcfile = (char *)my_args.rcfile;
	else {
		int rc;

		rc = asprintf(&rcfile, LXCPATH "/%s/config", my_args.name);
		if (rc == -1) {
			SYSERROR("failed to allocate memory");
			return -1;
		}

		/* container configuration does not exist */
		if (access(rcfile, F_OK)) {
			free(rcfile);
			rcfile = NULL;
		}
	}

	conf = lxc_conf_init();
	if (!conf) {
		ERROR("failed to initialize configuration");
		return -1;
	}

	if (rcfile && lxc_config_read(rcfile, conf)) {
		ERROR("failed to read configuration file");
		return -1;
	}

	if (lxc_config_define_load(&defines, conf))
		return -1;

	if (my_args.daemonize && daemon(0, 0)) {
		SYSERROR("failed to daemonize '%s'", my_args.name);
		return -1;
	}

	return lxc_execute(my_args.name, my_args.argv, my_args.quiet, conf,
			   atoi(my_args.uid), atoi(my_args.gid), my_args.ofile);
}
