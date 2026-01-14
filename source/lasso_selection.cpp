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

#include "lasso_selection.h"
#include "main.h"
#include <algorithm>
#include <cmath>

LassoSelection::LassoSelection()
    : m_active(false), m_closed(false),
      m_minPointDistance(0.5),         // Half a tile minimum distance
      m_minPointDistanceSquared(0.25), // Cached: 0.5 * 0.5
      m_simplifyTolerance(0.5)         // Simplification tolerance
{}

LassoSelection::~LassoSelection() {}

void LassoSelection::clear() {
  m_path.clear();
  m_simplifiedPath.clear();
  m_boundingBox.reset();
  m_active = false;
  m_closed = false;
}

void LassoSelection::addPoint(int x, int y) {
  LassoPoint newPoint(x, y);

  // Check minimum distance from last point (optimized: use squared distance)
  if (!m_path.empty()) {
    if (m_path.back().distanceSquaredTo(newPoint) < m_minPointDistanceSquared) {
      return; // Too close to last point
    }
  }

  m_path.push_back(newPoint);
  m_boundingBox.expand(x, y);

  // Optimization: Only simplify every 15 points instead of every point
  // RDP is O(n^2) worst case, calling it less frequently improves performance
  if (m_path.size() > 2 && m_path.size() % 15 == 0) {
    simplifyPath();
  } else if (m_path.size() <= 2) {
    m_simplifiedPath = m_path;
  }
}

void LassoSelection::closePath() {
  if (m_path.size() < 3) {
    clear();
    return;
  }

  // Close the polygon by adding first point at the end if needed
  if (!(m_path.front() == m_path.back())) {
    m_path.push_back(m_path.front());
  }

  m_closed = true;
  simplifyPath();
}

void LassoSelection::simplifyPath() {
  if (m_path.size() < 3) {
    m_simplifiedPath = m_path;
    return;
  }

  std::vector<bool> keep(m_path.size(), false);
  keep[0] = true;
  keep[m_path.size() - 1] = true;

  rdpSimplify(m_path, 0, static_cast<int>(m_path.size()) - 1,
              m_simplifyTolerance, keep);

  m_simplifiedPath.clear();
  for (size_t i = 0; i < m_path.size(); ++i) {
    if (keep[i]) {
      m_simplifiedPath.push_back(m_path[i]);
    }
  }
}

void LassoSelection::rdpSimplify(const std::vector<LassoPoint> &points,
                                 int start, int end, double epsilon,
                                 std::vector<bool> &keep) {
  if (end - start < 2)
    return;

  double maxDist = 0;
  int maxIndex = start;

  for (int i = start + 1; i < end; ++i) {
    double dist = perpendicularDistance(points[i], points[start], points[end]);
    if (dist > maxDist) {
      maxDist = dist;
      maxIndex = i;
    }
  }

  if (maxDist > epsilon) {
    keep[maxIndex] = true;
    rdpSimplify(points, start, maxIndex, epsilon, keep);
    rdpSimplify(points, maxIndex, end, epsilon, keep);
  }
}

double LassoSelection::perpendicularDistance(const LassoPoint &point,
                                             const LassoPoint &lineStart,
                                             const LassoPoint &lineEnd) const {
  double dx = lineEnd.x - lineStart.x;
  double dy = lineEnd.y - lineStart.y;

  double lineLengthSq = dx * dx + dy * dy;
  if (lineLengthSq == 0) {
    return point.distanceTo(lineStart);
  }

  // Calculate perpendicular distance using cross product
  double cross =
      std::abs((point.x - lineStart.x) * dy - (point.y - lineStart.y) * dx);
  return cross / std::sqrt(lineLengthSq);
}

std::vector<Position> LassoSelection::getTilesInPolygon(int floor) const {
  std::vector<Position> tiles;

  if (!m_closed || m_path.size() < 3) {
    return tiles;
  }

  scanlineFill(floor, tiles);
  return tiles;
}

void LassoSelection::scanlineFill(int floor,
                                  std::vector<Position> &tiles) const {
  // Use simplified path for the polygon
  const std::vector<LassoPoint> &poly =
      m_simplifiedPath.empty() ? m_path : m_simplifiedPath;

  if (poly.size() < 3)
    return;

  // Safety check: ensure bounding box is valid before iterating
  if (!m_boundingBox.isValid())
    return;

  // Iterate through each row in the bounding box
  for (int y = m_boundingBox.minY; y <= m_boundingBox.maxY; ++y) {
    std::vector<int> intersections;

    // Find all intersections with polygon edges
    for (size_t i = 0; i < poly.size() - 1; ++i) {
      const LassoPoint &p1 = poly[i];
      const LassoPoint &p2 = poly[i + 1];

      // Check if edge crosses this scanline
      if ((p1.y <= y && p2.y > y) || (p2.y <= y && p1.y > y)) {
        // Calculate x intersection (use floor for consistent rounding)
        double t = static_cast<double>(y - p1.y) / (p2.y - p1.y);
        int x = static_cast<int>(std::floor(p1.x + t * (p2.x - p1.x)));
        intersections.push_back(x);
      }
    }

    // Sort intersections
    std::sort(intersections.begin(), intersections.end());

    // Fill between pairs of intersections
    for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
      for (int x = intersections[i]; x <= intersections[i + 1]; ++x) {
        tiles.push_back(Position(x, y, floor));
      }
    }
  }
}

bool LassoSelection::pointInPolygon(int x, int y) const {
  const std::vector<LassoPoint> &poly =
      m_simplifiedPath.empty() ? m_path : m_simplifiedPath;

  if (poly.size() < 3)
    return false;

  // Ray casting algorithm
  bool inside = false;
  size_t j = poly.size() - 1;

  for (size_t i = 0; i < poly.size(); j = i++) {
    if (((poly[i].y > y) != (poly[j].y > y)) &&
        (x <
         (poly[j].x - poly[i].x) * (y - poly[i].y) / (poly[j].y - poly[i].y) +
             poly[i].x)) {
      inside = !inside;
    }
  }

  return inside;
}
