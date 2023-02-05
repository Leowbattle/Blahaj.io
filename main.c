#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <time.h>

#include <signal.h>
#include <execinfo.h>

#include <glad/glad.h>
#include <SDL2/SDL.h>

// #define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cglm/call.h>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>

// https://stackoverflow.com/questions/13408990/how-to-generate-random-float-number-in-c
float float_rand( float min, float max )
{
    float scale = rand() / (float) RAND_MAX; /* [0, 1.0] */
    return min + scale * ( max - min );      /* [min, max] */
}

#define PI 3.1415926

float deg2rad(float deg) {
	return deg * PI / 180;
}

float rad2deg(float rad) {
	return rad * 180 / PI;
}

float mapf(float x, float a1, float b1, float a2, float b2) {
	float t = (x - a1) / (b1 - a1);
	return a2 + t * (b2 - a2);
}

float clampf(float x, float a, float b) {
	if (x < a) {
		return a;
	}
	if (x > b) {
		return b;
	}
	return x;
}

float lerpf(float a, float b, float t) {
	return a + (b - a) * t;
}

SDL_Window* window;
SDL_Renderer* renderer;
SDL_GLContext gl;
bool running = true;

SDL_GameController* controller;

struct NVGcontext* vg;

int width = 1280;
int height = 720;

uint8_t* keyboardState = NULL;
uint8_t* lastKeyboardState = NULL;

void updateKeyboard() {
	int numKeys;
	const uint8_t* kb = SDL_GetKeyboardState(&numKeys);

	if (keyboardState == NULL) {
		keyboardState = malloc(numKeys);
		lastKeyboardState = malloc(numKeys);
	}

	memcpy(lastKeyboardState, keyboardState, numKeys);
	memcpy(keyboardState, kb, numKeys);
}

int frameNo = 0;
float globalTime = 0;
float dt = 1 / 60.0f;

mat4 projMat;
mat4 viewMat;

GLuint texturedShader;

void panic(const char* format, ...) {
	fprintf(stderr, "Panic: ");

	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	#define BACKTRACE_MAX 256
	void* data[BACKTRACE_MAX];
	int n = backtrace(data, BACKTRACE_MAX);

	char** syms = backtrace_symbols(data, n);

	for (int i = 0; i < n; i++) {
		printf("%s\n", syms[i]);
	}

	free(syms);

	exit(EXIT_FAILURE);
}

void* xmalloc(size_t size) {
	void* ptr = malloc(size);
	if (ptr == NULL) {
		panic("Failed to allocate %zu bytes of memory\n", size);
	}
	return ptr;
}

void* xrealloc(void* ptr, size_t size) {
	void* p2 = realloc(ptr, size);
	if (p2 == NULL) {
		panic("Failed to reallocate %ld bytes of memory\n", size);
	}
	return p2;
}

void xfree(void* ptr) {
	free(ptr);
}

typedef struct Vector Vector;

struct Vector {
	void* data;
	size_t capacity;
	size_t count;
	size_t size;
};

Vector* Vector_new(size_t size) {
	Vector* vec = xmalloc(sizeof(Vector));
	vec->data = NULL;
	vec->capacity = 0;
	vec->count = 0;
	vec->size = size;
	return vec;
}

void Vector_delete(Vector* vec) {
	if (vec->data != NULL) {
		xfree(vec->data);
	}
	xfree(vec);
}

void Vector_add(Vector* vec, void* item) {
	if (vec->data == NULL) {
		vec->capacity = 4;
		vec->data = xmalloc(vec->capacity * vec->size);
	}

	if (vec->count == vec->capacity) {
		vec->capacity *= 2;
		vec->data = xrealloc(vec->data, vec->capacity * vec->size);
	}

	memcpy((uint8_t*)(vec->data) + vec->count++ * vec->size, item, vec->size);
}

char* read_file(const char* path) {
	FILE* file = fopen(path, "r");
	if (file == NULL) {
		panic("Unable to open file %s\n", path);
	}

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* data = xmalloc(size + 1);
	fread(data, 1, size, file);
	data[size] = '\0';

	fclose(file);

	return data;
}

GLuint loadShader(const char* path, GLenum type) {
	GLuint shader = glCreateShader(type);

	char* data = read_file(path);
	glShaderSource(shader, 1, (const char* const*)&data, NULL);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		GLint len;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

		char* buf = xmalloc(len);
		glGetShaderInfoLog(shader, len, NULL, buf);
		panic("Shader compilation error %s: %s\n", path, buf);
		xfree(buf);
	}

	xfree(data);

	return shader;
}

GLuint loadShaderProg(const char* vs_path, const char* fs_path) {
	GLuint vs = loadShader(vs_path, GL_VERTEX_SHADER);
	GLuint fs = loadShader(fs_path, GL_FRAGMENT_SHADER);

	GLuint prog = glCreateProgram();

	glAttachShader(prog, vs);
	glAttachShader(prog, fs);

	glLinkProgram(prog);

	GLint status;
	glGetProgramiv(prog, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		GLint len;
		glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);

		char* buf = xmalloc(len);
		glGetProgramInfoLog(prog, len, NULL, buf);
		panic("Shader link error %s, %s: %s\n", vs_path, fs_path, buf);
		xfree(buf);
	}

	glDetachShader(prog, vs);
	glDetachShader(prog, fs);

	glDeleteShader(vs);
	glDeleteShader(fs);

	return prog;
}

typedef struct Model {
	GLuint vao;
	GLuint vbo;
	GLuint texture;
	int vertexCount;
} Model;

typedef struct ModelVertex {
	vec3 pos;
	vec2 uv;
	vec3 normal;
} ModelVertex;

Model* Model_load(const char* path) {
	FILE* file = fopen(path, "r");
	if (file == NULL) {
		panic("Unable to open file %s\n", path);
	}

	Vector* vertices = Vector_new(sizeof(vec3));
	Vector* texcoords = Vector_new(sizeof(vec2));
	Vector* normals = Vector_new(sizeof(vec3));
	Vector* modelVertices = Vector_new(sizeof(ModelVertex));

	int texture;

	char line[256];
	while (fgets(line, sizeof(line), file) != NULL) {
		if (line[0] == 'v') {
			if (line[1] == 't') {
				vec2 texcoord;
				sscanf(line, "vt %f %f", &texcoord[0], &texcoord[1]);

				Vector_add(texcoords, texcoord);
			}
			else if (line[1] == 'n') {
				vec3 normal;
				sscanf(line, "vn %f %f %f", &normal[0], &normal[1], &normal[2]);
				
				Vector_add(normals, normal);
			}
			else {
				vec3 pos;
				sscanf(line, "v %f %f %f", &pos[0], &pos[1], &pos[2]);
				
				Vector_add(vertices, pos);
			}
		}
		else if (line[0] == 'm') {
			char filename[256];
			sscanf(line, "mtllib %s", filename);

			FILE* mtlFile = fopen(filename, "r");
			if (mtlFile == NULL) {
				panic("Unable to open file %s\n", path);
			}

			char line2[256];
			while (fgets(line2, sizeof(line2), mtlFile)) {
				if (line2[0] == 'm') {
					char imgName[256];
					sscanf(line2, "map_Kd %s", imgName);

					int texW;
					int texH;
					int comp;

					stbi_set_flip_vertically_on_load(1);
					void* pixelData = stbi_load(imgName, &texW, &texH, &comp, 3);

					glActiveTexture(GL_TEXTURE0);
					glGenTextures(1, &texture);
					glBindTexture(GL_TEXTURE_2D, texture);

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texW, texH, 0, GL_RGB, GL_UNSIGNED_BYTE, pixelData);
				
					stbi_image_free(pixelData);
				}
			}

			fclose(mtlFile);
		}
		else if (line[0] == 'f') {
			int v1;
			int v2;
			int v3;
			int t1;
			int t2;
			int t3;
			int n1;
			int n2;
			int n3;

			sscanf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d",
				&v1, &t1, &n1,
				&v2, &t2, &n2,
				&v3, &t3, &n3);

			ModelVertex mv1;
			ModelVertex mv2;
			ModelVertex mv3;

			memcpy(mv1.pos, ((vec3*)vertices->data)[v1 - 1], sizeof(vec3));
			memcpy(mv1.uv, ((vec2*)texcoords->data)[t1 - 1], sizeof(vec2));
			memcpy(mv1.normal, ((vec3*)normals->data)[n1 - 1], sizeof(vec3));

			memcpy(mv2.pos, ((vec3*)vertices->data)[v2 - 1], sizeof(vec3));
			memcpy(mv2.uv, ((vec2*)texcoords->data)[t2 - 1], sizeof(vec2));
			memcpy(mv2.normal, ((vec3*)normals->data)[n2 - 1], sizeof(vec3));

			memcpy(mv3.pos, ((vec3*)vertices->data)[v3 - 1], sizeof(vec3));
			memcpy(mv3.uv, ((vec2*)texcoords->data)[t3 - 1], sizeof(vec2));
			memcpy(mv3.normal, ((vec3*)normals->data)[n3 - 1], sizeof(vec3));

			Vector_add(modelVertices, &mv1);
			Vector_add(modelVertices, &mv2);
			Vector_add(modelVertices, &mv3);
		}
	}

	fclose(file);

	Model* model = xmalloc(sizeof(Model));
	model->vertexCount = modelVertices->count;
	model->texture = texture;

	glGenVertexArrays(1, &model->vao);
	glBindVertexArray(model->vao);

	glGenBuffers(1, &model->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, model->vbo);
	glBufferData(GL_ARRAY_BUFFER, modelVertices->count * sizeof(ModelVertex), modelVertices->data, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), 0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)(3 * sizeof(float)));

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)(5 * sizeof(float)));

	Vector_delete(vertices);
	Vector_delete(texcoords);
	Vector_delete(normals);
	Vector_delete(modelVertices);

	return model;
}

GLuint mat_loc;
GLuint view_loc;
GLuint tex_loc;

struct {
	vec3 pos;
	vec3 dir;
	float speed;

	float scale;
	float scaleTarget;

	Model* model;

	float yaw;
	float pitch;
	float roll;

	float pitchTarget;
	float rollTarget;

	vec3 camPos;
	vec3 camTarget;
} Blahaj;

void Blahaj_init() {
	Blahaj.model = Model_load("data/models/blahaj.obj");

	Blahaj.yaw = 0;

	Blahaj.pitchTarget = 0;
	Blahaj.pitch = 0;
	Blahaj.roll = 0;

	Blahaj.scale = 1;
	Blahaj.scaleTarget = 1;

	glm_vec3_copy((vec3){0, 0, 0}, Blahaj.pos);
	glm_vec3_copy((vec3){2, 2, 0}, Blahaj.camPos);
}

struct {
	GLuint vao;
	GLuint ebo;
	GLuint vbo_xy;
	GLuint vbo_u;
	GLuint vbo_normal;

	float* u;
	float* dudt;
	vec3* normals;
	float c;
	int sim_size;
	float size;

	GLuint shader;
} Water;

void Water_add_pulse(float strength, float size, float cx, float cy);

void Blahaj_update() {
	const float turnRoll = deg2rad(30);
	const float acceleration = 5;
	const float deceleration = 5;
	const float maxSpeed = 20;
	const float gravity = 3;
	const float bouyancy = 5;
	const float jumpImpulse = 3;
	const float maxSpeedY = 4;

	Blahaj.rollTarget = 0;

	if (keyboardState[SDL_SCANCODE_LEFT]) {
		Blahaj.yaw += 0.1f;
		Blahaj.rollTarget = turnRoll;
	}
	if (keyboardState[SDL_SCANCODE_RIGHT]) {
		Blahaj.yaw -= 0.1f;
		Blahaj.rollTarget = -turnRoll;
	}

	glm_vec3_copy((vec3){-1, 0, 0}, Blahaj.dir);
	glm_vec3_rotate(Blahaj.dir, Blahaj.yaw, (vec3){0, 1, 0});

	bool accelerating = false;
	if (keyboardState[SDL_SCANCODE_UP]) {
		accelerating = true;

		Blahaj.speed = clampf(Blahaj.speed + acceleration * dt, 0, maxSpeed);

		Water_add_pulse(0.25f * Blahaj.scale, 0.05f * Blahaj.scale, Blahaj.pos[0], Blahaj.pos[2]);
	}
	if (keyboardState[SDL_SCANCODE_DOWN]) {
		accelerating = true;
	}

	if (accelerating) {
		Blahaj.pitchTarget = sinf(2 * PI * globalTime * 2) * deg2rad(7);
	} else {
		Blahaj.pitchTarget = 0;
		Blahaj.speed = clampf(Blahaj.speed - deceleration * dt, 0, maxSpeed);
	}

	Blahaj.pitch = lerpf(Blahaj.pitch, Blahaj.pitchTarget, 15 * dt);
	
	Blahaj.roll = lerpf(Blahaj.roll, Blahaj.rollTarget, 15 * dt);

	Blahaj.scale = lerpf(Blahaj.scale, Blahaj.scaleTarget, 15 * dt);

	vec3 v = {4 * Blahaj.scale, 2 * Blahaj.scale, 0};
	glm_vec3_rotate(v, Blahaj.yaw, (vec3){0, 1, 0});
	glm_vec3_add(v, Blahaj.pos, Blahaj.camTarget);

	float lerpK = 15 * dt;
	glm_vec3_lerp(Blahaj.camPos, Blahaj.camTarget, lerpK, Blahaj.camPos);

	vec3 d_pos;
	glm_vec3_scale(Blahaj.dir, Blahaj.speed * dt, d_pos);
	glm_vec3_add(Blahaj.pos, d_pos, Blahaj.pos);

	if (Blahaj.pos[0] < -Water.size / 2) {
		Blahaj.pos[0] = Water.size / 2;
	}
	if (Blahaj.pos[0] > Water.size / 2) {
		Blahaj.pos[0] = -Water.size / 2;
	}
	if (Blahaj.pos[2] < -Water.size / 2) {
		Blahaj.pos[2] = Water.size / 2;
	}
	if (Blahaj.pos[2] > Water.size / 2) {
		Blahaj.pos[2] = -Water.size / 2;
	}

	glUseProgram(texturedShader);
	glBindVertexArray(Blahaj.model->vao);

	mat4 modelMat;
	glm_mat4_identity(modelMat);
	glm_translate(modelMat, Blahaj.pos);
	glm_rotate_y(modelMat, Blahaj.yaw, modelMat);
	glm_rotate_z(modelMat, Blahaj.pitch, modelMat);
	glm_rotate_x(modelMat, Blahaj.roll, modelMat);
	glm_scale_uni(modelMat, Blahaj.scale);

	mat4 mvp;
	glm_mat4_mul(projMat, viewMat, mvp);
	glm_mat4_mul(mvp, modelMat, mvp);

	glUniformMatrix4fv(mat_loc, 1, GL_FALSE, (float*)mvp);
	glUniformMatrix4fv(view_loc, 1, GL_FALSE, (float*)viewMat);

	glBindTexture(GL_TEXTURE_2D, Blahaj.model->texture);

	glDrawArrays(GL_TRIANGLES, 0, Blahaj.model->vertexCount);
}

GLuint mat_loc2;
GLuint view_loc2;

void Water_init() {
	Water.sim_size = 500;
	Water.c = 400;
	Water.size = 100;
	Water.u = xmalloc(Water.sim_size * Water.sim_size * sizeof(float));
	Water.dudt = xmalloc(Water.sim_size * Water.sim_size * sizeof(float));
	Water.normals = xmalloc(Water.sim_size * Water.sim_size * sizeof(vec3));

	glGenVertexArrays(1, &Water.vao);
	glBindVertexArray(Water.vao);

	glGenBuffers(1, &Water.ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Water.ebo);

	int numTris = (Water.sim_size - 1) * (Water.sim_size - 1) * 2;
	unsigned int* indices = xmalloc(numTris * 3 * sizeof(unsigned int));
	int indicesI = 0;
	for (int i = 0; i < Water.sim_size - 1; i++) {
		for (int j = 0; j < Water.sim_size - 1; j++) {
			indices[indicesI++] = j * Water.sim_size + i;
			indices[indicesI++] = (j + 1) * Water.sim_size + i;
			indices[indicesI++] = j * Water.sim_size + i + 1;

			indices[indicesI++] = j * Water.sim_size + i + 1;
			indices[indicesI++] = (j + 1) * Water.sim_size + i;
			indices[indicesI++] = (j + 1) * Water.sim_size + i + 1;
		}
	}
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, numTris * 3 * sizeof(unsigned int), indices, GL_STATIC_DRAW);

	glGenBuffers(1, &Water.vbo_xy);
	glBindBuffer(GL_ARRAY_BUFFER, Water.vbo_xy);
	
	float* xy = xmalloc(Water.sim_size * Water.sim_size * 2 * sizeof(float));
	for (int i = 0; i < Water.sim_size; i++) {
		for (int j = 0; j < Water.sim_size; j++) {
			float x = mapf(j, 0, Water.sim_size - 1, -Water.size / 2, Water.size / 2);
			float y = mapf(i, 0, Water.sim_size - 1, -Water.size / 2, Water.size / 2);

			xy[2 * (i * Water.sim_size + j) + 0] = x;
			xy[2 * (i * Water.sim_size + j) + 1] = y;
		}
	}
	glBufferData(GL_ARRAY_BUFFER, Water.sim_size * Water.sim_size * 2 * sizeof(float), xy, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

	glGenBuffers(1, &Water.vbo_u);
	glBindBuffer(GL_ARRAY_BUFFER, Water.vbo_u);
	glBufferData(GL_ARRAY_BUFFER, Water.sim_size * Water.sim_size * sizeof(float), NULL, GL_STREAM_DRAW);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, 0);

	glGenBuffers(1, &Water.vbo_normal);
	glBindBuffer(GL_ARRAY_BUFFER, Water.vbo_normal);
	glBufferData(GL_ARRAY_BUFFER, Water.sim_size * Water.sim_size * sizeof(vec3), NULL, GL_STREAM_DRAW);

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

	Water.shader = loadShaderProg("data/shaders/water.vs", "data/shaders/water.fs");
	mat_loc2 = glGetUniformLocation(Water.shader, "u_mat");
	view_loc2 = glGetUniformLocation(Water.shader, "u_view");
}

void Water_add_pulse(float strength, float size, float cx, float cy) {
	for (int i = 0; i < Water.sim_size; i++) {
		for (int j = 0; j < Water.sim_size; j++) {
			float x = mapf(j, 0, Water.sim_size - 1, -Water.size / 2, Water.size / 2);
			float y = mapf(i, 0, Water.sim_size - 1, -Water.size / 2, Water.size / 2);

			x -= cx;
			y -= cy;

			float u = strength * expf(-(x * x + y * y) / size);

			Water.u[i * Water.sim_size + j] += u;
		}
	}
}

void Water_step_sim() {
	float c = 4;

	for (int i = 1; i < Water.sim_size - 1; i++) {
		for (int j = 1; j < Water.sim_size - 1; j++) {
			float dx = Water.size / Water.sim_size;

			float dudx = Water.u[i * Water.sim_size + j - 1] - 2 * Water.u[i * Water.sim_size + j] + Water.u[i * Water.sim_size + j + 1];
	  		float dudy = Water.u[(i - 1) * Water.sim_size + j] - 2 * Water.u[i * Water.sim_size + j] + Water.u[(i + 1) * Water.sim_size + j];
	  		dudx /= dx * dx;
			dudy /= dx * dx;
			Water.dudt[i * Water.sim_size + j] += (dudx + dudy) * c * c * dt;
		}
	}

	for (int i = 0; i < Water.sim_size; i++) {
		for (int j = 0; j < Water.sim_size; j++) {
			// float x = mapf(j, 0, Water.sim_size - 1, -Water.size / 2, Water.size / 2);
			// float y = mapf(i, 0, Water.sim_size - 1, -Water.size / 2, Water.size / 2);

			// Water.u[i * Water.sim_size + j] = sin(sqrtf(x * x + y * y)*4 + globalTime) / 16;

			Water.u[i * Water.sim_size + j] += Water.dudt[i * Water.sim_size + j] * dt;
		}
	}
	glBindBuffer(GL_ARRAY_BUFFER, Water.vbo_u);
	glBufferSubData(GL_ARRAY_BUFFER, 0, Water.sim_size * Water.sim_size * sizeof(float), Water.u);

	for (int i = 1; i < Water.sim_size - 1; i++) {
		for (int j = 1; j < Water.sim_size - 1; j++) {
			float dx = Water.size / Water.sim_size;

			float u1 = Water.u[i * Water.sim_size + j + 1] - Water.u[i * Water.sim_size + j];
			float u2 = Water.u[(i + 1) * Water.sim_size + j] - Water.u[i * Water.sim_size + j];

			// u1 /= (2 * dx);
			// u2 /= (2 * dx);

			vec3 vx = {0};
			vec3 vy = {0};
			
			vx[1] = 1;
			vx[0] = u1;
			vy[0] = u2;
			vy[2] = 1;

			vec3 normal;
			glm_vec3_cross(vx, vy, normal);
			glm_vec3_normalize(normal);
			glm_vec3_copy(normal, Water.normals[i * Water.sim_size + j]);
		}
	}
	glBindBuffer(GL_ARRAY_BUFFER, Water.vbo_normal);
	glBufferSubData(GL_ARRAY_BUFFER, 0, Water.sim_size * Water.sim_size * sizeof(vec3), Water.normals);
}

struct {
	GLuint shader;
	GLuint vao;
	GLuint vbo;
	GLuint texture;
} Sky;

void Water_update() {
	glBindVertexArray(Water.vao);
	glBindBuffer(GL_ARRAY_BUFFER, Water.vbo_u);

	Water_step_sim();

	glUseProgram(Water.shader);

	mat4 modelMat;
	glm_mat4_identity(modelMat);

	mat4 mvp;
	glm_mat4_mul(projMat, viewMat, mvp);
	glm_mat4_mul(mvp, modelMat, mvp);

	glUniformMatrix4fv(mat_loc2, 1, GL_FALSE, (float*)mvp);
	glUniformMatrix4fv(view_loc2, 1, GL_FALSE, (float*)viewMat);

	glDrawElements(GL_TRIANGLES, Water.sim_size * Water.sim_size * 6, GL_UNSIGNED_INT, NULL);

	glBindVertexArray(0);
}

GLuint projLoc3;
GLuint viewLoc3;

// https://learnopengl.com/Advanced-OpenGL/Cubemaps
unsigned int loadCubemap(const char** faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < 6; i++)
    {
        unsigned char *data = stbi_load(faces[i], &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
                         0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
            );
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

void Sky_init() {
	Sky.shader = loadShaderProg("data/shaders/sky.vs", "data/shaders/sky.fs");
	projLoc3 = glGetUniformLocation(Sky.shader, "projection");
	viewLoc3 = glGetUniformLocation(Sky.shader, "view");

	char* faces[6] = {
		"data/sky/right.jpg",
		"data/sky/left.jpg",
		"data/sky/bottom.jpg",
		"data/sky/top.jpg",
		"data/sky/front.jpg",
		"data/sky/back.jpg",
	};
	Sky.texture = loadCubemap((const char**)faces);

	glGenVertexArrays(1, &Sky.vao);
	glBindVertexArray(Sky.vao);

	glGenBuffers(1, &Sky.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, Sky.vbo);

	float skyboxVertices[] = {
		// positions          
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		1.0f, -1.0f, -1.0f,
		1.0f, -1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		1.0f,  1.0f, -1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		1.0f, -1.0f,  1.0f
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
}

void Sky_update() {
	glBindVertexArray(Sky.vao);
	glUseProgram(Sky.shader);

	glDepthMask(GL_FALSE);

	glUniformMatrix4fv(projLoc3, 1, GL_FALSE, (float*)projMat);

	mat4 matmat;
	glm_mat4_copy(viewMat, matmat);
	matmat[3][0] = 0;
	matmat[3][1] = 0;
	matmat[3][2] = 0;
	glUniformMatrix4fv(viewLoc3, 1, GL_FALSE, (float*)matmat);

	glDrawArrays(GL_TRIANGLES, 0, 36);

	glDepthMask(GL_TRUE);
}

typedef struct Fish {
	vec3 pos;
	float scale;
	float yaw;
	float targetYaw;

	float roll;

	int turnTimer;

	bool dead;
} Fish;

Vector* fishes;
Model* fishModel;

void Fishs_init() {
	fishModel = Model_load("data/models/blahaj.obj");

	fishes = Vector_new(sizeof(Fish));

	int n = 100;
	for (int i = 0; i < n; i++) {
		Fish fish;
		fish.pos[0] = float_rand(-Water.size / 2, Water.size / 2);
		fish.pos[1] = 0;
		fish.pos[2] = float_rand(-Water.size / 2, Water.size / 2);
		fish.yaw = float_rand(0, 2 * PI);
		fish.roll = float_rand(0, 2 * PI);
		fish.scale = 1;
		fish.targetYaw = fish.yaw;
		fish.turnTimer = 0;
		fish.dead = false;
		Vector_add(fishes, &fish);
	}
}

void Fishs_update() {
	const float fishSpeed = 15;
	const float turnMax = deg2rad(90);

	glUseProgram(texturedShader);
	glBindVertexArray(fishModel->vao);

	for (int i = 0; i < fishes->count; i++) {
		Fish* fish = &((Fish*)fishes->data)[i];

		fish->pos[0] += fishSpeed * cosf(fish->yaw) * dt;
		fish->pos[2] += fishSpeed * sinf(fish->yaw) * dt;

		if (fish->pos[0] < -Water.size / 2) {
			fish->pos[0] = Water.size / 2;
			// fish->pos[1] = 10;
		}
		if (fish->pos[0] > Water.size / 2) {
			fish->pos[0] = -Water.size / 2;
			// fish->pos[1] = 10;
		}
		if (fish->pos[2] < -Water.size / 2) {
			fish->pos[2] = Water.size / 2;
			// fish->pos[1] = 10;
		}
		if (fish->pos[2] > Water.size / 2) {
			fish->pos[2] = -Water.size / 2;
			// fish->pos[1] = 10;
		}

		if (fish->turnTimer++ > 60) {
			fish->turnTimer = 0;

			fish->targetYaw = fish->yaw + float_rand(-turnMax, turnMax);
		}
		fish->yaw = lerpf(fish->yaw, fish->targetYaw, 0.05f);

		fish->roll += deg2rad(90) * dt;

		float d2 = glm_vec3_distance2(Blahaj.pos, fish->pos);
		if (d2 < Blahaj.scale * 4) {
			Blahaj.scaleTarget += 0.1f;
			fish->dead = true;
		}
	
		mat4 modelMat;
		glm_mat4_identity(modelMat);
		glm_translate(modelMat, fish->pos);
		glm_rotate_y(modelMat, PI-fish->yaw, modelMat);
		glm_rotate_x(modelMat, fish->roll, modelMat);
		glm_scale_uni(modelMat, fish->scale);

		mat4 mvp;
		glm_mat4_mul(projMat, viewMat, mvp);
		glm_mat4_mul(mvp, modelMat, mvp);

		glUniformMatrix4fv(mat_loc, 1, GL_FALSE, (float*)mvp);
		glUniformMatrix4fv(view_loc, 1, GL_FALSE, (float*)viewMat);

		glBindTexture(GL_TEXTURE_2D, fishModel->texture);

		glDrawArrays(GL_TRIANGLES, 0, fishModel->vertexCount);
	}

	for (int i = 0; i < fishes->count; i++) {
		Fish* fish = &((Fish*)fishes->data)[i];

		if (fish->dead) {
			((Fish*)fishes->data)[i] = ((Fish*)fishes->data)[--fishes->count];
		}
	}
}

void sigsegv_func(int signo) {
	panic("Segmentation fault\n");
}

typedef enum GameState {
	STATE_MENU,
	STATE_GAME,
	STATE_OVER,
} GameState;

GameState state;

int logoImg;
NVGpaint logoPaint;

void MENU_init() {
	state = STATE_MENU;

	stbi_set_flip_vertically_on_load(0);
	logoImg = nvgCreateImage(vg, "data/logo.png", 0);
	logoPaint = nvgImagePattern(vg, 0, 0, width, height, 0, logoImg, 1);
}

void GAME_init();

void MENU_update() {
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	nvgBeginFrame(vg, width, height, 1);

	nvgBeginPath(vg);
	nvgFillPaint(vg, logoPaint);
	nvgRect(vg, 0, 0, width, height);
	nvgFill(vg);

	nvgEndFrame(vg);

	if (keyboardState[SDL_SCANCODE_RETURN]) {
		GAME_init();
	}
}

int timeLeft;
void GAME_init() {
	state = STATE_GAME;

	timeLeft = 3 * 60;
}

int logoImg2;
NVGpaint logoPaint2;

void OVER_init() {
	state = STATE_OVER;

	stbi_set_flip_vertically_on_load(0);
	logoImg2 = nvgCreateImage(vg, "data/bg.png", 0);
	logoPaint2 = nvgImagePattern(vg, 0, 0, width, height, 0, logoImg2, 1);
}

void OVER_update() {
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	nvgBeginFrame(vg, width, height, 1);

	nvgBeginPath(vg);
	nvgFillPaint(vg, logoPaint2);
	nvgRect(vg, 0, 0, width, height);
	nvgFill(vg);

	nvgFillColor(vg, nvgRGBA(255,192,0,255));
	nvgFontSize(vg, 72.0f);
	nvgFontFace(vg, "font");
	nvgTextAlign(vg, NVG_ALIGN_TOP);

	char text[256];
	sprintf(text, "Thanks for playing! Your score is %d", 100 - fishes->count);
	nvgText(vg, 0, 0, text, NULL);

	nvgEndFrame(vg);

	if (keyboardState[SDL_SCANCODE_RETURN]) {
		Blahaj_init();
		// Water_init();
		// Sky_init();
		Fishs_init();
		
		GAME_init();
	}
}

void GAME_update() {
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glm_perspective(deg2rad(90), width / (float)height, 0.1f, 100, projMat);
	
	vec3 up = {0, 1, 0};
	glm_lookat(Blahaj.camPos, Blahaj.pos, up, viewMat);

	Sky_update();
	Blahaj_update();
	Fishs_update();
	Water_update();

	nvgBeginFrame(vg, width, height, 1);

	nvgFillColor(vg, nvgRGBA(255,192,0,255));
	nvgFontSize(vg, 72.0f);
	nvgFontFace(vg, "font");
	nvgTextAlign(vg, NVG_ALIGN_TOP);

	char text[256];
	sprintf(text, "%.2f seconds left!", timeLeft / 60.0f);
	nvgText(vg, 0, 0, text, NULL);

	sprintf(text, "Score: %d", 100 - fishes->count);
	nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_RIGHT);
	nvgText(vg, width, 0, text, NULL);

	nvgEndFrame(vg);

	timeLeft--;

	if (timeLeft < 0) {
		OVER_init();
	}
}

int main(int argc, char** argv) {
	signal(SIGSEGV, sigsegv_func);

	srand(time(NULL));

	SDL_Init(SDL_INIT_EVERYTHING);

	window = SDL_CreateWindow("RoyalHackaway", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
	SDL_GetWindowSize(window, &width, &height);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 16);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	gl = SDL_GL_CreateContext(window);
	gladLoadGLLoader(SDL_GL_GetProcAddress);

	SDL_GL_SetSwapInterval(1);

	texturedShader = loadShaderProg("data/shaders/shader.vs", "data/shaders/shader.fs");
	mat_loc = glGetUniformLocation(texturedShader, "u_mat");
	view_loc = glGetUniformLocation(texturedShader, "u_view");
	tex_loc = glGetUniformLocation(texturedShader, "u_tex");

	Blahaj_init();
	Water_init();
	Sky_init();
	Fishs_init();

	vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
	nvgCreateFont(vg, "font", "data/Blinker-Regular.ttf");

	MENU_init();

	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
			case SDL_QUIT:
				running = false;
				break;
			default:
				break;
			}
		}

		updateKeyboard();

		switch (state) {
		case STATE_MENU:
			MENU_update();
			break;
		case STATE_GAME:
			GAME_update();
			break;
		case STATE_OVER:
			OVER_update();
			break;
		default:
			panic("Invalid game state %d\n", state);
		}

		SDL_GL_SwapWindow(window);

		frameNo++;
		globalTime = frameNo * dt;
	}

	return 0;
}
