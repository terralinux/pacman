/*
 *  remove.c
 *
 *  Copyright (c) 2006-2011 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
 *  Copyright (c) 2006 by David Kimpe <dnaku@frugalware.org>
 *  Copyright (c) 2005, 2006 by Miklos Vajna <vmiklos@frugalware.org>
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

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

/* libalpm */
#include "remove.h"
#include "alpm_list.h"
#include "alpm.h"
#include "trans.h"
#include "util.h"
#include "log.h"
#include "backup.h"
#include "package.h"
#include "db.h"
#include "deps.h"
#include "handle.h"

int SYMEXPORT alpm_remove_pkg(pmhandle_t *handle, pmpkg_t *pkg)
{
	const char *pkgname;
	pmtrans_t *trans;

	/* Sanity checks */
	CHECK_HANDLE(handle, return -1);
	ASSERT(pkg != NULL, RET_ERR(handle, PM_ERR_WRONG_ARGS, -1));
	ASSERT(handle == pkg->handle, RET_ERR(handle, PM_ERR_WRONG_ARGS, -1));
	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(handle, PM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED,
			RET_ERR(handle, PM_ERR_TRANS_NOT_INITIALIZED, -1));

	pkgname = pkg->name;

	if(_alpm_pkg_find(trans->remove, pkgname)) {
		RET_ERR(handle, PM_ERR_TRANS_DUP_TARGET, -1);
	}

	_alpm_log(handle, PM_LOG_DEBUG, "adding package %s to the transaction remove list\n",
			pkgname);
	trans->remove = alpm_list_add(trans->remove, _alpm_pkg_dup(pkg));
	return 0;
}

static void remove_prepare_cascade(pmhandle_t *handle, alpm_list_t *lp)
{
	pmtrans_t *trans = handle->trans;

	while(lp) {
		alpm_list_t *i;
		for(i = lp; i; i = i->next) {
			pmdepmissing_t *miss = (pmdepmissing_t *)i->data;
			pmpkg_t *info = _alpm_db_get_pkgfromcache(handle->db_local, miss->target);
			if(info) {
				if(!_alpm_pkg_find(trans->remove, alpm_pkg_get_name(info))) {
					_alpm_log(handle, PM_LOG_DEBUG, "pulling %s in target list\n",
							alpm_pkg_get_name(info));
					trans->remove = alpm_list_add(trans->remove, _alpm_pkg_dup(info));
				}
			} else {
				_alpm_log(handle, PM_LOG_ERROR, _("could not find %s in database -- skipping\n"),
									miss->target);
			}
		}
		alpm_list_free_inner(lp, (alpm_list_fn_free)_alpm_depmiss_free);
		alpm_list_free(lp);
		lp = alpm_checkdeps(handle, _alpm_db_get_pkgcache(handle->db_local),
				trans->remove, NULL, 1);
	}
}

static void remove_prepare_keep_needed(pmhandle_t *handle, alpm_list_t *lp)
{
	pmtrans_t *trans = handle->trans;

	/* Remove needed packages (which break dependencies) from target list */
	while(lp != NULL) {
		alpm_list_t *i;
		for(i = lp; i; i = i->next) {
			pmdepmissing_t *miss = (pmdepmissing_t *)i->data;
			void *vpkg;
			pmpkg_t *pkg = _alpm_pkg_find(trans->remove, miss->causingpkg);
			if(pkg == NULL) {
				continue;
			}
			trans->remove = alpm_list_remove(trans->remove, pkg, _alpm_pkg_cmp,
					&vpkg);
			pkg = vpkg;
			if(pkg) {
				_alpm_log(handle, PM_LOG_WARNING, _("removing %s from target list\n"),
						alpm_pkg_get_name(pkg));
				_alpm_pkg_free(pkg);
			}
		}
		alpm_list_free_inner(lp, (alpm_list_fn_free)_alpm_depmiss_free);
		alpm_list_free(lp);
		lp = alpm_checkdeps(handle, _alpm_db_get_pkgcache(handle->db_local),
				trans->remove, NULL, 1);
	}
}

/** Transaction preparation for remove actions.
 * This functions takes a pointer to a alpm_list_t which will be
 * filled with a list of pmdepmissing_t* objects representing
 * the packages blocking the transaction.
 * @param handle the context handle
 * @param data a pointer to an alpm_list_t* to fill
 */
int _alpm_remove_prepare(pmhandle_t *handle, alpm_list_t **data)
{
	alpm_list_t *lp;
	pmtrans_t *trans = handle->trans;
	pmdb_t *db = handle->db_local;

	if((trans->flags & PM_TRANS_FLAG_RECURSE) && !(trans->flags & PM_TRANS_FLAG_CASCADE)) {
		_alpm_log(handle, PM_LOG_DEBUG, "finding removable dependencies\n");
		_alpm_recursedeps(db, trans->remove,
				trans->flags & PM_TRANS_FLAG_RECURSEALL);
	}

	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_START, NULL, NULL);

		_alpm_log(handle, PM_LOG_DEBUG, "looking for unsatisfied dependencies\n");
		lp = alpm_checkdeps(handle, _alpm_db_get_pkgcache(db), trans->remove, NULL, 1);
		if(lp != NULL) {

			if(trans->flags & PM_TRANS_FLAG_CASCADE) {
				remove_prepare_cascade(handle, lp);
			} else if(trans->flags & PM_TRANS_FLAG_UNNEEDED) {
				/* Remove needed packages (which would break dependencies)
				 * from target list */
				remove_prepare_keep_needed(handle, lp);
			} else {
				if(data) {
					*data = lp;
				} else {
					alpm_list_free_inner(lp, (alpm_list_fn_free)_alpm_depmiss_free);
					alpm_list_free(lp);
				}
				RET_ERR(handle, PM_ERR_UNSATISFIED_DEPS, -1);
			}
		}
	}

	/* re-order w.r.t. dependencies */
	_alpm_log(handle, PM_LOG_DEBUG, "sorting by dependencies\n");
	lp = _alpm_sortbydeps(handle, trans->remove, 1);
	/* free the old alltargs */
	alpm_list_free(trans->remove);
	trans->remove = lp;

	/* -Rcs == -Rc then -Rs */
	if((trans->flags & PM_TRANS_FLAG_CASCADE) && (trans->flags & PM_TRANS_FLAG_RECURSE)) {
		_alpm_log(handle, PM_LOG_DEBUG, "finding removable dependencies\n");
		_alpm_recursedeps(db, trans->remove, trans->flags & PM_TRANS_FLAG_RECURSEALL);
	}

	if(!(trans->flags & PM_TRANS_FLAG_NODEPS)) {
		EVENT(trans, PM_TRANS_EVT_CHECKDEPS_DONE, NULL, NULL);
	}

	return 0;
}

static int can_remove_file(pmhandle_t *handle, const char *path,
		alpm_list_t *skip_remove)
{
	char file[PATH_MAX];

	snprintf(file, PATH_MAX, "%s%s", handle->root, path);

	if(alpm_list_find_str(skip_remove, file)) {
		/* return success because we will never actually remove this file */
		return 1;
	}
	/* If we fail write permissions due to a read-only filesystem, abort.
	 * Assume all other possible failures are covered somewhere else */
	if(access(file, W_OK) == -1) {
		if(errno != EACCES && errno != ETXTBSY && access(file, F_OK) == 0) {
			/* only return failure if the file ACTUALLY exists and we can't write to
			 * it - ignore "chmod -w" simple permission failures */
			_alpm_log(handle, PM_LOG_ERROR, _("cannot remove file '%s': %s\n"),
					file, strerror(errno));
			return 0;
		}
	}

	return 1;
}

/* Helper function for iterating through a package's file and deleting them
 * Used by _alpm_remove_commit. */
static void unlink_file(pmhandle_t *handle, pmpkg_t *info, const char *filename,
		alpm_list_t *skip_remove, int nosave)
{
	struct stat buf;
	char file[PATH_MAX];

	snprintf(file, PATH_MAX, "%s%s", handle->root, filename);

	/* check the remove skip list before removing the file.
	 * see the big comment block in db_find_fileconflicts() for an
	 * explanation. */
	if(alpm_list_find_str(skip_remove, filename)) {
		_alpm_log(handle, PM_LOG_DEBUG, "%s is in skip_remove, skipping removal\n",
				file);
		return;
	}

	/* we want to do a lstat here, and not a _alpm_lstat.
	 * if a directory in the package is actually a directory symlink on the
	 * filesystem, we want to work with the linked directory instead of the
	 * actual symlink */
	if(lstat(file, &buf)) {
		_alpm_log(handle, PM_LOG_DEBUG, "file %s does not exist\n", file);
		return;
	}

	if(S_ISDIR(buf.st_mode)) {
		if(rmdir(file)) {
			/* this is okay, other packages are probably using it (like /usr) */
			_alpm_log(handle, PM_LOG_DEBUG, "keeping directory %s\n", file);
		} else {
			_alpm_log(handle, PM_LOG_DEBUG, "removing directory %s\n", file);
		}
	} else {
		/* if the file needs backup and has been modified, back it up to .pacsave */
		pmbackup_t *backup = _alpm_needbackup(filename, alpm_pkg_get_backup(info));
		if(backup) {
			if(nosave) {
				_alpm_log(handle, PM_LOG_DEBUG, "transaction is set to NOSAVE, not backing up '%s'\n", file);
			} else {
				char *filehash = alpm_compute_md5sum(file);
				int cmp = filehash ? strcmp(filehash, backup->hash) : 0;
				FREE(filehash);
				if(cmp != 0) {
					char newpath[PATH_MAX];
					snprintf(newpath, PATH_MAX, "%s.pacsave", file);
					rename(file, newpath);
					_alpm_log(handle, PM_LOG_WARNING, _("%s saved as %s\n"), file, newpath);
					alpm_logaction(handle, "warning: %s saved as %s\n", file, newpath);
					return;
				}
			}
		}

		_alpm_log(handle, PM_LOG_DEBUG, "unlinking %s\n", file);

		if(unlink(file) == -1) {
			_alpm_log(handle, PM_LOG_ERROR, _("cannot remove file '%s': %s\n"),
					filename, strerror(errno));
		}
	}
}

int _alpm_upgraderemove_package(pmhandle_t *handle,
		pmpkg_t *oldpkg, pmpkg_t *newpkg)
{
	alpm_list_t *skip_remove, *b;
	alpm_list_t *newfiles, *lp;
	size_t filenum = 0;
	alpm_list_t *files = alpm_pkg_get_files(oldpkg);
	const char *pkgname = alpm_pkg_get_name(oldpkg);

	_alpm_log(handle, PM_LOG_DEBUG, "removing old package first (%s-%s)\n",
			oldpkg->name, oldpkg->version);

	if(handle->trans->flags & PM_TRANS_FLAG_DBONLY) {
		goto db;
	}

	/* copy the remove skiplist over */
	skip_remove = alpm_list_join(
			alpm_list_strdup(handle->trans->skip_remove),
			alpm_list_strdup(handle->noupgrade));
	/* Add files in the NEW backup array to the skip_remove array
	 * so this removal operation doesn't kill them */
	/* old package backup list */
	alpm_list_t *filelist = alpm_pkg_get_files(newpkg);
	for(b = alpm_pkg_get_backup(newpkg); b; b = b->next) {
		const pmbackup_t *backup = b->data;
		/* safety check (fix the upgrade026 pactest) */
		if(!alpm_list_find_str(filelist, backup->name)) {
			continue;
		}
		_alpm_log(handle, PM_LOG_DEBUG, "adding %s to the skip_remove array\n",
				backup->name);
		skip_remove = alpm_list_add(skip_remove, strdup(backup->name));
	}

	for(lp = files; lp; lp = lp->next) {
		if(!can_remove_file(handle, lp->data, skip_remove)) {
			_alpm_log(handle, PM_LOG_DEBUG,
					"not removing package '%s', can't remove all files\n", pkgname);
			RET_ERR(handle, PM_ERR_PKG_CANT_REMOVE, -1);
		}
		filenum++;
	}

	_alpm_log(handle, PM_LOG_DEBUG, "removing %ld files\n", (unsigned long)filenum);

	/* iterate through the list backwards, unlinking files */
	newfiles = alpm_list_reverse(files);
	for(lp = newfiles; lp; lp = alpm_list_next(lp)) {
		unlink_file(handle, oldpkg, lp->data, skip_remove, 0);
	}
	alpm_list_free(newfiles);
	FREELIST(skip_remove);

db:
	/* remove the package from the database */
	_alpm_log(handle, PM_LOG_DEBUG, "updating database\n");
	_alpm_log(handle, PM_LOG_DEBUG, "removing database entry '%s'\n", pkgname);
	if(_alpm_local_db_remove(handle->db_local, oldpkg) == -1) {
		_alpm_log(handle, PM_LOG_ERROR, _("could not remove database entry %s-%s\n"),
				pkgname, alpm_pkg_get_version(oldpkg));
	}
	/* remove the package from the cache */
	if(_alpm_db_remove_pkgfromcache(handle->db_local, oldpkg) == -1) {
		_alpm_log(handle, PM_LOG_ERROR, _("could not remove entry '%s' from cache\n"),
				pkgname);
	}

	return 0;
}

int _alpm_remove_packages(pmhandle_t *handle)
{
	pmpkg_t *info;
	alpm_list_t *targ, *lp;
	size_t pkg_count;
	pmtrans_t *trans = handle->trans;

	pkg_count = alpm_list_count(trans->remove);

	for(targ = trans->remove; targ; targ = targ->next) {
		int position = 0;
		char scriptlet[PATH_MAX];
		info = (pmpkg_t *)targ->data;
		const char *pkgname = NULL;
		size_t targcount = alpm_list_count(targ);

		if(trans->state == STATE_INTERRUPTED) {
			return 0;
		}

		/* get the name now so we can use it after package is removed */
		pkgname = alpm_pkg_get_name(info);
		snprintf(scriptlet, PATH_MAX, "%s%s-%s/install",
				_alpm_db_path(handle->db_local), pkgname, alpm_pkg_get_version(info));

		EVENT(trans, PM_TRANS_EVT_REMOVE_START, info, NULL);
		_alpm_log(handle, PM_LOG_DEBUG, "removing package %s-%s\n",
				pkgname, alpm_pkg_get_version(info));

		/* run the pre-remove scriptlet if it exists  */
		if(alpm_pkg_has_scriptlet(info) && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
			_alpm_runscriptlet(handle, scriptlet, "pre_remove",
					alpm_pkg_get_version(info), NULL);
		}

		if(!(trans->flags & PM_TRANS_FLAG_DBONLY)) {
			alpm_list_t *files = alpm_pkg_get_files(info);
			alpm_list_t *newfiles;
			size_t filenum = 0;

			for(lp = files; lp; lp = lp->next) {
				if(!can_remove_file(handle, lp->data, NULL)) {
					_alpm_log(handle, PM_LOG_DEBUG, "not removing package '%s', can't remove all files\n",
					          pkgname);
					RET_ERR(handle, PM_ERR_PKG_CANT_REMOVE, -1);
				}
				filenum++;
			}

			_alpm_log(handle, PM_LOG_DEBUG, "removing %ld files\n", (unsigned long)filenum);

			/* init progress bar */
			PROGRESS(trans, PM_TRANS_PROGRESS_REMOVE_START, info->name, 0,
					pkg_count, (pkg_count - targcount + 1));

			/* iterate through the list backwards, unlinking files */
			newfiles = alpm_list_reverse(files);
			for(lp = newfiles; lp; lp = alpm_list_next(lp)) {
				int percent;
				unlink_file(handle, info, lp->data, NULL, trans->flags & PM_TRANS_FLAG_NOSAVE);

				/* update progress bar after each file */
				percent = (position * 100) / filenum;
				PROGRESS(trans, PM_TRANS_PROGRESS_REMOVE_START, info->name,
						percent, pkg_count, (pkg_count - targcount + 1));
				position++;
			}
			alpm_list_free(newfiles);
		}

		/* set progress to 100% after we finish unlinking files */
		PROGRESS(trans, PM_TRANS_PROGRESS_REMOVE_START, pkgname, 100,
		         pkg_count, (pkg_count - targcount + 1));

		/* run the post-remove script if it exists  */
		if(alpm_pkg_has_scriptlet(info) && !(trans->flags & PM_TRANS_FLAG_NOSCRIPTLET)) {
			_alpm_runscriptlet(handle, scriptlet, "post_remove",
					alpm_pkg_get_version(info), NULL);
		}

		/* remove the package from the database */
		_alpm_log(handle, PM_LOG_DEBUG, "updating database\n");
		_alpm_log(handle, PM_LOG_DEBUG, "removing database entry '%s'\n", pkgname);
		if(_alpm_local_db_remove(handle->db_local, info) == -1) {
			_alpm_log(handle, PM_LOG_ERROR, _("could not remove database entry %s-%s\n"),
			          pkgname, alpm_pkg_get_version(info));
		}
		/* remove the package from the cache */
		if(_alpm_db_remove_pkgfromcache(handle->db_local, info) == -1) {
			_alpm_log(handle, PM_LOG_ERROR, _("could not remove entry '%s' from cache\n"),
			          pkgname);
		}

		EVENT(trans, PM_TRANS_EVT_REMOVE_DONE, info, NULL);
	}

	/* run ldconfig if it exists */
	_alpm_ldconfig(handle);

	return 0;
}

/* vim: set ts=2 sw=2 noet: */
