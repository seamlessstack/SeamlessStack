/*
 * Copyright (C) 2014 SeamlessStack
 *
 *  This file is part of SeamlessStack distributed file system.
 *
 * SeamlessStack distributed file system is free software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 *
 * SeamlessStack distributed file system is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SeamlessStack distributed file system. If not,
 * see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <pthread.h>
#include <sstack_log.h>
#include <sstack_transport.h>
#include <sstack_sfsd.h>

extern log_ctx_t *sfsd_ctx;

void run_daemon_sfsd(sfsd_t *sfsd)
{
	sleep(4);
	sfs_log(sfsd->log_ctx, SFS_INFO, "%s", "Daemon started\n");
}
