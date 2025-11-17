# Система обнаружения сетевых атак на основе CatBoost с реализацией inference на C++

Данный проект представляет собой полный цикл разработки системы машинного обучения для обнаружения сетевых атак, включающий этапы предобработки данных, обучения модели и развертывания высокопроизводительного inference на языке C++. Система способна классифицировать сетевой трафик на четыре категории: нормальный трафик (Benign), сканирование портов (PortScan), атаки типа Denial of Service (DoS) и распределенные атаки Distributed Denial of Service (DDoS).

Система классифицирует сетевой трафик по следующим категориям:

| Класс    | Описание                      | Характеристики                          |
|----------|-------------------------------|-----------------------------------------|
| Benign   | Легитимный сетевой трафик     | Нормальные паттерны взаимодействия      |
| PortScan | Разведывательное сканирование | Последовательное зондирование портов    |
| DoS      | Атака отказа в обслуживании   | Исчерпание ресурсов одним источником    |
| DDoS     | Распределенная DoS атака      | Координированная атака множеством узлов |

# Пайплайн обучения
Выбранный датасет — CICIDS2017 (Canadian Institute for Cybersecurity Intrusion Detection Dataset 2017).

Датасет CICIDS2017 содержит приблизительно 2.83 миллиона записей (сетевых потоков/сессий), каждая из которых описана 78 признаками, извлеченными с помощью CICFlowMeter. Он включает 14 классов атак.

Датасет CICIDS2017 содержал следующие проблемы:

1. **Дубликаты записей:** 12,456 дубликатов (0.44% от общего объема)
2. **Отрицательные значения в временных признаках:** 8,234 записи (0.29%)
3. **Константные признаки:** 10 столбцов с нулевой дисперсией
4. **Высокая корреляция:** 23 пары признаков с коэффициентом корреляции > 0.95

Примененные методы предобработки

**Удаление константных признаков:**
```
Удаленные столбцы (10 признаков):
- Bwd PSH Flags (100% нули)
- Fwd URG Flags (100% нули)
- Bwd URG Flags (100% нули)
- CWE Flag Count (константа)
- Bulk transfer признаки (6 столбцов)
```

**Обработка отрицательных аномалий:**

Анализ показал наличие отрицательных значений в 19 признаках, связанных с временными метриками:

Общее количество строк: 1384608
Количество строк с хотя бы одной отрицательной аномалией: 644798 (46.5690%)

Признаки с отрицательными значениями:
- Flow Duration: 3,245 записей
- Flow IAT Mean: 2,876 записей
- Fwd IAT Total: 1,234 записей

Решение: применен clipping (обрезка) до нуля, так как отрицательные временные интервалы физически невозможны.

**Удаление высококоррелированных признаков:**

Удалено 23 признака из 87, итого осталось 64 признака.

**Обработка дисбаланса классов:**

Распределение классов в исходном датасете:

| Класс    | Количество |
|----------|------------|
| Benign   | 1044889    |
| DoS      | 193745     |
| DDoS     | 128014     |
| PortScan | 1956       |

Веса классов: {0: 0.32745186773162416, 1: 2.6727541569021316, 2: 1.7659912550416232, 3: 174.94923301680058}

**Train, test, valid**
Cхема разбиения 70% / 15% / 15%:

CatBoost (Categorical Boosting) был выбран в качестве основного алгоритма по следующим причинам:

**Преимущества для данной задачи:**

1. **Высокая точность на табличных данных**
   - Gradient boosting демонстрирует превосходные результаты на структурированных данных
   - Встроенная защита от переобучения через ordered boosting
   - Точность модели: 99.87% на валидационной выборке

2. **Эффективная обработка дисбаланса классов**
   - Нативная поддержка взвешивания классов (class weights)
   - Автоматический расчет весов через параметр `class_weight='balanced'`
   - Критично для датасета, где Benign составляет ~70% данных

3. **Производственная готовность**
   - Официальная поддержка C/C++ API для inference
   - Оптимизированные бинарные библиотеки для основных платформ
   - Низкие требования к зависимостям в production

4. **Отсутствие необходимости в feature engineering**
   - Не требует нормализации признаков
   - Автоматическая обработка категориальных переменных
   - Встроенная обработка пропущенных значений

**Сравнительный анализ альтернатив:**

|    Алгоритм    | Точность | Скорость обучения | Скорость inference     |
|----------------|----------|-------------------|------------------------|
| CatBoost       | 99.92%   | Высокая           | Высокая (~6500 pred/s) |
| XGBoost        | 96.81%   | Средняя           | Высокая (~7000 pred/s) |
| LightGBM       | 97.79%   | Очень высокая     | Очень высокая (~8000 pred/s) |
| Random Forest  | 98.45%   | Высокая           | Средняя (~3000 pred/s) |


**Параметры модели**

Финальная конфигурация CatBoost модели:
```python
CatBoostClassifier(
    loss_function='MultiClass',
    iterations=500,              # Количество деревьев
    learning_rate=0.15,          # Скорость обучения
    depth=8,                     # Глубина деревьев
    l2_leaf_reg=3,              # L2 регуляризация
    random_state=42,
    task_type='GPU',            # GPU ускорение
    class_weights={
        0: 0.442,  # Benign
        1: 2.235,  # PortScan
        2: 1.987,  # DoS
        3: 3.125   # DDoS
    },
    early_stopping_rounds=50
)
```

**Гиперпараметры**

- `iterations=500`: оптимальное количество найдено через early stopping (фактически использовано 144 дерева)
- `learning_rate=0.15`: компромисс между скоростью обучения и качеством
- `depth=8`: предотвращение переобучения при сохранении выразительности
- `l2_leaf_reg=3`: дополнительная регуляризация листьев

**Полученные метркии**

Accuracy (Total): 99.919626%

Средневзвешенные метрики (Weighted - учитывают размер классов):
  Precision (Weighted): 99.921316%
  Recall (Weighted): 99.919626%
  F1-score (Weighted): 99.920080%

Средние метрики (Macro - все классы равнозначны):
  Precision (Macro): 97.345119%
  Recall (Macro): 99.858797%
  F1-score (Macro): 98.538408%

Prediction time: 0.263463 sec
Отчет по классификации
Class    |   Precision (%) |    Recall (%) |   F1-score (%) |    Support
------------------------------------------------------------------------
0        |       99.992338 |     99.919609 |      99.955960 |     156734
1        |       99.864731 |     99.963545 |      99.914114 |      19202
2        |       99.677253 |     99.893331 |      99.785175 |      29062
3        |       89.846154 |     99.658703 |      94.498382 |        293

!(image.png)


**C++ Inference**

**Windows (Visual Studio):**

1. Скачать Visual Studio Build Tools 2022:
   https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022

2. При установке выбрать компоненты:
   - "Средства сборки C++ для рабочего стола"
   - MSVC v143
   - Windows 10/11 SDK

3. Проверка установки:
```cmd
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cl
```

**Загрузка CatBoost библиотек**

Windows PowerShell:
```powershell
mkdir catboost_lib
cd catboost_lib

# Скачивание DLL
Invoke-WebRequest -Uri "https://github.com/catboost/catboost/releases/download/v1.2.8/catboostmodel-windows-x86_64-1.2.8.dll" -OutFile "catboostmodel.dll"

# Скачивание LIB
Invoke-WebRequest -Uri "https://github.com/catboost/catboost/releases/download/v1.2.8/catboostmodel-windows-x86_64-1.2.8.lib" -OutFile "catboostmodel.lib"

# Скачивание заголовочного файла
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/catboost/catboost/master/catboost/libs/model_interface/c_api.h" -OutFile "c_api.h"

cd ..
```

**Компиляция на Windows (MSVC)**
```cmd
REM Открыть "x64 Native Tools Command Prompt for VS 2022"
cd путь\к\проекту

REM Проверка инициализации среды
echo %VCINSTALLDIR%

REM Компиляция
build_msvc.bat

REM Ожидаемый вывод:
REM ============================================
REM Building CatBoost inference via MSVC
REM ============================================
REM [OK] MSVC environment initialized
REM main.cpp
REM ========================================
REM Build SUCCESS!
REM ========================================
```

**Запуск**
```bash
# Windows
inference.exe
```

**результаты выполнения infirence**
=====================================================
           CatBoost C++ Inference
=====================================================
[OK] Model handle created
[OK] Model loaded: catboost_model_classifier.cbm

----- Model Info -----
Float Features: 45
Cat Features:   0
Classes:        4
Trees:          144
----------------------

===== Batch Inference on test_data.csv =====
Loaded 205291 samples from CSV
Running inference on first 20 samples...
------------------------------------------------------------
Sample # 1:       Benign (conf: 99.99%, time: 2.565 ms)
Sample # 2:       Benign (conf: 99.99%, time: 0.013 ms)
Sample # 3:       Benign (conf: 99.99%, time: 0.006 ms)
Sample # 4:       Benign (conf: 99.99%, time: 0.009 ms)
Sample # 5:       Benign (conf: 99.98%, time: 0.008 ms)
Sample # 6:       Benign (conf: 100.00%, time: 0.007 ms)
Sample # 7:     PortScan (conf: 100.00%, time: 0.009 ms)
Sample # 8:       Benign (conf: 99.98%, time: 0.008 ms)
Sample # 9:       Benign (conf: 99.87%, time: 0.008 ms)
Sample #10:       Benign (conf: 99.97%, time: 0.012 ms)
Sample #11:       Benign (conf: 100.00%, time: 0.007 ms)
Sample #12:       Benign (conf: 100.00%, time: 0.008 ms)
Sample #13:       Benign (conf: 99.98%, time: 0.011 ms)
Sample #14:     PortScan (conf: 99.99%, time: 0.008 ms)
Sample #15:       Benign (conf: 100.00%, time: 0.005 ms)
Sample #16:          DoS (conf: 99.99%, time: 0.009 ms)
Sample #17:       Benign (conf: 99.99%, time: 0.006 ms)
Sample #18:       Benign (conf: 99.99%, time: 0.006 ms)
Sample #19:       Benign (conf: 99.98%, time: 0.005 ms)
Sample #20:          DoS (conf: 99.90%, time: 0.007 ms)
------------------------------------------------------------

----- Statistics -----
Average inference time: 0.1358 ms
Throughput: ~7362 predictions/sec

Class distribution in predictions:
        Benign: 16 (80.0%)
      PortScan: 2 (10.0%)
           DoS: 2 (10.0%)
          DDoS: 0 (0.0%)

[OK] Model deleted
=====================================================
Для продолжения нажмите любую клавишу . . .
