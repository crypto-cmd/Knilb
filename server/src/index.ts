import { ENV } from "./utils/config"
import LichessClient from "./api/LichessClient"
import type { LichessChallenge } from "./api/types";
import EngineInstance from "./engine/EngineInstance";

console.log("Lichess client initialized");
const client = new LichessClient(ENV.KNILB_BOT_API_KEY!);

const engine = new EngineInstance();

let isInGame = false;
// Challenge
const { events } = client.stream("/stream/event");
events.on("event", (event: any) => {
    if (event.type == "challenge") {
        if (event.challenge.challenger.id === "knilb") return;

        if (!isInGame) {
            client.acceptChallenge(event.challenge.id);
        } else {
            client.declineChallenge(event.challenge.id);
        }
    } else if (event.type == "challengeCanceled") { }
    else if (event.type == "gameStart") {
        // Start Game
        const gameId = event.game.gameId;

        // Only handle games compatible with the bot API
        if (event.game.compat?.bot === false) {
            console.log(`Skipping game ${gameId}: not bot-compatible`);
            return;
        }

        isInGame = true;

        const { events: game, close } = client.stream(`/bot/game/stream/${gameId}`);
        let isWhite = false;
        let initialFen = "startpos"
        engine.send("uci");
        engine.send("isready");
        game.on("event", async (gameEvent) => {
            console.log("Game event: " + JSON.stringify(gameEvent));
            if (gameEvent.type == "gameFull") {
                isWhite = gameEvent.white.id == "knilb";
                initialFen = gameEvent.initialFen;
                game.emit("event", gameEvent.state);
            } else if (gameEvent.type == "gameState") {
                if (gameEvent.status === "aborted") {
                    close();
                }
                if (gameEvent.status !== "started") return;
                // Is it my turn?
                const rawMoves = gameEvent.moves || "";              // handle null/undefined
                const trimmed = rawMoves.trim();                     // remove leading/trailing spaces

                const moveCount = trimmed === "" ? 0 : trimmed.split(/\s+/).length; // split on 1+ spaces

                const isWhiteTurn = moveCount % 2 === 0;
                const isMyTurn = (isWhite && isWhiteTurn) || (!isWhite && !isWhiteTurn);
                console.log("Is white? " + isWhite);
                console.log("Is white Turn? " + isWhiteTurn);

                console.log("Is my turn? " + isMyTurn);

                if (!isMyTurn) return;
                const moves = gameEvent.moves;
                const time = {
                    wtime: gameEvent.wtime,
                    btime: gameEvent.btime,
                    winc: gameEvent.winc,
                    binc: gameEvent.binc,
                };
                const best = await engine.getBestMove(initialFen, time, moves)

                console.log(best);
                if (isInGame) {
                    client.makeMove(gameId, best);
                } else {
                    console.warn("Not making move because game is no longer active");
                }
            }
        })
    } else if (event.type == "gameFinish") {
        console.log("Game finished: " + JSON.stringify(event));
        isInGame = false;
        engine.send("stop");
    }

});
events.on("disconnect", () => console.log("Lost connection"));
events.on("reconnect", () => console.log("Reconnected"));