#!/bin/bash
make clean && \
make && \
rm -rf release && \
mkdir -p release && \
ldd catskill.exe | grep '\/mingw.*\.dll' -o | xargs -I{} cp "{}" release && \
cp -vr audio music sprites condo hallway levels story title UI catskill.exe release && \
rm -f release/UI/Catskill.gif && \
make clean
echo done