#pragma once
// 게임 로직 전체 (시작 화면 → 3라운드 → 최종 결과 → 시작 화면).
//
// - 표적·보스의 움직임, 명중 판정, 점수, 라운드 진행을 모두 여기서 한다.
// - 씬은 입력(시선·클릭·누르고 있기)을 넘기고, 아래 «상태 조회» 함수로 상태를 읽어 그린다.
#include "GameConfig.h"
#include "GameTypes.h"
#include <glm/glm.hpp>
#include <random>
#include <vector>

class World {
public:
    struct PlayerInput {
        float yaw = 0.0f;
        float pitch = 0.0f;
        bool triggerHeld = false;
    };

    struct Target {
        glm::vec3 pos{ 0.0f };
        glm::vec3 vel{ 0.0f };
        float radius = cfg::kTargetRadius;
        float progress = 0.0f;    // Tracking: 연속 조준 시간(초)
    };

    struct Boss {
        bool alive = false;
        glm::vec3 pos{ 0.0f };
        glm::vec3 vel{ 0.0f };
        float radius = cfg::kBossRadius;
        int hp = 0;
        int maxHp = 0;
        float teleportTimer = 0.0f;
        bool shield = false;      // 보호막 중 (총알이 막힘, 좌클릭을 누른 채 따라가야 깨짐)
        float shieldHold = 0.0f;  // 보호막 게이지 (조준한 시간, 초)
        int shieldsDone = 0;      // 깨뜨린 보호막 수
    };

    World();

    int joinPlayer();                               // 첫 빈 자리 번호, 자리가 없으면 -1
    bool joinPlayer(int slot);                      // 지정한 빈 자리에 입장
    bool leavePlayer(int slot);

    // ---- 입력 ----
    void start();                                   // 시작 화면에서 게임 시작
    void setAim(float yawDeg, float pitchDeg);      // 지금 시선 (매 프레임)
    void fire();                                    // 클릭 한 번 (GridShot·Boss)
    void setTrigger(bool held);                     // 좌클릭을 누르고 있나 (Tracking·보호막)

    void setAim(int slot, float yawDeg, float pitchDeg);
    void setTrigger(int slot, bool held);

    // ---- 진행 ----
    void tick(float dt);                            // 매 프레임 호출
    void drainEvents(std::vector<GameEvent>& out);  // 이번 프레임에 일어난 사건 (파편 효과용)

    // ---- 상태 조회 (씬이 그릴 때 읽는다) ----
    Phase phase() const { return phase_; }
    float phaseTimeLeft() const;                    // 현재 단계의 남은 시간(초)
    int roundIndex() const { return roundIndex_; }  // 0~2
    RoundType round() const { return kRoundOrder[roundIndex_]; }
    const std::vector<Target>& targets() const { return targets_; }
    const Boss& boss() const { return boss_; }
    const PlayerInput* playerInput(int slot) const;
    int score() const { return score_; }                         // 전체 점수 (모든 라운드 합)
    int roundScore(int round) const { return roundScore_[round]; }
    int roundGoal() const { return roundGoal_; }                 // 통과 점수 (Boss는 0 = 처치가 조건)
    bool roundCleared() const;       // 진행 중: 이미 목표를 넘겼나 / 결과 단계: 이 라운드를 통과했나
    int failedRound() const { return failedRound_; }             // 통과하지 못한 라운드 (-1 = 없음)
    int trackingRemaining() const;   // Tracking 라운드 남은 표적 수
    float shieldProgress() const;    // 보호막 게이지 0~1 (보호막 중이 아니면 0)
    int shieldsLeft() const;         // 아직 남은 보호막 수
    bool shieldTracked() const { return shieldTracked_; }        // 지금 보호막 걸린 보스를 제대로 따라가고 있나

    // 라운드의 표적(보스)이 나오는 구역의 가운데. 라운드 시작 때 시점을 여기로 맞춘다.
    static glm::vec3 areaCenter(RoundType round);

private:
    struct Bounds {
        glm::vec3 min;
        glm::vec3 max;
    };

    // 단계 전환
    void enterPhase(Phase phase, float duration);
    void startMatch();
    void startRound(int index);
    void endRound();
    void resetToTitle();
    bool roundFinished() const;
    bool goalReached() const;    // 이번 라운드 통과 조건을 채웠나

    // 라운드별 생성·갱신
    void spawnGridShot();
    void spawnTracking();
    void spawnBoss();
    Target makeTrackingTarget();
    void updateTracking(float dt);
    void updateBoss(float dt);
    void hitBoss(const glm::vec3& point);
    void teleportBoss();
    int bossStage(int hp) const;
    void raiseShield();
    void breakShield();

    // 보조
    glm::vec3 aimDirection() const;              // 지금 시선 방향
    void credit();                               // 명중 기록: 점수 +1
    int pickTarget(const glm::vec3& origin, const glm::vec3& dir) const;  // 가장 가까운 표적
    glm::vec3 randomFreePosition(const Bounds& b, float minDistance, int ignoreIndex);
    glm::vec3 randomIn(const Bounds& b);
    float randf(float lo, float hi);
    void clearEntities();

    static Bounds gridBounds();
    static Bounds trackingBounds();
    static Bounds bossBounds();
    static void bounceInside(glm::vec3& pos, glm::vec3& vel, float radius, const Bounds& b);

    Phase phase_ = Phase::Title;
    float phaseTime_ = 0.0f;
    int roundIndex_ = 0;

    static constexpr int kMaxPlayers = 3;
    static constexpr int kLocalSlot = 0;
    static bool validSlot(int slot) { return slot >= 0 && slot < kMaxPlayers; }
    bool isPlayerJoined(int slot) const { return validSlot(slot) && playerJoined_[slot]; }
    PlayerInput playerInputs_[kMaxPlayers]{};
    bool playerJoined_[kMaxPlayers]{};

    std::vector<Target> targets_;
    Boss boss_;
    bool shieldTracked_ = false;

    int score_ = 0;
    int roundScore_[cfg::kRoundCount] = {};
    int roundGoal_ = 0;          // 이번 라운드 통과 점수
    bool lastRoundCleared_ = false;
    int failedRound_ = -1;       // 통과 못 한 라운드 (-1 = 없음)
    int trackKilled_ = 0;        // Tracking 라운드에서 처치한 수

    std::mt19937 rng_;
    std::vector<GameEvent> events_;
};
