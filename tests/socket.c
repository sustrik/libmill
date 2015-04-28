
#include <assert.h>
#include <netinet/in.h>
#include <string.h>
#include "../mill.h"

#include <stdio.h>

void connect_socket(void) {
    int cs = msocket(AF_INET, SOCK_STREAM, 0);
    assert(cs != -1);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = 0x0100007f;
    addr.sin_port = htons(5555);
    int rc = mconnect(cs, (struct sockaddr*)&addr, sizeof(addr));
    assert(rc != -1);
    close(cs);
}

int main() {
    int ls = msocket(AF_INET, SOCK_STREAM, 0);
    assert(ls != -1);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(5555);
    int rc = bind(ls, (struct sockaddr*)&addr, sizeof(addr));
    assert(rc != -1);
    rc = listen(ls, 10);
    assert(rc != -1);
    go(connect_socket());
    int ac = maccept(ls, NULL, NULL);
    assert(ac >= 0);
    close(ac);
    return 0;
}

