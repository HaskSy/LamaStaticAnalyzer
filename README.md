# Lama Static Analyzer

Статический анализатор байткода Lama v1.20

Домашнее задание по курсу VM 2024-2025.

Выполнили: Илья Барсуков, Сергей Ковальцов, Алексей Казаков

## Building & Testing 

Это делается в пару простых шагов:

```bash
git clone --recursive <link>
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/LamaInterpreter file
```

Для сборки тестовых файлов необходимо скомпилировать примеры с помощью `lamac`. 
Для её настройки и установки необходимо проследовать в оригинальный репозиторий Lama

Запуск тестов (и тестов производительности) осуществляется через скрипт:

```bash
./run_tests.sh
```

Результаты замеров производительности на Intel Core i7-12700H
```
Testing file: ../Lama/performance/Sort.lama
-------------------------------
Binary time:

real	0m0.987s
user	0m0.984s
sys	0m0.000s
-------------------------------
Original interpreter time with compilation:

real	0m1.469s
user	0m1.439s
sys	0m0.025s
-------------------------------
Our interpreter time with compilation:

real	0m0.453s
user	0m0.442s
sys	0m0.010s
-------------------------------
Our interpreter time without compilation:

real	0m0.391s
user	0m0.384s
sys	0m0.006s
-------------------------------
```

Данный проект использует git submodules, поскольку это *должно* облегчить взаимодействие с подпроектами
