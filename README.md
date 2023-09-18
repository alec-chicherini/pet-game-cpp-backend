## Описание
Учебный проект С++ backend сервера для браузерной игры. Результат прохождения курса Яндекс Практикум C++ для бэкенда

## Сервер 
<!-- не работает при выключенном сервере ![badge](https://img.shields.io/website?url=http%3A%2F%2Folololo.lol%2Findex.html&label=olololo.lol) -->
Игра: [http://olololo.lol](http://olololo.lol)\
Grafana: [http://olololo.lol:grafana](http://olololo.lol:3000/d/c8a938e2-3632-4fca-bc2a-b3c3ff619f46/4-metrics?orgId=1&refresh=5s&from=1694605465110&to=1694605765110)

Если сервер не работает запустить его можно нажав в браузере `Run workflow` -> `Server to start: olololo.lol` -> `Run workflow` на странице раннера [Workflow Restart](https://github.com/alec-chicherini/pet-game-cpp-backend/actions/workflows/restart.yaml)

## Deploy
Развернуть сервер с окружением( `Ubuntu 22.04 Docker 24.0.5` )

```bash
./deploy/./run_server.sh
```

## Сборка в винде
### build and run ctest
```bash
mkdir build
cd build
conan install .. --build=missing -s build_type=Debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
ctest
```
### run
```bash
.\Debug\game_server.exe -c ..\data\config.json -w ..\static\ -t 100 -p 1000 -s ..\data\state
```
### visual studio debugger args
```
-c $(ProjectDir)\..\data\config.json -w $(ProjectDir)\..\static\ -t 100 -p 1000 -s $(ProjectDir)\..\data\state
```

### Юнит тесты
![badge](https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/alec-chicherini/fa7d81fb0caaae8a2790c00d2a20c0d0/raw/test1.json)\
![badge](https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/alec-chicherini/fa7d81fb0caaae8a2790c00d2a20c0d0/raw/test2.json)\
![badge](https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/alec-chicherini/fa7d81fb0caaae8a2790c00d2a20c0d0/raw/test3.json)\
![badge](https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/alec-chicherini/fa7d81fb0caaae8a2790c00d2a20c0d0/raw/test4.json)

