# INSTALL xtensa/esp-open-sd on OSX

## some dependencies
brew install autoconf binutils help2man automake gnu-sed

## gsed needed in PATH
PATH="/usr/local/opt/gnu-sed/libexec/gnubin:$PATH"

## a case sensitive HD is needed
hdiutil create ~/Desktop/eos.dmg -volname "esp-open-sdk" -size 5g -fs "Case-sensitive HFS+"
hdiutil mount ~/Desktop/eos.dmg
cd /Volumes/esp-open-sdk/
git clone git@github.com:pfalcon/esp-open-sdk.git
git clone --recursive https://github.com/pfalcon/esp-open-sdk
cd esp-open-sdk
make STANDALONE=y |& tee make0.log

cd micropython; mkdir esp-open-sdk; cd esp-open-sdk
cp -av /Volumes/esp-open-sdk/esp-open-sdk/xtensa-lx106-elf .
