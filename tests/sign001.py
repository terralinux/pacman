self.description = "Add a signature to a package DB"

sp = pmpkg("pkg1")
sp.pgpsig = "asdfasdfsdfasdfsdafasdfsdfasd"
self.addpkg2db("sync+Always", sp)

self.args = "-Ss"

self.addrule("PACMAN_RETCODE=0")
