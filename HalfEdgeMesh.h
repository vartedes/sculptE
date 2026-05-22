#pragma once

#include <vector>
#include <unordered_map>
#include <utility>
#include <cmath>

// Use strongly typed index handles for optimal cache-locality and zero pointer-invalidation under mutation
using VertexHandle = int;
using HalfEdgeHandle = int;
using FaceHandle = int;

const VertexHandle INVALID_VERTEX_HANDLE = -1;
const HalfEdgeHandle INVALID_HALFEDGE_HANDLE = -1;
const FaceHandle INVALID_FACE_HANDLE = -1;

struct Vector3 {
    float x;
    float y;
    float z;

    Vector3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
};

struct Vertex {
    Vector3 position;
    Vector3 normal;
    // An outgoing half-edge starting from this vertex (O(1) access to neighborhood traversal)
    HalfEdgeHandle halfEdge = INVALID_HALFEDGE_HANDLE;
};

struct HalfEdge {
    // The target vertex this half-edge points pointer-to
    VertexHandle vertex = INVALID_VERTEX_HANDLE;
    // The face bordered by this half-edge
    FaceHandle face = INVALID_FACE_HANDLE;
    // Next half-edge in counter-clockwise order around the face
    HalfEdgeHandle next = INVALID_HALFEDGE_HANDLE;
    // Previous half-edge in counter-clockwise order around the face
    HalfEdgeHandle prev = INVALID_HALFEDGE_HANDLE;
    // Opposite directed half-edge matching this physical split edge
    HalfEdgeHandle twin = INVALID_HALFEDGE_HANDLE;
};

struct Face {
    // One of the bounding half-edges of this face polygon
    HalfEdgeHandle halfEdge = INVALID_HALFEDGE_HANDLE;
};

struct MemoryBufferBinding {
    // Contiguous flat float/int buffers in memory for direct GPU upload
    std::vector<float> positions;      // [x0, y0, z0, x1, y1, z1, ...]
    std::vector<float> normals;        // [nx0, ny0, nz1, nx1, ny1, nz1, ...]
    std::vector<unsigned int> indices; // [f0_v0, f0_v1, f0_v2, ...]

    // Direct pointers to flat buffers to leverage memory-mapping bindings (Pybind11, ctypes, Wasm heap offset keys, etc.)
    const float* getPositionsPtr() const { return positions.data(); }
    const float* getNormalsPtr() const { return normals.data(); }
    const unsigned int* getIndicesPtr() const { return indices.data(); }

    size_t getPositionsSize() const { return positions.size(); }
    size_t getNormalsSize() const { return normals.size(); }
    size_t getIndicesSize() const { return indices.size(); }
};

class HalfEdgeMesh {
public:
    HalfEdgeMesh() = default;
    ~HalfEdgeMesh() = default;

    /**
     * Initializes the half-edge topological representation from standard index/vertex buffers.
     * @param rawVertices Flat array representing vertex positions [x0, y0, z0, x1, y1, z1, ...]
     * @param rawIndices Flat array representing face triangle index sequences [f0_v0, f0_v1, f0_v2, ...]
     * @return True if successful, false otherwise.
     */
    bool initialize(const std::vector<float>& rawVertices, const std::vector<unsigned int>& rawIndices);

    /**
     * Recomputes highly optimized angle-weighted smooth vertex normals across the mesh,
     * completely eliminating low-quality shading artifacts or black spots.
     */
    void computeSmoothNormals();

    /**
     * Dynamic Topology (Dyntopo) main routine. 
     * Iterates over all active half-edges within the brush radius to split long edges
     * and collapse critically short edges to perform real-time optimization.
     */
    void dynamicRemesh(Vector3 brushCenter, float brushRadius, float detailThreshold);

    /**
     * Applies precise brush deformation on the Half-Edge mesh.
     * 1. Transform screen (X, Y) using the inverse view projection matrix to cast a 3D ray.
     * 2. Find the intersection point Phit on the mesh triangles.
     * 3. Compute Euclidean/Geodesic distance, applying Gaussian or Smoothstep falloff.
     * 4. Deform based on brush type (Clay: along vertex average normal / Grab: in the view-parallel plane).
     */
    void applyBrush(const std::string& brushType, float screenX, float screenY, const float* invViewProj, float brushRadius, float intensity, Vector3 cameraDir, Vector3 grabOffset);

    /**
     * Packs active vertices and faces from the Half-Edge topology structure back into 
     * standard flat index and vertices buffers.
     */
    void getRawBuffers(std::vector<float>& outVertices, std::vector<unsigned int>& outIndices) const;

    /**
     * Highly optimized memory rendering pipeline exporter (Binding/Export).
     * Extracts active vertices, positions, computed smooth normals, and face connectivity indices
     * directly into contiguous flat buffers in a single cache-friendly pass.
     * This avoids serialization overhead and achieves high-performance GPU upload speeds.
     */
    MemoryBufferBinding exportRenderBuffers() const;

    /**
     * Applies a 4x4 Homogeneous transformation directly to the mesh vertices.
     * Translation vector, and rotationAngles (Euler angles Pitch, Yaw, Roll).
     * Employs quaternions under the hood to prevent Gimbal Lock.
     */
    void applyGlobalTransform(Vector3 translation, Vector3 rotationAngles);

    /**
     * Serializes the current active half-edge mesh into the requested file format bytes.
     * Supported formats: '.obj', '.stl'.
     */
    std::vector<uint8_t> serializeMesh(std::string targetFormat) const;

    /**
     * O(1) query to retrieve adjacent vertex handles around vertex v.
     */
    std::vector<VertexHandle> getNeighboringVertices(VertexHandle v) const;

    /**
     * O(1) query to retrieve incident face handles sharing vertex v.
     */
    std::vector<FaceHandle> getNeighboringFaces(VertexHandle v) const;

    /**
     * O(1) query of neighboring face handles adjacent to face f.
     */
    std::vector<FaceHandle> getAdjacentFaces(FaceHandle f) const;

    // Direct accessors
    const std::vector<Vertex>& getVertices() const { return vertices; }
    const std::vector<HalfEdge>& getHalfEdges() const { return halfEdges; }
    const std::vector<Face>& getFaces() const { return faces; }

    // Boundary & index bounds validation checks
    bool isValidVertex(VertexHandle v) const { return v >= 0 && v < static_cast<VertexHandle>(vertices.size()); }
    bool isValidHalfEdge(HalfEdgeHandle he) const { return he >= 0 && he < static_cast<HalfEdgeHandle>(halfEdges.size()); }
    bool isValidFace(FaceHandle f) const { return f >= 0 && f < static_cast<FaceHandle>(faces.size()); }

private:
    VertexHandle splitEdge(HalfEdgeHandle he);
    bool collapseEdge(HalfEdgeHandle he);
    bool checkLinkCondition(VertexHandle U, VertexHandle V) const;

    std::vector<Vertex> vertices;
    std::vector<HalfEdge> halfEdges;
    std::vector<Face> faces;
};
