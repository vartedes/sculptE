#include "HalfEdgeMesh.h"
#include <iostream>
#include <cmath>
#include <algorithm>

bool HalfEdgeMesh::initialize(const std::vector<float>& rawVertices, const std::vector<unsigned int>& rawIndices) {
    vertices.clear();
    halfEdges.clear();
    faces.clear();

    if (rawVertices.size() % 3 != 0 || rawIndices.size() % 3 != 0) {
        return false;
    }

    size_t numVerts = rawVertices.size() / 3;
    size_t numFaces = rawIndices.size() / 3;

    vertices.resize(numVerts);
    faces.resize(numFaces);

    // 1. Setup flat vertices and initialize spatial elements
    for (size_t i = 0; i < numVerts; ++i) {
        vertices[i].position = Vector3(rawVertices[3 * i], rawVertices[3 * i + 1], rawVertices[3 * i + 2]);
        vertices[i].normal = Vector3(0.0f, 0.0f, 0.0f);
        vertices[i].halfEdge = INVALID_HALFEDGE_HANDLE;
    }

    // 2. Setup directed edges and map to pair twins together
    // Standard unordered_map with a custom hash to map a directed edge (src -> dest) to its allocated half-edge index
    struct PairHash {
        std::size_t operator()(const std::pair<VertexHandle, VertexHandle>& p) const {
            return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
        }
    };
    std::unordered_map<std::pair<VertexHandle, VertexHandle>, HalfEdgeHandle, PairHash> edgeMap;

    halfEdges.reserve(numFaces * 3);

    for (size_t f = 0; f < numFaces; ++f) {
        VertexHandle v0 = static_cast<VertexHandle>(rawIndices[3 * f]);
        VertexHandle v1 = static_cast<VertexHandle>(rawIndices[3 * f + 1]);
        VertexHandle v2 = static_cast<VertexHandle>(rawIndices[3 * f + 2]);

        // Guard against zero-area / degenerate triangles
        if (v0 == v1 || v1 == v2 || v2 == v0) {
            continue;
        }

        FaceHandle fHandle = static_cast<FaceHandle>(f);

        // Preallocate 3 half-edges belonging to this triangle face
        HalfEdgeHandle heIndices[3];
        for (int i = 0; i < 3; ++i) {
            heIndices[i] = static_cast<HalfEdgeHandle>(halfEdges.size());
            halfEdges.push_back(HalfEdge());
        }

        // Connect the face boundary to index 0 of the triangle loop
        faces[fHandle].halfEdge = heIndices[0];

        // Setup CCW cycle relationships around the triangle
        // Segment 0: v0 -> v1
        halfEdges[heIndices[0]].vertex = v1;
        halfEdges[heIndices[0]].face = fHandle;
        halfEdges[heIndices[0]].next = heIndices[1];
        halfEdges[heIndices[0]].prev = heIndices[2];

        // Segment 1: v1 -> v2
        halfEdges[heIndices[1]].vertex = v2;
        halfEdges[heIndices[1]].face = fHandle;
        halfEdges[heIndices[1]].next = heIndices[2];
        halfEdges[heIndices[1]].prev = heIndices[0];

        // Segment 2: v2 -> v0
        halfEdges[heIndices[2]].vertex = v0;
        halfEdges[heIndices[2]].face = fHandle;
        halfEdges[heIndices[2]].next = heIndices[0];
        halfEdges[heIndices[2]].prev = heIndices[1];

        // Map outgoing halfedge pointers onto vertices (guarantees O(1) start of query traverser)
        vertices[v0].halfEdge = heIndices[0];
        vertices[v1].halfEdge = heIndices[1];
        vertices[v2].halfEdge = heIndices[2];

        // Define primary directed edge keys for this triangle step
        std::pair<VertexHandle, VertexHandle> edge0(v0, v1);
        std::pair<VertexHandle, VertexHandle> edge1(v1, v2);
        std::pair<VertexHandle, VertexHandle> edge2(v2, v0);

        // Stitch opposite twin Half-Edges
        auto stitchTwin = [&](const std::pair<VertexHandle, VertexHandle>& activeEdge, HalfEdgeHandle heIdx) {
            std::pair<VertexHandle, VertexHandle> reciprocalKey(activeEdge.second, activeEdge.first);
            auto match = edgeMap.find(reciprocalKey);
            if (match != edgeMap.end()) {
                HalfEdgeHandle reciprocalHeIdx = match->second;
                halfEdges[heIdx].twin = reciprocalHeIdx;
                halfEdges[reciprocalHeIdx].twin = heIdx;
            }
            edgeMap[activeEdge] = heIdx;
        };

        stitchTwin(edge0, heIndices[0]);
        stitchTwin(edge1, heIndices[1]);
        stitchTwin(edge2, heIndices[2]);
    }

    // 3. Populate topological vertex normal orientations
    for (size_t f = 0; f < faces.size(); ++f) {
        HalfEdgeHandle he = faces[f].halfEdge;
        if (he == INVALID_HALFEDGE_HANDLE) continue;

        // Obtain indices of the three triangle vertices
        VertexHandle v0 = halfEdges[halfEdges[he].prev].vertex;
        VertexHandle v1 = halfEdges[he].vertex;
        VertexHandle v2 = halfEdges[halfEdges[he].next].vertex;

        const Vector3& p0 = vertices[v0].position;
        const Vector3& p1 = vertices[v1].position;
        const Vector3& p2 = vertices[v2].position;

        // Compute cross product vector cross(p1 - p0, p2 - p0)
        float ux = p1.x - p0.x;
        float uy = p1.y - p0.y;
        float uz = p1.z - p0.z;

        float vx = p2.x - p0.x;
        float vy = p2.y - p0.y;
        float vz = p2.z - p0.z;

        float nx = uy * vz - uz * vy;
        float ny = uz * vx - ux * vz;
        float nz = ux * vy - uy * vx;

        // Accumulate face area-weighted normal vectors
        vertices[v0].normal.x += nx; vertices[v0].normal.y += ny; vertices[v0].normal.z += nz;
        vertices[v1].normal.x += nx; vertices[v1].normal.y += ny; vertices[v1].normal.z += nz;
        vertices[v2].normal.x += nx; vertices[v2].normal.y += ny; vertices[v2].normal.z += nz;
    }

    // Normalize normal vectors
    for (size_t i = 0; i < vertices.size(); ++i) {
        float sqLength = vertices[i].normal.x * vertices[i].normal.x +
                          vertices[i].normal.y * vertices[i].normal.y +
                          vertices[i].normal.z * vertices[i].normal.z;
        if (sqLength > 1e-8f) {
            float len = std::sqrt(sqLength);
            vertices[i].normal.x /= len;
            vertices[i].normal.y /= len;
            vertices[i].normal.z /= len;
        }
    }

    return true;
}

std::vector<VertexHandle> HalfEdgeMesh::getNeighboringVertices(VertexHandle v) const {
    std::vector<VertexHandle> neighbors;
    if (!isValidVertex(v)) return neighbors;

    HalfEdgeHandle startHe = vertices[v].halfEdge;
    if (startHe == INVALID_HALFEDGE_HANDLE) return neighbors;

    HalfEdgeHandle currHe = startHe;
    do {
        // Collect destination vertex of current outgoing half-edge
        neighbors.push_back(halfEdges[currHe].vertex);

        // Traverse to next outgoing half-edge around vertex 'v':
        // 1. Get twin of the current outgoing half-edge
        // 2. Find the next of this twin
        HalfEdgeHandle twinHe = halfEdges[currHe].twin;
        if (twinHe == INVALID_HALFEDGE_HANDLE) {
            // Boundary encountered: halt cycle rotation for non-manifold / boundary sheets
            break;
        }
        currHe = halfEdges[twinHe].next;
    } while (currHe != startHe && currHe != INVALID_HALFEDGE_HANDLE);

    return neighbors;
}

std::vector<FaceHandle> HalfEdgeMesh::getNeighboringFaces(VertexHandle v) const {
    std::vector<FaceHandle> neighboringFaces;
    if (!isValidVertex(v)) return neighboringFaces;

    HalfEdgeHandle startHe = vertices[v].halfEdge;
    if (startHe == INVALID_HALFEDGE_HANDLE) return neighboringFaces;

    HalfEdgeHandle currHe = startHe;
    do {
        FaceHandle f = halfEdges[currHe].face;
        if (f != INVALID_FACE_HANDLE) {
            neighboringFaces.push_back(f);
        }

        HalfEdgeHandle twinHe = halfEdges[currHe].twin;
        if (twinHe == INVALID_HALFEDGE_HANDLE) {
            break;
        }
        currHe = halfEdges[twinHe].next;
    } while (currHe != startHe && currHe != INVALID_HALFEDGE_HANDLE);

    return neighboringFaces;
}

std::vector<FaceHandle> HalfEdgeMesh::getAdjacentFaces(FaceHandle f) const {
    std::vector<FaceHandle> adjacentFaces;
    if (!isValidFace(f)) return adjacentFaces;

    HalfEdgeHandle startHe = faces[f].halfEdge;
    if (startHe == INVALID_HALFEDGE_HANDLE) return adjacentFaces;

    HalfEdgeHandle currHe = startHe;
    do {
        HalfEdgeHandle twinHe = halfEdges[currHe].twin;
        if (twinHe != INVALID_HALFEDGE_HANDLE) {
            FaceHandle adjFace = halfEdges[twinHe].face;
            if (adjFace != INVALID_FACE_HANDLE) {
                adjacentFaces.push_back(adjFace);
            }
        }
        currHe = halfEdges[currHe].next;
    } while (currHe != startHe && currHe != INVALID_HALFEDGE_HANDLE);

    return adjacentFaces;
}

void HalfEdgeMesh::computeSmoothNormals() {
    size_t numVerts = vertices.size();
    
    // Reset all vertex normal buffers to zero
    for (size_t i = 0; i < numVerts; ++i) {
        vertices[i].normal = Vector3(0.0f, 0.0f, 0.0f);
    }

    // Traverse each vertex and compute angle-weighted smooth normals
    for (size_t v = 0; v < numVerts; ++v) {
        HalfEdgeHandle startHe = vertices[v].halfEdge;
        if (startHe == INVALID_HALFEDGE_HANDLE) continue;

        float sumNormalX = 0.0f;
        float sumNormalY = 0.0f;
        float sumNormalZ = 0.0f;

        HalfEdgeHandle currHe = startHe;
        do {
            FaceHandle f = halfEdges[currHe].face;
            if (f != INVALID_FACE_HANDLE) {
                VertexHandle v_target = halfEdges[currHe].vertex;
                
                // Retrieve the third vertex in the CCW triangle face
                HalfEdgeHandle nextHe = halfEdges[currHe].next;
                VertexHandle v_other = halfEdges[nextHe].vertex;

                const Vector3& p = vertices[v].position;
                const Vector3& p_target = vertices[v_target].position;
                const Vector3& p_other = vertices[v_other].position;

                // Create vectors incident to vertex v
                float ax = p_target.x - p.x;
                float ay = p_target.y - p.y;
                float az = p_target.z - p.z;

                float bx = p_other.x - p.x;
                float by = p_other.y - p.y;
                float bz = p_other.z - p.z;

                // Face normal calculation via vector cross-product
                float nx = ay * bz - az * by;
                float ny = az * bx - ax * bz;
                float nz = ax * by - ay * bx;

                float lenN = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (lenN > 1e-8f) {
                    float fnx = nx / lenN;
                    float fny = ny / lenN;
                    float fnz = nz / lenN;

                    float lenA = std::sqrt(ax * ax + ay * ay + az * az);
                    float lenB = std::sqrt(bx * bx + by * by + bz * bz);

                    if (lenA > 1e-8f && lenB > 1e-8f) {
                        float dot = (ax * bx + ay * by + az * bz) / (lenA * lenB);
                        // Guard against floating point rounding domain errors
                        if (dot < -1.0f) dot = -1.0f;
                        if (dot > 1.0f) dot = 1.0f;

                        float angle = std::acos(dot);

                        // Accumulate normal weighted by the corner angle
                        sumNormalX += fnx * angle;
                        sumNormalY += fny * angle;
                        sumNormalZ += fnz * angle;
                    }
                }
            }

            // Move to next outgoing half-edge around vertex
            HalfEdgeHandle twinHe = halfEdges[currHe].twin;
            if (twinHe == INVALID_HALFEDGE_HANDLE) {
                // We reached a boundary edge. For boundary or non-manifold vertices, 
                // try to rotate in the opposite direction to collect rest of the incident faces.
                break;
            }
            currHe = halfEdges[twinHe].next;
        } while (currHe != startHe && currHe != INVALID_HALFEDGE_HANDLE);

        // If we hit a boundary in the clockwise rotation, we can also traverse counter-clockwise:
        if (currHe != startHe && currHe == INVALID_HALFEDGE_HANDLE) {
            // Find the half-edge that ends at v (incoming half-edge)
            HalfEdgeHandle incomingHe = halfEdges[startHe].prev;
            while (incomingHe != INVALID_HALFEDGE_HANDLE) {
                FaceHandle f = halfEdges[incomingHe].face;
                if (f != INVALID_FACE_HANDLE) {
                    VertexHandle v_target = halfEdges[incomingHe].vertex; // Target is v list
                    HalfEdgeHandle prevHe = halfEdges[incomingHe].prev;
                    VertexHandle v_parent = halfEdges[prevHe].vertex; // Original start

                    // For incoming, we are looking at edges outgoing from v:
                    // Edge 1: v_parent -> v
                    // Edge 2: v_target(v) -> next of incoming vertex
                    HalfEdgeHandle nextHe = halfEdges[incomingHe].next;
                    VertexHandle v_other = halfEdges[nextHe].vertex;

                    const Vector3& p = vertices[v].position;
                    // v_parent is the source of incomingHe
                    const Vector3& p_parent = vertices[halfEdges[prevHe].vertex].position;
                    const Vector3& p_other = vertices[v_other].position;

                    float ax = p_parent.x - p.x;
                    float ay = p_parent.y - p.y;
                    float az = p_parent.z - p.z;

                    float bx = p_other.x - p.x;
                    float by = p_other.y - p.y;
                    float bz = p_other.z - p.z;

                    float nx = ay * bz - az * by;
                    float ny = az * bx - ax * bz;
                    float nz = ax * by - ay * bx;

                    float lenN = std::sqrt(nx * nx + ny * ny + nz * nz);
                    if (lenN > 1e-8f) {
                        float fnx = nx / lenN;
                        float fny = ny / lenN;
                        float fnz = nz / lenN;

                        float lenA = std::sqrt(ax * ax + ay * ay + az * az);
                        float lenB = std::sqrt(bx * bx + by * by + bz * bz);

                        if (lenA > 1e-8f && lenB > 1e-8f) {
                            float dot = (ax * bx + ay * by + az * bz) / (lenA * lenB);
                            if (dot < -1.0f) dot = -1.0f;
                            if (dot > 1.0f) dot = 1.0f;

                            float angle = std::acos(dot);
                            sumNormalX += fnx * angle;
                            sumNormalY += fny * angle;
                            sumNormalZ += fnz * angle;
                        }
                    }
                }

                HalfEdgeHandle incomingTwin = halfEdges[incomingHe].twin;
                if (incomingTwin == INVALID_HALFEDGE_HANDLE) {
                    break;
                }
                incomingHe = halfEdges[incomingTwin].prev;
            }
        }

        // Write finalized smoothed and normalized vector back to vertex properties
        float sqLen = sumNormalX * sumNormalX + sumNormalY * sumNormalY + sumNormalZ * sumNormalZ;
        if (sqLen > 1e-8f) {
            float len = std::sqrt(sqLen);
            vertices[v].normal = Vector3(sumNormalX / len, sumNormalY / len, sumNormalZ / len);
        }
    }
}

bool HalfEdgeMesh::checkLinkCondition(VertexHandle U, VertexHandle V) const {
    auto neighborsU = getNeighboringVertices(U);
    auto neighborsV = getNeighboringVertices(V);
    
    int commonCount = 0;
    for (auto nU : neighborsU) {
        if (std::find(neighborsV.begin(), neighborsV.end(), nU) != neighborsV.end()) {
            commonCount++;
        }
    }
    
    // Check if the edge is boundary
    HalfEdgeHandle he = vertices[U].halfEdge;
    bool isBoundary = false;
    if (he != INVALID_HALFEDGE_HANDLE) {
        HalfEdgeHandle startHe = he;
        HalfEdgeHandle currHe = he;
        do {
            if (halfEdges[currHe].vertex == V) {
                if (halfEdges[currHe].twin == INVALID_HALFEDGE_HANDLE) {
                    isBoundary = true;
                    break;
                }
                HalfEdgeHandle twinHe = halfEdges[currHe].twin;
                if (halfEdges[twinHe].twin == INVALID_HALFEDGE_HANDLE) {
                    isBoundary = true;
                    break;
                }
            }
            HalfEdgeHandle twinHe = halfEdges[currHe].twin;
            if (twinHe == INVALID_HALFEDGE_HANDLE) {
                isBoundary = true;
                break;
            }
            currHe = halfEdges[twinHe].next;
        } while (currHe != startHe && currHe != INVALID_HALFEDGE_HANDLE);
    }
    
    if (isBoundary) {
        return commonCount == 1;
    } else {
        return commonCount == 2;
    }
}

VertexHandle HalfEdgeMesh::splitEdge(HalfEdgeHandle he) {
    if (!isValidHalfEdge(he)) return INVALID_VERTEX_HANDLE;

    // U -> V
    HalfEdgeHandle h0 = he;
    VertexHandle V = halfEdges[h0].vertex;
    HalfEdgeHandle h2 = halfEdges[h0].prev;
    if (h2 == INVALID_HALFEDGE_HANDLE) return INVALID_VERTEX_HANDLE;
    VertexHandle U = halfEdges[h2].vertex;
    HalfEdgeHandle h1 = halfEdges[h0].next;
    if (h1 == INVALID_HALFEDGE_HANDLE) return INVALID_VERTEX_HANDLE;
    FaceHandle F = halfEdges[h0].face;
    VertexHandle W = halfEdges[h1].vertex;

    if (!isValidVertex(U) || !isValidVertex(V) || !isValidVertex(W)) return INVALID_VERTEX_HANDLE;

    // 1. Validation of Area Pre-Mutation (Primary Face)
    const Vector3& posU = vertices[U].position;
    const Vector3& posV = vertices[V].position;
    const Vector3& posW = vertices[W].position;

    Vector3 e1(posV.x - posU.x, posV.y - posU.y, posV.z - posU.z);
    Vector3 e2(posW.x - posU.x, posW.y - posU.y, posW.z - posU.z);
    float nx = e1.y * e2.z - e1.z * e2.y;
    float ny = e1.z * e2.x - e1.x * e2.z;
    float nz = e1.x * e2.y - e1.y * e2.x;
    float doubleAreaF = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (doubleAreaF * 0.5f < 1e-6f) return INVALID_VERTEX_HANDLE; // Abort: degenerate primary face

    // 1. Validation of Area Pre-Mutation (Twin Face, if present)
    HalfEdgeHandle t0 = halfEdges[h0].twin;
    VertexHandle O = INVALID_VERTEX_HANDLE;
    if (t0 != INVALID_HALFEDGE_HANDLE) {
        HalfEdgeHandle t1 = halfEdges[t0].next;
        if (t1 != INVALID_HALFEDGE_HANDLE) {
            O = halfEdges[t1].vertex;
            if (isValidVertex(O)) {
                const Vector3& posO = vertices[O].position;
                Vector3 te1(posO.x - posV.x, posO.y - posV.y, posO.z - posV.z);
                Vector3 te2(posU.x - posV.x, posU.y - posV.y, posU.z - posV.z);
                float tnx = te1.y * te2.z - te1.z * te2.y;
                float tny = te1.z * te2.x - te1.x * te2.z;
                float tnz = te1.x * te2.y - te1.y * te2.x;
                float doubleAreaTwin = std::sqrt(tnx * tnx + tny * tny + tnz * tnz);
                if (doubleAreaTwin * 0.5f < 1e-6f) return INVALID_VERTEX_HANDLE; // Abort: degenerate twin face
            }
        }
    }

    // Compute midpoint position and normal
    Vector3 midPos((posU.x + posV.x) * 0.5f, (posU.y + posV.y) * 0.5f, (posU.z + posV.z) * 0.5f);
    Vector3 midNorm((vertices[U].normal.x + vertices[V].normal.x) * 0.5f,
                    (vertices[U].normal.y + vertices[V].normal.y) * 0.5f,
                    (vertices[U].normal.z + vertices[V].normal.z) * 0.5f);
    float len = std::sqrt(midNorm.x * midNorm.x + midNorm.y * midNorm.y + midNorm.z * midNorm.z);
    if (len > 1e-6f) {
        midNorm.x /= len;
        midNorm.y /= len;
        midNorm.z /= len;
    }

    // 3. Fusion of Duplicated Vertices (Welding Proximity check)
    // Check if the generated coordinate is too close (< 0.001) to any existing active vertex
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].halfEdge != INVALID_HALFEDGE_HANDLE) {
            float dx = vertices[i].position.x - midPos.x;
            float dy = vertices[i].position.y - midPos.y;
            float dz = vertices[i].position.z - midPos.z;
            float dSq = dx * dx + dy * dy + dz * dz;
            if (dSq < 0.000001f) { // 0.001 * 0.001 = 1e-6
                return INVALID_VERTEX_HANDLE; // Avoid co-located vertices to prevent degeneracy
            }
        }
    }

    // Allocate new vertex
    VertexHandle M = static_cast<VertexHandle>(vertices.size());
    vertices.push_back({ midPos, midNorm, INVALID_VERTEX_HANDLE });

    // Allocate new face and 3 new edges for F's split
    FaceHandle F_new = static_cast<FaceHandle>(faces.size());
    faces.push_back({ INVALID_FACE_HANDLE });

    HalfEdgeHandle h_um = static_cast<HalfEdgeHandle>(halfEdges.size());
    halfEdges.push_back(HalfEdge()); // u -> m
    HalfEdgeHandle h_mw = h_um + 1;
    halfEdges.push_back(HalfEdge()); // m -> w
    HalfEdgeHandle h_wm = h_um + 2;
    halfEdges.push_back(HalfEdge()); // w -> m
    
    // Stitch Face F_a (stays F): U -> M -> W -> U
    halfEdges[h_um].vertex = M;
    halfEdges[h_um].face = F;
    halfEdges[h_um].next = h_mw;
    halfEdges[h_um].prev = h2;

    halfEdges[h_mw].vertex = W;
    halfEdges[h_mw].face = F;
    halfEdges[h_mw].next = h2;
    halfEdges[h_mw].prev = h_um;

    halfEdges[h2].next = h_um;
    halfEdges[h2].prev = h_mw;

    faces[F].halfEdge = h_um;

    // Stitch Face F_b (becomes F_new): M -> V -> W -> M
    // Reuse h0 as M -> V
    halfEdges[h0].face = F_new;
    halfEdges[h0].next = h1;
    halfEdges[h0].prev = h_wm;

    halfEdges[h1].face = F_new;
    halfEdges[h1].next = h_wm;
    halfEdges[h1].prev = h0;

    halfEdges[h_wm].vertex = M;
    halfEdges[h_wm].face = F_new;
    halfEdges[h_wm].next = h0;
    halfEdges[h_wm].prev = h1;

    faces[F_new].halfEdge = h0;

    // Set twins for new internal split
    halfEdges[h_mw].twin = h_wm;
    halfEdges[h_wm].twin = h_mw;

    // Handle twin of h0 if it exists
    if (t0 != INVALID_HALFEDGE_HANDLE && isValidVertex(O)) {
        FaceHandle F_twin = halfEdges[t0].face;

        FaceHandle F_twin_new = static_cast<FaceHandle>(faces.size());
        faces.push_back({ INVALID_FACE_HANDLE });

        HalfEdgeHandle t1 = halfEdges[t0].next;
        HalfEdgeHandle t2 = halfEdges[t0].prev;

        HalfEdgeHandle h_vm = static_cast<HalfEdgeHandle>(halfEdges.size());
        halfEdges.push_back(HalfEdge()); // v -> m
        HalfEdgeHandle h_mo = h_vm + 1;
        halfEdges.push_back(HalfEdge()); // m -> o
        HalfEdgeHandle h_om = h_vm + 2;
        halfEdges.push_back(HalfEdge()); // o -> m

        // Face F_twin_a (stays F_twin): V -> M -> O -> V
        halfEdges[h_vm].vertex = M;
        halfEdges[h_vm].face = F_twin;
        halfEdges[h_vm].next = h_mo;
        halfEdges[h_vm].prev = t2;

        halfEdges[h_mo].vertex = O;
        halfEdges[h_mo].face = F_twin;
        halfEdges[h_mo].next = t2;
        halfEdges[h_mo].prev = h_vm;

        halfEdges[t2].next = h_vm;
        halfEdges[t2].prev = h_mo;

        faces[F_twin].halfEdge = h_vm;

        // Face F_twin_b (becomes F_twin_new): M -> U -> O -> M
        // Reuse t0 as M -> U
        halfEdges[t0].face = F_twin_new;
        halfEdges[t0].next = t1;
        halfEdges[t0].prev = h_om;

        halfEdges[t1].face = F_twin_new;
        halfEdges[t1].next = h_om;
        halfEdges[t1].prev = t0;

        halfEdges[h_om].vertex = M;
        halfEdges[h_om].face = F_twin_new;
        halfEdges[h_om].next = t0;
        halfEdges[h_om].prev = t1;

        faces[F_twin_new].halfEdge = t0;

        // Establish twin linkages across split
        halfEdges[h_mo].twin = h_om;
        halfEdges[h_om].twin = h_mo;

        halfEdges[h_um].twin = t0;
        halfEdges[t0].twin = h_um;

        halfEdges[h0].twin = h_vm;
        halfEdges[h_vm].twin = h0;

        vertices[O].halfEdge = t1;
    } else {
        halfEdges[h_um].twin = INVALID_HALFEDGE_HANDLE;
        halfEdges[h0].twin = INVALID_HALFEDGE_HANDLE;
    }

    // Assign safe outbound halfedges
    vertices[U].halfEdge = h_um;
    vertices[V].halfEdge = h0;
    vertices[W].halfEdge = h2;
    vertices[M].halfEdge = h_mw;

    return M;
}

bool HalfEdgeMesh::collapseEdge(HalfEdgeHandle he) {
    if (!isValidHalfEdge(he)) return false;

    HalfEdgeHandle h0 = he;
    VertexHandle V = halfEdges[h0].vertex;
    HalfEdgeHandle h2 = halfEdges[h0].prev;
    VertexHandle U = halfEdges[h2].vertex;

    if (!isValidVertex(U) || !isValidVertex(V)) return false;

    // Check link condition to prevent topological issues/collapsed genus
    if (!checkLinkCondition(U, V)) return false;

    HalfEdgeHandle h1 = halfEdges[h0].next;
    FaceHandle F = halfEdges[h0].face;
    VertexHandle W = halfEdges[h1].vertex;
    HalfEdgeHandle t0 = halfEdges[h0].twin;

    // Merge V into U (placing at the dynamic midpoint)
    Vector3 posU = vertices[U].position;
    Vector3 posV = vertices[V].position;
    vertices[U].position = Vector3((posU.x + posV.x) * 0.5f, (posU.y + posV.y) * 0.5f, (posU.z + posV.z) * 0.5f);
    
    // Update all local boundary half-edges targeting V to point to U instead
    HalfEdgeHandle curr = h0;
    do {
        if (halfEdges[curr].vertex == V) {
            halfEdges[curr].vertex = U;
        }
        HalfEdgeHandle nextHe = halfEdges[curr].next;
        if (nextHe == INVALID_HALFEDGE_HANDLE) break;
        curr = halfEdges[nextHe].twin;
    } while (curr != h0 && curr != INVALID_HALFEDGE_HANDLE);

    // Global pass to ensure zero loose dangling vertices
    for (size_t i = 0; i < halfEdges.size(); ++i) {
        if (halfEdges[i].vertex == V) {
            halfEdges[i].vertex = U;
        }
    }

    // Stitch outer boundaries together
    HalfEdgeHandle t_h1 = halfEdges[h1].twin;
    HalfEdgeHandle t_h2 = halfEdges[h2].twin;
    if (t_h1 != INVALID_HALFEDGE_HANDLE) {
        halfEdges[t_h1].twin = t_h2;
    }
    if (t_h2 != INVALID_HALFEDGE_HANDLE) {
        halfEdges[t_h2].twin = t_h1;
    }

    // Flag old topology buffers as deleted
    if (F != INVALID_FACE_HANDLE) {
        faces[F].halfEdge = INVALID_HALFEDGE_HANDLE;
    }
    halfEdges[h0].vertex = INVALID_VERTEX_HANDLE;
    halfEdges[h0].face = INVALID_FACE_HANDLE;
    halfEdges[h1].vertex = INVALID_VERTEX_HANDLE;
    halfEdges[h1].face = INVALID_FACE_HANDLE;
    halfEdges[h2].vertex = INVALID_VERTEX_HANDLE;
    halfEdges[h2].face = INVALID_FACE_HANDLE;

    // Handle opposite twin elements if present
    if (t0 != INVALID_HALFEDGE_HANDLE) {
        HalfEdgeHandle t1 = halfEdges[t0].next;
        HalfEdgeHandle t2 = halfEdges[t0].prev;
        FaceHandle F_twin = halfEdges[t0].face;
        VertexHandle O = halfEdges[t1].vertex;

        HalfEdgeHandle t_t1 = halfEdges[t1].twin;
        HalfEdgeHandle t_t2 = halfEdges[t2].twin;
        if (t_t1 != INVALID_HALFEDGE_HANDLE) {
            halfEdges[t_t1].twin = t_t2;
        }
        if (t_t2 != INVALID_HALFEDGE_HANDLE) {
            halfEdges[t_t2].twin = t_t1;
        }

        if (F_twin != INVALID_FACE_HANDLE) {
            faces[F_twin].halfEdge = INVALID_HALFEDGE_HANDLE;
        }
        halfEdges[t0].vertex = INVALID_VERTEX_HANDLE;
        halfEdges[t0].face = INVALID_FACE_HANDLE;
        halfEdges[t1].vertex = INVALID_VERTEX_HANDLE;
        halfEdges[t1].face = INVALID_FACE_HANDLE;
        halfEdges[t2].vertex = INVALID_VERTEX_HANDLE;
        halfEdges[t2].face = INVALID_FACE_HANDLE;

        if (isValidVertex(O)) {
            vertices[O].halfEdge = (t_t1 != INVALID_HALFEDGE_HANDLE) ? t_t1 : INVALID_HALFEDGE_HANDLE;
        }
    }

    // Free topological reference of collapsed vertex V
    vertices[V].halfEdge = INVALID_HALFEDGE_HANDLE;

    // Keep active outbound references safe from dangling index issues
    if (isValidVertex(U)) {
        vertices[U].halfEdge = (t_h2 != INVALID_HALFEDGE_HANDLE) ? t_h2 : INVALID_HALFEDGE_HANDLE;
    }
    if (isValidVertex(W)) {
        vertices[W].halfEdge = (t_h1 != INVALID_HALFEDGE_HANDLE) ? t_h1 : INVALID_HALFEDGE_HANDLE;
    }

    return true;
}

void HalfEdgeMesh::dynamicRemesh(Vector3 brushCenter, float brushRadius, float detailThreshold) {
    if (detailThreshold <= 0.0f) return;

    float minThreshold = detailThreshold * 0.4f; // Minimum edge length before collapse

    std::vector<HalfEdgeHandle> edgesToSplit;
    std::vector<HalfEdgeHandle> edgesToCollapse;

    // Identify active half-edges under the brush radius for splitting or collapsing
    for (size_t he = 0; he < halfEdges.size(); ++he) {
        if (!isValidHalfEdge(static_cast<HalfEdgeHandle>(he))) continue;
        HalfEdgeHandle companion = halfEdges[he].twin;
        if (companion != INVALID_HALFEDGE_HANDLE && static_cast<HalfEdgeHandle>(he) > companion) {
            continue;
        }

        VertexHandle V = halfEdges[he].vertex;
        HalfEdgeHandle prev = halfEdges[he].prev;
        if (prev == INVALID_HALFEDGE_HANDLE) continue;
        VertexHandle U = halfEdges[prev].vertex;

        if (!isValidVertex(U) || !isValidVertex(V)) continue;

        const Vector3& posU = vertices[U].position;
        const Vector3& posV = vertices[V].position;

        float distToBrushU = std::sqrt((posU.x - brushCenter.x) * (posU.x - brushCenter.x) +
                                      (posU.y - brushCenter.y) * (posU.y - brushCenter.y) +
                                      (posU.z - brushCenter.z) * (posU.z - brushCenter.z));
        float distToBrushV = std::sqrt((posV.x - brushCenter.x) * (posV.x - brushCenter.x) +
                                      (posV.y - brushCenter.y) * (posV.y - brushCenter.y) +
                                      (posV.z - brushCenter.z) * (posV.z - brushCenter.z));

        // If either vertex is within our brush radius, act locally on it
        if (distToBrushU < brushRadius || distToBrushV < brushRadius) {
            float edgeLength = std::sqrt((posU.x - posV.x) * (posU.x - posV.x) +
                                         (posU.y - posV.y) * (posU.y - posV.y) +
                                         (posU.z - posV.z) * (posU.z - posV.z));

            if (edgeLength > detailThreshold) {
                edgesToSplit.push_back(static_cast<HalfEdgeHandle>(he));
            } else if (edgeLength < minThreshold) {
                edgesToCollapse.push_back(static_cast<HalfEdgeHandle>(he));
            }
        }
    }

    // Perform atomic edge splits
    for (HalfEdgeHandle he : edgesToSplit) {
        if (isValidHalfEdge(he)) {
            splitEdge(he);
        }
    }

    // Perform atomic edge collapses
    for (HalfEdgeHandle he : edgesToCollapse) {
        if (isValidHalfEdge(he)) {
            collapseEdge(he);
        }
    }

    // Recalculate smooth normals in modified topological region
    computeSmoothNormals();
}

void HalfEdgeMesh::getRawBuffers(std::vector<float>& outVertices, std::vector<unsigned int>& outIndices) const {
    outVertices.clear();
    outIndices.clear();

    // Map active vertex handles to packed output indices
    std::unordered_map<VertexHandle, unsigned int> activeVertMap;
    unsigned int activeCount = 0;

    for (size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].halfEdge != INVALID_HALFEDGE_HANDLE) {
            outVertices.push_back(vertices[i].position.x);
            outVertices.push_back(vertices[i].position.y);
            outVertices.push_back(vertices[i].position.z);
            activeVertMap[static_cast<VertexHandle>(i)] = activeCount++;
        }
    }

    // Gather and reconstruct updated triangle layouts
    for (size_t f = 0; f < faces.size(); ++f) {
        HalfEdgeHandle he = faces[f].halfEdge;
        if (he != INVALID_HALFEDGE_HANDLE) {
            HalfEdgeHandle prev = halfEdges[he].prev;
            if (prev == INVALID_HALFEDGE_HANDLE) continue;
            VertexHandle v0 = halfEdges[prev].vertex;
            VertexHandle v1 = halfEdges[he].vertex;
            HalfEdgeHandle next = halfEdges[he].next;
            if (next == INVALID_HALFEDGE_HANDLE) continue;
            VertexHandle v2 = halfEdges[next].vertex;

            if (activeVertMap.count(v0) && activeVertMap.count(v1) && activeVertMap.count(v2)) {
                outIndices.push_back(activeVertMap[v0]);
                outIndices.push_back(activeVertMap[v1]);
                outIndices.push_back(activeVertMap[v2]);
            }
        }
    }
}

MemoryBufferBinding HalfEdgeMesh::exportRenderBuffers() const {
    MemoryBufferBinding binding;
    
    // Map active vertex handles to packed output indices
    std::unordered_map<VertexHandle, unsigned int> activeVertMap;
    unsigned int activeCount = 0;

    // Pre-reserve buffers to minimize heap reallocations (crucial for real-time performance)
    binding.positions.reserve(vertices.size() * 3);
    binding.normals.reserve(vertices.size() * 3);
    binding.indices.reserve(faces.size() * 3);

    for (size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].halfEdge != INVALID_HALFEDGE_HANDLE) {
            binding.positions.push_back(vertices[i].position.x);
            binding.positions.push_back(vertices[i].position.y);
            binding.positions.push_back(vertices[i].position.z);

            binding.normals.push_back(vertices[i].normal.x);
            binding.normals.push_back(vertices[i].normal.y);
            binding.normals.push_back(vertices[i].normal.z);

            activeVertMap[static_cast<VertexHandle>(i)] = activeCount++;
        }
    }

    // Gather and reconstruct updated triangle layouts
    for (size_t f = 0; f < faces.size(); ++f) {
        HalfEdgeHandle he = faces[f].halfEdge;
        if (he != INVALID_HALFEDGE_HANDLE) {
            HalfEdgeHandle prev = halfEdges[he].prev;
            if (prev == INVALID_HALFEDGE_HANDLE) continue;
            VertexHandle v0 = halfEdges[prev].vertex;
            VertexHandle v1 = halfEdges[he].vertex;
            HalfEdgeHandle next = halfEdges[he].next;
            if (next == INVALID_HALFEDGE_HANDLE) continue;
            VertexHandle v2 = halfEdges[next].vertex;

            if (activeVertMap.count(v0) && activeVertMap.count(v1) && activeVertMap.count(v2)) {
                binding.indices.push_back(activeVertMap[v0]);
                binding.indices.push_back(activeVertMap[v1]);
                binding.indices.push_back(activeVertMap[v2]);
            }
        }
    }

    return binding;
}

// Helper for Möller-Trumbore ray-triangle intersection
static bool rayTriangleIntersectLocal(
    const Vector3& rayOrigin, const Vector3& rayDir,
    const Vector3& v0, const Vector3& v1, const Vector3& v2,
    float& t, float& u, float& vOut
) {
    Vector3 edge1(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
    Vector3 edge2(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);
    
    // Cross product: h = rayDir x edge2
    float hx = rayDir.y * edge2.z - rayDir.z * edge2.y;
    float hy = rayDir.z * edge2.x - rayDir.x * edge2.z;
    float hz = rayDir.x * edge2.y - rayDir.y * edge2.x;
    
    float a = edge1.x * hx + edge1.y * hy + edge1.z * hz;
    if (a > -1e-6f && a < 1e-6f) return false; // Ray is parallel to triangle
    
    float f = 1.0f / a;
    Vector3 s(rayOrigin.x - v0.x, rayOrigin.y - v0.y, rayOrigin.z - v0.z);
    u = f * (s.x * hx + s.y * hy + s.z * hz);
    if (u < 0.0f || u > 1.0f) return false;
    
    // Cross product: q = s x edge1
    float qx = s.y * edge1.z - s.z * edge1.y;
    float qy = s.z * edge1.x - s.x * edge1.z;
    float qz = s.x * edge1.y - s.y * edge1.x;
    
    vOut = f * (rayDir.x * qx + rayDir.y * qy + rayDir.z * qz);
    if (vOut < 0.0f || u + vOut > 1.0f) return false;
    
    t = f * (edge2.x * qx + edge2.y * qy + edge2.z * qz);
    return t > 1e-6f;
}

void HalfEdgeMesh::applyBrush(const std::string& brushType, float screenX, float screenY, const float* invViewProj, float brushRadius, float intensity, Vector3 cameraDir, Vector3 grabOffset) {
    if (!invViewProj || brushRadius <= 0.0f) return;

    // 1. Raycasting precise: unproject screen space coordinates directly utilizing the inverse unified camera projection matrix
    auto unproject = [](float x, float y, float z, const float* m) {
        float w = m[3] * x + m[7] * y + m[11] * z + m[15];
        if (std::abs(w) < 1e-9f) w = 1.0f;
        return Vector3(
            (m[0] * x + m[4] * y + m[8] * z + m[12]) / w,
            (m[1] * x + m[5] * y + m[9] * z + m[13]) / w,
            (m[2] * x + m[6] * y + m[10] * z + m[14]) / w
        );
    };

    Vector3 rayOrigin = unproject(screenX, screenY, -1.0f, invViewProj);
    Vector3 rayFar = unproject(screenX, screenY, 1.0f, invViewProj);
    Vector3 rayDir(rayFar.x - rayOrigin.x, rayFar.y - rayOrigin.y, rayFar.z - rayOrigin.z);

    float rayLen = std::sqrt(rayDir.x * rayDir.x + rayDir.y * rayDir.y + rayDir.z * rayDir.z);
    if (rayLen > 1e-6f) {
        rayDir.x /= rayLen;
        rayDir.y /= rayLen;
        rayDir.z /= rayLen;
    }

    // Identify closest triangle intersection point (P_local) and surface normal (N_surface)
    float tMin = 1e30f;
    Vector3 pHitLocal;
    Vector3 nHitLocal(0.0f, 0.0f, 1.0f);
    bool hitFound = false;

    for (size_t f = 0; f < faces.size(); ++f) {
        HalfEdgeHandle he = faces[f].halfEdge;
        if (he == INVALID_HALFEDGE_HANDLE) continue;

        HalfEdgeHandle prev = halfEdges[he].prev;
        HalfEdgeHandle next = halfEdges[he].next;
        if (prev == INVALID_HALFEDGE_HANDLE || next == INVALID_HALFEDGE_HANDLE) continue;

        VertexHandle v0 = halfEdges[prev].vertex;
        VertexHandle v1 = halfEdges[he].vertex;
        VertexHandle v2 = halfEdges[next].vertex;

        if (!isValidVertex(v0) || !isValidVertex(v1) || !isValidVertex(v2)) continue;

        float t = 0.0f, u = 0.0f, v = 0.0f;
        if (rayTriangleIntersectLocal(rayOrigin, rayDir, vertices[v0].position, vertices[v1].position, vertices[v2].position, t, u, v)) {
            if (t < tMin) {
                tMin = t;
                pHitLocal = Vector3(rayOrigin.x + rayDir.x * t, rayOrigin.y + rayDir.y * t, rayOrigin.z + rayDir.z * t);
                
                // Interpolate highly-precise surface normals using barycentric coordinates
                float wBary = 1.0f - u - v;
                float nx = vertices[v0].normal.x * wBary + vertices[v1].normal.x * u + vertices[v2].normal.x * v;
                float ny = vertices[v0].normal.y * wBary + vertices[v1].normal.y * u + vertices[v2].normal.y * v;
                float nz = vertices[v0].normal.z * wBary + vertices[v1].normal.z * u + vertices[v2].normal.z * v;

                float lenN = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (lenN > 1e-6f) {
                    nHitLocal = Vector3(nx / lenN, ny / lenN, nz / lenN);
                } else {
                    // Fall back to geometric face normal for degenerate triangles
                    Vector3 edge1(vertices[v1].position.x - vertices[v0].position.x, vertices[v1].position.y - vertices[v0].position.y, vertices[v1].position.z - vertices[v0].position.z);
                    Vector3 edge2(vertices[v2].position.x - vertices[v0].position.x, vertices[v2].position.y - vertices[v0].position.y, vertices[v2].position.z - vertices[v0].position.z);
                    float fnx = edge1.y * edge2.z - edge1.z * edge2.y;
                    float fny = edge1.z * edge2.x - edge1.x * edge2.z;
                    float fnz = edge1.x * edge2.y - edge1.y * edge2.x;
                    float lenFN = std::sqrt(fnx * fnx + fny * fny + fnz * fnz);
                    if (lenFN > 1e-6f) {
                        nHitLocal = Vector3(fnx / lenFN, fny / lenFN, fnz / lenFN);
                    } else {
                        nHitLocal = Vector3(0.0f, 0.0f, 1.0f);
                    }
                }
                hitFound = true;
            }
        }
    }

    if (!hitFound) return; // Cursor misses the model bounds

    // 2. Perform Euclidean distance filtering and calculate Gaussian falloff
    size_t numVerts = vertices.size();
    for (size_t v = 0; v < numVerts; ++v) {
        if (vertices[v].halfEdge == INVALID_HALFEDGE_HANDLE) continue;

        const Vector3& pos = vertices[v].position;
        float dx = pos.x - pHitLocal.x;
        float dy = pos.y - pHitLocal.y;
        float dz = pos.z - pHitLocal.z;
        float d = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (d <= brushRadius) {
            // Gaussian Falloff factor: w = exp(-4.0 * (d / radius)^2)
            float r = d / brushRadius;
            float falloff = std::exp(-4.0f * r * r);

            // 3. Apply Displacement based on Active Brush type
            if (brushType == "clay" || brushType == "Clay" || brushType == "clay_buildup" || brushType == "ClayBuildup") {
                // Clay / Standard brush: shift along locally interpolated surface normal vector
                vertices[v].position.x += nHitLocal.x * intensity * falloff;
                vertices[v].position.y += nHitLocal.y * intensity * falloff;
                vertices[v].position.z += nHitLocal.z * intensity * falloff;
            } else if (brushType == "grab" || brushType == "Grab" || brushType == "move_elastic" || brushType == "move") {
                // Grab brush: project translation offset onto tangent plane orthogonal to the intersecting surface normal
                float dot = grabOffset.x * nHitLocal.x + grabOffset.y * nHitLocal.y + grabOffset.z * nHitLocal.z;
                Vector3 tangentOffset(
                    grabOffset.x - dot * nHitLocal.x,
                    grabOffset.y - dot * nHitLocal.y,
                    grabOffset.z - dot * nHitLocal.z
                );

                vertices[v].position.x += tangentOffset.x * falloff * intensity;
                vertices[v].position.y += tangentOffset.y * falloff * intensity;
                vertices[v].position.z += tangentOffset.z * falloff * intensity;
            } else if (brushType == "smooth" || brushType == "Smooth") {
                // Smooth brush: apply neighbor-weighted Laplacian relaxation
                auto neighbors = getNeighboringVertices(static_cast<VertexHandle>(v));
                if (!neighbors.empty()) {
                    float sumX = 0.0f, sumY = 0.0f, sumZ = 0.0f;
                    for (VertexHandle nv : neighbors) {
                        sumX += vertices[nv].position.x;
                        sumY += vertices[nv].position.y;
                        sumZ += vertices[nv].position.z;
                    }
                    float avgX = sumX / neighbors.size();
                    float avgY = sumY / neighbors.size();
                    float avgZ = sumZ / neighbors.size();

                    vertices[v].position.x += (avgX - vertices[v].position.x) * intensity * falloff * 0.5f;
                    vertices[v].position.y += (avgY - vertices[v].position.y) * intensity * falloff * 0.5f;
                    vertices[v].position.z += (avgZ - vertices[v].position.z) * intensity * falloff * 0.5f;
                }
            } else {
                // Default fallback: deform along standard vertex normal
                const Vector3& norm = vertices[v].normal;
                vertices[v].position.x += norm.x * intensity * falloff;
                vertices[v].position.y += norm.y * intensity * falloff;
                vertices[v].position.z += norm.z * intensity * falloff;
            }
        }
    }

    // 4. Update data structures and recalculate optimized smooth normals instantly
    computeSmoothNormals();
}

void HalfEdgeMesh::applyGlobalTransform(Vector3 translation, Vector3 rotationAngles) {
    // Construct quaternion from Euler angles: Pitch (X-axis), Yaw (Y-axis), Roll (Z-axis)
    float cx = std::cos(rotationAngles.x * 0.5f);
    float sx = std::sin(rotationAngles.x * 0.5f);
    float cy = std::cos(rotationAngles.y * 0.5f);
    float sy = std::sin(rotationAngles.y * 0.5f);
    float cz = std::cos(rotationAngles.z * 0.5f);
    float sz = std::sin(rotationAngles.z * 0.5f);

    float qw = cx * cy * cz + sx * sy * sz;
    float qx = sx * cy * cz - cx * sy * sz;
    float qy = cx * sy * cz + sx * cy * sz;
    float qz = cx * cy * sz - sx * sy * cz;

    // Calculate active vertex centroid to act as pivot
    size_t numVerts = vertices.size();
    float sumX = 0.0f, sumY = 0.0f, sumZ = 0.0f;
    size_t activeCount = 0;
    for (size_t i = 0; i < numVerts; ++i) {
        if (vertices[i].halfEdge != INVALID_HALFEDGE_HANDLE) {
            sumX += vertices[i].position.x;
            sumY += vertices[i].position.y;
            sumZ += vertices[i].position.z;
            activeCount++;
        }
    }
    float centroidX = activeCount > 0 ? sumX / activeCount : 0.0f;
    float centroidY = activeCount > 0 ? sumY / activeCount : 0.0f;
    float centroidZ = activeCount > 0 ? sumZ / activeCount : 0.0f;

    for (size_t i = 0; i < numVerts; ++i) {
        if (vertices[i].halfEdge == INVALID_HALFEDGE_HANDLE) continue;

        // Subtract centroid to rotate around local pivot, rotate position vector V by Quaternion Q
        float vx = vertices[i].position.x - centroidX;
        float vy = vertices[i].position.y - centroidY;
        float vz = vertices[i].position.z - centroidZ;

        float tx = 2.0f * (qy * vz - qz * vy);
        float ty = 2.0f * (qz * vx - qx * vz);
        float tz = 2.0f * (qx * vy - qy * vx);

        float rx = vx + qw * tx + (qy * tz - qz * ty);
        float ry = vy + qw * ty + (qz * tx - qx * tz);
        float rz = vz + qw * tz + (qx * ty - qy * tx);

        // Add centroid back + global translation
        vertices[i].position.x = rx + centroidX + translation.x;
        vertices[i].position.y = ry + centroidY + translation.y;
        vertices[i].position.z = rz + centroidZ + translation.z;

        // Rotate normal vector N by Quaternion Q (no centroid offset or translation)
        float nx = vertices[i].normal.x;
        float ny = vertices[i].normal.y;
        float nz = vertices[i].normal.z;

        float ntx = 2.0f * (qy * nz - qz * ny);
        float nty = 2.0f * (qz * nx - qx * nz);
        float ntz = 2.0f * (qx * ny - qy * nx);

        vertices[i].normal.x = nx + qw * ntx + (qy * ntz - qz * nty);
        vertices[i].normal.y = ny + qw * nty + (qz * ntx - qx * ntz);
        vertices[i].normal.z = nz + qw * ntz + (qx * nty - qy * ntx);

        float normLen = std::sqrt(vertices[i].normal.x * vertices[i].normal.x +
                                  vertices[i].normal.y * vertices[i].normal.y +
                                  vertices[i].normal.z * vertices[i].normal.z);
        if (normLen > 1e-6f) {
            vertices[i].normal.x /= normLen;
            vertices[i].normal.y /= normLen;
            vertices[i].normal.z /= normLen;
        }
    }

    // Force recalculation of optimized surface normals
    computeSmoothNormals();
}

std::vector<uint8_t> HalfEdgeMesh::serializeMesh(std::string targetFormat) const {
    // Normalise targetFormat
    std::string format = targetFormat;
    std::transform(format.begin(), format.end(), format.begin(), ::tolower);

    // Default to OBJ if not stl
    if (format.find("stl") != std::string::npos) {
        std::vector<uint8_t> stlBytes;
        // 1. Write binary 80 bytes header (empty)
        stlBytes.resize(80, 0);

        // 2. Count active triangles
        uint32_t numTriangles = 0;
        for (size_t f = 0; f < faces.size(); ++f) {
            if (faces[f].halfEdge != INVALID_HALFEDGE_HANDLE) {
                numTriangles++;
            }
        }

        // 3. Write number of triangles (4 bytes)
        uint8_t countBytes[4];
        std::copy(reinterpret_cast<const uint8_t*>(&numTriangles), reinterpret_cast<const uint8_t*>(&numTriangles) + 4, countBytes);
        for (int b = 0; b < 4; ++b) {
            stlBytes.push_back(countBytes[b]);
        }

        // 4. For each face, write normal + 3 vertices loop
        for (size_t f = 0; f < faces.size(); ++f) {
            HalfEdgeHandle he = faces[f].halfEdge;
            if (he != INVALID_HALFEDGE_HANDLE) {
                HalfEdgeHandle prev = halfEdges[he].prev;
                VertexHandle v0 = halfEdges[prev].vertex;
                VertexHandle v1 = halfEdges[he].vertex;
                HalfEdgeHandle next = halfEdges[he].next;
                VertexHandle v2 = halfEdges[next].vertex;

                if (isValidVertex(v0) && isValidVertex(v1) && isValidVertex(v2)) {
                    const Vector3& p0 = vertices[v0].position;
                    const Vector3& p1 = vertices[v1].position;
                    const Vector3& p2 = vertices[v2].position;

                    // Calculate face normal
                    float ux = p1.x - p0.x;
                    float uy = p1.y - p0.y;
                    float uz = p1.z - p0.z;

                    float vx = p2.x - p0.x;
                    float vy = p2.y - p0.y;
                    float vz = p2.z - p0.z;

                    float nx = uy * vz - uz * vy;
                    float ny = uz * vx - ux * vz;
                    float nz = ux * vy - uy * vx;

                    float lenN = std::sqrt(nx * nx + ny * ny + nz * nz);
                    if (lenN > 1e-6f) {
                        nx /= lenN;
                        ny /= lenN;
                        nz /= lenN;
                    } else {
                        nx = 0.0f; ny = 0.0f; nz = 1.0f;
                    }

                    float facetData[12] = {
                        nx, ny, nz,
                        p0.x, p0.y, p0.z,
                        p1.x, p1.y, p1.z,
                        p2.x, p2.y, p2.z
                    };

                    uint8_t facetBytes[48];
                    std::copy(reinterpret_cast<const uint8_t*>(facetData), reinterpret_cast<const uint8_t*>(facetData) + 48, facetBytes);
                    for (int b = 0; b < 48; ++b) {
                        stlBytes.push_back(facetBytes[b]);
                    }

                    // Attribute byte count (2 bytes, set to 0)
                    stlBytes.push_back(0);
                    stlBytes.push_back(0);
                }
            }
        }
        return stlBytes;
    } else {
        // OBJ Format Text Serialization
        std::string objText;
        std::unordered_map<VertexHandle, int> activeVertMap;
        int indexCounter = 1;

        for (size_t i = 0; i < vertices.size(); ++i) {
            if (vertices[i].halfEdge != INVALID_HALFEDGE_HANDLE) {
                objText += "v " + std::to_string(vertices[i].position.x) + " " + std::to_string(vertices[i].position.y) + " " + std::to_string(vertices[i].position.z) + "\n";
                objText += "vn " + std::to_string(vertices[i].normal.x) + " " + std::to_string(vertices[i].normal.y) + " " + std::to_string(vertices[i].normal.z) + "\n";
                activeVertMap[static_cast<VertexHandle>(i)] = indexCounter++;
            }
        }

        for (size_t f = 0; f < faces.size(); ++f) {
            HalfEdgeHandle he = faces[f].halfEdge;
            if (he != INVALID_HALFEDGE_HANDLE) {
                HalfEdgeHandle prev = halfEdges[he].prev;
                VertexHandle v0 = halfEdges[prev].vertex;
                VertexHandle v1 = halfEdges[he].vertex;
                HalfEdgeHandle next = halfEdges[he].next;
                VertexHandle v2 = halfEdges[next].vertex;

                if (activeVertMap.count(v0) && activeVertMap.count(v1) && activeVertMap.count(v2)) {
                    std::string i0 = std::to_string(activeVertMap[v0]);
                    std::string i1 = std::to_string(activeVertMap[v1]);
                    std::string i2 = std::to_string(activeVertMap[v2]);
                    objText += "f " + i0 + "//" + i0 + " " + i1 + "//" + i1 + " " + i2 + "//" + i2 + "\n";
                }
            }
        }
        return std::vector<uint8_t>(objText.begin(), objText.end());
    }
}


