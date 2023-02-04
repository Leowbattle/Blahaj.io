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

typedef struct vec2 {
	float x;
	float y;
} vec2;

typedef struct vec3 {
	float x;
	float y;
	float z;
} vec3;

vec3 vec3_add(vec3 a, vec3 b) {
	return (vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

vec3 vec3_sub(vec3 a, vec3 b) {
	return (vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

vec3 vec3_scale(vec3 a, float s) {
	return (vec3){a.x * s, a.y * s, a.z * s};
}

float vec3_dot(vec3 a, vec3 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec3 vec3_cross(vec3 a, vec3 b) {
	return (vec3){
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}

float vec3_length(vec3 a) {
	return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
}

vec3 vec3_normalize(vec3 a) {
	float m = vec3_length(a);
	return (vec3){a.x / m, a.y / m, a.z / m};
}

typedef struct vec4 {
	float x;
	float y;
	float z;
	float w;
} vec4;

vec4 vec4_add(vec4 a, vec4 b) {
	return (vec4){a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

vec4 vec4_sub(vec4 a, vec4 b) {
	return (vec4){a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}

vec4 vec4_scale(vec4 a, float s) {
	return (vec4){a.x * s, a.y * s, a.z * s, a.w * s};
}

float vec4_dot(vec4 a, vec4 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

float vec4_length(vec4 a) {
	return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w);
}

vec4 vec4_normalize(vec4 a) {
	float m = vec4_length(a);
	return (vec4){a.x / m, a.y / m, a.z / m, a.w / m};
}

typedef struct mat4 {
	float m[16];
} mat4;

void mat4_identity(mat4* m) {
	float* M = m->m;

	M[0] = 1;
	M[1] = 0;
	M[2] = 0;
	M[3] = 0;

	M[4] = 0;
	M[5] = 1;
	M[6] = 0;
	M[7] = 0;

	M[8] = 0;
	M[9] = 0;
	M[10] = 1;
	M[11] = 0;

	M[12] = 0;
	M[13] = 0;
	M[14] = 0;
	M[15] = 1;
}

void mat4_mul(mat4* dst, mat4* a, mat4* b) {
	float* M = dst->m;
	float* A = a->m;
	float* B = b->m;

	M[0] = A[0] * B[0] + A[1] * B[4] + A[2] * B[8] + A[3] * B[12];
	M[1] = A[0] * B[1] + A[1] * B[5] + A[2] * B[9] + A[3] * B[13];
	M[2] = A[0] * B[2] + A[1] * B[6] + A[2] * B[10] + A[3] * B[14];
	M[3] = A[0] * B[3] + A[1] * B[7] + A[2] * B[11] + A[3] * B[15];

	M[4] = A[4] * B[0] + A[5] * B[4] + A[6] * B[8] + A[7] * B[12];
	M[5] = A[4] * B[1] + A[5] * B[5] + A[6] * B[9] + A[7] * B[13];
	M[6] = A[4] * B[2] + A[5] * B[6] + A[6] * B[10] + A[7] * B[14];
	M[7] = A[4] * B[3] + A[5] * B[7] + A[6] * B[11] + A[7] * B[15];

	M[8] =  A[8] * B[0] + A[9] * B[4] + A[10] * B[8] + A[11] * B[12];
	M[9] =  A[8] * B[1] + A[9] * B[5] + A[10] * B[9] + A[11] * B[13];
	M[10] = A[8] * B[2] + A[9] * B[6] + A[10] * B[10] + A[11] * B[14];
	M[11] = A[8] * B[3] + A[9] * B[7] + A[10] * B[11] + A[11] * B[15];

	M[12] = A[12] * B[0] + A[13] * B[4] + A[14] * B[8] + A[15] * B[12];
	M[13] = A[12] * B[1] + A[13] * B[5] + A[14] * B[9] + A[15] * B[13];
	M[14] = A[12] * B[2] + A[13] * B[6] + A[14] * B[10] + A[15] * B[14];
	M[15] = A[12] * B[3] + A[13] * B[7] + A[14] * B[11] + A[15] * B[15];
}

void mat4_scale(mat4* m, vec3 s) {
	float* M = m->m;

	M[0] *= s.x;
	M[4] *= s.x;
	M[8] *= s.x;

	M[1] *= s.y;
	M[5] *= s.y;
	M[9] *= s.y;

	M[2] *= s.z;
	M[6] *= s.z;
	M[10] *= s.z;
}

void mat4_translate(mat4* m, vec3 t) {
	float* M = m->m;

	M[3] += M[0] * t.x + M[1] * t.y + M[2] * t.z;
	M[7] += M[4] * t.x + M[5] * t.y + M[6] * t.z;
	M[11] += M[8] * t.x + M[9] * t.y + M[10] * t.z;
	M[15] += M[12] * t.x + M[13] * t.y + M[14] * t.z;
}

void mat4_rotatex(mat4* m, float t) {
	float ct = cosf(t);
	float st = sinf(t);

	float* M = m->m;

	mat4 m2 = {
		M[0],
		M[1] * ct + M[2] * st,
		M[1] * -st + M[2] * ct,
		M[3],
		
		M[4],
		M[5] * ct + M[6] * st,
		M[5] * -st + M[6] * ct,
		M[7],

		M[8],
		M[9] * ct + M[10] * st,
		M[9] * -st + M[10] * ct,
		M[11],

		M[12],
		M[13] * ct + M[14] * st,
		M[13] * -st + M[14] * ct,
		M[15],
	};

	memcpy(M, m2.m, sizeof(mat4));
}

void mat4_rotatey(mat4* m, float t) {
	float ct = cosf(t);
	float st = sinf(t);

	float* M = m->m;

	mat4 m2 = {
		M[0] * ct + M[2] * -st,
		M[1],
		M[0] * st + M[2] * ct,
		M[3],

		M[4] * ct + M[6] * -st,
		M[5],
		M[4] * st + M[6] * ct,
		M[7],

		M[8] * ct + M[10] * -st,
		M[9],
		M[8] * st + M[10] * ct,
		M[11],

		M[12] * ct + M[14] * -st,
		M[13],
		M[12] * st + M[14] * ct,
		M[15],
	};

	memcpy(M, m2.m, sizeof(mat4));
}

void mat4_rotatez(mat4* m, float t) {
	float ct = cosf(t);
	float st = sinf(t);

	float* M = m->m;

	mat4 m2 = {
		M[0] * ct + M[1] * -st,
		M[0] * st + M[1] * ct,
		M[2],
		M[3],

		M[4] * ct + M[5] * -st,
		M[4] * st + M[5] * ct,
		M[6],
		M[7],

		M[8] * ct + M[9] * -st,
		M[8] * st + M[9] * ct,
		M[10],
		M[11],

		M[12] * ct + M[13] * -st,
		M[12] * st + M[13] * ct,
		M[14],
		M[15],
	};

	memcpy(M, m2.m, sizeof(mat4));
}

void mat4_ypr(float yaw, float pitch, float roll);

// void mat4_lookat(mat4* m, vec3 pos, vec3 target, vec3 up) {
// 	vec3 zaxis = vec3_normalize(vec3_sub(target, pos));
// 	vec3 xaxis = vec3_normalize(vec3_cross(up, zaxis));
// 	vec3 yaxis = vec3_cross(zaxis, xaxis);

// 	// mat4 lam = {
// 	// 	xaxis.x, yaxis.x, zaxis.x, 0,
// 	// 	xaxis.y, yaxis.y, zaxis.y, 0,
// 	// 	xaxis.z, yaxis.z, zaxis.z, 0,
// 	// 	-vec3_dot(xaxis, pos), -vec3_dot(yaxis, pos), -vec3_dot(zaxis, pos), 1
// 	// };
// 	mat4 lam = {
// 		xaxis.x, xaxis.y, xaxis.z, -vec3_dot(xaxis, pos),
// 		yaxis.x, yaxis.y, yaxis.z, -vec3_dot(yaxis, pos),
// 		zaxis.x, zaxis.y, zaxis.z, -vec3_dot(zaxis, pos),
// 		0, 0, 0, 1
// 	};

// 	mat4_mul(m, m, &lam);
// }

void mat4_frustum(mat4* m, float l, float r, float b, float t, float n, float f);

void mat4_perspective(mat4* m, float fovy, float aspect, float n, float f) {
	float F = 1 / tanf(fovy / 2.);

	float* M = m->m;

	M[0] = F / aspect;
	M[1] = 0;
	M[2] = 0;
	M[3] = 0;

	M[4] = 0;
	M[5] = F;
	M[6] = 0;
	M[7] = 0;

	M[8] = 0;
	M[9] = 0;
	M[10] = (f + n) / (n - f);
	M[11] = 2 * f * n / (n - f);
	
	M[12] = 0;
	M[13] = 0;
	M[14] = -1;
	M[15] = 0;
}

void mat4_ortho(mat4* m, float l, float r, float b, float t, float n, float f) {
	float* M = m->m;

	M[0] = 2 / (r - l);
	M[1] = 0;
	M[2] = 0;
	M[3] = -(r + l) / (r - l);

	M[4] = 0;
	M[5] = 2 / (t - b);
	M[6] = 0;
	M[7] = -(t + b) / (t - b);

	M[8] = 0;
	M[9] = 0;
	M[10] = -2 / (f - n);
	M[11] = -(f + n) / (f - n);

	M[12] = 0;
	M[13] = 0;
	M[14] = 0;
	M[15] = 1;
}

void mat4_print(mat4* m) {
	float* M = m->m;
	printf("%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n",
		M[0], M[1], M[2], M[3],
		M[4], M[5], M[6], M[7],
		M[8], M[9], M[10], M[11],
		M[12], M[13], M[14], M[15]);
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
				sscanf(line, "vt %f %f", &texcoord.x, &texcoord.y);

				Vector_add(texcoords, &texcoord);
			}
			else if (line[1] == 'n') {
				vec3 normal;
				sscanf(line, "vn %f %f %f", &normal.x, &normal.y, &normal.z);
				
				Vector_add(normals, &normal);
			}
			else {
				vec3 pos;
				sscanf(line, "v %f %f %f", &pos.x, &pos.y, &pos.z);
				
				Vector_add(vertices, &pos);
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

			ModelVertex mv1 = {
				((vec3*)vertices->data)[v1 - 1],
				((vec2*)texcoords->data)[t1 - 1],
				((vec3*)normals->data)[n1 - 1],
			};
			ModelVertex mv2 = {
				((vec3*)vertices->data)[v2 - 1],
				((vec2*)texcoords->data)[t2 - 1],
				((vec3*)normals->data)[n2 - 1],
			};
			ModelVertex mv3 = {
				((vec3*)vertices->data)[v3 - 1],
				((vec2*)texcoords->data)[t3 - 1],
				((vec3*)normals->data)[n3 - 1],
			};

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

	glCullFace(GL_FRONT);

	Model* model = Model_load("data/models/blahaj.obj");

	GLuint prog = loadShaderProg("data/shaders/shader.vs", "data/shaders/shader.fs");
	GLuint mat_loc = glGetUniformLocation(prog, "u_mat");
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

		mat4 mat;
		mat4_identity(&mat);
		// mat4_perspective(&mat, deg2rad(90), width / (float)height, 0, 100);

		// mat4_translate(&mat, (vec3){0, 0, 5});
		// mat4_rotatex(&mat, deg2rad(90));

		mat4_rotatey(&mat, frameNo / 60.0f * deg2rad(60));
		mat4_rotatex(&mat, frameNo / 60.0f * deg2rad(60));
		mat4_scale(&mat, (vec3){0.5f, 0.5f, 0.5f});

		glUniformMatrix4fv(mat_loc, 1, GL_TRUE, mat.m);

		glDrawArrays(GL_TRIANGLES, 0, model->vertexCount);

		SDL_GL_SwapWindow(window);

		frameNo++;
	}

	return 0;
}
