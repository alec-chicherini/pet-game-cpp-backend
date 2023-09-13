# Импортируем три модуля:
# * json - для JSON,
# * sys - для чтения из stdin,
# * prometheus_client для работы с Prometheus.
# Последнему для краткости дадим имя prom.
# Это аналог конструкции из C++:
# namespace prom = prometheus_client;
import prometheus_client as prom
import sys
import json

# Создаём три метрики
good_lines = prom.Counter('webexporter_good_lines', 'Good JSON records')
wrong_lines = prom.Counter('webexporter_wrong_lines', 'Wrong JSON records')

response_time = prom.Histogram('webserver_request_duration', 'Response time', ['code', 'content_type'], 
    buckets=(.001, .002, .005, .010, .020, .050, .100, .200, .500, float("inf")))

# Определим функцию, которую мы будем использовать как main:
def my_main():
    prom.start_http_server(9200)

    # читаем в цикле строку из стандартного ввода
    for line in sys.stdin:
        try:
            data = json.loads(line)

            # Ошибка, если из JSON прочитан не объект
            if not isinstance(data, dict):
                raise ValueError()

            # Регистрирум запрос
            if data["message"] == "response sent":
                total_time_seconds = int(data["data"]["response_time"]) / 1000
                response_time.labels(
                    code=int(data["data"]["code"]),
                    content_type=data["data"]["content_type"]).observe(total_time_seconds)

            # Регистрирум успешно разобранную строку
            good_lines.inc()
        except (ValueError, KeyError):
            # Если мы пришли сюда, то при разборе JSON
            # обнаружилась синтаксическая ошибка.
            # Увеличим соотв. счётчик
            wrong_lines.inc()

# Так на самом деле выглядит точка входа в Pyhton.
# Просто вызовем в ней наш аналог функции main.
if __name__ == '__main__':
    my_main()
