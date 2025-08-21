#include "dia_configuration.h"
#include "dia_functions.h"
#include "dia_storage_interface.h"
#include <string.h>
#include <stdlib.h>

int DiaConfiguration::InitFromFile() {
    std::string resource = dia_get_resource(_Folder.c_str(), DIA_DEFAULT_FIRMWARE_FILENAME);
    return InitFromString(resource.c_str());
}

int DiaConfiguration::RunCommand(std::string command) {
    return 0;
}

int DiaConfiguration::Setup() {
    return GetRuntime()->Setup();
}

int DiaConfiguration::Loop() {
    return GetRuntime()->Loop();
}

int DiaConfiguration::InitFromString(const char * configuration_json) {
    json_t *root;
    json_error_t error;
    root = json_loads(configuration_json, 0, &error);

    if(!root) {
        fprintf(stderr, "json error: on line %d: %s\n", error.line, error.text);
        printf("json:\n%s\n\n", configuration_json);
        return CONFIGURATION_STATUS::ERROR_JSON;
    }

    int res = InitFromJson(root);
    json_decref(root);
    return res;
}

int DiaConfiguration::Init() {
    int err = InitFromFile();
    if(err) {
        return err;
    }
    _Gpio =0;
    if (!err) {
        int hideMouse = 0;
        int fullScreen = 0;
        #ifdef USE_GPIO
        hideMouse = 1;
        fullScreen = 1;
        #endif
        #ifdef DEBUG
        hideMouse = 0;
        fullScreen = 0;
        #endif
        _Screen = new DiaScreen(GetResX(), GetResY(), hideMouse, fullScreen);
        if (_Screen->InitializedOk!=1) {
            return CONFIGURATION_STATUS::ERROR_SCREEN;
        }
        _Gpio = 0;
        #ifdef USE_GPIO
        _Gpio = new DiaGpio(GetButtonsNumber(), GetRelaysNumber(), GetStorage());
        if (!_Gpio->InitializedOk) {
            printf("ERROR: GPIO INIT");
            return CONFIGURATION_STATUS::ERROR_GPIO;
        }
        #endif
    }
    return err;
}

DiaConfiguration::DiaConfiguration(std::string folder, DiaNetwork *newNet) {
    _Name = "";
    _Folder = folder;
    _ResX = 0;
    _ResY = 0;
    _ButtonsNumber = 0;
    _ProgramsNumber = 0;
    _RelaysNumber = 0;
    _PreflightSec = 0;
    _ServerRelayBoard = RelayBoardMode::LocalGPIO;

    // Must be rearranged
    registry = new DiaRuntimeRegistry(newNet);
    _Runtime = new DiaRuntime(registry);
    _Gpio = 0;
    _Screen = 0; // use Init();
    _svcWeather = new DiaRuntimeSvcWeather(newNet);

    _Storage = CreateEmptyInterface();
    _Net = newNet;
}

int DiaConfiguration::InitFromJson(json_t * configuration_json) {
    if (configuration_json == 0) {
         return CONFIGURATION_STATUS::ERROR_JSON;
    }

    // Let's unpack Name
    json_t * name_json = json_object_get(configuration_json, "name");
    if(!json_is_string(name_json)) {
        fprintf(stderr, "error: configuration name is not a string\n");
        return CONFIGURATION_STATUS::ERROR_JSON;
    }

    _Name = json_string_value(name_json);

    // Let's unpack Screens
    json_t * screens_json = json_object_get(configuration_json, "screens");
    if(!json_is_array(screens_json)) {
        fprintf(stderr, "error: screens is not an array\n");
        return CONFIGURATION_STATUS::ERROR_JSON;
    }

    json_t *resolution_json = json_object_get(configuration_json, "resolution");
    if(!json_is_string(resolution_json)) {
        fprintf(stderr, "error: resolution is not a string\n");
        return CONFIGURATION_STATUS::ERROR_JSON;
    }

    std::string _Resolution = json_string_value(resolution_json);
    int _ResN = _Resolution.length();
    _ResX = 0;
    int resolutionCursor;
    for (resolutionCursor=0; resolutionCursor < _ResN; resolutionCursor++) {
        char key = _Resolution[resolutionCursor];
        if (key>='0' && key<='9') {
            _ResX*=10;
            _ResX+=key-'0';
        } else {
            break;
        }
    }
    _ResY =0;
    resolutionCursor++;
     for (; resolutionCursor< _ResN; resolutionCursor++) {
        char key = _Resolution[resolutionCursor];
        if (key>='0' && key<='9') {
            _ResY*=10;
            _ResY+=key-'0';
        } else {
            printf("resolution parsing error: [%s], must by like [848x480]\n", _Resolution.c_str());
        }
    }

    _NeedToRotateTouchScreen = 0;
    json_t *touch_rotate_json = json_object_get(configuration_json, "touch_rotate");
    if(json_is_string(touch_rotate_json)) {
        _NeedToRotateTouchScreen = 180;
    }

    // Let's unpack buttons #
    json_t *buttons_json = json_object_get(configuration_json, "buttons");
    if(!json_is_integer(buttons_json)) {
        fprintf(stderr, "error: buttons is not an integer\n");
        return CONFIGURATION_STATUS::ERROR_JSON;
    }

    _ButtonsNumber = json_integer_value(buttons_json);

    json_t *programs_json = json_object_get(configuration_json, "programs");
    if(!json_is_integer(buttons_json)) {
        fprintf(stderr, "error: programs is not an integer\n");
        return CONFIGURATION_STATUS::ERROR_JSON;
    }
    _ProgramsNumber = json_integer_value(programs_json);

    // Let's check if we can use the last button as a pulse coin

    _LastButtonPulse = 0;
    json_t *last_button_pulse_json = json_object_get(configuration_json, "last_button_pulse");
    if (last_button_pulse_json) {
        if(json_is_boolean(last_button_pulse_json)) {
            _LastButtonPulse = json_boolean_value(last_button_pulse_json);
        } else if (json_is_integer(last_button_pulse_json)) {
            _LastButtonPulse = json_integer_value(last_button_pulse_json);
        }
    }

    // Let's unpack relays #
    json_t *relays_json = json_object_get(configuration_json, "relays");
    if(!json_is_integer(relays_json)) {
        fprintf(stderr, "error: relays is not a integer\n");
        return CONFIGURATION_STATUS::ERROR_JSON;
    }
    _RelaysNumber = json_integer_value(relays_json);

    for(unsigned int i = 0; i < json_array_size(screens_json); i++) {
        json_t * screen_json = json_array_get(screens_json, i);
        if(!json_is_object(screen_json)) {
            fprintf(stderr, "error: screen %d is not an object\n", i + 1);
            return CONFIGURATION_STATUS::ERROR_JSON;
        }

        DiaScreenConfig * screen_parsed = new DiaScreenConfig();
        json_t * id_json = json_object_get(screen_json, "id");
        if(!json_is_string(id_json)) {
            fprintf(stderr, "error: screen id is not a string\n");
            return CONFIGURATION_STATUS::ERROR_JSON;
        }

        std::string id = json_string_value(id_json);
        ScreenConfigs.insert(std::pair<std::string, DiaScreenConfig *>(id, screen_parsed));
        ScreenConfigs[id]->Init(_Folder, screen_json);
    }

    json_t * script_json = json_object_get(configuration_json, "script");
    json_t * include_json = json_object_get(configuration_json, "include");
    int err =  GetRuntime()->Init(_Folder, script_json, include_json);
    if (err != 0){
        return CONFIGURATION_STATUS::ERROR_JSON;
    } else {
        return CONFIGURATION_STATUS::SUCCESS;
    }
}

int DiaConfiguration::LoadConfig() {
    std::string answer;
    int err = _Net->GetStationConig(answer);
    if (err !=0) {
        fprintf(stderr, "error: load config\n");
        return 1;
    }
    json_error_t error;
    json_t * configuration_json;
    configuration_json = json_loads(answer.c_str(), 0, &error);
    if (!configuration_json) {
        printf("Error in LoadConfig: %d: %s\n", error.line, error.text );
        json_decref(configuration_json);
        return 1;
    }

    if(!json_is_object(configuration_json)) {
	    printf("LoadConfig not a JSON\n");
        json_decref(configuration_json);
                return 1;
    }

    json_t *preflight_json = json_object_get(configuration_json, "preflightSec");
    if(json_is_integer(preflight_json)) {
        _PreflightSec = json_integer_value(preflight_json);
    } else {
        _PreflightSec = 0;
    }
    json_t *relay_board_json = json_object_get(configuration_json, "relayBoard");
    if(json_is_string(relay_board_json)) {
        std::string board = json_string_value(relay_board_json);
        printf("LoadConfig relay board %s\n", board.c_str());
        if (board == "danBoard") {
            _ServerRelayBoard = RelayBoardMode::DanBoard;
        } else if (board == "all") {
            _ServerRelayBoard = RelayBoardMode::All;
        } else {
            _ServerRelayBoard = RelayBoardMode::LocalGPIO;
        }
    }
    
    std::map<int, DiaProgram*> tmpPrograms;
    // Let's unpack programs
    json_t *programs_json = json_object_get(configuration_json, "programs");
    if(!json_is_array(programs_json)) {
        fprintf(stderr, "error: programs is not an array\n");
        json_decref(programs_json);
        json_decref(configuration_json);
        return 1;
    }
    for (unsigned int i=0;i< json_array_size(programs_json); i++) {
        json_t * program_json = json_array_get(programs_json, i);
        if(!json_is_object(program_json)) {
            fprintf(stderr, "error: program %d is not an object\n", i + 1);
            json_decref(program_json);
            json_decref(programs_json);
            json_decref(configuration_json);
            return 1;
        }
        // XXX rebuild
        DiaProgram * program = new DiaProgram(program_json);
        if(!program->_InitializedOk) {
            printf("Something's wrong with the program");
            delete program;
            json_decref(program_json);
            json_decref(programs_json);
            json_decref(configuration_json);
            return 1;
        }
        int buttonID = program->ButtonID;
        delete program;
        tmpPrograms[buttonID] = new DiaProgram(program_json);
    }
    for (int i = 1;i <= GetProgramsNumber(); i++) {
        if (!tmpPrograms[i]) {
        fprintf(stderr, "error: LoadConfig, program # %d not found\n", i);
        json_decref(programs_json);
        json_decref(configuration_json);
        return 1;
        }
    }

    if(!this->_Programs.empty()) {
        std::map<int, DiaProgram*>::iterator it;

	    for (it = this->_Programs.begin(); it != this->_Programs.end(); it++) 
	    { 
            delete it->second;
	    }
    }

    this->_Programs = tmpPrograms;

    #ifdef USE_GPIO
    // Let's copy programs
    std::map<int, DiaProgram *>::iterator it;
    int programNum = 0;
    for(it=this->_Programs.begin();it!=this->_Programs.end();it++) {
        programNum++;
        DiaProgram * curProgram = it->second;
        for(int i=0;i<PIN_COUNT_CONFIG;i++) {
            if (curProgram->Relays.RelayNum[i]>0) {
                int id = curProgram->Relays.RelayNum[i];
                int ontime = curProgram->Relays.OnTime[i];
                int offtime = curProgram->Relays.OffTime[i];
                _Gpio->Programs[curProgram->ButtonID].InitRelay(id, ontime, offtime);
            }else{
                _Gpio->Programs[curProgram->ButtonID].ClearRelay(i);
            }
            if (curProgram->PreflightRelays.RelayNum[i]>0) {
                int id = curProgram->PreflightRelays.RelayNum[i];
                int ontime = curProgram->PreflightRelays.OnTime[i];
                int offtime = curProgram->PreflightRelays.OffTime[i];
                _Gpio->PreflightPrograms[curProgram->ButtonID].InitRelay(id, ontime, offtime);
            }else{
                _Gpio->PreflightPrograms[curProgram->ButtonID].ClearRelay(i);
            }
        }
    }
    #endif

    json_t *last_update = json_object_get(configuration_json, "lastUpdate");
    if (json_is_integer(last_update)){
        _LastUpdate = json_integer_value(last_update);
    }

    json_decref(programs_json);
    json_decref(relay_board_json);
    json_decref(preflight_json);
    json_decref(last_update);
    json_decref(configuration_json);
    return 0;
}

int DiaConfiguration::LoadDiscounts() {
    std::string answer;
    int err = _Net->GetDiscounts(answer);
    if (err != 0) {
        fprintf(stderr, "error: load advertisting campagins\n");
        return 1;
    }

    json_error_t error;
    json_t *station_discounts_json;

    station_discounts_json = json_loads(answer.c_str(), 0, &error);
    if (!station_discounts_json) {
        json_decref(station_discounts_json);
        return 1;
    }

    if (!json_is_array(station_discounts_json)) {
        json_decref(station_discounts_json);
        printf("LoadConfig not a JSON\n");
        return 1;
    }

    this->_Discounts.clear();

    json_t *button_discount_json;
    json_t *button_id_json;
    json_t *button_discount_value_json;
    for (unsigned int i = 0; i < json_array_size(station_discounts_json); i++) {
        button_discount_json = json_array_get(station_discounts_json, i);
        if (json_is_object(button_discount_json)) {
            int button, dicount;
            button_id_json = json_object_get(button_discount_json, "buttonID");
            if json_is_integer (button_id_json) {
                button = json_integer_value(button_id_json);
            } else {
                continue;
            }
            button_discount_value_json = json_object_get(button_discount_json, "discount");
            if json_is_integer (button_discount_value_json) {
                dicount = json_integer_value(button_discount_value_json);
                if (dicount > 0) {
                    this->_Discounts[button] = dicount;
                }
            }
        }
    }
    json_decref(station_discounts_json);
    json_decref(button_discount_json);
    json_decref(button_id_json);
    json_decref(button_discount_value_json);
    return 0;
}

DiaConfiguration::~DiaConfiguration() {
    printf("Destroying configuration ... \n");

    std::map<std::string, DiaScreenConfig *>::iterator it;
    for (it=ScreenConfigs.begin(); it!=ScreenConfigs.end(); it++) {
        DiaScreenConfig * curConfig = it->second;
        if(curConfig!=0) {
            delete curConfig;
        }
    }
    delete _Runtime;
    if (_Screen) {
        delete _Screen;
    }
    if(_Gpio) {
        delete _Gpio;
    }
    if(registry) {
        delete registry;
    }
    if(_svcWeather) {
        delete _svcWeather;
    }
}