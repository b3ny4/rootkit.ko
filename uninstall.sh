#!/bin/bash

if [ $(id -u) != 0 ]; then
    echo "Please run as root"
    exit
fi


echo "Uninstalling Rootkit..."
echo "====================="
echo "Unload rootkit from system..."
rmmod rootkit && echo "Rootkit unloaded" || (echo "ERROR! unable to unload rootkit" && exit)
echo "Remove rootkit from modules list..."
rm /etc/modules-load.d/rootkit.conf && echo "Rootkit removed from modules list." || (echo "ERROR! unable to remove rootkit from modules list." && exit)
echo "Remove rootkit from system..."
rm /lib/modules/$(uname -r)/rootkit.ko && echo "Rootkit removed." || (echo "ERROR! unable to remove rootkit" && exit)

