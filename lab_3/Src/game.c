#include "game.h"
#include "ST7920_parallel.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*====================================================
 * BOLOS T11 - TABLERO ELECTRONICO
 * STM32F429ZI + LCD ST7920 + TECLADO 4x4
 *
 * CONTROLES:
 * MENU: * iniciar, A instrucciones, # menu
 * JUEGO: 0-9 bolos, A strike, B deshacer,
 *        C pausa, # menu, * reiniciar
 * PAUSA: C continuar, # menu, * reiniciar
 * FIN: * nueva partida, # menu
 *====================================================*/

#define SCREEN_WIDTH             128U
#define SCREEN_HEIGHT             64U

/* Buzzer en PC8 */
#define BUZZER_PORT              GPIOC
#define BUZZER_PIN               GPIO_PIN_8
#define BUZZER_NORMAL_MS         45U
#define BUZZER_SPARE_MS          100U
#define BUZZER_STRIKE_MS         180U
#define BUZZER_FIN_MS            650U
#define BUZZER_ERROR_MS          35U

/* Barra de victoria de ocho LED */
#define LED1_PORT                GPIOF
#define LED1_PIN                 GPIO_PIN_13

#define LED2_PORT                GPIOE
#define LED2_PIN                 GPIO_PIN_9

#define LED3_PORT                GPIOE
#define LED3_PIN                 GPIO_PIN_11

#define LED4_PORT                GPIOF
#define LED4_PIN                 GPIO_PIN_14

#define LED5_PORT                GPIOE
#define LED5_PIN                 GPIO_PIN_13

#define LED6_PORT                GPIOF
#define LED6_PIN                 GPIO_PIN_15

#define LED7_PORT                GPIOG
#define LED7_PIN                 GPIO_PIN_14

#define LED8_PORT                GPIOG
#define LED8_PIN                 GPIO_PIN_9

#define FRAMES                   10U
#define MAX_ROLLS                21U
#define EMPTY_ROLL               255U
#define STATUS_MS                1300U

/* Estados del juego */
typedef enum
{
    GAME_MENU = 0,
    GAME_PLAYING,
    GAME_PAUSED,
    GAME_OVER
} GameState_t;

static GameState_t game_state = GAME_MENU;
static uint8_t menu_page = 0U;

/* Datos del jugador activo */
static uint8_t frame_rolls[FRAMES][3];
static uint8_t frame_roll_count[FRAMES];
static uint8_t roll_history[MAX_ROLLS];
static uint8_t roll_count = 0U;
static uint8_t current_frame = 0U;
static uint8_t balls_in_frame = 0U;
static uint8_t game_finished = 0U;

/* Datos independientes de cada jugador */
typedef struct
{
    uint8_t frame_rolls[FRAMES][3];
    uint8_t frame_roll_count[FRAMES];
    uint8_t roll_history[MAX_ROLLS];
    uint8_t roll_count;
    uint8_t current_frame;
    uint8_t balls_in_frame;
    uint8_t game_finished;
} PlayerData_t;

#define PLAYER_COUNT             2U
#define MAX_MATCH_ROLLS          (PLAYER_COUNT * MAX_ROLLS)

static PlayerData_t player_data[PLAYER_COUNT];

/* 0 = jugador 1, 1 = jugador 2 */
static uint8_t active_player = 0U;

/* Historial global de lanzamientos */
static uint8_t roll_owner_history[MAX_MATCH_ROLLS];
static uint8_t roll_event_count = 0U;

/* Mensajes temporales */
static char status_message[24] = "";
static uint32_t status_until = 0U;

/* Buzzer */
static uint8_t buzzer_active = 0U;
static uint32_t buzzer_until = 0U;

/* Animacion pseudoaleatoria de victoria */
static uint8_t victory_animation_active = 0U;
static uint8_t victory_leds_used = 0U;
static uint32_t victory_next_step = 0U;
static uint32_t victory_rng = 0xA341316CU;

/*====================================================
 * FUENTE GRANDE 5x7
 *====================================================*/
static const uint8_t font_letters[26][7] =
{
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    {0x0F,0x10,0x10,0x10,0x10,0x10,0x0F},
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0F,0x10,0x10,0x17,0x11,0x11,0x0F},
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F},
    {0x01,0x01,0x01,0x01,0x11,0x11,0x0E},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}
};

static const uint8_t font_numbers[10][7] =
{
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
    {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E},
    {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E},
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E}
};

/*====================================================
 * FUENTE COMPACTA 3x5
 *====================================================*/
static const uint8_t tiny_letters[26][5] =
{
    {2,5,7,5,5}, {6,5,6,5,6}, {3,4,4,4,3},
    {6,5,5,5,6}, {7,4,6,4,7}, {7,4,6,4,4},
    {3,4,5,5,3}, {5,5,7,5,5}, {7,2,2,2,7},
    {1,1,1,5,2}, {5,5,6,5,5}, {4,4,4,4,7},
    {5,7,7,5,5}, {5,7,7,7,5}, {2,5,5,5,2},
    {6,5,6,4,4}, {2,5,5,3,1}, {6,5,6,5,5},
    {3,4,2,1,6}, {7,2,2,2,2}, {5,5,5,5,7},
    {5,5,5,5,2}, {5,5,7,7,5}, {5,5,2,5,5},
    {5,5,2,2,2}, {7,1,2,4,7}
};

static const uint8_t tiny_numbers[10][5] =
{
    {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7},
    {7,1,7,1,7}, {5,5,7,1,1}, {7,4,7,1,7},
    {7,4,7,5,7}, {7,1,1,2,2}, {7,5,7,5,7},
    {7,5,7,1,7}
};

/*====================================================
 * CONTROL DE LOS OCHO LED
 * Bit 0 = LED1; bit 7 = LED8
 *====================================================*/
static void SetLEDPattern(uint8_t pattern)
{
    HAL_GPIO_WritePin(
        LED1_PORT, LED1_PIN,
        (pattern & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED2_PORT, LED2_PIN,
        (pattern & 0x02U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED3_PORT, LED3_PIN,
        (pattern & 0x04U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED4_PORT, LED4_PIN,
        (pattern & 0x08U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED5_PORT, LED5_PIN,
        (pattern & 0x10U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED6_PORT, LED6_PIN,
        (pattern & 0x20U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED7_PORT, LED7_PIN,
        (pattern & 0x40U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED8_PORT, LED8_PIN,
        (pattern & 0x80U) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );
}

/*====================================================
 * BUZZER Y MENSAJES
 *====================================================*/
static void BuzzerStart(uint32_t duration_ms)
{
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
    buzzer_active = 1U;
    buzzer_until = HAL_GetTick() + duration_ms;
}

static void BuzzerUpdate(uint32_t now)
{
    if (buzzer_active && ((int32_t)(now - buzzer_until) >= 0))
    {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        buzzer_active = 0U;
    }
}

static void SetStatus(const char *message, uint32_t duration_ms)
{
    strncpy(status_message, message, sizeof(status_message) - 1U);
    status_message[sizeof(status_message) - 1U] = '\0';
    status_until = HAL_GetTick() + duration_ms;
}

static uint8_t StatusIsVisible(uint32_t now)
{
    return (status_message[0] != '\0' &&
            (int32_t)(status_until - now) > 0) ? 1U : 0U;
}

/*====================================================
 * REINICIO DE DATOS
 *====================================================*/
static void ResetBowlingData(void)
{
    uint8_t f;
    uint8_t b;

    for (f = 0U; f < FRAMES; f++)
    {
        frame_roll_count[f] = 0U;

        for (b = 0U; b < 3U; b++)
        {
            frame_rolls[f][b] = EMPTY_ROLL;
        }
    }

    memset(roll_history, 0, sizeof(roll_history));

    roll_count = 0U;
    current_frame = 0U;
    balls_in_frame = 0U;
    game_finished = 0U;

    status_message[0] = '\0';
    status_until = 0U;
}

static void SaveActivePlayer(void)
{
    PlayerData_t *dst;

    if (active_player >= PLAYER_COUNT)
    {
        return;
    }

    dst = &player_data[active_player];

    memcpy(dst->frame_rolls, frame_rolls, sizeof(frame_rolls));
    memcpy(dst->frame_roll_count, frame_roll_count,
           sizeof(frame_roll_count));
    memcpy(dst->roll_history, roll_history, sizeof(roll_history));

    dst->roll_count = roll_count;
    dst->current_frame = current_frame;
    dst->balls_in_frame = balls_in_frame;
    dst->game_finished = game_finished;
}

static void LoadPlayerData(uint8_t player)
{
    PlayerData_t *src;

    if (player >= PLAYER_COUNT)
    {
        return;
    }

    active_player = player;
    src = &player_data[player];

    memcpy(frame_rolls, src->frame_rolls, sizeof(frame_rolls));
    memcpy(frame_roll_count, src->frame_roll_count,
           sizeof(frame_roll_count));
    memcpy(roll_history, src->roll_history, sizeof(roll_history));

    roll_count = src->roll_count;
    current_frame = src->current_frame;
    balls_in_frame = src->balls_in_frame;
    game_finished = src->game_finished;
}

static void ResetAllPlayers(void)
{
    uint8_t player;

    roll_event_count = 0U;
    memset(roll_owner_history, 0, sizeof(roll_owner_history));

    for (player = 0U; player < PLAYER_COUNT; player++)
    {
        active_player = player;

        ResetBowlingData();
        SaveActivePlayer();
    }

    LoadPlayerData(0U);
}

static void RecordRollEvent(uint8_t player)
{
    if ((player < PLAYER_COUNT) &&
        (roll_event_count < MAX_MATCH_ROLLS))
    {
        roll_owner_history[roll_event_count++] = player;
    }
}

/*====================================================
 * INICIAR PARTIDA Y VOLVER AL MENU
 *====================================================*/
static void StartGame(void)
{
    ResetAllPlayers();
    game_state = GAME_PLAYING;

    /* Detener animacion anterior */
    victory_animation_active = 0U;
    victory_leds_used = 0U;
    victory_next_step = 0U;

    SetLEDPattern(0x00U);

    buzzer_active = 0U;
    buzzer_until = 0U;

    HAL_GPIO_WritePin(
        BUZZER_PORT,
        BUZZER_PIN,
        GPIO_PIN_RESET
    );

    SetStatus("TURNO JUGADOR 1", STATUS_MS);
}

static void GoToMenu(void)
{
    game_state = GAME_MENU;
    menu_page = 0U;

    ResetAllPlayers();

    victory_animation_active = 0U;
    victory_leds_used = 0U;
    victory_next_step = 0U;

    SetLEDPattern(0x00U);

    HAL_GPIO_WritePin(
        BUZZER_PORT,
        BUZZER_PIN,
        GPIO_PIN_RESET
    );

    buzzer_active = 0U;
}

/*====================================================
 * CALCULO DEL PUNTAJE ACUMULADO
 *====================================================*/
static uint8_t GetCumulativeForFrame(
    uint8_t target_frame,
    uint16_t *cumulative_out
)
{
    uint8_t frame;
    uint16_t index = 0U;
    uint16_t cumulative = 0U;

    if (target_frame >= FRAMES || cumulative_out == 0)
    {
        return 0U;
    }

    for (frame = 0U; frame <= target_frame; frame++)
    {
        uint16_t frame_score;

        if (frame < 9U)
        {
            uint8_t first;
            uint8_t second;

            if (index >= roll_count)
            {
                return 0U;
            }

            first = roll_history[index];

            if (first == 10U)
            {
                if ((index + 2U) >= roll_count)
                {
                    return 0U;
                }

                frame_score = (uint16_t)(
                    10U +
                    roll_history[index + 1U] +
                    roll_history[index + 2U]
                );

                index += 1U;
            }
            else
            {
                if ((index + 1U) >= roll_count)
                {
                    return 0U;
                }

                second = roll_history[index + 1U];

                if ((uint16_t)first + second == 10U)
                {
                    if ((index + 2U) >= roll_count)
                    {
                        return 0U;
                    }

                    frame_score = (uint16_t)(
                        10U + roll_history[index + 2U]
                    );
                }
                else
                {
                    frame_score = (uint16_t)(first + second);
                }

                index += 2U;
            }

            cumulative = (uint16_t)(cumulative + frame_score);
        }
        else
        {
            uint16_t i;

            if (!game_finished)
            {
                return 0U;
            }

            for (i = index; i < roll_count; i++)
            {
                cumulative = (uint16_t)(
                    cumulative + roll_history[i]
                );
            }
        }
    }

    *cumulative_out = cumulative;

    return 1U;
}

static uint16_t GetLatestKnownCumulative(void)
{
    uint8_t frame;
    uint16_t value;
    uint16_t latest = 0U;

    for (frame = 0U; frame < FRAMES; frame++)
    {
        if (GetCumulativeForFrame(frame, &value))
        {
            latest = value;
        }
    }

    return latest;
}

/*====================================================
 * ANIMACION DE VICTORIA
 *
 * Recorre los ocho LED en orden pseudoaleatorio.
 * No repite un LED hasta haber recorrido los ocho.
 *====================================================*/
static void SetVictoryBarForWinner(void)
{
    uint8_t saved_player = active_player;
    uint16_t score_player1;
    uint16_t score_player2;

    SaveActivePlayer();

    LoadPlayerData(0U);
    score_player1 = GetLatestKnownCumulative();

    LoadPlayerData(1U);
    score_player2 = GetLatestKnownCumulative();

    LoadPlayerData(saved_player);

    victory_leds_used = 0U;
    SetLEDPattern(0x00U);

    if (score_player1 > score_player2)
    {
        SetStatus("GANA JUGADOR 1", STATUS_MS);
        victory_animation_active = 1U;
    }
    else if (score_player2 > score_player1)
    {
        SetStatus("GANA JUGADOR 2", STATUS_MS);
        victory_animation_active = 1U;
    }
    else
    {
        SetStatus("EMPATE", STATUS_MS);
        victory_animation_active = 0U;
        SetLEDPattern(0x00U);
        return;
    }

    /* Semilla pseudoaleatoria basada en el tiempo y los puntajes */
    victory_rng = HAL_GetTick() ^
                  ((uint32_t)score_player1 << 16) ^
                  (uint32_t)score_player2 ^
                  0x9E3779B9UL;

    if (victory_rng == 0U)
    {
        victory_rng = 0xA341316CU;
    }

    victory_next_step = HAL_GetTick();
}

/*====================================================
 * REGISTRAR LANZAMIENTO
 *====================================================*/
static uint8_t ApplyPins(uint8_t pins, uint8_t effects)
{
    uint8_t frame;
    uint8_t ball;
    uint8_t is_strike = 0U;
    uint8_t is_spare = 0U;
    uint8_t frame_before = current_frame;

    if (game_finished ||
        current_frame >= FRAMES ||
        pins > 10U ||
        roll_count >= MAX_ROLLS)
    {
        return 0U;
    }

    frame = current_frame;
    ball = balls_in_frame;

    if (frame < 9U)
    {
        if (ball == 0U)
        {
            frame_rolls[frame][0] = pins;
            frame_roll_count[frame] = 1U;
            roll_history[roll_count++] = pins;

            if (pins == 10U)
            {
                is_strike = 1U;
                current_frame++;
                balls_in_frame = 0U;
            }
            else
            {
                balls_in_frame = 1U;
            }
        }
        else
        {
            uint8_t first = frame_rolls[frame][0];

            if ((uint16_t)first + pins > 10U)
            {
                return 0U;
            }

            frame_rolls[frame][1] = pins;
            frame_roll_count[frame] = 2U;
            roll_history[roll_count++] = pins;

            if ((uint16_t)first + pins == 10U)
            {
                is_spare = 1U;
            }

            current_frame++;
            balls_in_frame = 0U;
        }
    }
    else
    {
        /* Decimo cuadro */
        uint8_t first = frame_rolls[9][0];
        uint8_t second = frame_rolls[9][1];

        if (ball == 0U)
        {
            frame_rolls[9][0] = pins;
            frame_roll_count[9] = 1U;
            roll_history[roll_count++] = pins;

            if (pins == 10U)
            {
                is_strike = 1U;
            }

            balls_in_frame = 1U;
        }
        else if (ball == 1U)
        {
            first = frame_rolls[9][0];

            if ((first < 10U) &&
                ((uint16_t)first + pins > 10U))
            {
                return 0U;
            }

            frame_rolls[9][1] = pins;
            frame_roll_count[9] = 2U;
            roll_history[roll_count++] = pins;

            if ((first == 10U) && (pins == 10U))
            {
                is_strike = 1U;
            }
            else if ((first < 10U) &&
                     ((uint16_t)first + pins == 10U))
            {
                is_spare = 1U;
            }

            if ((first == 10U) ||
                ((uint16_t)first + pins == 10U))
            {
                balls_in_frame = 2U;
            }
            else
            {
                game_finished = 1U;
                current_frame = 10U;
            }
        }
        else
        {
            first = frame_rolls[9][0];
            second = frame_rolls[9][1];

            if ((first == 10U) &&
                (second < 10U) &&
                ((uint16_t)second + pins > 10U))
            {
                return 0U;
            }

            frame_rolls[9][2] = pins;
            frame_roll_count[9] = 3U;
            roll_history[roll_count++] = pins;

            if ((first == 10U) &&
                (second == 10U) &&
                (pins == 10U))
            {
                is_strike = 1U;
            }
            else if ((first == 10U) &&
                     (second < 10U) &&
                     ((uint16_t)second + pins == 10U))
            {
                is_spare = 1U;
            }
            else if ((first < 10U) &&
                     ((uint16_t)first + second == 10U) &&
                     (pins == 10U))
            {
                is_strike = 1U;
            }

            game_finished = 1U;
            current_frame = 10U;
        }
    }

    if (effects)
    {
        uint8_t other_player;
        uint8_t frame_completed;

        if (game_finished)
        {
            SetStatus("JUGADOR TERMINO", STATUS_MS);
            BuzzerStart(BUZZER_FIN_MS);
        }
        else if (is_strike)
        {
            SetStatus("STRIKE", STATUS_MS);
            BuzzerStart(BUZZER_STRIKE_MS);
        }
        else if (is_spare)
        {
            SetStatus("SPARE", STATUS_MS);
            BuzzerStart(BUZZER_SPARE_MS);
        }
        else
        {
            SetStatus("LANZAMIENTO OK", STATUS_MS);
            BuzzerStart(BUZZER_NORMAL_MS);
        }

        /* Mantener apagada la barra durante el juego */
        SetLEDPattern(0x00U);

        SaveActivePlayer();

        frame_completed =
            ((current_frame != frame_before) || game_finished)
            ? 1U : 0U;

        if (frame_completed)
        {
            other_player = (uint8_t)(active_player ^ 1U);

            /*
             * Ambos terminaron:
             * determinar al ganador e iniciar la animacion.
             */
            if (game_finished &&
                player_data[other_player].game_finished)
            {
                game_state = GAME_OVER;
                BuzzerStart(BUZZER_FIN_MS);
                SetVictoryBarForWinner();
            }
            /*
             * El jugador actual terminó antes que el rival.
             * El rival continúa hasta finalizar su partida.
             */
            else if (game_finished)
            {
                if (!player_data[other_player].game_finished)
                {
                    LoadPlayerData(other_player);
                    game_state = GAME_PLAYING;

                    SetStatus(
                        (other_player == 0U)
                        ? "TURNO JUGADOR 1"
                        : "TURNO JUGADOR 2",
                        STATUS_MS
                    );

                    SetLEDPattern(0x00U);
                }
            }
            /*
             * Si el rival ya terminó, el jugador activo continúa
             * con sus cuadros pendientes.
             */
            else if (player_data[other_player].game_finished)
            {
                game_state = GAME_PLAYING;

                SetStatus(
                    (active_player == 0U)
                    ? "CONTINUA JUGADOR 1"
                    : "CONTINUA JUGADOR 2",
                    STATUS_MS
                );

                SetLEDPattern(0x00U);
            }
            else
            {
                LoadPlayerData(other_player);
                game_state = GAME_PLAYING;

                SetStatus(
                    (other_player == 0U)
                    ? "TURNO JUGADOR 1"
                    : "TURNO JUGADOR 2",
                    STATUS_MS
                );

                SetLEDPattern(0x00U);
            }
        }
    }

    return 1U;
}

/*====================================================
 * DESHACER ULTIMO LANZAMIENTO
 *====================================================*/
static void UndoLastRoll(void)
{
    uint8_t old_rolls[MAX_ROLLS];
    uint8_t old_count;
    uint8_t i;

    if (roll_count == 0U)
    {
        SetStatus("NO HAY LANZAMIENTOS", STATUS_MS);
        BuzzerStart(BUZZER_ERROR_MS);
        return;
    }

    old_count = (uint8_t)(roll_count - 1U);

    for (i = 0U; i < old_count; i++)
    {
        old_rolls[i] = roll_history[i];
    }

    ResetBowlingData();
    game_state = GAME_PLAYING;

    for (i = 0U; i < old_count; i++)
    {
        (void)ApplyPins(old_rolls[i], 0U);
    }

    SaveActivePlayer();
    SetLEDPattern(0x00U);
    SetStatus("LANZAMIENTO BORRADO", STATUS_MS);
}

static void UndoLastMatchRoll(void)
{
    uint8_t player;

    if (roll_event_count == 0U)
    {
        SetStatus("NO HAY LANZAMIENTOS", STATUS_MS);
        BuzzerStart(BUZZER_ERROR_MS);
        return;
    }

    player = roll_owner_history[--roll_event_count];

    LoadPlayerData(player);
    UndoLastRoll();
    LoadPlayerData(player);

    game_state = GAME_PLAYING;

    SetStatus(
        (player == 0U)
        ? "CORRECCION JUGADOR 1"
        : "CORRECCION JUGADOR 2",
        STATUS_MS
    );
}

/*====================================================
 * DIBUJO DE FUENTE GRANDE
 *====================================================*/
static void DrawChar(uint8_t x, uint8_t y, char c)
{
    const uint8_t *glyph = 0;
    uint8_t row;
    uint8_t col;

    if (c >= 'a' && c <= 'z')
    {
        c = (char)(c - 'a' + 'A');
    }

    if (c >= 'A' && c <= 'Z')
    {
        glyph = font_letters[(uint8_t)(c - 'A')];
    }
    else if (c >= '0' && c <= '9')
    {
        glyph = font_numbers[(uint8_t)(c - '0')];
    }
    else if (c == '/')
    {
        DrawLine(
            (uint8_t)(x + 4U), y,
            x, (uint8_t)(y + 6U)
        );
        return;
    }
    else if (c == '-')
    {
        DrawLine(
            x, (uint8_t)(y + 3U),
            (uint8_t)(x + 4U), (uint8_t)(y + 3U)
        );
        return;
    }
    else if (c == ':')
    {
        SetPixel((uint8_t)(x + 2U), (uint8_t)(y + 2U));
        SetPixel((uint8_t)(x + 2U), (uint8_t)(y + 5U));
        return;
    }
    else if (c == '!')
    {
        DrawLine(
            (uint8_t)(x + 2U), y,
            (uint8_t)(x + 2U), (uint8_t)(y + 4U)
        );
        SetPixel((uint8_t)(x + 2U), (uint8_t)(y + 6U));
        return;
    }
    else if (c == '*')
    {
        DrawLine(
            (uint8_t)(x + 2U), y,
            (uint8_t)(x + 2U), (uint8_t)(y + 6U)
        );
        DrawLine(
            x, (uint8_t)(y + 2U),
            (uint8_t)(x + 4U), (uint8_t)(y + 4U)
        );
        DrawLine(
            x, (uint8_t)(y + 4U),
            (uint8_t)(x + 4U), (uint8_t)(y + 2U)
        );
        return;
    }
    else
    {
        return;
    }

    for (row = 0U; row < 7U; row++)
    {
        for (col = 0U; col < 5U; col++)
        {
            if (glyph[row] & (1U << (4U - col)))
            {
                SetPixel((uint8_t)(x + col), (uint8_t)(y + row));
            }
        }
    }
}

static void DrawText(uint8_t x, uint8_t y, const char *text)
{
    uint16_t i;

    for (i = 0U; text[i] != '\0'; i++)
    {
        if (text[i] != ' ')
        {
            DrawChar(x, y, text[i]);
        }

        x = (uint8_t)(x + 6U);
    }
}

static void DrawTextCentered(uint8_t y, const char *text)
{
    uint16_t width = (uint16_t)(strlen(text) * 6U);

    uint8_t x = (width >= SCREEN_WIDTH)
                ? 0U
                : (uint8_t)((SCREEN_WIDTH - width) / 2U);

    DrawText(x, y, text);
}

static void DrawNumber(uint8_t x, uint8_t y, uint16_t value)
{
    char text[8];
    uint8_t i;

    (void)snprintf(
        text, sizeof(text), "%u",
        (unsigned int)value
    );

    for (i = 0U; text[i] != '\0'; i++)
    {
        DrawChar(x, y, text[i]);
        x = (uint8_t)(x + 6U);
    }
}

/*====================================================
 * DIBUJO DE FUENTE COMPACTA
 *====================================================*/
static void DrawTinyChar(uint8_t x, uint8_t y, char c)
{
    const uint8_t *glyph = 0;
    uint8_t row;

    if (c >= 'a' && c <= 'z')
    {
        c = (char)(c - 'a' + 'A');
    }

    if (c >= 'A' && c <= 'Z')
    {
        glyph = tiny_letters[(uint8_t)(c - 'A')];
    }
    else if (c >= '0' && c <= '9')
    {
        glyph = tiny_numbers[(uint8_t)(c - '0')];
    }
    else if (c == '/')
    {
        static const uint8_t slash[5] = {1,1,2,4,4};
        glyph = slash;
    }
    else if (c == '-')
    {
        static const uint8_t dash[5] = {0,0,7,0,0};
        glyph = dash;
    }
    else if (c == ':')
    {
        static const uint8_t colon[5] = {0,2,0,2,0};
        glyph = colon;
    }
    else if (c == '=')
    {
        static const uint8_t equal[5] = {0,7,0,7,0};
        glyph = equal;
    }
    else if (c == '*')
    {
        static const uint8_t star[5] = {0,5,2,5,0};
        glyph = star;
    }
    else
    {
        return;
    }

    for (row = 0U; row < 5U; row++)
    {
        uint8_t col;

        for (col = 0U; col < 3U; col++)
        {
            if (glyph[row] & (1U << (2U - col)))
            {
                SetPixel((uint8_t)(x + col), (uint8_t)(y + row));
            }
        }
    }
}

static void DrawTinyText(uint8_t x, uint8_t y, const char *text)
{
    uint16_t i;

    for (i = 0U; text[i] != '\0'; i++)
    {
        if (text[i] != ' ')
        {
            DrawTinyChar(x, y, text[i]);
        }

        x = (uint8_t)(x + 4U);
    }
}

static void DrawTinyNumber(uint8_t x, uint8_t y, uint16_t value)
{
    char text[6];
    uint8_t i;

    (void)snprintf(
        text, sizeof(text), "%u",
        (unsigned int)value
    );

    for (i = 0U; text[i] != '\0'; i++)
    {
        DrawTinyChar(x, y, text[i]);
        x = (uint8_t)(x + 4U);
    }
}

/*====================================================
 * SIMBOLOS DE LANZAMIENTOS
 * X = strike, / = spare, - = cero bolos
 *====================================================*/
static char GetRollSymbol(uint8_t frame, uint8_t ball)
{
    uint8_t value;
    uint8_t first;
    uint8_t second;

    if (frame >= FRAMES || ball > 2U)
    {
        return ' ';
    }

    value = frame_rolls[frame][ball];

    if (value == EMPTY_ROLL)
    {
        return ' ';
    }

    first = frame_rolls[frame][0];
    second = frame_rolls[frame][1];

    if (frame < 9U)
    {
        if ((ball == 0U) && (value == 10U))
        {
            return 'X';
        }

        if ((ball == 1U) &&
            (first != EMPTY_ROLL) &&
            ((uint16_t)first + value == 10U))
        {
            return '/';
        }
    }
    else
    {
        if ((ball == 0U) && (value == 10U))
        {
            return 'X';
        }

        if (ball == 1U)
        {
            if ((first == 10U) && (value == 10U))
            {
                return 'X';
            }

            if ((first < 10U) &&
                ((uint16_t)first + value == 10U))
            {
                return '/';
            }
        }

        if (ball == 2U)
        {
            if ((first == 10U) &&
                (second == 10U) &&
                (value == 10U))
            {
                return 'X';
            }

            if ((first == 10U) &&
                (second < 10U) &&
                ((uint16_t)second + value == 10U))
            {
                return '/';
            }

            if ((first < 10U) &&
                ((uint16_t)first + second == 10U) &&
                (value == 10U))
            {
                return 'X';
            }
        }
    }

    if (value == 0U)
    {
        return '-';
    }

    return (char)('0' + value);
}

static void DrawRollSymbol(
    uint8_t x,
    uint8_t y,
    uint8_t frame,
    uint8_t ball
)
{
    char symbol = GetRollSymbol(frame, ball);

    if (symbol != ' ')
    {
        DrawTinyChar(x, y, symbol);
    }
}

/*====================================================
 * MENU
 *====================================================*/
static void DrawMenu(void)
{
    DrawRectangle(3U, 2U, 121U, 59U);
    DrawTextCentered(5U, "BOLOS T11");
    DrawLine(18U, 14U, 109U, 14U);

    if (menu_page == 0U)
    {
        DrawTextCentered(19U, "BOLOS 2 JUGADORES");
        DrawTextCentered(29U, "10 FRAMES POR JUGADOR");
        DrawTextCentered(39U, "0-9 BOLOS A 10");
        DrawTextCentered(48U, "* INICIAR PARTIDA");
        DrawTextCentered(56U, "A AYUDA");
    }
    else
    {
        DrawTextCentered(18U, "P1 Y P2 ALTERNAN");
        DrawTextCentered(27U, "0-9: BOLOS DERRIBADOS");
        DrawTextCentered(36U, "A: 10 BOLOS / STRIKE");
        DrawTextCentered(44U, "B: DESHACER ULTIMO TIRO");
        DrawTextCentered(52U, "C: PAUSA   #: MENU");
    }
}

/*====================================================
 * TABLERO DE LOS DIEZ CUADROS
 *====================================================*/
static void DrawPlayerRows(
    uint8_t player,
    uint8_t rolls_y,
    uint8_t score_y
)
{
    uint8_t frame;
    uint16_t cumulative;

    LoadPlayerData(player);

    for (frame = 0U; frame < FRAMES; frame++)
    {
        uint8_t x = (uint8_t)(12U + frame * 11U);

        if (frame < 9U)
        {
            DrawRollSymbol(
                (uint8_t)(x + 1U), rolls_y, frame, 0U
            );

            DrawRollSymbol(
                (uint8_t)(x + 6U), rolls_y, frame, 1U
            );
        }
        else
        {
            DrawRollSymbol(x, rolls_y, frame, 0U);

            DrawRollSymbol(
                (uint8_t)(x + 3U), rolls_y, frame, 1U
            );

            DrawRollSymbol(
                (uint8_t)(x + 6U), rolls_y, frame, 2U
            );
        }

        if (GetCumulativeForFrame(frame, &cumulative))
        {
            char score_text[6];
            uint8_t length;
            uint8_t score_x;

            (void)snprintf(
                score_text, sizeof(score_text), "%u",
                (unsigned int)cumulative
            );

            length = (uint8_t)strlen(score_text);

            if (length >= 3U)
            {
                score_x = x;
            }
            else if (length == 2U)
            {
                score_x = (uint8_t)(x + 1U);
            }
            else
            {
                score_x = (uint8_t)(x + 3U);
            }

            DrawTinyText(score_x, score_y, score_text);
        }
    }
}

static void DrawScoreboard(void)
{
    uint8_t frame;
    uint8_t saved_player = active_player;
    uint16_t score1;
    uint16_t score2;
    char header[32];
    char footer[28];

    SaveActivePlayer();

    LoadPlayerData(0U);
    score1 = GetLatestKnownCumulative();

    LoadPlayerData(1U);
    score2 = GetLatestKnownCumulative();

    if (game_state == GAME_OVER)
    {
        (void)snprintf(
            header, sizeof(header),
            "P1:%u P2:%u FIN",
            (unsigned int)score1,
            (unsigned int)score2
        );
    }
    else
    {
        (void)snprintf(
            header, sizeof(header),
            "P1:%u P2:%u TURNO:%u",
            (unsigned int)score1,
            (unsigned int)score2,
            (unsigned int)(saved_player + 1U)
        );
    }

    DrawTextCentered(0U, header);
    DrawLine(0U, 8U, 127U, 8U);

    DrawText(0U, 10U, "F");
    DrawLine(12U, 10U, 12U, 45U);

    for (frame = 0U; frame <= FRAMES; frame++)
    {
        uint8_t x = (uint8_t)(12U + frame * 11U);
        DrawLine(x, 10U, x, 45U);
    }

    for (frame = 0U; frame < FRAMES; frame++)
    {
        uint8_t x = (uint8_t)(12U + frame * 11U);

        uint8_t label_x =
            (frame == 9U)
            ? (uint8_t)(x + 1U)
            : (uint8_t)(x + 3U);

        DrawTinyNumber(label_x, 11U, (uint16_t)(frame + 1U));
    }

    DrawLine(0U, 16U, 123U, 16U);
    DrawLine(0U, 29U, 123U, 29U);
    DrawLine(0U, 30U, 123U, 30U);
    DrawLine(0U, 45U, 123U, 45U);

    DrawTinyText(0U, 19U, "P1");
    DrawTinyText(0U, 34U, "P2");

    DrawPlayerRows(0U, 18U, 24U);
    DrawPlayerRows(1U, 33U, 39U);

    if ((game_state == GAME_PLAYING) &&
        (current_frame < FRAMES))
    {
        uint8_t x = (uint8_t)(12U + current_frame * 11U);
        uint8_t y1 = (saved_player == 0U) ? 16U : 30U;
        uint8_t y2 = (saved_player == 0U) ? 29U : 45U;

        DrawRectangle(x, y1, 10U, (uint8_t)(y2 - y1));
    }

    LoadPlayerData(saved_player);

    if (game_state == GAME_OVER)
    {
        if (score1 > score2)
        {
            (void)snprintf(footer, sizeof(footer), "GANA JUGADOR 1");
        }
        else if (score2 > score1)
        {
            (void)snprintf(footer, sizeof(footer), "GANA JUGADOR 2");
        }
        else
        {
            (void)snprintf(footer, sizeof(footer), "EMPATE");
        }

        DrawTextCentered(48U, footer);
        DrawTinyText(2U, 58U, "* NUEVA PARTIDA  # MENU");
    }
    else if (game_state == GAME_PAUSED)
    {
        DrawTextCentered(48U, "PAUSA");
        DrawTinyText(2U, 58U, "C CONTINUAR # MENU * RESET");
    }
    else if (StatusIsVisible(HAL_GetTick()))
    {
        DrawTextCentered(48U, status_message);
        DrawTinyText(2U, 58U, "0-9 PINOS A=10 B=UNDO C=PAUSA");
    }
    else
    {
        DrawText(2U, 48U, "TURNO");
        DrawNumber(38U, 48U, (uint16_t)(saved_player + 1U));
        DrawTinyText(2U, 58U, "0-9 PINOS A=10 B=UNDO C=PAUSA");
    }
}

static void DrawPauseScreen(void)
{
    DrawRectangle(8U, 8U, 111U, 48U);
    DrawTextCentered(17U, "PARTIDA EN PAUSA");
    DrawTextCentered(30U, "C CONTINUAR");
    DrawTextCentered(41U, "# VOLVER AL MENU");
    DrawTextCentered(51U, "* REINICIAR");
}

/*====================================================
 * API PUBLICA
 *====================================================*/
void Game_Init(void)
{
    game_state = GAME_MENU;
    menu_page = 0U;

    ResetAllPlayers();

    victory_animation_active = 0U;
    victory_leds_used = 0U;
    victory_next_step = 0U;
    victory_rng = 0xA341316CU;

    buzzer_active = 0U;
    buzzer_until = 0U;

    HAL_GPIO_WritePin(
        BUZZER_PORT,
        BUZZER_PIN,
        GPIO_PIN_RESET
    );

    SetLEDPattern(0x00U);
}

void Game_HandleKey(char tecla)
{
    uint8_t pins;

    if (tecla == '\0')
    {
        return;
    }

    if (game_state == GAME_MENU)
    {
        if (tecla == 'A')
        {
            menu_page = (uint8_t)(menu_page == 0U ? 1U : 0U);
        }
        else if (tecla == '*')
        {
            StartGame();
        }
        else if (tecla == '#')
        {
            menu_page = 0U;
        }

        return;
    }

    if (game_state == GAME_PAUSED)
    {
        if (tecla == 'C')
        {
            game_state = GAME_PLAYING;
            SetStatus("PARTIDA REANUDADA", STATUS_MS);
        }
        else if (tecla == '#')
        {
            GoToMenu();
        }
        else if (tecla == '*')
        {
            StartGame();
        }

        return;
    }

    if (game_state == GAME_OVER)
    {
        if (tecla == '*')
        {
            StartGame();
        }
        else if (tecla == '#')
        {
            GoToMenu();
        }

        return;
    }

    if (game_state != GAME_PLAYING)
    {
        return;
    }

    if (tecla >= '0' && tecla <= '9')
    {
        uint8_t roller = active_player;

        pins = (uint8_t)(tecla - '0');

        if (!ApplyPins(pins, 1U))
        {
            SetStatus("BOLOS NO VALIDOS", STATUS_MS);
            BuzzerStart(BUZZER_ERROR_MS);
        }
        else
        {
            RecordRollEvent(roller);
        }
    }
    else if (tecla == 'A')
    {
        uint8_t roller = active_player;

        if (!ApplyPins(10U, 1U))
        {
            SetStatus("NO QUEDAN 10 BOLOS", STATUS_MS);
            BuzzerStart(BUZZER_ERROR_MS);
        }
        else
        {
            RecordRollEvent(roller);
        }
    }
    else if (tecla == 'B')
    {
        UndoLastMatchRoll();
    }
    else if (tecla == 'C')
    {
        game_state = GAME_PAUSED;
    }
    else if (tecla == '#')
    {
        GoToMenu();
    }
    else if (tecla == '*')
    {
        StartGame();
    }
}

/*====================================================
 * ACTUALIZACION DE LA ANIMACION
 *
 * Cambia de LED cada 130 ms.
 * En cada ciclo recorre los ocho LED sin repetirse.
 *====================================================*/
void Game_Update(uint32_t ahora)
{
    uint8_t led;

    BuzzerUpdate(ahora);

    if ((game_state != GAME_OVER) ||
        (victory_animation_active == 0U))
    {
        return;
    }

    if ((int32_t)(ahora - victory_next_step) >= 0)
    {
        /* Generador pseudoaleatorio de 32 bits */
        victory_rng =
            victory_rng * 1664525UL + 1013904223UL;

        /* Elegir inicialmente un LED entre 0 y 7 */
        led = (uint8_t)((victory_rng >> 24) & 0x07U);

        /*
         * Si ese LED ya aparecio en este ciclo,
         * buscar el siguiente que no se haya usado.
         */
        while ((victory_leds_used &
                (uint8_t)(1U << led)) != 0U)
        {
            led = (uint8_t)((led + 1U) & 0x07U);
        }

        /* Marcar el LED como utilizado */
        victory_leds_used |= (uint8_t)(1U << led);

        /* Encender solamente el LED elegido */
        SetLEDPattern((uint8_t)(1U << led));

        /* Después de recorrer los ocho, iniciar otro ciclo */
        if (victory_leds_used == 0xFFU)
        {
            victory_leds_used = 0U;
        }

        /* Programar el siguiente cambio sin bloquear */
        victory_next_step = ahora + 130U;
    }
}

void Game_Draw(void)
{
    ST7920_ClearBuffer();

    if (game_state == GAME_MENU)
    {
        DrawMenu();
    }
    else if (game_state == GAME_PAUSED)
    {
        DrawPauseScreen();
    }
    else
    {
        DrawScoreboard();
    }

    ST7920_Update();
}