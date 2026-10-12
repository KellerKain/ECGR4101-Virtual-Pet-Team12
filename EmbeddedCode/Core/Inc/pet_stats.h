
#ifndef PET_STATS_H
#define PET_STATS_H

#include <stdint.h>

/* Statistics range from 0 to 100. */
#define PET_STAT_MIN 0
#define PET_STAT_MAX 100

/* Values at which the game ends. */
#define PET_HUNGER_LIMIT 100
#define PET_HAPPINESS_LIMIT 0

typedef enum
{
    PET_ALIVE,
    PET_DEAD,
    PET_RAN_AWAY
} PetStatus;

typedef struct
{
    uint8_t hunger;       /* 0 = full, 100 = starving */
    uint8_t happiness;    /* 0 = unhappy, 100 = happy */
    PetStatus status;
} PetStats;

/* Initialize a new pet. */
void PetStats_Init(PetStats *pet);

/* Update statistics; call once per second. */
void PetStats_Update(PetStats *pet);

/* Pet interaction functions. */
void PetStats_Feed(PetStats *pet);
void PetStats_Play(PetStats *pet);

#endif /* PET_STATS_H */
