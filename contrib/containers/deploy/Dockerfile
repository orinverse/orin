FROM phusion/baseimage:jammy-1.0.4
LABEL maintainer="Orin Developers <dev@orin.org>"
LABEL description="Dockerised OrinCore, built from CI"

ARG USER_ID
ARG GROUP_ID

ENV HOME="/home/orin"

# add user with specified (or default) user/group ids
ENV USER_ID="${USER_ID:-1000}"
ENV GROUP_ID="${GROUP_ID:-1000}"
RUN groupadd -g ${GROUP_ID} orin && \
    useradd -u ${USER_ID} -g orin -s /bin/bash -m -d /home/orin orin && \
    mkdir /home/orin/.orincore && \
    chown ${USER_ID}:${GROUP_ID} -R /home/orin

COPY bin/* /usr/local/bin/

RUN chmod a+x /usr/local/bin/*

USER orin

VOLUME ["/home/orin"]

EXPOSE 9998 9999 19998 19999

WORKDIR /home/orin
