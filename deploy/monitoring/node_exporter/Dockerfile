FROM ubuntu:22.04

RUN apt update && apt install -y wget

WORKDIR "/home/monitor"

RUN wget https://github.com/prometheus/node_exporter/releases/download/v1.4.0/node_exporter-1.4.0.linux-amd64.tar.gz && \
tar xvzf node_exporter-1.4.0.linux-amd64.tar.gz 

ENTRYPOINT ["./node_exporter-1.4.0.linux-amd64/node_exporter"]

