
#build
FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install "conan<2"

# копируем conanfile.txt в контейнер и запускаем conan install
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11
# только после этого копируем остальные иходники
COPY ./src /app/src
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

#TODO сделать чтобы всё это работало из под www юзера
#run
FROM ubuntu:22.04 as run

#RUN groupadd -r www && useradd -r -g www www
#USER www

COPY --from=build /app/build/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

#USER root
RUN mkdir /share
VOLUME ["/share"]
RUN mkfifo /share/pipe
RUN echo "/app/./game_server -c /app/data/config.json -w /app/static -t 10 -p 1000 -s /app/state > /share/pipe" > /app/run.sh
ENV GAME_DB_URL=postgres://server:server@172.172.1.2:5432/server_db
#USER www
ENTRYPOINT ["sh","/app/run.sh"]
