# GTA:Orange dedicated server
#
#   docker build -t gta-orange-server .
#   docker run --rm -it -p 7788:7788/udp -p 7789:7789 gta-orange-server

FROM ubuntu:22.04 AS build
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build ca-certificates libmysqlclient-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DORANGE_BUILD_CLIENT=OFF -DORANGE_BUILD_TOOLS=OFF \
 && cmake --build build --parallel \
 && cmake --install build --prefix /opt/gta-orange --component server

FROM ubuntu:22.04
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends libmysqlclient21 \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --system --create-home --home-dir /opt/gta-orange orange
COPY --from=build --chown=orange:orange /opt/gta-orange/server /opt/gta-orange/server
USER orange
WORKDIR /opt/gta-orange/server
EXPOSE 7788/udp 7789/tcp
CMD ["./orange_server"]
