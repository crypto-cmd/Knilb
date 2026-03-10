#include "UCI.hpp"
#include "Coordinate.hpp"
#include "MoveGeneration.hpp"
#include <random>
#include <sstream>

namespace Knilb::UCI{
Command GetCommand() {
    Command cmd;
    std::string line;

    // Block until we get a line from the GUI
    if (!std::getline(std::cin, line)) {
        cmd.type = CommandType::Quit; // Treat EOF as quit
        return cmd;
    }

    std::stringstream ss(line);
    std::string token;
    ss >> token;

    if (token == "uci") {
        cmd.type = CommandType::Uci;
    } 
    else if (token == "isready") {
        cmd.type = CommandType::IsReady;
    } 
    else if (token == "quit") {
        cmd.type = CommandType::Quit;
    } 
    else if (token == "stop") {
        cmd.type = CommandType::Stop;
    } 
    else if (token == "setoption") {
        cmd.type = CommandType::SetOption;
        std::string nameToken, valueToken;
        ss >> nameToken >> cmd.optionName >> valueToken >> cmd.optionValue;
    } 
    else if (token == "position") {
        cmd.type = CommandType::Position;
        std::string posType;
        ss >> posType;
        
        if (posType == "startpos") {
            cmd.fen = "startpos";
        } else if (posType == "fen") {
            std::string fenPart;
            for (int i = 0; i < 6; ++i) {
                if (ss >> fenPart) {
                    cmd.fen += (i > 0 ? " " : "") + fenPart;
                }
            }
        }

        ss >> token; 
        if (token == "moves") {
            std::string move;
            while (ss >> move) {
                cmd.moves.push_back(move);
            }
        }
    } 
    else if (token == "go") {
        cmd.type = CommandType::Go;
        std::string type;
        while (ss >> type) {
            if (type == "wtime") ss >> cmd.wtime;
            else if (type == "btime") ss >> cmd.btime;
            else if (type == "winc") ss >> cmd.winc;
            else if (type == "binc") ss >> cmd.binc;
            else if (type == "movestogo") ss >> cmd.movestogo;
            else if (type == "movetime") ss >> cmd.movetime;
            else if (type == "depth") ss >> cmd.depth;
            else if (type == "infinite") cmd.infinite = true;
        }
    }
    return cmd;
};
} // namespace Knilb::UCI