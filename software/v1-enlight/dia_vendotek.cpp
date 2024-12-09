// Copyright 2020, Roman Fedyashov

#include "./dia_vendotek.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <jansson.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <string>

#include "./dia_device.h"
#include "./money_types.h"
#include <assert.h>
#include "./vendotek/vendotek.h"

typedef struct stage_req_s {
    uint16_t  id;
    char     *valstr;
    long     *valint;
} stage_req_t;

typedef struct stage_resp_s {
    uint16_t  id;
    char     *valstr;
    long     *valint;
    char     *expstr;
    long     *expint;
    int       optional;
} stage_resp_t;

std::string convertToJSON(stage_req_t* array, size_t size) {
    json_t *list = json_array();

    for (size_t i = 0; i < size; ++i) {
        stage_req_t req = array[i];

        json_t *json_obj = json_object();

        json_object_set_new(json_obj, "id", json_integer(req.id));
        if (req.valstr != nullptr) {
            json_object_set_new(json_obj, "valstr", json_string(req.valstr));
        }
        if (req.valint != nullptr) {
            json_object_set_new(json_obj, "valint", json_integer(*req.valint));
        }

        json_array_append_new(list, json_obj);
    }

    char *json_cstr = json_dumps(list, 0);
    std::string json_str(json_cstr);

    free(json_cstr);
    json_decref(list);

    return json_str;
}

std::string convertToJSON(stage_resp_s* array, size_t size) {
    json_t *list = json_array();

    for (size_t i = 0; i < size; ++i) {
        stage_resp_s req = array[i];

        json_t *json_obj = json_object();

        json_object_set_new(json_obj, "id", json_integer(req.id));
        if (req.valstr != nullptr) {
            json_object_set_new(json_obj, "valstr", json_string(req.valstr));
        }
        if (req.valint != nullptr) {
            json_object_set_new(json_obj, "valint", json_integer(*req.valint));
        }
        if (req.expstr != nullptr) {
            json_object_set_new(json_obj, "expstr", json_integer(*req.expstr));
        }
        if (req.expint != nullptr) {
            json_object_set_new(json_obj, "expint", json_integer(*req.expint));
        }
        json_object_set_new(json_obj, "optional", json_integer(req.optional));

        json_array_append_new(list, json_obj);
    }

    char *json_cstr = json_dumps(list, 0);
    std::string json_str(json_cstr);

    free(json_cstr);
    json_decref(list);

    return json_str;
}

std::string strReqResp(stage_req_t* arrayReq, size_t sizeReq, stage_resp_s* arrayResp, size_t sizeResp) {
    return "req = " + convertToJSON(arrayReq, sizeReq) + ", resp = " + convertToJSON(arrayResp, sizeResp);
}

typedef struct stage_opts_s {
    vtk_t     *vtk;
    vtk_msg_t *mreq;
    vtk_msg_t *mresp;
    int        timeout;  /* poll timeout, ms */
    int        verbose;
    int        allow_eof;
} stage_opts_t;

typedef struct payment_s {
        long    opnum;
        long    evnum;
        char    *evname;
        long    prodid;
        char    *prodname;
        long    *host;
        long    price;
        long    price_confirmed;
        long    timeout;
    } payment_t;

int do_stage(stage_opts_t *opts, stage_req_t *req, stage_resp_t *resp)
{
    /*
     * fill & send request message
     */
    char  valbuf[0xff];
    char *value;
    vtk_msg_mod(opts->mreq, VTK_MSG_RESET, VTK_BASE_VMC, 0, NULL);

    for (int i = 0; req[i].id; i++) {
        if (req[i].valint) {
            snprintf(valbuf, sizeof(valbuf), "%ld", *req[i].valint);
            value = valbuf;
        } else if (req[i].valstr) {
            value = req[i].valstr;
        } else {
            continue;
        }
        vtk_msg_mod(opts->mreq, VTK_MSG_ADDSTR, req[i].id, 0, value);
    }
    if (opts->verbose) {
        vtk_msg_print(opts->mreq);
    }
    if (vtk_net_send(opts->vtk, opts->mreq) < 0) {
        return -1;
    }

    if (resp == NULL) {
        return 0;
    }

    /*
     * wait & validate response
     */
    struct pollfd pollfd = {
        .fd     = vtk_net_get_socket(opts->vtk),
        .events = POLLIN
    };
    int rpoll = poll(&pollfd, 1, opts->timeout);

    if (rpoll < 0) {
        vtk_loge("POS connection error: %s", strerror(errno));
        return -1;
    } else if (rpoll == 0) {
        vtk_loge("POS connection timeout");
        return -1;
    }
    int fleof = 0;
    if (vtk_net_recv(opts->vtk, opts->mresp, &fleof) < 0) {
        vtk_loge("Expected event can't be received/validated");
        return -1;
    }
    if (opts->verbose) {
        vtk_msg_print(opts->mresp);
    }
    for (int i = 0; resp[i].id; i++) {
        char    *valstr = NULL;
        long int valint = 0;
        int      idfound = vtk_msg_find_param(opts->mresp, resp[i].id, NULL, &valstr) >= 0;
        int      vsfound = valstr != NULL;
        int      vifound = vsfound && (sscanf(valstr, "%ld", &valint) == 1);

        if (!idfound && !resp[i].optional) {
            vtk_loge("Expected message parameter wasn't found: 0x%x (%s)", resp[i].id, vtk_msg_stringify(resp[i].id));
            return -1;
        } else  if (!idfound) {
            continue;
        }
        if (resp[i].valstr && vsfound) {
            resp[i].valstr = valstr;
        }
        if (resp[i].valint && vifound) {
            *resp[i].valint = valint;
        }
        if (resp[i].expstr && ! (vsfound && strcasecmp(resp[i].expstr, valstr) == 0)) {
            vtk_loge("Wrong string parameter. id: 0x%x, returned: %s, expected: %s",
                     resp[i].id, valstr, resp[i].expstr);
            return -1;
        }
        if (resp[i].expint && ! (vifound && (*resp[i].expint == valint))) {
            vtk_loge("Wrong numeric parameter. id: 0x%x, returned: %lld, expected: %lld",
                     resp[i].id, valint, *resp[i].expint);
            return -1;
        }
    }

    if (! opts->allow_eof && fleof) {
        vtk_loge("Connection with POS was closed unexpectedly");
        return -1;
    }

    return 0;
}


int do_payment(void * driverPtr, payment_opts_t *opts, VendotekStage key)
{
    vtk_loge("Vendotek executes program thread...\n");
    if (!driverPtr) {
         vtk_loge("%s", "Vendotek driver is empty. Panic!\n");
    }

    DiaVendotek * driver = reinterpret_cast<DiaVendotek *>(driverPtr);
    int state = 0;

    driver->logger->AddLog("Do payment: money = " + TO_STR(opts->price) + ", key = " + TO_STR(key), DIA_VENDOTEK_LOG_TYPE);
    
    /*
     * common state
     */ 
    payment_t payment;
    payment.evnum     = opts->evnum;
    payment.evname    = opts->evname;
    payment.prodid    = opts->prodid;
    payment.prodname  = opts->prodname;
    payment.price     = opts->price;
    payment.host      = opts->host;
    payment.timeout   = opts->timeout;
     
    stage_opts_t stopts;
        stopts.vtk     = opts->vtk;
        stopts.timeout = opts->timeout * 1000;
        stopts.verbose = opts->verbose;
        stopts.mreq    = opts->mreq;
        stopts.mresp   = opts->mresp;
        stopts.allow_eof = 0;

    int result = 0;
    switch (key)
    {
        case VendotekStage::RC_IDL_VRP:
            {
                int rc_idl = 0, rc_vrp = 0;

                pthread_mutex_lock(&driver->StateLock);
                state = driver->PaymentStage;
                if (state > 0) {
                    driver->PaymentStage = 2;
                }
                pthread_mutex_unlock(&driver->StateLock);

                if (state >0) {
                
                    vtk_logi("IDL Init stage");
                    stage_req_t idl1_req[7] = {};
                    idl1_req[0].id = 0x1;
                    idl1_req[0].valstr = (char*)"IDL";
                    idl1_req[1].id = 0x8;
                    idl1_req[1].valint =  payment.evname   ? &payment.evnum : NULL;
                    idl1_req[2].id = 0x7;
                    idl1_req[2].valstr =  payment.evname;
                    idl1_req[3].id = 0x9;
                    idl1_req[3].valint =  payment.prodname ? &payment.prodid : NULL;
                    idl1_req[4].id = 0xf;
                    idl1_req[4].valstr =  payment.prodname;
                    idl1_req[5].id = 0x4;
                    idl1_req[5].valint = &payment.price;
                    idl1_req[6].id =0;

                    stage_resp_t idl1_resp[5] = {};
                    idl1_resp[0].id = 0x1;
                    idl1_resp[0].expstr = (char*)"IDL";
                    idl1_resp[1].id = 0x3;
                    idl1_resp[1].valint = &payment.opnum;
                    idl1_resp[2].id = 0x6;
                    idl1_resp[2].valint = &payment.timeout;
                    idl1_resp[4].id = 0;

                    driver->logger->AddLog("Do payment RC_IDL_VRP: state = idl, " + strReqResp(idl1_req, 7, idl1_resp, 5), DIA_VENDOTEK_LOG_TYPE);

                    rc_idl = do_stage(&stopts, idl1_req, idl1_resp) >= 0;

                    driver->logger->AddLog("Do payment RC_IDL_VRP: state = idl, rc_idl = " + TO_STR(rc_idl), DIA_VENDOTEK_LOG_TYPE, rc_idl ? LogLevel::Info : LogLevel::Error);
                }

                /*
                 * 2 stage, VRP
                 */
                pthread_mutex_lock(&driver->StateLock);
                state = driver->PaymentStage;
                if (state > 0) {
                    driver->PaymentStage = 3;
                }
                pthread_mutex_unlock(&driver->StateLock);

                if (rc_idl && (state>0)) {
                
                    vtk_logi("VRP stage");

                    stopts.timeout = payment.timeout * 1000;
                    payment.opnum++;
                    stage_req_t vrp_req[6] = {};
                    vrp_req[0].id = 0x1;
                    vrp_req[0].valstr = (char*)"VRP";
                    vrp_req[1].id = 0x3;
                    vrp_req[1].valint = &payment.opnum;
                    vrp_req[2].id = 0x9;
                    vrp_req[2].valint =  payment.prodname ? &payment.prodid : NULL;
                    vrp_req[3].id = 0xf;
                    vrp_req[3].valstr =  payment.prodname;
                    vrp_req[4].id = 0x4;
                    vrp_req[4].valint = &payment.price;
                    vrp_req[5].id = 0;

                    stage_resp_t vrp_resp[4] = {};
                    vrp_resp[0].id = 0x1;
                    vrp_resp[0].expstr = (char*)"VRP";
                    vrp_resp[1].id = 0x3;
                    vrp_resp[1].expint = &payment.opnum;
                    vrp_resp[2].id = 0x4;
                    vrp_resp[2].expint = &payment.price;
                    vrp_resp[3].id = 0;
                    vtk_logi("timeout %d", stopts.timeout);

                    driver->logger->AddLog("Do payment RC_IDL_VRP: state = vrp, " + strReqResp(vrp_req, 6, vrp_resp, 4), DIA_VENDOTEK_LOG_TYPE);

                    rc_vrp = do_stage(&stopts, vrp_req, vrp_resp) >= 0;

                    driver->logger->AddLog("Do payment RC_IDL_VRP: state = vrp, rc_vrp = " + TO_STR(rc_vrp), DIA_VENDOTEK_LOG_TYPE, rc_vrp ? LogLevel::Info : LogLevel::Error);
                }
                result = ! (rc_idl && rc_vrp) ? -1 : 0;
                break;
            }        

        case VendotekStage::RC_FIN_IDL_END:
            {
                int rc_fin = 0, rc_idl = 0, rc_idl2 = 0;
                pthread_mutex_lock(&driver->StateLock);
                state = driver->PaymentStage;
                if (state > 0) {
                    driver->PaymentStage = 4;
                }
                pthread_mutex_unlock(&driver->StateLock);

                if (state>0) {
                    stage_req_t idl1_req[2] = {};
                    idl1_req[0].id = 0x1;
                    idl1_req[0].valstr = (char*)"IDL";
                    idl1_req[1].id = 0;

                    stage_resp_t idl1_resp[5] = {};
                    idl1_resp[0].id = 0x1;
                    idl1_resp[0].expstr = (char*)"IDL";
                    idl1_resp[1].id = 0x3;
                    idl1_resp[1].valint = &payment.opnum;
                    idl1_resp[2].id = 0x6;
                    idl1_resp[2].valint = &payment.timeout;
                    idl1_resp[3].id = 0x8;
                    idl1_resp[3].valint = &payment.evnum;
                    idl1_resp[4].id = 0;

                    driver->logger->AddLog("Do payment RC_FIN_IDL_END: state = idl, " + strReqResp(idl1_req, 2, idl1_resp, 5), DIA_VENDOTEK_LOG_TYPE);

                    rc_idl = do_stage(&stopts, idl1_req, idl1_resp) >= 0;

                    driver->logger->AddLog("Do payment RC_FIN_IDL_END: state = idl, rc_vrp = " + TO_STR(rc_idl), DIA_VENDOTEK_LOG_TYPE, rc_idl ? LogLevel::Info : LogLevel::Error);
                
                    vtk_logi("FIN stage");

                    stopts.allow_eof = 1;
                    stage_req_t fin_req[5] = {};
                    fin_req[0].id = 0x1;
                    fin_req[0].valstr = (char*)"FIN";
                    fin_req[1].id = 0x3;
                    fin_req[1].valint = &payment.opnum;
                    fin_req[2].id = 0x9;
                    fin_req[2].valint =  payment.prodname ? &payment.prodid : NULL;
                    fin_req[3].id = 0x4;
                    fin_req[3].valint = &payment.price;
                    fin_req[4].id = 0;

                    stage_resp_t fin_resp[4] = {};
                    fin_resp[0].id = 0x1;
                    fin_resp[0].expstr = (char*)"FIN";
                    fin_resp[1].id = 0x3;
                    fin_resp[1].expint = &payment.opnum;
                    fin_resp[2].id = 0x4;
                    fin_resp[2].expint = &payment.price;
                    fin_resp[3].id = 0;

                    driver->logger->AddLog("Do payment RC_FIN_IDL_END: state = fin, " + strReqResp(fin_req, 5, fin_resp, 4), DIA_VENDOTEK_LOG_TYPE);

                    rc_fin = do_stage(&stopts, fin_req, fin_resp) >= 0;

                    driver->logger->AddLog("Do payment RC_FIN_IDL_END: state = fin, rc_fin = " + TO_STR(rc_fin), DIA_VENDOTEK_LOG_TYPE, rc_fin ? LogLevel::Info : LogLevel::Error);
                }

                /*
                 * 4 stage, IDL 2, always
                 */
                vtk_logi("IDL Fini stage");
                stage_req_t idl2_req[2] = {};
                idl2_req[0].id = 0x1;
                idl2_req[0].valstr = (char*)"IDL";
                idl2_req[1].id = 0;

                stage_resp_t idl2_resp[2] = {};
                idl2_resp[0].id = 0x1;
                idl2_resp[0].expstr = (char*)"IDL";
                idl2_resp[1].id = 0;

                driver->logger->AddLog("Do payment RC_FIN_IDL_END: state = idl2, " + strReqResp(idl2_req, 2, idl2_resp, 2), DIA_VENDOTEK_LOG_TYPE);

                rc_idl2 = do_stage(&stopts, idl2_req, idl2_resp) >= 0;

                driver->logger->AddLog("Do payment ALL: state = idl2, rc_idl2 = " + TO_STR(rc_idl2), DIA_VENDOTEK_LOG_TYPE, rc_idl2 ? LogLevel::Info : LogLevel::Error);

                pthread_mutex_lock(&driver->StateLock);
                driver->PaymentStage = 0;
                pthread_mutex_unlock(&driver->StateLock);
                
                result = ! rc_fin ? -1 : 0;
                break;
            }

        case VendotekStage::ALL:
            {
                int rc_idl = 0, rc_vrp = 0, rc_fin = 0, rc_idl2 = 0;

                pthread_mutex_lock(&driver->StateLock);
                state = driver->PaymentStage;
                if (state > 0) {
                    driver->PaymentStage = 2;
                }
                pthread_mutex_unlock(&driver->StateLock);

                if (state >0) {
                    vtk_logi("IDL Init stage");
                    stage_req_t idl1_req[7] = {};
                    idl1_req[0].id = 0x1;
                    idl1_req[0].valstr = (char*)"IDL";
                    idl1_req[1].id = 0x8;
                    idl1_req[1].valint =  payment.evname   ? &payment.evnum : NULL;
                    idl1_req[2].id = 0x7;
                    idl1_req[2].valstr =  payment.evname;
                    idl1_req[3].id = 0x9;
                    idl1_req[3].valint =  payment.prodname ? &payment.prodid : NULL;
                    idl1_req[4].id = 0xf;
                    idl1_req[4].valstr =  payment.prodname;
                    idl1_req[5].id = 0x4;
                    idl1_req[5].valint = &payment.price;
                    idl1_req[6].id =0;

                    stage_resp_t idl1_resp[5] = {};
                    idl1_resp[0].id = 0x1;
                    idl1_resp[0].expstr = (char*)"IDL";
                    idl1_resp[1].id = 0x3;
                    idl1_resp[1].valint = &payment.opnum;
                    idl1_resp[2].id = 0x6;
                    idl1_resp[2].valint = &payment.timeout;
                    idl1_resp[3].id = 0x8;
                    idl1_resp[3].valint = &payment.evnum;
                    idl1_resp[4].id = 0;

                    driver->logger->AddLog("Do payment ALL: state = idl, " + strReqResp(idl1_req, 7, idl1_resp, 5), DIA_VENDOTEK_LOG_TYPE);

                    rc_idl = do_stage(&stopts, idl1_req, idl1_resp) >= 0;

                    driver->logger->AddLog("Do payment ALL: state = idl, rc_idl = " + TO_STR(rc_idl), DIA_VENDOTEK_LOG_TYPE, rc_idl ? LogLevel::Info : LogLevel::Error);
                }

                /*
                 * 2 stage, VRP
                 */
                pthread_mutex_lock(&driver->StateLock);
                state = driver->PaymentStage;
                if (state > 0) {
                    driver->PaymentStage = 3;
                }
                pthread_mutex_unlock(&driver->StateLock);

                if (rc_idl && (state>0)) {
                    vtk_logi("VRP stage");

                    stopts.timeout = payment.timeout * 1000;
                    payment.opnum++;
                    stage_req_t vrp_req[7] = {};
                    vrp_req[0].id = 0x1;
                    vrp_req[0].valstr = (char*)"VRP";
                    vrp_req[1].id = 0x3;
                    vrp_req[1].valint = &payment.opnum;
                    vrp_req[2].id = 0x9;
                    vrp_req[2].valint =  payment.prodname ? &payment.prodid : NULL;
                    vrp_req[3].id = 0xf;
                    vrp_req[3].valstr =  payment.prodname;
                    vrp_req[4].id = 0x4;
                    vrp_req[4].valint = &payment.price;
                    vrp_req[5].id = 0x19;
                    vrp_req[5].valint = payment.host;
                    vrp_req[6].id = 0;

                    stage_resp_t vrp_resp[4] = {};
                    vrp_resp[0].id = 0x1;
                    vrp_resp[0].expstr = (char*)"VRP";
                    vrp_resp[1].id = 0x3;
                    vrp_resp[1].expint = &payment.opnum;
                    vrp_resp[2].id = 0x4;
                    vrp_resp[2].expint = &payment.price;
                    vrp_resp[3].id = 0;
                    vtk_logi("timeout %d", stopts.timeout);

                    driver->logger->AddLog("Do payment ALL: state = vrp, " + strReqResp(vrp_req, 6, vrp_resp, 4), DIA_VENDOTEK_LOG_TYPE);
                    
                    rc_vrp = do_stage(&stopts, vrp_req, vrp_resp) >= 0;

                    driver->logger->AddLog("Do payment ALL: state = vrp, rc_vrp = " + TO_STR(rc_vrp), DIA_VENDOTEK_LOG_TYPE, rc_vrp ? LogLevel::Info : LogLevel::Error);
                }

                /*
                 * 3 stage, FIN
                 */
                pthread_mutex_lock(&driver->StateLock);
                state = driver->PaymentStage;
                if (state > 0) {
                    driver->PaymentStage = 4;
                }
                pthread_mutex_unlock(&driver->StateLock);

                if (rc_vrp && (state>0)) {
                    vtk_logi("FIN stage");

                    stopts.allow_eof = 1;
                    stage_req_t fin_req[5] = {};
                    fin_req[0].id = 0x1;
                    fin_req[0].valstr = (char*)"FIN";
                    fin_req[1].id = 0x3;
                    fin_req[1].valint = &payment.opnum;
                    fin_req[2].id = 0x9;
                    fin_req[2].valint =  payment.prodname ? &payment.prodid : NULL;
                    fin_req[3].id = 0x4;
                    fin_req[3].valint = &payment.price;
                    fin_req[4].id = 0;

                    stage_resp_t fin_resp[4] = {};
                    fin_resp[0].id = 0x1;
                    fin_resp[0].expstr = (char*)"FIN";
                    fin_resp[1].id = 0x3;
                    fin_resp[1].expint = &payment.opnum;
                    fin_resp[2].id = 0x4;
                    fin_resp[2].expint = &payment.price;
                    fin_resp[3].id = 0;

                    driver->logger->AddLog("Do payment ALL: state = fin, " + strReqResp(fin_req, 5, fin_resp, 4), DIA_VENDOTEK_LOG_TYPE);

                    rc_fin = do_stage(&stopts, fin_req, fin_resp) >= 0;

                    driver->logger->AddLog("Do payment ALL: state = fin, rc_fin = " + TO_STR(rc_fin), DIA_VENDOTEK_LOG_TYPE, rc_fin ? LogLevel::Info : LogLevel::Error);
                }

                /*
                 * 4 stage, IDL 2, always
                 */
                vtk_logi("IDL Fini stage");
                stage_req_t idl2_req[2] = {};
                idl2_req[0].id = 0x1;
                idl2_req[0].valstr = (char*)"IDL";
                idl2_req[1].id = 0;

                stage_resp_t idl2_resp[2] = {};
                idl2_resp[0].id = 0x1;
                idl2_resp[0].expstr = (char*)"IDL";
                idl2_resp[1].id = 0;

                driver->logger->AddLog("Do payment ALL: state = idl2, " + strReqResp(idl2_req, 2, idl2_resp, 2), DIA_VENDOTEK_LOG_TYPE);

                rc_idl2 = do_stage(&stopts, idl2_req, idl2_resp) >= 0;

                driver->logger->AddLog("Do payment ALL: state = idl2, rc_idl2 = " + TO_STR(rc_idl2), DIA_VENDOTEK_LOG_TYPE, rc_idl2 ? LogLevel::Info : LogLevel::Error);

                pthread_mutex_lock(&driver->StateLock);
                driver->PaymentStage = 0;
                pthread_mutex_unlock(&driver->StateLock);

                result = ! (rc_idl && rc_vrp && rc_fin) ? -1 : 0;
                break;
            }
        
        default:
            {
                result = -1;
            }
    }

    driver->logger->AddLog("Do payment: result = " + TO_STR(result), DIA_VENDOTEK_LOG_TYPE, result == 0 ? LogLevel::Info : LogLevel::Error);

    return result;
}



int do_ping(payment_opts_t *opts)
{
    vtk_logi("Ping");
    stage_opts_t stopts;
    stopts.vtk     = opts->vtk;
    stopts.timeout = opts->timeout * 1000;
    stopts.verbose = opts->verbose;
    stopts.mreq    = opts->mreq;
    stopts.mresp   = opts->mresp;
    stopts.allow_eof = 0;

    stage_req_t idl_req[2] = {};
    idl_req[0].id = 0x1;
    idl_req[0].valstr = (char*)"IDL";
    idl_req[1].id = 0;

    stage_resp_t idl_resp[2] = {};
    idl_resp[0].id = 0x1;
    idl_resp[0].expstr = (char*)"IDL";
    idl_resp[1].id = 0;

    if (do_stage(&stopts, idl_req, idl_resp) < 0) {
        return -1;
    }
    return 0;
}

int do_abort(payment_opts_t *opts)
{
    vtk_logi("Abort");
    stage_opts_t stopts;
    stopts.vtk     = opts->vtk;
    stopts.timeout = 1000;
    stopts.verbose = opts->verbose;
    stopts.mreq    = opts->mreq;
    stopts.mresp   = opts->mresp;
    stopts.allow_eof = 0;

    stage_req_t abr_req[2] = {};
    abr_req[0].id = 0x1;
    abr_req[0].valstr = (char*)"ABR";
    abr_req[1].id = 0;

    if (do_stage(&stopts, abr_req, NULL) < 0) {
        return -1;
    }
    return 0;
}

// Task thread
// Reads requested money amount and tries to call an executable,
// which works with card reader hardware
// If success - uses callback to report money
// If fails then sets requested money to 0 and stoppes
void * DiaVendotek_ExecuteDriverProgramThread(void * driverPtr) {
    vtk_logi("Card reader executes program thread...\n");
    if (!driverPtr) {
         vtk_loge("%s", "Card reader driver is empty. Panic!\n");
    }

    DiaVendotek * driver = reinterpret_cast<DiaVendotek *>(driverPtr);
    pthread_mutex_lock(&driver->MoneyLock);
    int sum = driver->RequestedMoney;
    pthread_mutex_unlock(&driver->MoneyLock);

    vtk_logi("reader request %d RUB...", sum);

    vtk_logd("isSeparated %d", driver->IsTransactionSeparated);

    driver->logger->AddLog("Execute driver program thread: money = " + TO_STR(sum) + ", separated" + TO_STR(driver->IsTransactionSeparated), DIA_VENDOTEK_LOG_TYPE);

    payment_opts_t popts;
    popts.timeout   = 2;
    popts.verbose   = LOG_DEBUG;
    popts.price     = sum * 100;
    popts.prodname  = (char*)"";
    popts.prodid    = 0;
    popts.evnum     = 0;
    popts.evname    = (char*)"";
    popts.host      = nullptr;

    static long hostSbp = 101;
    if (driver->IsSBP) {
        popts.host = &hostSbp;
    }

    int rcode = 0;

    vtk_logd("host %s, port %s", driver->Host, driver->Port);

    pthread_mutex_lock(&driver->OperationLock);

    vtk_init(&popts.vtk);

    rcode = vtk_net_set(popts.vtk, VTK_NET_CONNECTED, popts.timeout * 1000, (char*)driver->Host.c_str(), (char*)driver->Port.c_str());

    driver->logger->AddLog("Execute driver program thread: rcode = " + TO_STR(rcode), DIA_VENDOTEK_LOG_TYPE, rcode == 0 ? LogLevel::Info : LogLevel::Error);

    pthread_mutex_lock(&driver->StateLock);
    driver->_PaymentOpts = &popts;
    pthread_mutex_unlock(&driver->StateLock);

    if (rcode >= 0) {
        vtk_msg_init(&popts.mreq,  popts.vtk);
        vtk_msg_init(&popts.mresp, popts.vtk);

        if(!driver->IsTransactionSeparated){
            
            rcode = do_payment(driverPtr, &popts, VendotekStage::ALL);

        }
        else{
            rcode = do_payment(driverPtr, &popts, VendotekStage::RC_IDL_VRP);   
        }

        vtk_msg_free(popts.mreq);
        vtk_msg_free(popts.mresp);

    }
    
    vtk_free(popts.vtk);
 
    pthread_mutex_lock(&driver->StateLock);
    driver->_PaymentOpts = NULL;
    pthread_mutex_unlock(&driver->StateLock);

    pthread_mutex_unlock(&driver->OperationLock);

    vtk_logi("Card reader returned status code: %d", rcode);

    driver->logger->AddLog("Execute driver program thread: rcode after do payment = " + TO_STR(rcode), DIA_VENDOTEK_LOG_TYPE, rcode == 0 ? LogLevel::Info : LogLevel::Error);

    if (rcode == 0) {

        int moneytype = DIA_ELECTRON;
        if(driver->IsSBP) {
            moneytype = DIA_SBP;
        }

        if(!driver->IsTransactionSeparated){
            if (driver->IncomingMoneyHandler != NULL) {

                pthread_mutex_lock(&driver->MoneyLock);
                int sum = driver->RequestedMoney;
                driver->IncomingMoneyHandler(driver->_Manager, moneytype, sum);
                driver->RequestedMoney = 0;
                pthread_mutex_unlock(&driver->MoneyLock);

                vtk_logi("Reported money: %d", sum);

                driver->logger->AddLog("Execute driver program thread: reported money = " + TO_STR(sum), DIA_VENDOTEK_LOG_TYPE, LogLevel::Debug);

            } else {
                vtk_loge("No handler to report: %d", sum);
            }
        }else{
            if (driver->IncomingMoneyHandler != NULL) {

                pthread_mutex_lock(&driver->MoneyLock);
                int sum = driver->RequestedMoney;
                driver->IncomingMoneyHandler(driver->_Manager, moneytype, sum);
                pthread_mutex_unlock(&driver->MoneyLock);

                vtk_logi("Reported money: %d", sum);

                driver->logger->AddLog("Execute driver program thread: reported money = " + TO_STR(sum), DIA_VENDOTEK_LOG_TYPE, LogLevel::Debug);
            } 
            else {
                vtk_loge("No handler to report: %d", sum);
            }
        }
        
    } 
    else {

        pthread_mutex_lock(&driver->MoneyLock);
        driver->RequestedMoney = 0;
        pthread_mutex_unlock(&driver->MoneyLock);

    }
    pthread_exit(NULL);
    
    return NULL;
}


void * DiaVendotek_ExecutePaymentConfirmationDriverProgramThread(void *driverPtr){
    vtk_logi("Vendotek executes program thread...\n");
    if (!driverPtr) {
         vtk_loge("%s", "Card reader driver is empty. Panic!\n");
    }

    DiaVendotek * driver = reinterpret_cast<DiaVendotek *>(driverPtr);

    driver->logger->AddLog("Execute payment confirmation driver program thread: separated = " + TO_STR(driver->IsTransactionSeparated), DIA_VENDOTEK_LOG_TYPE);

    if(!driver->IsTransactionSeparated)
        return NULL;
        
    pthread_mutex_lock(&driver->MoneyLock);
    int sum = driver->RequestedMoney ;
    pthread_mutex_unlock(&driver->MoneyLock);

    vtk_logi("reader request %d RUB...", sum);
    payment_opts_t popts;
    popts.timeout   = 2;
    popts.verbose   = LOG_DEBUG;
    popts.price     = sum * 100;
    popts.prodname  = (char*)"";
    popts.prodid    = 0;
    popts.evnum     = 0;
    popts.evname    = (char*)"";

    int rcode = 0;
    vtk_logd("host %s, port %s", driver->Host, driver->Port);
    pthread_mutex_lock(&driver->OperationLock);
    vtk_init(&popts.vtk);
    rcode = vtk_net_set(popts.vtk, VTK_NET_CONNECTED, popts.timeout * 1000, (char*)driver->Host.c_str(), (char*)driver->Port.c_str());
    pthread_mutex_lock(&driver->StateLock);
    driver->_PaymentOpts = &popts;
    pthread_mutex_unlock(&driver->StateLock);

    driver->logger->AddLog("Execute payment confirmation driver program thread: rcode = " + TO_STR(rcode), DIA_VENDOTEK_LOG_TYPE, rcode == 0 ? LogLevel::Info : LogLevel::Error);

    if (rcode >= 0) {
        vtk_msg_init(&popts.mreq,  popts.vtk);
        vtk_msg_init(&popts.mresp, popts.vtk);

        rcode = do_payment(driverPtr, &popts, VendotekStage::RC_FIN_IDL_END);
    
        driver->_PaymentOpts = NULL;

        vtk_msg_free(popts.mreq);
        vtk_msg_free(popts.mresp);
    }
    vtk_free(popts.vtk);
    pthread_mutex_lock(&driver->StateLock);
    driver->_PaymentOpts = NULL;
    pthread_mutex_unlock(&driver->StateLock);
    pthread_mutex_unlock(&driver->OperationLock);


    vtk_logi("Card reader returned status code: %d", rcode);

    driver->logger->AddLog("Execute payment confirmation driver program thread: rcode after do payment = " + TO_STR(rcode), DIA_VENDOTEK_LOG_TYPE, rcode == 0 ? LogLevel::Info : LogLevel::Error);
 
    pthread_mutex_lock(&driver->MoneyLock);
    driver->RequestedMoney = 0;
    pthread_mutex_unlock(&driver->MoneyLock);


    pthread_exit(NULL);
    return NULL;
}

int DiaVendotek_Ping(void * driverPtr) {
    vtk_logi("Card reader executes program ping...");
    if (!driverPtr) {
         vtk_loge("%s", "Card reader driver is empty. Panic!");
    }

    DiaVendotek * driver = reinterpret_cast<DiaVendotek *>(driverPtr);

    payment_opts_t popts;
    popts.timeout   = 2;
    popts.verbose   = LOG_DEBUG;
    popts.ping = 1;
    popts.prodname = (char*)"";
    popts.prodid = 0;
    popts.evnum = 0;
    popts.evname = (char*)"";
    int rcode = 0;
    vtk_logd("host %s, port %s", driver->Host.c_str(), driver->Port.c_str());
    pthread_mutex_lock(&driver->OperationLock);
    vtk_init(&popts.vtk);
    rcode = vtk_net_set(popts.vtk, VTK_NET_CONNECTED, popts.timeout * 1000, (char*)driver->Host.c_str(), (char*)driver->Port.c_str());

    if (rcode >= 0) {
        vtk_msg_init(&popts.mreq,  popts.vtk);
        vtk_msg_init(&popts.mresp, popts.vtk);

        rcode = do_ping(&popts);

        vtk_msg_free(popts.mreq);
        vtk_msg_free(popts.mresp);
    }
    vtk_free(popts.vtk);
    pthread_mutex_unlock(&driver->OperationLock);
    
    pthread_mutex_lock(&driver->StateLock);
    if (rcode == 0) {
        driver->Available = 1;
    } else {
        driver->Available = 0;
    }
    pthread_mutex_unlock(&driver->StateLock);

    return rcode;
}


void * DiaVendotek_ExecutePingThread(void * driverPtr) {
    DiaVendotek * driver = reinterpret_cast<DiaVendotek *>(driverPtr);

    int status = 0;
    int oldStatus = DiaVendotek_GetAvailableStatus(driver);

    while (driver->ToBeDeleted == 0) {
        int res = DiaVendotek_Ping(driverPtr);
        status = DiaVendotek_GetAvailableStatus(driver);
        
        vtk_logi("DiaVendotek available=%d", status);

        if (oldStatus != status) {
            driver->logger->AddLog("Execute ping thread: available = " + TO_STR(status), DIA_VENDOTEK_LOG_TYPE, status ? LogLevel::Info : LogLevel::Warning);
        }

        oldStatus = status;

        if (res==0) {
            delay(120000);
        } else {
            delay(1000);
        }
    }

    pthread_exit(NULL);
    return NULL;
}

int DiaVendotek_StartPing(void * specificDriver) {
    DiaVendotek * driver = reinterpret_cast<DiaVendotek *>(specificDriver);

    if (specificDriver == NULL) {
        vtk_loge("DiaVendotek got NULL driver");
        return DIA_VENDOTEK_NULL_PARAMETER;
    }
    vtk_logline_set(NULL, LOG_DEBUG);
    int err = pthread_create(&driver->ExecutePingThread,
        NULL,
        DiaVendotek_ExecutePingThread,
        driver);
    if (err != 0) {
        vtk_loge("can't create thread :[%s]", strerror(err));
        return 5;
    }
    return DIA_VENDOTEK_NO_ERROR;
}


// Entry point function
// Creates task thread with requested parameter (money amount) and exits
int DiaVendotek_PerformTransaction(void * specificDriver, int money, bool isTrasactionSeparated, bool isSBP) {
    DiaVendotek * driver = reinterpret_cast<DiaVendotek *>(specificDriver);

    if (specificDriver == NULL || money == 0) {
        vtk_loge("DiaVendotek Perform Transaction got NULL driver");
        return DIA_VENDOTEK_NULL_PARAMETER;
    }

    driver->logger->AddLog("Perform transaction: money = " + TO_STR(money) + ", separated = " + TO_STR(isTrasactionSeparated), DIA_VENDOTEK_LOG_TYPE);

    pthread_mutex_lock(&driver->StateLock);
    driver->PaymentStage = 1;
    pthread_mutex_unlock(&driver->StateLock);

    vtk_logi("DiaVendotek started Perform Transaction, money = %d, isSeparated = %d, isSBP = %d", money, isTrasactionSeparated, isSBP);
    pthread_mutex_lock(&driver->MoneyLock);
    driver->RequestedMoney = money;
    driver->IsTransactionSeparated = isTrasactionSeparated;
    driver->IsSBP = isSBP;
    pthread_mutex_unlock(&driver->MoneyLock);

    int err = pthread_create(&driver->ExecuteDriverProgramThread,
        NULL,
        DiaVendotek_ExecuteDriverProgramThread,
        driver);
    if (err != 0) {
        vtk_loge("can't create thread :[%s]", strerror(err));
        return 1;
    }
    return DIA_VENDOTEK_NO_ERROR;
}

int DiaVendotek_ConfirmTransaction(void * specificDriver, int money){
    DiaVendotek * driver = reinterpret_cast<DiaVendotek *>(specificDriver);
    if (specificDriver == NULL) {
        vtk_loge("DiaVendotek Confirm Transaction got NULL driver");
        return DIA_VENDOTEK_NULL_PARAMETER;
    }

    vtk_logi("DiaVendotek started Confirm Transaction, money = %d", money);

    driver->logger->AddLog("Confirm transaction: money = " + TO_STR(money), DIA_VENDOTEK_LOG_TYPE);

    pthread_mutex_lock(&driver->MoneyLock);
    vtk_logi("DiaVendotek started Confirm Transaction, RequestedMoney = %d", driver->RequestedMoney);
    int remains = driver->RequestedMoney - money;
    pthread_mutex_unlock(&driver->MoneyLock);

    vtk_logi("DiaVendotek Confirm Transaction, remains = %d", remains);

    driver->logger->AddLog("Confirm transaction: remains = " + TO_STR(remains), DIA_VENDOTEK_LOG_TYPE);

    if (remains < 0){
        pthread_mutex_lock(&driver->MoneyLock);
        remains = money - driver->RequestedMoney;
        driver->RequestedMoney = 0;
        pthread_mutex_unlock(&driver->MoneyLock);
        
    }
    else{
        remains = 0;
        pthread_mutex_lock(&driver->MoneyLock);
        driver->RequestedMoney -= money;
        pthread_mutex_unlock(&driver->MoneyLock);
    }
    
    int err = pthread_create(&driver->ExecutePaymentConfirmationDriverProgramThread,
        NULL,
        DiaVendotek_ExecutePaymentConfirmationDriverProgramThread,
        driver);
    if (err != 0) {
        vtk_loge("can't create thread :[%s]", strerror(err));
        return 0;
    }
    return remains;
}


// Inner function for task thread destroy
int DiaVendotek_StopDriver(void * specificDriver) {
    if (specificDriver == NULL) {
        vtk_loge("DiaVendotek Stop Driver got NULL driver");
        return DIA_VENDOTEK_NULL_PARAMETER;
    }
    DiaVendotek * driver = reinterpret_cast<DiaVendotek *>(specificDriver);

    pthread_mutex_lock(&driver->StateLock);
    int stage = driver->PaymentStage;

    driver->logger->AddLog("Abort transaction: state = " + TO_STR(stage), DIA_VENDOTEK_LOG_TYPE);

    if (stage <4) { // FIN
        driver->PaymentStage = 0;
    }
    if ((stage ==0)||(stage >3)) {
        pthread_mutex_unlock(&driver->StateLock);

        driver->logger->AddLog("Abort transaction: failure ", DIA_VENDOTEK_LOG_TYPE, LogLevel::Error);

        pthread_join(driver->ExecuteDriverProgramThread, NULL);
        vtk_logi("Vendotek thread killed");

        return -1;
    }
    vtk_logi("Start Stop Driver");
    if (driver->_PaymentOpts) {
        int result = do_abort(driver->_PaymentOpts);
        
        driver->logger->AddLog("Abort transaction: result = " + TO_STR(result), DIA_VENDOTEK_LOG_TYPE);
    }
    pthread_mutex_unlock(&driver->StateLock);

    pthread_mutex_lock(&driver->MoneyLock);
    driver->RequestedMoney = 0;
    pthread_mutex_unlock(&driver->MoneyLock);

    pthread_join(driver->ExecuteDriverProgramThread, NULL);
    vtk_logi("Vendotek thread killed");

    driver->logger->AddLog("Abort transaction: success", DIA_VENDOTEK_LOG_TYPE, LogLevel::Debug);

    return DIA_VENDOTEK_NO_ERROR;
}

// API function for task thread destory
void DiaVendotek_AbortTransaction(void * specificDriver) {
    if (specificDriver == NULL) {
        vtk_logi("DiaVendotek Abort Transaction got NULL driver");
        return;
    }
    DiaVendotek_StopDriver(specificDriver);
}

// Get task thread status function
// If returned number > 0, thread expects money amount == result number
// If returned 0, then thread is not working (destroyed or not created)
// If returned -1, then error occurred during call
int DiaVendotek_GetTransactionStatus(void * specificDriver) {
    if (specificDriver == NULL) {
        vtk_logi("DiaVendotek Get Transaction Status got NULL driver");
        return -1;
    }

    DiaVendotek * driver = reinterpret_cast<DiaVendotek *>(specificDriver);

    pthread_mutex_lock(&driver->MoneyLock);
    int sum = driver->RequestedMoney;
    pthread_mutex_unlock(&driver->MoneyLock);
    return sum;
}

int DiaVendotek_GetAvailableStatus(void * specificDriver) {
    if (specificDriver == NULL) {
        vtk_logi("DiaVendotek Get Transaction Status got NULL driver");
        return -1;
    }

    DiaVendotek * driver = reinterpret_cast<DiaVendotek *>(specificDriver);

    pthread_mutex_lock(&driver->StateLock);
    int status = driver->Available;
    pthread_mutex_unlock(&driver->StateLock);

    return status;
}
