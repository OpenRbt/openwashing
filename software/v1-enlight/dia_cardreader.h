#ifndef CARDREADERDRIVER_H
#define CARDREADERDRIVER_H

#include "dia_device.h"
#include "dia_log.h"
#include <stdio.h>

#define DIA_CARDREADER_NO_ERROR 0
#define DIA_CARDREADER_NULL_PARAMETER 4
#define TO_STR std::to_string
#define DIA_CARDREADER_LOG_TYPE "cardreader"

class DiaCardReader
{
public: 
    void (*IncomingMoneyHandler)(void * manager, int moneyType, int newMoney);

    void * _Manager;
    Logger* logger = nullptr;

    pthread_t ExecuteDriverProgramThread;
    pthread_mutex_t MoneyLock = PTHREAD_MUTEX_INITIALIZER;
    int ToBeDeleted;
    int RequestedMoney;

    DiaCardReader(void * manager, void (*incomingMoneyHandler)(void * cardreader, int moneyType, int newMoney), Logger* logger) {
        _Manager = manager;
        IncomingMoneyHandler = incomingMoneyHandler;
        RequestedMoney = 0;
        this->logger = logger;
        printf("Card Reader created\n");
    }
    
    ~DiaCardReader() {
        ToBeDeleted = 1;
    }
};

void DiaCardReader_AbortTransaction(void * specificDriver);

int DiaCardReader_GetTransactionStatus(void * specificDriver);

void* DiaCardReader_ExecuteDriverProgramThread(void * devicePtr);

int DiaCardReader_PerformTransaction(void * specficDriver, int money);

int DiaCardReader_StopDriver(void * specficDriver);

#endif
