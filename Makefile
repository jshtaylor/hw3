all:
	g++ -Wall -O3 3D-floating-shape.cpp -o shape -lX11
clean:
	rm -f shape
