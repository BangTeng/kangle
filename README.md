# kangle[![Build Status](https://www.travis-ci.org/bangteng/kangle.svg?branch=master)](https://www.travis-ci.org/bangteng/kangle) [![StyleCI](https://styleci.io/repos/112868742/shield?branch=master)](https://styleci.io/repos/112868742)
kangle is a light, high-performance web server/reverse proxy.support fastcgi/isapi/ajp/uwsgi/scgi/hmux protocol.include a http manage console. Full support access control. memory/disk cache. virtual host can run in seperate process and user. and more kangle web server features

kangle Web Server can improve performance, security and reliability of your server, by a great margin

fastcgi/ajp/uwsgi/scgi/hmux/http protocol upstream
support asp/asp.net(windows version).
upstream keep alive
memory and disk cache.
full request/response access control
isapi/cgi support
easy used web manage console.
virtualhost based on host or port
each virtualhost can run seperate process and user.
.htaccess rewrite rule support.
xml config file
on the fly gzip
Apache compatible log files
ipv6 ready

	./configure --help to obtain the configure help.
	./configure --prefix=/usr/local/
	make
	make install
	config file is etc/config.xml
	virtualHost file is etc/vh.xml
	bin/kangle to start the server
	bin/kangle -q stop the server
	bin/kangle -h help

open url http://ip:3311/
enter user and password to monitor and manage the server.
default user is admin, password is kangle.

Kangle is released under the terms of the GPL license.

See COPYING for more information or see https://en.wikipedia.org/wiki/GNU_General_Public_License