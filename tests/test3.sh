#!/bin/bash
TEST_DURATION=30

bin/server tests/config/test3config.txt &
SERVER_PID=$!

sleep 2

clients_pids=()
for i in {1..10}; do
    ./tests/clientRandReq.sh &
    clients_pids+=($!)
done

echo "Test avviato attendere ${TEST_DURATION} secondi"

sleep ${TEST_DURATION}

kill -s SIGINT ${SERVER_PID}

for i in "${clients_pids[@]}"; do
    kill -9 ${i}
    wait ${i}
done

wait ${SERVER_PID}

exit 0