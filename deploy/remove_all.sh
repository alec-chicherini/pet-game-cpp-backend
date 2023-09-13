#!/bin/bash
echo " -- Docker clean up BEGIN"
for instance in node_exporter prom grafana prom_client my_http_server postgres_for_server; do
 docker container stop $instance 1> /dev/null 2> /dev/null
 docker container rm $instance  1> /dev/null 2> /dev/null
 docker image rm $instance 1>/dev/null 2> /dev/null
done
docker network rm monitoring_network 1> /dev/null 2> /dev/null
echo " -- Docker clean up END"
