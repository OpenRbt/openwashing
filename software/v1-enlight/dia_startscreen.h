#include <string>

enum STARTUP_MESSAGE {MAC, POST, LOCAL_IP, SERVER_IP,CARD_READER, VENDOTEK_INFO, CONFIGURATION,SETTINGS,RELAY_CONTROL_BOARD, DISPLAY_INFO};

int StartScreenInit(std::string path);
void StartScreenDrawBase();
void StartScreenDrawMessages();
void StartScreenDrawVertical();
void StartScreenDrawHorizontal();
void StartScreenUpdate();
void StartScreenUpdateIP();
void StartScreenMessage(STARTUP_MESSAGE type, std::string msg);
void StartScreenShutdown();
