/*
 *  callback.h
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
#ifndef _PM_CALLBACK_H
#define _PM_CALLBACK_H

#include <sys/types.h> /* off_t */

#include <alpm.h>

/* callback to handle messages/notifications from libalpm transactions */
void cb_trans_evt(pmtransevt_t event, void *data1, void *data2);

/* callback to handle questions from libalpm transactions (yes/no) */
void cb_trans_conv(pmtransconv_t event, void *data1, void *data2,
                   void *data3, int *response);

/* callback to handle display of transaction progress */
void cb_trans_progress(pmtransprog_t event, const char *pkgname, int percent,
                   size_t howmany, size_t remain);

/* callback to handle receipt of total download value */
void cb_dl_total(off_t total);
/* callback to handle display of download progress */
void cb_dl_progress(const char *filename, off_t file_xfered, off_t file_total);

/* callback to handle messages/notifications from pacman library */
void cb_log(pmloglevel_t level, const char *fmt, va_list args);

#endif /* _PM_CALLBACK_H */

/* vim: set ts=2 sw=2 noet: */
