#!/bin/bash
# Faccio partire il server in background e salvo il suo pid
valgrind --leak-check=full bin/server tests/config/test1config.txt &
SERVER_PID=$!
export SERVER_PID
bash -c 'sleep 10 && kill -s SIGHUP ${SERVER_PID}' &

bin/client -p -t 200 -f LSOfiletorage.sk -W ./dummyFiles/file1,dummyFiles/file2  -r $PWD/dummyFiles/file1,$PWD/dummyFiles/file2 -d tests/test1tmp1

bin/client -p -t 200 -f LSOfiletorage.sk -w ./dummyFiles,0  -R 0 -d tests/test1tmp2

bin/client -p -t 200 -f LSOfiletorage.sk -l $PWD/dummyFiles/file1 -c $PWD/dummyFiles/file1

bin/client -p -t 900 -f LSOfiletorage.sk -l $PWD/dummyFiles/file2 -u $PWD/dummyFiles/file2 &
bin/client -p -t 0 -f LSOfiletorage.sk -l $PWD/dummyFiles/file2

wait ${SERVER_PID}

exit 0