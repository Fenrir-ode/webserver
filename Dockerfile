FROM debian:bullseye-slim
ENV DEBIAN_FRONTEND noninteractive
WORKDIR /work
COPY . .
RUN apt-get update && \
    apt-get -y install gcc g++ cmake git && \
    rm -rf /var/lib/apt/lists/*
RUN mkdir build && cd build && cmake .. && make && make install && rm /work -rf


EXPOSE 3000