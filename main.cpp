#include "bfcpal.h"
#include <unistd.h>
#include <stdio.h>

void floor_granted_on_send(void* userdata){
    printf("*********************** floor_granted_on_send **************************\n");
}

void defualt_bfcp_callback(void* userdata){

    printf("defualt_bfcp_callback \n");
}

int main(){
    BFCPalCallbacks cb;
    cb.floor_denied_on_send = defualt_bfcp_callback;
    cb.floor_granted_on_recv = defualt_bfcp_callback;
    cb.floor_release = defualt_bfcp_callback;
    cb.floor_granted_on_send = floor_granted_on_send;
    BFCPal * bfcp = bfcpal_init(SalBfcpFloorCtrlClient, 123456, 2, 1, 3028,"127.0.0.1",9021, &cb,
    nullptr);
    sleep(6);
    bfcpal_start_send_content(bfcp);

    sleep(5);

    bfcpal_stop_send_content(bfcp);
    sleep(1);
    bfcpal_uninit(bfcp);
    int i = 0;
    while(true){
        sleep(1);
        i++;
        if (i >= 2)
            break;
    }

    return 1;
}