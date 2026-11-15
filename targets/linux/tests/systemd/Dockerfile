# Debian 12 with systemd as PID 1: a disposable controller for install tests.
FROM debian:12
RUN apt-get update \
 && apt-get install -y --no-install-recommends systemd systemd-sysv curl ca-certificates \
      libmodbus5 libsqlite3-0 \
 && rm -rf /var/lib/apt/lists/* \
 && systemctl mask getty.target console-getty.service systemd-logind.service
STOPSIGNAL SIGRTMIN+3
CMD ["/lib/systemd/systemd"]
