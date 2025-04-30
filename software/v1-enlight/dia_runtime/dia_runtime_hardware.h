#ifndef dia_runtime_hardware_h
#define dia_runtime_hardware_h

#include <ctime>

#include "dia_functions.h"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <jansson.h>

#include <list>
#include <string>

#include "LuaBridge.h"

using namespace luabridge;

class DiaRuntimeHardware {
   public:
    std::string Name;

    void* light_object;
    int (*turn_light_function)(void* object, int pin, int animation_id);
    int TurnLight(int light_pin, int AnimationID) {
        if (light_object && turn_light_function) {
            turn_light_function(light_object, light_pin, AnimationID);
        } else {
        }
        return 0;
    }

    void* program_object;
    int (*turn_program_function)(void* object, int program);
    int TurnProgram(int ProgramNum) {
        if (/*program_object && */ turn_program_function) {
            turn_program_function(program_object, ProgramNum);
        } else {
        }
        return 0;
    }

    int (*send_receipt_function)(int postPosition, int cash, int electronical, int qrMoney);

    int SendReceipt(int postPosition, int cash, int electronical, int qrMoney) {
        if (send_receipt_function) {
            send_receipt_function(postPosition, cash, electronical, qrMoney);
        } else {
            printf("error: NIL function SendReceipt\n");
        }
        return 0;
    }

    int (*CreateSession_function)();
    int CreateSession() {
        if(CreateSession_function){
            int ans = CreateSession_function();
            return ans;
        }
        else{
            printf("error: NIL object or function CreateSession\n");
        }
        return 0;
    }

    int (*EndSession_function)();
    int EndSession() {
        if(EndSession_function){
            int ans = EndSession_function();
            return ans;
        }
        else{
            printf("error: NIL object or function EndSession_function\n");
        }
        return 0;
    }

    int(*CloseVisibleSession_function)();
    int CloseVisibleSession() {
        if(CloseVisibleSession_function){
            int ans = CloseVisibleSession_function();
            return ans;
        }
        else{
            printf("error: NIL object or function CloseVisibleSession_function\n");
        }
        return 0;
    }

    int (*SetBonuses_function)(int bonuses);
    int SetBonuses(int bonuses) {
        if(SetBonuses_function){
            int ans = SetBonuses_function(bonuses);
            return ans;
        }
        else{
            printf("error: NIL object or function SetBonuses_function\n");
        }
        return 0;
    }

    std::string (*getQR_function)();
    std::string GetQR() {
        if(getQR_function){
            std::string QR = getQR_function();
            return QR;
        }
        else{
            printf("error: NIL object or function getQR_function\n");
        }
        return "";
    }

    std::string (*get_sbp_qr_function)();
    std::string GetSbpQR() {
        if(get_sbp_qr_function){
            std::string QR = get_sbp_qr_function();
            return QR;
        }
        else{
            printf("error: NIL object or function get_sbp_qr_function\n");
        }
        return "";
    }

    int (*sendPause_function)();
    int SendPause() {
        if(sendPause_function){
            int ans = sendPause_function();
            return ans;
        }
        else{
            printf("error: NIL object or function sendPause_function\n");
        }
        return 0;
    }

    int (*create_sbp_payment_function)(int amount);
    int CreateSbpPayment(int amount) {
        if(create_sbp_payment_function){
            int ans = create_sbp_payment_function(amount);
            return ans;
        }
        else{
            printf("error: NIL object or function create_sbp_payment_function\n");
        }
        return 0;
    }

    std::string (*getVisibleSession_function)();
    std::string GetVisibleSession() {
        if(getVisibleSession_function){
            std::string sessionID = getVisibleSession_function();
            return sessionID;
        }
        else{
            printf("error: NIL object or function getVisibleSession_function\n");
        }
        return "";
    }

    std::string (*getActiveSession_function)();
    std::string GetActiveSession() {
        if(getActiveSession_function){
            std::string sessionID = getActiveSession_function();
            return sessionID;
        }
        else{
            printf("error: NIL object or function getActiveSession_function\n");
        }
        return "";
    }

    int (*set_current_state_function)(int balance);
    int SetCurrentState(int balance) {
        if (set_current_state_function) {
            return set_current_state_function(balance);
        } else {
            printf("error: NIL object or function SetCurrentState\n");
        }
        return 0;
    }

    int (*increment_cars_function)();
    int IncrementCars() {
        if (increment_cars_function) {
            increment_cars_function();
        } else {
            printf("error: NIL function IncrementCars\n");
        }
        return 0;
    }

    void* coin_object;
    int (*get_coins_function)(void* object);
    int GetCoins() {
        if (coin_object && get_coins_function) {
            return get_coins_function(coin_object);
        } else {
            printf("error: NIL object or function GetCoins\n");
        }
        return 0;
    }

    void* banknote_object;
    int (*get_banknotes_function)(void* object);
    int GetBanknotes() {
        if (banknote_object && get_banknotes_function) {
            return get_banknotes_function(banknote_object);
        } else {
            printf("error: NIL object or function GetBanknotes\n");
        }
        return 0;
    }

    int (*get_service_function)();
    int GetService() {
        if (get_service_function) {
            return get_service_function();
        } else {
            printf("error: NIL object or function GetService\n");
        }
        return 0;
    }

    int (*get_bonuses_function)();
    int GetBonuses() {
        if (get_bonuses_function) {
            return get_bonuses_function();
        } else {
            printf("error: NIL object or function GetBonuses\n");
        }
        return 0;
    }

    int (*get_sbp_money_function)();
    int GetSbpMoney() {
        if (get_sbp_money_function) {
            return get_sbp_money_function();
        } else {
            printf("error: NIL object or function GetSbpMoney\n");
        }
        return 0;
    }

    int (*get_is_preflight_function)();
    int GetIsPreflight() {
        if (get_is_preflight_function) {
            return get_is_preflight_function();
        } else {
            printf("error: NIL object or function GetService\n");
        }
        return 0;
    }

    int (*get_openlid_function)();
    int GetOpenLid() {
        if (get_openlid_function) {
            return get_openlid_function();
        } else {
            printf("error: NIL object or function GetOpenLid");
        }
        return 0;
    }

    void* electronical_object;
    int (*get_electronical_function)(void* object);
    int GetElectronical() {
        if (electronical_object && get_electronical_function) {
            return get_electronical_function(electronical_object);
        } else {
            printf("error: NIL object or function GetElectronical\n");
        }
        return 0;
    }

    int (*get_sbp_terminal_money_function)(void* object);
    int GetSbpTerminalMoney() {
        if (electronical_object && get_sbp_terminal_money_function) {
            return get_sbp_terminal_money_function(electronical_object);
        } else {
            printf("error: NIL object or function GetSbpTerminalMoney\n");
        }
        return 0;
    }
    
    int (*confirm_transaction_function)(void* object, int balance);
    int ConfirmTransaction(int balance){
        if (confirm_transaction_function && electronical_object) {
            return confirm_transaction_function(electronical_object, balance);
        } else {
            printf("error: NIL object or function ConfirmTransaction\n");
        }
        return 0;
    }

    int (*request_transaction_function)(void* object, int money, bool isTerminalSBP);
    int RequestTransaction(int money) {
        if (electronical_object && request_transaction_function) {
            return request_transaction_function(electronical_object, money, false);
        } else {
            printf("error: NIL object or function RequestTransaction\n");
        }
        return 0;
    }
    int RequestTransactionTerminalSBP(int money) {
        if (electronical_object && request_transaction_function) {
            return request_transaction_function(electronical_object, money, true);
        } else {
            printf("error: NIL object or function RequestTransaction\n");
        }
        return 0;
    }

    int (*request_transaction_separated_function)(void* object, int money);
    int RequestTransactionSeparated(int money) {
        if (electronical_object && request_transaction_separated_function) {
            return request_transaction_separated_function(electronical_object, money);
        } else {
            printf("error: NIL object or function RequestTransactionSeparated\n");
        }
        return 0;
    }

    int (*get_volume_function)();
    int GetVolume() {
        if (get_volume_function) {
            return get_volume_function();
        } else {
            printf("error: NIL object or function GetVolume\n");
        }
        return 0;
    }

    bool (*get_sensor_active_function)();
    bool GetSensorActive() {
        if (get_sensor_active_function) {
            return get_sensor_active_function();
        } else {
            printf("error: NIL object or function GetSensorActive\n");
        }
        return false;
    }

    int (*start_fluid_flow_sensor_function)(int volume);
    int StartFluidFlowSensor(int volume) {
        if (start_fluid_flow_sensor_function) {
            start_fluid_flow_sensor_function(volume);
        } else {
            printf("error: NIL object or function StartFluidFlowSensor\n");
        }
        return 0;
    }

    bool (*get_can_play_video_function)();
    bool GetCanPlayVideo() {
        if (get_can_play_video_function) {
            return get_can_play_video_function();
        } else {
            printf("error: NIL object or function GetCanPlayVideo\n");
        }
        return false;
    }

    void (*set_can_play_video_function)(bool canPlayVideo);
    int SetCanPlayVideo(bool canPlayVideo) {
        if (set_can_play_video_function) {
            set_can_play_video_function(canPlayVideo);
        } else {
            printf("error: NIL object or function SetCanPlayVideo\n");
        }
        return 0;
    }

    bool (*get_is_playing_video_function)();
    bool GetIsPlayingVideo() {
        if (get_is_playing_video_function) {
            return get_is_playing_video_function();
        } else {
            printf("error: NIL object or function GetIsPlayingVideo\n");
        }
        return false;
    }

    void (*set_is_playing_video_function)(bool isPlatingVideo);
    int SetIsPlayingVideo(bool isPlatingVideo) {
        if (set_is_playing_video_function) {
            set_is_playing_video_function(isPlatingVideo);
        } else {
            printf("error: NIL object or function SetIsPlayingVideo\n");
        }
        return 0;
    }

    
    int (*get_process_id_function)();
    int GetProcessId() {
        if (get_process_id_function) {
            return get_process_id_function();
        } else {
            printf("error: NIL object or function GetProcessId\n");
        }
        return false;
    }

    bool (*get_is_connected_to_bonus_system_function)();
    bool GetIsConnectedToBonusSystem() {
        if (get_is_connected_to_bonus_system_function) {
            return get_is_connected_to_bonus_system_function();
        } else {
            printf("error: NIL object or function GetIsConnectedToBonusSystem\n");
        }
        return false;
    }

    void (*set_is_connected_to_bonus_system_function)(bool isConnectedToBonusSystem);
    int SetIsConnectedToBonusSystem(bool isConnectedToBonusSystem) {
        if (set_is_connected_to_bonus_system_function) {
            set_is_connected_to_bonus_system_function(isConnectedToBonusSystem);
        } else {
            printf("error: NIL object or function SetIsConnectedToBonusSystem\n");
        }
        return 0;
    }

    bool (*get_is_sbp_payment_on_terminal_available)();
    bool GetIsSbpPaymentOnTerminalAvailable() {
        if (get_is_sbp_payment_on_terminal_available) {
            return get_is_sbp_payment_on_terminal_available();
        } else {
            printf("error: NIL object or function GetIsSbpPaymentOnTerminalAvailable\n");
        }
        return "";
    }


    // StopProgramOnServer

    // Bonus system

    bool (*bonus_system_is_active_function)();
    bool BonusSystemIsActive() {
        if (bonus_system_is_active_function) {
            return bonus_system_is_active_function();
        }
        printf("error: NIL object or function GetBonusSystemActive\n");
        return false;
    }

    bool (*sbp_system_is_active_function)();
    bool SbpSystemIsActive() {
        if (sbp_system_is_active_function) {
            return sbp_system_is_active_function();
        }
        printf("error: NIL object or function GetSbpSystemActive\n");
        return false;
    }

    

    std::string (*authorized_session_ID_function)();
    std::string AuthorizedSessionID() {
        if (authorized_session_ID_function) {
            return authorized_session_ID_function();
        }
        printf("error: NIL object or function IsAuthorized\n");
        return "";
    }

    int (*bonus_system_refresh_active_qr_function)();
    int BonusSystemRefreshActiveQR() {
        if (bonus_system_refresh_active_qr_function) {
            return bonus_system_refresh_active_qr_function();
        }
        printf("error: NIL object or function BonusSystemRefreshActiveQR\n");
        return false;
    }

    int (*bonus_system_start_session_function)();
    int BonusSystemStartSession() {
        if (bonus_system_start_session_function) {
            return bonus_system_start_session_function();
        }
        printf("error: NIL object or function BonusSystemStartSession\n");
        return 0;
    }

    int (*bonus_system_confirm_session_function)();
    int BonusSystemConfirmSession() {
        if (bonus_system_confirm_session_function) {
            return bonus_system_confirm_session_function();
        }
        printf("error: NIL object or function BonusSystemConfirmSession\n");
        return 0;
    }

    int (*bonus_system_finish_session_unction)();
    int BonusSystemFinishSession() {
        if (bonus_system_finish_session_unction) {
            return bonus_system_finish_session_unction();
        }
        printf("error: NIL object or function BonusSystemFinishSession\n");
        return 0;
    }

    int GetHours() {
        std::time_t t = std::time(0);  // get time now
        std::tm* now = std::localtime(&t);
        return now->tm_hour;
    }

    int GetMinutes() {
        std::time_t t = std::time(0);  // get time now
        std::tm* now = std::localtime(&t);
        return now->tm_min;
    }

    int (*get_transaction_status_function)(void* object);
    int GetTransactionStatus() {
        if (electronical_object && get_transaction_status_function) {
            return get_transaction_status_function(electronical_object);
        } else {
            printf("error: NIL object or function GetTransactionStatus\n");
        }
        return 0;
    }

    int (*abort_transaction_function)(void* object);
    int AbortTransaction() {
        if (electronical_object && abort_transaction_function) {
            return abort_transaction_function(electronical_object);
        } else {
            printf("error: NIL object or function AbortTransaction\n");
        }
        return 0;
    }

    void* keys_object;
    int (*get_keys_function)(void* object);

    int GetKey() {
#if !defined(USE_GPIO) && !defined(MOCK_GPIO)
        assert(get_keys_function);
        return get_keys_function(0);
#endif
        if (keys_object && get_keys_function) {
            return get_keys_function(keys_object);
        }
        printf("error: NIL object or function GetKey\n");
        return 0;
    }

    void* delay_object;
    int (*smart_delay_function)(void* object, int milliseconds);
    int SmartDelay(int milliseconds) {
        if (delay_object && smart_delay_function) {
            return smart_delay_function(delay_object, milliseconds);
        } else {
            printf("error: NIL object or function SmartDelay\n");
        }
        return 0;
    }

    bool has_card_reader;
    bool HasCardReader() {
        return has_card_reader;
    }

    DiaRuntimeHardware() {
        light_object = 0;
        turn_light_function = 0;

        program_object = 0;
        turn_program_function = 0;

        send_receipt_function = 0;

        coin_object = 0;
        get_coins_function = 0;

        banknote_object = 0;
        get_banknotes_function = 0;

        get_service_function = 0;
        get_openlid_function = 0;

        electronical_object = 0;
        get_electronical_function = 0;
        request_transaction_function = 0;
        //----------------------------------------------------------
        confirm_transaction_function = 0;
        //------------------------------------------------------------
        get_transaction_status_function = 0;
        abort_transaction_function = 0;

        increment_cars_function = 0;

        keys_object = 0;
        get_keys_function = 0;

        delay_object = 0;
        smart_delay_function = 0;
        set_current_state_function = 0;
    }
};

#endif
