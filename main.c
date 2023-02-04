#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include <signal.h>
#include <execinfo.h>

#include <glad/glad.h>
#include <SDL2/SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cglm/call.h>

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

int width = 1280;
int height = 720;

int frameNo = 0;

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

			// ModelVertex mv1 = {
			// 	((vec3*)vertices->data)[v1 - 1],
			// 	((vec2*)texcoords->data)[t1 - 1],
			// 	((vec3*)normals->data)[n1 - 1],
			// };
			// ModelVertex mv2 = {
			// 	((vec3*)vertices->data)[v2 - 1],
			// 	((vec2*)texcoords->data)[t2 - 1],
			// 	((vec3*)normals->data)[n2 - 1],
			// };
			// ModelVertex mv3 = {
			// 	((vec3*)vertices->data)[v3 - 1],
			// 	((vec2*)texcoords->data)[t3 - 1],
			// 	((vec3*)normals->data)[n3 - 1],
			// };

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

void sigsegv_func(int signo) {
	panic("Segmentation fault\n");
}

int main(int argc, char** argv) {
	signal(SIGSEGV, sigsegv_func);

	SDL_Init(SDL_INIT_EVERYTHING);

	window = SDL_CreateWindow("RoyalHackaway", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL);

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

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	Model* model = Model_load("data/models/blahaj.obj");

	GLuint prog = loadShaderProg("data/shaders/shader.vs", "data/shaders/shader.fs");
	GLuint mat_loc = glGetUniformLocation(prog, "u_mat");
	GLuint view_loc = glGetUniformLocation(prog, "u_view");
	GLuint tex_loc = glGetUniformLocation(prog, "u_tex");

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

		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(prog);
		glBindVertexArray(model->vao);

		mat4 proj;
		glm_perspective(deg2rad(90), width / (float)height, 0.1f, 10, proj);
		
		vec3 eye = {3, sinf(frameNo / 60.0f * 5) * 3, 3};
		vec3 center = {0, 0, 0};
		vec3 up = {0, 1, 0};
		mat4 view;
		glm_lookat(eye, center, up, view);

		mat4 modelMat;
		glm_mat4_identity(modelMat);
		glm_rotate_y(modelMat, frameNo / 60.0f * deg2rad(60), modelMat);

		mat4 mvp;
		glm_mat4_mul(proj, view, mvp);
		glm_mat4_mul(mvp, modelMat, mvp);

		glUniformMatrix4fv(mat_loc, 1, GL_FALSE, (float*)mvp);
		glUniformMatrix4fv(view_loc, 1, GL_FALSE, (float*)view);

		glDrawArrays(GL_TRIANGLES, 0, model->vertexCount);

		SDL_GL_SwapWindow(window);

		frameNo++;
	}

	return 0;
}
