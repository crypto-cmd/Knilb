#include "Board.hpp"
#include "Bot.hpp"
#include "UCI.hpp"
#include "TimeController.hpp"
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <unordered_map>

auto ENGINE_NAME= "Knilb";
auto ENGINE_VERSION= "0.0.2-alpha";

int main() {

    std::thread searchThread;
    Knilb::Engine::Bot bot;
    std::atomic<bool> stopRequested{false};
    bot.setStopFlag(&stopRequested);
    auto isRunning= true;

    while(isRunning) {
        auto cmd= Knilb::UCI::GetCommand();
        if(cmd.type == Knilb::UCI::CommandType::Quit) {
            stopRequested.store(true);
            if(searchThread.joinable()) searchThread.join();
            isRunning = false;
            break;
        } else if(cmd.type == Knilb::UCI::CommandType::Stop) {
            stopRequested.store(true);
            if(searchThread.joinable()) searchThread.join();
        } else if(cmd.type == Knilb::UCI::CommandType::Uci) {
            std::cout << "id name " << ENGINE_NAME << "\n";
            std::cout << "id author Orby\n";
            std::cout << "id version " << ENGINE_VERSION << "\n";
            std::cout << "uciok\n";
        } else if(cmd.type == Knilb::UCI::CommandType::IsReady) {
            std::cout << "readyok\n";
        } else if(cmd.type == Knilb::UCI::CommandType::Position) {
            auto fen= cmd.fen == "startpos" ? Knilb::Representation::Board::STARTING_POS : cmd.fen;
            bot.setFen(fen);

            for(const auto& move: cmd.moves) {
                bot.PerformMove(move);
            }
        } else if(cmd.type == Knilb::UCI::CommandType::Go) {
            if(searchThread.joinable()) searchThread.join();
            stopRequested.store(false);

            Knilb::Engine::SearchConfig searchConfig;
            Knilb::Engine::TimeController::SearchLimit searchLimit;

            // Configure search limits based on the "go" command parameters
            searchLimit.wtime= cmd.wtime;
            searchLimit.btime= cmd.btime;
            searchLimit.winc= cmd.winc;
            searchLimit.binc= cmd.binc;
            searchLimit.depth= cmd.depth;
            searchLimit.movetime= cmd.movetime;
            // searchLimit.ponder= cmd.ponder;
            searchLimit.infinite= cmd.infinite;

            std::cout << "info string Starting search with limits: "
                      << "wtime=" << searchLimit.wtime << " "
                      << "btime=" << searchLimit.btime << " "
                      << "winc=" << searchLimit.winc << " "
                      << "binc=" << searchLimit.binc << " "
                      << "depth=" << searchLimit.depth << " "
                      << "movetime=" << searchLimit.movetime << " "
                      << "infinite=" << (searchLimit.infinite ? "true" : "false")
                      << std::endl;

            searchConfig.limit= searchLimit;
            // Launch search in background
            searchThread= std::thread([&bot, searchConfig]() {
                auto result= bot.getBestMove(searchConfig);
                std::cout << "info string Search thread completed" << std::endl;
                std::cout << "bestmove " << Knilb::Representation::Moves::toString(result.first) << std::endl;
            });
        } else if(cmd.type == Knilb::UCI::CommandType::Unknown) {
            std::cout << "info string received unknown command" << std::endl;
        }
    }
    return 0;
}
