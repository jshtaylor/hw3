#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
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

// Target Frame Rate Parameters (60 FPS = ~16.66ms per frame)
const auto TARGET_FRAME_DURATION = chrono::microseconds(16666);

struct Point3D {
    double x, y, z;
};

struct Point2D {
    int x, y;
};

struct Edge {
    int u, v;
};

struct Shape {
    string name;
    vector<Point3D> vertices;
    vector<Edge> edges;
};

struct Line3D {
    Point3D start, end;
};

// 3D Perspective Projection to 2D Window Coordinates
Point2D project(Point3D p) {
    double z = p.z;
    if (z <= 0.1) z = 0.1; // Prevent division by zero or negative clipping
    int screenX = static_cast<int>(WIDTH / 2 + (p.x * FOV / z));
    int screenY = static_cast<int>(HEIGHT / 2 - (p.y * FOV / z)); // Invert Y for screen space
    return {screenX, screenY};
}

// Rotate object around its own local origin
Point3D rotate_object(Point3D p, double rotY, double rotZ) {
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

// Transform world coordinates into camera space (camera orbits around world origin)
Point3D worldToCamera(Point3D p, double camDist, double camAngleY) {
    // 1. Calculate camera position on a circle around origin
    double camX = camDist * sin(camAngleY);
    double camZ = -camDist * cos(camAngleY);

    // 2. Translate point relative to camera position
    double tx = p.x - camX;
    double ty = p.y;
    double tz = p.z - camZ;

    // 3. Rotate world into camera view orientation
    double cosYaw = cos(-camAngleY);
    double sinYaw = sin(-camAngleY);

    double rx = tx * cosYaw + tz * sinYaw;
    double ry = ty;
    double rz = -tx * sinYaw + tz * cosYaw;

    return {rx, ry, rz};
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

    // 2. Define Geometries (Shapes remain at 0.5x scale)
    vector<Shape> shapes;

    // --- Shape 0: Cube ---
    shapes.push_back({
        "Cube",
        {
            {-0.5, -0.5, -0.5}, { 0.5, -0.5, -0.5}, { 0.5,  0.5, -0.5}, {-0.5,  0.5, -0.5},
            {-0.5, -0.5,  0.5}, { 0.5, -0.5,  0.5}, { 0.5,  0.5,  0.5}, {-0.5,  0.5,  0.5}
        },
        {
            {0,1}, {1,2}, {2,3}, {3,0},
            {4,5}, {5,6}, {6,7}, {7,4},
            {0,4}, {1,5}, {2,6}, {3,7}
        }
    });

    // --- Shape 1: Square-Based Pyramid ---
    shapes.push_back({
        "Square-Based Pyramid",
        {
            {-0.5, -0.5, -0.5}, { 0.5, -0.5, -0.5}, { 0.5, -0.5,  0.5}, {-0.5, -0.5,  0.5}, // Base
            { 0.0,  0.6,  0.0}                                                             // Apex
        },
        {
            {0,1}, {1,2}, {2,3}, {3,0}, // Base edges
            {0,4}, {1,4}, {2,4}, {3,4}  // Side edges to apex
        }
    });

    // --- Shape 2: Triangular Pyramid (Tetrahedron) ---
    shapes.push_back({
        "Triangle-Based Pyramid",
        {
            { 0.0,  0.6,  0.0},      // Apex
            {-0.5, -0.4, -0.35},     // Base vertex 1
            { 0.5, -0.4, -0.35},     // Base vertex 2
            { 0.0, -0.4,  0.5}       // Base vertex 3
        },
        {
            {1,2}, {2,3}, {3,1},     // Base edges
            {0,1}, {0,2}, {0,3}      // Side edges to apex
        }
    });

    // World Space 3D Coordinate Axes passing through Origin (0,0,0) (Doubled length)
    vector<Line3D> axes3D = {
        {{-2.50,  0.00,  0.00}, {2.50, 0.00, 0.00}}, // X Axis
        {{ 0.00, -2.50,  0.00}, {0.00, 2.50, 0.00}}, // Y Axis
        {{-1.50, -1.50, -1.50}, {1.50, 1.50, 1.50}}  // Z Axis
    };

    size_t currentShapeIndex = 0;
    double rotY = 0.0;
    double rotZ = 0.0;

    // Camera parameters
    double camDist = 4.5;    // Distance from world origin
    double camAngleY = 0.0;  // Orbital camera angle around Y-axis

    // Visibility toggles
    bool showAxes = true;
    bool running = true;

    // Array of colors for shape edges
    unsigned long edgeColors[] = { red.pixel, green.pixel, blue.pixel };

    // 3. Animation Loop
    while (running) {
        auto frameStart = chrono::high_resolution_clock::now();

        // Handle Input Events
        while (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == wmDeleteMessage) {
                running = false;
            }
            if (event.type == KeyPress) {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                if (keysym == XK_s || keysym == XK_S) {
                    currentShapeIndex = (currentShapeIndex + 1) % shapes.size();
                } else if (keysym == XK_Left) {
                    camAngleY -= 0.05; // Orbit camera left around fixed world origin
                } else if (keysym == XK_Right) {
                    camAngleY += 0.05; // Orbit camera right around fixed world origin
                } else if (keysym == XK_x || keysym == XK_X) {
                    showAxes = !showAxes; // Toggle axes visibility
                } else if (keysym == XK_Escape || keysym == XK_q || keysym == XK_Q) {
                    running = false;
                }
            }
        }

        // Clear Off-Screen Pixmap Buffer with White
        XSetForeground(display, gc, white.pixel);
        XFillRectangle(display, pixmap, gc, 0, 0, WIDTH, HEIGHT);

        // --- Render Thicker Black 3D World Axes Transformed by Camera ---
        if (showAxes) {
            XSetLineAttributes(display, gc, 3, LineSolid, CapButt, JoinMiter);
            XSetForeground(display, gc, black.pixel);

            for (const auto& axis : axes3D) {
                Point3D startCam = worldToCamera(axis.start, camDist, camAngleY);
                Point3D endCam = worldToCamera(axis.end, camDist, camAngleY);

                Point2D p1 = project(startCam);
                Point2D p2 = project(endCam);

                XDrawLine(display, pixmap, gc, p1.x, p1.y, p2.x, p2.y);
            }
        }

        // Reset line thickness to 2px for shape edges
        XSetLineAttributes(display, gc, 2, LineSolid, CapButt, JoinMiter);

        // --- Render Active Shape Transformed into Camera Space ---
        const auto& activeShape = shapes[currentShapeIndex];

        for (size_t i = 0; i < activeShape.edges.size(); ++i) {
            const auto& edge = activeShape.edges[i];

            XSetForeground(display, gc, edgeColors[i % 3]);

            // 1. Rotate vertex around object's local origin
            Point3D p1_world = rotate_object(activeShape.vertices[edge.u], rotY, rotZ);
            Point3D p2_world = rotate_object(activeShape.vertices[edge.v], rotY, rotZ);

            // 2. Transform world point into camera view space
            Point3D p1_cam = worldToCamera(p1_world, camDist, camAngleY);
            Point3D p2_cam = worldToCamera(p2_world, camDist, camAngleY);

            // 3. Project camera-relative coordinates to 2D screen space
            Point2D p1 = project(p1_cam);
            Point2D p2 = project(p2_cam);

            XDrawLine(display, pixmap, gc, p1.x, p1.y, p2.x, p2.y);
        }

        // Copy Pixmap to Window (Double Buffering Swap)
        XCopyArea(display, pixmap, window, gc, 0, 0, WIDTH, HEIGHT, 0, 0);
        XFlush(display);

        // Update Rotation Angles
        rotY += 0.015;
        rotZ += 0.01;

        // Frame rate limiter (Lock strictly to 60 FPS)
        auto frameEnd = chrono::high_resolution_clock::now();
        auto frameDuration = chrono::duration_cast<chrono::microseconds>(frameEnd - frameStart);

        if (frameDuration < TARGET_FRAME_DURATION) {
            this_thread::sleep_for(TARGET_FRAME_DURATION - frameDuration);
        }
    }

    // Cleanup Resources
    XFreePixmap(display, pixmap);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);

    return 0;
}