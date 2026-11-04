#include "Core/App.h"
#include "Game/AimMath.h"
#include "Core/Palette.h"
#include "Render/GL.h"
#include "Scene/TitleScene.h"
#include <algorithm>
#include <iostream>

App* App::s_app = nullptr;

namespace {
constexpr int kInitialWidth = 1200;
constexpr int kInitialHeight = 800;
constexpr int kFrameMs = 16;   // 약 60fps
}

int App::run(int argc, char** argv) {
    s_app = this;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(50, 50);
    glutInitWindowSize(kInitialWidth, kInitialHeight);
    glutCreateWindow("AimLab");
    // 창을 닫아도 프로그램을 바로 끝내지 않고 glutMainLoop가 돌아오게 해서 정리할 기회를 만든다
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW 초기화 실패\n";
        return 1;
    }
    if (!renderer_.init(kInitialWidth, kInitialHeight)) {
        std::cerr << "렌더러 초기화 실패 — 실행 파일 옆에 Shaders 폴더가 있는지 확인하세요\n";
        return 1;
    }
    input_.install(kInitialWidth, kInitialHeight);
    input_.setMouseLook(true);

    world_.joinPlayer(0);
    camera_.setPosition(aim::eyePosition());
    scenes_.change(std::make_unique<TitleScene>());

    glutDisplayFunc(displayCallback);
    glutReshapeFunc(reshapeCallback);
    glutCloseFunc(closeCallback);
    lastTime_ = std::chrono::steady_clock::now();
    glutTimerFunc(kFrameMs, timerCallback, 0);

    glutMainLoop();

    releaseGraphics();   // 보통은 closeCallback에서 이미 정리됨
    std::cout << "종료\n";
    return 0;
}

void App::quit() {
    quitting_ = true;
    glutLeaveMainLoop();
}

// ---- GLUT 콜백 ----

void App::displayCallback() { if (s_app) s_app->render(); }

void App::reshapeCallback(int w, int h) {
    if (!s_app) return;
    s_app->renderer_.resize(w, h);
    s_app->input_.onReshape(w, h);
}

void App::timerCallback(int) {
    if (!s_app || s_app->quitting_) return;
    s_app->frame();
    if (!s_app->quitting_) glutTimerFunc(kFrameMs, timerCallback, 0);
}

void App::closeCallback() {
    // 창이 사라지기 직전 (GL 컨텍스트가 아직 살아 있을 때) GPU 자원을 정리한다
    if (s_app) {
        s_app->quitting_ = true;
        s_app->releaseGraphics();
    }
}

void App::releaseGraphics() {
    if (graphicsReleased_) return;
    graphicsReleased_ = true;
    renderer_.shutdown();
}

// ---- 한 프레임 ----

void App::frame() {
    const auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - lastTime_).count();
    lastTime_ = now;
    dt = std::min(dt, 0.1f);   // 창을 끌거나 멈췄다 돌아와도 한 번에 크게 튀지 않게

    input_.beginFrame();
    if (input_.keyPressed(27)) {   // Esc: 마우스 시점 켜기/끄기 (창 밖으로 마우스를 빼고 싶을 때)
        input_.setMouseLook(!input_.mouseLookEnabled());
        std::cout << "마우스 시점 " << (input_.mouseLookEnabled() ? "켜짐" : "꺼짐 (Esc로 다시 켜기)") << "\n";
    }

    world_.tick(dt);
    handleEvents();
    debris_.update(dt, cfg::kRoomFloorY);
    if (settings_.infoTimer > 0.0f) settings_.infoTimer -= dt;

    scenes_.applyPending(*this);
    if (Scene* scene = scenes_.current()) scene->update(*this, dt);

    input_.endFrame();
    glutPostRedisplay();
}

void App::handleEvents() {
    frameEvents_.clear();
    world_.drainEvents(frameEvents_);
    for (const GameEvent& e : frameEvents_) {
        switch (e.kind) {
        case EventKind::TargetHit:
        case EventKind::TargetKilled:
            debris_.targetBurst(e.pos, palette::kTarget);
            break;
        case EventKind::BossHit:
            debris_.burst(e.pos, palette::kBoss, 2, 0.2f, 2.0f);
            break;
        case EventKind::BossTeleport:
            debris_.burst(e.pos, palette::kBoss, 8, 0.35f, 3.0f);
            break;
        case EventKind::BossKilled:
            debris_.burst(e.pos, palette::kBoss, 24, 0.45f, 5.0f);
            break;
        case EventKind::BossShieldUp:
            debris_.burst(e.pos, palette::kShield, 12, 0.3f, 3.0f);
            break;
        case EventKind::BossShieldBroken:
            debris_.burst(e.pos, palette::kShield, 24, 0.4f, 5.0f);
            break;
        }
    }
}

void App::render() {
    if (graphicsReleased_) return;
    if (Scene* scene = scenes_.current()) {
        scene->render(*this);
    } else {
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    glutSwapBuffers();
}
