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
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

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
}

void mat4_rotatex(mat4* m, float t);
void mat4_rotatey(mat4* m, float t);

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
void mat4_lookat(mat4* m, vec3 pos, vec3 target, vec3 up);
void mat4_frustum(mat4* m, float l, float r, float b, float t, float n, float f);
void mat4_perspective(mat4* m, float fovy, float aspect, float n, float f);

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

	GLuint prog = loadShaderProg("data/shaders/shader.vs", "data/shaders/shader.fs");
	GLuint mat_loc = glGetUniformLocation(prog, "u_mat");

	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	float data[] = {
		0, 0, 0, 1, 0, 0,
		1, 0, 0, 0, 1, 0,
		0, 1, 0, 0, 0, 1,
		1, 1, 0, 1, 1, 0,
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

	GLuint ebo;
	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	unsigned int indices[] = {
		0, 1, 2,
		1, 2, 3,
	};
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

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
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		glUseProgram(prog);
		glBindVertexArray(vao);

		mat4 mat;
		// mat4_identity(&mat);
		mat4_ortho(&mat, 0, width, 0, height, -1, 1);
		mat4_scale(&mat, (vec3){100, 100, 1});
		mat4_translate(&mat, (vec3){0.5f, 0.5f, 0});
		mat4_rotatez(&mat, frameNo / 60.0f * deg2rad(30));
		mat4_translate(&mat, (vec3){-0.5f, -0.5f, 0});
		glUniformMatrix4fv(mat_loc, 1, GL_TRUE, mat.m);

		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);

		SDL_GL_SwapWindow(window);

		frameNo++;
	}

	return 0;
}
