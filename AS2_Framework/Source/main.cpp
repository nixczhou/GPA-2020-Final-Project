#include "../Externals/Include/Common.h"

#define MENU_TIMER_START 1
#define MENU_TIMER_STOP 2
#define MENU_EXIT 3

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
	mat4 inv_vp_matrix = inverse(projection * view);

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

void loadScene() {
	//Importer
	const aiScene *scene = aiImportFile("sponza.obj", aiProcessPreset_TargetRealtime_MaxQuality);

	//Materials
	for (unsigned int i = 0; i < scene->mNumMaterials; i++)
	{
		aiMaterial *material = scene->mMaterials[i];
		Material materials;
		aiString texturePath;
		texture_data texture;
		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == aiReturn_SUCCESS)
		{
			// load width, height and data from texturePath.C_Str();
			texture = loadImg(texturePath.C_Str());
			glGenTextures(1, &materials.diffuse_tex);
			glBindTexture(GL_TEXTURE_2D, materials.diffuse_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture.width, texture.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture.data);
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		else{
			// load some default image as default_diffuse_tex

		}
		vertex_material.push_back(materials);
	}

	//Geometry
	for (unsigned int i = 0; i < scene->mNumMeshes; i++)
	{
		aiMesh *mesh = scene->mMeshes[i];
		Shape shape;
		glGenVertexArrays(1, &shape.vao);
		glBindVertexArray(shape.vao);

		// create 3 vbos to hold data
		glGenBuffers(1, &shape.vbo_position);
		glGenBuffers(1, &shape.vbo_normal);
		glGenBuffers(1, &shape.vbo_texcoord);

		float* position = new float[mesh->mNumVertices * 3];
		float* normal = new float[mesh->mNumVertices * 3];
		float* texcoord = new float[mesh->mNumVertices * 3];
		unsigned int* index = new unsigned int[mesh->mNumFaces * 3];
		int index_po, index_nor, index_tex, index_ibo;

		index_po = index_nor = index_tex = 0;
		for (unsigned int v = 0; v < mesh->mNumVertices; v++)
		{
			for (int i = 0; i < 3; i++) {
				*(position + index_po++) = mesh->mVertices[v][i];
				*(normal + index_nor++) = mesh->mNormals[v][i];
			}

			for (int i = 0; i < 2; i++)
				*(texcoord + index_tex++) = mesh->mTextureCoords[0][v][i];
		}
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_position);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), position, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_texcoord);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 2 * sizeof(float), texcoord, GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);

		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_normal);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), normal, GL_STATIC_DRAW);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		// create 1 ibo to hold data
		glGenBuffers(1, &shape.ibo);
		index_ibo = 0;
		for (unsigned int f = 0; f < mesh->mNumFaces; f++)
		{
			for (int i = 0; i < 3; i++)
				index[index_ibo++] = mesh->mFaces[f].mIndices[i];
		}

		glBindBuffer(GL_ARRAY_BUFFER, shape.ibo);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumFaces * 3 * sizeof(float), index, GL_STATIC_DRAW);

		shape.materialID = mesh->mMaterialIndex;
		shape.drawCount = mesh->mNumFaces * 3;
		vertex_shape.push_back(shape);
	}

	aiReleaseImport(scene);

}

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

	texture_location = glGetUniformLocation(scene_program, "tex");
	um4mv = glGetUniformLocation(scene_program, "um4mv");
	um4p = glGetUniformLocation(scene_program, "um4p");
	tex_mode = glGetUniformLocation(scene_program, "tex_mode");

	loadScene();
}

void renderScene() {
	glUseProgram(scene_program);

	// Clear the framebuffer with white
	static const GLfloat white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glClearBufferfv(GL_COLOR, 0, white);

	// Adjust Camera Parameters
	view = lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

	//To check camera parameters
	/*
	cout << cameraPos.x << " " << cameraPos.y << " " << cameraPos.z << endl;
	cout << cameraFront.x << " " << cameraFront.y << " " << cameraFront.z << endl;
	*/
	scaleOne = mat4(1.0f);
	M = scaleOne;
	ModelView = view * M;

	projection = scaleOne * projection;

	for (int i = 0; i < vertex_shape.size(); i++)
	{
		glBindVertexArray(vertex_shape[i].vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertex_shape[i].ibo);

		glUniformMatrix4fv(um4mv, 1, GL_FALSE, value_ptr(ModelView));
		glUniformMatrix4fv(um4p, 1, GL_FALSE, value_ptr(projection));

		glBindTexture(GL_TEXTURE_2D, vertex_material[vertex_shape[i].materialID].diffuse_tex);
		glDrawElements(GL_TRIANGLES, vertex_shape[i].drawCount, GL_UNSIGNED_INT, 0);
	}

	glActiveTexture(GL_TEXTURE0);
	glUniform1i(texture_location, 0);
	glUniformMatrix4fv(um4mv, 1, GL_FALSE, value_ptr(ModelView));
	glUniformMatrix4fv(um4p, 1, GL_FALSE, value_ptr(projection));
	glUniform1i(tex_mode, texture_mode);
}

//-----------------End Load Scene Function and Variables------------------------

void My_Init()
{
    glClearColor(0.0f, 0.6f, 0.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	initScene();
}

void My_Display()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	renderScene();

    glutSwapBuffers();
}

void My_Reshape(int width, int height)
{
	glViewport(0, 0, width, height);
	viewportAspect = (float)width / (float)height;
	projection = perspective(radians(45.0f), viewportAspect, 0.1f, 3000.f);
}

void My_Timer(int val)
{
	glutPostRedisplay();
	glutTimerFunc(timer_speed, My_Timer, val);
}

void My_Mouse(int button, int state, int x, int y)
{
	if(state == GLUT_DOWN)
	{
		printf("Mouse %d is pressed at (%d, %d)\n", button, x, y);
	}
	else if(state == GLUT_UP)
	{
		printf("Mouse %d is released at (%d, %d)\n", button, x, y);
	}
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
	switch(id)
	{
	case MENU_TIMER_START:
		if(!timer_enabled)
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
	glutInitWindowSize(600, 600);
	glutCreateWindow("AS2_Framework"); // You cannot use OpenGL functions before this line; The OpenGL context must be created first by glutCreateWindow()!
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
	glutPassiveMotionFunc(onMouseHover);
	glutKeyboardFunc(My_Keyboard);
	glutSpecialFunc(My_SpecialKeys);
	glutTimerFunc(timer_speed, My_Timer, 0); 

	// Enter main event loop.
	glutMainLoop();

	return 0;
}
