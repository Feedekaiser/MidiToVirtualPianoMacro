# Midi to Virtual Piano

Fork of https://github.com/Stereo101/python-MidiToVirtualPianoMacro/


**WINDOWS ONLY!!!**

## Instruction:
Put your `midi` files in the `midi` folder, then run `MidiAutoPlayer.exe` after running `pyMIDI.py` to convert a `midi` file of your choosing to `song.txt`



## Compile:

Haven't test on anything other than `MSVC`, but it *should* work with `gnu` and `clang` with
```
clang++|g++ MidiAutoPlayer.cpp -lstdc++fs -lwinmm -o MidiAutoPlayer.exe
```

