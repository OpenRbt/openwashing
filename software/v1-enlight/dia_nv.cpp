#include "dia_nv.h"
#include "stdio.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "dia_device.h"
#include <money_types.h>
#include <assert.h>

uint8_t nv_enable[] = {0x7F, 0x80, 0x01, 0x0A, 0x3F, 0x82};
uint8_t nv_sync[] = {0x7F, 0x80, 0x01, 0x11, 0x65, 0x82}; 
uint8_t nv_disable[] = {0x7F, 0x80, 0x01, 0x09, 0x35, 0x82}; 
uint8_t nv_poll[] = {0x7F, 0x80, 0x01, 0x07, 0x12, 0x02}; 
uint8_t nv_pollWithACK[] = {0x7F, 0x80, 0x01, 0x56, 0xF7, 0x83};       
uint8_t nv_eventACK[] = {0x7F, 0x80, 0x01, 0x57, 0xF2, 0x03};        
uint8_t nv_setupRequest[] = {0x7F, 0x80, 0x01, 0x05, 0x1D, 0x82}; 
uint8_t nv_hostProtocolVersion[] = {0x7F, 0x80, 0x02, 0x06, 0x06, 0x24, 0x14}; 
uint8_t nv_displayOn[] = {0x7F, 0x80, 0x01, 0x03, 0x09, 0x82}; 
uint8_t nv_setChannelInhibits_code = 0x02; 
uint8_t nv11_enablePayoutDevice[] = {0x7F, 0x80, 0x02, 0x5C, 0x03, 0x3F, 0x48};
uint8_t nv_reject[] = {0x7F, 0x80, 0x01, 0x08, 0x30, 0x02};
uint8_t nv_lastRejectCode[] = {0x7F, 0x80, 0x01, 0x17, 0x71, 0x82};
uint8_t nv_reset[] = {0x7F, 0x80, 0x01, 0x01, 0x06, 0x02};
uint8_t nv_setGenerator[] = {0x7F, 0x80, 0x09, 0x4A, 0xC5, 0x05, 0x8F, 0x3A, 0x00, 0x00, 0x00, 0x00, 0xB2, 0x73};
uint8_t nv_setModulus[] = {0x7F, 0x80, 0x09, 0x4B, 0x8D, 0xA6, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C, 0xF6};
uint8_t nv_requestKeyExchange[] = {0x7F, 0x80, 0x09, 0x4C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9D, 0x52};
uint8_t nv_sspEncryptionResetToDefault[] = {0x7F, 0x80, 0x01, 0x61, 0x46, 0x03};
uint8_t nv11_getNotePositions[] = {0x7F, 0x80, 0x01, 0x41, 0x85, 0x83}; 

enum nv_statusCodes 
{ 
    OK = 0xF0, UnknownCommand = 0xF2, WrongParametersNumber = 0xF3,
    ParameterOutOfRange = 0xF4, CommandCantBeProcessed = 0xF5,                
    SoftwareError = 0xF6, Fail = 0xF8, EncryptionKeyNotSet = 0xFA                
};

enum nv_pollCodes // all codes that require action
{ 
    unsafeNoteJam = 0xE9, readEvent = 0xEF, refillNoteCredit = 0x9E,
    disabled = 0xE8, stackerFull = 0xE7, noteClearedFromFront = 0xE1, 
    creditNote = 0xEE, fraudAttempt = 0xE6, ticketInBezelAtStartup = 0xA7,
    noteClearedToCashbox = 0xE2, rejected = 0xEC, ticketPrintingError = 0xA8,
    slaveReset = 0xF1, channelDisable = 0xB5,

    //nv11 specific:
    noteFloatRemoved = 0xC7, deviceFull = 0xCF,
    dispensing = 0xDA, dispensed = 0xD2, smartEmptying = 0xB3, 
    smartEmptied = 0xB4, noteStoredInPayout = 0xDB, payoutOutOfService = 0xC6,
    noteTransferedToStacker = 0xC9, noteIntoStoreAtReset = 0xCB, 
    noteIntoStackerAtReset = 0xCA, noteDispensedAtReset = 0xCD,
    payoutHalted = 0xD6, hopperOrPayoutJammed = 0xD5,
};

int DiaNv::InitStage = 0;

void DiaNv_InsertCrc16(uint8_t *source, int len, int startByte)
{
    uint16_t seed = 0xFFFF;
    uint16_t poly = 0x8005;
    uint16_t crc = seed;

    for (int i = startByte; i < len - 2; i++) {
        crc ^= (source[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = ((crc << 1) & 0xffff) ^ poly;
            } else {
                crc <<= 1;
            }
        }
    }
    source[len - 2] = (crc & 0xFF);
    source[len - 1] = ((crc >> 8) & 0xFF);
}

void DiaNv_ByteStuffing(uint8_t *source, int &len)
{
    for (int i = 1; i < len; i++)
    {
        if (source[i] == 0x7F)
        {
            for (int j = len - 1; j >= i; j--)
                source[j + 1] = source[j];
            len += 1; i += 1;
        }
    }
}

void DiaNv_UndoByteStuffing(char *source, int &len)
{
    for (int i = 1; i < len; i++)
    {
        if (source[i] == 0x7F && source[i + 1] == 0x7F)
        {
            for (int j = i + 1; j < len - 1; j++)
                source[j] = source[j + 1];
            len -= 1; 
        }
    }
}

int DiaNv_GetBalance(void * specificDriver)
{
    DiaNv * driver = (DiaNv *)specificDriver;
    return driver->Money;
}

void DiaNv_CleanBalance(void * specificDriver, int balanceToClean) 
{
    DiaNv * driver = (DiaNv *) specificDriver;
    pthread_mutex_lock(&driver->MoneyLock);
    driver->Money = driver->Money - balanceToClean;
    pthread_mutex_unlock(&driver->MoneyLock);
}

void DiaNv_Reset(DiaNv * driver)
{
    printf("sync>>\n");
    DiaNv_SendCommandAndReadAnswerCode(driver, nv_sync);
    driver->SequenceBit = 0;
    printf("reset>>\n");
    driver -> SendCommand(driver, nv_reset, 6);

}

void DiaNv_ResetFixedKey(DiaNv * driver)
{
    printf("sync>>\n");
    DiaNv_SendCommandAndReadAnswerCode(driver, nv_sync);
    driver->SequenceBit = 0;
    printf("reset fixed key>>\n");
    driver -> SendCommand(driver, nv_sspEncryptionResetToDefault, 6);
}

uint8_t DiaNv_Ack(DiaNv * driver)
{
    return DiaNv_SendCommandAndReadAnswerCode(driver, nv_eventACK, false);
}

// allow money
uint8_t DiaNv_SetInhibitsOff(DiaNv * driver) 
{
    uint8_t channelBytes = driver -> ChannelNumber / 8;
    uint8_t command[10] = {0x7F, 0x00, uint8_t(channelBytes + 1), nv_setChannelInhibits_code}; // up to 32 channels, 16 is maximum for most devices
    for (int i = 0; i < channelBytes; i++)
        command[4 + i] = 0xFF;
    return DiaNv_SendCommandAndReadAnswerCode(driver, command);
}

// forbid money
uint8_t DiaNv_SetInhibitsOn(DiaNv * driver) 
{
    uint8_t channelBytes = driver -> ChannelNumber / 8;
    uint8_t command[10] = {0x7F, 0x00, uint8_t(channelBytes + 1), nv_setChannelInhibits_code}; // up to 32 channels, 16 is maximum for most devices
    for (int i = 0; i < channelBytes; i++)
        command[4 + i] = 0x00;
    return DiaNv_SendCommandAndReadAnswerCode(driver, command);
}

int DiaNv_GetPollArray(DiaNv * driver, uint8_t array[])
{
    if (DiaNv_SendCommandAndReadAnswerCode(driver, nv_pollWithACK, false) != nv_statusCodes::OK) {
        printf("NV driver couldn't retrieve events\n");
        return 0;
    }
    int dataSize = driver -> _Device ->_Buf[2]; 
    for (int i = 0; i < dataSize - 1; i++)
    {
        array[i] = driver -> _Device ->_Buf[4 + i];
    }
    return dataSize - 1;
}

void DiaNv9Usb_ProcessEvent(DiaNv * driver, uint8_t events[], int &index)
{
    switch (events[index]) {
        //case nv_pollCodes::refillNoteCredit: 
        case nv_pollCodes::creditNote: 
            {
                uint8_t channelCode = events[index + 1];
                index += 1;
                if (channelCode < 0 || channelCode > driver->ChannelNumber)
                {
                    printf("Received wrong channel code: [%d]. Refetching channel data...\n", channelCode); 
                    if (DiaNv_GetChannelsValues(driver) != DIA_NV_NO_ERROR)          
                    {
                        printf("Failed to fetch channel data. Stopping the driver\n"); 
                        DiaNv_StopDriver(driver);
                    }            
                    printf("Successfully fetched channel data\n"); 
                }
                int new_money = driver->ChannelValues[channelCode];
                
                int ackStatus = DiaNv_Ack(driver);
                while (ackStatus != nv_statusCodes::OK)        
                {
                    printf("Failed to ack, retrying...\n");        
                    ackStatus = DiaNv_Ack(driver);
                }
                if(new_money > 0) 
                {               
                    printf("Got note. Channel: [%d], Value: [%d]\n", channelCode, new_money);        
                    if (driver->IncomingMoneyHandler){
                        driver->IncomingMoneyHandler(driver->_Device->Manager, DIA_BANKNOTES, new_money);
                    } else {
                        printf("Can't report money :( \n");
                    }
                }            
            }
            break;
        case nv_pollCodes::unsafeNoteJam:
            DiaNv_SendCommandAndReadAnswerCode(driver, nv_reject);
        case nv_pollCodes::fraudAttempt:
            DiaNv_Ack(driver);
            index += 1; 
            DiaNv_SendCommandAndReadAnswerCode(driver, nv_reject);
        case nv_pollCodes::readEvent:
            index += 1;
            break;
        case nv_pollCodes::disabled:
            if (DiaNv_SendCommandAndReadAnswerCode(driver, nv_enable) != nv_statusCodes::OK)
            {
                printf("Can't enable device");                   
            }
            break;      
        case nv_pollCodes::stackerFull: 
            printf("Stacker full\n");
            break;
        case nv_pollCodes::noteClearedFromFront:
            DiaNv_Ack(driver);
            index += 1;
            break;
        case nv_pollCodes::noteClearedToCashbox:
            DiaNv_Ack(driver);
            index += 1;
            break;
        case nv_pollCodes::rejected: 
            if (DiaNv_SendCommandAndReadAnswerCode(driver, nv_lastRejectCode) != nv_statusCodes::OK)
                printf("Banknote rejected. Error code: [%d]\n", driver -> _Device ->_Buf[4]);
            else
                printf("Banknote rejected. Unable to retrieve error code\n");
            break;           
        case nv_pollCodes::ticketPrintingError: // might be an error in docs 
            printf("Ticket printing error, code: [%d]\n", events[index + 1]);
            index += 1;
            break;
        case nv_pollCodes::slaveReset: 
            DiaNv_TurnOn(driver);
            break;
        case nv_pollCodes::ticketInBezelAtStartup: 
            break;
        case nv_pollCodes::channelDisable: 
            DiaNv_SetInhibitsOff(driver);
            break;
    }
}

void DiaNv11_ProcessEvent(DiaNv * driver, uint8_t events[], int &index)
{
    switch (events[index]) {
        //case nv_pollCodes::refillNoteCredit: 
        case nv_pollCodes::creditNote: 
            {
                uint8_t channelCode = events[index + 1];
                index += 1;
                if (channelCode < 0 || channelCode > driver->ChannelNumber)
                {
                    printf("Received wrong channel code: [%d]. Refetching channel data...\n", channelCode); 
                    if (DiaNv_GetChannelsValues(driver) != DIA_NV_NO_ERROR)          
                    {
                        printf("Failed to fetch channel data. Stopping the driver\n"); 
                        DiaNv_StopDriver(driver);
                    }            
                    printf("Successfully fetched channel data\n"); 
                }
                int new_money = driver->ChannelValues[channelCode];
                
                int ackStatus = DiaNv_Ack(driver);
                while (ackStatus != nv_statusCodes::OK)        
                {
                    printf("Failed to ack, retrying...\n");        
                    ackStatus = DiaNv_Ack(driver);
                }  
                if(new_money > 0) 
                {               
                    printf("Got note. Channel: [%d], Value: [%d]\n", channelCode, new_money);        
                    if (driver->IncomingMoneyHandler){
                        driver->IncomingMoneyHandler(driver->_Device->Manager, DIA_BANKNOTES, new_money);
                    } else {
                        printf("Can't report money :( \n");
                    }
                }            
            }
            break;
        case nv_pollCodes::stackerFull:
        case nv_pollCodes::deviceFull:
            printf("Stacker or Device full\n");
            break;
        case nv_pollCodes::noteFloatRemoved:
            break;
        case nv_pollCodes::unsafeNoteJam:
            DiaNv_SendCommandAndReadAnswerCode(driver, nv_reject);
        case nv_pollCodes::fraudAttempt:
            DiaNv_Ack(driver);
            index += 1; 
            DiaNv_SendCommandAndReadAnswerCode(driver, nv_reject);
        case nv_pollCodes::readEvent:
            index += 1;
            break;
        case nv_pollCodes::disabled:
            if (DiaNv_SendCommandAndReadAnswerCode(driver, nv_enable) != nv_statusCodes::OK)
            {
                printf("Can't enable device");                   
            }
            break;    
        case nv_pollCodes::noteClearedFromFront:
            DiaNv_Ack(driver);
            index += 1;
            break;
        case nv_pollCodes::noteClearedToCashbox:
            DiaNv_Ack(driver);
            index += 1;
            break;
        case nv_pollCodes::dispensing:
            index += 1 + events[index + 1] * 7;
            break;
        case nv_pollCodes::dispensed:
            DiaNv_Ack(driver);
            index += 1 + events[index + 1] * 7;
            break;
        case nv_pollCodes::hopperOrPayoutJammed:
            index += 1 + events[index + 1] * 7;
            break;
        case nv_pollCodes::smartEmptying:
            index += 1 + events[index + 1] * 7;
            break;
        case nv_pollCodes::smartEmptied:
            DiaNv_Ack(driver);
            index += 1 + events[index + 1] * 7;
            break;
        case nv_pollCodes::noteStoredInPayout:
            index += 8; // changeable by command, 8 is for the default setting
            break;
        case nv_pollCodes::payoutOutOfService: 
            if (DiaNv_SendCommandAndReadAnswerCode(driver, nv11_enablePayoutDevice))
            {
                printf("Failed to enable payout device\n");
            }
            break;
        case nv_pollCodes::noteTransferedToStacker:
            DiaNv_Ack(driver);
            index += 8; // changeable by command, 8 is for the default setting
            break;
        case nv_pollCodes::noteIntoStoreAtReset: 
            DiaNv_Ack(driver);
            index += 8; // changeable by command, 8 is for the default setting
            break;
        case nv_pollCodes::noteIntoStackerAtReset:  
            DiaNv_Ack(driver);
            index += 8; // changeable by command, 8 is for the default setting
            break;
        case nv_pollCodes::noteDispensedAtReset:
            DiaNv_Ack(driver);
            index += 8; // changeable by command, 8 is for the default setting
            break;
        case nv_pollCodes::payoutHalted: 
            index += 1 + events[index + 1] * 7;
            break;
        case nv_pollCodes::rejected: 
            if (DiaNv_SendCommandAndReadAnswerCode(driver, nv_lastRejectCode) == nv_statusCodes::OK)
                printf("Banknote rejected. Error code: [%d]\n", driver -> _Device ->_Buf[4]);
            else
                printf("Banknote rejected. Unable to retrieve error code\n");
            break;
        case nv_pollCodes::slaveReset: 
            DiaNv_TurnOn(driver);
            break;
        case nv_pollCodes::channelDisable: 
            DiaNv_SetInhibitsOff(driver);
            break;
        
    }
}

void * DiaNv_ProcessingThread(void * driverPtr) {
    printf("NV main processing thread...\n");
    DiaNv * driver = (DiaNv *)driverPtr;
    uint8_t events[255];  
    while(!driver->ToBeDeleted) {
        usleep(500000); 
        int eventNumber = DiaNv_GetPollArray(driver, events);
        if (eventNumber < 1)
            continue;
        printf("Got events: ");
        for (int ev = 0; ev < eventNumber; ev++)
            printf("[%d] ", events[ev]);
        printf("\n");
        int i = 0;
        while (i < eventNumber)
        {
            if (driver->IsNoteFloatAttached)
                DiaNv11_ProcessEvent(driver, events, i);
            else 
                DiaNv9Usb_ProcessEvent(driver, events, i);
            i = i + 1;
        }
    }
    printf("NV: Exit from thread\n");
    pthread_exit(NULL);
    return NULL;
}

int DiaNv_InitDriver(void * specficDriver) {
    switch(DiaNv::InitStage)
    {
        case 0:
            DiaNv_ResetFixedKey((DiaNv *) specficDriver);
            DiaNv::InitStage = 1;
            break;
        case 1:  
            DiaNv_Reset((DiaNv *) specficDriver);
            DiaNv::InitStage = 2;
            break;
        case 2:   
            DiaNv::InitStage = 0;
            return DiaNv_StartDriver(specficDriver);
            break;
    }
    return DiaNv::InitStage;
}

int DiaNv_StartDriver(void * specficDriver) {
    DiaNv * driver = (DiaNv *) specficDriver;
    printf("sync>>\n");
    int err = DiaNv_SendCommandAndReadAnswerCode(driver, nv_sync);
    driver->SequenceBit = 0;
    if (err != nv_statusCodes::OK) {
        printf("can't sync with cardreader\n");
        return 1;
    }
    printf("poll>>\n");

    err = DiaNv_SendCommandAndReadAnswerCode(driver, nv_poll);
    if (err != nv_statusCodes::OK) {
        printf("can't poll cardreader\n");
        return 1;
    }

    err = DiaNv_TurnOn(driver);
    if (err != DIA_NV_NO_ERROR) {
        printf("can't start cardreader, error code :[%d]\n", err);
        return 1;
    }

    err = pthread_create(&driver->CommandReadingThread, NULL, &DiaNv_ProcessingThread, driver);
    if (err != DIA_NV_NO_ERROR) {
        printf("can't create thread :[%s]\n", strerror(err));
        return 1;
    }

    return DIA_NV_NO_ERROR;
}

int DiaNv_StopDriver(void * specficDriver)
{
    if(specficDriver == NULL)
    {
        return DIA_NV_NULL_PARAMETER;
    }
    DiaNv * driver = (DiaNv *) specficDriver;
    driver->ToBeDeleted = 1;
    pthread_join(driver->CommandReadingThread, NULL);
    return DIA_NV_NO_ERROR;
}

int DiaNv_GetChannelsValues(DiaNv * driver)
{
    if (DiaNv_SendCommandAndReadAnswerCode(driver, nv_setupRequest, false) != nv_statusCodes::OK) { 
        printf("NV driver couldn't retrieve channel values\n");
        return -1;
    }
    driver -> IsNoteFloatAttached = driver -> _Device ->_Buf[4] == 0x07;
    driver -> ChannelNumber = driver -> _Device ->_Buf[4 + 11]; 
    driver -> ChannelValues[0] = 0; 
    int basicOffset = 4 + (16 + (driver -> ChannelNumber * 5));
    printf("Channel values: ");
    for (int i = 1; i < driver -> ChannelNumber + 1; i++)
    {
        driver -> ChannelValues[i] = 
            (uint8_t)driver -> _Device ->_Buf[basicOffset + (i - 1) * 4]
            + (uint8_t)driver -> _Device ->_Buf[basicOffset + (i - 1) * 4 + 1] * 256 
            + (uint8_t)driver -> _Device ->_Buf[basicOffset + (i - 1) * 4 + 2] * 256 * 256
            + (uint8_t)driver -> _Device ->_Buf[basicOffset + (i - 1) * 4 + 3] * 256 * 256 * 256;
        printf("[%d]: %d; ", i, driver -> ChannelValues[i]);
    }
    printf("\n");
    return DIA_NV_NO_ERROR;
}

int DiaNv_SetupEncryption(DiaNv * driver)
{
    driver -> EncryptPackets = false;
    unsigned long long generator = 115249; // set generator
    unsigned long long gen = generator;
    uint8_t generatorBytes[8];
    for (int i = 0; i < 8; i++)
        if (gen > 0)
        {
            generatorBytes[i] = gen % 256;
            gen = gen / 256;
        }
        else
            generatorBytes[i] = 0;
        
    uint8_t setGenerator[14];
    for (int i = 0; i < 14; i++)
        setGenerator[i] = nv_setGenerator[i];
    for (int i = 0; i < 8; i++)
        setGenerator[4 + i] = generatorBytes[i];
    printf("set generator>>\n"); 
    if (DiaNv_SendCommandAndReadAnswerCode(driver, setGenerator) != nv_statusCodes::OK)
        return 10;
    
    unsigned long long modulus = 4801; // set modulus
    unsigned long long mod = modulus;
    uint8_t modulusBytes[8];
    for (int i = 0; i < 8; i++)
        if (mod > 0)
        {
            modulusBytes[i] = mod % 256;
            mod = mod / 256;
        }
        else
            modulusBytes[i] = 0;
        
    uint8_t setModulus[14];
    for (int i = 0; i < 14; i++)
        setModulus[i] = nv_setModulus[i];
    for (int i = 0; i < 8; i++)
        setModulus[4 + i] = modulusBytes[i];
    printf("set modulus>>\n"); 
    if (DiaNv_SendCommandAndReadAnswerCode(driver, setModulus) != nv_statusCodes::OK)
        return 20;
    
    unsigned long long hostInterKey = generator % modulus; // HOST_RND = 1
    uint8_t hostInterKeyBytes[8];
    for (int i = 0; i < 8; i++)
        if (hostInterKey > 0)
        {
            hostInterKeyBytes[i] = hostInterKey % 256;
            hostInterKey = hostInterKey / 256;
        }
        else
            hostInterKeyBytes[i] = 0;
        
    uint8_t requestKeyExchange[14]; // key exchange
    for (int i = 0; i < 14; i++)
        requestKeyExchange[i] = nv_requestKeyExchange[i];
    for (int i = 0; i < 8; i++)
        requestKeyExchange[4 + i] = hostInterKeyBytes[i];

    printf("request key exchange>>\n"); 
    if (DiaNv_SendCommandAndReadAnswerCode(driver, requestKeyExchange) != nv_statusCodes::OK)
        return 30;

    uint8_t slaveKeyBytes[8];
    for (int i = 4; i <= 11; i++) // get slave key from buf
        slaveKeyBytes[i - 4] = driver -> _Device ->_Buf[i];

    unsigned long long encryptionKey = 0; 
    unsigned long long multiplyer = 1; //to avoid pow
    for (int i = 0; i < 8; i++)
    {
        encryptionKey += slaveKeyBytes[i] * multiplyer;
        multiplyer *= 256;
    }
    encryptionKey = encryptionKey % modulus; // calculate real key

    unsigned long long tmpKey = encryptionKey;
    for (int i = 7; i >= 0; i--)
        if (tmpKey > 0)
        {
            driver -> EncryptionKeyBytes[i] = tmpKey % 256;
            tmpKey = tmpKey / 256;
        }
        else
            break;       
    
    uint8_t tmp[16]; // reverse key
    for (int i = 0; i < 16; i++) 
        tmp[15 + 0 - i] = driver -> EncryptionKeyBytes[i];
    for (int i = 0; i < 16; i++) 
       driver -> EncryptionKeyBytes[i] = tmp[i];
    

    driver -> EncryptPackets = true;
    driver -> eCount = 0;

    return DIA_NV_NO_ERROR;
}

int DiaNv_TurnOn(DiaNv * driver)
{
    printf("sync>>\n");
    if (DiaNv_SendCommandAndReadAnswerCode(driver, nv_sync) != nv_statusCodes::OK) return 10;
    driver->SequenceBit = 0;

    printf("setProtocolVersion>>\n"); 
    if (DiaNv_SendCommandAndReadAnswerCode(driver, nv_hostProtocolVersion) != nv_statusCodes::OK) return 20;

    if (DiaNv_SetupEncryption(driver) != DIA_NV_NO_ERROR) return 30;
   
    if (DiaNv_GetChannelsValues(driver) != DIA_NV_NO_ERROR) return 40;

    printf("enable>>\n");
    if (DiaNv_SendCommandAndReadAnswerCode(driver, nv_enable) != nv_statusCodes::OK) return 50;

    /*if (driver->IsNoteFloatAttached) //nv11 specific
    {
        printf("enable payout device>>\n"); 
        if (DiaNv_SendCommandAndReadAnswerCode(driver, nv11_enablePayoutDevice) != nv_statusCodes::OK) return 60;
    }*/

    printf("set inhibits>>\n"); 
    if (DiaNv_SetInhibitsOff(driver) != nv_statusCodes::OK) return 70;

    printf("display on>>\n");
    if (DiaNv_SendCommandAndReadAnswerCode(driver, nv_displayOn) != nv_statusCodes::OK) return 80;

    driver->CurrentMode = DIA_NV_DRIVER_OPERATIONAL_STATE;

    return DIA_NV_NO_ERROR;
}

void DiaNv_TurnOff(DiaNv * driver)
{
    DiaNv_SetInhibitsOn(driver);
    printf("disable>>\n"); 
    if (DiaNv_SendCommandAndReadAnswerCode(driver, nv_disable) != nv_statusCodes::OK) 
    {
        printf("Can't disable device. Stopping NV driver\n");
        DiaNv_StopDriver(driver);
    }
}

uint8_t DiaNv_SendCommandAndReadAnswerCode(DiaNv * driver, uint8_t* command, bool isDebug) 
{
    assert(command);
    int bytes_read, attempts = 1, commandSize = command[2] + 5;
    driver -> SendCommand(driver, command, commandSize);
    if (isDebug)
    {
        printf("Sent Bytes: ");
        for (int ev = 0; ev < commandSize; ev++)
            printf("%d ", command[ev]);
        printf("\n");
    }
    usleep(100000);
    bytes_read = DiaDevice_ReadPortBytes(driver -> _Device);
    while (bytes_read < 1)
    {
        if (attempts % 10 == 0) // 1 second passed
        {
            if (attempts >= 100)
            {
                printf("Device doesn't respond for 10 seconds, stopping driver\n");
                DiaNv_StopDriver(driver);
                break;
            }    
            else
            {
                if (isDebug) printf("Repeating NV command [%d]\n", attempts + 1);
                driver -> SequenceBit = !driver -> SequenceBit;
                driver -> SendCommand(driver, command, commandSize);
            }
        }
        usleep(100000);
        bytes_read = DiaDevice_ReadPortBytes(driver -> _Device);
        attempts += 1; 
    }
    if (bytes_read < 4) 
        return -1;
    DiaNv_UndoByteStuffing(driver ->_Device ->_Buf, bytes_read);
    
    if (driver -> EncryptPackets)
    {
        AES aes = AES();
        uint8_t encSize = driver ->_Device ->_Buf[2] - 1;
        unsigned char encData[encSize];
        for (int i = 0; i < encSize; i++)
            encData[i] = driver ->_Device ->_Buf[4 + i];
        unsigned char* unEncData = aes.DecryptECB(encData, encSize * sizeof(unsigned char), driver -> EncryptionKeyBytes);
        if (isDebug)
        {
            printf("Got Bytes (unencrypted): ");
            for (int ev = 0; ev < 5 + unEncData[0]; ev++)
                printf("%d ", unEncData[ev]);
            printf("\n");
            printf("Got Bytes (raw): ");
            for (int ev = 0; ev < bytes_read; ev++)
                printf("%d ", uint8_t(driver ->_Device ->_Buf[ev]));
            printf("\n");
        }
        for (int i = 5; i < 5 + unEncData[0]; i++)
            driver ->_Device ->_Buf[i - 5 + 3] = unEncData[i]; // mimic normal packet
        driver ->_Device ->_Buf[2] = unEncData[0];
        delete []unEncData;
        driver -> eCount += 1;
    }
    else if (isDebug)
    {
        printf("Got Bytes: ");
        for (int ev = 0; ev < bytes_read; ev++)
            printf("%d ", uint8_t(driver ->_Device ->_Buf[ev]));
        printf("\n");
    }

    return driver ->_Device ->_Buf[3];
}

int DiaNv_EncryptCommand(DiaNv * driver, uint8_t* command, int size, uint8_t *result)
{
    result[0] = 0x7F; //STX
    result[1] = command[1];

    unsigned char EncryptedData[255];
    EncryptedData[0] = command[2]; //length    
    unsigned int EncrypteddataSize = 1;
    unsigned int tmpECount = driver -> eCount; //count
    for (int i = 1; i < 5; i++)
        if (tmpECount > 0)
        {
            EncryptedData[i] = tmpECount % 256;
            tmpECount = tmpECount / 256;
        }
        else
            EncryptedData[i] = 0;
    EncrypteddataSize += 4;
    
    for (int i = 0; i < EncryptedData[0]; i++) //data
        EncryptedData[EncrypteddataSize + i] = command[3 + i];
    EncrypteddataSize += command[2];

    int packageSize = ((EncrypteddataSize + 2) / 16 + 1) * 16 - (EncrypteddataSize + 2); //package
    for (int i = 0; i < packageSize; i++)
        EncryptedData[EncrypteddataSize + i] = i;
    EncrypteddataSize += packageSize;

    EncrypteddataSize += 2;
    DiaNv_InsertCrc16(EncryptedData, EncrypteddataSize, 0); //insert crc
    AES aes = AES();    

    unsigned int encryptedSize = 0; // encrypt data
    unsigned char* enc = aes.EncryptECB(EncryptedData, EncrypteddataSize * sizeof(unsigned char), driver -> EncryptionKeyBytes, encryptedSize);
    
    result[2] = encryptedSize + 1;
    result[3] = 0x7E; //STEX
    for (unsigned int i = 0; i < encryptedSize; i++)
    {
        result[4 + i] = enc[i];
    }
    int resultSize = 4 + encryptedSize + 2;
    DiaNv_InsertCrc16(result, resultSize, 1);
    delete[]enc;
    return resultSize;
}

int DiaNv::SendCommand(DiaNv * driver, uint8_t *command, int size) 
{
    uint8_t result[255];
    for (int i = 0; i < size; i++)
        result[i] = command[i];

    if (!SequenceBit)
    {
        result[1] = 0;
        SequenceBit = true;
    }
    else
    {
        result[1] = 128;
        SequenceBit = false;
    }
    DiaNv_InsertCrc16(result, size, 1);
    if (driver -> EncryptPackets)
    {
        uint8_t encResult[255];
        int encSize = DiaNv_EncryptCommand(driver, result, size, encResult);

        DiaNv_ByteStuffing(encResult, encSize);
        return DiaDevice_WritePort(_Device, (const char *)encResult, encSize);
    }
    DiaNv_ByteStuffing(result, size);
    return DiaDevice_WritePort(_Device, (const char *)result, size);
}
