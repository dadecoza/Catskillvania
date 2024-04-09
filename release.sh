#!/bin/bash

rm -rf release
mkdir -p release
ldd catskill.exe | grep '\/mingw.*\.dll' -o | xargs -I{} cp "{}" release
cp -vr audio sprites condo hallway levels story title UI catskill.exe release
echo done