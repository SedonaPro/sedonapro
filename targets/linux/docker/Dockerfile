# SedonaPro VM in a container (emulation, labs, CI).  Built by ./spro docker linux
# from build/packages; the svm is built on Debian 12, so the base must match glibc.
FROM debian:12-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends libmodbus5 libsqlite3-0 \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --system --home-dir /data --shell /usr/sbin/nologin sedona
COPY package/ /opt/sedonapro/package/
COPY entrypoint.sh /opt/sedonapro/entrypoint.sh
RUN mkdir -p /data && chown sedona:sedona /data
USER sedona
WORKDIR /data
VOLUME /data
# SOX, web, GPIO panel of the sim board
EXPOSE 1876/udp 8080/tcp 8090/tcp
ENV SPRO_GPIO_BIND=0.0.0.0
ENTRYPOINT ["/opt/sedonapro/entrypoint.sh"]
