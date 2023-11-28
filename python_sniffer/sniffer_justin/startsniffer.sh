#!/bin/sh

cleanup() {
	echo "SIGINT catch ctrl + c"
	pkill -f sniffer.py
	pkill -f dbwriter.py
	exit
}

trap cleanup INT

echo "start sniffer.py and dbwriter.py..."
python sniffer.py -s True -i any -z 1000000 -p False &
python dbwriter.py -t 10 -z 1000000 -u dds_paas -w postgres -d 10.1.1.200 -p 5433 -n paasdb&

wait

