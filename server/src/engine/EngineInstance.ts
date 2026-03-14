import { spawn, type FileSink } from "bun";
import { ENV } from "../utils/config";
export default class EngineInstance {
    process: Bun.Subprocess;
    writer: FileSink;
    decoder = new TextDecoder();

    private listeners: ((line: string) => void)[] = [];

    constructor() {
        this.process = spawn([ENV.KNILB_EXE_PATH ?? ""], {
            stdin: "pipe",
            stdout: "pipe",
            stderr: "pipe",
        });

        this.writer = this.process.stdin as FileSink;

        this.readOutput();
        this.readError();

        this.send("uci");

    }

    async readOutput() {
        if (!this.process.stdout) return;

        let buffer = "";

        for await (const chunk of this.process.stdout) {
            buffer += this.decoder.decode(chunk);

            let lines = buffer.split("\n");
            buffer = lines.pop()!;

            for (const line of lines) {
                const trimmed = line.trim();
                console.log("ENGINE:", trimmed);

                // Notify listeners
                for (const fn of this.listeners) fn(trimmed);
            }
        }
    }

    async readError() {
        if (!this.process.stderr) return;

        for await (const chunk of this.process.stderr) {
            console.error("ENGINE ERR:", this.decoder.decode(chunk));
        }
    }

    send(cmd: string) {
        console.log(`UCI: Sending ${cmd}`)
        this.writer.write(cmd + "\n");
    }

    async getBestMove(fen: string, timeForMe: { wtime: number, winc: number, btime: number, binc: number }, moves: string = ""): Promise<string> {
        return new Promise((resolve) => {
            const handler = (line: string) => {
                if (line.startsWith("bestmove")) {
                    const move = line.split(" ")[1]!;
                    resolve(move);

                    // Remove this listener after resolving
                    this.listeners = this.listeners.filter((fn) => fn !== handler);
                }
            };

            this.listeners.push(handler);

            // Send UCI commands
            this.send(`position ${fen} moves ${moves}`);
            this.send(`go wtime ${timeForMe.wtime} winc ${timeForMe.winc} btime ${timeForMe.btime} binc ${timeForMe.binc}`);
        });
    }
}
