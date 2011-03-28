/*
 *  be_sync.c
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

#include "config.h"

#include <errno.h>
#include <limits.h>

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libalpm */
#include "util.h"
#include "log.h"
#include "alpm.h"
#include "alpm_list.h"
#include "package.h"
#include "handle.h"
#include "delta.h"
#include "deps.h"
#include "dload.h"

/** Update a package database
 *
 * An update of the package database \a db will be attempted. Unless
 * \a force is true, the update will only be performed if the remote
 * database was modified since the last update.
 *
 * A transaction is necessary for this operation, in order to obtain a
 * database lock. During this transaction the front-end will be informed
 * of the download progress of the database via the download callback.
 *
 * Example:
 * @code
 * alpm_list_t *syncs = alpm_option_get_syncdbs();
 * if(alpm_trans_init(0, NULL, NULL, NULL) == 0) {
 *     for(i = syncs; i; i = alpm_list_next(i)) {
 *         pmdb_t *db = alpm_list_getdata(i);
 *         result = alpm_db_update(0, db);
 *         alpm_trans_release();
 *
 *         if(result < 0) {
 *	           printf("Unable to update database: %s\n", alpm_strerrorlast());
 *         } else if(result == 1) {
 *             printf("Database already up to date\n");
 *         } else {
 *             printf("Database updated\n");
 *         }
 *     }
 * }
 * @endcode
 *
 * @ingroup alpm_databases
 * @note After a successful update, the \link alpm_db_get_pkgcache()
 * package cache \endlink will be invalidated
 * @param force if true, then forces the update, otherwise update only in case
 * the database isn't up to date
 * @param db pointer to the package database to update
 * @return 0 on success, -1 on error (pm_errno is set accordingly), 1 if up to
 * to date
 */
int SYMEXPORT alpm_db_update(int force, pmdb_t *db)
{
	char *dbfile, *syncpath;
	const char *dbpath;
	struct stat buf;
	size_t len;
	int ret;
	mode_t oldmask;

	ALPM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(PM_ERR_HANDLE_NULL, -1));
	ASSERT(db != NULL && db != handle->db_local, RET_ERR(PM_ERR_WRONG_ARGS, -1));

	if(!alpm_list_find_ptr(handle->dbs_sync, db)) {
		RET_ERR(PM_ERR_DB_NOT_FOUND, -1);
	}

	len = strlen(db->treename) + 4;
	MALLOC(dbfile, len, RET_ERR(PM_ERR_MEMORY, -1));
	sprintf(dbfile, "%s.db", db->treename);

	dbpath = alpm_option_get_dbpath();
	len = strlen(dbpath) + 6;
	MALLOC(syncpath, len, RET_ERR(PM_ERR_MEMORY, -1));
	sprintf(syncpath, "%s%s", dbpath, "sync/");

	/* make sure we have a sane umask */
	oldmask = umask(0022);

	if(stat(syncpath, &buf) != 0) {
		_alpm_log(PM_LOG_DEBUG, "database dir '%s' does not exist, creating it\n",
				syncpath);
		if(_alpm_makepath(syncpath) != 0) {
			free(dbfile);
			free(syncpath);
			RET_ERR(PM_ERR_SYSTEM, -1);
		}
	} else if(!S_ISDIR(buf.st_mode)) {
		_alpm_log(PM_LOG_WARNING, _("removing invalid file: %s\n"), syncpath);
		if(unlink(syncpath) != 0 || _alpm_makepath(syncpath) != 0) {
			free(dbfile);
			free(syncpath);
			RET_ERR(PM_ERR_SYSTEM, -1);
		}
	}

	ret = _alpm_download_single_file(dbfile, db->servers, syncpath, force);

	if(ret == 1) {
		/* files match, do nothing */
		pm_errno = 0;
		goto cleanup;
	} else if(ret == -1) {
		/* pm_errno was set by the download code */
		_alpm_log(PM_LOG_DEBUG, "failed to sync db: %s\n", alpm_strerrorlast());
		goto cleanup;
	}

	/* Download and check the signature of the database if needed */
	if(db->pgp_verify != PM_PGP_VERIFY_NEVER) {
		char *sigfile, *sigfilepath;
		int sigret;

		len = strlen(dbfile) + 5;
		MALLOC(sigfile, len, RET_ERR(PM_ERR_MEMORY, -1));
		sprintf(sigfile, "%s.sig", dbfile);

		/* prevent old signature being used if the following download fails */
		len = strlen(syncpath) + strlen(sigfile) + 1;
		MALLOC(sigfilepath, len, RET_ERR(PM_ERR_MEMORY, -1));
		sprintf(sigfilepath, "%s%s", syncpath, sigfile);
		_alpm_rmrf(sigfilepath);
		free(sigfilepath);

		sigret = _alpm_download_single_file(sigfile, db->servers, syncpath, 0);
		free(sigfile);

		if(sigret == -1 && db->pgp_verify == PM_PGP_VERIFY_ALWAYS) {
			_alpm_log(PM_LOG_ERROR, _("Failed to download signature for db: %s\n"),
					alpm_strerrorlast());
			pm_errno = PM_ERR_SIG_INVALID;
			ret = -1;
			goto cleanup;
		}

		sigret = alpm_db_check_pgp_signature(db);
		if((db->pgp_verify == PM_PGP_VERIFY_ALWAYS && sigret != 0) ||
				(db->pgp_verify == PM_PGP_VERIFY_OPTIONAL && sigret == 1)) {
			/* pm_errno was set by the checking code */
			/* TODO: should we just leave the unverified database */
			ret = -1;
			goto cleanup;
		}
	}

	/* Cache needs to be rebuilt */
	_alpm_db_free_pkgcache(db);

cleanup:

	free(dbfile);
	free(syncpath);
	umask(oldmask);
	return ret;
}

/* Forward decl so I don't reorganize the whole file right now */
static int sync_db_read(pmdb_t *db, struct archive *archive,
		struct archive_entry *entry, pmpkg_t *likely_pkg);

/*
 * This is the data table used to generate the estimating function below.
 * "Weighted Avg" means averaging the bottom table values; thus each repo, big
 * or small, will have equal influence.  "Unweighted Avg" means averaging the
 * sums of the top table columns, thus each package has equal influence.  The
 * final values are calculated by (surprise) averaging the averages, because
 * why the hell not.
 *
 * Database   Pkgs  tar      bz2     gz      xz
 * community  2096  5294080  256391  421227  301296
 * core        180   460800   25257   36850   29356
 * extra      2606  6635520  294647  470818  339392
 * multilib    126   327680   16120   23261   18732
 * testing      76   204800   10902   14348   12100
 *
 * Bytes Per Package
 * community  2096  2525.80  122.32  200.97  143.75
 * core        180  2560.00  140.32  204.72  163.09
 * extra      2606  2546.25  113.06  180.67  130.23
 * multilib    126  2600.63  127.94  184.61  148.67
 * testing      76  2694.74  143.45  188.79  159.21

 * Weighted Avg     2585.48  129.42  191.95  148.99
 * Unweighted Avg   2543.39  118.74  190.16  137.93
 * Average of Avgs  2564.44  124.08  191.06  143.46
 */
static size_t estimate_package_count(struct stat *st, struct archive *archive)
{
	unsigned int per_package;

	switch(archive_compression(archive)) {
		case ARCHIVE_COMPRESSION_NONE:
			per_package = 2564;
			break;
		case ARCHIVE_COMPRESSION_GZIP:
			per_package = 191;
			break;
		case ARCHIVE_COMPRESSION_BZIP2:
			per_package = 124;
			break;
		case ARCHIVE_COMPRESSION_COMPRESS:
			per_package = 193;
			break;
		case ARCHIVE_COMPRESSION_LZMA:
		case ARCHIVE_COMPRESSION_XZ:
			per_package = 143;
			break;
		case ARCHIVE_COMPRESSION_UU:
			per_package = 3543;
			break;
		default:
			/* assume it is at least somewhat compressed */
			per_package = 200;
	}
	return (size_t)((st->st_size / per_package) + 1);
}

static int sync_db_populate(pmdb_t *db)
{
	size_t est_count;
	int count = 0;
	struct stat buf;
	struct archive *archive;
	struct archive_entry *entry;
	pmpkg_t *pkg = NULL;

	ALPM_LOG_FUNC;

	ASSERT(db != NULL, RET_ERR(PM_ERR_DB_NULL, -1));

	if((archive = archive_read_new()) == NULL)
		RET_ERR(PM_ERR_LIBARCHIVE, 1);

	archive_read_support_compression_all(archive);
	archive_read_support_format_all(archive);

	if(archive_read_open_filename(archive, _alpm_db_path(db),
				ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
		_alpm_log(PM_LOG_ERROR, _("could not open file %s: %s\n"), _alpm_db_path(db),
				archive_error_string(archive));
		archive_read_finish(archive);
		RET_ERR(PM_ERR_DB_OPEN, 1);
	}
	if(stat(_alpm_db_path(db), &buf) != 0) {
		RET_ERR(PM_ERR_DB_OPEN, 1);
	}
	est_count = estimate_package_count(&buf, archive);

	/* initialize hash at 66% full */
	db->pkgcache = _alpm_pkghash_create(est_count * 3 / 2);
	if(db->pkgcache == NULL) {
		RET_ERR(PM_ERR_MEMORY, -1);
	}

	while(archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
		const struct stat *st;

		st = archive_entry_stat(entry);

		if(S_ISDIR(st->st_mode)) {
			const char *name;

			pkg = _alpm_pkg_new();
			if(pkg == NULL) {
				archive_read_finish(archive);
				RET_ERR(PM_ERR_MEMORY, -1);
			}

			name = archive_entry_pathname(entry);

			if(_alpm_splitname(name, pkg) != 0) {
				_alpm_log(PM_LOG_ERROR, _("invalid name for database entry '%s'\n"),
						name);
				_alpm_pkg_free(pkg);
				continue;
			}

			/* duplicated database entries are not allowed */
			if(_alpm_pkghash_find(db->pkgcache, pkg->name)) {
				_alpm_log(PM_LOG_ERROR, _("duplicated database entry '%s'\n"), pkg->name);
				_alpm_pkg_free(pkg);
				continue;
			}

			pkg->origin = PKG_FROM_SYNCDB;
			pkg->ops = &default_pkg_ops;
			pkg->origin_data.db = db;

			/* add to the collection */
			_alpm_log(PM_LOG_FUNCTION, "adding '%s' to package cache for db '%s'\n",
					pkg->name, db->treename);
			db->pkgcache = _alpm_pkghash_add(db->pkgcache, pkg);
			count++;
		} else {
			/* we have desc, depends or deltas - parse it */
			sync_db_read(db, archive, entry, pkg);
		}
	}

	if(count > 0) {
		db->pkgcache->list = alpm_list_msort(db->pkgcache->list, (size_t)count, _alpm_pkg_cmp);
	}
	archive_read_finish(archive);

	return count;
}

#define READ_NEXT(s) do { \
	if(_alpm_archive_fgets(archive, &buf) != ARCHIVE_OK) goto error; \
	s = _alpm_strtrim(buf.line); \
} while(0)

#define READ_AND_STORE(f) do { \
	READ_NEXT(line); \
	STRDUP(f, line, goto error); \
} while(0)

#define READ_AND_STORE_ALL(f) do { \
	char *linedup; \
	READ_NEXT(line); \
	if(strlen(line) == 0) break; \
	STRDUP(linedup, line, goto error); \
	f = alpm_list_add(f, linedup); \
} while(1) /* note the while(1) and not (0) */

static int sync_db_read(pmdb_t *db, struct archive *archive,
		struct archive_entry *entry, pmpkg_t *likely_pkg)
{
	const char *entryname = NULL, *filename;
	char *pkgname, *p, *q;
	pmpkg_t *pkg;
	struct archive_read_buffer buf;

	ALPM_LOG_FUNC;

	if(db == NULL) {
		RET_ERR(PM_ERR_DB_NULL, -1);
	}

	if(entry != NULL) {
		entryname = archive_entry_pathname(entry);
	}
	if(entryname == NULL) {
		_alpm_log(PM_LOG_DEBUG, "invalid archive entry provided to _alpm_sync_db_read, skipping\n");
		return -1;
	}

	_alpm_log(PM_LOG_FUNCTION, "loading package data from archive entry %s\n",
			entryname);

	memset(&buf, 0, sizeof(buf));
	/* 512K for a line length seems reasonable */
	buf.max_line_size = 512 * 1024;

	/* get package and db file names */
	STRDUP(pkgname, entryname, RET_ERR(PM_ERR_MEMORY, -1));
	p = pkgname + strlen(pkgname);
	for(q = --p; *q && *q != '/'; q--);
	filename = q + 1;
	for(p = --q; *p && *p != '-'; p--);
	for(q = --p; *q && *q != '-'; q--);
	*q = '\0';

	/* package is already in db due to parsing of directory name */
	if(likely_pkg && strcmp(likely_pkg->name, pkgname) == 0) {
		pkg = likely_pkg;
	} else {
		if(db->pkgcache == NULL) {
			RET_ERR(PM_ERR_MEMORY, -1);
		}
		pkg = _alpm_pkghash_find(db->pkgcache, pkgname);
	}
	if(pkg == NULL) {
		_alpm_log(PM_LOG_DEBUG, "package %s not found in %s sync database",
					pkgname, db->treename);
		return -1;
	}

	if(strcmp(filename, "desc") == 0 || strcmp(filename, "depends") == 0
			|| strcmp(filename, "deltas") == 0) {
		while(_alpm_archive_fgets(archive, &buf) == ARCHIVE_OK) {
			char *line = _alpm_strtrim(buf.line);

			if(strcmp(line, "%NAME%") == 0) {
				READ_NEXT(line);
				if(strcmp(line, pkg->name) != 0) {
					_alpm_log(PM_LOG_ERROR, _("%s database is inconsistent: name "
								"mismatch on package %s\n"), db->treename, pkg->name);
				}
			} else if(strcmp(line, "%VERSION%") == 0) {
				READ_NEXT(line);
				if(strcmp(line, pkg->version) != 0) {
					_alpm_log(PM_LOG_ERROR, _("%s database is inconsistent: version "
								"mismatch on package %s\n"), db->treename, pkg->name);
				}
			} else if(strcmp(line, "%FILENAME%") == 0) {
				READ_AND_STORE(pkg->filename);
			} else if(strcmp(line, "%DESC%") == 0) {
				READ_AND_STORE(pkg->desc);
			} else if(strcmp(line, "%GROUPS%") == 0) {
				READ_AND_STORE_ALL(pkg->groups);
			} else if(strcmp(line, "%URL%") == 0) {
				READ_AND_STORE(pkg->url);
			} else if(strcmp(line, "%LICENSE%") == 0) {
				READ_AND_STORE_ALL(pkg->licenses);
			} else if(strcmp(line, "%ARCH%") == 0) {
				READ_AND_STORE(pkg->arch);
			} else if(strcmp(line, "%BUILDDATE%") == 0) {
				READ_NEXT(line);
				pkg->builddate = _alpm_parsedate(line);
			} else if(strcmp(line, "%PACKAGER%") == 0) {
				READ_AND_STORE(pkg->packager);
			} else if(strcmp(line, "%CSIZE%") == 0) {
				/* Note: the CSIZE and SIZE fields both share the "size" field in the
				 * pkginfo_t struct. This can be done b/c CSIZE is currently only used
				 * in sync databases, and SIZE is only used in local databases.
				 */
				READ_NEXT(line);
				pkg->size = atol(line);
				/* also store this value to isize if isize is unset */
				if(pkg->isize == 0) {
					pkg->isize = pkg->size;
				}
			} else if(strcmp(line, "%ISIZE%") == 0) {
				READ_NEXT(line);
				pkg->isize = atol(line);
			} else if(strcmp(line, "%MD5SUM%") == 0) {
				READ_AND_STORE(pkg->md5sum);
			} else if(strcmp(line, "%SHA256SUM%") == 0) {
				/* we don't do anything with this value right now */
				READ_NEXT(line);
			} else if(strcmp(line, "%PGPSIG%") == 0) {
				READ_AND_STORE(pkg->pgpsig.encdata);
			} else if(strcmp(line, "%REPLACES%") == 0) {
				READ_AND_STORE_ALL(pkg->replaces);
			} else if(strcmp(line, "%DEPENDS%") == 0) {
				/* Different than the rest because of the _alpm_splitdep call. */
				while(1) {
					READ_NEXT(line);
					if(strlen(line) == 0) break;
					pkg->depends = alpm_list_add(pkg->depends, _alpm_splitdep(line));
				}
			} else if(strcmp(line, "%OPTDEPENDS%") == 0) {
				READ_AND_STORE_ALL(pkg->optdepends);
			} else if(strcmp(line, "%CONFLICTS%") == 0) {
				READ_AND_STORE_ALL(pkg->conflicts);
			} else if(strcmp(line, "%PROVIDES%") == 0) {
				READ_AND_STORE_ALL(pkg->provides);
			} else if(strcmp(line, "%DELTAS%") == 0) {
				/* Different than the rest because of the _alpm_delta_parse call. */
				while(1) {
					READ_NEXT(line);
					if(strlen(line) == 0) break;
					pkg->deltas = alpm_list_add(pkg->deltas, _alpm_delta_parse(line));
				}
			}
		}
	} else if(strcmp(filename, "files") == 0) {
		/* currently do nothing with this file */
	} else {
		/* unknown database file */
		_alpm_log(PM_LOG_DEBUG, "unknown database file: %s\n", filename);
	}

error:
	FREE(pkgname);
	/* TODO: return 0 always? */
	return 0;
}

static int sync_db_version(pmdb_t *db)
{
	return 2;
}

struct db_operations sync_db_ops = {
	.populate         = sync_db_populate,
	.unregister       = _alpm_db_unregister,
	.version          = sync_db_version,
};

pmdb_t *_alpm_db_register_sync(const char *treename)
{
	pmdb_t *db;
	alpm_list_t *i;

	ALPM_LOG_FUNC;

	for(i = handle->dbs_sync; i; i = i->next) {
		pmdb_t *sdb = i->data;
		if(strcmp(treename, sdb->treename) == 0) {
			_alpm_log(PM_LOG_DEBUG, "attempt to re-register the '%s' database, using existing\n", sdb->treename);
			return sdb;
		}
	}

	_alpm_log(PM_LOG_DEBUG, "registering sync database '%s'\n", treename);

	db = _alpm_db_new(treename, 0);
	if(db == NULL) {
		RET_ERR(PM_ERR_DB_CREATE, NULL);
	}
	db->ops = &sync_db_ops;

	handle->dbs_sync = alpm_list_add(handle->dbs_sync, db);
	return db;
}


/* vim: set ts=2 sw=2 noet: */
