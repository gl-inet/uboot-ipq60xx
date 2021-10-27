## compile

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
git clone https://gitlab.com/gl.router/ax1800-uboot.git
cd ../..
```

compile uboot
```
make package/feeds/base/uboot-qca-ipq6018/compile V=s
```

the uboot binary will be:
bin/ipq/openwrt-ipq6018-u-boot.mbn
