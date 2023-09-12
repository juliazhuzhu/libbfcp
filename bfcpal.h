#ifndef BFCPAL_H
#define BFCPAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cstdint>

struct BFCPal;
typedef struct BFCPal BFCPal;

typedef enum {
	SalBfcpFloorCtrlBoth = 0,
	SalBfcpFloorCtrlServer,
	SalBfcpFloorCtrlClient
}SalBfcpFloorCtrlRole;

typedef void (*BFCPalOnFloorGranted)(void *userdata);
typedef void (*BFCPalOnFloorRelease)(void *userdata);
typedef void (*BFCPalOnFloorDenied)(void *userdata);

typedef struct _BFCPalCallbacks{
    BFCPalOnFloorGranted floor_granted_on_recv;
    BFCPalOnFloorGranted floor_granted_on_send;
    BFCPalOnFloorRelease floor_release;
    BFCPalOnFloorDenied floor_denied_on_send;
}BFCPalCallbacks;

BFCPal * bfcpal_init(SalBfcpFloorCtrlRole floorCtlRole, unsigned long int conferenceID, unsigned short int userID, unsigned short floorId, unsigned int local_port, const char * remote_addr, unsigned int remote_port, const BFCPalCallbacks *cbs, void *user_data);

void bfcpal_uninit(BFCPal *bfcpal);

void bfcpal_start_send_content(BFCPal *bfcpal, unsigned short is_whiteboard=0);

void bfcpal_stop_send_content( BFCPal *bfcpal);

#ifdef __cplusplus
}
#endif

#endif