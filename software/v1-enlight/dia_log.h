#ifndef DIA_LOG_H
#define DIA_LOG_H

#include "dia_network.h"
#include <iostream>
#include <queue>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

enum class LogLevel {Debug, Info, Warning, Error};

struct Log {
    std::string text;
    std::string type;
    LogLevel level;

    Log(std::string text, std::string type = "", LogLevel level = LogLevel::Info) {
        this->text = text;
        this->type = type;
        this->level = level;
    }
};

class Logger {
    private:

        std::queue<Log> queue;
        pthread_mutex_t mtx;
        pthread_cond_t cv;
        bool stop;
        pthread_t worker;
        DiaNetwork *network = nullptr;

        static void* processQueue(void* arg) {
            Logger* logger = static_cast<Logger*>(arg);
            logger->processQueueImpl();
            return nullptr;
        }

        static std::string logLevel(LogLevel level) {
            switch (level) {
            case LogLevel::Debug:
                return "debug";
            case LogLevel::Info:
                return "info";
            case LogLevel::Warning:
                return "warning";
            case LogLevel::Error:
                return "error";
            default:
                return "info";
            }
        }

        void processQueueImpl() {
            int result = 0;

            while (true) {
                pthread_mutex_lock(&mtx);
                while (queue.empty() && !stop) {
                    pthread_cond_wait(&cv, &mtx);
                }

                if (stop && queue.empty()) {
                    pthread_mutex_unlock(&mtx);
                    break;
                }

                while (!queue.empty()) {
                    Log msg = queue.front();
                    queue.pop();
                    pthread_mutex_unlock(&mtx);

                    for (int i = 0; i < 4; i++) {
                        result = network->AddLog(msg.text, msg.type, logLevel(msg.level));

                        if (result == 0) {
                            break;
                        }
                        if (result == 3) {
                            printf("Failed to save log (text: %s, type: %s): server connection error\n", msg.text.c_str(), msg.type.c_str());
                        }
                    }

                    pthread_mutex_lock(&mtx);
                }

                pthread_mutex_unlock(&mtx);
            }
        }

    public:

        Logger(DiaNetwork *network) : stop(false) {
            pthread_mutex_init(&mtx, nullptr);
            pthread_cond_init(&cv, nullptr);
            pthread_create(&worker, nullptr, &Logger::processQueue, this);
            this->network = network;
        }

        ~Logger() {
            pthread_mutex_lock(&mtx);
            stop = true;
            pthread_mutex_unlock(&mtx);
            pthread_cond_signal(&cv);
            pthread_join(worker, nullptr);
            pthread_mutex_destroy(&mtx);
            pthread_cond_destroy(&cv);
        }

        void AddLog(const std::string& text, const std::string& type = "", const LogLevel& level = LogLevel::Info) {
            pthread_mutex_lock(&mtx);
            queue.push({text, type, level});
            pthread_mutex_unlock(&mtx);
            pthread_cond_signal(&cv);
        }
};

#endif
