import { EventEmitter } from "node:events";
import axios from "axios";
export default class LichessClient {
    private token: string;
    private baseUrl: string;

    constructor(token: string, baseUrl = "https://lichess.org/api") {
        this.token = token;
        this.baseUrl = baseUrl;
    }

    stream(endpoint: string) {
        const emitter = new EventEmitter();
        const controller = new AbortController();

        (async () => {
            try {
                const response = await axios({
                    method: "GET",
                    url: `${this.baseUrl}${endpoint}`,
                    responseType: "stream",
                    headers: {
                        "Authorization": `Bearer ${this.token}`
                    },
                    signal: controller.signal
                });

                console.log("Connection established");


                for await (const chunk of response.data) {
                    const lines = chunk.toString().split("\n");
                    for (const line of lines) {
                        if (line.trim() === "") continue;
                        console.log("Received chunk: " + line);
                        const event = JSON.parse(line);
                        emitter.emit("event", event);
                    }
                }

            } catch (err) {
                console.error("Lichess stream error:", err);
                emitter.emit("disconnect", err);
            }

        })();

        return {
            events: emitter,
            close: () => controller.abort()
        };
    }
    declineChallenge(id: string, reason = "later") {

        const encodedParams = new URLSearchParams();
        encodedParams.set('reason', reason);

        return this.post(`/challenge/${id}/decline`, encodedParams);
    }
    acceptChallenge(id: string) {
        return this.post(`/challenge/${id}/accept`);
    }

    makeMove(gameId: string, move: string) {
        return this.post(`/bot/game/${gameId}/move/${move}`)
    }

    async post(endpoint: string, body: URLSearchParams = new URLSearchParams()) {
        try {
            const response = await axios({
                method: "POST",
                url: `${this.baseUrl}${endpoint}`,
                headers: {
                    "Authorization": `Bearer ${this.token}`,
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                data: body
            });
            return response.data;
        } catch (err: any) {
            console.error("Lichess POST error:", err.response?.data || err.message);
            throw err;
        }
    }


}