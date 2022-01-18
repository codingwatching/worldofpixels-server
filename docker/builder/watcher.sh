#!/bin/sh
dir=${1}
shift;
pid=0

blockingkill() {
    if [ $pid -gt 0 ]; then
        kill $pid >/dev/null 2>&1
        ( sleep 10 && kill -9 $pid &>/dev/null ) &
        wait $pid
        pid=0
    fi
}

cleanup() {
    echo "Exiting watcher..."
    blockingkill
    exit 0
}

trap cleanup INT TERM

while true; do
    if make -j $(nproc) -l $(nproc) $MAKEOPTS; then
        "${@}" &
        pid=$!
    fi
    
    inotifywait -r -q -e modify "$dir" &
    wait $!
    
    blockingkill
done
