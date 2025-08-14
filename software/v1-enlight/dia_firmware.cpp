#ifndef __has_include
  static_assert(false, "__has_include not supported");
#else
#  if __cplusplus >= 201703L && __has_include(<filesystem>)
#    include <filesystem>
     namespace fs = std::filesystem;
#  elif __has_include(<experimental/filesystem>)
#    include <experimental/filesystem>
     namespace fs = std::experimental::filesystem;
#  elif __has_include(<boost/filesystem.hpp>)
#    include <boost/filesystem.hpp>
     namespace fs = boost::filesystem;
#  endif
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <wiringPi.h>
#include <unistd.h>

#include <iostream>
#include <list>
#include <map>
#include <string>

#include "dia_configuration.h"
#include "dia_devicemanager.h"
#include "dia_functions.h"
#include "dia_gpio.h"
#include "dia_firmware.h"
#include "dia_runtime.h"
#include "dia_runtime_registry.h"
#include "dia_screen.h"
#include "dia_screen_item.h"
#include "dia_screen_item_image.h"
#include "dia_screen_item_qr.h"
#include "dia_security.h"
#include "dia_startscreen.h"
#include "dia_log.h"

#define DIA_VERSION "v1.8-enlight"

#define USE_KEYBOARD

// TODO: must be set via API
#define COIN_MULTIPLICATOR 1
#define BANKNOTE_MULTIPLICATOR 10
#define ALLOW_PULSE 1
// END must be set via API

#define BILLION 1000000000
#define MAX_ACCEPTABLE_FRAME_DRAW_TIME_MICROSEC 1000000

DiaConfiguration *config;

SDL_Event _event;

int _IntervalsCount;

// Public key for signing every request to Central Server.
const int centralKeySize = 6;
std::string centralKey;

int _DebugKey = 0;

bool _JustTurnedOn = true;

// Variable for storing an additional money.
// For instance, service money from Central Server can be transfered inside.
int _Balance = 0;
int _OpenLid = 0;
int _BalanceBonuses = 0;
int _BalanceSbp = 0;
std::string _SbpOrderId = "";
int _BalanceCoins = 0;
int _BalanceBanknotes = 0;

int _to_be_destroyed = 0;

int _CurrentBalance = 0;
int _CurrentProgram = -1;
int _CurrentProgramID = 0;
int _OldProgram = -1;
int _IsPreflight = 0;
RelayBoardMode _ServerRelayBoardMode = RelayBoardMode::LocalGPIO;
int _IntervalsCountProgram = 0;
int _IntervalsCountPreflight = 0;

int _WaitSecondsForNextSession = 180;
int _Volume = 0;
int _SensorVolume = 0;
bool _SensorActive = false;
bool _SensorActiveUI = false;
bool _SensorActivate = false;

volatile bool _CanPlayVideo = false;
volatile bool _IsPlayingVideo = false;
volatile int _ProcessId = 0;
int _CanPlayVideoTimer = 0;

bool _BonusSystemIsActive = false;
bool _SbpSystemActive = false;
bool _IsConnectedToBonusSystem = false;
std::string _AuthorizedSessionID = "";
std::string _ServerUrl = "";
bool _BonusSystemClient = false;
int _BonusSystemBalance = 0;

bool _IsSbpPaymentOnTerminalAvailable = false;

int _MaxAfkTime = 180;

std::string _FileName;

std::string _Qr = "";
std::string _VisibleSessionID = "";

std::string _SbpQr = "";

pthread_t run_program_thread;
pthread_t get_volume_thread;
pthread_t active_session_thread;
pthread_t play_video_thread;

int GetKey(DiaGpio *_gpio) {
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
DiaNetwork *network = new DiaNetwork();

int set_current_state(int balance) {
    _CurrentBalance = balance;
    return 0;
}

void setCanPlayVideo(bool canPlayVideo) {
    _CanPlayVideo = canPlayVideo;
}

bool getCanPlayVideo() {
    return _CanPlayVideo;
}

int getProcessId() {
    return _ProcessId;
}

void setIsPlayingVideo(bool isPlayingVideo) {
    _IsPlayingVideo = isPlayingVideo;
}

bool getIsPlayingVideo() {
    return _IsPlayingVideo;
}

void setIsConnectedToBonusSystem(bool isConnectedToBonusSystem) {
    _IsConnectedToBonusSystem = isConnectedToBonusSystem;
}

bool getIsConnectedToBonusSystem() {
    return _IsConnectedToBonusSystem;
}

bool getIsSbpPaymentOnTerminalAvailable() {
    return _IsSbpPaymentOnTerminalAvailable;
}

bool dirExists(const std::string& dirName_in)
{
    fs::path dirNamePath(dirName_in);

    if (fs::exists(dirNamePath) && fs::is_directory(dirNamePath)) {
        return true;
    }
    return false;
}

bool dirAccessRead(const std::string& dirName_in)
{
    if (access(dirName_in.c_str(), R_OK) != 0) {
        return false;
    }
    return true;
}

std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string toLowerCase(const std::string& input) {
    std::string result = input;
    for (char &c : result) {
        c = std::tolower(static_cast<unsigned char>(c));
    }
    return result;
}

bool check_directory() {
    std::list<std::string> directories;
    std::string directory;
    for (const auto &entry : fs::directory_iterator("/media")) {
        if(dirAccessRead(entry.path()) && fs::is_directory(entry.path())) {
            directories.push_back(entry.path());
        }
    }

    bool found = false;
    for (auto const &i : directories) {
        if(dirAccessRead(i)){
            for (const auto &subEntry : fs::recursive_directory_iterator(i)) {
                if (dirAccessRead(subEntry.path())){
                        if (fs::is_directory(subEntry) && subEntry.path().filename() == "openrbt_video" && dirAccessRead(subEntry.path().string())) {
                        directory = subEntry.path().string();
                        found = true;
                        break;
                    }
                }
            }
        }
        if (found) break;
    }

    if (!directory.empty()) {
        std::string extension;
        for (const auto &entry : fs::directory_iterator(directory)){
            extension = entry.path().extension();
            if(toLowerCase(extension) == ".mp4" || toLowerCase(extension) == ".avi"){
                _FileName += entry.path();
                _FileName += " ";
            }
            
        }
        return true;
        
    }
    return false;
}

void *play_video_func(void *ptr) {
    while (!_to_be_destroyed) {
        if(_CanPlayVideo) {
            if(check_directory() && !_FileName.empty()){
                std::vector<std::string> files = split(_FileName, ' ');

                std::string formattedFiles;
                for (const std::string &file : files) {
                    if (!formattedFiles.empty()) {
                        formattedFiles += " ";
                    }
                    formattedFiles += "\"" + file + "\"";
                }
                _IsPlayingVideo = true;
                int pid = system(("python ./video/player.py " + formattedFiles + " --repeat --mousebtn").c_str());
                _IsPlayingVideo = false;
                _ProcessId = pid;
                delay(100);
                
                printf("\nPlayVideo result: %d", pid);
            }
            else{
                delay(1000 * 30);
            }
            
        }
        delay(1000);
    }
    pthread_exit(0);
    return 0;
}


int CreateSession() {
    std::string QR;
    std::string sessionID;
    int answer = network->CreateSession(sessionID, QR);
    _VisibleSessionID = sessionID;
    _Qr = QR;
    return answer;
}

int EndSession() {
    if(_AuthorizedSessionID != ""){
        int answer = network->EndSession(_AuthorizedSessionID);
        return answer;
    }
    return 1;
}

int CloseVisibleSession() {
    if(_VisibleSessionID != ""){
        int answer = network->EndSession(_VisibleSessionID);
        return answer;
    }
    return 1;
}

std::string getQR() {
    return _Qr;
}

std::string getSbpQr() {
    return _SbpQr;
}

std::string getVisibleSession() {
    return _VisibleSessionID;
}

std::string getActiveSession() {
    if(_AuthorizedSessionID.empty())
        return _VisibleSessionID;
    return _AuthorizedSessionID;
}

// Saves new income money and creates money report to Central Server.
void SaveIncome(int cars_total, int coins_total, int banknotes_total, int cashless_total, int service_total, int bonuses_total, int sbp_total, std::string session_id) {
    network->SendMoneyReport(cars_total,
                             coins_total,
                             banknotes_total,
                             cashless_total,
                             service_total,
                             bonuses_total,
                             sbp_total,
                             session_id);
}

int SetBonuses(int bonuses) {
    return network->SetBonuses(bonuses);
}

int sendPause() {
    return network->sendPause(0);
}

////// Runtime functions ///////
int get_key(void *object) {
    return GetKey((DiaGpio *)object);
}

int turn_light(void *object, int pin, int animation_id) {
#ifdef USE_GPIO
    DiaGpio *gpio = (DiaGpio *)object;
    gpio->AnimationSubCode = pin;
    gpio->AnimationCode = animation_id;
#endif
    return 0;
}

pthread_t pinging_thread;

// Creates receipt request to Online Cash Register.
int send_receipt(int postPosition, int cash, int electronical, int qrMoney) {
    return network->ReceiptRequest(postPosition, cash, electronical + qrMoney);
}

int createSbpPayment(int amount) {
    return network->CreateSbpPayment(amount * 100);
}

// Increases car counter in config
int increment_cars() {
    printf("Cars incremented\n");
    SaveIncome(1, 0, 0, 0, 0, 0, 0, getActiveSession());
    return 0;
}

int turn_program(void *object, int program) {
    if (program != _CurrentProgram) {
        printf("TURN PROGRAM %d intervals count preflight %d\n", _CurrentProgramID, _IntervalsCountPreflight);
        _IntervalsCountProgram = 0;
        _CurrentProgram = program;
        _CurrentProgramID = 0;
        _IntervalsCountPreflight = 0;
        if ((config) && (program > 0)) {
            _CurrentProgramID = config->GetProgramID(program);
            _IntervalsCountPreflight = config->GetPreflightSec(program) * 10;
            printf("TURN PROGRAM %d intervals count preflight %d\n", _CurrentProgramID, _IntervalsCountPreflight);
        }
    }
    _IsPreflight = (_IntervalsCountPreflight > 0);
    return 0;
}

int get_volume() {
    return _Volume;
}

bool get_sensor_active() {
    return _SensorActiveUI;
}

int start_fluid_flow_sensor(int volume) {
    _SensorVolume = volume;
    _Volume = 0;
    _SensorActivate = true;
    _SensorActiveUI = true;
    return 0;
}

// Bonus system

bool bonus_system_is_active() {
    return _BonusSystemIsActive;
}

bool sbp_system_is_active() {
    return _SbpSystemActive;
}

std::string authorized_session_ID() {
    return _AuthorizedSessionID;
}

int bonus_system_refresh_active_qr() {
    return 0;
}

int bonus_system_start_session() {
    return 0;
}

int bonus_system_confirm_session() {
    return 0;
}

int bonus_system_finish_session() {
    return 0;
}

int get_service() {
    int curMoney = _Balance;
    _Balance = 0;

    if (curMoney > 0) {
        printf("service %d\n", curMoney);
        SaveIncome(0, 0, 0, 0, curMoney, 0, 0, getActiveSession());
    }
    return curMoney;
}

int get_bonuses() {
    int curMoney = _BalanceBonuses;
    _BalanceBonuses = 0;

    if (curMoney > 0) {
        printf("bonus %d\n", curMoney);
        SaveIncome(0, 0, 0, 0, 0, curMoney, 0, getActiveSession());
    }
    return curMoney;
}

int get_sbp_money() {
    int curMoney = _BalanceSbp;
    _BalanceSbp = 0;

    if (curMoney > 0) {
        printf("sbp money %d\n", curMoney);
        SaveIncome(0, 0, 0, 0, 0, 0, curMoney, getActiveSession());
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
int get_discount(int button) {
    if (config) {
        return config->GetDiscount(button);
    }
    return 0;
}

int get_is_finishing_program(int button) {
    if (config) {
        return config->GetIsFinishingProgram(button);
    }
    return 0;
}

int get_coins(void *object) {
    DiaDeviceManager *manager = (DiaDeviceManager *)object;
    int curMoney = manager->CoinMoney;
    manager->CoinMoney = 0;

    if (_BalanceCoins > 0) {
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
    if (totalMoney > 0) {
        SaveIncome(0, totalMoney, 0, 0, 0, 0, 0, getActiveSession());
    }

    return totalMoney;
}

int get_banknotes(void *object) {
    DiaDeviceManager *manager = (DiaDeviceManager *)object;
    int curMoney = manager->BanknoteMoney;
    manager->BanknoteMoney = 0;

    if (_BalanceBanknotes > 0) {
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
        SaveIncome(0, 0, totalMoney, 0, 0, 0, 0, getActiveSession());
    }
    return totalMoney;
}

int get_electronical(void *object) {
    DiaDeviceManager *manager = (DiaDeviceManager *)object;
    int curMoney = manager->ElectronMoney;
    if (curMoney > 0) {
        printf("electron %d\n", curMoney);
        SaveIncome(0, 0, 0, curMoney, 0, 0, 0, getActiveSession());
        manager->ElectronMoney = 0;
    }
    return curMoney;
}

int get_sbp_terminal_money(void *object) {
    DiaDeviceManager *manager = (DiaDeviceManager *)object;
    int curMoney = manager->SbpMoney;
    if (curMoney > 0) {
        printf("terminal sbp money %d\n", curMoney);
        SaveIncome(0, 0, 0, 0, 0, 0, curMoney, getActiveSession());
        manager->SbpMoney = 0;
    }
    return curMoney;
}

// Tries to perform a bank card NFC transaction.
// Gets money amount.
int request_transaction(void *object, int money, bool isTerminalSBP) {
    DiaDeviceManager *manager = (DiaDeviceManager *)object;
    if (money > 0) {
        DiaDeviceManager_PerformTransaction(manager, money, false, isTerminalSBP);
        return 0;
    }
    return 1;
}

int request_transaction_separated(void *object, int money){
    DiaDeviceManager *manager = (DiaDeviceManager *)object;
    if (money > 0) {
        DiaDeviceManager_PerformTransaction(manager, money, true, false);
        return 0;
    }
    return 1;
}

int confirm_transaction(void *object, int money){
    DiaDeviceManager *manager = (DiaDeviceManager *)object;
    //printf("Manager Electronical:    %d",manager->ElectronMoney);
    int bonuses = 0;
    bonuses = DiaDeviceManager_ConfirmTransaction(manager, money);

    if (bonuses > 0){
        SetBonuses(bonuses);
    }

    return 0;
}

// Returns a status of NFC transaction.
// Returned value == amount of money, which are expected by the reader.
// For example, 0 - reader is offline; 100 - reader expects 100 RUB.
int get_transaction_status(void *object) {
    DiaDeviceManager *manager = (DiaDeviceManager *)object;
    int status = DiaDeviceManager_GetTransactionStatus(manager);
    return status;
}

// Deletes actual NFC transaction.
int abort_transaction(void *object) {
    DiaDeviceManager *manager = (DiaDeviceManager *)object;
   
    DiaDeviceManager_AbortTransaction(manager);
    return 0;
}
inline int64_t micro_seconds_since(struct timespec *stored_time) {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC_RAW, &current_time);
    int64_t delta_time_passed_since_last_smart_delay_us = (current_time.tv_sec - stored_time->tv_sec) * 1000000 + (current_time.tv_nsec - stored_time->tv_nsec) / 1000;
    if (delta_time_passed_since_last_smart_delay_us > MAX_ACCEPTABLE_FRAME_DRAW_TIME_MICROSEC) {
        return MAX_ACCEPTABLE_FRAME_DRAW_TIME_MICROSEC;
    }
    if (delta_time_passed_since_last_smart_delay_us > 0) {
        return delta_time_passed_since_last_smart_delay_us;
    }

    return 0;
}
inline void set_current_time(struct timespec *stored_time) {
    clock_gettime(CLOCK_MONOTONIC_RAW, stored_time);
}

int smart_delay_function(void *arg, int ms) {
    struct timespec *stored_time = (struct timespec *)arg;

    int64_t delay_wanted = 1000 * ms;
    // us (usecond, microsecond) = 10^-6 seconds
    // tv_sec (second) is one million microseconds
    // tv_nsec (nanosecond) contains 1000 microseconds or 10^9 seconds
    int64_t micro_secs_passed = micro_seconds_since(stored_time);
    if (delay_wanted < MAX_ACCEPTABLE_FRAME_DRAW_TIME_MICROSEC) {
        if (micro_secs_passed < delay_wanted) {
            while ((delay_wanted > micro_seconds_since(stored_time)) && (_DebugKey == 0)) {
                usleep(1000);
            }
        }
    } else {
        delay_wanted -= micro_secs_passed;
        if (delay_wanted > 0) {
            usleep(delay_wanted);
        }
    }

    micro_secs_passed = micro_seconds_since(stored_time);
    if (micro_secs_passed < MAX_ACCEPTABLE_FRAME_DRAW_TIME_MICROSEC) {
        // everything is fine, we slept less than acceptable framerate
        stored_time->tv_nsec = stored_time->tv_nsec + micro_secs_passed * 1000;
        while (stored_time->tv_nsec >= BILLION) {
            stored_time->tv_nsec = stored_time->tv_nsec - BILLION;
            stored_time->tv_sec += 1;
        }
        return (int)(micro_secs_passed / 1000);
    }

    // if we are here we failed :( we slept more than acceptable framerate
    set_current_time(stored_time);
    return MAX_ACCEPTABLE_FRAME_DRAW_TIME_MICROSEC / 1000;
}
/////// End of Runtime functions ///////

bool IsRemoteOrAllRelayBoardMode() {
    return _ServerRelayBoardMode == RelayBoardMode::DanBoard || _ServerRelayBoardMode == RelayBoardMode::All;
}

bool IsRemoteRelayBoardMode() {
    return _ServerRelayBoardMode == RelayBoardMode::DanBoard;
}

bool IsLocalOrAllRelayBoardMode() {
    return _ServerRelayBoardMode == RelayBoardMode::LocalGPIO || _ServerRelayBoardMode == RelayBoardMode::All;
}

bool RunProgramOnServerWithRetry(int programID, bool isPreflight) {
    int count = 0;
    int err = 1;
    while (err && count < DIA_REQUEST_RETRY_ATTEMPTS) {
        count++;
        printf("relay control server board: run program%s programID=%d\n", isPreflight ? " preflight" : "", programID);
        err = network->RunProgramOnServer(programID, isPreflight);
        if (err != 0) {
            fprintf(stderr, "relay control server board: run program error\n");
            delay(100);
        }
    }
    return err == 0;
}

bool RunProgramOnServer(int programID, bool isPreflight) {
    int err = 1;
    printf("relay control server board: run program%s programID=%d\n", isPreflight ? " preflight" : "", programID);
    err = network->RunProgramOnServer(programID, isPreflight);
    return err == 0;
}

int RunProgram() {
    if (IsRemoteOrAllRelayBoardMode())
        _IntervalsCountProgram++;

    if (_IntervalsCountProgram < 0) {
        printf("Memory corruption on _IntervalsCountProgram\n");
        _IntervalsCountProgram = 0;
    }

    if (_CurrentProgram != _OldProgram) {
        _OldProgram = _CurrentProgram;
        if (IsRemoteOrAllRelayBoardMode())
            _IntervalsCountProgram = 1000;
    }

    if (_IntervalsCountPreflight > 0)
        _IntervalsCountPreflight--;

    if (_IntervalsCountPreflight == 0 && _IsPreflight) {
        _IsPreflight = 0;
        if (IsRemoteOrAllRelayBoardMode()) {
            _IntervalsCountProgram = 1000;
        }
    }

    if (IsLocalOrAllRelayBoardMode()) {
        #ifdef USE_GPIO
            DiaGpio *gpio = config->GetGpio();
            if (_CurrentProgram >= MAX_PROGRAMS_COUNT) {
                return 1;
            }
            if (gpio != nullptr) {
                gpio->CurrentProgram = _CurrentProgram;
                gpio->CurrentProgramIsPreflight = _IsPreflight;
            } else {
                printf("ERROR: trying to run program with null gpio object\n");
            }
        #endif
    }
   
    if (_IntervalsCountProgram > 20 && _CurrentProgramID >= 0) {
        bool success = RunProgramOnServer(_CurrentProgramID, _IsPreflight);
        if (success && _CurrentProgramID == 0)
            _CurrentProgramID = -1;
        _IntervalsCountProgram = 0;
        if (!success && IsRemoteOrAllRelayBoardMode())
            _IntervalsCountProgram = 1000;
    }
    
    return 0;
}

int GetVolume() {
    /**/
    if (_SensorActivate) {
        _SensorActivate = false;
        for (int i = 0; i < 4; i++) {
            int err = network->StartFluidFlowSensor(_SensorVolume, 1, 0);
            if (err == 0) {
                _SensorActive = true;
                break;
            }
            if (i == 3) {
                _SensorActiveUI = false;
            }
        }
    }
    if (_SensorActive) {
        std::string status;
        for (int i = 0; i < 4; i++) {
            int v = network->GetVolume(&status);
            if (v >= 0) {
                _Volume = v;
                break;
            }
            if (i == 3) {
                status = "Server connection error";
            }
        }
        if (_SensorVolume <= _Volume || status != "") {
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
    _IntervalsCount++;
    if (_IntervalsCount < 0) {
        printf("Memory corruption on _IntervalsCount\n");
        _IntervalsCount = 0;
    }

    DiaNetwork::PingResponse resp;
    network->SendPingRequest(_CurrentBalance, _CurrentProgramID, _JustTurnedOn, resp);

    network->GetServerInfo(_ServerUrl);
    if (config) {
        if (resp.lastUpdate != config->GetLastUpdate() && config->GetLastUpdate() != -1) {
            config->LoadConfig();
        }
        if (resp.discountLastUpdate != config->GetDiscountLastUpdate()) {
            config->LoadDiscounts();
            config->SetDiscountLastUpdate(resp.discountLastUpdate);
        }
    }
    if (resp.serviceMoney > 0) {
        // TODO protect with mutex
        _Balance += resp.serviceMoney;
    }
    if (resp.kaspiAmount > 0) {
        // TODO protect with mutex
        _BalanceSbp += resp.kaspiAmount;
    }

    if (resp.bonusAmount > 0) {
        // TODO protect with mutex
        _BalanceBonuses += resp.bonusAmount;
    }
    if (resp.sbpMoney > 0 && !resp.sbpQrFailed && !resp.sbpOrderId.empty() && _SbpOrderId != resp.sbpOrderId) {
        // TODO protect with mutex
        _SbpOrderId = resp.sbpOrderId;
        if (!network->ConfirmSbpPayment(resp.sbpOrderId)) {
            _BalanceSbp = resp.sbpMoney / 100;
        }
    }
    if (resp.openStation) {
        _OpenLid = _OpenLid + 1;
        printf("Door is going to be opened... \n");
        // TODO: add the function of turning on the relay, which will open the lock.
    }

    _SbpQr = resp.sbpUrl;

    if (resp.bonusSystemActive != _BonusSystemIsActive) {
        _BonusSystemIsActive = resp.bonusSystemActive;
        printf("Bonus system activated: %d\n", resp.bonusSystemActive);
    }
    if(resp.sbpSystemActive != _SbpSystemActive){
        _SbpSystemActive = resp.sbpSystemActive;
        printf("SBP system activated: %d\n", resp.sbpSystemActive);
    }
    if(_BonusSystemIsActive){
        if (_VisibleSessionID == resp.authorizedSessionID) {
            _IsConnectedToBonusSystem = !resp.authorizedSessionID.empty();
            CreateSession();
            resp.visibleSessionID = _VisibleSessionID;
        }
        if (_AuthorizedSessionID != resp.authorizedSessionID) {
            EndSession();
        }
        _VisibleSessionID = resp.visibleSessionID;
        _AuthorizedSessionID = resp.authorizedSessionID;
        _Qr = _VisibleSessionID.empty() ? "" : _ServerUrl + "/#/?sessionID=" + _VisibleSessionID;
    }
    if (resp.buttonID != 0) {
        printf("BUTTON PRESSED %d \n", resp.buttonID);
    }
#ifdef USE_GPIO
    if (config) {
        DiaGpio *gpio_b = config->GetGpio();
        if (gpio_b != 0) {
            gpio_b->LastPressedKey = resp.buttonID;
        } else {
            printf("ERROR: gpio_b used with no init. \n");
        }
    }
#endif

#ifdef USE_KEYBOARD
    _DebugKey = resp.buttonID;
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

void *pinging_func(void *ptr) {
    while (!_to_be_destroyed) {
        CentralServerDialog();
        sleep(1);
    }
    pthread_exit(0);
    return 0;
}

void *run_program_func(void *ptr) {
    while (!_to_be_destroyed) {
        RunProgram();
        delay(100);
    }
    pthread_exit(0);
    return 0;
}

void *get_volume_func(void *ptr) {
    while (!_to_be_destroyed) {
        GetVolume();
        delay(100);
    }
    pthread_exit(0);
    return 0;
}

void KeyPress(){
    while (SDL_PollEvent(&_event)){
        if (_event.type == SDL_QUIT || (_event.type == SDL_KEYDOWN && _event.key.keysym.sym == SDLK_ESCAPE)) {
            printf("SDL_QUIT\n");
            _to_be_destroyed = 1;
        }
    }
}

int RecoverRelay() {
    relay_report_t *last_relay_report = new relay_report_t;

    int err = network->GetLastRelayReport(last_relay_report);
    DiaGpio *gpio = config->GetGpio();
    if (err == 0 && gpio != 0) {
        if (gpio)
            for (int i = 0; i < MAX_RELAY_NUM; i++) {
                printf("Relay %d: switched - %d, total - %d\n", i, last_relay_report->RelayStats[i].switched_count,
                       last_relay_report->RelayStats[i].total_time_on * 1000);
            }

        bool update = false;
        for (int i = 0; i < MAX_RELAY_NUM; i++) {
            if ((gpio->Stat.relay_switch[i + 1] < last_relay_report->RelayStats[i].switched_count) ||
                (gpio->Stat.relay_time[i + 1] < last_relay_report->RelayStats[i].total_time_on * 1000)) {
                update = true;
            }
        }
        if (update) {
            fprintf(stderr, "Relays in config updated\n");
            for (int i = 0; i < MAX_RELAY_NUM; i++) {
                gpio->Stat.relay_switch[i + 1] = last_relay_report->RelayStats[i].switched_count;
                gpio->Stat.relay_time[i + 1] = last_relay_report->RelayStats[i].total_time_on * 1000;
            }
        }
    } else {
        fprintf(stderr, "Get last relay report err:%d\n", err);
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
    } else {
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

    DiaRuntimeRegistry *registry = config->GetRuntime()->Registry;
    
    std::string default_price = "15";
    DiaNetwork::PingResponse resp;

    int err = 1;
    while (err) {
        err = network->SendPingRequest(_CurrentBalance, _CurrentProgram, _JustTurnedOn, resp);

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

    _JustTurnedOn = false;

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
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        perror("Create socket");
    }
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    // Port defined Here:
    address.sin_port = htons(2223);
    // Bind
    int res = bind(socket_desc, (struct sockaddr *)&address, sizeof(address));
    if (res < 0) {
        printf("bind failed :(\n");
        return 0;
    }
    listen(socket_desc, 32);
    // Do other stuff (includes accepting connections)

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
        if (errAddVendotek != 0) {
            if (errAddVendotek == 4) {
                return CARD_READER_STATUS::VENDOTEK_NULL_DRIVER;
            }
            return CARD_READER_STATUS::VENDOTEK_THREAD_ERROR;
        }
        bool found_card_reader = false;
        for (int i = 0; i < 10 && !found_card_reader; i++) {
            sleep(1);
            found_card_reader = DiaDeviceManager_GetCardReaderStatus(manager) != 0;
            StartScreenMessage(STARTUP_MESSAGE::CARD_READER, "Try to find VENDOTEK (Attempt " + std::to_string(i + 1) + " of 10)");
        }
        if (found_card_reader) {
            _IsSbpPaymentOnTerminalAvailable = true;
            return CARD_READER_STATUS::VENDOTEK_SUCCES;
        } else {
            return CARD_READER_STATUS::VENDOTEK_NOT_FOUND;
        }
    }
    printf("card reader not used\n");
    return CARD_READER_STATUS::NOT_USED;
}

int main(int argc, char **argv) {

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

    if (StartScreenInit(folder)) {
        fprintf(stderr, "SDL Initialization Failed\n");
        return 1;
    }
    StartScreenUpdate();

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
        } else {
            StartScreenMessage(STARTUP_MESSAGE::SERVER_IP, "Server IP: " + serverIP);
            need_to_find = 0;
        }	    

        KeyPress();
        if (_to_be_destroyed) {
            return 1;
        }
    }

    network->SetHostAddress(serverIP);

    // Let's run a thread to ping server
    pthread_create(&pinging_thread, NULL, pinging_func, NULL);

    std::string stationIDasString = "";
    int stationID = 0;
    while (stationID == 0) {
        StartScreenMessage(STARTUP_MESSAGE::POST, "POST: check");
        stationIDasString = network->GetStationID();
        try {
            stationID = std::stoi(stationIDasString);
        } catch (...) {
            printf("Wrong post number received [%s]\n", stationIDasString.c_str());
        }
        if (stationID == 0) {
            sleep(1);
            StartScreenMessage(STARTUP_MESSAGE::POST, "POST is not assigned");
            printf("POST is not assigned\n");
            sleep(1);
        }

        KeyPress();
        if (_to_be_destroyed) {
            return 1;
        }
    }
    StartScreenMessage(STARTUP_MESSAGE::POST, "POST: " + std::to_string(stationID));

    printf("Card reader initialization...\n");
    StartScreenMessage(STARTUP_MESSAGE::CARD_READER, "Card Reader initialization...");
    // Runtime and firmware initialization
    Logger* logger = new Logger(network);
    DiaDeviceManager *manager = new DiaDeviceManager(logger);
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
                if (manager->_Vendotek) {
                    delete manager->_Vendotek;
                }
                sleep(1);
                break;
            case CARD_READER_STATUS::VENDOTEK_NULL_DRIVER:
                StartScreenMessage(STARTUP_MESSAGE::CARD_READER, "VENDOTEK: NULL driver");
                if (manager->_Vendotek) {
                    delete manager->_Vendotek;
                }
                sleep(1);
                break;
            case CARD_READER_STATUS::VENDOTEK_THREAD_ERROR:
                StartScreenMessage(STARTUP_MESSAGE::CARD_READER, "VENDOTEK ERROR: Failed to create thread");
                if (manager->_Vendotek) {
                    delete manager->_Vendotek;
                }
                sleep(1);
                break;
            case CARD_READER_STATUS::VENDOTEK_NOT_FOUND:
                StartScreenMessage(STARTUP_MESSAGE::CARD_READER, "VENDOTEK ERROR: Not found card reader");
                if (manager->_Vendotek) {
                    delete manager->_Vendotek;
                }
                sleep(1);
                break;
            default:
                break;
        }
        KeyPress();
        if (_to_be_destroyed) {
            return 1;
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
            KeyPress();
            if (_to_be_destroyed) {
                return 1;
            }
        }
        return 1;
    break;
    case CONFIGURATION_STATUS::ERROR_GPIO:
        fprintf(stderr,"Configuration initialization: Failed to init GPIO\n");
        StartScreenMessage(STARTUP_MESSAGE::CONFIGURATION, "Failed to init GPIO");
        while (1) {
            sleep(1);
            KeyPress();
            if (_to_be_destroyed) {
                return 1;
            }
        }
        return 1;
    break;
    case CONFIGURATION_STATUS::ERROR_JSON:
        fprintf(stderr,"Configuration initialization: Bad configuration file\n");
        StartScreenMessage(STARTUP_MESSAGE::CONFIGURATION, "Bad configuration file");
        while (1) {
            sleep(1);
            KeyPress();
            if (_to_be_destroyed) {
                return 1;
            }
        }
        return 1;
    break;
    case CONFIGURATION_STATUS::SUCCESS:
        StartScreenMessage(STARTUP_MESSAGE::CONFIGURATION, "Configuration initializated");
        break;    
    default:
        break;
    }
    
    err = 1;
    StartScreenMessage(STARTUP_MESSAGE::SETTINGS, "Loading settings from server");
    while (err != 0) {
        err = config->LoadConfig();
        if (err) {
            fprintf(stderr, "Error loading settings from server\n");
            StartScreenMessage(STARTUP_MESSAGE::SETTINGS, "Error loading settings from server");
            sleep(1);
        }

        KeyPress();
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
        
        KeyPress();
        if (_to_be_destroyed) {
            return 1;
        }
    }

    printf("Settings loaded...\n");
    StartScreenMessage(STARTUP_MESSAGE::SETTINGS, "Settings from server loaded");
    _ServerRelayBoardMode = config->GetServerRelayBoard();
    if (IsRemoteRelayBoardMode()) {
        int err = 1;
        StartScreenMessage(STARTUP_MESSAGE::RELAY_CONTROL_BOARD, "Checking relay control server board");
        while (err) {
            printf("check relay control server board\n");
            err = network->RunProgramOnServer(0, 0);
            if (err != 0) {
                fprintf(stderr, "relay control server board not found\n");
                StartScreenMessage(STARTUP_MESSAGE::RELAY_CONTROL_BOARD, "Relay control server board not found");
            }
            sleep(1);

            KeyPress();
            if (_to_be_destroyed) {
                return 1;
            }
        }
        StartScreenMessage(STARTUP_MESSAGE::RELAY_CONTROL_BOARD, "Relay control server board found");
    }
    
    // Get working data from server: money, relays, prices
    RecoverData();

    printf("Configuration is loaded...\n");

    StartScreenShutdown();

    printf("Shut down of the start screen complete..., loading screens\n");
    // Screen load
    std::map<std::string, DiaScreenConfig *>::iterator it;
    for (it = config->ScreenConfigs.begin(); it != config->ScreenConfigs.end(); it++) {
        std::string currentID = it->second->id;
        DiaRuntimeScreen *screen = new DiaRuntimeScreen();
        screen->Name = currentID;
        screen->object = (void *)it->second;
        screen->set_value_function = dia_screen_config_set_value_function;
        screen->screen_object = config->GetScreen();
        screen->display_screen = dia_screen_display_screen;
        config->GetRuntime()->AddScreen(screen);
    }
    
    config->GetRuntime()->AddAnimations();
  
    DiaRuntimeHardware * hardware = new DiaRuntimeHardware();
    hardware->keys_object = config->GetGpio();
    hardware->get_keys_function = get_key;

    printf("HW init 1...\n");
    hardware->light_object = config->GetGpio();
    hardware->turn_light_function = turn_light;

    hardware->CreateSession_function = CreateSession;
    hardware->EndSession_function = EndSession;
    hardware->CloseVisibleSession_function = CloseVisibleSession;

    hardware->getVisibleSession_function = getVisibleSession;
    hardware->getQR_function = getQR;
    hardware->SetBonuses_function = SetBonuses;

    hardware->get_sbp_qr_function = getSbpQr;

    hardware->sendPause_function = sendPause;

    hardware->create_sbp_payment_function = createSbpPayment;

    hardware->get_can_play_video_function = getCanPlayVideo;
    hardware->set_can_play_video_function = setCanPlayVideo;
    hardware->get_is_playing_video_function = getIsPlayingVideo;
    hardware->set_is_playing_video_function = setIsPlayingVideo;
    hardware->get_process_id_function = getProcessId;

    hardware->get_is_connected_to_bonus_system_function = getIsConnectedToBonusSystem;
    hardware->set_is_connected_to_bonus_system_function = setIsConnectedToBonusSystem;

    hardware->get_is_sbp_payment_on_terminal_available = getIsSbpPaymentOnTerminalAvailable;

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
    hardware->get_bonuses_function = get_bonuses;
    hardware->get_sbp_money_function = get_sbp_money;
    hardware->get_is_preflight_function = get_is_preflight;
    hardware->get_openlid_function = get_openlid;
    hardware->get_electronical_function = get_electronical;
    hardware->get_sbp_terminal_money_function = get_sbp_terminal_money;
    hardware->request_transaction_function = request_transaction;
    hardware->request_transaction_separated_function = request_transaction_separated;
    //--------------------------------------------------------------------------
    hardware->confirm_transaction_function = confirm_transaction;
    //--------------------------------------------------------------------------
    printf("HW init 8...\n");
    hardware->get_transaction_status_function = get_transaction_status;
    hardware->get_volume_function = get_volume;
    hardware->get_sensor_active_function = get_sensor_active;
    hardware->start_fluid_flow_sensor_function = start_fluid_flow_sensor;
    hardware->abort_transaction_function = abort_transaction;
    hardware->set_current_state_function = set_current_state;
    printf("HW init 7...\n");

    hardware->bonus_system_is_active_function = bonus_system_is_active;
    hardware->sbp_system_is_active_function = sbp_system_is_active;
    hardware->authorized_session_ID_function = authorized_session_ID;
    hardware->bonus_system_refresh_active_qr_function = bonus_system_refresh_active_qr;
    hardware->bonus_system_start_session_function = bonus_system_start_session;
    hardware->bonus_system_confirm_session_function = bonus_system_confirm_session;
    hardware->bonus_system_finish_session_unction = bonus_system_finish_session;

    hardware->delay_object = &stored_time;
    hardware->smart_delay_function = smart_delay_function;
    

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
    
    //InitSensorButtons();

    // Runtime start
    int keypress = 0;

    // Call Lua setup function
    config->GetRuntime()->Setup();
    
    printf("Pulse init...\n");
    // using button as pulse is a crap obviously
    if (config->UseLastButtonAsPulse() && config->GetGpio()) {
        printf("enabling additional coin handler\n");
        int preferredButton = config->GetButtonsNumber() + 1;
        DiaGpio_StartAdditionalHandler(config->GetGpio(), preferredButton);
    } else {
        printf("no additional coin handler\n");
    }

    pthread_create(&run_program_thread, NULL, run_program_func, NULL);
    printf("get_volume_func start...\n");
    pthread_create(&get_volume_thread, NULL, get_volume_func, NULL);

    pthread_create(&play_video_thread, NULL, play_video_func, NULL);

    while (!keypress) {
        // Call Lua loop function
        config->GetRuntime()->Loop();

        int x = 0;
        int y = 0;
        Uint32 mouseState = SDL_GetMouseState(&x, &y);
        if (config->NeedRotateTouch()) {
            x = config->GetResX() - x;
            y = config->GetResY() - y;
        }

        // Process pressed button
        DiaScreen *screen = config->GetScreen();
        std::string last = screen->LastDisplayed;

        for (auto it = config->ScreenConfigs[last]->clickAreas.begin(); 
            it != config->ScreenConfigs[last]->clickAreas.end(); ++it) {
            if (x >= (*it).X && x <= (*it).X + (*it).Width && 
                y >= (*it).Y && y <= (*it).Y + (*it).Height && 
                (mouseState & SDL_BUTTON_LMASK)) {  // Check if left button is pressed
                printf("CLICK!!!\n");
                _DebugKey = std::stoi((*it).ID);
                printf("DEBUG KEY = %d\n", _DebugKey);
                break;  // Probably want to break after first hit
            }
        }

        while (SDL_PollEvent(&_event)) {
            switch (_event.type) {
                case SDL_QUIT:
                    keypress = 1;
                    printf("Quitting by sdl_quit\n");
                    break;
                case SDL_KEYDOWN:
                    switch (_event.key.keysym.sym) {
                        case SDLK_UP:
                            // Debug service money addition
                            _BalanceBanknotes += 10;

                            printf("UP\n");
                            fflush(stdout);
                            break;
                        case SDLK_DOWN:
                            // Debug service money addition
                            _BalanceCoins += 1;

                            printf("DOWN\n");
                            fflush(stdout);
                            break;

                        case SDLK_1:
                            _DebugKey = 1;
                            printf("1\n");
                            fflush(stdout);
                            break;

                        case SDLK_2:
                            _DebugKey = 2;
                            printf("2\n");
                            fflush(stdout);
                            break;

                        case SDLK_3:
                            _DebugKey = 3;
                            printf("3\n");
                            fflush(stdout);
                            break;

                        case SDLK_4:
                            _DebugKey = 4;
                            printf("4\n");
                            fflush(stdout);
                            break;

                        case SDLK_5:
                            _DebugKey = 5;
                            printf("5\n");
                            fflush(stdout);
                            break;

                        case SDLK_6:
                            _DebugKey = 6;
                            printf("6\n");
                            fflush(stdout);
                            break;

                        case SDLK_7:
                            _DebugKey = 7;
                            printf("7\n");
                            fflush(stdout);
                            break;

                        case SDLK_ESCAPE:
                            keypress = 1;
                            printf("Quitting by escape key...\n");
                            break;

                        default:
                            keypress = 1;
                            printf("Quitting by keypress...\n");
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