#!/bin/sh

cleanup() {
    echo "SIGINT catch ctrl + c"
    for pid in $(pgrep -f capture); do
        kill -9 $pid  
        wait $pid           
    done
    for pid in $(pgrep -f dbwriter); do
        kill $pid  
        wait $pid           
    done
    exit
}

trap cleanup INT

echo "start sniffer.py and dbwriter.py..."

while true; do
    if pgrep -f rtiroutingserviceapp > /dev/null; then
        echo "RTI Routing Service is running, checking again in 10 seconds..."
        sleep 10
        ./capture -i eth0 -p 1000 -c $PY_DB_URL &
	./dbwriter -t 60 -d $PY_DB_URL &
	break
    else
	sleep 2
	continue
    fi
done

while true; do
    if ! pgrep -f ./capture > /dev/null; then
        echo "capture not running, starting it..."
        ./capture -i eth0 -p 1000 -c $PY_DB_URL &
    fi
    
    if ! pgrep -f ./dbwriter > /dev/null; then
        echo "dbwriter not running, starting it..."
        ./dbwriter -t 60 -d $PY_DB_URL &
    fi
    
    sleep 2
done

wait

