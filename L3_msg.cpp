#include "mbed.h"
#include "L3_msg.h"
#include "L3_FSMevent.h"

int L3_msg_isEndChat(char * originalWord){
    const char * ENDWORD1 = "exit";
    const char * ENDWORD2 = "EXIT";
    if(strncmp(originalWord, ENDWORD1, 4)==0 || strncmp(originalWord, ENDWORD2, 4)==0){
        //L3_event_setEventFlag(L3_event_endChat);
        return 1;
    }
    else {
        return 0;
    }
}