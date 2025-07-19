# Installation Guide
## Prerequisites
- Arduino IDE or PlatformIO <- This guide is only for the Arduino IDE
- Connection between the Computer and ESP32
- IO-Pin number for the LEDs
<details>
  <summary>How to find the right IO-Pin</summary>
  <img width="2160" height="884" alt="image" src="https://github.com/user-attachments/assets/b8966576-8be8-4bc5-8d23-2f62c73926fa" />

If you don't know what pin you want to use just use Pin 21
</details>

## Installing Dependencies
You need:
<WiFi.h> Preinstalled
<HTTPClient.h> Preinstalled
<ArduinoJson.h> by Benoît Blanchon
<Adafruit_NeoPixel.h> by AdaFruit
<TaskScheduler.h> by Anatoli Arkhipenko 

## Connecting ESP32
Connect the ESP-32 via USB(-C) to our computer 
> [!Warning]
> Do not wonder if the Serial monitor gives you weird output before Programming it
> If you get invalid characters after installing Change the BAUD rate to 115200 in the Serial Monitor

On some ESP-32 (Mostly Non-Mini Dev boards) you need to push a (BOOT) Button to enter the Programming mode

## Programming ESP32
Create a New Sketch (CTRL + N) 
Change the Values marked by the Placeholder "ChangeMe" and the LED informations
if you dont know what each value is check the comment above it.

After Programming you may need to press the (EN) Reset button to restart it

## Finishing Setup
After it booted it should start to Glow in Purple which indicates the search for WIFI
if this State does not change after 30 sec check the WIFI Credentials 
> [!Warning]
> All Values including WIFI Name are CASE SENSITIVE

When it connects to the WIFI the lights should turn off and you should get a notification to your webhook which should either return a error if your Application credentials are wrong or a CODE which you need to connect it to your account. 

It only asks for PresenceReadAll permissons to get the Presence value. it can ONLY get Presence and Activity which are by default Publicly accesible

By default it should now Poll the presence of the User(s) you specified and after and should display by Priority
It goes from Highest to Lowest
Busy Blinking (In call / Presentation / Group Call) > Busy (Calendar Event or Manual Presence) > Availible > Away > Offline

## Error Handing
If the light goes blue it is in an error state and a Restart should solve most Problems like session Timeout.
If not one of your credentials might be broken and / or Teams decided to change their api for no Reason
