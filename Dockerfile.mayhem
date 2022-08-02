# Build Stage
FROM --platform=linux/amd64 ubuntu:20.04 as builder

## Install build dependencies.
RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y libreadline-dev cmake make

ADD . /ravi
WORKDIR /ravi/build
RUN cmake ..
RUN make -j8
RUN mkdir -p /deps
RUN ldd /ravi/build/ravi | tr -s '[:blank:]' '\n' | grep '^/' | xargs -I % sh -c 'cp % /deps;'

FROM ubuntu:20.04 as package

COPY --from=builder /deps /deps
COPY --from=builder /ravi/build/ravi /ravi/build/ravi
ENV LD_LIBRARY_PATH=/deps
