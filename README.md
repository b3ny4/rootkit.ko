# Rootkit
Simple Hello World LKM

## installation
The installation tool builds the rootkit itself an cleans up afterwards. After the installation, the rootkit will be present on every reboot.
```
$ sudo ./install.sh
```

## Usage
After installation, you can use the `configure` tool to configure your rootkit.

Hide rootkit
```
$ configure hidemodule
```

Unhide rootkit
```
$ configure showmodule
```


## Uninstall
The uninstallation tool simply removes the rootkit from the system. Possible changes to the system won't recover until the next reboot!

```
$ sudo ./uninstall.sh
```


## Manually Installation
First build the rootkit using the `make` tool.

```
$ make
```

To install the rootkit, you have to copy it to the kernel module folder.

```
$ sudo cp rootkit.ko /lib/modules/$(uname -r)/
```

Once the module is installed, register it to load at bootup

```
$ echo "rootkit" | sudo tee /etc/modules-load.d/rootkit.conf > /dev/null
```

Now, you can clean up using the `make` tool.

```
$ make clean
```

Finally, you have to create a dependency map.

```
$ sudo depmod
```

[Optional] Either reboot your PC, or load the rootkit manually so it is available immediately.

```
$ sudo /usr/lib/systemd/systemd-modules-load /etc/modules-load.d/rootkit.conf
```

## Manually Uninstall

First unload the rootkit

```
$ sudo rmmod rootkit
```

Remove the rootkit from the modules list

```
$ sudo rm /etc/modules-load.d/rootkit.conf
```

Remove the rootkit from the modules

```
$ sudo rm /lib/modules/$(uname -r)/rootkit.ko
```