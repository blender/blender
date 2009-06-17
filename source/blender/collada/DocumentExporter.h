struct Scene;

class DocumentExporter
{
 public:
	void exportCurrentScene(Scene *sce, const char* filename);
	void exportScenes(const char* filename);
};
