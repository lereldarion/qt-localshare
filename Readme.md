LocalShare - Local file sharing application
===========================================

Small graphical application to send a file to a local peer.

Status
------

Done:
* mDNS browsing
* settings

Todo:
* interface
* transfer protocol

Setup
-----

Compilation:
> qmake
> make

Requires Qt5 and c++11 compiler support.

Binary is standalone and doesn't need any files to work (except for Qt libraries that should be installed system wide for convenience).
It may store some settings at user level (storage depends on the system, see the QtCore/QSettings documentation).

mDNS browsing and resolution
----------------------------

This application uses *Zeroconf* to find its peers on the local network.
More precisely, it uses the *Bonjour* API to implement mDNS Service Discovery.
*Zeroconf* support depends on the system:
* Linux:
	- Support through *Avahi* daemon and libraries
	- *Avahi* should be configured to resolve `.local` hostnames (https://wiki.archlinux.org/index.php/avahi#Hostname_resolution)
	- *Bonjour* API compatibility layer should be installed (`avahi-compat-libdns_sd`)
* Mac:
	- **Not tested**
	- Should not need any specific setup as Bonjour is native to Mac
* Windows:
	- **Not tested**
	- Support through *mDNSResponder* (Bonjour Windows service, provided by apple)
