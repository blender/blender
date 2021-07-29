cdef class MeshData:

    def __cinit__(self, Vector3DList vertices = None,
                        EdgeIndicesList edges = None,
                        PolygonIndicesList polygons = None):

        if vertices is None: vertices = Vector3DList()
        if edges is None: edges = EdgeIndicesList()
        if polygons is None: polygons = PolygonIndicesList()

        self.vertices = vertices
        self.edges = edges
        self.polygons = polygons

    def copy(self):
        return MeshData(self.vertices.copy(), self.edges.copy(), self.polygons.copy())

    def isValid(self):
        if len(self.edges) > 0:
            if self.edges.getMaxIndex() >= len(self.vertices):
                return False
        if len(self.polygons) > 0:
            if self.polygons.getMaxIndex() >= len(self.vertices):
                return False
        return True

    def __repr__(self):
        return "<AN Mesh Data Object: Vertices: {}, Edges: {}, Polygons: {}>".format(
                len(self.vertices), len(self.edges), len(self.polygons))

    @classmethod
    def join(cls, *meshes):
        cdef MeshData newMeshData = MeshData()
        for meshData in meshes:
            newMeshData.append(meshData)
        return newMeshData

    def append(self, MeshData meshData):
        cdef long vertexOffset = self.vertices.length
        cdef long edgeOffset = self.edges.length
        cdef long polygonIndicesOffset = self.polygons.indices.length
        cdef long i

        self.vertices.extend(meshData.vertices)

        self.edges.extend(meshData.edges)
        for i in range(meshData.edges.length):
            self.edges.data[edgeOffset + i].v1 += vertexOffset
            self.edges.data[edgeOffset + i].v2 += vertexOffset

        self.polygons.extend(meshData.polygons)
        for i in range(meshData.polygons.indices.length):
            self.polygons.indices.data[polygonIndicesOffset + i] += vertexOffset
