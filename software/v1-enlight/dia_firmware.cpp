#include <stdio.h>
#include <wiringPi.h>

#include "dia_gpio.h"
#include "dia_screen.h"
#include "dia_devicemanager.h"
#include "dia_network.h"
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/file.h>
#include "dia_security.h"
#include "dia_functions.h"
#include "dia_configuration.h"
#include "dia_runtime.h"
#include "dia_runtime_registry.h"
#include <string>
#include <map>
#include <unistd.h>
#include "dia_screen_item.h"
#include "dia_screen_item_image.h"

#include "dia_startscreen.h"


#define DIA_VERSION "v1.8-enlight"


//#define USE_GPIO
#define USE_KEYBOARD


// TODO: must be set via API
#define COIN_MULTIPLICATOR 1
#define BANKNOTE_MULTIPLICATOR 10
#define ALLOW_PULSE 1
// END must be set via API

#define BILLION 1000000000
#define MAX_ACCEPTABLE_FRAME_DRAW_TIME_MICROSEC 1000000

DiaConfiguration * config;

int _IntervalsCount;

// Public key for signing every request to Central Server.
const int centralKeySize = 6;
std::string centralKey;

int _DebugKey = 0;

// Variable for storing an additional money.
// For instance, service money from Central Server can be transfered inside.
int _Balance = 0;
int _OpenLid = 0;
int _BalanceCoins = 0;
int _BalanceBanknotes = 0;

int _to_be_destroyed = 0;
int _start_listening_key_press = 0;

int _CurrentBalance = 0;
int _CurrentProgram = -1;
int _CurrentProgramID = 0;
int _OldProgram = -1;
int _IsPreflight = 0;
int _IsServerRelayBoard = 0;
int _IntervalsCountProgram = 0;
int _IntervalsCountPreflight = 0;

int _Volume = 0;
int _SensorVolume = 0;
bool _SensorActive = false;
bool _SensorActiveUI = false;
bool _SensorActivate = false;

pthread_t run_program_thread;
pthread_t get_volume_thread;
pthread_t key_press_thread;

int GetKey(DiaGpio * _gpio) {
    int key = 0;

#ifdef USE_GPIO
    key = DiaGpio_GetLastKey(_gpio);
#endif

#ifdef USE_KEYBOARD
    if (_DebugKey != 0) {
        key = _DebugKey;
    }
    _DebugKey = 0;
#endif

    if (key) {
        printf("Key %d reported\n", key);
    }
    return key;
}

// Main object for Client-Server communication.
DiaNetwork * network = new DiaNetwork();

int set_current_state(int balance) {
    _CurrentBalance = balance;
    return 0;
}

// Saves new income money and creates money report to Central Server.
void SaveIncome(int cars_total, int coins_total, int banknotes_total, int cashless_total, int service_total) {
        network->SendMoneyReport(cars_total,
        coins_total,
        banknotes_total,
        cashless_total,
        service_total);
}

////// Runtime functions ///////
int get_key(void *object) {
    return GetKey((DiaGpio *)object);
}

int turn_light(void *object, int pin, int animation_id) {
    #ifdef USE_GPIO
    DiaGpio * gpio = (DiaGpio *)object;
    gpio->AnimationSubCode = pin;
    gpio->AnimationCode = animation_id;
    #endif
    return 0;
}

pthread_t pinging_thread;

// Creates receipt request to Online Cash Register.
int send_receipt(int postPosition, int cash, int electronical) {
    return network->ReceiptRequest(postPosition, cash, electronical);
}

// Increases car counter in config
int increment_cars() {
    printf("Cars incremented\n");
    SaveIncome(1,0,0,0,0);
    return 0;
}

int turn_program(void *object, int program) {
    if (program != _CurrentProgram) {
        printf("TURN PROGRAM %d intervals count preflight %d\n", _CurrentProgramID, _IntervalsCountPreflight);
        _IntervalsCountProgram = 0;
        _CurrentProgram = program;
        _CurrentProgramID = 0;
        _IntervalsCountPreflight = 0;
        if ((config) && (program>0)){
            _CurrentProgramID = config->GetProgramID(program);
            _IntervalsCountPreflight = config->GetPreflightSec(program)*10;
            printf("TURN PROGRAM %d intervals count preflight %d\n", _CurrentProgramID, _IntervalsCountPreflight);
        }
    }
    _IsPreflight = (_IntervalsCountPreflight>0);
    return 0;
}

int get_volume() {
    return _Volume;
}

bool get_sensor_active() {
    return _SensorActiveUI;
}

int start_fluid_flow_sensor(int volume){
    _SensorVolume = volume;
    _Volume = 0;
    _SensorActivate = true;
    _SensorActiveUI = true;
    return 0;
}

int get_service() {
    int curMoney = _Balance;
    _Balance = 0;
        
    if (curMoney > 0) {
        printf("service %d\n", curMoney);
        SaveIncome(0,0,0,0,curMoney);
    }
    return curMoney;
}

int get_is_preflight() {
    return _IsPreflight;
}

int get_openlid() {
    int curOpenLid = _OpenLid;
    _OpenLid = 0;
   
    return curOpenLid;
}

int get_price(int button) {
    if (config) {
        return config->GetPrice(button);
    }
    return 0;
}
int get_discount(int button){
    if (config){
        return config->GetDiscount(button);
    }
    return 0;
}

int get_is_finishing_program(int button){
    if (config){
        return config->GetIsFinishingProgram(button);
    }
    return 0;
}

int get_coins(void *object) {
  DiaDeviceManager * manager = (DiaDeviceManager *)object;
  int curMoney = manager->CoinMoney;
  manager->CoinMoney  = 0;  

  if (_BalanceCoins>0) {
        curMoney += _BalanceCoins;
        _BalanceCoins = 0;
  }

  int gpioCoin = 0;

  if (ALLOW_PULSE && config) {
    DiaGpio *g = config->GetGpio();
    if (g) {
      gpioCoin = COIN_MULTIPLICATOR * g->CoinsHandler->Money;
      g->CoinsHandler->Money = 0;
    }
  }

  int gpioCoinAdditional = 0;

  if (ALLOW_PULSE && config) {
    DiaGpio *g = config->GetGpio();
    if (g && g->AdditionalHandler) {
      gpioCoinAdditional = COIN_MULTIPLICATOR * g->AdditionalHandler->Money;
      g->AdditionalHandler->Money = 0;
    }
  }

  if (curMoney > 0) printf("coins from manager %d\n", curMoney);
  if (gpioCoin > 0) printf("coins from gpio %d\n", gpioCoin);
  if (gpioCoinAdditional > 0) printf("coins from additional gpio %d\n", gpioCoinAdditional);

  int totalMoney = curMoney + gpioCoin + gpioCoinAdditional;
  if (totalMoney>0) {
      SaveIncome(0,totalMoney,0,0,0);
  }

  return totalMoney;
}

int get_banknotes(void *object) {
  DiaDeviceManager * manager = (DiaDeviceManager *)object;
  int curMoney = manager->BanknoteMoney;
  manager->BanknoteMoney = 0;

  if (_BalanceBanknotes>0) {
        curMoney += _BalanceBanknotes;
        _BalanceBanknotes = 0;
  }

  int gpioBanknote = 0;
  if (ALLOW_PULSE && config) {
    DiaGpio *g = config->GetGpio();
    if (g) {
      gpioBanknote = BANKNOTE_MULTIPLICATOR * g->BanknotesHandler->Money;
      g->BanknotesHandler->Money = 0;
    }
  }

  if (curMoney > 0) printf("banknotes from manager %d\n", curMoney);
  if (gpioBanknote > 0) printf("banknotes from GPIO %d\n", gpioBanknote);
  int totalMoney = curMoney + gpioBanknote;
  if (totalMoney > 0) {
    SaveIncome(0,0,totalMoney,0,0);
  }
  return totalMoney;
}

int get_electronical(void *object) {
    DiaDeviceManager * manager = (DiaDeviceManager *)object;
    int curMoney = manager->ElectronMoney;
    if (curMoney>0) {
        printf("electron %d\n", curMoney);
        SaveIncome(0,0,0,curMoney,0);
        manager->ElectronMoney  = 0;
    }
    return curMoney;
}

// Tries to perform a bank card NFC transaction.
// Gets money amount.
int request_transaction(void *object, int money) {
    DiaDeviceManager * manager = (DiaDeviceManager *)object;
    if (money > 0) {
        DiaDeviceManager_PerformTransaction(manager, money);
        return 0;
    }
    return 1;
}

// Returns a status of NFC transaction. 
// Returned value == amount of money, which are expected by the reader.
// For example, 0 - reader is offline; 100 - reader expects 100 RUB.
int get_transaction_status(void *object) {
    DiaDeviceManager * manager = (DiaDeviceManager *)object;
    int status = DiaDeviceManager_GetTransactionStatus(manager);
    fprintf(stderr, "Transaction status: %d\n", status);
    return status;
}

// Deletes actual NFC transaction. 
int abort_transaction(void *object) {
    DiaDeviceManager * manager = (DiaDeviceManager *)object;
    DiaDeviceManager_AbortTransaction(manager);
    return 0;
}
inline int64_t micro_seconds_since(struct timespec * stored_time) {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC_RAW, &current_time);
    int64_t delta_time_passed_since_last_smart_delay_us = (current_time.tv_sec - stored_time->tv_sec) * 1000000 + (current_time.tv_nsec - stored_time->tv_nsec) / 1000;
    // printf("passed %d\n", (int)delta_time_passed_since_last_smart_delay_us);
    if (delta_time_passed_since_last_smart_delay_us > MAX_ACCEPTABLE_FRAME_DRAW_TIME_MICROSEC) {
        return MAX_ACCEPTABLE_FRAME_DRAW_TIME_MICROSEC;
    }
    if (delta_time_passed_since_last_smart_delay_us>0) {
        return delta_time_passed_since_last_smart_delay_us;
    }
    
    return 0;
}
inline void set_current_time(struct timespec * stored_time) {
    clock_gettime(CLOCK_MONOTONIC_RAW, stored_time);
}

int smart_delay_function(void * arg, int ms) {
    struct timespec *stored_time = (struct timespec *) arg;

    int64_t delay_wanted = 1000 * ms;
    // us (usecond, microsecond) = 10^-6 seconds
    // tv_sec (second) is one million microseconds
    // tv_nsec (nanosecond) contains 1000 microseconds or 10^9 seconds
    int64_t micro_secs_passed = micro_seconds_since(stored_time);
   if  (delay_wanted<MAX_ACCEPTABLE_FRAME_DRAW_TIME_MICROSEC) {
       if (micro_secs_passed<delay_wanted) {
            while ((delay_wanted>micro_seconds_since(stored_time)) && (_DebugKey==0)) {
                usleep(1000);
            }
        }
   } else {
        delay_wanted -= micro_secs_passed;
        if (delay_wanted>0) {
            usleep(delay_wanted);
        }
   }
    
    micro_secs_passed = micro_seconds_since(stored_time);
    if (micro_secs_passed < MAX_ACCEPTABLE_FRAME_DRAW_TIME_MICROSEC) {
        // everything is fine, we slept less than acceptable framerate
        stored_time->tv_nsec = stored_time->tv_nsec + micro_secs_passed * 1000;
        while (stored_time->tv_nsec >= BILLION) {
            stored_time->tv_nsec = stored_time->tv_nsec - BILLION;
            stored_time->tv_sec +=1;
        }
        return (int)(micro_secs_passed / 1000);
    }

    // if we are here we failed :( we slept more than acceptable framerate
    set_current_time(stored_time);
    return MAX_ACCEPTABLE_FRAME_DRAW_TIME_MICROSEC / 1000;
}
/////// End of Runtime functions ///////

int RunProgram() {
    if ((_IsServerRelayBoard) && (_IsPreflight == 0)) {
        _IntervalsCountProgram++;
    }
    if(_IntervalsCountProgram < 0) {
        printf("Memory corruption on _IntervalsCountProgram\n");
        _IntervalsCountProgram = 0;
    }
    if (_IntervalsCountPreflight>0) {
        _IntervalsCountPreflight --;
        
    }
    if (_CurrentProgram != _OldProgram) {
        if (_IsPreflight) {
            if (1 || _IsServerRelayBoard) {
                int count = 0;
                int err = 1;
                while ((err) && (count<4))
                {
                    count++;
                    printf("relay control server board: run program preflight programID=%d\n",_CurrentProgramID);
                    err = network->RunProgramOnServer(_CurrentProgramID, _IsPreflight);
                    if (err != 0) {
                        fprintf(stderr,"relay control server board: run program error\n");
                        delay(500);
                    }
                }
            } 
        }
        _OldProgram = _CurrentProgram;
    }
    if ((_IntervalsCountPreflight == 0) && (_IsPreflight)) {
        _IsPreflight = 0;
        if (_IsServerRelayBoard) {
            _IntervalsCountProgram = 1000;
        } 
    }
    // printf("current program %d, preflight %d, count %d\n", _CurrentProgram,_IsPreflight,_IntervalsCountPreflight);
    if (_IsServerRelayBoard == 0) {
    #ifdef USE_GPIO
    DiaGpio * gpio = config->GetGpio();
    if (_CurrentProgram >= MAX_PROGRAMS_COUNT) {
        return 1;
    }
    if(gpio!=0) {
        gpio->CurrentProgram = _CurrentProgram;
        gpio->CurrentProgramIsPreflight = _IsPreflight;
    } else {
        printf("ERROR: trying to run program with null gpio object\n");
    }
    #endif
    }
    
    if(_IntervalsCountProgram > 20) {
        int count = 0;
        int err = 1;
        while ((err) && (count<4) && (_CurrentProgramID>=0))
        {
            count++;
            printf("relay control server board: run program programID=%d\n",_CurrentProgramID);
            err = network->RunProgramOnServer(_CurrentProgramID, _IsPreflight);
            if (err != 0) {
                fprintf(stderr,"relay control server board: run program error\n");
                delay(500);
            }
            if ((err == 0) && (_CurrentProgramID==0)) {
                _CurrentProgramID = -1;
            }
        }
        _IntervalsCountProgram = 0;
    } 
    return 0;
}

int GetVolume() {
    if (_SensorActivate){
        _SensorActivate = false;
        for (int i = 0; i < 4; i++){
            int err = network->StartFluidFlowSensor(_SensorVolume);
            if (err == 0){
                _SensorActive = true;
                break;
            }
            if(i == 3){
                _SensorActiveUI = false;
            }
        }
    }
    if (_SensorActive){
        std::string status;
        for (int i = 0; i < 4; i++){
            int v = network->GetVolume(&status);
            if (v >= 0){
                _Volume = v;
                break;
            }
            if (i == 3){
                status = "Server connection error";
            }
        }
        if (_SensorVolume <= _Volume || status != ""){
            _SensorActive = false;
            _SensorActiveUI = false;
            printf("Completion of fluid flow. Status: %s\n", status.c_str());
        }
    }
    return 0;
}

/////// Central server communication functions //////

// Sends PING request to Central Server every 2 seconds.
// May get service money from server.
int CentralServerDialog() {
    printf("PING CENTRAL SERVER\n");
    
    _IntervalsCount++;
    if(_IntervalsCount < 0) {
        printf("Memory corruption on _IntervalsCount\n");
        _IntervalsCount = 0;
    }

    printf("Sending another PING request to server...\n");

    int serviceMoney = 0;
    bool openStation = false;
    int buttonID = 0;
    int lastUpdate = 0;
    int discountLastUpdate = 0;
    network->SendPingRequest(serviceMoney, openStation, buttonID, _CurrentBalance, _CurrentProgramID, lastUpdate, discountLastUpdate);
    if (config) {
        if (lastUpdate != config->GetLastUpdate() &&  config->GetLastUpdate() != -1){
            config->LoadConfig();
        }
        if (discountLastUpdate != config->GetDiscountLastUpdate()){
            config->LoadDiscounts();
            config->SetDiscountLastUpdate(discountLastUpdate);
        }
    }
    if (serviceMoney > 0) {
        // TODO protect with mutex
        _Balance += serviceMoney;
    }
    if (openStation) {
        _OpenLid = _OpenLid + 1;
        printf("Door is going to be opened... \n");
        // TODO: add the function of turning on the relay, which will open the lock.
    }
    if (buttonID != 0) {
        printf("BUTTON PRESSED %d \n", buttonID);
    }
    #ifdef USE_GPIO
    if (config) {
        DiaGpio * gpio_b = config->GetGpio();
        if (gpio_b!=0) {
            gpio_b->LastPressedKey = buttonID;
        } else {
            printf("ERROR: gpio_b used with no init. \n");
        }
    }
    #endif
        
    #ifdef USE_KEYBOARD
        _DebugKey = buttonID;
    #endif

    if (config) {
        // Every 30 min (1800 sec) we go inside this
        static const int maxIntervalsCountWeather = 1800;
        static int intervalsCountWeather = maxIntervalsCountWeather;
        if (intervalsCountWeather >= maxIntervalsCountWeather) {
            intervalsCountWeather = 0;
            config->GetSvcWeather()->SetCurrentTemperature();
        }
        ++intervalsCountWeather;
    }
    return 0;
}

void * pinging_func(void * ptr) {
    while(!_to_be_destroyed) {
        CentralServerDialog();
        sleep(1);
    }
    pthread_exit(0);
    return 0;
}

void * run_program_func(void * ptr) {
    while(!_to_be_destroyed) {
        RunProgram();
        delay(100);
    }
    pthread_exit(0);
    return 0;
}

void * get_volume_func(void * ptr) {
    while(!_to_be_destroyed) {
        GetVolume();
        delay(100);
    }
    pthread_exit(0);
    return 0;
}

void KeyPress(){
    SDL_Event event;
    while (SDL_PollEvent(&event)){
        if (event.type == SDL_QUIT || event.key.keysym.sym == SDLK_ESCAPE) {
            printf("SDL_QUIT\n");
            _to_be_destroyed = 1;
        }
    }
}

void * key_press_func(void * ptr) {
    while(!_to_be_destroyed && !_start_listening_key_press) {
        KeyPress();
        delay(100);
    }
    pthread_exit(0);
    return 0;
}

int RecoverRelay() {
    relay_report_t* last_relay_report = new relay_report_t;

    int err = network->GetLastRelayReport(last_relay_report);
    DiaGpio * gpio = config->GetGpio();
    if (err == 0 && gpio != 0) {
        if (gpio)
        for (int i = 0; i < MAX_RELAY_NUM; i++) {
            printf("Relay %d: switched - %d, total - %d\n", i, last_relay_report->RelayStats[i].switched_count,
            last_relay_report->RelayStats[i].total_time_on*1000);
        }

        bool update = false;
        for(int i = 0; i < MAX_RELAY_NUM; i++) {
            if ((gpio->Stat.relay_switch[i+1] < last_relay_report->RelayStats[i].switched_count) ||
                (gpio->Stat.relay_time[i+1] < last_relay_report->RelayStats[i].total_time_on*1000)) {
                    update = true;
            }
        }
        if (update) {
            fprintf(stderr,"Relays in config updated\n");
            for(int i = 0; i < MAX_RELAY_NUM; i++) {
                gpio->Stat.relay_switch[i+1] = last_relay_report->RelayStats[i].switched_count;
                gpio->Stat.relay_time[i+1] = last_relay_report->RelayStats[i].total_time_on*1000;
            }
        }
    } else {
            fprintf(stderr,"Get last relay report err:%d\n",err);
    }

    delete last_relay_report;
    return err;
}
//////// End of Central server communication functions /////////

//////// Local registry Save/Load functions /////////
std::string GetLocalData(std::string key) {
    std::string filename = "registry_" + key + ".reg";
    if (file_exists(filename.c_str())) {
        char value[6];
        dia_security_read_file(filename.c_str(), value, 5);

        return std::string(value, 6);
    }
    else {
        return "0";
    }
}

void SetLocalData(std::string key, std::string value) {
    std::string filename = "registry_" + key + ".reg";
    fprintf(stderr, "Key: %s, Value: %s \n", key.c_str(), value.c_str());
    dia_security_write_file(filename.c_str(), value.c_str());
}
/////////////////////////////////////////////////////

/////// Registry processing function ///////////////
int RecoverRegistry() {
    printf("Recovering registries...\n");

    DiaRuntimeRegistry* registry = config->GetRuntime()->Registry;

    std::string value = "";

    int tmp = 0;
    bool openStation = false;
    int buttonID = 0;
    
    int lastUpdate = 0;
    int discountLastUpdate = 0;

    std::string default_price = "15";
    int err = 1;
    while (err) {
        err = network->SendPingRequest(tmp, openStation, buttonID, _CurrentBalance, _CurrentProgram, lastUpdate, discountLastUpdate);
        if (err) {
            printf("waiting for server proper answer \n");
            sleep(5);
            continue;
        }
        // Load all prices in online mode
        fprintf(stderr, "Online mode, registry got from Central Server...\n");

        for (int i = 1; i < 7; i++) {
            std::string key = "price" + std::to_string(i);
            std::string value = network->GetRegistryValueByKey(key);

            if (value != "") {
                fprintf(stderr, "Key-value read online => %s:%s; \n", key.c_str(), value.c_str());
            } else {
		        fprintf(stderr, "Server returned empty value, setting default...\n");
		        value = default_price;
		        network->SetRegistryValueByKeyIfNotExists(key, value);
	        }
	        registry->SetValue(key.c_str(), value.c_str());
        }
    }
    return err;
}
//////////////////////////////////////////////

// Just compilation of recovers.
void RecoverData() {
    RecoverRegistry();
    RecoverRelay();
}

int onlyOneInstanceCheck() {
  int socket_desc;
  socket_desc=socket(AF_INET,SOCK_STREAM,0);
  if (socket_desc==-1) {
    perror("Create socket");
  }
  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  //Port defined Here:
  address.sin_port=htons(2223);
  //Bind
  int res = bind(socket_desc,(struct sockaddr *)&address,sizeof(address));
  if (res < 0) {
      printf("bind failed :(\n");
      return 0;
  }
  listen(socket_desc,32);
//Do other stuff (includes accepting connections)

  return 1;
}
int addCardReader(DiaDeviceManager *manager) {
    std::string cardReaderType;
    std::string host;
    std::string port;
    network->GetCardReaderConig(cardReaderType, host, port);
    
    if (cardReaderType == "PAYMENT_WORLD") {
        DiaDeviceManager_AddCardReader(manager);
        printf("card reader PAYMENT_WORLD\n");
        return CARD_READER_STATUS::PAYMENT_WORLD_SUCCCES;
    }
    if (cardReaderType == "VENDOTEK") {
        printf("card reader VENDOTEK\n");
        if ((host.empty()) || (port.empty())) {
            printf("host and port required. host='%s', port='%s'\n", host.c_str(), port.c_str());
            return CARD_READER_STATUS::VENDOTEK_NO_HOST;
        }

        StartScreenMessage(STARTUP_MESSAGE::VENDOTEK_INFO, "VENDOTEK: " + host + ":" + port);

        int errAddVendotek = DiaDeviceManager_AddVendotek(manager, host, port);
        if (errAddVendotek !=0) {
            if (errAddVendotek==4){
                return CARD_READER_STATUS::VENDOTEK_NULL_DRIVER;
            }
            return CARD_READER_STATUS::VENDOTEK_THREAD_ERROR;
        }
        bool found_card_reader = false;
        for (int i = 0; i < 10 && !found_card_reader; i++){
            sleep(1);
            found_card_reader = DiaDeviceManager_GetCardReaderStatus(manager) !=0;
            StartScreenMessage(STARTUP_MESSAGE::CARD_READER, "Try to find VENDOTEK (Attempt " + std::to_string(i+1) + " of 10)");
        }
        if(found_card_reader){
            return CARD_READER_STATUS::VENDOTEK_SUCCES;
        } else {
            return CARD_READER_STATUS::VENDOTEK_NOT_FOUND;
        }
        
    }
    printf("card reader not used\n");
    return CARD_READER_STATUS::NOT_USED;
}

int main(int argc, char ** argv) {
    config = 0;
    if (!onlyOneInstanceCheck()) {
        printf("sorry, just one instance of the application allowed\n");
        return 0;
    }

    // Timer initialization
    struct timespec stored_time;
    clock_gettime(CLOCK_MONOTONIC_RAW, &stored_time);

    if (argc > 2) {
        fprintf(stderr, "Too many parameters. Please leave just folder with the firmware, like [firmware.exe .] \n");
        return 1;
    }

    // Set working folder 
    std::string folder = "./firmware";
    if (argc == 2) {
        folder = argv[1];
    }
    
    if (StartScreenInit(folder)){
        fprintf(stderr, "SDL Initialization Failed\n");
        return 1;
    }
    StartScreenUpdate();

    pthread_create(&key_press_thread, NULL, key_press_func, NULL);

    printf("Looking for firmware in [%s]\n", folder.c_str());
    printf("Version: %s\n", DIA_VERSION);
 
    centralKey = network->GetMacAddress(centralKeySize);
    network->SetPublicKey(std::string(centralKey));
    
    StartScreenMessage(STARTUP_MESSAGE::MAC, "MAC: " + centralKey);

    printf("MAC address or KEY: %s\n", centralKey.c_str());
    
    int need_to_find = 1; 
    std::string serverIP = "";

    StartScreenMessage(STARTUP_MESSAGE::SERVER_IP, "Server IP: Searching...");
    while (need_to_find) {
        StartScreenUpdateIP();
    	serverIP = network->GetCentralServerAddress(_to_be_destroyed);
    	if (serverIP.empty()) {
        	printf("Error: Center Server is unavailable. Next try...\n");
            StartScreenMessage(STARTUP_MESSAGE::SERVER_IP, "Server IP: Searching...");
    	} else{
            StartScreenMessage(STARTUP_MESSAGE::SERVER_IP, "Server IP: "+ serverIP);
            need_to_find = 0;
        }

        if (_to_be_destroyed) {
            return 1;
        }    
    }

    network->SetHostAddress(serverIP);

    // Let's run a thread to ping server
    pthread_create(&pinging_thread, NULL, pinging_func, NULL);

    std::string stationIDasString;
    int stationID = 0;
    while (stationID == 0) {
        StartScreenMessage(STARTUP_MESSAGE::POST, "POST: check");
        stationIDasString = network->GetStationID();
        try { 
            stationID = std::stoi(stationIDasString);
        }
        catch(...) {
            printf("Wrong post number received [%s]\n", stationIDasString.c_str() );
        }
        if (stationID == 0) {
            sleep(1);
            StartScreenMessage(STARTUP_MESSAGE::POST, "POST is not assigned");
            printf("POST is not assigned\n");
            sleep(1);
        }

        if (_to_be_destroyed) {
            return 1;
        }
    }
    StartScreenMessage(STARTUP_MESSAGE::POST, "POST: " + std::to_string(stationID));

    printf("Card reader initialization...\n");
    StartScreenMessage(STARTUP_MESSAGE::CARD_READER, "Card Reader initialization...");
    // Runtime and firmware initialization
    DiaDeviceManager *manager = new DiaDeviceManager;
    bool findCardReader = true;
    while (findCardReader) {
        int errCardReader = addCardReader(manager);
        switch (errCardReader) {
        case CARD_READER_STATUS::PAYMENT_WORLD_SUCCCES:
            StartScreenMessage(STARTUP_MESSAGE::CARD_READER, "Card Reader PAYMENT WORLD");
            StartScreenMessage(STARTUP_MESSAGE::VENDOTEK_INFO, "");
            findCardReader = false;
            break;
        case CARD_READER_STATUS::VENDOTEK_SUCCES:
            StartScreenMessage(STARTUP_MESSAGE::CARD_READER, "Card Reader VENDOTEK");
            findCardReader = false;
            break;
        case CARD_READER_STATUS::NOT_USED:
            StartScreenMessage(STARTUP_MESSAGE::CARD_READER, "Card Reader not used");
            StartScreenMessage(STARTUP_MESSAGE::VENDOTEK_INFO, "");
            findCardReader = false;
            break;
        case CARD_READER_STATUS::VENDOTEK_NO_HOST:
            StartScreenMessage(STARTUP_MESSAGE::CARD_READER, "VENDOTEK ERROR: Host and port required");
            if(manager->_Vendotek) {
                delete manager->_Vendotek;
            }
            sleep(1);
            break;
        case CARD_READER_STATUS::VENDOTEK_NULL_DRIVER:
            StartScreenMessage(STARTUP_MESSAGE::CARD_READER, "VENDOTEK: NULL driver");
            if(manager->_Vendotek) {
                delete manager->_Vendotek;
            }
            sleep(1);
            break;
        case CARD_READER_STATUS::VENDOTEK_THREAD_ERROR:
            StartScreenMessage(STARTUP_MESSAGE::CARD_READER, "VENDOTEK ERROR: Failed to create thread");
            if(manager->_Vendotek) {
                delete manager->_Vendotek;
            }
            sleep(1);
            break;
        case CARD_READER_STATUS::VENDOTEK_NOT_FOUND:
            StartScreenMessage(STARTUP_MESSAGE::CARD_READER, "VENDOTEK ERROR: Not found card reader");
            if(manager->_Vendotek) {
                delete manager->_Vendotek;
            }
            sleep(1);
            break;
        default:
            break;
        }
    }

    printf("Config initialization...\n");
    StartScreenMessage(STARTUP_MESSAGE::CONFIGURATION, "Configuration initialization...");
    config = new DiaConfiguration(folder, network);
    int err = config->Init();
    switch (err){
    case CONFIGURATION_STATUS::ERROR_SCREEN:
        fprintf(stderr,"Configuration initialization: Failed to create screen\n");
        StartScreenMessage(STARTUP_MESSAGE::CONFIGURATION, "Failed to create screen");
        while (1) {
            sleep(1);
            if (_to_be_destroyed) {
                return 1;
            }
        }
    break;
    case CONFIGURATION_STATUS::ERROR_GPIO:
        fprintf(stderr,"Configuration initialization: Failed to init GPIO\n");
        StartScreenMessage(STARTUP_MESSAGE::CONFIGURATION, "Failed to init GPIO");
        while (1) {
            sleep(1);
            if (_to_be_destroyed) {
                return 1;
            }
        }
    break;
    case CONFIGURATION_STATUS::ERROR_JSON:
        fprintf(stderr,"Configuration initialization: Bad configuration file\n");
        StartScreenMessage(STARTUP_MESSAGE::CONFIGURATION, "Bad configuration file");
        while (1) {
            sleep(1);
            if (_to_be_destroyed) {
                return 1;
            }
        }
    break;
    case CONFIGURATION_STATUS::SUCCESS:
        StartScreenMessage(STARTUP_MESSAGE::CONFIGURATION, "Configuration initializated");
        break;    
    default:
        break;
    }
    
    printf("Loading server settings...\n");
    err = 1;
    StartScreenMessage(STARTUP_MESSAGE::SETTINGS, "Loading settings from server");
    while (err != 0) {
        err = config->LoadConfig();
        if(err) {
            fprintf(stderr,"Error loading settings from server\n");
            StartScreenMessage(STARTUP_MESSAGE::SETTINGS, "Error loading settings from server");
            sleep(1);
        }

        if (_to_be_destroyed) {
            return 1;
        }
    }

    printf("Loading discounts...\n");
    err = 1;
    StartScreenMessage(STARTUP_MESSAGE::SETTINGS, "Loading discounts campagins from server");
    while (err != 0) {
        err = config->LoadDiscounts();
        if (err) {
            fprintf(stderr, "Error loading discounts from server\n");
            StartScreenMessage(STARTUP_MESSAGE::SETTINGS, "Error loading discounts from server");
            sleep(1);
        }
        
        if (_to_be_destroyed) {
            return 1;
        }
    }

    printf("Settings loaded...\n");
    StartScreenMessage(STARTUP_MESSAGE::SETTINGS, "Settings from server loaded");
    _IsServerRelayBoard = config->GetServerRelayBoard();
    if (config->GetServerRelayBoard()) {
        int err =1;
        StartScreenMessage(STARTUP_MESSAGE::RELAY_CONTROL_BOARD, "Checking relay control server board");
        while (err) {
            printf("check relay control server board\n");
            err = network->RunProgramOnServer(0, 0);
            if (err != 0) {
                fprintf(stderr,"relay control server board not found\n");
                StartScreenMessage(STARTUP_MESSAGE::RELAY_CONTROL_BOARD, "Relay control server board not found");
            }
            sleep(1);

            if (_to_be_destroyed) {
                return 1;
            }
        }
        StartScreenMessage(STARTUP_MESSAGE::RELAY_CONTROL_BOARD, "Relay control server board found");
    }
    
    printf("Recovering the data from the server...\n");
    // Get working data from server: money, relays, prices
    RecoverData();
 
    printf("Configuration is loaded...\n");

    StartScreenShutdown();


    printf("Shut down of the start screen complete..., loading screens\n");
    // Screen load
    std::map<std::string, DiaScreenConfig *>::iterator it;
    for (it = config->ScreenConfigs.begin(); it != config->ScreenConfigs.end(); it++) {
        std::string currentID = it->second->id;
        DiaRuntimeScreen * screen = new DiaRuntimeScreen();
        screen->Name = currentID;
        screen->object = (void *)it->second;
        screen->set_value_function = dia_screen_config_set_value_function;
        screen->screen_object = config->GetScreen();
        screen->display_screen = dia_screen_display_screen;
        config->GetRuntime()->AddScreen(screen);
    }
    
    printf("animations init...\n");
    config->GetRuntime()->AddAnimations();
  
    printf("Card reader initialization...\n");
    DiaRuntimeHardware * hardware = new DiaRuntimeHardware();
    hardware->keys_object = config->GetGpio();
    hardware->get_keys_function = get_key;

    printf("HW init 1...\n");
    hardware->light_object = config->GetGpio();
    hardware->turn_light_function = turn_light;

    printf("HW init 2...\n");
    hardware->program_object = config->GetGpio();
    hardware->turn_program_function = turn_program;

    printf("HW init 3...\n");
    hardware->send_receipt_function = send_receipt;
    hardware->increment_cars_function = increment_cars;

    printf("HW init 4...\n");
    hardware->coin_object = manager;
    hardware->get_coins_function = get_coins;

    printf("HW init 5...\n");
    hardware->banknote_object = manager;
    hardware->get_banknotes_function = get_banknotes;

    printf("HW init 6...\n");
    hardware->electronical_object = manager;
    hardware->get_service_function = get_service;
    hardware->get_is_preflight_function = get_is_preflight;
    hardware->get_openlid_function = get_openlid;
    hardware->get_electronical_function = get_electronical;    
    hardware->request_transaction_function = request_transaction;  
    hardware->get_transaction_status_function = get_transaction_status;
    hardware->get_volume_function = get_volume;
    hardware->get_sensor_active_function = get_sensor_active;
    hardware->start_fluid_flow_sensor_function = start_fluid_flow_sensor;
    hardware->abort_transaction_function = abort_transaction;
    hardware->set_current_state_function = set_current_state;
    printf("HW init 7...\n");

    hardware->delay_object = &stored_time;
    hardware->smart_delay_function = smart_delay_function;
    printf("HW init 8...\n");

    hardware->has_card_reader = manager->_Vendotek || manager->_CardReader;

    printf("HW init 9...\n");
    config->GetRuntime()->AddHardware(hardware);
    printf("HW init 10...\n");
    config->GetRuntime()->AddRegistry(config->GetRuntime()->Registry);
    printf("HW init 11...\n");
    config->GetRuntime()->AddSvcWeather(config->GetSvcWeather());
    printf("HW init 12...\n");
    config->GetRuntime()->Registry->SetPostID(stationID);
    printf("HW init 13...\n");
    config->GetRuntime()->Registry->get_price_function = get_price;
    printf("HW init 14...\n");
    config->GetRuntime()->Registry->get_discount_function = get_discount;
    printf("HW init 15...\n");
    config->GetRuntime()->Registry->get_is_finishing_program_function = get_is_finishing_program;
    printf("HW init 16...\n");

    printf("Lua script starting...\n");
    // Call Lua setup function
    config->GetRuntime()->Setup();
    
    printf("Pulse init...\n");
    // using button as pulse is a crap obviously
    if (config->UseLastButtonAsPulse() && config->GetGpio()) {
        printf("enabling additional coin handler\n");
        int preferredButton = config->GetButtonsNumber()+1;
        DiaGpio_StartAdditionalHandler(config->GetGpio(), preferredButton);
    } else {
        printf("no additional coin handler\n");
    }

    printf("run_program_func start...\n");
    pthread_create(&run_program_thread, NULL, run_program_func, NULL);
    printf("get_volume_func start...\n");
    pthread_create(&get_volume_thread, NULL, get_volume_func, NULL);
    
    _start_listening_key_press = 1;
    int keypress = _to_be_destroyed;
    int mousepress = 0;
    SDL_Event event;

    while(!keypress) {
        // Call Lua loop function
        config->GetRuntime()->Loop();

        int x = 0;
        int y = 0;
        SDL_GetMouseState(&x, &y);
        if (config->NeedRotateTouch()) {
            x = config->GetResX() - x;
            y = config->GetResY() - y;
        }

        // Process pressed button
        DiaScreen* screen = config->GetScreen();
        std::string last = screen->LastDisplayed;

        for (auto it = config->ScreenConfigs[last]->clickAreas.begin(); it != config->ScreenConfigs[last]->clickAreas.end(); ++it) {
            if (x >= (*it).X && x <= (*it).X + (*it).Width && y >= (*it).Y && y <= (*it).Y + (*it).Height && mousepress == 1) {
                printf("CLICK!!!\n");
                mousepress = 0;
                _DebugKey = std::stoi((*it).ID);
                printf("DEBUG KEY = %d\n", _DebugKey);
            }
        }
        
        while(SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    keypress = 1;
                    printf("Quitting by sdl_quit\n");
                break;
                case SDL_MOUSEBUTTONDOWN:
                    mousepress = 1;
                break;
                case SDL_KEYDOWN:
                    switch(event.key.keysym.sym) {
                        case SDLK_UP:
                            // Debug service money addition
                            _BalanceBanknotes += 10;

                            printf("UP\n"); fflush(stdout);
                            break;
                        case SDLK_DOWN:
                            // Debug service money addition
                            _BalanceCoins += 1;

                            printf("UP\n"); fflush(stdout);
                            break;
                        
                        case SDLK_1:
                            _DebugKey = 1;
                            printf("1\n"); fflush(stdout);
                            break;

                        case SDLK_2:
                            _DebugKey = 2;
                            printf("2\n"); fflush(stdout);
                            break;

                        case SDLK_3:
                            _DebugKey = 3;
                            printf("3\n"); fflush(stdout);
                            break;

                        case SDLK_4:
                            _DebugKey = 4;
                            printf("4\n"); fflush(stdout);
                            break;

                        case SDLK_5:
                            _DebugKey = 5;
                            printf("5\n"); fflush(stdout);
                            break;

                        case SDLK_6:
                            _DebugKey = 6;
                            printf("6\n"); fflush(stdout);
                            break;

                        case SDLK_7:
                            _DebugKey = 7;
                            printf("7\n"); fflush(stdout);
                            break;

                        default:
                            keypress = 1;
                            printf("Quitting by keypress...");
                            break;
                    }
                break;
            }
        }
    }
    _to_be_destroyed = 1;

    delay(2000);
    return 0;
}

std::string SetRegistryValueByKeyIfNotExists(std::string key, std::string value) {
    return network->SetRegistryValueByKeyIfNotExists(key, value);
}
