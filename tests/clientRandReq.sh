#!/bin/bash

requests=(
    '-W ./dummyFiles/file1 -r ./dummyFiles/file1 -d tests/test3tmp'
    '-w ./dummyFiles/rec,3'
    '-R 0 -d tests/test3tmp'
    '-W ./dummyFiles/rec/file4 -l ./dummyFiles/rec/file4 -c ./dummyFiles/rec/file4'
    '-w ./dummyFiles/rec/rec2/rec3 -D tests/evicted5'
    '-W ./dummyFiles/rec/file5 -l ./dummyFiles/rec/file5 -u ./dummyFiles/rec/file5'
    '-W ./dummyFiles/rec/file6 -c ./dummyFiles/rec/file6'
    '-w ./dummyFiles/rec/rec2/'
    '-l ./dummyFiles/rec/rec2/file8'
    '-W ./dummyFiles/rec/rec2/file9 -D tests/evicted5'
    )

    

while true 
do
    i=$(( RANDOM % ${#requests[@]}))
    bin/client -f LSOfiletorage.sk ${requests[i]}
done

exit 0