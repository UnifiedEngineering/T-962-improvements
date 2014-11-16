T-962 reflow oven improvements
==============================

As we had use for a small reflow oven for a small prototype run we settled for the T-962 even after having seen the negative reviews of it as there were plenty of suggestions all across the Internet on how it could be improved including replacing the existing controller and display(!). After having had a closer look at the hardware (replacing the masking tape inside with Kapton tape first) it was obvious that there was a simple way to improve the software disaster that is the T-962.

Here are a few improvements made to the cheap T-962 reflow oven utilizing the _existing_ controller HW with only a small, cheap, but very necessary modification. As you have to open the top part of the oven anyway to reflash the software this is a no-brainer fix: Adding a temperature sensor to the connector block where the thermocouples are connected to the controller board (the existing controller makes the assumption that the cold-junction is at 20 degrees Celsius at all times which made keeping a constant temperature "a bit" challenging as the terminal block sits _on_top_of_an_oven_ with two TRIACs nearby). 

It turns out that both an analog input and at least one generic GPIO pin is available on unpopulated pads on the board. GPIO0.7 in particular was very convenient for 1-wire operation as there was an adjacent pad with 3.3V so a 4k7 pull-up resistor could be placed there, then a jumper wire is run from GPIO0.7 pad to the Dq pin of a cheap DS18B20 1-wire temperature sensor that gets epoxied to the terminal block, soldering both Vcc and ground pins to the ground plane conveniently located right next to it. Pictures showing the modifications are available in this repository and the replacement firmware (also here) makes use of this sensor for cold-junction compensation. Some hot-glue may have to be removed to actually get to the side of the connector and the ground plane, someone seems to have been really trigger-happy with the glue gun!

The firmware is built with LPCXpresso 7.5.0 as I've never dealt with the LPC2000-series NXP microcontrollers before so I just wanted something that wouldn't require TOO much of work to actually produce a flashable image. Philips LPC2000 Flash Utility v2.2.3 was used to flash the controller through the ISP header present on the board.

LPCXpresso requires activation but is free for everything but large code sizes (the limit is larger than the 128kB flash size on this controller anyway so it's not really an issue). The flash utility unfortunately only runs on Windows but Flash Magic seems to be an alternative (that was not investigated).

The MCU is an LPC2134/01 with 128kB flash/16kB RAM, stated to be capable of running at up to 60MHz. Unfortunately the PLL in this chip is not that clever so with the supplied XTAL at 11.0592MHz we can only reach 55.296MHz (5x multiplier).

ISP pinout from left to right if looking at the board so the ISP text can be read:
1:n_ISP (connected to GPIO0.14, ground during reset to enter ISP mode)
2:n_RESET (active low reset input)
3:TXD0 (UART0 output to host, LVTTL 3.3V)
4:RXD0 (UART0 input to MCU, LVTTL 3.3V)
5:Ground (this ground is floating with respect to earth ground in the mains connector and chassis)

Please refer to the firmware source for other GPIO assignments.

This has only been tested on a fairly recent build of the T-962 (smallest version), build time on the back panel states 14.07 which I assume means 2014 July (or less likely week 7 of 2014).

As mentioned elsewhere, make sure the protective earth/ground wire from the main input actually makes contact with the back panel of the chassis and also that tha back panel makes contact both with the top and bottom haöves of the oven!

This is very much a quick hack to get only the basic functionality needed up and running. Everything in here is released under the GPLv3 license in hopes that it might be interesting for others to improve on this. Feedback is welcome!

Happy hacking!
