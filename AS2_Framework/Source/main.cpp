#include "../Externals/Include/Common.h"

#include <ctime>

#define MENU_TIMER_START 1
#define MENU_TIMER_STOP 2
#define MENU_EXIT 3
#define SHADOW_WIDTH 8192
#define SHADOW_HEIGHT 8192

GLubyte timer_cnt = 0;
bool timer_enabled = true;
unsigned int timer_speed = 16;

using namespace glm;
using namespace std;

char** loadShaderSource(const char* file)
{
    FILE* fp = fopen(file, "rb");
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *src = new char[sz + 1];
    fread(src, sizeof(char), sz, fp);
    src[sz] = '\0';
    char **srcp = new char*[1];
    srcp[0] = src;
    return srcp;
}

void freeShaderSource(char** srcp)
{
    delete[] srcp[0];
    delete[] srcp;
}

//-----------------Begin Load Scene Function and Variables---------------------
GLuint scene_program;
GLint um4mv, um4p;
GLint tex_mode;
GLuint texture_location;
int texture_mode = 0;

mat4 view;					// V of MVP, viewing matrix
mat4 projection;			// P of MVP, projection matrix
mat4 model;					// M of MVP, model matrix
mat4 ModelView;
float viewportAspect;
mat4 scaleOne, M;
mat4 model_matrix;

vec3 cameraPos = vec3(-300.0f, 20.0f, -30.0f);
vec3 cameraFront = vec3(-15.0f, 0.0f, 0.0f);
vec3 cameraUp = vec3(0.0f, 1.0f, 0.0f);

//Initialize Variable For Mouse Control
vec3 cameraSpeed = vec3(10.0f, 10.0f, 10.0f);
float yaws = -90.0;
float pitchs = 0.0;
bool firstMouse = true;
float lastX = 300, lastY = 300;

struct Shape {
	GLuint vao;
	GLuint vbo;
	GLuint vbo_position;
	GLuint vbo_normal;
	GLuint vbo_texcoord;
	GLuint ibo;
	int drawCount;
	int materialID;
};

struct Material {
	GLuint diffuse_tex;
};

vector<Material> vertex_material;
vector<Shape> vertex_shape;

Shape m_shape;


GLuint model_program;
GLuint ssao_program;
GLuint depth_program;

GLuint ssao_vao;
GLuint kernal_ubo;
GLuint plane_vao;
GLuint noise_map;

GLuint tex_toon;

struct
{
	struct
	{
		GLint mv_matrix;
		GLint proj_matrix;
	} toon;

	struct
	{
		GLint normal_map;
		GLint depth_map;
		GLint noise_map;
		GLint noise_scale;
		GLint proj;
	} ssao;

	struct
	{
		GLint mv_matrix;
		GLint proj_matrix;
	} render;
} uniforms;

struct
{
	GLuint fbo;
	GLuint normal_map;
	GLuint depth_map;
} gbuffer;

struct
{
	int width;
	int height;
} viewport_size;

// <---------------------------------------------------- Loader ----------------------------------------------------

typedef struct Vertex {
	vec3 position;
	vec2 texCoords;
	vec3 normal;
	vec3 tangent;
}Vertex;

typedef struct Texture {
	GLuint id;
	aiTextureType type;
	string path;
}Texture;

typedef struct _TextureData
{
	_TextureData(void) :
		width(0),
		height(0),
		data(0)
	{
	}

	int width;
	int height;
	unsigned char* data;
} TextureData;

typedef struct Mesh {
	vector<Vertex> vertexData;
	vector<GLuint> indices;
	vector<Texture> textures;
	GLuint vao, vbo, ebo;

	Mesh() : vao(0), vbo(0), ebo(0) {}

	Mesh(const vector<Vertex> &vertexData, const vector<Texture> &textures, const vector<GLuint> &indices) : vao(0), vbo(0), ebo(0) {
		setData(vertexData, textures, indices);
	}
	void setData(const std::vector<Vertex>& vertData,
		const std::vector<Texture> & textures,
		const std::vector<GLuint>& indices)
	{
		this->vertexData = vertData;
		this->indices = indices;
		this->textures = textures;
		if (!vertData.empty() && !indices.empty())
		{
			this->SetUpMesh();
		}
	}
	void Draw(GLuint program) const
	{
		if (vao == 0 || vbo == 0 || ebo == 0)
			return;

		glUseProgram(program);
		glBindVertexArray(vao);

		int diffuseCnt = 0, specularCnt = 0, textUnitCnt = 0;

		for (vector<Texture>::const_iterator it = textures.begin(); it != textures.end(); it++) {
			stringstream samplerNameStr;
			switch (it->type)
			{
			case aiTextureType_DIFFUSE:
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, it->id);

				samplerNameStr << "texture_diffuse0";
				glUniform1i(glGetUniformLocation(program,
					samplerNameStr.str().c_str()), 0);

				break;
			case aiTextureType_SPECULAR:
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, it->id);

				samplerNameStr << "texture_normal0";
				glUniform1i(glGetUniformLocation(program,
					samplerNameStr.str().c_str()), 1);

				break;
			default:
				std::cerr << "Warning::Mesh::draw, texture type" << it->type
					<< " current not supported." << std::endl;
				break;
			}
		}
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glUseProgram(0);
	}
	void final() const
	{
		glDeleteVertexArrays(1, &this->vao);
		glDeleteBuffers(1, &this->vbo);
		glDeleteBuffers(1, &this->ebo);
	}

	void SetUpMesh() {
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
		glGenBuffers(1, &ebo);

		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertexData.size(), &vertexData[0], GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(3 * sizeof(GL_FLOAT)));
		glEnableVertexAttribArray(1);

		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(5 * sizeof(GL_FLOAT)));
		glEnableVertexAttribArray(2);

		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(8 * sizeof(GL_FLOAT)));
		glEnableVertexAttribArray(3);


		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * indices.size(), &indices[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

	}
} Mesh;

typedef struct TextureHelper {
	static  GLuint load2DTexture(const char* filename, GLint internalFormat = GL_RGBA8,
		GLenum picFormat = GL_RGBA)
	{

		GLuint textureId = 0;
		glGenTextures(1, &textureId);
		glBindTexture(GL_TEXTURE_2D, textureId);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);

		TextureData imageTexData;
		GLubyte *imageData = NULL;
		int picWidth, picHeight;
		int channels = 0;

		imageTexData = loadPNG(filename);
		imageData = imageTexData.data;

		if (imageData == NULL)
		{
			std::cerr << "Error::Texture could not load texture file:" << filename << std::endl;
			return 0;
		}
		//printf("Loaded image with width[%d], height[%d]\n", imageTexData.width, imageTexData.height);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, imageTexData.width, imageTexData.height,
			0, picFormat, GL_UNSIGNED_BYTE, imageData);


		glGenerateMipmap(GL_TEXTURE_2D);

		glBindTexture(GL_TEXTURE_2D, 0);
		return textureId;
	}
#define FOURCC_DXT1 0x31545844 
#define FOURCC_DXT3 0x33545844 
#define FOURCC_DXT5 0x35545844 

	static GLuint loadDDS(const char * filename) {


		/* try to open the file */
		std::ifstream file(filename, std::ios::in | std::ios::binary);
		if (!file) {
			std::cout << "Error::loadDDs, could not open:"
				<< filename << "for read." << std::endl;
			return 0;
		}

		/* verify the type of file */
		char filecode[4];
		file.read(filecode, 4);
		if (strncmp(filecode, "DDS ", 4) != 0) {
			std::cout << "Error::loadDDs, format is not dds :"
				<< filename << std::endl;
			file.close();
			return 0;
		}

		/* get the surface desc */
		char header[124];
		file.read(header, 124);

		unsigned int height = *(unsigned int*)&(header[8]);
		unsigned int width = *(unsigned int*)&(header[12]);
		unsigned int linearSize = *(unsigned int*)&(header[16]);
		unsigned int mipMapCount = *(unsigned int*)&(header[24]);
		unsigned int fourCC = *(unsigned int*)&(header[80]);


		char * buffer = NULL;
		unsigned int bufsize;
		/* how big is it going to be including all mipmaps? */
		bufsize = mipMapCount > 1 ? linearSize * 2 : linearSize;
		buffer = new char[bufsize];
		file.read(buffer, bufsize);
		/* close the file pointer */
		file.close();

		unsigned int components = (fourCC == FOURCC_DXT1) ? 3 : 4;
		unsigned int format;
		switch (fourCC)
		{
		case FOURCC_DXT1:
			format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
			break;
		case FOURCC_DXT3:
			format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
			break;
		case FOURCC_DXT5:
			format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			break;
		default:
			delete[] buffer;
			return 0;
		}

		// Create one OpenGL texture
		GLuint textureID;
		glGenTextures(1, &textureID);

		// "Bind" the newly created texture : all future texture functions will modify this texture
		glBindTexture(GL_TEXTURE_2D, textureID);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		unsigned int blockSize = (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;
		unsigned int offset = 0;

		/* load the mipmaps */
		for (unsigned int level = 0; level < mipMapCount && (width || height); ++level)
		{
			unsigned int size = ((width + 3) / 4)*((height + 3) / 4)*blockSize;
			glCompressedTexImage2D(GL_TEXTURE_2D, level, format, width, height,
				0, size, buffer + offset);

			offset += size;
			width /= 2;
			height /= 2;

			// Deal with Non-Power-Of-Two textures. This code is not included in the webpage to reduce clutter.
			if (width < 1) width = 1;
			if (height < 1) height = 1;

		}

		delete[] buffer;

		return textureID;
	}

	static TextureData loadPNG(const char* const pngFilepath)
	{
		TextureData texture;
		int components;

		// load the texture with stb image, force RGBA (4 components required)
		stbi_uc *data = stbi_load(pngFilepath, &texture.width, &texture.height, &components, 4);

		// is the image successfully loaded?
		if (data != NULL)
		{
			// copy the raw data
			size_t dataSize = texture.width * texture.height * 4 * sizeof(unsigned char);
			texture.data = new unsigned char[dataSize];
			memcpy(texture.data, data, dataSize);

			// mirror the image vertically to comply with OpenGL convention
			for (size_t i = 0; i < texture.width; ++i)
			{
				for (size_t j = 0; j < texture.height / 2; ++j)
				{
					for (size_t k = 0; k < 4; ++k)
					{
						size_t coord1 = (j * texture.width + i) * 4 + k;
						size_t coord2 = ((texture.height - j - 1) * texture.width + i) * 4 + k;
						std::swap(texture.data[coord1], texture.data[coord2]);
					}
				}
			}

			// release the loaded image
			stbi_image_free(data);
		}

		return texture;
	}
} TextureHelper;

typedef struct Model {
	std::vector<Mesh> meshes;
	std::string modelFileDir;
	typedef std::map<std::string, Texture> LoadedTextMapType;
	LoadedTextMapType loadedTextureMap;

	bool processNode(const aiNode* node, const aiScene* sceneObjPtr)
	{
		if (!node || !sceneObjPtr)
		{
			return false;
		}

		for (size_t i = 0; i < node->mNumMeshes; ++i)
		{

			const aiMesh* meshPtr = sceneObjPtr->mMeshes[node->mMeshes[i]];
			if (meshPtr)
			{
				Mesh meshObj;
				if (this->processMesh(meshPtr, sceneObjPtr, meshObj))
				{
					this->meshes.push_back(meshObj);
				}
			}
		}

		for (size_t i = 0; i < node->mNumChildren; ++i)
		{
			this->processNode(node->mChildren[i], sceneObjPtr);
		}
		return true;
	}
	bool processMesh(const aiMesh* meshPtr, const aiScene* sceneObjPtr, Mesh& meshObj)
	{
		if (!meshPtr || !sceneObjPtr)
		{
			return false;
		}
		std::vector<Vertex> vertData;
		std::vector<Texture> textures;
		std::vector<GLuint> indices;

		for (size_t i = 0; i < meshPtr->mNumVertices; ++i)
		{
			Vertex vertex;

			if (meshPtr->HasPositions())
			{
				//glm::mat4 r = rotate(mat4(), radians(rotateAngle), vec3(1.0, 0.0, 0.0));
				vertex.position.x = meshPtr->mVertices[i].x;
				vertex.position.y = meshPtr->mVertices[i].y;
				vertex.position.z = meshPtr->mVertices[i].z;

				//vertex.position = vec3(r * vec4(vertex.position, 1.0));
			}

			if (meshPtr->HasTextureCoords(0))
			{
				vertex.texCoords.x = meshPtr->mTextureCoords[0][i].x;
				vertex.texCoords.y = meshPtr->mTextureCoords[0][i].y;
			}
			else
			{
				vertex.texCoords = glm::vec2(0.0f, 0.0f);
			}

			if (meshPtr->HasNormals())
			{
				vertex.normal.x = meshPtr->mNormals[i].x;
				vertex.normal.y = meshPtr->mNormals[i].y;
				vertex.normal.z = meshPtr->mNormals[i].z;
			}
			if (meshPtr->HasTangentsAndBitangents())
			{
				vertex.tangent.x = meshPtr->mTangents[i].x;
				vertex.tangent.y = meshPtr->mTangents[i].y;
				vertex.tangent.z = meshPtr->mTangents[i].z;
			}
			vertData.push_back(vertex);
		}

		for (size_t i = 0; i < meshPtr->mNumFaces; ++i)
		{
			aiFace face = meshPtr->mFaces[i];
			if (face.mNumIndices != 3)
			{
				std::cerr << "Error:Model::processMesh, mesh not transformed to triangle mesh." << std::endl;
				return false;
			}
			for (size_t j = 0; j < face.mNumIndices; ++j)
			{
				indices.push_back(face.mIndices[j]);
			}
		}

		if (meshPtr->mMaterialIndex >= 0)
		{
			const aiMaterial* materialPtr = sceneObjPtr->mMaterials[meshPtr->mMaterialIndex];

			std::vector<Texture> diffuseTexture;
			this->processMaterial(materialPtr, sceneObjPtr, aiTextureType_DIFFUSE, diffuseTexture);
			textures.insert(textures.end(), diffuseTexture.begin(), diffuseTexture.end());

			std::vector<Texture> specularTexture;
			this->processMaterial(materialPtr, sceneObjPtr, aiTextureType_SPECULAR, specularTexture);
			textures.insert(textures.end(), specularTexture.begin(), specularTexture.end());
		}
		meshObj.setData(vertData, textures, indices);
		return true;
	}
	/*
	* Get mesh of texture
	*/
	bool processMaterial(const aiMaterial* matPtr, const aiScene* sceneObjPtr,
		const aiTextureType textureType, std::vector<Texture>& textures)
	{
		textures.clear();

		if (!matPtr
			|| !sceneObjPtr)
		{
			return false;
		}
		if (matPtr->GetTextureCount(textureType) <= 0)
		{
			return true;
		}
		for (size_t i = 0; i < matPtr->GetTextureCount(textureType); ++i)
		{
			Texture text;
			aiString textPath;
			aiReturn retStatus = matPtr->GetTexture(textureType, i, &textPath);
			if (retStatus != aiReturn_SUCCESS
				|| textPath.length == 0)
			{
				std::cerr << "Warning, load texture type=" << textureType
					<< "index= " << i << " failed with return value= "
					<< retStatus << std::endl;
				continue;
			}
			//cout << textPath.C_Str() << "\n";
			std::string absolutePath = this->modelFileDir + "/" + textPath.C_Str();
			LoadedTextMapType::const_iterator it = this->loadedTextureMap.find(absolutePath);
			if (it == this->loadedTextureMap.end())
			{
				GLuint textId = TextureHelper::load2DTexture(absolutePath.c_str());
				text.id = textId;
				text.path = absolutePath;
				text.type = textureType;
				textures.push_back(text);
				loadedTextureMap[absolutePath] = text;
			}
			else
			{
				textures.push_back(it->second);
			}
		}
		return true;
	}

	void Draw(GLuint program) const
	{
		for (std::vector<Mesh>::const_iterator it = this->meshes.begin(); this->meshes.end() != it; ++it)
		{
			it->Draw(program);
		}
	}
	bool loadModel(const std::string& filePath)
	{
		Assimp::Importer importer;
		if (filePath.empty())
		{
			std::cerr << "Error:Model::loadModel, empty model file path." << std::endl;
			return false;
		}

		const aiScene* sceneObjPtr = importer.ReadFile(
			filePath.c_str(),
			aiProcess_OptimizeGraph |
			aiProcess_OptimizeMeshes |
			aiProcess_ImproveCacheLocality |
			aiProcess_SplitLargeMeshes |
			aiProcess_Triangulate |
			aiProcess_CalcTangentSpace |
			aiProcess_JoinIdenticalVertices |
			aiProcess_SortByPType
		);
		if (!sceneObjPtr
			|| sceneObjPtr->mFlags == AI_SCENE_FLAGS_INCOMPLETE
			|| !sceneObjPtr->mRootNode)
		{
			std::cerr << "Error:Model::loadModel, description: "
				<< importer.GetErrorString() << std::endl;
			return false;
		}
		this->modelFileDir = filePath.substr(0, filePath.find_last_of('/'));
		if (!this->processNode(sceneObjPtr->mRootNode, sceneObjPtr))
		{
			std::cerr << "Error:Model::loadModel, process node failed." << std::endl;
			return false;
		}
		return true;
	}
	~Model()
	{
		for (std::vector<Mesh>::const_iterator it = this->meshes.begin(); this->meshes.end() != it; ++it)
		{
			it->final();
		}
	}
} Model;

Model objModel;
Model objhuman;
// ---------------------------------------------------- Loader ---------------------------------------------------->

void My_LoadModels()
{
	tinyobj::attrib_t attrib;
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	string warn;
	string err;
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "nanosuit.obj");
	if (!warn.empty()) {
		cout << warn << endl;
	}
	if (!err.empty()) {
		cout << err << endl;
	}
	if (!ret) {
		exit(1);
	}

	vector<float> vertices, texcoords, normals;  // if OBJ preserves vertex order, you can use element array buffer for memory efficiency
	for (int s = 0; s < shapes.size(); ++s) {  // for 'ladybug.obj', there is only one object
		int index_offset = 0;
		for (int f = 0; f < shapes[s].mesh.num_face_vertices.size(); ++f) {
			int fv = shapes[s].mesh.num_face_vertices[f];
			for (int v = 0; v < fv; ++v) {
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				vertices.push_back(attrib.vertices[3 * idx.vertex_index + 0]);
				vertices.push_back(attrib.vertices[3 * idx.vertex_index + 1]);
				vertices.push_back(attrib.vertices[3 * idx.vertex_index + 2]);
				texcoords.push_back(attrib.texcoords[2 * idx.texcoord_index + 0]);
				texcoords.push_back(attrib.texcoords[2 * idx.texcoord_index + 1]);
				normals.push_back(attrib.normals[3 * idx.normal_index + 0]);
				normals.push_back(attrib.normals[3 * idx.normal_index + 1]);
				normals.push_back(attrib.normals[3 * idx.normal_index + 2]);
			}
			index_offset += fv;
			m_shape.drawCount += fv;
		}
	}

	glGenVertexArrays(1, &m_shape.vao);
	glBindVertexArray(m_shape.vao);

	glGenBuffers(1, &m_shape.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, m_shape.vbo);

	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float) + texcoords.size() * sizeof(float) + normals.size() * sizeof(float), NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
	glBufferSubData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), texcoords.size() * sizeof(float), texcoords.data());
	glBufferSubData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float) + texcoords.size() * sizeof(float), normals.size() * sizeof(float), normals.data());

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(vertices.size() * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(vertices.size() * sizeof(float) + texcoords.size() * sizeof(float)));
	glEnableVertexAttribArray(2);

	shapes.clear();
	shapes.shrink_to_fit();
	materials.clear();
	materials.shrink_to_fit();
	vertices.clear();
	vertices.shrink_to_fit();
	texcoords.clear();
	texcoords.shrink_to_fit();
	normals.clear();
	normals.shrink_to_fit();

	cout << "Load " << m_shape.drawCount << " vertices" << endl;
}

GLuint lightSpaceMatrixLocation;
GLuint modelLocation;

GLuint depthMapFBO;
GLuint depthMap;
GLuint depthMapLocation;

void shadow_Init()
{
	depth_program = glCreateProgram();

	GLuint depth_vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint depth_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	char** depth_vertexShaderSource = loadShaderSource("depth.vs.glsl");
	char** depth_fragmentShaderSource = loadShaderSource("depth.fs.glsl");

	glShaderSource(depth_vertexShader, 1, depth_vertexShaderSource, NULL);
	glShaderSource(depth_fragmentShader, 1, depth_fragmentShaderSource, NULL);

	freeShaderSource(depth_vertexShaderSource);
	freeShaderSource(depth_fragmentShaderSource);

	glCompileShader(depth_vertexShader);
	glCompileShader(depth_fragmentShader);

	shaderLog(depth_vertexShader);
	shaderLog(depth_fragmentShader);

	glAttachShader(depth_program, depth_vertexShader);
	glAttachShader(depth_program, depth_fragmentShader);
	glLinkProgram(depth_program);
	//glUseProgram(depth_program);

	lightSpaceMatrixLocation = glGetUniformLocation(depth_program, "lightSpaceMatrix");
	modelLocation = glGetUniformLocation(depth_program, "model");

	// Gen FBO
	glGenFramebuffers(1, &depthMapFBO);
	// Gen texture
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
		SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// Attach to FBO
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


/*-----------------------------------------------Toon Shading and Model Part-----------------------------------------------*/

void toon_Init() {
	uniforms.toon.mv_matrix = glGetUniformLocation(model_program, "mv_matrix");
	uniforms.toon.proj_matrix = glGetUniformLocation(model_program, "proj_matrix");

	static const GLubyte toon_tex_data[] =
	{
		//0x44, 0x00, 0x00, 0x00,
		//0x88, 0x00, 0x00, 0x00,
		//0xCC, 0x00, 0x00, 0x00,
		//0xFF, 0x00, 0x00, 0x00
		0xC5, 0xB3, 0x58, 0x00,
		0xCF, 0xB5, 0x3B, 0x00,
		0xD4, 0xAF, 0x37, 0x00,
		0xFF, 0xDF, 0x00, 0x00
	};

	glGenTextures(1, &tex_toon);
	glBindTexture(GL_TEXTURE_1D, tex_toon);
	glTexImage1D(GL_TEXTURE_1D, 0,
		GL_RGBA, sizeof(toon_tex_data) / 4, 0,
		GL_RGBA, GL_UNSIGNED_BYTE,
		toon_tex_data);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
}

void toon_Render() {


	float currentTime = glutGet(GLUT_ELAPSED_TIME) * 0.001f;

	glUseProgram(model_program);


	model_matrix = translate(mat4(1.0), vec3());
	model_matrix = translate(model_matrix, vec3(0.0f, 0.0f, 0.0f));
	model_matrix = scale(model_matrix, vec3(5.0f, 5.0f, 5.0f));

	glBindVertexArray(m_shape.vao);

	glUniformMatrix4fv(uniforms.toon.mv_matrix, 1, GL_FALSE, &(view * model_matrix)[0][0]);
	glUniformMatrix4fv(uniforms.toon.proj_matrix, 1, GL_FALSE, &projection[0][0]);

	glBindTexture(GL_TEXTURE_1D, tex_toon);

	glDrawArrays(GL_TRIANGLES, 0, m_shape.drawCount);
}

void model_Init() {
	model_program = glCreateProgram();

	GLuint model_vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint model_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	char** model_vertexShaderSource = loadShaderSource("toon.vs.glsl");
	char** model_fragmentShaderSource = loadShaderSource("toon.fs.glsl");

	glShaderSource(model_vertexShader, 1, model_vertexShaderSource, NULL);
	glShaderSource(model_fragmentShader, 1, model_fragmentShaderSource, NULL);

	freeShaderSource(model_vertexShaderSource);
	freeShaderSource(model_fragmentShaderSource);

	glCompileShader(model_vertexShader);
	glCompileShader(model_fragmentShader);

	shaderLog(model_vertexShader);
	shaderLog(model_fragmentShader);

	glAttachShader(model_program, model_vertexShader);
	glAttachShader(model_program, model_fragmentShader);
	glLinkProgram(model_program);
	glUseProgram(model_program);

	toon_Init();
	//shadow_Init();
	//SSAO_Init();
	My_LoadModels();
}

/*-----------------------------------------------Toon Shading and Model Part-----------------------------------------------*/


/*-----------------------------------------------Skybox part-----------------------------------------------*/
vector<string> faces = { "cubemaps\\face-r.jpg", "cubemaps\\face-l.jpg", "cubemaps\\face-t.jpg", "cubemaps\\face-d.jpg", "cubemaps\\face-b.jpg", "cubemaps\\face-f.jpg" };

GLuint skybox;
GLuint cubemapTexture;
GLuint skyboxVAO;

void skyboxInitFunction()
{
	skybox = glCreateProgram();

	GLuint skybox_vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint skybox_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	char** skybox_vertexShaderSource = loadShaderSource("skybox.vs.glsl");
	char** skybox_fragmentShaderSource = loadShaderSource("skybox.fs.glsl");

	glShaderSource(skybox_vertexShader, 1, skybox_vertexShaderSource, NULL);
	glShaderSource(skybox_fragmentShader, 1, skybox_fragmentShaderSource, NULL);

	freeShaderSource(skybox_vertexShaderSource);
	freeShaderSource(skybox_fragmentShaderSource);

	glCompileShader(skybox_vertexShader);
	glCompileShader(skybox_fragmentShader);

	shaderLog(skybox_vertexShader);
	shaderLog(skybox_fragmentShader);

	glAttachShader(skybox, skybox_vertexShader);
	glAttachShader(skybox, skybox_fragmentShader);
	glLinkProgram(skybox);
	glUseProgram(skybox);

	//load skybox Texture
	glGenTextures(1, &cubemapTexture);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	for (int i = 0; i < 6; i++)
	{
		texture_data envmap_data = loadImg(faces[i].c_str());
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, envmap_data.width, envmap_data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, envmap_data.data);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	glGenVertexArrays(1, &skyboxVAO);
}

void SkyboxRendering()
{
	static const GLfloat gray[] = { 0.2f, 0.2f, 0.2f, 1.0f };
	static const GLfloat ones[] = { 1.0f };

	mat4 mv_matrix = view;
	//cout << mv_matrix[0][0] << endl << mv_matrix[1][0] << endl;

	mat4 inv_vp_matrix = inverse(projection * view);
	//cout << inv_vp_matrix[0][0] << endl << inv_vp_matrix[1][0] << endl;

	glClearBufferfv(GL_COLOR, 0, gray);
	glClearBufferfv(GL_DEPTH, 0, ones);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	glUniform1i(glGetUniformLocation(skybox, "tex_cubemap"), 0);

	glUseProgram(skybox);
	glBindVertexArray(skyboxVAO);
	glUniformMatrix4fv(glGetUniformLocation(skybox, "inv_vp_matrix"), 1, GL_FALSE, &inv_vp_matrix[0][0]);
	glUniform3fv(glGetUniformLocation(skybox, "eye"), 1, &cameraPos[0]);

	glDisable(GL_DEPTH_TEST);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glEnable(GL_DEPTH_TEST);
}
/*-----------------------------------------------Skybox part-----------------------------------------------*/

/*----------------------------------------------- Terrain part (teacher) -----------------------------------------------*/
GLuint terrain_program;
GLuint tex_displacement;
GLuint tex_color;
GLuint terrain_vao;

float dmap_depth;
bool enable_displacement;
bool wireframe;
bool enable_fog;

void Terrain_init()
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	terrain_program = glCreateProgram();

	GLuint terrain_vs = glCreateShader(GL_VERTEX_SHADER);
	GLuint terrain_tcs = glCreateShader(GL_TESS_CONTROL_SHADER);
	GLuint terrain_tes = glCreateShader(GL_TESS_EVALUATION_SHADER);
	GLuint terrain_fs = glCreateShader(GL_FRAGMENT_SHADER);

	char** terrain_vs_source = loadShaderSource("terrain_lp.vs.glsl");
	char** terrain_tcs_source = loadShaderSource("terrain_lp.tcs");
	char** terrain_tes_source = loadShaderSource("terrain_lp.tes");
	char** terrain_fs_source = loadShaderSource("terrain_lp.fs.glsl");

	glShaderSource(terrain_vs, 1, terrain_vs_source, NULL);
	glShaderSource(terrain_tcs, 1, terrain_tcs_source, NULL);
	glShaderSource(terrain_tes, 1, terrain_tes_source, NULL);
	glShaderSource(terrain_fs, 1, terrain_fs_source, NULL);

	freeShaderSource(terrain_vs_source);
	freeShaderSource(terrain_tcs_source);
	freeShaderSource(terrain_tes_source);
	freeShaderSource(terrain_fs_source);

	glCompileShader(terrain_vs);
	glCompileShader(terrain_tcs);
	glCompileShader(terrain_tes);
	glCompileShader(terrain_fs);

	shaderLog(terrain_vs);
	shaderLog(terrain_tcs);
	shaderLog(terrain_tes);
	shaderLog(terrain_fs);

	glAttachShader(terrain_program, terrain_vs);
	glAttachShader(terrain_program, terrain_tcs);
	glAttachShader(terrain_program, terrain_tes);
	glAttachShader(terrain_program, terrain_fs);

	glLinkProgram(terrain_program);
	glUseProgram(terrain_program);

	glGenVertexArrays(1, &terrain_vao);
	glBindVertexArray(terrain_vao);

	dmap_depth = 6.0f;

	texture_data tdata = loadImg("terragen.png");
	tdata.data == NULL ? printf("load terrain height image fail\n") : printf("load terrain color height sucessful\n");
	glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &tex_displacement);
	glBindTexture(GL_TEXTURE_2D, tex_displacement);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tdata.width, tdata.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tdata.data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glActiveTexture(GL_TEXTURE1);
	texture_data tdata2 = loadImg("terragen_color.png");
	tdata2.data == NULL ? printf("load terrain color image fail\n") : printf("load terrain color image sucessful\n");
	glGenTextures(1, &tex_color);
	glBindTexture(GL_TEXTURE_2D, tex_color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tdata2.width, tdata2.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tdata2.data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glPatchParameteri(GL_PATCH_VERTICES, 4);

	//glEnable(GL_CULL_FACE);

	enable_displacement = true;
	wireframe = false;
	enable_fog = false;
}

void Terrain_rendering()
{
	mat4 model_matrix = mat4(1.0f);
	glm::vec3 scale = glm::vec3(30, 10, 30);
	model_matrix = glm::scale(model_matrix, scale);

	glUseProgram(terrain_program);
	glBindVertexArray(terrain_vao);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex_displacement);
	glUniform1i(glGetUniformLocation(terrain_program, "tex_displacement"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, tex_color);
	glUniform1i(glGetUniformLocation(terrain_program, "tex_color"), 1);

	glUniformMatrix4fv(glGetUniformLocation(terrain_program, "mv_matrix"), 1, GL_FALSE, value_ptr(view * model_matrix));
	glUniformMatrix4fv(glGetUniformLocation(terrain_program, "proj_matrix"), 1, GL_FALSE, value_ptr(projection));
	glUniformMatrix4fv(glGetUniformLocation(terrain_program, "mvp_matrix"), 1, GL_FALSE, value_ptr(projection * view));
	glUniform1f(glGetUniformLocation(terrain_program, "dmap_depth"), enable_displacement ? dmap_depth : 0.0f);
	glUniform1i(glGetUniformLocation(terrain_program, "enable_fog"), enable_fog ? 1 : 0);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	if (wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glDrawArraysInstanced(GL_PATCHES, 0, 4, 64 * 64);
}
/*----------------------------------------------- Terrain part (teacher) -----------------------------------------------*/



glm::mat4 lightViewing = glm::lookAt(glm::vec3(100.0f, 2000.0f, -1000.0f), glm::vec3(1000.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
glm::mat4 lightProj = glm::ortho(-3500.0f, 3500.0f, -3500.0f, 3500.0f, 800.0f, 4000.0f);
glm::mat4 lightSpace = lightProj * lightViewing;


GLuint lightEffect_switch;
GLuint normalMap_switch;
GLuint shadowMap_switch;

void initScene() {
	// Create Shader Program
	scene_program = glCreateProgram();

	// Create customize shader by tell openGL specify shader type 
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	// Load shader file
	char** vertexShaderSource = loadShaderSource("vertex.vs.glsl");
	char** fragmentShaderSource = loadShaderSource("fragment.fs.glsl");

	// Assign content of these shader files to those shaders we created before
	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
	glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);

	// Free the shader file string(won't be used any more)
	freeShaderSource(vertexShaderSource);
	freeShaderSource(fragmentShaderSource);

	// Compile these shaders
	glCompileShader(vertexShader);
	glCompileShader(fragmentShader);

	// Logging
	shaderLog(vertexShader);
	shaderLog(fragmentShader);

	// Assign the program we created before with these shaders
	glAttachShader(scene_program, vertexShader);
	glAttachShader(scene_program, fragmentShader);
	glLinkProgram(scene_program);

	//texture_location = glGetUniformLocation(scene_program, "tex");
	um4mv = glGetUniformLocation(scene_program, "um4mv");
	um4p = glGetUniformLocation(scene_program, "um4p");
	//tex_mode = glGetUniformLocation(scene_program, "tex_mode");

	lightEffect_switch = glGetUniformLocation(scene_program, "lightEffect_switch");
	normalMap_switch = glGetUniformLocation(scene_program, "normalMap_switch");
	shadowMap_switch = glGetUniformLocation(scene_program, "shadowMap_switch");
	depthMapLocation = glGetUniformLocation(scene_program, "depthMap");

	glUseProgram(scene_program);

	//loadScene();
	objModel.loadModel("./eastern ancient casttle.obj");
	objhuman.loadModel("./nanosuit.obj");
}

void renderScene() {
	glUseProgram(scene_program);

	view = lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
	scaleOne = mat4(1.0f);
	M = scaleOne;
	ModelView = view * M;

	projection = scaleOne * projection;
	glUniformMatrix4fv(um4p, 1, GL_FALSE, value_ptr(projection));
	glUniformMatrix4fv(um4mv, 1, GL_FALSE, value_ptr(ModelView));
	glUniformMatrix4fv(glGetUniformLocation(scene_program, "lightSpaceMatrix"), 1, GL_FALSE, value_ptr(lightSpace));
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glUniform1i(depthMapLocation, 9);

	objModel.Draw(scene_program);
}

void renderModel() {
	glUseProgram(scene_program);

	model_matrix = translate(mat4(1.0), vec3());
	model_matrix = translate(model_matrix, vec3(0.0f, 0.0f, 0.0f));
	model_matrix = scale(model_matrix, vec3(5.0f, 5.0f, 5.0f));
	glUniformMatrix4fv(um4mv, 1, GL_FALSE, value_ptr(view * model_matrix));
	objhuman.Draw(scene_program);
}

//-----------------End Load Scene Function and Variables------------------------




/*-----------------------------------------------FRAMEBUFFER POSTPROCESSING------------------------------------*/
GLuint postprocessing_program;
GLuint FBO;
GLuint depthRBO;
GLuint FBODataTexture;
GLuint mainColorTexture;
GLuint vao2;
GLuint window_vertex_buffer;
GLuint filter_mode_location;
GLuint noiseTexture;
int mode = 0;
bool move_bar = true;
int magnify;

static const GLfloat window_vertex[] =
{
	//vec2 position vec2 texture_coord
	1.0f, -1.0f, 1.0f, 0.0f,
   -1.0f, -1.0f, 0.0f, 0.0f,
   -1.0f,  1.0f, 0.0f, 1.0f,
	1.0f,  1.0f, 1.0f, 1.0f
};

void init_post_framebuffer() {

	postprocessing_program = glCreateProgram();

	// Create customize shader by tell openGL specify shader type 
	GLuint vertexShader2 = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader2 = glCreateShader(GL_FRAGMENT_SHADER);

	// Load shader file
	char** vertexShaderSource2 = loadShaderSource("post_vertex.vs.glsl");
	char** fragmentShaderSource2 = loadShaderSource("post_fragment.fs.glsl");

	// Assign content of these shader files to those shaders we created before
	glShaderSource(vertexShader2, 1, vertexShaderSource2, NULL);
	glShaderSource(fragmentShader2, 1, fragmentShaderSource2, NULL);

	// Free the shader file string(won't be used any more)
	freeShaderSource(vertexShaderSource2);
	freeShaderSource(fragmentShaderSource2);

	// Compile these shaders
	glCompileShader(vertexShader2);
	glCompileShader(fragmentShader2);

	// Logging
	shaderLog(vertexShader2);
	shaderLog(fragmentShader2);

	// Assign the program we created before with these shaders
	glAttachShader(postprocessing_program, vertexShader2);
	glAttachShader(postprocessing_program, fragmentShader2);
	glLinkProgram(postprocessing_program);

	//FBO Vertex Data Settings
	glGenVertexArrays(1, &vao2);
	glBindVertexArray(vao2);

	glGenBuffers(1, &window_vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, window_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(window_vertex), window_vertex, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT) * 4, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT) * 4, (const GLvoid*)(sizeof(GL_FLOAT) * 2));

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	//Create FBO
	glGenFramebuffers(1, &FBO);

	filter_mode_location = glGetUniformLocation(postprocessing_program, "mode");

	// load noise texture
	texture_data noise_tex = loadImg("noise.jpg");
	glGenTextures(1, &noiseTexture);
	glBindTexture(GL_TEXTURE_2D, noiseTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, noise_tex.width, noise_tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, noise_tex.data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void init_post_rbo() {
	//Create Depth RBO
	glDeleteRenderbuffers(1, &depthRBO);
	glDeleteTextures(1, &FBODataTexture);
	glGenRenderbuffers(1, &depthRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, depthRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, viewport_size.width, viewport_size.height);

	// Create fboDataTexture
	// Generate a texture for FBO
	glGenTextures(1, &FBODataTexture);
	// Bind it so that we can specify the format of the textrue
	glBindTexture(GL_TEXTURE_2D, FBODataTexture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, viewport_size.width, viewport_size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	//Bind the framebuffer with first parameter "GL_DRAW_FRAMEBUFFER" 
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	//Set depthrbo to current fbo
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRBO);

	//Set buffertexture to current fbo
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FBODataTexture, 0);
}

void post_render() {

	// Return to the default framebuffer
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mainColorTexture);

	glBindVertexArray(vao2);
	glUseProgram(postprocessing_program);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glUniform1i(filter_mode_location, mode);
	glUniform1f(glGetUniformLocation(postprocessing_program, "width"), viewport_size.width);
	glUniform1f(glGetUniformLocation(postprocessing_program, "height"), viewport_size.height);
	glUniform1f(glGetUniformLocation(postprocessing_program, "magnify"), magnify);

	float t = glutGet(GLUT_ELAPSED_TIME);
	glUniform1f(glGetUniformLocation(postprocessing_program, "time"), t / 1000);
}
/*-----------------------------------------------FRAMEBUFFER POSTPROCESSING--------------------------------------*/



/*-----------------------------------------------SSAO--------------------------------------*/

int lightEffect = 1;
int fogEffect = 0;
int normalMapEffect = 1;
int shadowMapEffect = 1;
int ssaoEffect = 0;

GLuint mainFBO;

GLuint mainDepthTexture;
GLuint mainNormalTexture;
GLuint viewSpacePosTex;
GLuint ssaoProgram;

GLuint fogEffect_switch;

void ssaoSetup()
{
	ssaoProgram = glCreateProgram();
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	char** vertexShaderSource = loadShaderSource("ssao.vs.glsl");
	char** fragmentShaderSource = loadShaderSource("ssao.fs.glsl");
	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
	glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
	freeShaderSource(vertexShaderSource);
	freeShaderSource(fragmentShaderSource);
	glCompileShader(vertexShader);
	glCompileShader(fragmentShader);
	shaderLog(vertexShader);
	shaderLog(fragmentShader);
	glAttachShader(ssaoProgram, vertexShader);
	glAttachShader(ssaoProgram, fragmentShader);
	glLinkProgram(ssaoProgram);

	// Gen FBO
	glGenFramebuffers(1, &mainFBO);
}

void ssao_reshape_setup() {
	// Gen texture
	glGenTextures(1, &mainColorTexture);
	glBindTexture(GL_TEXTURE_2D, mainColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, viewport_size.width, viewport_size.height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenTextures(1, &mainDepthTexture);
	glBindTexture(GL_TEXTURE_2D, mainDepthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, viewport_size.width, viewport_size.height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenTextures(1, &mainNormalTexture);
	glBindTexture(GL_TEXTURE_2D, mainNormalTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, viewport_size.width, viewport_size.height, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenTextures(1, &viewSpacePosTex);
	glBindTexture(GL_TEXTURE_2D, viewSpacePosTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, viewport_size.width, viewport_size.height, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// Attach to FBO
	glBindFramebuffer(GL_FRAMEBUFFER, mainFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mainColorTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mainNormalTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, viewSpacePosTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mainDepthTexture, 0);
	GLuint attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 , GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, attachments);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	// Noise map
	glGenTextures(1, &noise_map);
	glBindTexture(GL_TEXTURE_2D, noise_map);
	vec3 noiseData[16];
	for (int i = 0; i < 16; ++i)
	{
		noiseData[i] = normalize(vec3(
			rand() / (float)RAND_MAX, // 0.0 ~ 1.0
			rand() / (float)RAND_MAX, // 0.0 ~ 1.0
			0.0f
		));
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 4, 4, 0, GL_RGB, GL_FLOAT, &noiseData[0][0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// Kernal UBO
	glGenBuffers(1, &kernal_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, kernal_ubo);
	const int numKernels = 32;
	vec4 kernals[numKernels];
	srand(time(NULL));
	for (int i = 0; i < numKernels; ++i)
	{
		float scale = i / numKernels;
		scale = 0.1f + 0.9f * scale * scale;
		kernals[i] = vec4(normalize(vec3(
			rand() / (float)RAND_MAX * 2.0f - 1.0f,
			rand() / (float)RAND_MAX * 2.0f - 1.0f,
			rand() / (float)RAND_MAX * 0.85f + 0.15f)) * scale,
			0.0f
		);
	}
	glBufferData(GL_UNIFORM_BUFFER, numKernels * sizeof(vec4), &kernals[0][0], GL_STATIC_DRAW);

	fogEffect_switch = glGetUniformLocation(ssaoProgram, "fogEffect_switch");
}

void ssao_render() {
	//glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(ssaoProgram);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mainColorTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, mainNormalTexture);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, mainDepthTexture);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, noise_map);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, viewSpacePosTex);
	glUniformMatrix4fv(glGetUniformLocation(ssaoProgram, "proj"), 1, GL_FALSE, &projection[0][0]);
	glUniform2f(glGetUniformLocation(ssaoProgram, "noise_scale"), viewport_size.width / 4.0f, viewport_size.height / 4.0f);
	glUniform1i(glGetUniformLocation(ssaoProgram, "color_map"), 0);
	glUniform1i(glGetUniformLocation(ssaoProgram, "normal_map"), 1);
	glUniform1i(glGetUniformLocation(ssaoProgram, "depth_map"), 2);
	glUniform1i(glGetUniformLocation(ssaoProgram, "noise_map"), 3);
	glUniform1i(glGetUniformLocation(ssaoProgram, "viewSpacePosTex"), 4);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, kernal_ubo);
	glUniform1i(glGetUniformLocation(ssaoProgram, "enabled"), ssaoEffect);
	glUniform1i(fogEffect_switch, fogEffect);
	//glBindVertexArray(ssao_vao);
	glDisable(GL_DEPTH_TEST);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glEnable(GL_DEPTH_TEST);
}

/*-----------------------------------------------SSAO--------------------------------------*/


void My_Init()
{
	glClearColor(0.0f, 0.6f, 0.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glDepthFunc(GL_LEQUAL);

	initScene();
	skyboxInitFunction();
	model_Init();
	shadow_Init();

	ssaoSetup();

	init_post_framebuffer();

	Terrain_init();
}

void My_Display()
{
	//======================= Begin Shadow Depth Pas=================================
	glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(depth_program);

	glUniformMatrix4fv(lightSpaceMatrixLocation, 1, GL_FALSE, value_ptr(lightSpace));
	glUniformMatrix4fv(modelLocation, 1, GL_FALSE, value_ptr(mat4(1.0f)));
	glCullFace(GL_FRONT);
	objModel.Draw(depth_program);

	glUseProgram(depth_program);

	model_matrix = translate(mat4(1.0), vec3());
	model_matrix = translate(model_matrix, vec3(0.0f, 0.0f, 0.0f));
	model_matrix = scale(model_matrix, vec3(5.0f, 5.0f, 5.0f));
	glUniformMatrix4fv(modelLocation, 1, GL_FALSE, value_ptr(model_matrix));
	objhuman.Draw(depth_program);

	glCullFace(GL_BACK);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//=======================End Shadow Depth Pas=================================

	glViewport(0, 0, viewport_size.width, viewport_size.height);

	// Bind Post Processing FBO
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// Which render buffer attachment is written
	//glDrawBuffer(GL_COLOR_ATTACHMENT0);

	SkyboxRendering();

	if (texture_mode == 0) {
		renderModel();
	}
	else if (texture_mode == 1) {
		toon_Render();
	}

	renderScene();

	Terrain_rendering();

	glBindFramebuffer(GL_FRAMEBUFFER, mainFBO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	//glDrawBuffer(GL_COLOR_ATTACHMENT0);

	SkyboxRendering();

	if (texture_mode == 0) {
		renderModel();
	}
	else if (texture_mode == 1) {
		toon_Render();
	}

	renderScene();

	ssao_render();

	Terrain_rendering();
	
	post_render();

    glutSwapBuffers();
}

void My_Reshape(int width, int height)
{
	viewport_size.width = width;
	viewport_size.height = height;
	glViewport(0, 0, width, height);
	viewportAspect = (float)width / (float)height;
	projection = perspective(radians(45.0f), viewportAspect, 0.1f, 3000.f);

	ssao_reshape_setup();

	init_post_rbo();
}

void My_Timer(int val)
{
	glutPostRedisplay();
	glutTimerFunc(timer_speed, My_Timer, val);
}

void My_Mouse(int button, int state, int x, int y)
{
	if (state == GLUT_DOWN)
	{
		printf("Mouse %d is pressed at (%d, %d)\n", button, x, y);
		if (mode == 2) magnify = 1;
		glUniform1f(glGetUniformLocation(postprocessing_program, "mouse_x"), (float)x);
		glUniform1f(glGetUniformLocation(postprocessing_program, "mouse_y"), (float)y);
		if (button == GLUT_LEFT_BUTTON)
		{
			//glUniform1f(glGetUniformLocation(program2, "bar_position"), (float)x / (float)window_width);
		}
	}
	else if (state == GLUT_UP)
	{
		if (mode == 2) magnify = 0;
		printf("Mouse %d is released at (%d, %d)\n", button, x, y);
	}
}

void onMouseMotion(int x, int y) {
	glUniform1f(glGetUniformLocation(postprocessing_program, "bar_position"), (float)x / (float)viewport_size.width);
}

void onMouseHover(int x, int y) {

	if (firstMouse) {
		lastX = x;
		lastY = y;
		firstMouse = false;
	}

	float xoffset = (x - lastX)* 0.5f;
	float yoffset = (lastY - y)* 0.5f;

	lastX = x;
	lastY = y;

	yaws = yaws + xoffset;
	pitchs = pitchs + yoffset;

	// to limit user not to see upside down scene
	if (pitchs > 89.0f) pitchs = 89.0f;
	if (pitchs < -89.0f) pitchs = -89.0f;

	vec3 direction;
	direction.x = cos(radians(yaws)) * cos(radians(pitchs));
	direction.y = sin(radians(pitchs));
	direction.z = sin(radians(yaws)) * cos(radians(pitchs));
	cameraFront = normalize(direction);
}

void My_Keyboard(unsigned char key, int x, int y)
{
	printf("Key %c is pressed at (%d, %d)\n", key, x, y);
	if (key == 'w' || key == 'W') cameraPos += cameraSpeed * cameraFront;
	if (key == 'a' || key == 'A') cameraPos -= normalize(cross(cameraFront, cameraUp)) * cameraSpeed;
	if (key == 's' || key == 'S') cameraPos -= cameraSpeed * cameraFront;
	if (key == 'd' || key == 'D') cameraPos += normalize(cross(cameraFront, cameraUp)) * cameraSpeed;
	if (key == 'z' || key == 'Z') cameraPos.y += 5.0f;
	if (key == 'x' || key == 'X') cameraPos.y -= 5.0f;

	if (key == 'r' || key == 'R') {
		texture_mode += 1;
		if (texture_mode == 3) {
			texture_mode = 0;
		}
	}
}

void My_SpecialKeys(int key, int x, int y)
{
	switch(key)
	{
	case GLUT_KEY_F1:
		printf("F1 is pressed at (%d, %d)\n", x, y);
		break;
	case GLUT_KEY_PAGE_UP:
		printf("Page up is pressed at (%d, %d)\n", x, y);
		break;
	case GLUT_KEY_LEFT:
		printf("Left arrow is pressed at (%d, %d)\n", x, y);
		break;
	default:
		printf("Other special key is pressed at (%d, %d)\n", x, y);
		break;
	}
}

void My_Menu(int id)
{
	switch (id)
	{
	case MENU_TIMER_START:
		if (!timer_enabled)
		{
			timer_enabled = true;
			glutTimerFunc(timer_speed, My_Timer, 0);
		}
		break;
	case MENU_TIMER_STOP:
		timer_enabled = false;
		break;
	case MENU_EXIT:
		exit(0);
		break;
	case '0':
		mode = 0;
		break;
	case '1':
		mode = 1;
		break;
	case '2':
		mode = 2;
		break;
	case '3':
		mode = 3;
		break;
	case '4':
		mode = 4;
		break;
	case '5':
		mode = 5;
		break;
	case '6':
		ssaoEffect = !ssaoEffect;
		break;
	case '7':
		fogEffect = !fogEffect;
		break;
	default:
		break;
	}
}

int main(int argc, char *argv[])
{
#ifdef __APPLE__
    // Change working directory to source code path
    chdir(__FILEPATH__("/../Assets/"));
#endif
	// Initialize GLUT and GLEW, then create a window.
	////////////////////
	glutInit(&argc, argv);
#ifdef _MSC_VER
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#else
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(1066, 600);
	glutCreateWindow("Final Project Team 8"); // You cannot use OpenGL functions before this line; The OpenGL context must be created first by glutCreateWindow()!
#ifdef _MSC_VER
	glewInit();
#endif
	dumpInfo();
	My_Init();

	// Create a menu and bind it to mouse right button.
	int menu_main = glutCreateMenu(My_Menu);
	int menu_timer = glutCreateMenu(My_Menu);

	glutSetMenu(menu_main);
	glutAddSubMenu("Timer", menu_timer);
	glutAddMenuEntry("Image Abstraction", '0');
	glutAddMenuEntry("Water Color", '1');
	glutAddMenuEntry("Magnifier", '2');
	glutAddMenuEntry("Bloom Effect", '3');
	glutAddMenuEntry("Pixelization", '4');
	glutAddMenuEntry("Sine Wave Distortion", '5');
	glutAddMenuEntry("SSAO", '6');
	glutAddMenuEntry("Fog Effect", '7');
	glutAddMenuEntry("Exit", MENU_EXIT);

	glutSetMenu(menu_timer);
	glutAddMenuEntry("Start", MENU_TIMER_START);
	glutAddMenuEntry("Stop", MENU_TIMER_STOP);

	glutSetMenu(menu_main);
	glutAttachMenu(GLUT_RIGHT_BUTTON);

	// Register GLUT callback functions.
	glutDisplayFunc(My_Display);
	glutReshapeFunc(My_Reshape);
	glutMouseFunc(My_Mouse);
	glutMotionFunc(onMouseMotion);
	glutPassiveMotionFunc(onMouseHover);
	glutKeyboardFunc(My_Keyboard);
	glutSpecialFunc(My_SpecialKeys);
	glutTimerFunc(timer_speed, My_Timer, 0);

	// Enter main event loop.
	glutMainLoop();

	return 0;
}
