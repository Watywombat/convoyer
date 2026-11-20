#pragma once
#include "conveyor.hpp"
#include <esp_err.h>
struct Config {
 char ssid[33],password[65],url[192],token[128];
 int speed, timeoutMs, watchdogS,warehouse[3];
};
struct Parcel {char ref[conveyor::maxRef+1]; int product, exit, observed; char transaction[33];};
bool readConfig(Config&);
esp_err_t writeJournal(conveyor::State,const Parcel&);
bool pendingJournal(Parcel* saved=nullptr);
esp_err_t installImage(const char*);
esp_err_t importConfig();
