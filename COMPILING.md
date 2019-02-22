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

The makefile has a target to download and build the [lpc21isp utility from sourceforge](http://sourceforge.net/projects/lpc21isp/). Just run

```
make flash
```
and it wil be downloaded and compiled for you.
