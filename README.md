# hw3
hw3 shape render


Makefile - has make and make clean as fucntion,due to nature of this project when using make it immediately run the executable, make clean removes the executable from the file.

3D-floating-shape.cpp - usese xlib11 GUI to render 3 different shapes at the origin R3, rotates the shapes about the z and y axis. frame rate is capped at 60 fps, using "s" key you can cycle between triangular pyramid, square pyramid, and cube. Using "x" you can hide the axis lable and using left and right arrow keys you can rotate the camera. 