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
	  m_minPointDistance(0.5),		   // Half a tile minimum distance
	  m_minPointDistanceSquared(0.25), // Cached: 0.5 * 0.5
	  m_simplifyTolerance(0.5)		   // Simplification tolerance
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
		if (m_path.back().distanceSquaredTo(newPoint) <
			m_minPointDistanceSquared) {
			return; // Too close to last point
		}
	}

	m_path.push_back(newPoint);
	m_boundingBox.expand(x, y);

	// OPTIMIZATION: Don't simplify during drag - only update simplified path
	// for rendering with minimal overhead. Full simplification happens on
	// close.
	if (m_path.size() <= 2) {
		m_simplifiedPath = m_path;
	} else if (m_path.size() % 20 == 0) {
		// Light simplification every 20 points for rendering only
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

	// OPTIMIZATION: Simplify only once when path is closed
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
	m_simplifiedPath.reserve(m_path.size() / 2); // Estimate ~50% reduction
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
		double dist =
			perpendicularDistance(points[i], points[start], points[end]);
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

	// OPTIMIZATION: Pre-reserve memory based on bounding box area estimate
	if (m_boundingBox.isValid()) {
		int estimatedTiles =
			(m_boundingBox.width() + 1) * (m_boundingBox.height() + 1);
		// Estimate ~50% fill ratio for most polygon shapes
		tiles.reserve(estimatedTiles / 2);
	}

	scanlineFillAET(floor, tiles);
	return tiles;
}

void LassoSelection::buildEdgeTable(
	std::vector<std::vector<LassoEdge>> &edgeTable, int minY) const {
	// Use simplified path for the polygon
	const std::vector<LassoPoint> &poly =
		m_simplifiedPath.empty() ? m_path : m_simplifiedPath;

	if (poly.size() < 3)
		return;

	// Build edge table - each bucket contains edges starting at that y
	for (size_t i = 0; i < poly.size() - 1; ++i) {
		const LassoPoint &p1 = poly[i];
		const LassoPoint &p2 = poly[i + 1];

		// Skip horizontal edges
		if (p1.y == p2.y)
			continue;

		// Determine which point is at the top (smaller y)
		int yMin, yMax;
		double xAtYMin;

		if (p1.y < p2.y) {
			yMin = p1.y;
			yMax = p2.y;
			xAtYMin = static_cast<double>(p1.x);
		} else {
			yMin = p2.y;
			yMax = p1.y;
			xAtYMin = static_cast<double>(p2.x);
		}

		// Calculate inverse slope (dx/dy)
		double invSlope =
			static_cast<double>(p2.x - p1.x) / static_cast<double>(p2.y - p1.y);

		// Add edge to the appropriate bucket
		int bucketIndex = yMin - minY;
		if (bucketIndex >= 0 &&
			bucketIndex < static_cast<int>(edgeTable.size())) {
			edgeTable[bucketIndex].emplace_back(yMax, xAtYMin, invSlope);
		}
	}
}

void LassoSelection::scanlineFillAET(int floor,
									 std::vector<Position> &tiles) const {
	// Use simplified path for the polygon
	const std::vector<LassoPoint> &poly =
		m_simplifiedPath.empty() ? m_path : m_simplifiedPath;

	if (poly.size() < 3)
		return;

	// Safety check: ensure bounding box is valid before iterating
	if (!m_boundingBox.isValid())
		return;

	int minY = m_boundingBox.minY;
	int maxY = m_boundingBox.maxY;
	int height = maxY - minY + 1;

	// Build edge table (edges indexed by their starting y coordinate)
	std::vector<std::vector<LassoEdge>> edgeTable(height);
	buildEdgeTable(edgeTable, minY);

	// Active Edge Table - edges currently being processed
	std::vector<LassoEdge> activeEdges;
	activeEdges.reserve(poly.size());

	// Process each scanline
	for (int y = minY; y <= maxY; ++y) {
		int bucketIndex = y - minY;

		// Add new edges from edge table to active edges
		if (bucketIndex < static_cast<int>(edgeTable.size())) {
			for (const LassoEdge &edge : edgeTable[bucketIndex]) {
				activeEdges.push_back(edge);
			}
		}

		// Remove edges that have ended (yMax == y)
		activeEdges.erase(
			std::remove_if(activeEdges.begin(), activeEdges.end(),
						   [y](const LassoEdge &e) { return e.yMax == y; }),
			activeEdges.end());

		// Skip if no active edges
		if (activeEdges.empty())
			continue;

		// Sort active edges by current x intersection
		std::sort(
			activeEdges.begin(), activeEdges.end(),
			[](const LassoEdge &a, const LassoEdge &b) { return a.x < b.x; });

		// Fill between pairs of intersections
		for (size_t i = 0; i + 1 < activeEdges.size(); i += 2) {
			int xStart = static_cast<int>(std::ceil(activeEdges[i].x));
			int xEnd = static_cast<int>(std::floor(activeEdges[i + 1].x));

			for (int x = xStart; x <= xEnd; ++x) {
				tiles.emplace_back(x, y, floor);
			}
		}

		// Update x values for next scanline
		for (LassoEdge &edge : activeEdges) {
			edge.x += edge.invSlope;
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
			(x < (poly[j].x - poly[i].x) * (y - poly[i].y) /
						 (poly[j].y - poly[i].y) +
					 poly[i].x)) {
			inside = !inside;
		}
	}

	return inside;
}
