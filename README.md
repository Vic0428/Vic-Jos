# Learning Jos From Scratch: Lab1

## Getting Started

### Mac OS

#### Build Your Own Complier Toolchain

First, install `brew`

```
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)”
```

Then, install the toolchains

```
brew install i386-elf-gcc i386-elf-gdb
```

#### QEMU Simulator

First, specify that we want use tap from [HMC CS134](https://www.cs.hmc.edu/~rhodes/courses/cs134/sp19/)

```shell
brew tap nrhodes/homebrew-os134sp19
```

Then, install qemu with

```
brew install nrhodes/homebrew-os134sp19/qemu
```

## Reference

- [Setting Up Environment](https://www.cs.hmc.edu/~rhodes/courses/cs134/sp19/tools.html)

