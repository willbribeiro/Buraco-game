#include "i18n.h"

#include <algorithm>
#include <cctype>

namespace i18n {
namespace {

Language g_language = Language::Pt;

std::string lower(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(std::string s) {
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

}  // namespace

Language language() { return g_language; }

void setLanguage(Language lang) { g_language = valid(lang) ? lang : Language::Pt; }

const char* code(Language lang) {
    return lang == Language::En ? "en" : "pt";
}

const char* displayName(Language lang) {
    return lang == Language::En ? "English" : "Portugues";
}

bool valid(Language lang) {
    return lang == Language::Pt || lang == Language::En;
}

bool parseLanguage(const std::string& text, Language& out) {
    const std::string value = lower(trim(text));
    if (value == "pt" || value == "por" || value == "portugues") {
        out = Language::Pt;
        return true;
    }
    if (value == "en" || value == "eng" || value == "english" || value == "ingles") {
        out = Language::En;
        return true;
    }
    return false;
}

Language toggled(Language lang) {
    return lang == Language::En ? Language::Pt : Language::En;
}

const char* text(Text id) {
    const bool en = g_language == Language::En;
    switch (id) {
        case Text::Empty: return "";
        case Text::DefaultPlayerName: return en ? "You" : "Voce";
        case Text::DefaultOpponentName: return en ? "Me" : "Eu";
        case Text::DefaultPlayer: return en ? "PLAYER" : "JOGADOR";
        case Text::DefaultPlayer1: return en ? "PLAYER 1" : "JOGADOR 1";
        case Text::DefaultPlayer2: return en ? "PLAYER 2" : "JOGADOR 2";
        case Text::PressEnter: return en ? "...now press ENTER" : "...agora aperte ENTER";
        case Text::Yes: return en ? "Yes" : "Sim";
        case Text::No: return en ? "No" : "Nao";

        case Text::HelpTitle: return en ? "BURACO - help" : "BURACO - ajuda";
        case Text::HelpIntro: return en ? "Shall we play buraco?" : "Vamos jogar buraco ?";
        case Text::HelpRules:
            return en ? "During the game you can answer my questions with \"Y\" or \"N\"."
                      : "Durante o jogo, voce pode responder minhas perguntas com \"S\" ou \"N\".";
        case Text::HelpArrows:
            return en ? "The left and right arrows choose your cards."
                      : "As flechas para esquerda e direita servem para escolher as suas cartas.";
        case Text::HelpScroll:
            return en
                       ? "When you have chosen, press the space bar so I know which card you "
                         "picked. If you no longer want that card, press the space bar again. "
                         "To lay a meld, choose some cards and press ENTER at the end. To "
                         "discard, press ENTER without choosing any card."
                       : "Quando voce tiver escolhido, aperte a barra de espacos para que eu "
                         "saiba qual carta voce escolheu. Se voce nao quizer mais esta carta, "
                         "aperte a barra novamente. Para baixar um jogo, escolha algumas cartas "
                         "e no fim aperte ENTER. Para descartar, aperte ENTER sem ter escolhido "
                         "nenhuma carta.";
        case Text::HelpMelds:
            return en
                       ? "The up and down arrows scroll the melds that are on the table."
                       : "As flechas para cima e para baixo mostram os jogos que estao na mesa.";
        case Text::HelpF1: return en ? "To see this screen, press F1."
                                     : "Para ver esta tela, aperte F1.";
        case Text::HelpF2: return en ? "To save the game and continue later, press F2."
                                     : "Para gravar o jogo e continuar depois, aperte F2.";
        case Text::HelpF3: return en ? "To see the match status, press F3."
                                     : "Para ver a situacao geral do jogo, aperte F3.";
        case Text::HelpOptions:
            return en ? "To see the game options, press F9."
                      : "Para ver as opcoes do jogo, aperte F9.";
        case Text::HelpLuck: return en ? "Good luck!" : "Boa sorte!";

        case Text::StatusTitle:
            return en ? "BURACO - match status" : "BURACO - situacao geral";
        case Text::StatusIntro:
            return en ? "The game so far:" : "A situacao do jogo ate aqui:";
        case Text::StatusMatches:
            return en ? "in %d matches, of which you won %d and I won %d."
                      : "nos j\xA0 %d partidas das quais voce ganhou %d e eu ganhou %d.";
        case Text::StatusLastHand:
            return en ? "In the last hand you scored %d, and I scored %d."
                      : "Na ultima partida voce fez %d, e eu fiz %d.";
        case Text::StatusTotals:
            return en ? "So far you have %d points and I have %d."
                      : "Ate agora voce ja fez %d pontos e eu fiz %d.";
        case Text::StatusMidHand:
            return en ? "We are in the middle of a match." : "Estamos no meio de uma partida.";
        case Text::StatusCards:
            return en ? "You have %d cards in hand, and I have %d."
                      : "Voce tem %d cartas na mao, e eu tenho %d.";
        case Text::StatusMelds:
            return en ? "You have laid %d melds and I %d."
                      : "Voce ja baixou %d jogos e eu %d.";
        case Text::StatusTable:
            return en ? "There are %d cards on the table." : "Na mesa ha %d cartas.";
        case Text::StatusStock:
            return en ? "There are %d cards in the stock." : "No monte ha %d cartas.";

        case Text::HandMine: return en ? "Your hand." : "Sua mao.";
        case Text::HandOther: return en ? "My hand." : "Minha mao.";
        case Text::HandInfo:
            return en ? "cards: %2d   points: %3d" : "cartas: %2d   pontos: %3d";
        case Text::MeldsYours: return en ? "YOUR MELDS" : "SEUS JOGOS";
        case Text::MeldsMine: return en ? "MY MELDS" : "MEUS JOGOS";
        case Text::MeldsNone: return en ? "no melds" : "nenhum jogo";
        case Text::HudTurn: return en ? "TURN: " : "VEZ: ";

        case Text::ScoreAccum: return en ? "total %d" : "acum %d";
        case Text::ScoreMeldLine: return en ? "Meld %2d %s" : "Jogo %2d %s";
        case Text::ScoreNoMelds: return en ? "(no melds)" : "(nenhum jogo)";
        case Text::ScoreMelds: return en ? "Melds" : "Jogos";
        case Text::ScoreBatida: return en ? "Going out" : "Batida";
        case Text::ScoreHandCards: return en ? "Cards in hand" : "Cartas na mao";
        case Text::ScoreMorto: return en ? "Morto" : "Morto";
        case Text::ScoreHandTotal: return en ? "Hand total" : "Total da mao";
        case Text::ScoreTitle:
            return en ? "SCORING   hand %d" : "CONTAGEM DE PONTOS   mao %d";
        case Text::ScoreContinue:
            return en ? "ENTER to continue" : "ENTER para continuar";

        case Text::ContinueTitle:
            return en ? "BURACO - saved game" : "BURACO - jogo gravado";
        case Text::ContinueFound:
            return en ? "I found a saved game:" : "Encontrei um jogo gravado:";
        case Text::ContinuePlayer: return en ? "Player: %s" : "Jogador: %s";
        case Text::ContinueQuestion:
            return en ? "Continue the game (Y) or create a new one (N)?"
                      : "Continuar o jogo (S) ou criar um novo (N)?";

        case Text::ModeTitle: return en ? "BURACO - new game" : "BURACO - novo jogo";
        case Text::ModeQuestion: return en ? "How many players?" : "Quantos jogadores?";
        case Text::ModeOne:
            return en ? "1 player  (you against the AI)"
                      : "1 jogador  (voce contra a IA)";
        case Text::ModeTwo:
            return en ? "2 players  (hot-seat, same machine)"
                      : "2 jogadores  (hot-seat, mesma maquina)";
        case Text::ModeOnline:
            return en ? "3 online  (two computers on the same network)"
                      : "3 online  (dois computadores na mesma rede)";
        case Text::ModeHint:
            return en ? "Arrows or 1/2/3 choose.  ENTER confirms.  ESC goes back."
                      : "Setas ou 1/2/3 escolhem.  ENTER confirma.  ESC volta.";

        case Text::NameOnlinePrompt:
            return en ? "Name for the online match?" : "Nome para a partida online?";
        case Text::NamePlayer1Prompt:
            return en ? "Name of PLAYER 1?" : "Nome do JOGADOR 1?";
        case Text::NamePlayer2Prompt:
            return en ? "Name of PLAYER 2?" : "Nome do JOGADOR 2?";
        case Text::NameSoloPrompt: return en ? "What is your name?" : "Qual o seu nome?";
        case Text::NameOnlineBlank:
            return en ? "Blank ENTER uses PLAYER." : "ENTER em branco usa JOGADOR.";
        case Text::NameOnlineShare:
            return en ? "The name is shown to the other player."
                      : "O nome aparece para o outro jogador.";
        case Text::NameConfirm:
            return en ? "ENTER confirms.  ESC returns to the start screen."
                      : "ENTER confirma.  ESC volta a tela inicial.";
        case Text::NameHotseatBlank1:
            return en ? "Blank ENTER uses PLAYER 1." : "ENTER em branco usa JOGADOR 1.";
        case Text::NameHotseatBlank2:
            return en ? "Blank ENTER uses PLAYER 2." : "ENTER em branco usa JOGADOR 2.";
        case Text::NameHotseatSave:
            return en ? "The save uses both names.  ESC returns to the start screen."
                      : "O save usa os dois nomes.  ESC volta a tela inicial.";
        case Text::NameSoloSave:
            return en ? "The typed name becomes the progress file name."
                      : "O nome digitado sera o nome do arquivo de progresso.";
        case Text::NameEmptyError:
            return en ? "Type a name before continuing."
                      : "Digite um nome antes de continuar.";
        case Text::NameLoadError:
            return en ? "I could not read the saved game. Type a name."
                      : "Nao consegui ler o jogo gravado. Digite um nome.";

        case Text::OnlineTitle: return "BURACO - online";
        case Text::OnlineIntro:
            return en ? "Play against another computer on the local network."
                      : "Jogar com outro computador na rede local.";
        case Text::OnlineHostChoice:
            return en ? "Create match  (host)" : "Criar partida  (host)";
        case Text::OnlineJoinChoice:
            return en ? "Join match  (client)" : "Entrar em partida  (cliente)";
        case Text::OnlineFirewall:
            return en ? "The host must allow the port through the firewall."
                      : "O host precisa liberar a porta no firewall.";
        case Text::OnlineMenuHint:
            return en ? "Arrows choose.  ENTER confirms.  ESC goes back."
                      : "Setas escolhem.  ENTER confirma.  ESC volta.";
        case Text::OnlineHostTitle:
            return en ? "Create online match" : "Criar partida online";
        case Text::OnlinePort: return en ? "Port: %d" : "Porta: %d";
        case Text::OnlineWaiting:
            return en ? "Waiting for a connection..." : "Aguardando conexao...";
        case Text::OnlineHostStep1:
            return en ? "On the other computer, choose \"Join match\""
                      : "No outro computador, escolha \"Entrar em partida\"";
        case Text::OnlineHostStep2:
            return en ? "and type this machine's IP address."
                      : "e digite o IP desta maquina.";
        case Text::OnlineCancel: return en ? "ESC cancels." : "ESC cancela.";
        case Text::OnlineJoinTitle:
            return en ? "Join online match" : "Entrar em partida online";
        case Text::OnlineHostAddress:
            return en ? "Host address:" : "Endereco do host:";
        case Text::OnlineJoinHint:
            return en ? "ENTER connects.  ESC cancels."
                      : "ENTER conecta.  ESC cancela.";

        case Text::SeatSwitchTitle:
            return en ? "BURACO - player switch" : "BURACO - troca de jogador";
        case Text::SeatSwitchPass:
            return en ? "Pass the keyboard to %s." : "Passe o controle para %s.";
        case Text::SeatSwitchHide:
            return en ? "Do not show your hand to the other player."
                      : "Nao mostre a mao para o outro jogador.";

        case Text::OptionsTitle: return en ? "BURACO - options" : "BURACO - opcoes";
        case Text::OptionsScale: return en ? "Window scale" : "Escala da janela";
        case Text::OptionsAdjustment: return en ? "Image scaling" : "Ajuste da imagem";
        case Text::OptionsFullscreen: return en ? "Fullscreen" : "Tela cheia";
        case Text::OptionsVsync: return "VSync";
        case Text::OptionsMaximized: return en ? "Start maximized" : "Iniciar maximizada";
        case Text::OptionsLanguage: return en ? "Language" : "Idioma";
        case Text::OptionsSound: return en ? "Sound" : "Som";
        case Text::OptionsVolume: return "Volume";
        case Text::OptionsCenter: return en ? "Center window" : "Centralizar janela";
        case Text::OptionsReset: return en ? "Restore defaults" : "Restaurar padroes";
        case Text::OptionsClose: return en ? "Back to the game" : "Voltar ao jogo";
        case Text::OptionsInteger:
            return en ? "Integer (pixel-perfect)" : "Inteiro (pixel-perfect)";
        case Text::OptionsStretched: return en ? "Stretched" : "Esticado";
        case Text::OptionsCentered: return en ? "Centered" : "Centralizada";
        case Text::OptionsFree:
            return en ? "Free (ENTER centers)" : "Livre (ENTER centraliza)";
        case Text::OptionsEnter: return "ENTER";
        case Text::OptionsEnterEsc: return en ? "ENTER or ESC" : "ENTER ou ESC";
        case Text::OptionsHint1:
            return en ? "Up/down select a row; left/right change it."
                      : "Setas cima/baixo escolhem; esquerda/direita alteram.";
        case Text::OptionsHint2:
            return en ? "ENTER applies.  F9 or ESC closes this screen."
                      : "ENTER aplica.  F9 ou ESC fecha esta tela.";
        case Text::OptionsHint3:
            return en ? "Video and sound are saved to buraco.cfg on exit."
                      : "Video e som sao gravados no buraco.cfg ao sair.";
        case Text::OptionsHint4:
            return en ? "1x..8x = 640x400 times the scale.  Auto uses the saved size"
                      : "1x..8x = 640x400 vezes a escala.  Auto usa o tamanho salvo";
        case Text::OptionsHint5:
            return en ? "and shrinks to 90% of the usable area.  Volume in 10% steps."
                      : "e reduz para 90% da area util.  Volume em passos de 10%.";
        case Text::OptionsIntegerNote:
            return en ? "Integer mode: the image may drop to 1x on small screens."
                      : "Modo inteiro: a imagem pode cair para 1x em telas pequenas.";

        case Text::MsgShuffling:
            return en ? "Shuffling. Please cut..." : "Embaralhando. Por favor, corte...";
        case Text::MsgShufflingEnter:
            return en ? "Shuffling. Please cut... <ENTER>"
                      : "Embaralhando. Por favor, corte... <ENTER>";
        case Text::MsgChooseDiscard:
            return en ? "Choose the card to discard." : "Escolha a carta a descartar.";
        case Text::MsgBoughtTable:
            return en ? "You bought the table." : "Voce comprou a mesa.";
        case Text::MsgAiBoughtTable:
            return en ? "I'm buying the table." : "Estou comprando a mesa.";
        case Text::MsgStockEmpty: return en ? "The stock is empty." : "O monte acabou.";
        case Text::MsgBoughtStock:
            return en ? "You drew from the stock." : "Voce comprou do monte.";
        case Text::MsgAiBoughtStock:
            return en ? "I'm drawing from the stock." : "Estou comprando do monte.";
        case Text::MsgNotLayTime:
            return en ? "It is not time to lay melds now."
                      : "Agora nao e hora de baixar jogos.";
        case Text::MsgChooseCard:
            return en ? "Choose at least one card." : "Escolha pelo menos uma carta.";
        case Text::MsgNeedCanastra:
            return en ? "You cannot go out without a canastra."
                      : "Voce nao pode bater porque nao tem canastra.";
        case Text::MsgNewMeldMin:
            return en ? "A new meld needs at least 3 cards."
                      : "Um jogo novo precisa de pelo menos 3 cartas.";
        case Text::MsgInvalidMeld:
            return en ? "These cards do not form a valid meld."
                      : "Estas cartas nao formam um jogo valido.";
        case Text::MsgNoMeldRoom:
            return en ? "There is no room for more melds on the table."
                      : "Nao cabem mais jogos na mesa.";
        case Text::MsgMeldLaid: return en ? "Meld laid." : "Jogo baixado.";
        case Text::MsgInvalidMeldTarget: return en ? "Invalid meld." : "Jogo invalido.";
        case Text::MsgCardsDontFit:
            return en ? "These cards do not fit that meld."
                      : "Estas cartas nao entram neste jogo.";
        case Text::MsgMeldGrown: return en ? "Meld extended." : "Jogo aumentado.";
        case Text::MsgTookMorto: return en ? "You took the morto." : "Voce pegou o morto.";
        case Text::MsgTurnOver: return en ? "Your turn is over." : "Sua vez acabou.";
        case Text::MsgBuyTableQuestion:
            return en ? "Buy the table? (Y/N)" : "Comprar a mesa? (S/N)";
        case Text::MsgDrawCard:
            return en ? "Draw a card from the stock. <ENTER>"
                      : "Compre uma carta do monte. <ENTER>";
        case Text::MsgMyTurn: return en ? "It is my turn now." : "Agora e minha vez.";
        case Text::MsgCountPoints:
            return en ? "Let's count the points." : "Vamos contar os pontos.";
        case Text::MsgMatchOver:
            return en ? "The match is over !!!" : "A partida acabou !!!";
        case Text::MsgBuyTableQuestionSpaced:
            return en ? "Buy the table (Y/N) ?" : "Comprar a mesa (S/N) ?";
        case Text::MsgLayMelds: return en ? "Lay your melds." : "Baixe os jogos.";
        case Text::MsgYouDiscarded: return en ? "You discarded." : "Voce descartou.";
        case Text::MsgAiDiscarded:
            return en ? "I'm going to discard." : "Eu vou descartar.";

        case Text::MsgOnlineStarted:
            return en ? "Online match started." : "Partida online iniciada.";
        case Text::MsgWaitYourTurn:
            return en ? "Wait for your turn." : "Aguarde a sua vez.";
        case Text::MsgConnectionLost:
            return en ? "Connection lost. The match continues as local hot-seat."
                      : "Conexao perdida. Partida continua em hot-seat local.";
        case Text::MsgMoveSent:
            return en ? "Move sent. Waiting for the host..."
                      : "Jogada enviada. Aguardando o host...";
        case Text::MsgDesyncHost:
            return en ? "Desync with the host. The match continues as local hot-seat."
                      : "Dessincronizacao com o host. Partida continua em hot-seat local.";
        case Text::MsgDesyncDetected:
            return en ? "Desync detected. The match continues as local hot-seat."
                      : "Dessincronizacao detectada. Partida continua em hot-seat local.";
        case Text::MsgConnectionClosedHotseat:
            return en ? "Connection closed. The match continues as local hot-seat."
                      : "Conexao encerrada. Partida continua em hot-seat local.";
        case Text::MsgRestored: return en ? "Game restored." : "Jogo restaurado.";
        case Text::MsgSaveDisabledOnline:
            return en ? "Saving is not available in online mode."
                      : "Gravar nao esta disponivel no modo online.";
        case Text::MsgSaveFailed:
            return en ? "I could not save the game." : "Nao consegui gravar o jogo.";
        case Text::MsgLoadDisabledOnline:
            return en ? "Restoring is not available in online mode."
                      : "Restaurar nao esta disponivel no modo online.";
        case Text::MsgLoadFailed:
            return en ? "I could not restore the game." : "Nao consegui restaurar o jogo.";
        case Text::MsgWaitHostContinue:
            return en ? "Waiting for the host to continue."
                      : "Aguardando o host continuar.";
        case Text::MsgOnlineMatchOver:
            return en ? "Online match ended. ESC to quit."
                      : "Partida online encerrada. ESC para sair.";
        case Text::MsgLayChoice:
            return en ? "Extend the meld (A) or lay a new one (N)?"
                      : "Aumentar o jogo (A) ou baixar um novo (N)?";
        case Text::MsgStartFailed:
            return en ? "Could not start: %s" : "Nao consegui iniciar: %s";
        case Text::MsgSavedTo: return en ? "Game saved to %s." : "Jogo gravado em %s.";
        case Text::MsgWaitingPort:
            return en ? "Waiting for a connection on port %d..."
                      : "Aguardando conexao na porta %d...";
        case Text::MsgConnecting:
            return en ? "Connecting to %s:%d..." : "Conectando a %s:%d...";
        case Text::MsgClientConnected:
            return en ? "%s connected. ENTER to start."
                      : "%s conectou. ENTER para comecar.";
        case Text::MsgConnectedWaiting:
            return en ? "Connected to %s. Waiting for the host to start..."
                      : "Conectado a %s. Aguardando o host iniciar...";
        case Text::MsgClientDisconnected:
            return en ? "Client disconnected. Waiting for another connection..."
                      : "Cliente desconectou. Aguardando outra conexao...";
        case Text::MsgConnectionEnded:
            return en ? "Connection closed." : "Conexao encerrada.";

        case Text::NetFailure: return en ? "network failure" : "falha de rede";
        case Text::NetNoPort:
            return en ? "could not discover the port" : "nao consegui descobrir a porta";
        case Text::NetMessageTooLarge:
            return en ? "message too large" : "mensagem grande demais";
        case Text::NetSendQueueFull:
            return en ? "send queue full" : "fila de envio cheia";
        case Text::NetNoConnection:
            return en ? "connection not established" : "conexao nao estabelecida";
        case Text::NetInvalidMessage:
            return en ? "invalid message" : "mensagem invalida";
        case Text::NetInvalidHello:
            return en ? "invalid HELLO: %s" : "HELLO invalido: %s";
        case Text::NetUnexpectedSeat:
            return en ? "unexpected SEAT" : "SEAT inesperado";
        case Text::NetInvalidSeat:
            return en ? "invalid SEAT: %s" : "SEAT invalido: %s";
        case Text::NetUnexpectedRemoteSeat:
            return en ? "unexpected remote seat" : "assento remoto inesperado";
        case Text::NetUnexpectedStart:
            return en ? "unexpected START" : "START inesperado";
        case Text::NetInvalidStart:
            return en ? "invalid START: %s" : "START invalido: %s";
        case Text::NetStartBeforeSeat:
            return en ? "START before SEAT" : "START antes de SEAT";
        case Text::NetActionBeforeStart:
            return en ? "action before the match started" : "acao antes do inicio";
        case Text::NetInvalidAction:
            return en ? "invalid action: %s" : "acao invalida: %s";
        case Text::NetInvalidFingerprint:
            return en ? "invalid fingerprint: %s" : "fingerprint invalido: %s";
        case Text::NetInvalidErr:
            return en ? "invalid ERR: %s" : "ERR invalido: %s";
        case Text::NetActionRefused:
            return en ? "action refused by the host" : "acao recusada pelo host";
        case Text::NetUnexpectedMessage:
            return en ? "unexpected message: %s" : "mensagem inesperada: %s";
        case Text::NetConnectTimeout:
            return en ? "timed out while connecting" : "tempo esgotado ao conectar";
        case Text::NetHandshakeTimeout:
            return en ? "timed out during the handshake" : "tempo esgotado no handshake";
        case Text::NetConnectionClosed:
            return en ? "connection closed" : "conexao encerrada";
        case Text::NetConnectionClosedHandshake:
            return en ? "connection closed during the handshake"
                      : "conexao encerrada durante o handshake";
        case Text::NetNoClient:
            return en ? "no client connected" : "sem cliente conectado";
        case Text::NetNoMatch:
            return en ? "session has no started match" : "sessao sem partida iniciada";
        case Text::NetNotConnected: return en ? "not connected" : "sem conexao";

        case Text::ProtoLineBreak:
            return en ? "message contains a line break" : "mensagem com quebra de linha";
        case Text::ProtoShortMessage:
            return en ? "short message" : "mensagem curta";
        case Text::ProtoUnknown:
            return en ? "unknown protocol" : "protocolo desconhecido";
        case Text::ProtoVersion:
            return en ? "incompatible version" : "versao incompativel";
        case Text::ProtoExtraArgs:
            return en ? "extra arguments" : "argumentos extras";
        case Text::ProtoExpectedHello:
            return en ? "expected HELLO" : "esperava HELLO";
        case Text::ProtoNameTooLong:
            return en ? "name too long" : "nome longo demais";
        case Text::ProtoExpectedSeat:
            return en ? "expected SEAT" : "esperava SEAT";
        case Text::ProtoInvalidSeat:
            return en ? "invalid seat" : "assento invalido";
        case Text::ProtoExpectedStart:
            return en ? "expected START" : "esperava START";
        case Text::ProtoMissingSeed:
            return en ? "missing seed" : "semente ausente";
        case Text::ProtoInvalidSeed:
            return en ? "invalid seed" : "semente invalida";
        case Text::ProtoExpectedFingerprint:
            return en ? "expected FP" : "esperava FP";
        case Text::ProtoInvalidFingerprint:
            return en ? "invalid fingerprint" : "fingerprint invalido";
        case Text::ProtoExpectedErr:
            return en ? "expected ERR" : "esperava ERR";
        case Text::ProtoErrTooLong:
            return en ? "error too long" : "erro longo demais";
        case Text::ProtoExpectedAction:
            return en ? "expected ACT" : "esperava ACT";
        case Text::ProtoMissingAction:
            return en ? "missing action" : "acao ausente";
        case Text::ProtoMissingSource:
            return en ? "missing source" : "fonte ausente";
        case Text::ProtoUnknownSource:
            return en ? "unknown source" : "fonte desconhecida";
        case Text::ProtoIncompleteMeld:
            return en ? "incomplete meld" : "melda incompleta";
        case Text::ProtoInvalidTarget:
            return en ? "invalid target" : "alvo invalido";
        case Text::ProtoInvalidCount:
            return en ? "invalid count" : "quantidade invalida";
        case Text::ProtoInvalidCard:
            return en ? "invalid card" : "carta invalida";
        case Text::ProtoMissingAnswer:
            return en ? "missing answer" : "resposta ausente";
        case Text::ProtoUnknownAnswer:
            return en ? "unknown answer" : "resposta desconhecida";
        case Text::ProtoUnknownAction:
            return en ? "unknown action" : "acao desconhecida";
        case Text::ProtoNotDrawPhase:
            return en ? "not the draw phase" : "nao e a fase de compra";
        case Text::ProtoEmptyTable:
            return en ? "the table is empty" : "mesa vazia";
        case Text::ProtoDrawRefused:
            return en ? "draw refused" : "compra recusada";
        case Text::ProtoDiscardRefused:
            return en ? "discard refused" : "descarte recusado";
        case Text::ProtoNotPlayPhase:
            return en ? "not the play phase" : "nao e a fase de jogo";
        case Text::ProtoNoQuestion:
            return en ? "no pending question" : "sem pergunta";
        case Text::ProtoNotHandOver:
            return en ? "not the end of the hand" : "nao e o fim da mao";
    }
    return "";
}

}  // namespace i18n
