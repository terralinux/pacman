self.description = "Install a package with an existing file (--force)"

p = pmpkg("dummy")
p.files = ["etc/dummy.conf"]
self.addpkg(p)

self.filesystem = ["etc/dummy.conf"]

self.args = "-Uf %s" % p.filename()

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=dummy")
self.addrule("FILE_MODIFIED=etc/dummy.conf")
self.addrule("!FILE_PACNEW=etc/dummy.conf")
self.addrule("!FILE_PACORIG=etc/dummy.conf")
