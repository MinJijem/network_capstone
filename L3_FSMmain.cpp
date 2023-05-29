#include "L3_FSMevent.h"
#include "L3_msg.h"
#include "L3_timer.h"
#include "L3_LLinterface.h"
#include "protocol_parameters.h"
#include "mbed.h"


//FSM state -------------------------------------------------
#define L3STATE_IDLE                0
#define CHATTING                    1
#define ENDCHAT                     2
#define MATCHING                    3

#define MANAGER_ID                  0

//state variables
static uint8_t main_state = L3STATE_IDLE; //protocol state
static uint8_t prev_state = main_state;

//SDU (input)
static uint8_t originalWord[1030];
static uint8_t wordLen=0;

static uint8_t sdu[1030];

//serial port interface
static Serial pc(USBTX, USBRX);
static uint8_t myDestId;

//application event handler : generating SDU from keyboard input
static void L3service_processInputWord(void)
{
    char c = pc.getc();
    if (!L3_event_checkEventFlag(L3_event_dataToSend))
    {
        if (c == '\n' || c == '\r')
        {
            originalWord[wordLen++] = '\0';
            L3_event_setEventFlag(L3_event_dataToSend);
            debug_if(DBGMSG_L3,"word is ready! ::: %s\n", originalWord);
        }
        else
        {
            originalWord[wordLen++] = c;
            if (wordLen >= L3_MAXDATASIZE-1)
            {
                originalWord[wordLen++] = '\0';
                L3_event_setEventFlag(L3_event_dataToSend);
                pc.printf("\n max reached! word forced to be ready :::: %s\n", originalWord);
            }
        }
    }
}


void L3_initFSM(uint8_t destId)
{
    myDestId = destId;
    //initialize service layer
    pc.attach(&L3service_processInputWord, Serial::RxIrq);
    pc.printf("Wanna start a random chat? press the [Enter]");
}

void L3_FSMrun(void)
{   
    if (prev_state != main_state)
    {
        debug_if(DBGMSG_L3, "[L3] State transition from %i to %i\n", prev_state, main_state);
        prev_state = main_state;
    }

    //FSM should be implemented here! ---->>>>
    switch (main_state)
    {
        case L3STATE_IDLE: //IDLE state description
            if (L3_event_checkEventFlag(L3_event_dataToSend)){
                pc.printf("finding user... please wait.\n");
                strcpy((char*)sdu, (char*)originalWord);
                debug("[L3] msg length : %i\n", wordLen);
                L3_LLI_dataReqFunc(sdu, wordLen, myDestId);
                debug_if(DBGMSG_L3, "[L3] sending msg....\n");
                wordLen = 0;
                L3_event_clearEventFlag(L3_event_dataToSend);
                prev_state = main_state;
                main_state = MATCHING;
            }
            break;
        case MATCHING:
            if (L3_event_checkEventFlag(L3_event_matchRcvd)){
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                uint8_t size = L3_LLI_getSize();
                debug("\n -------------------------------------------------\nRCVD Matched ID : %s (length:%i)\n -------------------------------------------------\n", 
                            dataPtr, size);
                if(dataPtr[0]){
                    myDestId = dataPtr[0];
                    prev_state = main_state;
                    main_state = CHATTING;
                    pc.printf("\n -------------------------------------------------\nStart a chat!\n -------------------------------------------------\n");
                    pc.printf("Give a word to send : ");
                }
                else {
                    pc.printf("[MatchERR] Please restart.");
                }
                L3_event_clearEventFlag(L3_event_matchRcvd);
            }
            break;
        case CHATTING:
            if (L3_event_checkEventFlag(L3_event_msgRcvd)) //if data reception event happens
            {
                //Retrieving data info.
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                uint8_t size = L3_LLI_getSize();
                if(L3_msg_isEndChat((char*)dataPtr)){
                    pc.printf("\n -------------------------------------------------\nChatting Endded\n -------------------------------------------------\n");
                    pc.printf("Wanna add this user to blacklist?(y/n) :");
                    prev_state = main_state;
                    main_state = ENDCHAT;
                    L3_event_clearEventFlag(L3_event_msgRcvd);
                    break;
                }

                debug("\n -------------------------------------------------\nRCVD MSG : %s (length:%i)\n -------------------------------------------------\n", 
                            dataPtr, size);
                
                pc.printf("Give a word to send : ");
                
                L3_event_clearEventFlag(L3_event_msgRcvd);
            }
            else if (L3_event_checkEventFlag(L3_event_dataToSend)) //if data needs to be sent (keyboard input)
            {
                //end chat condition check
                if(L3_msg_isEndChat((char*)originalWord)){
                    //L3_event_clearEventFlag(L3_event_dataToSend);
                    pc.printf("\n -------------------------------------------------\nChatting Endded\n -------------------------------------------------\n");
                    strcpy((char*)sdu, (char*)originalWord);
                    debug("[L3] msg length : %i\n", wordLen);
                    L3_LLI_dataReqFunc(sdu, wordLen, myDestId);
                    debug_if(DBGMSG_L3, "[L3] sending msg....\n");
                    wordLen = 0;
                    pc.printf("Wanna add this user to blacklist?(y/n) :");
                    //myDestId = MANAGER_ID;
                    prev_state = main_state;
                    main_state = ENDCHAT;
                    L3_event_clearEventFlag(L3_event_dataToSend);
                    break;
                }
                //msg header setting
                strcpy((char*)sdu, (char*)originalWord);
                debug("[L3] msg length : %i\n", wordLen);
                L3_LLI_dataReqFunc(sdu, wordLen, myDestId);

                debug_if(DBGMSG_L3, "[L3] sending msg....\n");
                wordLen = 0;

                pc.printf("Give a word to send : ");

                L3_event_clearEventFlag(L3_event_dataToSend);
            }
            else if(L3_event_checkEventFlag(L3_event_disconnected)){
                pc.printf("\n -------------------------------------------------\nThe connection has been lost!\n -------------------------------------------------\n");

                pc.printf("Wanna add this user to blacklist?(y/n) :");
                myDestId = MANAGER_ID;
                prev_state = main_state;
                main_state = ENDCHAT;
                L3_event_clearEventFlag(L3_event_disconnected);
            }
            break;
        case ENDCHAT:
            if(L3_event_checkEventFlag(L3_event_dataToSend)){
                pc.printf("this is ENDCHAT-dataToSend\n");
                myDestId = MANAGER_ID;
                strcpy((char*)sdu, (char*)originalWord);
                debug("[L3] msg length : %i\n", wordLen);
                L3_LLI_dataReqFunc(sdu, wordLen, myDestId);
                debug_if(DBGMSG_L3, "[L3] sending msg....\n");
                wordLen = 0;
                L3_event_clearEventFlag(L3_event_dataToSend);
                prev_state = main_state;
                main_state = MATCHING;
            }   
            break;
        default :
            break;
    }
}