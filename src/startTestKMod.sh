#!/bin/bash

if uname -a | grep -q "raspberrypi"; then
    echo "Set CPU governor to 'performance' on all cores..."
    echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

else
    # On Odroid XU4:
    echo "Set CPU governor to 'performance' on all cores..."
    echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

    # Only necessary if docker is installed and daemon started:
    # echo "Set RT scheduling runtime to -1..."
    # sudo sysctl -w kernel.sched_rt_runtime_us=-1
fi

echo "Waiting 5 seconds..."
sleep 5s

echo "Executing test with real-time priority on CPU core 0..."
if [ -e /dev/ttyACM0 ]; then
    if uname -a | grep -q "raspberrypi"; then
	sudo taskset -c 0 chrt 99 ./latencyTestKMod $@ /dev/ttyACM0
    else
	sudo taskset -c 5 chrt 99 ./latencyTestKMod $@ /dev/ttyACM0
    fi
else
    if uname -a | grep -q "raspberrypi"; then
	sudo taskset -c 0 chrt 99 ./latencyTestKMod $@
    else
	sudo taskset -c 5 chrt 99 ./latencyTestKMod $@
    fi
fi
