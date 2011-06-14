/*
 *  package.h
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
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
#ifndef _PM_PACKAGE_H
#define _PM_PACKAGE_H

#include <alpm.h>

/* TODO it would be nice if we didn't duplicate a backend type */
enum pkg_from {
	PKG_FROM_FILE = 1,
	PKG_FROM_LOCALDB,
	PKG_FROM_SYNCDB
};

void dump_pkg_full(pmpkg_t *pkg, enum pkg_from from, int extra);

void dump_pkg_backups(pmpkg_t *pkg);
void dump_pkg_files(pmpkg_t *pkg, int quiet);
void dump_pkg_changelog(pmpkg_t *pkg);

#endif /* _PM_PACKAGE_H */

/* vim: set ts=2 sw=2 noet: */
