FROM debian:stretch-slim

ARG ANNOUNCE
ARG TLSKEY=temp.key
ARG TLSCERT=temp.cert

LABEL \
    org.label-schema.vendor="The cmail developers <devel@cmail.rocks>" \
    org.label-schema.url="https://cmail.rocks/" \
    org.label-schema.name="cmail server" \
    org.label-schema.vcs-url="https://github.com/cmail-mta/cmail" \
    org.label-schema.license="BSD 2-Clause" \
    org.label-schema.schema-version="1.0"

ENV DEBIAN_FRONTEND=noninteractive

WORKDIR /root
RUN apt-get -y update && \
	apt-get -y --no-install-recommends install build-essential git ca-certificates curl man-db manpages && \
	apt-get -y --no-install-recommends install libsqlite3-dev nettle-dev libgnutls28-dev libadns1-dev && \
	apt-get -y --no-install-recommends install sqlite3 openssl procps && \
	apt-get clean
RUN git clone https://github.com/cmail-mta/cmail
WORKDIR /root/cmail
ENV BANNER=$ANNOUNCE
ENV TLSKEY=$TLSKEY
ENV TLSCERT=$TLSCERT
RUN make
RUN make install
RUN make tls-init
WORKDIR /home/cmail

# Docker can't do conditional copys, so we copy the entire folder anyway
COPY keys /etc/cmail/keys

EXPOSE 25 465 587 110 995

VOLUME ["/etc/cmail/databases"]

ENTRYPOINT ["/root/cmail/docker/run"]

CMD ["all"]
