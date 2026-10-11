#include <SDL.h>
#include <GLES2/gl2.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void sdlCheck(bool ok, const char* operation) {
    if (!ok) throw std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    if (!shader) throw std::runtime_error("glCreateShader failed");
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0, length = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length > 1) {
        std::vector<char> log(static_cast<size_t>(length));
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        std::fprintf(stderr, "%s shader log: %s\n",
                     type == GL_VERTEX_SHADER ? "Vertex" : "Fragment", log.data());
    }
    if (!ok) {
        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed");
    }
    return shader;
}

// Matching varying precision is explicit, including on ES 2.0 implementations.
constexpr const char* vertexSource = R"(#version 100
attribute vec2 a_position;
attribute vec3 a_color;
uniform vec2 u_rotation;
uniform float u_aspect;
varying mediump vec3 v_color;
void main() {
    vec2 p = vec2(u_rotation.x * a_position.x - u_rotation.y * a_position.y,
                  u_rotation.y * a_position.x + u_rotation.x * a_position.y);
    gl_Position = vec4(p.x * u_aspect, p.y, 0.0, 1.0);
    v_color = a_color;
}
)";
constexpr const char* fragmentSource = R"(#version 100
precision mediump float;
varying mediump vec3 v_color;
void main() {
    gl_FragColor = vec4(v_color, 1.0);
}
)";

struct Resources {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    GLuint vertex = 0, fragment = 0, program = 0, buffer = 0;
    std::map<SDL_JoystickID, SDL_GameController*> controllers;
    ~Resources() {
        for (auto& entry : controllers) SDL_GameControllerClose(entry.second);
        if (context) {
            if (buffer) glDeleteBuffers(1, &buffer);
            if (program) glDeleteProgram(program);
            if (vertex) glDeleteShader(vertex);
            if (fragment) glDeleteShader(fragment);
            SDL_GL_DeleteContext(context);
        }
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }
    void openController(int index) {
        const char* name = SDL_JoystickNameForIndex(index);
        std::printf("Input %d: %s; controller mapping: %s\n", index,
                    name ? name : "unknown", SDL_IsGameController(index) ? "yes" : "no");
        if (!SDL_IsGameController(index)) return;
        SDL_GameController* controller = SDL_GameControllerOpen(index);
        if (!controller) {
            std::fprintf(stderr, "SDL_GameControllerOpen(%d): %s\n", index, SDL_GetError());
            return;
        }
        SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
        if (!controllers.emplace(id, controller).second) SDL_GameControllerClose(controller);
        else std::printf("Controller opened: instance %d\n", id);
    }
};
} // namespace

int main(int argc, char** argv) {
    // Line buffered even when redirected by the Ports launcher.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    double runSeconds = 0.0;
    if (argc != 1) {
        if (argc != 3 || std::strcmp(argv[1], "--seconds") != 0) {
            std::fprintf(stderr, "Usage: %s [--seconds positive-number]\n", argv[0]);
            return 2;
        }
        char* end = nullptr;
        runSeconds = std::strtod(argv[2], &end);
        if (end == argv[2] || *end || !std::isfinite(runSeconds) || runSeconds <= 0) {
            std::fprintf(stderr, "Invalid duration: %s\n", argv[2]);
            return 2;
        }
    }
    Resources resources;
    try {
        bool hasMali = false;
        for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i) {
            const char* driver = SDL_GetVideoDriver(i);
            std::printf("SDL video driver %d: %s\n", i, driver);
            hasMali |= std::strcmp(driver, "mali") == 0;
        }
        if (!SDL_getenv("SDL_VIDEODRIVER") && hasMali)
            sdlCheck(SDL_setenv("SDL_VIDEODRIVER", "mali", 0) == 0, "SDL_setenv");
        const char* requested = SDL_getenv("SDL_VIDEODRIVER");
        std::printf("SDL_VIDEODRIVER: %s\n", requested ? requested : "automatic (desktop)");
        sdlCheck(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) == 0, "SDL_Init");
        std::printf("Active video driver: %s\n", SDL_GetCurrentVideoDriver());
        for (const auto& attribute : std::array<std::pair<SDL_GLattr, int>, 7>{{
                 {SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES},
                 {SDL_GL_CONTEXT_MAJOR_VERSION, 2}, {SDL_GL_CONTEXT_MINOR_VERSION, 0},
                 {SDL_GL_DOUBLEBUFFER, 1}, {SDL_GL_RED_SIZE, 8},
                 {SDL_GL_GREEN_SIZE, 8}, {SDL_GL_BLUE_SIZE, 8}}}) {
            sdlCheck(SDL_GL_SetAttribute(attribute.first, attribute.second) == 0,
                     "SDL_GL_SetAttribute");
        }
        resources.window = SDL_CreateWindow("Knulli GLES triangle", SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
        sdlCheck(resources.window != nullptr, "SDL_CreateWindow(640x480 fullscreen)");
        resources.context = SDL_GL_CreateContext(resources.window);
        sdlCheck(resources.context != nullptr, "SDL_GL_CreateContext(ES 2.0)");
        sdlCheck(SDL_GL_MakeCurrent(resources.window, resources.context) == 0, "SDL_GL_MakeCurrent");
        for (const auto& item : std::array<std::pair<GLenum, const char*>, 4>{{
                 {GL_VENDOR, "GL_VENDOR"}, {GL_RENDERER, "GL_RENDERER"},
                 {GL_VERSION, "GL_VERSION"}, {GL_SHADING_LANGUAGE_VERSION, "GL_SHADING_LANGUAGE_VERSION"}}}) {
            const GLubyte* value = glGetString(item.first);
            if (!value) throw std::runtime_error(std::string("glGetString failed: ") + item.second);
            std::printf("%s: %s\n", item.second, reinterpret_cast<const char*>(value));
        }
        int profile = 0, major = 0, minor = 0;
        sdlCheck(SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile) == 0, "Get GL profile");
        sdlCheck(SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major) == 0, "Get GL major");
        sdlCheck(SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor) == 0, "Get GL minor");
        std::printf("Actual SDL GL context: %d.%d profile=0x%x\n", major, minor, profile);
        if (profile != SDL_GL_CONTEXT_PROFILE_ES || major < 2)
            throw std::runtime_error("An OpenGL ES 2.0 or newer context is required");
        if (SDL_GL_SetSwapInterval(1) != 0)
            std::fprintf(stderr, "Vsync unavailable: %s; software limiter remains active\n", SDL_GetError());

        resources.vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
        resources.fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
        resources.program = glCreateProgram();
        if (!resources.program) throw std::runtime_error("glCreateProgram failed");
        glAttachShader(resources.program, resources.vertex);
        glAttachShader(resources.program, resources.fragment);
        glBindAttribLocation(resources.program, 0, "a_position");
        glBindAttribLocation(resources.program, 1, "a_color");
        glLinkProgram(resources.program);
        GLint linked = 0, length = 0;
        glGetProgramiv(resources.program, GL_LINK_STATUS, &linked);
        glGetProgramiv(resources.program, GL_INFO_LOG_LENGTH, &length);
        if (length > 1) {
            std::vector<char> log(static_cast<size_t>(length));
            glGetProgramInfoLog(resources.program, length, nullptr, log.data());
            std::fprintf(stderr, "Program link log: %s\n", log.data());
        }
        if (!linked) throw std::runtime_error("Shader program linking failed");
        std::puts("GLSL ES 1.00 shaders compiled and linked successfully");
        glUseProgram(resources.program);
        const GLint rotation = glGetUniformLocation(resources.program, "u_rotation");
        const GLint aspect = glGetUniformLocation(resources.program, "u_aspect");
        if (rotation < 0 || aspect < 0) throw std::runtime_error("Shader uniforms missing");
        constexpr GLfloat vertices[] = {
            0.0f, 0.7f, 1.0f, 0.0f, 0.0f,
            -0.606f, -0.35f, 0.0f, 1.0f, 0.0f,
            0.606f, -0.35f, 0.0f, 0.0f, 1.0f};
        glGenBuffers(1, &resources.buffer);
        glBindBuffer(GL_ARRAY_BUFFER, resources.buffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), nullptr);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
                              reinterpret_cast<const void*>(2 * sizeof(GLfloat)));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glClearColor(0.025f, 0.035f, 0.06f, 1.0f);
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("GL buffer/setup failed");

        const int joysticks = SDL_NumJoysticks();
        sdlCheck(joysticks >= 0, "SDL_NumJoysticks");
        std::printf("Available input devices: %d\n", joysticks);
        for (int i = 0; i < joysticks; ++i) resources.openController(i);
        std::puts("Exit: hold START + BACK/SELECT on one controller, or press Escape.");
        const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
        const auto now = [frequency]() { return SDL_GetPerformanceCounter() / frequency; };
        const double start = now();
        double reportStart = start, nextFrame = start;
        unsigned frames = 0;
        // Axis logs are limited to 5 Hz per axis and meaningful changes.
        std::map<std::pair<SDL_JoystickID, Uint8>, std::pair<double, int>> axisLogs;
        bool running = true;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_QUIT: running = false; break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
                    break;
                case SDL_CONTROLLERDEVICEADDED: resources.openController(event.cdevice.which); break;
                case SDL_CONTROLLERDEVICEREMOVED: {
                    const auto found = resources.controllers.find(event.cdevice.which);
                    if (found != resources.controllers.end()) {
                        SDL_GameControllerClose(found->second);
                        resources.controllers.erase(found);
                    }
                    for (auto it = axisLogs.begin(); it != axisLogs.end(); ) {
                        if (it->first.first == event.cdevice.which) it = axisLogs.erase(it);
                        else ++it;
                    }
                    std::printf("Controller removed: %d\n", event.cdevice.which);
                    break;
                }
                case SDL_CONTROLLERBUTTONDOWN:
                case SDL_CONTROLLERBUTTONUP: {
                    const char* name = SDL_GameControllerGetStringForButton(
                        static_cast<SDL_GameControllerButton>(event.cbutton.button));
                    std::printf("Controller %d button %s (%u): %s\n", event.cbutton.which,
                        name ? name : "unknown", event.cbutton.button,
                        event.type == SDL_CONTROLLERBUTTONDOWN ? "down" : "up");
                    break;
                }
                case SDL_CONTROLLERAXISMOTION: {
                    auto& last = axisLogs[{event.caxis.which, event.caxis.axis}];
                    const double time = now();
                    if (time - last.first >= 0.2 && std::abs(event.caxis.value - last.second) >= 2048) {
                        const char* name = SDL_GameControllerGetStringForAxis(
                            static_cast<SDL_GameControllerAxis>(event.caxis.axis));
                        std::printf("Controller %d axis %s: %d\n", event.caxis.which,
                            name ? name : "unknown", event.caxis.value);
                        last = {time, event.caxis.value};
                    }
                    break;
                }
                default: break;
                }
            }
            for (const auto& entry : resources.controllers) {
                if (SDL_GameControllerGetButton(entry.second, SDL_CONTROLLER_BUTTON_START) &&
                    SDL_GameControllerGetButton(entry.second, SDL_CONTROLLER_BUTTON_BACK)) running = false;
            }
            const double time = now();
            if (runSeconds > 0 && time - start >= runSeconds) running = false;
            if (!running) break;
            int width = 0, height = 0;
            SDL_GL_GetDrawableSize(resources.window, &width, &height);
            if (width <= 0 || height <= 0) throw std::runtime_error("Invalid drawable dimensions");
            glViewport(0, 0, width, height);
            glUniform1f(aspect, static_cast<float>(height) / width);
            const float angle = static_cast<float>(std::fmod((time - start) * 0.8, 6.283185307));
            glUniform2f(rotation, std::cos(angle), std::sin(angle));
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            const GLenum error = glGetError();
            if (error != GL_NO_ERROR) {
                std::fprintf(stderr, "Render GL error: 0x%x\n", error);
                throw std::runtime_error("Rendering failed");
            }
            SDL_GL_SwapWindow(resources.window);
            ++frames;
            const double afterSwap = now();
            if (afterSwap - reportStart >= 5.0) {
                std::printf("Average FPS (%.2fs): %.2f\n", afterSwap - reportStart,
                            frames / (afterSwap - reportStart));
                frames = 0;
                reportStart = afterSwap;
            }
            nextFrame += 1.0 / 60.0;
            const double remaining = nextFrame - now();
            if (remaining > 0) SDL_Delay(static_cast<Uint32>(std::ceil(remaining * 1000.0)));
            else if (remaining < -1.0 / 60.0) nextFrame = now();
        }
        std::puts("Clean exit");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Fatal: %s\n", error.what());
        return 1;
    }
}
