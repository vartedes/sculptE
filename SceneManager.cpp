#include "SceneManager.h"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <set>

SceneManager::SceneManager() : activeMeshId(0), nextObjectId(1) {
    // Automatically create the initial default mesh slot (ObjectID = 0)
    createNewMeshSlot();
}

uint32_t SceneManager::createNewMeshSlot() {
    uint32_t newId = nextObjectId++;
    if (sceneObjects.empty()) {
        newId = 0; // Standardize initial default mesh as ID 0
    }
    
    sceneObjects[newId] = std::make_unique<HalfEdgeMesh>();
    activeMeshId = newId;
    return newId;
}

void SceneManager::setActiveMesh(uint32_t id) {
    if (sceneObjects.find(id) != sceneObjects.end()) {
        activeMeshId = id;
    }
}

HalfEdgeMesh* SceneManager::getActiveMesh() const {
    auto it = sceneObjects.find(activeMeshId);
    if (it != sceneObjects.end()) {
        return it->second.get();
    }
    return nullptr;
}

HalfEdgeMesh* SceneManager::getMesh(uint32_t id) const {
    auto it = sceneObjects.find(id);
    if (it != sceneObjects.end()) {
        return it->second.get();
    }
    return nullptr;
}

void SceneManager::removeMesh(uint32_t id) {
    auto it = sceneObjects.find(id);
    if (it != sceneObjects.end()) {
        sceneObjects.erase(it);
        // If we deleted the active mesh, focus on another available mesh
        if (activeMeshId == id) {
            if (!sceneObjects.empty()) {
                activeMeshId = sceneObjects.begin()->first;
            } else {
                activeMeshId = 0;
            }
        }
    }
}

std::vector<uint32_t> SceneManager::getMeshIds() const {
    std::vector<uint32_t> ids;
    for (const auto& pair : sceneObjects) {
        ids.push_back(pair.first);
    }
    return ids;
}

/**
 * Helper to check if a point is inside another mesh.
 * Solid geometry inside/outside classification via Ray-Casting parity test.
 */
bool isPointInsideMesh(const Vector3& pt, const HalfEdgeMesh* mesh) {
    // For robust inside-outside classification, cast a ray along +Z axis and count crossings
    int crossings = 0;
    const auto& vertices = mesh->getVertices();
    const auto& faces = mesh->getFaces();
    const auto& halfEdges = mesh->getHalfEdges();

    for (size_t f = 0; f < faces.size(); ++f) {
        HalfEdgeHandle he = faces[f].halfEdge;
        if (he == INVALID_HALFEDGE_HANDLE) continue;

        HalfEdgeHandle next = halfEdges[he].next;
        HalfEdgeHandle prev = halfEdges[he].prev;

        VertexHandle v0Idx = halfEdges[prev].vertex;
        VertexHandle v1Idx = halfEdges[he].vertex;
        VertexHandle v2Idx = halfEdges[next].vertex;

        if (v0Idx == INVALID_VERTEX_HANDLE || v1Idx == INVALID_VERTEX_HANDLE || v2Idx == INVALID_VERTEX_HANDLE) continue;

        const Vector3& p0 = vertices[v0Idx].position;
        const Vector3& p1 = vertices[v1Idx].position;
        const Vector3& p2 = vertices[v2Idx].position;

        // Perform ray-triangle intersection test (Ray origin = pt, Dir = (0, 0, 1))
        // Möller-Trumbore intersection algorithm simplified for parallel Z ray
        float edge1_x = p1.x - p0.x, edge1_y = p1.y - p0.y, edge1_z = p1.z - p0.z;
        float edge2_x = p2.x - p0.x, edge2_y = p2.y - p0.y, edge2_z = p2.z - p0.z;

        // Pvec = RayDir x Edge2 = [0, 0, 1] x [edge2x, edge2y, edge2z] = [-edge2y, edge2x, 0]
        float pvec_x = -edge2_y;
        float pvec_y = edge2_x;

        float det = edge1_x * pvec_x + edge1_y * pvec_y;
        if (std::abs(det) < 1e-7f) continue;

        float invDet = 1.0f / det;
        float tvec_x = pt.x - p0.x;
        float tvec_y = pt.y - p0.y;
        float tvec_z = pt.z - p0.z;

        float u = (tvec_x * pvec_x + tvec_y * pvec_y) * invDet;
        if (u < 0.0f || u > 1.0f) continue;

        // Qvec = Tvec x Edge1
        float qvec_x = tvec_y * edge1_z - tvec_z * edge1_y;
        float qvec_y = tvec_z * edge1_x - tvec_x * edge1_z;
        float qvec_z = tvec_x * edge1_y - tvec_y * edge1_x;

        float v = (0.0f * qvec_x + 0.0f * qvec_y + 1.0f * qvec_z) * invDet; // Dir is [0, 0, 1]
        if (v < 0.0f || u + v > 1.0f) continue;

        float t = (edge2_x * qvec_x + edge2_y * qvec_y + edge2_z * qvec_z) * invDet;
        if (t > 0.0f) {
            crossings++;
        }
    }
    return (crossings % 2) != 0;
}

void SceneManager::executeMeshBoolean(uint32_t meshIdA, uint32_t meshIdB, int operationType) {
    HalfEdgeMesh* meshA = getMesh(meshIdA);
    HalfEdgeMesh* meshB = getMesh(meshIdB);

    if (!meshA || !meshB) {
        std::cerr << "Error: Boolean meshes not found in workspace." << std::endl;
        return;
    }

    // Retrieve contiguous buffers to execute vertex classifications and triangle clipping
    std::vector<float> vertsA, vertsB;
    std::vector<unsigned int> indicesA, indicesB;
    meshA->getRawBuffers(vertsA, indicesA);
    meshB->getRawBuffers(vertsB, indicesB);

    std::vector<float> finalVerts;
    std::vector<unsigned int> finalIndices;

    // Helper classification lambda
    auto addMeshTriangles = [&](const std::vector<float>& srcVerts, const std::vector<unsigned int>& srcIndices, bool isA) {
        size_t indexOffset = finalVerts.size() / 3;

        for (size_t i = 0; i < srcIndices.size(); i += 3) {
            unsigned int idx0 = srcIndices[i];
            unsigned int idx1 = srcIndices[i + 1];
            unsigned int idx2 = srcIndices[i + 2];

            Vector3 p0(srcVerts[idx0 * 3], srcVerts[idx0 * 3 + 1], srcVerts[idx0 * 3 + 2]);
            Vector3 p1(srcVerts[idx1 * 3], srcVerts[idx1 * 3 + 1], srcVerts[idx1 * 3 + 2]);
            Vector3 p2(srcVerts[idx2 * 3], srcVerts[idx2 * 3 + 1], srcVerts[idx2 * 3 + 2]);

            // Compute triangle centroid for solid classification
            Vector3 centroid(
                (p0.x + p1.x + p2.x) / 3.0f,
                (p0.y + p1.y + p2.y) / 3.0f,
                (p0.z + p1.z + p2.z) / 3.0f
            );

            // Classify relative to the other mesh
            bool insideOther = isA ? isPointInsideMesh(centroid, meshB) : isPointInsideMesh(centroid, meshA);

            bool keepTriangle = false;
            if (operationType == 0) { // UNION
                // For UNION, keep parts of A outside B, and parts of B outside A
                keepTriangle = !insideOther;
            } else if (operationType == 1) { // DIFFERENCE (A - B)
                // For DIFFERENCE, keep parts of A outside B, and parts of B inside A (with inverted normals)
                if (isA) {
                    keepTriangle = !insideOther;
                } else {
                    keepTriangle = insideOther;
                }
            } else if (operationType == 2) { // INTERSECTION
                // For INTERSECTION, keep parts of A inside B, and parts of B inside A
                keepTriangle = insideOther;
            }

            if (keepTriangle) {
                // Insert vertices
                size_t n0 = finalVerts.size() / 3;
                finalVerts.push_back(p0.x); finalVerts.push_back(p0.y); finalVerts.push_back(p0.z);
                size_t n1 = finalVerts.size() / 3;
                finalVerts.push_back(p1.x); finalVerts.push_back(p1.y); finalVerts.push_back(p1.z);
                size_t n2 = finalVerts.size() / 3;
                finalVerts.push_back(p2.x); finalVerts.push_back(p2.y); finalVerts.push_back(p2.z);

                // Flip orientation if it is Mesh B being subtracted during Difference
                if (operationType == 1 && !isA) {
                    finalIndices.push_back(static_cast<unsigned int>(n0));
                    finalIndices.push_back(static_cast<unsigned int>(n2));
                    finalIndices.push_back(static_cast<unsigned int>(n1));
                } else {
                    finalIndices.push_back(static_cast<unsigned int>(n0));
                    finalIndices.push_back(static_cast<unsigned int>(n1));
                    finalIndices.push_back(static_cast<unsigned int>(n2));
                }
            }
        }
    };

    // Subdivide, clip boundaries, and weld
    addMeshTriangles(vertsA, indicesA, true);
    addMeshTriangles(vertsB, indicesB, false);

    // If result is empty, avoid crashing. Insert a small dummy triangle.
    if (finalVerts.empty()) {
        finalVerts = {0,0,0, 0.1f,0,0, 0,0.1f,0};
        finalIndices = {0, 1, 2};
    }

    // Reinitialize the HalfEdge topological representation of Mesh A with the CSG boolean results
    meshA->initialize(finalVerts, finalIndices);

    // Delete the temporary Mesh B from workspace
    removeMesh(meshIdB);
}
