
#include <bits/stdc++.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#define BUFFER_SIZE 256
#define PI 3.14159265359

using namespace std;

GLint compileShader(const char* filename, GLenum type) {

    FILE* file = fopen(filename, "rb");

    if (file == NULL) {
        cerr << "Cannot open shader " << filename << endl;
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    const int size = ftell(file);
    rewind(file);

    const GLchar* source = new GLchar[size+1];
    fread(const_cast<char*>(source), sizeof(char), size, file);
    const_cast<char&>(source[size]) = '\0';

    const GLint shader = glCreateShader(type);

    if (not shader) {
        cerr << "Cannot create a shader of type " << shader << endl;
        exit(1);
    }

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    {
        GLint compiled;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (not compiled) {
            std::cerr << "Cannot compile shader " << filename << std::endl;
            abort();
        }
    }

    return shader;

}

// compiles a shader program based on paths for vertex and fragment shaders, then returns id of the program
GLint compileShaderProgram(const char* vertexShaderFilename, const char* fragmentShaderFilename) {

    const GLint program = glCreateProgram();

    if (not program) {
        cerr << "Cannot create a shader program" << endl;
        abort();
    }

    glAttachShader(program, compileShader(vertexShaderFilename, GL_VERTEX_SHADER));
    glAttachShader(program, compileShader(fragmentShaderFilename, GL_FRAGMENT_SHADER));

    glLinkProgram(program);

    {
        GLint linked;
        glGetShaderiv(program, GL_LINK_STATUS, &linked);
        if (not linked) {
            cerr << "Cannot link shader program with shaders " << vertexShaderFilename << " and " << fragmentShaderFilename << endl;
            abort();
        }
    }

    return program;

}

// struct for a shader program to make initialization and usage of shaders easier
struct ShaderProgram {

    GLuint id;
    unordered_map<const char*, uint> uloc;

    ShaderProgram(const char* vertex_shader_path, const char* fragment_shader_path, vector<const char*> uniform_names = {}) {
        id = compileShaderProgram(vertex_shader_path, fragment_shader_path);
        for (const char* uniform_name : uniform_names) {
            uloc[uniform_name] = glGetUniformLocation(id, uniform_name);
        }
    }

    void add_uloc(const char* name) {
        uloc[name] = glGetUniformLocation(id, name);
    }

    void use() {
        glUseProgram(id);
    }

};

struct VBO {

    GLuint id;

    VBO(vector<float>& vertices) {
        glGenBuffers(1, &id);
        glBindBuffer(GL_ARRAY_BUFFER, id);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    }

    void bind() {
        glBindBuffer(GL_ARRAY_BUFFER, id);
    }

    void unbind() {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

};

struct VAO {

    GLuint id;

    VAO() {
        glGenVertexArrays(1, &id);
    }

    void link_VBO(VBO& vbo, GLuint layout, int attrib_size) {
        vbo.bind();
        glVertexAttribPointer(layout, attrib_size, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glEnableVertexAttribArray(layout);
        vbo.unbind();
    }

    void bind() {
        glBindVertexArray(id);
    }

    void unbind() {
        glBindVertexArray(0);
    }

};

struct EBO {

    GLuint id;

    EBO(vector<int>& indices) {
        glGenBuffers(1, &id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), &indices[0], GL_STATIC_DRAW);
    }

    void bind() {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
    }

    void unbind() {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

};

struct Tex2D {

    GLuint id;
    int loc;

    Tex2D(const char* tex_path) {
        SDL_Surface* surface = IMG_Load(tex_path);
        glGenTextures(1, &id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        SDL_FreeSurface(surface);
    }

    // bind texture to a texture location
    void bind(int location) {
        loc = location;
        glActiveTexture(GL_TEXTURE0+loc);
        glBindTexture(GL_TEXTURE_2D, id);
    }

};

int main() {

    if (!glfwInit()) {
        cerr << "GLFW init failed." << endl;
        exit(1);
    }

    int ww = 800;
    int wh = 800;

    GLFWwindow* window;
    window = glfwCreateWindow(ww, wh, "this is a window", NULL, NULL);
    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        cerr << "GLEW init failed." << endl;
        exit(1);
    }

    ShaderProgram default_program("../src/shaders/default.vert", "../src/shaders/default.frag", {});
    default_program.use();

    Tex2D tex("../textures/jesser.png");

    vector<float> vertices = {
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
    };

    VAO vao;
    vao.bind();

    VBO vbo(vertices);
    // EBO ebo(indices);

    vao.link_VBO(vbo, 0, 3);
    vao.unbind();
    vbo.unbind();

    glEnable(GL_DEPTH_TEST);

    double time_when_fps = glfwGetTime();
    int updates_since_fps = 0;
    float dt = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        
        float time_start = glfwGetTime();

        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) {
            break;
        }

        glfwGetWindowSize(window, &ww, &wh);
        glViewport(0, 0, ww, wh);

        tex.bind(2);
        glUniform1i(default_program.uloc["tex"], tex.loc);

        glClearColor(0.3f, 0.4f, 0.45f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        default_program.use();
        vao.bind();
        glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 3);
        vao.unbind();

        glfwSwapBuffers(window);

        updates_since_fps++;
        if (glfwGetTime() - time_when_fps >= 1.0) {
            cout << "FPS: " << floor(updates_since_fps / (glfwGetTime() - time_when_fps)) << endl;
            time_when_fps = glfwGetTime();
            updates_since_fps = 0;
        }
        dt = glfwGetTime() - time_start;

    }

    glfwTerminate();

}
