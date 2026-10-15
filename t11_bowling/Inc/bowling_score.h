#ifndef BOWLING_SCORE_H_
#define BOWLING_SCORE_H_

#include <stdint.h>

#define BOWLING_MAX_ROLLS 21U
#define BOWLING_FRAMES    10U

typedef struct
{
    uint8_t rolls[BOWLING_MAX_ROLLS];
    uint8_t roll_count;
} BowlingScoreState;

void BowlingScore_Reset(BowlingScoreState *state);
void BowlingScore_Roll(BowlingScoreState *state, uint8_t pins);
int16_t BowlingScore_FrameScore(const BowlingScoreState *state, uint8_t frame_index);
uint16_t BowlingScore_Total(const BowlingScoreState *state);
uint8_t BowlingScore_IsComplete(const BowlingScoreState *state);

#endif
