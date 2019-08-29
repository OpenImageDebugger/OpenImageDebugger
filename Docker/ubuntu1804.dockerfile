# prebuilt image of Ubuntu 18.04
FROM  ubuntu:bionic

# install dependencies
RUN apt update -y && apt upgrade -y && apt install -y \
    python3-dev \
    qt5-default \
    build-essential \
    git \
    pkg-config
