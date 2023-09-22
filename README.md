# nandloader
Cross Platform Nintendo Wii/vWii NANDLoader

## Why?
NANDLoaders come in two flavours, vWii and Wii. For the Wii Shop Channel, we wanted to have only one NANDLoader that works on both Wii and vWii for easier installation on our end. Less files on server and no need to check if the installing console is a Wii or vWii.

## How to compile
You will need `devkitppc` with the `DEVKITPRO` and `DEVKITPPC` enviornment variables set. After it is compiled, you will need to use [wiipax](https://github.com/fail0verflow/hbc/tree/master/wiipax) from the Homebrew Channel repository to fix up the ELF. Use the flag `-c dkppcchannel` and run `elf2dol` on the outputted ELF. After that, pack into your channel and it should work!

## Credits
fail0verflow's [Homebrew Channel](https://github.com/fail0verflow/hbc)
WiiPAX and the stub present in the `data` folder was sourced from them, as well as the `reloc` function.
