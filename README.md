# <img src="https://raw.githubusercontent.com/dadecoza/Catskillvania/main/UI/Catskillvania.ico" alt="Bud" height="28px" /> Catskillvania - CONDO of BUD

 [Windows Download](https://github.com/dadecoza/Catskillvania/releases/download/fourth/catskill_win64_20240411.zip)

 [Benheck's](https://github.com/benheck) original source code for the [gameBadge3](https://github.com/benheck/gamebadge3).

![Game](https://github.com/dadecoza/Catskillvania/blob/main/UI/Catskill.gif?raw=true)

# Dependencies
* libgtk-3-dev
* libgme-dev

### Debian/Ubuntu
```
sudo apt-get update && sudo apt-get install build-essential libgtk-3-dev libgme-dev
```

### Windows (MinGW)
```
pacman -S base-devel mingw-w64-i686-toolchain mingw-w64-i686-gtk3 mingw-w64-x86_64-libgme
```

### macOS
```
brew install llvm gtk+3 game-music-emu
```

# Installation
Once the dependencies are installed ...
```
git clone https://github.com/dadecoza/Catskillvania.git
cd Catskillvania
make
./catskill
```

# Useful links
### Awsome open source libraries
* https://www.gtk.org/
* https://github.com/libgme/game-music-emu
* https://miniaud.io/index.html
### The original game
* https://github.com/benheck/gamebadge3
* Youtube Videos
  * Ben Heck Hacks - [Gamebadge3 Part 1: Creating Graphics](https://www.youtube.com/watch?v=43q2bR-B3sI)
  * Ben Heck Hacks - [Gamebadge3 Part 2 Assembly and Code](https://www.youtube.com/watch?v=VSEMkjyJ5Pk)
  * Ben Heck Hacks - [GameBadge Part 3: Game Design and Testing](https://www.youtube.com/watch?v=T-2CkQOANOM) <-- *a lot of technical detail regarding this game!*
  * Ben Heck Hacks - [MGC 2024 Gamebadge 3B - Sponsored by PCBWay!](https://www.youtube.com/watch?v=4nOpY00oYIk)
  * Ben Heck Hacks - [gamebadge3 Catskillvania: Condo of Bud](https://www.youtube.com/shorts/jR1i1R3pn3c)
  * What's Ken Making - [The New 2024 gameBadge | Episode 1: Design Overview](https://www.youtube.com/watch?v=2F5WBwUce5I)
  * What's Ken Making - [The New 2024 gameBadge | Episode 2: Step-by-Step Build](https://www.youtube.com/watch?v=bGKaDP4sNoU)
  * What's Ken Making - [The New 2024 gameBadge | Episode 3: Games & Emulators](https://www.youtube.com/watch?v=j67-PjNnY_U)
