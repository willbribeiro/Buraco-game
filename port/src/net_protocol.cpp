#include "net_protocol.h"

#include <iomanip>
#include <sstream>

#include "report.h"

namespace buraco::net {
namespace {

constexpr const char* kMagic = "BURACO-NET";

bool fail(std::string* err, const std::string& why) {
    if (err) *err = why;
    return false;
}

bool stripEnvelope(const std::string& message, std::string& rest,
                   std::string* err) {
    if (message.find_first_of("\r\n") != std::string::npos)
        return fail(err, "mensagem com quebra de linha");
    std::istringstream in(message);
    std::string magic;
    int version = 0;
    if (!(in >> magic >> version)) return fail(err, "mensagem curta");
    if (magic != kMagic) return fail(err, "protocolo desconhecido");
    if (version != kProtocolVersion) return fail(err, "versao incompativel");
    std::getline(in, rest);
    if (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
    return true;
}

bool noExtra(std::istringstream& in, std::string* err) {
    std::string extra;
    if (in >> extra) return fail(err, "argumentos extras");
    return true;
}

}  // namespace

std::string makeHello(const std::string& name) {
    std::string clean;
    for (char c : name) {
        if (c == '\r' || c == '\n') continue;
        clean += c;
        if (clean.size() == kMaxNameLength) break;
    }
    return std::string(kMagic) + " " + std::to_string(kProtocolVersion) +
           " HELLO " + clean;
}

bool parseHello(const std::string& message, std::string& name,
                std::string* err) {
    std::string rest;
    if (!stripEnvelope(message, rest, err)) return false;
    std::istringstream in(rest);
    std::string tag;
    if (!(in >> tag) || tag != "HELLO") return fail(err, "esperava HELLO");
    std::getline(in, name);
    if (!name.empty() && name.front() == ' ') name.erase(name.begin());
    if (name.size() > kMaxNameLength) return fail(err, "nome longo demais");
    return true;
}

std::string makeAction(const Action& action) {
    std::ostringstream out;
    out << kMagic << ' ' << kProtocolVersion << " ACT ";
    switch (action.kind) {
        case ActionKind::DrawStock:
            out << "draw stock";
            break;
        case ActionKind::DrawTable:
            out << "draw table";
            break;
        case ActionKind::LayMeld:
            out << "lay " << action.target << ' ' << action.cards.size();
            for (const Card& card : action.cards) out << ' ' << int(card.id);
            break;
        case ActionKind::Discard:
            out << "discard " << int(action.card.id);
            break;
        case ActionKind::RequestDiscard:
            out << "request-discard";
            break;
        case ActionKind::PromptYes:
            out << "prompt yes";
            break;
        case ActionKind::PromptNo:
            out << "prompt no";
            break;
        case ActionKind::Continue:
            out << "continue";
            break;
    }
    return out.str();
}

bool parseAction(const std::string& message, Action& action,
                 std::string* err) {
    std::string rest;
    if (!stripEnvelope(message, rest, err)) return false;
    std::istringstream in(rest);
    std::string tag;
    if (!(in >> tag) || tag != "ACT") return fail(err, "esperava ACT");

    std::string kind;
    if (!(in >> kind)) return fail(err, "acao ausente");
    action = Action{};

    if (kind == "draw") {
        std::string source;
        if (!(in >> source)) return fail(err, "fonte ausente");
        if (source == "stock")
            action.kind = ActionKind::DrawStock;
        else if (source == "table")
            action.kind = ActionKind::DrawTable;
        else
            return fail(err, "fonte desconhecida");
        return noExtra(in, err);
    }
    if (kind == "lay") {
        int target = 0;
        int count = 0;
        if (!(in >> target >> count)) return fail(err, "melda incompleta");
        if (target < -2 || target > 14) return fail(err, "alvo invalido");
        if (count < 1 || count > DECK_SIZE) return fail(err, "quantidade invalida");
        action.kind = ActionKind::LayMeld;
        action.target = target;
        for (int i = 0; i < count; ++i) {
            int id = 0;
            if (!(in >> id) || id < 1 || id > DECK_SIZE)
                return fail(err, "carta invalida");
            action.cards.push_back(makeCard(std::uint8_t(id)));
        }
        return noExtra(in, err);
    }
    if (kind == "discard") {
        int id = 0;
        if (!(in >> id) || id < 1 || id > DECK_SIZE)
            return fail(err, "carta invalida");
        action.kind = ActionKind::Discard;
        action.card = makeCard(std::uint8_t(id));
        return noExtra(in, err);
    }
    if (kind == "request-discard") {
        action.kind = ActionKind::RequestDiscard;
        return noExtra(in, err);
    }
    if (kind == "prompt") {
        std::string answer;
        if (!(in >> answer)) return fail(err, "resposta ausente");
        if (answer == "yes")
            action.kind = ActionKind::PromptYes;
        else if (answer == "no")
            action.kind = ActionKind::PromptNo;
        else
            return fail(err, "resposta desconhecida");
        return noExtra(in, err);
    }
    if (kind == "continue") {
        action.kind = ActionKind::Continue;
        return noExtra(in, err);
    }
    return fail(err, "acao desconhecida");
}

bool apply(Game& game, const Action& action, std::string* err) {
    switch (action.kind) {
        case ActionKind::DrawStock:
            if (game.phase() != Phase::Draw)
                return fail(err, "nao e a fase de compra");
            if (game.monte().empty()) return fail(err, "monte vazio");
            return game.draw(false) || fail(err, "compra recusada");
        case ActionKind::DrawTable:
            if (game.phase() != Phase::Draw)
                return fail(err, "nao e a fase de compra");
            if (!game.canTakeDiscard()) return fail(err, "mesa vazia");
            return game.draw(true) || fail(err, "compra recusada");
        case ActionKind::LayMeld: {
            const std::string why = game.layMeld(action.cards, action.target);
            if (!why.empty()) return fail(err, why);
            return true;
        }
        case ActionKind::Discard:
            return game.discardCard(action.card) || fail(err, "descarte recusado");
        case ActionKind::RequestDiscard:
            if (game.phase() != Phase::Play)
                return fail(err, "nao e a fase de jogo");
            game.requestDiscard();
            return true;
        case ActionKind::PromptYes:
            if (game.prompt() == Prompt::None) return fail(err, "sem pergunta");
            game.answerPrompt(true);
            return true;
        case ActionKind::PromptNo:
            if (game.prompt() == Prompt::None) return fail(err, "sem pergunta");
            game.answerPrompt(false);
            return true;
        case ActionKind::Continue:
            if (game.phase() != Phase::HandOver)
                return fail(err, "nao e o fim da mao");
            game.continueAfterHand();
            return true;
    }
    return fail(err, "acao desconhecida");
}

std::uint64_t fingerprint(const Game& game) {
    std::uint64_t hash = 1469598103934665603ull;  // FNV-1a offset basis
    for (unsigned char c : report::strictLine(game)) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string fingerprintText(const Game& game) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << fingerprint(game);
    return out.str();
}

}  // namespace buraco::net
