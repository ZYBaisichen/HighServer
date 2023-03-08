/*
 * @Author: baisichen
 * @Date: 2023-02-14 11:24:13
 * @LastEditTime: 2023-02-15 11:16:15
 * @LastEditors: baisichen
 * @Description: 
 */
// target_include_directories(<library name> PRIVATE /usr/include ${CMAKE_CURRENT_SOURCE_DIR} ${SOME_DEP_DIR}/include)

#include <sys/signal.h>
#include <event.h>
#include<stdio.h>
#include <sys/sem.h>
void signal_cb(int fd, short event, void* argc) {
    struct event_base* base = (event_base*) argc;
    struct timeval delay = {2,0};
    printf("Caunght an interrupt signal; exiting cleanly in two seconds..\n");
    event_base_loopexit(base, &delay);
}

void timeout_cb(int fd, short event, void* argc) {
    printf("timeout\n");
}

int main() {
    struct event_base* base = event_init();
    struct event* signal_event = evsignal_new(base, SIGINT, signal_cb, base);
    event_add(signal_event, NULL);

    timeval tv={1,0};
    struct event* timeout_event = evtimer_new(base, timeout_cb, NULL);
    event_add(timeout_event, &tv);
    event_base_dispatch(base);

    event_free(timeout_event);
    event_free(signal_event);
    event_base_free(base);
    return 0;
}

