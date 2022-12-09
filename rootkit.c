#include <linux/module.h>

#define LICENCE "GPL"
#define DESCRIPTION "Rootkit that can hide arbitrary ressources by using the configure command line tool."
#define AUTHOR "Benjamin Haag"
#define VERSION "0.2"

MODULE_LICENSE(LICENCE);
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);
MODULE_VERSION(VERSION);



static int __init
module_load(void) {
    printk(KERN_INFO "Rootkit: loaded. Hello!\n");
    return 0;
}


static void __exit
module_unload(void) {
    printk(KERN_INFO "Rootkit: unloaded. Bye Bye!\n");
}


module_init(module_load);
module_exit(module_unload);