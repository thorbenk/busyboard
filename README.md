# Development setup

## OpenOCD

```bash
sudo apt install automake autoconf build-essential texinfo libtool \
 libftdi-dev libusb-1.0-0-dev libhidapi-dev libhidapi-hidraw0

git clone https://github.com/raspberrypi/openocd.git --branch rp2040 --depth=1
cd openocd
./bootstrap
./configure --prefix=$HOME/local --enable-picoprobe --enable-cmsis-dap-v2 --enable-cmsis-dap
make
make install

sudo cp ~/code/openocd/contrib/60-openocd.rules /etc/udev/rules.d/
```
