constexpr int beltPin=SITE_PWM_MOTEUR, servoPin=SITE_PWM_SERVO, nfcRx=SITE_RX;
constexpr int entryPin=SITE_ENTREE, readerPin=SITE_POSTE_NFC, stopSense=SITE_ARRET;
constexpr int gatePins[]={SITE_POSITION_1,SITE_POSITION_2,SITE_POSITION_3};
constexpr int exitPins[]={SITE_SORTIE_1,SITE_SORTIE_2,SITE_SORTIE_3};
constexpr int reconcilePin=SITE_CLE_REPRISE,maintenancePin=SITE_CLE_MAINTENANCE;
constexpr int sdMosi=SITE_MOSI,sdMiso=SITE_MISO,sdClock=SITE_SCLK,sdCs=SITE_CS;
constexpr int inputs[]={reconcilePin,entryPin,readerPin,stopSense,gatePins[0],gatePins[1],gatePins[2],exitPins[0],exitPins[1],exitPins[2]};
constexpr int servoDuty[]={SITE_PWM_1,SITE_PWM_2,SITE_PWM_3};
constexpr int nfcBaud=SITE_DEBIT;
