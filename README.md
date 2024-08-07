# PlayStation 2 Network Adapter Diagnosis Tool

The purpose of this project: 

I recently got a SCPH-30004 unit and SCPH-10350 network adapter extension. The latter one doesnt seem to work so i'm trying to understand where exactly it fails and what i can do. 
Most of the code has been taken from https://github.com/jmoseman01/ps2ConnectToSocket/blob/main/ps2ip.c and https://github.com/ps2dev/ps2sdk/blob/master/ee/network/tcpip/samples/tcpip_dhcp/ps2ip.c
At glance, it seems i cannot do much using netman, but maybe i'll commit some more time into it to see the other ways. 


The devcontainer is tested only on MacOS host powered by Apple SoC. Perhaps it would run just fine on x86_64 and/or linux hosts.

## Usage 

1. Open the repo folder in VS Code
2. *Remote Explorer* tab, select *Containers* in the combo box
3. Click on "reopen the current folder in a container"
4. Select cpptools configuration in lower right corner for proper syntax higlight

## Build

From VSCode's builtin terminal, cd into `src`, run `make`. See the produced ELF file. 


## Credits
[.vscode/c_cpp_properties.json](.vscode/c_cpp_properties.json) inspired by https://github.com/islandcontroller/ps2ded-vscode

.devcontainer is built on the top of [ps2dev](https://github.com/ps2dev/ps2dev)
