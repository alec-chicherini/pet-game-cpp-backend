#Этот файл предназначен для дебага приложения.
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
COPY ./tests /app/tests
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

#run
FROM ubuntu:22.04 as run

RUN groupadd -r www && useradd -r -g www www
USER www

COPY --from=build /app/build/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

ENTRYPOINT ["/app/game_server", "-c", "/app/data/config.json", "-w", "/app/static"]
#ENTRYPOINT ["/app/game_server", "-c", "/app/data/config.json", "-w", "/app/static", "-s", "/app/data/state"]
