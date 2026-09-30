#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>
#include <string>

using namespace std;

// Window dimensions
const int WIDTH = 800;
const int HEIGHT = 600;
const double FOV = 400.0; // Field of view scaling

struct Point3D {
    double x, y, z;
};

struct Point2D {
    int x, y;
};

struct Edge {
    int u, v;
};

// 3D Perspective Projection to 2D Window Coordinates
Point2D project(Point3D p) {
    double z_offset = p.z + 4.5; // Offset to keep shape in front of camera
    if (z_offset <= 0.1) z_offset = 0.1; // Prevent division by zero or negative clipping
    int screenX = static_cast<int>(WIDTH / 2 + (p.x * FOV / z_offset));
    int screenY = static_cast<int>(HEIGHT / 2 - (p.y * FOV / z_offset)); // Invert Y for screen space
    return {screenX, screenY};
}

// Rotation: Standard Y-axis rotation followed by Z-axis rotation
Point3D rotate_y_z(Point3D p, double rotY, double rotZ) {
    // 1. Rotation around Y-axis
    double x1 = p.x * cos(rotY) + p.z * sin(rotY);
    double y1 = p.y;
    double z1 = -p.x * sin(rotY) + p.z * cos(rotY);

    // 2. Rotation around Z-axis
    double x2 = x1 * cos(rotZ) - y1 * sin(rotZ);
    double y2 = x1 * sin(rotZ) + y1 * cos(rotZ);
    double z2 = z1;

    return {x2, y2, z2};
}

int main() {
    // 1. Initialize X Display
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        cerr << "Error: Unable to open X Display." << endl;
        return 1;
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    // Create Window with White Background
    Window window = XCreateSimpleWindow(
        display, root, 100, 100, WIDTH, HEIGHT, 1,
        BlackPixel(display, screen), WhitePixel(display, screen)
    );

    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);

    // Setup Graphics Context & Colors
    GC gc = XCreateGC(display, window, 0, NULL);
    
    Colormap colormap = DefaultColormap(display, screen);
    XColor white, black, red, green, blue;
    XAllocNamedColor(display, colormap, "white", &white, &white);
    XAllocNamedColor(display, colormap, "black", &black, &black);
    XAllocNamedColor(display, colormap, "#FF0000", &red, &red);
    XAllocNamedColor(display, colormap, "#00AA00", &green, &green);
    XAllocNamedColor(display, colormap, "#0000FF", &blue, &blue);

    // Create Off-Screen Buffer (Pixmap) for smooth rendering
    Pixmap pixmap = XCreatePixmap(display, window, WIDTH, HEIGHT, DefaultDepth(display, screen));

    // Handle Window Close Event
    Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wmDeleteMessage, 1);

    // 2. Define Cube Geometry
    vector<Point3D> cubeVertices = {
        {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
        {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
    };

    vector<Edge> cubeEdges = {
        {0,1}, {1,2}, {2,3}, {3,0},
        {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };

    double rotY = 0.0;
    double rotZ = 0.0;
    bool running = true;

    // Center coordinates for screen axes
    int originX = WIDTH / 2;
    int originY = HEIGHT / 2;

    // Array of colors for cube edges
    unsigned long edgeColors[] = { red.pixel, green.pixel, blue.pixel };

    // 3. Animation Loop
    while (running) {
        // Handle Input Events
        while (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == wmDeleteMessage) {
                running = false;
            }
            if (event.type == KeyPress) {
                running = false; // Any key press exits
            }
        }

        // Clear Off-Screen Pixmap Buffer with White
        XSetForeground(display, gc, white.pixel);
        XFillRectangle(display, pixmap, gc, 0, 0, WIDTH, HEIGHT);

        // --- Render Thicker Black X & Y Axes ---
        XSetLineAttributes(display, gc, 3, LineSolid, CapButt, JoinMiter); // Set line thickness to 3px
        XSetForeground(display, gc, black.pixel);

        // Horizontal X Axis Line
        XDrawLine(display, pixmap, gc, 0, originY, WIDTH, originY);
        XDrawString(display, pixmap, gc, WIDTH - 20, originY - 5, "X", 1);

        // Vertical Y Axis Line
        XDrawLine(display, pixmap, gc, originX, 0, originX, HEIGHT);
        XDrawString(display, pixmap, gc, originX + 8, 15, "Y", 1);

        // Reset line thickness to 2px for cube edges
        XSetLineAttributes(display, gc, 2, LineSolid, CapButt, JoinMiter);

        // --- Render Rotating Cube with RGB Edges ---
        for (size_t i = 0; i < cubeEdges.size(); ++i) {
            const auto& edge = cubeEdges[i];

            // Assign Red, Green, or Blue dynamically based on edge index
            XSetForeground(display, gc, edgeColors[i % 3]);

            Point3D p1_3d = rotate_y_z(cubeVertices[edge.u], rotY, rotZ);
            Point3D p2_3d = rotate_y_z(cubeVertices[edge.v], rotY, rotZ);

            Point2D p1 = project(p1_3d);
            Point2D p2 = project(p2_3d);

            XDrawLine(display, pixmap, gc, p1.x, p1.y, p2.x, p2.y);
        }

        // Copy Pixmap to Window (Double Buffering Swap)
        XCopyArea(display, pixmap, window, gc, 0, 0, WIDTH, HEIGHT, 0, 0);
        XFlush(display);

        // Update Rotation Angles (Halved speed: 0.015 & 0.01)
        rotY += 0.015;
        rotZ += 0.01;

        // Lock to ~60 FPS
        this_thread::sleep_for(chrono::milliseconds(16));
    }

    // Cleanup Resources
    XFreePixmap(display, pixmap);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);

    return 0;
}