#include "build_profile.h"
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_system.h"
#include "mbedtls/sha256.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if !CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
#error "Rollback must be enabled in the bootloader"
#endif
// M5Stack Basic v2.7 only. Buttons are active low.
enum { BTN_A=39, BTN_B=38, BTN_C=37, LCD_CS=14, SD_CS=4, CLK=18, MOSI=23, MISO=19 };
static const char *tag="BANC";
static bool pressed(int pin){return gpio_get_level(pin)==0;}

#if BENCH_IS_A
static esp_err_t install(void){
    static bool mounted;
    if(!mounted){
        sdmmc_host_t host=SDSPI_HOST_DEFAULT();
        static bool bus_ready;
        if(!bus_ready){
            spi_bus_config_t bus={.mosi_io_num=MOSI,.miso_io_num=MISO,.sclk_io_num=CLK,
                                  .quadwp_io_num=-1,.quadhd_io_num=-1};
            esp_err_t err=spi_bus_initialize(host.slot,&bus,SPI_DMA_CH_AUTO);
            if(err!=ESP_OK)return err;
            bus_ready=true;
        }
        sdspi_device_config_t dev=SDSPI_DEVICE_CONFIG_DEFAULT();
        dev.host_id=host.slot;dev.gpio_cs=SD_CS;
        esp_vfs_fat_sdmmc_mount_config_t cfg={.max_files=1};
        sdmmc_card_t *card=NULL;
        esp_err_t err=esp_vfs_fat_sdspi_mount("/sd",&host,&dev,&cfg,&card);
        if(err!=ESP_OK)return err;
        mounted=true;
    }
    FILE *f=fopen("/sd/firmware.bin","rb");
    if(!f)return ESP_ERR_NOT_FOUND;
    const esp_partition_t *target=esp_ota_get_next_update_partition(NULL);
    esp_ota_handle_t handle=0;
    esp_err_t err=target?esp_ota_begin(target,OTA_SIZE_UNKNOWN,&handle):ESP_ERR_NOT_FOUND;
    if(err!=ESP_OK){fclose(f);return err;}
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    uint8_t block[1024],digest[32];
    if(mbedtls_sha256_starts(&sha,0)!=0)err=ESP_FAIL;
    size_t n;
    while(err==ESP_OK&&(n=fread(block,1,sizeof block,f))>0){
        if(mbedtls_sha256_update(&sha,block,n)!=0)err=ESP_FAIL;
        if(err==ESP_OK)err=esp_ota_write(handle,block,n);
    }
    if(ferror(f)||mbedtls_sha256_finish(&sha,digest)!=0)err=ESP_FAIL;
    mbedtls_sha256_free(&sha);
    fclose(f);
    if(err==ESP_OK&&memcmp(digest,expected_digest,sizeof digest)!=0)err=ESP_ERR_INVALID_CRC;
    if(err!=ESP_OK){esp_ota_abort(handle);return err;}
    err=esp_ota_end(handle);
    return err==ESP_OK?esp_ota_set_boot_partition(target):err;
}
#endif

// Read-only presence check for the confirmed Basic-family I2C pin map.
static void probe_modules(void){
    i2c_master_bus_config_t config={
        .i2c_port=I2C_NUM_0,.sda_io_num=21,.scl_io_num=22,
        .clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7
    };
    i2c_master_bus_handle_t bus;
    esp_err_t err=i2c_new_master_bus(&config,&bus);
    if(err!=ESP_OK){
        ESP_LOGW(tag,"Diagnostic I2C indisponible : %s",esp_err_to_name(err));
        return;
    }
    const uint16_t addresses[]={0x28,0x70,0x71};
    for(size_t i=0;i<sizeof(addresses)/sizeof(addresses[0]);++i){
        err=i2c_master_probe(bus,addresses[i],50);
        ESP_LOGI(tag,"I2C 0x%02x : %s",(unsigned)addresses[i],esp_err_to_name(err));
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_del_master_bus(bus));
}

void app_main(void){
    gpio_config_t buttons={.pin_bit_mask=(1ULL<<BTN_A)|(1ULL<<BTN_B)|(1ULL<<BTN_C),.mode=GPIO_MODE_INPUT};
    ESP_ERROR_CHECK(gpio_config(&buttons));
    ESP_ERROR_CHECK(gpio_set_direction(LCD_CS,GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(LCD_CS,1));
    probe_modules();
    const esp_partition_t *running=esp_ota_get_running_partition();
    ESP_LOGI(tag,"Version %s | partition %s",BENCH_IS_A?"A":"B",running->label);
#if BENCH_IS_A
    ESP_LOGI(tag,"A : appuyer sur le bouton A (gauche) pour installer firmware.bin");
#else
    esp_ota_img_states_t state;
    ESP_ERROR_CHECK(esp_ota_get_state_partition(running,&state));
    if(state!=ESP_OTA_IMG_PENDING_VERIFY){ESP_LOGI(tag,"B confirmee");return;}
    ESP_LOGI(tag,"B en essai : bouton B (milieu) = confirmer, C (droite) = revenir a A");
#endif
    bool armed=false;
    for(;;){
        if(!pressed(BTN_A)&&!pressed(BTN_B)&&!pressed(BTN_C))armed=true;
#if BENCH_IS_A
        if(armed&&pressed(BTN_A)){
            armed=false;
            esp_err_t err=install();
            if(err==ESP_OK)esp_restart();
            ESP_LOGE(tag,"SD / image refusee : %s",esp_err_to_name(err));
        }
#else
        if(armed&&pressed(BTN_C))ESP_ERROR_CHECK(esp_ota_mark_app_invalid_rollback_and_reboot());
        if(armed&&pressed(BTN_B)){
            ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
            ESP_LOGI(tag,"B confirmee");return;
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}
