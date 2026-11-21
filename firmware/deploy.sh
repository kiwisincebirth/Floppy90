cd ./build
make
~/.pico-sdk/toolchain/14_2_Rel1/bin/arm-none-eabi-size -A Floppy90.elf
cp ./Floppy90.uf2 /Volumes/RP2350/
cd ..


