#pragma once
#include "Board.hpp"
#include "Bot.hpp"
#include <string>
namespace Knilb::UCI {
enum class CommandType {
    Uci,
    IsReady,
    Quit,
    Stop,
    Position,
    SetOption,
    Go,
    Unknown
};

// The structured command payload
struct Command {
    CommandType type= CommandType::Unknown;

    // Payload for "setoption"
    std::string optionName;
    std::string optionValue;

    // Payload for "position"
    std::string fen; // Will be "startpos" or the actual FEN string
    std::vector<std::string> moves;

    // Payload for "go"
    int wtime= 0;
    int btime= 0;
    int winc= 0;
    int binc= 0;
    int movestogo= 0;
    int movetime= 0;
    int depth= 0;
    bool infinite= false;
};
Command GetCommand();
}; // namespace Knilb::UCI
