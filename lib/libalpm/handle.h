/*
 *  handle.h
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
#ifndef _ALPM_HANDLE_H
#define _ALPM_HANDLE_H

#include <stdio.h>
#include <sys/types.h>

#include "alpm_list.h"
#include "alpm.h"

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

struct __pmhandle_t {
	/* internal usage */
	pmdb_t *db_local;       /* local db pointer */
	alpm_list_t *dbs_sync;  /* List of (pmdb_t *) */
	FILE *logstream;        /* log file stream pointer */
	FILE *lckstream;        /* lock file stream pointer if one exists */
	pmtrans_t *trans;

#ifdef HAVE_LIBCURL
	/* libcurl handle */
	CURL *curl;             /* reusable curl_easy handle */
	CURLcode curlerr;       /* last error produced by curl */
#endif

	/* callback functions */
	alpm_cb_log logcb;      /* Log callback function */
	alpm_cb_download dlcb;  /* Download callback function */
	alpm_cb_totaldl totaldlcb;  /* Total download callback function */
	alpm_cb_fetch fetchcb; /* Download file callback function */

	/* filesystem paths */
	char *root;              /* Root path, default '/' */
	char *dbpath;            /* Base path to pacman's DBs */
	char *logfile;           /* Name of the log file */
	char *lockfile;          /* Name of the lock file */
	char *gpgdir;            /* Directory where GnuPG files are stored */
	alpm_list_t *cachedirs;  /* Paths to pacman cache directories */

	/* package lists */
	alpm_list_t *noupgrade;   /* List of packages NOT to be upgraded */
	alpm_list_t *noextract;   /* List of files NOT to extract */
	alpm_list_t *ignorepkg;   /* List of packages to ignore */
	alpm_list_t *ignoregrp;   /* List of groups to ignore */

	/* options */
	int usesyslog;           /* Use syslog instead of logfile? */ /* TODO move to frontend */
	char *arch;              /* Architecture of packages we should allow */
	int usedelta;            /* Download deltas if possible */
	int checkspace;          /* Check disk space before installing */
	pgp_verify_t sigverify;  /* Default signature verification level */

	/* error code */
	enum _pmerrno_t pm_errno;
};

pmhandle_t *_alpm_handle_new(void);
void _alpm_handle_free(pmhandle_t *handle);

int _alpm_handle_lock(pmhandle_t *handle);
int _alpm_handle_unlock(pmhandle_t *handle);

enum _pmerrno_t _alpm_set_directory_option(const char *value,
		char **storage, int must_exist);

#endif /* _ALPM_HANDLE_H */

/* vim: set ts=2 sw=2 noet: */
