cmail-smtpd
-----------
This folder contains the code for the cmail SMTP server daemon. 
This component of cmail handles communication with remote mail servers
delivering mail for local addresses, as well as with mail clients submitting
new mail by local users for remote addresses (origination).

Build prerequisites
-------------------
libgnutls28-dev		When compiling without CMAIL_NO_TLS
libnettle-dev		(Comes with libgnutls)
libsqlite3-dev

Building
--------
Run make from within this directory to build only cmail-smtpd.
Alternatively, run make in the root directory to build the complete cmail suite.

Configuration
-------------
cmail-smtpd reads the config file given to it as an argument on startup,
binding the requested ports and setting up its operating environment.
Please refer to the website for details of the configuration file format.
An example configuration file may be found in the project root directory
within the folder example-configs.

All runtime configuration is stored in the master database and may be
modified by using cmail-admin-smtpd or the web administration tools and API.
