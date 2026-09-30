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
    char label; // 'X', 'Y', 'Z', or '#' for cube
};

// 3D Perspective Projection to 2D Window Coordinates
Point2D project(Point3D p) {
    double z_offset = p.z + 4.5; // Offset to keep shape in front of camera
    if (z_offset <= 0.1) z_offset = 0.1; // Prevent division by zero or negative clipping
    int screenX = static_cast<int>(WIDTH / 2 + (p.x * FOV / z_offset));
    int screenY = static_cast<int>(HEIGHT / 2 - (p.y * FOV / z_offset)); // Invert Y for screen space
    return {screenX, screenY};
}

// Rotation: Standard X-axis spin, Z-axis spin, then Y-axis global rotation
Point3D rotate_xz_then_y(Point3D p, double spinXZ, double rotY) {
    // 1. Rotation around X-axis
    double y1 = p.y * cos(spinXZ) - p.z * sin(spinXZ);
    double z1 = p.y * sin(spinXZ) + p.z * cos(spinXZ);
    double x1 = p.x;

    // 2. Rotation around Z-axis
    double x2 = x1 * cos(spinXZ) - y1 * sin(spinXZ);
    double y2 = x1 * sin(spinXZ) + y1 * cos(spinXZ);
    double z2 = z1;

    // 3. Global Rotation around Y-axis
    double x3 = x2 * cos(rotY) + z2 * sin(rotY);
    double z3 = -x2 * sin(rotY) + z2 * cos(rotY);
    double y3 = y2;

    return {x3, y3, z3};
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

    // Create Window
    Window window = XCreateSimpleWindow(
        display, root, 100, 100, WIDTH, HEIGHT, 1,
        BlackPixel(display, screen), BlackPixel(display, screen)
    );

    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);

    // Setup Graphics Context & Colors
    GC gc = XCreateGC(display, window, 0, NULL);
    
    Colormap colormap = DefaultColormap(display, screen);
    XColor white, red, green, blue;
    XAllocNamedColor(display, colormap, "white", &white, &white);
    XAllocNamedColor(display, colormap, "#FF4444", &red, &red);
    XAllocNamedColor(display, colormap, "#44FF44", &green, &green);
    XAllocNamedColor(display, colormap, "#4488FF", &blue, &blue);

    // Create Off-Screen Buffer (Pixmap) for smooth rendering
    Pixmap pixmap = XCreatePixmap(display, window, WIDTH, HEIGHT, DefaultDepth(display, screen));

    // Handle Window Close Event
    Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wmDeleteMessage, 1);

    // 2. Define Shape Geometry
    vector<Point3D> cubeVertices = {
        {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
        {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
    };

    vector<Edge> cubeEdges = {
        {0,1,'#'}, {1,2,'#'}, {2,3,'#'}, {3,0,'#'},
        {4,5,'#'}, {5,6,'#'}, {6,7,'#'}, {7,4,'#'},
        {0,4,'#'}, {1,5,'#'}, {2,6,'#'}, {3,7,'#'}
    };

    // Axes
    vector<Point3D> axisVertices = {
        {0, 0, 0},
        {2.2, 0, 0}, // X Tip
        {0, 2.2, 0}, // Y Tip
        {0, 0, 2.2}  // Z Tip
    };

    vector<Edge> axisEdges = {
        {0, 1, 'X'},
        {0, 2, 'Y'},
        {0, 3, 'Z'}
    };

    double spinXZ = 0.0;
    double rotY = 0.0;
    bool running = true;

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

        // Clear Off-Screen Pixmap Buffer
        XSetForeground(display, gc, BlackPixel(display, screen));
        XFillRectangle(display, pixmap, gc, 0, 0, WIDTH, HEIGHT);

        // --- Render Cube ---
        XSetForeground(display, gc, white.pixel);
        for (const auto& edge : cubeEdges) {
            Point3D p1_3d = rotate_xz_then_y(cubeVertices[edge.u], spinXZ, rotY);
            Point3D p2_3d = rotate_xz_then_y(cubeVertices[edge.v], spinXZ, rotY);

            Point2D p1 = project(p1_3d);
            Point2D p2 = project(p2_3d);

            XDrawLine(display, pixmap, gc, p1.x, p1.y, p2.x, p2.y);
        }

        // --- Render Axes with Labels ---
        for (const auto& edge : axisEdges) {
            Point3D p1_3d = rotate_xz_then_y(axisVertices[edge.u], spinXZ, rotY);
            Point3D p2_3d = rotate_xz_then_y(axisVertices[edge.v], spinXZ, rotY);

            Point2D p1 = project(p1_3d);
            Point2D p2 = project(p2_3d);

            // Set Axis Color
            if (edge.label == 'X') XSetForeground(display, gc, red.pixel);
            else if (edge.label == 'Y') XSetForeground(display, gc, green.pixel);
            else if (edge.label == 'Z') XSetForeground(display, gc, blue.pixel);

            // Draw Axis Line
            XDrawLine(display, pixmap, gc, p1.x, p1.y, p2.x, p2.y);

            // Draw Axis Text Label
            string labelStr(1, edge.label);
            XDrawString(display, pixmap, gc, p2.x + 5, p2.y + 5, labelStr.c_str(), 1);
        }

        // Copy Pixmap to Window (Double Buffering Swap)
        XCopyArea(display, pixmap, window, gc, 0, 0, WIDTH, HEIGHT, 0, 0);
        XFlush(display);

        // Update Angles
        spinXZ += 0.05;
        rotY += 0.02;

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