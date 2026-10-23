# ns-3.46.1 build environment for the FutureNet M01 (futurenet-ntn) project.
# ns-3 is downloaded and fully built inside the image, so containers start ready to use.
# The module source is bind-mounted from the host into contrib/futurenet-ntn at run time.
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        g++ cmake ninja-build python3 python3-matplotlib git ccache \
        clang-tidy python3-pip wget bzip2 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# ns-3.46.1's utils/check-style-clang-format.py only accepts clang-format 20, which Ubuntu 24.04
# does not package; the official clang-format wheel from PyPI provides it.
RUN pip3 install --break-system-packages --no-cache-dir "clang-format==20.1.8"

WORKDIR /opt
RUN wget -q https://www.nsnam.org/releases/ns-allinone-3.46.1.tar.bz2 \
    && tar xjf ns-allinone-3.46.1.tar.bz2 \
    && rm ns-allinone-3.46.1.tar.bz2

# Build only the ns-3 modules futurenet-ntn needs (their dependencies are pulled in automatically).
# Building everything (incl. the large contrib/nr 5G module) exhausted Docker's memory, and a
# limited parallel job count keeps peak RAM low. futurenet-ntn is appended to this list when the
# module is mounted and ns-3 is re-configured (see PROJECT_LOG.md, step 0.7).
ARG NS3_MODULES="core;network;mobility;internet;point-to-point;applications;flow-monitor"
ARG BUILD_JOBS=4
WORKDIR /opt/ns-allinone-3.46.1/ns-3.46.1
RUN ./ns3 configure --enable-examples --enable-tests -G Ninja \
        --enable-modules="${NS3_MODULES}" \
    && ./ns3 build -j ${BUILD_JOBS}

CMD ["bash"]
