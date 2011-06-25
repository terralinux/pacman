/*
 *  db.h
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2006 by Miklos Vajna <vmiklos@frugalware.org>
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
#ifndef _ALPM_DB_H
#define _ALPM_DB_H

#include <time.h>

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

#include "alpm.h"
#include "pkghash.h"
#include "signing.h"

/* Database entries */
typedef enum _pmdbinfrq_t {
	INFRQ_BASE = 1,
	INFRQ_DESC = (1 << 1),
	INFRQ_FILES = (1 << 2),
	INFRQ_SCRIPTLET = (1 << 3),
	INFRQ_DSIZE = (1 << 4),
	/* ALL should be info stored in the package or database */
	INFRQ_ALL = 0x1F
} pmdbinfrq_t;

/** Database status. Bitflags. */
enum _pmdbstatus_t {
	DB_STATUS_VALID = (1 << 0),
	DB_STATUS_PKGCACHE = (1 << 1),
	DB_STATUS_GRPCACHE = (1 << 2)
};

struct db_operations {
	int (*populate) (pmdb_t *);
	void (*unregister) (pmdb_t *);
};

/* Database */
struct __pmdb_t {
	pmhandle_t *handle;
	char *treename;
	/* do not access directly, use _alpm_db_path(db) for lazy access */
	char *_path;
	/* also indicates whether we are RO or RW */
	int is_local;
	/* flags determining validity, loaded caches, etc. */
	enum _pmdbstatus_t status;
	pmpkghash_t *pkgcache;
	alpm_list_t *grpcache;
	alpm_list_t *servers;
	pgp_verify_t pgp_verify;

	struct db_operations *ops;
};


/* db.c, database general calls */
pmdb_t *_alpm_db_new(const char *treename, int is_local);
void _alpm_db_free(pmdb_t *db);
const char *_alpm_db_path(pmdb_t *db);
char *_alpm_db_sig_path(pmdb_t *db);
int _alpm_db_cmp(const void *d1, const void *d2);
alpm_list_t *_alpm_db_search(pmdb_t *db, const alpm_list_t *needles);
pmdb_t *_alpm_db_register_local(pmhandle_t *handle);
pmdb_t *_alpm_db_register_sync(pmhandle_t *handle, const char *treename,
		pgp_verify_t level);
void _alpm_db_unregister(pmdb_t *db);

/* be_*.c, backend specific calls */
int _alpm_local_db_read(pmdb_t *db, pmpkg_t *info, pmdbinfrq_t inforeq);
int _alpm_local_db_prepare(pmdb_t *db, pmpkg_t *info);
int _alpm_local_db_write(pmdb_t *db, pmpkg_t *info, pmdbinfrq_t inforeq);
int _alpm_local_db_remove(pmdb_t *db, pmpkg_t *info);

/* cache bullshit */
/* packages */
void _alpm_db_free_pkgcache(pmdb_t *db);
int _alpm_db_add_pkgincache(pmdb_t *db, pmpkg_t *pkg);
int _alpm_db_remove_pkgfromcache(pmdb_t *db, pmpkg_t *pkg);
pmpkghash_t *_alpm_db_get_pkgcache_hash(pmdb_t *db);
alpm_list_t *_alpm_db_get_pkgcache(pmdb_t *db);
int _alpm_db_ensure_pkgcache(pmdb_t *db, pmdbinfrq_t infolevel);
pmpkg_t *_alpm_db_get_pkgfromcache(pmdb_t *db, const char *target);
/* groups */
void _alpm_db_free_grpcache(pmdb_t *db);
alpm_list_t *_alpm_db_get_grpcache(pmdb_t *db);
pmgrp_t *_alpm_db_get_grpfromcache(pmdb_t *db, const char *target);

#endif /* _ALPM_DB_H */

/* vim: set ts=2 sw=2 noet: */
