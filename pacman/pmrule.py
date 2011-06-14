#! /usr/bin/python
#
#  Copyright (c) 2006 by Aurelien Foret <orelien@chez.com>
# 
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

import os
import stat

import util

class pmrule(object):
    """Rule object
    """

    def __init__(self, rule):
        self.rule = rule
        self.false = 0
        self.result = 0

    def __str__(self):
        if len(self.rule) <= 40:
            return self.rule
        return self.rule[:37] + '...'

    def check(self, test):
        """
        """
        success = 1

        [testname, args] = self.rule.split("=")
        if testname[0] == "!":
            self.false = 1
            testname = testname.lstrip("!")
        [kind, case] = testname.split("_")
        if "|" in args:
            [key, value] = args.split("|", 1)
        else:
            [key, value] = [args, None]

        if kind == "PACMAN":
            if case == "RETCODE":
                if test.retcode != int(key):
                    success = 0
            elif case == "OUTPUT":
                logfile = os.path.join(test.root, util.LOGFILE)
                if not os.access(logfile, os.F_OK):
                    print "LOGFILE not found, cannot validate 'OUTPUT' rule"
                    success = 0
                elif not util.grep(logfile, key):
                    success = 0
            else:
                print "PACMAN rule '%s' not found" % case
                success = -1
        elif kind == "PKG":
            localdb = test.db["local"]
            newpkg = localdb.db_read(key)
            if not newpkg:
                success = 0
            else:
                if case == "EXIST":
                    success = 1
                elif case == "VERSION":
                    if value != newpkg.version:
                        success = 0
                elif case == "DESC":
                    if value != newpkg.desc:
                        success = 0
                elif case == "GROUPS":
                    if not value in newpkg.groups:
                        success = 0
                elif case == "PROVIDES":
                    if not value in newpkg.provides:
                        success = 0
                elif case == "DEPENDS":
                    if not value in newpkg.depends:
                        success = 0
                elif case == "OPTDEPENDS":
                    if not value in newpkg.optdepends:
                        success = 0
                elif case == "REASON":
                    if newpkg.reason != int(value):
                        success = 0
                elif case == "FILES":
                    if not value in newpkg.files:
                        success = 0
                elif case == "BACKUP":
                    found = 0
                    for f in newpkg.backup:
                        name, md5sum = f.split("\t")
                        if value == name:
                            found = 1
                    if not found:
                        success = 0
                else:
                    print "PKG rule '%s' not found" % case
                    success = -1
        elif kind == "FILE":
            filename = os.path.join(test.root, key)
            if case == "EXIST":
                if not os.path.isfile(filename):
                    success = 0
            elif case == "MODIFIED":
                for f in test.files:
                    if f.name == key:
                        if not f.ismodified():
                            success = 0
                        break
            elif case == "MODE":
                if not os.path.isfile(filename):
                    success = 0
                else:
                    mode = os.lstat(filename)[stat.ST_MODE]
                    if int(value, 8) != stat.S_IMODE(mode):
                        success = 0
            elif case == "TYPE":
                if value == "dir":
                    if not os.path.isdir(filename):
                        success = 0
                elif value == "file":
                    if not os.path.isfile(filename):
                        success = 0
                elif value == "link":
                    if not os.path.islink(filename):
                        success = 0
            elif case == "PACNEW":
                if not os.path.isfile("%s.pacnew" % filename):
                    success = 0
            elif case == "PACORIG":
                if not os.path.isfile("%s.pacorig" % filename):
                    success = 0
            elif case == "PACSAVE":
                if not os.path.isfile("%s.pacsave" % filename):
                    success = 0
            else:
                print "FILE rule '%s' not found" % case
                success = -1
        elif kind == "LINK":
            filename = os.path.join(test.root, key)
            if case == "EXIST":
                if not os.path.islink(filename):
                    success = 0
            else:
                print "LINK rule '%s' not found" % case
                success = -1
        elif kind == "CACHE":
            cachedir = os.path.join(test.root, util.PM_CACHEDIR)
            if case == "EXISTS":
                pkg = test.findpkg(key, value, allow_local=True)
                if not pkg or not os.path.isfile(
                        os.path.join(cachedir, pkg.filename())):
                    success = 0
        else:
            print "Rule kind '%s' not found" % kind
            success = -1

        if self.false and success != -1:
            success = not success
        self.result = success
        return success

# vim: set ts=4 sw=4 et:
