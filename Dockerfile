# Build Stage
FROM --platform=linux/amd64 ubuntu:20.04 as builder

## Install build dependencies.
RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y libreadline-dev cmake make

ADD . /ravi
WORKDIR /ravi/build
RUN cmake ..
RUN make -j8