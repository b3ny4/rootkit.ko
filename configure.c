#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <string.h>
#include <unistd.h>

#define NETLINK_PROTOCOL 31
#define MAX_PAYLOAD 1024

static struct sockaddr_nl src_addr;
static struct sockaddr_nl dst_addr;
static struct nlmsghdr * nlh = NULL;
static struct iovec iov;
static struct msghdr msg;
static int sock;

int main(int argc, char *argv[]) {

    // create socket
    sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_PROTOCOL);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    // configure src addr
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();

    // bind socket
    bind(sock, (struct sockaddr*)&src_addr, sizeof(src_addr));

    // configure dst address
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.nl_family = AF_NETLINK;
    dst_addr.nl_pid = 0;
    dst_addr.nl_groups = 0;

    // create netlink packet
    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;
    strcpy(NLMSG_DATA(nlh), "Hello Kernel!");

    // wrap netlink packet in IO vector
    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;

    // wrap IO Vector into socket packet
    msg.msg_name = (void *)&dst_addr;
    msg.msg_namelen = sizeof(dst_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    // send message
    sendmsg(sock, &msg, 0);

    // wait for reply
    recvmsg(sock, &msg, 0);

    // print message
    printf("%s\n", (char *)NLMSG_DATA(nlh));

    // close socket
    close(sock);

    return 0;
}