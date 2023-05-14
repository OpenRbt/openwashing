#ifndef _DIA_FIRMWARE_H
#define _DIA_FIRMWARE_H

#define DIA_REQUEST_RETRY_ATTEMPTS 50

#include "dia_network.h"

std::string SetRegistryValueByKeyIfNotExists(std::string key, std::string value);

#endif
