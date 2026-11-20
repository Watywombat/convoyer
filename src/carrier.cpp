#if __has_include("carrier_pins.hpp")
#include "carrier_pins.hpp"
#include "board.hpp"
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <driver/uart.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>
#include <driver/sdspi_host.h>
#include <sdmmc_cmd.h>
using namespace conveyor;
// Inputs are externally biased, active high, conditioned at 3.3 V.
// Servo position feedback must be one-hot. NFC bridge sends ASCII ref + LF.
namespace {
constexpr auto uart=UART_NUM_2;
constexpr auto mode=LEDC_LOW_SPEED_MODE;
constexpr int pwmMax=1023,servoHz=50,beltHz=1000;
bool armed=false,overflow=false;size_t used=0;uint8_t bytes[maxRef];
bool pwm(ledc_channel_t ch,int duty){return ledc_set_duty(mode,ch,duty)==ESP_OK&&ledc_update_duty(mode,ch)==ESP_OK;}
bool channel(int pin,ledc_channel_t ch,ledc_timer_t timer,int hz){
 ledc_timer_config_t t{};t.speed_mode=mode;t.duty_resolution=LEDC_TIMER_10_BIT;t.timer_num=timer;t.freq_hz=hz;t.clk_cfg=LEDC_AUTO_CLK;
 ledc_channel_config_t c{};c.gpio_num=pin;c.speed_mode=mode;c.channel=ch;c.timer_sel=timer;
 return ledc_timer_config(&t)==ESP_OK&&ledc_channel_config(&c)==ESP_OK;
}
bool active(int p){return gpio_get_level(gpio_num_t(p));}
}
extern "C" bool maintenance_sd_ready(){
 gpio_config_t g{};g.pin_bit_mask=1ULL<<maintenancePin;g.mode=GPIO_MODE_INPUT;
 if(gpio_config(&g)!=ESP_OK||!active(maintenancePin))return false;
 sdmmc_host_t host=SDSPI_HOST_DEFAULT();spi_bus_config_t bus{};
 bus.mosi_io_num=sdMosi;bus.miso_io_num=sdMiso;bus.sclk_io_num=sdClock;
 bus.quadwp_io_num=-1;bus.quadhd_io_num=-1;
 ESP_ERROR_CHECK(spi_bus_initialize(spi_host_device_t(host.slot),&bus,SPI_DMA_CH_AUTO));
 sdspi_device_config_t dev=SDSPI_DEVICE_CONFIG_DEFAULT();dev.host_id=spi_host_device_t(host.slot);dev.gpio_cs=gpio_num_t(sdCs);
 esp_vfs_fat_sdmmc_mount_config_t mount{};mount.max_files=1;sdmmc_card_t*card=nullptr;
 ESP_ERROR_CHECK(esp_vfs_fat_sdspi_mount("/sdcard",&host,&dev,&mount,&card));return true;
}
extern "C" void board_stop(){if(armed)pwm(LEDC_CHANNEL_0,0);}
extern "C" bool board_init(){
 for(auto p:inputs){gpio_config_t g{};g.pin_bit_mask=1ULL<<p;g.mode=GPIO_MODE_INPUT;if(gpio_config(&g)!=ESP_OK)return false;}
 uart_config_t u{};u.baud_rate=nfcBaud;u.data_bits=UART_DATA_8_BITS;u.parity=UART_PARITY_DISABLE;u.stop_bits=UART_STOP_BITS_1;u.source_clk=UART_SCLK_DEFAULT;
 armed=channel(beltPin,LEDC_CHANNEL_0,LEDC_TIMER_0,beltHz);
 return armed&&channel(servoPin,LEDC_CHANNEL_1,LEDC_TIMER_1,servoHz)&&uart_driver_install(uart,256,0,0,nullptr,0)==ESP_OK&&uart_param_config(uart,&u)==ESP_OK&&uart_set_pin(uart,UART_PIN_NO_CHANGE,nfcRx,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE)==ESP_OK;
}
extern "C" bool board_enter(State s,const Config&c,const Parcel&p){
 if(!pwm(LEDC_CHANNEL_0,0))return false;
 if(s==State::Feed){used=0;overflow=false;if(uart_flush_input(uart)!=ESP_OK)return false;}
 if(s==State::Feed||s==State::Verify)return pwm(LEDC_CHANNEL_0,c.speed*pwmMax/100);
 if(s==State::Gate)return p.exit>=1&&p.exit<=3&&pwm(LEDC_CHANNEL_1,servoDuty[p.exit-1]);
 return true;
}
extern "C" bool board_poll(State s,Event&e,Parcel&p){
 static bool previousKey=true;bool key=active(reconcilePin);bool rising=key&&!previousKey;previousKey=key;
 if((s==State::Hold||s==State::Recovery)&&rising&&!active(stopSense)&&!active(entryPin)&&!active(readerPin)){for(int pin:exitPins)if(active(pin))return false;e=Event::Cleared;return true;}
 if(active(stopSense)){e=Event::Stop;return true;}
 if(s==State::Idle&&active(entryPin)){for(int pin:exitPins)if(active(pin))return false;e=Event::Arrival;return true;}
 if(s==State::Feed&&active(readerPin)){e=Event::AtReader;return true;}
 if(s==State::Read){uint8_t b;
 for(size_t i=0;i<=maxRef&&uart_read_bytes(uart,&b,1,0)==1;i++){
 if(b=='\n'){e=!overflow&&parseRef(bytes,used,p.ref)?Event::Tag:Event::InvalidTag;return true;}
 if(used<sizeof bytes)bytes[used++]=b;else overflow=true;
 }}
 if(s==State::Gate||s==State::Verify){
 int observed=0;for(int i=0;i<3;i++)if(active(s==State::Gate?gatePins[i]:exitPins[i])){observed=observed?-1:i+1;}
 if(s==State::Gate&&observed==p.exit){
 for(int pin:exitPins)if(active(pin)){e=Event::Failed;return true;}
 e=Event::GateReady;return true;}
 if(s==State::Verify&&observed){p.observed=observed;e=exitEdge(p.exit,observed);return true;}
 }
 return false;
}
#else
#include "board.hpp"
extern "C" bool maintenance_sd_ready(){return false;}
extern "C" bool board_init(){return false;}
extern "C" void board_stop(){}
extern "C" bool board_enter(conveyor::State,const Config&,const Parcel&){return false;}
extern "C" bool board_poll(conveyor::State,conveyor::Event&,Parcel&){return false;}
#endif
