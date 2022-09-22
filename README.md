## Compile

For ipq6018, uboot run in 32-bit mode, compile ipq6018 uboot by ipq40xx toolchain.

clone sdk, ie. toolchain to compile uboot:
```
git clone https://github.com/gl-inet-builder/openwrt-sdk-ipq_ipq40xx-qsdk11 sdk32
```

update and install feeds
```
cd sdk32
./scripts/feeds update -a
./scripts/feeds install -a
```
Manuall copy https://github.com/gl-inet-builder/qsdk11_feeds/tree/master/qca/feeds/bootloader/uboot-qca-ipq6018
uboot package definition to sdk32/package/ directory is also okay, if you not prefer to update feeds.

clone uboot source code
```
cd qca/src/
git clone https://github.com/gl-inet/uboot-ipq60xx.git u-boot-2016
cd ../..
```

compile uboot
```
make package/feeds/base/uboot-qca-ipq6018/compile V=s
```

## To fix error build
```
ERROR:root:code for hash sha1 was not found.
```

edit apt source: /etc/apt/sources.list to add
```
deb http://security.ubuntu.com/ubuntu bionic-security main
```

Then install libssl1.0-dev
```
sudo apt update && apt-cache policy libssl1.0-dev
sudo apt-get install libssl1.0-dev
```

the uboot binary will be:
**bin/ipq/openwrt-ipq6018-u-boot.mbn**
