#ifndef NVDRIVER_H
#define NVDRIVER_H
#include <3rd/aes/aes.h>
#include "dia_device.h"
#define DIA_NV_NO_ERROR 0
#define DIA_NV_UNKNOWN_COMMAND 1
#define DIA_NV_EMPTY_COMMAND 2
#define DIA_NV_THREAD_ERROR 3
#define DIA_NV_NULL_PARAMETER 4

#define DIA_NV_JUSTCREATED 0
#define DIA_NV_DRIVER_STARTED 1
#define DIA_NV_DRIVER_OPERATIONAL_STATE 2
#define DIA_NV_DRIVER__MONEY_ON_DEPOSITE 3
#define DIA_NV_DRIVER__ERROR_HAPPENED_PLEASE_REINITIALIZE 4

class DiaNv
{
public:
    static int InitStage;
    int Money;

    int CurrentMode;
    void (*IncomingMoneyHandler)(void * manager, int moneyType, int newMoney);

    bool SequenceBit;
    int ChannelNumber;
    int ChannelValues[128];
    bool IsNoteFloatAttached; // true for nv11

    uint8_t EncryptionKeyBytes[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x23, 0x45, 0x67, 0x01, 0x23, 0x45, 0x67};
    unsigned int eCount;
    bool EncryptPackets = false;
    DiaDevice * _Device;


    pthread_t CommandReadingThread;
    pthread_mutex_t MoneyLock = PTHREAD_MUTEX_INITIALIZER;
    int ToBeDeleted;

    DiaNv(DiaDevice * device, void (*incomingMoneyHandler)(void * nv11, int moneyType, int newMoney) ) {
        _Device = device;
        IncomingMoneyHandler = incomingMoneyHandler;
	ToBeDeleted = 0;
    }
    
    ~DiaNv() {
        ToBeDeleted = 1;
    }

    int SendCommand(DiaNv * driver, uint8_t *command, int size);


};
int DiaNv_InitDriver(void * specficDriver);
int DiaNv_StartDriver(void * specficDriver);
int DiaNv_StopDriver(void * specficDriver);

void * DiaNv_ProcessingThread(void * driverPtr);

int DiaNv_GetBalance(void * specificDriver);
void DiaNv_CleanBalance(void * specificDriver, int balanceToClean);

uint8_t DiaNv_SendCommandAndReadAnswerCode(DiaNv * driver, uint8_t* command, bool isDebug = false);
void DiaNv_InsertCrc16(uint8_t *source, int len);
void DiaNv_ByteStuffing(uint8_t *source, int &len);
void DiaNv_UndoByteStuffing(char *source, int &len);

int DiaNv_TurnOn(DiaNv * driver);
void DiaNv_TurnOff(DiaNv * driver);

int DiaNv_GetPollArray(DiaNv * driver, uint8_t * &array);
void DiaNv9Usb_ProcessEvent(DiaNv * driver, uint8_t events[], int &index);
void DiaNv11_ProcessEvent(DiaNv * driver, uint8_t * events, int &index);

void DiaNv_Reset(DiaNv * driver);
void DiaNv_ResetFixedKey(DiaNv * driver);
uint8_t DiaNv_Ack(DiaNv * driver);
uint8_t DiaNv_SetInhibitsOff(DiaNv * driver);
uint8_t DiaNv_SetInhibitsOn(DiaNv * driver);

int DiaNv_GetChannelsValues(DiaNv * driver);
int DiaNv_SetupEncryption(DiaNv * driver);
#endif
