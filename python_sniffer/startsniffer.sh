#!/bin/sh

cleanup() {
	echo "SIGINT catch ctrl + c"
	pkill -f capture
	pkill -f dbwriter
	exit
}

trap cleanup INT

echo "start sniffer.py and dbwriter.py..."

./capture -i any -p 15000 &
sleep 2
./dbwriter -t 3 -d postgresql://postgres:admin@140.110.7.17:5433/postgres &

wait

