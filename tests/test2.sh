#!/bin/bash

bin/server tests/config/test2configFIFO.txt &
SERVER_PID=$!

echo -e "Algoritmo FIFO per il rimpiazzamento dei file"
echo ""

bin/client -p -f LSOfiletorage.sk -W ./dummyFiles/rec/file4,./dummyFiles/rec/file5

sleep 1

#dovrebbe espellere file4
bin/client -p -f LSOfiletorage.sk -W ./dummyFiles/rec/file6 -D tests/evicted1 


kill -s SIGHUP $SERVER_PID
wait $SERVER_PID

sleep 3

#--------------------------------------------------------------------------------------------------------
bin/server tests/config/test2configLRU.txt &
SERVER_PID=$!

echo ""
echo -e "Algoritmo LRU per il rimpiazzamento dei file"
echo ""

bin/client -p -f LSOfiletorage.sk -W ./dummyFiles/rec/file4,./dummyFiles/rec/file5 

sleep 1

bin/client -p -f LSOfiletorage.sk -r $PWD/dummyFiles/rec/file4

sleep 1

#dovrebbe espellere file5
bin/client -p -f LSOfiletorage.sk -W ./dummyFiles/rec/file6 -D tests/evicted2


kill -s SIGHUP $SERVER_PID
wait $SERVER_PID

sleep 3

#--------------------------------------------------------------------------------------------------------
bin/server tests/config/test2configLFU.txt &
SERVER_PID=$!

echo ""
echo -e "Algoritmo LFU per il rimpiazzamento dei file"
echo ""

bin/client -p -f LSOfiletorage.sk -W ./dummyFiles/rec/file4,./dummyFiles/rec/file5 -r $PWD/dummyFiles/rec/file4

sleep 1

#dovrebbe espellere file5
bin/client -p -f LSOfiletorage.sk -W ./dummyFiles/rec/file6 -D tests/evicted3


kill -s SIGHUP $SERVER_PID
wait $SERVER_PID

sleep 3

#--------------------------------------------------------------------------------------------------------
bin/server tests/config/test2configSC.txt &
SERVER_PID=$!

echo ""
echo -e "Algoritmo Second-chance per il rimpiazzamento dei file"
echo ""

bin/client -p -f LSOfiletorage.sk -W ./dummyFiles/rec/rec2/file7,./dummyFiles/rec/rec2/file8,./dummyFiles/rec/rec2/file9

sleep 1

#dovrebbe espellere file7 e poi file9
bin/client -p -f LSOfiletorage.sk -W ./dummyFiles/rec/rec2/file10 -D tests/evicted4 -r $PWD/dummyFiles/rec/rec2/file8 -W ./dummyFiles/rec/rec2/file7 -D tests/evicted4


kill -s SIGHUP $SERVER_PID
wait $SERVER_PID

exit 0