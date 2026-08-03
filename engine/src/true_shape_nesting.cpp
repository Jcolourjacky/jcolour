#include "true_shape_nesting.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <chrono>
#include <iostream>

namespace jcolour {
namespace nesting {

struct TrueShapeNester::Impl {
    Config config;
    std::mt19937 rng;

    Impl() {
        rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
    }
};

TrueShapeNester::TrueShapeNester() : pImpl_(new Impl) {
    pImpl_->config = Config{297, 420};  // Default A3
}

TrueShapeNester::~TrueShapeNester() = default;

void TrueShapeNester::set_config(const Config& config) {
    pImpl_->config = config;
}

std::vector<Polygon> TrueShapeNester::rotate_items(
    const std::vector<PrintItem>& items) const {

    std::vector<Polygon> polygons;
    polygons.reserve(items.size());

    for (const auto& item : items) {
        Polygon poly;
        poly.id = std::stoi(item.id.toStdString());
        poly.rotation = 0;
        poly.vertices = {
            {0, 0},
            {item.bounds.width(), 0},
            {item.bounds.width(), item.bounds.height()},
            {0, item.bounds.height()}
        };

        if (pImpl_->config.allow_rotation && item.rotation != 0) {
            poly.vertices = poly.get_rotated(item.rotation * M_PI / 180.0);
            poly.rotation = item.rotation;
        }

        polygons.push_back(poly);
    }

    return polygons;
}

NestingResult TrueShapeNester::nest(const std::vector<PrintItem>& items) {
    NestingResult result;
    result.pagesRequired = 1;

    const auto& cfg = pImpl_->config;
    double usable_width = cfg.sheet_width - cfg.margin_left - cfg.margin_right;
    double usable_height = cfg.sheet_height - cfg.margin_top - cfg.margin_bottom;

    auto polygons = rotate_items(items);
    std::vector<Polygon> placed;
    double current_y = cfg.margin_top;
    double current_x = cfg.margin_left;
    double max_height_in_row = 0;

    for (const auto& poly : polygons) {
        AABB bounds = poly.bounding_box();
        double poly_width = bounds.width();
        double poly_height = bounds.height();

        // Check if fits in current row
        if (current_x + poly_width > cfg.sheet_width - cfg.margin_right) {
            // Move to next row
            current_x = cfg.margin_left;
            current_y += max_height_in_row + cfg.gutter;
            max_height_in_row = 0;
        }

        // Check if fits on current page
        if (current_y + poly_height > cfg.sheet_height - cfg.margin_bottom) {
            // Start new page
            result.pagesRequired++;
            current_x = cfg.margin_left;
            current_y = cfg.margin_top;
            max_height_in_row = 0;
        }

        // Place polygon
        Point2D pos(current_x - bounds.min_x, current_y - bounds.min_y);
        Polygon placed_poly = poly;

        // Offset vertices
        for (auto& v : placed_poly.vertices) {
            v.x += pos.x;
            v.y += pos.y;
        }

        placed.push_back(placed_poly);

        // Update position
        current_x += poly_width + cfg.gutter;
        max_height_in_row = std::max(max_height_in_row, poly_height);

        // Add to result
        PrintItem placed_item;
        placed_item.id = QString::number(poly.id);
        placed_item.bounds = QRectF(
            pos.x, pos.y,
            bounds.width(), bounds.height());
        placed_item.rotation = poly.rotation;
        result.placedItems.push_back(placed_item);
    }

    result.utilization = calculate_utilization(placed, cfg);

    return result;
}

bool TrueShapeNester::can_place(
    const Polygon& poly,
    const Point2D& pos,
    double rotation,
    const std::vector<Polygon>& placed,
    double sheet_width, double sheet_height,
    const Config& config) const {

    // Check bounds
    AABB bounds = poly.bounding_box();
    if (pos.x + bounds.max_x > sheet_width - config.margin_right ||
        pos.y + bounds.max_y > sheet_height - config.margin_bottom ||
        pos.x + bounds.min_x < config.margin_left ||
        pos.y + bounds.min_y < config.margin_top) {
        return false;
    }

    // Check collision with placed polygons
    for (const auto& p : placed) {
        if (bounds.intersects(p.bounding_box(), config.gutter)) {
            // More detailed collision check
            for (const auto& v1 : poly.vertices) {
                Point2D vp(v1.x + pos.x, v1.y + pos.y);
                if (p.bounding_box().contains(vp)) {
                    return false;
                }
            }
        }
    }

    return true;
}

Point2D TrueShapeNester::find_best_position(
    const Polygon& poly,
    const std::vector<Polygon>& placed,
    const Config& config) const {

    Point2D best_pos;
    double best_score = -1e10;

    // Try different positions
    for (double y = config.margin_top; y < config.sheet_height - config.margin_bottom; y += 10) {
        for (double x = config.margin_left; x < config.sheet_width - config.margin_right; x += 10) {
            if (can_place(poly, Point2D(x, y), 0, placed,
                        config.sheet_width, config.sheet_height, config)) {
                // Score based on placement (prefer bottom-left packing)
                double score = -y * 0.1 - x * 0.01;
                if (score > best_score) {
                    best_score = score;
                    best_pos = Point2D(x, y);
                }
            }
        }
    }

    return best_pos;
}

double TrueShapeNester::calculate_utilization(
    const std::vector<Polygon>& placed,
    const Config& config) const {

    double total_area = 0;
    for (const auto& p : placed) {
        total_area += p.area();
    }

    double sheet_area = (config.sheet_width - config.margin_left - config.margin_right) *
                       (config.sheet_height - config.margin_top - config.margin_bottom);

    return total_area / sheet_area;
}

double Polygon::area() const {
    if (vertices.size() < 3) return 0;

    double sum = 0;
    for (size_t i = 0; i < vertices.size(); ++i) {
        size_t j = (i + 1) % vertices.size();
        sum += vertices[i].x * vertices[j].y;
        sum -= vertices[j].x * vertices[i].y;
    }
    return std::abs(sum) / 2.0;
}

Point2D Polygon::centroid() const {
    double cx = 0, cy = 0;
    for (const auto& v : vertices) {
        cx += v.x;
        cy += v.y;
    }
    if (!vertices.empty()) {
        cx /= vertices.size();
        cy /= vertices.size();
    }
    return Point2D(cx, cy);
}

std::vector<Point2D> Polygon::get_rotated(double angle) const {
    std::vector<Point2D> rotated;
    rotated.reserve(vertices.size());

    Point2D c = centroid();
    double cos_a = std::cos(angle);
    double sin_a = std::sin(angle);

    for (const auto& v : vertices) {
        double dx = v.x - c.x;
        double dy = v.y - c.y;
        rotated.push_back(Point2D(
            c.x + dx * cos_a - dy * sin_a,
            c.y + dx * sin_a + dy * cos_a
        ));
    }

    return rotated;
}

AABB Polygon::bounding_box() const {
    AABB box{0, 0, 0, 0};
    if (vertices.empty()) return box;

    box.min_x = box.max_x = vertices[0].x;
    box.min_y = box.max_y = vertices[0].y;

    for (const auto& v : vertices) {
        box.min_x = std::min(box.min_x, v.x);
        box.max_x = std::max(box.max_x, v.x);
        box.min_y = std::min(box.min_y, v.y);
        box.max_y = std::max(box.max_y, v.y);
    }

    return box;
}

bool AABB::intersects(const AABB& other, double margin) const {
    return !(max_x + margin < other.min_x ||
             min_x - margin > other.max_x ||
             max_y + margin < other.min_y ||
             min_y - margin > other.max_y);
}

bool AABB::contains(const Point2D& p) const {
    return p.x >= min_x && p.x <= max_x &&
           p.y >= min_y && p.y <= max_y;
}

std::vector<Point2D> ConvexHull::compute(const std::vector<Point2D>& points) {
    if (points.size() < 3) return points;

    std::vector<Point2D> pts = points;
    std::sort(pts.begin(), pts.end(), [](const Point2D& a, const Point2D& b) {
        if (std::abs(a.x - b.x) < 1e-10) return a.y < b.y;
        return a.x < b.x;
    });

    std::vector<Point2D> hull;

    // Build lower hull
    for (const auto& p : pts) {
        while (hull.size() >= 2) {
            auto& p1 = hull[hull.size() - 2];
            auto& p2 = hull[hull.size() - 1];
            double cross = (p2.x - p1.x) * (p.y - p1.y) -
                         (p2.y - p1.y) * (p.x - p1.x);
            if (cross <= 0) hull.pop_back();
            else break;
        }
        hull.push_back(p);
    }

    // Build upper hull
    size_t lower_size = hull.size();
    for (int i = pts.size() - 2; i >= 0; --i) {
        const auto& p = pts[i];
        while (hull.size() > lower_size) {
            auto& p1 = hull[hull.size() - 2];
            auto& p2 = hull[hull.size() - 1];
            double cross = (p2.x - p1.x) * (p.y - p1.y) -
                         (p2.y - p1.y) * (p.x - p1.x);
            if (cross <= 0) hull.pop_back();
            else break;
        }
        hull.push_back(p);
    }

    if (!hull.empty()) hull.pop_back();
    return hull;
}

double ConvexHull::area(const std::vector<Point2D>& hull) {
    if (hull.size() < 3) return 0;

    double sum = 0;
    for (size_t i = 0; i < hull.size(); ++i) {
        size_t j = (i + 1) % hull.size();
        sum += hull[i].x * hull[j].y;
        sum -= hull[j].x * hull[i].y;
    }
    return std::abs(sum) / 2.0;
}

struct PlacementOptimizer::Impl {
    std::mt19937 rng;

    Impl() {
        rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
    }
};

PlacementOptimizer::PlacementOptimizer() : pImpl_(new Impl) {}
PlacementOptimizer::~PlacementOptimizer() = default;

std::vector<Placement> PlacementOptimizer::optimize_placement(
    const std::vector<PrintItem>& items,
    const TrueShapeNester::Config& config) {

    std::vector<Placement> placements;

    // Simple first-fit decreasing height algorithm
    std::vector<PrintItem> sorted = items;
    std::sort(sorted.begin(), sorted.end(), [](const PrintItem& a, const PrintItem& b) {
        return a.bounds.height() > b.bounds.height();
    });

    for (const auto& item : sorted) {
        Placement p;
        p.item_id = std::stoi(item.id.toStdString());
        p.position = Point2D(config.margin_left, config.margin_top);
        p.rotation = 0;
        p.success = true;
        placements.push_back(p);
    }

    return placements;
}

}
}
