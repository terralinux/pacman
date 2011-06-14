/*
 *  diskspace.h
 *
 *  Copyright (c) 2010-2011 Pacman Development Team <pacman-dev@archlinux.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ALPM_DISKSPACE_H
#define _ALPM_DISKSPACE_H

#if defined(HAVE_SYS_MOUNT_H)
#include <sys/mount.h>
#endif
#if defined(HAVE_SYS_STATVFS_H)
#include <sys/statvfs.h>
#endif

#include "alpm.h"

enum mount_used_level {
	USED_REMOVE = 1,
	USED_INSTALL = (1 << 1),
};

typedef struct __alpm_mountpoint_t {
	/* mount point information */
	char *mount_dir;
	size_t mount_dir_len;
	/* storage for additional disk usage calculations */
	long blocks_needed;
	long max_blocks_needed;
	enum mount_used_level used;
	int read_only;
	FSSTATSTYPE fsp;
} alpm_mountpoint_t;

int _alpm_check_diskspace(pmhandle_t *handle);

#endif /* _ALPM_DISKSPACE_H */

/* vim: set ts=2 sw=2 noet: */
