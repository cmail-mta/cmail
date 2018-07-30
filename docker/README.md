# cmail Docker container

This container allows you to run a full cmail installation from source within a Docker container.
Note that, due to the fact that cmail is installed from source, an entire build environment is installed
within the container, somewhat pushing up the size.

Still, for quick testing and experimentation, this should do.

The persistent database is stored in a volume, so you should be able to edit it from the host, too.

## Build parameters

* *ANNOUNCE*: The banner your server will announce to clients/peers. This should probably be your hostname or IP address.
* *TLSCERT*: The name of your TLS certificate file in keys/. If unset, the installer should generate a temporary snake-oil key.
* *TLSKEY*: The name of your TLS key file in keys/. If unset, the installer should generate a temporary snake-oil key.

Fixes, changes, additions, bug reports, etc very welcome!
