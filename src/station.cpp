#include "station.hpp"
#include <nvs.h>
#include <esp_ota_ops.h>
#include <esp_secure_boot.h>
#include <cstdio>
#include <mbedtls/pk.h>
#include <mbedtls/gcm.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/sha256.h>
#if __has_include("config_public_key.hpp")
#include "config_public_key.hpp"
#else
static constexpr char configPublicKey[]="";
#endif
#include <cstring>
#include <cJSON.h>
namespace {
bool field(cJSON *j,const char*k,char*out,size_t cap) {
 auto p=cJSON_GetObjectItemCaseSensitive(j,k);
 if(!cJSON_IsString(p))return false;
 size_t n=strlen(p->valuestring);if(!n||n>=cap)return false;
 memcpy(out,p->valuestring,n+1);return true;
}
bool number(cJSON*j,const char*k,int &out,int min,int max){
 auto p=cJSON_GetObjectItemCaseSensitive(j,k);
 if(!cJSON_IsNumber(p)||p->valuedouble<min||p->valuedouble>max||p->valueint!=p->valuedouble)return false;
 out=p->valueint;return true;
}
}
static bool authorized(const uint8_t*raw,size_t rawSize,const uint8_t*signature,size_t size){
 mbedtls_pk_context key;mbedtls_pk_init(&key);unsigned char digest[32];
 bool ok=mbedtls_pk_parse_public_key(&key,(const unsigned char*)configPublicKey,sizeof configPublicKey)==0
 &&mbedtls_sha256(raw,rawSize,digest,0)==0
 &&mbedtls_pk_verify(&key,MBEDTLS_MD_SHA256,digest,sizeof digest,signature,size)==0;
 mbedtls_pk_free(&key);return ok;
}
static bool decode(const char* raw,Config &c){
 auto j=cJSON_ParseWithOpts(raw,nullptr,true);if(!j)return false;
 bool ok=field(j,"wifi_ssid",c.ssid,sizeof c.ssid)&&field(j,"wifi_pass",c.password,sizeof c.password)
 &&field(j,"api_url",c.url,sizeof c.url)&&strncmp(c.url,"https://",8)==0&&strlen(c.url)>8
 &&field(j,"api_token",c.token,sizeof c.token)&&number(j,"belt_speed",c.speed,1,100)
 &&number(j,"timeout_ms",c.timeoutMs,100,60000)&&number(j,"watchdog_s",c.watchdogS,2,120)
 &&number(j,"warehouse_1",c.warehouse[0],1,2147483647)&&number(j,"warehouse_2",c.warehouse[1],1,2147483647)
 &&number(j,"warehouse_3",c.warehouse[2],1,2147483647)&&conveyor::exitFor(c.warehouse[0],c.warehouse);
 cJSON_Delete(j);return ok;
}
namespace {
// One NVS blob atomically stores the signed envelope and its signature.
struct Envelope {uint16_t size,signatureSize;uint8_t bytes[1056],signature[128];};
constexpr size_t prefix=16, tagLength=16;
uint32_t revision(const Envelope &e){
 return uint32_t(e.bytes[0])<<24|uint32_t(e.bytes[1])<<16|uint32_t(e.bytes[2])<<8|e.bytes[3];
}
bool openEnvelope(nvs_handle_t h,const Envelope&e,Config&c){
 if(e.size<=prefix+tagLength||e.size>sizeof e.bytes||!revision(e)||e.signatureSize>sizeof e.signature||
 !authorized(e.bytes,e.size,e.signature,e.signatureSize))return false;
 uint8_t key[32];size_t size=sizeof key;
 if(nvs_get_blob(h,"devicekey",key,&size)!=ESP_OK||size!=sizeof key)return false;
 char raw[1025];size_t count=e.size-prefix-tagLength;mbedtls_gcm_context g;mbedtls_gcm_init(&g);
 bool ok=mbedtls_gcm_setkey(&g,MBEDTLS_CIPHER_ID_AES,key,256)==0&&
 mbedtls_gcm_auth_decrypt(&g,count,e.bytes+4,12,e.bytes,4,e.bytes+prefix+count,tagLength,e.bytes+prefix,(uint8_t*)raw)==0;
 mbedtls_gcm_free(&g);mbedtls_platform_zeroize(key,sizeof key);raw[count]=0;
 ok=ok&&!memchr(raw,0,count)&&decode(raw,c);mbedtls_platform_zeroize(raw,sizeof raw);return ok;
}
esp_err_t readFile(const char*path,uint8_t*buf,size_t cap,uint16_t &n){
 FILE*f=fopen(path,"rb");if(!f)return ESP_ERR_NOT_FOUND;
 n=fread(buf,1,cap,f);bool ok=n&&fgetc(f)==EOF&&!ferror(f);fclose(f);
 return ok?ESP_OK:ESP_ERR_INVALID_SIZE;
}
}
bool readConfig(Config &c){
 nvs_handle_t h;if(nvs_open_from_partition("settings","conveyor",NVS_READONLY,&h)!=ESP_OK)return false;
 Envelope e{};size_t size=sizeof e;
 bool ok=nvs_get_blob(h,"bundle",&e,&size)==ESP_OK&&size==sizeof e&&openEnvelope(h,e,c);
 nvs_close(h);return ok;
}
esp_err_t importConfig(){
 Envelope e{};auto err=readFile("/sdcard/config.enc",e.bytes,sizeof e.bytes,e.size);if(err!=ESP_OK)return err;
 if(readFile("/sdcard/config.sig",e.signature,sizeof e.signature,e.signatureSize)!=ESP_OK)return ESP_ERR_INVALID_ARG;
 nvs_handle_t h;err=nvs_open_from_partition("settings","conveyor",NVS_READWRITE,&h);if(err!=ESP_OK)return err;
 Config candidate{};Envelope old{};size_t size=sizeof old;auto prior=nvs_get_blob(h,"bundle",&old,&size);
 bool newer=prior==ESP_ERR_NVS_NOT_FOUND||(prior==ESP_OK&&size==sizeof old&&revision(e)>revision(old));
 if(!newer||!openEnvelope(h,e,candidate))err=ESP_ERR_INVALID_ARG;
 else err=nvs_set_blob(h,"bundle",&e,sizeof e);
 if(err==ESP_OK){err=nvs_commit(h);}nvs_close(h);return err;
}
struct Record {conveyor::State state; Parcel parcel;};
esp_err_t writeJournal(conveyor::State s,const Parcel&p){
 nvs_handle_t h;auto e=nvs_open("conveyor",NVS_READWRITE,&h);if(e!=ESP_OK)return e;
 Record record{s,p};e=nvs_set_blob(h,"parcel",&record,sizeof record);
 if(e==ESP_OK)e=nvs_commit(h);
 nvs_close(h);return e;
}
bool pendingJournal(Parcel* saved){
 nvs_handle_t h;auto opened=nvs_open("conveyor",NVS_READONLY,&h);if(opened==ESP_ERR_NVS_NOT_FOUND)return false;if(opened!=ESP_OK)return true;
 Record r{};size_t n=sizeof r;auto e=nvs_get_blob(h,"parcel",&r,&n);nvs_close(h);
 if(e==ESP_OK&&n==sizeof r&&saved){*saved=r.parcel;saved->ref[sizeof saved->ref-1]=0;saved->transaction[sizeof saved->transaction-1]=0;}
 return e!=ESP_ERR_NVS_NOT_FOUND&&(e!=ESP_OK||n!=sizeof r||r.state!=conveyor::State::Idle);
}
// Called in maintenance with the line stopped and the SD volume mounted.
esp_err_t installImage(const char*path){
 FILE*f=fopen(path,"rb");if(!f)return ESP_ERR_NOT_FOUND;
 if(!esp_secure_boot_enabled()){fclose(f);return ESP_ERR_INVALID_STATE;}
 auto target=esp_ota_get_next_update_partition(nullptr);
 esp_ota_handle_t h;auto e=target?esp_ota_begin(target,OTA_SIZE_UNKNOWN,&h):ESP_ERR_NOT_FOUND;
 if(e==ESP_OK){
  unsigned char buf[1024];size_t n;
  while((n=fread(buf,1,sizeof buf,f)) && e==ESP_OK)e=esp_ota_write(h,buf,n);
  if(ferror(f))e=ESP_FAIL;
  if(e==ESP_OK)e=esp_ota_end(h);else esp_ota_abort(h);
  if(e==ESP_OK)e=esp_ota_set_boot_partition(target);
 }
 fclose(f);return e;
}
