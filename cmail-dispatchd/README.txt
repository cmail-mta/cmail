cmail-dispatchd
---------------
This folder contains the code for the cmail SMTP client daemon. 
This component handles delivery of outbound and originated mail to remote
SMTP servers.

Build prerequisites
-------------------
libadns1-dev
libgnutls28-dev		When compiling without CMAIL_NO_TLS
libnettle-dev		(Comes with libgnutls)
libsqlite3-dev

Building
--------
Run make from within this directory to build only cmail-dispatchd.
Alternatively, run make in the root directory to build the complete cmail suite.

Configuration
-------------
cmail-dispatchd reads the config file given to it as an argument on startup.
Please refer to the website for details of the configuration file format.
An example configuration file may be found in the project root directory
within the folder example-configs.
