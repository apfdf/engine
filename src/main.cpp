
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
        glBindTexture(GL_TEXTURE_2D, id);
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

struct Player {

    glm::vec3 p;
    glm::vec3 v;
    float speed;
    float yaw; // rotation around y-axis
    float pitch; // rotation around x-axis
    float fov;
    float height;

    Player(glm::vec3 pos) {
        p = pos;
        speed = 1.0;
        fov = PI/2;
        yaw = 0.0;
        pitch = 0.0;
        height = 1.0;
    }

};

// Vertical rectangle that can collide and stuff
struct Wall {

    glm::vec2 p1, p2;
    float y_lo, y_hi;

    Wall(glm::vec2 p1, glm::vec2 p2, float y_lo, float y_hi): p1(p1), p2(p2), y_lo(y_lo), y_hi(y_hi) {}

};

// Horizontal polygon that can collide and stuff
struct Platform {

    float y;
    vector<glm::vec2> polygon_vertices;

    Platform(float y, vector<glm::vec2> polygon_vertices) : y(y), polygon_vertices(polygon_vertices) {}

};

struct Level {

    vector<Platform> platforms;
    vector<Wall> walls;

};

bool point_in_polygon(glm::vec2 p, vector<glm::vec2>& polygon_vertices) {

    // cast a ray to +x direction and check if intersections with polygon edges is even or odd

    bool res = false;
    int count = 0;

    for (int i = 0; i < polygon_vertices.size(); i++) {

        glm::vec2 p1 = polygon_vertices[i];
        glm::vec2 p2 = polygon_vertices[(i+1) % polygon_vertices.size()];

        if (p1.x > p2.x) {
            swap(p1, p2);
        }

        if (p.y >= min(p1.y, p2.y) && p.y <= max(p1.y, p2.y)) {

            if (p.x <= max(p1.x, p2.x)) {

                if (p1.y == p2.y) {
                    if (p.x >= min(p1.x, p2.x)) {
                        res = true;
                    } else {
                        count++;
                    }
                    continue;
                }

                if (p1.x == p2.x) {
                    if (p.x == p1.x) {
                        res = true;
                    } else {
                        count++;
                    }
                    continue;
                }

                float t = (p.y - p1.y) / ((p2.y - p1.y) / (p2.x - p1.x)) + p1.x;

                if (t == p.x) {
                    res = true;
                } else if (t >= p.x) {
                    count++;
                }

            }

        }

    }

    if (count % 2 != 0) {
        res = true;
    }

    return res;

}

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

    ShaderProgram worldspace_program("../src/shaders/worldspace.vert", "../src/shaders/worldspace.frag", {
        "tex",
        "view_mat",
        "project_mat",
    });
    worldspace_program.use();

    ShaderProgram default_program("../src/shaders/default.vert", "../src/shaders/default.frag", {
        "tex"
    });

    Tex2D tex("../textures/jesser.png");

    vector<float> vertices = {
        -1.0f, -1.0f, 1.0f,
        1.0f, -1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
    };

    VAO vao;
    vao.bind();

    VBO vbo(vertices);
    // EBO ebo(indices);

    vao.link_VBO(vbo, 0, 3);
    vao.unbind();
    vbo.unbind();

    glEnable(GL_DEPTH_TEST);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);


    // player and level stuff
    Player plr(glm::vec3(0.0, 0.0, 0.0));
    vector<Platform> platforms;
    platforms.push_back(Platform(-1.0f, {{-1.0, -1.0}, {1.0, -1.0}, {1.0, 1.0}, {-1.0, 1.0}}));

    double time_when_fps = glfwGetTime();
    int updates_since_fps = 0;
    float dt = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        
        float time_start = glfwGetTime();

        glfwPollEvents();

        double n_mx, n_my;
        glfwGetCursorPos(window, &n_mx, &n_my);
        double dmx = n_mx - mx;
        double dmy = n_my - my;
        mx = n_mx;
        my = n_my;

        plr.yaw -= dmx / 100;
        plr.pitch -= dmy / 100;
        plr.pitch = min(plr.pitch, (float)PI/2);
        plr.pitch = max(plr.pitch, -(float)PI/2);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) {
            break;
        }
        if (glfwGetKey(window, GLFW_KEY_W)) {
            plr.p.z -= dt * cos(plr.yaw) * plr.speed;
            plr.p.x -= dt * sin(plr.yaw) * plr.speed;
        }
        if (glfwGetKey(window, GLFW_KEY_S)) {
            plr.p.z += dt * cos(plr.yaw) * plr.speed;
            plr.p.x += dt * sin(plr.yaw) * plr.speed;
        }
        if (glfwGetKey(window, GLFW_KEY_D)) {
            plr.p.z += dt * -sin(plr.yaw) * plr.speed;
            plr.p.x += dt * cos(plr.yaw) * plr.speed;
        }
        if (glfwGetKey(window, GLFW_KEY_A)) {
            plr.p.z -= dt * -sin(plr.yaw) * plr.speed;
            plr.p.x -= dt * cos(plr.yaw) * plr.speed;
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE)) {
            plr.p.y += dt * plr.speed;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
            plr.p.y -= dt * plr.speed;
        }

        for (Platform platform : platforms) {



        }

        glfwGetWindowSize(window, &ww, &wh);
        glViewport(0, 0, ww, wh);

        // matrix that transforms based on players position and rotation
        glm::mat4 view_mat = glm::mat4(1.0);
        view_mat = glm::rotate(view_mat, -plr.pitch, glm::vec3(1.0, 0.0, 0.0));
        view_mat = glm::rotate(view_mat, -plr.yaw, glm::vec3(0.0, 1.0, 0.0));
        view_mat = glm::translate(view_mat, -plr.p);
        // matrix that transforms based on perspective of player
        glm::mat4 project_mat = glm::mat4(1.0);
        project_mat = glm::perspective(plr.fov / 2, (float)ww / wh, 0.1f, 100.0f);

        glUniformMatrix4fv(worldspace_program.uloc["view_mat"], 1, false, glm::value_ptr(view_mat));
        glUniformMatrix4fv(worldspace_program.uloc["project_mat"], 1, false, glm::value_ptr(project_mat));

        tex.bind(0);
        glUniform1i(worldspace_program.uloc["tex"], tex.loc);

        glClearColor(0.3f, 0.4f, 0.45f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        worldspace_program.use();
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
