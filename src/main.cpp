
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
    float r;
    bool on_platform;

    Player(glm::vec3 pos) {
        p = pos;
        speed = 1.0;
        fov = PI/2;
        yaw = 0.0;
        pitch = 0.0;
        height = 1.0;
        r = 0.2;
        on_platform = false;
    }

};

// Vertical rectangle that can collide and stuff
struct Wall {

    glm::vec2 p1, p2;
    float y_lo, y_hi;

    glm::vec2 normal;

    Wall(glm::vec2 p1, glm::vec2 p2, float y_lo, float y_hi): p1(p1), p2(p2), y_lo(y_lo), y_hi(y_hi) {
        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        normal = glm::normalize(glm::vec2(-dy, dx));
    }

};

// Horizontal polygon that can collide and stuff
struct Platform {

    float y;
    vector<glm::vec2> polygon_vertices;

    Platform(float y, vector<glm::vec2> polygon_vertices) : y(y), polygon_vertices(polygon_vertices) {}

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

bool lines_intersect(glm::vec2 p1, glm::vec2 q1, glm::vec2 p2, glm::vec2 q2) {

    if (p1.x > q1.x) {
        swap(p1, q1);
    }
    if (p2.x > q2.x) {
        swap(p2, q2);
    }

    if (p1.x != q1.x && p2.x != q2.x) {

        float a1 = (q1.y - p1.y) / (q1.x - p1.x);
        float b1 = p1.y - a1 * p1.x;

        float a2 = (q2.y - p2.y) / (q2.x - p2.x);
        float b2 = p2.y - a2 * p2.x;

        if (a1 == a2) {
            if (b1 == b2) {
                return q1.x >= p2.x;
            } else {
                return false;
            }
        }

        // a1 * x + b1 = a2 * x + b2
        // (a1 - a2) * x = (b2 - b1)
        // x = (b2 - b1) / (a1 - a2)

        float t = (b2 - b1) / (a1 - a2);

        return t >= p1.x && t <= q1.x && t >= p2.x && t <= q2.x;

    } else {

        if (p1.x == q1.x) {
            swap(p1, p2);
            swap(q1, q2);
        }

        if (p1.x == q1.x) {
            return min(p1.y, q1.y) <= max(p2.y, q2.y);
        }

        if (q1.x < p2.x || p1.x > q2.x) {
            return false;
        }

        float a1 = (q1.y - p1.y) / (q1.x - p1.x);
        float b1 = p1.y - a1 * p1.x;

        float y = a1 * p2.x + b1;

        return y >= min(p2.y, q2.y) && y <= max(p2.y, q2.y);

    }

}


void wall_to_mesh(vector<float>& vertices, Wall wall) {

    vertices.insert(vertices.end(), {
        
        wall.p1.x, wall.y_lo, wall.p1.y,
        wall.p2.x, wall.y_lo, wall.p2.y,
        wall.p2.x, wall.y_hi, wall.p2.y,

        wall.p1.x, wall.y_lo, wall.p1.y,
        wall.p1.x, wall.y_hi, wall.p1.y,
        wall.p2.x, wall.y_hi, wall.p2.y,

    });

}

void platform_to_mesh(vector<float>& vertices, Platform platform) {

    // i is starting vertex
    for (int i = 0; i < vertices.size(); i++) {

        bool possible = true;

        vector<pair<int, int>> added_edges = {};



    }

    // vector<glm::vec2> vertices_copy = platform.polygon_vertices;
    // vector<bool> has_triangle(vertices.size());

    // vector<pair<glm::vec2, glm::vec2>> point_pairs = {};
    // for (int i = 0; i < vertices.size(); i++) {
    //     point_pairs.push_back({vertices[i], vertices[(i+1) % vertices.size()]});
    // }

    // for (int i = 0; i < vertices.size(); i++) {
        
    // }

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

    Tex2D tex("../textures/bricks.png");

    vector<float> vertices = {
        // -1.0f, -1.0f, 1.0f,
        // 1.0f, -1.0f, 1.0f,
        // 1.0f, 1.0f, 1.0f,
        // -1.0f, -1.0f, 1.0f,
        // -1.0f, 1.0f, 1.0f,
        // 1.0f, 1.0f, 1.0f,
    };

    

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);


    // player and level stuff
    float g = 2.0;
    float max_v = 5.0f;
    Player plr(glm::vec3(0.0, 0.0, 0.0));

    vector<Platform> platforms;
    platforms.push_back(Platform(-1.0f, {{-1.0, -1.0}, {1.0, -1.0}, {1.0, 1.0}, {-1.0, 1.0}}));

    vector<Wall> walls;
    walls.push_back(Wall({-1.0, 1.0}, {1.0, 1.0}, -1.0, 1.0));
    walls.push_back(Wall({-1.0, 1.0}, {-1.0, -1.0}, -1.0, 1.0));

    for (Wall wall : walls) {
        wall_to_mesh(vertices, wall);
    }


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

        plr.v.x = 0.0;
        plr.v.z = 0.0;
        if (!plr.on_platform) {
            plr.v.y -= g * dt;
        }
        plr.v.y = max(plr.v.y, -max_v);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) {
            break;
        }
        if (glfwGetKey(window, GLFW_KEY_W)) {
            plr.v.z -= cos(plr.yaw) * plr.speed;
            plr.v.x -= sin(plr.yaw) * plr.speed;
        }
        if (glfwGetKey(window, GLFW_KEY_S)) {
            plr.v.z += cos(plr.yaw) * plr.speed;
            plr.v.x += sin(plr.yaw) * plr.speed;
        }
        if (glfwGetKey(window, GLFW_KEY_D)) {
            plr.v.z += -sin(plr.yaw) * plr.speed;
            plr.v.x += cos(plr.yaw) * plr.speed;
        }
        if (glfwGetKey(window, GLFW_KEY_A)) {
            plr.v.z -= -sin(plr.yaw) * plr.speed;
            plr.v.x -= cos(plr.yaw) * plr.speed;
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE)) {
            if (plr.on_platform) {
               plr.v.y = 1.0;
               plr.on_platform = false;
            }
            // plr.v.y += dt * plr.speed;
        }
        // if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
        //     plr.v.y -= dt * plr.speed;
        // }

        plr.p += plr.v * dt;

        for (Platform platform : platforms) {

            float y1 = plr.p.y - plr.v.y * dt;
            float y2 = plr.p.y;

            if (platform.y >= min(y1, y2) - plr.height / 2 && platform.y <= max(y1, y2) + plr.height / 2) {

                if (point_in_polygon(glm::vec2(plr.p.x, plr.p.z), platform.polygon_vertices)) {
                    if (plr.v.y > 0 && y1 + plr.height / 2 <= platform.y && y2 + plr.height / 2 >= platform.y) {
                        plr.v.y = 0.0;
                        plr.on_platform = true;
                        plr.p.y = platform.y - plr.height / 2;
                    } else if (plr.v.y < 0 && y1 - plr.height / 2 >= platform.y && y2 - plr.height / 2 <= platform.y) {
                        plr.v.y = 0.0;                        
                        plr.on_platform = true;
                        plr.p.y = platform.y + plr.height / 2;
                    }
                }

            }

        }

        if (!(plr.v.x == 0 && plr.v.z == 0)) {

            for (Wall wall : walls) {

                if (plr.p.y - plr.height / 2 > wall.y_hi || plr.p.y + plr.height / 2 < wall.y_lo) {
                    continue;
                }

                // float orientation;
                // if (glm::length(glm::vec2(plr.p.x, plr.p.z) - (wall.p1 + wall.normal)) > glm::length(glm::vec2(plr.p.x, plr.p.z) - (wall.p1 - wall.normal))) {
                //     orientation = 1.0;
                // } else {
                //     orientation = -1.0;
                // }

                float orientation = glm::dot(glm::vec2(plr.v.x, plr.v.z), wall.normal);
                orientation /= abs(orientation);

                glm::vec2 p1 = wall.p1 - wall.normal * plr.r * orientation;
                glm::vec2 q1 = wall.p2 - wall.normal * plr.r * orientation;

                glm::vec2 p2 = glm::vec2(plr.p.x - plr.v.x * dt, plr.p.z - plr.v.z * dt);
                glm::vec2 q2 = glm::vec2(plr.p.x, plr.p.z);

                if (lines_intersect(p1, q1, p2, q2)) {

                    float a = wall.normal.x;
                    float b = wall.normal.y;
                    float c = -a * p1.x - b * p1.y;

                    float d = abs(a * plr.p.x + b * plr.p.z + c) / sqrt(a*a + b*b);

                    plr.p.x -= (d+0.00001) * wall.normal.x * orientation;
                    plr.p.z -= (d+0.00001) * wall.normal.y * orientation;

                } else {

                    // glm::vec2 v1 = wall.p1 - glm::vec2(plr.p.x, plr.p.z);
                    // glm::vec2 v2 = wall.p2 - glm::vec2(plr.p.x, plr.p.z);

                    // float d1 = glm::length(v1);
                    // float d2 = glm::length(v2);

                    // if (d1 < plr.r) {
                    //     glm::vec2 displace = glm::normalize(v1) * (plr.r - d1 + 0.001f);
                    //     plr.p.x -= displace.x;
                    //     plr.p.z -= displace.y;
                    // }
                    // if (d2 < plr.r) {
                    //     glm::vec2 displace = glm::normalize(v2) * (plr.r - d2 + 0.001f);
                    //     plr.p.x -= displace.x;
                    //     plr.p.z -= displace.y;
                    // }

                }

            }

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
