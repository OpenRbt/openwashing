#ifndef DIA_CONFIGURATION_H
#define DIA_CONFIGURATION_H
#include <map>
#include <string>
#include <jansson.h>
#include "dia_screen_config.h"
#include "dia_screen.h"
#include "dia_runtime.h"
#include "dia_gpio.h"
#include "dia_program.h"

#define DIA_DEFAULT_FIRMWARE_FILENAME "main.json"

enum CONFIGURATION_STATUS {SUCCESS,ERROR_GPIO,ERROR_SCREEN, ERROR_JSON};
enum RelayBoardMode {
    DanBoard,
    LocalGPIO,
    All
};

class DiaConfiguration {
private:
    DiaRuntimeRegistry * registry;
public:
    std::map<std::string, DiaScreenConfig *> ScreenConfigs;

    DiaConfiguration(std::string folder, DiaNetwork * newNet);
    ~DiaConfiguration();

    int Init();
    // how did they get here??? they must be runtime!
    // we just need to READ all configs here!
    int Setup();
    int Loop();
    int RunCommand(std::string command);
    int LoadConfig();
    int LoadDiscounts();

    // DELETE methods above
    DiaScreen * GetScreen() {
        assert(_Screen);
        return _Screen;
    }
    
    storage_interface_t *GetStorage() {
        assert(_Storage);
        return _Storage;
    }
    
    DiaRuntime *GetRuntime() {
        assert(_Runtime);
        return _Runtime;
    }
    
    DiaGpio *GetGpio() {
        #if defined(USE_GPIO) || defined(MOCK_GPIO)
        return _Gpio;
        #endif
        return 0;        
    }

    DiaRuntimeSvcWeather *GetSvcWeather() {
        assert(_svcWeather);
        return _svcWeather;
    }
    
    int GetResX() {
        assert(_ResX);
        return _ResX;
    }
    
    int GetResY() {
        assert(_ResY);
        return _ResY;
    }
    
    int GetButtonsNumber() {
        return _ButtonsNumber;
    }

    int GetProgramsNumber() {
        return _ProgramsNumber;
    }

    int GetPrice(int button) {
        if (_Programs[button]) {
            if (_Discounts.count(button)) {
                return (int)(_Programs[button]->Price * (100 - _Discounts[button]) / 100.0);
            }
            return _Programs[button]->Price;
        }
        return 0;
    }

    int GetDiscount(int button){
        if (_Discounts.count(button)){
            return _Discounts[button];
        }
        return 0;
    }

    int GetPreflightSec(int button) {
        if ((_Programs[button]) && (_PreflightSec>0)) {
            if (_Programs[button]->PreflightEnabled) {
                return _PreflightSec;
            }
        }
        return 0;
    }
    int GetProgramID(int button) {
        if (_Programs[button]) {
            return _Programs[button]->ProgramID;
        }
        return 0;
    }

    int GetIsFinishingProgram(int button){
        if (_Programs[button]){
            if (_Programs[button]->IsFinishingProgram){
                return 1;
            }
        }
        return 0;
    }

    int GetRelaysNumber() {
        return _RelaysNumber;
    }

    RelayBoardMode GetServerRelayBoard() {
        return _ServerRelayBoard;
    }

    inline int NeedRotateTouch() {
        return _NeedToRotateTouchScreen;
    }
    
    int UseLastButtonAsPulse() {
        return _LastButtonPulse;
    }
    
    int GetLastUpdate(){
        return _LastUpdate;
    }

    int GetDiscountLastUpdate(){
        return _DiscountLastUpdate;
    }
    void SetDiscountLastUpdate(int value){
        _DiscountLastUpdate = value;
    }

    std::string GetFolder(){
        return _Folder;
    }

    private:
    std::string _Name;
    std::string _Folder;
    std::map<int, DiaProgram*> _Programs;
    std::map<int, int> _Discounts;
    int _PreflightSec;
    RelayBoardMode _ServerRelayBoard;
    DiaScreen * _Screen;
    DiaRuntime * _Runtime;
    DiaGpio * _Gpio;
    DiaRuntimeSvcWeather * _svcWeather;
    storage_interface_t * _Storage;
    DiaNetwork * _Net;
    
    
    int _ResX;
    int _ResY;
    int _ButtonsNumber;
    int _ProgramsNumber;
    int _RelaysNumber;
    int _NeedToRotateTouchScreen;
    
    int _LastButtonPulse;

    int _LastUpdate = -1;
    int _DiscountLastUpdate = -1;
    int InitFromFile();
    int InitFromString(const char * configuration_json);// never ever is going to be virtual
    int InitFromJson(json_t * configuration_json); //never ever virtual
};
#endif