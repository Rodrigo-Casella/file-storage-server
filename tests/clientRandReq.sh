#!/bin/bash

requests=(
    '-W ./dummyFiles/file1'
    '-W ./dummyFiles/file2'
    '-W ./dummyFiles/file3'
    '-W ./dummyFiles/rec/file4'
    '-W ./dummyFiles/rec/file5'
    '-W ./dummyFiles/rec/file6'
    '-W ./dummyFiles/rec/rec2/file7'
    '-W ./dummyFiles/rec/rec2/file8'
    '-W ./dummyFiles/rec/rec2/file9'
    '-W ./dummyFiles/rec/rec2/file10'
    )

    

while true 
do
    i=$(( RANDOM % ${#requests[@]}))
    bin/client -f LSOfiletorage.sk ${requests[i]}
done

exit 0