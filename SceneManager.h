#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "HalfEdgeMesh.h"

class SceneManager {
public:
    SceneManager();
    ~SceneManager() = default;

    /**
     * Misión 2: Añade un nuevo modelo vacío o preestablecido al espacio de trabajo.
     * Retorna el ObjectID de la ranura creada.
     */
    uint32_t createNewMeshSlot();

    /**
     * Selecciona la malla activa por ID.
     */
    void setActiveMesh(uint32_t id);

    /**
     * Obtiene el ObjectID de la malla seleccionada actualmente.
     */
    uint32_t getActiveMeshId() const { return activeMeshId; }

    /**
     * Obtiene un puntero a la malla actualmente seleccionada/activa.
     */
    HalfEdgeMesh* getActiveMesh() const;

    /**
     * Obtiene un puntero a un objeto de la escena por su ID.
     */
    HalfEdgeMesh* getMesh(uint32_t id) const;

    /**
     * Libera un objeto de malla de la escena.
     */
    void removeMesh(uint32_t id);

    /**
     * Retorna la lista de todas las IDs de mallas en la escena.
     */
    std::vector<uint32_t> getMeshIds() const;

    /**
     * Misión 3: Pipeline de Operaciones Booleanas de Malla.
     * operationType: 0 = Unión, 1 = Diferencia/Corte, 2 = Intersección
     */
    void executeMeshBoolean(uint32_t meshIdA, uint32_t meshIdB, int operationType);

private:
    std::map<uint32_t, std::unique_ptr<HalfEdgeMesh>> sceneObjects;
    uint32_t activeMeshId;
    uint32_t nextObjectId;
};
