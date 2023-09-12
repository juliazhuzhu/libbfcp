#include "bfcpal.h"
#include "bfcp_participant.h"
#include "assert.h"
#include <bfcp_strings.h>
#define DEFAULT_REFRESH_SEC                 5

typedef enum _bfcp_content_dir{CONTENT_RECV, CONTENT_SEND, CONTENT_SENDING, CONTENT_STOPPING, CONTENT_STOP} bfcp_content_dir;

struct BFCPal {
    SalBfcpFloorCtrlRole  floorCtrl;
    char local_addr[64];
    unsigned int local_port;
    char remote_addr[64];
    unsigned short int remote_port;
    unsigned short int status;
    unsigned short int snd_whiteboard;  //1: send white board 0: send content
    BFCPalCallbacks cbs;
    void * user_data;
    bfcp_content_dir content_dir;
    uint64_t last_send_floorstatus_sec;
    uint64_t last_send_floorrelease_sec;
    uint64_t last_send_floorrequest_sec;
    uint64_t last_send_hello_sec;
    bfcp_participant_context* bfcp_context;
    bfcp_participant_information* bfcp_participant_list;
    unsigned short floorId;
    bool exiting;
    pthread_t thread;
    //unsigned short int    last_sent_primitive;

};

void bfcpal_set_callbacks(BFCPal *bfcpal, const BFCPalCallbacks *cbs);

static void unimplemented_stub(void){
    //ms_warning("Unimplemented SAL callback");
}

void bfcpal_set_callbacks(BFCPal *bfcpal, const BFCPalCallbacks *cbs);
void listen_server(bfcp_received_message *recv_msg,bfcp_participant_context* context);
void keepalive_thread(void* bfcpal);

BFCPal * bfcpal_init(SalBfcpFloorCtrlRole floorCtlRole, unsigned long int conferenceID, unsigned short userID, unsigned short floorId, unsigned int local_port, const char * remote_addr, unsigned int remote_port, const BFCPalCallbacks *cbs, void *user_data){

    assert (floorCtlRole == SalBfcpFloorCtrlClient);

    BFCPal * bfcpal = (BFCPal*)malloc(sizeof(BFCPal));
    memset(bfcpal,0, sizeof(BFCPal));
    bfcpal->floorCtrl = SalBfcpFloorCtrlClient;
    bfcpal->local_port = local_port;
    strncpy(bfcpal->remote_addr, remote_addr, sizeof(bfcpal->remote_addr)-1);
    bfcpal->remote_port = remote_port;
    bfcpal->last_send_floorstatus_sec = 0;
    bfcpal->last_send_floorrelease_sec = 0;
    bfcpal->last_send_floorrequest_sec = 0;
    bfcpal->user_data = user_data;
    bfcpal->status = BFCP_RELEASED;
    bfcpal->exiting = false;
    bfcpal_set_callbacks(bfcpal, cbs);

    bfcp_participant_context* context = (bfcp_participant_context*) calloc(1, sizeof(bfcp_participant_context));
    bfcpal->bfcp_participant_list = bfcp_initialize_bfcp_participant(conferenceID, userID,bfcpal->local_port,bfcpal->remote_addr, bfcpal->remote_port, listen_server,context);
    if(bfcpal->bfcp_participant_list == NULL) {
        printf("Couldn't create the new BFCP participant...\n");
        return NULL;
    }
    printf("BFCP Participant created.\n");
    bfcpal->bfcp_context = context;
    context->userdata = bfcpal;
    if(bfcp_insert_floor_participant(bfcpal->bfcp_participant_list, floorId) < 0) {
        printf("Couldn't add the new floor...\n");
        return NULL;
    }

    printf("Floor added.\n");
    bfcpal->floorId =  floorId;
    bfcpal->content_dir = CONTENT_STOP;
//    bfcp_hello_participant(bfcpal->bfcp_participant_list, context);
    bfcpal->last_send_hello_sec = 0;
    if(pthread_create(&bfcpal->thread, NULL, reinterpret_cast<void *(*)(void *)>(keepalive_thread), (void *)bfcpal) < 0) {
        printf("Couldn't create bfcpal worker thread\n");
        return NULL;
    }
    //pthread_detach(bfcpal->thread);
    return bfcpal;
}


void bfcpal_uninit(BFCPal *bfcpal) {

    if (bfcpal){
        bfcpal->exiting = true;
//        pthread_cancel(bfcpal->thread);
        pthread_join(bfcpal->thread,NULL);
        if (bfcpal->bfcp_participant_list && bfcpal->bfcp_context){
            bfcp_destroy_bfcp_participant(&bfcpal->bfcp_participant_list,bfcpal->bfcp_context);
            free(bfcpal->bfcp_context);
            free(bfcpal->bfcp_participant_list);
        }
        free(bfcpal);
    }

}

void bfcpal_start_send_content(BFCPal *bfcpal, unsigned short is_whiteboard){

    if (bfcpal->exiting)
        return;

    bfcpal->snd_whiteboard = is_whiteboard;
    if (bfcpal->floorCtrl == SalBfcpFloorCtrlClient){
        uint64_t msnow = time(NULL);
        //bfcpal_send_message(bfcpal, FloorRequest,0);
        bfcp_floors_participant* floorIds = create_floor_list_p(bfcpal->floorId, NULL);
        bfcp_floorRequest_participant(bfcpal->bfcp_participant_list,0,0,floorIds, NULL,bfcpal->bfcp_context);
        bfcpal->content_dir = CONTENT_SENDING;
        bfcpal->last_send_floorrequest_sec = msnow;
    }
}

void bfcpal_stop_send_content( BFCPal *bfcpal){

    if (bfcpal->exiting)
        return;

    if (bfcpal->floorCtrl== SalBfcpFloorCtrlClient){
        uint64_t msnow = time(NULL);
        //bfcpal_send_message(bfcpal, FloorRelease,0);
        unsigned short int floorRequestID = 1;
        bfcp_floorRelease_participant(bfcpal->bfcp_participant_list,floorRequestID, bfcpal->bfcp_context);
        bfcpal->content_dir = CONTENT_STOPPING;
        bfcpal->last_send_floorrelease_sec = msnow;//tv.tv_sec;
    }
}

void bfcpal_set_callbacks(BFCPal *bfcpal, const BFCPalCallbacks *cbs) {
    memcpy(&bfcpal->cbs, cbs, sizeof(*cbs));
    if(bfcpal->cbs.floor_granted_on_recv == NULL)
        bfcpal->cbs.floor_granted_on_recv=(BFCPalOnFloorGranted)unimplemented_stub;
    if (bfcpal->cbs.floor_granted_on_send == NULL)
        bfcpal->cbs.floor_granted_on_send=(BFCPalOnFloorGranted)unimplemented_stub;
    if(bfcpal->cbs.floor_release == NULL)
        bfcpal->cbs.floor_release=(BFCPalOnFloorRelease)unimplemented_stub;
    if(bfcpal->cbs.floor_denied_on_send == NULL)
        bfcpal->cbs.floor_denied_on_send=(BFCPalOnFloorDenied)unimplemented_stub;

}

void listen_server(bfcp_received_message *recv_msg, bfcp_participant_context* context) {
    int j, i;
    bfcp_supported_list *info_primitives, *info_attributes;
    bfcp_floor_request_information *tempInfo;
    bfcp_floor_request_status *tempID;
    BFCPal *bfcpal = (BFCPal*)context->userdata;
    if (!bfcpal) {
        printf("Invalid BFCPal data...\n");
        return;
    }
    if (bfcpal->exiting){
        printf("BFCPal module is exiting... \n");
        return;
    }

    if(!recv_msg) {
        printf("Invalid message received...\n");
        return;
    }
    if(!recv_msg->arguments) {
        printf("Invalid arguments in the received message...\n");
        bfcp_free_received_message(recv_msg);
        return;
    }
    if(!recv_msg->entity) {
        printf("Invalid IDs in the message header...\n");
        bfcp_free_received_message(recv_msg);
        return;
    }


    unsigned long int conferenceID = recv_msg->entity->conferenceID;
    unsigned short int userID = recv_msg->entity->userID;
    unsigned short int transactionID = recv_msg->entity->transactionID;

    /* Output will be different according to the BFCP primitive in the message header */
    switch(recv_msg->primitive) {
        case Error:
            printf("\nError:\n");
            if(!recv_msg->arguments->error)
                break;
            printf("\tError n.    %d\n", recv_msg->arguments->error->code);
            printf("\tError info: %s\n", recv_msg->arguments->eInfo ? recv_msg->arguments->eInfo : "No info");
            break;
        case HelloAck:
            printf("\nHelloAck:\n");
            info_primitives = recv_msg->arguments->primitives;
            printf("\tSupported Primitives:\n");
            while(info_primitives != NULL) {
                printf("\t\t%s\n", bfcp_primitive[info_primitives->element-1].description);
                info_primitives = info_primitives->next;
            }
            info_attributes=recv_msg->arguments->attributes;
            printf("\tSupported Attributes:\n");
            while(info_attributes != NULL) {
                printf("\t\t%s\n", bfcp_attribute[info_attributes->element-1].description);
                info_attributes = info_attributes->next;
            }
            printf("\n");
            break;
        case ChairActionAck:
            printf("ChairActionAck:\n");
            printf("\tTransactionID: %d\n", transactionID);
            printf("\tUserID         %d\n", userID);
            printf("\tConferenceID:  %lu\n", conferenceID);
            printf("\n");
            break;
        case FloorRequestStatus:
        case FloorStatus: {
            if (recv_msg->primitive == FloorStatus)
                printf("FloorStatus:\n");
            else
                printf("FloorRequestStatus:\n");
            bfcp_floors_participant *floorStatus_node = NULL;

            printf("\tTransactionID: %d\n", transactionID);
            printf("\tUserID         %d\n", userID);
            printf("\tConferenceID:  %lu\n", conferenceID);
            if (recv_msg->arguments->fID != NULL)
                printf("\tFloorID:       %d\n", recv_msg->arguments->fID->ID);
            if (recv_msg->arguments->frqInfo) {
                tempInfo = recv_msg->arguments->frqInfo;
                while (tempInfo) {
                    printf("FLOOR-REQUEST-INFORMATION:\n");
                    if (tempInfo->frqID)
                        printf("   Floor Request ID:   %d\n", tempInfo->frqID);
                    if (tempInfo->oRS) {
                        printf("   OVERALL REQUEST STATUS:\n");
                        if (tempInfo->oRS->rs) {
                            printf("      Queue Position  %d\n", tempInfo->oRS->rs->qp);
                            printf("      RequestStatus   %s\n", bfcp_status[tempInfo->oRS->rs->rs - 1].description);
                            switch(tempInfo->oRS->rs->rs) {
                                case BFCP_GRANTED: {
                                    if (recv_msg->primitive == FloorStatus) {
                                        bfcpal->cbs.floor_granted_on_recv((void *) bfcpal->user_data);
                                        bfcpal->content_dir = CONTENT_RECV;
                                    } else {
                                        bfcpal->cbs.floor_granted_on_send((void *) bfcpal->user_data);
                                        bfcpal->content_dir = CONTENT_SEND;
                                    }
                                    bfcpal->status = BFCP_GRANTED;
                                }break;

                                case BFCP_RELEASED: {
                                    bfcpal->cbs.floor_release((void *) bfcpal->user_data);
                                    bfcpal->content_dir = CONTENT_STOP;
                                    bfcpal->status = BFCP_RELEASED;
                                }break;

                                case BFCP_DENIED: {
                                    bfcpal->cbs.floor_denied_on_send((void*)bfcpal->user_data);
                                    bfcpal->content_dir=CONTENT_STOP;
                                    bfcpal->status = BFCP_DENIED;
                                }break;


                                case BFCP_REVOKED:{
                                    bfcpal->cbs.floor_release((void*)bfcpal->user_data);
                                    bfcpal->content_dir=CONTENT_STOP;
                                    bfcpal->status = BFCP_REVOKED;
                                }break;

                                default:
                                    break;
                            }
                        }
                        if (tempInfo->oRS->sInfo)
                            printf("      Status Info:   %s\n", tempInfo->oRS->sInfo);
                    }
                    if (tempInfo->fRS) {
                        printf("   FLOOR REQUEST STATUS:\n");
                        tempID = tempInfo->fRS;
                        j = 0;


                        while (tempID) {
                            printf("   FLOOR IDs:\n");
                            j++;
                            printf("      (n.%d):  %d\n", j, tempID->fID);
                            if (floorStatus_node == NULL)
                                floorStatus_node = create_floor_list_p(tempID->fID, NULL);
                            else
                                floorStatus_node = insert_floor_list_p(floorStatus_node, tempID->fID, NULL);
                            if (tempID->rs->rs)
                                printf("      RequestStatus  %s\n", bfcp_status[tempID->rs->rs - 1].description);
                            printf("      Status Info:   %s\n", tempID->sInfo);
                            tempID = tempID->next;
                        }
                    }
                    if (tempInfo->beneficiary) {
                        printf("   BENEFICIARY-INFORMATION:\n");
                        if (tempInfo->beneficiary->ID)
                            printf("      Benefeciary ID: %d\n", tempInfo->beneficiary->ID);
                        if (tempInfo->beneficiary->display)
                            printf("      Display Name:   %s\n", tempInfo->beneficiary->display);
                        if (tempInfo->beneficiary->uri)
                            printf("      User URI:       %s\n", tempInfo->beneficiary->uri);
                    }
                    if (tempInfo->requested_by) {
                        printf("    REQUESTED BY INFORMATION:\n");
                        if (tempInfo->requested_by->ID)
                            printf("      Requested-by ID:  %d\n", tempInfo->requested_by->ID);
                        if (tempInfo->requested_by->display)
                            printf("      Display Name:     %s\n", tempInfo->requested_by->display);
                        if (tempInfo->requested_by->uri)
                            printf("      User URI:         %s\n", tempInfo->requested_by->uri);
                    }
                    if (tempInfo->priority)
                        printf("    PRIORITY:                   %d\n", tempInfo->priority);
                    if (tempInfo->pInfo)
                        printf("    PARTICIPANT PROVIDED INFO:  %s\n", tempInfo->pInfo);
                    printf("---\n");
                    tempInfo = tempInfo->next;
                }
            }
            if (recv_msg->primitive == FloorRequestStatus) {
                /* Allocate and setup the new participant */
                conference_participant struct_participant = (conference_participant) calloc(1,
                                                                                            sizeof(bfcp_participant_information));
                struct_participant->conferenceID = conferenceID;
                struct_participant->userID = userID;
                struct_participant->pfloors = floorStatus_node;


                bfcp_floorRequestStatusAck_participant(struct_participant, floorStatus_node, transactionID, context);
            }
            printf("\n");
        }
            break;
        case UserStatus:
            printf("UserStatus:\n");
            printf("\tTransactionID: %d\n", transactionID);
            printf("\tUserID         %d\n", userID);
            printf("\tConferenceID:  %ld\n", conferenceID);
            i = 0;
            if(recv_msg->arguments->beneficiary)
                printf("BeneficiaryInformation %d:\n", recv_msg->arguments->beneficiary->ID);
            tempInfo=recv_msg->arguments->frqInfo;
            while(tempInfo) {
                i++;
                printf("FLOOR-REQUEST-INFORMATION (%d):\n",i);
                tempID = tempInfo->fRS;
                j = 0;
                while(tempID) {
                    printf("   FLOOR IDs:\n");
                    j++;
                    printf("      (n.%d): %d\n", j, tempID->fID);
                    if(tempID->rs->rs) printf("      RequestStatus  %s\n", bfcp_status[tempID->rs->rs-1].description);
                    printf("      Status Info:   %s\n", tempID->sInfo ? tempID->sInfo : "No info");
                    tempID = tempID->next;
                }
                printf("   FloorRequestID %d\n", tempInfo->frqID);
                if(tempInfo->oRS) {
                    printf("   OVERALL REQUEST STATUS:\n");
                    if(tempInfo->oRS->rs){
                        printf("      Queue Position  %d\n", tempInfo->oRS->rs->qp);
                        printf("      RequestStatus   %s\n", bfcp_status[tempInfo->oRS->rs->rs-1].description);
                    }
                    if(tempInfo->oRS->sInfo)
                        printf("      Status Info:   %s\n", tempInfo->oRS->sInfo ? tempInfo->oRS->sInfo : "No info");
                }
                if(tempInfo->beneficiary)
                    printf("   BeneficiaryID  %d\n", tempInfo->beneficiary->ID);
                if(tempInfo->requested_by)
                    printf("   Requested_byID %d\n", tempInfo->requested_by->ID);
                printf("   Participant Provided info:     %s\n", tempInfo->pInfo ? tempInfo->pInfo : "No info");
                tempInfo = tempInfo->next;
            }
            printf("\n");
            break;

        default:
            break;
    }

    if(recv_msg != NULL)
        bfcp_free_received_message(recv_msg);
}

void keepalive_thread(void* bfcpal){

    while(true){
        BFCPal *bfcp = (BFCPal *)bfcpal;
        if (bfcp->exiting)
            break;

        uint64_t now = time(NULL);
        switch (bfcp->content_dir) {

            case CONTENT_STOP: {
                if (now - bfcp->last_send_hello_sec >= 5) {
                    bfcp_hello_participant(bfcp->bfcp_participant_list, bfcp->bfcp_context);
                    bfcp->last_send_hello_sec = now;
                }
            }
            break;

            case CONTENT_SENDING: {

                //resend floorRequest every one second

            }
            break;

            case CONTENT_STOPPING: {
                //resend floorRelease every one second
                //stopped after 5 time try
            }
            break;

            default:
                break;
        }

        sleep(1);
    }

    printf("bfcpal worker thread exit. \n");
}