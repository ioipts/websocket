FROM alpine:3.14
RUN apk --no-cache add gcc g++ make
EXPOSE 8027
CMD ["./websocket", "38894"]
RUN mkdir -p /src
RUN mkdir -p /src/sample
RUN mkdir -p /data
WORKDIR /src

COPY websockserver2.* /src/
COPY ./sample/mainwebsockserver.cpp /src/sample/
COPY ./sample/makefile /src/sample/
WORKDIR /src/sample
RUN make