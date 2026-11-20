#include "station.hpp"
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_log.h>
#include <cJSON.h>
#include <climits>
#include <cstdio>
#include <cstring>
#include <cstdlib>
using namespace conveyor;
namespace {
struct Body {char data[4096];size_t used;bool overflow;};
esp_err_t collect(esp_http_client_event_t *e){
 if(e->event_id!=HTTP_EVENT_ON_DATA)return ESP_OK;
 auto &b=*static_cast<Body*>(e->user_data);
 if(size_t(e->data_len)>=sizeof(b.data)-b.used){b.overflow=true;return ESP_FAIL;}
 memcpy(b.data+b.used,e->data,e->data_len);b.used+=e->data_len;b.data[b.used]=0;return ESP_OK;
}
int id(const cJSON*n){
 if(cJSON_IsNumber(n)&&n->valuedouble>0&&n->valuedouble<=INT_MAX&&n->valuedouble==n->valueint)return n->valueint;
 if(!cJSON_IsString(n)||!n->valuestring[0])return 0;
 for(const char*p=n->valuestring;*p;p++)if(*p<'0'||*p>'9')return 0;
 char*end;long long v=strtoll(n->valuestring,&end,10);return !*end&&v>0&&v<=INT_MAX?int(v):0;
}
}
Event wms(const Config&c,State s,Parcel&p){
 char path[192],payload[320]{},url[400];
 if(s==State::Lookup){
 char safe[maxRef+1];size_t n=strnlen(p.ref,sizeof p.ref);
 if(!parseRef(reinterpret_cast<const uint8_t*>(p.ref),n,safe))return Event::Failed;
 snprintf(path,sizeof path,"/products/ref/%s",safe);
 }else{
 if(p.exit<1||p.exit>3||p.product<=0)return Event::Failed;
 int wh=c.warehouse[p.exit-1];
 if(s==State::Capacity)snprintf(path,sizeof path,"/conveyor/capacity/%d",wh);
 else if(s==State::Commit){strcpy(path,"/stockmovements");snprintf(payload,sizeof payload,"{\"product_id\":%d,\"warehouse_id\":%d,\"qty\":1,\"movementcode\":\"%s\",\"label\":\"Conveyor\"}",p.product,wh,p.transaction);}
 else if(s==State::Report){strcpy(path,"/conveyor/incidents");snprintf(payload,sizeof payload,"{\"movementcode\":\"%s\",\"expected_exit\":%d,\"observed_exit\":%d}",p.transaction,p.exit,p.observed);}
 else return Event::Failed;
 }
 snprintf(url,sizeof url,"%s%s",c.url,path);Body body{};
 esp_http_client_config_t hc{};hc.url=url;hc.timeout_ms=c.timeoutMs;hc.crt_bundle_attach=esp_crt_bundle_attach;hc.disable_auto_redirect=true;hc.event_handler=collect;hc.user_data=&body;
 auto h=esp_http_client_init(&hc);if(!h)return Event::Failed;
 auto err=esp_http_client_set_header(h,"DOLAPIKEY",c.token);
 if(*payload&&err==ESP_OK)err=esp_http_client_set_method(h,HTTP_METHOD_POST);
 if(*payload&&err==ESP_OK)err=esp_http_client_set_header(h,"Content-Type","application/json");
 if(*payload&&err==ESP_OK)err=esp_http_client_set_post_field(h,payload,strlen(payload));
 if(err==ESP_OK)err=esp_http_client_perform(h);
 int status=esp_http_client_get_status_code(h);esp_http_client_cleanup(h);
 if(err!=ESP_OK||body.overflow){ESP_LOGE("WMS","transport=%s overflow=%d",esp_err_to_name(err),body.overflow);return err==ESP_ERR_TIMEOUT?Event::Timeout:Event::Failed;}
 if(status<200||status>=300){ESP_LOGE("WMS","HTTP %d state %d",status,int(s));return httpResult(s,status);}
 auto j=cJSON_ParseWithOpts(body.data,nullptr,true);Event result=Event::Failed;
 if(s==State::Lookup&&j){
 auto ref=cJSON_GetObjectItemCaseSensitive(j,"ref");
 p.product=id(cJSON_GetObjectItemCaseSensitive(j,"id"));
 auto extras=cJSON_GetObjectItemCaseSensitive(j,"array_options");
 p.exit=exitFor(id(cJSON_GetObjectItemCaseSensitive(extras,"options_destination_warehouse")),c.warehouse);
 if(cJSON_IsString(ref)&&!strcmp(ref->valuestring,p.ref)&&p.product&&p.exit)result=Event::Found;
 }else if(s==State::Capacity){
 auto available=cJSON_GetObjectItemCaseSensitive(j,"available");
 if(cJSON_IsBool(available))result=cJSON_IsTrue(available)?Event::Space:Event::Full;
 }else if(s==State::Commit&&id(j))result=Event::Recorded;
 else if(s==State::Report&&id(j))result=Event::Reported;
 cJSON_Delete(j);return result;
}
