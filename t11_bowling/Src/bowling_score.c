#include "bowling_score.h"

static uint8_t is_strike(const BowlingScoreState *state, uint8_t roll_index)
{
    return state->rolls[roll_index] == 10U;
}

void BowlingScore_Reset(BowlingScoreState *state)
{
    uint8_t i;

    state->roll_count = 0U;

    for (i = 0U; i < BOWLING_MAX_ROLLS; i++)
    {
        state->rolls[i] = 0U;
    }
}

void BowlingScore_Roll(BowlingScoreState *state, uint8_t pins)
{
    if (state->roll_count >= BOWLING_MAX_ROLLS)
    {
        return;
    }

    state->rolls[state->roll_count] = pins;
    state->roll_count++;
}

int16_t BowlingScore_FrameScore(const BowlingScoreState *state, uint8_t frame_index)
{
    uint8_t frame;
    uint8_t roll_index;

    if (frame_index >= BOWLING_FRAMES)
    {
        return -1;
    }

    roll_index = 0U;

    for (frame = 0U; frame < BOWLING_FRAMES; frame++)
    {
        int16_t frame_score;

        if (roll_index >= state->roll_count)
        {
            return -1;
        }

        if (frame < 9U)
        {
            if (is_strike(state, roll_index) != 0U)
            {
                if ((uint8_t)(roll_index + 2U) >= state->roll_count)
                {
                    return -1;
                }

                frame_score = (int16_t)(10U +
                                        state->rolls[roll_index + 1U] +
                                        state->rolls[roll_index + 2U]);

                if (frame == frame_index)
                {
                    return frame_score;
                }

                roll_index++;
                continue;
            }

            if ((uint8_t)(roll_index + 1U) >= state->roll_count)
            {
                return -1;
            }

            frame_score = (int16_t)(state->rolls[roll_index] +
                                    state->rolls[roll_index + 1U]);

            if (frame_score == 10)
            {
                if ((uint8_t)(roll_index + 2U) >= state->roll_count)
                {
                    return -1;
                }

                frame_score = (int16_t)(10U + state->rolls[roll_index + 2U]);
            }

            if (frame == frame_index)
            {
                return frame_score;
            }

            roll_index = (uint8_t)(roll_index + 2U);
            continue;
        }

        if ((uint8_t)(roll_index + 1U) >= state->roll_count)
        {
            return -1;
        }

        frame_score = (int16_t)(state->rolls[roll_index] + state->rolls[roll_index + 1U]);

        if ((state->rolls[roll_index] == 10U) ||
            ((uint8_t)(state->rolls[roll_index] + state->rolls[roll_index + 1U]) == 10U))
        {
            if ((uint8_t)(roll_index + 2U) >= state->roll_count)
            {
                return -1;
            }

            frame_score = (int16_t)(frame_score + state->rolls[roll_index + 2U]);
        }

        if (frame == frame_index)
        {
            return frame_score;
        }

        return -1;
    }

    return -1;
}

uint16_t BowlingScore_Total(const BowlingScoreState *state)
{
    uint8_t frame;
    uint16_t total;

    total = 0U;

    for (frame = 0U; frame < BOWLING_FRAMES; frame++)
    {
        int16_t frame_score;

        frame_score = BowlingScore_FrameScore(state, frame);

        if (frame_score < 0)
        {
            break;
        }

        total = (uint16_t)(total + (uint16_t)frame_score);
    }

    return total;
}

uint8_t BowlingScore_IsComplete(const BowlingScoreState *state)
{
    uint8_t roll_index;
    uint8_t frame;

    roll_index = 0U;

    for (frame = 0U; frame < 9U; frame++)
    {
        if (roll_index >= state->roll_count)
        {
            return 0U;
        }

        if (state->rolls[roll_index] == 10U)
        {
            roll_index++;
        }
        else
        {
            if ((uint8_t)(roll_index + 1U) >= state->roll_count)
            {
                return 0U;
            }

            roll_index = (uint8_t)(roll_index + 2U);
        }
    }

    if (roll_index >= state->roll_count)
    {
        return 0U;
    }

    if ((uint8_t)(roll_index + 1U) >= state->roll_count)
    {
        return 0U;
    }

    if ((state->rolls[roll_index] == 10U) ||
        ((uint8_t)(state->rolls[roll_index] + state->rolls[roll_index + 1U]) == 10U))
    {
        if ((uint8_t)(roll_index + 2U) >= state->roll_count)
        {
            return 0U;
        }
    }

    return 1U;
}
