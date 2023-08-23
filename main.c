#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

typedef struct _userdata {
    bool captured;
    double prev_xpos;
    double prev_ypos;
    vec3 pos;
    vec3 ang;
    vec3 vel;
    bool w;      // forward
    bool s;      // back
    bool a;      // moveleft
    bool d;      // moveright
    bool c;      // duck
    bool j;      // jump
    bool su;     // scroll up
    bool sd;     // scroll dn
    bool bPrevJ; // pressed jump
    bool bGround;
    bool bDucked;
    float flDuckAmount;
    vec3 jumpOff;
} userdata_t;

static void cbGlfwError(int error, const char *description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void cbGLDebug(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam) {
    fprintf(stderr, "GL Debug: %s\n", message);
}

static void setCapture(GLFWwindow *window, userdata_t *ud, bool capture) {
    if (ud->captured == capture)
        return;
    if ((ud->captured = capture)) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwGetCursorPos(window, &ud->prev_xpos, &ud->prev_ypos);
    } else
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    ud->w = false;
    ud->s = false;
    ud->a = false;
    ud->d = false;
    ud->c = false;
    ud->j = false;
    ud->su = false;
    ud->sd = false;
}

static void cbGLFWKey(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (action == GLFW_REPEAT)
        return;
    userdata_t *ud = (userdata_t *)glfwGetWindowUserPointer(window);
    bool pressed = action == GLFW_PRESS;
    switch (key) {
    case GLFW_KEY_W:
        ud->w = pressed && ud->captured;
        break;
    case GLFW_KEY_S:
        ud->s = pressed && ud->captured;
        break;
    case GLFW_KEY_A:
        ud->a = pressed && ud->captured;
        break;
    case GLFW_KEY_D:
        ud->d = pressed && ud->captured;
        break;
    case GLFW_KEY_LEFT_SHIFT:
        ud->c = pressed && ud->captured;
        break;
    case GLFW_KEY_SPACE:
        ud->j = pressed && ud->captured;
        break;
    case GLFW_KEY_ESCAPE:
        setCapture(window, ud, false);
        break;
    case GLFW_KEY_Q:
        setCapture(window, ud, true);
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        break;
    }
}

static void cbGLFWScr(GLFWwindow *window, double xoffset, double yoffset) {
    userdata_t *ud = (userdata_t *)glfwGetWindowUserPointer(window);
    if (ud->captured) {
        if (yoffset > 0)
            ud->su = true;
        else
            ud->sd = true;
    }
}

static void cbGLFWPos(GLFWwindow *window, double xpos, double ypos) {
    userdata_t *ud = (userdata_t *)glfwGetWindowUserPointer(window);
    if (!ud->captured)
        return;
    double xmove = xpos - ud->prev_xpos;
    double ymove = ypos - ud->prev_ypos;
    ud->prev_xpos = xpos;
    ud->prev_ypos = ypos;
    ud->ang[0] -= xmove * 0.022 * 4.5;
    ud->ang[1] += ymove * 0.022 * 4.5;
    if (ud->ang[0] >= 180.0)
        ud->ang[0] -= 360.0;
    if (ud->ang[0] < -180.0)
        ud->ang[0] += 360.0;
    ud->ang[1] = glm_clamp(ud->ang[1], -89.0, +89.0);
}

static void cbGLFWBtn(GLFWwindow *window, int button, int action, int mods) {
    if (action != GLFW_PRESS)
        return;
    userdata_t *ud = (userdata_t *)glfwGetWindowUserPointer(window);
    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
        setCapture(window, ud, true);
        break;
    case GLFW_MOUSE_BUTTON_RIGHT:
        setCapture(window, ud, false);
        break;
    }
}

static void cbGLFWFocus(GLFWwindow *window, int focused) {
    userdata_t *ud = (userdata_t *)glfwGetWindowUserPointer(window);
    if (focused == GLFW_FALSE)
        setCapture(window, ud, false);
}

static float vbo_data[] = {
    -1024.0f, -1024.0f, 0.0f, /* */ -1024.0f, -1024.0f, //
    +1024.0f, -1024.0f, 0.0f, /* */ +1024.0f, -1024.0f, //
    -1024.0f, +1024.0f, 0.0f, /* */ -1024.0f, +1024.0f, //
    -1024.0f, +1024.0f, 0.0f, /* */ -1024.0f, +1024.0f, //
    +1024.0f, -1024.0f, 0.0f, /* */ +1024.0f, -1024.0f, //
    +1024.0f, +1024.0f, 0.0f, /* */ +1024.0f, +1024.0f, //
};

static void angle_vectors(vec3 degs, float *f, float *s, float *u) {
    float y = glm_rad(degs[0]);
    float p = glm_rad(degs[1]);
    float r = glm_rad(degs[2]);
    float sr = sin(r), sp = sin(p), sy = sin(y);
    float cr = cos(r), cp = cos(p), cy = cos(y);
    if (f) {
        f[0] = cp * cy;
        f[1] = cp * sy;
        f[2] = -sp;
    }
    if (s) {
        s[0] = -sr * sp * cy + cr * sy;
        s[1] = -sr * sp * sy - cr * cy;
        s[2] = -sr * cp;
    }
    if (u) {
        u[0] = cr * sp * cy + sr * sy;
        u[1] = cr * sp * sy - sr * cy;
        u[2] = cr * cp;
    }
}

static void mv_friction(vec3 vel, float dt, float stopspeed, float friction, float surfaceFriction) {
    float spd = glm_vec3_norm(vel);
    if (spd < 0.1)
        return;
    float ctrl = GLM_MAX(stopspeed, spd);
    float drop = dt * ctrl * friction * surfaceFriction;
    glm_vec3_scale(vel, GLM_MAX(0.0, spd - drop) / spd, vel);
}

static void mv_accelerate(vec3 vel, float dt, vec3 wishdir, float wishspd, float accel, float surfaceFriction) {
    float speed = glm_vec3_dot(vel, wishdir);
    float acc_1 = GLM_MAX(0.0, wishspd - speed);
    float acc_2 = dt * wishspd * accel * surfaceFriction;
    glm_vec3_muladds(wishdir, GLM_MIN(acc_1, acc_2), vel);
}

static void mv_airaccelerate(vec3 vel, float dt, vec3 wishdir, float wishspd, float aircap, float accel, float surfaceFriction) {
    wishspd = glm_clamp(wishspd, 0, aircap);
    float speed = glm_vec3_dot(vel, wishdir);
    float acc_1 = GLM_MAX(0.0, wishspd - speed);
    float acc_2 = dt * wishspd * accel * surfaceFriction;
    glm_vec3_muladds(wishdir, GLM_MIN(acc_1, acc_2), vel);
}

static void calc_wishvel(userdata_t *ud, vec3 wishdir, float *wishspd,
                         float forwardspeed, float sidespeed, float duckmod, float maxspeed) {
    vec3 f, s;
    angle_vectors(ud->ang, f, s, NULL);
    f[2] = 0.0;
    s[2] = 0.0;
    glm_vec3_normalize(f);
    glm_vec3_normalize(s);
    glm_vec3_zero(wishdir);
    if (ud->w)
        glm_vec3_muladds(f, +forwardspeed, wishdir);
    if (ud->s)
        glm_vec3_muladds(f, -forwardspeed, wishdir);
    if (ud->a)
        glm_vec3_muladds(s, -sidespeed, wishdir);
    if (ud->d)
        glm_vec3_muladds(s, +sidespeed, wishdir);
    *wishspd = glm_vec3_norm(wishdir);
    glm_vec3_normalize(wishdir);
    *wishspd = glm_clamp(*wishspd, 0, maxspeed);
    if (ud->bDucked)
        *wishspd *= duckmod;
}

const float sv_gravity = 800.0;
const float sv_maxspeed = 250.0;
const float cl_sidespeed = 450.0;
const float cl_forwardspeed = 450.0;
const float sv_stopspeed = 100.0;
const float sv_friction = 4.0;
const float sv_accelerate = 5.0;
const float sv_airaccelerate = 100.0;
const float sv_jump_impulse = 301.993377;
const float duck_time = 0.125;

static void player_move(userdata_t *ud, float dt) {
    vec3 wishdir;
    float wishspd;
    calc_wishvel(ud, wishdir, &wishspd, cl_forwardspeed, cl_sidespeed, 0.34, sv_maxspeed);

    if (ud->bGround) {
        mv_friction(ud->vel, dt, sv_stopspeed, sv_friction, 1.0);
        mv_accelerate(ud->vel, dt, wishdir, wishspd, sv_accelerate, 1.0);
        ud->vel[2] = 0.0;
    } else {
        mv_airaccelerate(ud->vel, dt, wishdir, wishspd, 30.0, sv_airaccelerate, 1.0);
        ud->vel[2] -= sv_gravity * dt / 2;
    }
    {
        bool wantJump = ud->j || ud->sd;
        bool should_J = wantJump && !ud->bPrevJ;
        ud->bPrevJ = wantJump;
        ud->sd = false;
        if (ud->bGround && should_J) {
            fprintf(stderr, "prespeed: %.3f\n", glm_vec2_norm(ud->vel));
            glm_vec3_copy(ud->pos, ud->jumpOff);
            ud->vel[2] = sv_jump_impulse;
        }
    }
    {
        bool wantDuck = ud->c || ud->su;
        ud->su = false;
        if (wantDuck) {
            if (ud->bGround) {
                ud->flDuckAmount = glm_clamp(ud->flDuckAmount + dt / duck_time, 0.0, 1.0);
                if (ud->flDuckAmount == 1.0)
                    ud->bDucked = true;
            } else {
                if (!ud->bDucked) {
                    ud->pos[2] += 9.0;
                    ud->bDucked = true;
                    ud->flDuckAmount = 1.0;
                }
            }
        } else {
            if (ud->bGround) {
                ud->flDuckAmount = glm_clamp(ud->flDuckAmount - dt / duck_time, 0.0, 1.0);
                if (ud->flDuckAmount == 0.0)
                    ud->bDucked = false;
            } else if (ud->bDucked && ud->pos[2] >= 9.0) {
                ud->pos[2] -= 9.0;
                ud->bDucked = false;
                ud->flDuckAmount = 0.0;
            }
        }
    }
    bool prevGround = ud->bGround;
    ud->bGround = false;
    if (ud->pos[2] < 0.0)
        ud->pos[2] = 0.0;
    if (ud->pos[2] <= 0.1 && ud->vel[2] <= 0.0)
        ud->bGround = true;
    if (ud->vel[2] < 0.0) {
        float t = ud->pos[2] / -ud->vel[2];
        if (t <= dt) {
            ud->bGround = true;
            glm_vec3_muladds(ud->vel, t, ud->pos);
            dt -= t;
            ud->vel[2] = 0.0;
        }
    }
    glm_vec3_muladds(ud->vel, dt, ud->pos);
    if (!ud->bGround)
        ud->vel[2] -= sv_gravity * dt / 2;
    if (prevGround == false && ud->bGround) {
        vec2 dist;
        glm_vec2_sub(ud->pos, ud->jumpOff, dist);
        fprintf(stderr, "distance: %.3f\n\n", glm_vec2_norm(dist) + 32.0);
    }
}

static const char *vss =
    "#version 330 core\n"
    "uniform mat4 mvp;"
    "in vec3 vtxPos;"
    "in vec2 texPos;"
    "out vec2 texXY;"
    "void main() {"
    "  gl_Position = mvp * vec4(vtxPos, 1.0);"
    "  texXY = texPos;"
    "}";
static const char *fss =
    "#version 330 core\n"
    "#define DIVIDE 16.0\n"
    "in vec2 texXY;"
    "void main() {"
    "  int ix = int(floor(texXY.x / DIVIDE));"
    "  int iy = int(floor(texXY.y / DIVIDE));"
    "  if (ix % 2 == 0 && iy % 2 == 0)"
    "    gl_FragColor = vec4(vec3(ix % 4 < 2 ? 1.0 : 0.0,"
    "                             iy % 4 < 2 ? 1.0 : 0.0,"
    "                             1.0),"
    "                        1.0);"
    "  else"
    "    discard;"
    "}";

int main(void) {
    glfwSetErrorCallback(cbGlfwError);
    glfwInit();
    glfwWindowHint(GLFW_RESIZABLE, false);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow *window = glfwCreateWindow(1280, 720, "GL Game", NULL, NULL);
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    glfwMakeContextCurrent(window);
    glewExperimental = true;
    glewInit();
    glDebugMessageCallback(cbGLDebug, NULL);

    userdata_t ud;
    memset(&ud, 0, sizeof(ud));
    glfwSetWindowUserPointer(window, &ud);
    glfwSetKeyCallback(window, cbGLFWKey);
    glfwSetScrollCallback(window, cbGLFWScr);
    glfwSetCursorPosCallback(window, cbGLFWPos);
    glfwSetMouseButtonCallback(window, cbGLFWBtn);
    glfwSetWindowFocusCallback(window, cbGLFWFocus);

    GLuint sh = glCreateProgram();
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vs, 1, (const GLchar *const *)&vss, NULL);
    glShaderSource(fs, 1, (const GLchar *const *)&fss, NULL);
    glCompileShader(vs);
    glCompileShader(fs);
    glAttachShader(sh, vs);
    glAttachShader(sh, fs);
    glLinkProgram(sh);
    glDetachShader(sh, vs);
    glDetachShader(sh, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glUseProgram(sh);
    GLuint vao, vbo;
    glCreateVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vbo_data), &vbo_data, GL_STATIC_DRAW);
    GLuint locVtxPos = glGetAttribLocation(sh, "vtxPos");
    GLuint locTexPos = glGetAttribLocation(sh, "texPos");
    glEnableVertexAttribArray(locVtxPos);
    glVertexAttribPointer(locVtxPos, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(locTexPos);
    glVertexAttribPointer(locTexPos, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    GLuint locMVP = glGetUniformLocation(sh, "mvp");

    glfwSwapInterval(0);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    mat4 m_proj, m_view, m_mvp;
    vec3 v_eye, v_lookat;
    double prevTime = glfwGetTime();

    while (glfwWindowShouldClose(window) == 0) {
        double currTime = glfwGetTime();
        float dt = glm_clamp(currTime - prevTime, 0.0, 0.5);
        prevTime = currTime;
        glfwPollEvents();
        player_move(&ud, dt);
        float eye_height = 64 - ud.flDuckAmount * 18;
        glm_vec3_add(ud.pos, (vec3){0, 0, eye_height}, v_eye);
        angle_vectors(ud.ang, v_lookat, NULL, NULL);
        glm_vec3_add(v_eye, v_lookat, v_lookat);
        glm_perspective(glm_rad(45.0), 1280.0 / 720.0, 0.1, 4096.0, m_proj);
        glm_lookat(v_eye, v_lookat, GLM_ZUP, m_view);
        glm_mat4_mul(m_proj, m_view, m_mvp);
        glUniformMatrix4fv(locMVP, 1, GL_FALSE, &m_mvp[0][0]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}