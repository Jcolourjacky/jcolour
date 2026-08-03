#pragma once

#include "nesting.h"
#include <vector>
#include <array>
#include <memory>

namespace jcolour {
namespace nesting {

struct Point2D {
    double x, y;
    Point2D() : x(0), y(0) {}
    Point2D(double x_, double y_) : x(x_), y(y_) {}
};

struct Polygon {
    std::vector<Point2D> vertices;
    int id;
    double rotation;
    double area() const;
    Point2D centroid() const;
    std::vector<Point2D> get_rotated(double angle) const;
    AABB bounding_box() const;
};

struct AABB {
    double min_x, min_y, max_x, max_y;
    double width() const { return max_x - min_x; }
    double height() const { return max_y - min_y; }
    double area() const { return width() * height(); }
    bool intersects(const AABB& other, double margin = 0) const;
    bool contains(const Point2D& p) const;
};

class TrueShapeNester {
public:
    TrueShapeNester();
    ~TrueShapeNester();

    struct Config {
        double sheet_width;
        double sheet_height;
        double margin_top = 10;
        double margin_right = 10;
        double margin_bottom = 10;
        double margin_left = 10;
        double gutter = 5;
        bool allow_rotation = true;
        int max_iterations = 1000;
        double temperature = 1.0;  // For simulated annealing
    };

    void set_config(const Config& config);
    NestingResult nest(const std::vector<PrintItem>& items);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;

    bool can_place(const Polygon& poly, const Point2D& pos,
                   double rotation,
                   const std::vector<Polygon>& placed,
                   double sheet_width, double sheet_height,
                   const Config& config) const;

    Point2D find_best_position(const Polygon& poly,
                              const std::vector<Polygon>& placed,
                              const Config& config) const;

    double calculate_utilization(const std::vector<Polygon>& placed,
                               const Config& config) const;

    std::vector<Polygon> rotate_items(const std::vector<PrintItem>& items) const;
};

class ConvexHull {
public:
    static std::vector<Point2D> compute(const std::vector<Point2D>& points);
    static double area(const std::vector<Point2D>& hull);
};

class NoFitPolygon {
public:
    static std::vector<Point2D> compute(const Polygon& a, const Polygon& b,
                                        bool rotate_a = false, bool rotate_b = false);
    static bool place_without_collision(const Polygon& a, const Polygon& b,
                                      const Point2D& offset);
};

class ClipperWrapper {
public:
    static std::vector<Polygon> union_all(const std::vector<Polygon>& polygons);
    static std::vector<Polygon> subtract(const Polygon& a, const Polygon& b);
    static double overlap_area(const Polygon& a, const Polygon& b);
};

struct Placement {
    int item_id;
    Point2D position;
    double rotation;
    bool success;
};

class PlacementOptimizer {
public:
    PlacementOptimizer();
    ~PlacementOptimizer();

    std::vector<Placement> optimize_placement(
        const std::vector<PrintItem>& items,
        const TrueShapeNester::Config& config);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

}
}

// Clipper library bindings
#ifdef USE_CLIPPER
#include <clipper.hpp>
namespace jcolour {
namespace nesting {

using ClipperPath = std::vector<IntPoint>;
using ClipperPaths = std::vector<ClipperPath>;

class ClipperAdapter {
public:
    static ClipperPath polygon_to_path(const Polygon& poly, int scale = 1000);
    static Polygon path_to_polygon(const ClipperPath& path, int scale = 1000);
    static ClipperPaths union_paths(const ClipperPaths& paths);
    static ClipperPaths offset_path(const ClipperPath& path, double delta);
};

}
}
#endif
