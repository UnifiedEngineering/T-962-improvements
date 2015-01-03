# Compiling without LPCXpresso

We need the `gcc-arm-none-eabi` compiler, for example in Ubuntu:

```
sudo add-apt-repository -y ppa:terry.guo/gcc-arm-embedded
sudo apt-get update
sudo apt-get install gcc-arm-none-eabi
```

And then run

```
make
```

## Flashing in Linux

Using [lpc21isp], (after entering the bootloader) it's a matter of running

```
./lpc21isp build/T-962-controller.hex /dev/ttyUSB0 57600 11059
```

[lpc21isp]:  http://sourceforge.net/projects/lpc21isp/
