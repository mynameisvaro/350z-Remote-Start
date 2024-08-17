# 350z - Remote Start

**DISCLAMER: ONLY TESTED ON USA, 2006, BASE MODEL, TERMINAL ARRANGEMENT TYPE 2 NISSAN 350Z WITH PCF7936AS TRANSPONDER CHIP**

**VERIFY ANY INFORMATION FROM THIS REPOSITORY WITH THE ONE CORRESPONDING TO YOUR OWN VEHICLE**

This repository is a "guide" on how to remote start a nissan 350z with your phone. However, it is not a finished project.

Things that need to be addressed:

- Gear selector sensor (safety measure)
- Rewrite all the code for a better understanding of it

**The following sections are based on the available tools at the moment of writting this. On later versions, the procedure may change.**

## Where to start

First thing first, to achieve a remote start, there is a handfull of things that are needed. Said things are in this list:

- Arduino UNO, ESP32, ESP8266 (preferably ESP32)
- MCP2515 CAN Module
- 12v to 5v voltage regulator
- 3v3/5v to 12v logic converter (both ways)
- Mosfets (explained afterwards)
- Transistors (explained afterwards)
- Resistors (explained afterwards)

## Guide:

1. [Capture Nonce and Challenge Response pairs](#first-step---capture-nonce-and-challenge-response-pairs)
2. [Obtain the secret key](#second-step---obtain-the-secret-key)
3. [Code the ESP32](#third-step---code-the-esp32)
4. [Hardware connections](#forth-step---hardware-connections)
5. [Start the car!](#fifth-step---start-the-car)
6. [My version](#my-version)

## First step - Capture Nonce and Challenge response pairs

During this first step, we'll get the Nonce and Challenge pairs using a ESP32.

### **Context and a bit of ilustration**

The Nissan 350z, along with other Nissan's vehicles manufactured near 2000s, it's equiped with a transponder basestation (in the car) and the actual transponder (on the keyfob). This is one of the main security measurements agains car thiefs. Without the corresponding transponder, the car will be able to turn over, but won't start. The transponder basestation is based on the [PCF7991AT](/assets/PCF7991AT.pdf) IC.

The [PCF7991AT](/assets/PCF7991AT.pdf) runs on 5v, but the PCB has components to translate the 5v logic to a 12v logic (in both ways) using pull-ups resistors (for 12v output) and a transistor (for 0v output)

In this picture of the PCB **(where the board is flipped upside down)**, the connector has 4 pins. From left to right, the pins are:

| Pin   | 1    | 2   | 3   | 4   |
| ----- | ---- | --- | --- | --- |
| Value | DATA | GND | CLK | 12V |

![PCF7991AT PCB PHOTO](/assets/PCF7991AT%20PCB.png)

### **How to connect the ESP32 to the car**

Once we know where the pins are and what do they do, the only thing left is to connect the ESP32 and read the pairs right? Well, no. There are two ways of reading the pairs.

#### 1. READING ON THE BCM (RECOMMENDED)

This is the easy method since we only need to remove one plastic panel and backprobe three pins. The panel that needs to be removed is the one sitting to the left of the clutch pedal, where a fuse panel is hidden behind. This panel doesn't just cover the fuse panel, but also the BCM, which is the brain of the car.

This module has three connectors. The one we want is the big white one. It should look something like this:

![BCM WHITE CONNECTOR PINOUT](/assets/BCM%20WHITE%20CONNECTOR%20PINOUT.png)

Now that we have located the connector, we need to plug 3 wires to it (from behind, **do not remove the connector**, just slide a wire between the plastic connector and the actual BCM wire just enough to make contact). One DATA, one CLK and one GND.

| Value  | DATA  | CLK         | GND     |
| ------ | ----- | ----------- | ------- |
| Color  | Brown | Green/White | Black   |
| Number | 25    | 21          | Chassis |

The other end of the wire should be connected to some "3v3/5v to 12v logic converter", since the ESP32 needs 3v3 and the cars outputs 12v. For this, I recommend using a simple [voltage divider](https://www.digikey.com/en/resources/conversion-calculators/conversion-calculator-voltage-divider) for each pin because we only need to read from the car. The output from the voltage divider (should be 3v3) needs to be connected to the pins as described in [350z-UIDNonceChallengeReader](/src/capture_nonce_challenge/capture_nonce_challenge.ino) file.

Now it's time to capture the pairs.

#### 2. READING ON THE TRANSPONDER BASESTATION (NOT RECOMMENDED)

First of all, we need to know where to connect the ESP32 to the car. To do so, we first need to remove everything underneath the steering wheel column. Once this is done, we will be able to see a light grey cylindrical looking thing with a black box on top with a black ring near the end of the cylinder. This is the ignition cylinder, and the transponder basestation. On the back of the black box is a white connector, unplug it and leave it hanging for now.

Now is time for the "3v3/5v to 12v logic converter" to kick in, since the car sends signals at 12v and the ESP32 only reads 3v3. We can use a simple [voltage divider](https://www.digikey.com/en/resources/conversion-calculators/conversion-calculator-voltage-divider) for each pin because we only need to read data. After taking two resistors and making sure that when 12v is applied at one end, the output is 3v, it's time to plug the ESP32 to the car.

![CONNECTION DIAGRAM FOR ARDUINO](/assets/LOGIC%20ARDUION%20CONNECTION.jpg)

As shown in the picture, the wires from the white connector that was previously removed from the transponder basestation are:

| Color | Blue | Green/White | Black | Brown |
| ----- | ---- | ----------- | ----- | ----- |
| Value | 12V  | CLK         | GND   | DATA  |

> **MAKE SURE TO BACKPROBE THEM BECAUSE YOU NEED TO PLUG THE CONNECTOR BACK TO THE TRANSPONDER BASESTATION**

After backprobing the wires to the white connector, connect the other end of each wire to a voltage divider. The output of each voltage divider should be connected to the corresponding pins as described in [350z-UIDNonceChallengeReader](/src/capture_nonce_challenge/capture_nonce_challenge.ino) file.
Once all the wires are connected, the only thing left to do is capture the pairs.

### **How to capture the Nonce and Challenge response pairs**

This is the easiest of the steps. All it's needed for the capture is to simply plug the ESP32, open the Arduino IDE console and turn the key 16 times to the run position (the position when the fuel pump start priming) waiting 4 seconds between each cycle.

After each cycle, you'll see a new line appear on the Arduino IDE console. Mark down the UID once, and every pair of values for later use. Or just wait until 16 samples have been taken, it will print a line with every Nonce and Challenge.

If you wonder how the pairs are extracted... well, this is how:

The [PCF7991AT](/assets/PCF7991AT.pdf) has the communication over the serial interface described within its datasheet. Most of the commands the BCM sends are just useless for us, since it uses them to configure the behaviour of the transponder basestatation. We only need two important bits, that aren't described in the [PCF7991AT](/assets/PCF7991AT.pdf) datasheet, but are preceded by a specific command from the BCM, which is documented in said datasheet.

![PCF7971AT COMMAND SET](/assets/PCF7991AT%20COMMAND%20SET.png)

The command we're interested in is the `WRITE_TAG` command. This is used to tell the transponder chip to start the new authentication and what Nonce and Challenge pair to use for the encryption. The other command we're interested in is the `READ_TAG`, since the IDE (or UID) needs to be read aswell. The communication has 2 write operations and 2 read operations taking the BCM as a reference (described in the [PCF7936AS](/assets/PCF7936AS.pdf) datasheet). The communication goes as such:

![BCM&TRANSPONDER START_AUTH](/assets/BCM&TRANSPONDER%20START_AUTH.png)

So the only thing to do is to extract the Random Number (aka Nonce) and the Signature (aka Challenge, which is 0xFFFFFFFF but encrypted), and the IDE (or UID).

However, the data is transferred using two specific encodings. One is the Manchester Encoding and the other one is the BPLM (Bit pulse length modulation) encoding:

![MANCHESTER ENCODING EXAMPLE](/assets/MANCHESTER%20ENCODING%20EXAMPLE.png)
![BPLM ENCODING EXAMPLE](/assets/BPLM%20ENCODING%20EXAMPLE.png)

Once we have obtained all of these pairs and the corresponding IDE, its time to decrypt everything and get the secret key

## Second step - Obtain the secret key

For this step, we'll be using a script that [AdamLaurie](https://github.com/AdamLaurie) made. We'll be using the [crack4](https://github.com/AdamLaurie/RFIDler/tree/master/hitag2crack/crack4) from his [hitag2crack](https://github.com/AdamLaurie/RFIDler/tree/master/hitag2crack) repo. Download it, compile it (using make) and run it as it is described in the readme file.

> Make sure to write the nonce and challenge pairs as such: **0x12345678 0x9abcdef0**, and make sure that below the last line of nonces and challenges, is another line without anything written to it. Otherwise the code will not run

Now that we have run the code, and have obtained the secret key, its time to code

## Third step - Code the ESP32

It's time to code! But I made it easy. All you need to do to make the car start is change some parameters on the [code](/src/remote_start/remote_start.ino) and uploaded. Yey!

All you need to change is the **UID** and the **Secret Key** in [defines.h](/src/remote_start/defines.h).

> **IMPORTANT:** Change partition scheme to "Huge App (3MB No OTA/1MB SPIFFS)"

> **IMPORTANT:** Compile the code with ESP32 version < 3.0.0 (i.e: 2.0.17), since the timer api has changed and I haven't had time to migrate it.
> Once the code is compiled and uploaded, its time for the hardware

## Forth step - Hardware connections

Here is where it gets a little messy. To make all the connections, you need to remove everything under the steering wheel. This is the plastic cover that hides the steering column and the metal shield that protects the steering column. It seems easy, but to remove the plastic cover, you also need to remove a screw that's hidden behind a plastic trim near the door frame... you know how this goes.

Once everything is removed, you'll need to have access to a few thigs:

- The clutch interlock switch connector
- The NATS Antenna connector
- The 6 pin power connector that goes to the ignition cylinder
- CAN Hight and Low wires (I recommend getting a OBD connector and taking the lines from there, it's easier)

Everything needs to be connected to the ESP32 board pins as indicated in the [remote_start sketch defines](/src/remote_start/defines.h)

The **clutch interlock switch connector** is found above the clutch pedal. Since it needs to mimic a swtich, I recommend getting a relay or a mosfet to do so (relay is easier). The pin that controls the relay, connect it to the corresponding pin in the ESP32. Make sure that when the relay is open when ESP32 is at idle.

The **NATS Antenna connetor** and its connection can be found [here](#2-reading-on-the-transponder-basestation-not-recommended)

The **6 pin power connector** is just another big switch with 6 channels, so you'll need 3 big relays or mosfets (same here, relays are easier). With this connector, you are all alone, because I've seen the pinout changing whith the car (I guess it depends on the region, EU or US). But it is still relatively easy, all you need to do is check what wire is carrying +12v hot and check for continuity on the cylinder pins with each turn of the key to see where the 12v are distributed across the positions. Once you have this sorted out, the first turn is called ACC, the second ON and the third position START. Connect each relay accordingly.

The **CAN H and L** wires are accessible in a lot of modules along the car, however it's easier to just take them from the OBD II connector. Once you have the lines, connect them to the MCP2515 H and L inputs. (You'll also need to connect the MCP2515 to the ESP32, for this, check [defines.h](/src/remote_start/defines.h) for CS and INT pins, and ESP32 pinout for the rest of SPI connections)

> **NOTE:** Leave TPD_PWR pin disconnected, since it will not be neede here

If you've reached this point, the rest should be piece of cake. Right?

## Fifth step - Start the car!

Download an app that can send BLE messages (like nRF Connect), find the "350z" device and send a 0x01 to start it, and a 0x00 to turn it off.

> **If it is the first time doing it, be prepared to unplug the ESP32 in case anything goes wrong**

## My version

Here are some pictures of the boards I designed to make this more compact

Here is a [video](https://varohome.duckdns.org/GithubAssets/Remote_Start.mp4) of how everything works together

> **NOTE:** I made a T in every connector to maintain the original functionalities of the car, I also power off the transponder in every remote start situation to avoid multiple devices taking on the same connection

### Version 1.0

In this verion, I also included a LED controller for the footwell. It was a temporal test, and I wanted to try other aditional features.

![V1Example1](/assets/V1Example1.jpg)
In the first photo a few things are visible. There is the T for the 6 pin connector, the wires going to the clutch interlock switch, the wires from the CAN bus, the pin wires going to the NATS antenna and to the LEDs. Also, the red main switch is there to avoid any unwanted damage.
![V1Example2](/assets/V1Example2.jpg)
This is just the board with nothing connected

### Version 2.0 (WIP)

At the time writing this, I only have two of the boards. The CAN board and the main board. In the future, there will be more boards and because it's a bus, I can add any board I want. I'm planning to add a board to control the LEDs, the front and rear headlights to have some courtesy lights and a light sensor to have automatic headlights.

![V2Example1](/assets/V2Example1.jpg)
