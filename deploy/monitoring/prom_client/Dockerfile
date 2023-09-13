FROM ubuntu:22.04
RUN  apt update && \
     apt install -y python3-pip && \
     pip3 install prometheus_client

WORKDIR "/home/monitor"
COPY web_exporter.py .
COPY run.sh .
ENTRYPOINT ["/bin/bash","./run.sh"]

