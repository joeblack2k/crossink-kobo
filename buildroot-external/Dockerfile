FROM debian@sha256:60eac759739651111db372c07be67863818726f754804b8707c90979bda511df

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        bc \
        bison \
        build-essential \
        ca-certificates \
        cpio \
        e2fsprogs \
        fakeroot \
        file \
        fdisk \
        flex \
        git \
        libncurses-dev \
        locales \
        patch \
        perl \
        python3 \
        rsync \
        u-boot-tools \
        unzip \
        wget \
        xz-utils \
    && rm -rf /var/lib/apt/lists/*

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8
WORKDIR /workspace
