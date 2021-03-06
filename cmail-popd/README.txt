cmail-popd
----------
This folder contains the code for the cmail POP3 daemon. It allows cmail users 
to access their mailboxes via a simple protocol supported by most mail clients.

Build prerequisites
-------------------
libgnutls28-dev		When compiling without CMAIL_NO_TLS
libnettle-dev		(Comes with libgnutls)
libsqlite3-dev

Building
--------
Run make from within this directory to build only cmail-popd.
Alternatively, run make in the root directory to build the complete cmail suite.

Configuration
-------------
cmail-popd reads the config file given to it as an argument on startup,
binding the requested ports and setting up its operating environment.
Please refer to the website for details of the configuration file format.
An example configuration file may be found in the project root directory
within the folder example-configs.

All runtime configuration is stored in the master database and may be
modified by using cmail-admin-popd or the web administration tools and API.
