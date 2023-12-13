#!/bin/sh

cleanup() {
    echo "SIGINT catch ctrl + c"
    pkill -f capture
    pkill -f dbwriter
    exit
}

trap cleanup INT

echo "start sniffer.py and dbwriter.py..."

# 持續檢查 capture 和 dbwriter 是否在運行
while true; do
    # 檢查 capture 是否正在運行
    if ! pgrep -f ./capture > /dev/null; then
        echo "capture not running, starting it..."
        ./capture -i eno1 -p 1000 -c postgresql://postgres:admin@140.110.7.17:5433/postgres &
    fi

    # 檢查 dbwriter 是否正在運行
    if ! pgrep -f ./dbwriter > /dev/null; then
        echo "dbwriter not running, starting it..."
        ./dbwriter -t 60 -d postgresql://postgres:admin@140.110.7.17:5433/postgres &
    fi

    sleep 2
done

wait

