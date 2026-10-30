void Tick(const std::vector<BHPlayer>& players) {
    if (!cfg.dexBot) return;
    if (players.size() < 2) return;

    const BHPlayer& local = players[0];
    const BHPlayer& enemy = players[1];

    AttackState curr = enemy.atkState;
    float dx   = enemy.worldPos.x - local.worldPos.x;
    float dist = enemy.distToLocal;

    // ── 1. CHASE — toujours actif, indépendant de attacking ─────────
    // était bloqué par if(!attacking) → bot restait figé pendant cooldown
    if (dist > ATTACK_RANGE && dist < CHASE_RANGE) {
        WORD wantDir = dx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT;
        if (lastMoveDir != wantDir) {
            if (lastMoveDir) Input::ReleaseKey(lastMoveDir);
            Input::HoldKey(wantDir);
            lastMoveDir = wantDir;
        }
    } else {
        if (lastMoveDir) {
            Input::ReleaseKey(lastMoveDir);
            lastMoveDir = 0;
        }
    }

    // ── 2. VULNERABILITY DETECTION ──────────────────────────────────
    bool justFinished = (curr == AttackState::Idle) &&
                        (prevEnemyAtk != AttackState::Idle);
    bool inRecovery   = (curr == AttackState::Recovery);
    prevEnemyAtk = curr;

    // ── 3. ATTACK DECISION ──────────────────────────────────────────
    if (dist > ATTACK_RANGE) return;
    if (attacking)           return;

    bool doAttack = false;
    if (cfg.dexBotOnlyHit) {
        doAttack = justFinished || inRecovery;
    } else {
        doAttack = (curr == AttackState::Idle);
    }

    if (!doAttack) return;

    std::cout << "[DEXBOT] attack — dist=" << (int)dist
              << " punish=" << justFinished << "\n";

    if (lastMoveDir) {
        Input::ReleaseKey(lastMoveDir);
        lastMoveDir = 0;
    }

    WORD faceDir = std::abs(dx) > 30.f
        ? (dx > 0 ? Input::KEY_RIGHT : Input::KEY_LEFT)
        : 0;

    int delay = Input::Jitter(cfg.comboDelayMs, cfg.timingVarianceMs);
    FireAttack(faceDir, delay);
}
