#include "game.h"

#include <algorithm>
#include <cstdlib>

namespace buraco {
namespace {

// Bounded so a long headless run never grows the queue: nobody drains events
// in --auto/--check, and the oldest ones are irrelevant by then.
constexpr size_t kMaxSoundEvents = 64;

bool hasCanastra(const PlayerState& p) {
    for (const Meld& m : p.melds)
        if (!m.empty() && m.canastra()) return true;
    return false;
}

int canastraCount(const PlayerState& p) {
    int n = 0;
    for (const Meld& m : p.melds)
        if (!m.empty() && m.canastra()) ++n;
    return n;
}

void removeCard(std::vector<Card>& hand, const Card& c) {
    for (auto it = hand.begin(); it != hand.end(); ++it) {
        if (it->id == c.id) {
            hand.erase(it);
            return;
        }
    }
}

}  // namespace

Game::Game() {
    players_[0].name = "Voce";
    players_[1].name = "Eu";
}

uint32_t Game::rnd() {
    // xorshift32 – deterministic per match, plenty for shuffling 108 cards.
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
}

void Game::shuffle(std::vector<Card>& deck) {
    for (size_t i = deck.size(); i > 1; --i) {
        size_t j = rnd() % i;
        std::swap(deck[i - 1], deck[j]);
    }
}

void Game::newMatch(const std::string& playerName, uint32_t seed,
                    const std::string& opponentName) {
    if (!playerName.empty()) players_[0].name = playerName;
    if (!opponentName.empty()) players_[1].name = opponentName;
    rng_ = seed ? seed : 0x9E3779B9u;
    for (PlayerState& p : players_) {
        p.total = 0;
        p.lastHandPoints = 0;
    }
    handNumber_ = 0;
    handsPlayed_ = 0;
    wins_[0] = wins_[1] = 0;
    newHand();
}

void Game::newHand() {
    for (PlayerState& p : players_) {
        p.hand.clear();
        p.melds.clear();
        p.tookMorto = false;
        p.wentOut = false;
        // lastHandPoints is intentionally kept: F3 reports the score of the
        // previous hand even after the next one has already been dealt.
    }
    monte_.clear();
    discard_.clear();
    morto_.clear();
    lastDrawn_ = Card{};

    std::vector<Card> deck;
    deck.reserve(DECK_SIZE);
    for (uint8_t id = 1; id <= DECK_SIZE; ++id) deck.push_back(makeCard(id));
    shuffle(deck);
    emit(sound::Event::Shuffle);

    for (int i = 0; i < HAND_SIZE; ++i) {
        players_[0].hand.push_back(deck.back());
        deck.pop_back();
        players_[1].hand.push_back(deck.back());
        deck.pop_back();
    }
    for (int i = 0; i < MORTO_SIZE; ++i) {
        morto_.push_back(deck.back());
        deck.pop_back();
    }
    monte_ = deck;
    emit(sound::Event::Deal);

    ++handNumber_;
    lastWentOutSeat_ = -1;
    lastWentOutByMeld_ = false;
    current_ = (handNumber_ - 1) % 2;
    prompt_ = Prompt::None;
    setPhase(Phase::Draw);
    message_ = "Embaralhando. Por favor, corte...";
    timer_ = 0.0;
    beginTurn();
}

void Game::setPhase(Phase p) {
    phase_ = p;
    timer_ = 0.0;
}

void Game::info(const std::string& m) { message_ = m; }

void Game::emit(sound::Event e) {
    if (events_.size() >= kMaxSoundEvents) events_.erase(events_.begin());
    events_.push_back(e);
}

std::vector<sound::Event> Game::takeEvents() {
    std::vector<sound::Event> out;
    out.swap(events_);
    return out;
}

bool Game::canTakeDiscard() const { return !discard_.empty(); }

void Game::requestDiscard() {
    if (phase_ != Phase::Play) return;
    setPhase(Phase::Discard);
    info("Escolha a carta a descartar.");
}

bool Game::draw(bool fromTable) {
    if (phase_ != Phase::Draw) return false;
    PlayerState& p = players_[current_];

    if (fromTable) {
        if (discard_.empty()) return false;
        // Buraco: buying the table takes the whole discard pile, not just the
        // top card.  Keeping the vector order leaves the top card at the end
        // of the hand, which is also what lastDrawn_ points at below.
        p.hand.insert(p.hand.end(), discard_.begin(), discard_.end());
        discard_.clear();
        info(isHuman(current_) ? "Voce comprou a mesa." : "Estou comprando a mesa.");
        emit(sound::Event::TakePile);
    } else {
        if (monte_.empty()) {
            // The original's deck is a 108-slot array and FUN_1000_2a03 returns
            // 0 when every slot is empty: the draw simply gives no card, the
            // hand shrinks by one on the discard and the deal finally ends.
            info("O monte acabou.");
            prompt_ = Prompt::None;
            setPhase(Phase::Play);
            return false;
        }
        p.hand.push_back(monte_.back());
        monte_.pop_back();
        info(isHuman(current_) ? "Voce comprou do monte." : "Estou comprando do monte.");
        emit(sound::Event::DrawDeck);
    }
    prompt_ = Prompt::None;
    setPhase(Phase::Play);
    lastDrawn_ = p.hand.back();
    return true;
}

Game::LayOptions Game::layOptions(const std::vector<Card>& cards) const {
    LayOptions o;
    if (phase_ != Phase::Play || cards.empty()) return o;

    const PlayerState& p = players_[current_];
    Meld built;
    o.isMeld = buildMeldAuto(cards, built);

    const bool wouldEmpty = cards.size() == p.hand.size();
    const bool blocked = wouldEmpty && p.tookMorto && !hasCanastra(p);
    o.canCreate = o.isMeld && int(p.melds.size()) < kMaxMelds && !blocked;

    // Extending is legal with any number of cards; only a brand-new meld needs
    // three.  This is what lets the player drop a single card on an existing
    // run, as in the original.
    for (size_t i = 0; i < p.melds.size(); ++i) {
        if (p.melds[i].empty()) continue;
        Meld probe = p.melds[i];
        if (extendMeld(probe, cards)) o.targets.push_back(int(i));
    }
    return o;
}

std::string Game::layMeld(const std::vector<Card>& cards, int target) {
    if (phase_ != Phase::Play) return "Agora nao e hora de baixar jogos.";
    if (cards.empty()) return "Escolha pelo menos uma carta.";

    PlayerState& p = players_[current_];
    const bool wouldEmpty = cards.size() == p.hand.size();
    if (wouldEmpty && p.tookMorto && !hasCanastra(p))
        return "Voce nao pode bater porque nao tem canastra.";

    // A completion is audible only when a meld crossed the canastra mark now;
    // growing an existing canastra stays a plain meld sound.
    const int canastraBefore = canastraCount(p);

    auto takeFromHand = [&p, &cards]() {
        for (const Card& c : cards) removeCard(p.hand, c);
    };
    auto finish = [&]() {
        if (p.hand.empty()) afterEmptyHand(true);
        return std::string();
    };
    // All successful paths remove the cards, report and then check for an empty
    // hand (which may take the morto or score the hand).
    auto commit = [&](const char* msg) {
        takeFromHand();
        info(msg);
        emit(sound::Event::LayMeld);
        if (canastraCount(p) > canastraBefore) emit(sound::Event::Canastra);
        return finish();
    };

    // The player explicitly asked for a second meld of a suit that is already
    // on the table.
    if (target == kNewMeld) {
        if (cards.size() < size_t(MELD_MIN)) return "Um jogo novo precisa de pelo menos 3 cartas.";
        Meld built;
        if (!buildMeldAuto(cards, built)) return "Estas cartas nao formam um jogo valido.";
        if (int(p.melds.size()) >= kMaxMelds) return "Nao cabem mais jogos na mesa.";
        p.melds.push_back(built);
        return commit("Jogo baixado.");
    }

    // The player explicitly picked a meld to grow.
    if (target >= 0) {
        if (target >= int(p.melds.size()) || p.melds[target].empty())
            return "Jogo invalido.";
        if (!extendMeld(p.melds[target], cards)) return "Estas cartas nao entram neste jogo.";
        return commit("Jogo aumentado.");
    }

    // Automatic mode (AI and scripted play): extend when possible, otherwise
    // lay the cards as a new meld.
    Meld built;
    if (buildMeldAuto(cards, built)) {
        for (Meld& m : p.melds) {
            if (m.empty() || m.suit != built.suit) continue;
            if (!extendMeld(m, cards)) continue;
            return commit("Jogo aumentado.");
        }
        if (int(p.melds.size()) >= kMaxMelds) return "Nao cabem mais jogos na mesa.";
        p.melds.push_back(built);
        return commit("Jogo baixado.");
    }

    for (Meld& m : p.melds) {
        if (m.empty()) continue;
        if (!extendMeld(m, cards)) continue;
        return commit("Jogo aumentado.");
    }
    return "Estas cartas nao formam um jogo valido.";
}

void Game::afterEmptyHand(bool byMeld) {
    PlayerState& p = players_[current_];
    if (!p.hand.empty()) return;
    // Emptying the hand without having taken the morto is only possible while
    // there are still cards in it; otherwise the player is out.
    if (takeMorto()) {
        setPhase(Phase::Discard);
    } else {
        p.tookMorto = true;
        p.wentOut = true;
        lastWentOutSeat_ = current_;
        lastWentOutByMeld_ = byMeld;
        endHand();
    }
}

// Hands the first half of the morto to the current player.  Returns false when
// the morto was already taken or there is nothing left in it.
bool Game::takeMorto() {
    PlayerState& p = players_[current_];
    if (p.tookMorto || morto_.empty()) return false;
    for (int i = 0; i < MORTO_HALF && !morto_.empty(); ++i) {
        p.hand.push_back(morto_.back());
        morto_.pop_back();
    }
    p.tookMorto = true;
    info("Voce pegou o morto.");
    emit(sound::Event::Morto);
    return true;
}

bool Game::discardCard(const Card& card) {
    if (phase_ != Phase::Discard) return false;
    PlayerState& p = players_[current_];
    auto it = std::find_if(p.hand.begin(), p.hand.end(),
                           [&](const Card& c) { return c.id == card.id; });
    if (it == p.hand.end()) return false;

    p.hand.erase(it);
    discard_.push_back(card);
    emit(sound::Event::Discard);

    if (p.hand.empty()) {
        if (takeMorto()) {
            nextTurn();
        } else {
            p.tookMorto = true;
            p.wentOut = true;
            lastWentOutSeat_ = current_;
            lastWentOutByMeld_ = false;
            endHand();
        }
        return true;
    }

    if (isHuman(current_)) info("Sua vez acabou.");
    nextTurn();
    return true;
}

void Game::nextTurn() {
    current_ ^= 1;
    beginTurn();
}

void Game::beginTurn() {
    setPhase(Phase::Draw);
    prompt_ = Prompt::None;
    lastDrawn_ = Card{};
    if (isHuman(current_)) {
        if (canTakeDiscard())
            info("Comprar a mesa? (S/N)");
        else
            info("Compre uma carta do monte. <ENTER>");
    } else {
        info("Agora e minha vez.");
    }
}

void Game::endHand() {
    emit(sound::Event::Batida);
    // Cards left in the morto are not added to any hand: the original only
    // charges the flat 100 point penalty for each player who never took it.
    morto_.clear();

    const int bonusSeat = lastWentOutSeat_;
    // Batida is a flat +100 for whoever went out, whether by laying every card
    // as a meld or by discarding the last one.
    const int bonus = 100;

    for (int seat = 0; seat < 2; ++seat) {
        PlayerState& p = players_[seat];
        int pts = 0;
        for (const Meld& m : p.melds) pts += m.value();
        int handPts = 0;
        for (const Card& c : p.hand) handPts += cardPoints(c);
        if (!p.tookMorto) handPts += 100;  // "perdeu 100 pontos pelo morto"
        pts -= handPts;
        if (seat == bonusSeat) pts += bonus;
        p.lastHandPoints = pts;
        p.total += pts;
    }

    ++handsPlayed_;
    if (bonusSeat == 0 || bonusSeat == 1) ++wins_[bonusSeat];

    prompt_ = Prompt::NewHand;
    setPhase(Phase::HandOver);
    message_ = "Vamos contar os pontos.";
}

void Game::continueAfterHand() {
    if (players_[0].total >= kTargetScore || players_[1].total >= kTargetScore) {
        setPhase(Phase::MatchOver);
        message_ = "A partida acabou !!!";
        return;
    }
    newHand();
}

void Game::answerPrompt(bool yes) {
    if (prompt_ == Prompt::TakeDiscard) {
        prompt_ = Prompt::None;
        if (yes) draw(true);
        else draw(false);
        return;
    }
    if (prompt_ == Prompt::NewHand) {
        prompt_ = Prompt::None;
        continueAfterHand();
    }
}

void Game::update(double dt) {
    if (phase_ == Phase::HandOver || phase_ == Phase::MatchOver) return;
    // Only the AI plays on its own; hot-seat seats and remotes wait for input.
    if (!isAi(current_)) return;

    timer_ += dt;
    if (timer_ < 0.7) return;
    autoStep();
}

void Game::announceTurn() {
    if (phase_ == Phase::HandOver || phase_ == Phase::MatchOver) return;
    if (!isHuman(current_)) {
        message_ = "Agora e minha vez.";
        return;
    }
    switch (phase_) {
        case Phase::Draw:
            message_ = canTakeDiscard() ? "Comprar a mesa (S/N) ?"
                                        : "Compre uma carta do monte. <ENTER>";
            break;
        case Phase::Play:
            message_ = "Baixe os jogos.";
            break;
        case Phase::Discard:
            message_ = "Escolha a carta a descartar.";
            break;
        default:
            break;
    }
}

// One phase of an automated turn (draw, lay melds, or discard).
void Game::autoStep() {
    if (phase_ == Phase::HandOver || phase_ == Phase::MatchOver) return;
    switch (phase_) {
        case Phase::Draw:
            aiTurn();
            break;
        case Phase::Play:
            aiLayMelds();
            // layMeld/afterEmptyHand may already have ended the hand (batida)
            // or moved straight to the discard after taking the morto.
            if (phase_ == Phase::Play) setPhase(Phase::Discard);
            break;
        case Phase::Discard:
            aiDiscard();
            break;
        default:
            break;
    }
}

void Game::aiTurn() {
    // Take the table card only when it can be used right away: it extends an
    // existing meld or completes a three-card meld with two cards in hand.
    // Otherwise draw from the monte, which is what actually advances the deal.
    const bool takeTable = !discard_.empty() && canUseTableCard(discard_.back());
    draw(takeTable);
}

bool Game::canUseTableCard(const Card& top) const {
    const PlayerState& me = players_[current_];
    for (const Meld& m : me.melds) {
        Meld probe = m;
        if (extendMeld(probe, {top})) return true;
    }

    const bool room = int(me.melds.size()) < kMaxMelds;
    for (size_t i = 0; i < me.hand.size(); ++i) {
        for (size_t j = i + 1; j < me.hand.size(); ++j) {
            std::vector<Card> sel{top, me.hand[i], me.hand[j]};
            Meld probe;
            if (!buildMeldAuto(sel, probe)) continue;
            if (room) return true;
            // When every meld slot is taken layMeld can only grow an existing
            // one; without this check the AI kept buying a table it could not
            // actually use, so the hand never ended.
            for (const Meld& m : me.melds) {
                if (m.empty() || m.suit != probe.suit) continue;
                Meld grow = m;
                if (extendMeld(grow, sel)) return true;
            }
        }
    }
    return false;
}

std::vector<Card> Game::aiFindMeld() const {
    const PlayerState& me = players_[current_];
    std::vector<Card> best;
    int bestScore = -1;

    for (uint8_t suit = 1; suit <= 4; ++suit) {
        bool already = false;
        for (const Meld& m : me.melds)
            if (!m.empty() && m.suit == suit) already = true;
        if (already) continue;

        std::vector<Card> natural;   // natural cards of this suit
        std::vector<Card> wildPool;  // 2s and jokers
        for (const Card& c : me.hand) {
            if (isWild(c)) wildPool.push_back(c);
            else if (c.suit == suit) natural.push_back(c);
        }
        std::sort(natural.begin(), natural.end(),
                  [](const Card& a, const Card& b) { return a.rank < b.rank; });

        for (int variant = 0; variant < 2; ++variant) {
            // variant 0: ace low, variant 1: ace high (after the king)
            std::vector<Card> pool;
            for (const Card& c : natural) pool.push_back(c);
            if (variant == 1) {
                bool kingish = false;
                for (const Card& c : pool)
                    if (c.rank == QUEEN || c.rank == KING) kingish = true;
                if (!kingish) continue;
                for (Card& c : pool)
                    if (c.rank == ACE) c.rank = ACE_HIGH;
            }
            std::sort(pool.begin(), pool.end(),
                      [](const Card& a, const Card& b) { return a.rank < b.rank; });

            for (size_t i = 0; i < pool.size(); ++i) {
                std::vector<Card> run{pool[i]};
                int wildUsed = 0;
                for (size_t j = i + 1; j < pool.size(); ++j) {
                    const int gap = int(pool[j].rank) - int(run.back().rank) - 1;
                    if (gap < 0) continue;
                    if (gap == 0) {
                        run.push_back(pool[j]);
                    } else if (wildUsed + gap <= int(wildPool.size())) {
                        wildUsed += gap;
                        run.push_back(pool[j]);
                    } else {
                        break;
                    }
                }
                const int total = int(run.size()) + wildUsed;
                if (total < MELD_MIN) continue;

                std::vector<Card> chosen;
                for (const Card& c : run) {
                    // The ace-high variant borrows rank 14 while sorting and
                    // measuring gaps.  Never let that synthetic rank reach the
                    // meld: the save file stores only card ids, so a reloaded
                    // match would rebuild the ace as rank 1 and the AI would
                    // take different decisions (see --savetest).
                    chosen.push_back(c.rank == ACE_HIGH ? makeCard(c.id) : c);
                }
                for (int w = 0; w < wildUsed; ++w) chosen.push_back(wildPool[w]);

                int score = 0;
                for (const Card& c : chosen) score += cardPoints(c) + 1;
                if (score > bestScore) {
                    bestScore = score;
                    best = chosen;
                }
            }
        }
    }
    return best;
}

bool Game::aiLayMelds() {
    bool laid = false;
    PlayerState& me = players_[current_];

    // Play the card just drawn first.  When it came from the table this is what
    // keeps the deal moving: a purchase always turns into a meld instead of
    // bouncing between the two hands forever.
    if (lastDrawn_.valid()) {
        const Card lead = lastDrawn_;
        lastDrawn_ = Card{};
        for (Meld& m : me.melds) {
            if (m.empty()) continue;
            if (extendMeld(m, {lead})) {
                removeCard(me.hand, lead);
                laid = true;
                if (me.hand.empty()) {
                    afterEmptyHand(true);
                    return laid;
                }
                break;
            }
        }
        if (!laid) {
            for (size_t i = 0; i < me.hand.size() && !laid; ++i) {
                for (size_t j = i + 1; j < me.hand.size(); ++j) {
                    std::vector<Card> sel{lead, me.hand[i], me.hand[j]};
                    Meld probe;
                    if (!buildMeldAuto(sel, probe)) continue;
                    if (layMeld(sel).empty()) {
                        laid = true;
                        break;
                    }
                    // No free meld slot accepts these cards: try another pair instead.
                }
            }
        }
    }

    for (int guard = 0; guard < 8; ++guard) {
        std::vector<Card> meld = aiFindMeld();
        if (meld.empty()) break;
        if (!layMeld(meld).empty()) break;
        laid = true;
    }
    // Try to grow existing melds with single cards.
    for (int guard = 0; guard < 12; ++guard) {
        bool grew = false;
        for (Meld& m : me.melds) {
            if (m.empty()) continue;
            for (const Card& c : me.hand) {
                if (extendMeld(m, {c})) {
                    removeCard(me.hand, c);
                    grew = true;
                    laid = true;
                    if (me.hand.empty()) {
                        afterEmptyHand(true);
                        return laid;
                    }
                    break;
                }
            }
            if (grew) break;
        }
        if (!grew) break;
    }
    return laid;
}

void Game::aiDiscard() {
    PlayerState& me = players_[current_];
    if (me.hand.empty()) return;

    // Prefer to throw away the card least connected to the rest of the hand,
    // and among those the most expensive one.
    int bestIdx = 0;
    int bestScore = 1 << 30;
    for (size_t i = 0; i < me.hand.size(); ++i) {
        const Card& c = me.hand[i];
        int connections = 0;
        for (const Card& o : me.hand) {
            if (o.id == c.id) continue;
            if (o.suit == c.suit && std::abs(int(o.rank) - int(c.rank)) == 1) ++connections;
            if (isWild(o)) ++connections;
        }
        if (isWild(c)) connections += 4;  // keep wildcards
        const int score = connections * 100 - cardPoints(c);
        if (score < bestScore) {
            bestScore = score;
            bestIdx = int(i);
        }
    }
    const Card card = me.hand[bestIdx];
    info(isHuman(current_) ? "Voce descartou." : "Eu vou descartar.");
    discardCard(card);
}

}  // namespace buraco

