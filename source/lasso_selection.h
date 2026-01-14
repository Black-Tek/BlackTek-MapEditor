//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#ifndef RME_LASSO_SELECTION_H
#define RME_LASSO_SELECTION_H

#include "position.h"
#include <cmath>
#include <vector>

// Point structure for lasso path (in tile coordinates)
struct LassoPoint {
  int x;
  int y;

  LassoPoint() : x(0), y(0) {}
  LassoPoint(int x, int y) : x(x), y(y) {}

  bool operator==(const LassoPoint &other) const {
    return x == other.x && y == other.y;
  }

  double distanceTo(const LassoPoint &other) const {
    double dx = x - other.x;
    double dy = y - other.y;
    return std::sqrt(dx * dx + dy * dy);
  }

  // Optimized: avoid sqrt for distance comparisons
  double distanceSquaredTo(const LassoPoint &other) const {
    double dx = x - other.x;
    double dy = y - other.y;
    return dx * dx + dy * dy;
  }
};

// Bounding box for optimization
struct LassoBoundingBox {
  int minX, minY, maxX, maxY;
  bool hasPoints; // Track if any points have been added

  LassoBoundingBox() : minX(0), minY(0), maxX(0), maxY(0), hasPoints(false) {}

  void reset() {
    // Use safe values instead of INT_MAX/INT_MIN to avoid overflow in loops
    minX = minY = 0;
    maxX = maxY = 0;
    hasPoints = false;
  }

  void expand(int x, int y) {
    if (!hasPoints) {
      // First point - initialize all values to this point
      minX = maxX = x;
      minY = maxY = y;
      hasPoints = true;
    } else {
      if (x < minX)
        minX = x;
      if (x > maxX)
        maxX = x;
      if (y < minY)
        minY = y;
      if (y > maxY)
        maxY = y;
    }
  }

  bool contains(int x, int y) const {
    if (!hasPoints)
      return false;
    return x >= minX && x <= maxX && y >= minY && y <= maxY;
  }

  bool isValid() const { return hasPoints && minX <= maxX && minY <= maxY; }

  int width() const { return hasPoints ? (maxX - minX) : 0; }
  int height() const { return hasPoints ? (maxY - minY) : 0; }
};

// Edge structure for Active Edge Table (AET) optimization
struct LassoEdge {
  int yMax;        // Maximum y coordinate of the edge
  double x;        // Current x intersection
  double invSlope; // 1/slope = dx/dy for incremental updates

  LassoEdge(int yMax, double x, double invSlope)
      : yMax(yMax), x(x), invSlope(invSlope) {}
};

// Lasso selection mode
enum class LassoMode {
  Replace, // Replace current selection
  Add,     // Add to current selection (Ctrl)
  Subtract // Subtract from current selection (Alt)
};

class LassoSelection {
public:
  LassoSelection();
  ~LassoSelection();

  // Path management
  void clear();
  void addPoint(int x, int y);
  void closePath();
  bool isActive() const { return m_active; }
  bool isClosed() const { return m_closed; }
  void setActive(bool active) { m_active = active; }

  // Path access
  const std::vector<LassoPoint> &getPath() const { return m_path; }
  const std::vector<LassoPoint> &getSimplifiedPath() const {
    return m_simplifiedPath;
  }
  const LassoBoundingBox &getBoundingBox() const { return m_boundingBox; }

  // Get tiles inside the lasso polygon
  std::vector<Position> getTilesInPolygon(int floor) const;

  // Configuration
  void setMinPointDistance(double dist) { m_minPointDistance = dist; }
  void setSimplifyTolerance(double tol) { m_simplifyTolerance = tol; }

private:
  // Ramer-Douglas-Peucker path simplification
  void simplifyPath();
  void rdpSimplify(const std::vector<LassoPoint> &points, int start, int end,
                   double epsilon, std::vector<bool> &keep);
  double perpendicularDistance(const LassoPoint &point,
                               const LassoPoint &lineStart,
                               const LassoPoint &lineEnd) const;

  // Optimized scanline fill using Active Edge Table (AET)
  void scanlineFillAET(int floor, std::vector<Position> &tiles) const;

  // Build edge table for AET algorithm
  void buildEdgeTable(std::vector<std::vector<LassoEdge>> &edgeTable,
                      int minY) const;

  // Point-in-polygon test (ray casting)
  bool pointInPolygon(int x, int y) const;

  std::vector<LassoPoint> m_path;           // Raw path points
  std::vector<LassoPoint> m_simplifiedPath; // Simplified path for rendering
  LassoBoundingBox m_boundingBox;

  bool m_active = false;
  bool m_closed = false;
  double m_minPointDistance = 0.5; // Minimum distance between points (in tiles)
  double m_minPointDistanceSquared =
      0.25;                         // Cached squared value for fast comparisons
  double m_simplifyTolerance = 0.5; // Tolerance for path simplification
};

#endif // RME_LASSO_SELECTION_H
