#pragma once

// ============================================
// CLIPPER2 HEADER-ONLY LIBRARY INTEGRATION
// For polygon clipping in Chams system
// ============================================

#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <deque>

namespace Clipper2Lib {

// ============================================
// BASIC TYPES
// ============================================
struct Point64 {
    int64_t x;
    int64_t y;
    
    Point64() : x(0), y(0) {}
    Point64(int64_t x_, int64_t y_) : x(x_), y(y_) {}
    
    bool operator==(const Point64& other) const {
        return x == other.x && y == other.y;
    }
    
    bool operator!=(const Point64& other) const {
        return !(*this == other);
    }
};

struct PointD {
    double x;
    double y;
    
    PointD() : x(0.0), y(0.0) {}
    PointD(double x_, double y_) : x(x_), y(y_) {}
    
    bool operator==(const PointD& other) const {
        return x == other.x && y == other.y;
    }
};

using Path64 = std::vector<Point64>;
using Paths64 = std::vector<Path64>;
using PathD = std::vector<PointD>;
using PathsD = std::vector<PathD>;

// ============================================
// RECTANGLE STRUCTURE
// ============================================
template <typename T>
struct Rect {
    T left;
    T top;
    T right;
    T bottom;
    
    Rect() : left(0), top(0), right(0), bottom(0) {}
    Rect(T l, T t, T r, T b) : left(l), top(t), right(r), bottom(b) {}
    
    T Width() const { return right - left; }
    T Height() const { return bottom - top; }
    
    bool Contains(const Point64& pt) const {
        return pt.x >= left && pt.x <= right && pt.y >= top && pt.y <= bottom;
    }
};

using Rect64 = Rect<int64_t>;
using RectD = Rect<double>;

// ============================================
// FILL RULE
// ============================================
enum class FillRule {
    EvenOdd,
    NonZero,
    Positive,
    Negative
};

// ============================================
// CLIP TYPE
// ============================================
enum class ClipType {
    None,
    Intersection,
    Union,
    Difference,
    Xor
};

// ============================================
// PATH UTILITIES
// ============================================
inline Rect64 GetBounds(const Path64& path) {
    if (path.empty()) return Rect64();
    
    int64_t minX = path[0].x;
    int64_t maxX = path[0].x;
    int64_t minY = path[0].y;
    int64_t maxY = path[0].y;
    
    for (const auto& pt : path) {
        minX = std::min(minX, pt.x);
        maxX = std::max(maxX, pt.x);
        minY = std::min(minY, pt.y);
        maxY = std::max(maxY, pt.y);
    }
    
    return Rect64(minX, minY, maxX, maxY);
}

inline double CrossProduct(const Point64& a, const Point64& b, const Point64& c) {
    return static_cast<double>(b.x - a.x) * (c.y - a.y) - 
           static_cast<double>(b.y - a.y) * (c.x - a.x);
}

inline bool IsPointInPolygon(const Point64& pt, const Path64& polygon) {
    if (polygon.size() < 3) return false;
    
    int windingNumber = 0;
    size_t n = polygon.size();
    
    for (size_t i = 0; i < n; i++) {
        const Point64& curr = polygon[i];
        const Point64& next = polygon[(i + 1) % n];
        
        if (curr.y <= pt.y) {
            if (next.y > pt.y && CrossProduct(curr, next, pt) > 0)
                windingNumber++;
        } else {
            if (next.y <= pt.y && CrossProduct(curr, next, pt) < 0)
                windingNumber--;
        }
    }
    
    return windingNumber != 0;
}

// ============================================
// SIMPLIFIED CLIPPER FOR CHAMS
// ============================================
class Clipper64 {
public:
    Clipper64() : fill_rule_(FillRule::NonZero), precision_(100) {}
    
    void AddSubject(const Paths64& subjects) {
        for (const auto& path : subjects) {
            if (!path.empty()) subjects_.push_back(path);
        }
    }
    
    void AddSubject(const Path64& subject) {
        if (!subject.empty()) subjects_.push_back(subject);
    }
    
    void AddClip(const Paths64& clips) {
        for (const auto& path : clips) {
            if (!path.empty()) clips_.push_back(path);
        }
    }
    
    void AddClip(const Path64& clip) {
        if (!clip.empty()) clips_.push_back(clip);
    }
    
    Paths64 Execute(ClipType clip_type, FillRule fill_rule) {
        fill_rule_ = fill_rule;
        
        switch (clip_type) {
            case ClipType::Intersection:
                return ExecuteIntersection();
            case ClipType::Difference:
                return ExecuteDifference();
            case ClipType::Union:
                return ExecuteUnion();
            default:
                return subjects_;
        }
    }
    
    void Clear() {
        subjects_.clear();
        clips_.clear();
    }
    
private:
    Paths64 subjects_;
    Paths64 clips_;
    FillRule fill_rule_;
    int precision_;
    
    Paths64 ExecuteIntersection() {
        // Simplified intersection: return subjects that are inside clips
        Paths64 result;
        
        for (const auto& subject : subjects_) {
            for (const auto& clip : clips_) {
                if (PathsIntersect(subject, clip)) {
                    // Simplified: return the subject path clipped to clip bounds
                    Path64 clipped = ClipPathToRect(subject, GetBounds(clip));
                    if (!clipped.empty()) {
                        result.push_back(clipped);
                    }
                }
            }
        }
        
        return result;
    }
    
    Paths64 ExecuteDifference() {
        // Difference: subjects minus clips
        Paths64 result;
        
        for (const auto& subject : subjects_) {
            bool clipped = false;
            for (const auto& clip : clips_) {
                if (PathsIntersect(subject, clip)) {
                    clipped = true;
                    // For simplified difference, exclude subjects that intersect clips
                    break;
                }
            }
            if (!clipped) {
                result.push_back(subject);
            }
        }
        
        return result;
    }
    
    Paths64 ExecuteUnion() {
        // Simplified union: combine all paths
        Paths64 result = subjects_;
        for (const auto& clip : clips_) {
            result.push_back(clip);
        }
        return result;
    }
    
    bool PathsIntersect(const Path64& path1, const Path64& path2) {
        Rect64 bounds1 = GetBounds(path1);
        Rect64 bounds2 = GetBounds(path2);
        
        // Quick AABB check
        if (bounds1.right < bounds2.left || bounds1.left > bounds2.right ||
            bounds1.bottom < bounds2.top || bounds1.top > bounds2.bottom) {
            return false;
        }
        
        // Check if any point of path1 is inside path2
        for (const auto& pt : path1) {
            if (IsPointInPolygon(pt, path2)) return true;
        }
        
        // Check if any point of path2 is inside path1
        for (const auto& pt : path2) {
            if (IsPointInPolygon(pt, path1)) return true;
        }
        
        return false;
    }
    
    Path64 ClipPathToRect(const Path64& path, const Rect64& rect) {
        Path64 result;
        for (const auto& pt : path) {
            if (rect.Contains(pt)) {
                result.push_back(pt);
            }
        }
        return result;
    }
};

// ============================================
// POLYGON TRIANGULATION (FOR RENDERING)
// ============================================
struct Triangle {
    Point64 a, b, c;
    
    Triangle(const Point64& a_, const Point64& b_, const Point64& c_) 
        : a(a_), b(b_), c(c_) {}
};

inline std::vector<Triangle> TriangulatePolygon(const Path64& polygon) {
    std::vector<Triangle> triangles;
    
    if (polygon.size() < 3) return triangles;
    
    // Simple fan triangulation for convex polygons
    for (size_t i = 1; i < polygon.size() - 1; i++) {
        triangles.emplace_back(polygon[0], polygon[i], polygon[i + 1]);
    }
    
    return triangles;
}

// ============================================
// CONVERSION UTILITIES
// ============================================
inline Point64 ToPoint64(float x, float y, int precision = 100) {
    return Point64(static_cast<int64_t>(x * precision), 
                   static_cast<int64_t>(y * precision));
}

inline PointD FromPoint64(const Point64& pt, int precision = 100) {
    return PointD(static_cast<double>(pt.x) / precision, 
                  static_cast<double>(pt.y) / precision);
}

// Convert ImGui screen coordinates to Clipper2 format
inline Path64 BoxToPolygon(const ImVec2& min, const ImVec2& max, int precision = 100) {
    Path64 poly;
    poly.emplace_back(static_cast<int64_t>(min.x * precision), 
                      static_cast<int64_t>(min.y * precision));
    poly.emplace_back(static_cast<int64_t>(max.x * precision), 
                      static_cast<int64_t>(min.y * precision));
    poly.emplace_back(static_cast<int64_t>(max.x * precision), 
                      static_cast<int64_t>(max.y * precision));
    poly.emplace_back(static_cast<int64_t>(min.x * precision), 
                      static_cast<int64_t>(max.y * precision));
    return poly;
}

// Create polygon from screen-space player box (chams style)
inline Paths64 CreatePlayerChamsPolygons(const ImVec2& box_min, const ImVec2& box_max, 
                                          float expansion = 0.0f, int precision = 100) {
    Paths64 result;
    
    // Main body polygon
    Path64 body;
    float exp = expansion;
    body.emplace_back(static_cast<int64_t>((box_min.x - exp) * precision), 
                      static_cast<int64_t>((box_min.y - exp) * precision));
    body.emplace_back(static_cast<int64_t>((box_max.x + exp) * precision), 
                      static_cast<int64_t>((box_min.y - exp) * precision));
    body.emplace_back(static_cast<int64_t>((box_max.x + exp) * precision), 
                      static_cast<int64_t>((box_max.y + exp) * precision));
    body.emplace_back(static_cast<int64_t>((box_min.x - exp) * precision), 
                      static_cast<int64_t>((box_max.y + exp) * precision));
    result.push_back(body);
    
    return result;
}

// ============================================
// WALL OCCLUSION CLIPPING (MAIN CHAMS FUNCTION)
// ============================================
// This function clips the player chams against "wall" regions
// to create the "hidden behind wall" effect
inline Paths64 ClipChamsAgainstWalls(const Paths64& player_polygons, 
                                      const Paths64& wall_polygons,
                                      bool clip_behind_walls = true) {
    if (!clip_behind_walls) return player_polygons;
    
    Clipper64 clipper;
    clipper.AddSubject(player_polygons);
    clipper.AddClip(wall_polygons);
    
    // Difference = player polygons minus wall regions
    // This gives us only the visible parts
    return clipper.Execute(ClipType::Difference, FillRule::NonZero);
}

// Create a "wall" polygon from screen coordinates
// Used to simulate what parts of player are behind walls
inline Path64 CreateWallPolygon(float screen_x, float screen_y, float width, float height,
                                 int precision = 100) {
    Path64 wall;
    wall.emplace_back(static_cast<int64_t>(screen_x * precision), 
                      static_cast<int64_t>(screen_y * precision));
    wall.emplace_back(static_cast<int64_t>((screen_x + width) * precision), 
                      static_cast<int64_t>(screen_y * precision));
    wall.emplace_back(static_cast<int64_t>((screen_x + width) * precision), 
                      static_cast<int64_t>((screen_y + height) * precision));
    wall.emplace_back(static_cast<int64_t>(screen_x * precision), 
                      static_cast<int64_t>((screen_y + height) * precision));
    return wall;
}

} // namespace Clipper2Lib

// ============================================
// IMGUI INTEGRATION HELPERS
// ============================================
#ifdef IMGUI_H
namespace Clipper2Lib {

// Draw clipped chams polygon using ImGui
inline void DrawClippedChamsPolygon(ImDrawList* dl, const Paths64& polygons,
                                     const ImColor& visible_color, 
                                     const ImColor& hidden_color,
                                     int precision = 100,
                                     float thickness = 1.0f) {
    for (const auto& path : polygons) {
        if (path.size() < 3) continue;
        
        // Convert to ImVec2 points
        std::vector<ImVec2> points;
        points.reserve(path.size());
        for (const auto& pt : path) {
            points.emplace_back(static_cast<float>(pt.x) / precision, 
                               static_cast<float>(pt.y) / precision);
        }
        
        // Draw filled polygon
        dl->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), 
                               visible_color);
        
        // Draw outline
        dl->AddPolyline(points.data(), static_cast<int>(points.size()), 
                       visible_color, ImDrawFlags_Closed, thickness);
    }
}

// Draw chams with wall clipping
inline void DrawChamsWithClipping(ImDrawList* dl, 
                                   const ImVec2& box_min, const ImVec2& box_max,
                                   const std::vector<ImVec2>& wall_regions,
                                   const ImColor& visible_color,
                                   const ImColor& hidden_color,
                                   bool is_visible,
                                   float expansion = 2.0f,
                                   int precision = 100) {
    // Create player polygon
    Paths64 player_polys = CreatePlayerChamsPolygons(box_min, box_max, expansion, precision);
    
    if (wall_regions.empty() || is_visible) {
        // No walls or player is visible - draw normally
        for (const auto& poly : player_polys) {
            if (poly.size() < 3) continue;
            
            std::vector<ImVec2> points;
            points.reserve(poly.size());
            for (const auto& pt : poly) {
                points.emplace_back(static_cast<float>(pt.x) / precision, 
                                   static_cast<float>(pt.y) / precision);
            }
            
            dl->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), 
                                   visible_color);
        }
        return;
    }
    
    // Create wall polygons
    Paths64 wall_polys;
    for (size_t i = 0; i < wall_regions.size(); i += 4) {
        if (i + 3 >= wall_regions.size()) break;
        
        Path64 wall;
        for (size_t j = 0; j < 4 && (i + j) < wall_regions.size(); j++) {
            wall.emplace_back(static_cast<int64_t>(wall_regions[i + j].x * precision),
                              static_cast<int64_t>(wall_regions[i + j].y * precision));
        }
        if (wall.size() >= 3) {
            wall_polys.push_back(wall);
        }
    }
    
    // Clip player polygons against walls
    Paths64 visible_parts = ClipChamsAgainstWalls(player_polys, wall_polys, true);
    
    // Draw visible parts
    DrawClippedChamsPolygon(dl, visible_parts, visible_color, hidden_color, precision);
    
    // Calculate hidden parts (for "behind wall" effect)
    if (!wall_polys.empty()) {
        Clipper64 clipper_hidden;
        clipper_hidden.AddSubject(player_polys);
        clipper_hidden.AddClip(wall_polys);
        // Intersection = parts that are behind walls
        Paths64 hidden_parts = clipper_hidden.Execute(ClipType::Intersection, FillRule::NonZero);
        
        // Draw hidden parts with hidden color
        for (const auto& path : hidden_parts) {
            if (path.size() < 3) continue;
            
            std::vector<ImVec2> points;
            points.reserve(path.size());
            for (const auto& pt : path) {
                points.emplace_back(static_cast<float>(pt.x) / precision, 
                                   static_cast<float>(pt.y) / precision);
            }
            
            dl->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), 
                                   hidden_color);
        }
    }
}

} // namespace Clipper2Lib
#endif // IMGUI_H
