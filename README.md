# MMS-OS
Molecular Multiverse Services Operating System, or simply MMS-OS, is a WIP operating system made with C.

## Current Features & Overview
- Command line interface like setup
- Calculator
- Wordle game
- Basic file system with commands
- Basic Tiny CC integration (close enough to actual C syntax, uses source functions)
- Login system

## Use/Build instructions
**Use from release ISO:** (iso/output)
- Load ISO in your system of choice. It should work fine out the box.
- This OS has only been successfully tested in Qemu. I cannot guarantee its success in another VM or on, God forbid you try, actual hardware.

**Build from source:**
- Requires a Linux system or Windows Subsystem for Linux
- Git clone project to your preferred directory
- Make sure you are in the project directory in your terminal AND have Qemu installed
- Run `chmod +x build.sh` and `chmod +x run.sh`
- First run `./build.sh`, then if successful run `./run.sh`
- Congrats! You are now in MMS-OS!

## Contact
This project is developed and maintained by the Molecular Multiverse Services team (just me so far, therealiodinemacer :D)
For any questions, suggestions, issues, etc, feel free to reach out to @therealiodinemacer on Discord or join our server [here](https://discord.gg/ZAx3NN5TJY))

## Roadmap
- ~~Better filesystem (preferably FAT)~~
- Better command handling
- Possible graphics implementations

## Disclaimers
MMS-OS is a VERY work-in-progress project. Lag, bugs, etc are to be expected. As more of a hobby project, also expect slow progress and a lack of functionality. This project is just for fun, don't expect much :)

Some AI was used in the making of this project, especially with parts of the initial setup or other complicated pieces. I really don't know too much of C, and this is the easiest way for me to learn the language while also making progress. Outside of the initial setup and components that I needed help with, it is however my work. If you doubt that, check some of the scripts, there's no way anyone other than a stupid human could make some of those...
