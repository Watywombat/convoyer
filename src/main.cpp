#include "board.hpp"
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_netif_sntp.h>
#include <esp_event.h>
#include <esp_timer.h>
#include <esp_task_wdt.h>
#include <esp_http_server.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <cJSON.h>
#include <cstdio>
#include <cstring>
#include <esp_random.h>
#include <esp_system.h>
using namespace conveyor;
extern Event wms(const Config&,State,Parcel&);
namespace {
constexpr int pollMs=20, milliseconds=1000;
constexpr unsigned stackController=6144,stackNetwork=16384;
Config cfg{};QueueHandle_t requests,replies;
struct Job {State state;Parcel parcel;Event result;uint32_t seq;};
TaskHandle_t fsmTask=nullptr,wmsTask=nullptr;
struct Health {char last[maxRef+1];Error error;int64_t when;State state;};
Health metrics{};portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;char hash[65];
esp_err_t health(httpd_req_t*r){
 portENTER_CRITICAL(&lock);auto m=metrics;portEXIT_CRITICAL(&lock);
 unsigned fsmFree=fsmTask?uxTaskGetStackHighWaterMark(fsmTask):0;
 unsigned wmsFree=wmsTask?uxTaskGetStackHighWaterMark(wmsTask):0;
 char body[480];snprintf(body,sizeof body,"{\"version\":\"1.1.0-poc\",\"sha256\":\"%s\",\"uptime_s\":%lld,\"last_ref\":\"%s\",\"last_error\":%d,\"last_error_uptime_ms\":%lld,\"state\":%d,\"fsm_stack_free_min\":%u,\"wms_stack_free_min\":%u}",hash,(long long)(esp_timer_get_time()/1000000),m.last,int(m.error),(long long)m.when,int(m.state),fsmFree,wmsFree);
 httpd_resp_set_type(r,"application/json");return httpd_resp_sendstr(r,body);
}
void wifiEvent(void*,esp_event_base_t,int32_t id,void*){
 if(id==WIFI_EVENT_STA_START||id==WIFI_EVENT_STA_DISCONNECTED)ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
}
void network(void*){
 Job j;for(;;){xQueueReceive(requests,&j,portMAX_DELAY);j.result=wms(cfg,j.state,j.parcel);
 xQueueSend(replies,&j,portMAX_DELAY);}
}
bool remote(State s){return s==State::Lookup||s==State::Capacity||s==State::Commit||s==State::Report;}
void controller(void*){
 ESP_ERROR_CHECK(esp_task_wdt_add(nullptr));
 State s=State::Boot;Parcel p{};uint32_t seq=0;int64_t deadline=0;
 bool hw=board_init();
 board_stop();
 bool unfinished=pendingJournal(&p);
 if(unfinished)ESP_LOGW("RECOVERY","ref=%s mouvement=%s sortie=%d",p.ref,p.transaction,p.exit);
 Event ev=(!hw||unfinished)?Event::Pending:Event::Ready;bool available=true;bool outstanding=false;
 esp_ota_img_states_t imageState;
 if(esp_ota_get_state_partition(esp_ota_get_running_partition(),&imageState)==ESP_OK&&imageState==ESP_OTA_IMG_PENDING_VERIFY){
  if(!hw)ESP_ERROR_CHECK(esp_ota_mark_app_invalid_rollback_and_reboot());
  ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
 }
 for(;;){
 ESP_ERROR_CHECK(esp_task_wdt_reset());
 if(!available){
 Job j{};
 bool received=xQueueReceive(replies,&j,0);if(received)outstanding=false;
 if(hw&&board_poll(s,ev,p)){available=ev!=Event::Cleared||!outstanding;}
 else if(received&&j.seq==seq&&j.state==s){p=j.parcel;ev=j.result;available=true;}
 else if(deadline&&esp_timer_get_time()>=deadline){ev=Event::Timeout;available=true;}
 }
 if(available){
 auto r=step(s,ev);available=false;
 if(r.accepted){
 if(r.next!=s)board_stop();
 if(ev==Event::Cleared&&writeJournal(State::Idle,p)!=ESP_OK)r={State::Recovery,Error::Interrupted,true};
 if(s==State::Boot&&ev==Event::Ready&&(!hw||pendingJournal()))r=step(s,Event::Pending);
 if(s==State::Idle&&ev==Event::Arrival){
 p=Parcel{};uint32_t words[4];esp_fill_random(words,sizeof words);
 for(int i=0;i<4;++i)snprintf(p.transaction+i*8,9,"%08x",(unsigned)words[i]);
 }
 bool done=s==State::Verify&&ev==Event::CorrectExit;
 // Preserve ambiguous evidence until an operator has reconciled the journal.
 if(r.next!=State::Boot&&r.next!=State::Recovery&&writeJournal(r.next,p)!=ESP_OK){
 r={State::Recovery,Error::Interrupted,true};done=false;
 }
 s=r.next;++seq;
 if(r.error!=Error::None)ESP_LOGW("FSM","erreur=%d ref=%s mouvement=%s",int(r.error),p.ref,p.transaction);
 portENTER_CRITICAL(&lock);metrics.state=s;
 if(done)memcpy(metrics.last,p.ref,sizeof metrics.last);
 if(r.error!=Error::None){metrics.error=r.error;metrics.when=esp_timer_get_time()/1000;}
 portEXIT_CRITICAL(&lock);
 deadline=0;
 if(s==State::Recovery||s==State::Hold||s==State::ConfigFault){board_stop();}
 else if(s==State::Boot){ev=(!hw||pendingJournal())?Event::Pending:Event::Ready;available=true;}
 else if(remote(s)){
 board_stop();Job j{s,p,Event::Failed,seq};
 if(!xQueueSend(requests,&j,0)){available=true;ev=Event::Failed;}else outstanding=true;
 }else if(!board_enter(s,cfg,p)){board_stop();ev=Event::Stop;available=true;}
 if(remote(s)||(s>=State::Feed&&s<=State::Verify))deadline=esp_timer_get_time()+int64_t(cfg.timeoutMs)*milliseconds;
 }}
 ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(pollMs));
 }}
}
extern "C" void app_main(){
 board_stop();
 ESP_ERROR_CHECK(nvs_flash_init());
 ESP_ERROR_CHECK(nvs_flash_init_partition("settings"));
 esp_ota_img_states_t bootState{};
 bool trial=esp_ota_get_state_partition(esp_ota_get_running_partition(),&bootState)==ESP_OK&&bootState==ESP_OTA_IMG_PENDING_VERIFY;
 if(!trial&&maintenance_sd_ready()){
  auto configResult=importConfig();
  if(configResult!=ESP_OK&&configResult!=ESP_ERR_NOT_FOUND){ESP_LOGE("CONFIG","Configuration SD refusee");return;}
  auto err=installImage("/sdcard/firmware.bin");
  if(err==ESP_OK)esp_restart();
  if(err==ESP_ERR_NOT_FOUND&&configResult==ESP_OK){ESP_LOGI("CONFIG","Configuration importee; retirer la cle et redemarrer");}
  else {ESP_LOGE("OTA","Installation interrompue: %s",esp_err_to_name(err));}
  return;
 }
 if(!readConfig(cfg)){if(trial)ESP_ERROR_CHECK(esp_ota_mark_app_invalid_rollback_and_reboot());metrics.state=step(State::Boot,Event::BadConfig).next;ESP_LOGE("CONVEYOR","ConfigFault: configuration NVS absente ou invalide");return;}
 esp_task_wdt_config_t wd{};wd.timeout_ms=cfg.watchdogS*1000;wd.trigger_panic=true;
 ESP_ERROR_CHECK(esp_task_wdt_reconfigure(&wd));
 uint8_t digest[32];ESP_ERROR_CHECK(esp_partition_get_sha256(esp_ota_get_running_partition(),digest));
 for(size_t i=0;i<sizeof digest;++i)snprintf(hash+2*i,3,"%02x",digest[i]);
 ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());esp_netif_create_default_wifi_sta();
 wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&init));
 ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifiEvent,nullptr));
 wifi_config_t wc{};memcpy(wc.sta.ssid,cfg.ssid,strlen(cfg.ssid));memcpy(wc.sta.password,cfg.password,strlen(cfg.password));
 ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&wc));ESP_ERROR_CHECK(esp_wifi_start());
 esp_sntp_config_t ntp=ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");ESP_ERROR_CHECK(esp_netif_sntp_init(&ntp));
 httpd_handle_t h;httpd_config_t hc=HTTPD_DEFAULT_CONFIG();ESP_ERROR_CHECK(httpd_start(&h,&hc));
 httpd_uri_t uri{};uri.uri="/health";uri.method=HTTP_GET;uri.handler=health;ESP_ERROR_CHECK(httpd_register_uri_handler(h,&uri));
 requests=xQueueCreate(1,sizeof(Job));replies=xQueueCreate(1,sizeof(Job));ESP_ERROR_CHECK(requests&&replies?ESP_OK:ESP_ERR_NO_MEM);
 ESP_ERROR_CHECK(xTaskCreate(network,"wms",stackNetwork,nullptr,3,&wmsTask)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
 ESP_ERROR_CHECK(xTaskCreate(controller,"fsm",stackController,nullptr,4,&fsmTask)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
}
