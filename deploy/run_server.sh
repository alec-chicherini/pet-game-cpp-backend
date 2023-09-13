#!/usr/bin/bash
sleep 5
IS_USER_IS_ROOT=$(id -u)
if [[ $IS_USER_IS_ROOT -eq 0 ]]; then
 echo "Script must be run by user. Not root"
 exit 1
fi

SCRIPT_DIR=$(realpath "$(dirname "${BASH_SOURCE[0]}")")
SERVER_SOURCE_DIR=$SCRIPT_DIR/..
DEPLOY_SERVER_BUILD_DIR=$SCRIPT_DIR/build

IP_SUB_NET=172.172.0.0/24
IP_NODE_EXPORTER=172.172.0.4
IP_PROMETHEUS=172.172.0.2
IP_GRAFANA=172.172.0.3
IP_PROM_CLIENT=172.172.0.5

IP_SUB_NET_POSTGRES=172.172.1.0/24
IP_POSTGRES=172.172.1.2

#PREPARE SERVER BUILD FILES
mkdir $DEPLOY_SERVER_BUILD_DIR > /dev/null
rm -rf $DEPLOY_SERVER_BUILD_DIR/*
cp -rf $SERVER_SOURCE_DIR/src $DEPLOY_SERVER_BUILD_DIR
cp -f $SERVER_SOURCE_DIR/conanfile.txt $DEPLOY_SERVER_BUILD_DIR
cp -f $SERVER_SOURCE_DIR/CMakeLists.txt $DEPLOY_SERVER_BUILD_DIR
cp -rf $SERVER_SOURCE_DIR/data $DEPLOY_SERVER_BUILD_DIR
cp -rf $SERVER_SOURCE_DIR/static $DEPLOY_SERVER_BUILD_DIR
cp -f $SCRIPT_DIR/my_http_server/Dockerfile $DEPLOY_SERVER_BUILD_DIR > /dev/null
#

#NETWORK BEGIN
docker network inspect monitoring_network > /dev/null
IS_NETWORK_EXISTS=$?

if [[ $IS_NETWORK_EXISTS -eq 1 ]]; then
 echo " -- Docker network for monitoring:Create"
 docker network create --subnet=$IP_SUB_NET monitoring_network
else
 echo " -- Docker network for monitoring:Exists"
fi

docker network inspect postgres_network > /dev/null
IS_NETWORK_POSTGRES_EXISTS=$?

if [[ $IS_NETWORK_POSTGRES_EXISTS -eq 1 ]]; then
 echo " -- Docker network for postgre:Create"
 docker network create --subnet=$IP_SUB_NET_POSTGRES postgres_network
else
 echo " -- Docker network for postgres:Exists"
fi
#NETWORK END

#BUILD BEGIN
is_builded(){
 IMAGE=$(docker images -q $1 2> /dev/null)
 if [[ -z "$IMAGE" ]]; then
  echo 1
 else
  echo 0
 fi
}

if [[ $(is_builded my_http_server) -ne 0 ]]; then
 echo " -- Docker my_http_server:Building"
 docker build -t my_http_server $DEPLOY_SERVER_BUILD_DIR
else
 echo " -- Docker my_http_server:Exists"
fi

if [[ $(is_builded postgres_for_server) -ne 0 ]]; then
 echo " -- Docker postgres_for_server:Building"
 docker build -t postgres_for_server $SCRIPT_DIR/../deploy/database/postgres_for_server
else
 echo " -- Docker postgres_for_server:Exists"
fi

for instance in prom_client node_exporter prom grafana; do
 if [[ $(is_builded $instance) -ne 0 ]]; then
  echo " -- Docker $instance:Building"
  docker build -t $instance $SCRIPT_DIR/../deploy/monitoring/$instance
 else
  echo " -- Docker $instance:Exists"
 fi
done
#BUILD END

#RUN BEGIN
is_running(){
INSPECT=$(docker container inspect -f '{{.State.Running}}' $1 2>/dev/null )
IS_CONTAINER_EXISTS=$?
if [[ $IS_CONTAINER_EXISTS -eq 1 ]]; then
 echo 2
return 0
fi

if [ "$INSPECT" == "true" ]; then
 echo 0
else
 echo 1
fi
return 0
}

if [[ $(is_running postgres_for_server) -ne 0 ]]; then
  echo " -- Docker postgres_for_server:Run now"
  docker run -d --rm --network postgres_network --ip $IP_POSTGRES --name postgres_for_server postgres_for_server
else
  echo " -- Docker postgres_for_server:Already running"
fi

TIME_TO_WAIT=30000
TIME_CURRENT=0
until nc -z $IP_POSTGRES 5432
do
    echo "-- Waiting for postgres container..."
    sleep 0.1
    TIME_CURRENT=$((100+$TIME_CURRENT))
    if [ $TIME_CURRENT -ge $TIME_TO_WAIT ]; then
      echo "TIME_CURRENT = $TIME_CURRENT"
      echo "TIME_TO_WAIT = $TIME_TO_WAIT"
      echo " -- ERROR: postgres server too much time to wait. Break."
      exit 1
      break
    fi
done
if [[ $(is_running my_http_server) -ne 0 ]]; then
  echo " -- Docker my_http_server:Run now"
  docker container rm  my_http_server
  docker create -p 80:8080 --name my_http_server my_http_server
  docker network connect postgres_network my_http_server
  docker start my_http_server
else
  echo " -- Docker my_http_server:Already running"
fi

if [[ $(is_running prom_client) -ne 0 ]]; then
  echo " -- Docker prom_client:Run now"
  docker run -d --rm --network monitoring_network --ip $IP_PROM_CLIENT --volumes-from my_http_server --name prom_client prom_client
else
  echo " -- Docker prom_client:Already running"
fi

if [[ $(is_running node_exporter) -ne 0 ]]; then
  echo " -- Docker node_exporter:Run now"
  docker run -d --rm --network monitoring_network --ip $IP_NODE_EXPORTER --name node_exporter node_exporter 
else
  echo " -- Docker node_exporter:Already running"
fi

if [[ $(is_running prom) -ne 0 ]]; then
 if [[ $(is_running prom) -eq 1 ]]; then
  echo " -- Docker prometheus:Start now"
  docker start prom
 else # -eq 2
  echo " -- Docker prometheus:Run now"
  docker run -d --network monitoring_network --ip $IP_PROMETHEUS --name prom prom
 fi
else
  echo " -- Docker prometheus:Already running"
fi

if [[ $(is_running grafana) -ne 0 ]]; then
 if [[ $(is_running grafana) -eq 1 ]]; then
  echo " -- Docker grafana:Start now"
  docker start grafana
 else # -eq 2
  echo " -- Docker grafana:Run now"
  docker run -d -p 3000:3000 --network monitoring_network --ip $IP_GRAFANA --name grafana grafana
 fi
else
 echo " -- Docker grafana:Already running"
fi
#RUN END
