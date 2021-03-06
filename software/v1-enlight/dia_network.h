#ifndef _DIA_NETWORK_LIB
#define _DIA_NETWORK_LIB

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <assert.h>
#include <curl/curl.h>
#include <unistd.h>
#include "dia_channel.h"
#include <new>
#include <list>
#include <map>
#include <jansson.h>
#include <iomanip>
#include <iostream>
#include <queue>

#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#define MAX_RELAY_NUM 6
#define CHANNEL_SIZE 8192
#define RETRY_DELAY 5

#define SERVER_UNAVAILABLE 2

// Message for report sending channel.
class NetworkMessage {
    public:
    uint8_t resolved;
    unsigned long long entry_id;
    std::string json_request;
    std::string type_request;
    std::string time_stamp;
    std::string route;
    uint64_t stime;
};

class ReceiptToSend {
public:
    int PostPosition;
    int Cash;
    int Electronical;
    ReceiptToSend(int postPosition, int cash, int electronical) {
        PostPosition = postPosition;
        Cash = cash;
        Electronical = electronical;
    }
};

typedef struct curl_answer {
    char * data;
    size_t length;
} curl_answer_t;

typedef struct money_report {
    int cars_total;
    int coins_total;
    int banknotes_total;
    int cashless_total;
    int service_total;
} money_report_t;

typedef struct RelayStat {
    int switched_count;
    int total_time_on;
} RelayStat_t;

typedef struct relay_report {
    RelayStat_t RelayStats[MAX_RELAY_NUM];
} relay_report_t;

class DiaNetwork {
private:
// For receipts


public:
    DiaChannel<ReceiptToSend> * receipts_channel;

    DiaNetwork() {
        _Host = "";
	    _Port = ":8020";
        curl_global_init(CURL_GLOBAL_ALL);

        _OnlineCashRegister = "";
        _PublicKey = "";
        receipts_channel = new DiaChannel<ReceiptToSend>();

        pthread_create(&entry_processing_thread, NULL, DiaNetwork::process_extract, this);
        pthread_create(&receipts_processing_thread, NULL, DiaNetwork::process_receipts, this);
    }

    ~DiaNetwork() {
        printf("Destroying DiaNetwork\n");
        delete receipts_channel;
        curl_global_cleanup();
        StopTheWorld();
        int status = pthread_join(entry_processing_thread, NULL);
        if (status != 0) {
            printf("Main error: can't join reports thread, status = %d\n", status);
        }
        status = pthread_join(receipts_processing_thread, NULL);
        if (status != 0) {
            printf("Main error: can't join receipts thread, status = %d\n", status);
        }
    }

    
    // Base function for sending a GET request.
    // Parameters: gets pre-created HTTP body, modifies answer from server, gets address of host (URL).
    int SendRequestGet(std::string *answer, std::string host_addr, int timeout) {
        assert(answer);

        CURL *curl;
        CURLcode res;
        curl_answer_t raw_answer;

        InitCurlAnswer(&raw_answer);

        curl = curl_easy_init();
        if (curl == NULL) {
            DestructCurlAnswer(&raw_answer);
            return 1;
        }

        curl_easy_setopt(curl, CURLOPT_URL, host_addr.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "diae/0.1");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, this->_Writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw_answer);
	    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            DestructCurlAnswer(&raw_answer);
            curl_easy_cleanup(curl);
            return 1;
        }
        *answer = raw_answer.data;
        DestructCurlAnswer(&raw_answer);
        curl_easy_cleanup(curl);
        return 0;
    }

    // Base function for sending a POST request.
    // Parameters: gets pre-created HTTP body, modifies answer from server, gets address of host (URL).
    int SendRequest(std::string *body, std::string *answer, std::string host_addr) {
        assert(body);
        assert(answer);

        CURL *curl;
        CURLcode res;
        curl_answer_t raw_answer;

        InitCurlAnswer(&raw_answer);

        curl = curl_easy_init();
        if (curl == NULL) {
            DestructCurlAnswer(&raw_answer);
            printf("curl is NULL :( \n");
            return 1;
        }

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "charsets: utf-8");

        curl_easy_setopt(curl, CURLOPT_URL, host_addr.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body->c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "diae/0.1");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, this->_Writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw_answer);
	    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            printf("CURL code is wrong %d\n", res);
            DestructCurlAnswer(&raw_answer);
            curl_easy_cleanup(curl);
            return 1;
        }
        *answer = raw_answer.data;
        DestructCurlAnswer(&raw_answer);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        return 0;
    }

    // Central server searching, if it's IP address is unknown.
    // Gets local machine's IP and starts pinging every address in block.
    // Modifies IP address.
    int SearchCentralServer(std::string &ip) {
        printf("Checking localhost...\n");
        int err = this->SendPingRequestGet("localhost");
        if (!err) {
            ip = "localhost";
	        return 0;
        }

        int sock = socket(PF_INET, SOCK_DGRAM, 0);
        sockaddr_in loopback;

        if (sock == -1) {
            printf("Could not socket\n");
            return SERVER_UNAVAILABLE;
        }

        memset(&loopback, 0, sizeof(loopback));
        loopback.sin_family = AF_INET;
        loopback.sin_addr.s_addr = INADDR_LOOPBACK;   
        loopback.sin_port = htons(9);                 

        if (connect(sock, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback)) == -1) {
            close(sock);
            printf("Could not connect\n");
            return SERVER_UNAVAILABLE;
        }

        socklen_t addrlen = sizeof(loopback);
        if (getsockname(sock, reinterpret_cast<sockaddr*>(&loopback), &addrlen) == -1) {
            close(sock);
            printf("Could not getsockname\n");
            return SERVER_UNAVAILABLE;
        }

        close(sock);

        char buf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &loopback.sin_addr, buf, INET_ADDRSTRLEN) == 0x0) {
            printf("Could not inet_ntop\n");
            return SERVER_UNAVAILABLE;
        } else {
            printf("Local ip address: %s\n", buf);
        }

	    std::string baseIP(buf);
        std::string reqIP;

	    printf("Base IP: %s\n", baseIP.c_str());

	    // Truncate baseIP to prepare for scan
        int dotCount = 0;
        for (int j = 0; j < INET_ADDRSTRLEN; j++) {
            if (baseIP[j] == '.')
                dotCount++;
            reqIP += baseIP[j];
            if (dotCount == 3)
                break;
        }

        // Scan whole block
        for (int i = 1; i < 255; i++) {
            std::string reqUrl = reqIP + std::to_string(i);

            //printf("%s\n", reqUrl.c_str());

            err = this->SendPingRequestGet(reqUrl);

            if (!err) {
                // We found it!
                ip = reqIP + std::to_string(i);
                return 0;
            }         	
        }   	

        return SERVER_UNAVAILABLE;
    }

    // Returns local machine's MAC address of eth0 interface.
    // Modfifes out parameter == MAC address without ":" symbols.
    std::string GetMacAddress(const int outSize) {
   	    int fd;
	
	    struct ifreq ifr;
	    const char *iface = "eth0";
	    char *mac = 0;
	
	    fd = socket(AF_INET, SOCK_DGRAM, 0);
	    ifr.ifr_addr.sa_family = AF_INET;
	    strncpy((char *)ifr.ifr_name , (const char *)iface , IFNAMSIZ-1);
	    ioctl(fd, SIOCGIFHWADDR, &ifr);
	    close(fd);
	
	    mac = (char *)ifr.ifr_hwaddr.sa_data;

        // Convert MAC bytes to hexagonal with fixed width
        std::stringstream ss;
        for(int i = 0; i < outSize; ++i) {
            ss << std::setw(2) << std::setfill('0') << std::hex << (int)abs(mac[i]);
        }
        return ss.str();
    }

    // Just key setter.
    int SetPublicKey(std::string publicKey) {
        _PublicKey = publicKey;
        return 0;
    }

    // Sets Central Server host address.
    // Also sets same host name to Online Cash Register, which is located on the same server as Central.
    int SetHostAddress(std::string hostName) {
        _Host = hostName;
        _OnlineCashRegister = hostName;
        return 0;
    }

    // Just host name getter.
    std::string GetHostName() {
        return _Host;
    }

    // Returns Central Server IP address.
    std::string GetCentralServerAddress() {
        std::string serverIP = "";
        int res = -1;

	    printf("Looking for Central Wash server ...\n");
        res = this->SearchCentralServer(serverIP);
        
        if (res == 0) {
            printf("Server located on: %s\n", serverIP.c_str());
        } else {
            printf("Failed: no server found...\n");
		    serverIP = "";
	    }
        
        return serverIP;
    }

    // GetStationConig request to specified URL with method POST. 
    // Returns 0, if request was OK, other value - in case of failure.
    int GetStationConig(std::string& answer) {
	    std::string url = _Host+ _Port + "/station-program-by-hash";
        
        int result;
        std::string json_ping_request = json_create_card_reader_config();
        result = SendRequest(&json_ping_request, &answer, url);
        if (result) {
            return 1;
        }
        return 0;
    }

    // RunProgramOnServer request to specified URL with method POST. 
    // Returns 0, if request was OK, other value - in case of failure.
    int RunProgramOnServer(int programID, int preflight) {
	    std::string url = _Host+ _Port + "/run-program";
        std::string answer;

        int result;
        std::string json_run_program_request = json_create_run_program(programID, preflight);
        result = SendRequest(&json_run_program_request, &answer, url);
        if ((result) || (answer !="")) {
            fprintf(stderr, "RunProgramOnServer answer %s\n", answer.c_str());
            return 1;
        }
        return 0;
    }

    // GetCardReaderConig request to specified URL with method POST. 
    // Returns 0, if request was OK, other value - in case of failure.
    int GetCardReaderConig(std::string& cardReaderType, std::string& host, std::string& port) {
        std::string answer;
	    std::string url = _Host+ _Port + "/card-reader-config-by-hash";
        
        int result;
        std::string json_ping_request = json_create_card_reader_config();
        result = SendRequest(&json_ping_request, &answer, url);
	
        if (result) {
            return 1;
        }
	
        json_t *object;
        json_error_t error;

        int err = 0;
        object = json_loads(answer.c_str(), 0, &error);
        printf("GetCardReaderConig answer %s\n", answer.c_str());
        do {
            if (!object) {
                printf("Error in GetCardReaderConig: %d: %s\n", error.line, error.text );
                err = 1;
                break;
            }

            if(!json_is_object(object)) {
		        printf("Not a JSON\n");
                break;
            }

            json_t *obj_card_reader_type;
            obj_card_reader_type = json_object_get(object, "cardReaderType");
            cardReaderType = json_string_value(obj_card_reader_type);

            json_t *obj_host;
            obj_host = json_object_get(object, "host");
            if (obj_host) {
                host = json_string_value(obj_host);
            }

            json_t *obj_port;
            obj_port = json_object_get(object, "port");
            if (obj_port) {
                port = json_string_value(obj_port);
            }
        } while (0);
        json_decref(object);
        return err;
    }

    // PING request to specified URL with method GET. 
    // Returns 0, if request was OK, other value - in case of failure.
    int SendPingRequestGet(std::string url) {
        std::string answer;
	    url += _Port + "/ping";
        printf("trying %s ...\n", url.c_str());
        return SendRequestGet(&answer, url, 200);
    }

    // PING request to specified URL with method POST. 
    // Returns 0, if request was OK, other value - in case of failure.
    // Modifies service money, if server returned that kind of data.
    int SendPingRequest(int& service_money, bool& open_station, int& button_id, int balance, int program, int& lastUpdate) {
        std::string answer;
	    std::string url = _Host + _Port + "/ping";
        
        int result;
        std::string json_ping_request = json_create_ping_report(balance, program);
        result = SendRequest(&json_ping_request, &answer, url);
	//printf("Server answer on PING:\n%s\n", answer.c_str());
	
        if (result == 2) {
            return 3;
        }
        if (result) {
            return 1;
        }
	
        json_t *object;
        json_error_t error;

        int err = 0;
        object = json_loads(answer.c_str(), 0, &error);
        do {
            if (!object) {
                printf("Error in PING: %d: %s\n", error.line, error.text );
                err = 1;
                break;
            }

            if(!json_is_object(object)) {
		printf("Not a JSON\n");
                break;
            }

            json_t *obj_service_amount;
            obj_service_amount = json_object_get(object, "serviceAmount");
            service_money = (int)json_integer_value(obj_service_amount);

            json_t *obj_open_station;
            obj_open_station = json_object_get(object, "openStation");
            open_station = (bool)json_boolean_value(obj_open_station);

            json_t *obj_button_id;
            obj_button_id = json_object_get(object, "ButtonID");
            button_id = (int)json_integer_value(obj_button_id);
            
            json_t *obj_last_update;
            obj_last_update = json_object_get(object, "lastUpdate");
            lastUpdate = (int)json_integer_value(obj_last_update);            
        } while (0);
        json_decref(object);
        return err;
    }


    // Adds a receipt to a queue.
    int ReceiptRequest(int postPosition, int cash, int electronical) {
        if ((cash + electronical) >0) {
            ReceiptToSend * incomingReceipt = new ReceiptToSend(postPosition, cash, electronical);
            receipts_channel->Push(incomingReceipt);
        }
        return 0;
    }

    // Base function for receipt sending to Online Cash Register.
    int SendReceiptRequest(int postPosition, int cash, int electronical) {
        CURL *curl;
        CURLcode res;

        curl = curl_easy_init();
        if (curl == NULL) {
            return 1;
        }

        std::string reqUrl;	    
        reqUrl = "https://" + _OnlineCashRegister + ":8443/";
        reqUrl += "V2/" + std::to_string(postPosition) + "/" + std::to_string(cash) + "/" + std::to_string(electronical);

        curl_easy_setopt(curl, CURLOPT_URL, reqUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "diae/0.1");
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
	        printf("%s", curl_easy_strerror(res));
	        printf("\n");
            curl_easy_cleanup(curl);
            return SERVER_UNAVAILABLE;
        }

        curl_easy_cleanup(curl);
        return 0;
    }

    // Encodes money report data and sends it to Central Server via SAVE request.
    int SendMoneyReport(int cars_total, int coins_total, int banknotes_total, int cashless_total, int service_total) {
        money_report_t money_report_data = {0,0,0,0,0};
        
        money_report_data.cars_total = cars_total;
        money_report_data.coins_total = coins_total;
        money_report_data.banknotes_total = banknotes_total;
        money_report_data.cashless_total = cashless_total;
        money_report_data.service_total = service_total;

	    printf("Sending money report...\n");
        // Encode data to JSON
        std::string json_money_report_request = json_create_money_report(&money_report_data);
	    printf("JSON:\n%s\n", json_money_report_request.c_str());
        // Send a request
        CreateAndPushEntry(json_money_report_request, "/save-money");
        return 0;
    }

    // Encodes relay report data and sends it to Central Server via SAVE request.
    int SendRelayReport(struct RelayStat RelayStats[MAX_RELAY_NUM]) {
        relay_report_t relay_report_data={{{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}}};

        for(int i = 0; i < MAX_RELAY_NUM; i++) {
            relay_report_data.RelayStats[i].switched_count = RelayStats[i].switched_count;
            relay_report_data.RelayStats[i].total_time_on = RelayStats[i].total_time_on;
        }
	printf("Sending relay report...\n");
        // Encode data to JSON
        std::string json_relay_report_request = json_create_relay_report(&relay_report_data);
	printf("JSON:\n%s\n", json_relay_report_request.c_str());
        // Send a request
        CreateAndPushEntry(json_relay_report_request, "/save-relay");
        return 0;
    }

    // Sends LOAD request to Central Server and decodes JSON result to money report.
    int GetLastMoneyReport(money_report_t *money_report_data) {
        std::string answer;
        
        // Encode LOAD request to JSON
        std::string json_get_last_money_report_request = json_get_last_money_report();

        // Send request to Central Server
	std::string url = _Host + _Port + "/load-money";
        int res = SendRequest(&json_get_last_money_report_request, &answer, url);
        
        if (res > 0) {
            printf("No connection to server\n");
            return 1;
        }

        json_t *object;
        json_error_t error;
        object = json_loads(answer.c_str(), 0, &error);
        int err = 0;

	printf("Server returned for Get Last Money: \n%s\n", answer.c_str());
        do {
            if (!object) {
                printf("Error in get_last_money_report on line %d: %s\n", error.line, error.text );
                err = 1;
                break;
            }

            if(!json_is_object(object)) {
                err = 1;
                break;
            }
            
            json_t* obj_var = json_object_get(object, "carsTotal");
            if(!json_is_integer(obj_var)) {
                money_report_data->cars_total = 0;
            } else
                money_report_data->cars_total = json_integer_value(obj_var);

            obj_var = json_object_get(object, "coins");
            if(!json_is_integer(obj_var)) {
                money_report_data->coins_total = 0;
            } else
                money_report_data->coins_total = json_integer_value(obj_var);

            obj_var = json_object_get(object, "banknotes");
            if(!json_is_integer(obj_var)) {
                money_report_data->banknotes_total = 0;
            } else
            	money_report_data->banknotes_total = json_integer_value(obj_var);

            obj_var = json_object_get(object, "electronical");
            if(!json_is_integer(obj_var)) {
                money_report_data->cashless_total = 0;
            } else
            	money_report_data->cashless_total = json_integer_value(obj_var);

            obj_var = json_object_get(object, "service");
            if(!json_is_integer(obj_var)) {
                money_report_data->service_total = 0;
            } else
            	money_report_data->service_total = json_integer_value(obj_var);
        } while(0);
        json_decref(object);
        return err;
    }

    // Sends LOAD request to Central Server and decodes JSON result to relay report.
    int GetLastRelayReport(relay_report_t *relay_report_data) {
        std::string answer;
        
        // Encode LOAD request to JSON
        std::string json_get_last_relay_report_request = json_get_last_relay_report();

        // Send request to Central Server
	std::string url = _Host + _Port + "/load-relay";
        int res = SendRequest(&json_get_last_relay_report_request, &answer, url);
        
        if (res > 0) {
            printf("No connection to server\n");
            return 1;
        }

        fprintf(stderr,"%s\n",answer.c_str());

        json_t *object;
        json_error_t error;
        object = json_loads(answer.c_str(), 0, &error);
        int err = 0;

        do {
            if (!object) {
                printf("Error in get_last_relay_report on line %d: %s\n", error.line, error.text );
                err = 1;
                break;
            }

            if(!json_is_object(object)) {
		printf("JSON is not an object\n");
                err = 1;
                break;
            }

            json_t *obj_array;
            obj_array = json_object_get(object, "relayStats");
            if(!json_is_array(obj_array)) {
                err = 1;
                break;
            }

            json_t *element;
            json_t *obj_var;
            int i, relay_id, switched_count, total_time_on;

            json_array_foreach(obj_array, i, element) {
                obj_var = json_object_get(element, "relayID" );
                if(!json_is_integer(obj_var)) {
		    printf("relayId problem\n");
		    err = 1;
		    break;
                }
                relay_id = json_integer_value(obj_var);
            
                obj_var = json_object_get(element, "switchedCount" );
                if(!json_is_integer(obj_var)) {
		    switched_count = 0;
                } else
                    switched_count = json_integer_value(obj_var);

                obj_var = json_object_get(element, "totalTimeOn" );
                if(!json_is_integer(obj_var)) {
		    total_time_on = 0;
                } else
                    total_time_on = json_integer_value(obj_var);
                relay_report_data->RelayStats[relay_id-1].switched_count = switched_count;
                relay_report_data->RelayStats[relay_id-1].total_time_on = total_time_on;
            }
        } while(0);

        json_decref(object);
        return err;
    }

    // Sends SAVE IF NOT EXISTS request to Central Server and decodes JSON result to value string.
    // Gets key and value strings.
    std::string SetRegistryValueByKeyIfNotExists(std::string key, std::string value) {
        std::string answer;
        std::string result = "";

        // Encode SAVE request to JSON with key string
        std::string set_registry_value = json_set_registry_value(key, value);
        printf("JSON:\n%s\n", set_registry_value.c_str());

        // Send request to Central Server
        std::string url = _Host + _Port + "/save-if-not-exists";
        int res = SendRequest(&set_registry_value, &answer, url);

        printf("Server answer: \n%s\n", answer.c_str());

        if (res > 0) {
            printf("No connection to server\n");
        }
        return result;
    }


    // Sends SAVE request to Central Server and decodes JSON result to value string.
    // Gets key and value strings.
    std::string SetRegistryValueByKey(std::string key, std::string value) {
        std::string answer;
        std::string result = "";

        // Encode SAVE request to JSON with key string
        std::string set_registry_value = json_set_registry_value(key, value);
        printf("JSON:\n%s\n", set_registry_value.c_str());

        // Send request to Central Server
        std::string url = _Host + _Port + "/save";
        int res = SendRequest(&set_registry_value, &answer, url);

        printf("Server answer: \n%s\n", answer.c_str());

        if (res > 0) {
            printf("No connection to server\n");
        }
        return result;
    }

    // Sends LOAD request to Central Server and decodes JSON result to value string.
    // Gets key string.
    std::string GetRegistryValueByKey(std::string key) {
        std::string answer;
        std::string result = "";

        // Encode LOAD request to JSON with key string
        std::string get_registry_value = json_get_registry_value(key);
	printf("JSON:\n%s\n", get_registry_value.c_str());

        // Send request to Central Server
	std::string url = _Host + _Port + "/load";
        int res = SendRequest(&get_registry_value, &answer, url);
        
	printf("Server answer: %s\n", answer.c_str());

        if (res > 0) {
            printf("No connection to server\n");
        } else {
	    if (answer.length() > 3) result = answer.substr(1, answer.length()-3);
	}
        return result;
    }

    std::string GetRegistryValueFromStationByKey(int stationID, std::string key){
        std::string answer;
        std::string result = "";

        // Encode LOAD-FROM-STATION request to JSON with key string
        std::string get_registry_value_from_station = json_get_registry_value_from_station(stationID, key);
    printf("JSON:\n%s\n",get_registry_value_from_station.c_str());

         // Send request to Central Server
	std::string url = _Host + _Port + "/load-from-station";
        int res = SendRequest(&get_registry_value_from_station, &answer, url);
        
	printf("Server answer: %s\n", answer.c_str());

        if (res > 0) {
            printf("No connection to server\n");
        } else {
	    if (answer.length() > 3) result = answer.substr(1, answer.length()-3);
	}

        return result;
    }

// Sends request to Central Server and return station id.
    std::string GetStationID() {
        std::string answer;
        std::string result = "";

        std::string get_public_key = json_get_public_key();
	    printf("GetStationID JSON:\n%s\n", get_public_key.c_str());

        // Send request to Central Server
	    std::string url = _Host + _Port + "/station-by-hash";
            int res = SendRequest(&get_public_key, &answer, url);
        
	    printf("Server answer: %s\n", answer.c_str());

        if (res > 0) {
            printf("No connection to server\n");
        } else {
	        if (answer != "") result = answer;
	    }
        return result;
    }
private:
    int interrupted = 0;
    std::string _PublicKey;
    std::string _OnlineCashRegister;
    std::string _Host;
    std::string _Port;

    DiaChannel<NetworkMessage> channel;
    pthread_t entry_processing_thread;
    pthread_t receipts_processing_thread;
    pthread_mutex_t nfct_entries_mutex = PTHREAD_MUTEX_INITIALIZER;

    // Thread, which tries to send reports to Central Server.
    static void *process_extract(void *arg) {
        DiaNetwork *Dia = (DiaNetwork*) arg;

        while(!Dia->interrupted) {
            int res = Dia->PopAndSend();
            if (res == SERVER_UNAVAILABLE) {
                sleep(5);
                printf("Server's unavailable ... \n");
            }
        }
        pthread_exit(NULL);
        return NULL;
    }

    // Thread, which tries to send reports to Central Server.
    static void *process_receipts(void *arg) {
        DiaNetwork *Dia = (DiaNetwork*) arg;

        while(!Dia->interrupted) {
            int res = Dia->PopAndSendReceipt();
            if (res == SERVER_UNAVAILABLE) {
                sleep(5);
                printf("Kasses's server unavailable... \n");
            }
        }
        pthread_exit(NULL);
        return NULL;
    }

    int PopAndSendReceipt() {
        ReceiptToSend * extractedReceipt;
        int err = receipts_channel->Peek(&extractedReceipt);        

        if (err) {
            //CHANNEL_BUFFER_EMPTY IS THE ONLY ERR
            sleep(1);
            return 0;
        }

        int res = SendReceiptRequest(extractedReceipt->PostPosition, extractedReceipt->Cash, extractedReceipt->Electronical);

        if (res > 0) {
            printf("No connection to server\n");
            return SERVER_UNAVAILABLE;
        } else {
            receipts_channel->DropOne();
        }

        return 0;
    }

    // Interrupt the report processing thread. 
    int StopTheWorld() {
        interrupted = 1;
        return 0;
    }

    // Pop message from channel (queue) and send it to the Central Server. 
    int PopAndSend() {
        NetworkMessage * message;
        int err = channel.Peek(&message);

        if (err) {
            //CHANNEL_BUFFER_EMPTY IS THE ONLY ERR
            sleep(1);
            return 0;
        }

        std::string answer;
        std::string url = _Host + _Port + message->route;

        int res = SendRequest(&(message->json_request), &answer, url);

        if (res > 0) {
            printf("No connection to server\n");
            return SERVER_UNAVAILABLE;
        } else {
            channel.DropOne();
        }

        return 0;
    }

    // Add new message (report) to the channel. Thread will pop it in the future.
    int CreateAndPushEntry(std::string json_string, std::string route) {
        NetworkMessage * entry = new NetworkMessage();
        if(!entry) {
            return 1;
        }

        entry->stime = time(NULL);
        entry->json_request = json_string;
	entry->route = route;

        if (channel.Push(entry) == CHANNEL_BUFFER_OVERFLOW) {
            printf("CHANNEL BUFFER OVERFLOW\n");
            return 1;
        }
        else {
            return 0;
        }
    }

    // Encodes _PublicKey to JSON string.
    std::string json_create_ping_report(int balance, int program) {
        json_t *object = json_object();

        json_object_set_new(object, "Hash", json_string(_PublicKey.c_str()));
        json_object_set_new(object, "CurrentBalance", json_integer(balance));
        json_object_set_new(object, "CurrentProgram", json_integer(program));
        char *str = json_dumps(object, 0);
        std::string res = str;

        free(str);
        str = 0;
        json_decref(object);
        return res;
    }

    std::string json_create_card_reader_config() {
        json_t *object = json_object();

        json_object_set_new(object, "Hash", json_string(_PublicKey.c_str()));
        char *str = json_dumps(object, 0);
        std::string res = str;

        free(str);
        str = 0;
        json_decref(object);
        return res;
    }

    std::string json_create_run_program(int programID, int preflight) {
        json_t *object = json_object();

        json_object_set_new(object, "hash", json_string(_PublicKey.c_str()));
        json_object_set_new(object, "programID", json_integer(programID));
        json_object_set_new(object, "preflight", json_boolean(preflight));
        char *str = json_dumps(object, 0);
        std::string res = str;

        free(str);
        str = 0;
        json_decref(object);
        return res;
    }

    // Encodes money report struct to JSON string. 
    std::string json_create_money_report(struct money_report *s) {
        json_t *object = json_object();

        json_object_set_new(object, "Hash", json_string(_PublicKey.c_str()));
        json_object_set_new(object, "Banknotes", json_integer(s->banknotes_total));
        json_object_set_new(object, "CarsTotal", json_integer(s->cars_total));
        json_object_set_new(object, "Coins", json_integer(s->coins_total));
        json_object_set_new(object, "Electronical",json_integer(s->cashless_total));
        json_object_set_new(object, "Service",json_integer(s->service_total));
    
        char *str = json_dumps(object, 0);
        std::string res = str;
        free(str);
        str = 0;
        json_decref(object);
        return res;
    }

    // Encodes relay report struct to JSON string.
    std::string json_create_relay_report(struct relay_report *s) {
        json_t *object = json_object();
        json_t *relayarr = json_array();
        json_t *relayobj[MAX_RELAY_NUM];

        json_object_set_new(object, "Hash", json_string(_PublicKey.c_str()));

        for(int i = 0; i < MAX_RELAY_NUM; i++) {
            relayobj[i] = json_object();
            json_object_set_new(relayobj[i], "RelayID", json_integer(i+1));
            json_object_set_new(relayobj[i], "SwitchedCount", json_integer(s->RelayStats[i].switched_count));
            json_object_set_new(relayobj[i], "TotalTimeOn", json_integer(s->RelayStats[i].total_time_on));
            json_array_append_new(relayarr, relayobj[i]);
        }

        json_object_set_new(object, "RelayStats", relayarr);
        char *str = json_dumps(object, 0);
        std::string res = str;
        free(str);
        str = 0;
        json_decref(object);
        return res;
    }

    // Encodes key and value to JSON string.
    std::string json_set_registry_value(std::string key, std::string value) {
        json_t *object = json_object();
	json_t *keypair = json_object();

        json_object_set_new(object, "Hash", json_string(_PublicKey.c_str()));
        json_object_set_new(keypair, "Key", json_string(key.c_str()));
	json_object_set_new(keypair, "Value", json_string(value.c_str()));
	json_object_set_new(object, "KeyPair", keypair);

        char *str = json_dumps(object, 0);
        std::string res = str;

        free(str);
        str = 0;
        json_decref(object);
        return res;
    }

    // Encodes key to JSON string.
    std::string json_get_registry_value(std::string key) {
        json_t *object = json_object();

        json_object_set_new(object, "Hash", json_string(_PublicKey.c_str()));
        json_object_set_new(object, "Key", json_string(key.c_str()));

        char *str = json_dumps(object, 0);
        std::string res = str;

        free(str);
        str = 0;
        json_decref(object);
        return res;
    }
    
    std::string json_get_registry_value_from_station(int stationID, std::string key){
        json_t *object = json_object();

        json_object_set_new(object, "Hash", json_string(_PublicKey.c_str()));
        json_object_set_new(object, "StationID", json_integer(stationID));
        json_object_set_new(object, "Key", json_string(key.c_str()));

        char *str = json_dumps(object, 0);
        std::string res = str;

        free(str);
        str = 0;
        json_decref(object);
        return res;
    }

    // Encode PublicKey to JSON string.
    std::string json_get_public_key() {
        json_t *object = json_object();

        json_object_set_new(object, "Hash", json_string(_PublicKey.c_str()));

        char *str = json_dumps(object, 0);
        std::string res = str;

        free(str);
        str = 0;
        json_decref(object);
        return res;
    }

    // Encodes empty money report to JSON string.
    std::string json_get_last_money_report() {
        json_t *object = json_object();

        json_object_set_new(object, "Hash", json_string(_PublicKey.c_str()));
        json_object_set_new(object, "Banknotes", json_integer(1));
        json_object_set_new(object, "CarsTotal", json_integer(1));
        json_object_set_new(object, "Coins", json_integer(1));
        json_object_set_new(object, "Electronical",json_integer(1));
        json_object_set_new(object, "Service",json_integer(1));
    
        char *str = json_dumps(object, 0);
        std::string res = str;
        free(str);
        str = 0;
        json_decref(object);
        return res;
    }

    // Encodes empty relay report to JSON string.
    std::string json_get_last_relay_report() {
        json_t *object = json_object();
        json_t *relayarr = json_array();
        json_t *relayobj[MAX_RELAY_NUM];

        json_object_set_new(object, "Hash", json_string(_PublicKey.c_str()));

        for(int i = 0; i < MAX_RELAY_NUM; i++) {
            relayobj[i] = json_object();
            json_object_set_new(relayobj[i], "RelayID", json_integer(1));
            json_object_set_new(relayobj[i], "SwitchedCount", json_integer(1));
            json_object_set_new(relayobj[i], "TotalTimeOn", json_integer(1));
            json_array_append_new(relayarr, relayobj[i]);
        }

        json_object_set_new(object, "RelayStats", relayarr);
        char *str = json_dumps(object, 0);
        std::string res = str;
        free(str);
        str = 0;
        json_decref(object);
        return res;
    }

    static size_t _Writefunc(void *ptr, size_t size, size_t nmemb, curl_answer_t *answer) {
        assert(answer);
         size_t new_len = answer->length + size*nmemb;
        answer->data = (char*)realloc(answer->data, new_len+1);
        assert(answer->data);
        memcpy(answer->data+answer->length, ptr, size*nmemb);
        answer->data[new_len] = '\0';
        answer->length = new_len;

        return size*nmemb;
    }

    void InitCurlAnswer(curl_answer_t * raw_answer) {
        raw_answer->data = (char *) malloc(1);
        raw_answer->length = 0;
        raw_answer->data[0] = 0;
    }

    void DestructCurlAnswer(curl_answer_t * raw_answer) {
        free(raw_answer->data);
    }
};

#endif
