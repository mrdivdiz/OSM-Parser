## OSM Parser
# Парсер и конвертер формата OSM на C++ с GUI (Qt)
# PBF, сжатый BZIP, не поддерживается! Используйте GZIP.

Используемые библиотеки:

https://osmcode.org/libosmium/ - libosmium для работы с бинарными данными OSM (OSM PBF), "OSM Extracts" (https://download.geofabrik.de/)

http://postgis.net/ - расширение для PostgreSQL конкретно для работы с географическими данными.

https://habr.com/ru/articles/30636/

Ввиду неввозможности "купить" современный QT Opensource для GUI взял QT 4.7.4. На Windows 10 (NT 6.2) он работает нормально и пока еще не имеет проблем с совместимостью. Компилятор MinGW (пришлось использовать QtCreator, т.к. интеграция с Eclipse умерла еще в 2014)
