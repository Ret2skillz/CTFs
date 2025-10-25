FROM ubuntu:25.04@sha256:76d816faff9ffc55af334ac9a26808b021607fffc1a7c4e6fcb3d488d410fdb6 AS chroot

COPY app /
RUN mv /flag.txt /flag-$(md5sum /flag.txt | awk '{print $1}').txt

FROM ghcr.io/google/nsjail/nsjail@sha256:03a904cd42b07ecc6e6f318c1ea14bd28339e1411d180d8744b2a5e2929e895f

COPY --from=chroot / /chroot
COPY nsjail.cfg /nsjail.cfg

CMD ["nsjail", "--config", "/nsjail.cfg"]