#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>

using namespace std;

const int WIDTH = 80;
const int HEIGHT = 40;
const double FOV = 60.0;
const double SCALE_X = 40.0;
const double SCALE_Y = 20.0;

struct Point3D {
    double x, y, z;
};

struct Point2D {
    int x, y;
};

struct Edge {
    int u, v;
    char symbol;
};

void clear_screen() {
    cout << "\033[H\033[J";
}

Point2D project(Point3D p) {
    double z_offset = p.z + 4.0; 
    
    int screenX = static_cast<int>(WIDTH / 2 + (p.x * FOV / z_offset) * (SCALE_X / 40.0));
    int screenY = static_cast<int>(HEIGHT / 2 - (p.y * FOV / z_offset) * (SCALE_Y / 20.0));
    
    return {screenX, screenY};
}

// Applies local X-Z spin first, then rotates around Y-axis
Point3D rotate_xz_then_y(Point3D p, double spinXZ, double rotY) {
    // 1. LOCAL SPIN (X-Z plane spin)
    // Rotate around Z axis
    double x1 = p.x * cos(spinXZ) - p.y * sin(spinXZ);
    double y1 = p.x * sin(spinXZ) + p.y * cos(spinXZ);
    double z1 = p.z;

    // Rotate around X axis
    double y2 = y1 * cos(spinXZ) - z1 * sin(spinXZ);
    double z2 = y1 * sin(spinXZ) + z1 * cos(spinXZ);
    double x2 = x1;

    // 2. GLOBAL ROTATION around Y-axis
    double x3 = x2 * cos(rotY) + z2 * sin(rotY);
    double z3 = -x2 * sin(rotY) + z2 * cos(rotY);
    double y3 = y2;

    return {x3, y3, z3};
}

void draw_line(vector<string>& buffer, Point2D p1, Point2D p2, char symbol) {
    int x1 = p1.x, y1 = p1.y;
    int x2 = p2.x, y2 = p2.y;

    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (x1 >= 0 && x1 < WIDTH && y1 >= 0 && y1 < HEIGHT) {
            buffer[y1][x1] = symbol;
        }

        if (x1 == x2 && y1 == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

int main() {
    vector<Point3D> cubeVertices = {
        {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
        {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
    };

    vector<Edge> cubeEdges = {
        {0,1,'#'}, {1,2,'#'}, {2,3,'#'}, {3,0,'#'},
        {4,5,'#'}, {5,6,'#'}, {6,7,'#'}, {7,4,'#'},
        {0,4,'#'}, {1,5,'#'}, {2,6,'#'}, {3,7,'#'}
    };

    // Origin + 3 Axis Tips
    vector<Point3D> axisVertices = {
        {0, 0, 0},
        {2, 0, 0}, // X
        {0, 2, 0}, // Y
        {0, 0, 2}  // Z
    };

    vector<Edge> axisEdges = {
        {0, 1, 'X'},
        {0, 2, 'Y'},
        {0, 3, 'Z'}
    };

    double spinXZ = 0.0; // Fast local spin on X-Z plane
    double rotY = 0.0;   // Slower rotation around Y-axis

    while (true) {
        vector<string> buffer(HEIGHT, string(WIDTH, ' '));

        // Render Cube
        for (const auto& edge : cubeEdges) {
            Point3D p1_3d = rotate_xz_then_y(cubeVertices[edge.u], spinXZ, rotY);
            Point3D p2_3d = rotate_xz_then_y(cubeVertices[edge.v], spinXZ, rotY);

            Point2D p1 = project(p1_3d);
            Point2D p2 = project(p2_3d);
            draw_line(buffer, p1, p2, edge.symbol);
        }

        // Render Axes
        for (const auto& edge : axisEdges) {
            Point3D p1_3d = rotate_xz_then_y(axisVertices[edge.u], spinXZ, rotY);
            Point3D p2_3d = rotate_xz_then_y(axisVertices[edge.v], spinXZ, rotY);

            Point2D p1 = project(p1_3d);
            Point2D p2 = project(p2_3d);
            draw_line(buffer, p1, p2, edge.symbol);
        }

        clear_screen();
        for (const string& row : buffer) {
            cout << row << "\n";
        }

        // Increment angles at different rates
        spinXZ += 0.08; // Faster local spin rate
        rotY += 0.03;   // Y-axis rotation rate

        this_thread::sleep_for(chrono::milliseconds(33));
    }

    return 0;
}