if [ $# -eq 0 ]
  then
    echo "Usage: ./statistiche.sh pathToLogfile"
    exit 1
fi

# numero di operazioni
read_op=$(grep "readFile" -c $1)
mean_bytes_read=0

echo -e -n "Numero operazioni di read: ${read_op}\n"
bytes_read=$(grep -zoP 'readFile\n.*\nbytesProcessed: [0-9]+' $1 | grep -aEo 'bytesProcessed: [0-9]+' | grep -aEo '[0-9]+' | { sum=0; while read num; do ((sum+=num)); done; echo $sum; } )

if [ ${read_op} -gt 0 ];
  then
    mean_bytes_read=$(echo "scale=0; ${bytes_read} / ${read_op}" | bc -l)
fi

echo -e -n "Media byte letti: ${mean_bytes_read}\n"

write_op=$(grep "writeFile" -c $1)
mean_bytes_write=0

echo -e -n "Numero operazioni di write: ${write_op}\n"
bytes_write=$(grep -zoP 'writeFile\n.*\nbytesProcessed: [0-9]+' $1 | grep -aEo 'bytesProcessed: [0-9]+' | grep -aEo '[0-9]+' | { sum=0; while read num; do ((sum+=num)); done; echo $sum; } )

if [ ${write_op} -gt 0 ];
  then
    mean_bytes_write=$(echo "scale=0; ${bytes_write} / ${write_op}" | bc -l)
fi

echo -e -n "Media byte scritti: ${mean_bytes_write}\n"

echo -e -n "Numero di operazioni di lock: "
grep "lockFile" -c $1

echo -e -n "Numero di operazioni di open-lock: "
grep "open-lock" -c $1

echo -e -n "Numero di operazioni di unlock: "
grep "unlockFile" -c $1

echo -e -n "Numero di operazioni di open: "
grep "openFile" -c $1

echo -e -n "Numero di operazioni di close: "
grep "closeFile" -c $1

echo -e -n "Dimensione massima raggiunta dallo storage (in Mbytes): "
grep -zoP 'absMaxMemory\n.*\nbytesProcessed: [0-9]+' $1 | grep -aEo '[0-9]+' | { read num; echo "scale=4;$num/1000000"; } | bc -l

echo -e -n "Numero di file massimo raggiunto dallo storage: "
grep -zoP 'absMaxFiles\n.*\nbytesProcessed: [0-9]+' $1 | grep -aEo '[0-9]+'

echo -e -n "Numero di espulsioni dalla cache: "
grep "evicted" -c $1

echo "Numero richieste servite da ogni thread: "
grep -o "workerTid: .*" $1 | sort | uniq -c

echo -e -n "Numero massimo di client connessi contemporeanamente: "
grep -zoP 'MaxClientConnected\n.*\nbytesProcessed: [0-9]+' $1 | grep -aEo '[0-9]+'