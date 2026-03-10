export enum LichessEventType {
    Challenge = "challenge",
    ChallengeCanceled = "challengeCanceled",
    ChallengeAccepted = "challengeAccepted",
}

export type LichessChallenge = {
    type: LichessEventType.Challenge,
    challenge: {
        id: string,
        challenger: {
            name: string
        }
    }
}


