#define LOG_LOCAL_LEVEL ESP_LOG_INFO

#include "CanonBLERemote.h"
#include <Arduino.h>
#include "Ticker.h"
#include "M5StickCPlus.h"
#include "Display.h"
#include "TimeLapse_Management.h"

#include "esp_log.h"
#include <esp32-hal-log.h>

String name_remote = "BR-M5";
CanonBLERemote canon_ble(name_remote);
Display M5_display(&M5.Lcd, name_remote);

TimeLapse timelapse(400);

enum class RemoteMode
{
    Settings,
    SettingsTransition,
    Shooting,
    ShootingTransition,
    TimeLapse,
    TimeLapseTransition,
};

RemoteMode current_mode;

double getBatteryLevel(void)
{
    AXP192 axp192;
    auto vbatData = axp192.GetBatVoltage();
    return 100.0 * ((vbatData - 3.0) / (4.15 - 3.0));
}

void setup()
{
    Serial.begin(115200);
    esp_log_level_set("*", ESP_LOG_INFO); 

    M5.begin();
    bool do_pair = M5.BtnA.isPressed();
    M5.Axp.ScreenBreath(9);
    M5.Lcd.setRotation(1);
    M5_display.set_init_screen(do_pair);
    

    current_mode = RemoteMode::Shooting;

    canon_ble.init();
    
    delay(500);
    // Pairing
    if(do_pair){
        // pair() function should be called only when you want to pair with the new camera. 
        // After paired, the pair() function should not be called.
        do{
            Serial.println("Pairing...");
        }
        while(!canon_ble.pair(10));

    }

    delay(500);
    Serial.println("Setup Done");
    
    
    

    AXP192 axp192;
    Serial.println("Battery status:");
    Serial.println(getBatteryLevel());

    Serial.println("Temp status:");
    Serial.println(axp192.GetTempInAXP192());

    Serial.println("Voltage status:");
    Serial.println(axp192.GetBatVoltage());

    char batlevelchar[20];
    snprintf(batlevelchar, sizeof(batlevelchar), "%f", getBatteryLevel());
    String batlevel{batlevelchar};

    M5_display.set_address(batlevel);
    M5_display.set_main_menu_screen(-1, "Battery Level");

    delay(2000);
    M5_display.set_address(canon_ble.getPairedAddressString());
    M5_display.set_main_menu_screen(-1, "Single Shooting");
}

void single_shot()
{
    M5_display.set_main_menu_screen(-1, "Cheese");
    Serial.println("Single shot");
    if(!canon_ble.trigger()){
        Serial.println("Trigger Failed");
    }
    delay(200);
    M5_display.set_main_menu_screen(-1, "Single Shooting");
}

void timelapse_shooting_trigger()
{
    if (timelapse.Recording_OnOFF())
    {
        Serial.println("Start timelapse");
        M5_display.set_main_menu_screen(timelapse.get_interval(), "Shooting timelapse");
    }
    else
    {
        Serial.println("Stop timelapse");
        M5_display.set_main_menu_screen(timelapse.get_interval(), "Ready for timelapse");
    }


}

void timelapse_shooting_worker()
{
    // Update timelapse
    if (timelapse.TimeLapse_Trigger())
    {
        M5_display.set_main_menu_screen(timelapse.get_interval(), "Cheese");
        Serial.println("Trigger timelapse");
        if(!canon_ble.trigger()){
            Serial.println("Trigger Failed");
        }
        delay(200);
        M5_display.set_main_menu_screen(timelapse.get_interval(), "Shooting timelapse");
    }
}

void update_settings()
{
    static AXP192 axp192;
    if (axp192.GetBtnPress())
    {
        timelapse.TimeLapse_decDelay();
        M5_display.set_main_menu_screen(timelapse.get_interval(), "Setting interval");
    }
    if (M5.BtnB.wasReleased())
    {
        timelapse.TimeLapse_incDelay();
        M5_display.set_main_menu_screen(timelapse.get_interval(), "Setting interval");
    }
}

void loop()
{
    // Update buttons state
    M5.update();

    switch (current_mode)
    {
    case RemoteMode::ShootingTransition:
        if(M5.BtnB.wasReleased())
        {
            current_mode = RemoteMode::Shooting;
        }
    break;

    case RemoteMode::Shooting:
        if (M5.BtnB.pressedFor(700))
        {
            // M5.BtnB.reset();
            current_mode = RemoteMode::SettingsTransition;
            M5_display.set_main_menu_screen(timelapse.get_interval(), "Setting interval");
        }
        else if (M5.BtnA.wasPressed())
        {
            single_shot();
        }
        break;

    case RemoteMode::SettingsTransition:
        if(M5.BtnB.wasReleased())
        {
            current_mode = RemoteMode::Settings;
        }
    break;
    case RemoteMode::Settings:
        if (M5.BtnB.pressedFor(700))
        {
            // M5.BtnB.reset();
            current_mode = RemoteMode::TimeLapseTransition;
            M5_display.set_main_menu_screen(timelapse.get_interval(), "Timelapse");
        }
        else
        {
            update_settings();
        }
        break;
    
    case RemoteMode::TimeLapseTransition:
        if(M5.BtnB.wasReleased())
        {
            current_mode = RemoteMode::TimeLapse;
        }
    break;


    case RemoteMode::TimeLapse:
        if (M5.BtnB.pressedFor(700))
        {
            // M5.BtnB.reset();
            current_mode = RemoteMode::ShootingTransition;
            M5_display.set_main_menu_screen(-1, "Single Shooting");
        }
        else if (M5.BtnA.wasPressed())
        {
            timelapse_shooting_trigger();
        }
        timelapse_shooting_worker();


    default:
        break;
    }
    delay(10);
}