/*
 *  database.c
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

#include <stdio.h>

#include <alpm.h>
#include <alpm_list.h>

/* pacman */
#include "pacman.h"
#include "conf.h"
#include "util.h"

/**
 * @brief Modify the 'local' package database.
 *
 * @param targets a list of packages (as strings) to modify
 *
 * @return 0 on success, 1 on failure
 */
int pacman_database(alpm_list_t *targets)
{
	alpm_list_t *i;
	pmdb_t *db_local;
	int retval = 0;
	pmpkgreason_t reason;

	if(targets == NULL) {
		pm_printf(PM_LOG_ERROR, _("no targets specified (use -h for help)\n"));
		return 1;
	}

	if(config->flags & PM_TRANS_FLAG_ALLDEPS) { /* --asdeps */
		reason = PM_PKG_REASON_DEPEND;
	} else if(config->flags & PM_TRANS_FLAG_ALLEXPLICIT) { /* --asexplicit */
		reason = PM_PKG_REASON_EXPLICIT;
	} else {
		pm_printf(PM_LOG_ERROR, _("no install reason specified (use -h for help)\n"));
		return 1;
	}

	/* Lock database */
	if(trans_init(0) == -1) {
		return 1;
	}

	db_local = alpm_option_get_localdb(config->handle);
	for(i = targets; i; i = alpm_list_next(i)) {
		char *pkgname = i->data;
		if(alpm_db_set_pkgreason(db_local, pkgname, reason) == -1) {
			pm_printf(PM_LOG_ERROR, _("could not set install reason for package %s (%s)\n"),
							pkgname, alpm_strerrorlast());
			retval = 1;
		} else {
			if(reason == PM_PKG_REASON_DEPEND) {
				printf(_("%s: install reason has been set to 'installed as dependency'\n"), pkgname);
			} else {
				printf(_("%s: install reason has been set to 'explicitly installed'\n"), pkgname);
			}
		}
	}

	/* Unlock database */
	if(trans_release() == -1) {
		return 1;
	}
	return retval;
}

/* vim: set ts=2 sw=2 noet: */
