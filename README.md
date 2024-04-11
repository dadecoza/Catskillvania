# Catskillvania - Condo of Bud
## for the PC

 [Windows Download](https://github.com/dadecoza/Catskillvania/releases/download/third/catskill_win64_202404093.zip)

 [Benheck's](https://github.com/benheck) original source code for the [gameBadge3](https://github.com/benheck/gamebadge3).

![Screenshot](https://raw.githubusercontent.com/dadecoza/Catskillvania/main/UI/Screenshot.png)

# Dependencies
* libgtk-3-dev

## Debian/Ubuntu
```
sudo apt-get update && sudo apt-get install build-essential libgtk-3-dev
```

## Windows (MinGW)
```
pacman -S base-devel mingw-w64-i686-toolchain mingw-w64-i686-gtk3 
```

## macOS
```
brew install llvm gtk+3
```

# Installation
Once the dependencies are installed ...
```
git clone https://github.com/dadecoza/Catskillvania.git
cd Catskillvania
make
./catskillvania
```
