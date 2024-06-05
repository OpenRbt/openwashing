#ifndef DIA_DEVICE_MANAGER
#define DIA_DEVICE_MANAGER

#include <list>
#include "dia_device.h"
#include "dia_cardreader.h"
#include "dia_vendotek.h"
#include "dia_log.h"
#include <pthread.h>
#include <stdlib.h>

#define DIAE_DEVICE_MANAGER_NOERROR 0
#define DIA_DEVICE_MANAGER_LOG_TYPE "devicemanager"

enum CARD_READER_STATUS {PAYMENT_WORLD_SUCCCES, VENDOTEK_SUCCES,NOT_USED, VENDOTEK_NO_HOST, VENDOTEK_NULL_DRIVER, VENDOTEK_THREAD_ERROR, VENDOTEK_NOT_FOUND};

class DiaDeviceManager {
public:
    int CoinMoney;
    int BanknoteMoney;
    int ElectronMoney;
    int ServiceMoney;
    
    int NeedWorking;
    char * _PortName;
    Logger* logger = nullptr;

    DiaCardReader* _CardReader = NULL;
    DiaVendotek* _Vendotek = NULL;
    
    std::list<DiaDevice*> _Devices;

    pthread_t WorkingThread;
    DiaDeviceManager(Logger* logger);
    ~DiaDeviceManager();
};
void * DiaDeviceManager_WorkingThread(void * manager);
void DiaDeviceManager_ScanDevices(DiaDeviceManager * manager);
void DiaDeviceManager_StartDeviceScan(DiaDeviceManager * manager);
void DiaDeviceManager_FinishDeviceScan(DiaDeviceManager * manager);
void DiaDeviceManager_CheckOrAddDevice(DiaDeviceManager *manager, char * PortName, int isACM);
void DiaDeviceManager_ReportMoney(void *manager, int moneyType, int Money);
void DiaDeviceManager_PerformTransaction(void *manager, int money, bool isTransactionSeparated);
//-------------------------------------------------------------------------------
int DiaDeviceManager_ConfirmTransaction(void* manager, int money);
//--------------------------------------------------------------------------------
void DiaDeviceManager_AbortTransaction(void *manager);
int DiaDeviceManager_GetTransactionStatus(void *manager);
void DiaDeviceManager_AddCardReader(DiaDeviceManager * manager);
int DiaDeviceManager_AddVendotek(DiaDeviceManager * manager, std::string host, std::string port);
int DiaDeviceManager_GetCardReaderStatus(void *manager);

#endif
