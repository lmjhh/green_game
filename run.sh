#!/bin/bash

# g++ -O3 main.cpp -o demo -Iinclude/ -I/usr/include/mysql -L/usr/lib64/mysql -lmysqlclient
g++ -O3 main.cpp -o demo -I include/ -I /usr/include/mysql -l mysqlclient